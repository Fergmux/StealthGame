// Story asn-004 (production/epics/assassination-defence-reversal/story-004-wire-structs-fixed-width.md): the fixed-width
// serialisers of FActionState (208 bits) and FDeathState (65 bits), ADR-0018 section 1, E1, E2. SerializeBits only:
// FArchive::SerializeInt would write 32 bits on the base archive and is capped only by the net bit-writer.

#include "View/StealthEncounterWire.h"

namespace
{
	/** Bitwise equality of two doubles (NaN equals itself, -0 differs from +0: a change of bits is a change). */
	bool SameBits(double A, double B)
	{
		return FMemory::Memcmp(&A, &B, sizeof(double)) == 0;
	}

	void SerializeDouble(FArchive& Ar, double& Value)
	{
		Ar.SerializeBits(&Value, 64);
	}

	void SerializeInt16(FArchive& Ar, int16& Value)
	{
		Ar.SerializeBits(&Value, 16);
	}

	/** Pose 8 | Seq 16 | Variant 8 | Start 64 | Contact 64; the caller adds WarpPoint (48 in production). */
	void SerializeHead(FActionState& State, FArchive& Ar)
	{
		uint8 PoseByte = static_cast<uint8>(State.Pose);
		Ar.SerializeBits(&PoseByte, 8);
		if (Ar.IsLoading())
		{
			// A value outside the closed enum is read as None; the width is unchanged either way.
			State.Pose = PoseByte < GActionPoseCount ? static_cast<EActionPose>(PoseByte) : EActionPose::None;
		}
		Ar.SerializeBits(&State.Seq, 16);
		Ar.SerializeBits(&State.Variant, 8);
		SerializeDouble(Ar, State.StartServerTime);
		SerializeDouble(Ar, State.ContactServerTime);
	}

	void SerializeFixed(FActionState& State, FArchive& Ar)
	{
		SerializeHead(State, Ar);
		SerializeInt16(Ar, State.WarpPoint.X);
		SerializeInt16(Ar, State.WarpPoint.Y);
		SerializeInt16(Ar, State.WarpPoint.Z);
	}

#if WITH_STEALTH_TEST_SEAMS
	StealthEncounterWireTestSeams::FActionStateSerializer GActionStateSerializerOverride = nullptr;
#endif
}

FFixedWarpOffset FFixedWarpOffset::FromOffsetCm(const FVector& OffsetCm)
{
	auto Quantise = [](double Cm)
	{
		const double Units = FMath::RoundHalfFromZero(Cm / UnitCm);
		return static_cast<int16>(FMath::Clamp(Units, static_cast<double>(MIN_int16), static_cast<double>(MAX_int16)));
	};
	FFixedWarpOffset Out;
	Out.X = Quantise(OffsetCm.X);
	Out.Y = Quantise(OffsetCm.Y);
	Out.Z = Quantise(OffsetCm.Z);
	return Out;
}

bool FActionState::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
#if WITH_STEALTH_TEST_SEAMS
	if (GActionStateSerializerOverride != nullptr)
	{
		GActionStateSerializerOverride(*this, Ar);
		bOutSuccess = true;
		return true;
	}
#endif
	SerializeFixed(*this, Ar);
	bOutSuccess = true;
	return true;
}

bool FActionState::Identical(const FActionState* Other, uint32 PortFlags) const
{
	return Other != nullptr && Pose == Other->Pose && Seq == Other->Seq && Variant == Other->Variant && SameBits(StartServerTime, Other->StartServerTime)
		&& SameBits(ContactServerTime, Other->ContactServerTime) && WarpPoint == Other->WarpPoint;
}

bool FDeathState::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	SerializeDouble(Ar, DeathServerTime);
	uint8 Concealed = bDiedConcealed ? 1 : 0;
	Ar.SerializeBits(&Concealed, 1);
	if (Ar.IsLoading())
	{
		bDiedConcealed = (Concealed & 1) != 0;
	}
	bOutSuccess = true;
	return true;
}

bool FDeathState::Identical(const FDeathState* Other, uint32 PortFlags) const
{
	return Other != nullptr && SameBits(DeathServerTime, Other->DeathServerTime) && bDiedConcealed == Other->bDiedConcealed;
}

#if WITH_STEALTH_TEST_SEAMS
void StealthEncounterWireTestSeams::SetActionStateSerializerOverride(FActionStateSerializer Serializer)
{
	GActionStateSerializerOverride = Serializer;
}

void StealthEncounterWireTestSeams::SerializeWarpPointByMagnitude(FActionState& State, FArchive& Ar)
{
	SerializeHead(State, Ar);
	int16* Axes[] = { &State.WarpPoint.X, &State.WarpPoint.Y, &State.WarpPoint.Z };
	for (int16* Axis : Axes)
	{
		// Zig-zag then variable-length: small offsets take fewer bytes (exactly what ADR-0018 forbids).
		uint32 Packed = static_cast<uint32>((static_cast<int32>(*Axis) << 1) ^ (static_cast<int32>(*Axis) >> 31));
		Ar.SerializeIntPacked(Packed);
		if (Ar.IsLoading())
		{
			*Axis = static_cast<int16>(static_cast<int32>(Packed >> 1) ^ -static_cast<int32>(Packed & 1));
		}
	}
}
#endif
