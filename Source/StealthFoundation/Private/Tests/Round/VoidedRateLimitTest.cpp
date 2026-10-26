// Story rmf-015 (production/epics/round-match-framework/story-015-voided-rate-limit.md): GDD AC 61.
// One source submitting 100 invalid commands per second for 10 s of fake time: at most one Voided entry per second
// is written for it (the rest counted, never logged), a second source has its own budget, and the kill timeline of
// the round is intact (every protected entry present, in order). Window rule (RMF Exit A ruling): a Voided entry
// is written when at least 1.000 s has passed since that source's last written one, so any 1 s window holds one.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Server/StealthQueuePayloads.h"
#include "Server/Testing/StealthRoundLogTestSupport.h"

namespace VoidedRateLimitTestData
{
	/** One kill-timeline entry every this many commands. */
	constexpr int32 KillEvery = 37;
	/** Source B's commands, one per this many of A's. */
	constexpr int32 SecondSourceEvery = 50;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoundVoidedRateLimitTest, "StealthGame.Round.VoidedRateLimit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRoundVoidedRateLimitTest::RunTest(const FString& Parameters)
{
	using namespace VoidedRateLimitTestData;
	using namespace RoundTestFixtures;
	using namespace RoundLogFixtures;

	// Arrange.
	FStealthRoundLogRig Rig;
	const FMailboxId A = FMailboxId::ForParticipant(FParticipantId(FixtureParticipantIds[0]));
	const FMailboxId B = FMailboxId::ForParticipant(FParticipantId(FixtureParticipantIds[1]));
	const ERoundLogType Timeline[] = { ERoundLogType::KillResolved, ERoundLogType::StateTransition, ERoundLogType::Reversal, ERoundLogType::StrikeLimit, ERoundLogType::ObjectiveStage };
	const int32 Commands = FloodCommandsPerSecond * FloodSeconds;
	const double Step = 1.0 / FloodCommandsPerSecond;
	TArray<uint32> TimelineWritten;
	int32 BSubmitted = 0;

	// Act: 100 invalid commands per second from A (each resolver result becomes a Voided entry), B now and then,
	// and kill-timeline entries interleaved through the flood.
	for (int32 Index = 0; Index < Commands; ++Index)
	{
		const double Now = Rig.Clock.GetServerTimeSeconds();
		FRoundLogEntry Voided = MakeEntry(ERoundLogType::Voided, FQueueKey(Index + 1, 1), Now, A);
		Voided.Code = static_cast<uint16>(EVoidReason::Revalidation);
		Rig.Log.Append(Voided);
		if (Index % SecondSourceEvery == 0)
		{
			Rig.Log.Append(MakeEntry(ERoundLogType::Voided, FQueueKey(Index + 1, 2), Now, B));
			++BSubmitted;
		}
		if (Index % KillEvery == 0)
		{
			FRoundLogEntry Kill = MakeEntry(Timeline[(Index / KillEvery) % UE_ARRAY_COUNT(Timeline)], FQueueKey(Index + 1, 3), Now, A);
			Kill.Data[5] = static_cast<uint32>(TimelineWritten.Num());  // ordinal
			TimelineWritten.Add(Kill.Data[5]);
			Rig.Log.Append(Kill);
		}
		Rig.Clock.AdvanceSeconds(Step);
	}

	// Assert (read at Ended).
	Rig.Gate.bReleasable = true;
	TArray<FRoundLogEntry> Entries;
	TestTrue(TEXT("entries readable at Ended"), Rig.Log.ReadEntries(Entries) == ERoundLogReadResult::Ok);
	TArray<double> ATimes;
	int32 BWritten = 0;
	TArray<uint32> TimelineRead;
	for (const FRoundLogEntry& E : Entries)
	{
		if (E.Type == ERoundLogType::Voided && E.SourceMailbox == A)
		{
			ATimes.Add(E.ServerTime);
		}
		else if (E.Type == ERoundLogType::Voided && E.SourceMailbox == B)
		{
			++BWritten;
		}
		else if (IsProtectedRoundLogType(E.Type))
		{
			TimelineRead.Add(E.Data[5]);
		}
	}
	int32 WindowViolations = 0;
	for (int32 Index = 1; Index < ATimes.Num(); ++Index)
	{
		WindowViolations += FMath::RoundToInt64((ATimes[Index] - ATimes[Index - 1]) * 1000.0) < FMath::RoundToInt64(FRoundEventLog::VoidedMinSpacingSec * 1000.0) ? 1 : 0;  // whole ms, as the store
	}
	TestEqual(TEXT("at most one Voided entry for A in any 1 s window"), WindowViolations, 0);
	TestTrue(FString::Printf(TEXT("A wrote about one Voided entry per second (%d over %d s)"), ATimes.Num(), FloodSeconds), ATimes.Num() >= FloodSeconds - 1 && ATimes.Num() <= FloodSeconds);
	TestEqual(TEXT("A's suppressed voids are counted, not logged"), static_cast<int32>(Rig.Log.GetSuppressedVoidedCount(A)), Commands - ATimes.Num());
	TestTrue(TEXT("B has its own budget: its first void was written while A's budget was spent"), BWritten >= 1);
	TestEqual(TEXT("B's voids are written or counted"), BWritten + static_cast<int32>(Rig.Log.GetSuppressedVoidedCount(B)), BSubmitted);
	TestTrue(TEXT("the kill timeline is intact: every entry present, in order"), TimelineRead == TimelineWritten);
	TestEqual(TEXT("nothing was evicted"), Rig.Log.GetTotalTruncatedCount(), static_cast<uint64>(0));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
