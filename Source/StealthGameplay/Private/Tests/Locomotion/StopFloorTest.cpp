// Story SCL-009 (production/epics/shared-character-locomotion/story-009-stop-floor.md), AC-SCL-50 StopFloor (F2, D2):
// GIVEN stops to rest from v0 = 20, 37, 60, 93, 112 and 140 cm/s at walk and 200 and 450 cm/s at run, THEN each stop
// lasts max(v0 / d_g, StopMinSec) +- 1 frame (0.5 s for 20-93 at walk and 200 at run; 0.60, 0.75 and 0.75 s for 112,
// 140 and 450), the deceleration is constant through each stop (latched at its start; per-frame deceleration equals the
// latched value within a pinned epsilon at a fixed 1/60 s step), and the same input sequence gives the same speed trace
// on a player pawn (the owner input path) and an NPC pawn (UPawnCommandComponent::Move), equal within 1 frame.
// Also: no stop lasts longer than T_stop,max (I-SCL-12/15 rely on it).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/Locomotion/LocomotionTestWorld.h"
#include "View/Core/StealthLocomotionModel.h"

namespace StopFloorTestData
{
	struct FStopCase
	{
		float V0;
		EGait Gait;
		float ExpectedSec;
	};
	/** The AC table: v0 and the stop duration it states. */
	const FStopCase Cases[] = {
		{ 20.0f, EGait::Walk, 0.50f }, { 37.0f, EGait::Walk, 0.50f }, { 60.0f, EGait::Walk, 0.50f }, { 93.0f, EGait::Walk, 0.50f },
		{ 112.0f, EGait::Walk, 0.60f }, { 140.0f, EGait::Walk, 0.75f }, { 200.0f, EGait::Run, 0.50f }, { 450.0f, EGait::Run, 0.75f },
	};
	/** Per-frame deceleration must equal the latched value within this (cm/s^2) at the 1/60 s step. */
	constexpr float DecelEpsilon = 0.01f;
	constexpr int32 MaxFrames = 600;

