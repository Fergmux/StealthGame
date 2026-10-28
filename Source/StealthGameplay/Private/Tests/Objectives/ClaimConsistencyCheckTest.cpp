// Story oc-006 (production/epics/objectives-chain/story-006-fixture-claims-and-interact-target-owner.md), AC4 (ADR-0020 5.1)
// ClaimConsistencyCheck (non-Shipping): at drain end each fixture's claim count equals the number of IIF Interacting slots
// targeting it (the stand-in's running holds per court). Matching counts (including zero slots and zero claims) report
// nothing; a seeded mismatch reports InvariantViolation to the fault sink exactly once, however many drains it persists; the
// check is compiled out of Shipping (every declaration and definition of CheckConsistency / CheckClaimConsistency sits
// inside #if !UE_BUILD_SHIPPING, by source scan).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "ObjectivesQueueRig.h"
#include "ObjectivesSourceScan.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectivesClaimConsistencyCheckTest, "StealthGame.Objectives.ClaimConsistencyCheck",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FObjectivesClaimConsistencyCheckTest::RunTest(const FString& Parameters)
{
	using namespace StealthObjectivesTest;

	FQueueRig Rig;
	TestTrue(TEXT("Active"), Rig.ToActive());
	TMap<FCourtFixtureId, int32> Slots;

	// Zero slots, zero claims: passes.
	Rig.Holds->GetInteractingSlotsByCourt(Slots);
	TestEqual(TEXT("zero slots and zero claims: no mismatch"), Rig.Service->CheckClaimConsistency(Slots), 0);

	// Two tenders at one court, one at another: the drain-end counts agree.
	AStealthPawn* A = Rig.Pawn(CourtOrdinary, 0);
	AStealthPawn* B = Rig.Pawn(CourtOrdinary, 2);
	AStealthPawn* C = Rig.Pawn(CourtFinal, 0);
	Rig.Begin(A, CourtOrdinary);
	Rig.Begin(B, CourtOrdinary);
	Rig.Begin(C, CourtFinal);
	Rig.Q.Tick(0.1);
	Rig.Holds->GetInteractingSlotsByCourt(Slots);
	TestEqual(TEXT("two Interacting slots at the ordinary court"), Slots.FindRef(FCourtFixtureId(CourtOrdinary)), 2);
	const int32 FaultsBefore = Rig.Q.Faults.GetRecords().Num();
	TestEqual(TEXT("claims equal Interacting slots"), Rig.Service->CheckClaimConsistency(Slots), 0);
	TestEqual(TEXT("no fault"), Rig.Q.Faults.GetRecords().Num(), FaultsBefore);

	// Seeded mismatch: one more slot than claims at the final court, checked on three drains: reported exactly once.
	TMap<FCourtFixtureId, int32> Seeded = Slots;
	Seeded.FindOrAdd(FCourtFixtureId(CourtFinal)) += 1;
	for (int32 Drain = 0; Drain < 3; ++Drain)
	{
		TestEqual(FString::Printf(TEXT("drain %d: one mismatching court"), Drain), Rig.Service->CheckClaimConsistency(Seeded), 1);
	}
	const TArray<FStealthFaultRecord> Faults = Rig.Q.Faults.GetRecords();
	TestEqual(TEXT("the mismatch reports exactly once"), Faults.Num(), FaultsBefore + 1);
	if (Faults.Num() == FaultsBefore + 1)
	{
		TestTrue(TEXT("an InvariantViolation naming the court"), Faults.Last().Fault == EStealthFault::InvariantViolation && Faults.Last().Detail == CourtFinal);
	}

	// The check is absent in Shipping.
	for (const StealthObjectivesSourceScan::FSource& Source : StealthObjectivesSourceScan::LoadSources(TEXT("StealthGameplay/Features/Objectives")))
	{
		for (const TCHAR* Name : { TEXT("CheckConsistency"), TEXT("CheckClaimConsistency") })
		{
			const TArray<int32> Outside = StealthObjectivesSourceScan::FindOutsideShippingGuard(Source.Text, Name);
			TestEqual(FString::Printf(TEXT("%s: %s only inside #if !UE_BUILD_SHIPPING"), *Source.Path, Name), Outside.Num(), 0);
		}
	}
	TestEqual(TEXT("seeded: an unguarded definition is found"),
		StealthObjectivesSourceScan::FindOutsideShippingGuard(TEXT("int32 F::CheckConsistency() { return 0; }\n"), TEXT("CheckConsistency")).Num(), 1);
	return true;
}

#endif
