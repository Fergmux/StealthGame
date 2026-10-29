// Stories con-003..con-018, con-024: the server-only Concealment service and its engine adapters. See
// Server/ConcealmentService.h.

#include "Server/ConcealmentService.h"

#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/RootMotionSource.h"
#include "Server/ActionHub.h"
#include "Server/Core/StealthPawnCommandComponent.h"
#include "Server/Queue/StealthResolutionQueueCore.h"
#include "Server/Roster/StealthRosterStore.h"
#include "View/ConcealmentComponent.h"
#include "View/Core/StealthPawn.h"
#include "View/Core/StealthPawnMovementComponent.h"

/** The engine adapters behind the core's seams. */
struct UConcealmentService::FAdapters final : public IConcealmentPawnWorld, public IConcealmentRelations, public IConcealmentQueue
{
	TWeakObjectPtr<UWorld> World;
	TWeakObjectPtr<URosterStore> Roster;
	TWeakObjectPtr<UActionHub> Hub;
	FResolutionQueueCore* Queue = nullptr;

	static const AStealthPawn* AsPawn(const APawn& Pawn) { return Cast<AStealthPawn>(&Pawn); }

	// IConcealmentPawnWorld
	virtual bool IsLive(const APawn& Pawn) const override
	{
		const AStealthPawn* Body = AsPawn(Pawn);
		return IsValid(&Pawn) && !Pawn.IsActorBeingDestroyed() && (Body == nullptr || !Body->HasDeathWrite());
	}
	virtual FVector GetLocation(const APawn& Pawn) const override { return Pawn.GetActorLocation(); }
	virtual double GetCapsuleRadiusCm(const APawn& Pawn) const override
	{
		const ACharacter* Character = Cast<ACharacter>(&Pawn);
		return Character != nullptr ? Character->GetCapsuleComponent()->GetScaledCapsuleRadius() : AStealthPawn::SampleCapsuleRadiusCm;
	}
	virtual double GetCapsuleHalfHeightCm(const APawn& Pawn) const override
	{
		const ACharacter* Character = Cast<ACharacter>(&Pawn);
		return Character != nullptr ? Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : AStealthPawn::SampleCapsuleHalfHeightCm;
	}
	virtual double GetLookYawDeg(const APawn& Pawn) const override { return Pawn.GetControlRotation().Yaw; }
	virtual ECommandLegalityClass GetLegalityClass(const APawn& Pawn) const override
	{
		URosterStore* Store = Roster.Get();
		if (Store == nullptr || !Store->IsStoreInitialized())
		{
			return ECommandLegalityClass::Active; // no Roster (headless worlds): every pawn is an unbound, Active pawn
		}
		switch (Store->GetLegalityClass(const_cast<APawn*>(&Pawn)))
		{
		case EParticipantState::Active: return ECommandLegalityClass::Active;
		case EParticipantState::Informant: return ECommandLegalityClass::Informant;
		default: return ECommandLegalityClass::RoundEnded;
		}
	}
	virtual bool IsRunning(const APawn& Pawn) const override
	{
		const AStealthPawn* Body = AsPawn(Pawn);
		const UStealthMovementComponent* Movement = Body != nullptr ? Body->GetStealthMovement() : nullptr;
		return Movement != nullptr && Movement->IsRunning();
	}
	virtual bool IsTraversing(const APawn& Pawn) const override
	{
		const AStealthPawn* Body = AsPawn(Pawn);
		const UStealthMovementComponent* Movement = Body != nullptr ? Body->GetStealthMovement() : nullptr;
		return Movement != nullptr && Movement->IsInTraversal();
	}
	virtual void WritePose(APawn& Pawn, EActionPose Pose, double StartServerTime) override
	{
		AStealthPawn* Body = Cast<AStealthPawn>(&Pawn);
		if (UPawnCommandComponent* Command = Body != nullptr ? Body->GetPawnCommand() : nullptr)
		{
			FActionStateWrite Write;
			Write.Pose = Pose;
			Write.StartServerTime = StartServerTime;
			Command->WriteActionState(Write);
		}
	}
	virtual FActionState GetActionState(const APawn& Pawn) const override
	{
		const AStealthPawn* Body = AsPawn(Pawn);
		return Body != nullptr ? Body->GetActionState() : FActionState();
	}
	virtual void StartWarp(APawn& Pawn, const FVector& Target, double DurationSec, bool bEnter) override
	{
		ACharacter* Character = Cast<ACharacter>(&Pawn);
		UCharacterMovementComponent* Movement = Character != nullptr ? Character->GetCharacterMovement() : nullptr;
		if (Movement == nullptr)
		{
			return;
		}
		// Story con-011: one FRootMotionSource_MoveToForce to a FIXED point (never MoveToDynamicForce), the same server path
		// for every pawn kind. A new hide transition replaces any running concealment move.
		Movement->RemoveRootMotionSource(TEXT("ConcealEnter"));
		Movement->RemoveRootMotionSource(TEXT("ConcealExit"));
		TSharedPtr<FRootMotionSource_MoveToForce> Source = MakeShared<FRootMotionSource_MoveToForce>();
		Source->InstanceName = bEnter ? TEXT("ConcealEnter") : TEXT("ConcealExit");
		Source->AccumulateMode = ERootMotionAccumulateMode::Override;
		Source->Priority = 900;
		Source->StartLocation = Pawn.GetActorLocation();
		Source->TargetLocation = Target;
		Source->Duration = static_cast<float>(FMath::Max(DurationSec, 0.05));
		Source->bRestrictSpeedToExpected = false;
		Source->FinishVelocityParams.Mode = ERootMotionFinishVelocityMode::SetVelocity;
		Source->FinishVelocityParams.SetVelocity = FVector::ZeroVector;
		Movement->ApplyRootMotionSource(Source);
		if (UConcealmentComponent* Component = UConcealmentComponent::Find(&Pawn))
		{
			Component->OnServerWarpStarted(DurationSec); // the owner-side mechanism's server half (story con-012)
		}
	}
	virtual void OnStateChanged(APawn& Pawn, EConcealState State, uint16 BushId, double TransitionStartServerTime) override
	{
		if (UConcealmentComponent* Component = UConcealmentComponent::Find(&Pawn))
		{
			Component->SetServerState(State, BushId, TransitionStartServerTime);
		}
	}
	virtual void GetPawnsNear(const FVector& Point, double RadiusCm, TArray<APawn*>& OutPawns) const override
	{
		OutPawns.Reset();
		if (UWorld* Target = World.Get())
		{
			for (TActorIterator<AStealthPawn> It(Target); It; ++It)
			{
				if (IsValid(*It) && FVector::Dist2D(It->GetActorLocation(), Point) <= RadiusCm)
				{
					OutPawns.Add(*It);
				}
			}
		}
	}
	virtual void SetExitDepenetrationCap(APawn& Pawn, double CapCm) override
	{
		ACharacter* Character = Cast<ACharacter>(&Pawn);
		if (UCharacterMovementComponent* Movement = Character != nullptr ? Character->GetCharacterMovement() : nullptr)
		{
			Movement->MaxDepenetrationWithPawn = static_cast<float>(CapCm);
		}
	}

