#include "Server/CrowdDraws.h"

#include <cmath>

// ---- FCrowdDecisionCounters ----------------------------------------------------------------------------------------

uint32 FCrowdDecisionCounters::Take(uint32 Ordinal, ECrowdDecision Decision)
{
	const int32 Row = static_cast<int32>(Ordinal);
	if (Row >= Rows.Num())
	{
		Rows.SetNum(Row + 1);
	}
	uint32& Slot = Rows[Row].Index[static_cast<int32>(Decision)];
	return Slot++;
}

uint32 FCrowdDecisionCounters::Peek(uint32 Ordinal, ECrowdDecision Decision) const
{
	const int32 Row = static_cast<int32>(Ordinal);
	return Rows.IsValidIndex(Row) ? Rows[Row].Index[static_cast<int32>(Decision)] : 0u;
}

void FCrowdDecisionCounters::Reserve(int32 Count)
{
	if (Count > Rows.Num())
	{
		Rows.SetNum(Count);
	}
}

// ---- FCrowdDraws ---------------------------------------------------------------------------------------------------

FCrowdDraws::FCrowdDraws(uint64 SubSeed)
	: Stream(SubSeed, CrowdBehaviourStreamTag)
{
}

uint64 FCrowdDraws::PeekTuple(uint32 Ordinal, ECrowdDecision Decision, uint32 Index, uint32 Retry) const
{
	const uint64 Tuple[4] = { static_cast<uint64>(Ordinal), static_cast<uint64>(Decision), static_cast<uint64>(Index), static_cast<uint64>(Retry) };
	return Stream.Draw(MakeArrayView(Tuple));
}

uint64 FCrowdDraws::Draw(uint32 Ordinal, ECrowdDecision Decision)
{
	const uint32 Index = Counters.Take(Ordinal, Decision);
	return PeekTuple(Ordinal, Decision, Index, 0);
}

uint32 FCrowdDraws::DrawBelow(uint32 Ordinal, ECrowdDecision Decision, uint32 N)
{
	const uint32 Index = Counters.Take(Ordinal, Decision);
	LastRetryCount = 0;
	if (N == 0)
	{
		return 0;
	}
	uint32 Retry = 0;
	uint64 M = static_cast<uint64>(static_cast<uint32>(PeekTuple(Ordinal, Decision, Index, Retry))) * static_cast<uint64>(N);
	uint32 Low = static_cast<uint32>(M);
	if (Low < N)
	{
		// The one `%` computes Lemire's rejection threshold, never the result (ADR-0006 section 6).
		const uint32 Threshold = (0u - N) % N;
		while (Low < Threshold)
		{
			++Retry;
			M = static_cast<uint64>(static_cast<uint32>(PeekTuple(Ordinal, Decision, Index, Retry))) * static_cast<uint64>(N);
			Low = static_cast<uint32>(M);
		}
	}
	LastRetryCount = Retry;
	return static_cast<uint32>(M >> 32);
}

int32 FCrowdDraws::DrawWeighted(uint32 Ordinal, ECrowdDecision Decision, TConstArrayView<uint32> CumWeights)
{
	const uint32 Total = CumWeights.Num() > 0 ? CumWeights.Last() : 0u;
	const uint32 Pick = DrawBelow(Ordinal, Decision, Total);
	if (Total == 0)
	{
		return INDEX_NONE;
	}
	for (int32 Index = 0; Index < CumWeights.Num(); ++Index)
	{
		if (Pick < CumWeights[Index])
		{
			return Index;
		}
	}
	return CumWeights.Num() - 1;
}

double FCrowdDraws::DrawUnit(uint32 Ordinal, ECrowdDecision Decision)
{
	return UnitFraction53(Draw(Ordinal, Decision));
}

double FCrowdDraws::DrawUniform(uint32 Ordinal, ECrowdDecision Decision, double A, double B)
{
	return A + DrawUnit(Ordinal, Decision) * (B - A);
}

