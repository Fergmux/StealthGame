// Story asn-002 (production/epics/assassination-defence-reversal/story-002-reach-f1-pure-function.md), AC-ASN-02:
// F1 reach with r_c 35, R 200, Z 75, T_v 0.40, lambda 0.15, X_max 240, g 15, Theta 90. The attacker stands at the origin
// facing +X; positions and velocities are injected (no world). AC1: standing range boundary (0.5 cm epsilon on the range
// clause only), |dz| boundary on both signs, bearing boundary, toward-walker with X_eff 0. AC2: moving victims (walk 140
// braking 187, run 450 braking 600), including the runner-away X_max boundary with no epsilon on the warp clause.
// The F1 constants come from the default tuning structs (FStrikeReachParams::FromTuning) and are checked against the AC.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Server/StealthTuningSubstructs.h"
#include "Server/StrikeReachCore.h"

namespace AssassinationReachF1TestData
{
	/** The AC's capsule radius (SCL provisional), injected so the numbers never move with the pool capsule. */
	constexpr double CapsuleRadiusCm = 35.0;
	/** Concealment's BushStrikeEmergeSec GDD default (unused by the standard strike). */
	constexpr double BushStrikeEmergeSec = 0.35;
	/** SCL gaits (F2): speed and baseline braking. */
	constexpr double WalkSpeed = 140.0;
	constexpr double WalkBraking = 187.0;
	constexpr double RunSpeed = 450.0;
	constexpr double RunBraking = 600.0;

	FStrikeReachParams StandardParams()
	{
		return FStrikeReachParams::FromTuning(FAssassinationTuning_Public(), FAssassinationTuning_Server(), EStrikeReachVariant::Standard, BushStrikeEmergeSec, CapsuleRadiusCm);
	}

	/** A victim at (X, Y, Z) moving with (Vx, Vy) and braking a_b, attacker at the origin facing +X. */
	FStrikeReachResult Eval(const FStrikeReachParams& Params, double X, double Y, double Z, double Vx = 0.0, double Vy = 0.0, double Braking = 0.0)
	{
		FStrikeReachInput In;
		In.AttackerLocation = FVector::ZeroVector;
		In.AttackerFacingYawDeg = 0.0;
		In.VictimLocation = FVector(X, Y, Z);
		In.VictimVelocity = FVector(Vx, Vy, 0.0);
		In.VictimBrakingCmPerSec2 = Braking;
		return FStrikeReachCore::Evaluate(Params, In);
	}

