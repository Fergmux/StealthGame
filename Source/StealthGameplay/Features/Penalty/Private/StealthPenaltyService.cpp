#include "Server/StealthPenaltyService.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Server/Core/Round/StealthCrowdControl.h"
#include "Server/DefenceStateComponent.h"
#include "Server/EncounterService.h"
#include "Server/Queue/StealthResolutionQueueCore.h"
#include "Server/Roster/StealthRosterCore.h"
#include "Server/Roster/StealthRosterStore.h"
#include "Server/StealthServerTuning.h"
#include "StealthPenaltyCore.h"
#include "UObject/UObjectArray.h"
#include "View/Core/StealthPawn.h"

// Stories nkp-001 (shell, subscriptions, carry-over), nkp-002 (OnPawnKilled), nkp-003 (IStrikeTallySource, re-push),
// nkp-005 (the exile's gateway), nkp-008 (the informant kill's RetireBinding).
// ADR-0007 thin facade: every adapter below adapts one engine owner and adds no rule.

/** The production adapters the service binds (one engine owner each). */
struct FPenaltyServiceAdapters
{
	/** IPenaltyRosterQuery over URosterStore's records (an unknown or removed participant has no state). */
	struct FRosterQuery final : IPenaltyRosterQuery
	{
		TWeakObjectPtr<URosterStore> Store;

		virtual TOptional<EParticipantState> GetState(FParticipantId Participant) const override
		{
			const URosterStore* Roster = Store.Get();
			const FParticipantRecord* Record = Roster != nullptr && Roster->IsStoreInitialized() ? Roster->FindRecord(Participant) : nullptr;
			return Record != nullptr ? TOptional<EParticipantState>(Record->State) : TOptional<EParticipantState>();
		}

		virtual uint32 GetRoundEpoch() const override
		{
			const URosterStore* Roster = Store.Get();
			return Roster != nullptr && Roster->IsStoreInitialized() ? Roster->GetEpochToken().RoundEpoch : 0;
		}
	};

	/** IPenaltySlotContext over the round queue (FResolutionQueueCore::IsInSlot / CurrentKey). */
	struct FQueueSlot final : IPenaltySlotContext
	{
		FResolutionQueueCore* Queue = nullptr;

		virtual bool IsInSlot() const override { return Queue != nullptr && Queue->IsInSlot(); }
		virtual FQueueKey CurrentKey() const override { return Queue != nullptr ? Queue->CurrentKey() : FQueueKey(); }
	};

	/**
	 * STAND-IN for the TDC tuning store's frozen set (as the encounter service's FEncounterParams::Defaults): the schema
	 * defaults of the server face, read at each latch.
	 */
	struct FSchemaDefaultsTuning final : IPenaltyTuningSource
	{
		virtual FPenaltyParams ReadPenaltyParams() const override { return FPenaltyParams::FromTuning(FStealthServerTuning()); }
	};

	/**
	 * IPenaltyBodyGateway over the Roster store, Round's ICrowdControl and the composition root's IPenaltyPawnGateway. The
	 * core names pawns by key; the service registers the slot's two pawns (victim, killer) for the duration of one
	 * OnPawnKilled, so a key resolves only to a pawn of the current kill.
	 */
	struct FBodies final : IPenaltyBodyGateway
	{
		TWeakObjectPtr<URosterStore> Store;
		ICrowdControl* Crowd = nullptr;
		IPenaltyPawnGateway* Pawns = nullptr;
		TMap<FPenaltyPawnKey, TWeakObjectPtr<APawn>> SlotPawns;

		APawn* Resolve(FPenaltyPawnKey Key) const
		{
			const TWeakObjectPtr<APawn>* Found = SlotPawns.Find(Key);
			return Found != nullptr ? Found->Get() : nullptr;
		}

		virtual ERosterResult RequestStrikeLimit(FParticipantId Participant, uint32 ExpectedRoundEpoch) override
		{
			URosterStore* Roster = Store.Get();
			if (Roster == nullptr || !Roster->IsStoreInitialized())
			{
				return ERosterResult::Rejected_UnknownParticipant;
			}
			FRosterTransitionOptions Options;
			FRosterEpochToken Expected = Roster->GetEpochToken();
			Expected.RoundEpoch = ExpectedRoundEpoch;
			Options.ExpectedEpoch = Expected;
			return Roster->RequestTransition(Participant, EParticipantState::Informant, ERosterReason::StrikeLimit, Options);
		}

		virtual void ReleaseCard(FPenaltyPawnKey Body) override
		{
			APawn* Pawn = Resolve(Body);
			if (Pawn != nullptr && Pawns != nullptr)
			{
				Pawns->ReleaseCard(Pawn);
			}
		}

