#pragma once

#include "CoreMinimal.h"
#include "View/CommandReplyTypes.h"

#include "StealthEncounterWire.generated.h"

class UPackageMap;

/**
 * The encounter wire types (ADR-0018 section 1 and Key Interfaces; GDD assassination-defence-reversal.md ASN-R21,
 * AC-ASN-20; story asn-004). Declared once in Foundation Public/View so the IIF, Camera, HUD and Assassination all read
 * one declaration (LP concern 2).
 *
 * Pillar-1 rule: a size is a side channel that would encode the outcome, so FActionState is exactly 208 bits and
 * FDeathState exactly 65 bits for EVERY value, through custom NetSerialize (SerializeBits only; no optional field, no
 * conditional bit, no magnitude-packed vector) and Identical (every field, Seq included, by bit pattern). No pawn, actor,
 * object or FName member: neither struct names who struck whom.
 *
 * Replicated defaults are sentinels (Seq 1, StartServerTime and ContactServerTime -1, DeathServerTime -1), so an actor
 * opened late that receives both structs in one bunch presents correctly (story asn-018). The replicated properties on
 * AStealthPawn (OnRep_ActionState / OnRep_DeathState, allowlist rows) belong to story asn-012; FStrikeResult and
 * FDefenceResult (View/CommandReplyTypes.h) are built by story asn-015.
 *
 * Usage (server writer, story asn-012):
 *   FActionState Next;  Next.Pose = EActionPose::Strike;  Next.Seq = FActionState::NextSeq(Current.Seq);
 *   Next.Variant = StrikeVariant::Strike_L60;  Next.StartServerTime = Now;  Next.ContactServerTime = Now + T_v;
 *   Next.WarpPoint = FFixedWarpOffset::FromOffsetCm(WarpTarget - AttackerLocation);
 */

/** The pose of FActionState (IIF R12 list, ADR-0011 owns it; append-only: the values are on the wire). */
UENUM()
enum class EActionPose : uint8
{
	None,
	Strike,
	Brace,
	Counter,
	Interact,
	BushEnter,
	BushExit,
};

/** Number of EActionPose values. */
inline constexpr uint8 GActionPoseCount = 7;

// EStrikeResult, EDefenceResult, FStrikeResult and FDefenceResult moved to View/CommandReplyTypes.h (story asn-015): they
// are entries of the batched ClientCommandReplies (FCommandReply carries them), and that header is included above.

/** FActionState.Variant for Pose Strike: bits 0-1 the entry variant (ASN-R16), bit 2 the bush preface; 0 for every other pose. */
namespace StrikeVariant
{
	inline constexpr uint8 Strike_F = 0;
	inline constexpr uint8 Strike_L60 = 1;
	inline constexpr uint8 Strike_R60 = 2;
	inline constexpr uint8 EntryMask = 0x3;
	inline constexpr uint8 BushPreface = 0x4;
}

/**
 * The strike's warp target as an offset from the attacker's server capsule location at the slot: three int16 in 0.1 cm on
 * world axes (range +-3,276.7 cm, far above the ADR-0018 section 2.2 clamp, so the clamp, not the encoding, bounds it).
 * (0,0,0) for every non-Strike pose. UPROPERTY members for legacy change detection (ADR-0003 section 4).
 */
USTRUCT()
struct STEALTHFOUNDATION_API FFixedWarpOffset
{
	GENERATED_BODY()

	/** Centimetres per unit. */
	static constexpr double UnitCm = 0.1;

	UPROPERTY()
	int16 X = 0;

	UPROPERTY()
	int16 Y = 0;

	UPROPERTY()
	int16 Z = 0;

	/** Quantises an offset in cm to the nearest 0.1 cm, saturating at the int16 range. */
	static FFixedWarpOffset FromOffsetCm(const FVector& OffsetCm);

	/** The offset in cm. */
	FVector ToOffsetCm() const { return FVector(X * UnitCm, Y * UnitCm, Z * UnitCm); }

	bool operator==(const FFixedWarpOffset& Other) const { return X == Other.X && Y == Other.Y && Z == Other.Z; }
	bool operator!=(const FFixedWarpOffset& Other) const { return !(*this == Other); }
};

