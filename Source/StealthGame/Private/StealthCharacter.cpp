#include "StealthCharacter.h"
#include "StealthBush.h"
#include "StealthGame.h"
#include "StealthGameMode.h"
#include "StealthMovementComponent.h"
#include "StealthPlayerState.h"
#include "StealthRules.h"
#include "StealthSettings.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/DataBunch.h"
#include "Net/UnrealNetwork.h"
#include "Server/Roster/StealthRosterStore.h"

#if !UE_BUILD_SHIPPING
#include "Spikes/StealthIdentitySpike.h" // THROWAWAY SPIKE hook (ADR-0001 S1/S2/S14, 2026-10-25)
#include "Server/Spikes/StealthSpikeHooks.h" // THROWAWAY SPIKE hook (IVP S9, 2026-10-25)
#endif

namespace
{
	/** Server-synchronised time, so client predictions use the same clock as server rules. */
	double StealthServerNow(const UWorld* World)
	{
		if (!World)
		{
			return 0.0;
		}
		if (const AGameStateBase* GameState = World->GetGameState())
		{
			return GameState->GetServerWorldTimeSeconds();
		}
		return World->GetTimeSeconds();
	}

	/** Seconds the client waits for the server to confirm a defence before undoing its predicted stance. */
	constexpr float DefenceConfirmTimeout = 0.6f;
}

AStealthCharacter::AStealthCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UStealthPrototypeMovementComponent>(ACharacter::CharacterMovementComponentName)
		// D53 adapter: the sample Blueprint still owns an SCS component named MotionWarping (ADR-0010 Blueprint cleanup pending).
		.DoNotCreateDefaultSubobject(AStealthPawn::MotionWarpingComponentName))
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(GetMesh(), FirstPersonSocket);
	FirstPersonCamera->bUsePawnControlRotation = true;
	FirstPersonCamera->bAutoActivate = false;

}

void AStealthCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AStealthCharacter, AppearanceIndex);
	DOREPLIFETIME(AStealthCharacter, bDying);
	DOREPLIFETIME(AStealthCharacter, bPreparingDefence);
	DOREPLIFETIME(AStealthCharacter, bConcealed);
	DOREPLIFETIME(AStealthCharacter, bRevealedUntilExit);
	DOREPLIFETIME(AStealthCharacter, HeldUntilServerTime);

#if !UE_BUILD_SHIPPING
	StealthSpike::ApplyPawnTrims(StaticClass(), OutLifetimeProps); // SPIKE: no-op unless stealth.Spike.IdentityTrims is set
#endif
}

void AStealthCharacter::BeginPlay()
{
	Super::BeginPlay();
	ApplyAppearance();
}

void AStealthCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority())
	{
		if (URosterStore* Roster = URosterStore::Get(GetWorld()); Roster && Roster->IsMatchOpen())
		{
			Roster->ClearBinding(this, EndPlayReason);
		}
	}
	Super::EndPlay(EndPlayReason);
}

void AStealthCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const double Now = StealthServerNow(GetWorld());

	if (HasAuthority() && bPreparingDefence)
	{
		const UStealthSettings* Settings = UStealthSettings::Get();
		if (FStealthRules::ShouldEndDefence(IsRunning(), IsInTraversal(), Now, DefenceActivationTime, Settings->DefenceDuration))
		{
			StopDefence();
		}
	}

	if (GetNetMode() != NM_DedicatedServer)
	{
		UpdateConcealmentVisibility();
	}
}

void AStealthCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// ADR-0011 section 9 (story iif-094): gameplay input lives on AStealthPlayerController, whose IMC_Common/IMC_Active
	// contexts (priority above the sample's IMC_Sandbox) consume Move, Look, Ctrl/Alt, E, Q, LMB, F, G and V. The
	// prototype's legacy key binds (LMB/F attack, hold Q, V camera, T ping, the sample's IA_Interact on E) are retired.
	// Super still binds the reparented sample Blueprint's own input events, which keep the sample-only actions
	// (crouch, strafe, aim, and sprint and jump/traversal through the pass-through IA_Run / IA_Traverse) until the
	// SCL speed-tier and traversal stories replace them.
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

