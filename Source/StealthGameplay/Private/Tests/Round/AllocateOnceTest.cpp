// Story rmf-031 (production/epics/round-match-framework/story-031-allocate-once.md), GDD AC 8a / R6 P2-P3:
//   - in Preparing with tuning withheld (the fake TDC never raises OnTuningReady), AllocateAppearances is called 0 times
//     however many drains run on the fake clock; there is no layout request either;
//   - after OnTuningReady it is called exactly once, with every rostered participant (connected or not) and NpcCount;
//   - repeated drains and a duplicate OnTuningReady do not call it again.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "StealthRoundExitBTestRig.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoundAllocateOnceTest, "StealthGame.Round.AllocateOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRoundAllocateOnceTest::RunTest(const FString& Parameters)
{
	using namespace RoundTestFixtures;

	FRoundExitBRig Rig;
	Rig.OpenLobby();
	Rig.SetLayout(Layout2v2.Team0, Layout2v2.Team1);
	Rig.EnterPreparing();
	// One participant drops during the load: Players for the deal is every rostered id all the same.
	Rig.Fakes.Session.ConnectionStates[FRoundExitBRig::HostIdValue + 3] = ERoundConnectionState::Disconnected;
	TestTrue(TEXT("the round is in Preparing"), Rig.Core->GetPhase() == ERoundPhase::Preparing);
	TestEqual(TEXT("Preparing entered was signalled to TDC once"), Rig.CountSince(0, TEXT("SignalPreparingEntered")), 1);

	// Tuning withheld: drains on the fake clock make no deal and no layout request.
	for (int32 Frame = 0; Frame < 120; ++Frame)
	{
		Rig.Frame();
	}
	TestEqual(TEXT("AllocateAppearances 0 times while tuning is withheld"), Rig.CountSince(0, TEXT("AllocateAppearances")), 0);
	TestEqual(TEXT("no layout request before OnTuningReady"), Rig.CountSince(0, TEXT("RequestLayout")), 0);
	TestEqual(TEXT("the sequencer's own count is 0"), Rig.Core->GetPreparing().GetAllocateCallCount(), 0);

	// OnTuningReady: exactly one deal.
	Rig.Fakes.LevelSocket.Script(TEXT("RequestLayout"));
	Rig.Fakes.CharacterPool.Script(TEXT("AllocateAppearances"), static_cast<int64>(ERoundAllocateResult::Ok));
	Rig.Core->NotifyTuningReady(PreparingRigTuningHash);
	TestEqual(TEXT("AllocateAppearances exactly once after OnTuningReady"), Rig.CountSince(0, TEXT("AllocateAppearances")), 1);
	const FRoundFakeCall* Deal = Rig.Fakes.CharacterPool.OwnCalls.FindByPredicate([](const FRoundFakeCall& Call) { return Call.Call == FName(TEXT("AllocateAppearances")); });
	if (TestNotNull(TEXT("the deal was recorded"), Deal))
	{
		TestEqual(TEXT("Players = every rostered participant, connected or not"), Deal->Args[0], static_cast<int64>(Layout2v2.Team0 + Layout2v2.Team1));
		TestEqual(TEXT("the deal carries NpcCount"), Deal->Args[1], static_cast<int64>(PreparingRigNpcCount));
	}

	// Edge cases: repeated drains and a duplicate OnTuningReady deal nothing more.
	for (int32 Frame = 0; Frame < 30; ++Frame)
	{
		Rig.Frame();
	}
	Rig.Core->NotifyTuningReady(PreparingRigTuningHash);
	TestEqual(TEXT("still exactly once after more drains and a duplicate OnTuningReady"), Rig.CountSince(0, TEXT("AllocateAppearances")), 1);
	TestEqual(TEXT("one layout request"), Rig.CountSince(0, TEXT("RequestLayout")), 1);
	TestTrue(TEXT("no unexpected call"), Rig.Log.Unexpected.IsEmpty());
	return true;
}

#endif
