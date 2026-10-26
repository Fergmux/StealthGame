#include "Server/CrowdAgent.h"

#include "Server/StealthTuningSubstructs.h"
#include "View/Core/StealthLocomotionModel.h"
#include "View/StealthTuningPublicSubstructs.h"

namespace CrowdAgentLocal
{
	/** Query radius for the follower's neighbours (avoidance horizon and C4 range). */
	constexpr double SteeringQueryCm = FCrowdAvoidance::QueryRangeCm;
	/** G5 hysteresis band: the partner-look time leads or lags share x conversation time by at most this. */
	constexpr double PartnerLookBandSec = 0.6;
	/** Look-away offset during a conversation (deg). */
	constexpr float LookAwayDeg = 35.0f;
	/** A conversation partner farther than this multiple of CrowdConverseRangeCm has left. */
	constexpr double PartnerLeaveFactor = 1.5;
	/** A goods look starts this long into a long pause at a goods context (s). */
	constexpr double GoodsLookDelaySec = 0.6;
	/** Salient stimuli closer together than this (s) are one event for a watcher. */
	constexpr double SalientRefractorySec = 2.0;
}

float FCrowdAgent::YawTo(const FVector& From, const FVector& To)
{
	return static_cast<float>(FMath::RadiansToDegrees(FMath::Atan2(To.Y - From.Y, To.X - From.X)));
}

bool FCrowdAgent::Faces(const FCrowdPerceivedPawn& Other, const FVector& SelfLocation, double AngleDeg)
{
	const float Bearing = YawTo(Other.Location, SelfLocation);
	return FMath::Abs(StealthLocomotion::DeltaYawDeg(Other.FacingYawDeg, Bearing)) <= AngleDeg;
}

void FCrowdAgent::SetActivity(ECrowdActivity NewActivity, ICrowdAgentWorld& World)
{
	if (Activity != NewActivity)
	{
		Activity = NewActivity;
		World.ReportActivity(Ordinal, Activity);
	}
}

void FCrowdAgent::Initialize(uint32 InOrdinal, ECrowdDriverKind InDriver, const FCrowdPerceivedPawn& Self, double GridPhaseSec, TConstArrayView<FCrowdDeviceProfile> Profiles,
	ICrowdAgentWorld& World)
{
	Ordinal = InOrdinal;
	Driver = InDriver;
	Follower.Initialize(Ordinal, GridPhaseSec, World.GetCrowdTuning(), World.GetDraws());
	VirtualInput.Initialize(Ordinal, Profiles, Self.FacingYawDeg, World.GetDraws());
	LastLook = FRotator(0.0, Self.FacingYawDeg, 0.0);
}

void FCrowdAgent::Start(const FCrowdFirstActivity& First, double Now, const FCrowdPerceivedPawn& Self, ICrowdAgentWorld& World, ICrowdAgentOutput& Output)
{
	if (bStarted || bHalted)
	{
		return;
	}
	const FCrowdTuning_Server& Tuning = World.GetCrowdTuning();
	bStarted = true;
	World.GetTellSampler().StartNpc(Ordinal, Now, Tuning, World.GetDraws());
	DueList.Schedule(ECrowdDueKind::GlanceDue, Now + World.GetDraws().DrawUniform(Ordinal, ECrowdDecision::GlanceInterval, Tuning.CrowdGlanceIntervalMinSec, Tuning.CrowdGlanceIntervalMaxSec));
	if (First.bPause)
	{
		BeginPause(Now, Self, World, First.PauseSec);
	}
	else
	{
		BeginLeg(Now, Self, World, First.LegCm);
	}
	// The first command of the round, in this server step: the head and the held Move.
	EmitInput(Now, 0.0f, FCrowdFollowerOutput(), ComputeGaze(Now, Self, World), World, Output);
}

