// Story cnb-009 (production/epics/crowd-npc-behaviour/story-009-follower-shaping-pace-and-stops.md): the follower
// (ADR-0016 section 4.2; GDD CNB-R3.1, R3.2, AC-CNB-03, AC-CNB-04; SCL F2 stop floor, decision D2).
//   PaceShare      10 min of the headless reference scene (50 NPCs, the real SCL movement model at 60 Hz): every Move
//                  magnitude is in {0, 0.8, 0.9, 1.0}; the walking-time share at 1.0 is CrowdFullPacePct +-3 points and
//                  >= 50 %; C4 matches occur and each lasts at most CrowdGroupDriftSec; every other pace change is at a
//                  leg start.
//   Corners        200 legs with one 45-90 degree corner: >= 95 % taken without speed below 0.8 x the leg's speed; no
//                  commanded heading change at or above ReversalAngleDeg while moving.
//   StopFloor      every stop to rest of the reference scene lasts at least StopMinSec (minus one frame), stops begun
//                  inside a start included.
//   ArrivalAndLane Move(0) at the F2 stop distance of the pace (53 / 42 / 34 cm at 140 / 126 / 112 cm/s); the lane
//                  offset is drawn once per pawn within +-CrowdLaneOffsetCm.
// The headless scene is the SCL graybox stand-in (an open plane with the density model's nodes): the real-pawn PIE run is
// the run-and-observe evidence of the batch.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Server/CrowdFollower.h"
#include "Tests/Crowd/CrowdSimWorld.h"
#include "View/Core/WalkSlowerScale.h"

namespace CrowdFollowerTestData
{
	struct FPaceRecorder final : public ICrowdAgentObserver
	{
		TMap<uint32, double> DriftStart;
		TArray<double> DriftLengths;
		int32 OtherPaceChanges = 0;
		virtual void OnPaceChanged(uint32 Ordinal, double Now, float Pace, ECrowdPaceCause Cause) override
		{
			if (Cause == ECrowdPaceCause::GroupDrift)
			{
				DriftStart.Add(Ordinal, Now);
			}
			else if (Cause == ECrowdPaceCause::DriftEnd)
			{
				if (const double* Start = DriftStart.Find(Ordinal))
				{
					DriftLengths.Add(Now - *Start);
					DriftStart.Remove(Ordinal);
				}
			}
			else
			{
				++OtherPaceChanges;
			}
		}
	};