bool AStealthCharacter::IsNetRelevantFor(const AActor* RealViewer, const AActor* ViewTarget, const FVector& SrcLocation) const
{
#if !UE_BUILD_SHIPPING
	// SPIKE (IVP S9): the harness may replace relevancy entirely; no-op unless a scenario installs it.
	if (const StealthSpikeHooks::FRelevancyOverride& Override = StealthSpikeHooks::GetRelevancyOverride())
	{
		const TOptional<bool> Decision = Override(this, RealViewer, ViewTarget);
		if (Decision.IsSet())
		{
			return Decision.GetValue();
		}
	}
#endif
	return Super::IsNetRelevantFor(RealViewer, ViewTarget, SrcLocation);
}

void AStealthCharacter::OnActorChannelOpen(FInBunch& InBunch, UNetConnection* Connection)
{
	Super::OnActorChannelOpen(InBunch, Connection);
#if !UE_BUILD_SHIPPING
	// SPIKE (IVP S10 re-run): no-op unless the harness installs it.
	if (const StealthSpikeHooks::FChannelOpenHook& Hook = StealthSpikeHooks::GetChannelOpenHook())
	{
		Hook(this, InBunch.ChIndex, InBunch.PacketId, Connection);
	}
#endif
}

void AStealthCharacter::PostNetInit()
{
	Super::PostNetInit();
#if !UE_BUILD_SHIPPING
	// SPIKE (IVP S10 re-run): no-op unless the harness installs it.
	if (const StealthSpikeHooks::FPostNetInitHook& Hook = StealthSpikeHooks::GetPostNetInitHook())
	{
		Hook(this);
	}
#endif
}

bool AStealthCharacter::CanJumpInternal_Implementation() const
{
	if (IsInformant() || bPreparingDefence || IsHeldInPlace() || bDying)
	{
		return false;
	}
	return Super::CanJumpInternal_Implementation();
}

// ---- Queries -----------------------------------------------------------------------------

AStealthPlayerState* AStealthCharacter::GetStealthPlayerState() const
{
	return GetPlayerState<AStealthPlayerState>();
}

bool AStealthCharacter::IsNpc() const
{
	return GetStealthPlayerState() == nullptr;
}

uint8 AStealthCharacter::GetTeam() const
{
	const AStealthPlayerState* PS = GetStealthPlayerState();
	return PS ? PS->Team : STEALTH_NO_TEAM;
}

bool AStealthCharacter::IsInformant() const
{
	const AStealthPlayerState* PS = GetStealthPlayerState();
	return PS && PS->IsInformant();
}

bool AStealthCharacter::IsActiveAgent() const
{
	const AStealthPlayerState* PS = GetStealthPlayerState();
	return PS && PS->IsActivePlayer() && !bDying;
}

bool AStealthCharacter::IsRunning() const
{
	return FStealthRules::IsRunningSpeed(GetVelocity().Size2D(), UStealthSettings::Get()->RunSpeedThreshold);
}

bool AStealthCharacter::IsInTraversal() const
{
	const UCharacterMovementComponent* Movement = GetCharacterMovement();
	return Movement && (Movement->IsFlying() || Movement->IsFalling());
}

bool AStealthCharacter::IsHeldInPlace() const
{
	return HeldUntilServerTime > 0.0 && StealthServerNow(GetWorld()) < HeldUntilServerTime;
}

float AStealthCharacter::GetLocalInteractProgress() const
{
	if (!bLocalInteracting || !GetWorld())
	{
		return -1.f;
	}
	const float Elapsed = static_cast<float>(GetWorld()->GetTimeSeconds() - LocalInteractStartTime);
	return FMath::Clamp(Elapsed / UStealthSettings::Get()->ObjectiveInteractTime, 0.f, 1.f);
}

