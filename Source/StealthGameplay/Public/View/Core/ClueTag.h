#pragma once

#include "CoreMinimal.h"

#include "ClueTag.generated.h"

/**
 * A court's role-blind clue (ADR-0020 section 4 and Key Interfaces; GDD objectives-chain.md OC-5 clue, OQ3): the area type and
 * the nearest distinctive public feature of a court, taken from the Level Socket Contract's per-court public ClueTag record.
 * Client-safe (View/Core/, beside the other layout id types): only these two small enums travel in the team view; the phrase
 * ("A court in the market, by the water steps") is built client-side from a localised phrase table, so no string travels.
 *
 * Declared by Objectives story oc-010 because FObjectiveLiveSite carries it; story oc-013 owns its content: the key set below
 * (EClueAreaType x EClueFeatureKind, the phrase table's keys), the role-blind clue function and the ClueTooBroad check
 * (Features/Objectives/Private/ObjectivesClue.h). Value 0 of either field means None; FeatureKind None is the area-only
 * fallback. The phrase is built by ObjectiveViewHelpers::BuildClueText (story oc-016).
 */

/** The LSC area types (GDD level-socket-contract.md: Causeway, Market, Temple, Residential; art 6.2). Append only. */
enum class EClueAreaType : uint8
{
	None = 0,
	Causeway = 1,
	Market = 2,
	Temple = 3,
	Residential = 4,
};

/** The nearest distinctive public feature kinds of a court (LSC R3 court row). Append only. */
enum class EClueFeatureKind : uint8
{
	None = 0,
	WaterSteps = 1,
	Shrine = 2,
	Stairway = 3,
	Well = 4,
	Stalls = 5,
	GreatTree = 6,
	Archway = 7,
};

/** The highest key of each enum (the phrase table's key set; a value above it is unknown: the safe fallback). */
inline constexpr uint8 GClueAreaTypeMax = static_cast<uint8>(EClueAreaType::Residential);
inline constexpr uint8 GClueFeatureKindMax = static_cast<uint8>(EClueFeatureKind::Archway);

/** The clue that travels: {AreaType, FeatureKind} (2 bytes; no string member). */
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
