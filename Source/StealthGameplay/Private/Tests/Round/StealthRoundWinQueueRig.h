#pragma once

// Shared rig of the same-tick win tests (stories rmf-045..047): the Round core over the recording fakes (Exit B rig),
// driven to Active, with the REAL resolution queue on the rig's fake clock and two stub resolvers standing in for the
// Assassination strike (PawnCommand) and the Objectives completion follow-on (HoldTimer(Complete)). Keys are forced by
// submitting both items from one mailbox in one tick (FIFO: Seq n, then n + 1), never by the intake permutation.

#include "CoreMinimal.h"

#if WITH_STEALTH_TEST_SEAMS

#include "Server/Queue/StealthStagedWriteApplier.h"
#include "Server/Testing/StealthQueueTestRig.h"
#include "StealthRoundExitBTestRig.h"

/** Applies (counts) the strike stub's death writes, so a strike-decided round-end binds and runs in the same drain. */
class FRoundWinQueueDeathApplier final : public IStagedWriteApplier
{
public:
	int32 Applied = 0;
	virtual void ApplyStagedWrite(const FStagedWrite& Write) override { ++Applied; }
};

struct FRoundWinQueueRig
{
	FRoundExitBRig Rig;
	FStealthQueueTestRig Support;
	TUniquePtr<FResolutionQueueCore> Queue;
	FStealthStubResolver Strikes;
	FStealthStubResolver Completions;
	FRoundWinQueueDeathApplier Deaths;

	/** Strike tag -> (attacker, victim) participant values. */
	TMap<uint32, TPair<uint32, uint32>> StrikeTargets;
	/** Completion tag -> (completer, team). */
	TMap<uint32, TPair<uint32, uint8>> CompletionTenders;

	explicit FRoundWinQueueRig(const FRoundRuleParams& Params = FRoundExitBRig::DefaultParams())
		: Rig(Params)
	{
		FResolutionQueueDeps Deps;
		Deps.Clock = &Rig.Clock;
		Deps.SubSeeds = &Support.Seeds;
		Deps.Epoch = &Support.Epoch;
		Deps.Log = Rig.Core->GetRoundLogWriter();
		Deps.FaultSink = &Support.Faults;
		Queue = MakeUnique<FResolutionQueueCore>(Deps);
		Queue->RegisterResolver(EQueueItemType::PawnCommand, &Strikes);
		Queue->RegisterResolver(EQueueItemType::HoldTimer, &Completions);
		Queue->RegisterStagedWriteApplier(EStagedWriteType::DeathWrite, &Deaths);
		Rig.Core->SetRoundQueue(Queue.Get());

		// The strike stub: re-validates the attacker, removes the victim (the Roster transition), and the Roster flush of
		// this slot dispatches the event to Round's F2 check inline (R11 follow-ons, ADR-0002 section 5).
		Strikes.OnResolve = [this](const FQueueItem& Item, FQueueKey Key)
		{
			const TPair<uint32, uint32>* Target = StrikeTargets.Find(FStealthStubResolver::ArgOf(Item));
			if (Target == nullptr || !IsActive(Target->Key) || !IsActive(Target->Value))
			{
				return EResolveResult::Voided;
			}
			// The victim's death write at the slot's own time (no presentation lead in these ordering tests): a deciding
			// strike's round-end step binds to it and runs in this drain's staging pass (ADR-0002 8.3, story rmf-053).
			FStagedWrite Death;
			Death.Type = EStagedWriteType::DeathWrite;
			Death.ServerTime = Rig.Clock.GetServerTimeSeconds();
			Queue->StageWrite(Death);
			Kill(Target->Value, ERosterReason::Assassinated);
			return EResolveResult::Executed;
		};
		// The completion stub: re-validates the tender (Active completer) and declares the objective win.
		Completions.OnResolve = [this](const FQueueItem& Item, FQueueKey Key)
		{
			const TPair<uint32, uint8>* Tender = CompletionTenders.Find(FStealthStubResolver::ArgOf(Item));
			if (Tender == nullptr || !IsActive(Tender->Key))
			{
				return EResolveResult::Voided;
			}
			Rig.Core->DeclareObjectiveWin(Tender->Value, Key);
			return EResolveResult::Executed;
		};
	}

	~FRoundWinQueueRig()
	{
		Rig.Core->SetRoundQueue(nullptr);
	}

	/** Lobby -> Active with Team0 v Team1 members, all Active and Connected (the forced Active-entry driver). */
	void ToActive(int32 Team0, int32 Team1)
	{
		Rig.OpenLobby();
		Rig.SetLayout(Team0, Team1);
		Rig.Fakes.Roster.SetAllStates(EParticipantState::Active);
		Rig.DriveTo(ERoundPhase::Active);
		// The round-end step's phase write (Ended) or a void's Reset pushes one admission mode.
		Rig.Fakes.Session.Script(TEXT("SetAdmissionMode"));
	}

	bool IsActive(uint32 Participant) const
	{
		const FRoundRosterMember* Member = Rig.Fakes.Roster.Members.FindByPredicate([Participant](const FRoundRosterMember& M) { return M.Participant.Value == Participant; });
		return Member != nullptr && Member->State == EParticipantState::Active;
	}

	int32 ActiveOn(ERosterTeam Team) const
	{
		return Rig.Fakes.Roster.Members.FilterByPredicate([Team](const FRoundRosterMember& M) { return M.Team == Team && M.State == EParticipantState::Active; }).Num();
	}

	/** The Roster transition Active -> Informant and its event, evaluated by Round (as the store forwards it). */
	void Kill(uint32 Participant, ERosterReason Reason)
	{
		const FRoundRosterMember* Member = Rig.Fakes.Roster.Members.FindByPredicate([Participant](const FRoundRosterMember& M) { return M.Participant.Value == Participant; });
		check(Member != nullptr);
		Rig.Fakes.Roster.SetState(FParticipantId(Participant), EParticipantState::Informant);
		FRoundTransitionEvent Event;
		Event.Participant = FParticipantId(Participant);
		Event.Team = Member->Team;
		Event.Old = EParticipantState::Active;
		Event.New = EParticipantState::Informant;
		Event.Reason = Reason;
		Event.ActiveCountAfter[0] = ActiveOn(ERosterTeam::Team0);
		Event.ActiveCountAfter[1] = ActiveOn(ERosterTeam::Team1);
		Rig.Core->OnRosterTransition(Event);
	}

	/** A strike item (tag Tag) from Attacker on Victim, and a final completion item (tag Tag) by Completer for Team. */
	FQueueItem Strike(uint32 Tag, uint32 Attacker, uint32 Victim)
	{
		StrikeTargets.Add(Tag, { Attacker, Victim });
		return Support.MakeItem(EQueueItemType::PawnCommand, Tag);
	}
	FQueueItem Completion(uint32 Tag, uint32 Completer, uint8 Team)
	{
		CompletionTenders.Add(Tag, { Completer, Team });
		return Support.MakeItem(EQueueItemType::HoldTimer, Tag);
	}

	/** Submits the items in order from one mailbox (one tick: consecutive Seqs) and drains once. */
	void DrainOneTick(TConstArrayView<FQueueItem> Items)
	{
		for (const FQueueItem& Item : Items)
		{
			Queue->Submit(FMailboxId::ServerLocal(), Item);
		}
		Rig.Clock.Advance(RoundTestFixtures::RigFrameSec, 1);
		Queue->Drain();
	}

	int32 CountLogged(ERoundLogType Type) const { return Rig.EventLog.CountOfType(Type); }
};

#endif // WITH_STEALTH_TEST_SEAMS
