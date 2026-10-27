// Story cnb-019 (production/epics/crowd-npc-behaviour/story-019-run-traversal-reversal-herrings.md): late-porter runs
// (T1), public traversals (T2) and "forgot something" reversals (T12) (GDD CNB-R7, R5 C7, R16; AC-CNB-04 heading clause,
// AC-CNB-10 rate shape). Headless accelerated reference scenes (CrowdSimWorld, fixed seeds, every tell's clock on, the
// reference herring sites: 14 courts, bushes every 15 m, coping of 0.8 / 1.2 / 1.6 / 2.0 m every 12 m).
//   RunBouts            every run bout (Run gait commanded) lasts 2-6 s, starts at a leg start and ends by braking at a
//                       leg end (no speed drop faster than the SCL run braking); destinations are density route nodes
//                       (R13 weights, the same pick as every leg), never a court.
//   TraversalHerrings   performed vault and climb rates in the 99 % Poisson interval of F2's accepted rates; no climb at
//                       coping below 1.6 m (1.6 m itself is a climb); every Traverse follows a dry run of its point.
//   TraversalRetryBlock a refused dry run abandons the traversal and the point is not tried again for CrowdRetryBlockSec.
//   ReversalBehaviour   the reversal rate in the 99 % Poisson interval of 60 / CrowdReversalIntervalSec; every reversal
//                       stop lasts at least StopMinSec; no commanded heading change above ReversalAngleDeg while moving.
// The 600-minute city-wide rate gate over all eight tells is story cnb-023 (CrowdTellCalibrationTest).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "CrowdTestStats.h"
#include "HAL/PlatformTime.h"
#include "Server/CrowdVirtualInput.h"
#include "Tests/Crowd/CrowdHerringRecorder.h"
#include "Tests/Crowd/CrowdSimWorld.h"
#include "View/Core/StealthLocomotionModel.h"

namespace CrowdHerringMotionTestData
{
	using namespace StealthCrowdSim;

	/** The reference herring scene: 50 NPCs, every tell clock on, the reference sites. */
	inline FConfig ReferenceConfig(uint64 Seed)
	{
		FConfig Config;
		Config.HalfSizeCm = 3000.0;
		Config.SubSeed = Seed;
		Config.EnabledTellMask = FConfig::AllTells;
		return Config;
	}

