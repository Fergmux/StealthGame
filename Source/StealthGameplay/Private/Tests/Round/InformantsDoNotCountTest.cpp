// Story rmf-048 (production/epics/round-match-framework/story-048-informants-do-not-count.md), GDD AC 19 / R15, F2:
// team 1 = {1 Active, 3 Informants}. (1) An informant is killed (VictimKind Informant: KillResolved logged, no lifecycle
// transition) and (2) an Informant -> Informant request answers NoOp_AlreadyInState (no event). For both: Round's
// elimination evaluation is not entered (no StateTransition reaches F2, TryTerminate never called), no WinLatched, and
// team 1's Active count stays 1. Only Active -> Informant can zero a team.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "StealthRoundEndRig.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoundInformantsDoNotCountTest, "StealthGame.Round.InformantsDoNotCount",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRoundInformantsDoNotCountTest::RunTest(const FString& Parameters)
{
	using namespace RoundTestFixtures;
	// Arrange: 4v4 (ids 1-4 team 0, 5-8 team 1); team 1's 6, 7, 8 are Informants (fixture states, set before any item).
	FRoundEndRig R;
	R.ToActive(4, 4);
	for (const uint32 Informant : { 6u, 7u, 8u })
	{
		R.Rig.Fakes.Roster.SetState(FParticipantId(Informant), EParticipantState::Informant);
	}
	TestEqual(TEXT("fixture: team 1 has 1 Active"), R.ActiveOn(ERosterTeam::Team1), 1);
	const int32 TransitionsBefore = R.CountLogged(ERoundLogType::StateTransition);

	// Act 1: an informant kill resolved in a slot (the stub writes KillResolved, VictimKind Informant; no transition).
	R.Strikes.OnResolve = [&R](const FQueueItem& Item, FQueueKey Key)
	{
		FRoundLogEntry Kill;
		Kill.Type = ERoundLogType::KillResolved;
		Kill.Key = Key;
		Kill.Code = static_cast<uint16>(ERoundKillResult::Other);
		Kill.Participants[0] = 1;
		Kill.Participants[1] = 6;
		Kill.Data[0] = 0;
		R.Rig.Core->GetRoundLogWriter()->Append(Kill);
		return EResolveResult::Executed;
	};
	R.DrainOneTick({ R.Support.MakeItem(EQueueItemType::PawnCommand, 1) });

	// Assert 1.
	TestEqual(TEXT("informant kill: KillResolved logged"), R.CountLogged(ERoundLogType::KillResolved), 1);
	TestEqual(TEXT("informant kill: no transition reached the F2 check"), R.CountLogged(ERoundLogType::StateTransition), TransitionsBefore);
	TestEqual(TEXT("informant kill: TryTerminate never called"), R.Rig.Core->GetLatch().GetSuppressedCount(), 0);
	TestFalse(TEXT("informant kill: not terminated"), R.Rig.Core->IsTerminated());
	TestEqual(TEXT("informant kill: no WinLatched"), R.CountLogged(ERoundLogType::WinLatched), 0);
	TestEqual(TEXT("informant kill: team 1 Active count stays 1"), R.ActiveOn(ERosterTeam::Team1), 1);

	// Act 2: an Informant -> Informant request through the Roster: NoOp_AlreadyInState emits no event.
	R.Rig.Fakes.Roster.Script(TEXT("RequestTransition"), static_cast<int64>(ERosterResult::NoOp_AlreadyInState));
	const ERosterResult NoOp = R.Rig.Fakes.Roster.RequestTransition(FParticipantId(7), EParticipantState::Informant, ERosterReason::Assassinated,
		R.Rig.Core->GetPreparing().GetSpawnEpoch(), TOptional<uint32>());

	// Assert 2.
	TestTrue(TEXT("Informant -> Informant answers NoOp_AlreadyInState"), NoOp == ERosterResult::NoOp_AlreadyInState);
	TestEqual(TEXT("no-op: no transition reached the F2 check"), R.CountLogged(ERoundLogType::StateTransition), TransitionsBefore);
	TestEqual(TEXT("no-op: TryTerminate never called"), R.Rig.Core->GetLatch().GetSuppressedCount(), 0);
	TestEqual(TEXT("no-op: no WinLatched"), R.CountLogged(ERoundLogType::WinLatched), 0);
	TestEqual(TEXT("no-op: team 1 Active count stays 1"), R.ActiveOn(ERosterTeam::Team1), 1);
	TestTrue(TEXT("still Active"), R.Rig.Core->GetPhase() == ERoundPhase::Active);
	TestTrue(TEXT("no unexpected call"), R.Rig.Log.Unexpected.IsEmpty());
	return true;
}

#endif
