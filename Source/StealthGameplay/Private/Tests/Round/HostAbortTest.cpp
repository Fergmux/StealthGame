// Story rmf-057 (production/epics/round-match-framework/story-057-host-abort.md), GDD AC 11 / R8, R22:
// with bHostMayAbortActive true, the host's abort in Active goes Active -> Reset: EndReason Aborted, bCalledOff true,
// MatchScore unchanged, the host's id in the log, nothing released (no ReleaseToCrowd; the slice archived, never
// released), RoundsPlayedCount keeps its Active-entry increment. With the flag false: WrongPhase, the phase stays Active
// and nothing changes. The flag is injected through the rule params (the TDC key's stand-in), never read from a file.
// Decision (story Open items): the Active abort runs on the host entry point (no queue Seq), and the refused abort writes
// no log entry (the AC names the host's id only for the accepted abort).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "StealthRoundEndRig.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoundHostAbortTest, "StealthGame.Round.HostAbort",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRoundHostAbortTest::RunTest(const FString& Parameters)
{
	for (const bool bFlag : { true, false })
	{
		// Arrange.
		FRoundRuleParams Params = FRoundEndRig::DefaultParams();
		Params.bHostMayAbortActive = bFlag;
		FRoundEndRig R(Params);
		R.ToActive(2, 2);
		const uint32 Played = R.Rig.Core->GetPhaseCore().GetRoundsPlayedCount();
		const FRoundPublicState Before = R.Rig.Core->GetPhaseCore().MakePublicState();
		const int32 Entries = R.Rig.EventLog.Entries.Num();
		const int32 Mark = R.Rig.Mark();
		if (bFlag)
		{
			R.ScriptReset();
		}

		// Act.
		const ERoundCommandResult Result = R.Rig.Core->ExecuteHostCommand(FRoundExitBRig::HostCaller(), ERoundHostCommand::Abort);

		// Assert.
		const FRoundPublicState After = R.Rig.Core->GetPhaseCore().MakePublicState();
		if (bFlag)
		{
			TestTrue(TEXT("flag: executed, Active -> Reset (no Ended)"), Result == ERoundCommandResult::Executed && After.Phase == ERoundPhase::Reset);
			TestTrue(TEXT("flag: bCalledOff"), After.bCalledOff);
			TestTrue(TEXT("flag: MatchScore unchanged, no result"), After.MatchScore[0] == Before.MatchScore[0] && After.MatchScore[1] == Before.MatchScore[1]
				&& After.LastResult == Before.LastResult);
			TestEqual(TEXT("flag: RoundsPlayedCount keeps its Active-entry increment"), R.Rig.Core->GetPhaseCore().GetRoundsPlayedCount(), Played);
			TestEqual(TEXT("flag: the round was played (count 1)"), Played, 1u);
			const FRoundLogEntry* Abort = R.Rig.EventLog.Entries.FindByPredicate([](const FRoundLogEntry& E)
			{
				return E.Type == ERoundLogType::WinLatched && E.Code == static_cast<uint16>(ERoundEndReason::Aborted);
			});
			TestTrue(TEXT("flag: EndReason Aborted logged with the host's id"), Abort != nullptr && Abort->Participants[0] == FRoundExitBRig::HostIdValue);
			TestEqual(TEXT("flag: nothing released to the crowd"), R.Rig.CountSince(Mark, TEXT("ReleaseToCrowd")), 0);
			const TArray<FRoundArchivedSlice>& Archive = R.Rig.Core->GetLogArchive().GetArchive();
			TestTrue(TEXT("flag: the slice archived as Aborted, never released (no Ended)"), Archive.Num() == 1 && Archive[0].EndReason == ERoundEndReason::Aborted
				&& !Archive[0].bReachedEnded);
			TestTrue(TEXT("flag: viewers never became PostRound"), R.Rig.Core->GetViewerClass(FParticipantId(1)) == ERoundViewerClass::InRound);
		}
		else
		{
			TestTrue(TEXT("no flag: WrongPhase"), Result == ERoundCommandResult::WrongPhase);
			TestTrue(TEXT("no flag: the phase stays Active, nothing changed"), After == Before);
			TestEqual(TEXT("no flag: no log entry"), R.Rig.EventLog.Entries.Num(), Entries);
			TestEqual(TEXT("no flag: no mutating call"), R.Rig.CallsSince(Mark).Num(), 0);
		}
		TestTrue(TEXT("no unexpected call"), R.Rig.Log.Unexpected.IsEmpty());
	}
	return true;
}

#endif
