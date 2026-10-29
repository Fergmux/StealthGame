// Story cs-006: the eye pivot, boom and collision core (GDD camera-system.md CS-R4, F2). See View/Core/StealthCameraCore.h.

#include "View/Core/StealthCameraCore.h"

namespace StealthCameraCoreLocal
{
	double Smooth(double T)
	{
		const double C = FMath::Clamp(T, 0.0, 1.0);
		return C * C * (3.0 - 2.0 * C);
	}
}

FStealthCameraCore::FStealthCameraCore(const FStealthCameraParams& InParams)
	: Params(InParams)
{
}

double FStealthCameraCore::SwapBlendSec() const
{
	return Params.ShoulderSwapBlendSec;
}

void FStealthCameraCore::RequestShoulderSwap()
{
	if (IsSwapBlending())
	{
		bSwapBuffered = true; // one buffered press, run once when the blend ends
		return;
	}
	SwapFrom = SideBlend;
	Side = -Side;
	SwapElapsed = 0.0;
}

FStealthCameraOutput FStealthCameraCore::Step(const FStealthCameraBodyInput& Body, const IStealthCameraTraces& Traces)
{
	using namespace StealthCameraCoreLocal;
	FStealthCameraOutput Out;
	const double Dt = FMath::Max(Body.DtSec, 0.0);
	const double CapsuleBottom = Body.CapsuleCentre.Z - Body.CapsuleHalfHeightCm;
	const double CapsuleTop = Body.CapsuleCentre.Z + Body.CapsuleHalfHeightCm;
	const double EyeTarget = FMath::Clamp(Body.EyeSocketZ, CapsuleBottom, CapsuleTop);
	const double MaxStep = Params.PivotMaxVertCmPerSec * Dt;
	const bool bSnap = Body.bCorrection && Body.CorrectionDelta.Size() > Params.Consumed.PawnSmoothSnapThresholdCm;

	// --- Eye pivot (CS-R4) ---
	if (!bInitialised || bSnap)
	{
		PivotZ = EyeTarget;
		CorrectionOffset = FVector::ZeroVector;
		bInitialised = true;
	}
	else
	{
		if (Body.bCorrection)
		{
			// A small owner correction: the pivot stays where it was and the offset decays (no step).
			CorrectionOffset -= Body.CorrectionDelta;
			PivotZ -= Body.CorrectionDelta.Z;
		}
		const double Lag = Body.bInTraversal ? Params.CameraPivotLagTraversalSec : Params.CameraPivotLagSec;
		const double Alpha = Lag > 0.0 ? 1.0 - FMath::Exp(-Dt / Lag) : 1.0;
		const double Wanted = (EyeTarget - PivotZ) * Alpha;
		// The target is inside the capsule; the speed cap wins over the capsule while the body moves faster than the cap
		// (a drop or a climb), and the mask origin never rises above the real eye whatever the lag (min below).
		PivotZ += FMath::Clamp(Wanted, -MaxStep, MaxStep);
		const FVector OffsetXY(CorrectionOffset.X, CorrectionOffset.Y, 0.0);
		const double OffsetLen = OffsetXY.Size();
		if (OffsetLen > 0.0)
		{
			const double Decay = FMath::Min(OffsetLen, FMath::Max(OffsetLen * Alpha, 0.0));
			CorrectionOffset -= OffsetXY / OffsetLen * FMath::Min(Decay, MaxStep);
		}
		CorrectionOffset.Z = 0.0;
	}
	Out.Pivot = FVector(Body.CapsuleCentre.X + CorrectionOffset.X, Body.CapsuleCentre.Y + CorrectionOffset.Y, PivotZ);
	Out.MaskOrigin = FVector(Out.Pivot.X, Out.Pivot.Y, FMath::Min(PivotZ, Body.EyeSocketZ));

	// --- Shoulder swap blend ---
	if (Body.bReduceMotion && IsSwapBlending())
	{
		SwapElapsed = SwapBlendSec(); // a cut
	}
	if (IsSwapBlending())
	{
		SwapElapsed += Dt;
		SideBlend = FMath::Lerp(SwapFrom, static_cast<double>(Side), Smooth(SwapBlendSec() > 0.0 ? SwapElapsed / SwapBlendSec() : 1.0));
	}
	else
	{
		SideBlend = static_cast<double>(Side);
		if (bSwapBuffered)
		{
			bSwapBuffered = false;
			RequestShoulderSwap();
		}
	}

	// --- Squeeze: a blocked shoulder slides the offset to 0 (in 0.15 s) and back (in 0.40 s); never a flip ---
	const FRotator YawRot(0.0, Body.ViewYawDeg, 0.0);
	const double Height = Body.bConcealed ? 0.0 : Params.BoomHeightOffsetCm;
	const double FullOffset = Body.bConcealed ? 0.0 : Params.ShoulderOffsetCm;
	if (FullOffset > 0.0)
	{
		const FVector ShoulderPoint = Out.Pivot + YawRot.RotateVector(FVector(0.0, (Side >= 0 ? 1.0 : -1.0) * FullOffset, Height));
		const bool bBlocked = Traces.SweepFraction(Out.Pivot, ShoulderPoint, Params.CameraProbeRadiusCm) < 1.0 - 1e-6;
		const double In = Params.ShoulderSqueezeInSec > 0.0 ? Dt / Params.ShoulderSqueezeInSec : 1.0;
		const double OutRate = Params.ShoulderSqueezeOutSec > 0.0 ? Dt / Params.ShoulderSqueezeOutSec : 1.0;
		SqueezeFrac = bBlocked ? FMath::Max(0.0, SqueezeFrac - In) : FMath::Min(1.0, SqueezeFrac + OutRate);
	}
	const double Lateral = FullOffset * SqueezeFrac * SideBlend;
	Out.ShoulderOffsetCm = Lateral;

	// --- Boom (F2) ---
	const double Length = Body.bConcealed ? Params.ConcealedBoomCm : Params.BoomLengthCm;
	const FVector Ideal = Out.Pivot + YawRot.RotateVector(FVector(-Length, Lateral, Height));
	const FVector Offset = Ideal - Out.Pivot;
	const double DMax = Offset.Size();
	const FVector Dir = DMax > 0.0 ? Offset / DMax : FVector::BackwardVector;
	const double TRay = Traces.RayFraction(Out.Pivot, Ideal, Params.CameraGuardRadiusCm);
	const double TSweep = Traces.SweepFraction(Out.Pivot, Ideal, Params.CameraProbeRadiusCm);
	const double Target = FMath::Max(0.0, FMath::Min(TRay, TSweep) * DMax);
	if (!bInitialised || Target < Boom || Body.bReduceMotion || Boom <= 0.0)
	{
		if (Target < Boom)
		{
			HoldElapsed = 0.0;
		}
		Boom = Target; // pull-in in the same frame (and a cut under Reduce camera motion)
	}
	else if (Target > Boom)
	{
		HoldElapsed += Dt;
		if (HoldElapsed >= Params.BoomRecoverHoldSec)
		{
			const double Alpha = Params.BoomRecoverTauSec > 0.0 ? 1.0 - FMath::Exp(-Dt / Params.BoomRecoverTauSec) : 1.0;
			Boom += FMath::Min((Target - Boom) * Alpha, Params.BoomRecoverCmPerSec * Dt);
		}
	}
	else
	{
		HoldElapsed = 0.0;
	}
	Boom = FMath::Min(Boom, DMax);
	Out.FreeBoomCm = Boom;
	Out.IdealBoomCm = DMax;
	Out.LensLocation = Out.Pivot + Dir * Boom;
	Out.LensRotation = FRotator(Body.ViewPitchDeg, Body.ViewYawDeg, 0.0); // pitch rotates the lens in place: no orbit
	Out.bGuardClear = Traces.RayFraction(Out.Pivot, Out.LensLocation, Params.CameraGuardRadiusCm) >= 1.0 - 1e-6;
	Out.bCloseThirdPerson = Boom < Params.BoomCloseCm;
	// The mask-off band (D6): off below 25 cm, on again only above 25 + hysteresis.
	if (bMaskOff)
	{
		bMaskOff = Boom <= MaskOffBelowCm + Params.CameraMaskOffHysteresisCm;
	}
	else
	{
		bMaskOff = Boom < MaskOffBelowCm;
	}
	Out.bMaskOff = bMaskOff;
	return Out;
}
