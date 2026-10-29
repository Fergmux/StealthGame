// Stories con-003 and con-008: the per-pawn concealment component (server mirror, motion gate). See
// View/ConcealmentComponent.h.

#include "View/ConcealmentComponent.h"
#include "View/AllyBodyHidden.h"

#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "View/Core/StealthPawn.h"
#include "View/Core/StealthPawnMovementComponent.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/RootMotionSource.h"
#include "HAL/IConsoleManager.h"
#include "View/BushRuntimeSubsystem.h"
#include "View/BodyHiddenLatch.h"
#include "View/StealthRosterTeamView.h"
#include "View/StealthRosterTypes.h"
#include "Components/AudioComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"

namespace ConcealmentComponentLocal
{
	/** Stealth.Concealment.OwnerWarp: 0 server only, 1 a1 (on FActionState arrival), 2 b (predict on press; the verdict). */
	TAutoConsoleVariable<int32> CVarOwnerWarp(TEXT("Stealth.Concealment.OwnerWarp"), 2,
		TEXT("Concealment owner-side warp (spike S-CON-1): 0 server only, 1 start on FActionState arrival, 2 predict on press (default)."));

	/** Every AStealthPawn creates this component (Core reads the registry in the pawn constructor). */
	const StealthPawnFeatureComponents::FRegistrar Registrar(TEXT("Concealment"), &UConcealmentComponent::StaticClass);

	/** Assassination's AttackAttemptLockoutSec (public key): an emerging striker's recovery, the end of its inside window. */
	double EmergeEndSec()
	{
		return static_cast<double>(FAssassinationTuning_Public().AttackAttemptLockoutSec);
	}
}

const FName UConcealmentComponent::ComponentName(TEXT("Concealment"));

UConcealmentComponent::UConcealmentComponent()
{
	SetIsReplicatedByDefault(false);
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

UConcealmentComponent* UConcealmentComponent::Find(const AActor* Pawn)
{
	return Pawn != nullptr ? Pawn->FindComponentByClass<UConcealmentComponent>() : nullptr;
}

void UConcealmentComponent::OnRegister()
{
	Super::OnRegister();
	// The motion gate is registered here, not at BeginPlay: a move can run before BeginPlay (and a world without a game
	// mode never begins play), and the gate must hold for every move the pawn makes.
	if (AStealthPawn* Pawn = GetOwner<AStealthPawn>())
	{
		if (UStealthMovementComponent* Movement = Pawn->GetStealthMovement())
		{
			Movement->SetMotionGate(this);
		}
	}
	// ADR-0017 section 5, E8: every pawn actor a CLIENT creates starts presentation-hidden, before any RepNotify can run;
	// it is first evaluated from its full replicated state once it is net-initialised (TickComponent). The authority's own
	// pawns (the listen host's, the server's) are not network-created and start shown.
	const UWorld* World = GetWorld();
	if (World != nullptr && World->IsGameWorld() && World->GetNetMode() == NM_Client)
	{
		bPresentationHidden = true;
		bFreshPending = true;
	}
}

void UConcealmentComponent::BeginPlay()
{
	Super::BeginPlay();
	if (AStealthPawn* Pawn = GetOwner<AStealthPawn>())
	{
		ActionStateHandle = Pawn->OnActionStateChanged.AddUObject(this, &UConcealmentComponent::OnPawnActionStateChanged);
		HandleActionState(Pawn->GetActionState());
		// A fresh client actor is evaluated at its PostNetInit (AStealthPawn broadcasts OnActionStateChanged there) - in the
		// same call that runs this BeginPlay, before any frame renders - so nothing is toggled here; an actor that begins play
		// already net-initialised or locally spawned is evaluated now.
		if (Pawn->IsPresentationReady() || Pawn->HasAuthority())
		{
			RefreshPresentation(ServerNow());
		}
	}
}

void UConcealmentComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AStealthPawn* Pawn = GetOwner<AStealthPawn>())
	{
		Pawn->OnActionStateChanged.Remove(ActionStateHandle);
	}
	Super::EndPlay(EndPlayReason);
}

