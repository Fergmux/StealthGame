// NPC-Kill Penalty story nkp-011 (production/epics/npc-kill-penalty-informants/story-011-t-due-owner-carrier-adapter.md),
// AC3's Roster half (ADR-0019 section 6 option (a); ADR-0001 C-TeamView owner part; the respawn-due half of roster story
// psr-038 built early): GIVEN a 3v3 push step with one view per participant, WHEN the carrier writes
// SetOwnerRespawnDue(P, 115.0, epoch), THEN only P's own view is rewritten and only its owner part carries T_due = 115.0
// (every other view's owner part reads 0 and no teammate entry has a T_due field at all); a stale epoch is
// Rejected_StaleEpoch with no write; an unchanged value is NoOp_Unchanged with no write; 0 clears it; and a Roster reset
// clears every T_due. The owner-only replication of the component itself (COND_OwnerOnly) is psr-062's, and the wire
// capture on every connection is the PIE run of the story's evidence.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Roster/StealthRosterPushStep.h"
#include "Tests/Roster/StealthRosterTestFixture.h"
#include "View/StealthRosterTeamView.h"

namespace RosterOwnerRespawnDueTestData
{
	inline constexpr double DueServerTime = 115.0;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRosterOwnerRespawnDueOwnerOnlyTest, "StealthGame.Roster.OwnerRespawnDueOwnerOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRosterOwnerRespawnDueOwnerOnlyTest::RunTest(const FString& Parameters)
{
	using namespace RosterOwnerRespawnDueTestData;

	// Arrange: a 3v3 core fixture (0,1,2 | 3,4,5), everyone Active, a push step with one view per participant.
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
	TArray<int32> WritesBefore;
	for (const URosterTeamViewComponent* View : Views)
	{
		WritesBefore.Add(View->GetServerWriteCount());
	}
	const uint32 Epoch = F.Core.GetRoundEpoch();

	// Act + Assert: the owner's write.
	TestTrue(TEXT("SetOwnerRespawnDue applies"), Push.SetOwnerRespawnDue(Ids[0], DueServerTime, Epoch) == ERosterResult::Applied);
	TestEqual(TEXT("the next flush writes only the owner's view"), Push.Flush(), 1);
	TestEqual(TEXT("the owner part carries T_due"), Views[0]->GetView().Owner.T_due, DueServerTime);
	for (int32 Index = 1; Index < Views.Num(); ++Index)
	{
		TestEqual(FString::Printf(TEXT("view %d (ally or opponent) is not rewritten"), Index), Views[Index]->GetServerWriteCount(), WritesBefore[Index]);
		TestEqual(FString::Printf(TEXT("view %d's owner part has no T_due"), Index), Views[Index]->GetView().Owner.T_due, 0.0);
	}

	// A stale epoch, an unchanged value: no write.
	TestTrue(TEXT("a stale epoch is Rejected_StaleEpoch"), Push.SetOwnerRespawnDue(Ids[0], DueServerTime + 1.0, Epoch + 1) == ERosterResult::Rejected_StaleEpoch);
	TestTrue(TEXT("the same value is NoOp_Unchanged"), Push.SetOwnerRespawnDue(Ids[0], DueServerTime, Epoch) == ERosterResult::NoOp_Unchanged);
	TestEqual(TEXT("neither writes a view"), Push.Flush(), 0);

	// 0 clears.
	TestTrue(TEXT("0 clears"), Push.SetOwnerRespawnDue(Ids[0], 0.0, Epoch) == ERosterResult::Applied);
	Push.Flush();
	TestEqual(TEXT("the owner part reads 0 again"), Views[0]->GetView().Owner.T_due, 0.0);

	// A reset clears every T_due.
	Push.SetOwnerRespawnDue(Ids[3], DueServerTime, Epoch);
	Push.Flush();
	TestEqual(TEXT("fixture: an opponent's own T_due"), Views[3]->GetView().Owner.T_due, DueServerTime);
	F.Core.ResetAll(2);
	Push.Flush();
	TestEqual(TEXT("a Roster reset clears T_due"), Views[3]->GetView().Owner.T_due, 0.0);

	F.Core.RemoveListener(&Push);
	for (URosterTeamViewComponent* View : Views)
	{
		View->RemoveFromRoot();
	}
	return true;
}

#endif
