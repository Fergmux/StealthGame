#pragma once

// Shared rig of the round-end, disconnect and forfeit tests (stories rmf-048..065): the Round core over the recording
// fakes (Exit B rig) driven through the real Preparing sequence to Active, with the REAL resolution queue on the rig's
// fake clock (epoch (0, FixtureRoundEpoch), the round's spawn epoch, so Round's own timer and connection items pass the
// epoch check), Round's grace, connection and round-end handlers registered, and stub resolvers standing in for the
// Assassination strike (PawnCommand: stages its DeathWrite and removes the victim) and the Objectives final completion
// (HoldTimer: the completer's Valid(i) and DeclareObjectiveWin). The fake Roster's Applied transitions update the layout
// and dispatch the event to Round inside the causing slot (the Roster flush stand-in). Keys are forced by submitting
// items from one mailbox in one tick (FIFO), never by the intake permutation.

#include "CoreMinimal.h"

#if WITH_STEALTH_TEST_SEAMS

#include "Server/Queue/StealthStagedWriteApplier.h"
#include "Server/Testing/StealthQueueTestRig.h"
#include "StealthRoundExitBTestRig.h"

/** Records every staged write the queue applies (the DeathWrite applier Assassination would register). */
class FRoundRecordingApplier final : public IStagedWriteApplier
{
public:
	struct FApplied
	{
		FStagedWrite Write;
		double AppliedAt = 0.0;
		ERoundPhase PhaseAtApply = ERoundPhase::Unknown;
	};

	const IStealthClock* Clock = nullptr;
	const FRoundMatchCore* Core = nullptr;
	TArray<FApplied> Applied;

	virtual void ApplyStagedWrite(const FStagedWrite& Write) override
	{
		Applied.Add({ Write, Clock != nullptr ? Clock->GetServerTimeSeconds() : 0.0, Core != nullptr ? Core->GetPhase() : ERoundPhase::Unknown });
	}
};

struct FRoundEndRig
{
	/** One stub strike: Attacker on Victim; the victim falls (reason Reason) and its DeathWrite is DeathDelaySec after the slot. */
	struct FStrike
	{
		uint32 Attacker = 0;
		uint32 Victim = 0;
		double DeathDelaySec = 0.0;
		ERosterReason Reason = ERosterReason::Assassinated;
	};

	/** One stub final completion by Completer for Team (Control: the hold's control epochs). */
	struct FCompletion
	{
		uint32 Completer = 0;
		uint8 Team = 0;
		FRoundCompleterControl Control;
		bool bSiteLive = true;
	};

	FRoundExitBRig Rig;
	FStealthQueueTestRig Support;
	TUniquePtr<FResolutionQueueCore> Queue;
	FStealthStubResolver Strikes;
	FStealthStubResolver Completions;
	FRoundRecordingApplier Deaths;
	TMap<uint32, FStrike> StrikeTargets;
	TMap<uint32, FCompletion> CompletionTenders;
	/** Every phase write's public state, in order (the replicated-struct spy). */
	TArray<FRoundPublicState> PublicWrites;
	int32 RoundEndedCount = 0;

	static FRoundRuleParams DefaultParams()
	{
		FRoundRuleParams Params = FRoundExitBRig::DefaultParams();
		Params.DisconnectGraceSec = RoundTestFixtures::GraceDisconnectSec;
		Params.RejoinWindowSec = RoundTestFixtures::GraceRejoinWindowSec;
		Params.RunStopSec = RoundTestFixtures::RunStopSec;
		Params.StrikeNetLeadSec = RoundTestFixtures::StrikeNetLeadSec;
		Params.Preparing.TravelLoadTimeoutSec = RoundTestFixtures::ResetTravelLoadTimeoutSec;
		return Params;
	}