	/** One scripted segment of the parity sequence. */
	struct FSegment
	{
		int32 Frames;
		float Magnitude;
		EGait Gait;
	};
	/** Walk, stop, a key tap, stop, run, release Run (walk on), stop. */
	const FSegment Sequence[] = {
		{ 120, 1.0f, EGait::Walk }, { 60, 0.0f, EGait::Walk }, { 6, 1.0f, EGait::Walk }, { 45, 0.0f, EGait::Walk },
		{ 120, 1.0f, EGait::Run }, { 60, 1.0f, EGait::Walk }, { 70, 0.0f, EGait::Walk },
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLocomotionStopFloorTest, "StealthGame.Locomotion.StopFloor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FLocomotionStopFloorTest::RunTest(const FString& Parameters)
{
	using namespace StopFloorTestData;
	const FLocomotionTuning_Public Tuning;
	const float Frame = StealthLocomotionTest::Step;

	// Model: every AC stop, with the deceleration latched at its start.
	for (const FStopCase& Case : Cases)
	{
		FStealthMotionState State;
		State.Speed = Case.V0;
		State.LastGait = Case.Gait;
		const FStealthMoveInput Released;
		int32 Frames = 0;
		float Latched = -1.0f;
		bool bConstant = true;
		while (State.Speed > 0.0f && Frames < MaxFrames)
		{
			const float Before = State.Speed;
			FStealthLocomotionModel::Step(Tuning, Released, Frame, State);
			++Frames;
			const float Decel = (Before - State.Speed) / Frame;
			if (Latched < 0.0f)
			{
				Latched = State.StopDecel;
			}
			// Every frame but the last (which ends at exactly 0) decelerates at the latched value.
			if (State.Speed > 0.0f && FMath::Abs(Decel - Latched) > DecelEpsilon)
			{
				bConstant = false;
			}
		}
		const float Seconds = Frames * Frame;
		const float Formula = FMath::Max(Case.V0 / (Case.Gait == EGait::Run ? StealthLocomotion::RunDeceleration(Tuning) : StealthLocomotion::WalkDeceleration(Tuning)), Tuning.StopMinSec);
		AddInfo(FString::Printf(TEXT("STOPFLOOR v0 %.0f %s: %.3f s (%d frames), latched d_eff %.2f cm/s^2"), Case.V0, Case.Gait == EGait::Run ? TEXT("run") : TEXT("walk"), Seconds, Frames, Latched));
		TestTrue(FString::Printf(TEXT("v0 %.0f: stop lasts %.3f s, AC %.2f s +- 1 frame"), Case.V0, Seconds, Case.ExpectedSec), FMath::Abs(Seconds - Case.ExpectedSec) <= Frame + 1e-4f);
		TestTrue(FString::Printf(TEXT("v0 %.0f: stop equals max(v0 / d_g, StopMinSec) = %.3f s +- 1 frame"), Case.V0, Formula), FMath::Abs(Seconds - Formula) <= Frame + 1e-4f);
		TestTrue(FString::Printf(TEXT("v0 %.0f: constant deceleration through the stop"), Case.V0), bConstant);
		TestTrue(FString::Printf(TEXT("v0 %.0f: at least StopMinSec"), Case.V0), Seconds >= Tuning.StopMinSec - Frame);
		TestTrue(FString::Printf(TEXT("v0 %.0f: no longer than T_stop,max %.2f s"), Case.V0, StealthLocomotion::LongestStopSec(Tuning)), Seconds <= StealthLocomotion::LongestStopSec(Tuning) + Frame + 1e-4f);
	}

	// World: the same sequence through the owner input path and the Move entry point gives the same speed trace.
	StealthLocomotionTest::FWorld TestWorld(TEXT("StopFloorTestWorld"));
	AStealthPawn* Player = TestWorld.SpawnPawn(FVector2D(0.0, 0.0));
	AStealthPawn* Npc = TestWorld.SpawnPawn(FVector2D(0.0, 600.0));
	double MaxDiff = 0.0;
	int32 PlayerRest = INDEX_NONE;
	int32 NpcRest = INDEX_NONE;
	int32 FrameIndex = 0;
	bool bPlayerMoved = false;
	for (const FSegment& Segment : Sequence)
	{
		StealthLocomotionTest::DriveMove(Npc, 0.0f, Segment.Magnitude, Segment.Gait);
		for (int32 Index = 0; Index < Segment.Frames; ++Index, ++FrameIndex)
		{
			StealthLocomotionTest::DriveOwner(Player, 0.0f, Segment.Magnitude, Segment.Gait);
			StealthLocomotionTest::StepPawn(Player);
			StealthLocomotionTest::StepPawn(Npc);
			const double PlayerSpeed = StealthLocomotionTest::Speed2D(Player);
			const double NpcSpeed = StealthLocomotionTest::Speed2D(Npc);
			bPlayerMoved |= PlayerSpeed > 100.0;
			MaxDiff = FMath::Max(MaxDiff, FMath::Abs(PlayerSpeed - NpcSpeed));
			if (PlayerRest == INDEX_NONE && bPlayerMoved && PlayerSpeed <= 0.0 && Segment.Magnitude <= 0.0f) { PlayerRest = FrameIndex; }
			if (NpcRest == INDEX_NONE && bPlayerMoved && NpcSpeed <= 0.0 && Segment.Magnitude <= 0.0f) { NpcRest = FrameIndex; }
		}
	}
	AddInfo(FString::Printf(TEXT("STOPFLOOR parity: max per-frame speed difference %.4f cm/s; first rest frame player %d, NPC %d"), MaxDiff, PlayerRest, NpcRest));
	TestTrue(TEXT("the pawns walked and ran"), bPlayerMoved);
	TestTrue(TEXT("player and NPC speed traces are equal frame by frame"), MaxDiff <= 0.5);
	TestTrue(TEXT("player and NPC reach rest within 1 frame of each other"), PlayerRest != INDEX_NONE && NpcRest != INDEX_NONE && FMath::Abs(PlayerRest - NpcRest) <= 1);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
