// Story cnb-019 (T1 runs) and the shared red-herring dispatch of stories cnb-019 to cnb-022 (GDD CNB-R7, F2, R16 T1;
// ADR-0016 sections 3 and 9). Every accepted tell of the sampler (story cnb-018) is queued here for its behaviour and
// performed through the commands a human uses; its visible moment is checked against the chain gap once more, so no
// NPC ever shows two different tells within CrowdTellChainGapSec (AC-CNB-31).

#include "Server/CrowdAgent.h"

#include "Server/StealthTuningSubstructs.h"
#include "View/Core/StealthLocomotionModel.h"
#include "View/StealthTuningPublicSubstructs.h"

bool FCrowdAgent::CanStartHerring() const
{
	return bStarted && !bHalted && Driver == ECrowdDriverKind::Full && Mode == EHerringMode::None
		&& (Activity == ECrowdActivity::Routing || Activity == ECrowdActivity::Pausing);
}

void FCrowdAgent::LeaveRestForHerring(double Now)
{
	if (Activity == ECrowdActivity::Pausing)
	{
		EndPause(Now);
	}
}

void FCrowdAgent::OnTellAccepted(ECrowdTell Tell, double Now, ICrowdAgentWorld& World)
{
	const int32 Index = static_cast<int32>(Tell);
	if (Observer != nullptr)
	{
		Observer->OnTellAccepted(Ordinal, Now, Tell);
	}
	if (Driver != ECrowdDriverKind::Full || bHalted)
	{
		DropPending(Tell, Now, World);
		return;
	}
	switch (Tell)
	{
	case ECrowdTell::HeadHold:
		bHeadHoldPending = true;
		PendingSince[Index] = Now;
		break;
	case ECrowdTell::Brace:
		// T4: the clock decides WHEN a brace is due; the next flinch trigger within the window performs it.
		if (BraceDueUntil < 0.0)
		{
			BraceDueUntil = Now + CrowdHerringConstants::BraceDueWindowSec;
			PendingSince[Index] = Now;
		}
		else
		{
			// Already due: the window keeps its start (one brace per window); this acceptance is not shown.
			World.GetTellSampler().NoteDropped(Ordinal, Tell);
			if (Observer != nullptr)
			{
				Observer->OnTellDropped(Ordinal, Now, Tell);
			}
		}
		break;
	default:
		if (PendingSince[Index] < 0.0)
		{
			PendingSince[Index] = Now;
		}
		break;
	}
}

ECrowdTell FCrowdAgent::TellInProgress() const
{
	switch (Mode)
	{
	case EHerringMode::DecoyWalk:
	case EHerringMode::DecoyBeginCheck:
		return ECrowdTell::DecoyTend;
	case EHerringMode::BushWalk:
		return ECrowdTell::Bush;
	case EHerringMode::TraverseApproach:
		return bTraversalClimb ? ECrowdTell::Climb : ECrowdTell::Vault;
	default:
		return ECrowdTell::Count;
	}
}

void FCrowdAgent::RequeueInProgress(ECrowdTell Tell)
{
	if (Tell != ECrowdTell::Count)
	{
		PendingSince[static_cast<int32>(Tell)] = InProgressAcceptedAt >= 0.0 ? InProgressAcceptedAt : 0.0;
	}
}

bool FCrowdAgent::MustDeferTell(ECrowdTell Tell, double Now, ICrowdAgentWorld& World) const
{
	const ECrowdTell InProgress = TellInProgress();
	return (InProgress != ECrowdTell::Count && InProgress != Tell) || World.GetTellSampler().IsInShownGap(Ordinal, Tell, Now, World.GetCrowdTuning());
}

bool FCrowdAgent::TryPerformTell(ECrowdTell Tell, double Now, ICrowdAgentWorld& World, bool bFromClock)
{
	FCrowdTellSampler& Tells = World.GetTellSampler();
	if (MustDeferTell(Tell, Now, World))
	{
		// A different tell was shown less than the chain gap ago, or one is on its way: this one waits (evidence
		// compounds, CNB-R7); it stays pending until its wait runs out.
		return false;
	}
	Tells.NotePerformed(Ordinal, Tell, Now, bFromClock);
	PendingSince[static_cast<int32>(Tell)] = -1.0;
	if (Observer != nullptr)
	{
		Observer->OnTellPerformed(Ordinal, Now, Tell);
	}
	return true;
}

void FCrowdAgent::DropPending(ECrowdTell Tell, double Now, ICrowdAgentWorld& World)
{
	const int32 Index = static_cast<int32>(Tell);
	PendingSince[Index] = -1.0;
	if (Tell == ECrowdTell::HeadHold)
	{
		bHeadHoldPending = false;
	}
	if (Tell == ECrowdTell::Brace)
	{
		BraceDueUntil = -1.0;
		BraceHoldPlanned = -1.0;
	}
	World.GetTellSampler().NoteDropped(Ordinal, Tell);
	if (Observer != nullptr)
	{
		Observer->OnTellDropped(Ordinal, Now, Tell);
	}
}