void UConcealmentComponent::OnUnregister()
{
	if (AStealthPawn* Pawn = GetOwner<AStealthPawn>())
	{
		if (UStealthMovementComponent* Movement = Pawn->GetStealthMovement(); Movement != nullptr && Movement->GetMotionGate() == this)
		{
			Movement->SetMotionGate(nullptr);
		}
	}
	Super::OnUnregister();
}

void UConcealmentComponent::OnPawnActionStateChanged(AStealthPawn* Pawn)
{
	if (Pawn != nullptr)
	{
		HandleActionState(Pawn->GetActionState());
		RefreshPresentation(ServerNow());
	}
}

void UConcealmentComponent::ReadAllyEvidence(bool& bOutAllyConcealed, bool& bOutUnresolvedEntry) const
{
	bOutAllyConcealed = false;
	bOutUnresolvedEntry = false;
	const UWorld* World = GetWorld();
	const APlayerController* Local = World != nullptr ? World->GetFirstPlayerController() : nullptr; // LOCAL-PAWN-OK: this machine's viewer
	const URosterTeamViewComponent* TeamView = Local != nullptr ? Local->FindComponentByClass<URosterTeamViewComponent>() : nullptr;
	if (TeamView == nullptr)
	{
		return;
	}
	const AActor* Self = GetOwner();
	for (const FTeamViewTeammateEntry& Entry : TeamView->GetView().Teammates)
	{
		const bool bLiving = Entry.State == static_cast<uint8>(EParticipantState::Active) || Entry.State == static_cast<uint8>(EParticipantState::Informant);
		if (Entry.PawnRef == Self)
		{
			bOutAllyConcealed = Entry.bIsConcealed;
		}
		else if (bLiving && Entry.bIsConcealed && Entry.PawnRef == nullptr)
		{
			bOutUnresolvedEntry = true;
		}
	}
}

void UConcealmentComponent::RefreshPresentation(double EstimatedNow)
{
	AStealthPawn* Pawn = GetOwner<AStealthPawn>();
	if (Pawn == nullptr)
	{
		return;
	}
	const UWorld* World = GetWorld();
	if (World == nullptr || World->GetNetMode() == NM_DedicatedServer)
	{
		return; // nothing is drawn on a dedicated server
	}
	// RepNotifies of a new actor's first bunch arrive before it is net-initialised: they may set the latch, never clear it.
	const bool bReady = Pawn->IsPresentationReady();
	bool bAlly = false;
	bool bUnresolved = false;
	ReadAllyEvidence(bAlly, bUnresolved);
	FBodyHiddenInputs In;
	In.Action = Pawn->GetActionState();
	In.Death = Pawn->GetDeathState();
	In.bAllyIsConcealed = bAlly;
	In.bIsOwner = Pawn->IsLocallyControlled();
	In.Now = EstimatedNow;
	In.bWasHidden = bPresentationHidden;
	In.bFresh = bFreshPending && bReady;
	bool bHidden = EvaluateBodyHidden(In, Tuning);
	if (bFreshPending)
	{
		if (!bReady)
		{
			bHidden = true; // hidden until the full state is in place
		}
		else if (!bHidden && bUnresolved && !In.bIsOwner)
		{
			// ADR-0017 section 5, unresolved ally evidence: while a CONCEALED living teammate entry's PawnRef is unresolved
			// this fresh actor may be that teammate: it stays hidden until the reference resolves (bounded: the next view
			// update; a 2 s safety bound keeps a stale entry from hiding a body for good). An unresolved teammate that is not
			// concealed (an informant waiting to respawn has no pawn at all) is no reason to hold anything: it would be shown.
			FreshHeldSince = FreshHeldSince < 0.0 ? EstimatedNow : FreshHeldSince;
			bHidden = EstimatedNow - FreshHeldSince < 2.0;
			bFreshPending = bHidden;
		}
		else
		{
			bFreshPending = false;
		}
	}
	bPresentationHidden = bHidden;
	ApplyHidden(bHidden);
}

