// Story tdc-047: the F-3 derived design indicators (see Server/StealthTuningDerived.h).

#include "Server/StealthTuningDerived.h"

#include "Server/StealthServerTuning.h"

#include <cmath>

namespace
{
	/** Numerator / Denominator, or 0 when the denominator is not positive (unclamped input only). */
	double SafeRatio(double Numerator, double Denominator)
	{
		return Denominator > 0.0 ? Numerator / Denominator : 0.0;
	}
}

FTuningDerived FTuningDerived::Compute(const FStealthServerTuning& Values)
{
	const double TeamSize = static_cast<double>(Values.Round_Public.TeamSize);
	const double NpcCount = static_cast<double>(Values.Crowd_Server.NpcCount);
	const double StrikeLimit = static_cast<double>(Values.Penalty_Public.NpcKillStrikeLimit);
	const double Duration = static_cast<double>(Values.Assassination_Public.DefenceDurationSec);
	const double Cooldown = static_cast<double>(Values.Assassination_Public.DefenceCooldownSec);

	FTuningDerived Derived;
	Derived.CrowdRatio = SafeRatio(NpcCount, 2.0 * TeamSize);
	Derived.DutyCycle = SafeRatio(Duration, Cooldown);
	Derived.P = SafeRatio(TeamSize, TeamSize + NpcCount);
	Derived.BlindGuessSuccess = 1.0 - std::pow(1.0 - Derived.P, StrikeLimit);
	Derived.ExpectedBlindKills = SafeRatio(StrikeLimit * Derived.P, 1.0 - Derived.P);
	Derived.MissToHit = SafeRatio(NpcCount, TeamSize);
	return Derived;
}
