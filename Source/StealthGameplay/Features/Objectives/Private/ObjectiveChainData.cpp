// Story oc-002 (production/epics/objectives-chain/story-002-chain-data-types-and-cacao-asset.md): the chain asset's primary
// asset id (ADR-0020 section 1, E5).

#include "View/ObjectiveChainData.h"

const FPrimaryAssetType UObjectiveChainData::PrimaryAssetType(TEXT("ObjectiveChain"));

FPrimaryAssetId UObjectiveChainData::GetPrimaryAssetId() const
{
	if (HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
	{
		return FPrimaryAssetId();
	}
	return FPrimaryAssetId(PrimaryAssetType, GetFName());
}