void UConcealmentComponent::ApplyHidden(bool bHidden)
{
	if (bHidden == bAppliedHidden)
	{
		return;
	}
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (Character == nullptr)
	{
		return;
	}
	bAppliedHidden = bHidden;
	++PresentationWrites;
	// ADR-0017 section 6: the mesh and every attached primitive (body, hair, garments, props) lose their render proxy; the
	// three gating flags are class defaults (never toggled here), so no shadow, Lumen, ray-traced or reflection presence
	// remains. The capsule and the mesh stop answering the client-local presentation aim channel.
	if (USkeletalMeshComponent* Mesh = Character->GetMesh())
	{
		Mesh->SetVisibility(!bHidden, /*bPropagateToChildren*/ true);
		// Pose freshness: a hidden mesh keeps evaluating its pose, so its first visible frame is the seeked pose.
		// Restored, not overwritten, at the show: the S10 warm-up of a fresh actor may hold the option meanwhile.
		if (bHidden)
		{
			PreHideTickOption = Mesh->VisibilityBasedAnimTickOption;
			Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		}
		else
		{
			Mesh->VisibilityBasedAnimTickOption = PreHideTickOption;
		}
		Mesh->SetCollisionResponseToChannel(ConcealmentCollision::PresentationAimChannel, bHidden ? ECR_Ignore : ECR_Block);
		// Story con-022 AC1: no footstep decal, foley, notify-spawned Niagara or sound while hidden. Every one of those is an
		// anim notify on the sample character; the notifies are presentation only (the server keys govern), so suppressing
		// their dispatch on the hidden mesh removes them all at once.
		Mesh->bSuppressNotifyEventDispatch = bHidden;
	}
	// ... and no attached audio emitter plays while hidden.
	TInlineComponentArray<UAudioComponent*> Emitters(Character);
	for (UAudioComponent* Emitter : Emitters)
	{
		if (Emitter != nullptr)
		{
			Emitter->SetPaused(bHidden);
		}
	}
	TInlineComponentArray<UPrimitiveComponent*> Primitives(Character);
	for (UPrimitiveComponent* Primitive : Primitives)
	{
		if (Primitive == nullptr || Primitive == Character->GetCapsuleComponent())
		{
			continue;
		}
		if (Primitive->GetAttachParent() == Character->GetRootComponent() && Primitive != Character->GetMesh())
		{
			Primitive->SetVisibility(!bHidden, true); // root-attached props
		}
	}
	if (UPrimitiveComponent* Capsule = Character->GetCapsuleComponent())
	{
		Capsule->SetCollisionResponseToChannel(ConcealmentCollision::PresentationAimChannel, bHidden ? ECR_Ignore : ECR_Block);
	}
}

EConcealOwnerWarp UConcealmentComponent::GetOwnerWarpMode()
{
	return static_cast<EConcealOwnerWarp>(FMath::Clamp(ConcealmentComponentLocal::CVarOwnerWarp.GetValueOnGameThread(), 0, 2));
}

double UConcealmentComponent::RoundTripSec() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	const APlayerState* State = Pawn != nullptr ? Pawn->GetPlayerState() : nullptr; // LOCAL-PAWN-OK: the owner connection's own ping
	const double PingMs = State != nullptr ? static_cast<double>(State->GetPingInMilliseconds()) : 0.0;
	return FMath::Clamp(PingMs > 0.0 ? PingMs / 1000.0 : 0.25, 0.05, 0.8);
}