	explicit FRoundEndRig(const FRoundRuleParams& Params = DefaultParams())
		: Rig(Params)
	{
		Support.Epoch.Current = FEpochToken(0, RoundTestFixtures::FixtureRoundEpoch);
		FResolutionQueueDeps Deps;
		Deps.Clock = &Rig.Clock;
		Deps.SubSeeds = &Support.Seeds;
		Deps.Epoch = &Support.Epoch;
		Deps.Log = Rig.Core->GetRoundLogWriter();
		Deps.FaultSink = &Support.Faults;
		Queue = MakeUnique<FResolutionQueueCore>(Deps);
		Queue->RegisterResolver(EQueueItemType::PawnCommand, &Strikes);
		Queue->RegisterResolver(EQueueItemType::HoldTimer, &Completions);
		Queue->RegisterResolver(EQueueItemType::ConnectionTransition, Rig.Core->GetConnectionResolver());
		Queue->RegisterTimerResolver(ETimerKind::GraceExpiry, Rig.Core->GetGraceResolver());
		Deaths.Clock = &Rig.Clock;
		Deaths.Core = Rig.Core.Get();
		Queue->RegisterStagedWriteApplier(EStagedWriteType::DeathWrite, &Deaths);
		Rig.Core->SetRoundQueue(Queue.Get());
		Rig.Core->OnRoundEnded = [this](const FRoundResult&) { ++RoundEndedCount; };
		Rig.Core->GetPhaseCore().SetOnPhaseChanged([this](ERoundPhase, ERoundPhase) { PublicWrites.Add(Rig.Core->GetPhaseCore().MakePublicState()); });

		// The Roster flush stand-in: an Applied transition updates the layout and its event reaches Round in the slot.
		Rig.Fakes.Roster.OnTransitionApplied = [this](FParticipantId Participant, EParticipantState NewState, ERosterReason Reason)
		{
			Transition(Participant.Value, NewState, Reason);
		};

		// The strike stub: re-validates attacker and victim, stages the victim's DeathWrite, removes the victim.
		Strikes.OnResolve = [this](const FQueueItem& Item, FQueueKey Key)
		{
			const FStrike* Strike = StrikeTargets.Find(FStealthStubResolver::ArgOf(Item));
			if (Strike == nullptr || !IsActive(Strike->Attacker) || !IsActive(Strike->Victim))
			{
				return EResolveResult::Voided;
			}
			const uint32 Fallen = Strike->Reason == ERosterReason::ReversedAgainst ? Strike->Attacker : Strike->Victim;
			FStagedWrite Death;
			Death.Type = EStagedWriteType::DeathWrite;
			Death.ServerTime = Rig.Clock.GetServerTimeSeconds() + Strike->DeathDelaySec;
			Death.Payload.Set<FDeathWritePayload>(FDeathWritePayload{ FPawnHandle(), Death.ServerTime });
			Queue->StageWrite(Death);
			Transition(Fallen, EParticipantState::Informant, Strike->Reason);
			return EResolveResult::Executed;
		};
		// The completion stub: the shared Valid(i) and DeclareObjectiveWin (story rmf-049's SubmitFinalCompletion).
		Completions.OnResolve = [this](const FQueueItem& Item, FQueueKey Key)
		{
			const FCompletion* Tender = CompletionTenders.Find(FStealthStubResolver::ArgOf(Item));
			if (Tender == nullptr)
			{
				return EResolveResult::Voided;
			}
			return Rig.Core->SubmitFinalCompletion(FParticipantId(Tender->Completer), Tender->Team, Tender->Control, Tender->bSiteLive, Key)
				? EResolveResult::Executed : EResolveResult::Voided;
		};
	}

	~FRoundEndRig()
	{
		Rig.Core->SetRoundQueue(nullptr);
	}

	/** Lobby -> Active through the real Preparing sequence with Team0 v Team1 members, all Active and Connected. */
	void ToActive(int32 Team0, int32 Team1)
	{
		Rig.OpenLobby();
		Rig.SetLayout(Team0, Team1);
		Rig.Fakes.Roster.SetAllStates(EParticipantState::Active);
		for (const FRoundRosterMember& Member : Rig.Fakes.Roster.Members)
		{
			Rig.Fakes.PawnControl.BoundPawnIds.Add(Member.Participant.Value, 100 + Member.Participant.Value);
		}
		Rig.EnterPreparing();
		Rig.RunPreparingToActive();
		check(Rig.Core->GetPhase() == ERoundPhase::Active);
	}

	/** Scripts the strict calls of one round-end step (the Ended admission push, EndRoundAll, Crowd Halt). */
	void ScriptRoundEnd()
	{
		Rig.Fakes.Session.Script(TEXT("SetAdmissionMode"));
		Rig.Fakes.Roster.Script(TEXT("EndRoundAll"), static_cast<int64>(ERosterResult::Applied));
		Rig.Fakes.Crowd.Script(TEXT("Halt"));
	}

	/** Scripts the strict calls of one Reset entry (the Closed admission push and the lobby travel). */
	void ScriptReset()
	{
		Rig.Fakes.Session.Script(TEXT("SetAdmissionMode"));
		Rig.Fakes.Session.Script(TEXT("RequestLobbyTravel"));
	}

	const FRoundRosterMember* Member(uint32 Participant) const
	{
		return Rig.Fakes.Roster.Members.FindByPredicate([Participant](const FRoundRosterMember& M) { return M.Participant.Value == Participant; });
	}

	bool IsActive(uint32 Participant) const
	{
		const FRoundRosterMember* Found = Member(Participant);
		return Found != nullptr && Found->State == EParticipantState::Active;
	}

