#pragma once

#include "CoreMinimal.h"
#include "Server/Core/Round/StealthRoundGateways.h"
#include "Server/StealthPenaltySeams.h"
#include "UObject/WeakObjectPtrTemplates.h"

class AStealthGameMode;
class APawn;
class UPenaltyService;
class UStealthCompositionRoot;
class UWorld;

/**
 * The composition root's IPenaltyPawnGateway (NPC-Kill Penalty story nkp-005; consumer-owned seam of UPenaltyService):
 * the pawn-level half of the exile that other epics own.
 *
 *   ReleaseCard          STAND-IN: the Character Pool & Appearance epic (UAppearanceAllocator::ReleaseCard, BoundLive ->
 *                        NpcLive with the same AppearanceId) is not built; the prototype pawn keeps its look and no card
 *                        ledger exists, so nothing changes (the "same AppearanceId" property holds trivially).
 *   RequestHandOverToAi  UStealthPawnFactory::RequestHandOver(Pawn, Ai, lockout end): the commit (epoch bump, the player's
 *                        controller released with its view kept) now, the crowd controller's possession after the
 *                        strike lockout; then the prototype mirrors (AStealthGameMode::HandleProductionExile: life state,
 *                        the objective interaction dropped). The respawn is Penalty's own (stories nkp-009, nkp-012).
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

/**
 * Round's IInformantBodyProvider (ADR-0019 section 1; story nkp-009), registered once in FRoundGateways::InformantBodies:
 * a thin forwarder to the current server world's UPenaltyService, held weakly (the match core can outlive a world; a
 * world without a bound service answers nothing).
 */
class FStealthInformantBodiesProvider final : public IInformantBodyProvider
{
public:
	/** The service of the current server world (null: every call is dropped). */
	void SetService(UPenaltyService* InService);

	virtual void OnInformantAwaitingBody(FParticipantId Participant, ERespawnCause Cause, double LostServerTime) override;
	virtual void OnInformantReadmitted(FParticipantId Participant) override;
	virtual void OnInformantPawnLost(FParticipantId Participant) override;

private:
	TWeakObjectPtr<UPenaltyService> Service;
};

/**
 * The Roster owner-view feed as Penalty's T_due carrier adapter uses it (story nkp-011; ADR-0019 section 6 option (a)):
 *
 *   SetOwnerRespawnDue  URosterStore::SetOwnerRespawnDue: the C-TeamView owner part's T_due (owner audience only), the
 *                       respawn-due half of roster story psr-038 built early by nkp-011.
 *   SetOwnerTally       STAND-IN no-op: the tally half of psr-038 (SetOwnerTally) is not built; the tally still reaches
 *                       its owner through FStrikeResult.TallyAfter. This feed is therefore NOT passed to UPenaltyService as
 *                       its OwnerView (the re-push stays off until psr-038), only to the carrier adapter.
 */
class FStealthRosterOwnerViewFeed final : public IPenaltyOwnerViewFeed
{
public:
	explicit FStealthRosterOwnerViewFeed(UStealthCompositionRoot* InRoot);

	virtual void SetOwnerTally(FParticipantId Participant, uint8 WrongTargetTally, FClientCommandSeq LastResultSeq, uint32 ExpectedEpoch) override;
	virtual void SetOwnerRespawnDue(FParticipantId Participant, double DueServerTime, uint32 ExpectedEpoch) override;

private:
	TWeakObjectPtr<UStealthCompositionRoot> Root;
};

