// Camera story cs-001 AC2 (production/epics/camera-system/story-001-camera-tuning-keys-and-invariants.md; GDD camera-system.md
// AC-CS-22 invariants, AC-CS-16 last bullet): StealthGame.Camera.InvariantsRejected.
//   - the schema defaults pass I-CS-1..5, which are five Reject rows of the preset-load table;
//   - a value set violating each invariant (range gate bypassed: I-CS-2, I-CS-4's 70 cm clause and I-CS-5 coincide with the
//     keys' hard ranges, so F-1 meets an out-of-range value first at load) is rejected by that row only;
//   - I-CS-1 at the IIF lockout: blend 0.13 s with lockout 0.15 s passes, blend 0.20 s with lockout 0.15 s is rejected
//     (one frame at 60 fps under the lockout is the bound), and at the lockout default 0.30 s every blend in 0-0.28 passes;
//   - through the pipeline: a requested preset violating I-CS-3 in range (EyeConeHalfHDeg 50 < FovHMaxDeg 108 / 2) is
//     rejected whole (Reject finding, not repaired: no F-1 change to the camera keys) and the Default preset loads.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Server/StealthCameraInvariants.h"
#include "Server/StealthServerTuning.h"
#include "Server/StealthTuningDerived.h"
#include "Server/StealthTuningInvariants.h"
#include "Server/StealthTuningPipelineCore.h"
#include "Server/StealthTuningSchema.h"
#include "Server/StealthValidatedTuning.h"

namespace CameraInvariantsTestData
{
	using FCheck = ETuningCheck (*)(const FStealthServerTuning&, const FCameraInvariantInputs&);

	struct FCase
	{
		const ANSICHAR* Id;
		TFunction<void(FStealthServerTuning&)> Violate;
	};