	/** A standing victim at distance D and bearing BearingDeg from the facing. */
	FStrikeReachResult EvalAtBearing(const FStrikeReachParams& Params, double D, double BearingDeg)
	{
		const double Rad = FMath::DegreesToRadians(BearingDeg);
		return Eval(Params, D * FMath::Cos(Rad), D * FMath::Sin(Rad), 0.0);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAssassinationReachF1Test, "StealthGame.Assassination.ReachF1",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAssassinationReachF1Test::RunTest(const FString& Parameters)
{
	using namespace AssassinationReachF1TestData;
	const FStrikeReachParams P = StandardParams();

	// The defaults are the AC's constants (read from tuning, not literals in the core).
	TestEqual(TEXT("R 200"), P.RangeCm, 200.0);
	TestEqual(TEXT("Z 75"), P.VerticalReachCm, 75.0);
	TestEqual(TEXT("T_v 0.40"), P.ContactSec, 0.40, 1e-6);
	TestEqual(TEXT("lambda 0.15"), P.NetLeadSec, 0.15, 1e-6);
	TestEqual(TEXT("X_max 240"), P.WarpLimitCm, 240.0);
	TestEqual(TEXT("g 15"), P.ContactGapCm, 15.0);
	TestEqual(TEXT("Theta 90"), P.BearingLimitDeg, 90.0);

	// ---- AC1: standing range boundary (epsilon on the range clause only) ----
	{
		const FStrikeReachResult In = Eval(P, 235.4, 0.0, 0.0);
		const FStrikeReachResult Out = Eval(P, 235.6, 0.0, 0.0);
		TestTrue(TEXT("standing at d0 235.4 is in reach"), In.bInReach);
		TestFalse(TEXT("standing at d0 235.6 is out of reach"), Out.bInReach);
		TestFalse(TEXT("235.6 fails on the range clause"), Out.bRangeOk);
		TestTrue(TEXT("235.6 passes the warp clause (X 150.6)"), Out.bWarpOk);
		TestEqual(TEXT("standing at d0 235: X = 235 - 70 - 15 = 150"), Eval(P, 235.0, 0.0, 0.0).WarpTranslationCm, 150.0, 1e-9);
	}
	// |dz| boundary, both signs, exact (no epsilon).
	{
		TestTrue(TEXT("dz 75 in reach"), Eval(P, 150.0, 0.0, 75.0).bInReach);
		TestFalse(TEXT("dz 76 out of reach"), Eval(P, 150.0, 0.0, 76.0).bInReach);
		TestTrue(TEXT("dz -75 in reach"), Eval(P, 150.0, 0.0, -75.0).bInReach);
		TestFalse(TEXT("dz -76 out of reach"), Eval(P, 150.0, 0.0, -76.0).bInReach);
		TestFalse(TEXT("dz 75.01 out (no epsilon on the vertical clause)"), Eval(P, 150.0, 0.0, 75.01).bVerticalOk);
	}
	// Bearing boundary at d0 150, both sides.
	{
		const FStrikeReachResult B89 = EvalAtBearing(P, 150.0, 89.0);
		TestTrue(TEXT("theta 89 in reach"), B89.bInReach);
		TestEqual(TEXT("theta_T is 89"), B89.BearingDeg, 89.0, 1e-6);
		TestFalse(TEXT("theta 91 out of reach"), EvalAtBearing(P, 150.0, 91.0).bInReach);
		TestTrue(TEXT("theta -89 in reach"), EvalAtBearing(P, 150.0, -89.0).bInReach);
		TestFalse(TEXT("theta -91 out of reach"), EvalAtBearing(P, 150.0, -91.0).bInReach);
		TestFalse(TEXT("theta 100 (GDD example) is a whiff"), EvalAtBearing(P, 150.0, 100.0).bInReach);
	}
	// A victim walking toward the attacker at d0 100: X = 46.1 - 85 < 0, X_eff = 0, theta 0.
	{
		const FStrikeReachResult R = Eval(P, 100.0, 0.0, 0.0, -WalkSpeed, 0.0, WalkBraking);
		TestTrue(TEXT("toward-walker at d0 100 is in reach"), R.bInReach);
		TestTrue(TEXT("its raw X is negative"), R.WarpTranslationCm < 0.0);
		TestEqual(TEXT("X_eff is 0 (never negative)"), R.WarpTranslationEffCm, 0.0);
		TestEqual(TEXT("theta_T is 0"), R.BearingDeg, 0.0, 1e-9);
		TestEqual(TEXT("its contact point is 46.1 cm ahead"), R.VictimAtContact.X, 46.1, 0.1);
	}

	// ---- AC2: moving victims (F1 extrapolation; both gaits take the lambda < v/a_b branch) ----
	{
		TestTrue(TEXT("walk takes the lambda < v/a_b branch"), P.NetLeadSec < WalkSpeed / WalkBraking);
		TestTrue(TEXT("run takes the lambda < v/a_b branch"), P.NetLeadSec < RunSpeed / RunBraking);

		const FStrikeReachResult WalkAway = Eval(P, 235.0, 0.0, 0.0, WalkSpeed, 0.0, WalkBraking);
		TestEqual(TEXT("walking away: D = 53.9"), WalkAway.ExtrapolationCm, 53.9, 0.1);
		TestEqual(TEXT("walking away: X = 203.9"), WalkAway.WarpTranslationCm, 203.9, 0.1);
		TestTrue(TEXT("walking away at d0 235 is in reach"), WalkAway.bInReach);

		const FStrikeReachResult RunAway = Eval(P, 235.0, 0.0, 0.0, RunSpeed, 0.0, RunBraking);
		TestEqual(TEXT("running away: D = 173.25"), RunAway.ExtrapolationCm, 173.25, 1e-3);
		TestEqual(TEXT("running away: X = 323.25"), RunAway.WarpTranslationCm, 323.25, 1e-3);
		TestFalse(TEXT("running away at d0 235 is out of reach"), RunAway.bInReach);
		TestFalse(TEXT("it fails on the warp clause"), RunAway.bWarpOk);

		const FStrikeReachResult RunAcross = Eval(P, 235.0, 0.0, 0.0, 0.0, RunSpeed, RunBraking);
		TestEqual(TEXT("running across: X = 207.0"), RunAcross.WarpTranslationCm, 207.0, 0.1);
		TestEqual(TEXT("running across: bearing atan(173.25 / 235) = 36.4"), RunAcross.BearingDeg, 36.4, 0.1);
		TestTrue(TEXT("running across at d0 235 is in reach"), RunAcross.bInReach);

		const FStrikeReachResult EdgeIn = Eval(P, 151.5, 0.0, 0.0, RunSpeed, 0.0, RunBraking);
		const FStrikeReachResult EdgeOut = Eval(P, 152.0, 0.0, 0.0, RunSpeed, 0.0, RunBraking);
		TestEqual(TEXT("runner away at d0 151.5: X = 239.75"), EdgeIn.WarpTranslationCm, 239.75, 1e-3);
		TestTrue(TEXT("runner away at d0 151.5 is in reach"), EdgeIn.bInReach);
		TestEqual(TEXT("runner away at d0 152.0: X = 240.25"), EdgeOut.WarpTranslationCm, 240.25, 1e-3);
		TestFalse(TEXT("runner away at d0 152.0 is out of reach (no epsilon on the warp clause)"), EdgeOut.bInReach);
	}

	// Braking branch pieces (pure helpers): to-rest branch and no braking.
	TestEqual(TEXT("B(v) to rest when lambda >= v/a_b: 100^2 / (2 * 600)"), FStrikeReachCore::BrakingDistance(100.0, 0.5, 600.0), 100.0 * 100.0 / 1200.0, 1e-9);
	TestEqual(TEXT("B(0) is 0"), FStrikeReachCore::BrakingDistance(0.0, 0.15, 187.0), 0.0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
