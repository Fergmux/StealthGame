// Story cnb-024 (outcome-blind strike reactions R8; GDD CNB-R8, R7 chain gap; ADR-0016 section 5). A responder (picked
// by FCrowdStrikeFanOut: line of sight within CrowdStrikeReactRadiusCm, lowest keyed priority, at most
// CrowdStrikeResponderLimit) draws one reaction and its delay from the stimulus time: none, a glance at the strike point
// (never at the attacker, and no glance tracks the attacker afterwards), a pause facing the point, a reroute away at walk,
// a short panic run, or a startle brace. Panic runs and startle braces happen only within CrowdStartleRadiusCm; beyond it,
// inside the responder's chain gap, or when the pawn cannot move (a herring in progress, a conversation, the stand bound)
// they become glances. Panic runs count against the T1 budget and startle braces against T4 (the same tell records).
// Refractory CrowdReactRefractorySec per NPC. Nothing here reads the strike's outcome, the victim or any kind.

#include "Server/CrowdAgent.h"

#include "Server/StealthTuningSubstructs.h"
#include "View/StealthTuningPublicSubstructs.h"

bool FCrowdAgent::IsStrikeRefractory(double Now, const FCrowdTuning_Server& Tuning) const
{
	return Now - LastStrikeReaction < Tuning.CrowdReactRefractorySec;
}

void FCrowdAgent::OnStrikeStimulus(double StimulusTime, const FVector& Point, uint64 SourceKey, double DistanceCm, double Now, ICrowdAgentWorld& World)
{
	const FCrowdTuning_Server& Tuning = World.GetCrowdTuning();
	if (!bStarted || bHalted || Driver != ECrowdDriverKind::Full || IsStrikeRefractory(StimulusTime, Tuning))
	{
		return;
	}
	FCrowdDraws& Draws = World.GetDraws();
	LastStrikeReaction = StimulusTime;
	// The R8 shares (provisional constants registered as keys): none, glance, pause, reroute, panic run, startle brace.
	const double Shares[] = { Tuning.CrowdReactNonePct, Tuning.CrowdReactGlancePct, Tuning.CrowdReactPausePct, Tuning.CrowdReactReroutePct, Tuning.CrowdPanicRunPct,
		Tuning.CrowdStartleBracePct };
	double Total = 0.0;
	for (const double Share : Shares)
	{
		Total += FMath::Max(Share, 0.0);
	}
	const double U = Draws.DrawUnit(Ordinal, ECrowdDecision::ReactionKind) * FMath::Max(Total, 1e-9);
	ECrowdStrikeReaction Reaction = ECrowdStrikeReaction::StartleBrace;
	double Cumulative = 0.0;
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Shares); ++Index)
	{
		Cumulative += FMath::Max(Shares[Index], 0.0);
		if (U < Cumulative)
		{
			Reaction = static_cast<ECrowdStrikeReaction>(Index);
			break;
		}
	}
	// Panic runs and startle braces only within the startle radius; beyond it they are glances.
	if ((Reaction == ECrowdStrikeReaction::PanicRun || Reaction == ECrowdStrikeReaction::StartleBrace) && DistanceCm > Tuning.CrowdStartleRadiusCm)
	{
		Reaction = ECrowdStrikeReaction::Glance;
	}
	const double Delay = Draws.DrawUniform(Ordinal, ECrowdDecision::ReactionDelay, Tuning.CrowdReactDelayMinSec, Tuning.CrowdReactDelayMaxSec);
	PendingReaction = Reaction;
	StrikePoint = Point;
	StrikeSourceKey = SourceKey;
	StrikeStimulusTime = StimulusTime;
	StrikeDistanceCm = DistanceCm;
	// The glance-avoidance of the attacker runs for the refractory window from the stimulus (no tracking afterwards).
	AvoidGlanceKey = SourceKey;
	AvoidGlanceUntil = StimulusTime + Tuning.CrowdReactRefractorySec;
	DueList.Schedule(ECrowdDueKind::StrikeReaction, FMath::Max(StimulusTime + Delay, Now));
}