	// IConcealmentRelations
	virtual FParticipantId GetParticipant(const APawn& Pawn) const override
	{
		URosterStore* Store = Roster.Get();
		return Store != nullptr && Store->IsStoreInitialized() ? Store->GetParticipant(const_cast<APawn*>(&Pawn)) : FParticipantId();
	}
	virtual bool IsAllyViewer(FParticipantId Viewer, const APawn& Pawn) const override
	{
		const FParticipantId Owner = GetParticipant(Pawn);
		if (!Viewer.IsValid() || !Owner.IsValid())
		{
			return false; // an NPC has no allies; an unknown viewer is a non-ally
		}
		if (Viewer == Owner)
		{
			return true;
		}
		const URosterStore* Store = Roster.Get();
		const FParticipantRecord* A = Store != nullptr ? Store->FindRecord(Viewer) : nullptr;
		const FParticipantRecord* B = Store != nullptr ? Store->FindRecord(Owner) : nullptr;
		return A != nullptr && B != nullptr && A->Team != ERosterTeam::None && A->Team == B->Team;
	}
	virtual bool IsPlayerPawn(const APawn& Pawn) const override { return GetParticipant(Pawn).IsValid(); }

	// IConcealmentQueue
	virtual void ScheduleAt(double DueServerTime, const FQueueItem& Item) override
	{
		if (Queue != nullptr)
		{
			Queue->ScheduleAt(DueServerTime, Item);
		}
	}
	virtual void SetVoidCode(uint32 OwnerCode) override
	{
		if (Queue != nullptr && Queue->IsInSlot())
		{
			Queue->SetVoidCode(OwnerCode);
		}
	}
	virtual void AppendReply(FReplySinkId Sink, FQueueKey Key, const FCommandReply& Reply) override
	{
		if (UActionHub* Target = Hub.Get())
		{
			Target->GetReplySinks().Append(Sink, Key, Reply);
		}
	}
	virtual void OnActionStarted(APawn& Pawn, EActionType Type) override
	{
		if (UActionHub* Target = Hub.Get())
		{
			Target->OnActionStarted.Broadcast(Cast<AStealthPawn>(&Pawn), Type);
		}
	}
};

