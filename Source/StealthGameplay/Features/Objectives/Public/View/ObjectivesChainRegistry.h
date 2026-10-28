#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "ObjectivesChainRegistry.generated.h"

class UObjectiveChainData;

/** The cooked chains a round may select from (ADR-0020 section 2). Production: UObjectivesChainRegistry; tests: a stub. */
class IObjectivesChainSource
{
public:
	virtual ~IObjectivesChainSource() = default;

	/** Every cooked ObjectiveChain primary asset, loaded, in id order. */
	virtual void GetCookedChains(TArray<const UObjectiveChainData*>& OutChains) const = 0;
};

/**
 * Holds the cooked chain assets for the session (ADR-0020 section 1: loaded once per session; a few KB of text). Client-safe
 * (View/): clients render card text from the same loaded asset; only row indices travel. It resolves every primary asset
 * of type ObjectiveChain through the Asset Manager (DefaultGame.ini scan, AlwaysCook) and loads them synchronously on first
 * use. It chooses nothing: the server's Objectives service fails Preparing when the count is not exactly one.
 *
 * Usage:
 *   TArray<const UObjectiveChainData*> Chains;
 *   UObjectivesChainRegistry::Get(World)->GetCookedChains(Chains);
 */
UCLASS(NotBlueprintType, NotBlueprintable)
class STEALTHGAMEPLAY_API UObjectivesChainRegistry : public UGameInstanceSubsystem, public IObjectivesChainSource
{
	GENERATED_BODY()

public:
	/** The registry of World's game instance, or null. */
	static UObjectivesChainRegistry* Get(const UWorld* World);

	virtual void GetCookedChains(TArray<const UObjectiveChainData*>& OutChains) const override;

	virtual void Deinitialize() override;

private:
	void LoadOnce() const;

	UPROPERTY(Transient)
	mutable TArray<TObjectPtr<UObjectiveChainData>> Chains;

	mutable bool bLoaded = false;
};
