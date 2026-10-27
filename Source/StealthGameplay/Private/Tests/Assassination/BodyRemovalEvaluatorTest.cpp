// Story asn-020 supporting test (VA-ASN-2, ADR-0018 section 6.1): StealthGame.Assassination.BodyRemovalEvaluator
// (class FAssassinationBodyRemovalEvaluatorTest). The cut-at-occlusion rule's timing and one-frame-cut logic, headless:
// before DeathServerTime + DeathFallSec + BodyLingerSec a body is kept whatever the view; in the window it is hidden on the
// first frame it is out of the frustum or fully occluded; a body still in view at grace expiry is cut on that frame (one
// decision, no intermediate state); a body first received after the linger is never shown; a hidden body stays hidden.
// The same inputs give the same decision for every victim kind (the rule has no kind input).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "BodyRemovalEvaluator.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAssassinationBodyRemovalEvaluatorTest, "StealthGame.Assassination.BodyRemovalEvaluator",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAssassinationBodyRemovalEvaluatorTest::RunTest(const FString& Parameters)
{
	constexpr double Frame = 1.0 / 60.0;
	FBodyRemovalInput Base;
	Base.DeathServerTime = 50.0;
	const double Window = FBodyRemovalEvaluator::WindowOpensAt(Base);
	const double Grace = FBodyRemovalEvaluator::GraceEndsAt(Base);
	TestTrue(TEXT("the window opens at DeathServerTime + 1.2 + 6.0"), FMath::IsNearlyEqual(Window, 57.2, 1e-9));
	TestTrue(TEXT("the grace ends 2.0 s later"), FMath::IsNearlyEqual(Grace, 59.2, 1e-9));

	auto Decide = [&Base](double Now, bool bInFrustum, bool bOccluded)
	{
		FBodyRemovalInput In = Base;
		In.Now = Now;
		In.bInFrustum = bInFrustum;
		In.bFullyOccluded = bOccluded;
		return FBodyRemovalEvaluator::Evaluate(In);
	};

	// Before the window: kept even when out of view or occluded (the fall and the linger are never cut short).
	TestEqual(TEXT("falling, out of view: kept"), static_cast<int32>(Decide(50.5, false, false)), static_cast<int32>(EBodyRemovalDecision::Keep));
	TestEqual(TEXT("lingering, occluded: kept"), static_cast<int32>(Decide(Window - Frame, true, true)), static_cast<int32>(EBodyRemovalDecision::Keep));
	// In the window: the first frame out of the frustum or fully occluded hides it.
	TestEqual(TEXT("window, out of the frustum: hidden this frame"), static_cast<int32>(Decide(Window, false, false)), static_cast<int32>(EBodyRemovalDecision::HideOutOfFrustum));
	TestEqual(TEXT("window, fully occluded: hidden this frame"), static_cast<int32>(Decide(Window + Frame, true, true)), static_cast<int32>(EBodyRemovalDecision::HideOccluded));
	TestEqual(TEXT("window, still in view: kept"), static_cast<int32>(Decide(Grace - Frame, true, false)), static_cast<int32>(EBodyRemovalDecision::Keep));
	// Grace expiry: the one-frame cut.
	TestEqual(TEXT("still looking at grace expiry: cut in this single frame"), static_cast<int32>(Decide(Grace, true, false)), static_cast<int32>(EBodyRemovalDecision::CutAtGrace));
	TestTrue(TEXT("the cut hides (one decision from fully visible to fully hidden)"), FBodyRemovalEvaluator::Hides(EBodyRemovalDecision::CutAtGrace));
	TestFalse(TEXT("Keep never hides"), FBodyRemovalEvaluator::Hides(EBodyRemovalDecision::Keep));

	FBodyRemovalInput Hidden = Base;
	Hidden.Now = Grace + 1.0;
	Hidden.bAlreadyHidden = true;
	TestEqual(TEXT("a hidden body stays hidden"), static_cast<int32>(FBodyRemovalEvaluator::Evaluate(Hidden)), static_cast<int32>(EBodyRemovalDecision::AlreadyHidden));
	TestFalse(TEXT("AlreadyHidden asks nothing more"), FBodyRemovalEvaluator::Hides(EBodyRemovalDecision::AlreadyHidden));

	FBodyRemovalInput LateActor = Base;
	LateActor.Now = Window + 0.5;
	LateActor.bFirstSeenAfterLinger = true;
	TestEqual(TEXT("an actor first received after the linger is never shown"), static_cast<int32>(FBodyRemovalEvaluator::Evaluate(LateActor)), static_cast<int32>(EBodyRemovalDecision::NeverShow));

	FBodyRemovalInput NoDeath;
	NoDeath.Now = 1000.0;
	TestEqual(TEXT("no death write: kept"), static_cast<int32>(FBodyRemovalEvaluator::Evaluate(NoDeath)), static_cast<int32>(EBodyRemovalDecision::Keep));

	// Identity-blind: the decision sequence over a whole sequence is a function of time and geometry only, so three
	// victims of three kinds with the same DeathServerTime and view give the same decisions on the same frames.
	TArray<EBodyRemovalDecision> Sequence;
	for (double T = Base.DeathServerTime; T <= Grace + Frame; T += Frame)
	{
		Sequence.Add(Decide(T, true, false));
	}
	const int32 FirstCut = Sequence.IndexOfByKey(EBodyRemovalDecision::CutAtGrace);
	TestTrue(TEXT("in view throughout: exactly the grace-expiry frame cuts (no earlier hide)"), FirstCut != INDEX_NONE
		&& !Sequence.ContainsByPredicate([](EBodyRemovalDecision D) { return D == EBodyRemovalDecision::HideOccluded || D == EBodyRemovalDecision::HideOutOfFrustum; }));
	TestTrue(TEXT("the cut frame is the grace expiry (+- one frame)"), FMath::Abs(Base.DeathServerTime + FirstCut * Frame - Grace) <= Frame + 1e-6);
	return true;
}

#endif
