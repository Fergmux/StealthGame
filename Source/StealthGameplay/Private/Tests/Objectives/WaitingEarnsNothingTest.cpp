// Objectives Chain story oc-015 AC2 (AC-OC-16 [U]; SP-3, SP-6): StealthGame.Objectives.WaitingEarnsNothing.
// GIVEN a round where no pawn tends WHEN the fake clock runs 10 x TargetRoundMinutes (Round's non-binding key, read only as the
// run length) THEN no Objectives state, team view or RivalPrelimCount changes, no Objectives timer exists (a release timer
// exists only while a release is pending, and none is), and the round is still Active, in coarse (1 s) and fine (1/60 s)
// clock steps. Control: one counted preliminary makes exactly one StageSignal timer exist until it resolves.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "ObjectivesRivalRig.h"
#include "Server/StealthTuningSubstructs.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectivesWaitingEarnsNothingTest, "StealthGame.Objectives.WaitingEarnsNothing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FObjectivesWaitingEarnsNothingTest::RunTest(const FString& Parameters)
{
	using namespace StealthObjectivesTest;
	const double RunSec = 10.0 * 60.0 * static_cast<double>(FRoundTuning_Server().TargetRoundMinutes);
	const double Steps[] = { 1.0, 1.0 / 60.0 };
	for (const double Step : Steps)
	{
		// Arrange
		FRivalRig Rig;
		UObjectiveTeamViewComponent* Views[2] = { Rig.ViewTargets.Add(1, 0), Rig.ViewTargets.Add(2, 1) };
		TestTrue(TEXT("Active"), Rig.ToActive());
		const TArray<EObjectiveSiteState> Before = Rig.Core().GetDefinitionCount() > 0 ? FCoreRig::SnapshotOf(Rig.Core()) : TArray<EObjectiveSiteState>();
		const FObjectiveTeamView ViewBefore[2] = { Views[0]->GetView(), Views[1]->GetView() };
		const int32 PushesBefore = Rig.Views.Pushes.Num();
		const int32 TimersBefore = Rig.Q.Queue->GetPendingTimerCount();
		int32 MaxObjectivesTimers = 0;
		// Act: nobody tends.
		const double End = Rig.Q.Clock.GetServerTimeSeconds() + RunSec;
		while (Rig.Q.Clock.GetServerTimeSeconds() < End)
		{
			Rig.Q.Tick(Step);
			TArray<ETimerKind> Kinds;
			Rig.Q.Queue->GetPendingTimerKinds(Kinds);
			MaxObjectivesTimers = FMath::Max(MaxObjectivesTimers, Kinds.FilterByPredicate([](ETimerKind Kind) { return Kind == ETimerKind::StageSignal; }).Num());
		}
		// Assert
		const FString Label = Step >= 1.0 ? TEXT("coarse") : TEXT("fine");
		TestTrue(*(Label + TEXT(": no site state changed")), FCoreRig::SnapshotOf(Rig.Core()) == Before);
		TestTrue(*(Label + TEXT(": no team view changed")), Views[0]->GetView() == ViewBefore[0] && Views[1]->GetView() == ViewBefore[1] && Rig.Views.Pushes.Num() == PushesBefore);
		TestTrue(*(Label + TEXT(": RivalPrelimCount stays 0")), Rig.RivalCount(0) == 0 && Rig.RivalCount(1) == 0);
		TestEqual(*(Label + TEXT(": no Objectives timer ever existed")), MaxObjectivesTimers, 0);
		TestEqual(*(Label + TEXT(": nothing scheduled by Objectives")), Rig.Service->GetScheduledStageSignalCount(), 0);
		TestEqual(*(Label + TEXT(": no timer at all appeared in the heap")), Rig.Q.Queue->GetPendingTimerCount(), TimersBefore);
		TestTrue(*(Label + TEXT(": the round is still Active")), Rig.Round.IsActive() && !Rig.Q.Queue->IsLatched() && Rig.Core().IsActive());
		AddInfo(FString::Printf(TEXT("OC15 %s: %.0f s of waiting in %.4f s frames, Objectives timers %d"), *Label, RunSec, Step, MaxObjectivesTimers));
	}

	// Control: one counted preliminary makes exactly one StageSignal timer exist until it resolves, then none.
	FRivalRig Rig;
	Rig.ToActive();
	Rig.CompleteStageFast(0);
	TArray<ETimerKind> Kinds;
	Rig.Q.Queue->GetPendingTimerKinds(Kinds);
	TestEqual(TEXT("control: one StageSignal timer while a release is pending"), Kinds.FilterByPredicate([](ETimerKind Kind) { return Kind == ETimerKind::StageSignal; }).Num(), 1);
	Rig.RunUntil(Rig.Q.Clock.GetServerTimeSeconds() + 200.0, 1.0);
	Kinds.Reset();
	Rig.Q.Queue->GetPendingTimerKinds(Kinds);
	TestEqual(TEXT("control: none once it resolved"), Kinds.FilterByPredicate([](ETimerKind Kind) { return Kind == ETimerKind::StageSignal; }).Num(), 0);
	TestEqual(TEXT("control: team 1 saw the release"), static_cast<int32>(Rig.RivalCount(1)), 1);
	return true;
}

#endif
