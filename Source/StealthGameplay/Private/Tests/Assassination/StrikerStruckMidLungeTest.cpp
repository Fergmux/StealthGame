// Story asn-010, AC3 / AC-ASN-37 StrikerStruckMidLunge (ASN-R13). Stub items A, B, C with keys k < k': A kills B at key k;
// C's Attack on A resolves in a later drain while A is Striking (k' one tick later, and in a second run in the next server
// frame's first Seq): A is killed (Striking is never protection), B stays killed, and exactly two OnPawnKilled are recorded.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Tests/Assassination/EncounterTestRig.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAssassinationStrikerStruckMidLungeTest, "StealthGame.Assassination.StrikerStruckMidLunge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAssassinationStrikerStruckMidLungeTest::RunTest(const FString& Parameters)
{
	using namespace StealthEncounterTest;
	for (const double Later : { 1.0 / 60.0, 0.5 })
	{
		FRig R(TEXT("StrikerMidLunge"));
		APawn* A = R.Spawn(FVector::ZeroVector, 0);
		APawn* B = R.Spawn(FVector(150.0, 0.0, 0.0));
		APawn* C = R.Spawn(FVector(-150.0, 0.0, 0.0), 1);
		const FQueueKey K = R.NextKey();
		R.AttackRef(A, B, K);
		R.Advance(Later);
		const FQueueKey KLater = R.NextKey();
		TestTrue(FString::Printf(TEXT("+%.3f s: k < k'"), Later), K < KLater);
		TestTrue(FString::Printf(TEXT("+%.3f s: A is Striking"), Later), R.Core->GetRole(A) == EStrikeRole::Striking);
		R.AttackRef(C, A, KLater);
		TestTrue(FString::Printf(TEXT("+%.3f s: A is killed mid-lunge"), Later), R.Last().Victim == A && R.Last().Outcome == EAttackOutcome::Kill && R.Core->IsDead(A));
		TestTrue(FString::Printf(TEXT("+%.3f s: B stays killed"), Later), R.Core->IsDead(B));
		TestEqual(FString::Printf(TEXT("+%.3f s: two OnPawnKilled"), Later), R.Kills.Num(), 2);
		TestTrue(FString::Printf(TEXT("+%.3f s: A's own strike outcome stands"), Later), R.Core->GetResolutions()[0].Victim == B && R.Core->GetResolutions()[0].Outcome == EAttackOutcome::Kill);
	}
	return true;
}

#endif