/**
 * The respawn commit's gateways (IPenaltyRespawnGateway; story nkp-012; ADR-0019 section 4.1), by pawn key, for the
 * current server world:
 *
 *   PickRespawnPawn          STAND-IN for Level Socket Contract story 033 (ULayoutService::PickRespawnPawn): the team's
 *                            respawn region is approximated by a straight-line radius of InformantRespawnPathCm around
 *                            the team's spawn anchor (the centroid of its tagged PlayerStarts; LSC's path-distance cells
 *                            and nearer-anchor cut are not built). Candidates: unbound, alive, Crowd-Eligible crowd pawns
 *                            not in Exclude, UNSEEN (no line of sight from any live opposing participant's view point;
 *                            LSC's ViewBlock traces stand-in), sorted by spawn ordinal, one uniform draw from the
 *                            request's RespawnPick sub-seed stream (it continues across the request's retries). Widening
 *                            stand-in: an empty region widens at once to 2 x the radius (level 1), then to any cell not
 *                            within the radius of the enemy anchor (level 2); LSC's time-based widening (RespawnStarvationSec
 *                            after the request's first pick) is LSC 033's, Penalty records whatever level is reported (story
 *                            nkp-013). One pick per server frame (Deferred after the first). No eligible unseen pawn: None,
 *                            and the participant keeps waiting (the unseen test is never relaxed and no pawn is ever spawned,
 *                            story nkp-014). The one exception is HARNESS-ONLY: in a test-seam build whose PIE harness round
 *                            switched StealthPenaltyHarnessSeams' crowd-less respawn on, a world with NO crowd agent at all
 *                            (the 0-NPC harness rounds) gets a reserved key whose takeover spawns the participant's pawn at
 *                            its team start (AStealthGameMode::SpawnHarnessCrowdlessRespawnPawn). Off by default, compiled
 *                            out of Shipping; story nkp-014's OneRespawnPath shows a production round never spawns.
 *   CheckTakeOverEligibility STAND-IN for Shared Character story 028's predicate: alive (no death write), not being
 *                            destroyed, unbound, crowd-driven (no PlayerController) and Crowd-Eligible.
 *   TryReleaseNpc            UCrowdDirector::TryReleaseNpc (Crowd story cnb-030).
 *   TakeOverCrowdPawn        STAND-IN for Shared Character story 028: the same predicate, Roster BindPawn(P, Pawn,
 *                            ExpectedEpoch) (the "factory binds on Penalty's behalf"), ICrowdControl::ReleaseControl and
 *                            UStealthPawnFactory::RequestHandOverToPlayer (the possession at the next frame start, itself
 *                            a STAND-IN for the SCL hand-over pass). No pawn is spawned.
 *   AdoptReleasedPawn        UCrowdDirector::AdoptReleasedPawn(Pawn, {}) (the exact reverse of TryReleaseNpc, cnb-030).
 *   TakeNpcCard              STAND-IN for Character Pool (UAppearanceAllocator::TakeNpcCard): no card ledger exists and
 *                            the pawn keeps its look; answers AppearanceId 0 (none), so no LedgerMismatch is reported.
 *   RecordAppearance         STAND-IN no-op (the Roster writer of AppearanceHistory is unnamed, psr-022; Round's own
 *                            RecordAppearance gateway is a no-op too).
 */
class FStealthPenaltyRespawnGateway final : public IPenaltyRespawnGateway
{
public:
	FStealthPenaltyRespawnGateway(UWorld* InWorld, UStealthCompositionRoot* InRoot);

	virtual FPenaltyRespawnPick PickRespawnPawn(ERosterTeam Team, uint32 RequestSeq, TConstArrayView<FPenaltyPawnKey> Exclude) override;
	virtual bool CheckTakeOverEligibility(FPenaltyPawnKey Pawn) const override;
	virtual bool TryReleaseNpc(FPenaltyPawnKey Pawn, uint32 ExpectedRoundEpoch) override;
	virtual EPenaltyTakeoverResult TakeOverCrowdPawn(FPenaltyPawnKey Pawn, FParticipantId Participant, uint32 ExpectedRoundEpoch) override;
	virtual void AdoptReleasedPawn(FPenaltyPawnKey Pawn) override;
	virtual TOptional<uint16> TakeNpcCard(FParticipantId Participant, FPenaltyPawnKey Pawn) override;
	virtual void RecordAppearance(FParticipantId Participant, uint16 AppearanceId) override;

private:
	/** The stand-in pick itself (PickRespawnPawn adds story nkp-014's probe record around it). */
	FPenaltyRespawnPick Pick(ERosterTeam Team, uint32 RequestSeq, TConstArrayView<FPenaltyPawnKey> Exclude);
	/** Crowd's TryReleaseNpc (TryReleaseNpc adds the probe record and the injected failure around it). */
	bool Release(FPenaltyPawnKey Pawn, uint32 ExpectedRoundEpoch);
	/** The stand-in takeover itself (TakeOverCrowdPawn adds the probe record and the commit-tick sight query around it). */
	EPenaltyTakeoverResult TakeOver(FPenaltyPawnKey Pawn, FParticipantId Participant, uint32 ExpectedRoundEpoch);
	APawn* Resolve(FPenaltyPawnKey Key) const;
	/** The stand-in takeover predicate (alive, unbound, crowd-driven, Crowd-Eligible). */
	bool IsTakeoverEligible(APawn* Pawn) const;
	/** The team's spawn anchor (the centroid of its tagged PlayerStarts); false when the team has none. */
	bool FindTeamAnchor(ERosterTeam Team, FVector& OutAnchor) const;
	/** True when no live opposing participant pawn has a line of sight to Candidate. */
	bool IsUnseenByOpponents(ERosterTeam Team, const APawn* Candidate) const;

	TWeakObjectPtr<UWorld> World;
	TWeakObjectPtr<UStealthCompositionRoot> Root;
	/** The pawns this gateway named by key (every candidate of the last picks; the commit resolves them in its stack). */
	TMap<FPenaltyPawnKey, TWeakObjectPtr<APawn>> Known;
	/** Blocks already drawn per request (its RespawnPick stream continues across retries). */
	TMap<uint32, uint32> DrawsByRequest;
	/** The server frame of the last pick (the one-pick-per-frame guard). */
	uint64 LastPickFrame = MAX_uint64;
};
