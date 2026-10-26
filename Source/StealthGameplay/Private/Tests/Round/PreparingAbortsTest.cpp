// Story rmf-034 (production/epics/round-match-framework/story-034-preparing-aborts.md), GDD AC 7 / R6, R8, R10:
//   every Preparing abort, one table row per code, goes through the one abort path; after lobby travel the phase is Lobby,
//   RoundsPlayedCount and MatchScore are unchanged, there is no FRoundResult in the public struct, 0 pawns exist, the
//   archived slice holds PreparingFailed(code), bCalledOff is true only for the host abort and nothing is released (the
//   slice never reached Ended). Any number of client ClientHash/PoolHash mismatches never abort: each only disconnects
//   that client (ParamsMismatch, Session) and the round reaches Active.
// Code set (decision D48): ADR-0002 section 8.6's closed set, all 13 codes (the AC's "TDC Failed" is TuningLoadFailed;
// Error_DeckTooSmall aborts as DeckInvalid and is a second injection of that row), plus the host abort (EndReason
// Aborted; no abort code). No string fixture codes are quoted here (StealthGame.Round.FixtureConstants).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "StealthRoundExitBTestRig.h"

namespace PreparingAbortsTestData
{
	enum class EInject : uint8
	{
		TravelFailed,
		TuningFailed,
		SnapshotFault,
		DeckInvalid,
		DeckTooSmall,
		AlreadyDealt,
		LayoutAborted,
		LayoutSeedMismatch,
		TeamAbsent,
		BindFailed,
		PlacementInvariant,
		PlacementInfeasible,
		NpcCountTooLow,
		Watchdog,
		HostAbort,
	};

	struct FRow
	{
		EInject Inject;
		ERoundPreparingAbortCode Expected;
		const TCHAR* Name;
	};

	const FRow Rows[] = {
		{ EInject::TravelFailed, ERoundPreparingAbortCode::BadParams, TEXT("P1 travel failed") },
		{ EInject::TuningFailed, ERoundPreparingAbortCode::TuningLoadFailed, TEXT("P2 TDC Failed") },
		{ EInject::SnapshotFault, ERoundPreparingAbortCode::TuningSnapshotFault, TEXT("P2 server self-check fault") },
		{ EInject::DeckInvalid, ERoundPreparingAbortCode::DeckInvalid, TEXT("P3 deck invalid") },
		{ EInject::DeckTooSmall, ERoundPreparingAbortCode::DeckInvalid, TEXT("P3 deck too small") },
		{ EInject::AlreadyDealt, ERoundPreparingAbortCode::AllocatorMisuse, TEXT("P3 second deal") },
		{ EInject::LayoutAborted, ERoundPreparingAbortCode::LayoutAborted, TEXT("P4 layout aborted") },
		{ EInject::LayoutSeedMismatch, ERoundPreparingAbortCode::LayoutSeedMismatch, TEXT("P4 seed mismatch") },
		{ EInject::TeamAbsent, ERoundPreparingAbortCode::TeamAbsent, TEXT("P5b team absent") },
		{ EInject::BindFailed, ERoundPreparingAbortCode::BindFailed, TEXT("P6 bind rejected") },
		{ EInject::PlacementInvariant, ERoundPreparingAbortCode::PlacementInvariant, TEXT("P6 zone share") },
		{ EInject::PlacementInfeasible, ERoundPreparingAbortCode::PlacementInfeasible, TEXT("P6 placement infeasible") },
		{ EInject::NpcCountTooLow, ERoundPreparingAbortCode::NpcCountTooLow, TEXT("P6 NPC count too low") },
		{ EInject::Watchdog, ERoundPreparingAbortCode::PreparingWatchdog, TEXT("watchdog") },
		{ EInject::HostAbort, ERoundPreparingAbortCode::None, TEXT("host abort") },
	};