bool AStealthCharacter::IsTeammateOfLocalViewer() const
{
	if (IsNpc() || GetTeam() == STEALTH_NO_TEAM)
	{
		return false;
	}
	const APlayerController* LocalPC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	const AStealthPlayerState* LocalPS = LocalPC ? LocalPC->GetPlayerState<AStealthPlayerState>() : nullptr;
	return LocalPS && LocalPS->Team == GetTeam();
}

// ---- Server-side mutation ------------------------------------------------------------------

void AStealthCharacter::KillCharacter()
{
	if (!HasAuthority() || bDying)
	{
		return;
	}
	bDying = true;
	StopDefence();
	ApplyDeathPresentation();

	if (AController* OwningController = GetController())
	{
		const bool bIsPlayer = OwningController->IsPlayerController();
		OwningController->UnPossess();
		if (!bIsPlayer)
		{
			OwningController->Destroy();
		}
	}
	SetLifeSpan(UStealthSettings::Get()->BodyLifetime);
	ForceNetUpdate();
}

void AStealthCharacter::StartDefence()
{
	if (!HasAuthority())
	{
		return;
	}
	const double Now = StealthServerNow(GetWorld());
	bPreparingDefence = true;
	DefenceActivationTime = Now;
	if (AStealthPlayerState* PS = GetStealthPlayerState())
	{
		PS->LastDefenceActivationTime = Now;
	}
	SetDefenceStance(true);
	ForceNetUpdate();
}

void AStealthCharacter::StopDefence()
{
	if (!HasAuthority() || !bPreparingDefence)
	{
		return;
	}
	bPreparingDefence = false;
	SetDefenceStance(false);
	ForceNetUpdate();
}

void AStealthCharacter::HoldInPlace(float Duration)
{
	if (HasAuthority())
	{
		HeldUntilServerTime = StealthServerNow(GetWorld()) + Duration;
		ForceNetUpdate();
	}
}

void AStealthCharacter::SetInBush(AStealthBush* Bush, bool bInside)
{
	if (!HasAuthority())
	{
		return;
	}
	OverlappingBushes.RemoveAll([](const TWeakObjectPtr<AStealthBush>& Entry) { return !Entry.IsValid(); });
	if (bInside)
	{
		OverlappingBushes.AddUnique(Bush);
	}
	else
	{
		OverlappingBushes.Remove(Bush);
	}
	bConcealed = OverlappingBushes.Num() > 0;
	if (!bConcealed)
	{
		bRevealedUntilExit = false;
	}
}

void AStealthCharacter::RevealFromConcealment()
{
	if (HasAuthority() && bConcealed)
	{
		bRevealedUntilExit = true;
		ForceNetUpdate();
	}
}

void AStealthCharacter::SetAppearanceIndex(int32 NewIndex)
{
	if (HasAuthority())
	{
		AppearanceIndex = NewIndex;
		ApplyAppearance();
	}
}

void AStealthCharacter::CancelTraversal()
{
	StopAnimMontage();

	// The sample tracks traversal in a Blueprint variable; clear it so the character is not stuck.
	if (FBoolProperty* DoingTraversal = FindFProperty<FBoolProperty>(GetClass(), TEXT("DoingTraversalAction")))
	{
		DoingTraversal->SetPropertyValue_InContainer(this, false);
	}
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->ClearMoveIgnoreComponents();
	}
}

// ---- Presentation -----------------------------------------------------------------------------

