// Story oc-009 (production/epics/objectives-chain/story-009-final-completion-and-win-declaration.md), AC3 / AC-OC-05
// NoStackNoCancel: GIVEN two teammates holding at their live site, Begin 0.5 s apart (D = ObjectiveInteractSec) WHEN the first
// completes THEN the stage counts once, the second hold is NOT cancelled (it is still running, no Cancelled event at all, so
// no TargetInvalid), and it completes at its own D with no effect. Two teammates do not tend faster (the first completes at D,
// not earlier). Edges: a Begin gap of 0 (the same tick: ordered by Begin key, the lower key counts); the second tender
// releasing voluntarily before its D is an ordinary cancel (Released) and changes nothing.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "ObjectivesQueueRig.h"

namespace ObjectivesNoStackNoCancelTestData
{
	struct FEvents
	{
		TArray<TPair<const AStealthPawn*, double>> Completed;
		TArray<TPair<const AStealthPawn*, ECancelReason>> Cancelled;
	};

	void Listen(StealthObjectivesTest::FQueueRig& Rig, FEvents& Out)
	{
		Rig.Hub->OnActionCompleted.AddLambda([&Rig, &Out](AStealthPawn* Pawn, EActionType) { Out.Completed.Emplace(Pawn, Rig.Q.Clock.GetServerTimeSeconds()); });
		Rig.Hub->OnActionCancelled.AddLambda([&Out](AStealthPawn* Pawn, EActionType, ECancelReason Reason) { Out.Cancelled.Emplace(Pawn, Reason); });
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectivesNoStackNoCancelTest, "StealthGame.Objectives.NoStackNoCancel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FObjectivesNoStackNoCancelTest::RunTest(const FString& Parameters)
{
	using namespace StealthObjectivesTest;
	using namespace ObjectivesNoStackNoCancelTestData;
	const double D = 3.0;

	// Begin 0.5 s apart.
	{
		FQueueRig Rig;
		TestTrue(TEXT("Active"), Rig.ToActive());
		FEvents Events;
		Listen(Rig, Events);
		AStealthPawn* First = Rig.Pawn(CourtP1Team0, 0, 1, 0);
		AStealthPawn* Second = Rig.Pawn(CourtP1Team0, 1, 2, 0);
		Rig.Begin(First, CourtP1Team0);
		Rig.Q.Tick(0.1);
		const double FirstBegin = Rig.Q.Clock.GetServerTimeSeconds();
		Rig.Run(0.5);
		Rig.Begin(Second, CourtP1Team0);
		Rig.Q.Tick(0.1);
		const double SecondBegin = Rig.Q.Clock.GetServerTimeSeconds();
		Rig.Run(D - 0.6 + 0.05); // just past the first's D
		TestEqual(TEXT("the first completed"), Events.Completed.Num(), 1);
		TestEqual(TEXT("the stage counted once"), Rig.Logged(ERoundLogType::ObjectiveStage), 1);
		TestTrue(TEXT("the first completed at its D, not earlier (no stacking)"), Events.Completed.Num() == 1 && Events.Completed[0].Value >= FirstBegin + D - 1e-6);
		TestNotNull(TEXT("the second hold is still running (not cancelled)"), Rig.Holds->FindHold(Second));
		Rig.Run(0.6);
		TestEqual(TEXT("the second completed too"), Events.Completed.Num(), 2);
		TestTrue(TEXT("at its own D"), Events.Completed.Num() == 2 && Events.Completed[1].Key == Second && Events.Completed[1].Value >= SecondBegin + D - 1e-6);
		TestEqual(TEXT("still one ObjectiveStage (the second had no effect)"), Rig.Logged(ERoundLogType::ObjectiveStage), 1);
		TestEqual(TEXT("no hold was cancelled (no TargetInvalid)"), Events.Cancelled.Num(), 0);
		Rig.Hub->OnActionCompleted.Clear();
		Rig.Hub->OnActionCancelled.Clear();
	}

	// Begin gap 0: the same tick, ordered by Begin key.
	{
		FQueueRig Rig;
		TestTrue(TEXT("Active"), Rig.ToActive());
		AStealthPawn* First = Rig.Pawn(CourtP1Team0, 0, 1, 0);
		AStealthPawn* Second = Rig.Pawn(CourtP1Team0, 1, 2, 0);
		Rig.Begin(First, CourtP1Team0);
		Rig.Begin(Second, CourtP1Team0);
		Rig.Run(D + 0.2);
		const TArray<FRoundLogEntry> Stages = Rig.LoggedOf(ERoundLogType::ObjectiveStage);
		TestTrue(TEXT("same tick: the lower Begin key counts"), Stages.Num() == 1 && Stages[0].Participants[0] == 1);
	}

	// The second tender releases voluntarily before its D.
	{
		FQueueRig Rig;
		TestTrue(TEXT("Active"), Rig.ToActive());
		FEvents Events;
		Listen(Rig, Events);
		AStealthPawn* First = Rig.Pawn(CourtP1Team0, 0, 1, 0);
		AStealthPawn* Second = Rig.Pawn(CourtP1Team0, 1, 2, 0);
		Rig.Begin(First, CourtP1Team0);
		Rig.Q.Tick(0.1);
		Rig.Run(0.5);
		Rig.Begin(Second, CourtP1Team0);
		Rig.Run(D - 0.3); // past the first's D (it counts), before the second's
		TestEqual(TEXT("the first counted while the second still holds"), Rig.Logged(ERoundLogType::ObjectiveStage), 1);
		Rig.End(Second);
		Rig.Q.Tick(0.1);
		TestTrue(TEXT("the voluntary release is an ordinary cancel (Released)"), Events.Cancelled.Num() == 1 && Events.Cancelled[0].Value == ECancelReason::Released);
		TestEqual(TEXT("the stage still counted once"), Rig.Logged(ERoundLogType::ObjectiveStage), 1);
		Rig.Hub->OnActionCompleted.Clear();
		Rig.Hub->OnActionCancelled.Clear();
	}
	return true;
}

#endif
