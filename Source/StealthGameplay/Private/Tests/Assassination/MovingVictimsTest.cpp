// Story asn-008, AC3 / AC-ASN-11 MovingVictims (ASN-R12, F1). Server positions, the victim's own velocity until the death
// write and its gait's baseline braking after it: a runner 450 cm/s straight away at d0 = 150 (run braking 600) is killed
// (X = 238.25 <= 240); vaulters at d0 = 150 moving 300 cm/s away at dz 0 and dz 75 (inclusive) are killed; a climber at
// dz 80 is NoContact. An attacker that is running, or in a traversal, is rejected NotNow (IIF R7, re-checked at the slot).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Tests/Assassination/EncounterTestRig.h"

namespace MovingVictimsTestData
{
	constexpr double RunBrakingCmPerSec2 = 600.0;

	struct FCase
	{
		const TCHAR* Name;
		double Speed;
		double DeltaZ;
		bool bKilled;
	};

	const FCase Cases[] = {
		{ TEXT("runner 450 cm/s away, d0 150"), 450.0, 0.0, true },
		{ TEXT("vaulter 300 cm/s, dz 0"), 300.0, 0.0, true },
		{ TEXT("vaulter 300 cm/s, dz 75"), 300.0, 75.0, true },
		{ TEXT("climber, dz 80"), 0.0, 80.0, false },
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAssassinationMovingVictimsTest, "StealthGame.Assassination.MovingVictims",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAssassinationMovingVictimsTest::RunTest(const FString& Parameters)
{
	using namespace StealthEncounterTest;
	using namespace MovingVictimsTestData;
	for (const FCase& Case : Cases)
	{
		FRig R(TEXT("MovingVictims"));
		APawn* A = R.Spawn(FVector::ZeroVector, 0);
		APawn* V = R.Spawn(FVector(150.0, 0.0, Case.DeltaZ), 1);
		R.Facts(V).Velocity = FVector(Case.Speed, 0.0, 0.0);
		R.Facts(V).BrakingCmPerSec2 = RunBrakingCmPerSec2;
		R.AttackRef(A, V);
		AddInfo(FString::Printf(TEXT("MOVING %s: X_eff %.2f, in reach %d"), Case.Name, R.Last().Reach.WarpTranslationEffCm, R.Last().Reach.bInReach ? 1 : 0));
		TestTrue(FString::Printf(TEXT("%s: %s"), Case.Name, Case.bKilled ? TEXT("killed at its server position") : TEXT("NoContact")),
			Case.bKilled ? (R.Last().Victim == V && R.Last().StrikeResult.Result == EStrikeResult::RivalEliminated) : (R.Last().Victim == nullptr && R.Last().StrikeResult.Result == EStrikeResult::NoContact));
	}

	for (const bool bRunning : { true, false })
	{
		FRig R(TEXT("MovingAttacker"));
		APawn* A = R.Spawn(FVector::ZeroVector, 0);
		APawn* V = R.Spawn(FVector(150.0, 0.0, 0.0), 1);
		(bRunning ? R.Facts(A).bRunning : R.Facts(A).bTraversing) = true;
		const EResolveResult Result = R.AttackRef(A, V);
		TestTrue(FString::Printf(TEXT("a %s attacker is rejected NotNow"), bRunning ? TEXT("running") : TEXT("traversing")),
			Result == EResolveResult::Voided && R.LastReply().Code == ECommandRejectCode::NotNow && R.Core->GetResolutions().Num() == 0 && R.State(A) == nullptr);
	}
	return true;
}

#endif
