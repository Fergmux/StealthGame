// Story psr-009 (production/epics/participant-state-roster/story-009-connection-projection.md):
// AC-PSR-46, Session connection states project onto the Connection fields only (R4, R11).
//
// Gap resolution (recorded in the story): the Roster takes the 3-value projection, not Session's
// ConnectionState; the R11 mapping table is Session's adapter responsibility and is held here only as test
// data that drives NotifyConnectionChanged with each row's projection. "Exactly one OnConnectionChanged"
// applies to a notify that changes the projection; a notify that changes nothing (Disconnected ->
// Disconnected for Rejoining/Expired/failed rejoin, Connected -> Connected for Unresponsive/Traveling) is
// NoOp_Unchanged, emits nothing and consumes no EventSeq (I6 counts only emitted events). Session's
// Connecting (no record yet) and Rejected (unchanged) rows send no notify. Decision D45: the generation
// bumps only on entry to Disconnected.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Tests/Roster/StealthRosterTestFixture.h"

namespace ConnectionProjectionTestData
{
	/** R11 mapping table rows, as test data (Session state name -> Roster projection). */
	struct FSessionRow
	{
		const TCHAR* SessionState;
		ERosterConnection Projection;
		int64 AtMs;
		bool bKicked;
	};

	/** A walk through every Session state that sends a notify, in an order that exercises each move. */
	const FSessionRow Walk[] = {
		{ TEXT("Connected"), ERosterConnection::Connected, 1000, false },
		{ TEXT("Unresponsive"), ERosterConnection::Connected, 2000, false },
		{ TEXT("Disconnected"), ERosterConnection::Disconnected, 3000, false },
		{ TEXT("Rejoining"), ERosterConnection::Disconnected, 4000, false },
		{ TEXT("Expired"), ERosterConnection::Disconnected, 5000, false },
		{ TEXT("Connected (rejoined)"), ERosterConnection::Connected, 6000, false },
		{ TEXT("Traveling"), ERosterConnection::Connected, 7000, false },
		{ TEXT("Disconnected (supersede, D24)"), ERosterConnection::Disconnected, 8000, false },
		{ TEXT("Rejoining (failed supersede-rejoin)"), ERosterConnection::Disconnected, 9000, false },
		{ TEXT("Connected (rejoined)"), ERosterConnection::Connected, 9500, false },
		{ TEXT("Disconnected (Kicked)"), ERosterConnection::Disconnected, 10000, true },
		{ TEXT("Expired (kick path)"), ERosterConnection::Disconnected, 10000, true },
		{ TEXT("Dormant"), ERosterConnection::Dormant, 11000, false },
		{ TEXT("Disconnected (from Dormant)"), ERosterConnection::Disconnected, 12000, false },
	};