	/** Scripts P3/P6 with the row's answers (over-scripting is harmless: unconsumed answers are never unexpected). */
	void ScriptSequence(FRoundExitBRig& Rig, EInject Inject)
	{
		const int32 Players = Rig.Fakes.Roster.Members.Num();
		const ERoundAllocateResult Deal = Inject == EInject::DeckInvalid ? ERoundAllocateResult::DeckInvalid
			: Inject == EInject::DeckTooSmall ? ERoundAllocateResult::Error_DeckTooSmall
			: Inject == EInject::AlreadyDealt ? ERoundAllocateResult::Error_AlreadyDealt : ERoundAllocateResult::Ok;
		const ERoundPlacementResult Placement = Inject == EInject::PlacementInfeasible ? ERoundPlacementResult::PlacementInfeasible
			: Inject == EInject::NpcCountTooLow ? ERoundPlacementResult::NpcCountTooLow : ERoundPlacementResult::Ok;
		Rig.Fakes.LevelSocket.Script(TEXT("RequestLayout"));
		Rig.Fakes.CharacterPool.Script(TEXT("AllocateAppearances"), static_cast<int64>(Deal));
		Rig.Fakes.LevelSocket.Script(TEXT("RequestStartPlacement"), static_cast<int64>(Placement));
		Rig.Fakes.CharacterPool.Script(TEXT("GetAssignedAppearance"), 0, Players);
		Rig.Fakes.Roster.Script(TEXT("RecordAppearance"), static_cast<int64>(ERosterResult::Applied), Players);
		Rig.Fakes.Session.Script(TEXT("GetServerSubSeed"), static_cast<int64>(RoundTestFixtures::PreparingRigSpawnSubSeed));
		Rig.Fakes.Roster.Script(TEXT("GetEpochToken"), RoundTestFixtures::FixtureRoundEpoch);
		// The abort path: admission Closed, the deal discarded, lobby travel; then Lobby: teams unlocked, admission Open.
		Rig.Fakes.Session.Script(TEXT("SetAdmissionMode"), 0, 2);
		Rig.Fakes.CharacterPool.Script(TEXT("DiscardDeal"));
		Rig.Fakes.Session.Script(TEXT("RequestLobbyTravel"));
		Rig.Fakes.Roster.Script(TEXT("SetTeamsLocked"), static_cast<int64>(ERosterResult::Applied));
	}