void UConcealmentComponent::StartLocalWarp(const FVector& Target, double DurationSec, bool bEnter, double ElapsedSec)
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	UCharacterMovementComponent* Movement = Character != nullptr ? Character->GetCharacterMovement() : nullptr;
	if (Movement == nullptr)
	{
		return;
	}
	// The identical source: same InstanceName, parameters and fixed target as the server's (ConcealmentService StartWarp).
	Movement->RemoveRootMotionSource(TEXT("ConcealEnter"));
	Movement->RemoveRootMotionSource(TEXT("ConcealExit"));
	TSharedPtr<FRootMotionSource_MoveToForce> Source = MakeShared<FRootMotionSource_MoveToForce>();
	Source->InstanceName = bEnter ? TEXT("ConcealEnter") : TEXT("ConcealExit");
	Source->AccumulateMode = ERootMotionAccumulateMode::Override;
	Source->Priority = 900;
	Source->StartLocation = Character->GetActorLocation();
	Source->TargetLocation = Target;
	Source->Duration = static_cast<float>(FMath::Max(DurationSec - ElapsedSec, 0.05));
	Source->bRestrictSpeedToExpected = false;
	Source->FinishVelocityParams.Mode = ERootMotionFinishVelocityMode::SetVelocity;
	Source->FinishVelocityParams.SetVelocity = FVector::ZeroVector;
	Movement->ApplyRootMotionSource(Source);
}

void UConcealmentComponent::OnOwnerHidePressed(ECommandKind Kind, uint16 BushId, uint8 Side)
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (GetOwnerWarpMode() != EConcealOwnerWarp::PredictOnPress || Character == nullptr || Character->HasAuthority() || !Character->IsLocallyControlled())
	{
		return;
	}
	const UBushRuntimeSubsystem* Bushes = UBushRuntimeSubsystem::Get(GetWorld());
	const double Now = ServerNow();
	const double HalfHeight = Character->GetSimpleCollisionHalfHeight();
	const double Radius = Character->GetSimpleCollisionRadius();
	double Duration = 0.0;
	FVector Target;
	if (Kind == ECommandKind::HideEnter)
	{
		const FBushRuntime* Bush = Bushes != nullptr ? Bushes->Find(BushId) : nullptr;
		if (Bush == nullptr)
		{
			return; // nothing in reach: the server answers OutOfReach, nothing to predict
		}
		Target = Bush->Anchor + FVector(0.0, 0.0, HalfHeight);
		Duration = Tuning.BushEnterSwapSec;
		OwnerPredictedUntil = Now + Tuning.BushEnterDurSec + RoundTripSec();
	}
	else if (Kind == ECommandKind::HideExit)
	{
		const FBushRuntime* Bush = Bushes != nullptr ? Bushes->FindBushAt(Character->GetActorLocation()) : nullptr;
		if (Bush == nullptr)
		{
			return;
		}
		const uint8 Valid = ConcealmentGeometry::ValidateSide(Bush->EntryAxisMask, Side);
		Target = ConcealmentGeometry::ExitPoint(*Bush, Valid, Radius) + FVector(0.0, 0.0, HalfHeight);
		Duration = Tuning.BushExitDurSec;
		OwnerPredictedUntil = -1.0;
		LatchClearAt = Now + Duration + RoundTripSec(); // the predicted exit ends the inside latch (the server's pose confirms)
	}
	else
	{
		return;
	}
	StartLocalWarp(Target, Duration, Kind == ECommandKind::HideEnter, 0.0);
	if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
	{
		Movement->bClientIgnoreMovementCorrections = true;
	}
	OwnerIgnoreCorrectionsUntil = Now + Duration + RoundTripSec();
}

void UConcealmentComponent::OnServerWarpStarted(double DurationSec)
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (GetOwnerWarpMode() != EConcealOwnerWarp::PredictOnPress || Character == nullptr || !Character->HasAuthority()
		|| !Character->IsPlayerControlled() || Character->IsLocallyControlled())
	{
		return; // AI pawns and the listen host's own pawn have no remote owner to protect
	}
	if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
	{
		// Stops the server GENERATING corrections; the server never takes the client's position
		// (bServerAcceptClientAuthoritativePosition stays false, ADR-0003 section 3).
		Movement->bIgnoreClientMovementErrorChecksAndCorrection = true;
		Movement->bServerAcceptClientAuthoritativePosition = false;
	}
	ServerIgnoreCorrectionsUntil = ServerNow() + DurationSec + RoundTripSec();
}

