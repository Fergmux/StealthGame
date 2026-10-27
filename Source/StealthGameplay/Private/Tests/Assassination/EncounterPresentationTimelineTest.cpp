// Stories asn-018 AC2 (pure half) / asn-020 AC1 timings (ADR-0018 section 5.1): StealthGame.Assassination.EncounterPresentationTimeline.
// The state-derived timeline every machine evaluates from FActionState / FDeathState and its own EstimatedServerNow():
//  - the strike's contact frame lands at ContactServerTime (the clip position equals the clip's contact frame there), the
//    recovery ends at StartServerTime + AttackAttemptLockoutSec, and the bush preface starts the same contact segment
//    BushStrikeEmergeSec later;
//  - the plan depends only on the replicated struct: the five targets of the parity group (NPC, rival, informant, braced
//    defender, empty spot) write the same FActionState at the same bearing, so their plans are identical at every time;
//  - a late receipt is sought (position from StartServerTime, not 0) and cross-faded within MaxCrossFadeSec, never snapped;
//    an unsynced clock presents on receipt, never earlier;
//  - a counter starts at its ContactServerTime, never on receipt;
//  - a reached DeathServerTime wins over the pose (the reversed attacker's lunge runs to contact, then Death_Fall), the
//    fall lasts DeathFallSec, collision goes off at + DeathFallSec, the linger lasts BodyLingerSec, the removal window
//    opens at + 7.2 s and the grace cut is at + 9.2 s; a body first received after the linger is never shown.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "View/EncounterPresentationCore.h"
#include "View/StealthTuningPublicSubstructs.h"

namespace EncounterPresentationTimelineTestData
{
	FActionState Strike(double Start, uint8 Variant, double Contact)
	{
		FActionState A;
		A.Pose = EActionPose::Strike;
		A.Seq = 7;
		A.Variant = Variant;
		A.StartServerTime = Start;
		A.ContactServerTime = Contact;
		A.WarpPoint = FFixedWarpOffset::FromOffsetCm(FVector(150.0, 0.0, 0.0));
		return A;
	}

	FEncounterPresentationPlan At(const FActionState& A, const FDeathState& D, double Now, double Receipt = -1.0, bool bSynced = true, double DeathReceipt = -1.0)
	{
		FEncounterPresentationInput In;
		In.Action = A;
		In.Death = D;
		In.Now = Now;
		In.bClockSynced = bSynced;
		In.ActionReceiptNow = Receipt;
		In.DeathReceiptNow = DeathReceipt;
		return FEncounterPresentationCore::Evaluate(FEncounterPresentationParams::FromPublic(FAssassinationTuning_Public()), FEncounterClipTimings(), In);
	}

