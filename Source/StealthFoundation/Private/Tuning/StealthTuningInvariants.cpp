// Stories tdc-048 / tdc-055: the invariant table (TuningInvariants.cpp of ADR-0004 section 3). See
// Server/StealthTuningInvariants.h. One row per GDD invariant; each Check is pure over the clamped
// const set and returns Pass, Warn or Reject. A new cross-field constraint arrives through a TDC change
// request (CR-15.6) and is added here as a row, never as a check inside the owner's system.

#include "Server/StealthTuningInvariants.h"

#include "Server/StealthServerTuning.h"
#include "Server/StealthTuningConstants.h"
#include "Server/StealthTuningDerived.h"
#include "Misc/StringBuilder.h"

namespace
{
	/** I-1: DefenceCooldownSec >= 8 * DefenceDurationSec (float comparison; 8 * is exact). Reject. */
	ETuningCheck CheckI1(const FStealthServerTuning& Values, const FTuningDerived&, const FTuningCheckContext&)
	{
		const float Duration = Values.Assassination_Public.DefenceDurationSec;
		const float Cooldown = Values.Assassination_Public.DefenceCooldownSec;
		return Cooldown >= 8.0f * Duration ? ETuningCheck::Pass : ETuningCheck::Reject;
	}

	/** I-2: NpcCount >= 2 * TeamSize (CrowdRatio >= 1). Reject. */
	ETuningCheck CheckI2(const FStealthServerTuning& Values, const FTuningDerived&, const FTuningCheckContext&)
	{
		return Values.Crowd_Server.NpcCount >= 2 * Values.Round_Public.TeamSize ? ETuningCheck::Pass : ETuningCheck::Reject;
	}

	/**
	 * I-3: 2 * TeamSize + NpcCount >= DeckCopiesFloorCount * DeckActiveAppearancesMinCount (24 at the
	 * defaults). Reject in a public preset (Default, HostSelectable), warn in a non-selectable one.
	 */
	ETuningCheck CheckI3(const FStealthServerTuning& Values, const FTuningDerived&, const FTuningCheckContext& Context)
	{
		const int32 Floor = StealthTuningPendingOwnerKeys::DeckCopiesFloorCount * StealthTuningPendingOwnerKeys::DeckActiveAppearancesMinCount;
		if (2 * Values.Round_Public.TeamSize + Values.Crowd_Server.NpcCount >= Floor)
		{
			return ETuningCheck::Pass;
		}
		return Context.bIsPublic ? ETuningCheck::Reject : ETuningCheck::Warn;
	}

	/**
	 * I-4: 100 * BlindGuessSuccess < BlindGuessWarnPct (schema constant 50). Reject in a public preset
	 * (HostSelectable or Default), warn in a non-selectable one.
	 */
	ETuningCheck CheckI4(const FStealthServerTuning&, const FTuningDerived& Derived, const FTuningCheckContext& Context)
	{
		if (100.0 * Derived.BlindGuessSuccess < StealthTuningConstants::BlindGuessWarnPct)
		{
			return ETuningCheck::Pass;
		}
		return Context.bIsPublic ? ETuningCheck::Reject : ETuningCheck::Warn;
	}

	/** I-5: Min <= Max for every range pair of the context's schema view. Reject. */
	ETuningCheck CheckI5(const FStealthServerTuning& Values, const FTuningDerived&, const FTuningCheckContext& Context)
	{
		for (const FTuningRangePair& Pair : FindTuningRangePairs(Context.SchemaOrGenerated()))
		{
			if (Pair.Min->Get != nullptr && Pair.Max->Get != nullptr && Pair.Min->Get(Values) > Pair.Max->Get(Values))
			{
				return ETuningCheck::Reject;
			}
		}
		return ETuningCheck::Pass;
	}

	/** The preset-load rows (none offline), TDC rows first. Owner epics append their rows here. */
	const FTuningInvariant LoadRows[] = {
		{ "I-1", "TDC", &CheckI1 },
		{ "I-2", "TDC", &CheckI2 },
		{ "I-3", "TDC", &CheckI3 },
		{ "I-4", "TDC", &CheckI4 },
		{ "I-5", "TDC", &CheckI5 },
	};

	/** True when Key ends with Tail (byte-wise). */
	bool EndsWith(FAnsiStringView Key, FAnsiStringView Tail)
	{
		return Key.Len() >= Tail.Len() && Key.Right(Tail.Len()).Equals(Tail, ESearchCase::CaseSensitive);
	}
}

TConstArrayView<FTuningInvariant> GetTuningInvariants()
{
	return MakeArrayView(LoadRows);
}

TConstArrayView<FTuningInvariant> GetTuningOfflineInvariants()
{
	// No offline row is registered yet (O-CNB-6, O-CNB-7, I-SCL-10/11/14 and the LSC layout predicate
	// arrive with their owner epics and are enforced by those owners' CI or load-time checks).
	return TConstArrayView<FTuningInvariant>();
}

TArray<FTuningRangePair> FindTuningRangePairs(TConstArrayView<FTuningSchemaRow> Schema)
{
	TArray<FTuningRangePair> Pairs;
	for (const FTuningSchemaRow& MinRow : Schema)
	{
		if (MinRow.Key == nullptr || MinRow.Unit == nullptr)
		{
			continue;
		}
		const FAnsiStringView Key(MinRow.Key);
		const FAnsiStringView Unit(MinRow.Unit);
		TAnsiStringBuilder<64> MinTail;
		MinTail << "Min" << Unit;
		if (!EndsWith(Key, MinTail.ToView()) || Key.Len() == MinTail.Len())
		{
			continue;
		}
		TAnsiStringBuilder<256> MaxKey;
		MaxKey << Key.LeftChop(MinTail.Len()) << "Max" << Unit;
		for (const FTuningSchemaRow& MaxRow : Schema)
		{
			if (MaxRow.Key != nullptr && MaxRow.Unit != nullptr && FAnsiStringView(MaxRow.Key).Equals(MaxKey.ToView(), ESearchCase::CaseSensitive) && FAnsiStringView(MaxRow.Unit).Equals(Unit, ESearchCase::CaseSensitive))
			{
				Pairs.Add({ &MinRow, &MaxRow });
				break;
			}
		}
	}
	return Pairs;
}
