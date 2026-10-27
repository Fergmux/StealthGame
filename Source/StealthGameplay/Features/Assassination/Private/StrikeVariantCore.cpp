// Story asn-018: the strike entry variant and warp numbers (ASN-R16). See StrikeVariantCore.h.

#include "StrikeVariantCore.h"

#include "View/StealthEncounterWire.h"

uint8 FStrikeVariantCore::EntryForBearing(double BearingDeg, double FrontHalfAngleDeg)
{
	if (FMath::Abs(BearingDeg) < FrontHalfAngleDeg)
	{
		return StrikeVariant::Strike_F;
	}
	return BearingDeg > 0.0 ? StrikeVariant::Strike_R60 : StrikeVariant::Strike_L60;
}

uint8 FStrikeVariantCore::Entry(uint8 VariantBits)
{
	return VariantBits & StrikeVariant::EntryMask;
}

bool FStrikeVariantCore::HasBushPreface(uint8 VariantBits)
{
	return (VariantBits & StrikeVariant::BushPreface) != 0;
}

double FStrikeVariantCore::EntryYawDeg(uint8 VariantBits)
{
	switch (Entry(VariantBits))
	{
	case StrikeVariant::Strike_L60: return -60.0;
	case StrikeVariant::Strike_R60: return 60.0;
	default: return 0.0;
	}
}

double FStrikeVariantCore::WarpYawDeg(double BearingDeg, uint8 VariantBits, double YawLimitDeg)
{
	const double Limit = FMath::Max(0.0, YawLimitDeg);
	return FMath::Clamp(FRotator::NormalizeAxis(BearingDeg - EntryYawDeg(VariantBits)), -Limit, Limit);
}

double FStrikeVariantCore::BearingOfOffset(const FVector& OffsetCm, double FacingYawDeg)
{
	if (FVector2D(OffsetCm.X, OffsetCm.Y).IsNearlyZero())
	{
		return 0.0;
	}
	return FRotator::NormalizeAxis(FMath::RadiansToDegrees(FMath::Atan2(OffsetCm.Y, OffsetCm.X)) - FacingYawDeg);
}

double FStrikeVariantCore::WarpTranslationCm(const FVector& OffsetCm, double ContactSeparationCm)
{
	return FMath::Max(0.0, FVector2D(OffsetCm.X, OffsetCm.Y).Size() - ContactSeparationCm);
}

const TCHAR* FStrikeVariantCore::EntryName(uint8 VariantBits)
{
	switch (Entry(VariantBits))
	{
	case StrikeVariant::Strike_L60: return TEXT("Strike_L60");
	case StrikeVariant::Strike_R60: return TEXT("Strike_R60");
	default: return TEXT("Strike_F");
	}
}
