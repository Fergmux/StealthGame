// Story cs-010: field of view (GDD camera-system.md F5). See View/Core/StealthFov.h.

#include "View/Core/StealthFov.h"

#include "View/Tuning/StealthCameraParams.h"

namespace StealthFovLocal
{
	double VerticalForHorizontal(double HorizontalDeg, double Aspect)
	{
		return FMath::RadiansToDegrees(2.0 * FMath::Atan(FMath::Tan(FMath::DegreesToRadians(HorizontalDeg) * 0.5) / Aspect));
	}
}

FStealthFovResult StealthFov::Compute(double VerticalDeg, double Aspect, const FStealthCameraParams& Params)
{
	FStealthFovResult Out;
	const double A = FMath::Max(Aspect, 0.01);
	const double Uncapped = FMath::RadiansToDegrees(2.0 * FMath::Atan(FMath::Tan(FMath::DegreesToRadians(VerticalDeg) * 0.5) * A));
	Out.bCapped = Uncapped > Params.FovHMaxDeg;
	Out.HorizontalDeg = FMath::Min(Uncapped, Params.FovHMaxDeg);
	Out.VerticalEffDeg = Out.bCapped ? StealthFovLocal::VerticalForHorizontal(Params.FovHMaxDeg, A) : VerticalDeg;
	return Out;
}

FStealthFovSliderRange StealthFov::SliderRange(double Aspect, const FStealthCameraParams& Params)
{
	FStealthFovSliderRange Out;
	Out.MinDeg = Params.FovVMinDeg;
	const double CapTop = StealthFovLocal::VerticalForHorizontal(Params.FovHMaxDeg, FMath::Max(Aspect, 0.01));
	Out.bDisabled = CapTop <= Params.FovVMinDeg;
	Out.TopDeg = FMath::Max(FMath::Min(Params.FovVMaxDeg, CapTop), Params.FovVMinDeg);
	return Out;
}

void FStealthFovEase::SetTarget(double TargetDeg, bool bReduceMotion)
{
	if (!bHasValue || bReduceMotion)
	{
		From = Target = Current = TargetDeg;
		Elapsed = StealthFov::EaseSec;
		bHasValue = true;
		return;
	}
	From = Current;
	Target = TargetDeg;
	Elapsed = 0.0;
}

void FStealthFovEase::Step(double DtSec)
{
	if (Elapsed >= StealthFov::EaseSec)
	{
		Current = Target;
		return;
	}
	Elapsed = FMath::Min(Elapsed + FMath::Max(DtSec, 0.0), StealthFov::EaseSec);
	const double T = Elapsed / StealthFov::EaseSec;
	Current = From + (Target - From) * (T * T * (3.0 - 2.0 * T));
}
