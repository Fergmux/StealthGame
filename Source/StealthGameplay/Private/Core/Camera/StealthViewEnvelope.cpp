// Story cs-002: the view envelope (GDD camera-system.md F1). See View/Core/StealthViewEnvelope.h.

#include "View/Core/StealthViewEnvelope.h"

#include "View/Tuning/StealthCameraParams.h"

FViewEnvelopeParams FViewEnvelopeParams::FromCamera(const FStealthCameraParams& Camera)
{
	FViewEnvelopeParams P;
	P.YawLimitDeg = Camera.Consumed.LookYawLimitDeg;
	P.PitchDownLimitDeg = Camera.ViewPitchDownLimitDeg;
	P.PitchUpLimitDeg = Camera.ViewPitchUpLimitDeg;
	P.SoftYawDeg = Camera.CameraEdgeSoftYawDeg;
	P.SoftPitchDeg = Camera.CameraEdgeSoftPitchDeg;
	P.MinGain = Camera.CameraEdgeSoftMinGain;
	P.PushDeg = Camera.CameraEdgePushDeg;
	P.PushMinRateDegPerSec = Camera.EdgePushMinRateDegPerSec;
	P.PushLatchSec = Camera.EdgePushLatchSec;
	P.RelaxDegPerSec = Camera.EdgePushRelaxDegPerSec;
	P.HeadLeadMaxDeg = Camera.ViewHeadLeadMaxDeg;
	return P;
}

namespace StealthViewEnvelopeLocal
{
	/** Hermite smoothstep of X over [Edge0, Edge1]; a zero-width band is a step at Edge1. */
	double SmoothStep(double Edge0, double Edge1, double X)
	{
		if (Edge1 <= Edge0)
		{
			return X >= Edge1 ? 1.0 : 0.0;
		}
		const double T = FMath::Clamp((X - Edge0) / (Edge1 - Edge0), 0.0, 1.0);
		return T * T * (3.0 - 2.0 * T);
	}

	/** One axis toward or away from a limit: the outward step is gained, the inward step is not. Returns the new value. */
	double StepAxis(double Value, double Delta, double Limit, double Soft, double MinGain)
	{
		if (Delta == 0.0)
		{
			return Value;
		}
		const bool bOutward = Value == 0.0 || (Delta > 0.0) == (Value > 0.0);
		if (!bOutward)
		{
			return Value + Delta; // inward: gain 1 on the same frame (no dead zone at the edge, AC-CS-04)
		}
		const double Gain = StealthViewEnvelope::SoftGain(FMath::Abs(Value), Limit, Soft, MinGain);
		const double Magnitude = FMath::Min(Limit, FMath::Abs(Value) + Gain * FMath::Abs(Delta));
		return Delta > 0.0 ? Magnitude : -Magnitude;
	}
}

double StealthViewEnvelope::SoftGain(double AbsValue, double Limit, double Soft, double MinGain)
{
	return FMath::Max(MinGain, 1.0 - StealthViewEnvelopeLocal::SmoothStep(Limit - Soft, Limit, AbsValue));
}

void StealthViewEnvelope::Step(FViewEnvelopeState& State, const FViewEnvelopeInput& Input, const FViewEnvelopeParams& Params)
{
	using namespace StealthViewEnvelopeLocal;
	const double Dt = FMath::Max(Input.DtSec, 0.0);
	State.TimeSec += Dt;
	const double Limit = Params.YawLimitDeg;
	const double D = Input.DeltaYawDeg;
	const double V = State.ViewYawOffsetDeg;
	const bool bOutward = D != 0.0 && (V == 0.0 || (D > 0.0) == (V > 0.0));
	const bool bInward = D != 0.0 && !bOutward;

	// Pushing: the raw outward rate (never the gained one), latched for T_latch.
	if (bOutward && Dt > 0.0 && FMath::Abs(D) / Dt >= Params.PushMinRateDegPerSec)
	{
		State.LastPushSec = State.TimeSec;
	}
	State.bPushing = State.TimeSec - State.LastPushSec <= Params.PushLatchSec + 1e-9;

	// Any inward input first pulls the target back to the limit.
	if (bInward && FMath::Abs(State.TargetYawOffsetDeg) > Limit)
	{
		State.TargetYawOffsetDeg = State.TargetYawOffsetDeg > 0.0 ? Limit : -Limit;
	}

	// The view yaw offset.
	double NewV = StepAxis(V, D, Limit, Params.SoftYawDeg, Params.MinGain);
	if (Params.HeadLeadMaxDeg > 0.0)
	{
		// CS-R3.5 (OQ2 fallback, disabled by default): the view leads the visible head by at most HeadLeadMaxDeg.
		NewV = FMath::Clamp(NewV, Input.HeadYawOffsetDeg - Params.HeadLeadMaxDeg, Input.HeadYawOffsetDeg + Params.HeadLeadMaxDeg);
	}
	State.ViewYawOffsetDeg = NewV;

	// The target.
	const double AbsV = FMath::Abs(NewV);
	if (AbsV < Limit - 1e-9)
	{
		State.TargetYawOffsetDeg = NewV;
	}
	else
	{
		const double Sign = NewV >= 0.0 ? 1.0 : -1.0;
		const double Overshoot = Input.bConcealed ? 0.0 : Params.PushDeg;
		double AbsR = FMath::Max(Limit, FMath::Abs(State.TargetYawOffsetDeg));
		if (FMath::Sign(State.TargetYawOffsetDeg) != Sign && State.TargetYawOffsetDeg != 0.0)
		{
			AbsR = Limit; // the target never sits past the opposite limit
		}
		if (State.bPushing)
		{
			AbsR = FMath::Min(AbsR + (bOutward ? FMath::Abs(D) : 0.0), Limit + Overshoot);
		}
		else
		{
			AbsR = FMath::Max(Limit, AbsR - Params.RelaxDegPerSec * Dt);
		}
		AbsR = FMath::Min(AbsR, Limit + Overshoot);
		State.TargetYawOffsetDeg = Sign * AbsR;
	}

	// The view pitch: the same gain ramp at both limits, no push.
	const double P = State.ViewPitchDeg;
	const double DP = Input.DeltaPitchDeg;
	if (DP != 0.0)
	{
		const bool bUp = DP > 0.0;
		const double PitchLimit = bUp ? Params.PitchUpLimitDeg : Params.PitchDownLimitDeg;
		const bool bTowardLimit = bUp ? P >= 0.0 : P <= 0.0;
		if (bTowardLimit)
		{
			const double Gain = SoftGain(FMath::Abs(P), PitchLimit, Params.SoftPitchDeg, Params.MinGain);
			const double Magnitude = FMath::Min(PitchLimit, FMath::Abs(P) + Gain * FMath::Abs(DP));
			State.ViewPitchDeg = bUp ? Magnitude : -Magnitude;
		}
		else
		{
			State.ViewPitchDeg = FMath::Clamp(P + DP, -Params.PitchDownLimitDeg, Params.PitchUpLimitDeg);
		}
	}
}
