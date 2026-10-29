// Story con-001 (production/epics/concealment-bushes/story-001-concealment-tuning-keys-and-timing-validator.md): the F3
// timing chain (GDD concealment-bushes.md F3) as six TDC Check rows I-CON-F3a..f. See Server/StealthConcealmentInvariants.h.

#include "Server/StealthConcealmentInvariants.h"

#include "Server/StealthServerTuning.h"
#include "Server/StealthTuningConstants.h"

namespace ConcealmentInvariantsLocal
{
	/** A >= B within the tolerance. */
	bool AtLeast(double A, double B) { return A + StealthConcealmentInvariants::CompareToleranceAbs >= B; }

	/** A <= B within the tolerance. */
	bool AtMost(double A, double B) { return A <= B + StealthConcealmentInvariants::CompareToleranceAbs; }

	/** A < B with a margin above the tolerance. */
	bool Below(double A, double B) { return A + StealthConcealmentInvariants::CompareToleranceAbs < B; }
}

FConcealmentF3Inputs FConcealmentF3Inputs::FromValues(const FStealthServerTuning& Values)
{
	FConcealmentF3Inputs Out;
	Out.ConcealCullDelayMs = StealthTuningPendingOwnerKeys::ConcealCullDelayMs;
	Out.L99Ms = StealthConcealmentInvariants::LeakNetProfileL99Ms;
	Out.ReopenMs = StealthConcealmentInvariants::ChannelReopenMs;
	Out.PawnNetUpdateHz = static_cast<double>(Values.Locomotion_Server.PawnNetUpdateHz);
	Out.WalkSpeedCmPerSec = static_cast<double>(Values.Locomotion_Public.WalkSpeedCmPerSec);
	Out.CapsuleRadiusCm = StealthTuningConstants::SclCapsuleRadiusCm;
	Out.StrikeContactSec = static_cast<double>(Values.Assassination_Public.StrikeContactSec);
	return Out;
}

namespace StealthConcealmentInvariants
{
	using ConcealmentInvariantsLocal::AtLeast;
	using ConcealmentInvariantsLocal::AtMost;
	using ConcealmentInvariantsLocal::Below;

	double WarpMeanSpeedCmPerSec(const FStealthServerTuning& V, const FConcealmentF3Inputs& In)
	{
		const double Swap = static_cast<double>(V.Concealment_Public.BushEnterSwapSec);
		return Swap > 0.0 ? (static_cast<double>(V.Concealment_Public.BushEntryReachCm) + In.CapsuleRadiusCm) / Swap : TNumericLimits<double>::Max();
	}

	uint8 EvaluateF3(const FStealthServerTuning& V, const FConcealmentF3Inputs& In)
	{
		const FConcealmentTuning_Public& C = V.Concealment_Public;
		const double EnterDur = C.BushEnterDurSec;
		const double Swap = C.BushEnterSwapSec;
		const double ExitDur = C.BushExitDurSec;
		const double Cross = C.BushExitCrossSec;
		const double Emerge = C.BushStrikeEmergeSec;
		const double Uncull = In.UncullSec();
		uint8 Violated = 0;
		Violated |= AtLeast(EnterDur, Swap + In.ConcealCullDelayMs / 1000.0) ? 0 : F3a;
		Violated |= AtLeast(Cross, Uncull) ? 0 : F3b;
		Violated |= AtLeast(Emerge, Uncull) ? 0 : F3c;
		Violated |= AtMost(WarpMeanSpeedCmPerSec(V, In), WarpSpeedWalkMult * In.WalkSpeedCmPerSec) ? 0 : F3d;
		Violated |= Below(Swap, EnterDur) && Below(Cross, ExitDur) ? 0 : F3e;
		Violated |= AtMost(In.StrikeContactSec + Emerge, StrikePlusEmergeMaxSec) ? 0 : F3f;
		return Violated;
	}

	FString DescribeF3(uint8 Violated)
	{
		FString Out;
		const TCHAR Letters[] = { TEXT('a'), TEXT('b'), TEXT('c'), TEXT('d'), TEXT('e'), TEXT('f') };
		for (int32 Bit = 0; Bit < 6; ++Bit)
		{
			if ((Violated & (1 << Bit)) != 0)
			{
				Out.AppendChar(Letters[Bit]);
			}
		}
		return Out;
	}
}
