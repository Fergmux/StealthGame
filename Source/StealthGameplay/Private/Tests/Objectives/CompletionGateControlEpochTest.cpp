// Story oc-008 (production/epics/objectives-chain/story-008-completion-gate-and-stage-effects.md), AC2 (ADR-0020 5.4 (c)-(e))
// CompletionGateControlEpoch:
//   - a pawn whose control epoch changed since Begin (E -> E+1 before Complete, even with no hand-over pending): no effect;
//   - a pawn with a hand-over pending (epoch unchanged): no effect;
//   - a second completion at an already Completed site: no effect and no ObjectiveStage entry;
//   - a rejected completion emits nothing beyond the IIF's normal OnActionCompleted (through the queue: Completed is still
//     broadcast; no view push, no rival release; only the server-only TendCompleted(NoEffect));
//   - two completions at one site in the same drain (two teammates whose holds are due together): the first counts, the
//     second has no effect.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "ObjectivesQueueRig.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectivesCompletionGateControlEpochTest, "StealthGame.Objectives.CompletionGateControlEpoch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FObjectivesCompletionGateControlEpochTest::RunTest(const FString& Parameters)
{
	using namespace StealthObjectivesTest;
	StealthAssassinationTest::FScopedWorld World(TEXT("ObjectivesCompletionGateControlEpoch"));

	// The epoch changed since Begin (no hand-over pending).
	{
		FCoreRig Rig;
		AStealthPawn* Pawn = FCoreRig::PawnAt(World, CourtP1Team0, 0);
		Rig.Roster.Bind(Pawn, 1, 0);
		Rig.PawnControl.Epochs.Add(Pawn, 4);
		Rig.Core.OnInteractExecuted(*Pawn, FCourtFixtureId(CourtP1Team0), FQueueKey(1, 1));
		Rig.PawnControl.Epochs[Pawn] = 5;
		TestEqual(TEXT("epoch E+1 at Complete: no effect"), static_cast<int32>(Rig.Core.OnInteractEnded(*Pawn, FCourtFixtureId(CourtP1Team0), EObjectivesInteractEnd::Completed, FQueueKey(2, 1))),
			static_cast<int32>(EObjectivesCompletion::NoEffect));
		TestEqual(TEXT("epoch changed: no ObjectiveStage"), Rig.Round.Count(ERoundLogType::ObjectiveStage), 0);
	}
	// A hand-over pending (epoch unchanged).
	{
		FCoreRig Rig;
		AStealthPawn* Pawn = FCoreRig::PawnAt(World, CourtP1Team0, 0);
		Rig.Roster.Bind(Pawn, 1, 0);
		Rig.Core.OnInteractExecuted(*Pawn, FCourtFixtureId(CourtP1Team0), FQueueKey(1, 1));
		Rig.PawnControl.Pending.Add(Pawn);
		TestEqual(TEXT("hand-over pending: no effect"), static_cast<int32>(Rig.Core.OnInteractEnded(*Pawn, FCourtFixtureId(CourtP1Team0), EObjectivesInteractEnd::Completed, FQueueKey(2, 1))),
			static_cast<int32>(EObjectivesCompletion::NoEffect));
	}
	// A second completion at a Completed site.
	{
		FCoreRig Rig;
		AStealthPawn* First = FCoreRig::PawnAt(World, CourtP1Team0, 0);
		AStealthPawn* Second = FCoreRig::PawnAt(World, CourtP1Team0, 1);
		Rig.Roster.Bind(First, 1, 0);
		Rig.Roster.Bind(Second, 2, 0);
		TestEqual(TEXT("the first completion counts"), static_cast<int32>(Rig.Tend(*First, CourtP1Team0)), static_cast<int32>(EObjectivesCompletion::CountedStage));
		TestEqual(TEXT("a second completion at the Completed site: no effect"), static_cast<int32>(Rig.Tend(*Second, CourtP1Team0, FQueueKey(3, 1))),
			static_cast<int32>(EObjectivesCompletion::NoEffect));
		TestEqual(TEXT("one ObjectiveStage only"), Rig.Round.Count(ERoundLogType::ObjectiveStage), 1);
	}

	// Through the queue: a rejected completion still ends with the IIF's normal OnActionCompleted and nothing else.
	{
		FQueueRig Rig;
		TestTrue(TEXT("Active"), Rig.ToActive());
		AStealthPawn* Pawn = Rig.Pawn(CourtP1Team0, 0, 1, 0);
		int32 Completed = 0;
		Rig.Hub->OnActionCompleted.AddLambda([&Completed](AStealthPawn*, EActionType) { ++Completed; });
		const int32 Pushes = Rig.Views.Pushes.Num();
		Rig.Begin(Pawn, CourtP1Team0);
		Rig.Q.Tick(0.1);
		Rig.PawnControl.Epochs.Add(Pawn, 9); // a takeover mid-hold
		Rig.Run(3.2);
		TestEqual(TEXT("the IIF's OnActionCompleted still fires once"), Completed, 1);
		TestEqual(TEXT("no ObjectiveStage"), Rig.Logged(ERoundLogType::ObjectiveStage), 0);
		TestEqual(TEXT("no view push"), Rig.Views.Pushes.Num(), Pushes);
		TestEqual(TEXT("no rival release"), Rig.Rival.Calls.Num(), 0);
		TestEqual(TEXT("one server-only TendCompleted(NoEffect)"), Rig.Logged(ERoundLogType::TendCompleted), 1);
		Rig.Hub->OnActionCompleted.Clear();
	}

	// Two completions at one site in the same drain: the first counts, the second has no effect.
	{
		FQueueRig Rig;
		TestTrue(TEXT("Active"), Rig.ToActive());
		AStealthPawn* A = Rig.Pawn(CourtP1Team0, 0, 1, 0);
		AStealthPawn* B = Rig.Pawn(CourtP1Team0, 1, 2, 0);
		Rig.Begin(A, CourtP1Team0);
		Rig.Begin(B, CourtP1Team0);
		Rig.Run(3.2);
		TestEqual(TEXT("same drain: one ObjectiveStage"), Rig.Logged(ERoundLogType::ObjectiveStage), 1);
		const TArray<FRoundLogEntry> Tends = Rig.LoggedOf(ERoundLogType::TendCompleted);
		if (TestEqual(TEXT("same drain: two TendCompleted"), Tends.Num(), 2))
		{
			TestTrue(TEXT("first Counted, second NoEffect, in key order"), Tends[0].Code == static_cast<uint16>(ERoundTendResult::Counted)
				&& Tends[1].Code == static_cast<uint16>(ERoundTendResult::NoEffect) && Tends[0].Key < Tends[1].Key);
		}
	}
	return true;
}

#endif
