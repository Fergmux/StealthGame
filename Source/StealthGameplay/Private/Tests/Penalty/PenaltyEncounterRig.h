#pragma once

// The Penalty integration rig (stories nkp-005 AC2/AC3, nkp-007, nkp-008): FPenaltyCore wired to the REAL strike
// resolver on the REAL round queue with the REAL Round core (FRoundEncounterRig, Assassination stories asn-007..010), the
// way the composition root wires the production service: Penalty's OnPawnKilled subscription inside the Attack's slot, and
// the resolver's IStrikeTallySource answered by the core after the fan-out. Penalty's body gateway acts on the rig's
// Roster fakes as the Roster does: StrikeLimit releases the body (Unbound: it resolves Npc and its participant is no
// longer named) and raises the transition event Round's F2 check evaluates in the same slot; RetireBinding makes the body
// resolve NotTargetable(AlreadyResolved). The Event Log rows go through Round's writer into the rig's recorder.

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Tests/Assassination/EncounterRoundRig.h"
#include "Tests/Penalty/PenaltyTestRig.h"

namespace StealthPenaltyEncounterTest
{
	using namespace StealthEncounterTest;

	/** A pawn key from the pawn's address (the rig never recycles a pawn within a test). */
	inline FPenaltyPawnKey KeyOf(const APawn* Pawn) { return FPenaltyPawnKey(static_cast<uint64>(reinterpret_cast<UPTRINT>(Pawn))); }

	/** IPenaltyRosterQuery over Round's fake Roster (the state Round's F2 check reads). */
	struct FRoundRosterQuery final : IPenaltyRosterQuery
	{
		FRoundWinQueueRig* W = nullptr;

		virtual TOptional<EParticipantState> GetState(FParticipantId Participant) const override
		{
			const FRoundRosterMember* Member = W->Rig.Fakes.Roster.Members.FindByPredicate([Participant](const FRoundRosterMember& M) { return M.Participant == Participant; });
			return Member != nullptr ? TOptional<EParticipantState>(Member->State) : TOptional<EParticipantState>();
		}
		virtual uint32 GetRoundEpoch() const override { return StealthPenaltyTest::FixtureEpoch; }
	};

	/** IPenaltySlotContext over the real round queue. */
	struct FQueueSlotContext final : IPenaltySlotContext
	{
		FResolutionQueueCore* Queue = nullptr;

		virtual bool IsInSlot() const override { return Queue != nullptr && Queue->IsInSlot(); }
		virtual FQueueKey CurrentKey() const override { return Queue != nullptr ? Queue->CurrentKey() : FQueueKey(); }
	};

	/** The body gateway on the rig's Roster fakes, recording each call with the slot it ran in. */
	struct FRigBodies final : IPenaltyBodyGateway
	{
		FRoundEncounterRig* R = nullptr;
		const IPenaltySlotContext* Slot = nullptr;
		TMap<FPenaltyPawnKey, APawn*> Pawns;
		struct FCall
		{
			FString Name;
			FParticipantId Participant;
			APawn* Body = nullptr;
			FQueueKey SlotKey;
		};
		TArray<FCall> Calls;

