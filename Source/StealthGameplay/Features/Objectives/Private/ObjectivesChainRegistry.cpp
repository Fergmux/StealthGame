// Story oc-005 (production/epics/objectives-chain/story-005-service-shell-and-composition-root.md): the session's cooked chain
// assets (ADR-0020 section 1). See View/ObjectivesChainRegistry.h.

#include "View/ObjectivesChainRegistry.h"

#include "Engine/AssetManager.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "View/ObjectiveChainData.h"

UObjectivesChainRegistry* UObjectivesChainRegistry::Get(const UWorld* World)
{
	const UGameInstance* GameInstance = World != nullptr ? World->GetGameInstance() : nullptr;
	return GameInstance != nullptr ? GameInstance->GetSubsystem<UObjectivesChainRegistry>() : nullptr;
}

void UObjectivesChainRegistry::LoadOnce() const
{
	if (bLoaded || !UAssetManager::IsInitialized())
	{
		return;
	}
	bLoaded = true;
	TArray<FPrimaryAssetId> Ids;
	UAssetManager::Get().GetPrimaryAssetIdList(UObjectiveChainData::PrimaryAssetType, Ids);
	Ids.Sort([](const FPrimaryAssetId& A, const FPrimaryAssetId& B) { return A.PrimaryAssetName.LexicalLess(B.PrimaryAssetName); });
	for (const FPrimaryAssetId& Id : Ids)
	{
		const FSoftObjectPath Path = UAssetManager::Get().GetPrimaryAssetPath(Id);
		if (UObjectiveChainData* Chain = Cast<UObjectiveChainData>(Path.TryLoad()))
		{
			Chains.Add(Chain);
		}
	}
}

void UObjectivesChainRegistry::GetCookedChains(TArray<const UObjectiveChainData*>& OutChains) const
{
	LoadOnce();
	OutChains.Reset();
	for (const TObjectPtr<UObjectiveChainData>& Chain : Chains)
	{
		if (Chain != nullptr)
		{
			OutChains.Add(Chain.Get());
		}
	}
}

void UObjectivesChainRegistry::Deinitialize()
{
	Chains.Reset();
	bLoaded = false;
	Super::Deinitialize();
}

#if !UE_BUILD_SHIPPING
#include "HAL/IConsoleManager.h"

namespace ObjectivesChainRegistryLocal
{
	/**
	 * Stealth.Objectives.ResolveChain (story oc-002, OBJ-V3; non-Shipping): resolves FPrimaryAssetId("ObjectiveChain",
	 * DA_ObjectiveChain_Cacao) through the Asset Manager in whatever build runs it (a cooked client included), loads it and
	 * prints one summary line: "OBJECTIVES CHAIN RESOLVE: PASS|FAIL id=... path=... rows=N serial=S".
	 */
	void ResolveChain()
	{
		const FPrimaryAssetId Id(UObjectiveChainData::PrimaryAssetType, FName(TEXT("DA_ObjectiveChain_Cacao")));
		if (!UAssetManager::IsInitialized())
		{
			UE_LOG(LogTemp, Error, TEXT("OBJECTIVES CHAIN RESOLVE: FAIL id=%s (no Asset Manager)"), *Id.ToString());
			return;
		}
		TArray<FPrimaryAssetId> Ids;
		UAssetManager::Get().GetPrimaryAssetIdList(UObjectiveChainData::PrimaryAssetType, Ids);
		const FSoftObjectPath Path = UAssetManager::Get().GetPrimaryAssetPath(Id);
		const UObjectiveChainData* Chain = Cast<UObjectiveChainData>(Path.TryLoad());
		UE_LOG(LogTemp, Display, TEXT("OBJECTIVES CHAIN RESOLVE: %s id=%s path=%s chains=%d rows=%d serial=%d cooked=%d"), Chain != nullptr ? TEXT("PASS") : TEXT("FAIL"),
			*Id.ToString(), *Path.ToString(), Ids.Num(), Chain != nullptr ? Chain->Definitions.Num() : -1, Chain != nullptr ? static_cast<int32>(Chain->ChainSerial) : -1,
			FPlatformProperties::RequiresCookedData() ? 1 : 0);
	}

	FAutoConsoleCommand ResolveCommand(
		TEXT("Stealth.Objectives.ResolveChain"),
		TEXT("Objectives story oc-002 (OBJ-V3): resolves and loads FPrimaryAssetId(ObjectiveChain, DA_ObjectiveChain_Cacao) and prints the result."),
		FConsoleCommandDelegate::CreateStatic(&ResolveChain));
}
#endif
