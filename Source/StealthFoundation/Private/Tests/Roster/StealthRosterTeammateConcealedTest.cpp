// Concealment story con-023 (production/epics/concealment-bushes/story-023-owner-and-team-concealed-flags.md; ADR-0001
// section 5, ADR-0017 section 1), StealthGame.Roster.TeammateConcealedTeamOnly: SetTeammateConcealed(P, true) marks P's entry
// in its TEAMMATES' views only (the owner's own view and every opponent's are not rewritten), the push step stays the only
// writer (one flush writes the team's other views), the same value is NoOp_Unchanged, false clears it, and a Roster reset
// clears every flag.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Roster/StealthRosterPushStep.h"
#include "Tests/Roster/StealthRosterTestFixture.h"
#include "View/StealthRosterTeamView.h"

namespace RosterTeammateConcealedTestData
{
	/** P's entry in View, or null. */
	const FTeamViewTeammateEntry* EntryOf(const URosterTeamViewComponent* View, uint32 Handle)
	{
		return View->GetView().Teammates.FindByPredicate([Handle](const FTeamViewTeammateEntry& E) { return E.Handle == Handle; });
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRosterTeammateConcealedTest, "StealthGame.Roster.TeammateConcealedTeamOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRosterTeammateConcealedTest::RunTest(const FString& Parameters)
{
	using namespace RosterTeammateConcealedTestData;
	// Arrange: 3v3, one view per participant.
	FRosterTestFixture F;
	const TArray<FParticipantId> Ids = F.BeginAndRegister(3, 6);
	F.AssignSplit(Ids, 3);
	F.Core.SetTeamsLocked(true);
	F.Core.ResetAll(1);
	FRosterPushStep Push(F.Core, F.RoundPhase, FRosterPushStep::FPawnResolver());
	F.Core.AddListener(&Push);
	TArray<URosterTeamViewComponent*> Views;
	for (const FParticipantId& Id : Ids)
	{
		URosterTeamViewComponent* View = NewObject<URosterTeamViewComponent>(GetTransientPackage());
		View->AddToRoot();
		Views.Add(View);
		Push.RegisterView(Id, View);
	}
	Push.Flush();
	TArray<int32> Before;
	for (const URosterTeamViewComponent* View : Views)
	{
		Before.Add(View->GetServerWriteCount());
	}
	const uint32 Handle = Views[0]->GetView().Owner.Handle;

	// Act: participant 0 (team 0) is concealed.
	TestTrue(TEXT("SetTeammateConcealed applies"), Push.SetTeammateConcealed(Ids[0], true) == ERosterResult::Applied);
	TestEqual(TEXT("one flush writes the two teammates' views"), Push.Flush(), 2);
	for (int32 Index = 1; Index < 3; ++Index)
	{
		const FTeamViewTeammateEntry* Entry = EntryOf(Views[Index], Handle);
		TestTrue(FString::Printf(TEXT("teammate %d sees bIsConcealed"), Index), Entry != nullptr && Entry->bIsConcealed);
	}
	TestEqual(TEXT("the owner's own view is not rewritten (its flag is the owner component's)"), Views[0]->GetServerWriteCount(), Before[0]);
	for (int32 Index = 3; Index < 6; ++Index)
	{
		TestEqual(FString::Printf(TEXT("opponent %d's view is not rewritten"), Index), Views[Index]->GetServerWriteCount(), Before[Index]);
		TestNull(FString::Printf(TEXT("opponent %d has no entry for the hider"), Index), EntryOf(Views[Index], Handle));
	}
	TestTrue(TEXT("the same value is NoOp_Unchanged"), Push.SetTeammateConcealed(Ids[0], true) == ERosterResult::NoOp_Unchanged);
	TestTrue(TEXT("the store reports it"), Push.IsTeammateConcealed(Ids[0]));

	// false clears it.
	TestTrue(TEXT("clearing applies"), Push.SetTeammateConcealed(Ids[0], false) == ERosterResult::Applied);
	Push.Flush();
	const FTeamViewTeammateEntry* Cleared = EntryOf(Views[1], Handle);
	TestTrue(TEXT("the crossing clears the teammate flag"), Cleared != nullptr && !Cleared->bIsConcealed);

	// A reset clears every flag.
	Push.SetTeammateConcealed(Ids[1], true);
	Push.Flush();
	F.Core.ResetAll(2);
	Push.Flush();
	TestFalse(TEXT("a Roster reset clears the flag"), Push.IsTeammateConcealed(Ids[1]));
	const FTeamViewTeammateEntry* AfterReset = EntryOf(Views[0], Views[1]->GetView().Owner.Handle);
	TestTrue(TEXT("... in the views too"), AfterReset != nullptr && !AfterReset->bIsConcealed);
	TestTrue(TEXT("an unknown participant is rejected"), Push.SetTeammateConcealed(FParticipantId(9999), true) == ERosterResult::Rejected_UnknownParticipant);

	F.Core.RemoveListener(&Push);
	for (URosterTeamViewComponent* View : Views)
	{
		View->RemoveFromRoot();
	}
	return true;
}

#endif
