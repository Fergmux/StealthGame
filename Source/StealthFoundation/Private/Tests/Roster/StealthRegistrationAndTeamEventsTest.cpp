// Story psr-008 (production/epics/participant-state-roster/story-008-registration-and-team-events.md):
// AC-PSR-55, registration, a team batch, a removal and a Dormant reclaim each fire exactly one sequenced
// event consuming one EventSeq; a rejected call of each kind fires none (R4, R5, R12, I6).
// The Dormant record comes from the real Lobby projection (story psr-009, same batch).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Tests/Roster/StealthRosterTestFixture.h"

namespace RegistrationEventsTestData
{
	/** Listener that also records the core's EventSeq at dispatch time: dispatch happens after the call's writes. */
	class FSeqAtDispatchRecorder : public FRosterEventRecorder
	{
	public:
		explicit FSeqAtDispatchRecorder(const FRosterCore& InCore) : Core(InCore) {}
		virtual void OnRosterEvent(const FRosterEvent& Event) override
		{
			FRosterEventRecorder::OnRosterEvent(Event);
			CoreSeqAtDispatch.Add(Core.GetEventSeq());
		}
		const FRosterCore& Core;
		TArray<uint64> CoreSeqAtDispatch;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRosterRegistrationAndTeamEventsTest, "StealthGame.Roster.RegistrationAndTeamEvents",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRosterRegistrationAndTeamEventsTest::RunTest(const FString& Parameters)
{
	using namespace RosterTestData;
	using namespace RegistrationEventsTestData;

	// Arrange: TeamSize 1 (capacity 2) so "roster full" is one registration away.
	FRosterTestFixture F;
	F.Core.BeginMatch(1);
	FSeqAtDispatchRecorder Rec(F.Core);
	F.Core.AddListener(&Rec);

	// Runs Op and asserts it dispatched exactly ExpectedCount events of Type, each consuming exactly one EventSeq.
	const auto ExpectEvents = [&](const TCHAR* What, TFunctionRef<void()> Op, ERosterEventType Type, int32 ExpectedCount)
	{
		const int32 EventsBefore = Rec.Events.Num();
		const uint64 SeqBefore = F.Core.GetEventSeq();
		Op();
		const int32 NewEvents = Rec.Events.Num() - EventsBefore;
		TestEqual(FString::Printf(TEXT("%s: dispatched events"), What), NewEvents, ExpectedCount);
		TestEqual(FString::Printf(TEXT("%s: EventSeq consumed"), What), F.Core.GetEventSeq() - SeqBefore, static_cast<uint64>(ExpectedCount));
		for (int32 Index = EventsBefore; Index < Rec.Events.Num(); ++Index)
		{
			TestTrue(FString::Printf(TEXT("%s: event type"), What), Rec.Events[Index].Type == Type);
			TestEqual(FString::Printf(TEXT("%s: event carries the next EventSeq"), What), Rec.Events[Index].EventSeq, SeqBefore + 1 + (Index - EventsBefore));
		}
	};

	FParticipantId A, B;
	// A key registers: one OnParticipantRegistered.
	ExpectEvents(TEXT("register A"), [&] { A = F.Core.RegisterParticipant(Key(0), TEXT("A")).Id; }, ERosterEventType::ParticipantRegistered, 1);
	TestEqual(TEXT("the registered event names A"), Rec.Events.Last().Id, A);
	ExpectEvents(TEXT("register B"), [&] { B = F.Core.RegisterParticipant(Key(1), TEXT("B")).Id; }, ERosterEventType::ParticipantRegistered, 1);
	// Rejected registration (roster full): none.
	ExpectEvents(TEXT("rejected: roster full"), [&] { TestEqual(TEXT("C is rejected"), F.Core.RegisterParticipant(Key(2), TEXT("C")).Result, ERosterResult::Rejected_RosterFull); },
		ERosterEventType::ParticipantRegistered, 0);

	// A team batch applies: one OnTeamsChanged.
	const FRosterTeamAssignment Teams[] = { { A, 0 }, { B, 1 } };
	ExpectEvents(TEXT("team batch"), [&] { TestEqual(TEXT("batch applies"), F.Core.AssignTeams(Teams), ERosterResult::Applied); }, ERosterEventType::TeamsChanged, 1);
	// Rejected batch (invalid): none (re-verifies story psr-007 against the real queue).
	const FRosterTeamAssignment Invalid[] = { { A, 2 } };
	ExpectEvents(TEXT("rejected: invalid batch"), [&] { TestEqual(TEXT("invalid batch rejected"), F.Core.AssignTeams(Invalid), ERosterResult::Rejected_InvalidBatch); },
		ERosterEventType::TeamsChanged, 0);

	// RemoveParticipant runs: one OnParticipantRemoved.
	ExpectEvents(TEXT("remove B"), [&] { TestEqual(TEXT("B removed"), F.Core.RemoveParticipant(B), ERosterResult::Applied); }, ERosterEventType::ParticipantRemoved, 1);

	// A Dormant key reclaims: one OnParticipantReclaimed (the Lobby departure itself is a connection event).
	ExpectEvents(TEXT("A leaves the Lobby"), [&] { F.Core.NotifyConnectionChanged(A, ERosterConnection::Dormant, 1000); }, ERosterEventType::ConnectionChanged, 1);
	ExpectEvents(TEXT("Dormant A reclaims"), [&]
		{
			const FRosterRegisterResult R = F.Core.RegisterParticipant(Key(0), TEXT("A"));
			TestEqual(TEXT("Dormant key returns Reclaimed"), R.Result, ERosterResult::Reclaimed);
			TestEqual(TEXT("Dormant key returns as itself"), R.Id, A);
		}, ERosterEventType::ParticipantReclaimed, 1);
	TestTrue(TEXT("the Dormant reclaim keeps its team (room under F5)"), F.Record(A).Team == ERosterTeam::Team0);

	// Locked rejections: removal and Dormant reclaim fire none.
	ExpectEvents(TEXT("A connects"), [&] { F.Core.NotifyConnectionChanged(A, ERosterConnection::Connected, 1100); }, ERosterEventType::ConnectionChanged, 1);
	FParticipantId D;
	ExpectEvents(TEXT("register D"), [&] { D = F.Core.RegisterParticipant(Key(3), TEXT("D")).Id; }, ERosterEventType::ParticipantRegistered, 1);
	const FRosterTeamAssignment DTeam[] = { { D, 1 } };
	ExpectEvents(TEXT("D assigned"), [&] { F.Core.AssignTeams(DTeam); }, ERosterEventType::TeamsChanged, 1);
	ExpectEvents(TEXT("A Dormant again"), [&] { F.Core.NotifyConnectionChanged(A, ERosterConnection::Dormant, 1200); }, ERosterEventType::ConnectionChanged, 1);
	TestEqual(TEXT("teams lock"), F.Core.SetTeamsLocked(true), ERosterResult::Applied);
	ExpectEvents(TEXT("rejected: locked removal"), [&] { TestEqual(TEXT("locked removal"), F.Core.RemoveParticipant(D), ERosterResult::Rejected_TeamsLocked); },
		ERosterEventType::ParticipantRemoved, 0);
	ExpectEvents(TEXT("rejected: locked Dormant reclaim"), [&] { TestEqual(TEXT("locked Dormant reclaim"), F.Core.RegisterParticipant(Key(0), TEXT("A")).Result, ERosterResult::Rejected_TeamsLocked); },
		ERosterEventType::ParticipantReclaimed, 0);

	// Every dispatched EventSeq is contiguous from 1, and each event was dispatched after its call's writes.
	for (int32 Index = 0; Index < Rec.Events.Num(); ++Index)
	{
		TestEqual(FString::Printf(TEXT("event %d EventSeq contiguous"), Index), Rec.Events[Index].EventSeq, static_cast<uint64>(Index + 1));
		TestTrue(FString::Printf(TEXT("event %d dispatched after its call queued it"), Index), Rec.CoreSeqAtDispatch[Index] >= Rec.Events[Index].EventSeq);
	}
	F.Core.RemoveListener(&Rec);
	return true;
}

#endif