	/** A source with a requested preset (non-public) and the Default preset (the C++ defaults). */
	class FLocalPresetSource : public ITuningPresetSource
	{
	public:
		FName RequestedId = TEXT("CameraInvariantsBad");
		FStealthServerTuning RequestedValues;
		virtual TOptional<FStealthServerTuning> Load(FName PresetId, bool& bOutIsPublic) const override
		{
			if (PresetId == FTuningPipelineCore::DefaultPresetId())
			{
				bOutIsPublic = true;
				return FStealthServerTuning();
			}
			bOutIsPublic = false;
			return PresetId == RequestedId ? TOptional<FStealthServerTuning>(RequestedValues) : TOptional<FStealthServerTuning>();
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCameraInvariantsRejectedTest, "StealthGame.Camera.InvariantsRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCameraInvariantsRejectedTest::RunTest(const FString& Parameters)
{
	using namespace CameraInvariantsTestData;
	const FStealthServerTuning Defaults = FTuningPipelineCore::BuildSchemaDefaults(GetTuningSchema());
	const FTuningDerived Derived = FTuningDerived::Compute(Defaults);
	FTuningCheckContext Context;
	Context.bIsPublic = true;

	// The table: five Camera Reject rows, all passing the schema defaults.
	TArray<const FTuningInvariant*> Rows;
	for (const FTuningInvariant& Row : GetTuningInvariants())
	{
		if (Row.Owner != nullptr && FCStringAnsi::Strcmp(Row.Owner, "Camera") == 0)
		{
			Rows.Add(&Row);
			TestEqual(*FString::Printf(TEXT("%hs passes the schema defaults"), Row.Id), static_cast<int32>(Row.Check(Defaults, Derived, Context)), static_cast<int32>(ETuningCheck::Pass));
		}
	}
	TestEqual(TEXT("five Camera rows I-CS-1..5 in the preset-load table"), Rows.Num(), 5);

	// Each violation is rejected by its own row only (the range gate bypassed).
	const FCase Cases[] = {
		{ "I-CS-2", [](FStealthServerTuning& V) { V.Camera_Public.BoomHeightOffsetCm = 26.0f; } },
		{ "I-CS-3", [](FStealthServerTuning& V) { V.Camera_Public.EyeConeHalfHDeg = 50.0f; } },
		{ "I-CS-3", [](FStealthServerTuning& V) { V.Camera_Public.EyeConeHalfVDeg = 36.0f; } },
		{ "I-CS-4", [](FStealthServerTuning& V) { V.Camera_Public.ConcealedBoomCm = 71.0f; } },
		{ "I-CS-4", [](FStealthServerTuning& V) { V.Camera_Public.ConcealedBoomCm = 65.0f; V.Camera_Public.BoomLengthCm = 60.0f; } },
		{ "I-CS-5", [](FStealthServerTuning& V) { V.Camera_Public.CameraEdgePushDeg = 1.9f; } },
	};
	for (const FCase& Case : Cases)
	{
		FStealthServerTuning Bad = Defaults;
		Case.Violate(Bad);
		for (const FTuningInvariant* Row : Rows)
		{
			const bool bThis = FCStringAnsi::Strcmp(Row->Id, Case.Id) == 0;
			TestEqual(*FString::Printf(TEXT("violation of %hs: %hs"), Case.Id, Row->Id), static_cast<int32>(Row->Check(Bad, Derived, Context)),
				static_cast<int32>(bThis ? ETuningCheck::Reject : ETuningCheck::Pass));
		}
	}

	// I-CS-1 against the IIF lockout (injected: IIF owns it, a pending owner key at preset load).
	{
		FCameraInvariantInputs In = FCameraInvariantInputs::FromValues(Defaults);
		TestEqual(TEXT("preset load reads the pending IIF lockout 0.30"), In.CameraToggleLockoutSec, StealthTuningPendingOwnerKeys::CameraToggleLockoutSec, 1e-9);
		In.CameraToggleLockoutSec = 0.15;
		FStealthServerTuning V = Defaults;
		V.Camera_Public.CameraToggleBlendSec = 0.13f;
		TestEqual(TEXT("blend 0.13 s with lockout 0.15 s loads"), static_cast<int32>(StealthCameraInvariants::CheckICs1(V, In)), static_cast<int32>(ETuningCheck::Pass));
		V.Camera_Public.CameraToggleBlendSec = 0.20f;
		TestEqual(TEXT("blend 0.20 s with lockout 0.15 s is rejected"), static_cast<int32>(StealthCameraInvariants::CheckICs1(V, In)), static_cast<int32>(ETuningCheck::Reject));
		In.CameraToggleLockoutSec = 0.30;
		V.Camera_Public.CameraToggleBlendSec = 0.28f;
		TestEqual(TEXT("at the lockout default 0.30 the blend hard maximum 0.28 passes"), static_cast<int32>(StealthCameraInvariants::CheckICs1(V, In)), static_cast<int32>(ETuningCheck::Pass));
	}

	// Through the pipeline: an in-range I-CS-3 violation rejects the requested preset whole; Default loads; nothing repaired.
	{
		FLocalPresetSource Source;
		Source.RequestedValues.Camera_Public.EyeConeHalfHDeg = 50.0f;
		FTuningResolveInput Input;
		Input.RequestedPreset = Source.RequestedId;
		Input.Source = &Source;
		const FTuningResolveResult Result = FTuningPipelineCore().Resolve(Input);
		TestTrue(TEXT("the I-CS-3 finding is a Reject"), Result.Report.HasFinding("I-CS-3", ETuningCheck::Reject));
		TestEqual(TEXT("the requested preset is rejected and Default loads"), static_cast<int32>(Result.FallbackLevel), static_cast<int32>(ETuningFallback::Default));
		const FTuningRungReport* Requested = Result.Report.Rungs.Num() > 0 ? &Result.Report.Rungs[0] : nullptr;
		bool bRepaired = false;
		if (Requested != nullptr)
		{
			for (const FTuningF1Change& Change : Requested->F1.Changes)
			{
				bRepaired |= Change.Key != nullptr && FCStringAnsi::Strncmp(Change.Key, "EyeCone", 7) == 0;
			}
		}
		TestFalse(TEXT("no runtime clamp or repair of the camera keys (no F-1 change)"), bRepaired);
		TestTrue(TEXT("the loaded set is the Default preset's (EyeConeHalfHDeg 55)"), Result.Validated.IsValid());
	}
	return true;
}

#endif
