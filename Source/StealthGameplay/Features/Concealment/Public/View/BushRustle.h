#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPath.h"
#include "View/ConcealmentTypes.h"
#include "View/StealthEncounterWire.h"
#include "View/StealthTuningPublicSubstructs.h"

/**
 * The bush rustle (ADR-0017 section 9; GDD concealment-bushes.md CON-R13, AC-CON-18, VA-2, VA-6; story con-029). A stateless
 * pulse ring: 8 slots {WorldPos.xyz, StartTime; SideDir.xy, Amplitude, unused} that every bush material reads through one
 * shared WPO function (the material and MPC_BushRustle are the Art epic's; the graybox bush has none, so the ring is the
 * client-local source of truth and is written to a Material Parameter Collection only when one is configured). Each
 * client writes a slot when its presentation path sees a BushEnter, BushExit or bush-emerge pose of ANY pawn, timed from
 * the pose's StartServerTime (onset 0.1 s before the swap or the leaf crossing), at the bush face on the transition's side.
 * The SFX comes from one shared pool spatialised at the face. Identical for every pawn kind, both camera modes and every
 * LOD; never scaled by occupancy; nothing while a bush is merely occupied. The rustle is tagged eye-masked (Camera CS-R5b,
 * ADR-0015: the third-person mask applies it; Camera story 015).
 *
 * Nothing here reads the pawn's kind, team, controller, LOD, camera mode or the bush's occupancy: the pulse is a function
 * of {bush, side, transition, pose start, public tuning} only (StealthGame.Concealment.RustlePulseSlot scans for it).
 */

/** Which transition a pulse belongs to. */
enum class EBushRustleKind : uint8
{
	Enter,
	Exit,
	Emerge,
};

/** One ring slot (and the observation record of the write). */
struct FBushRustlePulse
{
	uint16 BushId = 0;
	uint8 Side = 0;
	EBushRustleKind Kind = EBushRustleKind::Enter;
	/** The pose's StartServerTime. */
	double StartServerTime = 0.0;
	/** The slot's StartTime: 0.1 s before the swap (Enter) or the leaf crossing (Exit, Emerge). */
	double OnsetServerTime = 0.0;
	/** The bush face centre on the transition's side, at the anchor's ground height. */
	FVector WorldPos = FVector::ZeroVector;
	/** The side's outward horizontal direction. */
	FVector2D SideDir = FVector2D::ZeroVector;
	/** BushRustleAmplitudeCm. */
	float AmplitudeCm = 0.0f;
	/** The one shared SFX (identical for every pulse). */
	FSoftObjectPath Sound;
	/** Tagged for the third-person eye-origin mask (Camera CS-R5b). */
	bool bEyeMasked = true;
};

namespace BushRustle
{
	/** Slots of the ring (ADR-0017 section 9). */
	inline constexpr int32 SlotCount = 8;
	/** The onset leads the swap / crossing by this much. */
	inline constexpr double OnsetLeadSec = 0.1;
	/** How long a pulse is live (the material's response window; a late writer still seeks into it). */
	inline constexpr double PulseLifeSec = 1.0;

	/** STAND-IN (graybox): the one shared rustle SFX until the Art epic records the foliage sound (a sample asset, read only). */
	STEALTHGAMEPLAY_API FSoftObjectPath DefaultSound();

	/** The transition a pose is, or unset for a pose that is no hide transition. */
	STEALTHGAMEPLAY_API TOptional<EBushRustleKind> KindOfPose(const FActionState& Action);

	/** The onset server time of a transition that started at StartServerTime. */
	STEALTHGAMEPLAY_API double OnsetFor(EBushRustleKind Kind, double StartServerTime, const FConcealmentTuning_Public& Tuning);

	/** The face centre of Side (footprint edge midpoint at ground height). */
	STEALTHGAMEPLAY_API FVector FacePoint(const FBushRuntime& Bush, uint8 Side);

	/** The pulse for one transition (pure). */
	STEALTHGAMEPLAY_API FBushRustlePulse MakePulse(const FBushRuntime& Bush, uint8 Side, EBushRustleKind Kind, double StartServerTime,
		const FConcealmentTuning_Public& Tuning, const FSoftObjectPath& Sound);
}

/** The 8-slot ring, reused round-robin client-locally: the 9th pulse overwrites the oldest slot. */
class STEALTHGAMEPLAY_API FBushRustleRing
{
public:
	/** Writes Pulse into the next slot (the oldest); returns its index. */
	int32 Write(const FBushRustlePulse& Pulse);

	/** Slot Index (0..SlotCount-1); unset until first written. */
	const FBushRustlePulse* GetSlot(int32 Index) const;

	/** Pulses written since construction. */
	int32 GetWriteCount() const { return Writes; }

	/** The slot the next write uses. */
	int32 GetNextSlot() const { return Next; }

private:
	FBushRustlePulse Slots[BushRustle::SlotCount];
	bool bWritten[BushRustle::SlotCount] = {};
	int32 Next = 0;
	int32 Writes = 0;
};
