// Story SCL-047 (production/epics/shared-character-locomotion/story-047-traversal-eligibility-matrix.md), AC-SCL-12
// TraversalEligibilityMatrix (F5, heights final per spike SCL-045: vault 0.8 m, hurdle 1.2 m, climb 1.6 m): GIVEN coping
// at 45, 75, 83, 100, 120, 155, 165 and 175 cm, WHEN an idle Active pawn presses Traverse at 70 cm and 20 degrees off
// facing, THEN results are none, vault, vault, none, hurdle (the 1.2 m band added by the spike; the AC written before it
// read none), climb, climb, none; AND with the camera turned 180 degrees but the body unchanged the results are identical
// (the camera is not an F5 input: the query carries body facing only, so the result is evaluated twice with a camera
// yaw that has nowhere to enter); AND with a pawn on the landing cell (80 cm coping) the press is rejected (NotNow at the
// IIF, LandingOccupied here). Also: an informant, a busy slot, an airborne pawn and a pawn already traversing are
// refused; reach and the facing cone bound eligibility; selection takes the nearest, then the straightest, then the
// lowest id. Fixtures are built from the tuning snapshot values (no hard-coded capability heights).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "View/Core/StealthLocomotionModel.h"

namespace TraversalEligibilityTestData
{
	constexpr float PressDistanceCm = 70.0f;
	constexpr float OffFacingDeg = 20.0f;

	FStealthCopingCandidate Coping(int32 Id, float Height, float Distance = PressDistanceCm, float NormalYaw = OffFacingDeg)
	{
		FStealthCopingCandidate C;
		C.Id = Id;
		C.HeightCm = Height;
		C.DistanceCm = Distance;
		C.InwardNormalYawDeg = NormalYaw;
		C.LandingClearCm = 120.0f;
		return C;
	}

	FStealthTraversalDecision Press(const FStealthCopingCandidate& C, const FLocomotionTuning_Public& Tuning, float BodyYaw = 0.0f)
	{
		FStealthTraversalQuery Query;
		Query.BodyFacingYawDeg = BodyYaw;
		return StealthLocomotion::EvaluateTraversal(Query, MakeArrayView(&C, 1), Tuning);
	}

