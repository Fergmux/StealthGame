// Story SCL-014 (production/epics/shared-character-locomotion/story-014-informant-restrictions.md), AC-SCL-10
// InformantRestrictions (SCL half): GIVEN an informant pawn (a stub IStealthGaitLegalitySource returning Informant),
// WHEN Run is issued and the traversal movement mode (MOVE_Flying, spike SCL-045) is forced directly, THEN speed stays
// <= 140 and SCL refuses the mode switch (the backstop reports EStealthFault::MovementMisuse / RefusedTraversalMode); a
// planted Brace pawn is refused the same way; an Active pawn is not (control). The IIF NotNow reply to Traverse is the
// IIF epic's (no Traverse legality predicate is registered yet): recorded as a deferred integration check in the story.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Server/Testing/StealthFaultSinkTestSupport.h"
#include "Tests/Locomotion/LocomotionTestWorld.h"

namespace InformantRestrictionsTestData
{
	class FStubLegality final : public IStealthGaitLegalitySource
	{
	public:
		explicit FStubLegality(EStealthGaitLegality InClass) : Class(InClass) {}
		virtual EStealthGaitLegality GetGaitLegality(const APawn&) const override { return Class; }
		EStealthGaitLegality Class;
	};
	constexpr int32 RunFrames = 240;

	int32 CountMovementMisuse(const FStealthRecordingFaultSink& Sink, int64 Detail = -1)
	{
		return Sink.GetRecords().FilterByPredicate([Detail](const FStealthFaultRecord& R) { return R.Fault == EStealthFault::MovementMisuse && (Detail < 0 || R.Detail == Detail); }).Num();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLocomotionInformantRestrictionsTest, "StealthGame.Locomotion.InformantRestrictions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FLocomotionInformantRestrictionsTest::RunTest(const FString& Parameters)
{
	using namespace InformantRestrictionsTestData;
	StealthLocomotionTest::FWorld TestWorld(TEXT("InformantRestrictionsTestWorld"));
	FStubLegality Informant(EStealthGaitLegality::Informant);
	FStealthRecordingFaultSink Faults;

	AStealthPawn* Pawn = TestWorld.SpawnPawn(FVector2D(-5000.0, 0.0));
	UStealthMovementComponent* Movement = Pawn->GetStealthMovement();
	Movement->SetGaitLegalitySource(&Informant);
	Movement->SetFaultSink(&Faults);

	// Run is clamped to Walk: speed never above WalkSpeedCmPerSec.
	StealthLocomotionTest::DriveMove(Pawn, 0.0f, 1.0f, EGait::Run);
	double Peak = 0.0;
	for (int32 Frame = 0; Frame < RunFrames; ++Frame)
	{
		StealthLocomotionTest::StepPawn(Pawn);
		Peak = FMath::Max(Peak, StealthLocomotionTest::Speed2D(Pawn));
	}
	AddInfo(FString::Printf(TEXT("INFORMANT Run: peak %.2f cm/s, effective gait %s"), Peak, Movement->GetEffectiveGait() == EGait::Run ? TEXT("Run") : TEXT("Walk")));
	TestTrue(FString::Printf(TEXT("informant Run: speed stays <= 140 (peak %.2f)"), Peak), Peak <= Movement->GetLocomotionTuning().WalkSpeedCmPerSec + 0.01);
	TestTrue(TEXT("informant Run: the effective gait is Walk"), Movement->GetEffectiveGait() == EGait::Walk);
	TestFalse(TEXT("informant Run: never IsRunning"), Movement->IsRunning());

	// The traversal mode forced directly is refused (backstop) and reported.
	const int32 HitsBefore = Movement->GetBackstopHitCount();
	Movement->SetMovementMode(UStealthMovementComponent::TraversalMovementMode);
	TestTrue(TEXT("informant: the traversal mode switch is refused"), Movement->MovementMode == MOVE_Walking);
	TestEqual(TEXT("informant: the refusal is a backstop hit"), Movement->GetBackstopHitCount(), HitsBefore + 1);
	TestTrue(TEXT("informant: the refusal reaches the fault sink as MovementMisuse/RefusedTraversalMode"),
		CountMovementMisuse(Faults) == 1 && CountMovementMisuse(Faults, static_cast<int64>(EStealthMovementMisuse::RefusedTraversalMode)) == 1);

	// A planted brace is refused the same way.
	AStealthPawn* Braced = TestWorld.SpawnPawn(FVector2D(-5000.0, 1000.0));
	Braced->GetStealthMovement()->SetFaultSink(&Faults);
	Braced->GetStealthMovement()->SetActionPose(EStealthActionPose::Brace);
	Braced->GetStealthMovement()->SetMovementMode(UStealthMovementComponent::TraversalMovementMode);
	TestTrue(TEXT("braced: the traversal mode switch is refused"), Braced->GetStealthMovement()->MovementMode == MOVE_Walking);

	// Control: an Active pawn may enter the traversal mode.
	AStealthPawn* Active = TestWorld.SpawnPawn(FVector2D(-5000.0, 2000.0));
	Active->GetStealthMovement()->SetFaultSink(&Faults);
	Active->GetStealthMovement()->SetMovementMode(UStealthMovementComponent::TraversalMovementMode);
	TestTrue(TEXT("control: an Active pawn enters the traversal mode"), Active->GetStealthMovement()->MovementMode == UStealthMovementComponent::TraversalMovementMode);
	TestEqual(TEXT("control: two refusals in all (informant, braced)"), CountMovementMisuse(Faults), 2);

	Movement->SetGaitLegalitySource(nullptr);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
