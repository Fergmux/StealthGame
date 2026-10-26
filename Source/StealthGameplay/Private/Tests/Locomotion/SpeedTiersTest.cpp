// Story SCL-010 (production/epics/shared-character-locomotion/story-010-speed-tiers.md), AC-SCL-05 SpeedTiers: GIVEN
// default tuning in a minimal test world, WHEN Move is issued with (m = 1, Walk), (0.9, Walk), (0.5, Walk), (1, Run),
// (1, Run) as an informant, (0, Run) and (1, Walk) while braced, THEN steady speeds are 140, 126, 112, 450, 140, 0 and
// 0 cm/s (+- 1); AND a client stick reading of 0.19 is sent as 0 (the 0.2 dead zone; a probe above 0.2 is only asserted
// non-zero). The quantiser is also tested directly: (0, 0.8] -> 0.8, the +-0.03 hysteresis around 0.85 and 0.95, and
// m <= 0 -> no input. The informant legality comes from an injected stub IStealthGaitLegalitySource; the brace pose from
// a stub pose write (the IIF FActionState writer is not built). Braced Run is clamped to Walk (never cancels the brace).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/Locomotion/LocomotionTestWorld.h"
#include "View/Core/StealthLocomotionModel.h"

namespace SpeedTiersTestData
{
	/** Stub legality source: one fixed class for every pawn it is injected into. */
	class FStubLegality final : public IStealthGaitLegalitySource
	{
	public:
		explicit FStubLegality(EStealthGaitLegality InClass) : Class(InClass) {}
		virtual EStealthGaitLegality GetGaitLegality(const APawn&) const override { return Class; }
		EStealthGaitLegality Class;
	};

