// Story rmf-013 (production/epics/round-match-framework/story-013-log-size-bound.md): GDD AC 34b (revised).
// A scripted 300-entry round reports the mean entry size against F8's 64-96 B estimate (advisory: reported, never
// asserted); with RoundLogMaxEntriesCount set above its clamp and 20,000 entries logged, the ring is never above
// 16,384 (the setting is clamped to 16,384). Counts and sizes only are reported, never entry content.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Server/Testing/StealthRoundLogTestSupport.h"

namespace LogSizeBoundTestData
{
	/** A representative R19 mix for the scripted round (types cycle in this order). */
	const ERoundLogType ScriptedMix[] = {
		ERoundLogType::StateTransition, ERoundLogType::KillResolved, ERoundLogType::ObjectiveStage, ERoundLogType::TendCompleted,
		ERoundLogType::StageSignalReleased, ERoundLogType::Unresponsive, ERoundLogType::Rejoined, ERoundLogType::RespawnScheduled,
		ERoundLogType::StrikeCounted, ERoundLogType::HandOver,
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoundLogSizeBoundTest, "StealthGame.Round.LogSizeBound",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRoundLogSizeBoundTest::RunTest(const FString& Parameters)
{
	using namespace LogSizeBoundTestData;
	using namespace RoundTestFixtures;
	using namespace RoundLogFixtures;

	// 1. Scripted 300-entry round: report the mean entry size (advisory).
	{
		FStealthRoundLogRig Rig;
		Rig.Log.WriteHeader(MakeHeader());
		for (int32 Index = 0; Index < SizeScriptedEntries; ++Index)
		{
			Rig.Log.Append(MakeEntry(ScriptedMix[Index % UE_ARRAY_COUNT(ScriptedMix)], FQueueKey(Index + 1, Index + 1), Index * 0.5));
		}
		TestEqual(TEXT("the scripted round holds its 300 entries"), Rig.Log.Num(), SizeScriptedEntries);
		// Entries are fixed-size (no variable-length data), so the mean is the entry size; the slot adds bookkeeping.
		const int32 EntryBytes = FRoundEventLog::GetEntryBytes();
		const int32 SlotBytes = FRoundEventLog::GetSlotBytes();
		const bool bWithinF8 = EntryBytes >= F8EntryBytesMin && EntryBytes <= F8EntryBytesMax;
		AddInfo(FString::Printf(TEXT("F8 advisory: mean entry size %d B over %d entries (ring slot %d B); F8 estimate %d-%d B: %s; ring of %d at the default %d B, at the 16,384 maximum %d B"),
			EntryBytes, SizeScriptedEntries, SlotBytes, F8EntryBytesMin, F8EntryBytesMax, bWithinF8 ? TEXT("within") : TEXT("outside"),
			FRoundEventLog::MaxEntriesDefault, FRoundEventLog::MaxEntriesDefault * SlotBytes, FRoundEventLog::MaxEntriesHardMax * SlotBytes));
	}

	// 2. Clamp: a configured value above the hard maximum is clamped to 16,384, and 20,000 appends never exceed it.
	{
		TestEqual(TEXT("the clamp maps the above-maximum fixture to 16,384"), FRoundEventLog::ClampMaxEntries(AboveClampMaxEntries), ClampedMaxEntries);
		TestEqual(TEXT("the clamp keeps the hard minimum"), FRoundEventLog::ClampMaxEntries(1), FRoundEventLog::MaxEntriesHardMin);
		FStealthRoundLogRig Rig(AboveClampMaxEntries);
		TestEqual(TEXT("the store's capacity reads back as 16,384"), Rig.Log.GetCapacity(), ClampedMaxEntries);
		int32 MaxSeen = 0;
		int32 OverBound = 0;
		for (int32 Index = 0; Index < ClampFloodEntries; ++Index)
		{
			Rig.Log.Append(MakeEntry(ERoundLogType::RespawnScheduled, FQueueKey(Index + 1, Index + 1), Index * 0.01));
			MaxSeen = FMath::Max(MaxSeen, Rig.Log.Num());
			OverBound += Rig.Log.Num() > ClampedMaxEntries ? 1 : 0;
		}
		TestEqual(TEXT("the ring size is never above 16,384 at any insert"), OverBound, 0);
		TestEqual(TEXT("the ring fills to exactly 16,384"), MaxSeen, ClampedMaxEntries);
		TestEqual(TEXT("all-ordinary flood: 20,000 - 16,384 = 3,616 evicted"), Rig.Log.GetTotalTruncatedCount(), static_cast<uint64>(ClampFloodEntries - ClampedMaxEntries));
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
