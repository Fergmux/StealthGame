#pragma once

// FPenaltyCore over the REAL Roster core (stories nkp-006 AC1, nkp-008 AC1): FRosterCore with deterministic fakes for
// its injected dependencies (authority, game thread, pawn liveness, random, Round phase) and a Penalty body gateway that
// calls the Roster exactly as the production adapter does (RequestTransition(StrikeLimit) with the expected epoch,
// RetireBinding with InformantKilled). Pawns are Roster handles keyed by the Penalty pawn key (generation 1), so nothing
// needs a UObject. Penalty's Roster reads go through the same core. Crowd, Character Pool and the factory are recorded.

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Server/Roster/StealthRosterCore.h"
#include "Tests/Penalty/PenaltyTestRig.h"

namespace StealthPenaltyRealRoster
{
	struct FAuthority final : IRosterAuthority { virtual bool HasAuthority() const override { return true; } };
	struct FGameThread final : IRosterThreadCheck { virtual bool IsGameThread() const override { return true; } };
	struct FLiveness final : IRosterPawnLiveness
	{
		TSet<FRosterPawnHandle> Destroyed;
		virtual ERosterPawnLiveness GetLiveness(const FRosterPawnHandle& Pawn) const override
		{
			return Destroyed.Contains(Pawn) ? ERosterPawnLiveness::Destroyed : ERosterPawnLiveness::Valid;
		}
	};
	struct FRandom final : IRosterRandom
	{
		virtual uint64 Draw(ERosterRandomDomain Domain, uint64 Counter) override
		{
			return 0x9E3779B97F4A7C15ull * (Counter + 1) ^ (static_cast<uint64>(Domain) << 40);
		}
	};
	struct FRoundPhase final : IRosterRoundPhase
	{
		bool bActive = true;
		virtual bool IsRoundPhaseActive() const override { return bActive; }
	};
	/** Records every dispatched Roster event. */
	struct FEvents final : IRosterEventListener
	{
		TArray<FRosterEvent> Events;
		virtual void OnRosterEvent(const FRosterEvent& Event) override { Events.Add(Event); }
	};

	/** The Roster handle of a Penalty pawn key. */
	inline FRosterPawnHandle HandleOf(FPenaltyPawnKey Key) { return FRosterPawnHandle(Key.Value, 1); }

	/** IPenaltyRosterQuery over the real core. */
	struct FQuery final : IPenaltyRosterQuery
	{
		const FRosterCore* Core = nullptr;
		virtual TOptional<EParticipantState> GetState(FParticipantId Participant) const override
		{
			const FParticipantRecord* Record = Core->FindRecord(Participant);
			return Record != nullptr ? TOptional<EParticipantState>(Record->State) : TOptional<EParticipantState>();
		}
		virtual uint32 GetRoundEpoch() const override { return Core->GetRoundEpoch(); }
	};

	/** The body gateway: Roster calls on the real core (as UPenaltyService's FBodies), the rest recorded. */
	struct FBodies final : IPenaltyBodyGateway
	{
		FRosterCore* Core = nullptr;
		TArray<FString> Calls;
		double RestrictedUntil = -1.0;
		bool bExitAtNextSlot = false;

		virtual ERosterResult RequestStrikeLimit(FParticipantId Participant, uint32 ExpectedRoundEpoch) override
		{
			Calls.Add(TEXT("RequestTransition"));
			FRosterTransitionOptions Options;
			FRosterEpochToken Expected = Core->GetEpochToken();
			Expected.RoundEpoch = ExpectedRoundEpoch;
			Options.ExpectedEpoch = Expected;
			return Core->RequestTransition(Participant, EParticipantState::Informant, ERosterReason::StrikeLimit, Options);
		}
		virtual void ReleaseCard(FPenaltyPawnKey Body) override { Calls.Add(TEXT("ReleaseCard")); }
		virtual void AdoptReleasedPawn(FPenaltyPawnKey Body, double RestrictedUntilServerTime, bool bExitAtNextScheduledSlot) override
		{
			Calls.Add(TEXT("AdoptReleasedPawn"));
			RestrictedUntil = RestrictedUntilServerTime;
			bExitAtNextSlot = bExitAtNextScheduledSlot;
		}
		virtual void RequestHandOverToAi(FPenaltyPawnKey Body, uint32 ExpectedRoundEpoch, double PossessNotBeforeServerTime) override
		{
			Calls.Add(TEXT("RequestHandOver"));
		}
		virtual ERosterResult RetireBinding(FPenaltyPawnKey Body, double RetireAtServerTime) override
		{
			Calls.Add(TEXT("RetireBinding"));
			return Core->RetireBinding(HandleOf(Body), static_cast<int64>(FMath::RoundToDouble(RetireAtServerTime * 1000.0)), ERosterReason::InformantKilled);
		}
	};

	/**
	 * A 2v2 match on the real Roster (participants 0, 1 on team 0; 2, 3 on team 1), every participant Active and InRound
	 * with a bound body (keys 101..104), and FPenaltyCore wired to it.
	 */
	struct FRig
	{
		FFakeStealthClock Clock{ 100.0, 1 };
		FStealthRecordingFaultSink RosterFaults;
		FAuthority Authority;
		FGameThread GameThread;
		FLiveness Liveness;
		FRandom Random;
		FRoundPhase Phase;
		FEvents Events;
		TUniquePtr<FRosterCore> Roster;
		FQuery Query;
		StealthPenaltyTest::FFakeTuning Tuning;
		StealthPenaltyTest::FFakeSlot Slot;
		StealthPenaltyTest::FRecordingLog Log;
		StealthPenaltyTest::FRecordingRespawns Respawns;
		FStealthRecordingFaultSink PenaltyFaults;
		FBodies Bodies;
		TUniquePtr<FPenaltyCore> Penalty;
		TArray<FParticipantId> Ids;

