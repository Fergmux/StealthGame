// Story asn-002 (production/epics/assassination-defence-reversal/story-002-reach-f1-pure-function.md): GDD F1 reach and
// warp as a pure function (ADR-0018 sections 2.1-2.2). See Server/StrikeReachCore.h.

#include "Server/StrikeReachCore.h"

#include "Server/StealthTuningSubstructs.h"

FStrikeReachParams FStrikeReachParams::FromTuning(const FAssassinationTuning_Public& Public, const FAssassinationTuning_Server& Server,
	EStrikeReachVariant Variant, double BushStrikeEmergeSec, double CapsuleRadiusCm)
{
	FStrikeReachParams Out;
	Out.RangeCm = Public.AssassinationRangeCm;
	Out.VerticalReachCm = Server.StrikeVerticalReachCm;
	Out.ContactSec = static_cast<double>(Public.StrikeContactSec) + (Variant == EStrikeReachVariant::BushStrike ? BushStrikeEmergeSec : 0.0);
	Out.NetLeadSec = Server.StrikeNetLeadSec;
	Out.WarpLimitCm = Public.StrikeWarpLimitCm;
	Out.ContactGapCm = Public.StrikeContactGapCm;
	Out.BearingLimitDeg = Public.StrikeBearingLimitDeg;
	Out.CapsuleRadiusCm = CapsuleRadiusCm;
	return Out;
}

double FStrikeReachCore::BrakingDistance(double SpeedCmPerSec, double NetLeadSec, double BrakingCmPerSec2)
{
	if (SpeedCmPerSec <= 0.0)
	{
		return 0.0;
	}
	const double Lead = FMath::Max(NetLeadSec, 0.0);
	if (BrakingCmPerSec2 <= 0.0)
	{
		return SpeedCmPerSec * Lead; // no braking modelled: the victim keeps its speed through the lead
	}
	// Still moving at the end of the lead: distance under constant deceleration; otherwise it reached rest.
	if (Lead < SpeedCmPerSec / BrakingCmPerSec2)
	{
		return SpeedCmPerSec * Lead - 0.5 * BrakingCmPerSec2 * Lead * Lead;
	}
	return SpeedCmPerSec * SpeedCmPerSec / (2.0 * BrakingCmPerSec2);
}

double FStrikeReachCore::ExtrapolationDistance(double SpeedCmPerSec, double ContactSec, double NetLeadSec, double BrakingCmPerSec2)
{
	const double FreeSec = FMath::Max(ContactSec - NetLeadSec, 0.0);
	return FMath::Max(SpeedCmPerSec, 0.0) * FreeSec + BrakingDistance(SpeedCmPerSec, NetLeadSec, BrakingCmPerSec2);
}

FStrikeReachResult FStrikeReachCore::Evaluate(const FStrikeReachParams& Params, const FStrikeReachInput& Input)
{
	FStrikeReachResult Out;
	const FVector2D Attacker(Input.AttackerLocation.X, Input.AttackerLocation.Y);
	const FVector2D Victim(Input.VictimLocation.X, Input.VictimLocation.Y);
	const FVector2D Velocity(Input.VictimVelocity.X, Input.VictimVelocity.Y);

	Out.DistanceCm = FVector2D::Distance(Victim, Attacker);
	Out.DeltaZCm = Input.VictimLocation.Z - Input.AttackerLocation.Z;

	const double Speed = Velocity.Size();
	Out.ExtrapolationCm = ExtrapolationDistance(Speed, Params.ContactSec, Params.NetLeadSec, Input.VictimBrakingCmPerSec2);
	const FVector2D Direction = Speed > UE_DOUBLE_SMALL_NUMBER ? Velocity / Speed : FVector2D::ZeroVector;
	const FVector2D AtContact = Victim + Direction * Out.ExtrapolationCm;
	Out.VictimAtContact = FVector(AtContact.X, AtContact.Y, Input.VictimLocation.Z);

	const FVector2D ToContact = AtContact - Attacker;
	Out.WarpTranslationCm = ToContact.Size() - 2.0 * Params.CapsuleRadiusCm - Params.ContactGapCm;
	Out.WarpTranslationEffCm = FMath::Max(Out.WarpTranslationCm, 0.0);

	const double TargetYawDeg = ToContact.IsNearlyZero() ? Input.AttackerFacingYawDeg : FMath::RadiansToDegrees(FMath::Atan2(ToContact.Y, ToContact.X));
	Out.BearingDeg = FRotator::NormalizeAxis(TargetYawDeg - Input.AttackerFacingYawDeg);

	Out.bRangeOk = Out.DistanceCm - Params.CapsuleRadiusCm <= Params.RangeCm + RangeEpsilonCm;
	Out.bVerticalOk = FMath::Abs(Out.DeltaZCm) <= Params.VerticalReachCm;
	Out.bWarpOk = Out.WarpTranslationEffCm <= Params.WarpLimitCm;
	Out.bBearingOk = FMath::Abs(Out.BearingDeg) <= Params.BearingLimitDeg;
	Out.bInReach = Out.bRangeOk && Out.bVerticalOk && Out.bWarpOk && Out.bBearingOk;
	return Out;
}