double FCrowdDraws::DrawExponential(uint32 Ordinal, ECrowdDecision Decision, double Lambda)
{
	const double U = DrawUnit(Ordinal, Decision);
	if (!(Lambda > 0.0))
	{
		return TNumericLimits<double>::Max();
	}
	return -std::log(1.0 - U) / Lambda;
}

double FCrowdDraws::DrawLogNormal(uint32 Ordinal, ECrowdDecision Decision, double Mu, double Sigma)
{
	return std::exp(Mu + Sigma * InvNormalAS241(DrawUnit(Ordinal, Decision)));
}

double FCrowdDraws::UnitFraction53(uint64 Bits)
{
	// 2^-53 exactly.
	constexpr double TwoPowMinus53 = 1.0 / 9007199254740992.0;
	// Exactly (x + 0.5) * 2^-53 except at the very top: for x = 2^53 - 1 the double sum rounds (ties to even) to 2^53, so
	// u would be 1.0; the guard returns the largest double below 1 instead, keeping u in the open interval (0, 1).
	constexpr double LargestBelowOne = 1.0 - TwoPowMinus53;
	const double U = (static_cast<double>(Bits >> 11) + 0.5) * TwoPowMinus53;
	return U < 1.0 ? U : LargestBelowOne;
}

double FCrowdDraws::InvNormalAS241(double U)
{
	// Wichura, M. J. (1988), Algorithm AS 241: The percentage points of the normal distribution, PPND16.
	const double Q = U - 0.5;
	if (std::fabs(Q) <= 0.425)
	{
		const double R = 0.180625 - Q * Q;
		const double Num = ((((((R * 2509.0809287301226727 + 33430.575583588128105) * R + 67265.770927008700853) * R
			+ 45921.953931549871457) * R + 13731.693765509461125) * R + 1971.5909503065514427) * R + 133.14166789178437745) * R
			+ 3.387132872796366608;
		const double Den = ((((((R * 5226.495278852545925 + 28729.085735721942674) * R + 39307.89580009271061) * R
			+ 21213.794301586595867) * R + 5394.1960214247511077) * R + 687.1870074920579083) * R + 42.313330701600911252) * R
			+ 1.0;
		return Q * Num / Den;
	}
	double R = Q < 0.0 ? U : 1.0 - U;
	R = std::sqrt(-std::log(R));
	double Value;
	if (R <= 5.0)
	{
		R -= 1.6;
		const double Num = ((((((R * 7.7454501427834140764e-4 + 0.0227238449892691845833) * R + 0.24178072517745061177) * R
			+ 1.27045825245236838258) * R + 3.64784832476320460504) * R + 5.7694972214606914055) * R + 4.6303378461565452959) * R
			+ 1.42343711074968357734;
		const double Den = ((((((R * 1.05075007164441684324e-9 + 5.475938084995344946e-4) * R + 0.0151986665636164571966) * R
			+ 0.14810397642748007459) * R + 0.68976733498510000455) * R + 1.6763848301838038494) * R + 2.05319162663775882187) * R
			+ 1.0;
		Value = Num / Den;
	}
	else
	{
		R -= 5.0;
		const double Num = ((((((R * 2.01033439929228813265e-7 + 2.71155556874348757815e-5) * R + 0.0012426609473880784386) * R
			+ 0.026532189526576123093) * R + 0.29656057182850489123) * R + 1.7848265399172913358) * R + 5.4637849111641143699) * R
			+ 6.6579046435011037772;
		const double Den = ((((((R * 2.04426310338993978564e-15 + 1.4215117583164458887e-7) * R + 1.8463183175100546818e-5) * R
			+ 7.868691311456132591e-4) * R + 0.0148753612908506148525) * R + 0.13692988092273580531) * R + 0.59983220655588793769) * R
			+ 1.0;
		Value = Num / Den;
	}
	return Q < 0.0 ? -Value : Value;
}
