#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Server/StealthTuningData.h"
#include "UObject/SoftObjectPtr.h"

#include "StealthTuningCatalog.generated.h"

/** One catalog entry (ADR-0004 section 1; story tdc-039). */
USTRUCT()
struct STEALTHFOUNDATION_API FStealthTuningCatalogEntry
{
	GENERATED_BODY()

	/** Catalog name of the preset (`?Tuning=` matches it case-insensitively, later story). */
	UPROPERTY(EditAnywhere, Category = "Tuning")
	FName PresetId;

	/** The preset asset (loaded through the Asset Manager at session open, later story). */
	UPROPERTY(EditAnywhere, Category = "Tuning")
	TSoftObjectPtr<UStealthTuningData> Preset;

	/** A host may select this preset in Development and Test builds (never honoured in Shipping). */
	UPROPERTY(EditAnywhere, Category = "Tuning")
	bool bHostSelectable = false;

	/**
	 * Lowers NpcKillStrikeLimit's hard minimum from 2 to 1 for this preset in F-1 only (user decision
	 * 2026-10-25, Penalty OQ-NKP-11). Rejected by the catalog check on Default and on any
	 * HostSelectable entry. The field exists in every configuration (one serialised layout); F-1
	 * ignores it in Shipping, which is how "compiled out of Shipping" is met.
	 */
	UPROPERTY(EditAnywhere, Category = "Tuning")
	bool bTestAllowsStrikeLimitOne = false;
};

/**
 * The preset catalog (ADR-0004 section 1; story tdc-039): the list of presets with Default
 * mandatory (CR-2). Primary asset type StealthTuningCatalog (registered by story tdc-057).
 */
UCLASS(NotBlueprintType, NotBlueprintable)
class STEALTHFOUNDATION_API UStealthTuningCatalog : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** The Asset Manager primary asset type of the catalog. */
	static const FPrimaryAssetType PrimaryAssetType;

	/** The mandatory preset name (CR-2). */
	static const FName DefaultPresetId;

	/** Returns {StealthTuningCatalog, <object name>}. */
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	/**
	 * The catalog check (pure, no asset loading): Default must be present, and
	 * bTestAllowsStrikeLimitOne is rejected on Default and on any HostSelectable entry. Returns true
	 * when the entries pass; every problem is appended to OutProblems as one readable line.
	 */
	static bool CheckEntries(TConstArrayView<FStealthTuningCatalogEntry> InEntries, TArray<FString>& OutProblems);

	/** The presets of this catalog. */
	UPROPERTY(EditAnywhere, Category = "Tuning")
	TArray<FStealthTuningCatalogEntry> Entries;
};
