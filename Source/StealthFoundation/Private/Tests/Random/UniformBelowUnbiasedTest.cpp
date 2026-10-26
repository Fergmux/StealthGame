// Story st-007 (production/epics/session-travel/story-007-ranged-draw-helpers-unbiased.md): UniformBelow,
// UniformInRange, WeightedPick and Fisher-Yates are unbiased and edge-safe (ADR-0006 section 6, TD-ADR item 9).
//   1. Exhaustive window: for every n in 1..64, a sweep stream whose 2^16 outputs span the 32-bit range evenly
//      (x_k = k * 2^16 + 2^15) maps onto [0, n) with bucket counts differing by at most 1 (the best any 2^16 inputs
//      can do), and no window output falls in the rejection zone.
//   2. Rejection: an output in the rejection zone (low word < (2^32 - n) mod n) is discarded and the next one used.
//   3. Chi-square at a fixed seed (FIntStream(42, 54), fresh per n) for n in {3, 24, 1000, 2^31 - 1} (the last
//      binned into 64 equal-width bins) below the p = 0.001 critical values (Wilson-Hilferty for df 999 and 63).
//   4. UniformBelow(0) is refused (returns 0, consumes nothing); UniformInRange(INT32_MIN, INT32_MAX) returns
//      NextU32() directly; Lo > Hi is refused (returns Lo, consumes nothing).
//   5. Fisher-Yates over a canonically sorted array is a deterministic permutation of the stream.
//   6. WeightedPick never picks a zero weight, follows the weights (chi-square) and refuses bad input.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "View/Random/StealthIntStream.h"
#include "View/Random/StealthRangedDraw.h"

namespace UniformBelowUnbiasedTestData
{
	/** A stream replaying a fixed list of outputs (then 0), counting how many were consumed. */
	struct FScriptedStream
	{
		TArray<uint32> Outputs;
		int32 Consumed = 0;
		uint32 NextU32()
		{
			const uint32 Value = Outputs.IsValidIndex(Consumed) ? Outputs[Consumed] : 0u;
			++Consumed;
			return Value;
		}
	};

	/** The exhaustive sweep window: 2^16 outputs evenly spanning the 32-bit range. */
	struct FSweepStream
	{
		uint32 K = 0;
		uint32 NextU32() { return (K++ << 16) | 0x8000u; }
	};

	struct FChiCase
	{
		uint32 N;
		int32 Samples;
		int32 Bins;          // 0: one bin per value
		double Critical;     // p = 0.001
	};

	constexpr FChiCase ChiCases[] = {
		{ 3u, 30000, 0, 13.816 },
		{ 24u, 48000, 0, 49.728 },
		{ 1000u, 200000, 0, 1142.8 },
		{ 2147483647u, 64000, 64, 103.5 },
	};

