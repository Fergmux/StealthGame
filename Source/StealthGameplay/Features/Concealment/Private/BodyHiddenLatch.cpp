// Story con-019: the pure, fail-closed body-hidden latch. See View/BodyHiddenLatch.h.

#include "View/BodyHiddenLatch.h"

bool IsBushEmergeStrike(const FActionState& Action)
{
	return Action.Pose == EActionPose::Strike && (Action.Variant & StrikeVariant::BushPreface) != 0;
}

bool EvaluateBodyHidden(const FBodyHiddenInputs& In, const FConcealmentTuning_Public& Tuning)
{
	if (In.bIsOwner)
	{
		return false;
	}
	const FActionState& A = In.Action;
	const bool bDead = In.Death.DeathServerTime >= 0.0;
	if (In.Death.bDiedConcealed)
	{
		return true;
	}
	// Set rules.
	if (A.Pose == EActionPose::BushEnter)
	{
		const double Swap = A.StartServerTime + Tuning.BushEnterSwapSec;
		// Death before the commit cancels the pending swap: an ordinary visible kill (CON-R6b rule 4).
		const bool bDiedBeforeSwap = bDead && In.Death.DeathServerTime < Swap;
		if (!bDiedBeforeSwap && In.Now >= Swap)
		{
			return true;
		}
		if (bDiedBeforeSwap && !In.bAllyIsConcealed)
		{
			return false;
		}
	}
	if (In.bAllyIsConcealed)
	{
		return true;
	}
	if (A.Pose == EActionPose::BushExit)
	{
		// Before the crossing hidden; after it the crossing is the clearing evidence.
		return In.Now < A.StartServerTime + Tuning.BushExitCrossSec;
	}
	if (IsBushEmergeStrike(A))
	{
		return In.Now < A.StartServerTime + Tuning.BushStrikeEmergeSec;
	}
	if (A.Pose == EActionPose::BushEnter)
	{
		return In.bWasHidden && In.Now >= A.StartServerTime + Tuning.BushEnterSwapSec; // before the swap: visible
	}
	// No set rule holds and no crossing passed. A fresh actor with no concealment evidence at all clears; a running set
	// latch holds (pose None after the enter's End, a death pose, a plain strike); an unset one stays unset.
	return In.bFresh ? false : In.bWasHidden;
}
