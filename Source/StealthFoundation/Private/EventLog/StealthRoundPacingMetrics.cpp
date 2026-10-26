#include "Server/EventLog/StealthRoundPacingMetrics.h"

#include "Server/EventLog/StealthRoundEventLog.h"

namespace
{
	int64 PacingToMilliseconds(double Seconds) { return FMath::RoundToInt64(Seconds * 1000.0); }

	bool IsWin(ERoundEndReason Reason)
	{
		return Reason == ERoundEndReason::ObjectiveWin || Reason == ERoundEndReason::Elimination || Reason == ERoundEndReason::ForfeitElimination;
	}

	bool IsAbortOrVoid(ERoundEndReason Reason)
	{
		return Reason == ERoundEndReason::Aborted || Reason == ERoundEndReason::VoidAbandoned || Reason == ERoundEndReason::VoidKickedLastActive
			|| Reason == ERoundEndReason::VoidPawnLost;
	}

	/** Rounds that enter any F7 count: wins, Active aborts and voids, never debug. */
	bool IsCounted(const FRoundPacingInput& Round)
	{
		return !Round.bDebug && Round.EndReason != ERoundEndReason::DebugForced && (IsWin(Round.EndReason) || IsAbortOrVoid(Round.EndReason));
	}

	void AddToShares(const FRoundPacingInput& Round, FRoundShare& ObjectiveShare, FRoundShare& AbortedShare)
	{
		if (Round.EndReason == ERoundEndReason::ObjectiveWin)
		{
			++ObjectiveShare.Numerator;
			++ObjectiveShare.Denominator;
		}
		else if (Round.EndReason == ERoundEndReason::Elimination)
		{
			++ObjectiveShare.Denominator;  // forfeits are excluded from ObjectiveEndShare
		}
		++AbortedShare.Denominator;
		if (IsAbortOrVoid(Round.EndReason))
		{
			++AbortedShare.Numerator;
		}
	}
}

FRoundPacingResult StealthRoundPacing::ComputeRound(const FRoundPacingInput& Input, const FRoundPacingTuning& Tuning)
{
	FRoundPacingResult Result;
	Result.PacingRatio = Tuning.TargetRoundMinutes > 0 ? Input.DurationSec / (60.0 * Tuning.TargetRoundMinutes) : 0.0;
	const int64 DurationMs = PacingToMilliseconds(Input.DurationSec);
	Result.bStallFlag = DurationMs > static_cast<int64>(Tuning.StallFlagMinutes) * 60000;
	Result.bAbortFlagged = Input.EndReason == ERoundEndReason::Aborted && DurationMs > static_cast<int64>(Tuning.StallFlagMinutes) * 30000;
	Result.Bucket = FMath::Min(Input.TeamSize[0], Input.TeamSize[1]);
	return Result;
}

bool StealthRoundPacing::MakeInputFromLog(const FRoundEventLog& Log, FRoundPacingInput& OutInput)
{
	const TOptional<double> ActiveStart = Log.GetActivePhaseStartServerInternal();
	bool bLatched = false;
	FRoundPacingInput Input;
	double LatchTime = 0.0;
	Log.ForEachEntryServerInternal([&](const FRoundLogEntry& Entry)
	{
		if (Entry.Type == ERoundLogType::WinLatched && !bLatched && Entry.Code <= static_cast<uint16>(ERoundEndReason::DebugForced))
		{
			bLatched = true;
			LatchTime = Entry.ServerTime;
			Input.EndReason = static_cast<ERoundEndReason>(Entry.Code);
			Input.bDebug = (Entry.Flags & RoundLogFlags::Debug) != 0;
		}
	});
	if (!ActiveStart.IsSet() || !bLatched)
	{
		return false;  // OutInput untouched
	}
	Input.DurationSec = LatchTime - ActiveStart.GetValue();
	if (const FRoundLogHeader* Header = Log.GetHeaderServerInternal())
	{
		Input.TeamSize[0] = Header->TeamRosters.Team[0].Num();
		Input.TeamSize[1] = Header->TeamRosters.Team[1].Num();
	}
	OutInput = Input;
	return true;
}

FRoundPacingAggregate StealthRoundPacing::Aggregate(TConstArrayView<FRoundPacingInput> Rounds, const FRoundPillar4GateSpec& Gate)
{
	FRoundPacingAggregate Aggregate;
	int32 GateRounds = 0;
	for (const FRoundPacingInput& Round : Rounds)
	{
		if (!IsCounted(Round))
		{
			continue;
		}
		const int32 Bucket = FMath::Min(Round.TeamSize[0], Round.TeamSize[1]);
		AddToShares(Round, Aggregate.ObjectiveEndShareByBucket.FindOrAdd(Bucket), Aggregate.AbortedActiveShareByBucket.FindOrAdd(Bucket));
		AddToShares(Round, Aggregate.ObjectiveEndShareAll, Aggregate.AbortedActiveShareAll);
		if (Bucket >= Gate.MinBucket && (Round.EndReason == ERoundEndReason::ObjectiveWin || Round.EndReason == ERoundEndReason::Elimination))
		{
			FRoundShare Unused;
			AddToShares(Round, Aggregate.Pillar4Share, Unused);
			++GateRounds;
		}
	}
	if (GateRounds < Gate.MinRounds || !Aggregate.Pillar4Share.IsAvailable())
	{
		Aggregate.Pillar4 = ERoundPillar4Gate::InsufficientRounds;
	}
	else
	{
		// Integer comparison in thousandths, so 0.70 exactly passes.
		const int64 MinShareThousandths = FMath::RoundToInt64(Gate.MinShare * 1000.0);
		const bool bPass = static_cast<int64>(Aggregate.Pillar4Share.Numerator) * 1000 >= MinShareThousandths * Aggregate.Pillar4Share.Denominator;
		Aggregate.Pillar4 = bPass ? ERoundPillar4Gate::Pass : ERoundPillar4Gate::Fail;
	}
	return Aggregate;
}
