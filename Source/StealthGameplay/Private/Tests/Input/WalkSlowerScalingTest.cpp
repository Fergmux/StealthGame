// Story iif-017 (production/epics/input-interaction-framework/story-017-walk-slower-scaling.md), GDD AC 76 client
// clause (R15a, Logic, U WalkSlowerScaling): GIVEN a keyboard walker holding W, WHEN it holds Left Ctrl, then Left Alt,
// then both, then neither, THEN the Move magnitude the owning client produces is 0.8, 0.9, 0.8 and 1.0; holding Run
// with Left Ctrl gives run speed (no scale); AND the gamepad stick's magnitudes are unchanged by the modifier.
// Also: diagonal W+D with Ctrl is 0.8 (clamp, not 1.13); "both" is a minimum (not the highest-absolute accumulation of
// the raw paces); IA_WalkSlower's reduction value (Ctrl 0.2, Alt 0.1, highest of both 0.2) maps to the same paces;
// releasing returns 1.0 at once (smoothing is SCL's).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "View/Core/WalkSlowerScale.h"

namespace WalkSlowerScalingTestData
{
	/** The IA_WalkSlower mapping scalars of story iif-007 (InputAssetSpec.h). */
	constexpr float CtrlReduction = 0.2f;
	constexpr float AltReduction = 0.1f;
	constexpr double Tolerance = 1e-4;

	const FVector2D HoldW(0.0, 1.0);
	const FVector2D HoldWD(1.0, 1.0);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInputWalkSlowerScalingTest, "StealthGame.Input.WalkSlowerScaling",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FInputWalkSlowerScalingTest::RunTest(const FString& Parameters)
{
	using namespace WalkSlowerScalingTestData;
	using namespace StealthWalkSlower;

	struct FCase
	{
		const TCHAR* Name;
		bool bCtrl;
		bool bAlt;
		double Expected;
	};
	const FCase Sequence[] = {
		{ TEXT("Left Ctrl"), true, false, StealthPaceSet::Slow },
		{ TEXT("Left Alt"), false, true, StealthPaceSet::Middle },
		{ TEXT("both"), true, true, StealthPaceSet::Slow },
		{ TEXT("neither"), false, false, StealthPaceSet::Full },
	};
	for (const FCase& Case : Sequence)
	{
		const FVector2D Sent = ScaleMove(HoldW, EMoveInputDevice::Keyboard, PaceFromModifiers(Case.bCtrl, Case.bAlt), false);
		TestEqual(FString::Printf(TEXT("W + %s: magnitude"), Case.Name), Sent.Size(), Case.Expected, Tolerance);
		TestTrue(FString::Printf(TEXT("W + %s: direction unchanged"), Case.Name), Sent.GetSafeNormal().Equals(HoldW, Tolerance));

		// The same pace from IA_WalkSlower's accumulated value (highest absolute of the held keys' reductions).
		const float Reduction = FMath::Max(Case.bCtrl ? CtrlReduction : 0.f, Case.bAlt ? AltReduction : 0.f);
		TestEqual(FString::Printf(TEXT("%s: the action value maps to the same pace"), Case.Name), PaceFromReduction(Reduction), PaceFromModifiers(Case.bCtrl, Case.bAlt), 1e-6f);
	}

	// "Both" is an explicit minimum: the highest-absolute accumulation of the raw paces would have given 0.9.
	TestEqual(TEXT("both held is 0.8, not max(0.8, 0.9)"), PaceFromModifiers(true, true), StealthPaceSet::Slow, 1e-6f);

	// Diagonal: clamped to 1 before scaling.
	TestEqual(TEXT("W+D with Ctrl is 0.8, not 1.13"), ScaleMove(HoldWD, EMoveInputDevice::Keyboard, PaceFromModifiers(true, false), false).Size(), static_cast<double>(StealthPaceSet::Slow), Tolerance);
	TestEqual(TEXT("W+D alone is 1.0"), ScaleMove(HoldWD, EMoveInputDevice::Keyboard, PaceFromModifiers(false, false), false).Size(), 1.0, Tolerance);

	// Run held with Ctrl: no scale (Run has no scale; the gait is Run).
	TestEqual(TEXT("Run + Ctrl gives full magnitude (run speed)"), ScaleMove(HoldW, EMoveInputDevice::Keyboard, PaceFromModifiers(true, false), true).Size(), 1.0, Tolerance);

	// Gamepad: magnitudes unchanged by the modifier.
	const FVector2D Sticks[] = { FVector2D(0.0, 0.5), FVector2D(0.3, 0.7), FVector2D(0.0, 1.0) };
	for (const FVector2D& Stick : Sticks)
	{
		TestTrue(FString::Printf(TEXT("gamepad %s unchanged with Ctrl"), *Stick.ToString()), ScaleMove(Stick, EMoveInputDevice::Gamepad, PaceFromModifiers(true, false), false).Equals(Stick));
		TestTrue(FString::Printf(TEXT("gamepad %s unchanged with Alt"), *Stick.ToString()), ScaleMove(Stick, EMoveInputDevice::Gamepad, PaceFromModifiers(false, true), false).Equals(Stick));
	}

	// Release mid-walk: 1.0 at once; no input gives zero; a rebound key cannot leave the pace set.
	TestEqual(TEXT("release returns 1.0 the same frame"), ScaleMove(HoldW, EMoveInputDevice::Keyboard, PaceFromReduction(0.f), false).Size(), 1.0, Tolerance);
	TestTrue(TEXT("no Move input stays zero"), ScaleMove(FVector2D::ZeroVector, EMoveInputDevice::Keyboard, StealthPaceSet::Slow, false).IsZero());
	TestEqual(TEXT("an out-of-set reduction is clamped to the slowest pace"), PaceFromReduction(0.7f), StealthPaceSet::Slow, 1e-6f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
