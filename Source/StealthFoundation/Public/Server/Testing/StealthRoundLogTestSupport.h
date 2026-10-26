#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"

#if WITH_STEALTH_TEST_SEAMS

#include "Server/EventLog/StealthRoundEventLog.h"
#include "Server/Testing/StealthFakeClock.h"
#include "Server/Testing/StealthFaultSinkTestSupport.h"
#include "Server/Testing/StealthRoundTestConstants.h"

/**
 * Round Event Log test support (stories rmf-011 .. rmf-017). Compiled only under WITH_STEALTH_TEST_SEAMS.
 *
 *   FStealthFakeLogGate     IRoundLogAccessGate the test opens when its fixture round reaches Ended or Reset
 *   FStealthSpyLogFileSink  IRoundLogFileSink that keeps every file the disk writer produced
 *   FStealthRoundLogRig     a store wired to the fake clock, gate and a recording fault sink
 *   RoundLogFixtures        builders for fixture headers and entries (no real layout, tuning or pool data)
 */

class FStealthFakeLogGate : public IRoundLogAccessGate
{
public:
	bool bReleasable = false;

	virtual bool IsLogReleasable() const override { return bReleasable; }
};

class FStealthSpyLogFileSink : public IRoundLogFileSink
{
public:
	TArray<TPair<FString, FString>> Files;

	virtual bool WriteFile(const FString& FileName, const FString& Contents) override
	{
		Files.Emplace(FileName, Contents);
		return true;
	}
};

class FStealthRoundLogRig
{
public:
	explicit FStealthRoundLogRig(int32 MaxEntries = FRoundEventLog::MaxEntriesDefault)
		: Log(Gate, Clock, Faults)
	{
		Log.BeginRound(MaxEntries);
	}

	FFakeStealthClock Clock;
	FStealthFakeLogGate Gate;
	FStealthRecordingFaultSink Faults;
	FRoundEventLog Log;
};

namespace RoundLogFixtures
{
	/** A header with every field set to a distinct non-default value (sentinel hashes and ids where they apply). */
	inline FRoundLogHeader MakeHeader(int32 Team0Size = 5, int32 Team1Size = 5)
	{
		using namespace RoundTestFixtures;
		FRoundLogHeader Header;
		Header.RoundIndex = 3;
		Header.RoundEpoch = FixtureRoundEpoch;
		Header.Seed = 424242;
		Header.LayoutId = 17;
		Header.LayoutAttempt = 2;
		Header.LayoutHash = SentinelLayoutHash;
		Header.ProviderKind = 1;
		Header.LayoutFallbackLevel = 1;
		Header.PresetId = FName(TEXT("Playtest"));
		Header.TuningHash = SentinelTuningHash;
		Header.SchemaVersion = 12;
		Header.FallbackLevel = 1;
		Header.HotReloaded = true;
		Header.TuningWarnings = { 4, 9 };
		Header.DealDigest = SentinelDealDigest;
		Header.AppearanceSet = { 1, 2, 3 };
		Header.AppearanceCounts = { 20, 20, 20 };
		Header.PoolBelowMvpFloor = true;
		for (int32 Index = 0; Index < Team0Size; ++Index)
		{
			Header.TeamRosters.Team[0].Add(100 + Index);
		}
		for (int32 Index = 0; Index < Team1Size; ++Index)
		{
			Header.TeamRosters.Team[1].Add(200 + Index);
		}
		Header.HostParticipantId = SentinelParticipantIds[0];
		Header.ZoneShares = { 500, 500 };
		Header.BuildVersion = 58902052;
		return Header;
	}

	/** An entry of Type at Key and Time with the given data words. */
	inline FRoundLogEntry MakeEntry(ERoundLogType Type, FQueueKey Key, double ServerTime, FMailboxId Source = FMailboxId::ServerLocal())
	{
		FRoundLogEntry Entry;
		Entry.Type = Type;
		Entry.Key = Key;
		Entry.ServerTime = ServerTime;
		Entry.RoundEpoch = RoundTestFixtures::FixtureRoundEpoch;
		Entry.SourceMailbox = Source;
		return Entry;
	}

	/** WinLatched(outcome, EndReason, key): Code = EndReason, D0 = winner team. */
	inline FRoundLogEntry MakeWinLatched(ERoundEndReason Reason, uint32 WinnerTeam, FQueueKey Key, double ServerTime, bool bDebug = false)
	{
		FRoundLogEntry Entry = MakeEntry(ERoundLogType::WinLatched, Key, ServerTime);
		Entry.Code = static_cast<uint16>(Reason);
		Entry.Data[0] = WinnerTeam;
		Entry.Flags = bDebug ? RoundLogFlags::Debug : 0;
		return Entry;
	}

	/** PhaseChanged(old, new) (Round phase values Lobby..Reset = 1..5). */
	inline FRoundLogEntry MakePhaseChanged(uint32 OldPhase, uint32 NewPhase, FQueueKey Key, double ServerTime)
	{
		FRoundLogEntry Entry = MakeEntry(ERoundLogType::PhaseChanged, Key, ServerTime);
		Entry.Data[0] = OldPhase;
		Entry.Data[1] = NewPhase;
		return Entry;
	}

	/** Round phase values (ADR-0002 section 8.10). */
	inline constexpr uint32 PhaseLobby = 1;
	inline constexpr uint32 PhasePreparing = 2;
	inline constexpr uint32 PhaseActive = 3;
	inline constexpr uint32 PhaseEnded = 4;
	inline constexpr uint32 PhaseReset = 5;
}

#endif // WITH_STEALTH_TEST_SEAMS