void FCrowdAgent::TickHerrings(double Now, const FCrowdPerceivedPawn& Self, ICrowdAgentWorld& World, ICrowdAgentOutput& Output)
{
	using namespace CrowdHerringConstants;
	if (!bStarted || bHalted || Driver != ECrowdDriverKind::Full)
	{
		return;
	}
	// A walk a herring owned was abandoned (blocked ladder, failed path): the herring ends with it.
	if (bLegAbandoned && (Mode == EHerringMode::RunLeg || Mode == EHerringMode::DecoyWalk || Mode == EHerringMode::BushWalk || Mode == EHerringMode::TraverseApproach))
	{
		// A walk toward a tell's visible moment was blocked: the tell waits for another chance.
		RequeueInProgress(TellInProgress());
		Mode = EHerringMode::None;
	}
	if (bLegAbandoned && Mode == EHerringMode::ExitWalk)
	{
		// The way into the alley is blocked: the exit is given up like a watched one.
		World.OnAmbientExitAbandoned(Ordinal, ExitSite);
		Mode = EHerringMode::None;
	}

	const float Dt = LastHerringTick >= 0.0 ? static_cast<float>(FMath::Max(Now - LastHerringTick, 0.0)) : 0.0f;
	LastHerringTick = Now;
	// Herrings in progress.
	switch (Mode)
	{
	case EHerringMode::ExitWalk:
	case EHerringMode::ExitWait:
		TickAmbientExit(Now, Dt, Self, World);
		break;
	case EHerringMode::TraverseApproach:
	case EHerringMode::Traversing:
		TickTraversal(Now, Self, World, Output);
		break;
	case EHerringMode::BraceHold:
		if (Now >= BraceEnd)
		{
			EndBrace(Now, Self, World, Output);
		}
		break;
	case EHerringMode::DecoyBeginCheck:
	case EHerringMode::DecoyHold:
		TickDecoy(Now, Self, World, Output);
		break;
	case EHerringMode::BushWalk:
	case EHerringMode::BushHidden:
		TickBush(Now, Self, World, Output);
		break;
	case EHerringMode::ReactPause:
		if (Now >= ReactEnd)
		{
			Mode = EHerringMode::None;
			BeginLeg(Now, Self, World, -1.0);
		}
		break;
	default:
		break;
	}
	// R8: a strike reaction whose delay has run.
	if (bStrikeReactionDue)
	{
		bStrikeReactionDue = false;
		StartStrikeReaction(Now, Self, World, Output);
	}

	// Pending tells that waited too long are dropped (counted, never shown late next to another tell).
	for (int32 Index = 0; Index < static_cast<int32>(ECrowdTell::Count); ++Index)
	{
		const ECrowdTell Tell = static_cast<ECrowdTell>(Index);
		if (PendingSince[Index] < 0.0 || Tell == ECrowdTell::HeadHold || Tell == ECrowdTell::Brace)
		{
			continue;
		}
		const double MaxWait = Tell == ECrowdTell::Bush ? PendingBushMaxSec
			: (Tell == ECrowdTell::Vault || Tell == ECrowdTell::Climb ? PendingTraversalMaxSec : PendingTellMaxSec);
		if (Now - PendingSince[Index] > MaxWait)
		{
			DropPending(Tell, Now, World);
		}
	}
	if (IsPending(ECrowdTell::HeadHold) && Now - PendingSince[static_cast<int32>(ECrowdTell::HeadHold)] > PendingTellMaxSec)
	{
		DropPending(ECrowdTell::HeadHold, Now, World);
	}

	// T4: a due brace waits for a flinch trigger, then is an ambient startle (never caused by a trigger alone).
	if (BraceDueUntil >= 0.0 && Mode == EHerringMode::None && CanStartHerring())
	{
		// A trigger performs the brace only inside its window; after the window it is the ambient startle.
		const bool bTriggered = Now < BraceDueUntil && FlinchTriggered(Self, World);
		if (bTriggered || Now >= BraceDueUntil)
		{
			PerformBrace(Now, Self, World, Output, bTriggered);
		}
	}

	if (!CanStartHerring())
	{
		return;
	}
	// One herring at a time, in a fixed order (each was accepted on its own clock).
	if (IsPending(ECrowdTell::Run))
	{
		StartRunLeg(Now, Self, World, false, -1.0);
	}
	else if (IsPending(ECrowdTell::Reversal))
	{
		StartReversal(Now, Self, World);
	}
	else if (IsPending(ECrowdTell::Vault) || IsPending(ECrowdTell::Climb))
	{
		StartTraversal(Now, Self, World, !IsPending(ECrowdTell::Vault));
	}
	else if (IsPending(ECrowdTell::DecoyTend))
	{
		const double Accepted = PendingSince[static_cast<int32>(ECrowdTell::DecoyTend)];
		if (ChooseCourt(Now, Self, World, INDEX_NONE))
		{
			InProgressAcceptedAt = Accepted;
			PendingSince[static_cast<int32>(ECrowdTell::DecoyTend)] = -1.0;
		}
	}
	else if (IsPending(ECrowdTell::Bush))
	{
		StartBushWalk(Now, Self, World);
	}
}

