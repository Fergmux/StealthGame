// Story oc-001 (production/epics/objectives-chain/story-001-objectives-tuning-rows-and-invariants.md): the Objectives
// Chain rows I-OC1, I-OC2 and I-OC3 (GDD objectives-chain.md Tuning Knobs and F4; ADR-0020 section 8). Registered in the
// TDC table (StealthTuningInvariants.cpp); see Server/StealthObjectivesInvariants.h.

#include "Server/StealthObjectivesInvariants.h"

#include "Server/StealthServerTuning.h"

namespace ObjectivesInvariantsLocal
{
	/** A >= B within the tolerance. */
	bool AtLeast(double A, double B) { return A + StealthObjectivesInvariants::CompareToleranceAbs >= B; }

	/** A <= B within the tolerance. */
	bool AtMost(double A, double B) { return A <= B + StealthObjectivesInvariants::CompareToleranceAbs; }
}

namespace StealthObjectivesInvariants
{
	using ObjectivesInvariantsLocal::AtLeast;
	using ObjectivesInvariantsLocal::AtMost;

	double EffectiveWindowSec(const FStealthServerTuning& V)
	{
		const FObjectivesTuning_Server& S = V.Objectives_Server;
		return (static_cast<double>(S.RivalSignalDelayMaxSec) - static_cast<double>(S.RivalSignalDelayMinSec)) + static_cast<double>(S.RivalSignalReleaseQuantumSec);
	}

	double SignalLikelihoodRatio(double Window, double ReleaseRatePerSec)
	{
		if (Window <= 0.0 || ReleaseRatePerSec <= 0.0)
		{
			return TNumericLimits<double>::Max();
		}
		return 1.0 / (1.0 - FMath::Exp(-ReleaseRatePerSec * Window));
	}

	ETuningCheck CheckIOc1(const FStealthServerTuning& V)
	{
		const double Floor = static_cast<double>(V.Crowd_Server.CrowdAmbientTouchMaxSec) + InteractOverTouchMarginSec;
		return AtLeast(static_cast<double>(V.Objectives_Public.ObjectiveInteractSec), Floor) ? ETuningCheck::Pass : ETuningCheck::Warn;
	}

	ETuningCheck CheckIOc2(const FStealthServerTuning& V)
	{
		const FObjectivesTuning_Server& S = V.Objectives_Server;
		const bool bSpreadOk = AtLeast(static_cast<double>(S.RivalSignalDelayMaxSec), static_cast<double>(S.RivalSignalDelayMinSec) + DelaySpreadMinSec);
		const double Window = EffectiveWindowSec(V);
		if (!bSpreadOk || !AtLeast(Window, WindowRejectBelowSec))
		{
			return ETuningCheck::Reject;
		}
		return AtLeast(Window, WindowWarnBelowSec) ? ETuningCheck::Pass : ETuningCheck::Warn;
	}

	ETuningCheck CheckIOc3(const FStealthServerTuning& V)
	{
		const double Cue = static_cast<double>(V.Objectives_Public.TendCueDelaySec);
		const bool bAfterDecoys = AtLeast(Cue + CueReactionAllowanceSec, static_cast<double>(V.Crowd_Server.CrowdDecoyEarlyReleaseMinSec));
		const bool bBeforeComplete = AtMost(Cue, static_cast<double>(V.Objectives_Public.ObjectiveInteractSec) - CueBeforeCompleteSec);
		return bAfterDecoys && bBeforeComplete ? ETuningCheck::Pass : ETuningCheck::Reject;
	}
}
