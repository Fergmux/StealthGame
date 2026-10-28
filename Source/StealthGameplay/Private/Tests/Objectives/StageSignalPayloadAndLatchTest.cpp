// Objectives Chain story oc-015 AC3 (ADR-0020 section 7 steps 3-5 [U]): StealthGame.Objectives.StageSignalPayloadAndLatch.
// The scheduled TimerFired payload is Subject none, Generation 0, Seq k, Phase = viewing team (the completer's opponent); the
// resolver increments that team's released count, appends the server-only StageSignalReleased(viewingTeam, count) and writes
// the view; Seq k is distinct per release; OnRosterReset clears the pending list (a timer still queued then resolves to
// nothing); after the latch the queue returns Rejected_Latched for a pending release; the count never exceeds 2 (a third
// release attempt trips the invariant: a fault, nothing published).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "ObjectivesRivalRig.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectivesStageSignalPayloadAndLatchTest, "StealthGame.Objectives.StageSignalPayloadAndLatch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FObjectivesStageSignalPayloadAndLatchTest::RunTest(const FString& Parameters)
{
	using namespace StealthObjectivesTest;
	// Payload and resolver: team 0 completes P1 (k = 1), team 1 P1 (k = 2), team 0 P2 (k = 3).
	{
		FRivalRig Rig;
		UObjectiveTeamViewComponent* Team1View = Rig.ViewTargets.Add(2, 1);
		Rig.ViewTargets.Add(1, 0);
		Rig.ToActive();
		const double T0 = Rig.Core().GetActivePhaseStart();
		Rig.CompleteStageFast(0);
		Rig.CompleteStageFast(1);
		Rig.CompleteStageFast(0);
		Rig.RunUntil(T0 + 400.0, 0.5);
		TestEqual(TEXT("three releases resolved"), Rig.Spy.Payloads.Num(), 3);
		TSet<uint32> Seqs;
		int32 BadPayloads = 0;
		for (int32 Index = 0; Index < Rig.Spy.Payloads.Num(); ++Index)
		{
			const FTimerPayload& Payload = Rig.Spy.Payloads[Index];
			Seqs.Add(Payload.Seq);
			const uint8 ExpectedViewer = Payload.Seq == 2 ? 0 : 1; // k = 2 was team 1's completion
			BadPayloads += Payload.Kind == ETimerKind::StageSignal && Payload.Subject.Kind == ESubjectRefKind::None && Payload.Subject.CourtSocketId == 0
				&& Payload.Generation == 0 && Payload.Phase == ExpectedViewer && Rig.Spy.Results[Index] == EResolveResult::Executed ? 0 : 1;
		}
		TestEqual(TEXT("every payload is {StageSignal, Subject none, Generation 0, Seq k, Phase viewing team}"), BadPayloads, 0);
		TestEqual(TEXT("Seq k is distinct per release (1, 2, 3)"), Seqs.Num(), 3);
		TestTrue(TEXT("Seq values are the ordinals"), Seqs.Contains(1) && Seqs.Contains(2) && Seqs.Contains(3));
		TestEqual(TEXT("team 1 (viewer of team 0's two) counts 2"), static_cast<int32>(Rig.RivalCount(1)), 2);
		TestEqual(TEXT("team 0 (viewer of team 1's one) counts 1"), static_cast<int32>(Rig.RivalCount(0)), 1);
		TestEqual(TEXT("team 1's component was written"), static_cast<int32>(Team1View->GetView().RivalPrelimCount), 2);
		const TArray<FRoundLogEntry> Released = Rig.LoggedOf(ERoundLogType::StageSignalReleased);
		TestEqual(TEXT("three StageSignalReleased entries"), Released.Num(), 3);
		int32 BadEntries = 0;
		for (const FRoundLogEntry& Entry : Released)
		{
			// D0 viewing team, D1 the count after it; no participant, no court.
			BadEntries += Entry.Data[0] < 2 && Entry.Data[1] >= 1 && Entry.Data[1] <= 2 && Entry.Participants[0] == FParticipantId::InvalidValue ? 0 : 1;
		}
		TestEqual(TEXT("each entry is (viewing team, count) and names nobody"), BadEntries, 0);
		TestEqual(TEXT("nothing pending afterwards"), Rig.Core().GetPendingRivalReleases().Num(), 0);
	}

	// OnRosterReset clears the pending list: the still-queued timer resolves to nothing (Voided), no count, no entry.
	{
		FRivalRig Rig;
		Rig.ToActive();
		const double T0 = Rig.Core().GetActivePhaseStart();
		Rig.CompleteStageFast(1);
		TestEqual(TEXT("one pending before the reset"), Rig.Core().GetPendingRivalReleases().Num(), 1);
		Rig.Service->HandleRosterReset();
		TestEqual(TEXT("OnRosterReset cleared the pending list"), Rig.Core().GetPendingRivalReleases().Num(), 0);
		// A new round on the same queue epoch (the harness keeps the epoch; production drops the item by epoch first).
		Rig.ToActive();
		Rig.RunUntil(T0 + 400.0, 1.0);
		TestTrue(TEXT("the stale timer was Voided (nothing published)"), Rig.Spy.Results.Num() == 1 && Rig.Spy.Results[0] == EResolveResult::Voided);
		TestEqual(TEXT("no StageSignalReleased after the reset"), Rig.Logged(ERoundLogType::StageSignalReleased), 0);
		TestEqual(TEXT("count 0"), static_cast<int32>(Rig.RivalCount(0)), 0);
	}

	// Reset drops a pending release by epoch (the queue's Rejected_StaleEpoch): the resolver never runs.
	{
		FRivalRig Rig;
		Rig.ToActive();
		const double T0 = Rig.Core().GetActivePhaseStart();
		Rig.CompleteStageFast(1);
		Rig.Q.Epoch.Current.RoundEpoch += 1; // Round's Reset bumps the epoch
		Rig.RunUntil(T0 + 400.0, 1.0);
		TestEqual(TEXT("stale epoch: the resolver never ran"), Rig.Spy.Payloads.Num(), 0);
		TestEqual(TEXT("stale epoch: nothing published"), static_cast<int32>(Rig.RivalCount(0)), 0);
	}

	// After the latch the queue returns Rejected_Latched; the item is never rescheduled.
	{
		FRivalRig Rig;
		Rig.ToActive();
		const double T0 = Rig.Core().GetActivePhaseStart();
		Rig.CompleteStageFast(1);
		Rig.Q.Queue->Latch(FQueueKey(999999, 1));
		Rig.RunUntil(T0 + 400.0, 1.0);
		TestEqual(TEXT("latched: no resolver call"), Rig.Spy.Payloads.Num(), 0);
		TestEqual(TEXT("latched: Rejected_Latched logged once"), Rig.LoggedOf(ERoundLogType::Rejected_Latched).FilterByPredicate([](const FRoundLogEntry& E)
		{
			return E.Data[0] == static_cast<uint32>(EQueueItemType::TimerFired);
		}).Num(), 1);
		TestEqual(TEXT("latched: no StageSignal timer remains (never rescheduled)"), Rig.Q.Queue->GetPendingTimerCount(), 0);
		TestEqual(TEXT("latched: scheduled once"), Rig.Service->GetScheduledStageSignalCount(), 1);
	}

	// The count never exceeds 2: a third release for one team trips the invariant (fault) and publishes nothing.
	{
		FCoreRig Rig;
		FStealthRecordingFaultSink Faults;
		FObjectivesCoreDeps Deps;
		Deps.Roster = &Rig.Roster;
		Deps.Session = &Rig.Session;
		Deps.PawnControl = &Rig.PawnControl;
		Deps.Round = &Rig.Round;
		Deps.Courts = &Rig.Courts;
		Deps.Views = &Rig.Views;
		Deps.RivalRelease = &Rig.Rival;
		Deps.SecretDraw = &Rig.SecretDraw;
		Deps.Faults = &Faults;
		Rig.Core.Initialize(Deps);
		Rig.Core.ApplyDebugStageCompletion(1, FQueueKey(2, 1));
		Rig.Core.ApplyDebugStageCompletion(1, FQueueKey(3, 1));
		for (const FObjectivesRivalRelease& Release : Rig.Rival.Requests)
		{
			Rig.Core.ResolveRivalRelease(Release.ViewingTeam, Release.Ordinal, FQueueKey(10 + Release.Ordinal, 1));
		}
		TestEqual(TEXT("two releases: 2"), static_cast<int32>(Rig.Core.GetRivalPrelimCount(0)), 2);
		FObjectivesRivalRelease Third;
		Third.CompletingTeam = 1;
		Third.ViewingTeam = 0;
		Third.Ordinal = 99;
		Rig.Core.InjectPendingRivalReleaseForTest(Third);
		const int32 FaultsBefore = Faults.GetRecords().Num();
		TestFalse(TEXT("a third release is refused"), Rig.Core.ResolveRivalRelease(0, 99, FQueueKey(20, 1)));
		TestEqual(TEXT("the count stays 2"), static_cast<int32>(Rig.Core.GetRivalPrelimCount(0)), 2);
		TestEqual(TEXT("the invariant fault was reported"), Faults.GetRecords().Num() - FaultsBefore, 1);
		TestEqual(TEXT("no third StageSignalReleased"), Rig.Round.Count(ERoundLogType::StageSignalReleased), 2);
	}
	return true;
}

#endif