bool FCrowdAgent::OnHerringArrival(double Now, const FCrowdPerceivedPawn& Self, ICrowdAgentWorld& World, ICrowdAgentOutput& Output)
{
	switch (Mode)
	{
	case EHerringMode::RunLeg:
		// The bout ended by ordinary braking at the leg's end: an ordinary pause follows.
		Mode = EHerringMode::None;
		return false;
	case EHerringMode::ReversalStop:
		// At rest after the stop: the SCL pivot and the start back along the last leg, at the leg's pace.
		Mode = EHerringMode::None;
		BeginLegTo(Now, Self, World, ReversalTarget, true);
		return true;
	case EHerringMode::DecoyWalk:
		ArriveAtCourt(Now, Self, World, Output);
		return true;
	case EHerringMode::BushWalk:
		ArriveAtBush(Now, Self, World, Output);
		return true;
	case EHerringMode::TraverseApproach:
		// The leg ended before the crossing: the tell waits for another coping.
		RequeueInProgress(TellInProgress());
		Mode = EHerringMode::None;
		return false;
	case EHerringMode::Traversing:
		Mode = EHerringMode::None;
		return false;
	case EHerringMode::ExitWalk:
		// In the alley: wait there for the clear check (or give up when watched).
		Mode = EHerringMode::ExitWait;
		return true;
	case EHerringMode::ExitWait:
	case EHerringMode::ReactPause:
	case EHerringMode::BraceHold:
	case EHerringMode::DecoyBeginCheck:
	case EHerringMode::DecoyHold:
	case EHerringMode::BushHidden:
		// The stop of a planted brace, a tend or a bush entry: the herring itself decides what comes next.
		return true;
	default:
		Mode = EHerringMode::None;
		return false;
	}
}

void FCrowdAgent::StartRunLeg(double Now, const FCrowdPerceivedPawn& Self, ICrowdAgentWorld& World, bool bPanic, double BoutSec, const FVector* AwayDir)
{
	using namespace CrowdHerringConstants;
	const FLocomotionTuning_Public& Loco = World.GetLocomotionTuning();
	FCrowdDraws& Draws = World.GetDraws();
	const ECrowdTell Tell = ECrowdTell::Run;
	if (!TryPerformTell(Tell, Now, World, !bPanic))
	{
		return;
	}
	if (BoutSec <= 0.0)
	{
		BoutSec = Draws.DrawUniform(Ordinal, bPanic ? ECrowdDecision::PanicRunLength : ECrowdDecision::RunBoutLength, RunBoutMinSec, RunBoutMaxSec);
	}
	// The planned distance of the bout: accelerate to run speed from the current speed, hold it, brake at the end. The
	// destination is drawn by the R13 weights (the density model) at least that far away, so the bout ends by time at
	// the drawn duration, by ordinary SCL braking, and the leg ends where the braking ends.
	const double V = FMath::Max(static_cast<double>(Loco.RunSpeedCmPerSec), 1.0);
	const double V0 = FMath::Min(Self.Speed2D(), V);
	const double Accel = FMath::Max(static_cast<double>(StealthLocomotion::RunAcceleration(Loco)), 1.0);
	const double TAccel = FMath::Min((V - V0) / Accel, BoutSec);
	const double Distance = (V0 + V) * 0.5 * TAccel + V * (BoutSec - TAccel) + FCrowdFollower::StopDistanceCm(Loco, V, EGait::Run);
	FVector Goal;
	// PickLegGoal admits goals within 0.6-1.4 x the leg length: ask for one at least the bout's distance away.
	const double LegCm = Distance / 0.6 * 1.05;
	LeaveRestForHerring(Now);
	if (AwayDir != nullptr)
	{
		// An R8 panic run goes away from the strike point (not a destination draw).
		Goal = Self.Location + *AwayDir * (Distance + 300.0);
	}
	else if (!World.PickLegGoal(Ordinal, Self.Location, LegCm, Goal))
	{
		// No destination that far (the edge of the walkable area): the nearest band the density model offers.
		if (!World.PickLegGoal(Ordinal, Self.Location, Distance, Goal))
		{
			Goal = Self.Location + FRotator(0.0, Self.FacingYawDeg, 0.0).Vector() * Distance;
		}
	}
	BeginLegTo(Now, Self, World, Goal, true, EGait::Run, Now + BoutSec);
	Mode = EHerringMode::RunLeg;
	if (Observer != nullptr)
	{
		Observer->OnRunLeg(Ordinal, Now, BoutSec, Goal, bPanic);
		Observer->OnLegStarted(Ordinal, Now, Follower.GetPace(), LegCm);
	}
}
