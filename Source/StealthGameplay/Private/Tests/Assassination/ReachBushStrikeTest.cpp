// Story asn-002 (production/epics/assassination-defence-reversal/story-002-reach-f1-pure-function.md), AC-ASN-38:
// the bush strike's T_v is StrikeContactSec + BushStrikeEmergeSec = 0.40 + 0.35 = 0.75 s (t_d 0.60 with lambda 0.15),
// built by FStrikeReachParams::FromTuning, not a literal. A walker straight away (140 cm/s, braking 187) has D 102.9 and is
// in reach at d0 222.0 and out at 222.3; a runner straight away (450 cm/s, braking 600) has D 330.75 and is out of reach
// for every integer d0 in [70, 235] (X >= 315.75 at d0 70).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Server/StealthTuningSubstructs.h"
#include "Server/StrikeReachCore.h"

namespace AssassinationReachBushStrikeTestData
{
	constexpr double CapsuleRadiusCm = 35.0;
	/** Concealment's BushStrikeEmergeSec GDD default (the key is Concealment's; injected). */
	constexpr double BushStrikeEmergeSec = 0.35;

	FStrikeReachResult AwayAt(const FStrikeReachParams& Params, double D0, double Speed, double Braking)
	{
		FStrikeReachInput In;
		In.VictimLocation = FVector(D0, 0.0, 0.0);
		In.VictimVelocity = FVector(Speed, 0.0, 0.0);
		In.VictimBrakingCmPerSec2 = Braking;
		return FStrikeReachCore::Evaluate(Params, In);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAssassinationReachBushStrikeTest, "StealthGame.Assassination.ReachBushStrike",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAssassinationReachBushStrikeTest::RunTest(const FString& Parameters)
{
	using namespace AssassinationReachBushStrikeTestData;
	const FAssassinationTuning_Public Public;
	const FAssassinationTuning_Server Server;
	const FStrikeReachParams Bush = FStrikeReachParams::FromTuning(Public, Server, EStrikeReachVariant::BushStrike, BushStrikeEmergeSec, CapsuleRadiusCm);
	const FStrikeReachParams Standard = FStrikeReachParams::FromTuning(Public, Server, EStrikeReachVariant::Standard, BushStrikeEmergeSec, CapsuleRadiusCm);

	TestEqual(TEXT("bush T_v = StrikeContactSec + BushStrikeEmergeSec = 0.75"), Bush.ContactSec, 0.75, 1e-6);
	TestEqual(TEXT("the standard strike ignores BushStrikeEmergeSec"), Standard.ContactSec, static_cast<double>(Public.StrikeContactSec));

	// Walker straight away.
	{
		const FStrikeReachResult In = AwayAt(Bush, 222.0, 140.0, 187.0);
		TestEqual(TEXT("bush walker: D = 102.9"), In.ExtrapolationCm, 102.9, 0.1);
		TestTrue(TEXT("bush walker at d0 222.0 is in reach"), In.bInReach);
		TestFalse(TEXT("bush walker at d0 222.3 is out of reach"), AwayAt(Bush, 222.3, 140.0, 187.0).bInReach);
	}
	// Runner straight away: out at every d0 in [70, 235].
	{
		const FStrikeReachResult At70 = AwayAt(Bush, 70.0, 450.0, 600.0);
		TestEqual(TEXT("bush runner: D = 330.75"), At70.ExtrapolationCm, 330.75, 1e-3);
		TestEqual(TEXT("bush runner at d0 70: X = 315.75"), At70.WarpTranslationCm, 315.75, 1e-3);
		int32 InReach = 0;
		for (int32 D0 = 70; D0 <= 235; ++D0)
		{
			InReach += AwayAt(Bush, static_cast<double>(D0), 450.0, 600.0).bInReach ? 1 : 0;
		}
		TestEqual(TEXT("bush runner straight away is out of reach for every d0 in [70, 235]"), InReach, 0);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
