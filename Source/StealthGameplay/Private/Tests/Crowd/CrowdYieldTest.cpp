// Story cnb-011 (production/epics/crowd-npc-behaviour/story-011-avoidance-and-yield.md): avoidance as Move offsets
// (ADR-0016 section 4.2; GDD CNB-R3.3, AC-CNB-32; SCL R16).
//   YieldShare        2,000 scripted predicted approaches under 100 cm (1,000 with a scripted, player-like walker that
//                     only holds a Move, 1,000 with an NPC partner on a leg): the NPC yield share is CrowdYieldPct +-3
//                     points for both partner kinds, each decision taken once at first detection, and the side-step is
//                     an offset of the NPC's own carrot (nothing writes another pawn's motion: no pawn is pushed).
//   OffsetRules       the side comes from the relative position, keep-right on a tie, a per-pawn coin only on an exact
//                     head-on; the yield offset applies after the U(0.2, 0.6) s jitter (a due time); planted and tending
//                     pawns are static obstacles always stepped around; concealed or transitioning pawns are ignored.
//   BlockedLadder     blocked over 0.5 s widens to 100 cm, blocked 3 s repaths once, two failed repaths abandon the leg.
//   NoRvo             bUseRVOAvoidance is false on the Shared Pawn class defaults.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Server/CrowdAvoidance.h"
#include "Server/CrowdDraws.h"
#include "Server/CrowdFollower.h"
#include "Tests/Crowd/CrowdSimWorld.h"
#include "View/Core/StealthPawn.h"
#include "View/Core/StealthPawnMovementComponent.h"

namespace CrowdYieldTestData
{
	FCrowdPerceivedPawn Walker(uint64 Key, const FVector& Location, const FVector& Velocity)
	{
		FCrowdPerceivedPawn Pawn;
		Pawn.Key = Key;
		Pawn.Location = Location;
		Pawn.VelocityCmPerSec = Velocity;
		return Pawn;
	}

