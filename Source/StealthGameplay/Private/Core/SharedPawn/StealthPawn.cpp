#include "View/Core/StealthPawn.h"
#include "View/Core/StealthMotionGate.h"

#include "Components/CapsuleComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameplayTasksComponent.h"
#include "MotionWarpingComponent.h"
#include "Net/UnrealNetwork.h"
#include "Server/Core/StealthPawnCommandComponent.h"
#include "View/Core/StealthMotionWarpingComponent.h"
#include "View/Core/StealthPawnMovementComponent.h"
#if WITH_STEALTH_TEST_SEAMS
#include "Server/Testing/StealthRelevancyDenySeam.h"
#include "Server/Visibility/StealthConcealmentCull.h"
#endif

const FName AStealthPawn::GameplayTasksComponentName(TEXT("GameplayTasks"));
const FName AStealthPawn::MotionWarpingComponentName(TEXT("MotionWarping"));
const FName AStealthPawn::StrikeWarpTargetName(TEXT("StrikeContact"));

#if WITH_STEALTH_TEST_SEAMS
bool StealthPawnTestSeams::bSkipRemoteStrikeWarpTarget = false;
#endif

namespace StealthPawnLocal
{
	/** This machine's estimate of server time (the server's own world time on the server). */
	double ServerNow(const UWorld* World)
	{
		const AGameStateBase* GameState = World != nullptr ? World->GetGameState() : nullptr;
		return GameState != nullptr ? GameState->GetServerWorldTimeSeconds() : (World != nullptr ? World->GetTimeSeconds() : 0.0);
	}
}

FStealthForcedNetUpdate::FStealthForcedNetUpdate(AStealthPawn& Pawn, EForcedUpdateReason Reason)
{
	// At most once per pawn per frame (ADR-0003 section 2): the first force already sends this frame's state.
	if (Pawn.LastForcedNetUpdateFrame == GFrameCounter)
	{
		return;
	}
	Pawn.LastForcedNetUpdateFrame = GFrameCounter;
	++Pawn.ForcedNetUpdateCount;
	Pawn.ForceNetUpdate();
}

AStealthPawn::AStealthPawn(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UStealthMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	// Facing follows movement for every pawn; the look path (HeadLook) never turns the body.
	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	// Only the factory creates and possesses pawns (ADR-0010 section 3): no engine auto-possession,
	// and no default AI controller class for anything to spawn.
	AutoPossessAI = EAutoPossessAI::Disabled;
	AutoPossessPlayer = EAutoReceiveInput::Disabled;
	AIControllerClass = nullptr;

	// Present on every pawn so AI possession finds one instead of adding a component (AAIController
	// otherwise creates it at OnPossess). Local bookkeeping only: never replicated.
	GameplayTasks = CreateDefaultSubobject<UGameplayTasksComponent>(GameplayTasksComponentName);
	GameplayTasks->SetIsReplicated(false);

	// ADR-0011 section 1: one non-replicated command component per pawn, never added per kind.
	PawnCommand = CreateDefaultSubobject<UPawnCommandComponent>(UPawnCommandComponent::ComponentName);

	SharedMotionWarping = CreateOptionalDefaultSubobject<UStealthMotionWarpingComponent>(MotionWarpingComponentName);

	// Feature components on every pawn (Concealment's UConcealmentComponent, ADR-0017 section 1): the same classes, order
	// and flags on every pawn, registered by the features without Core naming them (View/Core/StealthMotionGate.h).
	for (const StealthPawnFeatureComponents::FEntry& Entry : StealthPawnFeatureComponents::GetEntries())
	{
		if (UClass* Class = Entry.GetClass != nullptr ? Entry.GetClass() : nullptr)
		{
			CreateDefaultSubobject(Entry.Name, UActorComponent::StaticClass(), Class, /*bIsRequired*/ true, /*bIsTransient*/ false);
		}
	}

	// R17.1 Body: one capsule size for every pawn, the sample character's values recorded here.
	GetCapsuleComponent()->InitCapsuleSize(SampleCapsuleRadiusCm, SampleCapsuleHalfHeightCm);

	// ADR-0017 section 6 (Concealment story con-022 AC1): the three gating flags are class defaults of every pawn primitive,
	// never toggled at runtime, so a primitive hidden by SetVisibility(false, true) keeps no render proxy (no shadow, no
	// Lumen, no far-field ray tracing). The engine defaults are already false; they are pinned here against a Blueprint
	// child overriding them (audited by StealthGame.Pawn.RenderFlagParity).
	for (UPrimitiveComponent* Primitive : { static_cast<UPrimitiveComponent*>(GetMesh()), static_cast<UPrimitiveComponent*>(GetCapsuleComponent()) })
	{
		if (Primitive != nullptr)
		{
			Primitive->bCastHiddenShadow = false;
			Primitive->bAffectIndirectLightingWhileHidden = false;
			Primitive->bRayTracingFarField = false;
		}
	}
}