void FCrowdAgent::BeginLeg(double Now, const FCrowdPerceivedPawn& Self, ICrowdAgentWorld& World, double LegCm)
{
	const FCrowdTuning_Server& Tuning = World.GetCrowdTuning();
	FCrowdDraws& Draws = World.GetDraws();
	if (LegCm <= 0.0)
	{
		LegCm = Draws.DrawUniform(Ordinal, ECrowdDecision::LegLength, Tuning.CrowdLegMinCm, Tuning.CrowdLegMaxCm);
	}
	FVector Goal;
	if (!World.PickLegGoal(Ordinal, Self.Location, LegCm, Goal))
	{
		// Nowhere to go: a short pause, then try again.
		BeginPause(Now, Self, World, Tuning.CrowdShortPauseMinSec);
		return;
	}
	const uint32 LegId = NextLegId++;
	FCrowdPathRequest Request;
	Request.Ordinal = Ordinal;
	Request.LegId = LegId;
	Request.ControlEpoch = World.GetCurrentControlEpoch(Ordinal);
	Request.From = Self.Location;
	Request.To = Goal;
	TArray<FVector> Corners;
	PathFrom = Self.Location;
	bLegAbandoned = false;
	const bool bInstant = World.RequestPath(Request, Corners);
	if (!bInstant)
	{
		Corners = { Goal };
	}
	Follower.StartLeg(LegId, Self.Location, Corners, Now, Tuning, Draws);
	if (!bInstant)
	{
		Follower.BeginWaitForPath(Now);
	}
	bConversationChecked = false;
	DecidedNeighbours.Reset();
	SetActivity(ECrowdActivity::Routing, World);
	if (Observer != nullptr)
	{
		Observer->OnLegStarted(Ordinal, Now, Follower.GetPace(), LegCm);
	}
}

void FCrowdAgent::BeginPause(double Now, const FCrowdPerceivedPawn& Self, ICrowdAgentWorld& World, double ForcedLength)
{
	const FCrowdTuning_Server& Tuning = World.GetCrowdTuning();
	FCrowdDraws& Draws = World.GetDraws();
	double Length = ForcedLength;
	if (Length < 0.0)
	{
		bPauseLong = Draws.DrawChance(Ordinal, ECrowdDecision::PauseKind, World.GetLongPausePct(Now) / 100.0);
		Length = bPauseLong
			? Draws.DrawUniform(Ordinal, ECrowdDecision::PauseLength, Tuning.CrowdIdleMinSec, Tuning.CrowdIdleMaxSec)
			: Draws.DrawUniform(Ordinal, ECrowdDecision::PauseLength, Tuning.CrowdShortPauseMinSec, Tuning.CrowdShortPauseMaxSec);
	}
	else
	{
		bPauseLong = Length >= Tuning.CrowdIdleMinSec;
	}
	Follower.Stop();
	PauseEnd = Now + Length;
	DueList.Schedule(ECrowdDueKind::PauseEnd, PauseEnd);
	// A long pause faces a stall, mural or view within CrowdPauseContextRadiusCm when one exists (no facing otherwise).
	bPauseHasContext = false;
	bPauseGoods = false;
	if (bPauseLong)
	{
		bPauseHasContext = World.FindPauseContext(Self.Location, Tuning.CrowdPauseContextRadiusCm, PauseContextPoint, bPauseGoods);
		if (bPauseHasContext && bGlanceActive && GlanceKind == ECrowdGlanceKind::PasserBy)
		{
			// The contextual look replaces a passer-by glance carried over from the walk (G2 yields to the context).
			bGlanceActive = false;
			DueList.Cancel(ECrowdDueKind::GlanceEnd);
		}
		if (bPauseHasContext && bPauseGoods)
		{
			DueList.Schedule(ECrowdDueKind::ContextLookEnd, Now + GoodsDelay());
		}
	}
	bConversationChecked = false;
	DecidedNeighbours.Reset();
	SetActivity(ECrowdActivity::Pausing, World);
	if (Observer != nullptr)
	{
		Observer->OnPauseStarted(Ordinal, Now, bPauseLong, Length, bPauseHasContext, PauseContextPoint);
	}
}

double FCrowdAgent::GoodsDelay()
{
	return CrowdAgentLocal::GoodsLookDelaySec;
}

