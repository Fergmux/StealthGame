// Story cnb-022 (T5 bush red herring; GDD CNB-R7 bush bullet, R16 T5, AC-CNB-15; ADR-0017). When a bush entry is due
// the NPC steps into the shade: the nearest bush with an open side within 10 m of its current leg, chosen without
// reading occupancy, occupants or any pawn's position (the site query is geometry only), through the one Hide path
// after the dry run. The stay is drawn at entry from the uncapped log-normal (CrowdBushDwellMedianSec,
// CrowdBushDwellSigma): it is the NPC's own choice of when to leave; nothing times it out. It obeys the same
// BushReenterCooldownSec as a human and leaves by the side that faces where it walks next. With no bushes in the world
// (Concealment absent) the bush clock never runs.

#include "Server/CrowdAgent.h"

#include "Server/StealthTuningSubstructs.h"
#include "View/StealthTuningPublicSubstructs.h"

namespace CrowdBushLocal
{
	/** The bush entry is reached within this distance (cm). */
	constexpr double EntryReachCm = 80.0;
}

bool FCrowdAgent::StartBushWalk(double Now, const FCrowdPerceivedPawn& Self, ICrowdAgentWorld& World)
{
	using namespace CrowdHerringConstants;
	// Only on a leg (the bush is near "its current leg"), and never inside the shared re-enter cooldown.
	if (Activity != ECrowdActivity::Routing || !Follower.IsOnLeg() || Now - LastBushExit < World.GetActionTimings().BushReenterCooldownSec)
	{
		return false;
	}
	TArray<FVector, TInlineAllocator<16>> Path;
	for (const FVector& Corner : Follower.GetCorners())
	{
		Path.Add(Corner);
	}
	FCrowdBushSite Site;
	if (!World.FindBush(Self.Location, Path, BushSearchCm, Site))
	{
		return false; // none near this leg: the entry waits for a leg that passes one
	}
	if (MustDeferTell(ECrowdTell::Bush, Now, World))
	{
		return false; // waits until no different tell was shown within the gap
	}
	InProgressAcceptedAt = PendingSince[static_cast<int32>(ECrowdTell::Bush)];
	PendingSince[static_cast<int32>(ECrowdTell::Bush)] = -1.0;
	BushSite = Site;
	BeginLegTo(Now, Self, World, Site.Entry, true);
	Mode = EHerringMode::BushWalk;
	return true;
}

void FCrowdAgent::ArriveAtBush(double Now, const FCrowdPerceivedPawn& Self, ICrowdAgentWorld& World, ICrowdAgentOutput& Output)
{
	const FCrowdTuning_Server& Tuning = World.GetCrowdTuning();
	FHideCommand Enter = FHideCommand::Enter(BushSite.BushId);
	if (World.GetTellSampler().IsInShownGap(Ordinal, ECrowdTell::Bush, Now, Tuning) || !Output.DryRun(EActionType::HideEnter, nullptr))
	{
		RequeueInProgress(ECrowdTell::Bush);
		Mode = EHerringMode::None;
		BeginPause(Now, Self, World);
		return;
	}
	Output.Hide(Enter);
	TryPerformTell(ECrowdTell::Bush, Now, World);
	// The stay is the NPC's own choice, drawn now from the uncapped log-normal.
	const double Stay = World.GetDraws().DrawLogNormal(Ordinal, ECrowdDecision::BushStay, FMath::Loge(FMath::Max(static_cast<double>(Tuning.CrowdBushDwellMedianSec), 0.01)),
		Tuning.CrowdBushDwellSigma);
	BushExitAt = Now + Stay;
	Mode = EHerringMode::BushHidden;
	SetActivity(ECrowdActivity::Hiding, World);
	if (Observer != nullptr)
	{
		Observer->OnBushEnter(Ordinal, Now, BushSite.BushId, Stay);
	}
}

void FCrowdAgent::TickBush(double Now, const FCrowdPerceivedPawn& Self, ICrowdAgentWorld& World, ICrowdAgentOutput& Output)
{
	if (Mode == EHerringMode::BushWalk)
	{
		if (FVector::Dist2D(Self.Location, BushSite.Entry) <= CrowdBushLocal::EntryReachCm)
		{
			Follower.Stop();
			ArriveAtBush(Now, Self, World, Output);
		}
		return;
	}
	if (Mode != EHerringMode::BushHidden || Now < BushExitAt)
	{
		return;
	}
	// Leave by the open side that faces the next leg (the exit-side rule a human follows: out where one walks on).
	FVector Next;
	const FCrowdTuning_Server& Tuning = World.GetCrowdTuning();
	const double LegCm = World.GetDraws().DrawUniform(Ordinal, ECrowdDecision::LegLength, Tuning.CrowdLegMinCm, Tuning.CrowdLegMaxCm);
	// A pawn released while concealed (story cnb-029) has no site record: it leaves from where it stands.
	const FVector From = BushSite.SidePoints.Num() > 0 ? BushSite.Center : Self.Location;
	const bool bHasNext = World.PickLegGoal(Ordinal, From, LegCm, Next);
	uint8 Side = 0;
	double Best = -2.0;
	for (int32 Index = 0; Index < BushSite.SidePoints.Num(); ++Index)
	{
		const FVector Out = (BushSite.SidePoints[Index] - From).GetSafeNormal2D();
		const double Score = bHasNext ? FVector::DotProduct(Out, (Next - From).GetSafeNormal2D()) : -static_cast<double>(Index);
		if (Score > Best)
		{
			Best = Score;
			Side = static_cast<uint8>(Index);
		}
	}
	Output.Hide(FHideCommand::Exit(Side));
	LastBushExit = Now;
	Mode = EHerringMode::None;
	if (Observer != nullptr)
	{
		Observer->OnBushExit(Ordinal, Now, BushSite.BushId);
	}
	if (bHasNext)
	{
		BeginLegTo(Now, Self, World, Next, false);
	}
	else
	{
		BeginLeg(Now, Self, World, -1.0);
	}
}
