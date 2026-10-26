#include "Server/CrowdTellSampler.h"

#include "Server/StealthTuningSubstructs.h"

void FCrowdTellSampler::TargetRatesPerMin(const FCrowdTuning_Server& T, double OutRates[static_cast<int32>(ECrowdTell::Count)])
{
	const float Intervals[] = { T.CrowdRunIntervalSec, T.CrowdVaultIntervalSec, T.CrowdClimbIntervalSec, T.CrowdBraceIntervalSec,
		T.CrowdDecoyTendIntervalSec, T.CrowdBushIntervalSec, T.CrowdReversalIntervalSec, T.CrowdHeadHoldIntervalSec };
	static_assert(UE_ARRAY_COUNT(Intervals) == static_cast<int32>(ECrowdTell::Count), "one interval key per tell");
	for (int32 Index = 0; Index < static_cast<int32>(ECrowdTell::Count); ++Index)
	{
		OutRates[Index] = 60.0 / FMath::Max(static_cast<double>(Intervals[Index]), 1.0);
	}
}

double FCrowdTellSampler::CompensatedRatePerMin(const FCrowdTuning_Server& Tuning, ECrowdTell Tell)
{
	double Rates[static_cast<int32>(ECrowdTell::Count)];
	TargetRatesPerMin(Tuning, Rates);
	double Lambda = 0.0;
	for (const double Rate : Rates)
	{
		Lambda += Rate;
	}
	const double Rj = Rates[static_cast<int32>(Tell)];
	const double Gap = Tuning.CrowdTellChainGapSec / 60.0;
	// I-CNB-5 keeps (Lambda - r_j) * T_gap <= 0.5, so the denominator stays >= 0.5.
	return Rj / FMath::Max(1.0 - (Lambda - Rj) * Gap, 0.5);
}

double FCrowdTellSampler::EffectiveRatePerMin(const FCrowdTuning_Server& Tuning, ECrowdTell Tell) const
{
	double Rates[static_cast<int32>(ECrowdTell::Count)];
	TargetRatesPerMin(Tuning, Rates);
	double Lambda = 0.0;
	for (int32 Index = 0; Index < static_cast<int32>(ECrowdTell::Count); ++Index)
	{
		Lambda += bEnabledTells[Index] ? Rates[Index] : 0.0;
	}
	const double Rj = Rates[static_cast<int32>(Tell)];
	const double Gap = Tuning.CrowdTellChainGapSec / 60.0;
	return Rj / FMath::Max(1.0 - FMath::Max(Lambda - Rj, 0.0) * Gap, 0.5);
}

ECrowdDecision FCrowdTellSampler::ClockDecision(ECrowdTell Tell)
{
	switch (Tell)
	{
	case ECrowdTell::Run: return ECrowdDecision::RunClock;
	case ECrowdTell::Vault: return ECrowdDecision::VaultClock;
	case ECrowdTell::Climb: return ECrowdDecision::ClimbClock;
	case ECrowdTell::Brace: return ECrowdDecision::BraceClock;
	case ECrowdTell::DecoyTend: return ECrowdDecision::DecoyClock;
	case ECrowdTell::Bush: return ECrowdDecision::BushClock;
	case ECrowdTell::Reversal: return ECrowdDecision::ReversalClock;
	default: return ECrowdDecision::HeadHoldClock;
	}
}

void FCrowdTellSampler::SetEnabled(ECrowdTell Tell, bool bEnabled)
{
	bEnabledTells[static_cast<int32>(Tell)] = bEnabled;
}

void FCrowdTellSampler::Count(int32 Field, ECrowdTell Tell)
{
	const int32 Index = static_cast<int32>(Tell);
	int64* Arrays[] = { Counters.Draws, Counters.DiscardedInGap, Counters.Accepted, Counters.Performed };
	++Arrays[Field][Index];
	if (Telemetry != nullptr)
	{
		Telemetry->AddCrowdCounter(Tell, Field, 1);
	}
}

