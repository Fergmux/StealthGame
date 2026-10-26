#pragma once

// Statistics helpers of the StealthGame.Crowd.* specs: one-sample Kolmogorov-Smirnov with the asymptotic p-value, the
// binomial and Poisson intervals the GDD acceptance criteria name. Deterministic (no draws).

#include "CoreMinimal.h"

namespace StealthCrowdStats
{
	/** The Kolmogorov distribution tail Q(lambda) = 2 sum (-1)^(k-1) exp(-2 k^2 lambda^2). */
	inline double KolmogorovTail(double Lambda)
	{
		if (Lambda < 0.27)
		{
			return 1.0;
		}
		double Sum = 0.0;
		for (int32 K = 1; K <= 100; ++K)
		{
			const double Term = FMath::Exp(-2.0 * K * K * Lambda * Lambda);
			Sum += (K % 2 == 1 ? 1.0 : -1.0) * Term;
			if (Term < 1e-12)
			{
				break;
			}
		}
		return FMath::Clamp(2.0 * Sum, 0.0, 1.0);
	}

	/** One-sample KS against Cdf: the statistic D and the asymptotic p-value (Stephens' small-sample correction). */
	inline double KsPValue(TArray<double> Samples, TFunctionRef<double(double)> Cdf, double* OutD = nullptr)
	{
		Samples.Sort();
		const int32 N = Samples.Num();
		double D = 0.0;
		for (int32 Index = 0; Index < N; ++Index)
		{
			const double F = Cdf(Samples[Index]);
			D = FMath::Max(D, FMath::Max(F - static_cast<double>(Index) / N, static_cast<double>(Index + 1) / N - F));
		}
		if (OutD != nullptr)
		{
			*OutD = D;
		}
		const double Root = FMath::Sqrt(static_cast<double>(N));
		return N > 0 ? KolmogorovTail((Root + 0.12 + 0.11 / Root) * D) : 0.0;
	}

	/** The z quantile of a two-sided interval (99 %: 2.5758, 95 %: 1.96). */
	inline constexpr double Z99 = 2.5758293035489;

	/** True when Count successes of N lie in the two-sided normal-approximation 99 % binomial interval around P. */
	inline bool InBinomial99(int64 Count, int64 N, double P, double* OutLow = nullptr, double* OutHigh = nullptr)
	{
		const double Mean = N * P;
		const double Sd = FMath::Sqrt(N * P * (1.0 - P));
		const double Low = Mean - Z99 * Sd;
		const double High = Mean + Z99 * Sd;
		if (OutLow) { *OutLow = Low / FMath::Max<double>(N, 1); }
		if (OutHigh) { *OutHigh = High / FMath::Max<double>(N, 1); }
		return Count >= Low && Count <= High;
	}

	/** True when Count events lie in the 99 % Poisson interval of the expected Mean (normal approximation, Mean > 30). */
	inline bool InPoisson99(int64 Count, double Mean, double* OutLow = nullptr, double* OutHigh = nullptr)
	{
		const double Sd = FMath::Sqrt(FMath::Max(Mean, 0.0));
		const double Low = Mean - Z99 * Sd;
		const double High = Mean + Z99 * Sd;
		if (OutLow) { *OutLow = Low; }
		if (OutHigh) { *OutHigh = High; }
		return Count >= Low && Count <= High;
	}

	/** The uniform CDF on [A, B]. */
	inline double UniformCdf(double X, double A, double B)
	{
		return FMath::Clamp((X - A) / FMath::Max(B - A, UE_DOUBLE_SMALL_NUMBER), 0.0, 1.0);
	}
}
