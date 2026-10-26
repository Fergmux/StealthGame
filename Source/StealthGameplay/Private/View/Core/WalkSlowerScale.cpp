#include "View/Core/WalkSlowerScale.h"

float StealthWalkSlower::PaceFromModifiers(bool bCtrlHeld, bool bAltHeld)
{
	float Pace = StealthPaceSet::Full;
	if (bCtrlHeld)
	{
		Pace = FMath::Min(Pace, StealthPaceSet::Slow);
	}
	if (bAltHeld)
	{
		Pace = FMath::Min(Pace, StealthPaceSet::Middle);
	}
	return Pace;
}

float StealthWalkSlower::PaceFromReduction(float Reduction)
{
	return FMath::Clamp(StealthPaceSet::Full - FMath::Abs(Reduction), StealthPaceSet::Slow, StealthPaceSet::Full);
}

FVector2D StealthWalkSlower::ScaleMove(const FVector2D& Move, EMoveInputDevice Device, float Pace, bool bRunHeld)
{
	if (Device == EMoveInputDevice::Gamepad)
	{
		return Move;
	}
	const double Magnitude = FMath::Min(Move.Size(), 1.0);
	if (Magnitude <= UE_SMALL_NUMBER)
	{
		return FVector2D::ZeroVector;
	}
	const double Scale = bRunHeld ? 1.0 : static_cast<double>(Pace);
	return Move.GetSafeNormal() * (Magnitude * Scale);
}
