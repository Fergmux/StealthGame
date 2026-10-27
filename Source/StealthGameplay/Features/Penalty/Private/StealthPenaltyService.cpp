#include "Server/StealthPenaltyService.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Server/EncounterService.h"
#include "Server/Queue/StealthResolutionQueueCore.h"
#include "Server/Roster/StealthRosterCore.h"
#include "Server/Roster/StealthRosterStore.h"
#include "Server/StealthServerTuning.h"
#include "StealthPenaltyCore.h"
#include "UObject/UObjectArray.h"

// Stories nkp-001 (shell, subscriptions, carry-over), nkp-002 (OnPawnKilled), nkp-003 (IStrikeTallySource, re-push).
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

	FRosterQuery RosterQuery;
	FQueueSlot Slot;
	FSchemaDefaultsTuning DefaultTuning;
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

	FPenaltyCoreDeps Deps;
	Deps.Clock = Init.Clock;
	Deps.Roster = &Adapters->RosterQuery;
	Deps.Tuning = Init.Tuning != nullptr ? Init.Tuning : &Adapters->DefaultTuning;
	Deps.Slot = &Adapters->Slot;
	Deps.Log = Init.Log;
	Deps.OwnerView = Init.OwnerView;
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
	return Core.IsValid() ? Core->GetTallyForResult(ResolveParticipant(Attacker), Result, Seq) : FStrikeTally();
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
	Core->OnPawnKilled(Kill);
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
