#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"

class APawn;

/**
 * IBushSpotQuery: the consumer-owned view of Concealment the strike resolver needs (ADR-0017 section 7, ADR-0018 section
 * 2.1; GDD ASN-R8 rule 2, CON-R9, CON-R12; story asn-006). Implemented by Concealment (epic not built yet), registered
 * weakly by the composition root. No Concealment type crosses it.
 *
 * Until Concealment exists the composition root's stand-in implements it over the prototype bushes (AStealthBush
 * footprints plus BushSpotToleranceCm, AStealthCharacter::IsConcealed for occupancy); with no query bound the resolver
 * sees no spot zones and no concealed pawn (rule 2 never applies, every pawn is an open-spot candidate).
 *
 * Every answer is server-only and read at the Attack's slot. Hide legality, replies and timing never read it (ADR-0017).
 */
class IBushSpotQuery
{
public:
	virtual ~IBushSpotQuery() = default;

	/** True when WorldPoint lies in a bush's spot zone (footprint plus BushSpotToleranceCm, CON F4); OutBushId names it. */
	virtual bool FindSpotZone(const FVector& WorldPoint, uint16& OutBushId) const = 0;

	/** GetSpotOccupants(BushId, Attacker): the bush's occupants (concealed, plus Exiting pawns already un-culled), any order. */
	virtual void GetSpotOccupants(uint16 BushId, const APawn* Attacker, TArray<APawn*>& OutOccupants) const = 0;

	/** The pawn is concealed now (a concealed pawn is a candidate only through rule 2). */
	virtual bool IsConcealed(const APawn* Pawn) const = 0;

	/**
	 * The bush's footprint (ADR-0017 FBushRuntime.Footprint, public geometry identical on server and clients): the strike
	 * WarpPoint is clamped to its attacker-side edge (ADR-0018 section 2.2, story asn-012). False: no footprint known.
	 */
	virtual bool GetFootprint(uint16 BushId, FBox2D& OutFootprint) const { return false; }

	/** CON-R9 in-slot occupancy exit of a victim killed while concealed (its un-cull is staged with the death write). */
	virtual void RemoveOccupant(APawn* Pawn) {}
};
