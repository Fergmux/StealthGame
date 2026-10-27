// Story asn-024 AC4 (ADR-0018 section 5.3 and Verification Required (3); HUD F2 / F3; UI-ASN-3), headless:
// StealthGame.Assassination.OwnResultMatcher. The plain C++ FOwnResultMatcher fed by hand with a fake server clock:
//  - the attacker's plate shows at FStrikeResult.ContactServerTime (never an inferred write time), not before;
//  - the defender's FDefenceResult and its Counter write arriving in either order: t_show = the Counter's contact;
//  - a dropped write: the result shows at t_arrival + HudResultPlateMaxHoldBackSec, and that cap never goes below 1.25 s;
//  - a duplicate result and an older sequence are dropped by (PossessionEpoch, Seq);
//  - an RTT beyond 300 ms: a late result shows on arrival, never before its contact;
//  - the write the pawn showed at the press is never matched; a rejected press never takes a later write;
//  - a Reversed attacker plate waits for its own OnRoleChangePresented(ReversedAgainst) (or the cap), and a role change that
//    came first releases a late result on arrival;
//  - F3: one timeline for every plate (250 ms in four steps, 3.0 s hold, 600 ms out); the two "Wrong target" texts are one;
//  - UI-ASN-3: the brace square stays up through a Counter until its ContactServerTime.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "View/OwnResultMatcher.h"

namespace OwnResultMatcherTestData
{
	/** Opaque pawn handles (never dereferenced). */
	const int32 AttackerTag = 1;
	const int32 DefenderTag = 2;
	const void* const Attacker = &AttackerTag;
	const void* const Defender = &DefenderTag;

	/** The strike lead used throughout: contact = slot + 0.4 s (StrikeContactSec), the result arriving 75 ms after the slot. */
	constexpr double ContactAfterSlotSec = 0.4;
	constexpr double ArrivalAfterSlotSec = 0.075;
	/** One 60 Hz client frame. */
	constexpr double FrameSec = 1.0 / 60.0;
	/** An RTT beyond 300 ms: the result is processed after its contact. */
	constexpr double LateBySec = 0.3;

	FClientCommandSeq Seq(uint32 Value, uint16 Epoch = 1)
	{
		FClientCommandSeq Out;
		Out.PossessionEpoch = Epoch;
		Out.Seq = Value;
		return Out;
	}

	FActionState State(EActionPose Pose, uint16 InSeq, double Contact)
	{
		FActionState Out;
		Out.Pose = Pose;
		Out.Seq = InSeq;
		Out.ContactServerTime = Contact;
		return Out;
	}

