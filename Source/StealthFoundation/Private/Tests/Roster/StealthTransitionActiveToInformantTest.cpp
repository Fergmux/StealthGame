// Story psr-014 (production/epics/participant-state-roster/story-014-transition-active-to-informant.md):
// AC-PSR-14, Active -> Informant applies for each entering reason (Assassinated, ReversedAgainst, StrikeLimit,
// Disconnect with Connection = Disconnected) with the right pawn disposition (R8, R9; Penalty CR-3; decision
// D42: after a StrikeLimit release the pawn resolves Npc). Exactly one OnParticipantStateChanged is dispatched.
// "Before the call returns": a listener inspects the record while the event is dispatched (dispatch runs after
// the call's writes, inside the outermost call), and LastEventSeq equals the dispatched event's EventSeq
// (the story's open point, confirmed here). Active is set through the core seam until ResetAll (psr-022).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Tests/Roster/StealthRosterTestFixture.h"

namespace TransitionActiveToInformantTestData
{
	/** At dispatch, captures what the record and the pawn look like. */
	class FInspectAtDispatch : public IRosterEventListener
	{
	public:
		FInspectAtDispatch(const FRosterCore& InCore, FParticipantId InId, FRosterPawnHandle InPawn) : Core(InCore), Id(InId), Pawn(InPawn) {}

		virtual void OnRosterEvent(const FRosterEvent& Event) override
		{
			Events.Add(Event);
			if (const FParticipantRecord* Record = Core.FindRecord(Id))
			{
				StateAtDispatch = Record->State;
				ReasonAtDispatch = Record->LastTransitionReason;
				LastEventSeqAtDispatch = Record->LastEventSeq;
				bLivePawnAtDispatch = Record->HasLivePawn();
			}
			KindAtDispatch = Core.ResolveVictimKind(Pawn);
		}

		const FRosterCore& Core;
		FParticipantId Id;
		FRosterPawnHandle Pawn;
		TArray<FRosterEvent> Events;
		EParticipantState StateAtDispatch = EParticipantState::Active;
		ERosterReason ReasonAtDispatch = ERosterReason::None;
		uint64 LastEventSeqAtDispatch = 0;
		bool bLivePawnAtDispatch = true;
		FRosterVictimKind KindAtDispatch;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRosterTransitionActiveToInformantTest, "StealthGame.Roster.Transition.ActiveToInformant",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRosterTransitionActiveToInformantTest::RunTest(const FString& Parameters)
{
	using namespace RosterTestData;
	using namespace TransitionActiveToInformantTestData;

	const ERosterReason Reasons[] = { ERosterReason::Assassinated, ERosterReason::ReversedAgainst, ERosterReason::StrikeLimit, ERosterReason::Disconnect };
	const TCHAR* const Names[] = { TEXT("Assassinated"), TEXT("ReversedAgainst"), TEXT("StrikeLimit"), TEXT("Disconnect") };

	for (int32 Case = 0; Case < static_cast<int32>(UE_ARRAY_COUNT(Reasons)); ++Case)
	{
		const ERosterReason Reason = Reasons[Case];
		const TCHAR* Name = Names[Case];
		const bool bRelease = Reason == ERosterReason::StrikeLimit;

		// Arrange: 2v1, every participant Active and InRound, P (team 0) with a live pawn.
		FRosterTestFixture F;
		const TArray<FParticipantId> Ids = F.BeginAndRegister(FullTeamSize, 3);
		F.AssignSplit(Ids, 2);
		for (const FParticipantId Id : Ids)
		{
			F.Core.SeamSetState(Id, EParticipantState::Active, true);
		}
		const FParticipantId P = Ids[0];
		const FRosterPawnHandle Body = Pawn(1);
		TestEqual(FString::Printf(TEXT("%s: fixture pawn binds"), Name), F.Core.BindPawn(P, Body), ERosterResult::Applied);
		FRosterTransitionOptions Options;
		if (Reason == ERosterReason::Disconnect)
		{
			TestEqual(FString::Printf(TEXT("%s: P disconnects"), Name), F.Core.NotifyConnectionChanged(P, ERosterConnection::Disconnected, 50000), ERosterResult::Applied);
			Options.ExpectedConnectionGeneration = F.Record(P).Connection.ConnectionGeneration;
			Options.ExpectedEpoch = F.Core.GetEpochToken();
		}
		FInspectAtDispatch Inspect(F.Core, P, Body);
		F.Core.AddListener(&Inspect);
		const uint64 SeqBefore = F.Core.GetEventSeq();
		F.Clock.AdvanceSeconds(1.5);
		const int64 CallTimeMs = static_cast<int64>(F.Clock.GetServerTimeSeconds() * 1000.0);

		// Act
		const ERosterResult Result = F.Core.RequestTransition(P, EParticipantState::Informant, Reason, Options);

		// Assert: Applied; state, reason and LastEventSeq written; pawn retired (or released for StrikeLimit).
		TestEqual(FString::Printf(TEXT("%s: Applied"), Name), Result, ERosterResult::Applied);
		const FParticipantRecord After = F.Record(P);
		TestTrue(FString::Printf(TEXT("%s: state Informant"), Name), After.State == EParticipantState::Informant);
		TestTrue(FString::Printf(TEXT("%s: reason written"), Name), After.LastTransitionReason == Reason);
		TestFalse(FString::Printf(TEXT("%s: no live pawn"), Name), F.Core.GetPawn(P).IsSet());
		const FRosterVictimKind ExpectedKind = bRelease
			? FRosterVictimKind::Make(ERosterVictimKind::Npc)
			: FRosterVictimKind::NotTargetable(ERosterNotTargetableReason::AlreadyResolved);
		TestTrue(FString::Printf(TEXT("%s: ResolveVictimKind"), Name), F.Core.ResolveVictimKind(Body) == ExpectedKind);
		TestFalse(FString::Printf(TEXT("%s: pawn not BoundLive"), Name), F.Core.IsPawnBound(Body));
		const FRosterBindingEntry* Row = F.Core.FindBinding(Body);
		if (bRelease)
		{
			TestTrue(FString::Printf(TEXT("%s: pawn Unbound (no row)"), Name), Row == nullptr);
		}
		else if (TestTrue(FString::Printf(TEXT("%s: pawn Retired"), Name), Row && Row->Status == ERosterBindingStatus::Retired))
		{
			TestEqual(FString::Printf(TEXT("%s: retire time defaults to the call's server time"), Name), Row->RetireAtServerTimeMs, CallTimeMs);
		}

		// Exactly one OnParticipantStateChanged, after the writes, with LastEventSeq equal to its EventSeq.
		if (TestEqual(FString::Printf(TEXT("%s: exactly one event"), Name), Inspect.Events.Num(), 1))
		{
			const FRosterEvent& Event = Inspect.Events[0];
			TestTrue(FString::Printf(TEXT("%s: OnParticipantStateChanged"), Name), Event.Type == ERosterEventType::ParticipantStateChanged);
			TestEqual(FString::Printf(TEXT("%s: one EventSeq consumed"), Name), Event.EventSeq, SeqBefore + 1);
			TestEqual(FString::Printf(TEXT("%s: LastEventSeq is the event's EventSeq"), Name), After.LastEventSeq, Event.EventSeq);
			TestTrue(FString::Printf(TEXT("%s: payload id/team/old/new/reason"), Name), Event.Id == P && Event.Team == ERosterTeam::Team0
				&& Event.Old == EParticipantState::Active && Event.New == EParticipantState::Informant && Event.Reason == Reason);
			TestEqual(FString::Printf(TEXT("%s: ActiveCountAfter[0]"), Name), Event.ActiveCountAfter[0], 1);
			TestEqual(FString::Printf(TEXT("%s: ActiveCountAfter[1]"), Name), Event.ActiveCountAfter[1], 1);
			TestEqual(FString::Printf(TEXT("%s: InformantCountAfter[0]"), Name), Event.InformantCountAfter[0], 1);
			TestEqual(FString::Printf(TEXT("%s: InformantCountAfter[1]"), Name), Event.InformantCountAfter[1], 0);
			// What the record looked like when the event was dispatched: every write done before dispatch.
			TestTrue(FString::Printf(TEXT("%s: Informant at dispatch"), Name), Inspect.StateAtDispatch == EParticipantState::Informant);
			TestTrue(FString::Printf(TEXT("%s: reason at dispatch"), Name), Inspect.ReasonAtDispatch == Reason);
			TestEqual(FString::Printf(TEXT("%s: LastEventSeq at dispatch"), Name), Inspect.LastEventSeqAtDispatch, Event.EventSeq);
			TestFalse(FString::Printf(TEXT("%s: no live pawn at dispatch"), Name), Inspect.bLivePawnAtDispatch);
			TestTrue(FString::Printf(TEXT("%s: pawn disposition at dispatch"), Name), Inspect.KindAtDispatch == ExpectedKind);
		}
		F.Core.RemoveListener(&Inspect);
		TestEqual(FString::Printf(TEXT("%s: no fault"), Name), F.Sink.GetRecords().Num(), 0);
	}

	// Edge (QA): the Disconnect case without the generation is Rejected_StaleConnection (R8 step 10).
	{
		FRosterTestFixture F;
		const FParticipantId P = F.BeginAndRegister(FullTeamSize, 1)[0];
		F.Core.SeamSetState(P, EParticipantState::Active, true);
		F.Core.NotifyConnectionChanged(P, ERosterConnection::Disconnected, 1000);
		TestEqual(TEXT("Disconnect without ExpectedConnectionGeneration"),
			F.Core.RequestTransition(P, EParticipantState::Informant, ERosterReason::Disconnect), ERosterResult::Rejected_StaleConnection);
		TestTrue(TEXT("still Active after the rejection"), F.Record(P).State == EParticipantState::Active);
	}
	return true;
}

#endif
