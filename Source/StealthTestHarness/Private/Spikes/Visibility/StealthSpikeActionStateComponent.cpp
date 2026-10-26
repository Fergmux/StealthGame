// THROWAWAY spike stub for IVP spike S10. See StealthSpikeActionStateComponent.h.
#include "Spikes/Visibility/StealthSpikeActionStateComponent.h"

#include "Scenarios/StealthHarnessRun.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "HAL/PlatformTime.h"
#include "Misc/Paths.h"
#include "Net/UnrealNetwork.h"
#include "UnrealClient.h"

UStealthSpikeActionStateComponent::UStealthSpikeActionStateComponent()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

void UStealthSpikeActionStateComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UStealthSpikeActionStateComponent, State);
}

TArray<int32>& UStealthSpikeActionStateComponent::CaptureFrames()
{
	static TArray<int32> Frames;
	return Frames;
}

int32& UStealthSpikeActionStateComponent::CaptureBudget()
{
	static int32 Budget = 0;
	return Budget;
}

bool& UStealthSpikeActionStateComponent::ExternalApply()
{
	static bool bExternal = false;
	return bExternal;
}

void UStealthSpikeActionStateComponent::ApplyPresentation(double EstimatedServerNow)
{
	RefreshPresentation(EstimatedServerNow, TEXT("apply"));
}

void UStealthSpikeActionStateComponent::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() == NM_Client && ExternalApply())
	{
		SetComponentTickEnabled(false);
		return;
	}
	if (GetNetMode() == NM_Client)
	{
		const AGameStateBase* GameState = GetWorld()->GetGameState();
		RefreshPresentation(GameState ? GameState->GetServerWorldTimeSeconds() : -1.0, TEXT("beginPlay"));
		if (CaptureBudget() > 0 && CaptureFrames().Num() > 0)
		{
			--CaptureBudget();
			bCapturing = true;
			CaptureTag = FString::Printf(TEXT("%s_%lld"), *StealthHarness::FRun::Guid(GetWorld(), GetOwner()), (long long)GFrameCounter);
		}
	}
}

void UStealthSpikeActionStateComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Client: the relevancy destroy of a culled subject. The render client captures the frame without it, so the
	// retained sequence shows culled -> re-opened (f00, f01, f02, f05).
	if (GetNetMode() == NM_Client && !ExternalApply() && EndPlayReason == EEndPlayReason::Destroyed && CaptureBudget() > 0 && CaptureFrames().Num() > 0)
	{
		const FString File = FPaths::Combine(StealthHarness::FRun::StringParam(TEXT("Out"), FPaths::ProjectSavedDir()), TEXT("frames"),
			FString::Printf(TEXT("reopen_%s_%lld_gone.png"), *StealthHarness::FRun::Guid(GetWorld(), GetOwner()), (long long)GFrameCounter));
		FScreenshotRequest::RequestScreenshot(File, /*bShowUI*/ false, /*bAddFilenameSuffix*/ false);
	}
	Super::EndPlay(EndPlayReason);
}

void UStealthSpikeActionStateComponent::RefreshPresentation(double EstimatedServerNow, const TCHAR* Phase)
{
	StealthHarness::FRun* Run = StealthHarness::FRun::Get();
	if (!Run || !GetOwner())
	{
		return;
	}
	// Pose time and death fast-forward come only from replicated server times and the estimated server clock.
	const double PoseTime = State.StartServerTime > 0.0 ? EstimatedServerNow - State.StartServerTime : -1.0;
	const double DeathElapsed = State.DeathServerTime >= 0.0 ? EstimatedServerNow - State.DeathServerTime : -1.0;
	const FVector Location = GetOwner()->GetActorLocation();
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	const float PingMs = PC && PC->PlayerState ? PC->PlayerState->ExactPing : -1.f; // for a latency-compensated comparison only
	Run->Write(TEXT("reopenPresent"), FString::Printf(TEXT("\"phase\":%s,\"guid\":%s,\"name\":%s,\"pingMs\":%.2f,\"estServerNow\":%.6f,\"start\":%.6f,\"death\":%.6f,\"poseTime\":%.6f,\"deathElapsed\":%.6f,\"loc\":[%.3f,%.3f,%.3f],\"frame\":%llu"),
		*StealthHarness::FRun::Q(Phase), *StealthHarness::FRun::Q(StealthHarness::FRun::Guid(GetWorld(), GetOwner())), *StealthHarness::FRun::Q(GetOwner()->GetName()),
		PingMs, EstimatedServerNow, State.StartServerTime, State.DeathServerTime, PoseTime, DeathElapsed, Location.X, Location.Y, Location.Z, (unsigned long long)GFrameCounter));
}

void UStealthSpikeActionStateComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (GetNetMode() != NM_Client)
	{
		SetComponentTickEnabled(false);
		return;
	}
	if (TicksSinceBegin == 0)
	{
		// The first frame the re-created actor ticks is the first frame it is rendered in.
		const AGameStateBase* GameState = GetWorld()->GetGameState();
		RefreshPresentation(GameState ? GameState->GetServerWorldTimeSeconds() : -1.0, TEXT("firstFrame"));
	}
	if (bCapturing && CaptureFrames().Contains(TicksSinceBegin))
	{
		const FString File = FPaths::Combine(StealthHarness::FRun::StringParam(TEXT("Out"), FPaths::ProjectSavedDir()),
			TEXT("frames"), FString::Printf(TEXT("reopen_%s_f%02d.png"), *CaptureTag, TicksSinceBegin));
		FScreenshotRequest::RequestScreenshot(File, /*bShowUI*/ false, /*bAddFilenameSuffix*/ false);
		if (StealthHarness::FRun* Run = StealthHarness::FRun::Get())
		{
			Run->Write(TEXT("frameCapture"), FString::Printf(TEXT("\"file\":%s,\"tick\":%d,\"frame\":%llu"),
				*StealthHarness::FRun::Q(File), TicksSinceBegin, (unsigned long long)GFrameCounter));
		}
	}
	++TicksSinceBegin;
	if (TicksSinceBegin > 30)
	{
		SetComponentTickEnabled(false);
	}
}
