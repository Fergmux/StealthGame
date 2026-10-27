#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"

class AController;
class APawn;

/**
 * The server body sequence's seams (GDD ASN-R17, ASN-R22; ADR-0018 section 6; story asn-019).
 *
 * For every victim kind alike, the kill slot schedules two due-time items in Round's queue: TimerFired(BodyRelease) at
 * DeathServerTime + DeathFallSec and TimerFired(BodyRemoval) at DeathServerTime + DeathFallSec + BodyLingerSec +
 * BodyRemovalGraceSec (FTimerPayload.Subject = the pawn handle, Generation = the resolver's body generation, bumped by
 * Reset). Release turns the body's collision off pawn channels and releases it from its controller (crowd controller
 * destroyed, PlayerController left pawnless in its normal playing state, never Spectating); removal calls the observer,
 * then tears the pawn down. Never an engine timer, never a destroy of a possessed pawn; after the Ended latch both items
 * are Rejected_Latched and the body lies until Reset (or travel) removes it.
 */

/**
 * IBodyRemovalObserver: told once per body when its removal commits, whatever the victim kind or concealment state
 * (Concealment clears its killed-concealed owner / team flags here, ADR-0017). Declared by Assassination, implemented by
 * Concealment (not built: the composition root registers a stand-in). A missing observer is a fault-sink report and the
 * body is still removed.
 */
class IBodyRemovalObserver
{
public:
	virtual ~IBodyRemovalObserver() = default;

	virtual void OnBodyRemovalCommitted(APawn* Body) = 0;
};

/**
 * IEncounterBodyHandOver: the pawn factory's body hand-over as the resolver needs it (ADR-0010 ReleaseBody; production:
 * FEncounterPawnFactoryHandOver over UStealthPawnFactory; tests: a recorder).
 */
class IEncounterBodyHandOver
{
public:
	virtual ~IEncounterBodyHandOver() = default;

	/** BodyRelease: collision off pawn channels, then the controller lets the body go (the only release path). */
	virtual void ReleaseBody(APawn* Body) = 0;

	/** BodyRemoval and Reset: the body is destroyed (released first if a controller still holds it). */
	virtual void RequestTeardown(APawn* Body) = 0;
};
