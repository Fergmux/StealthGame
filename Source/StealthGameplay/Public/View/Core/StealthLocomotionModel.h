#pragma once

#include "CoreMinimal.h"

/**
 * Shared Pawn body and traversal-height model (GDD shared-character-locomotion R17.1, F5; ADR-0010 section 2
 * "Body"; story SCL-002). Plain C++, client-safe (the classifier runs on both sides; story SCL-047 reuses it).
 *
 * Usage:
 *   const FStealthTraversalHeights Heights;                       // the TDC canonical defaults
 *   const EStealthCopingClass Class = StealthLocomotion::ClassifyCopingHeight(80.f, Heights);   // Vault
 */

/** The SCL traversal heights (TDC keys TraversalMinHeightCm, TraversalVaultHeightCm, TraversalClimbHeightCm,
 * TraversalHeightToleranceCm; Client audience, latch Round). The initialisers are the GDD canonical defaults and are
 * provisional pending the traversal spike: SCL-003 registers the keys, and the movement component then reads the
 * frozen snapshot. A changed default fails StealthGame.Locomotion.BodyConstantsPublished loudly. */
struct FStealthTraversalHeights
{
	/** At or below: stepped (also the CMC MaxStepHeight: one key, two uses). */
	float MinHeightCm = 45.0f;
	/** Vault capability height (the kit's vault coping). */
	float VaultHeightCm = 80.0f;
	/** Climb / mantle capability height. */
	float ClimbHeightCm = 160.0f;
	/** Accepted deviation from an authored height. */
	float ToleranceCm = 10.0f;
};

/** What an obstacle of a given top height is to a pawn (F5 height band only; reach, facing and landing are SCL-047). */
enum class EStealthCopingClass : uint8
{
	Step,
	Vault,
	Climb,
	Block,
};

/** The body constants every pawn shares, published to the Level Socket Contract and the Generator (R17.1). */
struct FStealthBodyConstants
{
	float CapsuleRadiusCm = 0.0f;
	float CapsuleHalfHeightCm = 0.0f;
	float MaxStepHeightCm = 0.0f;
	float WalkableFloorAngleDeg = 0.0f;
};

namespace StealthLocomotion
{
	/**
	 * F5 height bands: <= MinHeight steps; within Tolerance of the vault or climb height is a vault or climb; anything
	 * else (between the step height and the vault band, or between the bands) blocks.
	 */
	STEALTHGAMEPLAY_API EStealthCopingClass ClassifyCopingHeight(float HeightCm, const FStealthTraversalHeights& Heights);

	/** The engine default walkable floor angle the pawn keeps (44.765 degrees; the 40 degree temple stair passes). */
	inline constexpr float EngineDefaultWalkableFloorAngleDeg = 44.765f;
}