FStealthBodyConstants AStealthPawn::GetPublishedBodyConstants()
{
	const AStealthPawn* Defaults = GetDefault<AStealthPawn>();
	FStealthBodyConstants Out;
	Out.CapsuleRadiusCm = Defaults->GetCapsuleComponent()->GetUnscaledCapsuleRadius();
	Out.CapsuleHalfHeightCm = Defaults->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
	const UCharacterMovementComponent* Movement = Defaults->GetCharacterMovement();
	Out.MaxStepHeightCm = Movement->MaxStepHeight;
	Out.WalkableFloorAngleDeg = Movement->GetWalkableFloorAngle();
	return Out;
}

void AStealthPawn::Restart()
{
	// ADR-0010 E2: APawn::Restart would also StopMovementImmediately and consume the pending input
	// vector, and ACharacter::Restart would reset the movement mode. Keep only the eye height.
	RecalculateBaseEyeHeight();
	// A body that has never had a movement mode (a freshly spawned, first-possessed pawn) gets the default one, the
	// part of ACharacter::Restart a first possession needs; a body already moving keeps its mode, so a possession
	// change stays invisible in its motion (D53 integration fix, story iif-094: production input drove a MOVE_None body).
	// Only once per body: a body later disabled (MOVE_None on death) is never silently re-enabled by a re-possession.
	if (UCharacterMovementComponent* Movement = GetCharacterMovement(); Movement && Movement->MovementMode == MOVE_None && !bMovementModeInitialised)
	{
		Movement->SetDefaultMovementMode();
	}
	bMovementModeInitialised = true;
}

void AStealthPawn::PawnClientRestart()
{
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->ResetPredictionData_Client();
	}
	// Skip ACharacter::PawnClientRestart (it zeroes velocity); APawn's version calls Restart() above.
	APawn::PawnClientRestart();
}

void AStealthPawn::SpawnDefaultController()
{
	// Intentionally empty: UStealthPawnFactory possesses every pawn (ADR-0010).
}

UStealthMovementComponent* AStealthPawn::GetStealthMovement() const
{
	return CastChecked<UStealthMovementComponent>(GetCharacterMovement());
}

UPawnCommandComponent* AStealthPawn::GetPawnCommand() const
{
	return PawnCommand;
}

// ---- Encounter state (stories asn-012, asn-013, asn-014) -------------------------------------------------------------

void AStealthPawn::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	// Audience All, no condition (ADR-0018 sections 1.1, 1.2): the same rows on every pawn, NPC or player.
	DOREPLIFETIME(AStealthPawn, ActionState);
	DOREPLIFETIME(AStealthPawn, DeathState);
}

void AStealthPawn::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	// ADR-0018 section 7.2 / E5 (story asn-014): no strike or counter target may travel through motion warping. The Shared
	// Pawn's own component disables WarpTargets in its class (COND_Never); the sample Blueprint's component on the D53
	// prototype adapter replicates WarpTargets (COND_SimulatedOnly) and nothing else, so it is not replicated at all.
	// Simulated proxies do not run the sample's traversal (spike SCL-045: 6/204), so nothing of theirs reads it.
	TInlineComponentArray<UMotionWarpingComponent*> Warping(this);
	for (UMotionWarpingComponent* Component : Warping)
	{
		if (Component != nullptr && !Component->IsA<UStealthMotionWarpingComponent>())
		{
			Component->SetIsReplicated(false);
		}
	}
}

void AStealthPawn::PostNetInit()
{
	Super::PostNetInit();
	bNetInitialised = true;
	NetInitFrame = GFrameCounter;
	// ADR-0017 section 5 (Concealment story con-020): the body-hidden latch of a fresh actor is evaluated here, from the
	// full replicated state, in the same call that created it (before any frame renders or the S10 warm-up shows it).
	OnActionStateChanged.Broadcast(this);

	// ADR-0003 section 5.8 / ADR-0018 section 5.1 (story asn-022): a re-opened or late actor is presented from the state it
	// arrived with, through the same idempotent evaluation as every RepNotify (the first bunch's RepNotifies waited for this).
	if (HasDeathWrite() || ActionState.Pose != EActionPose::None)
	{
		OnAnyEncounterStateChanged().Broadcast(this);
	}
}

bool AStealthPawn::IsNetRelevantFor(const AActor* RealViewer, const AActor* ViewTarget, const FVector& SrcLocation) const
{
#if WITH_STEALTH_TEST_SEAMS
	if (StealthRelevancyDenySeam::IsDenied(this, RealViewer))
	{
		return false;
	}
#endif
	// ADR-0003 section 5 (IVP stories 024/025, Concealment story con-025): a culled concealed pawn is not relevant to a
	// non-ally viewer, and a re-open waits for the close's acknowledgement (the S10 guard). The same rule for every pawn kind.
	if (const UStealthConcealmentCull* Cull = UStealthConcealmentCull::Get(GetWorld()); Cull != nullptr && !Cull->IsRelevantTo(this, RealViewer))
	{
		return false;
	}
	return Super::IsNetRelevantFor(RealViewer, ViewTarget, SrcLocation);
}

