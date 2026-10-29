#pragma once

#include "CoreMinimal.h"
#include "View/StealthEncounterWire.h"
#include "View/StealthTuningPublicSubstructs.h"

/**
 * The client body-hidden latch (ADR-0017 section 5; GDD CON-R6b, AC-CON-08; story con-019): one state-derived, fail-closed
 * function evaluated on every client (and on the listen host with Now == ServerNow) from the replicated FActionState and
 * FDeathState, the ally's C-TeamView concealed flag and the previous latch value. No UObject dependency: headless.
 *
 *   Owner                                   never hidden (the owner keeps its own body)
 *   Set while                               BushEnter and Now >= Start + BushEnterSwapSec (unless the pawn died before
 *                                           that swap: an ordinary visible kill); bAllyIsConcealed; BushExit and
 *                                           Now < Start + BushExitCrossSec; a bush-emerge strike (Strike with the
 *                                           BushPreface variant) and Now < Start + BushStrikeEmergeSec; bDiedConcealed
 *   Clear only when                         no set rule holds AND the last transition evidence is an exit or emerge
 *                                           crossing that has passed (BushExit / bush-emerge strike past its crossing),
 *                                           or there is no concealment evidence at all on a fresh evaluation (pose None,
 *                                           ally flag false). A pose change to None, a death pose or a plain strike pose
 *                                           never clears a set latch; a death after the swap never clears it.
 *
 * Boundaries are exact: at Now = Start + swap the body is hidden; at Now = Start + crossing it is shown.
 *
 * Usage (client presentation, each refresh):
 *   FBodyHiddenInputs In{ Pawn->GetActionState(), Pawn->GetDeathState(), bAllyConcealed, bIsOwner, EstimatedServerNow };
 *   In.bWasHidden = bLatched;
 *   bLatched = EvaluateBodyHidden(In, Tuning);
 */
struct FBodyHiddenInputs
{
	FActionState Action;
	FDeathState Death;
	/** Ally clients: the teammate entry's bIsConcealed (C-TeamView); false on opponents and for NPCs. */
	bool bAllyIsConcealed = false;
	/** This machine's own pawn. */
	bool bIsOwner = false;
	/** EstimatedServerNow() (ServerNow on the listen host). */
	double Now = 0.0;
	/** The latch's previous value (a fresh actor starts hidden: true). */
	bool bWasHidden = true;
	/**
	 * The first evaluation of a fresh actor (PostNetInit, ADR-0017 section 5): with no concealment evidence at all (pose
	 * None or a non-hide pose, ally flag false, no bDiedConcealed) it clears; otherwise the set rules decide.
	 */
	bool bFresh = false;
};

/** The latch for In (pure). */
STEALTHGAMEPLAY_API bool EvaluateBodyHidden(const FBodyHiddenInputs& In, const FConcealmentTuning_Public& Tuning);

/** True when Action is a bush-emerge strike (Strike with the BushPreface variant bit). */
STEALTHGAMEPLAY_API bool IsBushEmergeStrike(const FActionState& Action);
