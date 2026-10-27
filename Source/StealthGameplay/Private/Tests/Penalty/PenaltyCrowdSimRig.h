#pragma once

// FPenaltyCore driving the headless crowd simulation (stories nkp-006 AC2 and AC3): the exile's AdoptReleasedPawn reaches
// the REAL crowd agent (FCrowdAgent::ScheduleAdoption: restricted until the lockout end, then the full catalogue at a leg
// boundary and, for an exile body, the next scheduled exit slot of the REAL ambient schedule, CNB-R9.1), exactly as the
// Crowd Director applies it when the body's crowd controller attaches. The sim (Tests/Crowd/CrowdSimWorld.h) stands in for
// LSC's sockets; Penalty's clock is the sim clock. Participants exile on their first NPC kill (L = 1 through the fake
// tuning), so every injected body is one exile.

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Tests/Crowd/CrowdSimWorld.h"
#include "Tests/Penalty/PenaltyTestRig.h"

namespace StealthPenaltyCrowdSim
{
	using namespace StealthCrowdSim;

	/** Penalty's clock is the sim's. */
	struct FSimClock final : IStealthClock
	{
		const FWorld* World = nullptr;
		virtual double GetServerTimeSeconds() const override { return World->GetNow(); }
		virtual uint64 GetServerTickIndex() const override { return static_cast<uint64>(World->GetFrame()); }
	};

	/** A pawn key of a sim ordinal (offset so no key is 0). */
	inline FPenaltyPawnKey KeyOfOrdinal(uint32 Ordinal) { return FPenaltyPawnKey(static_cast<uint64>(Ordinal) + 1000ull); }
	inline uint32 OrdinalOfKey(FPenaltyPawnKey Key) { return static_cast<uint32>(Key.Value - 1000ull); }

	/** The body gateway on the sim: the Roster release is recorded, the Crowd adoption is the real agent's. */
	struct FSimBodies final : IPenaltyBodyGateway
	{
		FWorld* World = nullptr;
		StealthPenaltyTest::FFakeRoster* Roster = nullptr;
		TArray<uint32> Adopted;
		TArray<double> RestrictedUntil;

		virtual ERosterResult RequestStrikeLimit(FParticipantId Participant, uint32 ExpectedRoundEpoch) override
		{
			Roster->States.Add(Participant, EParticipantState::Informant);
			return ERosterResult::Applied;
		}
		virtual void ReleaseCard(FPenaltyPawnKey Body) override {}
		virtual void AdoptReleasedPawn(FPenaltyPawnKey Body, double RestrictedUntilServerTime, bool bExitAtNextScheduledSlot) override
		{
			// UCrowdDirector::AttachDriver applies the pending adoption with the same arguments.
			if (FNpc* Npc = World->FindNpc(OrdinalOfKey(Body)))
			{
				Npc->Agent.ScheduleAdoption(World->GetNow(), RestrictedUntilServerTime, bExitAtNextScheduledSlot, false, 0.0, *World);
				Adopted.Add(OrdinalOfKey(Body));
				RestrictedUntil.Add(RestrictedUntilServerTime);
			}
		}
		virtual void RequestHandOverToAi(FPenaltyPawnKey Body, uint32 ExpectedRoundEpoch, double PossessNotBeforeServerTime) override {}
		virtual ERosterResult RetireBinding(FPenaltyPawnKey Body, double RetireAtServerTime) override { return ERosterResult::NoOp_NotBound; }
	};

	/** The ambient sockets of the crowd specs: Count alley mouths around the plane's edge. */
	inline void AddEdgeSockets(FWorld& World, int32 Count, double HalfCm)
	{
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const double Angle = 2.0 * UE_DOUBLE_PI * Index / Count;
			const FVector Out(FMath::Cos(Angle), FMath::Sin(Angle), 0.0);
			const FVector Edge = Out * HalfCm * 0.95;
			World.AddSocket(Edge - Out * 150.0, Edge);
		}
	}

	/** The sim, its ambient schedule started, and a Penalty core whose exiles release sim pawns. */
	struct FRig
	{
		FConfig Config;
		TUniquePtr<FWorld> World;
		FSimClock Clock;
		StealthPenaltyTest::FFakeRoster Roster;
		StealthPenaltyTest::FFakeTuning Tuning;
		StealthPenaltyTest::FFakeSlot Slot;
		StealthPenaltyTest::FRecordingLog Log;
		FStealthRecordingFaultSink Faults;
		FSimBodies Bodies;
		TUniquePtr<FPenaltyCore> Penalty;
		uint32 NextParticipant = 1;

		/** Sockets alley mouths; the crowd seed is Config's fixed default. */
		explicit FRig(int32 Sockets = 13)
		{
			Config.HalfSizeCm = 3000.0;
			World = MakeUnique<FWorld>(Config);
			AddEdgeSockets(*World, Sockets, Config.HalfSizeCm);
			World->SpawnNpcs();
			World->StartRound();
			Clock.World = World.Get();
			Bodies.World = World.Get();
			Bodies.Roster = &Roster;
			Tuning.Live.NpcKillStrikeLimit = 1; // every participant exiles on its first NPC kill (a test value, not L)
			FPenaltyCoreDeps Deps;
			Deps.Clock = &Clock;
			Deps.Roster = &Roster;
			Deps.Tuning = &Tuning;
			Deps.Slot = &Slot;
			Log.Slot = &Slot;
			Deps.Log = &Log;
			Deps.Bodies = &Bodies;
			Deps.Faults = &Faults;
			Penalty = MakeUnique<FPenaltyCore>(Deps);
		}

		/** A participant's walking body under the restricted driver (a player's pawn as the crowd will drive it). */
		FNpc& SpawnPlayerBody(const FVector& Location, float YawDeg)
		{
			return World->SpawnNpcAt(Location, YawDeg, ECrowdDriverKind::Restricted, Config.Locomotion.WalkSpeedCmPerSec, EGait::Walk);
		}

		/**
		 * Body's participant kills an NPC now: the exile (L = 1) releases Body through Penalty, its lockout ending
		 * AttackAttemptLockoutSec after the slot. Returns the participant.
		 */
		FParticipantId ExileNow(const FNpc& Body)
		{
			const FParticipantId Participant(NextParticipant++);
			Roster.States.Add(Participant, EParticipantState::Active);
			Slot.NextSlot();
			FPenaltyKill Kill{ FPenaltyPawnKey(900000 + Participant.Value), Participant, EVictimKind::Npc, EKillCause::Strike };
			Kill.KillerPawn = KeyOfOrdinal(Body.Body.Ordinal);
			Kill.KillerLockoutEndServerTime = World->GetNow() + StealthPenaltyTest::FRig::LockoutSec();
			Penalty->OnPawnKilled(Kill);
			Penalty->GetTallyForResult(Participant, EStrikeResult::WrongTarget, FClientCommandSeq(), Kill.KillerPawn);
			return Participant;
		}

		/** Total scheduled exit slots fired over every socket (the city-wide AmbientExit count, CNB-R9.1). */
		int32 ExitSlots() const
		{
			int32 Total = 0;
			for (const int32 Count : World->GetSchedule().GetExitsPerSocket())
			{
				Total += Count;
			}
			return Total;
		}
	};
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS
