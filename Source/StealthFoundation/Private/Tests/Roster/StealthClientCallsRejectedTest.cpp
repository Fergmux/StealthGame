// Story psr-004 (production/epics/participant-state-roster/story-004-client-calls-rejected.md):
// AC-PSR-02, every Roster call on a client is rejected or answers not-targetable, faults once per call,
// changes nothing, and never returns Npc or a real count (R2, R10).
//
// Per-call expected-result table (resolves the story's QL-STORY-READY gap; also pinned in the
// FRosterCore class comment). One table: later stories that add a call append a row here.
//   every mutator returning ERosterResult (incl. InvalidateAllBindings) -> Rejected_NotAuthority
//   RegisterParticipant                       -> Rejected_NotAuthority, no id
//   ResolveVictimKind                         -> NotTargetable(Stale)
//   IsPawnBound, IsTeamEliminated             -> false
//   CountActive, CountInformant, CountRoster  -> 0
//   GetLegalityClass                          -> Active (the Unbound answer: reveals no binding, R10)
//   GetPawn -> null handle; GetParticipant -> invalid id; GetUnboundActiveParticipants -> empty
// RebindView does not exist in the core yet (C-TeamView push step, story psr-062); it joins the table then.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Tests/Roster/StealthRosterTestFixture.h"

namespace ClientCallsTestData
{
	/** One row: the call, and a check that runs it and answers whether it returned the client answer. */
	struct FClientCallRow
	{
		ERosterCall Call;
		const TCHAR* Name;
		TFunction<bool(FRosterCore&)> RunAndCheck;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRosterClientCallsRejectedTest, "StealthGame.Roster.ClientCallsRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRosterClientCallsRejectedTest::RunTest(const FString& Parameters)
{
	using namespace RosterTestData;
	using namespace ClientCallsTestData;

	// Arrange: a populated store on the server: 2v2, Active, one bound pawn per player, round phase Active.
	FRosterTestFixture F;
	const TArray<FParticipantId> Ids = F.BeginAndRegister(2, 4);
	TestEqual(TEXT("fixture teams assign"), F.AssignSplit(Ids, 2), ERosterResult::Applied);
	for (int32 Index = 0; Index < Ids.Num(); ++Index)
	{
		F.Core.SeamSetState(Ids[Index], EParticipantState::Active);
		TestEqual(TEXT("fixture pawn binds"), F.Core.BindPawn(Ids[Index], Pawn(Index)), ERosterResult::Applied);
	}
	F.RoundPhase.bActive = true;
	const int32 ServerActive0 = F.Core.CountActive(0);
	const int32 ServerRoster1 = F.Core.CountRoster(1);
	TestEqual(TEXT("server sees two Active on team 0"), ServerActive0, 2);
	TestEqual(TEXT("fixture raised no fault"), F.Sink.GetRecords().Num(), 0);
	const FRosterSnapshot Before = F.Core.Snapshot();

	const FParticipantId P0 = Ids[0];
	const FRosterPawnHandle BoundPawn = Pawn(0);
	const FRosterPawnHandle NpcPawn = Pawn(99);
	const FRosterTeamAssignment Swap[] = { { Ids[0], 1 }, { Ids[2], 0 } };

	// The call table (every mutator, every server query of the AC, plus GetPawn/GetParticipant/GetUnbound...).
	const TArray<FClientCallRow> Rows = {
		{ ERosterCall::BeginMatch, TEXT("BeginMatch"), [](FRosterCore& C) { return C.BeginMatch(3) == ERosterResult::Rejected_NotAuthority; } },
		{ ERosterCall::EndMatch, TEXT("EndMatch"), [](FRosterCore& C) { return C.EndMatch() == ERosterResult::Rejected_NotAuthority; } },
		{ ERosterCall::SetTeamSize, TEXT("SetTeamSize"), [](FRosterCore& C) { return C.SetTeamSize(3) == ERosterResult::Rejected_NotAuthority; } },
		{ ERosterCall::RegisterParticipant, TEXT("RegisterParticipant"), [](FRosterCore& C)
			{
				const FRosterRegisterResult R = C.RegisterParticipant(Key(50), TEXT("Late"));
				return R.Result == ERosterResult::Rejected_NotAuthority && !R.Id.IsValid();
			} },
		{ ERosterCall::RemoveParticipant, TEXT("RemoveParticipant"), [P0](FRosterCore& C) { return C.RemoveParticipant(P0) == ERosterResult::Rejected_NotAuthority; } },
		{ ERosterCall::SetTeamsLocked, TEXT("SetTeamsLocked"), [](FRosterCore& C) { return C.SetTeamsLocked(true) == ERosterResult::Rejected_NotAuthority; } },
		{ ERosterCall::AssignTeams, TEXT("AssignTeams"), [&Swap](FRosterCore& C) { return C.AssignTeams(Swap) == ERosterResult::Rejected_NotAuthority; } },
		{ ERosterCall::NotifyConnectionChanged, TEXT("NotifyConnectionChanged"), [P0](FRosterCore& C)
			{ return C.NotifyConnectionChanged(P0, ERosterConnection::Disconnected, 1000) == ERosterResult::Rejected_NotAuthority; } },
		{ ERosterCall::BindPawn, TEXT("BindPawn"), [P0, NpcPawn](FRosterCore& C) { return C.BindPawn(P0, NpcPawn) == ERosterResult::Rejected_NotAuthority; } },
		{ ERosterCall::ReleaseToCrowd, TEXT("ReleaseToCrowd"), [BoundPawn](FRosterCore& C) { return C.ReleaseToCrowd(BoundPawn) == ERosterResult::Rejected_NotAuthority; } },
		{ ERosterCall::ClearBinding, TEXT("ClearBinding"), [BoundPawn](FRosterCore& C)
			{ return C.ClearBinding(BoundPawn, ERosterEndPlayReason::Destroyed) == ERosterResult::Rejected_NotAuthority; } },
		{ ERosterCall::InvalidateAllBindings, TEXT("InvalidateAllBindings"), [](FRosterCore& C) { return C.InvalidateAllBindings() == ERosterResult::Rejected_NotAuthority; } },
		{ ERosterCall::RequestTransition, TEXT("RequestTransition"), [P0](FRosterCore& C)
			{ return C.RequestTransition(P0, EParticipantState::Informant, ERosterReason::Assassinated) == ERosterResult::Rejected_NotAuthority; } },
		{ ERosterCall::ResolveVictimKind, TEXT("ResolveVictimKind(bound)"), [BoundPawn](FRosterCore& C)
			{ return C.ResolveVictimKind(BoundPawn) == FRosterVictimKind::NotTargetable(ERosterNotTargetableReason::Stale); } },
		{ ERosterCall::ResolveVictimKind, TEXT("ResolveVictimKind(unbound)"), [NpcPawn](FRosterCore& C)
			{ return C.ResolveVictimKind(NpcPawn) == FRosterVictimKind::NotTargetable(ERosterNotTargetableReason::Stale); } },
		{ ERosterCall::IsPawnBound, TEXT("IsPawnBound"), [BoundPawn](FRosterCore& C) { return !C.IsPawnBound(BoundPawn); } },
		{ ERosterCall::GetLegalityClass, TEXT("GetLegalityClass"), [BoundPawn](FRosterCore& C) { return C.GetLegalityClass(BoundPawn) == EParticipantState::Active; } },
		{ ERosterCall::CountActive, TEXT("CountActive"), [](FRosterCore& C) { return C.CountActive(0) == 0; } },
		{ ERosterCall::CountInformant, TEXT("CountInformant"), [](FRosterCore& C) { return C.CountInformant(0) == 0; } },
		{ ERosterCall::CountRoster, TEXT("CountRoster"), [](FRosterCore& C) { return C.CountRoster(1) == 0; } },
		{ ERosterCall::IsTeamEliminated, TEXT("IsTeamEliminated"), [](FRosterCore& C) { return !C.IsTeamEliminated(0); } },
		{ ERosterCall::GetPawn, TEXT("GetPawn"), [P0](FRosterCore& C) { return !C.GetPawn(P0).IsSet(); } },
		{ ERosterCall::GetParticipant, TEXT("GetParticipant"), [BoundPawn](FRosterCore& C) { return !C.GetParticipant(BoundPawn).IsValid(); } },
		{ ERosterCall::GetUnboundActiveParticipants, TEXT("GetUnboundActiveParticipants"), [](FRosterCore& C)
			{
				TArray<FParticipantId> Out;
				C.GetUnboundActiveParticipants(Out);
				return Out.Num() == 0;
			} },
	};

	// Every ERosterCall has at least one row, so a call added later without a row fails here.
	for (uint8 Call = 0; Call <= static_cast<uint8>(ERosterCall::GetUnboundActiveParticipants); ++Call)
	{
		TestTrue(FString::Printf(TEXT("call %d has a client row"), Call),
			Rows.ContainsByPredicate([Call](const FClientCallRow& Row) { return static_cast<uint8>(Row.Call) == Call; }));
	}

	// Act: authority false, each call once.
	F.Authority.bAuthority = false;
	for (const FClientCallRow& Row : Rows)
	{
		const int32 FaultsBefore = F.Sink.GetRecords().Num();
		TestTrue(FString::Printf(TEXT("%s returns the client answer"), Row.Name), Row.RunAndCheck(F.Core));
		// Assert: exactly one fault, with the RosterNotAuthority code naming the call.
		if (TestEqual(FString::Printf(TEXT("%s reports exactly one fault"), Row.Name), F.Sink.GetRecords().Num(), FaultsBefore + 1))
		{
			const FStealthFaultRecord& Fault = F.Sink.GetRecords().Last();
			TestTrue(FString::Printf(TEXT("%s fault is RosterNotAuthority"), Row.Name), Fault.Fault == EStealthFault::RosterNotAuthority);
			TestEqual(FString::Printf(TEXT("%s fault names the call"), Row.Name), Fault.Detail, static_cast<int64>(Row.Call));
		}
	}

	// The store is unchanged.
	TestTrue(TEXT("store unchanged after every client call"), F.Core.Snapshot() == Before);

	// Restoring authority shows the original counts (the zeros above were client answers, not state).
	F.Authority.bAuthority = true;
	TestEqual(TEXT("restored: CountActive(0)"), F.Core.CountActive(0), ServerActive0);
	TestEqual(TEXT("restored: CountRoster(1)"), F.Core.CountRoster(1), ServerRoster1);
	TestTrue(TEXT("restored: the bound pawn resolves Player"), F.Core.ResolveVictimKind(BoundPawn) == FRosterVictimKind::Make(ERosterVictimKind::Player));
	TestTrue(TEXT("restored: the unbound pawn resolves Npc"), F.Core.ResolveVictimKind(NpcPawn) == FRosterVictimKind::Make(ERosterVictimKind::Npc));
	return true;
}

#endif