void FCrowdAgent::EndPause(double Now)
{
	DueList.Cancel(ECrowdDueKind::PauseEnd);
	DueList.Cancel(ECrowdDueKind::ContextLookEnd);
	DueList.Cancel(ECrowdDueKind::GoodsEnd);
	if (Observer != nullptr)
	{
		Observer->OnPauseEnded(Ordinal, Now, static_cast<float>(LastLook.Yaw));
	}
	bPauseHasContext = false;
	bPauseGoods = false;
	if (GlanceKind == ECrowdGlanceKind::Goods || GlanceKind == ECrowdGlanceKind::Context)
	{
		bGlanceActive = false;
	}
}

void FCrowdAgent::ForceLeg(double Now, const FCrowdPerceivedPawn& Self, ICrowdAgentWorld& World, ICrowdAgentOutput& Output)
{
	if (Activity == ECrowdActivity::Conversing)
	{
		EndConversation(Now, false, Self, World);
	}
	if (Activity == ECrowdActivity::Pausing)
	{
		EndPause(Now);
	}
	BeginLeg(Now, Self, World, -1.0);
}

bool FCrowdAgent::TryConversation(double Now, const FCrowdPerceivedPawn& Self, ICrowdAgentWorld& World)
{
	if (Driver == ECrowdDriverKind::Restricted || Self.Speed2D() > RestSpeedCm)
	{
		return false;
	}
	const FCrowdTuning_Server& Tuning = World.GetCrowdTuning();
	FCrowdDraws& Draws = World.GetDraws();
	TArray<const FCrowdPerceivedPawn*> Near;
	World.GetHash().Query(Self.Location, Tuning.CrowdConverseRangeCm, ECrowdQuery::Perception, Self.Key, Near);
	for (const FCrowdPerceivedPawn* Other : Near)
	{
		if (Other->Speed2D() > RestSpeedCm || DecidedNeighbours.Contains(Other->Key))
		{
			continue;
		}
		// Reply (C3): a resting neighbour faces this pawn: turn back with CrowdConverseReplyPct, whoever it is.
		const bool bFaced = Faces(*Other, Self.Location, Tuning.CrowdConverseFaceDeg);
		// Initiate (C3): only when this pawn has just come to rest (the first decision of the rest).
		if (!bFaced && bConversationChecked)
		{
			continue;
		}
		DecidedNeighbours.Add(Other->Key);
		const bool bTurn = bFaced
			? Draws.DrawChance(Ordinal, ECrowdDecision::ConverseReply, Tuning.CrowdConverseReplyPct / 100.0)
			: Draws.DrawChance(Ordinal, ECrowdDecision::ConverseStart, Tuning.CrowdConverseStartPct / 100.0);
		if (Observer != nullptr)
		{
			Observer->OnConverseDecision(Ordinal, Now, Other->Key, bFaced, bTurn);
		}
		if (bTurn)
		{
			PartnerKey = Other->Key;
			bConversationFormed = false;
			ConversationTime = 0.0;
			PartnerLookTime = 0.0;
			bPartnerLookOn = true;
			PartnerLookSwitch = Now;
			DueList.Schedule(ECrowdDueKind::ConverseTurnAway, Now + Draws.DrawUniform(Ordinal, ECrowdDecision::ConverseTurnAway, Tuning.CrowdConverseTurnAwayMinSec, Tuning.CrowdConverseTurnAwayMaxSec));
			bConversationChecked = true;
			SetActivity(ECrowdActivity::Conversing, World);
			return true;
		}
	}
	bConversationChecked = true;
	return false;
}

