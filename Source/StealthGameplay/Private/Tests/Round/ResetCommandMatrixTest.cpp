// Story rmf-027 (production/epics/round-match-framework/story-027-reset-command-matrix.md), GDD AC 59 (+ the dumplog
// half of AC 60) / R22, R8-R10:
//   - stealth.round.reset: no-op in Lobby and Reset; aborts in Preparing; in Active only with bHostMayAbortActive
//     (else WrongPhase); in Ended held until EndedMinDwellSec after the round end, then executed (fake clock
//     timeline); abort behaves as its alias.
//   - swapteam, remove and preset return WrongPhase outside Lobby.
//   - [STATIC] the stealth.round.* names are registered only in the game-instance store's registration code.
//   - debugwin / debugeliminate go through the real resolution queue: each consumes a Seq, its log entry carries the
//     Debug flag, and F7 excludes it.
//   - dumplog is refused (WrongPhase: the GDD names no dedicated code, open point) in Lobby, Preparing and Active,
//     with no output, and allowed in Ended and Reset.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Server/Core/Round/StealthRoundMatchStore.h"
#include "Server/EventLog/StealthRoundPacingMetrics.h"
#include "StealthRoundExitBTestRig.h"

namespace ResetCommandMatrixTestData
{
	/** Source files allowed to register a stealth.round.* console command (the game-instance store). */
	const TCHAR* const AllowedRegistrationFile = TEXT("Source/StealthGameplay/Private/Core/Round/StealthRoundMatchStore.cpp");
	/** Files that must never hold a host command (R22: no command on a PlayerController, pawn, HUD or cheat manager). */
	const TCHAR* const ForbiddenOwnerMarkers[] = { TEXT("PlayerController"), TEXT("Pawn"), TEXT("HUD"), TEXT("CheatManager"), TEXT("Character") };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoundResetCommandMatrixTest, "StealthGame.Round.ResetCommandMatrix",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRoundResetCommandMatrixTest::RunTest(const FString& Parameters)
{
	using namespace RoundTestFixtures;
	using namespace ResetCommandMatrixTestData;
	const FRoundCommandCaller Host = FRoundExitBRig::HostCaller();

	auto MakeRig = [](bool bHostMayAbortActive) -> TUniquePtr<FRoundExitBRig>
	{
		FRoundRuleParams Params = FRoundExitBRig::DefaultParams();
		Params.bHostMayAbortActive = bHostMayAbortActive;
		TUniquePtr<FRoundExitBRig> Rig = MakeUnique<FRoundExitBRig>(Params);
		Rig->OpenLobby();
		Rig->SetLayout(Layout2v2.Team0, Layout2v2.Team1);
		return Rig;
	};

	// --- reset / abort per phase --------------------------------------------------------------------------------
	for (const ERoundHostCommand Command : { ERoundHostCommand::Reset, ERoundHostCommand::Abort })
	{
		const TCHAR* Name = Command == ERoundHostCommand::Reset ? TEXT("reset") : TEXT("abort");
		{
			TUniquePtr<FRoundExitBRig> Rig = MakeRig(false);
			const int32 Mark = Rig->Mark();
			TestTrue(FString::Printf(TEXT("%s in Lobby is a NoOp"), Name), Rig->Core->ExecuteHostCommand(Host, Command) == ERoundCommandResult::NoOp);
			TestEqual(FString::Printf(TEXT("%s in Lobby changes nothing"), Name), Rig->CallsSince(Mark, false).Num(), 0);
		}
		{
			TUniquePtr<FRoundExitBRig> Rig = MakeRig(false);
			Rig->DriveTo(ERoundPhase::Preparing);
			Rig->Fakes.Session.Script(TEXT("SetAdmissionMode"));
			TestTrue(FString::Printf(TEXT("%s in Preparing aborts"), Name), Rig->Core->ExecuteHostCommand(Host, Command) == ERoundCommandResult::Executed);
			TestTrue(FString::Printf(TEXT("%s in Preparing requests Reset"), Name), Rig->Core->GetPhase() == ERoundPhase::Reset);
			TestTrue(FString::Printf(TEXT("%s in Preparing calls the round off"), Name), Rig->Core->GetPhaseCore().WasLastRoundCalledOff());
		}
		for (const bool bFlag : { false, true })
		{
			TUniquePtr<FRoundExitBRig> Rig = MakeRig(bFlag);
			Rig->DriveTo(ERoundPhase::Active);
			Rig->Fakes.Session.Script(TEXT("SetAdmissionMode"));
			const ERoundCommandResult Result = Rig->Core->ExecuteHostCommand(Host, Command);
			if (bFlag)
			{
				TestTrue(FString::Printf(TEXT("%s in Active with bHostMayAbortActive aborts"), Name), Result == ERoundCommandResult::Executed && Rig->Core->GetPhase() == ERoundPhase::Reset);
			}
			else
			{
				TestTrue(FString::Printf(TEXT("%s in Active without the flag is WrongPhase"), Name), Result == ERoundCommandResult::WrongPhase && Rig->Core->GetPhase() == ERoundPhase::Active);
			}
		}
		{
			// Ended: held until the dwell, then executed (fake clock timeline).
			TUniquePtr<FRoundExitBRig> Rig = MakeRig(false);
			Rig->DriveTo(ERoundPhase::Ended);
			Rig->Clock.AdvanceSeconds(EndedMinDwellSec * 0.5);
			TestTrue(FString::Printf(TEXT("%s in Ended before the dwell is Held"), Name), Rig->Core->ExecuteHostCommand(Host, Command) == ERoundCommandResult::Held);
			Rig->Core->Tick();
			TestTrue(FString::Printf(TEXT("%s held: still Ended before the dwell"), Name), Rig->Core->GetPhase() == ERoundPhase::Ended);
			Rig->Clock.AdvanceSeconds(EndedMinDwellSec * 0.5 + 0.01);
			Rig->Fakes.Session.Script(TEXT("SetAdmissionMode"));
			Rig->Core->Tick();
			TestTrue(FString::Printf(TEXT("%s held: executed at the dwell's end (Reset)"), Name), Rig->Core->GetPhase() == ERoundPhase::Reset);
			// Reset: a no-op.
			const int32 Mark = Rig->Mark();
			TestTrue(FString::Printf(TEXT("%s in Reset is a NoOp"), Name), Rig->Core->ExecuteHostCommand(Host, Command) == ERoundCommandResult::NoOp);
			TestEqual(FString::Printf(TEXT("%s in Reset changes nothing"), Name), Rig->CallsSince(Mark, false).Num(), 0);
			TestTrue(FString::Printf(TEXT("%s: no unexpected call"), Name), Rig->Log.Unexpected.IsEmpty());
		}
	}

	// --- Lobby-only commands outside Lobby; dumplog matrix ----------------------------------------------------------------
	const ERoundPhase Phases[] = { ERoundPhase::Lobby, ERoundPhase::Preparing, ERoundPhase::Active, ERoundPhase::Ended, ERoundPhase::Reset };
	for (const ERoundPhase Phase : Phases)
	{
		TUniquePtr<FRoundExitBRig> Rig = MakeRig(false);
		Rig->DriveTo(Phase);
		int32 Dumps = 0;
		Rig->Core->SetDumpLogHandler([&Dumps]() { ++Dumps; });
		const TCHAR* PhaseName = LexRoundPhase(Phase);
		if (Phase != ERoundPhase::Lobby)
		{
			FRoundCommandArgs Args;
			Args.Participant = FParticipantId(FRoundExitBRig::HostIdValue);
			Args.PresetTeamSize = PresetTeamSize;
			TestTrue(FString::Printf(TEXT("swapteam in %s is WrongPhase"), PhaseName), Rig->Core->ExecuteHostCommand(Host, ERoundHostCommand::SwapTeam, Args) == ERoundCommandResult::WrongPhase);
			TestTrue(FString::Printf(TEXT("remove in %s is WrongPhase"), PhaseName), Rig->Core->ExecuteHostCommand(Host, ERoundHostCommand::Remove, Args) == ERoundCommandResult::WrongPhase);
			TestTrue(FString::Printf(TEXT("preset in %s is WrongPhase"), PhaseName), Rig->Core->ExecuteHostCommand(Host, ERoundHostCommand::Preset, Args) == ERoundCommandResult::WrongPhase);
		}
		const bool bDumpAllowed = Phase == ERoundPhase::Ended || Phase == ERoundPhase::Reset;
		const ERoundCommandResult Dump = Rig->Core->ExecuteHostCommand(Host, ERoundHostCommand::DumpLog);
		TestTrue(FString::Printf(TEXT("dumplog in %s is %s"), PhaseName, bDumpAllowed ? TEXT("allowed") : TEXT("refused")),
			bDumpAllowed ? Dump == ERoundCommandResult::Executed : Dump == ERoundCommandResult::WrongPhase);
		TestEqual(FString::Printf(TEXT("dumplog in %s output count"), PhaseName), Dumps, bDumpAllowed ? 1 : 0);
		TestTrue(FString::Printf(TEXT("%s: no unexpected call"), PhaseName), Rig->Log.Unexpected.IsEmpty());
	}

	// --- NotHost: a non-host caller changes nothing ----------------------------------------------------------------------
	{
		TUniquePtr<FRoundExitBRig> Rig = MakeRig(true);
		FRoundCommandCaller Remote = Host;
		Remote.bLocalController = false;
		const int32 Mark = Rig->Mark();
		for (int32 Index = 0; Index < URoundMatchStore::CommandCount; ++Index)
		{
			TestTrue(FString::Printf(TEXT("%s from a non-local caller is NotHost"), URoundMatchStore::CommandNames[Index]),
				Rig->Core->ExecuteHostCommand(Remote, static_cast<ERoundHostCommand>(Index)) == ERoundCommandResult::NotHost);
		}
		TestEqual(TEXT("NotHost calls reach no gateway"), Rig->CallsSince(Mark, false).Num(), 0);
	}

	// --- Debug commands through the real queue -----------------------------------------------------------------------------
	{
		FStealthQueueTestRig QueueRig;
		FRoundDebugResolver Resolver(QueueRig.Log, QueueRig.Clock);
		int32 Effects = 0;
		Resolver.OnDebug = [&Effects](const FDebugPayload&, FQueueKey) { ++Effects; };
		QueueRig.Queue->RegisterResolver(EQueueItemType::DebugItem, &Resolver);
		QueueRig.Epoch.Current = FEpochToken(0, FixtureRoundEpoch);

		TUniquePtr<FRoundExitBRig> Rig = MakeRig(false);
		Rig->DriveTo(ERoundPhase::Active);
		Rig->Core->SetDebugQueue(QueueRig.Queue.Get());
		Rig->Fakes.Roster.Script(TEXT("GetEpochToken"), FixtureRoundEpoch, 2);
		FRoundCommandArgs Win;
		Win.Team = 1;
		FRoundCommandArgs Eliminate;
		Eliminate.Participant = FParticipantId(FRoundExitBRig::HostIdValue);
		TestTrue(TEXT("debugwin submitted"), Rig->Core->ExecuteHostCommand(Host, ERoundHostCommand::DebugWin, Win) == ERoundCommandResult::Executed);
		TestTrue(TEXT("debugeliminate submitted"), Rig->Core->ExecuteHostCommand(Host, ERoundHostCommand::DebugEliminate, Eliminate) == ERoundCommandResult::Executed);
		QueueRig.Tick();

		TArray<const FRoundLogEntry*> DebugEntries;
		for (const FRoundLogEntry& Entry : QueueRig.Log.Entries)
		{
			if ((Entry.Flags & RoundLogFlags::Debug) != 0)
			{
				DebugEntries.Add(&Entry);
			}
		}
		if (TestEqual(TEXT("both debug commands were resolved and logged with the Debug flag"), DebugEntries.Num(), 2))
		{
			TestTrue(TEXT("each consumed a Seq (stamped, distinct keys)"), DebugEntries[0]->Key.IsSet() && DebugEntries[1]->Key.IsSet() && DebugEntries[0]->Key != DebugEntries[1]->Key);
			TestTrue(TEXT("debugwin logs a DebugForced WinLatched"), DebugEntries[0]->Type == ERoundLogType::WinLatched && DebugEntries[0]->Code == static_cast<uint16>(ERoundEndReason::DebugForced));
		}
		TestEqual(TEXT("both effects applied after logging"), Effects, 2);

		// F7 excludes a Debug-flagged round.
		FRoundPacingInput Input;
		Input.DurationSec = PacingDurationSec;
		Input.EndReason = ERoundEndReason::ObjectiveWin;
		Input.bDebug = true;
		Input.TeamSize[0] = Input.TeamSize[1] = FixtureTeamSize;
		const FRoundPacingAggregate Aggregate = StealthRoundPacing::Aggregate(MakeArrayView(&Input, 1), FRoundPillar4GateSpec());
		TestFalse(TEXT("F7 excludes the Debug round (no share)"), Aggregate.ObjectiveEndShareAll.IsAvailable());

		// Outside Active a debug command is WrongPhase.
		TUniquePtr<FRoundExitBRig> LobbyRig = MakeRig(false);
		LobbyRig->Core->SetDebugQueue(QueueRig.Queue.Get());
		TestTrue(TEXT("debugwin in Lobby is WrongPhase"), LobbyRig->Core->ExecuteHostCommand(Host, ERoundHostCommand::DebugWin, Win) == ERoundCommandResult::WrongPhase);
		QueueRig.Queue->UnregisterResolver(EQueueItemType::DebugItem);
	}

	// --- [STATIC] stealth.round.* registered only on the game instance store --------------------------------------------------
	{
		TArray<FString> Files;
		IFileManager::Get().FindFilesRecursive(Files, *FPaths::Combine(FPaths::ProjectDir(), TEXT("Source")), TEXT("*.cpp"), true, false);
		const FString Allowed = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), AllowedRegistrationFile));
		int32 Registrations = 0;
		for (const FString& File : Files)
		{
			if (File.Contains(TEXT("/Tests/")) || File.Contains(TEXT("\\Tests\\")) || File.Contains(TEXT("Intermediate")))
			{
				continue;
			}
			FString Text;
			if (!FFileHelper::LoadFileToString(Text, *File))
			{
				continue;
			}
			const bool bRegisters = Text.Contains(TEXT("RegisterConsoleCommand")) && Text.Contains(TEXT("stealth.round."));
			if (!bRegisters)
			{
				continue;
			}
			++Registrations;
			const FString Full = FPaths::ConvertRelativePathToFull(File);
			TestTrue(FString::Printf(TEXT("stealth.round.* registered only in the game-instance store (found in %s)"), *FPaths::GetCleanFilename(File)), FPaths::IsSamePath(Full, Allowed));
			for (const TCHAR* Marker : ForbiddenOwnerMarkers)
			{
				TestFalse(FString::Printf(TEXT("%s is not a %s file"), *FPaths::GetCleanFilename(File), Marker), FPaths::GetCleanFilename(File).Contains(Marker));
			}
		}
		TestEqual(TEXT("exactly one file registers the stealth.round.* commands"), Registrations, 1);
		TestEqual(TEXT("the frozen command list has nine names"), static_cast<int32>(UE_ARRAY_COUNT(RoundHostCommandNames)), URoundMatchStore::CommandCount);
		for (int32 Index = 0; Index < URoundMatchStore::CommandCount; ++Index)
		{
			TestEqual(FString::Printf(TEXT("command %d name matches the fixture"), Index), FString(URoundMatchStore::CommandNames[Index]), FString(RoundHostCommandNames[Index]));
		}
	}
	return true;
}

#endif