void FCrowdTellSampler::StartNpc(uint32 Ordinal, double Now, const FCrowdTuning_Server& Tuning, FCrowdDraws& Draws)
{
	FNpcClocks& Clocks = Npcs.FindOrAdd(Ordinal);
	for (int32 Index = 0; Index < static_cast<int32>(ECrowdTell::Count); ++Index)
	{
		const ECrowdTell Tell = static_cast<ECrowdTell>(Index);
		if (bEnabledTells[Index])
		{
			Clocks.NextDue[Index] = Now + Draws.DrawExponential(Ordinal, ClockDecision(Tell), EffectiveRatePerMin(Tuning, Tell) / 60.0);
		}
	}
}

void FCrowdTellSampler::Advance(uint32 Ordinal, double Now, const FCrowdTuning_Server& Tuning, FCrowdDraws& Draws, TArray<ECrowdTell, TInlineAllocator<2>>& OutAccepted)
{
	FNpcClocks* Clocks = Npcs.Find(Ordinal);
	if (Clocks == nullptr)
	{
		return;
	}
	const double Gap = Tuning.CrowdTellChainGapSec;
	for (int32 Index = 0; Index < static_cast<int32>(ECrowdTell::Count); ++Index)
	{
		if (!bEnabledTells[Index] || Clocks->NextDue[Index] < 0.0 || Clocks->NextDue[Index] > Now)
		{
			continue;
		}
		const ECrowdTell Tell = static_cast<ECrowdTell>(Index);
		const double Fired = Clocks->NextDue[Index];
		Count(0, Tell);
		const double Rate = EffectiveRatePerMin(Tuning, Tell) / 60.0;
		// The chain gap: the latest different tell of this NPC.
		double LatestOther = -1.0e9;
		for (int32 Other = 0; Other < static_cast<int32>(ECrowdTell::Count); ++Other)
		{
			if (Other != Index)
			{
				LatestOther = FMath::Max(LatestOther, Clocks->LastTell[Other]);
			}
		}
		if (Fired - LatestOther < Gap)
		{
			// Discarded: the clock restarts at the gap's end, so tells never pile up just after a gap.
			Count(1, Tell);
			Clocks->NextDue[Index] = LatestOther + Gap + Draws.DrawExponential(Ordinal, ClockDecision(Tell), Rate);
			continue;
		}
		Count(2, Tell);
		Clocks->LastTell[Index] = Fired;
		Clocks->NextDue[Index] = Fired + Draws.DrawExponential(Ordinal, ClockDecision(Tell), Rate);
		OutAccepted.Add(Tell);
	}
}

bool FCrowdTellSampler::IsInChainGap(uint32 Ordinal, ECrowdTell Tell, double Now, const FCrowdTuning_Server& Tuning) const
{
	const FNpcClocks* Clocks = Npcs.Find(Ordinal);
	if (Clocks == nullptr)
	{
		return false;
	}
	for (int32 Other = 0; Other < static_cast<int32>(ECrowdTell::Count); ++Other)
	{
		if (Other != static_cast<int32>(Tell) && Now - Clocks->LastTell[Other] < Tuning.CrowdTellChainGapSec)
		{
			return true;
		}
	}
	return false;
}

void FCrowdTellSampler::NotePerformed(uint32 Ordinal, ECrowdTell Tell, double Now)
{
	Count(3, Tell);
	if (FNpcClocks* Clocks = Npcs.Find(Ordinal))
	{
		Clocks->LastTell[static_cast<int32>(Tell)] = FMath::Max(Clocks->LastTell[static_cast<int32>(Tell)], Now);
	}
}

double FCrowdTellSampler::GetNextDue(uint32 Ordinal, ECrowdTell Tell) const
{
	const FNpcClocks* Clocks = Npcs.Find(Ordinal);
	return Clocks != nullptr ? Clocks->NextDue[static_cast<int32>(Tell)] : -1.0;
}
