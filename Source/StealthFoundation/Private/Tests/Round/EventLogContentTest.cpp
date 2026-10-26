// Story rmf-011 (production/epics/round-match-framework/story-011-event-log-content.md): GDD AC 32 (scoped).
// For each terminal outcome (ObjectiveWin, Elimination, ForfeitElimination, Void Abandoned, Void KickedLastActive,
// Void PawnLost, Aborted) the header holds every RoundLogHeaderFields field, the WinLatched entry holds the
// EndReason and the key, and the client/dump read API refuses every read before Ended (Reset for an abort or a
// void). The server-internal accessor Round uses for TeamSummary (rmf-054, MVP item 9 data) is a separate path
// (D48) that works in the latch step, before Ended. No entry content is ever printed.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Server/Testing/StealthRoundLogTestSupport.h"

namespace EventLogContentTestData
{
	struct FOutcomeCase
	{
		ERoundEndReason Reason;
		uint32 WinnerTeam;
		/** Wins publish Ended; aborts and voids go Active -> Reset. */
		uint32 TerminalPhase;
	};

	const FOutcomeCase Cases[] = {
		{ ERoundEndReason::ObjectiveWin, 0, RoundLogFixtures::PhaseEnded },
		{ ERoundEndReason::Elimination, 1, RoundLogFixtures::PhaseEnded },
		{ ERoundEndReason::ForfeitElimination, 0, RoundLogFixtures::PhaseEnded },
		{ ERoundEndReason::VoidAbandoned, GRoundLogNoTeam, RoundLogFixtures::PhaseReset },
		{ ERoundEndReason::VoidKickedLastActive, GRoundLogNoTeam, RoundLogFixtures::PhaseReset },
		{ ERoundEndReason::VoidPawnLost, GRoundLogNoTeam, RoundLogFixtures::PhaseReset },
		{ ERoundEndReason::Aborted, GRoundLogNoTeam, RoundLogFixtures::PhaseReset },
	};

