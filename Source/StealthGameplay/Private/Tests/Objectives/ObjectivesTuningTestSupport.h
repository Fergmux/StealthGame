#pragma once

// Shared fixture of the Objectives tuning tests (story oc-001): a test-local preset source over the REAL pipeline
// (FTuningPipelineCore::Resolve with the production invariant table), as the Penalty and Foundation tuning tests build
// presets in code (the project holds no checked-in test preset data). Test-only.

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Server/StealthServerTuning.h"
#include "Server/StealthTuningInvariants.h"
#include "Server/StealthTuningPipelineCore.h"
#include "Server/StealthValidatedTuning.h"
#include "Server/Testing/StealthLogCapture.h"

namespace StealthObjectivesTuningTest
{
	/** The requested preset of every case (non-public, unflagged). */
	inline FName RequestedPresetId() { return FName(TEXT("ObjectivesInvariantCase")); }

	/** Default is the C++ defaults; the requested preset is the C++ defaults with the case's authoring applied. */
	class FLocalPresetSource : public ITuningPresetSource
	{
	public:
		FStealthServerTuning RequestedValues;
		FStealthServerTuning DefaultValues;

		virtual TOptional<FStealthServerTuning> Load(FName PresetId, bool& bOutIsPublic) const override
		{
			if (PresetId == FTuningPipelineCore::DefaultPresetId())
			{
				bOutIsPublic = true;
				return DefaultValues;
			}
			bOutIsPublic = false;
			return PresetId == RequestedPresetId() ? TOptional<FStealthServerTuning>(RequestedValues) : TOptional<FStealthServerTuning>();
		}
	};

	/** A source whose requested preset applies Author on top of the C++ defaults. */
	inline FLocalPresetSource MakeSource(TFunctionRef<void(FStealthServerTuning&)> Author)
	{
		FLocalPresetSource Source;
		Author(Source.RequestedValues);
		return Source;
	}

	/** Resolves the requested preset and stops the capture so only this resolve's warnings count. */
	inline FTuningResolveResult Resolve(const FLocalPresetSource& Source, FStealthLogCapture& Capture)
	{
		FTuningResolveInput Input;
		Input.RequestedPreset = RequestedPresetId();
		Input.Source = &Source;
		const FTuningResolveResult Result = FTuningPipelineCore().Resolve(Input);
		Capture.Stop();
		return Result;
	}

	/** The first (requested) rung of a resolve, or null. */
	inline const FTuningRungReport* RequestedRung(const FTuningResolveResult& Result)
	{
		return Result.Report.Rungs.Num() > 0 && Result.Report.Rungs[0].PresetId == RequestedPresetId() ? &Result.Report.Rungs[0] : nullptr;
	}

	/** Findings of the requested rung with this Id (any result). */
	inline int32 CountFindings(const FTuningRungReport* Rung, const ANSICHAR* Id, ETuningCheck Result)
	{
		if (Rung == nullptr)
		{
			return 0;
		}
		return Rung->Findings.FilterByPredicate([Id, Result](const FTuningInvariantFinding& F) { return F.Result == Result && FCStringAnsi::Strcmp(F.Id, Id) == 0; }).Num();
	}

	/** The F-1 changes of the requested rung for Key. */
	inline TArray<FTuningF1Change> ChangesOf(const FTuningRungReport* Rung, const ANSICHAR* Key)
	{
		TArray<FTuningF1Change> Out;
		if (Rung != nullptr)
		{
			for (const FTuningF1Change& Change : Rung->F1.Changes)
			{
				if (FCStringAnsi::Strcmp(Change.Key, Key) == 0)
				{
					Out.Add(Change);
				}
			}
		}
		return Out;
	}

	/** Warnings naming Id as a whole token. */
	inline int32 WarningsNaming(const FStealthLogCapture& Capture, const TCHAR* Id)
	{
		return Capture.GetWarnings().FilterByPredicate([Id](const FString& Line) { return FStealthLogCapture::ContainsToken(Line, Id); }).Num();
	}

	/**
	 * The Crowd key I-CNB-6 reads, lowered below the floored hold (2.0 s) so a case that floors ObjectiveInteractSec to 2.0
	 * is not rejected by Crowd's own row (CrowdDecoyEarlyReleaseMaxSec < ObjectiveInteractSec): inside its hard range 1-5.
	 */
	inline constexpr float CrowdEarlyReleaseMaxBelowFloorSec = 1.9f;

	/** The Objectives invariant ids. */
	inline const ANSICHAR* const ObjectivesIds[] = { "I-OC1", "I-OC2", "I-OC3" };
}

#endif
