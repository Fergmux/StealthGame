// Story asn-026 AC1 / AC-ASN-32 [Config/ADVISORY] (GDD design/gdd/assassination-defence-reversal.md, F5 "Blind-guess
// economics"): StealthGame.Assassination.ProbeEconomicsF5. F5 is a balance model, not runtime code (TDC I-4 has not adopted
// it), so FProbeEconomicsF5 lives in this test only. At the TDC defaults (TeamSize 5, NpcCount 50, NpcKillStrikeLimit L 3,
// p = TeamSize / (TeamSize + NpcCount), b_P 0.067, b_N 0.00135):
//   E[N] = 2.98 +- 0.01, E[RivalKills] = 0.276 +- 0.002, P(>= 1 kill) = 0.232 +- 0.002, P(strike-out) = 0.976 +- 0.002,
//   P(reversed) = 0.024 +- 0.002 (strike-out and reversed are the two exclusive terminal outcomes: they sum to 1), and about
//   10.7 visible NPC kills per rival kill.
// Edge cases (QA): p = 0 (no rival: no kill, ever), p = 1 (every target a rival: the first non-kill ends the life by
// reversal), L = 1, b_P = 0 (no rival braces); and the TDC check: TDC's BlindGuessSuccess = 1 - (1 - p)^L and
// ExpectedBlindKills = L p / (1 - p) ignore reversals, so they are never below F5's P(>= 1 kill) and E[RivalKills] (the safe
// direction for I-4), 7-9 % optimistic at the defaults; a non-default TeamSize / NpcCount is reported with its optimism.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace ProbeEconomicsF5TestData
{
	/** F5's inputs (GDD F5 variables). */
	struct FInputs
	{
		int32 TeamSize = 5;
		int32 NpcCount = 50;
		/** NpcKillStrikeLimit. */
		int32 L = 3;
		/** Upper bound on a rival's braced share (F3 Duty). */
		double BP = 0.067;
		/** NPC braced share (Crowd F2 T4). */
		double BN = 0.00135;

		/** Share of non-ally pawns that are Active rivals (GDD F5: TeamSize / (TeamSize + NpcCount)). */
		double P() const { return static_cast<double>(TeamSize) / static_cast<double>(TeamSize + NpcCount); }
	};

	/** F5's outputs per active life of a uniform blind prober. */
	struct FOutputs
	{
		double Q = 0.0;
		double Rho = 0.0;
		double M = 0.0;
		double Pi = 0.0;
		double ExpectedN = 0.0;
		double ExpectedRivalKills = 0.0;
		double PStrikeOut = 0.0;
		double PReversed = 0.0;
		double PAtLeastOneKill = 0.0;
		/** Visible NPC kills per rival kill: (E[N] - P(reversed)) / E[RivalKills]. */
		double NpcKillsPerRivalKill = 0.0;
	};

	/**
	 * FProbeEconomicsF5 (GDD F5): q = p(1 - b_P), rho = p b_P + (1 - p) b_N, m = (1 - p)(1 - b_N), pi = m / (m + rho);
	 * E[N] = sum_{i<L} pi^i, E[RivalKills] = E[N] q / (1 - q), P(StrikeOut) = pi^L, P(Reversed) = 1 - pi^L,
	 * P(>= 1 kill) = 1 - [rho sum_{i<L} m^i + m^L].
	 */
	struct FProbeEconomicsF5
	{
		static FOutputs Evaluate(const FInputs& In)
		{
			FOutputs Out;
			const double P = In.P();
			Out.Q = P * (1.0 - In.BP);
			Out.Rho = P * In.BP + (1.0 - P) * In.BN;
			Out.M = (1.0 - P) * (1.0 - In.BN);
			Out.Pi = Out.M + Out.Rho > 0.0 ? Out.M / (Out.M + Out.Rho) : 0.0;
			double SumPi = 0.0;
			double SumM = 0.0;
			for (int32 I = 0; I < In.L; ++I)
			{
				SumPi += FMath::Pow(Out.Pi, static_cast<double>(I));
				SumM += FMath::Pow(Out.M, static_cast<double>(I));
			}
			Out.ExpectedN = SumPi;
			Out.ExpectedRivalKills = Out.Q < 1.0 ? Out.ExpectedN * Out.Q / (1.0 - Out.Q) : TNumericLimits<double>::Max();
			Out.PStrikeOut = FMath::Pow(Out.Pi, static_cast<double>(In.L));
			Out.PReversed = 1.0 - Out.PStrikeOut;
			Out.PAtLeastOneKill = 1.0 - (Out.Rho * SumM + FMath::Pow(Out.M, static_cast<double>(In.L)));
			Out.NpcKillsPerRivalKill = Out.ExpectedRivalKills > 0.0 ? (Out.ExpectedN - Out.PReversed) / Out.ExpectedRivalKills : 0.0;
			return Out;
		}

		/** TDC F-3's BlindGuessSuccess (reversals ignored): 1 - (1 - p)^L. */
		static double TdcBlindGuessSuccess(const FInputs& In) { return 1.0 - FMath::Pow(1.0 - In.P(), static_cast<double>(In.L)); }

		/** TDC F-3's ExpectedBlindKills (reversals ignored): L p / (1 - p). */
		static double TdcExpectedBlindKills(const FInputs& In) { return In.L * In.P() / (1.0 - In.P()); }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAssassinationProbeEconomicsF5Test, "StealthGame.Assassination.ProbeEconomicsF5",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAssassinationProbeEconomicsF5Test::RunTest(const FString& Parameters)
{
	using namespace ProbeEconomicsF5TestData;
	// Arrange: the TDC defaults (the AC's boundary values are the point of the check).
	const FInputs Defaults;

	// Act
	const FOutputs Out = FProbeEconomicsF5::Evaluate(Defaults);
	AddInfo(FString::Printf(TEXT("F5 defaults: p %.4f q %.4f rho %.5f m %.4f pi %.5f | E[N] %.4f E[RivalKills] %.4f P(>=1) %.4f P(strike-out) %.4f P(reversed) %.4f NPC kills per rival kill %.2f"),
		Defaults.P(), Out.Q, Out.Rho, Out.M, Out.Pi, Out.ExpectedN, Out.ExpectedRivalKills, Out.PAtLeastOneKill, Out.PStrikeOut, Out.PReversed, Out.NpcKillsPerRivalKill));

	// Assert: AC-ASN-32
	TestEqual(TEXT("E[N] = 2.98 +- 0.01"), Out.ExpectedN, 2.98, 0.01);
	TestEqual(TEXT("E[RivalKills] = 0.276 +- 0.002"), Out.ExpectedRivalKills, 0.276, 0.002);
	TestEqual(TEXT("P(>= 1 kill) = 0.232 +- 0.002"), Out.PAtLeastOneKill, 0.232, 0.002);
	TestEqual(TEXT("P(strike-out) = 0.976 +- 0.002"), Out.PStrikeOut, 0.976, 0.002);
	TestEqual(TEXT("P(reversed) = 0.024 +- 0.002"), Out.PReversed, 0.024, 0.002);
	TestEqual(TEXT("strike-out and reversed are exclusive and exhaustive"), Out.PStrikeOut + Out.PReversed, 1.0, 1e-12);
	TestEqual(TEXT("about 10.7 visible NPC kills per rival kill (the GDD's baseline)"), Out.NpcKillsPerRivalKill, 10.7, 0.05);
	TestEqual(TEXT("the GDD's intermediate values: q 0.085, rho 0.0073, m 0.908, pi 0.992"), Out.Q + Out.Rho + Out.M + Out.Pi, 0.085 + 0.0073 + 0.908 + 0.992, 0.002);

	// Edge: p = 0 (no rival among the targets).
	FInputs NoRival = Defaults;
	NoRival.TeamSize = 0;
	const FOutputs Zero = FProbeEconomicsF5::Evaluate(NoRival);
	TestEqual(TEXT("p = 0: no rival kill"), Zero.ExpectedRivalKills, 0.0, 1e-12);
	TestEqual(TEXT("p = 0: P(>= 1 kill) = 0"), Zero.PAtLeastOneKill, 0.0, 1e-12);
	TestTrue(TEXT("p = 0: only NPC braces reverse (P(reversed) small, > 0)"), Zero.PReversed > 0.0 && Zero.PReversed < 0.01);

	// Edge: p = 1 (every target a rival).
	FInputs AllRivals = Defaults;
	AllRivals.NpcCount = 0;
	const FOutputs One = FProbeEconomicsF5::Evaluate(AllRivals);
	TestEqual(TEXT("p = 1: no NPC kill, so no strike-out"), One.PStrikeOut, 0.0, 1e-12);
	TestEqual(TEXT("p = 1: the life ends by reversal"), One.PReversed, 1.0, 1e-12);
	TestEqual(TEXT("p = 1: E[N] = 1 (the reversing strike)"), One.ExpectedN, 1.0, 1e-12);
	TestEqual(TEXT("p = 1: P(>= 1 kill) = 1 - b_P"), One.PAtLeastOneKill, 1.0 - Defaults.BP, 1e-12);

	// Edge: L = 1 (the test preset only, bTestAllowsStrikeLimitOne).
	FInputs LimitOne = Defaults;
	LimitOne.L = 1;
	const FOutputs L1 = FProbeEconomicsF5::Evaluate(LimitOne);
	TestEqual(TEXT("L = 1: E[N] = 1"), L1.ExpectedN, 1.0, 1e-12);
	TestEqual(TEXT("L = 1: P(strike-out) = pi"), L1.PStrikeOut, L1.Pi, 1e-12);

	// Edge: b_P = 0 (no rival braces).
	FInputs NoBrace = Defaults;
	NoBrace.BP = 0.0;
	const FOutputs B0 = FProbeEconomicsF5::Evaluate(NoBrace);
	TestTrue(TEXT("b_P = 0: fewer reversals and more kills than at the default"), B0.PReversed < Out.PReversed && B0.PAtLeastOneKill > Out.PAtLeastOneKill);

	// TDC I-4: BlindGuessSuccess / ExpectedBlindKills ignore reversals and stay at or above F5 (the safe direction).
	const FInputs Cases[] = { Defaults, FInputs{ 4, 30, 3, 0.067, 0.00135 }, FInputs{ 5, 80, 4, 0.067, 0.00135 } };
	for (const FInputs& Case : Cases)
	{
		const FOutputs F = FProbeEconomicsF5::Evaluate(Case);
		const double Success = FProbeEconomicsF5::TdcBlindGuessSuccess(Case);
		const double Kills = FProbeEconomicsF5::TdcExpectedBlindKills(Case);
		AddInfo(FString::Printf(TEXT("TDC vs F5 at TeamSize %d NpcCount %d L %d: BlindGuessSuccess %.4f vs %.4f (+%.1f %%), ExpectedBlindKills %.4f vs %.4f (+%.1f %%)"),
			Case.TeamSize, Case.NpcCount, Case.L, Success, F.PAtLeastOneKill, 100.0 * (Success / F.PAtLeastOneKill - 1.0), Kills, F.ExpectedRivalKills, 100.0 * (Kills / F.ExpectedRivalKills - 1.0)));
		TestTrue(*FString::Printf(TEXT("TeamSize %d NpcCount %d: TDC's blind-guess numbers are never below F5's (the safe direction for I-4)"), Case.TeamSize, Case.NpcCount),
			Success >= F.PAtLeastOneKill && Kills >= F.ExpectedRivalKills);
	}
	TestEqual(TEXT("TDC BlindGuessSuccess at the defaults is 0.249"), FProbeEconomicsF5::TdcBlindGuessSuccess(Defaults), 0.249, 0.001);
	TestEqual(TEXT("TDC ExpectedBlindKills at the defaults is 0.300"), FProbeEconomicsF5::TdcExpectedBlindKills(Defaults), 0.300, 0.001);
	return true;
}

#endif
