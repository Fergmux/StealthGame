// Story tdc-052 (production/epics/tuning-data-config/story-052-round-session-stubs.md): self-test of the
// test-only Round, Session and Action-consumer stubs (Server/Testing/StealthStubRound.h, StealthStubSession.h,
// StealthStubActionConsumer.h). Scripted call sequences on each stub; each records exactly the calls made.
// The pawn-release tick comes from FFakeStealthClock (story tdc-007): no wall-clock read. The file stays at
// the plan path Tests/Tuning/ (the test name's system is Harness; see the story's folder note).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Server/StealthServerTuning.h"
#include "Server/Testing/StealthFakeClock.h"
#include "Server/Testing/StealthStubActionConsumer.h"
#include "Server/Testing/StealthStubRound.h"
#include "Server/Testing/StealthStubSession.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHarnessTuningStubsSelfTest, "StealthGame.Harness.TuningStubsSelfTest",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHarnessTuningStubsSelfTest::RunTest(const FString& Parameters)
{
	// FStubRound: BeginResolve forwards to the injected target; every call is recorded in order.
	{
		int32 TargetCalls = 0;
		FFakeStealthClock Clock(0.0, 100);
		FStubRound Round([&TargetCalls]() { ++TargetCalls; }, &Clock);
		TestEqual(TEXT("round starts in Lobby"), static_cast<int32>(Round.GetPhase()), static_cast<int32>(EStealthStubRoundPhase::Lobby));

		Round.EnterPhase(EStealthStubRoundPhase::Preparing);
		Round.BeginResolve();
		Round.OnTuningReady(0x1234ull);
		Clock.AdvanceTicks(7);
		const uint64 ReleaseTick = Round.ReleasePawns();
		Round.EnterPhase(EStealthStubRoundPhase::Playing);
		Round.EnterPhase(EStealthStubRoundPhase::Ended);
		Round.OnTuningFailed(ETuningFailReason::SnapshotFault);

		TestEqual(TEXT("BeginResolve forwarded once"), TargetCalls, 1);
		TestEqual(TEXT("pawns released at the clock's common tick"), ReleaseTick, static_cast<uint64>(107));
		TestEqual(TEXT("OnTuningFailed returns the round to Lobby"), static_cast<int32>(Round.GetPhase()), static_cast<int32>(EStealthStubRoundPhase::Lobby));

		TArray<FStealthStubRoundCall> Expected;
		const auto Add = [&Expected](EStealthStubRoundCall Kind) -> FStealthStubRoundCall& { FStealthStubRoundCall& C = Expected.AddDefaulted_GetRef(); C.Kind = Kind; return C; };
		Add(EStealthStubRoundCall::EnterPhase).Phase = EStealthStubRoundPhase::Preparing;
		Add(EStealthStubRoundCall::BeginResolve);
		Add(EStealthStubRoundCall::OnTuningReady).TuningHash = 0x1234ull;
		Add(EStealthStubRoundCall::ReleasePawns).Tick = 107;
		Add(EStealthStubRoundCall::EnterPhase).Phase = EStealthStubRoundPhase::Playing;
		Add(EStealthStubRoundCall::EnterPhase).Phase = EStealthStubRoundPhase::Ended;
		Add(EStealthStubRoundCall::OnTuningFailed).Reason = ETuningFailReason::SnapshotFault;
		TestTrue(TEXT("the round recorded exactly the calls made, in order"), Round.GetCalls() == Expected);

		// The Event Log header is an opaque buffer the test writes and reads back.
		Round.GetEventLogHeader() = { 1, 2, 3 };
		const FStubRound& ConstRound = Round;
		TestTrue(TEXT("the header buffer reads back what was written"), ConstRound.GetEventLogHeader() == TArray<uint8>({ 1, 2, 3 }));

		// Without a target or a clock: still recorded, release tick 0.
		FStubRound Bare;
		Bare.BeginResolve();
		TestEqual(TEXT("bare round: release tick 0"), Bare.ReleasePawns(), static_cast<uint64>(0));
		TestEqual(TEXT("bare round: two calls"), Bare.GetCalls().Num(), 2);
	}

	// FStubSession: one record per acknowledgement, the second from the same participant included.
	{
		FStubSession Session;
		Session.RejoinWindowSec = 90.0;
		Session.SetExpectedClientHash(0xABCDull);
		TestFalse(TEXT("participant 1: no command before its tuning is valid"), Session.AcceptCommand(1));
		TestFalse(TEXT("a wrong hash is rejected"), Session.Acknowledge(1, 0x9999ull));
		TestFalse(TEXT("still no command after a rejected acknowledgement"), Session.AcceptCommand(1));
		TestTrue(TEXT("the right hash is accepted"), Session.Acknowledge(1, 0xABCDull));
		TestTrue(TEXT("participant 1 may command once its tuning is valid"), Session.AcceptCommand(1));
		TestTrue(TEXT("a second acknowledgement is recorded as a second"), Session.Acknowledge(1, 0xABCDull));
		Session.Deadline(2);
		TestFalse(TEXT("participant 2 (deadline) has no valid tuning"), Session.AcceptCommand(2));
		TestEqual(TEXT("RejoinWindowSec is the supplied value"), Session.RejoinWindowSec, 90.0);

		const TArray<FStealthStubAck> Expected = {
			{ 1, 0x9999ull, false, false },
			{ 1, 0xABCDull, true, false },
			{ 1, 0xABCDull, true, false },
			{ 2, 0ull, false, true },
		};
		TestTrue(TEXT("the session recorded exactly the acknowledgements made, in order"), Session.GetAcks() == Expected);

		Session.BeginEpoch(0x5555ull);
		TestFalse(TEXT("a new epoch clears tuning-ready state"), Session.AcceptCommand(1));
		TestEqual(TEXT("the acknowledgement record survives the epoch"), Session.GetAcks().Num(), 4);
	}

	// FStubActionConsumer: captures DefenceDurationSec at start, ends at start + captured, never re-reads.
	{
		FStealthServerTuning Live;
		Live.Assassination_Public.DefenceDurationSec = 4.0f;
		FStubActionConsumer Consumer;
		Consumer.StartAction(10.0, Live);
		Live.Assassination_Public.DefenceDurationSec = 5.0f; // a later live change (hot-reload)
		TestEqual(TEXT("captured duration 4"), Consumer.GetRecord().CapturedDurationSec, 4.0);
		TestEqual(TEXT("ends at start + captured value"), Consumer.GetRecord().EndSec, 14.0);
		TestTrue(TEXT("active before the captured end"), Consumer.IsActive(13.9));
		TestFalse(TEXT("ended at start + 4, not start + 5"), Consumer.IsActive(14.0));
		TestEqual(TEXT("the live value was read exactly once"), Consumer.GetLiveReads(), 1);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
