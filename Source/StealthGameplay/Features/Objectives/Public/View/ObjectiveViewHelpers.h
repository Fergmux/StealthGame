#pragma once

#include "CoreMinimal.h"
#include "View/Core/ClueTag.h"
#include "View/CourtFixtureId.h"
#include "View/ObjectiveTeamView.h"

class UObjectiveChainData;

/**
 * The client-side Objectives presentation rules (Objectives Chain story oc-016; ADR-0020 sections 4 and 9; GDD OC-6, OC-15).
 * Pure functions of the team view, the cooked chain asset and client tuning: no world, no server header (the include half is
 * StealthGame.Objectives.ObjectivesIncludeLint), the same inputs give the same outputs. The HUD calls them; the server sends
 * nothing extra for the ring, the card or the clue.
 */
namespace ObjectiveViewHelpers
{
	/** The stage card's two lines (title, line) from the cooked chain; empty for no card. */
	struct FStageCardRow
	{
		FText Title;
		FText Line;
		/** The row index the card came from (INDEX_NONE for no card). */
		int32 DefIndex = INDEX_NONE;

		bool IsValid() const { return DefIndex != INDEX_NONE; }
	};

	/** True when Court is one of the team's own live sites (its current stage; presentation only, never an ingress gate). */
	STEALTHGAMEPLAY_API bool IsOwnLiveSite(const FObjectiveTeamView& View, FCourtFixtureId Court);

	/**
	 * The card row of the team's first live site (LiveSites[0].DefIndex) from Chain, which must be the chain the view names
	 * (Chain->ChainSerial == View.ChainSerial). No card (invalid row) for an empty view, a missing or other chain, or a bad index.
	 */
	STEALTHGAMEPLAY_API FStageCardRow GetStageCardRow(const FObjectiveTeamView& View, const UObjectiveChainData* Chain);

	/**
	 * "A court [phrase]" from the localised phrase table keyed by area type and feature kind (story oc-013's key set):
	 * "A court in the market, by the water steps"; FeatureKind None is the area-only form ("A court in the market"). A key
	 * outside the table returns the safe fallback "A court" and logs a warning; Clue {0, 0} returns empty text (no clue line).
	 */
	STEALTHGAMEPLAY_API FText BuildClueText(const FClueTag& Clue);

	/**
	 * The tend-cue ring (OC-6, AC-OC-32): Marigold only when OwnHoldTarget is an own live site, the viewer is not an informant
	 * and EstimatedServerNow - HoldServerStartTime >= TendCueDelaySec (read once per hold at Begin); Bone otherwise. A hold
	 * that was cancelled has no start (pass a negative HoldServerStartTime): Bone.
	 */
	STEALTHGAMEPLAY_API bool RingIsMarigold(const FObjectiveTeamView& View, FCourtFixtureId OwnHoldTarget, double HoldServerStartTime, double EstimatedServerNow,
		double TendCueDelaySec, bool bIsInformant);
}
