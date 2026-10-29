// Story con-019 (production/epics/concealment-bushes/story-019-body-hidden-latch-function.md; GDD CON-R6b, AC-CON-08;
// ADR-0017 section 5), StealthGame.Concealment.BodyHiddenLatchTable:
//   AC1 the table: an owner is never hidden; each set rule sets it at its exact boundary (BushEnter at Start + swap is
//       hidden, BushExit at Start + crossing is shown, a bush-emerge strike at Start + emerge is shown, the ally flag,
//       bDiedConcealed); it clears only on a passed crossing or, fresh, with no evidence; a pose change to None, a death
//       pose or a strike pose never clears a set latch.
//   AC2 precedence: before the swap with the ally flag false it waits; the ally flag sets it at once; a death before the
//       commit cancels the pending swap; a death after the commit never clears.
//   AC3 an ally watching the whole enter (pose BushEnter -> None at +0.9 s) swept at 1/60 s: hidden at every Now from the
//       swap on, so no body frame appears.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "View/BodyHiddenLatch.h"

namespace ConcealmentLatchTestData
{
	FActionState Pose(EActionPose P, double Start, uint8 Variant = 0)
	{
		FActionState A;
		A.Pose = P;
		A.StartServerTime = Start;
		A.Variant = Variant;
		return A;
	}