void AStealthCharacter::ApplyAppearance()
{
	const UStealthSettings* Settings = UStealthSettings::Get();
	USkeletalMeshComponent* MeshComponent = GetMesh();
	if (!MeshComponent)
	{
		return;
	}

	// Model pool: swap the model (all pool models share the sample's skeleton, so the same animation drives them).
	if (Settings->AppearanceMeshes.IsValidIndex(AppearanceIndex))
	{
		if (USkeletalMesh* PoolMesh = Settings->AppearanceMeshes[AppearanceIndex].LoadSynchronous())
		{
			MeshComponent->SetSkeletalMeshAsset(PoolMesh);
			MeshComponent->EmptyOverrideMaterials();
		}
		return;
	}

	if (!Settings->AppearancePalette.IsValidIndex(AppearanceIndex))
	{
		return;
	}
	const FLinearColor Tint = Settings->AppearancePalette[AppearanceIndex];
	UMaterialInterface* PoolMaterial = Settings->AppearanceMaterial.LoadSynchronous();
	for (int32 MaterialIndex = 0; MaterialIndex < MeshComponent->GetNumMaterials(); ++MaterialIndex)
	{
		UMaterialInstanceDynamic* Dynamic = PoolMaterial
			? MeshComponent->CreateDynamicMaterialInstance(MaterialIndex, PoolMaterial)
			: MeshComponent->CreateAndSetMaterialInstanceDynamic(MaterialIndex);
		if (!Dynamic)
		{
			continue;
		}
		if (PoolMaterial)
		{
			// The pool material is authored for this: its tint parameter is the first configured name.
			Dynamic->SetVectorParameterValue(Settings->AppearanceColorParameters.IsEmpty() ? FName(TEXT("Tint")) : Settings->AppearanceColorParameters[0], Tint);
			continue;
		}
		for (const FName& Parameter : Settings->AppearanceColorParameters)
		{
			FLinearColor Existing;
			if (Dynamic->GetVectorParameterValue(FHashedMaterialParameterInfo(Parameter), Existing))
			{
				Dynamic->SetVectorParameterValue(Parameter, Tint);
				break;
			}
		}
	}
}

void AStealthCharacter::ApplyDeathPresentation()
{
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->DisableMovement();
	}
	StopAnimMontage();

	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (USkeletalMeshComponent* MeshComponent = GetMesh())
	{
		// A body is always visible: the kill itself is the tell, even from a bush.
		MeshComponent->SetVisibility(true, true);
		MeshComponent->SetCollisionProfileName(TEXT("Ragdoll"));
		MeshComponent->SetAllBodiesSimulatePhysics(true);
		MeshComponent->SetSimulatePhysics(true);
		MeshComponent->WakeAllRigidBodies();
	}
}

void AStealthCharacter::SetDefenceStance(bool bPrepared)
{
	// Placeholder "brace" pose: the sample's crouch. Preparing a defence is a readable deviation (Pillar 2).
	if (bPrepared)
	{
		Crouch();
	}
	else
	{
		UnCrouch();
	}
}

void AStealthCharacter::UpdateConcealmentVisibility()
{
	if (bDying)
	{
		return;
	}
	const bool bShouldHide = IsConcealed() && !IsLocallyControlled() && !IsTeammateOfLocalViewer();
	if (bShouldHide != bHiddenForLocalViewer)
	{
		bHiddenForLocalViewer = bShouldHide;
		if (USkeletalMeshComponent* MeshComponent = GetMesh())
		{
			MeshComponent->SetVisibility(!bShouldHide, true);
		}
	}
}

void AStealthCharacter::OnRep_AppearanceIndex()
{
	ApplyAppearance();
}

void AStealthCharacter::OnRep_Dying()
{
	if (bDying)
	{
		ApplyDeathPresentation();
	}
}

void AStealthCharacter::OnRep_PreparingDefence()
{
	if (IsLocallyControlled() && !bPreparingDefence)
	{
		SetDefenceStance(false);
	}
}

// ---- Camera ---------------------------------------------------------------------------------

