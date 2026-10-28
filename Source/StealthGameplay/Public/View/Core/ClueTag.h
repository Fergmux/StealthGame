#pragma once

#include "CoreMinimal.h"

#include "ClueTag.generated.h"

/**
 * A court's role-blind clue (ADR-0020 section 4 and Key Interfaces; GDD objectives-chain.md OC-5 clue, OQ3): the area type and
 * the nearest distinctive public feature of a court, taken from the Level Socket Contract's per-court public ClueTag record.
 * Client-safe (View/Core/, beside the other layout id types): only these two small enums travel in the team view; the phrase
 * ("A court in the market, by the water steps") is built client-side from a localised phrase table, so no string travels.
 *
 * Declared by Objectives story oc-010 because FObjectiveLiveSite carries it; story oc-013 owns its content (the clue function,
 * the key set of the phrase table and the ClueTooBroad check). Until oc-013 lands every site carries the default {0, 0}
 * (no area, no feature): the HUD shows no clue line. Value 0 of either field means None.
 */
USTRUCT()
struct STEALTHGAMEPLAY_API FClueTag
{
	GENERATED_BODY()

	/** The court's LSC AreaTypeId (0 = None). */
	UPROPERTY()
	uint8 AreaType = 0;

	/** The nearest distinctive public feature (0 = None: the clue is the area type only, the GDD's fallback). */
	UPROPERTY()
	uint8 FeatureKind = 0;

	bool operator==(const FClueTag& Other) const { return AreaType == Other.AreaType && FeatureKind == Other.FeatureKind; }
	bool operator!=(const FClueTag& Other) const { return !(*this == Other); }
};
