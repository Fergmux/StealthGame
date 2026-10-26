// Story SCL-046 (production/epics/shared-character-locomotion/story-046-traversal-heights-match-kit.md), AC-SCL-34
// TraversalHeightsMatchKit: GIVEN the tuning snapshot and the kit coping manifest, WHEN validated at OnLayoutReady, THEN
// every coping height is within TraversalHeightToleranceCm of a capability height (the LSC cross-check); an
// out-of-tolerance coping makes the validator report the offending coping id and height and fail. The kit heights are
// the spike SCL-045 finals (vault 0.8 m, hurdle 1.2 m, climb 1.6 m). Fixtures are built from the snapshot values, not
// hard-coded numbers. The LSC coping manifest and its OnLayoutReady call are the LSC epic's; this is the SCL side.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "View/Core/StealthLocomotionModel.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLocomotionTraversalHeightsMatchKitTest, "StealthGame.Locomotion.TraversalHeightsMatchKit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FLocomotionTraversalHeightsMatchKitTest::RunTest(const FString& Parameters)
{
	const FLocomotionTuning_Public Tuning;
	const FStealthTraversalHeights Heights = FStealthTraversalHeights::FromTuning(Tuning);
	auto Make = [](int32 Id, float Height) { FStealthCopingCandidate C; C.Id = Id; C.HeightCm = Height; return C; };

	// A kit manifest at the capability heights and at the edges of the tolerance band: valid.
	const FStealthCopingCandidate Kit[] = {
		Make(1, Heights.VaultHeightCm), Make(2, Heights.HurdleHeightCm), Make(3, Heights.ClimbHeightCm),
		Make(4, Heights.VaultHeightCm + Heights.ToleranceCm), Make(5, Heights.ClimbHeightCm - Heights.ToleranceCm), Make(6, Heights.HurdleHeightCm - Heights.ToleranceCm),
	};
	TArray<FStealthCopingMismatch> Mismatches;
	TestTrue(TEXT("the kit at the capability heights (and +- tolerance) validates"), StealthLocomotion::ValidateKitCopingHeights(Kit, Heights, Mismatches));
	TestEqual(TEXT("no mismatch reported"), Mismatches.Num(), 0);

	// The spike finals are the snapshot heights.
	TestEqual(TEXT("vault capability height is 80 cm (spike SCL-045)"), Heights.VaultHeightCm, 80.0f);
	TestEqual(TEXT("hurdle capability height is 120 cm (spike SCL-045)"), Heights.HurdleHeightCm, 120.0f);
	TestEqual(TEXT("climb capability height is 160 cm (spike SCL-045)"), Heights.ClimbHeightCm, 160.0f);

	// Out-of-tolerance copings fail with their id and height.
	const float Outside = Heights.VaultHeightCm + Heights.ToleranceCm + 1.0f;
	const float Between = (Heights.HurdleHeightCm + Heights.ClimbHeightCm) * 0.5f;
	const FStealthCopingCandidate Bad[] = { Make(10, Heights.VaultHeightCm), Make(11, Outside), Make(12, Between), Make(13, Heights.MinHeightCm) };
	TestFalse(TEXT("a manifest with out-of-tolerance copings fails layout validation"), StealthLocomotion::ValidateKitCopingHeights(Bad, Heights, Mismatches));
	TestEqual(TEXT("three offending copings reported"), Mismatches.Num(), 3);
	if (Mismatches.Num() == 3)
	{
		TestTrue(TEXT("the report names coping 11 and its height"), Mismatches[0].CopingId == 11 && Mismatches[0].HeightCm == Outside);
		TestTrue(TEXT("the report names coping 12 and its height"), Mismatches[1].CopingId == 12 && Mismatches[1].HeightCm == Between);
		TestTrue(TEXT("a step-height coping is not traversable coping (coping 13)"), Mismatches[2].CopingId == 13);
	}

	// A tuning change moves the bands with it: the same kit fails under a changed vault height.
	FLocomotionTuning_Public Moved = Tuning;
	Moved.TraversalVaultHeightCm = Tuning.TraversalVaultHeightCm + 2.0f * Tuning.TraversalHeightToleranceCm;
	TestFalse(TEXT("the kit fails against a snapshot whose vault height moved"), StealthLocomotion::ValidateKitCopingHeights(Kit, FStealthTraversalHeights::FromTuning(Moved), Mismatches));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