	FStrikeResult Strike(uint32 InSeq, EStrikeResult Result, double Contact, uint8 Tally = 0)
	{
		FStrikeResult Out;
		Out.ClientCommandSeq = Seq(InSeq);
		Out.Result = Result;
		Out.ContactServerTime = Contact;
		Out.TallyAfter = Tally;
		return Out;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAssassinationOwnResultMatcherTest, "StealthGame.Assassination.OwnResultMatcher",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FAssassinationOwnResultMatcherTest::RunTest(const FString& Parameters)
{
	using namespace OwnResultMatcherTestData;
	const FOwnResultMatcherParams Params; // HudResultPlateMaxHoldBackSec 1.25 (the registry value)
	const double Cap = Params.HoldBackSec();
	TestTrue(TEXT("the cap is at least the 1.25 s hard minimum"), Cap >= FOwnResultMatcherParams::HardMinHoldBackSec);
	{
		FOwnResultMatcherParams Low;
		Low.MaxHoldBackSec = 1.0;
		TestEqual(TEXT("a configured cap below 1.25 s is raised to 1.25 s"), Low.HoldBackSec(), FOwnResultMatcherParams::HardMinHoldBackSec);
	}

	// 1 The attacker: FStrikeResult.ContactServerTime first, even when a write with another time was observed.
	{
		FOwnResultMatcher M(Params);
		const double Slot = 10.0;
		const double Contact = Slot + ContactAfterSlotSec;
		M.OnCommandSent(ECommandKind::Attack, Seq(1), Attacker, State(EActionPose::None, 5, -1.0));
		M.OnActionStateObserved(Attacker, State(EActionPose::Strike, 6, Contact - 0.05), Slot + ArrivalAfterSlotSec);
		TestTrue(TEXT("1 accepted"), M.OnStrikeResult(Strike(1, EStrikeResult::WrongTarget, Contact, 1), Slot + ArrivalAfterSlotSec, nullptr));
		TestEqual(TEXT("1 nothing on receipt"), M.PopDue(Slot + ArrivalAfterSlotSec).Num(), 0);
		TestEqual(TEXT("1 nothing one frame before contact"), M.PopDue(Contact - FrameSec).Num(), 0);
		const TArray<FOwnResultDue> Due = M.PopDue(Contact);
		if (TestEqual(TEXT("1 one plate at the contact frame"), Due.Num(), 1))
		{
			TestEqual(TEXT("1 t_show = FStrikeResult.ContactServerTime"), Due[0].ShowServerTime, Contact);
			TestTrue(TEXT("1 time from the result, never inferred"), Due[0].Source == EOwnResultTimeSource::ResultContact);
			TestTrue(TEXT("1 Wrong target plate with its tally"), Due[0].Plate == EOwnResultPlate::WrongTarget && Due[0].Strike.TallyAfter == 1);
			TestTrue(TEXT("1 the commanded pawn handle"), Due[0].PawnKey == Attacker && !Due[0].bLate);
		}
	}

	// 2 The defender, both arrival orders: t_show = the Counter write's ContactServerTime on the handle of the Prepare(Begin).
	for (int32 Order = 0; Order < 2; ++Order)
	{
		FOwnResultMatcher M(Params);
		const double Slot = 20.0;
		const double Contact = Slot + ContactAfterSlotSec;
		const double Arrival = Slot + ArrivalAfterSlotSec;
		M.OnCommandSent(ECommandKind::PrepareBegin, Seq(2), Defender, State(EActionPose::None, 3, -1.0));
		M.OnActionStateObserved(Defender, State(EActionPose::Brace, 4, -1.0), Slot - 1.0);
		FDefenceResult Defence;
		if (Order == 0)
		{
			TestTrue(TEXT("2a result first: accepted"), M.OnDefenceResult(Seq(2), Defence, Arrival, nullptr));
			TestEqual(TEXT("2a Pending: nothing before the write"), M.PopDue(Arrival + 0.1).Num(), 0);
			M.OnActionStateObserved(Defender, State(EActionPose::Counter, 5, Contact), Slot + 0.25);
		}
		else
		{
			M.OnActionStateObserved(Defender, State(EActionPose::Counter, 5, Contact), Slot + 0.02);
			TestTrue(TEXT("2b write first: accepted"), M.OnDefenceResult(Seq(2), Defence, Arrival, nullptr));
		}
		TestEqual(*FString::Printf(TEXT("2%c nothing one frame before contact"), Order == 0 ? 'a' : 'b'), M.PopDue(Contact - FrameSec).Num(), 0);
		const TArray<FOwnResultDue> Due = M.PopDue(Contact);
		if (TestEqual(*FString::Printf(TEXT("2%c one plate at the Counter's contact"), Order == 0 ? 'a' : 'b'), Due.Num(), 1))
		{
			TestEqual(TEXT("2 t_show = the Counter's ContactServerTime"), Due[0].ShowServerTime, Contact);
			TestTrue(TEXT("2 matched on the write"), Due[0].Source == EOwnResultTimeSource::MatchedActionState && Due[0].Plate == EOwnResultPlate::StrikeReversed);
		}
	}

	// 3 A dropped write: shown at t_arrival + cap; an attacker result without a time behaves alike.
	{
		FOwnResultMatcher M(Params);
		const double Arrival = 30.075;
		M.OnCommandSent(ECommandKind::PrepareBegin, Seq(3), Defender, State(EActionPose::None, 3, -1.0));
		M.OnDefenceResult(Seq(3), FDefenceResult(), Arrival, nullptr);
		M.OnStrikeResult(Strike(4, EStrikeResult::NoContact, -1.0), Arrival, Attacker);
		TestEqual(TEXT("3 nothing before the cap"), M.PopDue(Arrival + Cap - 0.01).Num(), 0);
		const TArray<FOwnResultDue> Due = M.PopDue(Arrival + Cap);
		TestEqual(TEXT("3 both shown at the cap"), Due.Num(), 2);
		for (const FOwnResultDue& D : Due)
		{
			TestTrue(TEXT("3 lost-data cap at t_arrival + HudResultPlateMaxHoldBackSec"), D.Source == EOwnResultTimeSource::LostDataCap
				&& FMath::IsNearlyEqual(D.ShowServerTime, Arrival + Cap) && D.ContactServerTime < 0.0);
		}
	}

	// 4 Duplicates and older sequences.
	{
		FOwnResultMatcher M(Params);
		TestTrue(TEXT("4 first accepted"), M.OnStrikeResult(Strike(8, EStrikeResult::NoContact, 40.4), 40.075, Attacker));
		TestFalse(TEXT("4 the duplicate is dropped"), M.OnStrikeResult(Strike(8, EStrikeResult::NoContact, 40.4), 40.08, Attacker));
		TestFalse(TEXT("4 an older sequence is dropped"), M.OnStrikeResult(Strike(7, EStrikeResult::NoContact, 39.0), 40.09, Attacker));
		FStrikeResult OtherEpoch = Strike(8, EStrikeResult::NoContact, 41.4);
		OtherEpoch.ClientCommandSeq.PossessionEpoch = 2;
		TestTrue(TEXT("4 the same Seq in a later possession epoch is a new result"), M.OnStrikeResult(OtherEpoch, 41.075, Attacker));
		TestEqual(TEXT("4 two dropped"), M.GetDroppedCount(), 2);
		TestEqual(TEXT("4 two plates in all"), M.PopDue(50.0).Num(), 2);
	}

	// 5 An RTT beyond 300 ms: a late result shows on arrival, never before its contact (attacker and defender).
	{
		FOwnResultMatcher M(Params);
		const double Contact = 50.4;
		const double Arrival = Contact + LateBySec;
		M.OnStrikeResult(Strike(9, EStrikeResult::RivalEliminated, Contact), Arrival, Attacker);
		M.OnCommandSent(ECommandKind::PrepareBegin, Seq(10), Defender, State(EActionPose::None, 3, -1.0));
		M.OnActionStateObserved(Defender, State(EActionPose::Counter, 4, Contact), Arrival - 0.01);
		M.OnDefenceResult(Seq(10), FDefenceResult(), Arrival, nullptr);
		const TArray<FOwnResultDue> Due = M.PopDue(Arrival);
		TestEqual(TEXT("5 both shown on arrival"), Due.Num(), 2);
		for (const FOwnResultDue& D : Due)
		{
			TestTrue(TEXT("5 late: t_show = t_arrival >= contact"), D.bLate && FMath::IsNearlyEqual(D.ShowServerTime, Arrival) && D.ShowServerTime >= Contact);
		}
	}

	// 6 The write shown at the press is never matched; a rejected press never takes a later write.
	{
		FOwnResultMatcher M(Params);
		const FActionState OldCounter = State(EActionPose::Counter, 12, 60.4);
		M.OnActionStateObserved(Defender, OldCounter, 60.3);
		M.OnCommandSent(ECommandKind::PrepareBegin, Seq(11), Defender, OldCounter);
		M.OnDefenceResult(Seq(11), FDefenceResult(), 62.075, nullptr);
		M.OnActionStateObserved(Defender, OldCounter, 62.1);
		TestEqual(TEXT("6 the previous Counter is not this result's write"), M.PopDue(62.4).Num(), 0);
		M.OnCommandSent(ECommandKind::PrepareBegin, Seq(13), Defender, OldCounter);
		M.OnRejected(Seq(13));
		M.OnActionStateObserved(Defender, State(EActionPose::Counter, 14, 62.6), 62.3);
		const TArray<FOwnResultDue> Due = M.PopDue(62.6);
		if (TestEqual(TEXT("6 the new Counter goes to the pending result, not the rejected press"), Due.Num(), 1))
		{
			TestTrue(TEXT("6 Seq 11 at 62.6"), Due[0].Seq == Seq(11) && FMath::IsNearlyEqual(Due[0].ShowServerTime, 62.6));
		}
	}

	// 7 The reversed attacker: released inside its OnRoleChangePresented(ReversedAgainst), else by the cap.
	{
		FOwnResultMatcher M(Params);
		const double Contact = 70.4;
		const double Arrival = 70.075;
		M.OnStrikeResult(Strike(15, EStrikeResult::Reversed, Contact), Arrival, Attacker);
		TestEqual(TEXT("7 not at contact without the role change"), M.PopDue(Contact).Num(), 0);
		M.OnReversedAgainstPresented(Attacker, Contact);
		const TArray<FOwnResultDue> Due = M.PopDue(Contact);
		TestTrue(TEXT("7 Reversed inside the role-change sequence"), Due.Num() == 1 && Due[0].Plate == EOwnResultPlate::Reversed && Due[0].bInRoleChange
			&& FMath::IsNearlyEqual(Due[0].ShowServerTime, Contact));

		M.OnStrikeResult(Strike(16, EStrikeResult::Reversed, 80.4), 80.075, Attacker);
		TestEqual(TEXT("7 no death state: nothing before the cap"), M.PopDue(80.075 + Cap - 0.01).Num(), 0);
		TestEqual(TEXT("7 no death state: shown at the cap"), M.PopDue(80.075 + Cap).Num(), 1);

		M.OnReversedAgainstPresented(Attacker, 90.4);
		M.OnStrikeResult(Strike(17, EStrikeResult::Reversed, 90.4), 90.4 + LateBySec, Attacker);
		const TArray<FOwnResultDue> Late = M.PopDue(90.4 + LateBySec);
		TestTrue(TEXT("7 the role change came first: the late result shows on arrival"), Late.Num() == 1 && Late[0].bLate && Late[0].bInRoleChange);
	}

	// 8 F3: one timeline for every plate.
	{
		const FOwnResultPlateTimelineParams T;
		TestTrue(TEXT("8 total 3.85 s"), FMath::IsNearlyEqual(T.TotalSec(), 3.85, 1e-9));
		TestTrue(TEXT("8 pending before the show"), FOwnResultPlateTimeline::Sample(T, -FrameSec).Phase == EOwnResultPlatePhase::Pending);
		const FOwnResultPlateSample First = FOwnResultPlateTimeline::Sample(T, 0.0);
		const FOwnResultPlateSample Second = FOwnResultPlateTimeline::Sample(T, 0.07);
		TestTrue(TEXT("8 stepped entrance: 1/4 then 2/4"), First.Phase == EOwnResultPlatePhase::Entering && FMath::IsNearlyEqual(First.Opacity, 0.25f)
			&& FMath::IsNearlyEqual(Second.Opacity, 0.5f));
		TestTrue(TEXT("8 holding from 250 ms"), FOwnResultPlateTimeline::Sample(T, 0.25).Phase == EOwnResultPlatePhase::Holding);
		TestTrue(TEXT("8 exiting from 3.25 s"), FOwnResultPlateTimeline::Sample(T, 3.30).Phase == EOwnResultPlatePhase::Exiting);
		TestTrue(TEXT("8 gone after 3.85 s"), FOwnResultPlateTimeline::Sample(T, 3.85 + 0.001).Phase == EOwnResultPlatePhase::Gone);
		TestEqual(TEXT("8 Wrong target: one string for an NPC and an informant"), OwnResultPlateText(OwnResultPlateOf(EStrikeResult::WrongTarget)).ToString(), FString(TEXT("Wrong target")));
		const TCHAR* const Expected[GOwnResultPlateCount] = { TEXT("Target eliminated"), TEXT("Wrong target"), TEXT("No contact"), TEXT("Reversed"), TEXT("Strike reversed") };
		for (int32 Index = 0; Index < GOwnResultPlateCount; ++Index)
		{
			TestEqual(*FString::Printf(TEXT("8 text of %s"), LexOwnResultPlate(static_cast<EOwnResultPlate>(Index))), OwnResultPlateText(static_cast<EOwnResultPlate>(Index)).ToString(), FString(Expected[Index]));
		}
	}

	// 9 UI-ASN-3: the brace square ends at a reversal's contact.
	{
		const double Contact = 100.4;
		TestTrue(TEXT("9 shown while braced"), FOwnBraceIndicator::IsShown(State(EActionPose::Brace, 2, -1.0), 99.0));
		TestTrue(TEXT("9 still shown at the Counter's arrival before contact"), FOwnBraceIndicator::IsShown(State(EActionPose::Counter, 3, Contact), Contact - 0.075));
		TestTrue(TEXT("9 still shown one frame before contact"), FOwnBraceIndicator::IsShown(State(EActionPose::Counter, 3, Contact), Contact - FrameSec));
		TestFalse(TEXT("9 removed at contact"), FOwnBraceIndicator::IsShown(State(EActionPose::Counter, 3, Contact), Contact));
		TestFalse(TEXT("9 not shown with no brace"), FOwnBraceIndicator::IsShown(State(EActionPose::None, 4, -1.0), 99.0));
	}
	return true;
}

#endif