	struct FTierCase
	{
		const TCHAR* Name;
		float Magnitude;
		EGait Gait;
		bool bInformant;
		bool bBraced;
		float ExpectedSpeed;
	};
	const FTierCase Cases[] = {
		{ TEXT("(1, Walk)"), 1.0f, EGait::Walk, false, false, 140.0f },
		{ TEXT("(0.9, Walk)"), 0.9f, EGait::Walk, false, false, 126.0f },
		{ TEXT("(0.5, Walk)"), 0.5f, EGait::Walk, false, false, 112.0f },
		{ TEXT("(1, Run)"), 1.0f, EGait::Run, false, false, 450.0f },
		{ TEXT("(1, Run) informant"), 1.0f, EGait::Run, true, false, 140.0f },
		{ TEXT("(0, Run)"), 0.0f, EGait::Run, false, false, 0.0f },
		{ TEXT("(1, Walk) braced"), 1.0f, EGait::Walk, false, true, 0.0f },
	};
	/** 3 s to reach a steady state from rest (the slowest ramp is 0.6 s). */
	constexpr int32 SettleFrames = 180;
	constexpr double SpeedTolerance = 1.0;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLocomotionSpeedTiersTest, "StealthGame.Locomotion.SpeedTiers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FLocomotionSpeedTiersTest::RunTest(const FString& Parameters)
{
	using namespace SpeedTiersTestData;

	// The pace quantiser (F1 q(m)).
	TestEqual(TEXT("q(0.5) = 0.8"), StealthLocomotion::QuantisePace(0.5f, 0.0f), 0.8f);
	TestEqual(TEXT("q(0.01) = 0.8"), StealthLocomotion::QuantisePace(0.01f, 0.0f), 0.8f);
	TestEqual(TEXT("q(0.9) = 0.9"), StealthLocomotion::QuantisePace(0.9f, 0.0f), 0.9f);
	TestEqual(TEXT("q(1.0) = 1.0"), StealthLocomotion::QuantisePace(1.0f, 0.0f), 1.0f);
	TestEqual(TEXT("q(0) = no input"), StealthLocomotion::QuantisePace(0.0f, 0.8f), 0.0f);
	TestEqual(TEXT("hysteresis: 0.87 from 0.8 stays 0.8"), StealthLocomotion::QuantisePace(0.87f, 0.8f), 0.8f);
	TestEqual(TEXT("hysteresis: 0.89 from 0.8 goes to 0.9"), StealthLocomotion::QuantisePace(0.89f, 0.8f), 0.9f);
	TestEqual(TEXT("hysteresis: 0.83 from 0.9 stays 0.9"), StealthLocomotion::QuantisePace(0.83f, 0.9f), 0.9f);
	TestEqual(TEXT("hysteresis: 0.81 from 0.9 goes to 0.8"), StealthLocomotion::QuantisePace(0.81f, 0.9f), 0.8f);
	TestEqual(TEXT("hysteresis: 0.97 from 0.9 stays 0.9"), StealthLocomotion::QuantisePace(0.97f, 0.9f), 0.9f);
	TestEqual(TEXT("hysteresis: 1.0 from 0.8 goes to 1.0"), StealthLocomotion::QuantisePace(1.0f, 0.8f), 1.0f);

	// The owning client's dead zone (applied before sending; the server applies none).
	TestTrue(TEXT("a 0.19 stick reading is sent as 0"), StealthLocomotion::ApplyStickDeadZone(FVector2D(0.0, 0.19)).IsZero());
	TestTrue(TEXT("a 0.25 stick reading is sent non-zero"), !StealthLocomotion::ApplyStickDeadZone(FVector2D(0.0, 0.25)).IsZero());
	TestTrue(TEXT("a stick reading is clamped to length 1"), FMath::IsNearlyEqual(StealthLocomotion::ApplyStickDeadZone(FVector2D(1.0, 1.0)).Size(), 1.0, 1e-6));

	// Effective-gait clamps (R5.2).
	TestTrue(TEXT("informant Run -> Walk"), StealthLocomotion::EffectiveGait(EGait::Run, EStealthGaitLegality::Informant, EStealthActionPose::None) == EGait::Walk);
	TestTrue(TEXT("braced Run -> Walk"), StealthLocomotion::EffectiveGait(EGait::Run, EStealthGaitLegality::Active, EStealthActionPose::Brace) == EGait::Walk);
	TestTrue(TEXT("interacting Run is not clamped (the IIF cancels the interaction)"), StealthLocomotion::EffectiveGait(EGait::Run, EStealthGaitLegality::Active, EStealthActionPose::InteractPose) == EGait::Run);

	// World: each case through the Move entry point, steady speed after 3 s.
	StealthLocomotionTest::FWorld TestWorld(TEXT("SpeedTiersTestWorld"));
	FStubLegality Informant(EStealthGaitLegality::Informant);
	int32 Lane = 0;
	for (const FTierCase& Case : Cases)
	{
		AStealthPawn* Pawn = TestWorld.SpawnPawn(FVector2D(-5000.0, -6000.0 + 700.0 * Lane++));
		UStealthMovementComponent* Movement = Pawn->GetStealthMovement();
		if (Case.bInformant)
		{
			Movement->SetGaitLegalitySource(&Informant);
		}
		if (Case.bBraced)
		{
			Movement->SetActionPose(EStealthActionPose::Brace);
		}
		StealthLocomotionTest::DriveMove(Pawn, 0.0f, Case.Magnitude, Case.Gait);
		double Peak = 0.0;
		for (int32 Frame = 0; Frame < SettleFrames; ++Frame)
		{
			StealthLocomotionTest::StepPawn(Pawn);
			Peak = FMath::Max(Peak, StealthLocomotionTest::Speed2D(Pawn));
		}
		const double Steady = StealthLocomotionTest::Speed2D(Pawn);
		AddInfo(FString::Printf(TEXT("SPEEDTIERS %s: steady %.2f cm/s (peak %.2f), effective gait %s, pace %.2f"), Case.Name, Steady, Peak,
			Movement->GetEffectiveGait() == EGait::Run ? TEXT("Run") : TEXT("Walk"), Movement->GetEffectivePace()));
		TestTrue(FString::Printf(TEXT("%s: steady speed %.2f is %.0f +- 1"), Case.Name, Steady, Case.ExpectedSpeed), FMath::Abs(Steady - Case.ExpectedSpeed) <= SpeedTolerance);
		TestTrue(FString::Printf(TEXT("%s: never above %.0f + 1"), Case.Name, Case.ExpectedSpeed), Peak <= Case.ExpectedSpeed + SpeedTolerance);
		if (Case.bBraced)
		{
			TestTrue(TEXT("braced: the brace is not cancelled by Move"), Movement->GetActionPose() == EStealthActionPose::Brace);
			Pawn->GetPawnCommand()->Move(StealthLocomotionTest::MoveDirection(0.0f, 1.0f), EGait::Run);
			for (int32 Frame = 0; Frame < 60; ++Frame)
			{
				StealthLocomotionTest::StepPawn(Pawn);
			}
			TestTrue(TEXT("braced Run: clamped to Walk, still no speed, brace kept"), Movement->GetEffectiveGait() == EGait::Walk && StealthLocomotionTest::Speed2D(Pawn) <= SpeedTolerance && Movement->GetActionPose() == EStealthActionPose::Brace);
		}
		Movement->SetGaitLegalitySource(nullptr);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
