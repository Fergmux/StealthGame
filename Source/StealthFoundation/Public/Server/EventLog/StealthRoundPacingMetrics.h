#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Server/EventLog/StealthRoundLogTypes.h"

class FRoundEventLog;

/**
 * Round's F7 pacing metrics (Round GDD F7, R19; ADR-0023 section 3; story rmf-016). Logged only, never a rule: a
 * stall is reported as an Objectives stall-pressure defect and never answered with a Round timer.
 *
 * Pure functions over Event Log data: the per-round values are computed at Ended from the header and the WinLatched
 * entry; the cross-round shares are aggregated offline by the analysis tool, but the aggregation lives here so the
 * fixtures prove the arithmetic. No UObject, no reflection, no text conversion.
 *
 *   PacingRatio        = DurationSec / (60 x TargetRoundMinutes)
 *   StallFlag          = DurationSec > 60 x StallFlagMinutes            (exact integer milliseconds)
 *   AbortFlag          = an Active abort with DurationSec > 30 x StallFlagMinutes
 *   ObjectiveEndShare  = N_obj / (N_obj + N_elim) per team-size bucket B = min(n0, n1); forfeits excluded
 *   AbortedActiveShare = (N_abort + N_void) / (N_obj + N_elim + N_forfeit + N_abort + N_void)
 * DebugForced rounds (and Debug-flagged ones), PreparingFailed and SessionEnded enter no count. A share with a zero
 * denominator is "n/a", never 0.
 */

/** The two tuning inputs (Round keys TargetRoundMinutes, default 12, and StallFlagMinutes, default 18). */
struct FRoundPacingTuning
{
	int32 TargetRoundMinutes = 0;
	int32 StallFlagMinutes = 0;
};

/** One round's summary as read from its log. */
struct FRoundPacingInput
{
	/** Latch time minus ActivePhaseStart. */
	double DurationSec = 0.0;
	ERoundEndReason EndReason = ERoundEndReason::ObjectiveWin;
	/** The WinLatched entry carries the Debug flag (R22). */
	bool bDebug = false;
	/** Team roster sizes at round start (the header's team rosters). */
	int32 TeamSize[2] = { 0, 0 };
};

/** One round's F7 values. */
struct FRoundPacingResult
{
	double PacingRatio = 0.0;
	bool bStallFlag = false;
	bool bAbortFlagged = false;
	/** B = min(n0, n1). */
	int32 Bucket = 0;
};

/** A share that may be unavailable ("n/a" when the denominator is 0). */
struct FRoundShare
{
	int32 Numerator = 0;
	int32 Denominator = 0;

	bool IsAvailable() const { return Denominator > 0; }
	/** The value; only meaningful when IsAvailable(). */
	double Value() const { return Denominator > 0 ? static_cast<double>(Numerator) / Denominator : 0.0; }
};

/** The Pillar 4 gate (ObjectiveEndShare >= 0.70 over >= 20 rounds in buckets of 4v4 and larger). */
enum class ERoundPillar4Gate : uint8
{
	Pass,
	Fail,
	InsufficientRounds,
};

/** Cross-round aggregates. */
struct FRoundPacingAggregate
{
	/** Per bucket B (min team size). */
	TMap<int32, FRoundShare> ObjectiveEndShareByBucket;
	TMap<int32, FRoundShare> AbortedActiveShareByBucket;
	/** Over every counted round. */
	FRoundShare ObjectiveEndShareAll;
	FRoundShare AbortedActiveShareAll;
	/** Over the win rounds of buckets >= the gate's minimum bucket. */
	FRoundShare Pillar4Share;
	ERoundPillar4Gate Pillar4 = ERoundPillar4Gate::InsufficientRounds;
};

/** The gate's analysis constants (from the GDD Pillar 4 statement; passed in, not gameplay values). */
struct FRoundPillar4GateSpec
{
	int32 MinBucket = 4;
	int32 MinRounds = 20;
	double MinShare = 0.70;
};

namespace StealthRoundPacing
{
	/** F7 for one round. */
	STEALTHFOUNDATION_API FRoundPacingResult ComputeRound(const FRoundPacingInput& Input, const FRoundPacingTuning& Tuning);

	/**
	 * Reads a round's summary from its log through the server-internal accessor: ActivePhaseStart as the store
	 * recorded it from the PhaseChanged entry into Active (phase value 3), the latch time, EndReason and Debug flag
	 * from WinLatched, the team sizes from the header. False (OutInput untouched) when the log has no WinLatched
	 * entry with a known EndReason or never entered Active.
	 */
	STEALTHFOUNDATION_API bool MakeInputFromLog(const FRoundEventLog& Log, FRoundPacingInput& OutInput);

	/** Aggregates rounds into the per-bucket and overall shares and evaluates the Pillar 4 gate. */
	STEALTHFOUNDATION_API FRoundPacingAggregate Aggregate(TConstArrayView<FRoundPacingInput> Rounds, const FRoundPillar4GateSpec& Gate);
}
