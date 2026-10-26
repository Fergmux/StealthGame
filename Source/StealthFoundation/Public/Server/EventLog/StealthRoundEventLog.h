#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Server/EventLog/StealthRoundLogTypes.h"
#include "Server/EventLog/StealthRoundLogWriter.h"
#include "Templates/Function.h"

class IStealthClock;
class IStealthFaultSink;

/**
 * FRoundEventLog: the server-only Round Event Log store (ADR-0002 section 8.9; Round GDD R19, F8; ADR-0007
 * section 6; ADR-0023 section 5). Plain C++ (no USTRUCT/UCLASS/UPROPERTY, no ToString), owned by one facade
 * subsystem through a TUniquePtr. Built by stories rmf-011 (store, header, read gate), rmf-012 (protected
 * eviction, TruncatedCount), rmf-013 (capacity clamp, entry size), rmf-014 (no leaks; the Ended/Reset disk
 * writer), rmf-015 (Voided rate limit) and rmf-017 (TendCompleted with the Latched flag, through Append).
 *
 * Storage: an uncapped header written once at P3, plus a ring of RoundLogMaxEntriesCount entries (default 4096,
 * hard 512..16384; a larger setting is clamped to 16384). KillResolved, Reversal, StateTransition, StrikeLimit,
 * ObjectiveStage, FinalCompleted and WinLatched occupy ring slots and are never evicted; when the ring is full the
 * oldest non-protected entry is dropped and its type's TruncatedCount increments. Eviction is O(1): ordinary
 * entries sit in a FIFO of slot indices, and no Append allocates once the ring is at capacity.
 *
 * Voided entries are rate-limited to one per source mailbox per second on the injected clock (an entry at least
 * 1.000 s, in whole milliseconds, after that source's last written Voided entry is written; the rest are counted,
 * never logged).
 *
 * Two read paths (decision D48):
 *   - the client/dump read API (Read*, WriteToDisk) refuses every read until the injected gate says the round is
 *     terminated (phase Ended or Reset, or the SessionEnded latch);
 *   - the server-internal accessor (GetHeaderServerInternal, ForEachEntryServerInternal) is Round's own path, used
 *     in the latch step to compute FRoundResult.TeamSummary (story rmf-054); it is never exposed to a client, a
 *     console command or a file.
 *
 * Never pass the store, an entry, the header or any of their fields to a log, check, ensure, trace, CSV or stat
 * macro (ADR-0007 lint rule (i)). Nothing here calls one.
 */

/** Team rosters of the header (participant id values per team). */
struct FRoundLogTeamRosters
{
	TArray<uint32> Team[2];

	bool operator==(const FRoundLogTeamRosters& Other) const { return Team[0] == Other.Team[0] && Team[1] == Other.Team[1]; }
	bool operator!=(const FRoundLogTeamRosters& Other) const { return !(*this == Other); }
};

/**
 * The header field list (R19 / ADR-0002 section 8.9 order): X(Type, Name, Default). The fixture constant
 * RoundLogHeaderFields names every one of them; adding a field here keeps the field-name list in step.
 */
#define STEALTH_ROUND_LOG_HEADER_FIELDS(X) \
	X(uint32, RoundIndex, 0) \
	X(uint32, RoundEpoch, 0) \
	X(int32, Seed, 0) \
	X(uint32, LayoutId, 0) \
	X(uint32, LayoutAttempt, 0) \
	X(uint64, LayoutHash, 0) \
	X(uint8, ProviderKind, 0) \
	X(uint8, LayoutFallbackLevel, 0) \
	X(FName, PresetId, NAME_None) \
	X(uint64, TuningHash, 0) \
	X(uint32, SchemaVersion, 0) \
	X(uint8, FallbackLevel, 0) \
	X(bool, HotReloaded, false) \
	X(TArray<uint32>, TuningWarnings, ) \
	X(uint64, DealDigest, 0) \
	X(TArray<uint16>, AppearanceSet, ) \
	X(TArray<uint16>, AppearanceCounts, ) \
	X(bool, PoolBelowMvpFloor, false) \
	X(FRoundLogTeamRosters, TeamRosters, ) \
	X(uint32, HostParticipantId, FParticipantId::InvalidValue) \
	X(TArray<uint16>, ZoneShares, ) \
	X(uint32, BuildVersion, 0)

