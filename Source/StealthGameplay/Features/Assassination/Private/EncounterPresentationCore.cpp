// Stories asn-018 / asn-020: the state-derived encounter presentation timeline (ADR-0018 section 5.1). See
// EncounterPresentationCore.h.

#include "View/EncounterPresentationCore.h"

#include "StrikeVariantCore.h"
#include "View/StealthTuningPublicSubstructs.h"

const TCHAR* LexEncounterClip(EEncounterClip Clip)
{
	switch (Clip)
	{
	case EEncounterClip::Strike_F: return TEXT("Strike_F");
	case EEncounterClip::Strike_L60: return TEXT("Strike_L60");
	case EEncounterClip::Strike_R60: return TEXT("Strike_R60");
	case EEncounterClip::Counter: return TEXT("Counter");
	case EEncounterClip::DeathFall: return TEXT("Death_Fall");
	default: return TEXT("None");
	}
}

FEncounterPresentationParams FEncounterPresentationParams::FromPublic(const FAssassinationTuning_Public& Public)
{
	FEncounterPresentationParams Out;
	Out.StrikeContactSec = Public.StrikeContactSec;
	Out.AttackAttemptLockoutSec = Public.AttackAttemptLockoutSec;
	Out.CounterDurationSec = Public.CounterDurationSec;
	Out.DeathFallSec = Public.DeathFallSec;
	Out.BodyLingerSec = Public.BodyLingerSec;
	Out.BodyRemovalGraceSec = Public.BodyRemovalGraceSec;
	Out.StrikeWarpYawLimitDeg = Public.StrikeWarpYawLimitDeg;
	Out.StrikeWarpLimitCm = Public.StrikeWarpLimitCm;
	Out.StrikeContactGapCm = Public.StrikeContactGapCm;
	return Out;
}

EEncounterClip FEncounterPresentationCore::StrikeClipOf(uint8 VariantBits)
{
	switch (FStrikeVariantCore::Entry(VariantBits))
	{
	case StrikeVariant::Strike_L60: return EEncounterClip::Strike_L60;
	case StrikeVariant::Strike_R60: return EEncounterClip::Strike_R60;
	default: return EEncounterClip::Strike_F;
	}
}

double FEncounterPresentationCore::BodyPhaseStartSec(const FEncounterPresentationParams& Params, EEncounterBodyPhase Phase)
{
	switch (Phase)
	{
	case EEncounterBodyPhase::Lingering: return Params.DeathFallSec;
	case EEncounterBodyPhase::Removal: return Params.DeathFallSec + Params.BodyLingerSec;
	case EEncounterBodyPhase::Cut: return Params.DeathFallSec + Params.BodyLingerSec + Params.BodyRemovalGraceSec;
	default: return 0.0;
	}
}

namespace EncounterPresentationLocal
{
	/** A small tolerance in server seconds (the queue's 1 ms due epsilon): a time "reached" within it counts as reached. */
	constexpr double DueEpsilon = 0.001;

	void SetDue(FEncounterPresentationPlan& Plan, double Due, double Now)
	{
		if (Due > Now + DueEpsilon && (Plan.NextDueServerTime < 0.0 || Due < Plan.NextDueServerTime))
		{
			Plan.NextDueServerTime = Due;
		}
	}

	/** The strike timeline (plan fields Clip ... WarpWindowEndSec). */
	void PlanStrike(const FEncounterPresentationParams& P, const FEncounterClipTimings& C, const FEncounterPresentationInput& In, FEncounterPresentationPlan& Plan)
	{
		const FActionState& A = In.Action;
		if (A.ContactServerTime < 0.0 || A.StartServerTime < 0.0)
		{
			return;
		}
		const double Lead = FMath::Max(0.01, FMath::Min(P.StrikeContactSec, A.ContactServerTime - A.StartServerTime));
		double SegStart = A.ContactServerTime - Lead; // = Start, or Start + BushStrikeEmergeSec with the preface
		double Contact = A.ContactServerTime;
		double End = FMath::Max(A.StartServerTime + P.AttackAttemptLockoutSec, Contact + 0.01);
		if (!In.bClockSynced && In.ActionReceiptNow > SegStart)
		{
			const double Shift = In.ActionReceiptNow - SegStart;
			SegStart += Shift;
			Contact += Shift;
			End += Shift;
			Plan.bUnsyncedFallback = true;
		}
		Plan.Clip = FEncounterPresentationCore::StrikeClipOf(A.Variant);
		Plan.InstanceKey = A.Seq;
		Plan.WarpWindowEndSec = C.StrikeClipContactSec;
		Plan.BlendInSec = P.MaxCrossFadeSec;
		const double ApproachRate = C.StrikeClipContactSec / Lead;
		const double RecoveryRate = FMath::Max(0.0, C.StrikeLengthSec - C.StrikeClipContactSec) / (End - Contact);
		const double Now = In.Now;
		if (Now < SegStart)
		{
			Plan.Position = (Now - SegStart) * ApproachRate; // negative: still to start
			Plan.PlayRate = ApproachRate;
			Plan.bNeedsTick = true;
			SetDue(Plan, SegStart, Now);
		}
		else if (Now < Contact)
		{
			Plan.Position = (Now - SegStart) * ApproachRate;
			Plan.PlayRate = ApproachRate;
			Plan.bNeedsTick = true;
			SetDue(Plan, Contact, Now);
		}
		else if (Now < End)
		{
			Plan.Position = C.StrikeClipContactSec + (Now - Contact) * RecoveryRate;
			Plan.PlayRate = RecoveryRate;
			Plan.bNeedsTick = true;
			SetDue(Plan, End, Now);
		}
		else
		{
			Plan.Position = C.StrikeLengthSec;
			Plan.PlayRate = RecoveryRate;
			Plan.bClipEnded = true;
		}
		const double FirstSeen = In.ActionReceiptNow >= 0.0 ? In.ActionReceiptNow : Now;
		Plan.bLate = FirstSeen > SegStart + DueEpsilon;
	}