bool UConcealmentService::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World != nullptr && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE) && World->GetNetMode() != NM_Client;
}

void UConcealmentService::Deinitialize()
{
	Unbind();
	Super::Deinitialize();
}

UConcealmentService* UConcealmentService::Get(const UWorld* World)
{
	return World != nullptr ? World->GetSubsystem<UConcealmentService>() : nullptr;
}

bool UConcealmentService::Bind(const FConcealmentServiceInit& Init)
{
	Unbind();
	if (Init.Clock == nullptr || Init.Queue == nullptr)
	{
		return false;
	}
	Adapters = MakeShared<FAdapters>();
	Adapters->World = GetWorld();
	Adapters->Roster = Init.Roster != nullptr ? Init.Roster : URosterStore::Get(GetWorld());
	Adapters->Hub = Init.Hub != nullptr ? Init.Hub : UActionHub::Get(GetWorld());
	Adapters->Queue = Init.Queue;
	Hub = Adapters->Hub;
	Roster = Adapters->Roster;
	Epoch = Init.Epoch;

	FConcealmentCoreDeps Deps;
	Deps.Clock = Init.Clock;
	Deps.Pawns = Init.PawnWorld != nullptr ? Init.PawnWorld : Adapters.Get();
	Deps.Queue = Adapters.Get();
	Deps.Relations = Adapters.Get();
	Deps.Flags = Init.Flags;
	Deps.Cull = Init.Cull;
	Deps.Log = Init.Log;
	Deps.Faults = Init.Faults;
	Core = MakeUnique<FConcealmentCore>(Deps, Init.Params);

	if (Init.Queue->RegisterTimerResolver(ETimerKind::ConcealmentTransition, this))
	{
		RegisteredQueue = Init.Queue;
	}
	if (UActionHub* Target = Hub.Get())
	{
		ActionStartedHandle = Target->OnActionStarted.AddUObject(this, &UConcealmentService::HandleActionStarted);
	}
	if (URosterStore* Store = Roster.Get())
	{
		RosterEventHandle = Store->OnRosterEvent.AddUObject(this, &UConcealmentService::HandleRosterEvent);
	}
	return true;
}

void UConcealmentService::Unbind()
{
	if (RegisteredQueue != nullptr)
	{
		RegisteredQueue->UnregisterTimerResolver(ETimerKind::ConcealmentTransition);
		RegisteredQueue = nullptr;
	}
	if (UActionHub* Target = Hub.Get())
	{
		Target->OnActionStarted.Remove(ActionStartedHandle);
	}
	if (URosterStore* Store = Roster.Get())
	{
		Store->OnRosterEvent.Remove(RosterEventHandle);
	}
	ActionStartedHandle.Reset();
	RosterEventHandle.Reset();
	Core.Reset();
	Adapters.Reset();
}

void UConcealmentService::SetBushes(TConstArrayView<FBushRuntime> Bushes)
{
	if (Core.IsValid())
	{
		Core->SetBushes(Bushes);
	}
}

void UConcealmentService::HandleActionStarted(AStealthPawn* Pawn, EActionType Type)
{
	if (Type != EActionType::Attack || Pawn == nullptr || !Core.IsValid())
	{
		return;
	}
	// The Attack executed in its slot (Assassination wrote the attacker's Strike pose first): a Concealed attacker breaks.
	FTimerPayload Unused;
	const FQueueItem Cause = FQueueItem::Make(EQueueItemType::TimerFired, Epoch != nullptr ? Epoch->GetCurrentEpoch() : FEpochToken(), FMailboxId::ServerLocal(), Unused);
	const FQueueKey Key = RegisteredQueue != nullptr ? RegisteredQueue->CurrentKey() : FQueueKey();
	Core->OnAttackExecuted(*Pawn, Pawn->GetActionState().ContactServerTime, Key, Cause);
}

