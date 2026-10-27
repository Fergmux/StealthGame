// Story asn-021: the own-role clock and the transient grade stand-in. See View/EncounterViewSubsystem.h.

#include "View/EncounterViewSubsystem.h"

#include "Camera/PlayerCameraManager.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/IConsoleManager.h"
#include "StealthGameplayModule.h"
#include "View/EncounterPresentationSettings.h"

namespace EncounterViewLocal
{
	/** STAND-IN for the accessibility settings menu (art bible 7.6 rule 8). */
	TAutoConsoleVariable<bool> CVarReduceScreenEffects(TEXT("Stealth.Accessibility.ReduceScreenEffects"), false,
		TEXT("Reduce screen effects: the assassination grade eases in over 0.5 s, its saturation drop is capped at -30 percent and its vignette is off."));
}

const TCHAR* LexRoleChangeReason(ERoleChangeReason Reason)
{
	switch (Reason)
	{
	case ERoleChangeReason::ReversedAgainst: return TEXT("ReversedAgainst");
	case ERoleChangeReason::InformantKilled: return TEXT("InformantKilled");
	default: return TEXT("Assassinated");
	}
}

UEncounterViewSubsystem* UEncounterViewSubsystem::FindForPawn(const APawn* Pawn)
{
	const APlayerController* Controller = Pawn != nullptr ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (Controller == nullptr || !Controller->IsLocalController())
	{
		return nullptr;
	}
	const ULocalPlayer* Local = Controller->GetLocalPlayer();
	return Local != nullptr ? Local->GetSubsystem<UEncounterViewSubsystem>() : nullptr;
}

bool UEncounterViewSubsystem::IsReduceScreenEffects()
{
	return EncounterViewLocal::CVarReduceScreenEffects.GetValueOnGameThread();
}

void UEncounterViewSubsystem::DispatchRoleChange(const APawn* Pawn, ERoleChangeReason Reason, double ServerNow, double DeathServerTime)
{
	FRoleChangeRecord& Record = RoleChanges.AddDefaulted_GetRef();
	Record.Reason = Reason;
	Record.ServerNow = ServerNow;
	Record.DeathServerTime = DeathServerTime;
	Record.PawnKey = Pawn;
	UE_LOGF(LogStealthGameplay, Log, "[EncounterView] role change %ls presented at %.3f (DeathServerTime %.3f)", LexRoleChangeReason(Reason), ServerNow, DeathServerTime);
	const UWorld* World = GetLocalPlayer() != nullptr ? GetLocalPlayer()->GetWorld() : nullptr;
	OnRoleChangePresented.Broadcast(Reason, World != nullptr ? World->GetTimeSeconds() : 0.0);
}

void UEncounterViewSubsystem::ApplyTransientGrade(ETransientGradeCause Cause, double ServerNow, double DueServerTime)
{
	// One beat per contact on this camera: a reversed attacker is both the attacker and the victim of one contact.
	if (GradeCalls.ContainsByPredicate([DueServerTime](const FTransientGradeCall& Previous) { return FMath::Abs(Previous.DueServerTime - DueServerTime) < 0.002; }))
	{
		return;
	}
	ULocalPlayer* Local = GetLocalPlayer();
	UWorld* World = Local != nullptr ? Local->GetWorld() : nullptr;
	FTransientGradeCall& Call = GradeCalls.AddDefaulted_GetRef();
	Call.Cause = Cause;
	Call.ServerNow = ServerNow;
	Call.DueServerTime = DueServerTime;
	Call.TimeDilation = World != nullptr && World->GetWorldSettings() != nullptr ? World->GetWorldSettings()->TimeDilation : 1.0f;
	Call.bReduced = IsReduceScreenEffects();
	GradeStartWorldTime = World != nullptr ? World->GetTimeSeconds() : 0.0;
	bGradeReduced = Call.bReduced;
	UE_LOGF(LogStealthGameplay, Log, "[EncounterView] transient grade cause %d at %.3f (due %.3f, reduced %d)", static_cast<int32>(Cause), ServerNow, DueServerTime, Call.bReduced ? 1 : 0);

	// STAND-IN for Camera story 019: the grade is a post-process beat of this player's camera only.
	APlayerController* Controller = Local != nullptr ? Local->GetPlayerController(World) : nullptr;
	if (Controller != nullptr && Controller->PlayerCameraManager != nullptr
		&& Controller->PlayerCameraManager->FindCameraModifierByClass(UEncounterGradeCameraModifier::StaticClass()) == nullptr)
	{
		Controller->PlayerCameraManager->AddNewCameraModifier(UEncounterGradeCameraModifier::StaticClass());
	}
}

float UEncounterViewSubsystem::GetGradeWeight(double WorldTime) const
{
	if (GradeStartWorldTime < 0.0)
	{
		return 0.0f;
	}
	const UEncounterPresentationSettings* Settings = UEncounterPresentationSettings::Get();
	const double In = FMath::Max(0.001, static_cast<double>(bGradeReduced ? Settings->ReducedGradeInSec : Settings->GradeInSec));
	const double Hold = Settings->GradeHoldSec;
	const double Out = FMath::Max(0.001, static_cast<double>(Settings->GradeOutSec));
	const double T = WorldTime - GradeStartWorldTime;
	if (T < 0.0 || T >= In + Hold + Out)
	{
		return 0.0f;
	}
	if (T < In)
	{
		return static_cast<float>(T / In);
	}
	if (T < In + Hold)
	{
		return 1.0f;
	}
	return static_cast<float>(1.0 - (T - In - Hold) / Out);
}

void UEncounterGradeCameraModifier::ModifyPostProcess(float DeltaTime, float& PostProcessBlendWeight, FPostProcessSettings& PostProcessSettings)
{
	const APlayerController* Controller = CameraOwner != nullptr ? CameraOwner->GetOwningPlayerController() : nullptr;
	const ULocalPlayer* Local = Controller != nullptr ? Controller->GetLocalPlayer() : nullptr;
	const UEncounterViewSubsystem* View = Local != nullptr ? Local->GetSubsystem<UEncounterViewSubsystem>() : nullptr;
	const UWorld* World = Controller != nullptr ? Controller->GetWorld() : nullptr;
	const float Weight = View != nullptr && World != nullptr ? View->GetGradeWeight(World->GetTimeSeconds()) : 0.0f;
	PostProcessBlendWeight = Weight;
	if (Weight <= 0.0f)
	{
		return;
	}
	const UEncounterPresentationSettings* Settings = UEncounterPresentationSettings::Get();
	const bool bReduced = UEncounterViewSubsystem::IsReduceScreenEffects();
	PostProcessSettings.bOverride_WhiteTemp = true;
	PostProcessSettings.WhiteTemp = Settings->GradeWhiteTemp;
	PostProcessSettings.bOverride_ColorSaturation = true;
	const float Saturation = bReduced ? FMath::Max(Settings->GradeSaturation, Settings->ReducedGradeSaturation) : Settings->GradeSaturation;
	PostProcessSettings.ColorSaturation = FVector4(Saturation, Saturation, Saturation, 1.0f);
	PostProcessSettings.bOverride_ColorContrast = true;
	PostProcessSettings.ColorContrast = FVector4(Settings->GradeContrast, Settings->GradeContrast, Settings->GradeContrast, 1.0f);
	PostProcessSettings.bOverride_VignetteIntensity = true;
	PostProcessSettings.VignetteIntensity = bReduced ? 0.0f : Settings->GradeVignette;
}
