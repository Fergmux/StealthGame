// Story oc-006 (production/epics/objectives-chain/story-006-fixture-claims-and-interact-target-owner.md), AC1 (ADR-0020 5.2)
// InteractHoldSecOneValue: GetInteractHoldSec() returns the frozen ObjectiveInteractSec (3.0), takes no court parameter (its
// type is double() const) and so is the same for every court; D is read once at execution: a tuning publish of 5.0 mid-hold
// never moves the running hold (it completes at Begin + 3.0), and the next Begin reads the new frozen 5.0. Edge: a publish
// in the same tick as the execution (before the drain that executes the Begin) is read by that Begin.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "ObjectivesQueueRig.h"

namespace ObjectivesInteractHoldSecOneValueTestData
{
	constexpr double FrozenSec = 3.0;
	constexpr float PublishedSec = 5.0f;
	constexpr double Tolerance = 1e-6;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectivesInteractHoldSecOneValueTest, "StealthGame.Objectives.InteractHoldSecOneValue",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FObjectivesInteractHoldSecOneValueTest::RunTest(const FString& Parameters)
{
	using namespace StealthObjectivesTest;
	using namespace ObjectivesInteractHoldSecOneValueTestData;

	// The API has no court parameter (a compile-time fact of the interface).
	static_assert(std::is_same_v<decltype(&IInteractTargetOwner::GetInteractHoldSec), double (IInteractTargetOwner::*)() const>,
		"GetInteractHoldSec takes no court parameter");

	FQueueRig Rig;
	TestTrue(TEXT("Active"), Rig.ToActive());
	TestEqual(TEXT("D is the frozen ObjectiveInteractSec"), Rig.Service->GetInteractHoldSec(), FrozenSec, Tolerance);

	// The same D at every court: holds begun at four different courts are all due D after their Begin.
	const uint16 Courts[] = { CourtP1Team0, CourtP2Team1, CourtFinal, CourtOrdinary };
	TArray<AStealthPawn*> Pawns;
	for (const uint16 Court : Courts)
	{
		AStealthPawn* Pawn = Rig.Pawn(Court, 0);
		Pawns.Add(Pawn);
		Rig.Begin(Pawn, Court);
	}
	Rig.Q.Tick(0.1);
	const double BeginTime = Rig.Q.Clock.GetServerTimeSeconds();
	for (int32 Index = 0; Index < Pawns.Num(); ++Index)
	{
		const FInteractHoldStandIn::FHold* Hold = Rig.Holds->FindHold(Pawns[Index]);
		if (TestNotNull(FString::Printf(TEXT("court %d: the hold runs"), Courts[Index]), Hold))
		{
			TestEqual(FString::Printf(TEXT("court %d: due D after Begin"), Courts[Index]), Hold->DueServerTime - BeginTime, FrozenSec, Tolerance);
		}
	}

	// A publish mid-hold never changes the running holds; the next Begin reads the new frozen value.
	FObjectivesTuning_Public Published;
	Published.ObjectiveInteractSec = PublishedSec;
	Rig.Run(1.0);
	Rig.Service->SetTuning(Published, FObjectivesTuning_Server());
	const FInteractHoldStandIn::FHold* Running = Rig.Holds->FindHold(Pawns[0]);
	TestTrue(TEXT("the running hold keeps Begin + 3.0"), Running != nullptr && FMath::IsNearlyEqual(Running->DueServerTime - BeginTime, FrozenSec, Tolerance));
	Rig.Run(2.1);
	TestEqual(TEXT("every running hold ended at its own 3.0 (not 5.0)"), Rig.Holds->GetHoldCount(), 0);
	TestEqual(TEXT("unbound pawns never count a stage (no ObjectiveStage)"), Rig.Logged(ERoundLogType::ObjectiveStage), 0);

	AStealthPawn* Next = Rig.Pawn(CourtOrdinary, 1);
	Rig.Begin(Next, CourtOrdinary);
	Rig.Q.Tick(0.1);
	const double NextBegin = Rig.Q.Clock.GetServerTimeSeconds();
	const FInteractHoldStandIn::FHold* NextHold = Rig.Holds->FindHold(Next);
	TestTrue(TEXT("the next Begin reads the new frozen 5.0"), NextHold != nullptr && FMath::IsNearlyEqual(NextHold->DueServerTime - NextBegin, static_cast<double>(PublishedSec), Tolerance));

	// Edge: published in the same tick as the execution (before its drain): that Begin reads the published value.
	Published.ObjectiveInteractSec = 4.0f;
	AStealthPawn* SameTick = Rig.Pawn(CourtP1Team1, 2);
	Rig.Begin(SameTick, CourtP1Team1);
	Rig.Service->SetTuning(Published, FObjectivesTuning_Server());
	Rig.Q.Tick(0.1);
	const FInteractHoldStandIn::FHold* SameHold = Rig.Holds->FindHold(SameTick);
	TestTrue(TEXT("a publish before the executing drain is read at execution (4.0)"), SameHold != nullptr
		&& FMath::IsNearlyEqual(SameHold->DueServerTime - Rig.Q.Clock.GetServerTimeSeconds(), 4.0, Tolerance));
	return true;
}

#endif