		virtual void AdoptReleasedPawn(FPenaltyPawnKey Body, double RestrictedUntilServerTime, bool bExitAtNextScheduledSlot) override
		{
			AStealthPawn* Pawn = Cast<AStealthPawn>(Resolve(Body));
			if (Pawn != nullptr && Crowd != nullptr)
			{
				FCrowdAdoptParams Params;
				Params.RestrictedUntilServerTime = RestrictedUntilServerTime;
				Params.bExitAtNextScheduledSlot = bExitAtNextScheduledSlot;
				Crowd->AdoptReleasedPawn(Pawn, Params);
			}
		}

		virtual void RequestHandOverToAi(FPenaltyPawnKey Body, uint32 ExpectedRoundEpoch, double PossessNotBeforeServerTime) override
		{
			APawn* Pawn = Resolve(Body);
			if (Pawn != nullptr && Pawns != nullptr)
			{
				Pawns->RequestHandOverToAi(Pawn, ExpectedRoundEpoch, PossessNotBeforeServerTime);
			}
		}

		virtual ERosterResult RetireBinding(FPenaltyPawnKey Body, double RetireAtServerTime) override
		{
			URosterStore* Roster = Store.Get();
			APawn* Pawn = Resolve(Body);
			if (Roster == nullptr || !Roster->IsStoreInitialized() || Pawn == nullptr)
			{
				return ERosterResult::NoOp_NotBound;
			}
			return Roster->RetireBinding(Pawn, static_cast<int64>(FMath::RoundToDouble(RetireAtServerTime * 1000.0)), ERosterReason::InformantKilled);
		}
	};

	FRosterQuery RosterQuery;
	FQueueSlot Slot;
	FSchemaDefaultsTuning DefaultTuning;
	FBodies Bodies;
};

bool UPenaltyService::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World != nullptr && World->IsGameWorld() && Super::ShouldCreateSubsystem(Outer);
}

void UPenaltyService::Deinitialize()
{
	Unbind();
	Super::Deinitialize();
}

bool UPenaltyService::Bind(const FPenaltyServiceInit& Init)
{
	Unbind();
	const UWorld* World = GetWorld();
	if (Init.Clock == nullptr || (World != nullptr && World->GetNetMode() == NM_Client))
	{
		return false;
	}
	Roster = Init.Roster != nullptr ? Init.Roster : URosterStore::Get(World);
	Encounters = Init.Encounters;

	Adapters = MakeShared<FPenaltyServiceAdapters>();
	Adapters->RosterQuery.Store = Roster;
	Adapters->Slot.Queue = Init.Queue;
	Adapters->Bodies.Store = Roster;
	Adapters->Bodies.Crowd = Init.Crowd;
	Adapters->Bodies.Pawns = Init.Pawns;

	FPenaltyCoreDeps Deps;
	Deps.Clock = Init.Clock;
	Deps.Roster = &Adapters->RosterQuery;
	Deps.Tuning = Init.Tuning != nullptr ? Init.Tuning : &Adapters->DefaultTuning;
	Deps.Slot = &Adapters->Slot;
	Deps.Log = Init.Log;
	Deps.OwnerView = Init.OwnerView;
	Deps.Bodies = &Adapters->Bodies;
	Deps.Respawns = Init.Respawns;
	Deps.Faults = Init.Faults;
	Core = MakeShared<FPenaltyCore>(Deps);

	if (UEncounterService* Service = Encounters.Get())
	{
		BoundPawnKilled = Service->GetOnPawnKilled();
		if (BoundPawnKilled != nullptr)
		{
			PawnKilledHandle = BoundPawnKilled->AddUObject(this, &UPenaltyService::HandlePawnKilled);
		}
	}
	if (URosterStore* Store = Roster.Get())
	{
		RosterEventHandle = Store->OnRosterEvent.AddUObject(this, &UPenaltyService::HandleRosterEvent);
	}
	return true;
}

void UPenaltyService::Unbind()
{
	if (BoundPawnKilled != nullptr)
	{
		// Remove only from the delegate still alive: a released encounter core answers null (or a new core's delegate).
		UEncounterService* Service = Encounters.Get();
		if (Service != nullptr && Service->GetOnPawnKilled() == BoundPawnKilled)
		{
			BoundPawnKilled->Remove(PawnKilledHandle);
		}
	}
	BoundPawnKilled = nullptr;
	PawnKilledHandle.Reset();
	if (URosterStore* Store = Roster.Get())
	{
		Store->OnRosterEvent.Remove(RosterEventHandle);
	}
	RosterEventHandle.Reset();
	Core.Reset();
	Adapters.Reset();
	Roster.Reset();
	Encounters.Reset();
}

