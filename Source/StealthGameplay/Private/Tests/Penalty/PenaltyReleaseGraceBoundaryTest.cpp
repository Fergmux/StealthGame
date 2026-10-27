// Story nkp-002 (production/epics/npc-kill-penalty-informants/story-002-strike-rule-and-release-grace.md), AC2 / AC-NKP-07
// (PEN-R5): StrikeOutReleaseGraceSec 2.0 on a fake clock; a released body killed by a rival at +1.999, +2.000 and +2.001
// s after its release (three runs) gives WrongTarget and tally +1 every time; the strike is +0, +0, +1 (the boundary is
// inclusive; t0 = 100.0 makes +2.000 compare exactly). Edge cases: a body never released is never graced; the reset clears
// the grace; the StrikeCounted row carries Graced.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Tests/Penalty/PenaltyTestRig.h"

namespace PenaltyReleaseGraceBoundaryTestData
{
	/** The fixture grace (the GDD default, set explicitly so the boundary is the test's, not the schema's). */
	constexpr float GraceSec = 2.0f;

	struct FRun
	{
		double AfterReleaseSec;
		int32 ExpectedStrike;
		bool bExpectedGraced;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPenaltyReleaseGraceBoundaryTest, "StealthGame.Penalty.ReleaseGraceBoundary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPenaltyReleaseGraceBoundaryTest::RunTest(const FString& Parameters)
{
	using namespace StealthPenaltyTest;
	using namespace PenaltyReleaseGraceBoundaryTestData;

	const auto Configure = [](FFakeTuning& Tuning)
	{
		FStealthServerTuning Values;
		Values.Penalty_Server.StrikeOutReleaseGraceSec = GraceSec;
		Tuning.Live = FPenaltyParams::FromTuning(Values);
	};
	const FRun Runs[] = { { 1.999, 0, true }, { 2.000, 0, true }, { 2.001, 1, false } };
	for (const FRun& Run : Runs)
	{
		// Arrange: a body released at t0 = 100.0 (the rig clock's start); the rival Q kills it Run.AfterReleaseSec later.
		FRig Rig(Configure);
		const double ReleaseTime = Rig.Clock.GetServerTimeSeconds();
		const FPenaltyPawnKey Body = Pawn(800);
		Rig.Core->RecordReleasedBody(Body, ReleaseTime);
		Rig.Clock.AdvanceSeconds(Run.AfterReleaseSec);
		Rig.Slot.NextSlot();

		// Act
		const EPenaltyKillOutcome Outcome = Rig.Core->OnPawnKilled(FPenaltyKill{ Body, Q, EVictimKind::Npc, EKillCause::Strike });
		const FStrikeTally Tally = Rig.Core->GetTallyForResult(Q, EStrikeResult::WrongTarget, FRig::Seq(1));

		// Assert
		const FString Name = FString::Printf(TEXT("+%.3f s"), Run.AfterReleaseSec);
		TestEqual(FString::Printf(TEXT("%s: WrongTarget ticks the tally"), *Name), static_cast<int32>(Tally.TallyAfter), 1);
		TestEqual(FString::Printf(TEXT("%s: strike"), *Name), static_cast<int32>(Rig.Core->GetStrikeCount(Q)), Run.ExpectedStrike);
		TestTrue(FString::Printf(TEXT("%s: outcome"), *Name), Outcome == (Run.bExpectedGraced ? EPenaltyKillOutcome::Graced : EPenaltyKillOutcome::Strike));
		const TArray<FRoundLogEntry> Rows = Rig.Log.OfType(ERoundLogType::StrikeCounted);
		FPenaltyStrikeCountedRow Row;
		TestTrue(FString::Printf(TEXT("%s: one StrikeCounted row with Graced %d"), *Name, Run.bExpectedGraced ? 1 : 0),
			Rows.Num() == 1 && PenaltyEventRows::Decode(Rows[0], Row) && Row.bGraced == Run.bExpectedGraced && Row.Killer == Q);
	}

	// Edge: a body never released is never graced, even at the same time.
	{
		FRig Rig(Configure);
		Rig.Core->RecordReleasedBody(Pawn(800), Rig.Clock.GetServerTimeSeconds());
		TestFalse(TEXT("another body is not graced"), Rig.Core->IsGraced(Pawn(801), Rig.Clock.GetServerTimeSeconds()));
		TestTrue(TEXT("the released body is graced at its release"), Rig.Core->IsGraced(Pawn(800), Rig.Clock.GetServerTimeSeconds()));

		// Edge: the reset clears the grace.
		Rig.Core->OnRosterReset(FixtureEpoch + 1);
		TestFalse(TEXT("the reset clears the grace"), Rig.Core->IsGraced(Pawn(800), Rig.Clock.GetServerTimeSeconds()));
	}
	return true;
}

#endif
