// Objectives Chain story oc-013 AC2 (AC-OC-28 [U]): StealthGame.Objectives.ClueTooBroadLogged.
// GIVEN a fixture layout in which a live site's clue matches 4 courts with ClueMatchMaxCount 3 THEN the round proceeds, the
// stage card shows the same clue (never a sharper one), and exactly one ClueTooBroad entry is logged for the layout hash: a
// second preparation with the same hash logs nothing, a different hash logs a new entry, a match count of exactly 3 logs
// nothing. The entry carries the hash and the counts only (no court id, no role).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "ObjectivesTestRig.h"
#include "View/ObjectiveViewHelpers.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectivesClueTooBroadLoggedTest, "StealthGame.Objectives.ClueTooBroadLogged",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FObjectivesClueTooBroadLoggedTest::RunTest(const FString& Parameters)
{
	using namespace StealthObjectivesTest;
	FCoreRig Rig(/*bActive*/ false);
	// Arrange: team 0's P1 court, P2 court, the final and an ordinary court all "in the market" (4 matches); team 1's apart.
	Rig.Clues.Add(CourtP1Team0, EClueAreaType::Market);
	Rig.Clues.Add(CourtP2Team0, EClueAreaType::Market);
	Rig.Clues.Add(CourtFinal, EClueAreaType::Market);
	Rig.Clues.Add(CourtOrdinary, EClueAreaType::Market);
	Rig.Clues.Add(CourtP1Team1, EClueAreaType::Temple, EClueFeatureKind::Shrine);
	Rig.Clues.Add(CourtP2Team1, EClueAreaType::Causeway, EClueFeatureKind::Stairway);
	constexpr uint64 HashA = 0x1234567800000077ull;
	constexpr uint64 HashB = 0x1234567800000078ull;

	// Act 1: the first preparation with HashA.
	TestTrue(TEXT("the bind succeeds (the round proceeds)"), Rig.Core.Bind(Rig.Chain.Get(), Rig.Assignment, HashA).IsValid());
	TestTrue(TEXT("Active entry happens"), Rig.Core.HandleActiveEntry(RoundEpoch, ActivePhaseStart));
	const FObjectiveTeamView View = Rig.Core.BuildTeamView(0);
	FClueTag Market;
	Market.AreaType = static_cast<uint8>(EClueAreaType::Market);
	TestTrue(TEXT("team 0's card carries the same (too broad) clue, never a sharper one"), View.LiveSites.Num() == 1 && View.LiveSites[0].Clue == Market);
	TestEqual(TEXT("the card's clue text is the area-only phrase"), ObjectiveViewHelpers::BuildClueText(View.LiveSites[0].Clue).ToString(), FString(TEXT("A court in the market")));
	TArray<FRoundLogEntry> Logged = Rig.Round.OfType(ERoundLogType::ClueTooBroad);
	if (TestEqual(TEXT("exactly one ClueTooBroad for HashA"), Logged.Num(), 1))
	{
		const FRoundLogEntry& Entry = Logged[0];
		TestEqual(TEXT("the entry carries the hash (low)"), Entry.Data[0], static_cast<uint32>(HashA & 0xFFFFFFFFull));
		TestEqual(TEXT("the entry carries the hash (high)"), Entry.Data[1], static_cast<uint32>(HashA >> 32));
		TestEqual(TEXT("the entry carries the worst match count"), Entry.Data[2], 4u);
		TestEqual(TEXT("and the bound"), Entry.Data[3], 3u);
		TestTrue(TEXT("no participant and no court in the entry"), Entry.Participants[0] == FParticipantId::InvalidValue && Entry.Data[4] == 0 && Entry.Data[5] == 0);
	}

	// Act 2: a second preparation of the same layout hash logs nothing more.
	Rig.Core.Bind(Rig.Chain.Get(), Rig.Assignment, HashA);
	TestEqual(TEXT("the same hash logs nothing more"), Rig.Round.Count(ERoundLogType::ClueTooBroad), 1);

	// Act 3: a different hash logs a new entry.
	Rig.Core.Bind(Rig.Chain.Get(), Rig.Assignment, HashB);
	TestEqual(TEXT("a different hash logs a new entry"), Rig.Round.Count(ERoundLogType::ClueTooBroad), 2);

	// Act 4: exactly ClueMatchMaxCount (3) matches logs nothing.
	Rig.Clues.Records.RemoveAll([](const FCourtClueRecord& Record) { return Record.Court.Value == CourtOrdinary; });
	Rig.Core.Bind(Rig.Chain.Get(), Rig.Assignment, 0x99ull);
	TestEqual(TEXT("a match count of exactly 3 logs nothing"), Rig.Round.Count(ERoundLogType::ClueTooBroad), 2);
	TestEqual(TEXT("team 0's P1 clue now matches 3"), Rig.Core.GetBinding(DefP1Team0)->ClueMatches, 3);
	return true;
}

#endif