	/** Runs the row's injection from Preparing entry; the abort happens somewhere inside. */
	void Inject(FRoundExitBRig& Rig, EInject Inject)
	{
		const int32 Seed = Rig.Core->GetPreparing().GetSeed();
		switch (Inject)
		{
		case EInject::TravelFailed: Rig.Core->NotifyTravelFailed(); return;
		case EInject::TuningFailed: Rig.Core->NotifyTuningFailed(); return;
		case EInject::SnapshotFault: Rig.Core->NotifyTuningSnapshotFault(); return;
		case EInject::HostAbort: Rig.Core->ExecuteHostCommand(FRoundExitBRig::HostCaller(), ERoundHostCommand::Abort); return;
		default: break;
		}
		Rig.Core->NotifyTuningReady(RoundTestFixtures::PreparingRigTuningHash);
		if (Rig.Core->GetPhase() != ERoundPhase::Preparing)
		{
			return; // a P3 abort
		}
		if (Inject == EInject::LayoutAborted)
		{
			Rig.Core->NotifyLayoutAborted();
			return;
		}
		Rig.Core->NotifyLayoutReady(1, Inject == EInject::LayoutSeedMismatch ? RoundTestFixtures::PreparingRigOtherSeed : Seed);
		if (Rig.Core->GetPhase() != ERoundPhase::Preparing)
		{
			return;
		}
		if (Inject == EInject::Watchdog)
		{
			// Nobody acknowledges: the barrier never releases and the watchdog (F6 effective, 150 s at defaults) fires.
			Rig.Frame(Rig.Core->GetPreparing().GetWatchdogBound().EffectiveSec + 1.0);
			return;
		}
		if (Inject == EInject::TeamAbsent)
		{
			// Team 1's participants dropped during the load: they leave the wait set and P5b finds no one Connected there.
			for (const FRoundRosterMember& Member : Rig.Fakes.Roster.Members)
			{
				if (Member.Team == ERosterTeam::Team1)
				{
					Rig.Fakes.Session.ConnectionStates[Member.Participant.Value] = ERoundConnectionState::Disconnected;
				}
			}
		}
		if (Inject == EInject::BindFailed)
		{
			Rig.Fakes.Spawner.BindFailParticipants.Add(FRoundExitBRig::HostIdValue + 1);
		}
		if (Inject == EInject::PlacementInvariant)
		{
			Rig.Fakes.LevelSocket.Zones = { { RoundTestFixtures::PlacementPlayers, RoundTestFixtures::PlacementNpcsFail } };
		}
		Rig.AckAll(/*bWorldReady*/ false);
		Rig.Frame(); // the barrier releases: P5b and P6
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoundPreparingAbortsTest, "StealthGame.Round.PreparingAborts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRoundPreparingAbortsTest::RunTest(const FString& Parameters)
{
	using namespace PreparingAbortsTestData;
	using namespace RoundTestFixtures;

	for (const FRow& Row : Rows)
	{
		// Arrange: a 2v2 match in Preparing.
		FRoundExitBRig Rig;
		Rig.OpenLobby();
		Rig.SetLayout(Layout2v2.Team0, Layout2v2.Team1);
		const uint32 PlayedBefore = Rig.Core->GetPhaseCore().GetRoundsPlayedCount();
		Rig.EnterPreparing();
		ScriptSequence(Rig, Row.Inject);

		// Act: inject, then let Session's lobby travel complete (Reset -> Lobby on the next tick).
		Inject(Rig, Row.Inject);
		TestTrue(FString::Printf(TEXT("%s: Preparing aborted into Reset"), Row.Name), Rig.Core->GetPhase() == ERoundPhase::Reset);
		Rig.Core->NotifyLobbyTravelComplete(); // Session: every participant terminal (R10 (7), story rmf-058)
		Rig.Frame();

		// Assert.
		const FRoundPublicState Public = Rig.Core->GetPhaseCore().MakePublicState();
		TestTrue(FString::Printf(TEXT("%s: phase Lobby after lobby travel"), Row.Name), Rig.Core->GetPhase() == ERoundPhase::Lobby);
		TestEqual(FString::Printf(TEXT("%s: RoundsPlayedCount unchanged"), Row.Name), Rig.Core->GetPhaseCore().GetRoundsPlayedCount(), PlayedBefore);
		TestTrue(FString::Printf(TEXT("%s: MatchScore unchanged"), Row.Name), Public.MatchScore[0] == 0 && Public.MatchScore[1] == 0);
		TestTrue(FString::Printf(TEXT("%s: no FRoundResult in the public struct"), Row.Name), Public.LastResult == FRoundResult());
		TestEqual(FString::Printf(TEXT("%s: 0 pawns"), Row.Name), Rig.Fakes.Spawner.LivePawns, 0);
		TestEqual(FString::Printf(TEXT("%s: lobby travel requested once"), Row.Name), Rig.CountSince(0, TEXT("RequestLobbyTravel")), 1);
		const bool bHost = Row.Inject == EInject::HostAbort;
		TestTrue(FString::Printf(TEXT("%s: bCalledOff only for the host abort"), Row.Name), Public.bCalledOff == bHost);

		const TArray<FRoundArchivedSlice>& Archive = Rig.Core->GetLogArchive().GetArchive();
		if (TestEqual(FString::Printf(TEXT("%s: one archived slice"), Row.Name), Archive.Num(), 1))
		{
			const FRoundArchivedSlice& Slice = Archive[0];
			TestTrue(FString::Printf(TEXT("%s: the slice holds the end reason"), Row.Name),
				Slice.EndReason == (bHost ? ERoundEndReason::Aborted : ERoundEndReason::PreparingFailed));
			TestEqual(FString::Printf(TEXT("%s: the slice holds the code"), Row.Name), Slice.AbortCode, static_cast<uint8>(Row.Expected));
			TestFalse(FString::Printf(TEXT("%s: nothing released (the slice never reached Ended)"), Row.Name), Slice.bReachedEnded);
			const FRoundLogEntry* Reason = Slice.Entries.FindByPredicate([](const FRoundLogEntry& E) { return E.Type == ERoundLogType::WinLatched; });
			TestTrue(FString::Printf(TEXT("%s: the PreparingFailed(code) entry is in the slice"), Row.Name), Reason != nullptr
				&& Reason->Code == static_cast<uint16>(bHost ? ERoundEndReason::Aborted : ERoundEndReason::PreparingFailed)
				&& Reason->Data[1] == static_cast<uint32>(Row.Expected) && Reason->Data[0] == GRoundLogNoTeam);
		}
		TestTrue(FString::Printf(TEXT("%s: the latch was never set (the queue is not open in Preparing)"), Row.Name), !Rig.Core->IsTerminated());
		TestTrue(FString::Printf(TEXT("%s: no unexpected call"), Row.Name), Rig.Log.Unexpected.IsEmpty());
	}

	// Client hash mismatches never abort: Session disconnects each mismatching client (ParamsMismatch) and that is all.
	{
		FRoundExitBRig Rig;
		Rig.OpenLobby();
		Rig.SetLayout(Layout2v2.Team0, Layout2v2.Team1);
		Rig.EnterPreparing();
		Rig.Fakes.Session.ConnectionStates[FRoundExitBRig::HostIdValue + 1] = ERoundConnectionState::Disconnected;
		Rig.Fakes.Session.ConnectionStates[FRoundExitBRig::HostIdValue + 3] = ERoundConnectionState::Disconnected;
		Rig.RunPreparingToActive();
		TestTrue(TEXT("two mismatching clients disconnected: the round still reaches Active"), Rig.Core->GetPhase() == ERoundPhase::Active);
		TestEqual(TEXT("nothing archived (no abort)"), Rig.Core->GetLogArchive().GetArchive().Num(), 0);
		TestTrue(TEXT("mismatch case: no unexpected call"), Rig.Log.Unexpected.IsEmpty());
	}
	return true;
}

#endif