	/** The counter timeline: starts at the reversed strike's contact, never on receipt. */
	void PlanCounter(const FEncounterPresentationParams& P, const FEncounterClipTimings& C, const FEncounterPresentationInput& In, FEncounterPresentationPlan& Plan)
	{
		const FActionState& A = In.Action;
		double Origin = A.ContactServerTime >= 0.0 ? A.ContactServerTime : A.StartServerTime;
		if (Origin < 0.0)
		{
			return;
		}
		if (!In.bClockSynced && In.ActionReceiptNow > Origin)
		{
			Origin = In.ActionReceiptNow;
			Plan.bUnsyncedFallback = true;
		}
		const double Duration = FMath::Max(0.01, P.CounterDurationSec);
		const double Rate = C.CounterLengthSec / Duration;
		Plan.Clip = EEncounterClip::Counter;
		Plan.InstanceKey = A.Seq;
		Plan.PlayRate = Rate;
		Plan.BlendInSec = P.MaxCrossFadeSec;
		Plan.Position = (In.Now - Origin) * Rate;
		if (In.Now < Origin + Duration)
		{
			Plan.bNeedsTick = true;
			SetDue(Plan, In.Now < Origin ? Origin : Origin + Duration, In.Now);
		}
		else
		{
			Plan.Position = C.CounterLengthSec;
			Plan.bClipEnded = true;
		}
		const double FirstSeen = In.ActionReceiptNow >= 0.0 ? In.ActionReceiptNow : In.Now;
		Plan.bLate = FirstSeen > Origin + DueEpsilon;
	}
}

FEncounterPresentationPlan FEncounterPresentationCore::Evaluate(const FEncounterPresentationParams& Params, const FEncounterClipTimings& Clips, const FEncounterPresentationInput& Input)
{
	using namespace EncounterPresentationLocal;
	FEncounterPresentationPlan Plan;
	const double Now = Input.Now;

	if (Input.Death.DeathServerTime >= 0.0)
	{
		double Death = Input.Death.DeathServerTime;
		const double FirstSeen = Input.DeathReceiptNow >= 0.0 ? Input.DeathReceiptNow : Now;
		bool bUnsynced = false;
		if (!Input.bClockSynced && FirstSeen > Death)
		{
			Death = FirstSeen;
			bUnsynced = true;
		}
		Plan.bNeverShow = FirstSeen >= Input.Death.DeathServerTime + BodyPhaseStartSec(Params, EEncounterBodyPhase::Removal) - DueEpsilon;
		if (Now + DueEpsilon >= Death)
		{
			const double Since = Now - Death;
			const double Fall = FMath::Max(0.01, Params.DeathFallSec);
			Plan.Clip = EEncounterClip::DeathFall;
			Plan.InstanceKey = 0;
			Plan.PlayRate = Clips.FallLengthSec / Fall;
			Plan.Position = FMath::Clamp(Since, 0.0, Fall) * Plan.PlayRate;
			Plan.bClipEnded = Since >= Fall;
			Plan.BlendInSec = Params.FallEntryBlendSec;
			Plan.bLate = FirstSeen > Death + DueEpsilon;
			Plan.bDeathPresented = true;
			Plan.bUnsyncedFallback = bUnsynced;
			Plan.bCollisionOff = Since + DueEpsilon >= Params.DeathFallSec;
			const double Linger = BodyPhaseStartSec(Params, EEncounterBodyPhase::Removal);
			const double Cut = BodyPhaseStartSec(Params, EEncounterBodyPhase::Cut);
			Plan.BodyPhase = Since + DueEpsilon >= Cut ? EEncounterBodyPhase::Cut
				: (Since + DueEpsilon >= Linger ? EEncounterBodyPhase::Removal
				: (Since + DueEpsilon >= Params.DeathFallSec ? EEncounterBodyPhase::Lingering : EEncounterBodyPhase::Falling));
			Plan.bNeedsTick = Plan.BodyPhase != EEncounterBodyPhase::Cut;
			SetDue(Plan, Death + Params.DeathFallSec, Now);
			SetDue(Plan, Death + Linger, Now);
			SetDue(Plan, Death + Cut, Now);
			return Plan;
		}
		// The death is written but not yet due here: the current pose (the reversed attacker's lunge) runs to it.
		Plan.bNeedsTick = true;
		SetDue(Plan, Death, Now);
	}

	FEncounterPresentationPlan Action;
	switch (Input.Action.Pose)
	{
	case EActionPose::Strike: PlanStrike(Params, Clips, Input, Action); break;
	case EActionPose::Counter: PlanCounter(Params, Clips, Input, Action); break;
	default: break;
	}
	if (Plan.bNeedsTick)
	{
		Action.bNeedsTick = true;
		SetDue(Action, Plan.NextDueServerTime, Now);
	}
	return Action;
}