void UConcealmentComponent::HandleActionState(const FActionState& State)
{
	// a1 (the spike's OnStateArrival mode): the owner starts the identical source when its pose arrives, seeked to Now - Start.
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (Character != nullptr && !Character->HasAuthority() && Character->IsLocallyControlled() && State.Seq != LastSeenActionSeq
		&& GetOwnerWarpMode() == EConcealOwnerWarp::OnStateArrival && (State.Pose == EActionPose::BushEnter || State.Pose == EActionPose::BushExit))
	{
		const UBushRuntimeSubsystem* Bushes = UBushRuntimeSubsystem::Get(GetWorld());
		const FVector Here = Character->GetActorLocation();
		const FBushRuntime* Bush = nullptr;
		if (Bushes != nullptr)
		{
			Bush = State.Pose == EActionPose::BushExit ? Bushes->FindBushAt(Here) : Bushes->FindEnterableBush(Here, Tuning.BushEntryReachCm + 40.0);
		}
		if (Bush != nullptr)
		{
			const double Elapsed = FMath::Max(ServerNow() - State.StartServerTime, 0.0);
			const bool bEnter = State.Pose == EActionPose::BushEnter;
			const uint8 Side = ConcealmentGeometry::SideFromYaw(Character->GetControlRotation().Yaw);
			const FVector Target = bEnter ? Bush->Anchor : ConcealmentGeometry::ExitPoint(*Bush, ConcealmentGeometry::ValidateSide(Bush->EntryAxisMask, Side), Character->GetSimpleCollisionRadius());
			StartLocalWarp(Target + FVector(0.0, 0.0, Character->GetSimpleCollisionHalfHeight()), bEnter ? Tuning.BushEnterSwapSec : Tuning.BushExitDurSec, bEnter, Elapsed);
		}
	}
	LastSeenActionSeq = State.Seq;
	if (State.Pose == EActionPose::BushEnter || State.Pose == EActionPose::BushExit)
	{
		OwnerPredictedUntil = -1.0; // the server's pose confirms the prediction
	}
	switch (State.Pose)
	{
	case EActionPose::BushEnter:
		bInsideLatch = true;
		LatchClearAt = -1.0;
		break;
	case EActionPose::BushExit:
		LatchClearAt = State.StartServerTime + Tuning.BushExitDurSec;
		break;
	case EActionPose::Strike:
		if ((State.Variant & StrikeVariant::BushPreface) != 0)
		{
			LatchClearAt = State.StartServerTime + FMath::Max(static_cast<double>(Tuning.BushStrikeEmergeSec), ConcealmentComponentLocal::EmergeEndSec());
		}
		break;
	default:
		break;
	}
}

void UConcealmentComponent::SetServerState(EConcealState InState, uint16 InBushId, double InTransitionStartServerTime)
{
	ServerState = InState;
	ServerBushId = InBushId;
	ServerTransitionStart = InTransitionStartServerTime;
}

double UConcealmentComponent::ServerNow() const
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World != nullptr ? World->GetGameState() : nullptr;
	return GameState != nullptr ? GameState->GetServerWorldTimeSeconds() : (World != nullptr ? World->GetTimeSeconds() : 0.0);
}

