// Stories con-002, con-005, con-007, con-015: the pure bush geometry of every machine. See View/ConcealmentTypes.h.

#include "View/ConcealmentTypes.h"

namespace ConcealmentGeometry
{
	double WrapYaw360(double YawDeg)
	{
		double Wrapped = FMath::Fmod(YawDeg, 360.0);
		if (Wrapped < 0.0)
		{
			Wrapped += 360.0;
		}
		return Wrapped >= 360.0 ? 0.0 : Wrapped;
	}

	uint8 SideFromYaw(double YawDeg)
	{
		// Sector k = [k*90 - 45, k*90 + 45): shift by 45 so each sector starts at a multiple of 90.
		const double Shifted = WrapYaw360(YawDeg + 45.0);
		return static_cast<uint8>(FMath::Clamp(static_cast<int32>(FMath::FloorToDouble(Shifted / 90.0)), 0, 3));
	}

	double AngularDistanceDeg(double A, double B)
	{
		const double Delta = WrapYaw360(A - B);
		return Delta > 180.0 ? 360.0 - Delta : Delta;
	}

	uint8 NearestOpenSide(uint8 EntryAxisMask, double YawDeg)
	{
		uint8 Best = SideFromYaw(YawDeg);
		double BestDistance = TNumericLimits<double>::Max();
		for (uint8 Side = 0; Side < 4; ++Side)
		{
			if ((EntryAxisMask & (1 << Side)) == 0)
			{
				continue;
			}
			const double Distance = AngularDistanceDeg(YawDeg, Side * 90.0);
			if (Distance < BestDistance) // strict: a tie keeps the lower index
			{
				BestDistance = Distance;
				Best = Side;
			}
		}
		return Best;
	}

	uint8 ValidateSide(uint8 EntryAxisMask, uint8 RequestedSide)
	{
		const uint8 Side = RequestedSide & 0x3;
		return (EntryAxisMask & (1 << Side)) != 0 ? Side : NearestOpenSide(EntryAxisMask, Side * 90.0);
	}

	FVector SideNormal(uint8 Side)
	{
		switch (Side & 0x3)
		{
		case 0: return FVector(1.0, 0.0, 0.0);
		case 1: return FVector(0.0, 1.0, 0.0);
		case 2: return FVector(-1.0, 0.0, 0.0);
		default: return FVector(0.0, -1.0, 0.0);
		}
	}

	double YawFromAnchor(const FBushRuntime& Bush, const FVector& Point)
	{
		const FVector Delta = Point - Bush.Anchor;
		return FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X));
	}

	double DistanceToFootprintCm(const FBox2D& Footprint, const FVector& Point)
	{
		if (!Footprint.bIsValid)
		{
			return TNumericLimits<double>::Max();
		}
		const double Dx = FMath::Max3(Footprint.Min.X - Point.X, 0.0, Point.X - Footprint.Max.X);
		const double Dy = FMath::Max3(Footprint.Min.Y - Point.Y, 0.0, Point.Y - Footprint.Max.Y);
		return FMath::Sqrt(Dx * Dx + Dy * Dy);
	}

	bool TargetsBush(const FBushRuntime& Bush, const FVector& WorldPoint, double ToleranceCm)
	{
		return DistanceToFootprintCm(Bush.Footprint, WorldPoint) <= ToleranceCm;
	}

	FVector ExitPoint(const FBushRuntime& Bush, uint8 Side, double CapsuleRadiusCm)
	{
		const double Out = FootprintSizeCm * 0.5 + CapsuleRadiusCm + ExitClearanceCm;
		return Bush.Anchor + SideNormal(Side) * Out;
	}

	bool IsCapsuleInsideHideVolume(const FBushRuntime& Bush, const FVector& Location, double CapsuleRadiusCm, double HalfHeightCm)
	{
		// Horizontal: the capsule's circle inside the footprint (1 cm tolerance for the warp's last frame).
		constexpr double ToleranceCm = 1.0;
		const FBox2D& F = Bush.Footprint;
		const bool bHorizontal = Location.X - CapsuleRadiusCm >= F.Min.X - ToleranceCm && Location.X + CapsuleRadiusCm <= F.Max.X + ToleranceCm
			&& Location.Y - CapsuleRadiusCm >= F.Min.Y - ToleranceCm && Location.Y + CapsuleRadiusCm <= F.Max.Y + ToleranceCm;
		// Vertical: from the ground (anchor) to the capsule top within the volume height.
		const double Bottom = Location.Z - HalfHeightCm;
		const double Top = Location.Z + HalfHeightCm;
		const bool bVertical = Bottom >= Bush.HideVolume.Min.Z - 20.0 && Top <= Bush.HideVolume.Max.Z + ToleranceCm;
		return bHorizontal && bVertical;
	}

	FBushRuntime MakeBushRuntime(uint16 Id, const FVector& GroundCentre, uint8 EntryAxisMask)
	{
		FBushRuntime Bush;
		Bush.Id = Id;
		Bush.Anchor = GroundCentre;
		Bush.EntryAxisMask = EntryAxisMask;
		const double Half = FootprintSizeCm * 0.5;
		Bush.Footprint = FBox2D(FVector2D(GroundCentre.X - Half, GroundCentre.Y - Half), FVector2D(GroundCentre.X + Half, GroundCentre.Y + Half));
		Bush.HideVolume = FBox(FVector(GroundCentre.X - Half, GroundCentre.Y - Half, GroundCentre.Z), FVector(GroundCentre.X + Half, GroundCentre.Y + Half, GroundCentre.Z + HideVolumeHeightCm));
		return Bush;
	}

	namespace
	{
		void HashBytes(uint64& State, const void* Data, int32 Count)
		{
			const uint8* Bytes = static_cast<const uint8*>(Data);
			for (int32 Index = 0; Index < Count; ++Index)
			{
				State ^= static_cast<uint64>(Bytes[Index]);
				State *= 1099511628211ull;
			}
		}

		void HashQuantised(uint64& State, double Value)
		{
			// 0.1 mm grid, little-endian int64: a stable byte form independent of the double's formatting.
			const int64 Q = static_cast<int64>(FMath::RoundToDouble(Value * 1000.0));
			uint8 Bytes[8];
			for (int32 Index = 0; Index < 8; ++Index)
			{
				Bytes[Index] = static_cast<uint8>((static_cast<uint64>(Q) >> (8 * Index)) & 0xFF);
			}
			HashBytes(State, Bytes, 8);
		}
	}

	uint64 HashBushRuntime(const FBushRuntime& Bush)
	{
		uint64 State = 14695981039346656037ull;
		const uint8 IdBytes[2] = { static_cast<uint8>(Bush.Id & 0xFF), static_cast<uint8>(Bush.Id >> 8) };
		HashBytes(State, IdBytes, 2);
		for (const double V : { Bush.HideVolume.Min.X, Bush.HideVolume.Min.Y, Bush.HideVolume.Min.Z, Bush.HideVolume.Max.X, Bush.HideVolume.Max.Y, Bush.HideVolume.Max.Z })
		{
			HashQuantised(State, V);
		}
		HashBytes(State, &Bush.EntryAxisMask, 1);
		for (const double V : { Bush.Anchor.X, Bush.Anchor.Y, Bush.Anchor.Z })
		{
			HashQuantised(State, V);
		}
		return State;
	}
}
