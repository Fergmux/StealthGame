// Story cnb-029 (ReleaseToCrowd; GDD CNB-R10.4, R9.1 exile exit, AC-CNB-24; ADR-0016 sections 7 and 8). A released
// participant pawn keeps its restricted driver until its strike lockout ends (RestrictedUntilServerTime), then joins the
// full catalogue at its next leg boundary: nothing visible changes at the release instant. An exile body
// (bExitAtNextScheduledSlot) then takes the next scheduled city-wide exit slot (the slot's chosen NPC simply stays) and
// reports AlleyExit until absorbed. A pawn released while concealed leaves the bush after a stay drawn from the NPC stay
// distribution conditioned on the time it has already spent there (uncapped).

#include "Server/CrowdAgent.h"

#include "Server/CrowdDraws.h"
#include "Server/StealthTuningSubstructs.h"

#include <cmath>

double FCrowdAgent::ConditionedStaySec(double U, double ElapsedSec, double MedianSec, double Sigma)
{
	// The log-normal stay T (median M, sigma s) conditioned on T > t0: draw F(T) uniformly in (F(t0), 1).
	const double Mu = FMath::Loge(FMath::Max(MedianSec, 0.01));
	const double SafeSigma = FMath::Max(Sigma, 1e-6);
	const double Z0 = ElapsedSec > 0.0 ? (FMath::Loge(ElapsedSec) - Mu) / SafeSigma : -40.0;
	const double F0 = 0.5 * FMath::Clamp(1.0 + std::erf(Z0 / 1.4142135623730951), 0.0, 2.0);
	const double Uc = FMath::Clamp(F0 + U * (1.0 - F0), 1e-12, 1.0 - 1e-12);
	const double Total = FMath::Exp(Mu + SafeSigma * FCrowdDraws::InvNormalAS241(Uc));
	return FMath::Max(Total - ElapsedSec, 0.0);
}

void FCrowdAgent::ScheduleAdoption(double Now, double RestrictedUntil, bool bExitAtNextSlot, bool bConcealed, double ConcealedSec, ICrowdAgentWorld& World)
{
	bAdoptPending = true;
	PromoteAt = FMath::Max(Now, RestrictedUntil);
	bExileExitPending = bExitAtNextSlot;
	if (bConcealed)
	{
		// In a bush: the release changes nothing visible; the NPC's own stay, conditioned on the time already spent.
		const FCrowdTuning_Server& Tuning = World.GetCrowdTuning();
		const double U = World.GetDraws().DrawUnit(Ordinal, ECrowdDecision::BushStay);
		ReleasedConcealedStaySec = ConditionedStaySec(U, ConcealedSec, Tuning.CrowdBushDwellMedianSec, Tuning.CrowdBushDwellSigma);
		bReleasedConcealed = true;
	}
}

bool FCrowdAgent::TryPromote(double Now, const FCrowdPerceivedPawn& Self, ICrowdAgentWorld& World)
{
	if (!bAdoptPending || Driver != ECrowdDriverKind::Restricted || Now < PromoteAt || bHalted)
	{
		return false;
	}
	bAdoptPending = false;
	Driver = ECrowdDriverKind::Full;
	World.OnAgentPromoted(Ordinal);
	World.GetTellSampler().StartNpc(Ordinal, Now, World.GetCrowdTuning(), World.GetDraws());
	if (bReleasedConcealed)
	{
		// Still in the bush: it leaves when its conditioned stay ends (Hide(Exit) through the full sink).
		bReleasedConcealed = false;
		BushSite = FCrowdBushSite();
		BushExitAt = Now + ReleasedConcealedStaySec;
		Mode = EHerringMode::BushHidden;
		SetActivity(ECrowdActivity::Hiding, World);
	}
	if (bExileExitPending)
	{
		bExileExitPending = false;
		// The next scheduled city-wide exit slot is this body's (AlleyExit from now until it is absorbed).
		World.ClaimExitSlot(Ordinal);
		bAwaitingExitSlot = true;
	}
	return true;
}