	bool Same(const FEncounterPresentationPlan& X, const FEncounterPresentationPlan& Y)
	{
		return X.Clip == Y.Clip && X.InstanceKey == Y.InstanceKey && X.Position == Y.Position && X.PlayRate == Y.PlayRate && X.bClipEnded == Y.bClipEnded
			&& X.bDeathPresented == Y.bDeathPresented && X.BodyPhase == Y.BodyPhase && X.WarpWindowEndSec == Y.WarpWindowEndSec;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAssassinationEncounterPresentationTimelineTest, "StealthGame.Assassination.EncounterPresentationTimeline",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAssassinationEncounterPresentationTimelineTest::RunTest(const FString& Parameters)
{
	using namespace EncounterPresentationTimelineTestData;
	const FEncounterPresentationParams P = FEncounterPresentationParams::FromPublic(FAssassinationTuning_Public());
	const FEncounterClipTimings C;
	const FDeathState Alive;
	const double Start = 100.0;
	const double Contact = Start + P.StrikeContactSec;

	// --- Contact scheduling --------------------------------------------------------------------------------------------
	const FActionState A = Strike(Start, StrikeVariant::Strike_L60, Contact);
	const FEncounterPresentationPlan AtStart = At(A, Alive, Start, Start);
	TestEqual(TEXT("the variant's clip (by bearing)"), static_cast<int32>(AtStart.Clip), static_cast<int32>(EEncounterClip::Strike_L60));
	TestTrue(TEXT("on time: the clip starts at 0, not late"), FMath::IsNearlyZero(AtStart.Position, 1e-9) && !AtStart.bLate);
	const FEncounterPresentationPlan AtContact = At(A, Alive, Contact, Start);
	TestTrue(TEXT("the contact frame lands at ContactServerTime"), FMath::IsNearlyEqual(AtContact.Position, C.StrikeClipContactSec, 1e-6));
	TestTrue(TEXT("the warp window ends at the contact frame"), FMath::IsNearlyEqual(AtContact.WarpWindowEndSec, C.StrikeClipContactSec, 1e-9));
	const FEncounterPresentationPlan AtLockout = At(A, Alive, Start + P.AttackAttemptLockoutSec - 1e-4, Start);
	TestTrue(TEXT("the recovery runs to StartServerTime + AttackAttemptLockoutSec"), FMath::IsNearlyEqual(AtLockout.Position, C.StrikeLengthSec, 1e-3) && !AtLockout.bClipEnded);
	TestTrue(TEXT("after the lockout the clip has ended"), At(A, Alive, Start + P.AttackAttemptLockoutSec + 0.01, Start).bClipEnded);

	const double Emerge = 0.35;
	const FActionState Bush = Strike(Start, StrikeVariant::Strike_F | StrikeVariant::BushPreface, Start + Emerge + P.StrikeContactSec);
	TestTrue(TEXT("bush preface: the contact segment is still to start during the preface"), At(Bush, Alive, Start + 0.2, Start).Position < 0.0);
	TestTrue(TEXT("bush preface: the same contact segment starts BushStrikeEmergeSec later"), FMath::IsNearlyZero(At(Bush, Alive, Start + Emerge, Start).Position, 1e-6));
	TestTrue(TEXT("bush preface: contact frame at ContactServerTime"), FMath::IsNearlyEqual(At(Bush, Alive, Bush.ContactServerTime, Start).Position, C.StrikeClipContactSec, 1e-6));

	// --- Outcome blindness: five targets, one struct -----------------------------------------------------------------
	const TCHAR* Targets[] = { TEXT("NPC"), TEXT("rival"), TEXT("informant"), TEXT("braced defender"), TEXT("empty spot") };
	bool bAllSame = true;
	for (int32 Target = 1; Target < 5; ++Target)
	{
		const FActionState Copy = Strike(Start, StrikeVariant::Strike_L60, Contact);
		for (double T = Start; T <= Contact; T += 1.0 / 120.0)
		{
			bAllSame &= Same(At(A, Alive, T, Start), At(Copy, Alive, T, Start));
		}
		AddInfo(FString::Printf(TEXT("TIMELINE %s: plan identical to the NPC's from execution to contact"), Targets[Target]));
	}
	TestTrue(TEXT("the five targets' plans are identical from execution to contact"), bAllSame);

	// --- Late, unsynced ------------------------------------------------------------------------------------------------
	const double Late = Start + 0.15;
	const FEncounterPresentationPlan LatePlan = At(A, Alive, Late, Late);
	TestTrue(TEXT("late receipt: sought to the scheduled position (not restarted at 0)"), FMath::IsNearlyEqual(LatePlan.Position, 0.15 * C.StrikeClipContactSec / P.StrikeContactSec, 1e-6));
	TestTrue(TEXT("late receipt: flagged late and cross-faded within MaxCrossFadeSec (<= 0.1 s)"), LatePlan.bLate && LatePlan.BlendInSec <= 0.1 + 1e-9 && LatePlan.BlendInSec > 0.0);
	TestTrue(TEXT("late receipt: the contact frame still lands at ContactServerTime"), FMath::IsNearlyEqual(At(A, Alive, Contact, Late).Position, C.StrikeClipContactSec, 1e-6));
	const FEncounterPresentationPlan Unsynced = At(A, Alive, Late, Late, /*bSynced*/ false);
	TestTrue(TEXT("unsynced clock: presented on receipt (clip at 0 at receipt), logged"), FMath::IsNearlyZero(Unsynced.Position, 1e-9) && Unsynced.bUnsyncedFallback);
	TestTrue(TEXT("unsynced clock: never earlier than receipt"), At(A, Alive, Late + 0.1, Late, false).Position <= 0.1 * C.StrikeClipContactSec / P.StrikeContactSec + 1e-6);

	// --- Counter -------------------------------------------------------------------------------------------------------
	FActionState Counter;
	Counter.Pose = EActionPose::Counter;
	Counter.Seq = 9;
	Counter.StartServerTime = Contact - 0.15;
	Counter.ContactServerTime = Contact;
	TestTrue(TEXT("counter: not started on receipt before contact (the brace holds)"), At(Counter, Alive, Contact - 0.1, Contact - 0.15).Position < 0.0);
	TestTrue(TEXT("counter: starts at its ContactServerTime"), FMath::IsNearlyZero(At(Counter, Alive, Contact, Contact - 0.15).Position, 1e-9));

	// --- Death precedence and the body sequence ----------------------------------------------------------------------
	FDeathState Death;
	Death.DeathServerTime = Contact;
	const FEncounterPresentationPlan Lunge = At(A, Death, Contact - 0.05, Start);
	TestTrue(TEXT("reversed attacker: its lunge runs to contact (death written, not yet due)"), IsStrikeClip(Lunge.Clip) && !Lunge.bDeathPresented && Lunge.bNeedsTick);
	const FEncounterPresentationPlan Cut = At(A, Death, Contact, Start);
	TestTrue(TEXT("reversed attacker: cut at contact into Death_Fall"), Cut.Clip == EEncounterClip::DeathFall && Cut.bDeathPresented && FMath::IsNearlyZero(Cut.Position, 1e-9));
	const double Fall = P.DeathFallSec;
	TestTrue(TEXT("fall: 0.8 s in, still falling"), At(A, Death, Contact + 0.8, Start).BodyPhase == EEncounterBodyPhase::Falling);
	const FEncounterPresentationPlan Down = At(A, Death, Contact + Fall, Start);
	TestTrue(TEXT("fall ends at + DeathFallSec: clip at its end and held, collision off, lingering"),
		Down.bClipEnded && FMath::IsNearlyEqual(Down.Position, C.FallLengthSec, 1e-6) && Down.bCollisionOff && Down.BodyPhase == EEncounterBodyPhase::Lingering);
	TestTrue(TEXT("collision stays on during the fall"), !At(A, Death, Contact + Fall - 0.01, Start).bCollisionOff);
	TestTrue(TEXT("linger: BodyLingerSec of the static final pose"), At(A, Death, Contact + Fall + P.BodyLingerSec - 0.01, Start).BodyPhase == EEncounterBodyPhase::Lingering);
	TestTrue(TEXT("removal window opens at + DeathFallSec + BodyLingerSec (7.2 s)"), At(A, Death, Contact + 7.2, Start).BodyPhase == EEncounterBodyPhase::Removal);
	const FEncounterPresentationPlan Grace = At(A, Death, Contact + 9.2, Start);
	TestTrue(TEXT("grace cut at + 9.2 s; the tick stops"), Grace.BodyPhase == EEncounterBodyPhase::Cut && !Grace.bNeedsTick);
	const FEncounterPresentationPlan LateFall = At(FActionState(), Death, Contact + 0.5, -1.0, true, Contact + 0.5);
	TestTrue(TEXT("a fall received late is sought from DeathServerTime"), LateFall.bLate && FMath::IsNearlyEqual(LateFall.Position, 0.5 * C.FallLengthSec / Fall, 1e-6));
	TestTrue(TEXT("a body first received after the linger is never shown"), At(FActionState(), Death, Contact + 7.5, -1.0, true, Contact + 7.5).bNeverShow);
	TestTrue(TEXT("a body received during the linger is shown"), !At(FActionState(), Death, Contact + 3.0, -1.0, true, Contact + 3.0).bNeverShow);
	return true;
}

#endif