void UConcealmentService::HandleRosterEvent(const FRosterEvent& Event)
{
	if (Event.Type == ERosterEventType::RosterReset && Core.IsValid())
	{
		Core->ResetAll();
	}
}

void UConcealmentService::HandlePawnKilled(APawn* Victim)
{
	if (Victim != nullptr && Core.IsValid())
	{
		Core->OnPawnKilled(*Victim, RegisteredQueue != nullptr ? RegisteredQueue->CurrentKey() : FQueueKey());
	}
}

void UConcealmentService::HandleDeathWrite(APawn* Victim)
{
	if (Victim != nullptr && Core.IsValid())
	{
		Core->OnDeathWrite(*Victim);
	}
}

void UConcealmentService::HandleParticipantRebound(FParticipantId Participant, const APawn* NewPawn)
{
	if (Core.IsValid())
	{
		Core->OnParticipantRebound(Participant, NewPawn);
	}
}

bool UConcealmentService::IsInBushOrTransition(const APawn* Pawn) const { return Core.IsValid() && Core->IsInBushOrTransition(Pawn); }
bool UConcealmentService::IsConcealedFrom(FParticipantId Viewer, const APawn* Pawn) const { return Core.IsValid() && Core->IsConcealedFrom(Viewer, Pawn); }
EConcealState UConcealmentService::GetState(const APawn* Pawn) const { return Core.IsValid() ? Core->GetState(Pawn) : EConcealState::Outside; }

EResolveResult UConcealmentService::Resolve(const FQueueItem& Item, FQueueKey Key)
{
	if (!Core.IsValid())
	{
		return EResolveResult::Voided;
	}
	return Item.Type == EQueueItemType::TimerFired ? Core->ResolveTimer(Item, Key) : Core->ResolveCommand(Item, Key);
}

FPredicateResult UConcealmentService::CanBegin(const AStealthPawn& Pawn, const FCommandPayload& Cmd) const
{
	return Core.IsValid() ? Core->CanBeginHide(Pawn, static_cast<ECommandKind>(Cmd.Kind), Cmd.Arg) : FPredicateResult::Reject(ECommandRejectCode::NotNow);
}

bool UConcealmentService::IsPerceivable(const APawn* Pawn) const { return !Core.IsValid() || Core->IsPerceivable(Pawn); }
bool UConcealmentService::IsSteeringObstacle(const APawn* Pawn) const { return !Core.IsValid() || Core->IsSteeringObstacle(Pawn); }

bool UConcealmentService::FindSpotZone(const FVector& WorldPoint, uint16& OutBushId) const
{
	const TOptional<uint16> Bush = Core.IsValid() ? Core->TargetsBush(WorldPoint) : TOptional<uint16>();
	if (Bush.IsSet())
	{
		OutBushId = *Bush;
		return true;
	}
	return false;
}

void UConcealmentService::GetSpotOccupants(uint16 BushId, const APawn* Attacker, TArray<APawn*>& OutOccupants) const
{
	OutOccupants.Reset();
	if (Core.IsValid())
	{
		Core->GetSpotOccupants(BushId, Attacker, OutOccupants);
	}
}

bool UConcealmentService::IsConcealed(const APawn* Pawn) const { return Core.IsValid() && Core->IsConcealed(Pawn); }

bool UConcealmentService::GetFootprint(uint16 BushId, FBox2D& OutFootprint) const
{
	const FBushRuntime* Bush = Core.IsValid() ? Core->FindBush(BushId) : nullptr;
	if (Bush == nullptr)
	{
		return false;
	}
	OutFootprint = Bush->Footprint;
	return true;
}

void UConcealmentService::RemoveOccupant(APawn* Pawn)
{
	HandlePawnKilled(Pawn); // CON-R9: the in-slot occupancy exit of a victim killed while concealed (idempotent)
}

void UConcealmentService::OnBodyRemovalCommitted(APawn* Body)
{
	if (Body != nullptr && Core.IsValid())
	{
		Core->OnBodyRemoved(*Body);
	}
}
