#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "ObjectiveChainData.generated.h"

/**
 * The Objectives Chain data (ADR-0020 sections 1-2; GDD objectives-chain.md OC-2, OC-3, OC-16; story oc-002). Client-safe
 * (View/): clients render the card title and line from their own cooked copy of the asset; only row indices and enums travel
 * (the team view, story oc-010). Nothing here is replicated and nothing here names a court: a definition binds to whichever
 * court the server-secret assignment gives its SiteRole at OnLayoutReady (story oc-003).
 */

/** Position of a definition in the chain (OC-2 StageKind). */
UENUM()
enum class EObjectiveStage : uint8
{
	Prelim1,
	Prelim2,
	Final,
};

/** Which team's completion of a definition counts (OC-2 TeamEligibility): a prelim of one team never advances the other. */
UENUM()
enum class EObjectiveEligibility : uint8
{
	Team0,
	Team1,
	Both,
};

/** Which FSiteAssignment slot a definition binds to (OC-2 SiteRole; matches StageKind). */
UENUM()
enum class EObjectiveSiteRole : uint8
{
	P1,
	P2,
	Final,
};

/** A team's place in the chain (OC-4 StageIndex; the team view's StageIndex, story oc-010). None before Active entry. */
UENUM()
enum class EObjectiveTeamStage : uint8
{
	None,
	Stage1,
	Stage2,
	Final,
};

/** How the court's public ClueTag becomes the card's clue line (OC-2 ClueForm; story oc-013 renders it). */
UENUM()
enum class EClueForm : uint8
{
	AreaAndFeature,
};

/**
 * One reusable objective definition (OC-2). It deliberately has NO hold-time field: the hold is the chain-wide
 * ObjectiveInteractSec for every court (OC-14; InteractSecOverride removed by the user 2026-10-25, because a per-row hold
 * would make D depend on the court's role, a timing oracle: IVP R7.4a, AC-OC-08). StealthGame.Objectives.ChainDefShape pins
 * the field set.
 */
USTRUCT()
struct STEALTHGAMEPLAY_API FObjectiveDef
{
	GENERATED_BODY()

	/** Stable id, unique per chain; logged server-side (the wire uses the row index, never this name). */
	UPROPERTY(EditDefaultsOnly, Category = "Objective")
	FName ObjectiveId;

	/** Position in the chain. */
	UPROPERTY(EditDefaultsOnly, Category = "Objective")
	EObjectiveStage StageKind = EObjectiveStage::Prelim1;

	/** Which team's completion counts (the final is Both). */
	UPROPERTY(EditDefaultsOnly, Category = "Objective")
	EObjectiveEligibility TeamEligibility = EObjectiveEligibility::Team0;

	/** The assignment slot that binds this row to a court (must match StageKind). */
	UPROPERTY(EditDefaultsOnly, Category = "Objective")
	EObjectiveSiteRole SiteRole = EObjectiveSiteRole::P1;

	/** Completed tends needed at the site (1 in MVP; above 1 fails Preparing). */
	UPROPERTY(EditDefaultsOnly, Category = "Objective", meta = (ClampMin = "1"))
	int32 CompletionsRequiredCount = 1;

	/** Team-only stage card title (localisable; rendered client-side, never replicated). */
	UPROPERTY(EditDefaultsOnly, Category = "Objective")
	FText CardTitle;

	/** Team-only stage card line (localisable; rendered client-side, never replicated). */
	UPROPERTY(EditDefaultsOnly, Category = "Objective")
	FText CardLine;

	/** How the clue line is built from the court's public ClueTag. */
	UPROPERTY(EditDefaultsOnly, Category = "Objective")
	EClueForm ClueForm = EClueForm::AreaAndFeature;
};

/**
 * The cooked chain asset (ADR-0020 section 1): primary asset type ObjectiveChain, cook rule AlwaysCook (Config/DefaultGame.ini
 * Asset Manager scan). GetPrimaryAssetId() is overridden so the type is ObjectiveChain rather than the native class name, and
 * the CDO reports an invalid id (ADR-0020 E5). MVP ships one chain, DA_ObjectiveChain_Cacao (OC-16).
 *
 * Usage (server, story oc-005): the chain registry resolves FPrimaryAssetId(PrimaryAssetType, AssetName) and the core
 * validates it (FObjectivesCore::ValidateChain) before binding.
 */
UCLASS(BlueprintType, Const)
class STEALTHGAMEPLAY_API UObjectiveChainData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** The primary asset type name; equals the DefaultGame.ini scan entry exactly (case-sensitive). */
	static const FPrimaryAssetType PrimaryAssetType;

	/** At most this many rows (the width of the team view's CompletedDefMask, story oc-010). */
	static constexpr int32 MaxDefinitions = 16;

	/** Authored, stable and unique per chain: tells a client which cooked chain the wire indices refer to. */
	UPROPERTY(EditDefaultsOnly, Category = "Chain")
	uint16 ChainSerial = 0;

	/** The definitions; the row index is the wire identity. */
	UPROPERTY(EditDefaultsOnly, Category = "Chain", meta = (TitleProperty = "ObjectiveId"))
	TArray<FObjectiveDef> Definitions;

	/** ("ObjectiveChain", GetFName()) for an asset; an invalid id for the class default object. */
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
};