	/** The record with the three Connection fields masked out, to prove nothing else changed. */
	FParticipantRecord WithoutConnection(FParticipantRecord Record)
	{
		Record.Connection = FRosterConnection();
		return Record;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRosterConnectionProjectionTest, "StealthGame.Roster.ConnectionProjection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRosterConnectionProjectionTest::RunTest(const FString& Parameters)
{
	using namespace RosterTestData;
	using namespace ConnectionProjectionTestData;

	// Part 1: each row's projection changes only Connection fields.
	{
		// Arrange: an Active record with a bound pawn and a team, Connected.
		FRosterTestFixture F;
		const TArray<FParticipantId> Ids = F.BeginAndRegister(FullTeamSize, 2);
		F.AssignSplit(Ids, 1);
		F.Core.SeamSetState(Ids[0], EParticipantState::Active);
		F.Core.BindPawn(Ids[0], Pawn(1));
		FRosterEventRecorder Rec;
		F.Core.AddListener(&Rec);
		const FParticipantId Id = Ids[0];
		const FRosterSnapshot OtherBefore = F.Core.Snapshot();

		for (const FSessionRow& Row : Walk)
		{
			const FParticipantRecord Before = F.Record(Id);
			const uint64 SeqBefore = F.Core.GetEventSeq();
			const int32 EventsBefore = Rec.Events.Num();
			const bool bChanges = Before.Connection.Projection != Row.Projection;

			// Act
			const ERosterResult Result = F.Core.NotifyConnectionChanged(Id, Row.Projection, Row.AtMs, Row.bKicked);
			const FParticipantRecord After = F.Record(Id);

			// Assert
			TestEqual(FString::Printf(TEXT("%s: result"), Row.SessionState), Result, bChanges ? ERosterResult::Applied : ERosterResult::NoOp_Unchanged);
			TestTrue(FString::Printf(TEXT("%s: projection"), Row.SessionState), After.Connection.Projection == Row.Projection);
			TestTrue(FString::Printf(TEXT("%s: only Connection fields change"), Row.SessionState), WithoutConnection(After) == WithoutConnection(Before));
			TestTrue(FString::Printf(TEXT("%s: lifecycle state unchanged"), Row.SessionState), After.State == EParticipantState::Active);
			TestTrue(FString::Printf(TEXT("%s: live pawn unchanged"), Row.SessionState), After.PawnBinding == Pawn(1));

			const bool bEntersDisconnected = bChanges && Row.Projection == ERosterConnection::Disconnected;
			TestEqual(FString::Printf(TEXT("%s: generation +1 only on entry to Disconnected"), Row.SessionState),
				After.Connection.ConnectionGeneration, Before.Connection.ConnectionGeneration + (bEntersDisconnected ? 1u : 0u));
			TestEqual(FString::Printf(TEXT("%s: DisconnectedAt written only on entry to Disconnected"), Row.SessionState),
				After.Connection.DisconnectedAtMs, bEntersDisconnected ? Row.AtMs : Before.Connection.DisconnectedAtMs);

			const int32 ExpectedEvents = bChanges ? 1 : 0;
			TestEqual(FString::Printf(TEXT("%s: OnConnectionChanged count"), Row.SessionState), Rec.Events.Num() - EventsBefore, ExpectedEvents);
			TestEqual(FString::Printf(TEXT("%s: EventSeq consumed"), Row.SessionState), F.Core.GetEventSeq() - SeqBefore, static_cast<uint64>(ExpectedEvents));
			if (bChanges && Rec.Events.Num() > EventsBefore)
			{
				const FRosterEvent& Event = Rec.Events.Last();
				TestTrue(FString::Printf(TEXT("%s: event type"), Row.SessionState), Event.Type == ERosterEventType::ConnectionChanged);
				TestEqual(FString::Printf(TEXT("%s: event id"), Row.SessionState), Event.Id, Id);
				TestTrue(FString::Printf(TEXT("%s: event projection"), Row.SessionState), Event.Connection == Row.Projection);
			}
		}

		// Three entries into Disconnected in the walk (drop, supersede, kick) plus one from Dormant: generation 4.
		TestEqual(TEXT("generation counts the entries into Disconnected only"), F.Record(Id).Connection.ConnectionGeneration, 4u);
		// The other participant was never touched.
		TestTrue(TEXT("the other record is unchanged"), F.Record(Ids[1]) == OtherBefore.Records[1].GetValue());
		F.Core.RemoveListener(&Rec);
	}

	// Part 2: a Lobby departure sets Dormant, keeps the id, and Rejected_RosterFull ignores it.
	{
		FRosterTestFixture F;
		const TArray<FParticipantId> Ids = F.BeginAndRegister(1, 2);
		TestEqual(TEXT("C is full before the departure"), F.Core.RegisterParticipant(Key(2), TEXT("C")).Result, ERosterResult::Rejected_RosterFull);
		TestEqual(TEXT("B leaves the Lobby"), F.Core.NotifyConnectionChanged(Ids[1], ERosterConnection::Dormant, 500), ERosterResult::Applied);
		const FParticipantRecord B = F.Record(Ids[1]);
		TestTrue(TEXT("B is Dormant"), B.Connection.Projection == ERosterConnection::Dormant);
		TestEqual(TEXT("B keeps its id"), B.ParticipantId, Ids[1]);
		TestTrue(TEXT("B's record still exists"), F.Core.FindRecord(Ids[1]) != nullptr);
		const FRosterRegisterResult C = F.Core.RegisterParticipant(Key(2), TEXT("C"));
		TestEqual(TEXT("C registers: the Dormant record holds no slot"), C.Result, ERosterResult::Registered);
		TestEqual(TEXT("D is full again"), F.Core.RegisterParticipant(Key(3), TEXT("D")).Result, ERosterResult::Rejected_RosterFull);
	}
	return true;
}

#endif
