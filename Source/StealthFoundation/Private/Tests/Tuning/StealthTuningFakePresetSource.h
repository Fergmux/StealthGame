#pragma once

// Test-local ITuningPresetSource for the Resolve() tests (stories tdc-049, tdc-050, tdc-051, tdc-053,
// tdc-054, tdc-055). A preset is either registered with values (and its public flag and F-1 options) or
// registered as corrupted; an unregistered preset is missing. Both missing and corrupted presets make
// Load return no value, the "corrupted" definition of ITuningPresetSource (StealthTuningPipelineCore.h).
// Records every Load call so a test can assert which rungs were tried.

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Server/StealthServerTuning.h"
#include "Server/StealthTuningPipelineCore.h"

class FStealthTuningFakePresetSource : public ITuningPresetSource
{
public:
	/** Registers a loadable preset. */
	void Add(FName PresetId, const FStealthServerTuning& Values, bool bIsPublic, bool bTestAllowsStrikeLimitOne = false)
	{
		FEntry& Entry = Entries.Add(PresetId);
		Entry.Values = Values;
		Entry.bIsPublic = bIsPublic;
		Entry.bCorrupted = false;
		Entry.Options.bTestAllowsStrikeLimitOne = bTestAllowsStrikeLimitOne;
	}

	/** Registers a preset whose asset cannot be loaded (corrupted: Load returns no value). */
	void AddCorrupted(FName PresetId, bool bIsPublic)
	{
		FEntry& Entry = Entries.Add(PresetId);
		Entry.bIsPublic = bIsPublic;
		Entry.bCorrupted = true;
	}

	virtual TOptional<FStealthServerTuning> Load(FName PresetId, bool& bOutIsPublic) const override
	{
		LoadCalls.Add(PresetId);
		const FEntry* Entry = Entries.Find(PresetId);
		bOutIsPublic = Entry != nullptr && Entry->bIsPublic;
		if (Entry == nullptr || Entry->bCorrupted)
		{
			return TOptional<FStealthServerTuning>();
		}
		return Entry->Values;
	}

	virtual FTuningF1Options GetF1Options(FName PresetId) const override
	{
		const FEntry* Entry = Entries.Find(PresetId);
		return Entry != nullptr ? Entry->Options : FTuningF1Options();
	}

	/** Every Load call, in order. */
	const TArray<FName>& GetLoadCalls() const { return LoadCalls; }

private:
	struct FEntry
	{
		FStealthServerTuning Values;
		bool bIsPublic = false;
		bool bCorrupted = false;
		FTuningF1Options Options;
	};
	TMap<FName, FEntry> Entries;
	mutable TArray<FName> LoadCalls;
};

#endif // WITH_DEV_AUTOMATION_TESTS
