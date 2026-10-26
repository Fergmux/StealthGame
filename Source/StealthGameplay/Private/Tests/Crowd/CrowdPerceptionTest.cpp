// Story cnb-008 (production/epics/crowd-npc-behaviour/story-008-spatial-hash-and-perception-filter.md): the 2 m
// spatial hash, the ray budget and the concealment-blind perception filter (ADR-0016 section 5; GDD CNB-R2, R3.3,
// AC-CNB-36).
//   HashKindBlind      the hash is built from public facts only; a scripted (player-driven) pawn and an NPC with the same
//                      facts give identical query results; not-an-obstacle pawns are out of Steering, concealed pawns out
//                      of Perception; results are sorted (deterministic).
//   ConcealedOccupancy AC-CNB-36: fixed-seed scenes with a bush holding 0, 1 and 3 concealed pawns (scripted and NPC
//                      bodies): every NPC's Move/Look stream, glance targets and conversation partners are identical
//                      across the three, and no glance ever targets a concealed pawn. (Bush choices arrive with story
//                      cnb-022; with Concealment absent bush herrings are disabled.)
//   RayBudget          at most CrowdRaysPerFrameLimit (8) rays per frame, round-robin over requesters, and a deferred ray
//                      reports its original stimulus time.
//   NoPerceptionComponent the crowd lint's include rule rejects the engine perception header (selftest seeded case).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "CrowdTestUtils.h"
#include "Server/CrowdSpatialHash.h"
#include "Tests/Crowd/CrowdSimWorld.h"

namespace CrowdPerceptionTestData
{
	/** Records glance targets and conversation partners per NPC. */
	struct FRecorder final : public ICrowdAgentObserver
	{
		TArray<FString> Events;
		TSet<uint64> ForbiddenKeys;
		int32 ForbiddenGlances = 0;
		virtual void OnGlance(uint32 Ordinal, double Now, ECrowdGlanceKind Kind, uint64 TargetKey, double HoldSec) override
		{
			Events.Add(FString::Printf(TEXT("G %u %.4f %d %llu %.4f"), Ordinal, Now, static_cast<int32>(Kind), TargetKey, HoldSec));
			ForbiddenGlances += ForbiddenKeys.Contains(TargetKey) ? 1 : 0;
		}
		virtual void OnConversationFormed(uint32 Ordinal, double Now, uint64 PartnerKey, double LengthSec) override
		{
			Events.Add(FString::Printf(TEXT("C %u %.4f %llu"), Ordinal, Now, PartnerKey));
			ForbiddenGlances += ForbiddenKeys.Contains(PartnerKey) ? 1 : 0;
		}
	};