void FCrowdAgent::UpdateConversation(double Now, const FCrowdPerceivedPawn& Self, ICrowdAgentWorld& World)
{
	const FCrowdTuning_Server& Tuning = World.GetCrowdTuning();
	const FCrowdPerceivedPawn* Partner = World.GetHash().Find(PartnerKey);
	const bool bPartnerGone = Partner == nullptr || !Partner->bPerceivable || Partner->Speed2D() > MovingSpeedCm
		|| FVector::Dist2D(Partner->Location, Self.Location) > CrowdAgentLocal::PartnerLeaveFactor * Tuning.CrowdConverseRangeCm;
	if (bPartnerGone)
	{
		EndConversation(Now, false, Self, World);
		return;
	}
	const bool bMutual = Faces(Self, Partner->Location, Tuning.CrowdConverseFaceDeg) && Faces(*Partner, Self.Location, Tuning.CrowdConverseFaceDeg);
	if (!bConversationFormed && bMutual)
	{
		// The conversation exists from the moment both face each other within CrowdConverseFaceDeg.
		bConversationFormed = true;
		DueList.Cancel(ECrowdDueKind::ConverseTurnAway);
		FCrowdDraws& Draws = World.GetDraws();
		const double Length = Draws.DrawUniform(Ordinal, ECrowdDecision::ConverseLength, Tuning.CrowdConversationMinSec, Tuning.CrowdConversationMaxSec);
		ConversationEnd = Now + Length;
		DueList.Schedule(ECrowdDueKind::PauseEnd, ConversationEnd);
		PartnerLookShare = Draws.DrawUniform(Ordinal, ECrowdDecision::PartnerLook, Tuning.CrowdPartnerLookMinPct / 100.0, Tuning.CrowdPartnerLookMaxPct / 100.0);
		ConversationTime = 0.0;
		PartnerLookTime = 0.0;
		PartnerLookSwitch = Now;
		bPartnerLookOn = true;
		if (Observer != nullptr)
		{
			Observer->OnConversationFormed(Ordinal, Now, PartnerKey, Length);
		}
	}
	else if (bConversationFormed && !bMutual)
	{
		// A conversation exists only while both face each other.
		EndConversation(Now, false, Self, World);
	}
}

void FCrowdAgent::EndConversation(double Now, bool bTurnedAway, const FCrowdPerceivedPawn& Self, ICrowdAgentWorld& World)
{
	if (Observer != nullptr)
	{
		Observer->OnConversationEnded(Ordinal, Now, PartnerKey, ConversationTime > 0.0 ? PartnerLookTime / ConversationTime : 0.0, bTurnedAway);
	}
	DueList.Cancel(ECrowdDueKind::ConverseTurnAway);
	const bool bWasFormed = bConversationFormed;
	PartnerKey = 0;
	bConversationFormed = false;
	if (bTurnedAway || !bWasFormed)
	{
		// The unanswered initiator turns away and keeps the rest of its pause.
		SetActivity(ECrowdActivity::Pausing, World);
		if (Now >= PauseEnd)
		{
			bWake = true;
		}
		else
		{
			DueList.Schedule(ECrowdDueKind::PauseEnd, PauseEnd);
		}
		return;
	}
	// A finished conversation walks on.
	PauseEnd = Now;
	SetActivity(ECrowdActivity::Pausing, World);
	bWake = true;
}

