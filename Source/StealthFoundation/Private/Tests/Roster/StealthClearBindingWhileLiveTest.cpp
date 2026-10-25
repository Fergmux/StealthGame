// Story psr-012 (production/epics/participant-state-roster/story-012-clear-binding-while-live.md):
// AC-PSR-25, clearing a live binding during Round phase Active faults; level-teardown clears and
// InvalidateAllBindings are silent (R9, I4). The Round phase comes from the injected IRosterRoundPhase
// (decision D43). Decision recorded in the story: OnBindingLost(PawnDestroyed) is implemented for the
// Destroyed + Active-participant case (R9, R12) and asserted here as an extra clause.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Tests/Roster/StealthRosterTestFixture.h"

namespace ClearBindingTestData
{
	/** An Active, InRound participant with a live pawn, Round phase Active, and a recorder. */
	struct FActiveWithPawn
	{
		FRosterTestFixture F;
		FRosterEventRecorder Rec;
		FParticipantId Id;
		FRosterPawnHandle Pawn = RosterTestData::Pawn(1);

		FActiveWithPawn()
		{
			Id = F.BeginAndRegister(RosterTestData::FullTeamSize, 1)[0];
			F.Core.SeamSetState(Id, EParticipantState::Active, true);
			F.Core.BindPawn(Id, Pawn);
			F.RoundPhase.bActive = true;
			F.Core.AddListener(&Rec);
		}
		~FActiveWithPawn() { F.Core.RemoveListener(&Rec); }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRosterClearBindingWhileLiveTest, "StealthGame.Roster.ClearBindingWhileLive",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRosterClearBindingWhileLiveTest::RunTest(const FString& Parameters)
{
	using namespace ClearBindingTestData;

	// Destroyed during phase Active: one fault, no live pawn, state unchanged, listed as unbound Active.
	{
		FActiveWithPawn S;
		const uint64 SeqBefore = S.F.Core.GetEventSeq();
		TestEqual(TEXT("Destroyed: result"), S.F.Core.ClearBinding(S.Pawn, ERosterEndPlayReason::Destroyed), ERosterResult::Applied);
		if (TestEqual(TEXT("Destroyed: exactly one fault"), S.F.Sink.GetRecords().Num(), 1))
		{
			TestTrue(TEXT("Destroyed: fault code"), S.F.Sink.GetRecords()[0].Fault == EStealthFault::RosterLiveBindingCleared);
			TestEqual(TEXT("Destroyed: fault names the participant"), S.F.Sink.GetRecords()[0].Detail, static_cast<int64>(S.Id.Value));
		}
		TestFalse(TEXT("Destroyed: GetPawn(Id) is null"), S.F.Core.GetPawn(S.Id).IsSet());
		TestTrue(TEXT("Destroyed: state unchanged (Active)"), S.F.Record(S.Id).State == EParticipantState::Active);
		TArray<FParticipantId> Unbound;
		S.F.Core.GetUnboundActiveParticipants(Unbound);
		TestTrue(TEXT("Destroyed: GetUnboundActiveParticipants contains Id"), Unbound.Contains(S.Id));
		// Extra clause (R9, R12): OnBindingLost(PawnDestroyed), one EventSeq.
		if (TestEqual(TEXT("Destroyed: one OnBindingLost"), S.Rec.CountOf(ERosterEventType::BindingLost), 1))
		{
			TestEqual(TEXT("Destroyed: OnBindingLost names the participant"), S.Rec.Events[0].Id, S.Id);
			TestTrue(TEXT("Destroyed: cause PawnDestroyed"), S.Rec.Events[0].Cause == ERosterBindingLostCause::PawnDestroyed);
		}
		TestEqual(TEXT("Destroyed: one EventSeq consumed"), S.F.Core.GetEventSeq(), SeqBefore + 1);
	}

	// Teardown reasons: no fault, no event.
	const ERosterEndPlayReason TeardownReasons[] = { ERosterEndPlayReason::EndPlayInEditor, ERosterEndPlayReason::LevelTransition };
	for (const ERosterEndPlayReason Reason : TeardownReasons)
	{
		FActiveWithPawn S;
		const uint64 SeqBefore = S.F.Core.GetEventSeq();
		TestEqual(TEXT("teardown: result"), S.F.Core.ClearBinding(S.Pawn, Reason), ERosterResult::Applied);
		TestEqual(TEXT("teardown: no fault"), S.F.Sink.GetRecords().Num(), 0);
		TestEqual(TEXT("teardown: no event"), S.Rec.Events.Num(), 0);
		TestEqual(TEXT("teardown: no EventSeq"), S.F.Core.GetEventSeq(), SeqBefore);
		TestFalse(TEXT("teardown: the binding is cleared"), S.F.Core.IsPawnBound(S.Pawn));
	}

	// InvalidateAllBindings: no fault, no event, every binding and tombstone gone, lifecycle untouched.
	{
		FActiveWithPawn S;
		S.F.Core.SeamSetBinding(RosterTestData::Pawn(2), S.Id, ERosterBindingStatus::Retired, 0);
		const uint64 SeqBefore = S.F.Core.GetEventSeq();
		TestEqual(TEXT("InvalidateAllBindings: result"), S.F.Core.InvalidateAllBindings(), ERosterResult::Applied);
		TestEqual(TEXT("InvalidateAllBindings: no fault"), S.F.Sink.GetRecords().Num(), 0);
		TestEqual(TEXT("InvalidateAllBindings: no event"), S.Rec.Events.Num(), 0);
		TestEqual(TEXT("InvalidateAllBindings: no EventSeq"), S.F.Core.GetEventSeq(), SeqBefore);
		TestTrue(TEXT("InvalidateAllBindings: binding table empty"), S.F.Core.Snapshot().Bindings.Num() == 0);
		TestFalse(TEXT("InvalidateAllBindings: no live pawn"), S.F.Core.GetPawn(S.Id).IsSet());
		TestTrue(TEXT("InvalidateAllBindings: state unchanged"), S.F.Record(S.Id).State == EParticipantState::Active);
	}

	// Destroyed outside phase Active: silent fault-wise (the R9 fault is phase-Active only).
	{
		FActiveWithPawn S;
		S.F.RoundPhase.bActive = false;
		S.F.Core.ClearBinding(S.Pawn, ERosterEndPlayReason::Destroyed);
		TestEqual(TEXT("Destroyed outside phase Active: no fault"), S.F.Sink.GetRecords().Num(), 0);
	}

	// Wrong input: an Unbound pawn.
	{
		FActiveWithPawn S;
		const FRosterSnapshot Before = S.F.Core.Snapshot();
		TestEqual(TEXT("Unbound pawn: NoOp_NotBound"), S.F.Core.ClearBinding(RosterTestData::Pawn(9), ERosterEndPlayReason::Destroyed), ERosterResult::NoOp_NotBound);
		TestTrue(TEXT("Unbound pawn: nothing changed"), S.F.Core.Snapshot() == Before);
		TestEqual(TEXT("Unbound pawn: no fault"), S.F.Sink.GetRecords().Num(), 0);
	}
	return true;
}

#endif