void UPenaltyService::OnRoundPreparing()
{
	if (Core.IsValid())
	{
		Core->OnRoundPreparing();
	}
}

void UPenaltyService::OnViewRebound(FParticipantId Participant)
{
	if (Core.IsValid())
	{
		Core->OnViewRebound(Participant);
	}
}

FStrikeTally UPenaltyService::GetTallyForResult(const APawn* Attacker, EStrikeResult Result)
{
	return GetTallyForStrike(Attacker, Result, FClientCommandSeq());
}

FStrikeTally UPenaltyService::GetTallyForStrike(const APawn* Attacker, EStrikeResult Result, const FClientCommandSeq& Seq)
{
	return Core.IsValid() ? Core->GetTallyForResult(ResolveParticipant(Attacker), Result, Seq, MakePawnKey(Attacker)) : FStrikeTally();
}

FPenaltyPawnKey UPenaltyService::MakePawnKey(const APawn* Pawn)
{
	if (Pawn == nullptr)
	{
		return FPenaltyPawnKey();
	}
	const int32 Index = GUObjectArray.ObjectToIndex(Pawn);
	const int32 Serial = GUObjectArray.AllocateSerialNumber(Index);
	return FPenaltyPawnKey((static_cast<uint64>(static_cast<uint32>(Serial)) << 32) | static_cast<uint64>(static_cast<uint32>(Index)));
}

void UPenaltyService::HandlePawnKilled(APawn* Victim, APawn* Killer, EVictimKind Kind, EKillCause Cause)
{
	if (!Core.IsValid())
	{
		return;
	}
	// Inline in the Attack's slot (ADR-0018 section 2.1 step 6): the killer's pawn is still BoundLive here.
	FPenaltyKill Kill;
	Kill.Victim = MakePawnKey(Victim);
	Kill.Killer = ResolveParticipant(Killer);
	Kill.VictimKind = Kind;
	Kill.Cause = Cause;
	Kill.KillerPawn = MakePawnKey(Killer);
	Kill.KillerLockoutEndServerTime = ReadLockoutEnd(Killer);
	Kill.VictimParticipant = ResolveParticipant(Victim);
	const UEncounterService* Service = Encounters.Get();
	const TOptional<double> Death = Service != nullptr ? Service->GetScheduledDeathServerTime(Victim) : TOptional<double>();
	Kill.VictimDeathServerTime = Death.IsSet() ? Death.GetValue() : 0.0;
	// The exile and the retire name these two pawns by key (FBodies resolves them for this call only).
	Adapters->Bodies.SlotPawns.Reset();
	Adapters->Bodies.SlotPawns.Add(Kill.Victim, Victim);
	Adapters->Bodies.SlotPawns.Add(Kill.KillerPawn, Killer);
	const TSharedPtr<FPenaltyServiceAdapters> KeepAdapters = Adapters; // a listener of the exile's Roster event may Unbind
	const TSharedPtr<FPenaltyCore> KeepCore = Core;
	KeepCore->OnPawnKilled(Kill);
	KeepAdapters->Bodies.SlotPawns.Reset();
}

double UPenaltyService::ReadLockoutEnd(const APawn* Pawn)
{
	const UDefenceStateComponent* State = Pawn != nullptr ? UDefenceStateComponent::Find(*Pawn) : nullptr;
	return State != nullptr ? State->GetCore().GetLockoutEnd() : -1.0;
}

void UPenaltyService::HandleRosterEvent(const FRosterEvent& Event)
{
	if (!Core.IsValid())
	{
		return;
	}
	switch (Event.Type)
	{
	case ERosterEventType::RosterReset:
		Core->OnRosterReset(Event.RoundEpoch);
		break;
	case ERosterEventType::RosterRoundEnded:
		Core->OnRosterRoundEnded();
		break;
	case ERosterEventType::ParticipantReclaimed:
		Core->OnParticipantReclaimed(Event.Id);
		break;
	case ERosterEventType::ConnectionChanged:
		Core->OnConnectionChanged(Event.Id, Event.Connection == ERosterConnection::Connected);
		break;
	default:
		// ParticipantStateChanged is never a scheduling trigger here (delegate order inside the flush is unspecified,
		// ADR-0019 section 1): Round calls IInformantBodyProvider instead (story nkp-009).
		break;
	}
}

FParticipantId UPenaltyService::ResolveParticipant(const APawn* Pawn) const
{
	URosterStore* Store = Roster.Get();
	return Pawn != nullptr && Store != nullptr && Store->IsStoreInitialized() ? Store->GetParticipant(const_cast<APawn*>(Pawn)) : FParticipantId();
}