void FCrowdAgent::ProcessDue(double Now, const FCrowdPerceivedPawn& Self, ICrowdAgentWorld& World)
{
	const FCrowdTuning_Server& Tuning = World.GetCrowdTuning();
	FCrowdDraws& Draws = World.GetDraws();
	FCrowdDueEntry Entry;
	while (DueList.PopDue(Now, Entry))
	{
		switch (Entry.Kind)
		{
		case ECrowdDueKind::PauseEnd:
			// A due entry acts in the tick in which it falls due (ADR-0016 section 3): the pause or conversation ends and
			// the next leg starts now; the tree is woken to re-select its state.
			if (Activity == ECrowdActivity::Conversing && bConversationFormed)
			{
				EndConversation(Now, false, Self, World);
			}
			if (Activity == ECrowdActivity::Pausing && Now >= PauseEnd - 1e-9)
			{
				EndPause(Now);
				BeginLeg(Now, Self, World, -1.0);
			}
			bWake = true;
			break;
		case ECrowdDueKind::ConverseTurnAway:
			if (Activity == ECrowdActivity::Conversing && !bConversationFormed)
			{
				EndConversation(Now, true, Self, World);
			}
			break;
		case ECrowdDueKind::GlanceDue:
			if (Activity == ECrowdActivity::Routing || Activity == ECrowdActivity::Pausing)
			{
				StartGlance(Now, Self, World);
			}
			else
			{
				DueList.Schedule(ECrowdDueKind::GlanceDue, Now + GlanceRetrySec);
			}
			break;
		case ECrowdDueKind::GlanceEnd:
			bGlanceActive = false;
			DueList.Schedule(ECrowdDueKind::GlanceDue, Now + Draws.DrawUniform(Ordinal, ECrowdDecision::GlanceInterval, Tuning.CrowdGlanceIntervalMinSec, Tuning.CrowdGlanceIntervalMaxSec));
			break;
		case ECrowdDueKind::ContextLookEnd:
			// The goods look (G3) starts: pitch down for U(CrowdGoodsHoldMin, Max) s.
			if (bPauseGoods && Activity == ECrowdActivity::Pausing && !bGlanceActive)
			{
				const double Hold = Draws.DrawUniform(Ordinal, ECrowdDecision::GoodsHold, Tuning.CrowdGoodsHoldMinSec, Tuning.CrowdGoodsHoldMaxSec);
				GlanceKind = ECrowdGlanceKind::Goods;
				GlancePitch = -static_cast<float>(Draws.DrawUniform(Ordinal, ECrowdDecision::GoodsPitch, Tuning.CrowdGoodsPitchMinDeg, Tuning.CrowdGoodsPitchMaxDeg));
				GlancePoint = PauseContextPoint;
				GlanceTargetKey = 0;
				bGlanceActive = true;
				DueList.Schedule(ECrowdDueKind::GoodsEnd, Now + Hold);
				if (Observer != nullptr)
				{
					Observer->OnGlance(Ordinal, Now, ECrowdGlanceKind::Goods, 0, Hold);
				}
			}
			break;
		case ECrowdDueKind::GoodsEnd:
			if (GlanceKind == ECrowdGlanceKind::Goods)
			{
				bGlanceActive = false;
			}
			break;
		case ECrowdDueKind::ReactionStart:
			if (!bHalted && Activity != ECrowdActivity::Conversing)
			{
				const double Hold = Draws.DrawUniform(Ordinal, ECrowdDecision::GlanceHold, Tuning.CrowdGlanceHoldMinSec, Tuning.CrowdGlanceHoldMaxSec);
				const FCrowdPerceivedPawn* Source = World.GetHash().Find(SalientKey);
				GlanceKind = ECrowdGlanceKind::Salient;
				GlancePoint = Source != nullptr && Source->bPerceivable ? Source->Location + FVector(0.0, 0.0, Source->EyeHeightCm) : SalientPoint;
				GlanceTargetKey = Source != nullptr && Source->bPerceivable ? SalientKey : 0;
				GlancePitch = 0.0f;
				bGlanceActive = true;
				DueList.Schedule(ECrowdDueKind::GlanceEnd, Now + Hold);
				if (Observer != nullptr)
				{
					Observer->OnGlance(Ordinal, Now, ECrowdGlanceKind::Salient, GlanceTargetKey, Hold);
				}
			}
			break;
		default:
			break;
		}
	}
}