#define STEALTH_ROUND_LOG_HEADER_DECLARE(Type, Name, Default) Type Name{ Default };
#define STEALTH_ROUND_LOG_HEADER_EQUAL(Type, Name, Default) && Name == Other.Name

/**
 * The Event Log header, written once at P3 (R6). Holds the public Seed and hashes only: never a server sub-seed,
 * the server secret or entropy (R19 "Secrets are not stored").
 */
struct FRoundLogHeader
{
	STEALTH_ROUND_LOG_HEADER_FIELDS(STEALTH_ROUND_LOG_HEADER_DECLARE)

	bool operator==(const FRoundLogHeader& Other) const { return true STEALTH_ROUND_LOG_HEADER_FIELDS(STEALTH_ROUND_LOG_HEADER_EQUAL); }
	bool operator!=(const FRoundLogHeader& Other) const { return !(*this == Other); }

	/** The header field names, in declaration order (for the RoundLogHeaderFields fixture check). */
	STEALTHFOUNDATION_API static TConstArrayView<const TCHAR*> GetFieldNames();
};

/** "Is the round terminated" as far as the log's read and disk paths are concerned: phase Ended or Reset, or the SessionEnded latch. */
class IRoundLogAccessGate
{
public:
	virtual ~IRoundLogAccessGate() = default;

	virtual bool IsLogReleasable() const = 0;
};

/** The disk writer's target (the facade adapts it to a Saved/ file; tests spy on it). */
class IRoundLogFileSink
{
public:
	virtual ~IRoundLogFileSink() = default;

	virtual bool WriteFile(const FString& FileName, const FString& Contents) = 0;
};

/** A read through the client/dump API. */
enum class ERoundLogReadResult : uint8
{
	Ok,
	/** Refused: the round is not Ended (or Reset). */
	Refused_NotReleasable,
	/** Readable, but no header was written this round. */
	NoHeader,
};

/** Fault-sink Detail values for EStealthFault::RoundLogMisuse. */
enum class EStealthRoundLogMisuse : uint8
{
	HeaderWrittenTwice = 1,
	RingFullOfProtectedEntries = 2,
};

/** Per-team inputs of FRoundResult.TeamSummary (ADR-0002 section 8.9 reader (a); story rmf-054 owns the result). */
struct FRoundLogTeamTallies
{
	/** ObjectiveStage plus FinalCompleted entries of the team. */
	uint32 StepsCompleted[2] = { 0, 0 };
	/** StateTransition entries Active -> Informant of a member of the team. */
	uint32 EliminatedCount[2] = { 0, 0 };
	/** KillResolved entries with the attacker on the team and result WrongTarget. */
	uint32 WrongTargetCount[2] = { 0, 0 };
};

class STEALTHFOUNDATION_API FRoundEventLog : public IRoundLogWriter
{
public:
	static constexpr int32 MaxEntriesHardMin = 512;
	static constexpr int32 MaxEntriesHardMax = 16384;
	static constexpr int32 MaxEntriesDefault = 4096;
	/** R19: Voided entries per source mailbox, at most one per this many seconds. */
	static constexpr double VoidedMinSpacingSec = 1.0;

	/** Gate, Clock and FaultSink must outlive the store. The ring starts at the default capacity. */
	FRoundEventLog(const IRoundLogAccessGate& InGate, const IStealthClock& InClock, IStealthFaultSink& InFaultSink);

	FRoundEventLog(const FRoundEventLog&) = delete;
	FRoundEventLog& operator=(const FRoundEventLog&) = delete;

	/** The RoundLogMaxEntriesCount clamp: [512, 16384] (a mis-set value can never exhaust server memory). */
	static int32 ClampMaxEntries(int32 Configured) { return FMath::Clamp(Configured, MaxEntriesHardMin, MaxEntriesHardMax); }

	/** Starts a round's log: clears everything and reserves the ring at the clamped capacity. */
	void BeginRound(int32 ConfiguredMaxEntries);

	/** Writes the header once (P3). A second write is refused (false) and reported. */
	bool WriteHeader(const FRoundLogHeader& Header);