	bool IsPaceSetMagnitude(double Magnitude)
	{
		return Magnitude < 1e-4 || FMath::Abs(Magnitude - StealthPaceSet::Slow) < 1e-3 || FMath::Abs(Magnitude - StealthPaceSet::Middle) < 1e-3 || FMath::Abs(Magnitude - StealthPaceSet::Full) < 1e-3;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdFollowerPaceShareTest, "StealthGame.Crowd.Follower.PaceShareAndStopFloor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdFollowerPaceShareTest::RunTest(const FString& Parameters)
{
	using namespace CrowdFollowerTestData;
	using namespace StealthCrowdSim;
	FConfig Config;
	FWorld World(Config);
	FPaceRecorder Recorder;
	World.SetObserver(&Recorder);
	World.SpawnNpcs();
	World.StartRound();
	const double StopMinSec = Config.Locomotion.StopMinSec;
	int64 WalkingFrames = 0;
	int64 FullFrames = 0;
	int64 OffSetMagnitudes = 0;
	TArray<double> Stops;
	TArray<double> StopStart;
	TArray<bool> Stopping;
	StopStart.SetNumZeroed(World.NumNpcs());
	Stopping.SetNumZeroed(World.NumNpcs());
	const int32 Frames = 10 * 60 * 60;
	for (int32 Frame = 0; Frame < Frames; ++Frame)
	{
		World.Step();
		for (int32 Index = 0; Index < World.NumNpcs(); ++Index)
		{
			const FNpc& Npc = World.GetNpc(Index);
			const double Magnitude = Npc.Body.MoveIntent.Size();
			OffSetMagnitudes += IsPaceSetMagnitude(Magnitude) ? 0 : 1;
			if (Magnitude > 1e-4 && Npc.Body.Motion.Speed > 1.0f)
			{
				++WalkingFrames;
				FullFrames += FMath::Abs(Magnitude - 1.0) < 1e-3 ? 1 : 0;
			}
			// A stop to rest: from the first frame without Move while moving to zero speed.
			if (!Stopping[Index] && Magnitude < 1e-4 && Npc.Body.Motion.Speed > 5.0f)
			{
				Stopping[Index] = true;
				StopStart[Index] = World.GetNow();
			}
			else if (Stopping[Index] && (Magnitude > 1e-4 || Npc.Body.Motion.Speed <= 0.5f))
			{
				if (Npc.Body.Motion.Speed <= 0.5f)
				{
					Stops.Add(World.GetNow() - StopStart[Index]);
				}
				Stopping[Index] = false;
			}
		}
	}
	const double Share = WalkingFrames > 0 ? static_cast<double>(FullFrames) / WalkingFrames : 0.0;
	double ShortestStop = Stops.Num() > 0 ? Stops[0] : 0.0;
	for (const double Stop : Stops)
	{
		ShortestStop = FMath::Min(ShortestStop, Stop);
	}
	double LongestDrift = 0.0;
	for (const double Drift : Recorder.DriftLengths)
	{
		LongestDrift = FMath::Max(LongestDrift, Drift);
	}
	AddInfo(FString::Printf(TEXT("PACESHARE full-pace walking share %.4f (target %.1f %%), %lld walking NPC-frames, %d C4 matches (longest %.2f s), %d stops (shortest %.3f s), off-set magnitudes %lld"),
		Share, Config.Tuning.CrowdFullPacePct, WalkingFrames, Recorder.DriftLengths.Num(), LongestDrift, Stops.Num(), ShortestStop, OffSetMagnitudes));
	TestEqual(TEXT("every Move magnitude is in the pace set {0.8, 0.9, 1.0} (or 0)"), OffSetMagnitudes, int64(0));
	TestTrue(TEXT("the full-pace share is CrowdFullPacePct +-3 points (AC-CNB-03)"), FMath::Abs(Share * 100.0 - Config.Tuning.CrowdFullPacePct) <= 3.0);
	TestTrue(TEXT("the full-pace share is never below 50 %"), Share >= 0.5);
	TestTrue(TEXT("C4 pace matches occur"), Recorder.DriftLengths.Num() > 0);
	TestTrue(TEXT("each C4 match lasts at most CrowdGroupDriftSec"), LongestDrift <= Config.Tuning.CrowdGroupDriftSec + Config.StepSec + 1e-6);
	TestEqual(TEXT("no pace change other than a leg start or C4"), Recorder.OtherPaceChanges, 0);
	TestTrue(TEXT("stops happened"), Stops.Num() > 500);
	TestTrue(TEXT("every stop lasts at least StopMinSec (AC-CNB-04 c, SCL F2 floor)"), ShortestStop >= StopMinSec - Config.StepSec - 1e-6);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdFollowerCornersTest, "StealthGame.Crowd.Follower.Corners",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdFollowerCornersTest::RunTest(const FString& Parameters)
{
	using namespace StealthCrowdSim;
	constexpr int32 Legs = 200;
	constexpr double ArmCm = 1200.0;
	int32 CleanCorners = 0;
	double MaxCommandedTurn = 0.0;
	for (int32 Leg = 0; Leg < Legs; ++Leg)
	{
		// A 45-90 degree corner, alternating left and right.
		const double Angle = 45.0 + 45.0 * (Leg % 10) / 9.0;
		const double Sign = Leg % 2 == 0 ? 1.0 : -1.0;
		const FVector Corner(ArmCm, 0.0, 0.0);
		const FVector End = Corner + FRotator(0.0, Sign * Angle, 0.0).Vector() * ArmCm;
		FConfig Config;
		Config.NpcCount = 1;
		Config.SubSeed = 0xC0DE0000ull + Leg;
		FWorld World(Config);
		bool bGoalGiven = false;
		World.SetRouteFixture([&bGoalGiven, End](uint32, const FVector&, FVector& OutGoal)
		{
			if (bGoalGiven)
			{
				return false;
			}
			bGoalGiven = true;
			OutGoal = End;
			return true;
		});
		World.SetPathFixture([Corner, End](const FVector&, const FVector&, TArray<FVector>& OutCorners) { OutCorners = { Corner, End }; });
		FNpc& Npc = World.SpawnNpcAt(FVector::ZeroVector, 0.0f);
		World.StartRound();
		if (Npc.Agent.GetActivity() != ECrowdActivity::Routing)
		{
			Npc.Agent.ForceLeg(World.GetNow(), World.FactsOf(Npc.Body), World, Npc);
		}
		const double LegSpeed = Config.Locomotion.WalkSpeedCmPerSec * Npc.Agent.GetFollower().GetPace();
		double MinCornerSpeed = TNumericLimits<double>::Max();
		for (int32 Frame = 0; Frame < 60 * 30 && Npc.Agent.GetActivity() == ECrowdActivity::Routing; ++Frame)
		{
			World.Step();
			const double Speed = Npc.Body.Motion.Speed;
			const double ToCorner = FVector::Dist2D(Npc.Body.Location, Corner);
			if (ToCorner < 300.0)
			{
				MinCornerSpeed = FMath::Min(MinCornerSpeed, Speed);
			}
			// The commanded heading against the current motion, while moving.
			if (Speed > Config.Locomotion.PivotSpeedCmPerSec && Npc.Body.MoveIntent.Size() > 1e-4)
			{
				const double Commanded = FCrowdVirtualInput::WorldYawOfInput(Npc.Body.MoveIntent, Npc.Body.Control.Yaw);
				MaxCommandedTurn = FMath::Max(MaxCommandedTurn, static_cast<double>(FMath::Abs(StealthLocomotion::DeltaYawDeg(Npc.Body.Motion.FacingYawDeg, static_cast<float>(Commanded)))));
			}
		}
		CleanCorners += MinCornerSpeed >= 0.8 * LegSpeed ? 1 : 0;
	}
	const double Rate = static_cast<double>(CleanCorners) / Legs;
	AddInfo(FString::Printf(TEXT("CORNERS %d of %d corners without speed below 0.8 x leg speed (%.1f %%), max commanded turn while moving %.1f deg (reversal %.0f)"),
		CleanCorners, Legs, Rate * 100.0, MaxCommandedTurn, FLocomotionTuning_Public().ReversalAngleDeg));
	TestTrue(TEXT(">= 95 % of 45-90 degree corners keep >= 0.8 x leg speed (AC-CNB-04 a)"), Rate >= 0.95);
	TestTrue(TEXT("no commanded heading change at or above ReversalAngleDeg while moving (AC-CNB-04 b)"), MaxCommandedTurn < FLocomotionTuning_Public().ReversalAngleDeg);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdFollowerArrivalAndLaneTest, "StealthGame.Crowd.Follower.ArrivalAndLane",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdFollowerArrivalAndLaneTest::RunTest(const FString& Parameters)
{
	const FCrowdTuning_Server Tuning;
	const FLocomotionTuning_Public Locomotion;
	// The F2 stop distances of the pace set.
	struct FCase { double Speed; double Expected; };
	const FCase Cases[] = { { 140.0, 52.5 }, { 126.0, 42.5 }, { 112.0, 33.6 } };
	for (const FCase& Case : Cases)
	{
		TestTrue(FString::Printf(TEXT("stop distance at %.0f cm/s is %.1f cm"), Case.Speed, Case.Expected), FMath::IsNearlyEqual(FCrowdFollower::StopDistanceCm(Locomotion, Case.Speed), Case.Expected, 0.6));
		// The follower releases Move(0) exactly when the remaining path is at the stop distance.
		FCrowdDraws Draws(0x42);
		FCrowdFollower Follower;
		Follower.Initialize(1, 0.0, Tuning, Draws);
		const FVector Goal(1000.0, 0.0, 0.0);
		Follower.StartLeg(1, FVector::ZeroVector, MakeArrayView(&Goal, 1), 0.0, Tuning, Draws);
		FCrowdPerceivedPawn Self;
		Self.Key = 1;
		Self.VelocityCmPerSec = FVector(Case.Speed, 0.0, 0.0);
		double ReleasedAt = -1.0;
		for (int32 Step = 0; Step < 600 && ReleasedAt < 0.0; ++Step)
		{
			Self.Location = FVector(Step * Case.Speed / 60.0, 0.0, 0.0);
			const FCrowdFollowerOutput Out = Follower.Tick(Step / 60.0, 1.0f / 60.0f, Self, {}, Tuning, Locomotion, Draws);
			if (Out.bReleased)
			{
				ReleasedAt = FVector::Dist2D(Self.Location, Goal);
			}
		}
		TestTrue(FString::Printf(TEXT("Move(0) %.1f cm before the end at %.0f cm/s (F2 %.1f)"), ReleasedAt, Case.Speed, Case.Expected), ReleasedAt <= Case.Expected + 0.6 && ReleasedAt >= Case.Expected - Case.Speed / 60.0 - 0.6);
	}
	// The lane offset: drawn once per pawn (same ordinal, same stream: same offset), always within +-CrowdLaneOffsetCm.
	double MinOffset = 0.0, MaxOffset = 0.0;
	for (uint32 Ordinal = 0; Ordinal < 500; ++Ordinal)
	{
		FCrowdDraws Draws(0x1A9E);
		FCrowdFollower A;
		A.Initialize(Ordinal, 0.0, Tuning, Draws);
		FCrowdDraws DrawsAgain(0x1A9E);
		FCrowdFollower B;
		B.Initialize(Ordinal, 0.0, Tuning, DrawsAgain);
		TestEqual(TEXT("one lane offset per pawn"), A.GetLaneOffsetCm(), B.GetLaneOffsetCm());
		MinOffset = FMath::Min(MinOffset, A.GetLaneOffsetCm());
		MaxOffset = FMath::Max(MaxOffset, A.GetLaneOffsetCm());
	}
	TestTrue(TEXT("lane offsets within +-CrowdLaneOffsetCm"), MinOffset >= -Tuning.CrowdLaneOffsetCm && MaxOffset <= Tuning.CrowdLaneOffsetCm);
	TestTrue(TEXT("both sides occur"), MinOffset < -0.8 * Tuning.CrowdLaneOffsetCm && MaxOffset > 0.8 * Tuning.CrowdLaneOffsetCm);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