	/** Per-NPC run-span tracking from the commanded gait (the bout as an observer of the pawn's motion would time it). */
	struct FSpanTracker
	{
		TMap<uint32, double> RunStart;
		TArray<TPair<uint32, FVector2D>> Spans; // ordinal, (start, end)
		void Sample(const FNpc& Npc, double Now)
		{
			const bool bRunning = Npc.Body.Gait == EGait::Run && Npc.Body.MoveIntent.Size() > 0.01;
			const double* Start = RunStart.Find(Npc.Body.Ordinal);
			if (bRunning && Start == nullptr)
			{
				RunStart.Add(Npc.Body.Ordinal, Now);
			}
			else if (!bRunning && Start != nullptr)
			{
				Spans.Add({ Npc.Body.Ordinal, FVector2D(*Start, Now) });
				RunStart.Remove(Npc.Body.Ordinal);
			}
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdHerringRunBoutsTest, "StealthGame.Crowd.Herring.RunBouts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdHerringRunBoutsTest::RunTest(const FString& Parameters)
{
	using namespace CrowdHerringMotionTestData;
	using namespace StealthCrowdHerringTest;
	const double WallStart = FPlatformTime::Seconds();
	FWorld World(ReferenceConfig(0x5EED0019A001ull));
	World.AddReferenceHerringSites();
	FRecorder Recorder;
	World.SetObserver(&Recorder);
	World.SpawnNpcs();
	World.StartRound();
	FSpanTracker Spans;
	const FLocomotionTuning_Public& Loco = World.GetLocomotionTuning();
	const double RunDecel = StealthLocomotion::RunDeceleration(Loco);
	TMap<uint32, float> LastSpeed;
	TSet<uint32> WasTraversing;
	int32 HardStops = 0;
	const double Minutes = 20.0;
	while (World.GetNow() < Minutes * 60.0)
	{
		World.Step();
		for (int32 Index = 0; Index < World.NumNpcs(); ++Index)
		{
			FNpc& Npc = World.GetNpc(Index);
			Spans.Sample(Npc, World.GetNow());
			// Ordinary braking: no NPC sheds speed faster than the SCL run braking (5 % numerical margin).
			const float Speed = Npc.Body.Motion.Speed;
			const bool bTraversalFrame = Npc.Body.bTraversing || WasTraversing.Contains(Npc.Body.Ordinal);
			if (Npc.Body.bTraversing) { WasTraversing.Add(Npc.Body.Ordinal); } else { WasTraversing.Remove(Npc.Body.Ordinal); }
			// (the sim's traversal and bush entry move the body by fiat, not by the SCL model: those frames are skipped)
			if (const float* Last = LastSpeed.Find(Npc.Body.Ordinal); Last != nullptr && !bTraversalFrame && Npc.Body.bPerceivable
				&& (*Last - Speed) > RunDecel * (1.0 / 60.0) * 1.05 + 1.0)
			{
				++HardStops;
			}
			LastSpeed.Add(Npc.Body.Ordinal, Speed);
		}
	}
	// Bouts: each span matches a run leg started at its start, and lasts 2-6 s (one step of tolerance either side).
	const double Step = 1.0 / 60.0;
	int32 Matched = 0, BadLength = 0, NotAtLegStart = 0, CourtGoals = 0, OffNodeGoals = 0;
	for (const TPair<uint32, FVector2D>& Span : Spans.Spans)
	{
		const double Length = Span.Value.Y - Span.Value.X;
		const FRunLeg* Leg = Recorder.Runs.FindByPredicate([&](const FRunLeg& R) { return R.Ordinal == Span.Key && FMath::Abs(R.Time - Span.Value.X) <= 2.0 * Step; });
		if (Leg == nullptr)
		{
			++NotAtLegStart;
			continue;
		}
		++Matched;
		BadLength += (Length < CrowdHerringConstants::RunBoutMinSec - 2.0 * Step || Length > CrowdHerringConstants::RunBoutMaxSec + 2.0 * Step) ? 1 : 0;
	}
	for (const FRunLeg& Leg : Recorder.Runs)
	{
		bool bNode = false;
		for (const FCrowdRouteNode& Node : World.GetDensity().GetNodes())
		{
			bNode |= FVector::Dist2D(Node.Location, Leg.Goal) < 1.0;
		}
		OffNodeGoals += bNode ? 0 : 1;
		// A destination is a court when it is one of the courts' stand points or fixtures (a route node merely near a
		// court is an ordinary destination: R13 weights know no courts).
		for (const FCourtFixture& Court : World.GetCourts())
		{
			CourtGoals += FVector::Dist2D(Court.Center, Leg.Goal) < 1.0 ? 1 : 0;
		}
	}
	AddInfo(FString::Printf(TEXT("RUNBOUTS %d run legs, %d run spans over %.0f min x %d NPCs (%.1f s wall): matched %d, not at a leg start %d, length outside 2-6 s %d, goals off the route nodes %d, at a court %d, braking faster than SCL %d"),
		Recorder.Runs.Num(), Spans.Spans.Num(), Minutes, World.NumNpcs(), FPlatformTime::Seconds() - WallStart, Matched, NotAtLegStart, BadLength, OffNodeGoals, CourtGoals, HardStops));
	TestTrue(TEXT("run bouts happen (about 0.148 per NPC-minute)"), Recorder.Runs.Num() > 80);
	TestEqual(TEXT("every run span starts at a run leg's start"), NotAtLegStart, 0);
	TestEqual(TEXT("every bout lasts 2-6 s"), BadLength, 0);
	TestEqual(TEXT("every destination is a density route node (R13 weights)"), OffNodeGoals, 0);
	TestEqual(TEXT("no destination is a court"), CourtGoals, 0);
	TestEqual(TEXT("every run ends by ordinary SCL braking (never faster)"), HardStops, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdHerringTraversalTest, "StealthGame.Crowd.Herring.TraversalHerrings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdHerringTraversalTest::RunTest(const FString& Parameters)
{
	using namespace CrowdHerringMotionTestData;
	using namespace StealthCrowdHerringTest;
	const double WallStart = FPlatformTime::Seconds();
	FWorld World(ReferenceConfig(0x5EED0019A002ull));
	World.AddReferenceHerringSites();
	FRecorder Recorder;
	World.SetObserver(&Recorder);
	World.SpawnNpcs();
	World.StartRound();
	const double Minutes = 30.0;
	World.RunUntil(Minutes * 60.0);
	const FCrowdTuning_Server& Tuning = World.GetCrowdTuning();
	const double NpcMinutes = Minutes * World.NumNpcs();
	const int32 Vaults = Recorder.CountPerformed(ECrowdTell::Vault);
	const int32 Climbs = Recorder.CountPerformed(ECrowdTell::Climb);
	double VLow = 0, VHigh = 0, CLow = 0, CHigh = 0;
	const bool bVault = StealthCrowdStats::InPoisson99(Vaults, NpcMinutes * 60.0 / Tuning.CrowdVaultIntervalSec, &VLow, &VHigh);
	const bool bClimb = StealthCrowdStats::InPoisson99(Climbs, NpcMinutes * 60.0 / Tuning.CrowdClimbIntervalSec, &CLow, &CHigh);
	int32 LowClimbs = 0, HighVaults = 0, WithoutDryRun = 0;
	for (const TPair<FTellEvent, FCrowdTraversalSite>& Traversal : Recorder.Traversals)
	{
		const bool bClimbEvent = Traversal.Key.Tell == ECrowdTell::Climb;
		LowClimbs += bClimbEvent && Traversal.Value.CopingHeightCm < CrowdHerringConstants::ClimbMinCopingCm ? 1 : 0;
		HighVaults += !bClimbEvent && Traversal.Value.CopingHeightCm >= CrowdHerringConstants::ClimbMinCopingCm ? 1 : 0;
		const bool bDryRun = World.TraversalDryRuns.ContainsByPredicate([&](const FWorld::FTraversalDryRun& D)
			{ return D.Ordinal == Traversal.Key.Ordinal && D.PointId == Traversal.Value.PointId && D.bAllowed && FMath::Abs(D.Time - Traversal.Key.Time) < 1e-6; });
		WithoutDryRun += bDryRun ? 0 : 1;
	}
	int32 TraverseCommands = 0;
	for (const FCommandRecord& Record : World.GetCommandLog())
	{
		TraverseCommands += Record.Kind == FCommandRecord::EKind::Traverse ? 1 : 0;
	}
	const int64 AcceptedV = World.GetTells().GetCounters().Accepted[static_cast<int32>(ECrowdTell::Vault)];
	const int64 AcceptedC = World.GetTells().GetCounters().Accepted[static_cast<int32>(ECrowdTell::Climb)];
	AddInfo(FString::Printf(TEXT("TRAVERSAL %.0f NPC-min (%.1f s wall): vaults %d (accepted %lld; 99 %% %.1f-%.1f), climbs %d (accepted %lld; 99 %% %.1f-%.1f), Traverse commands %d, climbs below 1.6 m %d, vaults at 1.6 m+ %d, without a dry run %d, refusals %d"),
		NpcMinutes, FPlatformTime::Seconds() - WallStart, Vaults, AcceptedV, VLow, VHigh, Climbs, AcceptedC, CLow, CHigh, TraverseCommands, LowClimbs, HighVaults, WithoutDryRun, Recorder.TraversalRefusals));
	TestTrue(TEXT("the vault rate is in the 99 % Poisson interval of F2's accepted rate"), bVault);
	TestTrue(TEXT("the climb rate is in the 99 % Poisson interval of F2's accepted rate"), bClimb);
	TestEqual(TEXT("no climb at coping below 1.6 m"), LowClimbs, 0);
	TestEqual(TEXT("coping of 1.6 m or more is climbed, never vaulted"), HighVaults, 0);
	TestEqual(TEXT("every Traverse follows an allowed dry run of its point in the same tick"), WithoutDryRun, 0);
	TestEqual(TEXT("every traversal went through the sink's Traverse"), TraverseCommands, Vaults + Climbs);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdHerringTraversalRetryTest, "StealthGame.Crowd.Herring.TraversalRetryBlock",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdHerringTraversalRetryTest::RunTest(const FString& Parameters)
{
	using namespace StealthCrowdSim;
	using namespace StealthCrowdHerringTest;
	FConfig Config;
	Config.NpcCount = 1;
	Config.HalfSizeCm = 2000.0;
	Config.EnabledTellMask = 1u << static_cast<uint32>(ECrowdTell::Vault);
	// Accelerated: a vault due about every 20 s (the clock rate only; the behaviour under test is the refusal).
	Config.Tuning.CrowdVaultIntervalSec = 20.0f;
	FWorld World(Config);
	FCrowdTraversalSite Site;
	Site.PointId = 7;
	Site.Near = FVector(600.0, 0.0, 0.0);
	Site.Far = FVector(720.0, 0.0, 0.0);
	Site.CopingHeightCm = 80.0;
	World.AddTraversal(Site);
	World.RefuseTraversalPoint = 7;
	FRecorder Recorder;
	World.SetObserver(&Recorder);
	World.SpawnNpcAt(FVector::ZeroVector, 0.0f);
	World.StartRound();
	World.RunUntil(10.0 * 60.0);
	TArray<double> Asked;
	for (const FWorld::FTraversalDryRun& DryRun : World.TraversalDryRuns)
	{
		Asked.Add(DryRun.Time);
	}
	double MinGap = 1.0e9;
	for (int32 Index = 1; Index < Asked.Num(); ++Index)
	{
		MinGap = FMath::Min(MinGap, Asked[Index] - Asked[Index - 1]);
	}
	int32 Traverses = 0;
	for (const FCommandRecord& Record : World.GetCommandLog())
	{
		Traverses += Record.Kind == FCommandRecord::EKind::Traverse ? 1 : 0;
	}
	AddInfo(FString::Printf(TEXT("TRAVERSALRETRY %d dry runs of the refused point, shortest gap %.2f s (block %.1f s), Traverse sent %d"), Asked.Num(), MinGap, World.GetCrowdTuning().CrowdRetryBlockSec, Traverses));
	TestTrue(TEXT("the refused point was asked more than once"), Asked.Num() >= 2);
	TestTrue(TEXT("a refused point is not tried again within CrowdRetryBlockSec"), MinGap >= World.GetCrowdTuning().CrowdRetryBlockSec - 1e-6);
	TestEqual(TEXT("a refused dry run sends no Traverse"), Traverses, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdHerringReversalTest, "StealthGame.Crowd.Herring.ReversalBehaviour",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdHerringReversalTest::RunTest(const FString& Parameters)
{
	using namespace CrowdHerringMotionTestData;
	using namespace StealthCrowdHerringTest;
	const double WallStart = FPlatformTime::Seconds();
	FWorld World(ReferenceConfig(0x5EED0019A003ull));
	World.AddReferenceHerringSites();
	FRecorder Recorder;
	World.SetObserver(&Recorder);
	World.SpawnNpcs();
	World.StartRound();
	const FLocomotionTuning_Public& Loco = World.GetLocomotionTuning();
	// Each reversal: the time its stop began, and when the pawn reached rest.
	TMap<uint32, double> StopStarted;
	TArray<double> StopLengths;
	int32 BigTurnsMoving = 0;
	TMap<int32, int32> BigByMode;
	const double Minutes = 30.0;
	int32 SeenReversals = 0;
	while (World.GetNow() < Minutes * 60.0)
	{
		World.Step();
		for (; SeenReversals < Recorder.Reversals.Num(); ++SeenReversals)
		{
			StopStarted.Add(Recorder.Reversals[SeenReversals].Ordinal, Recorder.Reversals[SeenReversals].Time);
		}
		for (int32 Index = 0; Index < World.NumNpcs(); ++Index)
		{
			FNpc& Npc = World.GetNpc(Index);
			if (const double* Started = StopStarted.Find(Npc.Body.Ordinal); Started != nullptr && Npc.Body.Motion.Speed <= 1.0f)
			{
				StopLengths.Add(World.GetNow() - *Started);
				StopStarted.Remove(Npc.Body.Ordinal);
			}
			// The commanded heading against the travel heading while moving (C7 is the only big turn, and from rest).
			if (Npc.Body.Motion.Speed > Loco.PivotSpeedCmPerSec && Npc.Body.MoveIntent.Size() > 0.05 && !Npc.Body.bTraversing)
			{
				const double Commanded = FCrowdVirtualInput::WorldYawOfInput(Npc.Body.MoveIntent, Npc.Body.Control.Yaw);
				const bool bBig = FMath::Abs(FMath::FindDeltaAngleDegrees(Npc.Body.Motion.FacingYawDeg, Commanded)) > Loco.ReversalAngleDeg;
				BigTurnsMoving += bBig ? 1 : 0;
				if (bBig)
				{
					BigByMode.FindOrAdd(static_cast<int32>(Npc.Agent.GetHerringMode()) * 100 + static_cast<int32>(Npc.Agent.GetActivity()))++;
				}
			}
		}
	}
	const double NpcMinutes = Minutes * World.NumNpcs();
	const int32 Reversals = Recorder.CountPerformed(ECrowdTell::Reversal);
	double Low = 0, High = 0;
	const bool bRate = StealthCrowdStats::InPoisson99(Reversals, NpcMinutes * 60.0 / World.GetCrowdTuning().CrowdReversalIntervalSec, &Low, &High);
	double ShortestStop = 1.0e9;
	for (const double Length : StopLengths)
	{
		ShortestStop = FMath::Min(ShortestStop, Length);
	}
	AddInfo(FString::Printf(TEXT("REVERSAL %.0f NPC-min (%.1f s wall): %d reversals (accepted %lld; 99 %% %.1f-%.1f), stops measured %d, shortest %.3f s (floor %.2f s), commanded turns above %.0f deg while moving %d"),
		NpcMinutes, FPlatformTime::Seconds() - WallStart, Reversals, World.GetTells().GetCounters().Accepted[static_cast<int32>(ECrowdTell::Reversal)], Low, High,
		StopLengths.Num(), ShortestStop, Loco.StopMinSec, Loco.ReversalAngleDeg, BigTurnsMoving));
	for (const TPair<int32, int32>& Pair : BigByMode)
	{
		AddInfo(FString::Printf(TEXT("REVERSAL big turns in herring mode %d activity %d: %d"), Pair.Key / 100, Pair.Key % 100, Pair.Value));
	}
	TestTrue(TEXT("the reversal rate is in the 99 % Poisson interval of 60 / CrowdReversalIntervalSec"), bRate);
	TestTrue(TEXT("every reversal's stop was measured"), StopLengths.Num() >= Reversals - 2);
	TestTrue(TEXT("every reversal stop lasts at least StopMinSec"), ShortestStop >= Loco.StopMinSec - 1.0 / 60.0);
	TestEqual(TEXT("no commanded heading change above ReversalAngleDeg while moving"), BigTurnsMoving, 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
