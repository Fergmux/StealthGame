// Story asn-022 (production/epics/assassination-defence-reversal/story-022-culled-victim-opens-falling-and-late-actors.md),
// AC2 edge cases (pure half; the PIE half is StealthGame.Assassination.PIE.LateActorSeeksNeverSnaps):
// StealthGame.Assassination.LateActorBoundaries. A body whose FDeathState this machine first receives late is evaluated from
// the replicated DeathServerTime and this machine's clock, never from the arrival:
//  - on time: Death_Fall from 0 with the inertialised entry (FallEntryBlendSec), not late;
//  - 0.6 s late: sought to 0.6 s of server time (clip position 0.6 x rate, +- nothing: the plan is exact), late, cross-faded
//    within MaxCrossFadeSec (<= 0.1 s), still Falling and shown;
//  - exactly at + DeathFallSec (1.2 s): held at the clip end, Lingering, collision off at once, shown;
//  - just before + 7.2 s: shown (Lingering); exactly at + 7.2 s, during the grace (+ 8.0 s) and at + 7.5 s: never shown
//    (bNeverShow), and VA-ASN-2's evaluator answers NeverShow for it, a hiding decision;
//  - the plan is the same whether the arrival is a re-open or a first sight (it reads only the struct and the clock).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "BodyRemovalEvaluator.h"
#include "View/EncounterPresentationCore.h"
#include "View/StealthTuningPublicSubstructs.h"

namespace LateActorBoundariesTestData
{
	/** The death this suite presents (any time works: the plan reads only offsets from it). */
	constexpr double DeathServerTime = 500.0;

	/** The plan of a body first received FirstSeenOffset after its DeathServerTime, evaluated on arrival. */
	FEncounterPresentationPlan Arrive(double FirstSeenOffset)
	{
		FEncounterPresentationInput In;
		In.Death.DeathServerTime = DeathServerTime;
		In.Now = DeathServerTime + FirstSeenOffset;
		In.DeathReceiptNow = In.Now;
		In.bClockSynced = true;
		return FEncounterPresentationCore::Evaluate(FEncounterPresentationParams::FromPublic(FAssassinationTuning_Public()), FEncounterClipTimings(), In);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAssassinationLateActorBoundariesTest, "StealthGame.Assassination.LateActorBoundaries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAssassinationLateActorBoundariesTest::RunTest(const FString& Parameters)
{
	using namespace LateActorBoundariesTestData;
	// Arrange
	const FEncounterPresentationParams P = FEncounterPresentationParams::FromPublic(FAssassinationTuning_Public());
	const FEncounterClipTimings Clips;
	const double Rate = Clips.FallLengthSec / P.DeathFallSec;
	const double Linger = P.DeathFallSec + P.BodyLingerSec;

	// Act + Assert: on time
	const FEncounterPresentationPlan OnTime = Arrive(0.0);
	TestTrue(TEXT("on time: Death_Fall from its first frame"), OnTime.Clip == EEncounterClip::DeathFall && FMath::IsNearlyZero(OnTime.Position));
	TestFalse(TEXT("on time: not late"), OnTime.bLate);
	TestEqual(TEXT("on time: the inertialised entry (FallEntryBlendSec)"), OnTime.BlendInSec, P.FallEntryBlendSec);

	// 0.6 s late
	const FEncounterPresentationPlan Mid = Arrive(0.6);
	TestTrue(TEXT("0.6 s late: Death_Fall"), Mid.Clip == EEncounterClip::DeathFall && Mid.bDeathPresented);
	TestEqual(TEXT("0.6 s late: sought to 0.6 s of the fall (clip position 0.6 x rate)"), Mid.Position, 0.6 * Rate, 1e-9);
	TestEqual(TEXT("0.6 s late: played at the fall rate"), Mid.PlayRate, Rate, 1e-9);
	TestTrue(TEXT("0.6 s late: flagged late"), Mid.bLate);
	TestTrue(TEXT("0.6 s late: cross-faded within MaxCrossFadeSec (<= 0.1 s), never a snap"), Mid.BlendInSec > 0.0 && Mid.BlendInSec <= P.MaxCrossFadeSec + 1e-9 && P.MaxCrossFadeSec <= 0.1 + 1e-9);
	TestTrue(TEXT("0.6 s late: Falling, collision still on, shown"), Mid.BodyPhase == EEncounterBodyPhase::Falling && !Mid.bCollisionOff && !Mid.bNeverShow);

	// Exactly at + DeathFallSec
	const FEncounterPresentationPlan FallEnd = Arrive(P.DeathFallSec);
	TestTrue(TEXT("+ 1.2 s: held at the clip end, Lingering"), FallEnd.bClipEnded && FallEnd.BodyPhase == EEncounterBodyPhase::Lingering);
	TestEqual(TEXT("+ 1.2 s: clip position at its end"), FallEnd.Position, Clips.FallLengthSec, 1e-9);
	TestTrue(TEXT("+ 1.2 s: collision off at once, shown"), FallEnd.bCollisionOff && !FallEnd.bNeverShow);

	// Around + 7.2 s (the linger's end) and past it
	const FEncounterPresentationPlan BeforeLingerEnd = Arrive(Linger - 0.01);
	TestTrue(TEXT("just before + 7.2 s: shown (Lingering)"), !BeforeLingerEnd.bNeverShow && BeforeLingerEnd.BodyPhase == EEncounterBodyPhase::Lingering);
	for (const double Offset : { Linger, Linger + 0.3, Linger + 0.8 })
	{
		const FEncounterPresentationPlan Late = Arrive(Offset);
		TestTrue(*FString::Printf(TEXT("+ %.1f s: never shown"), Offset), Late.bNeverShow);
		FBodyRemovalInput Removal;
		Removal.Now = DeathServerTime + Offset;
		Removal.DeathServerTime = DeathServerTime;
		Removal.DeathFallSec = P.DeathFallSec;
		Removal.BodyLingerSec = P.BodyLingerSec;
		Removal.BodyRemovalGraceSec = P.BodyRemovalGraceSec;
		Removal.bInFrustum = true;
		Removal.bFirstSeenAfterLinger = Late.bNeverShow;
		const EBodyRemovalDecision Decision = FBodyRemovalEvaluator::Evaluate(Removal);
		TestTrue(*FString::Printf(TEXT("+ %.1f s: VA-ASN-2 answers NeverShow even in full view, and it hides"), Offset),
			Decision == EBodyRemovalDecision::NeverShow && FBodyRemovalEvaluator::Hides(Decision));
	}

	// A re-open and a first sight are the same evaluation: the plan has no other input.
	const FEncounterPresentationPlan Again = Arrive(0.6);
	TestTrue(TEXT("the same inputs give the same plan (re-open = first sight)"), Again.Position == Mid.Position && Again.BlendInSec == Mid.BlendInSec && Again.bLate == Mid.bLate);
	return true;
}

#endif