void FCrowdAgent::StartGlance(double Now, const FCrowdPerceivedPawn& Self, ICrowdAgentWorld& World)
{
	const FCrowdTuning_Server& Tuning = World.GetCrowdTuning();
	FCrowdDraws& Draws = World.GetDraws();
	// A long pause at a stall, mural or view holds its contextual look (G3 and the context facing), not passer-by glances.
	if (bGlanceActive || (Activity == ECrowdActivity::Pausing && bPauseHasContext))
	{
		DueList.Schedule(ECrowdDueKind::GlanceDue, Now + GlanceRetrySec);
		return;
	}
	TArray<const FCrowdPerceivedPawn*> Near;
	World.GetHash().Query(Self.Location, Tuning.CrowdGlanceRangeCm, ECrowdQuery::Perception, Self.Key, Near);
	const FVector Eye = Self.Location + FVector(0.0, 0.0, Self.EyeHeightCm);
	for (const FCrowdPerceivedPawn* Other : Near)
	{
		// G2: the nearest MOVING pawn with line of sight; any kind, never a concealed one (the query excludes them).
		if (Other->Speed2D() < MovingSpeedCm)
		{
			continue;
		}
		const FVector Target = Other->Location + FVector(0.0, 0.0, Other->EyeHeightCm);
		if (!World.HasLineOfSight(Ordinal, Eye, Target))
		{
			continue;
		}
		double Hold;
		if (bHeadHoldPending)
		{
			// G6: the T13 clock turns this glance into a long hold.
			bHeadHoldPending = false;
			GlanceKind = ECrowdGlanceKind::LongHold;
			Hold = Draws.DrawUniform(Ordinal, ECrowdDecision::HeadHoldLength, Tuning.CrowdHeadHoldMinSec, Tuning.CrowdHeadHoldMaxSec);
			World.GetTellSampler().NotePerformed(Ordinal, ECrowdTell::HeadHold, Now);
		}
		else
		{
			GlanceKind = ECrowdGlanceKind::PasserBy;
			Hold = Draws.DrawUniform(Ordinal, ECrowdDecision::GlanceHold, Tuning.CrowdGlanceHoldMinSec, Tuning.CrowdGlanceHoldMaxSec);
		}
		GlanceTargetKey = Other->Key;
		GlancePoint = Target;
		GlancePitch = 0.0f;
		bGlanceActive = true;
		DueList.Schedule(ECrowdDueKind::GlanceEnd, Now + Hold);
		if (Observer != nullptr)
		{
			Observer->OnGlance(Ordinal, Now, GlanceKind, GlanceTargetKey, Hold);
		}
		return;
	}
	DueList.Schedule(ECrowdDueKind::GlanceDue, Now + GlanceRetrySec);
}

void FCrowdAgent::OnSalientStimulus(double StimulusTime, uint64 SourceKey, const FVector& Where, double Now, ICrowdAgentWorld& World)
{
	if (!bStarted || bHalted || StimulusTime - LastSalientTime < CrowdAgentLocal::SalientRefractorySec || DueList.IsPending(ECrowdDueKind::ReactionStart))
	{
		return;
	}
	LastSalientTime = StimulusTime;
	const FCrowdTuning_Server& Tuning = World.GetCrowdTuning();
	FCrowdDraws& Draws = World.GetDraws();
	if (!Draws.DrawChance(Ordinal, ECrowdDecision::SalientGlance, Tuning.CrowdSalientGlancePct / 100.0))
	{
		return;
	}
	// G4: the delay runs from the stimulus time (a deferred line-of-sight ray kept it), never from a decision tick.
	const double Start = StimulusTime + Draws.DrawUniform(Ordinal, ECrowdDecision::ReactionDelay, Tuning.CrowdReactDelayMinSec, Tuning.CrowdReactDelayMaxSec);
	SalientKey = SourceKey;
	SalientPoint = Where;
	DueList.Schedule(ECrowdDueKind::ReactionStart, FMath::Max(Start, Now));
	if (Observer != nullptr)
	{
		Observer->OnSalientGlanceScheduled(Ordinal, StimulusTime, Start);
	}
}

