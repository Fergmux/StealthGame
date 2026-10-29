// Camera story cs-002 AC3 (production/epics/camera-system/story-002-view-envelope-pure-function.md; GDD camera-system.md
// AC-CS-38, F1 pushing): StealthGame.Camera.MouseLatchedPush. Mouse-style input at the yaw limit: 0.08 s bursts at
// 200 deg/s separated by 0.12 s gaps for 1.5 s keep `pushing` true throughout and the target above 70; with 0.2 s gaps
// (longer than EdgePushLatchSec 0.15) the target relaxes in each gap.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "CameraEnvelopeTestSupport.h"

namespace CameraMouseLatchedPushTestData
{
	using namespace StealthCameraEnvelopeTest;

	struct FResult
	{
		bool bPushingThroughout = true;
		bool bTargetAboveThroughout = true;
		int32 GapsRelaxed = 0;
		int32 Gaps = 0;
	};

	/** At the limit first, then bursts of 0.08 s at 200 deg/s separated by GapSec, for 1.5 s. */
	FResult Run(double GapSec)
	{
		const FViewEnvelopeParams Params = DefaultParams();
		FViewEnvelopeState State;
		for (int32 F = 0; F < FramesOf(1.0); ++F)
		{
			Frame(State, Params, 200.0); // reach the limit and the 75 target
		}
		FResult Out;
		const int32 Burst = FramesOf(0.08);
		const int32 Gap = FramesOf(GapSec);
		int32 Elapsed = 0;
		while (Elapsed < FramesOf(1.5))
		{
			for (int32 F = 0; F < Burst; ++F, ++Elapsed)
			{
				Frame(State, Params, 200.0);
				Out.bPushingThroughout &= State.bPushing;
				Out.bTargetAboveThroughout &= State.TargetYawOffsetDeg > 70.0 + 1e-6;
			}
			bool bRelaxed = false;
			for (int32 F = 0; F < Gap; ++F, ++Elapsed)
			{
				Frame(State, Params, 0.0);
				Out.bPushingThroughout &= State.bPushing;
				Out.bTargetAboveThroughout &= State.TargetYawOffsetDeg > 70.0 + 1e-6;
				bRelaxed |= State.TargetYawOffsetDeg < 75.0 - 1e-6;
			}
			++Out.Gaps;
			Out.GapsRelaxed += bRelaxed ? 1 : 0;
		}
		return Out;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCameraMouseLatchedPushTest, "StealthGame.Camera.MouseLatchedPush",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCameraMouseLatchedPushTest::RunTest(const FString& Parameters)
{
	using namespace CameraMouseLatchedPushTestData;
	const FResult Short = Run(0.12);
	TestTrue(TEXT("0.12 s gaps: pushing stays true throughout"), Short.bPushingThroughout);
	TestTrue(TEXT("0.12 s gaps: the target stays above 70"), Short.bTargetAboveThroughout);
	TestEqual(TEXT("0.12 s gaps: no gap relaxes the target"), Short.GapsRelaxed, 0);
	const FResult Long = Run(0.2);
	TestFalse(TEXT("0.2 s gaps: pushing lapses"), Long.bPushingThroughout);
	TestTrue(*FString::Printf(TEXT("0.2 s gaps: the target relaxes in each gap (%d of %d)"), Long.GapsRelaxed, Long.Gaps), Long.Gaps > 0 && Long.GapsRelaxed == Long.Gaps);
	return true;
}

#endif
