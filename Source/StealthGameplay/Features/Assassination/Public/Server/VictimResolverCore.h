#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Server/EncounterTypes.h"
#include "View/Random/StealthRandomTag.h"

class IBushSpotQuery;
class IServerSubSeedSource;
class FStrikeRoleTable;

/**
 * FVictimResolverCore: ASN-R8 victim resolution on server capsules (GDD ASN-R8, R9, F1, F4; ADR-0018 section 2.1 step 1;
 * stories asn-005 rules 1, 3, 4 and the candidate filter, asn-006 rule 2). Plain C++, pure over its injected seams: it
 * reads facts and changes nothing, so the same call is the resolver's step 1 and a test's direct probe.
 *
 * Candidates exclude the attacker, every pawn GetRelation(Attacker, Pawn) = Teammate, every NotTargetable pawn and every
 * pawn whose strike role is Dead (including one whose death write is still pending). Then the first match wins:
 *   1. Honoured PawnRef: CanViewerKnow(Attacker, Ref) and its capsule axis within PawnRefAgreementCm (horizontal) of the
 *      WorldPoint. A Dead (or Roster AlreadyResolved) honoured ref is a terminal whiff; an honoured candidate is the victim
 *      if it passes F1, else a terminal whiff; an honoured ally, the attacker itself or a stale ref falls through to the
 *      spot rules with allies excluded (ASN-R9). An unhonoured (culled, forged, far) ref always falls through, so it is
 *      identical to a TargetSpot at the same point (IIF R10, IVP R7.4).
 *   2. Bush spot (terminal): the WorldPoint lies in a spot zone; the zone's occupants, filtered by the candidate rule and
 *      the full F1 test, are sorted by spawn serial and one is picked by UniformBelow(|Reachable|) from the secret
 *      BushSpotPick stream, FSecretStream(GetServerSubSeed(BushSpotPick, LayoutAttempt), "CON_SPOT") in counter mode keyed
 *      (ServerTickIndex, AttackerCommandSeq = the item's queue Seq, BushId, draw index). Exactly one UniformBelow draw per
 *      resolving bush strike, also when none or one is reachable, so the draw count never reveals the occupancy. Never
 *      RoundSeed, never a modulo, never an engine RNG. No survivor: a whiff, never rule 3.
 *   3. Open spot: non-concealed candidates within r_c + OpenSpotToleranceCm (horizontal, inclusive) of the WorldPoint that
 *      pass F1; the nearest wins, an exact tie goes to the lower spawn serial (never a kind or team).
 *   4. Otherwise a whiff.
 * No term reads a controller, a binding or a kind. O(pawns) capsule tests.
 */

/** The seams one resolution reads (Pawns and Roster required; the others optional). */
struct FVictimResolverDeps
{
	const IEncounterPawnQuery* Pawns = nullptr;
	IEncounterRosterGateway* Roster = nullptr;
	/** Null: default-deny (no PawnRef is ever honoured; ADR-0003). */
	IEncounterVisibilityQuery* Visibility = nullptr;
	/** Null: no spot zones, nobody concealed. */
	const IBushSpotQuery* Bushes = nullptr;
	/** Null: no pawn is Dead. */
	const FStrikeRoleTable* Roles = nullptr;
	/** Required for rule 2 (null: a bush strike is a whiff; a wiring defect). */
	const IServerSubSeedSource* SubSeeds = nullptr;
};

/** One Attack as the resolver sees it at the slot. */
struct FVictimRequest
{
	FEncounterPawnFacts Attacker;
	FVector WorldPoint = FVector::ZeroVector;
	/** The optional PawnRef (never evidence of visibility on its own). */
	APawn* PawnRef = nullptr;
	/** The Attack's queue key (the bush pick key and nothing else). */
	FQueueKey Key;
	/** The attacker's strike variant (BushStrike from a bush: F1's T_v adds BushStrikeEmergeSec). */
	EStrikeReachVariant Variant = EStrikeReachVariant::Standard;
};

/** The result of one resolution. Equal for equal inputs (a culled ref and a TargetSpot compare equal). */
struct FVictimResolution
{
	APawn* Victim = nullptr;
	EVictimRule Rule = EVictimRule::None;
	/** Rule 2: the bush whose zone held the WorldPoint (also on a bush whiff). */
	uint16 BushId = 0;
	/** Rule 2 applied (the WorldPoint lies in BushId's spot zone), hit or whiff. */
	bool bInBushZone = false;
	/** F1 for the victim. */
	FStrikeReachResult Reach;
	/**
	 * Rule 1 ended the resolution (an honoured reference: a hit, a reach-failed whiff, or a body), so the strike's warp
	 * target is the reference's F1 extrapolation RefAtContact (ADR-0018 section 2.2, story asn-012). Knowable by definition.
	 */
	bool bRefHonoured = false;
	FVector RefAtContact = FVector::ZeroVector;
	/** Rule 2 observation (tests): the survivors of the reach filter and the UniformBelow draws consumed. */
	int32 ReachableCount = 0;
	int32 BushDraws = 0;

	bool operator==(const FVictimResolution& Other) const
	{
		return Victim == Other.Victim && Rule == Other.Rule && BushId == Other.BushId && Reach.bInReach == Other.Reach.bInReach
			&& Reach.DistanceCm == Other.Reach.DistanceCm && Reach.WarpTranslationEffCm == Other.Reach.WarpTranslationEffCm && Reach.BearingDeg == Other.Reach.BearingDeg;
	}
	bool operator!=(const FVictimResolution& Other) const { return !(*this == Other); }
};

class STEALTHGAMEPLAY_API FVictimResolverCore
{
public:
	/** ADR-0006 section 5 stream tag of the BushSpotPick draw (Concealment F4, domain 14). */
	static constexpr uint64 BushSpotPickTag = StealthRandom::Tag("CON_SPOT");

	/** Resolves the victim of Request (see the file comment). Changes nothing. */
	static FVictimResolution Resolve(const FEncounterParams& Params, const FVictimResolverDeps& Deps, const FVictimRequest& Request);

	/** The candidate filter alone (not the attacker, not a teammate, targetable, not Dead). */
	static bool IsCandidate(const FVictimResolverDeps& Deps, const APawn* Attacker, const APawn* Pawn);

	/** F1 for one attacker-victim pair at the slot. */
	static FStrikeReachResult Reach(const FEncounterParams& Params, EStrikeReachVariant Variant, const FEncounterPawnFacts& Attacker, const FEncounterPawnFacts& Victim);
};