		/** The fixture body of participant Index (0..3). */
		static FPenaltyPawnKey BodyOf(int32 Index) { return FPenaltyPawnKey(101 + Index); }

		FRig()
		{
			FRosterCoreDeps Deps;
			Deps.Liveness = &Liveness;
			Deps.Authority = &Authority;
			Deps.ThreadCheck = &GameThread;
			Deps.Clock = &Clock;
			Deps.Random = &Random;
			Deps.FaultSink = &RosterFaults;
			Deps.RoundPhase = &Phase;
			Roster = MakeUnique<FRosterCore>(Deps);
			Roster->BeginMatch(2);
			TArray<FRosterTeamAssignment> Batch;
			for (int32 Index = 0; Index < 4; ++Index)
			{
				const FParticipantId Id = Roster->RegisterParticipant(FIdentityKey(0xFEED, 1000 + Index), FString::Printf(TEXT("P%d"), Index)).Id;
				Ids.Add(Id);
				Batch.Add({ Id, Index < 2 ? 0 : 1 });
			}
			Roster->AssignTeams(Batch);
			Roster->SetTeamsLocked(true);
			Roster->ResetAll(1); // every record Active and InRound, RoundEpoch 1
			for (int32 Index = 0; Index < 4; ++Index)
			{
				Roster->BindPawn(Ids[Index], HandleOf(BodyOf(Index)));
			}
			Roster->AddListener(&Events);
			Query.Core = Roster.Get();
			Bodies.Core = Roster.Get();
			Log.Slot = &Slot;
			FPenaltyCoreDeps PenaltyDeps;
			PenaltyDeps.Clock = &Clock;
			PenaltyDeps.Roster = &Query;
			PenaltyDeps.Tuning = &Tuning;
			PenaltyDeps.Slot = &Slot;
			PenaltyDeps.Log = &Log;
			PenaltyDeps.Bodies = &Bodies;
			PenaltyDeps.Respawns = &Respawns;
			PenaltyDeps.Faults = &PenaltyFaults;
			Penalty = MakeUnique<FPenaltyCore>(PenaltyDeps);
		}

		~FRig()
		{
			Roster->RemoveListener(&Events);
		}

		/** One executed Attack in its own slot: OnPawnKilled then the tally, as the resolver orders them. */
		FStrikeTally Attack(int32 Attacker, FPenaltyKill Kill, EStrikeResult Result, EPenaltyKillOutcome* OutOutcome = nullptr)
		{
			Slot.NextSlot();
			Clock.Advance(0.25, 1);
			const EPenaltyKillOutcome Outcome = Penalty->OnPawnKilled(Kill);
			if (OutOutcome != nullptr)
			{
				*OutOutcome = Outcome;
			}
			const FParticipantId Participant = Roster->GetParticipant(HandleOf(BodyOf(Attacker)));
			return Penalty->GetTallyForResult(Participant, Result, FClientCommandSeq(), BodyOf(Attacker));
		}

		/** An NPC kill by participant Index (the NPC key is any key no participant holds). */
		FStrikeTally NpcKill(int32 Attacker, uint64 Npc, EPenaltyKillOutcome* OutOutcome = nullptr)
		{
			FPenaltyKill Kill{ FPenaltyPawnKey(Npc), Ids[Attacker], EVictimKind::Npc, EKillCause::Strike };
			Kill.KillerPawn = BodyOf(Attacker);
			Kill.KillerLockoutEndServerTime = Clock.GetServerTimeSeconds() + 0.25 + StealthPenaltyTest::FRig::LockoutSec();
			return Attack(Attacker, Kill, EStrikeResult::WrongTarget, OutOutcome);
		}

		/** A kill of Victim's body by Attacker with the victim kind the Roster resolves now (as Assassination reads it). */
		FStrikeTally KillBody(int32 Attacker, FPenaltyPawnKey Victim, EPenaltyKillOutcome* OutOutcome = nullptr)
		{
			const FRosterVictimKind Kind = Roster->ResolveVictimKind(HandleOf(Victim));
			FPenaltyKill Kill{ Victim, Ids[Attacker],
				Kind.Kind == ERosterVictimKind::Informant ? EVictimKind::Informant : (Kind.Kind == ERosterVictimKind::Player ? EVictimKind::Player : EVictimKind::Npc),
				EKillCause::Strike };
			Kill.KillerPawn = BodyOf(Attacker);
			Kill.VictimParticipant = Roster->GetParticipant(HandleOf(Victim));
			Kill.VictimDeathServerTime = Clock.GetServerTimeSeconds() + 0.25 + 0.4;
			const EStrikeResult Result = Kind.Kind == ERosterVictimKind::Player ? EStrikeResult::RivalEliminated : EStrikeResult::WrongTarget;
			return Attack(Attacker, Kill, Result, OutOutcome);
		}
	};
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS
