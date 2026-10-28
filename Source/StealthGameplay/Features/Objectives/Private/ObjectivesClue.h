#pragma once

#include "CoreMinimal.h"
#include "Server/ObjectivesSeams.h"
#include "View/Core/ClueTag.h"

/**
 * The role-blind clue of a court (Objectives Chain story oc-013; ADR-0020 sections 3-4; GDD OC-15, AC-OC-13, AC-OC-28).
 * Pure functions of PUBLIC court data: ClueOf reads one court's ClueTag record and nothing else (no assignment, role, team or
 * stage parameter exists), so a clue can never depend on a court's role. CountMatches is the number of the layout's courts
 * that share a clue; the core compares it with ClueMatchMaxCount once per layout and logs ClueTooBroad once per layout hash
 * (never a sharper clue). Computed once at OnLayoutReady: no per-frame cost.
 */
struct STEALTHGAMEPLAY_API FObjectivesClue
{
	/** The clue of Record: {AreaTypeId, NearestFeatureKind}; a value outside the key set becomes None (FeatureKind None: area only). */
	static FClueTag ClueOf(const FCourtClueRecord& Record);

	/** The record of Court in Courts, or null (a court the layout did not publish). */
	static const FCourtClueRecord* FindRecord(TConstArrayView<FCourtClueRecord> Courts, FCourtFixtureId Court);

	/** How many of Courts have Clue (by ClueOf). */
	static int32 CountMatches(TConstArrayView<FCourtClueRecord> Courts, const FClueTag& Clue);
};