	int32 ActiveOn(ERosterTeam Team) const
	{
		return Rig.Fakes.Roster.Members.FilterByPredicate([Team](const FRoundRosterMember& M) { return M.Team == Team && M.State == EParticipantState::Active; }).Num();
	}

	/** A Roster transition and its event, evaluated by Round (as the store forwards it). No-op unless the state changes. */
	void Transition(uint32 Participant, EParticipantState NewState, ERosterReason Reason)
	{
		const FRoundRosterMember* Found = Member(Participant);
		check(Found != nullptr);
		const EParticipantState Old = Found->State;
		const ERosterTeam Team = Found->Team;
		if (Old == NewState)
		{
			return;
		}
		Rig.Fakes.Roster.SetState(FParticipantId(Participant), NewState);
		FRoundTransitionEvent Event;
		Event.Participant = FParticipantId(Participant);
		Event.Team = Team;
		Event.Old = Old;
		Event.New = NewState;
		Event.Reason = Reason;
		Event.ActiveCountAfter[0] = ActiveOn(ERosterTeam::Team0);
		Event.ActiveCountAfter[1] = ActiveOn(ERosterTeam::Team1);
		Rig.Core->OnRosterTransition(Event);
	}

	/** A strike item (tag Tag) from Attacker on Victim whose death write lands Tv - StrikeNetLeadSec after the slot. */
	FQueueItem Strike(uint32 Tag, uint32 Attacker, uint32 Victim, double Tv = RoundTestFixtures::StrikeTvSec, ERosterReason Reason = ERosterReason::Assassinated)
	{
		StrikeTargets.Add(Tag, { Attacker, Victim, Tv - RoundTestFixtures::StrikeNetLeadSec, Reason });
		return Support.MakeItem(EQueueItemType::PawnCommand, Tag);
	}

	/** A final completion item (tag Tag) by Completer for Team, human-controlled unless Control says otherwise. */
	FQueueItem Completion(uint32 Tag, uint32 Completer, uint8 Team, const FRoundCompleterControl& Control = FRoundCompleterControl())
	{
		CompletionTenders.Add(Tag, { Completer, Team, Control, true });
		return Support.MakeItem(EQueueItemType::HoldTimer, Tag);
	}

	/** One server frame: the clock advances (time and tick), then the Round tick, then the drain (the store's order). */
	void Frame(double Seconds = RoundTestFixtures::RigFrameSec)
	{
		Rig.Clock.Advance(Seconds, 1);
		Rig.Core->Tick();
		Queue->Drain();
	}

	/** Submits the items in order from one mailbox (one tick: consecutive Seqs), then one frame. */
	void DrainOneTick(TConstArrayView<FQueueItem> Items)
	{
		for (const FQueueItem& Item : Items)
		{
			Queue->Submit(FMailboxId::ServerLocal(), Item);
		}
		Frame();
	}

	/** Frames until Time (one tick each, the last landing exactly on Time). */
	void RunUntil(double Time)
	{
		while (Rig.Clock.GetServerTimeSeconds() < Time)
		{
			Frame(FMath::Min(RoundTestFixtures::RigFrameSec, Time - Rig.Clock.GetServerTimeSeconds()));
		}
	}

	/** The participant disconnects now (Session's fact, R17.2): Session's state and Round's handler. */
	void Disconnect(uint32 Participant, ERoundDisconnectReason Reason = ERoundDisconnectReason::Quit)
	{
		Rig.Fakes.Session.ConnectionStates.Add(Participant, ERoundConnectionState::Disconnected);
		Rig.Core->OnParticipantDisconnected(FParticipantId(Participant), Reason, Rig.Clock.GetServerTimeSeconds());
	}

	/** The participant's rejoin completed (after Session's acknowledgement): Session's state and Round's handler. */
	void Rejoin(uint32 Participant)
	{
		Rig.Fakes.Session.ConnectionStates.Add(Participant, ERoundConnectionState::Connected);
		Rig.Core->OnParticipantRejoined(FParticipantId(Participant));
	}

	int32 CountLogged(ERoundLogType Type) const { return Rig.EventLog.CountOfType(Type); }

	/** The logged entries of Type, in order. */
	TArray<FRoundLogEntry> Logged(ERoundLogType Type) const
	{
		return Rig.EventLog.Entries.FilterByPredicate([Type](const FRoundLogEntry& E) { return E.Type == Type; });
	}

	/** Index in the shared call log of the first call named Call at or after From, or INDEX_NONE. */
	int32 IndexOf(FName Call, int32 From = 0) const
	{
		for (int32 Index = From; Index < Rig.Log.Calls.Num(); ++Index)
		{
			if (Rig.Log.Calls[Index].Call == Call)
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}
};

#endif // WITH_STEALTH_TEST_SEAMS