/**
 * The outcome-blind action state of a pawn (ADR-0018 section 1.1): one ReplicatedUsing property on AStealthPawn, audience
 * All, written only by UPawnCommandComponent::WriteActionState (story asn-012), which assigns Seq. 208 bits always:
 * Pose 8 | Seq 16 | Variant 8 | StartServerTime 64 | ContactServerTime 64 | WarpPoint 48.
 */
USTRUCT()
struct STEALTHFOUNDATION_API FActionState
{
	GENERATED_BODY()

	/** Serialised width in bits, for every value. */
	static constexpr int32 WireBits = 208;

	UPROPERTY()
	EActionPose Pose = EActionPose::None;

	/** Per-pawn write counter (+1 on every write of that pawn, wraps 65535 to 1; default 1). Never a ClientCommandSeq. */
	UPROPERTY()
	uint16 Seq = 1;

	/** Strike: StrikeVariant bits; every other pose 0. */
	UPROPERTY()
	uint8 Variant = 0;

	/** Server time the pose began (every pose; -1 sentinel before the first write). */
	UPROPERTY()
	double StartServerTime = -1.0;

	/** Strike: slot time + StrikeContactSec (+ BushStrikeEmergeSec with BushPreface); Counter: the reversed strike's; else -1. */
	UPROPERTY()
	double ContactServerTime = -1.0;

	/** Strike: the warp target offset; every other pose (0,0,0). */
	UPROPERTY()
	FFixedWarpOffset WarpPoint;

	/** The Seq after Current: +1, wrapping 65535 to 1 (0 is never used). */
	static uint16 NextSeq(uint16 Current) { return Current >= 65535 ? 1 : static_cast<uint16>(Current + 1); }

	/** Writes or reads exactly WireBits with SerializeBits. Always returns true ("mapped": no object references). */
	bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess);

	/** True only when all six fields are equal (doubles by bit pattern, so NaN equals itself and -0 differs from +0). */
	bool Identical(const FActionState* Other, uint32 PortFlags) const;
};

template <>
struct TStructOpsTypeTraits<FActionState> : public TStructOpsTypeTraitsBase2<FActionState>
{
	enum
	{
		WithNetSerializer = true,
		WithNetSharedSerialization = true,
		WithIdentical = true,
	};
};

/**
 * The death of a pawn (ADR-0018 section 1.2): one ReplicatedUsing property on AStealthPawn, audience All, written once by
 * the death write (story asn-013). 65 bits always: DeathServerTime 64 | bDiedConcealed 1.
 */
USTRUCT()
struct STEALTHFOUNDATION_API FDeathState
{
	GENERATED_BODY()

	/** Serialised width in bits, for every value. */
	static constexpr int32 WireBits = 65;

	/** The deciding strike's ContactServerTime; -1 while alive. */
	UPROPERTY()
	double DeathServerTime = -1.0;

	/** True only when the victim was Concealed or CullPending at the death write (every victim kind alike). */
	UPROPERTY()
	bool bDiedConcealed = false;

	/** Writes or reads exactly WireBits with SerializeBits. Always returns true. */
	bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess);

	/** True only when both fields are equal (DeathServerTime by bit pattern). */
	bool Identical(const FDeathState* Other, uint32 PortFlags) const;
};

template <>
struct TStructOpsTypeTraits<FDeathState> : public TStructOpsTypeTraitsBase2<FDeathState>
{
	enum
	{
		WithNetSerializer = true,
		WithNetSharedSerialization = true,
		WithIdentical = true,
	};
};

#if WITH_STEALTH_TEST_SEAMS
/**
 * Test seam (ADR-0008 section 3; story asn-004 AC3): replaces FActionState::NetSerialize's body so a negative control can
 * prove the width test catches a variable-width encoding. Compiled only under WITH_STEALTH_TEST_SEAMS (never Shipping).
 */
namespace StealthEncounterWireTestSeams
{
	/** A replacement serialiser. */
	using FActionStateSerializer = void (*)(FActionState& State, FArchive& Ar);

	/** Installs Serializer (null restores the production serialiser). Test-thread only. */
	STEALTHFOUNDATION_API void SetActionStateSerializerOverride(FActionStateSerializer Serializer);

	/** The forbidden encoding: the production layout except WarpPoint, packed by magnitude (zig-zag + SerializeIntPacked). */
	STEALTHFOUNDATION_API void SerializeWarpPointByMagnitude(FActionState& State, FArchive& Ar);
}
#endif