	constexpr uint64 Tick = 900;
	constexpr double ActiveStart = 20.0;
	constexpr double LatchTime = 700.0;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoundEventLogContentTest, "StealthGame.Round.EventLogContent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRoundEventLogContentTest::RunTest(const FString& Parameters)
{
	using namespace EventLogContentTestData;
	using namespace RoundLogFixtures;

	// The header type names every fixture field (the fixture list lives in the constant file).
	const TConstArrayView<const TCHAR*> FieldNames = FRoundLogHeader::GetFieldNames();
	for (const TCHAR* Fixture : RoundTestFixtures::RoundLogHeaderFields)
	{
		TestTrue(FString::Printf(TEXT("header declares fixture field %s"), Fixture),
			FieldNames.ContainsByPredicate([Fixture](const TCHAR* Name) { return FCString::Strcmp(Name, Fixture) == 0; }));
	}
	TestEqual(TEXT("header field count equals the fixture's"), FieldNames.Num(), static_cast<int32>(UE_ARRAY_COUNT(RoundTestFixtures::RoundLogHeaderFields)));

	for (const FOutcomeCase& Case : Cases)
	{
		const FString Name = FString::Printf(TEXT("outcome %d"), static_cast<int32>(Case.Reason));
		FStealthRoundLogRig Rig;
		const FRoundLogHeader Fixture = MakeHeader();

		// Lobby, Preparing (header at P3), Active, each refused.
		uint32 Phase = PhaseLobby;
		const auto ExpectRefused = [&]()
		{
			FRoundLogHeader Header;
			TArray<FRoundLogEntry> Entries;
			TArray<uint32> Truncated;
			FStealthSpyLogFileSink Disk;
			TestTrue(FString::Printf(TEXT("%s, phase %u: header read refused"), *Name, Phase), Rig.Log.ReadHeader(Header) == ERoundLogReadResult::Refused_NotReleasable);
			TestTrue(FString::Printf(TEXT("%s, phase %u: entry read refused"), *Name, Phase), Rig.Log.ReadEntries(Entries) == ERoundLogReadResult::Refused_NotReleasable && Entries.Num() == 0);
			TestTrue(FString::Printf(TEXT("%s, phase %u: counter read refused"), *Name, Phase), Rig.Log.ReadTruncatedCounts(Truncated) == ERoundLogReadResult::Refused_NotReleasable && Truncated.Num() == 0);
			TestTrue(FString::Printf(TEXT("%s, phase %u: dump refused, nothing written"), *Name, Phase), !Rig.Log.WriteToDisk(Disk, TEXT("round.log")) && Disk.Files.Num() == 0);
		};
		ExpectRefused();
		Phase = PhasePreparing;
		Rig.Log.Append(MakePhaseChanged(PhaseLobby, PhasePreparing, FQueueKey(Tick, 0), 10.0));
		TestTrue(FString::Printf(TEXT("%s: header written once at P3"), *Name), Rig.Log.WriteHeader(Fixture));
		ExpectRefused();
		Phase = PhaseActive;
		Rig.Log.Append(MakePhaseChanged(PhasePreparing, PhaseActive, FQueueKey(Tick, 0), ActiveStart));
		// TeamSummary inputs: two stages and the final for team 0, an elimination and a wrong-target kill by team 1.
		FRoundLogEntry Stage = MakeEntry(ERoundLogType::ObjectiveStage, FQueueKey(Tick + 1, 1), 100.0);
		Stage.Data[0] = 0;
		Rig.Log.Append(Stage);
		Rig.Log.Append(Stage);
		FRoundLogEntry Final = MakeEntry(ERoundLogType::FinalCompleted, FQueueKey(Tick + 2, 2), 200.0);
		Final.Data[0] = 0;
		Rig.Log.Append(Final);
		FRoundLogEntry Eliminated = MakeEntry(ERoundLogType::StateTransition, FQueueKey(Tick + 3, 3), 300.0);
		Eliminated.Data[0] = static_cast<uint32>(EParticipantState::Active);
		Eliminated.Data[1] = static_cast<uint32>(EParticipantState::Informant);
		Eliminated.Data[2] = 1;
		Rig.Log.Append(Eliminated);
		FRoundLogEntry Wrong = MakeEntry(ERoundLogType::KillResolved, FQueueKey(Tick + 4, 4), 400.0);
		Wrong.Code = static_cast<uint16>(ERoundKillResult::WrongTarget);
		Wrong.Data[0] = 1;
		Rig.Log.Append(Wrong);
		ExpectRefused();

		// Latch step: WinLatched (also for a void and an abort), then Round's own server-internal read.
		const FQueueKey TerminalKey(Tick + 5, 5);
		Rig.Log.Append(MakeWinLatched(Case.Reason, Case.WinnerTeam, TerminalKey, LatchTime));
		const FRoundLogTeamTallies Tallies = Rig.Log.TallyTeamSummaryInputsServerInternal();
		TestTrue(FString::Printf(TEXT("%s: server-internal accessor reads the header before Ended"), *Name), Rig.Log.GetHeaderServerInternal() != nullptr);
		TestTrue(FString::Printf(TEXT("%s: TeamSummary inputs from the log (steps 3/0, eliminated 0/1, wrong target 0/1)"), *Name),
			Tallies.StepsCompleted[0] == 3 && Tallies.StepsCompleted[1] == 0 && Tallies.EliminatedCount[0] == 0 && Tallies.EliminatedCount[1] == 1
			&& Tallies.WrongTargetCount[0] == 0 && Tallies.WrongTargetCount[1] == 1);
		ExpectRefused();  // the latch step itself releases nothing

		// Ended (wins) or Reset (abort, void): the client/dump read API now answers.
		Phase = Case.TerminalPhase;
		Rig.Log.Append(MakePhaseChanged(PhaseActive, Case.TerminalPhase, TerminalKey, LatchTime));
		Rig.Gate.bReleasable = true;
		FRoundLogHeader Header;
		TArray<FRoundLogEntry> Entries;
		TestTrue(FString::Printf(TEXT("%s: header readable at the terminal phase"), *Name), Rig.Log.ReadHeader(Header) == ERoundLogReadResult::Ok);
		TestTrue(FString::Printf(TEXT("%s: the header holds every fixture field value"), *Name), Header == Fixture);
		TestTrue(FString::Printf(TEXT("%s: entries readable"), *Name), Rig.Log.ReadEntries(Entries) == ERoundLogReadResult::Ok);
		const FRoundLogEntry* Latched = Entries.FindByPredicate([](const FRoundLogEntry& E) { return E.Type == ERoundLogType::WinLatched; });
		if (TestNotNull(*FString::Printf(TEXT("%s: one WinLatched entry"), *Name), Latched))
		{
			TestTrue(FString::Printf(TEXT("%s: WinLatched holds the EndReason"), *Name), Latched->Code == static_cast<uint16>(Case.Reason));
			TestTrue(FString::Printf(TEXT("%s: WinLatched holds the key"), *Name), Latched->Key == TerminalKey);
			TestTrue(FString::Printf(TEXT("%s: WinLatched holds the winner (none for a void or abort)"), *Name), Latched->Data[0] == Case.WinnerTeam);
		}
		TestEqual(FString::Printf(TEXT("%s: exactly one WinLatched"), *Name), Entries.FilterByPredicate([](const FRoundLogEntry& E) { return E.Type == ERoundLogType::WinLatched; }).Num(), 1);
		TestFalse(FString::Printf(TEXT("%s: a second header write is refused"), *Name), Rig.Log.WriteHeader(Fixture));

		const FStealthFaultTeardownResult Teardown = Rig.Faults.EvaluateTeardown({ { EStealthFault::RoundLogMisuse } });
		TestTrue(Teardown.Describe(), Teardown.IsClean());
	}

	// Release classes: the kill timeline and win record are PostRoundEligible; mailbox and connection facts never.
	TestTrue(TEXT("WinLatched is PostRoundEligible"), FRoundEventLog::GetReleaseClass(ERoundLogType::WinLatched) == ERoundLogReleaseClass::PostRoundEligible);
	TestTrue(TEXT("MailboxOverflow is NeverReleased"), FRoundEventLog::GetReleaseClass(ERoundLogType::MailboxOverflow) == ERoundLogReleaseClass::NeverReleased);
	TestTrue(TEXT("Dropped(StaleConnection) is NeverReleased"), FRoundEventLog::GetReleaseClass(ERoundLogType::Dropped_StaleConnection) == ERoundLogReleaseClass::NeverReleased);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
