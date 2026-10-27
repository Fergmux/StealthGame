#pragma once

#include "CoreMinimal.h"
#include "Server/StealthPenaltySeams.h"
#include "UObject/WeakObjectPtrTemplates.h"

class AStealthGameMode;

/**
 * The composition root's IPenaltyPawnGateway (NPC-Kill Penalty story nkp-005; consumer-owned seam of UPenaltyService):
 * the pawn-level half of the exile that other epics own.
 *
 *   ReleaseCard          STAND-IN: the Character Pool & Appearance epic (UAppearanceAllocator::ReleaseCard, BoundLive ->
 *                        NpcLive with the same AppearanceId) is not built; the prototype pawn keeps its look and no card
 *                        ledger exists, so nothing changes (the "same AppearanceId" property holds trivially).
 *   RequestHandOverToAi  UStealthPawnFactory::RequestHandOver(Pawn, Ai, lockout end): the commit (epoch bump, the player's
 *                        controller released with its view kept) now, the crowd controller's possession after the
 *                        strike lockout; then the prototype follow-ons (AStealthGameMode::HandleProductionExile: life
 *                        state mirror and the STAND-IN informant respawn until stories nkp-009..015).
 *
 * Replaces the shadow tally adapter of stories nkp-001..004: UPenaltyService is now the encounter service's
 * IStrikeTallySource itself. Story nkp-021 retires the prototype follow-ons.
 */
class FStealthPenaltyPawnGateway final : public IPenaltyPawnGateway
{
public:
	explicit FStealthPenaltyPawnGateway(AStealthGameMode* InGameMode);

	virtual void ReleaseCard(APawn* Pawn) override;
	virtual void RequestHandOverToAi(APawn* Pawn, uint32 ExpectedRoundEpoch, double PossessNotBeforeServerTime) override;

private:
	TWeakObjectPtr<AStealthGameMode> GameMode;
};
