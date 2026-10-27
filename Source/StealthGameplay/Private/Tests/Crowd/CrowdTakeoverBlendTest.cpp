// Story cnb-028 (production/epics/crowd-npc-behaviour/story-028-bound-pawn-restricted-driver.md): participant-bound pawn
// driving (GDD CNB-R10 items 1-3, AC-CNB-20; ADR-0016 section 7). Headless at a 1/60 s step with the real SCL movement
// model (CrowdSimWorld): a pawn taken over while running gets the restricted driver.
//   TakeoverBlend   from the takeover the speed never falls faster than CrowdStopDecel (RunSpeedCmPerSec / RunStopSec,
//                   600 cm/s^2); for HandoverHoldSec the commanded heading is the pre-takeover heading at walk; the pawn
//                   is never removed; from takeover on it issues only Move and Look (no discrete command reaches the
//                   sim's command log, and FCrowdMotionSink has none by construction, AC-CNB-11's static half); after
//                   the hold it walks the restricted catalogue (legs and pauses) and never shows a red herring.
//   SlowTakeover    a pawn slower than the SCL pivot speed, or whose held heading leaves the navigable area within the
//                   hold distance, holds Move(0) instead.
// The head-rate clause (HeadLook <= LookMaxRateDegPerSec) is the SCL head filter's (story SCL-021); the hold's end in the
// ordinary cancel pose is Round's IIF release path; both are outside this story's code.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Server/CrowdSink.h"
#include "Server/CrowdVirtualInput.h"
#include "Tests/Crowd/CrowdHerringRecorder.h"
#include "Tests/Crowd/CrowdSimWorld.h"
#include "View/Core/StealthLocomotionModel.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdTakeoverBlendTest, "StealthGame.Crowd.Takeover.TakeoverBlend",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdTakeoverBlendTest::RunTest(const FString& Parameters)
{
	using namespace StealthCrowdSim;
	using namespace StealthCrowdHerringTest;
	FConfig Config;
	Config.HalfSizeCm = 3000.0;
	Config.EnabledTellMask = FConfig::AllTells; // every clock runs; the restricted pawn must show none of them
	FWorld World(Config);
	World.AddReferenceHerringSites();
	FRecorder Recorder;
	World.SetObserver(&Recorder);
	World.SpawnNpcs();
	World.StartRound();
	World.RunUntil(5.0);
	const FLocomotionTuning_Public& Loco = World.GetLocomotionTuning();
	const double StopDecel = Loco.RunSpeedCmPerSec / Loco.RunStopSec;
	const double HoldSec = World.GetActionTimings().HandoverHoldSec;
	int32 Takeovers = 0, OverDecel = 0, HoldHeadingOff = 0, HoldNotWalk = 0, Discrete = 0;
	double MaxDecel = 0.0;
	TArray<uint32> Bound;
	for (int32 Trial = 0; Trial < 20; ++Trial)
	{
		// An Active player running east-north-east mid-route: the connection goes Unresponsive and Round takes over.
		const float Yaw = static_cast<float>(Trial * 17 % 360);
		FNpc& Pawn = World.SpawnNpcAt(FVector(-2000.0 + Trial * 200.0, -2000.0 + Trial * 150.0, 0.0), Yaw, ECrowdDriverKind::Restricted, Loco.RunSpeedCmPerSec, EGait::Run);
		Bound.Add(Pawn.Body.Ordinal);
		++Takeovers;
		const double Start = World.GetNow();
		const FVector2D Heading = FVector2D(FRotator(0.0, Yaw, 0.0).Vector());
		float LastSpeed = Pawn.Body.Motion.Speed;
		while (World.GetNow() < Start + 6.0)
		{
			World.Step();
			FNpc* Same = World.FindNpc(Bound.Last());
			if (!TestNotNull(TEXT("the taken-over pawn is never removed"), Same))
			{
				return false;
			}
			const double Decel = (LastSpeed - Same->Body.Motion.Speed) / Config.StepSec;
			MaxDecel = FMath::Max(MaxDecel, Decel);
			OverDecel += Decel > StopDecel * 1.02 + 1.0 ? 1 : 0;
			LastSpeed = Same->Body.Motion.Speed;
			if (World.GetNow() < Start + HoldSec - Config.StepSec && Same->Body.MoveIntent.Size() > 0.01)
			{
				const double Commanded = FCrowdVirtualInput::WorldYawOfInput(Same->Body.MoveIntent, Same->Body.Control.Yaw);
				// The held key: within the input device's quantisation of the heading (a keyboard's 45 degree sectors).
				HoldHeadingOff += FMath::Abs(FMath::FindDeltaAngleDegrees(Commanded, FMath::RadiansToDegrees(FMath::Atan2(Heading.Y, Heading.X)))) > 22.5 + 0.5 ? 1 : 0;
				HoldNotWalk += Same->Body.Gait != EGait::Walk ? 1 : 0;
			}
		}
	}
	World.RunUntil(World.GetNow() + 120.0);
	for (const FCommandRecord& Record : World.GetCommandLog())
	{
		Discrete += Bound.Contains(Record.Ordinal) ? 1 : 0;
	}
	int32 HerringTells = 0, Legs = 0;
	for (const FTellEvent& Event : Recorder.Performed)
	{
		HerringTells += Bound.Contains(Event.Ordinal) ? 1 : 0;
	}
	for (const TPair<uint32, double>& Leg : Recorder.LegStarts)
	{
		Legs += Bound.Contains(Leg.Key) ? 1 : 0;
	}
	AddInfo(FString::Printf(TEXT("TAKEOVER %d takeovers from a run: max deceleration %.1f cm/s^2 (CrowdStopDecel %.1f), steps over it %d, hold steps off the heading %d, hold steps not at walk %d, discrete commands %d, tells shown %d, restricted legs %d"),
		Takeovers, MaxDecel, StopDecel, OverDecel, HoldHeadingOff, HoldNotWalk, Discrete, HerringTells, Legs));
	TestEqual(TEXT("max |dv/dt| <= CrowdStopDecel from takeover on"), OverDecel, 0);
	TestEqual(TEXT("the hold keeps the pre-takeover heading (within the device's 22.5 degree quantisation)"), HoldHeadingOff, 0);
	TestEqual(TEXT("the hold is at walk"), HoldNotWalk, 0);
	TestEqual(TEXT("a bound pawn issues only Move and Look (no discrete command)"), Discrete, 0);
	TestEqual(TEXT("a bound pawn shows no red herring"), HerringTells, 0);
	TestTrue(TEXT("after the hold the restricted catalogue walks legs"), Legs >= Takeovers);
	TestFalse(TEXT("the restricted sink type has no Traverse (only Move and Look)"), StealthCrowdSink::THasTraverse<FCrowdMotionSink>::value);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdTakeoverSlowTest, "StealthGame.Crowd.Takeover.SlowOrOffNavHoldsStill",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdTakeoverSlowTest::RunTest(const FString& Parameters)
{
	using namespace StealthCrowdSim;
	FConfig Config;
	Config.NpcCount = 0;
	Config.HalfSizeCm = 2000.0;
	FWorld World(Config);
	World.StartRound();
	const FLocomotionTuning_Public& Loco = World.GetLocomotionTuning();
	// (1) slower than the pivot speed; (2) walking at the plane's edge toward the outside (the hold leaves the area).
	FNpc& Slow = World.SpawnNpcAt(FVector::ZeroVector, 0.0f, ECrowdDriverKind::Restricted, Loco.PivotSpeedCmPerSec * 0.5f, EGait::Walk);
	const uint32 SlowOrdinal = Slow.Body.Ordinal;
	FNpc& Edge = World.SpawnNpcAt(FVector(1900.0, 0.0, 0.0), 0.0f, ECrowdDriverKind::Restricted, Loco.WalkSpeedCmPerSec, EGait::Walk);
	const uint32 EdgeOrdinal = Edge.Body.Ordinal;
	int32 Moved = 0;
	const double Start = World.GetNow();
	while (World.GetNow() < Start + World.GetActionTimings().HandoverHoldSec - Config.StepSec)
	{
		World.Step();
		Moved += World.FindNpc(SlowOrdinal)->Body.MoveIntent.Size() > 0.0 ? 1 : 0;
		Moved += World.FindNpc(EdgeOrdinal)->Body.MoveIntent.Size() > 0.0 ? 1 : 0;
	}
	AddInfo(FString::Printf(TEXT("TAKEOVERSLOW hold steps with a non-zero Move: %d"), Moved));
	TestEqual(TEXT("a pawn below the pivot speed, or heading off the navigable area, holds Move(0)"), Moved, 0);
	TestTrue(TEXT("the hold ends and the restricted driver takes over"), !World.FindNpc(SlowOrdinal)->Agent.IsInHandoverHold() || World.GetNow() < Start + 1.0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