	const TCHAR* Name(const FStealthTraversalDecision& D)
	{
		if (!D.bEligible) { return TEXT("none"); }
		switch (D.Type)
		{
		case EStealthCopingClass::Vault: return TEXT("vault");
		case EStealthCopingClass::Hurdle: return TEXT("hurdle");
		case EStealthCopingClass::Climb: return TEXT("climb");
		default: return TEXT("?");
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLocomotionTraversalEligibilityMatrixTest, "StealthGame.Locomotion.TraversalEligibilityMatrix",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FLocomotionTraversalEligibilityMatrixTest::RunTest(const FString& Parameters)
{
	using namespace TraversalEligibilityTestData;
	const FLocomotionTuning_Public Tuning;
	const float Min = Tuning.TraversalMinHeightCm;
	const float Vault = Tuning.TraversalVaultHeightCm;
	const float Hurdle = Tuning.TraversalHurdleHeightCm;
	const float Climb = Tuning.TraversalClimbHeightCm;
	const float Tol = Tuning.TraversalHeightToleranceCm;

	// The AC heights as offsets from the snapshot (45, 75, 83, 100, 120, 155, 165, 175 at the defaults).
	struct FRow { float Height; const TCHAR* Expected; };
	const FRow Rows[] = {
		{ Min, TEXT("none") }, { Vault - 5.0f, TEXT("vault") }, { Vault + 3.0f, TEXT("vault") }, { Vault + 2.0f * Tol, TEXT("none") },
		{ Hurdle, TEXT("hurdle") }, { Climb - 5.0f, TEXT("climb") }, { Climb + 5.0f, TEXT("climb") }, { Climb + 1.5f * Tol, TEXT("none") },
	};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Rows); ++Index)
	{
		const FStealthTraversalDecision Body = Press(Coping(Index, Rows[Index].Height), Tuning);
		// The camera turned 180 degrees: it is not an input, so the identical query gives the identical result.
		const float CameraYaw = 180.0f;
		(void)CameraYaw;
		const FStealthTraversalDecision CameraTurned = Press(Coping(Index, Rows[Index].Height), Tuning);
		TestEqual(FString::Printf(TEXT("coping %.0f cm at 70 cm, 20 deg off facing"), Rows[Index].Height), FString(Name(Body)), FString(Rows[Index].Expected));
		TestEqual(FString::Printf(TEXT("coping %.0f cm: camera turned 180, body unchanged: identical"), Rows[Index].Height), FString(Name(CameraTurned)), FString(Name(Body)));
	}

	// Body facing (not the camera) is what F5 reads: turning the body 180 makes the 80 cm coping ineligible.
	TestFalse(TEXT("body turned 180: out of the facing cone"), Press(Coping(1, Vault), Tuning, 180.0f).bEligible);

	// A pawn on the landing cell: rejected.
	{
		FStealthCopingCandidate Occupied = Coping(9, Vault);
		Occupied.LandingOccupants = 1;
		const FStealthTraversalDecision D = Press(Occupied, Tuning);
		TestTrue(TEXT("80 cm coping with a pawn on the landing cell: rejected (LandingOccupied, the IIF replies NotNow)"), !D.bEligible && D.Refusal == EStealthTraversalRefusal::LandingOccupied);
	}

	// Reach, cone and landing clearance bound eligibility.
	TestFalse(TEXT("beyond TraversalReachCm: none"), Press(Coping(1, Vault, Tuning.TraversalReachCm + 1.0f), Tuning).bEligible);
	TestTrue(TEXT("at TraversalReachCm: eligible"), Press(Coping(1, Vault, Tuning.TraversalReachCm), Tuning).bEligible);
	TestFalse(TEXT("outside the facing cone: none"), Press(Coping(1, Vault, PressDistanceCm, Tuning.TraversalFacingHalfAngleDeg + 1.0f), Tuning).bEligible);
	{
		FStealthCopingCandidate Short = Coping(1, Vault);
		Short.LandingClearCm = StealthLocomotion::PendingTraversalLandingClearCm - 1.0f;
		TestFalse(TEXT("landing clearance below TraversalLandingClearCm: none"), Press(Short, Tuning).bEligible);
	}

	// Who may traverse (R7.3): only an Active pawn with a free slot, grounded, not already traversing.
	{
		const FStealthCopingCandidate C = Coping(1, Vault);
		FStealthTraversalQuery Q;
		Q.Legality = EStealthGaitLegality::Informant;
		TestTrue(TEXT("informant: refused NotActive"), StealthLocomotion::EvaluateTraversal(Q, MakeArrayView(&C, 1), Tuning).Refusal == EStealthTraversalRefusal::NotActive);
		Q = FStealthTraversalQuery();
		Q.bSlotAllows = false;
		TestTrue(TEXT("bracing / striking slot: refused SlotBusy"), StealthLocomotion::EvaluateTraversal(Q, MakeArrayView(&C, 1), Tuning).Refusal == EStealthTraversalRefusal::SlotBusy);
		Q = FStealthTraversalQuery();
		Q.bGrounded = false;
		TestTrue(TEXT("airborne: refused NotGrounded"), StealthLocomotion::EvaluateTraversal(Q, MakeArrayView(&C, 1), Tuning).Refusal == EStealthTraversalRefusal::NotGrounded);
		Q = FStealthTraversalQuery();
		Q.bInTraversal = true;
		TestTrue(TEXT("already traversing: refused"), StealthLocomotion::EvaluateTraversal(Q, MakeArrayView(&C, 1), Tuning).Refusal == EStealthTraversalRefusal::AlreadyTraversing);
	}

	// Selection: nearest, then straightest, then lowest id.
	{
		const FStealthCopingCandidate Candidates[] = { Coping(5, Vault, 80.0f, 0.0f), Coping(3, Climb, 60.0f, 30.0f), Coping(4, Climb, 60.0f, 10.0f), Coping(2, Climb, 60.0f, 10.0f) };
		const FStealthTraversalDecision D = StealthLocomotion::EvaluateTraversal(FStealthTraversalQuery(), Candidates, Tuning);
		TestTrue(TEXT("selection: nearest, then straightest, then lowest id (id 2, climb)"), D.bEligible && D.CopingId == 2 && D.Type == EStealthCopingClass::Climb);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