void FCrowdAgent::StartStrikeReaction(double Now, const FCrowdPerceivedPawn& Self, ICrowdAgentWorld& World, ICrowdAgentOutput& Output)
{
	using namespace CrowdHerringConstants;
	const FCrowdTuning_Server& Tuning = World.GetCrowdTuning();
	FCrowdDraws& Draws = World.GetDraws();
	ECrowdStrikeReaction Reaction = PendingReaction;
	const bool bCanMove = Mode == EHerringMode::None && (Activity == ECrowdActivity::Routing || Activity == ECrowdActivity::Pausing);
	if (Reaction == ECrowdStrikeReaction::PanicRun || Reaction == ECrowdStrikeReaction::StartleBrace)
	{
		// A reaction that would be a tell inside the responder's chain gap becomes a glance (CNB-R7).
		const ECrowdTell Tell = Reaction == ECrowdStrikeReaction::PanicRun ? ECrowdTell::Run : ECrowdTell::Brace;
		const bool bInGap = MustDeferTell(Tell, Now, World);
		const bool bCooldown = Reaction == ECrowdStrikeReaction::StartleBrace
			&& (Now - LastBraceTime < World.GetActionTimings().PrepareCooldownSec || !BraceFitsStretch(Now, World.GetActionTimings().PrepareCapSec, World));
		if (bInGap || bCooldown || !bCanMove)
		{
			if (bInGap)
			{
				World.GetTellSampler().NoteReactionDowngraded();
			}
			Reaction = ECrowdStrikeReaction::Glance;
		}
	}
	if ((Reaction == ECrowdStrikeReaction::Pause || Reaction == ECrowdStrikeReaction::Reroute) && !bCanMove)
	{
		Reaction = ECrowdStrikeReaction::Glance;
	}
	double Length = 0.0;
	switch (Reaction)
	{
	case ECrowdStrikeReaction::Glance:
		Length = Draws.DrawUniform(Ordinal, ECrowdDecision::ReactionLength, Tuning.CrowdReactGlanceMinSec, Tuning.CrowdReactGlanceMaxSec);
		break;
	case ECrowdStrikeReaction::Pause:
	{
		Length = Draws.DrawUniform(Ordinal, ECrowdDecision::ReactionLength, Tuning.CrowdReactPauseMinSec, Tuning.CrowdReactPauseMaxSec);
		// The stand bound: the pause ends inside CrowdMaxIdleSec even after a long rest (AC-CNB-13).
		const double Room = Tuning.CrowdMaxIdleSec - WatchdogLeadSec - World.GetStationaryAge(Ordinal, Now) - 0.1;
		if (Activity == ECrowdActivity::Pausing && Room < Length)
		{
			Length = Room;
			if (Length < Tuning.CrowdReactPauseMinSec * 0.5)
			{
				Reaction = ECrowdStrikeReaction::Glance;
				Length = Draws.DrawUniform(Ordinal, ECrowdDecision::ReactionLength, Tuning.CrowdReactGlanceMinSec, Tuning.CrowdReactGlanceMaxSec);
			}
		}
		break;
	}
	case ECrowdStrikeReaction::PanicRun:
		Length = Draws.DrawUniform(Ordinal, ECrowdDecision::PanicRunLength, FMath::Min(1.0, static_cast<double>(Tuning.CrowdPanicRunMaxSec)), Tuning.CrowdPanicRunMaxSec);
		break;
	default:
		break;
	}
	if (Observer != nullptr)
	{
		Observer->OnStrikeReaction(Ordinal, StrikeStimulusTime, Now, PendingReaction, Reaction, StrikeDistanceCm);
	}
	const FVector Away = (Self.Location - StrikePoint).GetSafeNormal2D().IsNearlyZero() ? -FRotator(0.0, Self.FacingYawDeg, 0.0).Vector()
		: (Self.Location - StrikePoint).GetSafeNormal2D();
	switch (Reaction)
	{
	case ECrowdStrikeReaction::Glance:
		// A look at the strike point (a fixed point: the glance never follows the attacker).
		GlanceKind = ECrowdGlanceKind::Reaction;
		GlancePoint = StrikePoint + FVector(0.0, 0.0, Self.EyeHeightCm);
		GlanceTargetKey = 0;
		GlancePitch = 0.0f;
		bGlanceActive = true;
		DueList.Schedule(ECrowdDueKind::GlanceEnd, Now + Length);
		if (Observer != nullptr)
		{
			Observer->OnGlance(Ordinal, Now, ECrowdGlanceKind::Reaction, 0, Length);
		}
		break;
	case ECrowdStrikeReaction::Pause:
		if (Activity == ECrowdActivity::Pausing)
		{
			EndPause(Now);
		}
		Follower.Stop();
		ReactEnd = Now + Length;
		Mode = EHerringMode::ReactPause;
		SetActivity(ECrowdActivity::Reacting, World);
		break;
	case ECrowdStrikeReaction::Reroute:
		LeaveRestForHerring(Now);
		BeginLegTo(Now, Self, World, Self.Location + Away * ReactionRerouteCm, false);
		break;
	case ECrowdStrikeReaction::PanicRun:
		StartRunLeg(Now, Self, World, true, Length, &Away);
		break;
	case ECrowdStrikeReaction::StartleBrace:
		BraceDueUntil = Now;
		bBraceIsStartle = true;
		PerformBrace(Now, Self, World, Output, true);
		bBraceIsStartle = false;
		break;
	default:
		break;
	}
}