	bool Eval(const FActionState& A, double Now, bool bWas, bool bAlly = false, bool bOwner = false, double DeathAt = -1.0, bool bDiedConcealed = false, bool bFresh = false)
	{
		FBodyHiddenInputs In;
		In.Action = A;
		In.Death.DeathServerTime = DeathAt;
		In.Death.bDiedConcealed = bDiedConcealed;
		In.bAllyIsConcealed = bAlly;
		In.bIsOwner = bOwner;
		In.Now = Now;
		In.bWasHidden = bWas;
		In.bFresh = bFresh;
		return EvaluateBodyHidden(In, FConcealmentTuning_Public());
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConcealmentBodyHiddenLatchTest, "StealthGame.Concealment.BodyHiddenLatchTable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FConcealmentBodyHiddenLatchTest::RunTest(const FString& Parameters)
{
	using namespace ConcealmentLatchTestData;
	const FConcealmentTuning_Public T;
	const double S = 10.0;
	const FActionState Enter = Pose(EActionPose::BushEnter, S);
	const FActionState Exit = Pose(EActionPose::BushExit, S);
	const FActionState Emerge = Pose(EActionPose::Strike, S, StrikeVariant::BushPreface);
	const FActionState None = Pose(EActionPose::None, S);
	const FActionState PlainStrike = Pose(EActionPose::Strike, S, 0);

	// AC1: the owner never hides its own body, whatever the evidence.
	TestFalse(TEXT("owner: BushEnter after the swap"), Eval(Enter, S + 0.6, true, true, true));
	TestFalse(TEXT("owner: bDiedConcealed"), Eval(None, S + 2.0, true, false, true, S + 1.0, true));
	// Set rules at their boundaries.
	TestFalse(TEXT("BushEnter just before the swap: shown"), Eval(Enter, S + T.BushEnterSwapSec - 1e-4, false));
	TestTrue(TEXT("BushEnter at Start + swap exactly: hidden"), Eval(Enter, S + T.BushEnterSwapSec, false));
	TestTrue(TEXT("the ally flag: hidden"), Eval(None, S, false, true));
	TestTrue(TEXT("BushExit before the crossing: hidden"), Eval(Exit, S + T.BushExitCrossSec - 1e-4, true));
	TestFalse(TEXT("BushExit at Start + crossing exactly: shown (the crossing clears)"), Eval(Exit, S + T.BushExitCrossSec, true));
	TestTrue(TEXT("bush-emerge strike before its crossing: hidden"), Eval(Emerge, S + T.BushStrikeEmergeSec - 1e-4, true));
	TestFalse(TEXT("bush-emerge strike at Start + emerge exactly: shown"), Eval(Emerge, S + T.BushStrikeEmergeSec, true));
	TestTrue(TEXT("bDiedConcealed: hidden"), Eval(None, S + 3.0, false, false, false, S + 1.0, true));
	// Clear rules.
	TestTrue(TEXT("a set latch holds through a pose change to None"), Eval(None, S + 1.0, true));
	TestTrue(TEXT("a set latch holds through a plain strike pose"), Eval(PlainStrike, S + 1.0, true));
	TestTrue(TEXT("a set latch holds through a death (no bDiedConcealed)"), Eval(None, S + 5.0, true, false, false, S + 4.0, false));
	TestFalse(TEXT("an unset latch stays unset with no evidence"), Eval(None, S + 1.0, false));
	TestFalse(TEXT("fresh actor, no evidence at all: clears"), Eval(None, S + 1.0, true, false, false, -1.0, false, true));
	TestTrue(TEXT("fresh actor during BushEnter after the swap: hidden"), Eval(Enter, S + 0.7, true, false, false, -1.0, false, true));
	TestTrue(TEXT("fresh actor during BushExit before the crossing: hidden"), Eval(Exit, S + 0.2, true, false, false, -1.0, false, true));
	TestFalse(TEXT("fresh actor after the crossing time passed: shown"), Eval(Exit, S + 0.6, true, false, false, -1.0, false, true));
	TestTrue(TEXT("fresh actor, Concealed with pose None on an ally client: hidden"), Eval(None, S + 2.0, true, true, false, -1.0, false, true));
	TestTrue(TEXT("fresh actor after a death with bDiedConcealed: hidden"), Eval(None, S + 2.0, true, false, false, S + 1.0, true, true));

	// AC2 precedence.
	TestFalse(TEXT("BushEnter before its swap, ally flag false: waits (shown)"), Eval(Enter, S + 0.2, false));
	TestTrue(TEXT("ally flag true before the pose sets: hidden at once"), Eval(Enter, S + 0.2, false, true));
	TestFalse(TEXT("death before the commit cancels the pending swap (shown after the swap time)"), Eval(Enter, S + 0.7, false, false, false, S + 0.3));
	TestTrue(TEXT("death after the commit never clears"), Eval(Enter, S + 1.5, true, false, false, S + 0.6));

	// AC3: an ally watching the whole enter, BushEnter -> None at +0.9 s, swept at 1/60 s.
	bool bLatch = false;
	int32 BodyFramesAfterSwap = 0;
	for (int32 Frame = 0; Frame <= 120; ++Frame)
	{
		const double Now = S + Frame / 60.0;
		const bool bEnded = Now >= S + T.BushEnterDurSec;
		const bool bAllyFlag = Now >= S + T.BushEnterSwapSec + 0.1; // the C-TeamView flag arrives a little after the commit
		bLatch = Eval(bEnded ? None : Enter, Now, bLatch, bAllyFlag);
		if (Now >= S + T.BushEnterSwapSec && !bLatch)
		{
			++BodyFramesAfterSwap;
		}
	}
	TestEqual(TEXT("ally sweep: no body frame from the swap on"), BodyFramesAfterSwap, 0);
	// The same sweep without the ally flag (an opponent before its cull lands): hidden from the swap on as well.
	bLatch = false;
	BodyFramesAfterSwap = 0;
	for (int32 Frame = 0; Frame <= 120; ++Frame)
	{
		const double Now = S + Frame / 60.0;
		bLatch = Eval(Now >= S + T.BushEnterDurSec ? None : Enter, Now, bLatch);
		BodyFramesAfterSwap += Now >= S + T.BushEnterSwapSec && !bLatch ? 1 : 0;
	}
	TestEqual(TEXT("opponent sweep: no body frame from the swap on (the latch holds through pose None)"), BodyFramesAfterSwap, 0);
	TestTrue(TEXT("IsBushEmergeStrike reads the BushPreface bit"), IsBushEmergeStrike(Emerge) && !IsBushEmergeStrike(PlainStrike));
	return true;
}

#endif