bool AStealthPawn::IsPresentationReady() const
{
	return HasAuthority() || bNetInitialised || HasActorBegunPlay();
}

UMotionWarpingComponent* AStealthPawn::GetWarpingForStrike() const
{
	if (SharedMotionWarping != nullptr)
	{
		return SharedMotionWarping;
	}
	return FindComponentByClass<UMotionWarpingComponent>();
}

bool AStealthPawn::HasStrikeWarpTarget() const
{
	const UMotionWarpingComponent* Warping = GetWarpingForStrike();
	return Warping != nullptr && Warping->FindWarpTarget(StrikeWarpTargetName) != nullptr;
}

int32 AStealthPawn::CountWarpingThatCanReplicateTargets() const
{
	int32 Count = 0;
	TInlineComponentArray<UMotionWarpingComponent*> Warping(this);
	for (const UMotionWarpingComponent* Component : Warping)
	{
		Count += Component != nullptr && !Component->IsA<UStealthMotionWarpingComponent>() && Component->GetIsReplicated() ? 1 : 0;
	}
	return Count;
}

EStealthActionPose AStealthPawn::ToSclActionPose(EActionPose Pose)
{
	switch (Pose)
	{
	case EActionPose::Strike: return EStealthActionPose::Strike;
	case EActionPose::Brace: return EStealthActionPose::Brace;
	case EActionPose::Counter: return EStealthActionPose::Counter;
	case EActionPose::Interact: return EStealthActionPose::InteractPose;
	case EActionPose::BushEnter: return EStealthActionPose::BushEnter;
	case EActionPose::BushExit: return EStealthActionPose::BushExit;
	default: return EStealthActionPose::None;
	}
}

double AStealthPawn::GetDeathFreezeServerTime(double Now) const
{
	// The freeze never interrupts a lunge (ADR-0018 section 3.2, closeout 2026-10-25): it waits for this pawn's contact.
	if (ActionState.Pose == EActionPose::Strike && ActionState.ContactServerTime > Now)
	{
		return ActionState.ContactServerTime;
	}
	return Now;
}

bool AStealthPawn::ApplyDeathWrite(const FDeathState& State)
{
	if (!HasAuthority() || HasDeathWrite() || State.DeathServerTime < 0.0)
	{
		return false;
	}
	{
		FStealthForcedNetUpdate Forced(*this, EForcedUpdateReason::DeathWrite);
		DeathState = State;
	}
#if WITH_STEALTH_TEST_SEAMS
	// Story asn-022: the Visibility Policy's death-write re-open (ADR-0018 section 3.2 step 3, ADR-0003 section 5.8) has no
	// production cull to undo in the MVP (SCL R13); the harness's deny seam stands for it and re-opens here, in this step.
	StealthRelevancyDenySeam::NotifyDeathWrite(this);
#endif
	HandleDeathStateChanged(); // the server (and the listen host's view) present it directly (ADR-0018 E7)
	return true;
}

void AStealthPawn::HandleActionStateChanged()
{
	if (UStealthMovementComponent* Movement = GetStealthMovement())
	{
		Movement->SetActionPose(ToSclActionPose(ActionState.Pose));
	}
	if (ActionState.Pose == EActionPose::Strike && ActionState.Seq != StrikeWarpSeq)
	{
		StrikeWarpSeq = ActionState.Seq;
		bool bSet = true;
#if WITH_STEALTH_TEST_SEAMS
		bSet = HasAuthority() || !StealthPawnTestSeams::bSkipRemoteStrikeWarpTarget;
#endif
		if (UMotionWarpingComponent* Warping = GetWarpingForStrike(); Warping != nullptr && bSet)
		{
			// Computed once from this machine's capsule and the replicated offset; the victim is never re-tracked.
			Warping->AddOrUpdateWarpTargetFromLocation(StrikeWarpTargetName, GetActorLocation() + ActionState.WarpPoint.ToOffsetCm());
		}
	}
	OnActionStateChanged.Broadcast(this);
	OnAnyEncounterStateChanged().Broadcast(this);
}

void AStealthPawn::HandleDeathStateChanged()
{
	if (HasDeathWrite())
	{
		if (UStealthMovementComponent* Movement = GetStealthMovement())
		{
			Movement->FreezeCommandedMotionFrom(GetDeathFreezeServerTime(StealthPawnLocal::ServerNow(GetWorld())));
		}
	}
	OnDeathStateChanged.Broadcast(this);
	OnAnyEncounterStateChanged().Broadcast(this);
}

AStealthPawn::FOnEncounterStateChanged& AStealthPawn::OnAnyEncounterStateChanged()
{
	static FOnEncounterStateChanged Delegate;
	return Delegate;
}

void AStealthPawn::OnRep_ActionState()
{
	HandleActionStateChanged();
}

void AStealthPawn::OnRep_DeathState()
{
	HandleDeathStateChanged();
}
