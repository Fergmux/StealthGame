#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Server/StealthServerTuning.h"
#include "Server/StealthTuningPipelineCore.h"

/**
 * The frozen, validated tuning set (GDD CR-7; ADR-0004 section 5 and Key Interfaces; created by story
 * tdc-050, immutability proven by story tdc-054). Every server system reads this (through
 * GetServerTuning() / GetValidatedTuning(), later stories), never the raw preset asset.
 *
 * Immutable by construction: every member is const and reached only through a const accessor, so no
 * mutator exists and assignment is ill-formed (copy and move assignment are deleted). It is held as
 * TSharedRef<const FValidatedTuning> (FTuningResolveResult::Validated is the nullable form). The one
 * sanctioned way to produce a different set is a new object (the next round's Resolve, or the PIE
 * hot-reload path with Revision + 1, story tdc-085). A deliberate const_cast is outside what the type
 * system can prevent and is a code-review matter.
 *
 * Plain C++ (not a USTRUCT or UObject).
 */
class FValidatedTuning
{
public:
	/** Freezes a validated value set. Only the pipeline core (Resolve) and the hot-reload path construct one. */
	FValidatedTuning(const FStealthServerTuning& InValues, uint64 InTuningHash, uint64 InClientHash, int32 InSchemaVersion, FName InPresetId,
		int32 InRevision, ETuningFallback InFallbackLevel, const FValidationReport& InReport)
		: ValuesData(InValues)
		, TuningHashData(InTuningHash)
		, ClientHashData(InClientHash)
		, SchemaVersionData(InSchemaVersion)
		, PresetIdData(InPresetId)
		, RevisionData(InRevision)
		, FallbackLevelData(InFallbackLevel)
		, ReportData(InReport)
	{
	}

	FValidatedTuning(const FValidatedTuning&) = default;
	FValidatedTuning& operator=(const FValidatedTuning&) = delete;
	FValidatedTuning& operator=(FValidatedTuning&&) = delete;

	/** Every key's validated (post F-1) value. */
	const FStealthServerTuning& Values() const { return ValuesData; }
	/** F-2 hash over every key (never replicated). */
	uint64 TuningHash() const { return TuningHashData; }
	/** F-2 hash over the public keys (what clients recompute). */
	uint64 ClientHash() const { return ClientHashData; }
	/** TuningSchemaVersion the hashes were computed with. */
	int32 SchemaVersion() const { return SchemaVersionData; }
	/** The active preset (SchemaDefaults for the schema-defaults rung). */
	FName PresetId() const { return PresetIdData; }
	/** 0 at Preparing; + 1 per accepted PIE hot-reload. */
	int32 Revision() const { return RevisionData; }
	/** 0 requested, 1 Default, 2 schema defaults. */
	ETuningFallback FallbackLevel() const { return FallbackLevelData; }
	/** The full validation report of the resolve that produced this set. */
	const FValidationReport& Report() const { return ReportData; }

private:
	const FStealthServerTuning ValuesData;
	const uint64 TuningHashData;
	const uint64 ClientHashData;
	const int32 SchemaVersionData;
	const FName PresetIdData;
	const int32 RevisionData;
	const ETuningFallback FallbackLevelData;
	const FValidationReport ReportData;
};
