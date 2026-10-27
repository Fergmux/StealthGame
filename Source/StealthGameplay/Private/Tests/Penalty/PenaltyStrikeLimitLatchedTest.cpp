// Story nkp-005 (production/epics/npc-kill-penalty-informants/story-005-exile-sequence-in-attack-slot.md), AC4 /
// AC-NKP-40: GIVEN L = 3 latched at Preparing and P at k = 2, WHEN NpcKillStrikeLimit is changed to 4 during Active and
// P's third ungraced NPC kill resolves, THEN the exile fires on that kill (the latched L, never the live knob: zero live
// reads mid-round); AND after the next round's Preparing latches 4 the third kill does not exile and the fourth does
// (PEN-R4, TDC rule CR-11 Latching).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Tests/Penalty/PenaltyTestRig.h"

namespace PenaltyStrikeLimitLatchedTestData
{
	/** The round's latched L and the value the host switches to mid-round (the AC's numbers). */
	inline constexpr int32 LatchedL = 3;
	inline constexpr int32 LiveL = 4;
	inline constexpr uint64 BodyOfP = 70;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPenaltyStrikeLimitLatchedTest, "StealthGame.Penalty.StrikeLimitLatched",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPenaltyStrikeLimitLatchedTest::RunTest(const FString& Parameters)
{
	using namespace StealthPenaltyTest;
	using namespace PenaltyStrikeLimitLatchedTestData;

	// Arrange: L = 3 latched at Preparing; P at k = 2.
	FRig Rig([](FFakeTuning& Tuning) { Tuning.Live.NpcKillStrikeLimit = LatchedL; }, /*bWithGateways*/ true);
	Rig.Core->OnRoundPreparing();
	Rig.NpcKillWithBody(P, Pawn(BodyOfP), 900);
	Rig.NpcKillWithBody(P, Pawn(BodyOfP), 901);
	const int32 ReadsBeforeChange = Rig.Tuning.Reads;

	// Act: the host changes the live value during Active, then P's third ungraced NPC kill.
	Rig.Tuning.Live.NpcKillStrikeLimit = LiveL;
	EPenaltyKillOutcome Outcome = EPenaltyKillOutcome::NoStrike;
	const FStrikeTally Third = Rig.NpcKillWithBody(P, Pawn(BodyOfP), 902, &Outcome);

	// Assert: the latched L decided.
	TestTrue(TEXT("the third kill exiles under the latched L = 3"), Outcome == EPenaltyKillOutcome::StrikeAndExile && Third.bExiled);
	TestEqual(TEXT("the exile ran (one StrikeLimit transition)"), Rig.Bodies.Count(TEXT("RequestTransition")), 1);
	TestEqual(TEXT("zero reads of the live knob mid-round"), Rig.Tuning.Reads, ReadsBeforeChange);
	TestEqual(TEXT("the latched L is still 3"), Rig.Core->GetLatchedStrikeLimit(), LatchedL);

	// Next round: the reset, then Preparing latches 4.
	Rig.Roster.States.Add(P, EParticipantState::Active);
	Rig.Core->OnRosterReset(FixtureEpoch + 1);
	Rig.Roster.Epoch = FixtureEpoch + 1;
	Rig.Core->OnRoundPreparing();
	TestEqual(TEXT("the next Preparing latches 4"), Rig.Core->GetLatchedStrikeLimit(), LiveL);
	Rig.Bodies.Calls.Reset();
	FStrikeTally Tally;
	for (int32 Kill = 1; Kill <= LiveL - 1; ++Kill)
	{
		Tally = Rig.NpcKillWithBody(P, Pawn(BodyOfP + 1), 910 + Kill, &Outcome);
		TestFalse(FString::Printf(TEXT("next round: kill %d does not exile"), Kill), Tally.bExiled || Outcome == EPenaltyKillOutcome::StrikeAndExile);
	}
	TestEqual(TEXT("next round: no exile call before the fourth kill"), Rig.Bodies.Calls.Num(), 0);
	Tally = Rig.NpcKillWithBody(P, Pawn(BodyOfP + 1), 920, &Outcome);
	TestTrue(TEXT("next round: the fourth kill exiles"), Outcome == EPenaltyKillOutcome::StrikeAndExile && Tally.bExiled);
	TestTrue(TEXT("next round: the exile carries the new epoch"), Rig.Bodies.Calls.Num() > 0 && Rig.Bodies.Calls[0].Epoch == FixtureEpoch + 1);
	TestEqual(TEXT("no fault"), Rig.Faults.GetRecords().Num(), 0);
	return true;
}

#endif