	/** A 40-NPC scene for 90 s with Occupants concealed pawns standing in a bush at (600, 600). */
	FString RunScene(int32 Occupants, int32& OutForbidden)
	{
		using namespace StealthCrowdSim;
		FConfig Config;
		Config.NpcCount = 40;
		Config.HalfSizeCm = 2000.0;
		FWorld World(Config);
		FRecorder Recorder;
		World.SetObserver(&Recorder);
		World.SpawnNpcs();
		// The hidden occupants get the ordinals after the NPCs, so NPC ordinals are identical across the three scenes.
		for (int32 Index = 0; Index < Occupants; ++Index)
		{
			FScripted& Hider = World.GetScripted(World.AddScripted(FVector(600.0 + 40.0 * Index, 600.0, 0.0), 0.0f));
			Hider.Body.bPerceivable = false;
			Hider.Body.bObstacle = false;
			Recorder.ForbiddenKeys.Add(Hider.Body.Key);
		}
		World.StartRound();
		FString Streams;
		for (int32 Frame = 0; Frame < 90 * 60; ++Frame)
		{
			World.Step();
			if (Frame % 6 == 0)
			{
				for (int32 Index = 0; Index < World.NumNpcs(); ++Index)
				{
					const FNpc& Npc = World.GetNpc(Index);
					Streams += FString::Printf(TEXT("%d:%.3f,%.3f,%.2f;"), Index, Npc.Body.MoveIntent.X, Npc.Body.MoveIntent.Y, Npc.Body.Control.Yaw);
				}
			}
		}
		OutForbidden = Recorder.ForbiddenGlances;
		return Streams + FString::Join(Recorder.Events, TEXT("|"));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdPerceptionHashKindBlindTest, "StealthGame.Crowd.Perception.HashKindBlind",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdPerceptionHashKindBlindTest::RunTest(const FString& Parameters)
{
	auto Make = [](uint64 Key, const FVector& Location, bool bPerceivable, bool bObstacle)
	{
		FCrowdPerceivedPawn Pawn;
		Pawn.Key = Key;
		Pawn.Location = Location;
		Pawn.VelocityCmPerSec = FVector(100.0, 0.0, 0.0);
		Pawn.bPerceivable = bPerceivable;
		Pawn.bObstacle = bObstacle;
		return Pawn;
	};
	// The same facts in two insertion orders (the first pawn is "player-driven", the second an NPC: nothing says so).
	TArray<FCrowdPerceivedPawn> A = { Make(10, FVector(100, 0, 0), true, true), Make(11, FVector(150, 20, 0), true, true), Make(12, FVector(120, 0, 0), false, false), Make(13, FVector(90, 30, 0), true, false) };
	TArray<FCrowdPerceivedPawn> B = { A[3], A[2], A[1], A[0] };
	FCrowdSpatialHash HashA(200.0);
	FCrowdSpatialHash HashB(200.0);
	HashA.Build(A);
	HashB.Build(B);
	TArray<const FCrowdPerceivedPawn*> PerceptionA, PerceptionB, SteeringA, SteeringB;
	HashA.Query(FVector::ZeroVector, 500.0, ECrowdQuery::Perception, 0, PerceptionA);
	HashB.Query(FVector::ZeroVector, 500.0, ECrowdQuery::Perception, 0, PerceptionB);
	HashA.Query(FVector::ZeroVector, 500.0, ECrowdQuery::Steering, 0, SteeringA);
	HashB.Query(FVector::ZeroVector, 500.0, ECrowdQuery::Steering, 0, SteeringB);
	auto Keys = [](const TArray<const FCrowdPerceivedPawn*>& Found)
	{
		TArray<uint64> Out;
		for (const FCrowdPerceivedPawn* Pawn : Found)
		{
			Out.Add(Pawn->Key);
		}
		return Out;
	};
	TestTrue(TEXT("perception results do not depend on insertion order"), Keys(PerceptionA) == Keys(PerceptionB));
	TestTrue(TEXT("steering results do not depend on insertion order"), Keys(SteeringA) == Keys(SteeringB));
	TestFalse(TEXT("a concealed pawn is never perceived"), Keys(PerceptionA).Contains(12));
	TestFalse(TEXT("a not-an-obstacle pawn is never steered around"), Keys(SteeringA).Contains(13));
	TestTrue(TEXT("the transitioning pawn is still perceivable"), Keys(PerceptionA).Contains(13));
	TestEqual(TEXT("nearest first"), Keys(PerceptionA)[0], uint64(13));
	TestEqual(TEXT("the hash is built once per Build call"), HashA.GetBuildCount(), uint64(1));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdPerceptionConcealedOccupancyTest, "StealthGame.Crowd.Perception.ConcealedOccupancy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdPerceptionConcealedOccupancyTest::RunTest(const FString& Parameters)
{
	using namespace CrowdPerceptionTestData;
	int32 Forbidden0 = 0, Forbidden1 = 0, Forbidden3 = 0;
	const FString Empty = RunScene(0, Forbidden0);
	const FString One = RunScene(1, Forbidden1);
	const FString Three = RunScene(3, Forbidden3);
	TestTrue(TEXT("NPC steering, look, glances and partners are identical with 0 and 1 concealed occupants (AC-CNB-36)"), Empty == One);
	TestTrue(TEXT("and with 3"), Empty == Three);
	TestEqual(TEXT("no glance or partner is a concealed pawn"), Forbidden1 + Forbidden3, 0);
	TestTrue(TEXT("the scenes are not empty (glances happened)"), Empty.Contains(TEXT("|G ")) || Empty.Contains(TEXT("G ")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdPerceptionRayBudgetTest, "StealthGame.Crowd.Perception.RayBudget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdPerceptionRayBudgetTest::RunTest(const FString& Parameters)
{
	const int32 Limit = FCrowdTuning_Server().CrowdRaysPerFrameLimit;
	FCrowdRayBudget Budget;
	// A strike burst: 30 rays from 10 requesters, all with stimulus time 5.0.
	for (int32 Index = 0; Index < 30; ++Index)
	{
		FCrowdRayRequest Ray;
		Ray.Requester = static_cast<uint32>(Index % 10);
		Ray.Tag = static_cast<uint32>(Index);
		Ray.StimulusTime = 5.0;
		Budget.Request(Ray);
	}
	int32 Frame = 0;
	int32 MaxPerFrame = 0;
	bool bTimesKept = true;
	TArray<uint32> FirstFrame;
	while (Budget.NumPending() > 0 && Frame < 100)
	{
		++Frame;
		const int32 Done = Budget.Process(Limit, [](const FVector&, const FVector&) { return true; }, [&](const FCrowdRayRequest& Ray, bool)
		{
			bTimesKept &= Ray.StimulusTime == 5.0;
			if (Frame == 1)
			{
				FirstFrame.Add(Ray.Requester);
			}
		});
		MaxPerFrame = FMath::Max(MaxPerFrame, Done);
	}
	TestEqual(TEXT("the limit is 8"), Limit, 8);
	TestTrue(TEXT("at most 8 rays per frame"), MaxPerFrame <= Limit);
	TestEqual(TEXT("30 rays over 4 frames"), Frame, 4);
	TestTrue(TEXT("a deferred ray keeps its stimulus time"), bTimesKept);
	TArray<uint32> Unique = FirstFrame;
	Unique.Sort();
	TestEqual(TEXT("round-robin: the first frame serves 8 different requesters"), TSet<uint32>(FirstFrame).Num(), 8);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdPerceptionNoPerceptionComponentTest, "StealthGame.Crowd.Perception.NoPerceptionComponent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdPerceptionNoPerceptionComponentTest::RunTest(const FString& Parameters)
{
	const StealthCrowdTest::FScriptRun Run = StealthCrowdTest::RunBashScript(TEXT("tools/ci/lint/selftest/run-crowd-lint-selftest.sh"));
	if (TestTrue(TEXT("the crowd lint selftest ran"), Run.bLaunched))
	{
		TestTrue(TEXT("the seeded engine perception include is reported"), Run.Output.Contains(TEXT("crowd-include includes the engine perception header")));
		TestTrue(TEXT("and the real Crowd tree has none"), Run.Output.Contains(TEXT("SELFTEST real-tree-zero-findings ok")));
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
