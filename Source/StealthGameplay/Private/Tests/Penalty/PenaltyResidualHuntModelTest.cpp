// Story nkp-020 (production/epics/npc-kill-penalty-informants/story-020-penalty-tuning-warnings-and-residual-models.md),
// AC2 / AC-NKP-35 (GDD npc-kill-penalty-informants.md F4 and F5): the residual and hunt-cost balance model, evaluated by a
// TEST-ONLY evaluator from unrounded inputs (the formulas are planning analysis, never runtime code). A later change to a
// knob shows as a visible diff against this model.
//   F4  q = b*I / (b*I + N)                      P_res = 1 - (1 - q)^L
//   F5  T_deny = D + L_pick + d_back / v         R = c_I * T_deny / c_N   (c_N > 0)
// Inputs: NpcCount 50, walk speed 140 cm/s (the GDD formula tables), and the AC's cases; each result +/- 0.01.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace PenaltyResidualHuntModelTestData
{
	/** Tolerance of every model result (the AC's +/- 0.01). */
	constexpr double ModelToleranceAbs = 0.01;

	// Shared inputs (GDD F4 and F5 variable tables).
	constexpr int32 NpcCount = 50;
	constexpr double WalkSpeedCmPerSec = 140.0;

	// F4 cases (AC2): the default limit L = 3 for the first three, the hard maximum L = 5 for the last.
	constexpr int32 DefaultStrikeLimit = 3;
	constexpr int32 HardMaxStrikeLimit = 5;

	// F5 example inputs (AC2).
	constexpr double ChanceNpc = 0.6;
	constexpr double ChanceInformant = 0.3;
	constexpr double RespawnDelaySec = 15.0;
	constexpr double PickLatencySec = 0.5;
	constexpr double WalkBackCm = 6000.0;

	/** The test-only F4 evaluator: the chance that a player who reaches tally L has a non-strike victim among them. */
	struct FResidualModel
	{
		/** q: the share of WrongTarget victims who are informants. */
		static double InformantShare(int32 EmbodiedInformants, double OddsRatio, int32 Npcs)
		{
			const double Weighted = OddsRatio * static_cast<double>(EmbodiedInformants);
			return Weighted / (Weighted + static_cast<double>(Npcs));
		}

		/** P_res = 1 - (1 - q)^L. */
		static double Residual(int32 StrikeLimit, int32 EmbodiedInformants, double OddsRatio, int32 Npcs)
		{
			return 1.0 - FMath::Pow(1.0 - InformantShare(EmbodiedInformants, OddsRatio, Npcs), static_cast<double>(StrikeLimit));
		}
	};

	/** The test-only F5 evaluator: informant time denied by one successful informant kill, and the exchange rate. */
	struct FHuntCostModel
	{
		/** T_deny = D + L_pick + d_back / v. */
		static double TimeDeniedSec(double DelaySec, double PickSec, double BackCm, double SpeedCmPerSec)
		{
			return DelaySec + PickSec + BackCm / SpeedCmPerSec;
		}

		/** R = c_I * T_deny / c_N: informant-seconds denied per expected strike spent (defined for c_N > 0). */
		static double SecondsPerStrike(double InformantChance, double TimeDenied, double NpcChance)
		{
			return InformantChance * TimeDenied / NpcChance;
		}
	};

	struct FResidualCase
	{
		int32 StrikeLimit;
		int32 EmbodiedInformants;
		double OddsRatio;
		/** The AC's rounded P_res. */
		double Expected;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPenaltyResidualHuntModelTest, "StealthGame.Tuning.ResidualHuntModel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPenaltyResidualHuntModelTest::RunTest(const FString& Parameters)
{
	using namespace PenaltyResidualHuntModelTestData;

	// F4: P_res for the four AC cases (I = 2 b = 1, I = 2 b = 3, I = 4 b = 3 at the default L; L = 5 I = 4 b = 3).
	const FResidualCase Cases[] = {
		{ DefaultStrikeLimit, 2, 1.0, 0.111 },
		{ DefaultStrikeLimit, 2, 3.0, 0.288 },
		{ DefaultStrikeLimit, 4, 3.0, 0.476 },
		{ HardMaxStrikeLimit, 4, 3.0, 0.659 },
	};
	for (const FResidualCase& Case : Cases)
	{
		const double Residual = FResidualModel::Residual(Case.StrikeLimit, Case.EmbodiedInformants, Case.OddsRatio, NpcCount);
		TestTrue(FString::Printf(TEXT("P_res (L=%d, I=%d, b=%.0f) = %.4f, want %.3f +/- 0.01"), Case.StrikeLimit, Case.EmbodiedInformants, Case.OddsRatio, Residual, Case.Expected),
			FMath::IsNearlyEqual(Residual, Case.Expected, ModelToleranceAbs));
	}

	// F4 sanity: q is the share of informants, and no informant means no residual.
	TestTrue(TEXT("q for I = 2, b = 1 is 2 / 52"), FMath::IsNearlyEqual(FResidualModel::InformantShare(2, 1.0, NpcCount), 2.0 / 52.0, 1e-9));
	TestTrue(TEXT("P_res is 0 with no embodied informant"), FMath::IsNearlyZero(FResidualModel::Residual(DefaultStrikeLimit, 0, 3.0, NpcCount), 1e-12));

	// F5: T_deny and R for the AC's example, from unrounded inputs.
	const double TimeDenied = FHuntCostModel::TimeDeniedSec(RespawnDelaySec, PickLatencySec, WalkBackCm, WalkSpeedCmPerSec);
	const double PerStrike = FHuntCostModel::SecondsPerStrike(ChanceInformant, TimeDenied, ChanceNpc);
	constexpr double ExpectedTimeDeniedSec = 58.36;
	constexpr double ExpectedSecondsPerStrike = 29.18;
	TestTrue(FString::Printf(TEXT("T_deny = %.4f s, want %.2f +/- 0.01"), TimeDenied, ExpectedTimeDeniedSec), FMath::IsNearlyEqual(TimeDenied, ExpectedTimeDeniedSec, ModelToleranceAbs));
	TestTrue(FString::Printf(TEXT("R = %.4f s per strike, want %.2f +/- 0.01"), PerStrike, ExpectedSecondsPerStrike), FMath::IsNearlyEqual(PerStrike, ExpectedSecondsPerStrike, ModelToleranceAbs));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
