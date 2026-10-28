// Story oc-012 (production/epics/objectives-chain/story-012-rejoin-rebind-and-reset.md), AC2 (ADR-0020 section 6, ADR-0005
// RebindView): StealthGame.Objectives.ViewReboundWritesTeamView. A bound UObjectivesService subscribed to a REAL URosterStore
// (2v2; the store's RegisterTeamView raises OnParticipantViewRebound):
//   - an event before Active entry writes the empty default (no component changes; write count 0);
//   - after Active entry a rejoin (a new controller's component, then the store's rebind event) writes that participant's team
//     view inline in the handler (already in place when RegisterTeamView returns), equal to its teammate's; no other component
//     is written; an event for a participant of the other team writes only that participant, with its own team's view;
//   - after Unbind the subscription is gone (a rebind writes nothing);
//   - seamless travel (a fresh world and service, new controllers rebound before Active): the view write count is 0 and every
//     new controller's view is the empty default until Active entry, which then writes each once.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "ObjectivesQueueRig.h"
#include "ObjectivesRosterFixture.h"

namespace ObjectivesViewReboundTestData
{
	/** Makes Rig's stubs know the fixture's four participants and subscribes the service to the fixture's store. */
	void Wire(StealthObjectivesTest::FQueueRig& Rig, StealthObjectivesTest::FRosterStoreFixture& Roster)
	{
		for (int32 Index = 0; Index < StealthObjectivesTest::FRosterStoreFixture::Players; ++Index)
		{
			const uint32 Id = Roster.Ids[Index].Value;
			Rig.Roster.Teams.Add(Id, Roster.TeamOf(Index));
			Rig.Roster.States.Add(Id, EParticipantState::Active);
			Rig.ViewTargets.Add(Id, Roster.TeamOf(Index));
		}
		Rig.Init.RosterEvents = Roster.Store;
		Rig.Service->Bind(Rig.Init);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectivesViewReboundWritesTeamViewTest, "StealthGame.Objectives.ViewReboundWritesTeamView",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FObjectivesViewReboundWritesTeamViewTest::RunTest(const FString& Parameters)
{
	using namespace StealthObjectivesTest;
	using namespace ObjectivesViewReboundTestData;

	FRosterStoreFixture Roster;
	{
		// Arrange.
		FQueueRig Rig;
		Wire(Rig, Roster);
		const uint32 A0 = Roster.Ids[0].Value, A1 = Roster.Ids[1].Value, B0 = Roster.Ids[2].Value, B1 = Roster.Ids[3].Value;

		// An event before Active writes the empty default.
		Roster.Rebind(0);
		TestEqual(TEXT("before Active: the rebind writes nothing (empty default already in place)"), Rig.Service->GetViewWriteCount(), 0);
		TestTrue(TEXT("before Active: the view is the empty default"), Rig.ViewTargets.ViewOf(A0)->GetView().IsEmpty());

		TestTrue(TEXT("to Active"), Rig.ToActive());
		TestEqual(TEXT("Active entry wrote each of the four components"), Rig.Service->GetViewWriteCount(), 4);

		// A rejoin of A1 inside the round: a new controller, then the store's rebind event.
		UObjectiveTeamViewComponent* NewA1 = Rig.ViewTargets.Replace(A1);
		Roster.Rebind(1);
		TestEqual(TEXT("rejoin: the new controller's view was written inline, once"), NewA1->GetServerWriteCount(), 1);
		TestTrue(TEXT("rejoin: the rejoiner's view equals its teammate's"), NewA1->GetView() == Rig.ViewTargets.ViewOf(A0)->GetView());
		TestFalse(TEXT("rejoin: the view is not the empty default"), NewA1->GetView().IsEmpty());
		TestEqual(TEXT("rejoin: exactly one write in all"), Rig.Service->GetViewWriteCount(), 5);
		for (const uint32 Other : { A0, B0, B1 })
		{
			TestEqual(FString::Printf(TEXT("rejoin: participant %u's component untouched"), Other), Rig.ViewTargets.ViewOf(Other)->GetServerWriteCount(), 1);
		}

		// An event for a participant of the other team writes only that participant, with its own team's view.
		UObjectiveTeamViewComponent* NewB1 = Rig.ViewTargets.Replace(B1);
		Roster.Rebind(3);
		TestEqual(TEXT("other team: only that participant written"), Rig.Service->GetViewWriteCount(), 6);
		TestTrue(TEXT("other team: its own team's view"), NewB1->GetView() == Rig.ViewTargets.ViewOf(B0)->GetView());
		TestTrue(TEXT("other team: never team 0's view"), NewB1->GetView() != Rig.ViewTargets.ViewOf(A0)->GetView());

		// After Unbind the subscription is gone.
		Rig.Service->Unbind();
		UObjectiveTeamViewComponent* AfterUnbind = Rig.ViewTargets.Replace(A0);
		Roster.Rebind(0);
		TestEqual(TEXT("unbound: a rebind writes nothing (unsubscribed)"), AfterUnbind->GetServerWriteCount(), 0);
	}
	{
		// Seamless travel: a fresh world and service; every controller is new and rebinds before Active entry.
		FQueueRig Rig;
		Wire(Rig, Roster);
		for (int32 Index = 0; Index < FRosterStoreFixture::Players; ++Index)
		{
			Roster.Rebind(Index);
		}
		TestEqual(TEXT("travel: the view write count is 0"), Rig.Service->GetViewWriteCount(), 0);
		for (int32 Index = 0; Index < FRosterStoreFixture::Players; ++Index)
		{
			TestTrue(FString::Printf(TEXT("travel: controller %d holds the empty default"), Index), Rig.ViewTargets.ViewOf(Roster.Ids[Index].Value)->GetView().IsEmpty());
		}
		TestTrue(TEXT("travel: to Active"), Rig.ToActive());
		TestEqual(TEXT("travel: Active entry writes each new controller once"), Rig.Service->GetViewWriteCount(), FRosterStoreFixture::Players);
	}
	TestEqual(TEXT("the store raised no fault"), Roster.Sink.GetRecords().Num(), 0);
	return true;
}

#endif
