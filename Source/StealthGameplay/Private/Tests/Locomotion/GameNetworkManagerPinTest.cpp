// Story SCL-003 (production/epics/shared-character-locomotion/story-003-tuning-keys-registered.md), invariant I-SCL-15
// (ADR-0010 section 6; TR-scl-014): the engine's movement-network values are read from the AGameNetworkManager class
// default object (GlobalConfig, Config/DefaultGame.ini [/Script/Engine.GameNetworkManager]), so the boot test asserts
// the CDO carries the pinned values and that they satisfy the SCL keys: MAXCLIENTUPDATEINTERVAL == InputStarvationSec
// (0.25 s), MaxClientForcedUpdateDuration 1.40 s >= InputStarvationSec + T_stop,max + 0.25 s, MaxMoveDeltaTime <=
// MAXCLIENTUPDATEINTERVAL, and both movement-time discrepancy flags on. The same check runs as the I-SCL-15 invariant row
// at preset load (server boot). Negative controls: drifted engine values (interval, duration, a flag), and the default values with
// a key moved out of step, both fail the check.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "GameFramework/GameNetworkManager.h"
#include "Server/StealthServerTuning.h"
#include "Server/StealthTuningInvariants.h"

namespace GameNetworkManagerPinTestData
{
	/** The values pinned in Config/DefaultGame.ini (ADR-0010 "Config applied", cross-review 2026-10-25-b). */
	constexpr float PinnedMaxClientUpdateInterval = 0.25f;
	constexpr float PinnedMaxClientForcedUpdateDuration = 1.40f;
	constexpr float PinnedMaxMoveDeltaTime = 0.125f;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSclGameNetworkManagerPinTest, "StealthGame.Locomotion.GameNetworkManagerPin",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSclGameNetworkManagerPinTest::RunTest(const FString& Parameters)
{
	using namespace GameNetworkManagerPinTestData;
	const AGameNetworkManager* Cdo = GetDefault<AGameNetworkManager>();
	const FStealthServerTuning Defaults;

	// The CDO carries the pinned config values.
	TestEqual(TEXT("MAXCLIENTUPDATEINTERVAL is pinned"), Cdo->MAXCLIENTUPDATEINTERVAL, PinnedMaxClientUpdateInterval);
	TestEqual(TEXT("MaxClientForcedUpdateDuration is pinned"), Cdo->MaxClientForcedUpdateDuration, PinnedMaxClientForcedUpdateDuration);
	TestEqual(TEXT("MaxMoveDeltaTime is pinned"), Cdo->MaxMoveDeltaTime, PinnedMaxMoveDeltaTime);
	TestTrue(TEXT("bMovementTimeDiscrepancyDetection is on"), Cdo->bMovementTimeDiscrepancyDetection);
	TestTrue(TEXT("bMovementTimeDiscrepancyResolution is on"), Cdo->bMovementTimeDiscrepancyResolution);

	// The CDO satisfies the SCL keys (the I-SCL-15 boot check).
	TestEqual(TEXT("MAXCLIENTUPDATEINTERVAL equals InputStarvationSec"), Cdo->MAXCLIENTUPDATEINTERVAL, Defaults.Locomotion_Server.InputStarvationSec);
	const StealthTuningEngineNetPins::FValues Pinned = StealthTuningEngineNetPins::Read(*Cdo);
	TestTrue(TEXT("I-SCL-15 holds for the CDO and the default keys"), StealthTuningEngineNetPins::Check(Pinned, Defaults));

	// Negative controls: drifted engine values and an out-of-step key are both caught by the same check.
	{
		StealthTuningEngineNetPins::FValues Drifted = Pinned;
		Drifted.MaxClientUpdateInterval = 0.3f;
		TestFalse(TEXT("negative control: a manager interval out of step with InputStarvationSec fails"), StealthTuningEngineNetPins::Check(Drifted, Defaults));
		Drifted = Pinned;
		Drifted.MaxClientForcedUpdateDuration = 1.0f;
		TestFalse(TEXT("negative control: a forced-update duration shorter than starvation + longest stop + 0.25 fails"), StealthTuningEngineNetPins::Check(Drifted, Defaults));
		Drifted = Pinned;
		Drifted.bMovementTimeDiscrepancyDetection = false;
		TestFalse(TEXT("negative control: discrepancy detection off fails"), StealthTuningEngineNetPins::Check(Drifted, Defaults));
	}
	{
		FStealthServerTuning Slow = Defaults;
		Slow.Locomotion_Public.WalkStopSec = 1.2f; // T_stop,max = 1.5 s: 0.25 + 1.5 + 0.25 > 1.40
		TestFalse(TEXT("negative control: a longest stop that no longer fits the pinned duration fails"), StealthTuningEngineNetPins::Check(Pinned, Slow));
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