void AStealthCharacter::ToggleCameraMode()
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC || !IsLocallyControlled() || bDying)
	{
		return;
	}
	bFirstPerson = !bFirstPerson;

	TInlineComponentArray<UActorComponent*> Components;
	GetComponents(Components);
	UActorComponent* SampleCamera = nullptr;
	for (UActorComponent* Component : Components)
	{
		if (Component && Component != FirstPersonCamera && Component->GetClass()->GetName().Contains(TEXT("GameplayCamera")))
		{
			SampleCamera = Component;
			break;
		}
	}

	if (bFirstPerson)
	{
		if (SampleCamera)
		{
			SampleCamera->Deactivate();
		}
		FirstPersonCamera->Activate();
		PC->SetViewTarget(this);
		GetMesh()->HideBoneByName(FirstPersonSocket, PBO_None);
	}
	else
	{
		FirstPersonCamera->Deactivate();
		GetMesh()->UnHideBoneByName(FirstPersonSocket);
		if (SampleCamera)
		{
			SampleCamera->Activate();
		}
		// Re-run the sample's own camera activation (CBP_SandboxCharacter::SetupCamera).
		if (UFunction* SetupCamera = FindFunction(TEXT("SetupCamera")))
		{
			ProcessEvent(SetupCamera, nullptr);
		}
		else
		{
			PC->SetViewTarget(this);
		}
	}
}

// ---- Input ------------------------------------------------------------------------------------

AStealthCharacter* AStealthCharacter::FindAssassinationTarget() const
{
	const UStealthSettings* Settings = UStealthSettings::Get();
	const FVector Origin = GetActorLocation();
	const FVector ViewDir = FRotator(0.f, GetControlRotation().Yaw, 0.f).Vector();
	const float CosLimit = FMath::Cos(FMath::DegreesToRadians(Settings->AssassinationConeHalfAngle));

	AStealthCharacter* Best = nullptr;
	float BestScore = TNumericLimits<float>::Max();
	for (TActorIterator<AStealthCharacter> It(GetWorld()); It; ++It)
	{
		AStealthCharacter* Other = *It;
		if (Other == this || Other->bDying || Other->bHiddenForLocalViewer || Other->IsTeammateOfLocalViewer())
		{
			continue;
		}
		const FVector ToOther = Other->GetActorLocation() - Origin;
		const float Distance = ToOther.Size2D();
		if (Distance > Settings->AssassinationRange || Distance < KINDA_SMALL_NUMBER)
		{
			continue;
		}
		const float Cos = FVector::DotProduct(ViewDir, ToOther.GetSafeNormal2D());
		if (Cos < CosLimit)
		{
			continue;
		}
		const float Score = Distance * (2.f - Cos);
		if (Score < BestScore)
		{
			BestScore = Score;
			Best = Other;
		}
	}
	return Best;
}

void AStealthCharacter::HandleOwnerCommandPredicted(ECommandKind Kind)
{
	// Presentation-only owner prediction (D53 adapter): the server decides; OnRep_PreparingDefence and the confirmation
	// timeout undo a wrong guess, as before.
	switch (Kind)
	{
	case ECommandKind::PrepareBegin:
	{
		const AStealthPlayerState* PS = GetStealthPlayerState();
		const UStealthSettings* Settings = UStealthSettings::Get();
		const double Now = StealthServerNow(GetWorld());
		if (!PS || !IsActiveAgent()
			|| !FStealthRules::CanStartDefence(true, IsRunning(), IsInTraversal(), Now, PS->LastDefenceActivationTime, Settings->DefenceCooldown))
		{
			return;
		}
		SetDefenceStance(true);
		TWeakObjectPtr<AStealthCharacter> WeakThis(this);
		FTimerHandle Handle;
		GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([WeakThis]()
		{
			if (AStealthCharacter* Self = WeakThis.Get())
			{
				if (!Self->bPreparingDefence)
				{
					Self->SetDefenceStance(false);
				}
			}
		}), DefenceConfirmTimeout, false);
		break;
	}
	case ECommandKind::PrepareEnd:
		if (bPreparingDefence || bIsCrouched)
		{
			SetDefenceStance(false);
		}
		break;
	case ECommandKind::InteractBegin:
		if (IsActiveAgent())
		{
			bLocalInteracting = true;
			LocalInteractStartTime = GetWorld()->GetTimeSeconds();
		}
		break;
	case ECommandKind::InteractEnd:
		bLocalInteracting = false;
		break;
	default:
		break;
	}
}