		void Record(const TCHAR* Name, FParticipantId Participant, FPenaltyPawnKey Body)
		{
			APawn* const* Found = Pawns.Find(Body);
			Calls.Add({ Name, Participant, Found != nullptr ? *Found : nullptr, Slot != nullptr ? Slot->CurrentKey() : FQueueKey() });
		}
		virtual ERosterResult RequestStrikeLimit(FParticipantId Participant, uint32 ExpectedRoundEpoch) override
		{
			Record(TEXT("RequestTransition"), Participant, FPenaltyPawnKey());
			// The Roster's StrikeLimit: the live pawn is released (Unbound, an NPC), then the event reaches Round's F2 check.
			for (auto It = R->Roster.Members.CreateIterator(); It; ++It)
			{
				if (It->Value.Participant == Participant.Value)
				{
					It.RemoveCurrent();
				}
			}
			R->W.Kill(Participant.Value, ERosterReason::StrikeLimit);
			return ERosterResult::Applied;
		}
		virtual void ReleaseCard(FPenaltyPawnKey Body) override { Record(TEXT("ReleaseCard"), FParticipantId(), Body); }
		virtual void AdoptReleasedPawn(FPenaltyPawnKey Body, double RestrictedUntilServerTime, bool bExitAtNextScheduledSlot) override
		{
			Record(TEXT("AdoptReleasedPawn"), FParticipantId(), Body);
		}
		virtual void RequestHandOverToAi(FPenaltyPawnKey Body, uint32 ExpectedRoundEpoch, double PossessNotBeforeServerTime) override
		{
			Record(TEXT("RequestHandOver"), FParticipantId(), Body);
		}
		virtual ERosterResult RetireBinding(FPenaltyPawnKey Body, double RetireAtServerTime) override
		{
			Record(TEXT("RetireBinding"), FParticipantId(), Body);
			APawn* const* Found = Pawns.Find(Body);
			if (Found == nullptr)
			{
				return ERosterResult::NoOp_NotBound;
			}
			if (R->Roster.Resolved.Contains(*Found))
			{
				return ERosterResult::NoOp_AlreadyRetired;
			}
			R->Roster.Resolved.Add(*Found);
			return ERosterResult::Applied;
		}
		int32 Count(const TCHAR* Name) const
		{
			return Calls.FilterByPredicate([Name](const FCall& Call) { return Call.Name == Name; }).Num();
		}
	};

	/** A recording fallback for the non-strike commands (Interact, Hide, Traverse): the deliveries a body received. */
	class FRecordingFallback final : public IQueueResolver
	{
	public:
		TArray<const APawn*> Delivered;
		virtual EResolveResult Resolve(const FQueueItem& Item, FQueueKey Key) override
		{
			const FCommandPayload* Cmd = Item.Payload.TryGet<FCommandPayload>();
			Delivered.Add(Cmd != nullptr ? Cmd->Pawn.Pawn.Get() : nullptr);
			return EResolveResult::Executed;
		}
		int32 CountFor(const APawn* Pawn) const
		{
			return Delivered.FilterByPredicate([Pawn](const APawn* P) { return P == Pawn; }).Num();
		}
	};

	struct FPenaltyEncounterRig
	{
		FRoundEncounterRig R;
		FRoundRosterQuery RosterQuery;
		FQueueSlotContext Slot;
		StealthPenaltyTest::FFakeTuning Tuning;
		StealthPenaltyTest::FRecordingRespawns Respawns;
		FStealthRecordingFaultSink PenaltyFaults;
		FRigBodies Bodies;
		FRecordingFallback Fallback;
		TUniquePtr<FPenaltyCore> Penalty;
		/** The tally the core returned for each executed Attack, by attacker pawn, in order. */
		TArray<TPair<const APawn*, FStrikeTally>> Tallies;
		/** OnPawnKilled outcomes, in order. */
		TArray<EPenaltyKillOutcome> Outcomes;

