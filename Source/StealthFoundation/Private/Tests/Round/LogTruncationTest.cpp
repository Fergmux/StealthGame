// Story rmf-012 (production/epics/round-match-framework/story-012-log-truncation.md): GDD AC 33.
// 4,999 ordinary entries plus WinLatched into the 4096-entry ring (header outside the ring, WinLatched inside it and
// protected): the header and WinLatched survive, the oldest ordinary entries were dropped first (the earliest
// survivor is the 905th) and TruncatedCount = 904. Checked with WinLatched last and with it mid-stream.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Server/Testing/StealthRoundLogTestSupport.h"

namespace LogTruncationTestData
{
	/** The fixture's ordinary entry type (not protected, not rate-limited). */
	constexpr ERoundLogType Ordinary = ERoundLogType::RespawnScheduled;
	constexpr double EntrySpacingSec = 0.1;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoundLogTruncationTest, "StealthGame.Round.LogTruncation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRoundLogTruncationTest::RunTest(const FString& Parameters)
{
	using namespace LogTruncationTestData;
	using namespace RoundTestFixtures;
	using namespace RoundLogFixtures;

	for (const int32 WinLatchedAfter : { TruncationOrdinaryEntries, TruncationOrdinaryEntries / 2 })
	{
		const FString Name = WinLatchedAfter == TruncationOrdinaryEntries ? TEXT("WinLatched last") : TEXT("WinLatched mid-stream");
		FStealthRoundLogRig Rig(TruncationCap);
		TestEqual(*FString::Printf(TEXT("%s: cap"), *Name), Rig.Log.GetCapacity(), TruncationCap);
		Rig.Log.WriteHeader(MakeHeader());
		const FQueueKey LatchKey(5000, 5000);
		for (int32 Index = 0; Index < TruncationOrdinaryEntries; ++Index)
		{
			if (Index == WinLatchedAfter)
			{
				Rig.Log.Append(MakeWinLatched(ERoundEndReason::Elimination, 1, LatchKey, Index * EntrySpacingSec));
			}
			FRoundLogEntry Entry = MakeEntry(Ordinary, FQueueKey(Index + 1, Index + 1), Index * EntrySpacingSec);
			Entry.Data[0] = static_cast<uint32>(Index);  // 0-based ordinal of the ordinary entry
			Rig.Log.Append(Entry);
		}
		if (WinLatchedAfter == TruncationOrdinaryEntries)
		{
			Rig.Log.Append(MakeWinLatched(ERoundEndReason::Elimination, 1, LatchKey, TruncationOrdinaryEntries * EntrySpacingSec));
		}

		// Read at Ended through the gated API.
		Rig.Gate.bReleasable = true;
		FRoundLogHeader Header;
		TArray<FRoundLogEntry> Entries;
		TArray<uint32> Truncated;
		TestTrue(*FString::Printf(TEXT("%s: header survives"), *Name), Rig.Log.ReadHeader(Header) == ERoundLogReadResult::Ok && Header == MakeHeader());
		TestTrue(*FString::Printf(TEXT("%s: entries read"), *Name), Rig.Log.ReadEntries(Entries) == ERoundLogReadResult::Ok);
		TestTrue(*FString::Printf(TEXT("%s: counters read"), *Name), Rig.Log.ReadTruncatedCounts(Truncated) == ERoundLogReadResult::Ok);
		TestEqual(*FString::Printf(TEXT("%s: the ring is full"), *Name), Entries.Num(), TruncationCap);
		const int32 LatchedCount = Entries.FilterByPredicate([&LatchKey](const FRoundLogEntry& E) { return E.Type == ERoundLogType::WinLatched && E.Key == LatchKey; }).Num();
		TestEqual(*FString::Printf(TEXT("%s: WinLatched survives"), *Name), LatchedCount, 1);
		const FRoundLogEntry* FirstOrdinary = Entries.FindByPredicate([](const FRoundLogEntry& E) { return E.Type == Ordinary; });
		if (TestNotNull(*FString::Printf(TEXT("%s: ordinary entries remain"), *Name), FirstOrdinary))
		{
			TestEqual(*FString::Printf(TEXT("%s: the earliest surviving ordinary entry is the 905th"), *Name), FirstOrdinary->Data[0], static_cast<uint32>(TruncationExpectedCount));
		}
		bool bInOrder = true;
		uint32 Previous = 0;
		bool bFirst = true;
		for (const FRoundLogEntry& E : Entries)
		{
			if (E.Type == Ordinary)
			{
				bInOrder &= bFirst || E.Data[0] == Previous + 1;
				Previous = E.Data[0];
				bFirst = false;
			}
		}
		TestTrue(*FString::Printf(TEXT("%s: survivors are contiguous and in order (oldest dropped first)"), *Name), bInOrder);
		TestEqual(*FString::Printf(TEXT("%s: TruncatedCount = 904 for the ordinary type"), *Name), Truncated[static_cast<int32>(Ordinary)], static_cast<uint32>(TruncationExpectedCount));
		TestEqual(*FString::Printf(TEXT("%s: TruncatedCount = 904 in total"), *Name), Rig.Log.GetTotalTruncatedCount(), static_cast<uint64>(TruncationExpectedCount));
		TestEqual(*FString::Printf(TEXT("%s: no protected type was truncated"), *Name), Truncated[static_cast<int32>(ERoundLogType::WinLatched)], 0u);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