	// IRoundLogWriter: the single write path (game thread, inside the producing slot, key order).
	virtual void Append(const FRoundLogEntry& Entry) override;

	// --- Client/dump read API: refused before the round is terminated ---------------------------------------------

	ERoundLogReadResult ReadHeader(FRoundLogHeader& OutHeader) const;
	/** Every entry still in the ring, in append order. */
	ERoundLogReadResult ReadEntries(TArray<FRoundLogEntry>& OutEntries) const;
	/** TruncatedCount per ERoundLogType (indexed by the enum value). */
	ERoundLogReadResult ReadTruncatedCounts(TArray<uint32>& OutPerType) const;

	/**
	 * The store's single disk writer and the only text conversion of log content (ADR-0007 section 6): refused
	 * (false, nothing written) unless the gate is releasable; never mid-round.
	 */
	bool WriteToDisk(IRoundLogFileSink& Sink, const FString& FileName) const;

	// --- Server-internal accessor (D48): Round's own latch-step path, not the client/dump API ------------------------

	/** The header, or nullptr when none was written. */
	const FRoundLogHeader* GetHeaderServerInternal() const { return bHeaderWritten ? &Header : nullptr; }
	/** Calls Visit for every entry still in the ring, in append order. */
	void ForEachEntryServerInternal(TFunctionRef<void(const FRoundLogEntry&)> Visit) const;
	/** The server time of the first PhaseChanged entry into Active (kept even if that row is evicted). */
	TOptional<double> GetActivePhaseStartServerInternal() const { return ActivePhaseStart; }
	/** TeamSummary inputs computed from the ring (rmf-054 builds FRoundResult.TeamSummary from them). */
	FRoundLogTeamTallies TallyTeamSummaryInputsServerInternal() const;

	// --- Counts and sizes (never content) ------------------------------------------------------------------------------

	int32 GetCapacity() const { return Capacity; }
	int32 Num() const { return LiveCount; }
	uint32 GetTruncatedCount(ERoundLogType Type) const { return TruncatedCounts[static_cast<int32>(Type)]; }
	uint64 GetTotalTruncatedCount() const;
	uint32 GetSuppressedVoidedCount(FMailboxId Source) const;
	uint64 GetTotalSuppressedVoidedCount() const { return TotalSuppressedVoided; }
	/** F8's EntryBytes as this store defines it: entries are fixed-size, with no variable-length data. */
	static constexpr int32 GetEntryBytes() { return static_cast<int32>(sizeof(FRoundLogEntry)); }
	/** What one ring slot occupies (the entry plus the store's per-slot bookkeeping). */
	static constexpr int32 GetSlotBytes();

	/** The release class of each entry type (R19, provisional). */
	static ERoundLogReleaseClass GetReleaseClass(ERoundLogType Type);

private:
	struct FSlot
	{
		FRoundLogEntry Entry;
		uint64 InsertSeq = 0;
		bool bLive = false;
	};

	struct FVoidedBudget
	{
		uint32 Source = 0;
		int64 LastWrittenMs = 0;
		bool bWritten = false;
		uint32 Suppressed = 0;
	};

	void CollectLive(TArray<const FSlot*>& Out) const;

	const IRoundLogAccessGate& Gate;
	const IStealthClock& Clock;
	IStealthFaultSink& FaultSink;

	FRoundLogHeader Header;
	bool bHeaderWritten = false;

	int32 Capacity = MaxEntriesDefault;
	TArray<FSlot> Slots;
	TArray<int32> FreeSlots;
	/** Ring buffer (Capacity long) of slot indices of the live ordinary (non-protected) entries, oldest at OrdinaryHead. */
	TArray<int32> OrdinaryFifo;
	int32 OrdinaryHead = 0;
	int32 OrdinaryCount = 0;
	int32 LiveCount = 0;
	uint64 NextInsertSeq = 1;

	uint32 TruncatedCounts[GRoundLogTypeCount] = {};
	TOptional<double> ActivePhaseStart;
	TArray<FVoidedBudget> VoidedBudgets;
	uint64 TotalSuppressedVoided = 0;
};

constexpr int32 FRoundEventLog::GetSlotBytes()
{
	return static_cast<int32>(sizeof(FSlot));
}