EConcealState UConcealmentComponent::GetEffectiveState(double ServerTime) const
{
	const AActor* Owner = GetOwner();
	if (Owner != nullptr && Owner->HasAuthority())
	{
		return ServerState;
	}
	const AStealthPawn* Pawn = Cast<AStealthPawn>(Owner);
	const FActionState A = Pawn != nullptr ? Pawn->GetActionState() : FActionState();
	if (A.Pose == EActionPose::BushEnter && ServerTime >= A.StartServerTime)
	{
		return ServerTime >= A.StartServerTime + Tuning.BushEnterSwapSec ? EConcealState::Concealed : EConcealState::Entering;
	}
	if (A.Pose == EActionPose::BushExit && ServerTime < A.StartServerTime + Tuning.BushExitDurSec)
	{
		return EConcealState::Exiting;
	}
	if (A.Pose == EActionPose::Strike && (A.Variant & StrikeVariant::BushPreface) != 0
		&& ServerTime < A.StartServerTime + FMath::Max(static_cast<double>(Tuning.BushStrikeEmergeSec), ConcealmentComponentLocal::EmergeEndSec()))
	{
		return EConcealState::Emerging;
	}
	if (bConcealedEvidence || (bInsideLatch && (LatchClearAt < 0.0 || ServerTime < LatchClearAt)))
	{
		return EConcealState::Concealed;
	}
	if (OwnerPredictedUntil >= 0.0 && ServerTime < OwnerPredictedUntil)
	{
		return EConcealState::Entering; // the owner's predicted enter (PredictOnPress), until the server's pose confirms it
	}
	return EConcealState::Outside;
}

uint8 UConcealmentComponent::EvaluateMoveState(double MoveServerTime)
{
	if (bInsideLatch && LatchClearAt >= 0.0 && MoveServerTime >= LatchClearAt)
	{
		bInsideLatch = false; // the exit or emerge completed: the latch is spent
	}
	return GetEffectiveState(MoveServerTime) != EConcealState::Outside ? 1 : 0;
}

void UConcealmentComponent::ApplyMoveState(uint8 State)
{
	CollisionWriter.Apply(GetCapsule(), ComputeConcealCollision(State != 0 ? EConcealState::Concealed : EConcealState::Outside, 0.0));
}

UPrimitiveComponent* UConcealmentComponent::GetCapsule() const
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	return Character != nullptr ? Character->GetCapsuleComponent() : nullptr;
}

void UConcealmentComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	// Simulated proxies run no PerformMovement: their capsule follows the same rule each frame (no proxy jitters against a
	// hide volume it is inside). The authority and the owner apply it per simulated move through the movement component.
	const AActor* Owner = GetOwner();
	if (Owner != nullptr && Owner->GetLocalRole() == ROLE_SimulatedProxy)
	{
		ApplyMoveState(EvaluateMoveState(ServerNow()));
	}
	// The correction windows of PredictOnPress end on time (then one normal correction lands on the fixed end point).
	const double Now = ServerNow();
	// The client latch (and the listen host's): every frame (cheap; the hide windows and the ally flag change between
	// RepNotifies). A dedicated server draws nothing.
	RefreshPresentation(Now);
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	UCharacterMovementComponent* Movement = Character != nullptr ? Character->GetCharacterMovement() : nullptr;
	if (Movement != nullptr && OwnerIgnoreCorrectionsUntil >= 0.0 && Now >= OwnerIgnoreCorrectionsUntil)
	{
		Movement->bClientIgnoreMovementCorrections = false;
		OwnerIgnoreCorrectionsUntil = -1.0;
	}
	if (Movement != nullptr && ServerIgnoreCorrectionsUntil >= 0.0 && Now >= ServerIgnoreCorrectionsUntil)
	{
		Movement->bIgnoreClientMovementErrorChecksAndCorrection = false;
		ServerIgnoreCorrectionsUntil = -1.0;
	}
	if (OwnerPredictedUntil >= 0.0 && Now >= OwnerPredictedUntil)
	{
		OwnerPredictedUntil = -1.0; // never confirmed: a misprediction; the next correction restores the server's position
	}
}

bool IsAllyBodyHidden(const AStealthPawn* Pawn)
{
	const UConcealmentComponent* Component = UConcealmentComponent::Find(Pawn);
	return Component != nullptr && Component->IsPresentationHidden();
}