	double ChiSquare(const FChiCase& Case)
	{
		FIntStream Stream(42u, 54u);
		const int32 Buckets = Case.Bins > 0 ? Case.Bins : static_cast<int32>(Case.N);
		TArray<int64> Counts;
		Counts.SetNumZeroed(Buckets);
		for (int32 Draw = 0; Draw < Case.Samples; ++Draw)
		{
			const uint64 Value = StealthRandom::UniformBelow(Stream, Case.N);
			const int32 Bucket = Case.Bins > 0 ? static_cast<int32>((Value * static_cast<uint64>(Case.Bins)) / Case.N) : static_cast<int32>(Value);
			++Counts[Bucket];
		}
		double Chi = 0.0;
		for (int32 Bucket = 0; Bucket < Buckets; ++Bucket)
		{
			// Values per bucket: ceil((b + 1) * n / bins) - ceil(b * n / bins) (exact integer bin sizes).
			const uint64 Lo = Case.Bins > 0 ? (static_cast<uint64>(Bucket) * Case.N + Case.Bins - 1) / Case.Bins : static_cast<uint64>(Bucket);
			const uint64 Hi = Case.Bins > 0 ? (static_cast<uint64>(Bucket + 1) * Case.N + Case.Bins - 1) / Case.Bins : static_cast<uint64>(Bucket + 1);
			const double Expected = static_cast<double>(Case.Samples) * static_cast<double>(Hi - Lo) / static_cast<double>(Case.N);
			const double Delta = static_cast<double>(Counts[Bucket]) - Expected;
			Chi += Delta * Delta / Expected;
		}
		return Chi;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRandomUniformBelowUnbiasedTest, "StealthGame.Random.UniformBelowUnbiased",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRandomUniformBelowUnbiasedTest::RunTest(const FString& Parameters)
{
	using namespace UniformBelowUnbiasedTestData;

	// 1. Exhaustive window, n = 1..64.
	for (uint32 N = 1; N <= 64; ++N)
	{
		FSweepStream Sweep;
		TArray<int32> Counts;
		Counts.SetNumZeroed(N);
		for (int32 Index = 0; Index < 65536; ++Index)
		{
			++Counts[StealthRandom::UniformBelow(Sweep, N)];
		}
		int32 Min = MAX_int32;
		int32 Max = 0;
		for (const int32 Count : Counts)
		{
			Min = FMath::Min(Min, Count);
			Max = FMath::Max(Max, Count);
		}
		TestTrue(FString::Printf(TEXT("n=%u: bucket counts differ by at most 1 over the 2^16 window (min %d, max %d)"), N, Min, Max), Max - Min <= 1);
		TestEqual(FString::Printf(TEXT("n=%u: the window consumed exactly 2^16 outputs (no rejection)"), N), Sweep.K, 65536u);
	}

	// 2. Rejection: for n = 3 the threshold is (2^32 - 3) mod 3 = 1, so x = 0 (low word 0) is rejected.
	{
		FScriptedStream Script;
		Script.Outputs = { 0u, 0xC0000000u };
		const uint32 Value = StealthRandom::UniformBelow(Script, 3u);
		TestEqual(TEXT("Rejected output discarded: the second output decides"), Value, 2u);
		TestEqual(TEXT("Rejection consumed two outputs"), Script.Consumed, 2);
	}

	// 3. Chi-square at a fixed seed.
	for (const FChiCase& Case : ChiCases)
	{
		const double Chi = ChiSquare(Case);
		TestTrue(FString::Printf(TEXT("n=%u: chi-square %.2f below the p=0.001 critical value %.2f"), Case.N, Chi, Case.Critical), Chi < Case.Critical);
	}

	// 4. Edge cases.
	{
		FScriptedStream Script;
		Script.Outputs = { 7u };
		TestEqual(TEXT("UniformBelow(0) is refused: returns 0"), StealthRandom::UniformBelow(Script, 0u), 0u);
		TestEqual(TEXT("UniformBelow(0) consumes nothing"), Script.Consumed, 0);
	}
	{
		FIntStream Stream(7u, 9u);
		FIntStream Twin(7u, 9u);
		const int32 Full = StealthRandom::UniformInRange(Stream, MIN_int32, MAX_int32);
		TestEqual(TEXT("A span that wraps to 0 returns NextU32() directly"), static_cast<uint32>(Full), Twin.NextU32());
		FScriptedStream Script;
		Script.Outputs = { 7u };
		TestEqual(TEXT("Lo > Hi is refused: returns Lo"), StealthRandom::UniformInRange(Script, 5, 4), 5);
		TestEqual(TEXT("Lo > Hi consumes nothing"), Script.Consumed, 0);
		FIntStream Range(3u, 4u);
		bool bInside = true;
		for (int32 Draw = 0; Draw < 10000; ++Draw)
		{
			const int32 Value = StealthRandom::UniformInRange(Range, -3, 3);
			bInside &= Value >= -3 && Value <= 3;
		}
		TestTrue(TEXT("UniformInRange(-3, 3) stays inside [-3, 3]"), bInside);
	}

	// 5. Fisher-Yates determinism over a canonically sorted array.
	{
		TArray<int32> A;
		for (int32 Index = 0; Index < 32; ++Index)
		{
			A.Add(Index);
		}
		TArray<int32> B = A;
		const TArray<int32> Sorted = A;
		FIntStream StreamA(11u, 12u);
		FIntStream StreamB(11u, 12u);
		StealthRandom::FisherYates(StreamA, MakeArrayView(A));
		StealthRandom::FisherYates(StreamB, MakeArrayView(B));
		TestTrue(TEXT("Same stream, same sorted input: same permutation"), A == B);
		TArray<int32> Check = A;
		Check.Sort();
		TestTrue(TEXT("The result is a permutation of the input"), Check == Sorted);
		TestTrue(TEXT("The permutation is not the identity"), A != Sorted);
	}

	// 6. WeightedPick.
	{
		const uint32 Cumulative[] = { 10u, 10u, 40u, 100u }; // weights 10, 0, 30, 60
		FIntStream Stream(5u, 6u);
		int32 Counts[4] = { 0, 0, 0, 0 };
		const int32 Samples = 50000;
		for (int32 Draw = 0; Draw < Samples; ++Draw)
		{
			const int32 Pick = StealthRandom::WeightedPick(Stream, TConstArrayView<uint32>(Cumulative, 4));
			if (Pick >= 0 && Pick < 4)
			{
				++Counts[Pick];
			}
		}
		TestEqual(TEXT("A zero weight is never picked"), Counts[1], 0);
		const double Weights[4] = { 10.0, 0.0, 30.0, 60.0 };
		double Chi = 0.0;
		for (int32 Index = 0; Index < 4; ++Index)
		{
			if (Weights[Index] > 0.0)
			{
				const double Expected = Samples * Weights[Index] / 100.0;
				Chi += (Counts[Index] - Expected) * (Counts[Index] - Expected) / Expected;
			}
		}
		TestTrue(FString::Printf(TEXT("WeightedPick follows the weights (chi-square %.2f < 13.82, df 2)"), Chi), Chi < 13.816);
		FScriptedStream Script;
		Script.Outputs = { 1u };
		const uint32 Empty[] = { 0u };
		const uint32 Decreasing[] = { 5u, 3u };
		TestEqual(TEXT("Empty weights are refused"), StealthRandom::WeightedPick(Script, TConstArrayView<uint32>()), static_cast<int32>(INDEX_NONE));
		TestEqual(TEXT("A zero total is refused"), StealthRandom::WeightedPick(Script, TConstArrayView<uint32>(Empty, 1)), static_cast<int32>(INDEX_NONE));
		TestEqual(TEXT("A decreasing cumulative weight is refused"), StealthRandom::WeightedPick(Script, TConstArrayView<uint32>(Decreasing, 2)), static_cast<int32>(INDEX_NONE));
		TestEqual(TEXT("Refused picks consume nothing"), Script.Consumed, 0);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