FCrowdGazeIntent FCrowdAgent::ComputeGaze(double Now, const FCrowdPerceivedPawn& Self, ICrowdAgentWorld& World)
{
	FCrowdGazeIntent Gaze;
	const FVector Eye = Self.Location + FVector(0.0, 0.0, Self.EyeHeightCm);
	if (Activity == ECrowdActivity::Conversing && PartnerKey != 0)
	{
		if (const FCrowdPerceivedPawn* Partner = World.GetHash().Find(PartnerKey))
		{
			const FVector Head = Partner->Location + FVector(0.0, 0.0, Partner->EyeHeightCm);
			Gaze.bActive = true;
			Gaze.YawDeg = YawTo(Eye, Head);
			if (bConversationFormed)
			{
				// G5: the look tracks the drawn share with a band (sigma-delta with hysteresis): it looks away once the look
				// time is PartnerLookBandSec ahead of share x conversation time and back once it is that far behind, so the
				// share at any end time is the drawn one within the band (about a 4 s look, 1.7 s away at 70 %).
				const double Target = PartnerLookShare * ConversationTime;
				bool bLookOn = bPartnerLookOn;
				if (bPartnerLookOn && PartnerLookTime >= Target + CrowdAgentLocal::PartnerLookBandSec)
				{
					bLookOn = false;
				}
				else if (!bPartnerLookOn && PartnerLookTime <= Target - CrowdAgentLocal::PartnerLookBandSec)
				{
					bLookOn = true;
				}
				if (!bLookOn)
				{
					Gaze.YawDeg += (Ordinal % 2 == 0) ? CrowdAgentLocal::LookAwayDeg : -CrowdAgentLocal::LookAwayDeg;
				}
				bPartnerLookOn = bLookOn;
			}
			return Gaze;
		}
	}
	if (bGlanceActive)
	{
		FVector Point = GlancePoint;
		if (GlanceTargetKey != 0)
		{
			// A glance follows its target; a target that vanished or became unperceivable ends the glance.
			const FCrowdPerceivedPawn* Target = World.GetHash().Find(GlanceTargetKey);
			if (Target == nullptr || !Target->bPerceivable)
			{
				bGlanceActive = false;
			}
			else
			{
				Point = Target->Location + FVector(0.0, 0.0, Target->EyeHeightCm);
				GlancePoint = Point;
			}
		}
		if (bGlanceActive)
		{
			Gaze.bActive = true;
			Gaze.YawDeg = YawTo(Eye, Point);
			Gaze.PitchDeg = GlanceKind == ECrowdGlanceKind::Goods ? GlancePitch : 0.0f;
			return Gaze;
		}
	}
	if (Activity == ECrowdActivity::Pausing && bPauseHasContext)
	{
		Gaze.bActive = true;
		Gaze.YawDeg = YawTo(Eye, PauseContextPoint);
	}
	return Gaze;
}

void FCrowdAgent::EmitInput(double Now, float Dt, const FCrowdFollowerOutput& Motion, const FCrowdGazeIntent& Gaze, ICrowdAgentWorld& World, ICrowdAgentOutput& Output)
{
	FCrowdVirtualInputOutput In = VirtualInput.Tick(Now, Dt, Motion.WorldDirection, Motion.Magnitude, Motion.Gait, Gaze, World.GetLocomotionTuning(), World.GetDraws());
#if !UE_BUILD_SHIPPING
	if (LookOverride.IsSet())
	{
		In.Look = LookOverride.GetValue();
	}
#endif
	Output.Move(In.Move, In.Gait);
	Output.Look(In.Look);
	LastMove = In.Move;
	LastLook = In.Look;
	bMoveSent = true;
}

