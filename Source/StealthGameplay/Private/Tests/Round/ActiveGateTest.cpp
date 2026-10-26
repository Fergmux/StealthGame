// Story rmf-042 (production/epics/round-match-framework/story-042-active-gate.md), GDD AC 10 / R6, R7:
//   - in the same drain that enters Active: ActiveAtServerTime is 0 in FRoundPublicState, ActivePhaseStart is stamped
//     (the drain's server time), RoundsPlayedCount rose by 1, OnRoundActive fired once, Crowd received one start, and the
//     resolution queue is open for the round (Seq restarts at 1);
//   - a command submitted in Preparing returns NotNow (Round's phase gate: the queue is not open in Preparing);
//   - later drains re-fire nothing (the increment happens exactly once).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Server/Testing/StealthQueueTestRig.h"
#include "StealthRoundExitBTestRig.h"
#include "View/CommandReplyTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoundActiveGateTest, "StealthGame.Round.ActiveGate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRoundActiveGateTest::RunTest(const FString& Parameters)
{
	using namespace RoundTestFixtures;

	// Arrange: a queue that has already stamped items (an earlier round), a 2v2 round released to Active.
	FStealthQueueTestRig QueueRig;
	FStealthStubResolver Stub;
	QueueRig.Queue->RegisterResolver(EQueueItemType::PawnCommand, &Stub);
	QueueRig.Queue->Submit(FMailboxId::ServerLocal(), QueueRig.MakeItem(EQueueItemType::PawnCommand, 1));
	QueueRig.Queue->Submit(FMailboxId::ServerLocal(), QueueRig.MakeItem(EQueueItemType::PawnCommand, 2));
	QueueRig.Tick();
	TestTrue(TEXT("the earlier round consumed Seqs"), QueueRig.Queue->PeekNextSeq() > 1);

	FRoundExitBRig Rig;
	Rig.Core->SetRoundQueue(QueueRig.Queue.Get());
	int32 ActiveEvents = 0;
	double ActiveEventStart = -1.0;
	uint32 PlayedAtEvent = 0;
	Rig.Core->OnRoundActive = [&](uint32, double Start)
	{
		++ActiveEvents;
		ActiveEventStart = Start;
		PlayedAtEvent = Rig.Core->GetPhaseCore().GetRoundsPlayedCount();
	};
	Rig.OpenLobby();
	Rig.SetLayout(Layout2v2.Team0, Layout2v2.Team1);
	const uint32 PlayedBefore = Rig.Core->GetPhaseCore().GetRoundsPlayedCount();
	Rig.EnterPreparing();

	// A command in Preparing: NotNow.
	const TOptional<ECommandRejectCode> PreparingReply = Rig.Core->AdmitCommand();
	TestTrue(TEXT("a command submitted in Preparing returns NotNow"), PreparingReply.IsSet() && PreparingReply.GetValue() == ECommandRejectCode::NotNow);

	Rig.ScriptPreparingSequence();
	Rig.Core->NotifyTuningReady(PreparingRigTuningHash);
	Rig.Core->NotifyLayoutReady(1, Rig.Core->GetPreparing().GetSeed());
	Rig.AckAll(false);
	Rig.Frame();
	Rig.AckAll(true);
	Rig.Frame();
	TestTrue(TEXT("P7 published ActiveAtServerTime"), Rig.Core->GetPhaseCore().MakePublicState().ActiveAtServerTime > 0.0);
	TestTrue(TEXT("still NotNow before Active"), Rig.Core->AdmitCommand().IsSet());

	// Act: the drain that enters Active.
	Rig.ScriptActiveEntry();
	Rig.Frame(Rig.Core->GetParams().Preparing.StartLeadSec);
	const double DrainTime = Rig.Clock.GetServerTimeSeconds();

	// Assert: all in that same drain.
	const FRoundPublicState Public = Rig.Core->GetPhaseCore().MakePublicState();
	TestTrue(TEXT("phase Active"), Rig.Core->GetPhase() == ERoundPhase::Active);
	TestEqual(TEXT("ActiveAtServerTime is 0 in the public struct"), Public.ActiveAtServerTime, 0.0);
	TestEqual(TEXT("ActivePhaseStart is stamped with the drain's server time"), Rig.Core->GetActivePhaseStart(), DrainTime);
	TestEqual(TEXT("RoundsPlayedCount rose by 1"), Rig.Core->GetPhaseCore().GetRoundsPlayedCount(), PlayedBefore + 1);
	TestEqual(TEXT("OnRoundActive fired once"), ActiveEvents, 1);
	TestEqual(TEXT("OnRoundActive carries ActivePhaseStart"), ActiveEventStart, DrainTime);
	TestEqual(TEXT("subscribers already see the new RoundsPlayedCount"), PlayedAtEvent, PlayedBefore + 1);
	TestEqual(TEXT("Crowd received one start"), Rig.CountSince(0, TEXT("StartRound")), 1);
	TestEqual(TEXT("the queue is open for the round: Seq restarts at 1"), QueueRig.Queue->PeekNextSeq(), static_cast<uint64>(1));
	TestFalse(TEXT("commands are admitted in Active"), Rig.Core->AdmitCommand().IsSet());

	// Later drains re-fire nothing.
	for (int32 Frame = 0; Frame < 30; ++Frame)
	{
		Rig.Frame();
	}
	TestEqual(TEXT("OnRoundActive still once"), ActiveEvents, 1);
	TestEqual(TEXT("one Crowd start"), Rig.CountSince(0, TEXT("StartRound")), 1);
	TestEqual(TEXT("RoundsPlayedCount incremented exactly once"), Rig.Core->GetPhaseCore().GetRoundsPlayedCount(), PlayedBefore + 1);
	TestFalse(TEXT("a forced second entry is refused (Active -> Active is no transition)"), Rig.Core->EnterActive());
	TestTrue(TEXT("no unexpected call"), Rig.Log.Unexpected.IsEmpty());
	QueueRig.Queue->UnregisterResolver(EQueueItemType::PawnCommand);
	return true;
}

#endif