		/** bProductionWrites: the resolver's own FActionState / death writes, recorded (FRoundEncounterRig). */
		explicit FPenaltyEncounterRig(const TCHAR* Name, int32 Limit = 0, bool bProductionWrites = false)
			: R(Name, bProductionWrites)
		{
			R.W.ToActive(RoundTestFixtures::Layout2v2.Team0, RoundTestFixtures::Layout2v2.Team1);
			if (Limit > 0)
			{
				Tuning.Live.NpcKillStrikeLimit = Limit;
			}
			RosterQuery.W = &R.W;
			Slot.Queue = R.W.Queue.Get();
			Bodies.R = &R;
			Bodies.Slot = &Slot;
			FPenaltyCoreDeps Deps;
			Deps.Clock = &R.W.Rig.Clock;
			Deps.Roster = &RosterQuery;
			Deps.Tuning = &Tuning;
			Deps.Slot = &Slot;
			Deps.Log = R.W.Rig.Core->GetRoundLogWriter();
			Deps.Bodies = &Bodies;
			Deps.Respawns = &Respawns;
			Deps.Faults = &PenaltyFaults;
			Penalty = MakeUnique<FPenaltyCore>(Deps);
			R.Core->SetFallbackResolvers(&Fallback, nullptr);
			// The service's OnPawnKilled subscription (UPenaltyService::HandlePawnKilled), inline in the Attack's slot.
			R.Core->OnPawnKilled.AddLambda([this](APawn* Victim, APawn* Killer, EVictimKind Kind, EKillCause Cause)
			{
				FPenaltyKill Kill;
				Kill.Victim = KeyOf(Victim);
				Kill.Killer = R.Roster.GetParticipant(Killer);
				Kill.VictimKind = Kind;
				Kill.Cause = Cause;
				Kill.KillerPawn = KeyOf(Killer);
				const FDefenceStateCore* KillerState = R.Defence.Find(Killer);
				Kill.KillerLockoutEndServerTime = KillerState != nullptr ? KillerState->GetLockoutEnd() : -1.0;
				Kill.VictimParticipant = R.Roster.GetParticipant(Victim);
				const TOptional<double> Death = R.Core->GetScheduledDeathServerTime(Victim);
				Kill.VictimDeathServerTime = Death.IsSet() ? Death.GetValue() : 0.0;
				Bodies.Pawns.Add(Kill.Victim, Victim);
				Bodies.Pawns.Add(Kill.KillerPawn, Killer);
				Outcomes.Add(Penalty->OnPawnKilled(Kill));
			});
			// The service's IStrikeTallySource: after the fan-out, by the attacker's participant (or its released body's).
			R.Tally.Answer = [this](const APawn* Attacker, EStrikeResult Result)
			{
				const FStrikeTally Tally = Penalty->GetTallyForResult(R.Roster.GetParticipant(Attacker), Result, FClientCommandSeq(), KeyOf(Attacker));
				Tallies.Add(TPair<const APawn*, FStrikeTally>(Attacker, Tally));
				return Tally;
			};
		}

		/** An NPC in front of Attacker (150 cm along +X of the attacker's scripted location). */
		APawn* NpcInFrontOf(const APawn* Attacker, double SideCm = 0.0)
		{
			const FEncounterPawnFacts* Facts = R.Pawns.Find(Attacker);
			const FVector At = (Facts != nullptr ? Facts->Location : FVector::ZeroVector) + FVector(150.0, SideCm, 0.0);
			return R.Spawn(At);
		}

		/** Lets Attacker's strike lockout run out (roles expire by stamped time; one empty tick runs due items). */
		void SettleLockout()
		{
			R.W.Rig.Clock.Advance(static_cast<double>(FAssassinationTuning_Public().AttackAttemptLockoutSec) + 0.1, 1);
			R.Tick();
		}

		/** Attacker kills Count NPCs, one drain each, its lockout settled after each (the arranged k). */
		void ArrangeNpcKills(APawn* Attacker, int32 Count)
		{
			for (int32 Index = 0; Index < Count; ++Index)
			{
				APawn* Npc = NpcInFrontOf(Attacker);
				R.Drain({ R.AttackAt(Attacker, Npc) });
				R.Pawns.Remove(Npc); // the body leaves the candidate set (its removal is not under test)
				SettleLockout();
			}
		}

		/** The StrikeCounted rows the Event Log recorded with Key. */
		int32 CountStrikeCounted(FQueueKey Key) const
		{
			return R.W.Rig.EventLog.Entries.FilterByPredicate([Key](const FRoundLogEntry& E) { return E.Type == ERoundLogType::StrikeCounted && E.Key == Key; }).Num();
		}
		int32 CountRows(ERoundLogType Type) const
		{
			return R.W.Rig.EventLog.Entries.FilterByPredicate([Type](const FRoundLogEntry& E) { return E.Type == Type; }).Num();
		}
		/** Round's StateTransition row with Reason for Participant (the StrikeLimit row is Round's). */
		int32 CountTransitionRows(FParticipantId Participant, ERosterReason Reason) const
		{
			return R.W.Rig.EventLog.Entries.FilterByPredicate([Participant, Reason](const FRoundLogEntry& E)
			{
				return E.Type == ERoundLogType::StateTransition && E.Participants[0] == Participant.Value && E.Code == static_cast<uint16>(Reason);
			}).Num();
		}
	};
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS
