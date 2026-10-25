// Story psr-002 (production/epics/participant-state-roster/story-002-begin-match-and-register.md):
// AC-PSR-05, BeginMatch and registration assign ids and handles and reject a full roster (R3, R4).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Tests/Roster/StealthRosterTestFixture.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRosterBeginMatchAndRegisterTest, "StealthGame.Roster.BeginMatchAndRegister",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRosterBeginMatchAndRegisterTest::RunTest(const FString& Parameters)
{
	using namespace RosterTestData;

	// Arrange
	FRosterTestFixture F;
	TestEqual(TEXT("BeginMatch(5) applies"), F.Core.BeginMatch(FullTeamSize), ERosterResult::Applied);
	TestEqual(TEXT("BeginMatch(0) is an invalid team size"), F.Core.BeginMatch(0), ERosterResult::Rejected_InvalidTeamSize);
	TestEqual(TEXT("BeginMatch(6) is an invalid team size"), F.Core.BeginMatch(6), ERosterResult::Rejected_InvalidTeamSize);
	const uint64 FirstMatchId = F.Core.GetMatchId();
	TestNotEqual(TEXT("a MatchId is drawn"), FirstMatchId, 0ull);

	// Act: K1..K10 register.
	TArray<uint32> Handles;
	for (int32 Index = 0; Index < 10; ++Index)
	{
		const FRosterRegisterResult R = F.Core.RegisterParticipant(Key(Index), TEXT("P"));
		TestEqual(FString::Printf(TEXT("K%d registers"), Index + 1), R.Result, ERosterResult::Registered);
		TestEqual(FString::Printf(TEXT("K%d gets id %d"), Index + 1, Index), R.Id.Value, static_cast<uint32>(Index));
		const FParticipantRecord Rec = F.Record(R.Id);
		TestTrue(FString::Printf(TEXT("record %d is RoundEnded"), Index), Rec.State == EParticipantState::RoundEnded);
		TestTrue(FString::Printf(TEXT("record %d has Team None"), Index), Rec.Team == ERosterTeam::None);
		TestTrue(FString::Printf(TEXT("record %d is Connected"), Index), Rec.Connection.Projection == ERosterConnection::Connected);
		Handles.Add(Rec.PublicHandle);
	}

	// K11 is rejected and consumes no EventSeq.
	const uint64 SeqBeforeFull = F.Core.GetEventSeq();
	const FRosterRegisterResult Full = F.Core.RegisterParticipant(Key(10), TEXT("P"));
	TestEqual(TEXT("K11 gets Rejected_RosterFull"), Full.Result, ERosterResult::Rejected_RosterFull);
	TestFalse(TEXT("K11 gets no id"), Full.Id.IsValid());
	TestEqual(TEXT("the rejection consumes no EventSeq"), F.Core.GetEventSeq(), SeqBeforeFull);

	// RemoveParticipant(3) then a new key: id 10, never 3.
	TestEqual(TEXT("RemoveParticipant(3) applies while unlocked"), F.Core.RemoveParticipant(FParticipantId(3)), ERosterResult::Applied);
	const FRosterRegisterResult AfterRemoval = F.Core.RegisterParticipant(Key(11), TEXT("P"));
	TestEqual(TEXT("a new key after the removal registers"), AfterRemoval.Result, ERosterResult::Registered);
	TestEqual(TEXT("the new id is 10, not the retired 3"), AfterRemoval.Id.Value, 10u);
	Handles.Add(F.Record(AfterRemoval.Id).PublicHandle);

	// Every PublicHandle unique and non-zero; with the seeded source the first ten are not in id order.
	TSet<uint32> Unique(Handles);
	TestEqual(TEXT("every PublicHandle is unique"), Unique.Num(), Handles.Num());
	TestFalse(TEXT("no PublicHandle is 0"), Unique.Contains(0u));
	bool bAscending = true;
	for (int32 Index = 1; Index < 10; ++Index)
	{
		bAscending &= Handles[Index - 1] < Handles[Index];
	}
	TestFalse(TEXT("handles are not in id order (seeded source)"), bAscending);

	// Second BeginMatch: counters reset, a different MatchId.
	const uint64 SeqBeforeSecond = F.Core.GetEventSeq();
	TestTrue(TEXT("EventSeq is non-zero before the second BeginMatch"), SeqBeforeSecond > 0);
	TestEqual(TEXT("second BeginMatch applies"), F.Core.BeginMatch(FullTeamSize), ERosterResult::Applied);
	TestEqual(TEXT("EventSeq reset to 0"), F.Core.GetEventSeq(), 0ull);
	TestEqual(TEXT("RoundEpoch reset to 0"), F.Core.GetRoundEpoch(), 0u);
	TestNotEqual(TEXT("a different MatchId is drawn"), F.Core.GetMatchId(), FirstMatchId);
	TestTrue(TEXT("no record survives BeginMatch"), F.Core.FindRecord(FParticipantId(0)) == nullptr);
	TestEqual(TEXT("ParticipantId restarts at 0"), F.Register(0).Value, 0u);

	TestTrue(F.Sink.EvaluateTeardown({}).Describe(), F.Sink.EvaluateTeardown({}).IsClean());
	return true;
}

#endif
