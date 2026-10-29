#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "View/ConcealmentTypes.h"

/**
 * Concealment collision (ADR-0017 section 4; GDD CON-R4; stories con-002, con-008). Client-safe and pure.
 *
 * The two collision-profile entries this feature adds in Config/DefaultEngine.ini (the implementation story adds them):
 *   BushVolume        object channel ECC_GameTraceChannel3, default response Block: every pawn capsule blocks a hide
 *                     volume unless its concealment state says otherwise (CON-R1: hide volumes block non-hiding pawns)
 *   PresentationAim   trace channel ECC_GameTraceChannel4, default response Block: the client-local aim, Strike-hint and
 *                     prompt traces (ADR-0017 section 6); a presentation-hidden pawn switches its response to Ignore
 *
 * ComputeConcealCollision(State, ElapsedInTransition) is the one rule, evaluated on the server and on the owner for each
 * simulated move and never set by an anim notify: Outside blocks BushVolume and Pawn; Entering, Concealed, Exiting and
 * Emerging ignore both at every elapsed time. Responses are written only when the computed value changes (each write
 * refreshes the body's physics state).
 */
namespace ConcealmentCollision
{
	/** The hide volumes' object channel (DefaultEngine.ini "BushVolume"). */
	inline constexpr ECollisionChannel BushVolumeChannel = ECC_GameTraceChannel3;
	/** The client-local presentation aim trace channel (DefaultEngine.ini "PresentationAim"). */
	inline constexpr ECollisionChannel PresentationAimChannel = ECC_GameTraceChannel4;
}

/** A capsule's responses to the two channels concealment controls. */
struct FConcealCollision
{
	ECollisionResponse BushVolume = ECR_Block;
	ECollisionResponse Pawn = ECR_Block;

	bool operator==(const FConcealCollision& Other) const { return BushVolume == Other.BushVolume && Pawn == Other.Pawn; }
	bool operator!=(const FConcealCollision& Other) const { return !(*this == Other); }
};

/** The collision of State at ElapsedInTransition seconds into the transition (elapsed never changes the answer). Pure. */
STEALTHGAMEPLAY_API FConcealCollision ComputeConcealCollision(EConcealState State, double ElapsedInTransition);

/** True when Move input produces no displacement in State (every state but Outside, from transition start). Pure. */
STEALTHGAMEPLAY_API bool IsConcealMoveBlocked(EConcealState State);

/**
 * Writes collision responses only when the computed value changes (story con-008 AC2): Apply returns true and counts a
 * write when Next differs from the last applied value. The owner of a capsule keeps one per pawn.
 */
struct STEALTHGAMEPLAY_API FConcealCollisionWriter
{
	/** Applies Next to Capsule when it differs from the last applied value. Null capsule: records only (tests). */
	bool Apply(class UPrimitiveComponent* Capsule, const FConcealCollision& Next);

	FConcealCollision Current;
	int32 WriteCount = 0;
};