void FCrowdAgent::Tick(double Now, float Dt, const FCrowdPerceivedPawn& Self, ICrowdAgentWorld& World, ICrowdAgentOutput& Output)
{
	if (!bStarted || bHalted)
	{
		return;
	}
	const FCrowdTuning_Server& Tuning = World.GetCrowdTuning();
	ProcessDue(Now, Self, World);

	// Tell clocks (G6 only in this build).
	TArray<ECrowdTell, TInlineAllocator<2>> Accepted;
	World.GetTellSampler().Advance(Ordinal, Now, Tuning, World.GetDraws(), Accepted);
	for (const ECrowdTell Tell : Accepted)
	{
		if (Tell == ECrowdTell::HeadHold)
		{
			bHeadHoldPending = true;
		}
	}

	TArray<const FCrowdPerceivedPawn*> Neighbours;
	World.GetHash().Query(Self.Location, CrowdAgentLocal::SteeringQueryCm, ECrowdQuery::Steering, Self.Key, Neighbours);
	const float PaceBefore = Follower.GetPace();
	FCrowdFollowerOutput Motion = Follower.Tick(Now, Dt, Self, Neighbours, Tuning, World.GetLocomotionTuning(), World.GetDraws());
	if (Observer != nullptr && Follower.GetPace() != PaceBefore && Follower.IsOnLeg())
	{
		Observer->OnPaceChanged(Ordinal, Now, Follower.GetPace(), Follower.GetLastPaceCause());
	}
	if (Motion.bWantsRepath)
	{
		FCrowdPathRequest Request;
		Request.Ordinal = Ordinal;
		Request.LegId = Follower.GetLegId();
		Request.ControlEpoch = World.GetCurrentControlEpoch(Ordinal);
		Request.From = Self.Location;
		Request.To = Follower.GetCorners().Num() > 0 ? Follower.GetCorners().Last() : Self.Location;
		TArray<FVector> Corners;
		PathFrom = Self.Location;
		if (World.RequestPath(Request, Corners))
		{
			Follower.ReplacePath(Self.Location, Corners);
		}
	}
	if (Motion.bAbandoned)
	{
		bLegAbandoned = true;
	}
	if (Motion.bArrived && Activity == ECrowdActivity::Routing && !bLegAbandoned)
	{
		// At rest at the leg's end: the pause starts in this tick (C2).
		BeginPause(Now, Self, World);
	}
	if (Motion.bArrived || Motion.bAbandoned)
	{
		bWake = true;
	}
	if (Activity == ECrowdActivity::Conversing)
	{
		if (bConversationFormed)
		{
			ConversationTime += Dt;
			PartnerLookTime += bPartnerLookOn ? Dt : 0.0;
		}
		UpdateConversation(Now, Self, World);
	}

	// The stationary-stretch watchdog: a stretch about to reach CrowdMaxIdleSec gets a Routing leg in this tick.
	if ((Activity == ECrowdActivity::Pausing || Activity == ECrowdActivity::Conversing)
		&& World.GetStationaryAge(Ordinal, Now) >= Tuning.CrowdMaxIdleSec - WatchdogLeadSec)
	{
		if (Observer != nullptr)
		{
			Observer->OnWatchdog(Ordinal, Now, World.GetStationaryAge(Ordinal, Now));
		}
		ForceLeg(Now, Self, World, Output);
		Motion = Follower.Tick(Now, 0.0f, Self, Neighbours, Tuning, World.GetLocomotionTuning(), World.GetDraws());
	}
	EmitInput(Now, Dt, Motion, ComputeGaze(Now, Self, World), World, Output);
}

bool FCrowdAgent::Decide(double Now, const FCrowdPerceivedPawn& Self, ICrowdAgentWorld& World, ICrowdAgentOutput& Output)
{
	if (!bStarted || bHalted)
	{
		return false;
	}
	const ECrowdActivity Before = Activity;
	switch (Activity)
	{
	case ECrowdActivity::Routing:
		if (bLegAbandoned && !Follower.IsOnLeg())
		{
			// Abandoned (CNB-R3.3): draw a new destination at once.
			BeginLeg(Now, Self, World, -1.0);
		}
		else if (!Follower.IsOnLeg() && !Follower.IsStopping())
		{
			BeginPause(Now, Self, World);
		}
		break;
	case ECrowdActivity::Pausing:
		if (Now >= PauseEnd)
		{
			EndPause(Now);
			BeginLeg(Now, Self, World, -1.0);
		}
		else
		{
			TryConversation(Now, Self, World);
		}
		break;
	case ECrowdActivity::Conversing:
		UpdateConversation(Now, Self, World);
		break;
	default:
		break;
	}
	return Activity != Before;
}

bool FCrowdAgent::OnPathResult(const FCrowdPathResult& Result, ICrowdAgentWorld& World)
{
	if (bHalted || !Follower.IsOnLeg() || !FCrowdPathBudget::Accept(Result, Follower.GetLegId(), World.GetCurrentControlEpoch(Ordinal)))
	{
		return false;
	}
	if (!Result.bValid || Result.bPartial || Result.Corners.Num() == 0)
	{
		// A failed or partial query abandons the leg (CNB-R3.3 progress monitor); the next decision draws a new one.
		Follower.Stop();
		bLegAbandoned = true;
		bWake = true;
		return true;
	}
	Follower.ReplacePath(PathFrom, Result.Corners);
	return true;
}

void FCrowdAgent::Halt(ICrowdAgentOutput& Output)
{
	bHalted = true;
	DueList.Clear();
	Follower.Stop();
	Activity = ECrowdActivity::Halted;
	Output.Move(FVector2D::ZeroVector, EGait::Walk);
	LastMove = FVector2D::ZeroVector;
}