	/**
	 * One approach trial: the subject NPC (ordinal Trial) walks +X from the origin toward (2000, 0); the partner starts at
	 * (900, Lateral) walking -X. bNpcPartner: the partner is an NPC agent on a leg; otherwise a scripted walker holding a
	 * Move (a player-like pawn). Returns the subject's yield decision (-1: no approach detected).
	 */
	int32 RunTrial(int32 Trial, bool bNpcPartner)
	{
		using namespace StealthCrowdSim;
		FConfig Config;
		Config.NpcCount = 2;
		Config.SubSeed = 0xA11CEull;
		// The subject's ordinal is unique per trial, so each trial reads its own draws (the same index for both partner kinds).
		Config.FirstNpcOrdinal = static_cast<uint32>(Trial) * 4;
		FWorld World(Config);
		const double Lateral = 20.0 + (Trial % 7) * 10.0;
		TMap<uint32, FVector> Goals;
		World.SetRouteFixture([&Goals](uint32 Ordinal, const FVector&, FVector& OutGoal)
		{
			if (const FVector* Goal = Goals.Find(Ordinal))
			{
				OutGoal = *Goal;
				Goals.Remove(Ordinal);
				return true;
			}
			return false;
		});
		FNpc& Subject = World.SpawnNpcAt(FVector::ZeroVector, 0.0f);
		Goals.Add(Subject.Body.Ordinal, FVector(2000.0, 0.0, 0.0));
		FNpc* PartnerNpc = nullptr;
		int32 Scripted = INDEX_NONE;
		if (bNpcPartner)
		{
			PartnerNpc = &World.SpawnNpcAt(FVector(900.0, Lateral, 0.0), 180.0f);
			Goals.Add(PartnerNpc->Body.Ordinal, FVector(-1100.0, Lateral, 0.0));
		}
		else
		{
			Scripted = World.AddScripted(FVector(900.0, Lateral, 0.0), 180.0f);
			FScripted& Player = World.GetScripted(Scripted);
			Player.Body.MoveIntent = FVector2D(0.0, 1.0); // forward under a 180 degree camera: walking -X
		}
		World.StartRound();
		for (FNpc* Npc : { &Subject, PartnerNpc })
		{
			if (Npc != nullptr && Npc->Agent.GetActivity() != ECrowdActivity::Routing)
			{
				Npc->Agent.ForceLeg(World.GetNow(), World.FactsOf(Npc->Body), World, *Npc);
			}
		}
		const FCrowdAvoidance& Avoidance = Subject.Agent.GetFollower().GetAvoidance();
		for (int32 Frame = 0; Frame < 60 * 8 && Avoidance.GetApproachCount() == 0; ++Frame)
		{
			World.Step();
		}
		if (Avoidance.GetApproachCount() == 0)
		{
			return -1;
		}
		return Avoidance.GetYieldCount() > 0 ? 1 : 0;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdYieldShareTest, "StealthGame.Crowd.Avoidance.YieldShare",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdYieldShareTest::RunTest(const FString& Parameters)
{
	using namespace CrowdYieldTestData;
	const double YieldPct = FCrowdTuning_Server().CrowdYieldPct;
	int32 Yields[2] = { 0, 0 };
	int32 Approaches[2] = { 0, 0 };
	int32 Undetected = 0;
	int32 Differ = 0;
	for (int32 Trial = 0; Trial < 1000; ++Trial)
	{
		const int32 WithPlayer = RunTrial(Trial, false);
		const int32 WithNpc = RunTrial(Trial, true);
		for (int32 Kind = 0; Kind < 2; ++Kind)
		{
			const int32 Result = Kind == 0 ? WithPlayer : WithNpc;
			if (Result < 0)
			{
				++Undetected;
				continue;
			}
			++Approaches[Kind];
			Yields[Kind] += Result;
		}
		Differ += WithPlayer >= 0 && WithNpc >= 0 && WithPlayer != WithNpc ? 1 : 0;
	}
	const double PlayerShare = Approaches[0] > 0 ? 100.0 * Yields[0] / Approaches[0] : 0.0;
	const double NpcShare = Approaches[1] > 0 ? 100.0 * Yields[1] / Approaches[1] : 0.0;
	AddInfo(FString::Printf(TEXT("YIELD player-like partner %.2f %% of %d, NPC partner %.2f %% of %d (target %.0f %%), undetected %d, trials deciding differently %d"),
		PlayerShare, Approaches[0], NpcShare, Approaches[1], YieldPct, Undetected, Differ));
	TestTrue(TEXT("approaches were detected in (nearly) every trial"), Approaches[0] >= 990 && Approaches[1] >= 990);
	TestTrue(TEXT("yield share with a player-like partner within CrowdYieldPct +-3 (AC-CNB-32)"), FMath::Abs(PlayerShare - YieldPct) <= 3.0);
	TestTrue(TEXT("yield share with an NPC partner within CrowdYieldPct +-3 (AC-CNB-32)"), FMath::Abs(NpcShare - YieldPct) <= 3.0);
	// Kind-blind by construction: the one draw per approach is the same tuple whatever the partner is.
	TestEqual(TEXT("the decision never depends on the partner kind"), Differ, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdAvoidanceOffsetRulesTest, "StealthGame.Crowd.Avoidance.OffsetRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdAvoidanceOffsetRulesTest::RunTest(const FString& Parameters)
{
	using namespace CrowdYieldTestData;
	FCrowdTuning_Server Tuning;
	Tuning.CrowdYieldPct = 100.0f; // every approach yields, so the side and the jitter are observable
	const FCrowdPerceivedPawn Self = Walker(1, FVector::ZeroVector, FVector(140.0, 0.0, 0.0));
	auto FirstOffset = [&Tuning, &Self](const FCrowdPerceivedPawn& Other, double& OutActiveAfter)
	{
		FCrowdDraws Draws(0xA);
		FCrowdAvoidance Avoidance;
		const FCrowdPerceivedPawn* Neighbour = &Other;
		double Offset = 0.0;
		OutActiveAfter = -1.0;
		for (int32 Frame = 0; Frame < 120; ++Frame)
		{
			const double Now = Frame / 60.0;
			const FCrowdAvoidanceOutput Out = Avoidance.Step(1, Self, MakeArrayView(&Neighbour, 1), Now, true, false, Tuning, Draws);
			if (Out.LateralOffsetCm != 0.0 && OutActiveAfter < 0.0)
			{
				OutActiveAfter = Now;
				Offset = Out.LateralOffsetCm;
			}
		}
		return Offset;
	};
	double After = 0.0;
	// The other walker slightly to the left (+Y is right in UE: a negative Y is left): step right (+).
	const double Right = FirstOffset(Walker(2, FVector(400.0, -30.0, 0.0), FVector(-140.0, 0.0, 0.0)), After);
	TestEqual(TEXT("an approach on the left: step right by CrowdAvoidOffsetCm"), Right, static_cast<double>(Tuning.CrowdAvoidOffsetCm));
	TestTrue(TEXT("after the U(0.2, 0.6) s jitter"), After >= Tuning.CrowdYieldJitterMinSec - 1e-6 && After <= Tuning.CrowdYieldJitterMaxSec + 1.0 / 60.0);
	const double Left = FirstOffset(Walker(3, FVector(400.0, 30.0, 0.0), FVector(-140.0, 0.0, 0.0)), After);
	TestEqual(TEXT("an approach on the right: step left"), Left, -static_cast<double>(Tuning.CrowdAvoidOffsetCm));
	const double Tie = FirstOffset(Walker(4, FVector(400.0, -5.0, 0.0), FVector(-140.0, 0.0, 0.0)), After);
	TestEqual(TEXT("a near tie: keep right"), Tie, static_cast<double>(Tuning.CrowdAvoidOffsetCm));
	const double HeadOn = FirstOffset(Walker(5, FVector(400.0, 0.0, 0.0), FVector(-140.0, 0.0, 0.0)), After);
	TestEqual(TEXT("an exact head-on: the per-pawn coin picks a side"), FMath::Abs(HeadOn), static_cast<double>(Tuning.CrowdAvoidOffsetCm));
	// A planted pawn ahead: always stepped around, without a yield draw.
	FCrowdPerceivedPawn Planted = Walker(6, FVector(150.0, 20.0, 0.0), FVector::ZeroVector);
	Planted.bStaticObstacle = true;
	FCrowdTuning_Server NoYield;
	NoYield.CrowdYieldPct = 0.0f;
	FCrowdDraws Draws(0xB);
	FCrowdAvoidance Avoidance;
	const FCrowdPerceivedPawn* PlantedPtr = &Planted;
	const FCrowdAvoidanceOutput Around = Avoidance.Step(1, Self, MakeArrayView(&PlantedPtr, 1), 0.0, true, false, NoYield, Draws);
	TestTrue(TEXT("a planted pawn is a static obstacle, stepped around even at CrowdYieldPct 0"), Around.LateralOffsetCm < 0.0);
	// A concealed or transitioning pawn is not in the Steering query at all (story cnb-008 filter), so it is never avoided.
	FCrowdPerceivedPawn Hidden = Walker(7, FVector(300.0, 0.0, 0.0), FVector(-140.0, 0.0, 0.0));
	Hidden.bObstacle = false;
	FCrowdSpatialHash Hash(200.0);
	Hash.Build(MakeArrayView(&Hidden, 1));
	TArray<const FCrowdPerceivedPawn*> Found;
	Hash.Query(FVector::ZeroVector, 600.0, ECrowdQuery::Steering, 1, Found);
	TestEqual(TEXT("a not-an-obstacle pawn is never an avoidance obstacle"), Found.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdAvoidanceBlockedLadderTest, "StealthGame.Crowd.Avoidance.BlockedLadder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdAvoidanceBlockedLadderTest::RunTest(const FString& Parameters)
{
	const FCrowdTuning_Server Tuning;
	const FLocomotionTuning_Public Locomotion;
	FCrowdDraws Draws(0xC);
	FCrowdFollower Follower;
	Follower.Initialize(1, 0.0, Tuning, Draws);
	const FVector Goal(3000.0, 0.0, 0.0);
	Follower.StartLeg(1, FVector::ZeroVector, MakeArrayView(&Goal, 1), 0.0, Tuning, Draws);
	FCrowdPerceivedPawn Self;
	Self.Key = 1;
	// Blocked: the pawn wants to walk but does not move (a wall of people).
	int32 Repaths = 0;
	double FirstRepath = -1.0;
	double AbandonedAt = -1.0;
	for (int32 Frame = 0; Frame < 60 * 20 && AbandonedAt < 0.0; ++Frame)
	{
		const double Now = Frame / 60.0;
		const FCrowdFollowerOutput Out = Follower.Tick(Now, 1.0f / 60.0f, Self, {}, Tuning, Locomotion, Draws);
		if (Out.bWantsRepath)
		{
			++Repaths;
			FirstRepath = FirstRepath < 0.0 ? Now : FirstRepath;
			// The repath "succeeds" with the same corners: the pawn is still blocked.
			Follower.ReplacePath(Self.Location, MakeArrayView(&Goal, 1));
		}
		if (Out.bAbandoned)
		{
			AbandonedAt = Now;
		}
	}
	AddInfo(FString::Printf(TEXT("BLOCKED first repath at %.2f s, %d repaths, abandoned at %.2f s"), FirstRepath, Repaths, AbandonedAt));
	TestTrue(TEXT("blocked 3 s (after the 1 s start grace) repaths"), FirstRepath > Tuning.CrowdBlockedRepathSec && FirstRepath < Tuning.CrowdBlockedRepathSec + 1.5);
	TestEqual(TEXT("two repaths"), Repaths, 2);
	TestTrue(TEXT("after two failed repaths the leg is abandoned"), AbandonedAt > FirstRepath);
	TestFalse(TEXT("an abandoned leg stops (Move(0))"), Follower.IsOnLeg());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdAvoidanceNoRvoTest, "StealthGame.Crowd.Avoidance.NoRvo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdAvoidanceNoRvoTest::RunTest(const FString& Parameters)
{
	const AStealthPawn* Cdo = GetDefault<AStealthPawn>();
	const UCharacterMovementComponent* Movement = Cdo != nullptr ? Cdo->GetCharacterMovement() : nullptr;
	if (TestNotNull(TEXT("the Shared Pawn CDO has a movement component"), Movement))
	{
		TestFalse(TEXT("bUseRVOAvoidance is false on the Shared Pawn class defaults"), Movement->bUseRVOAvoidance);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
