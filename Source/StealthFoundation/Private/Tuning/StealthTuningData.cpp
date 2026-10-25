// Stories tdc-039 / tdc-040: the preset and catalog data-asset classes (ADR-0004 sections 1 and 6).

#include "Server/StealthTuningData.h"
#include "Server/StealthTuningCatalog.h"

const FPrimaryAssetType UStealthTuningData::PrimaryAssetType(TEXT("StealthTuningPreset"));
const FPrimaryAssetType UStealthTuningCatalog::PrimaryAssetType(TEXT("StealthTuningCatalog"));
const FName UStealthTuningCatalog::DefaultPresetId(TEXT("Default"));

FPrimaryAssetId UStealthTuningData::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(PrimaryAssetType, GetFName());
}

FPrimaryAssetId UStealthTuningCatalog::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(PrimaryAssetType, GetFName());
}

bool UStealthTuningCatalog::CheckEntries(TConstArrayView<FStealthTuningCatalogEntry> InEntries, TArray<FString>& OutProblems)
{
	const int32 ProblemsBefore = OutProblems.Num();
	bool bHasDefault = false;
	for (const FStealthTuningCatalogEntry& Entry : InEntries)
	{
		// FName comparison is case-insensitive, matching the case-insensitive preset lookup.
		const bool bIsDefault = Entry.PresetId == DefaultPresetId;
		bHasDefault |= bIsDefault;
		if (Entry.bTestAllowsStrikeLimitOne && bIsDefault)
		{
			OutProblems.Add(FString::Printf(TEXT("catalog entry '%s': bTestAllowsStrikeLimitOne is not allowed on Default"), *Entry.PresetId.ToString()));
		}
		if (Entry.bTestAllowsStrikeLimitOne && Entry.bHostSelectable)
		{
			OutProblems.Add(FString::Printf(TEXT("catalog entry '%s': bTestAllowsStrikeLimitOne is not allowed on a HostSelectable preset"), *Entry.PresetId.ToString()));
		}
	}
	if (!bHasDefault)
	{
		OutProblems.Add(FString::Printf(TEXT("catalog has no '%s' entry (mandatory, CR-2)"), *DefaultPresetId.ToString()));
	}
	return OutProblems.Num() == ProblemsBefore;
}
