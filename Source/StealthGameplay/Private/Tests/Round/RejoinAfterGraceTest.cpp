// Story rmf-062 (production/epics/round-match-framework/story-062-rejoin-after-grace.md), GDD AC 23b / R18, R17.4:
// a rejoin after the grace expiry, within the window, during Active: the participant is (and stays) an Informant, Round
// calls IInformantBodyProvider::OnInformantReadmitted(P) exactly once, after the acknowledgement and inside its own slot
// (a ConnectionTransition item), and Penalty's C1 takeover re-pawns it: the fake chain records one NPC deregistered,
// BindPawn, then RecordAppearance with that pawn's card, and the pawn count is unchanged. The Penalty stand-in executes
// the PEN-R9 order against the fakes; the real chain is the Penalty, Level Socket and Shared Character epics' to test.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "StealthRoundEndRig.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoundRejoinAfterGraceTest, "StealthGame.Round.RejoinAfterGrace",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRoundRejoinAfterGraceTest::RunTest(const FString& Parameters)
{
	using namespace RoundTestFixtures;
	// Arrange: 2v2; participant 2 disconnects and converts at the expiry (its pawn is released to the crowd).
	FRoundEndRig R;
	R.ToActive(2, 2);
	const uint32 Away = 2;
	int32 Npcs = PreparingRigNpcCount + 1; // the released pawn joined the crowd
	int32 Bound = R.Rig.Fakes.Roster.Members.Num() - 1;
	const int32 TotalBefore = Npcs + Bound;
	R.Disconnect(Away);
	const double Expiry = R.Rig.Core->GetGraceTable().Find(FParticipantId(Away))->GraceExpiry;
	R.Rig.Fakes.Roster.Script(TEXT("RequestTransition"), static_cast<int64>(ERosterResult::Applied));
	R.Rig.Fakes.Roster.Script(TEXT("ReleaseToCrowd"), static_cast<int64>(ERosterResult::Applied));
	R.RunUntil(Expiry);
	TestTrue(TEXT("converted at the expiry"), R.Member(Away)->State == EParticipantState::Informant);

	// The Penalty stand-in: PEN-R9's order on the fakes (PickRespawnPawn, Crowd release, TakeOverCrowdPawn -> BindPawn, card).
	int32 Readmitted = 0;
	bool bInSlot = false;
	R.Rig.Fakes.InformantBodies.OnReadmitted = [&](FParticipantId Participant)
	{
		++Readmitted;
		bInSlot = R.Queue->IsInSlot();
		R.Rig.Log.Calls.Add(FRoundFakeCall{ ERoundFakeGateway::Crowd, TEXT("DeregisterNpc"), { FixtureTakenNpcCard } });
		--Npcs;
		if (R.Rig.Fakes.Roster.BindPawn(Participant, FPawnHandle()) == ERosterResult::Applied)
		{
			++Bound;
			R.Rig.Fakes.Roster.RecordAppearance(Participant, FixtureTakenNpcCard);
		}
	};
	R.Rig.Fakes.Roster.Script(TEXT("BindPawn"), static_cast<int64>(ERosterResult::Applied));
	R.Rig.Fakes.Roster.Script(TEXT("RecordAppearance"), static_cast<int64>(ERosterResult::Applied));
	const int32 Mark = R.Rig.Mark();

	// Act: the rejoin completes inside the window (after Session's acknowledgement); the next drain runs the slot.
	R.Rig.Clock.AdvanceSeconds(RejoinBeforeExpirySec);
	R.Rejoin(Away);
	TestEqual(TEXT("not called before Round's slot"), Readmitted, 0);
	R.Frame();

	// Assert.
	TestTrue(TEXT("ShouldAdmitExpired: InRound in Active"), R.Rig.Core->ShouldAdmitExpired(FParticipantId(Away)));
	TestTrue(TEXT("still an Informant (reconnecting never changes state)"), R.Member(Away)->State == EParticipantState::Informant);
	TestEqual(TEXT("OnInformantReadmitted exactly once"), Readmitted, 1);
	TestTrue(TEXT("inside Round's slot"), bInSlot);
	const int32 Deregister = R.IndexOf(TEXT("DeregisterNpc"), Mark);
	const int32 Bind = R.IndexOf(TEXT("BindPawn"), Mark);
	const int32 Card = R.IndexOf(TEXT("RecordAppearance"), Mark);
	TestTrue(TEXT("one NPC deregistered, then BindPawn, then RecordAppearance"), Deregister != INDEX_NONE && Deregister < Bind && Bind < Card);
	TestTrue(TEXT("RecordAppearance with the taken pawn's card"), Card != INDEX_NONE && R.Rig.Log.Calls[Card].Args[1] == FixtureTakenNpcCard);
	TestEqual(TEXT("the pawn count is unchanged"), Npcs + Bound, TotalBefore);
	TestEqual(TEXT("no spawn"), R.Rig.Fakes.Spawner.Spawns.Num(), R.Rig.Fakes.Roster.Members.Num() + PreparingRigNpcCount);
	TestTrue(TEXT("no unexpected call"), R.Rig.Log.Unexpected.IsEmpty());
	return true;
}

#endif
