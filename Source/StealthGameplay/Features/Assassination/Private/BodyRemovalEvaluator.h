#pragma once

// Story asn-020 (production/epics/assassination-defence-reversal/story-020-client-fall-linger-and-occlusion-removal.md):
// VA-ASN-2's cut-at-occlusion removal rule, plain C++ (ADR-0018 section 6.1). It reads only geometry and time, never who
// the body was, so a removal (even one seen at the grace cut) reveals nothing about the victim's identity.

#include "CoreMinimal.h"

/** What this machine does with a body this frame. */
enum class EBodyRemovalDecision : uint8
{
	/** Shown (falling, lingering, or in the removal window while in view). */
	Keep,
	/** In the removal window and fully hidden by world geometry: hidden this frame. */
	HideOccluded,
	/** In the removal window and outside the viewer's frustum: hidden this frame. */
	HideOutOfFrustum,
	/** Still in view when BodyRemovalGraceSec ran out: cut in this single frame, no fade, dissolve, sink or effect. */
	CutAtGrace,
	/** First received after the linger ended (story asn-022's late actor): never shown. */
	NeverShow,
	/** Already hidden: nothing to do (a hidden body is never shown again). */
	AlreadyHidden,
};

/** Log name of a decision. */
STEALTHGAMEPLAY_API const TCHAR* LexBodyRemovalDecision(EBodyRemovalDecision Decision);

/** One frame's inputs for one body on one machine. */
struct FBodyRemovalInput
{
	/** EstimatedServerNow() of this machine. */
	double Now = 0.0;
	double DeathServerTime = -1.0;
	double DeathFallSec = 1.2;
	double BodyLingerSec = 6.0;
	double BodyRemovalGraceSec = 2.0;
	/** Any part of the body's bounds is inside the local viewer's frustum. */
	bool bInFrustum = true;
	/** Every in-frustum sample of the body's bounds is blocked by world geometry (pawns are not occluders). */
	bool bFullyOccluded = false;
	/** This machine first received the body after its linger ended. */
	bool bFirstSeenAfterLinger = false;
	/** The body is already hidden on this machine. */
	bool bAlreadyHidden = false;
};

/**
 * FBodyRemovalEvaluator::Evaluate: from DeathServerTime + DeathFallSec + BodyLingerSec the body is hidden on the first
 * frame it is out of the viewer's frustum or fully occluded; at + BodyRemovalGraceSec it is cut whatever the view. A hide
 * is final. Before the window the body is always kept (the fall and the linger are seen by everyone who looks).
 *
 * Usage (UEncounterPresentationSubsystem, per frame while the body is in its window):
 *   FBodyRemovalInput In; In.Now = Now; In.DeathServerTime = Death.DeathServerTime; In.bInFrustum = View.bInFrustum; ...
 *   if (FBodyRemovalEvaluator::Hides(FBodyRemovalEvaluator::Evaluate(In))) { HideBody(); }
 */
struct STEALTHGAMEPLAY_API FBodyRemovalEvaluator
{
	static EBodyRemovalDecision Evaluate(const FBodyRemovalInput& Input);

	/** True for the decisions that hide the body this frame. */
	static bool Hides(EBodyRemovalDecision Decision);

	/** Server time the removal window opens (DeathServerTime + DeathFallSec + BodyLingerSec). */
	static double WindowOpensAt(const FBodyRemovalInput& Input);

	/** Server time the grace runs out (the window plus BodyRemovalGraceSec). */
	static double GraceEndsAt(const FBodyRemovalInput& Input);
};
