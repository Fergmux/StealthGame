#pragma once

// Stories asn-018 / asn-020 / asn-021 (production/epics/assassination-defence-reversal/): the state-derived timeline of one
// pawn's encounter presentation (ADR-0018 section 5.1). Plain C++: given the replicated FActionState / FDeathState and this
// machine's server-time estimate it says which clip should be playing, where in it, at what rate, and where the body is in
// its fall / linger / removal sequence. UEncounterPresentationSubsystem::Refresh applies the plan; this core decides nothing
// from the outcome (neither struct carries one), so a kill, a whiff and a reversal present identically up to contact.

#include "CoreMinimal.h"
#include "View/StealthEncounterWire.h"

struct FAssassinationTuning_Public;

/** The encounter clips (VA-ASN-1). The three strike entries are chosen by bearing, never by outcome (ASN-R16). */
enum class EEncounterClip : uint8
{
	None,
	Strike_F,
	Strike_L60,
	Strike_R60,
	Counter,
	DeathFall,
};

/** Number of EEncounterClip values. */
inline constexpr int32 GEncounterClipCount = 6;

/** Log name of a clip. */
STEALTHGAMEPLAY_API const TCHAR* LexEncounterClip(EEncounterClip Clip);

/** True for Strike_F, Strike_L60 and Strike_R60. */
inline bool IsStrikeClip(EEncounterClip Clip)
{
	return Clip == EEncounterClip::Strike_F || Clip == EEncounterClip::Strike_L60 || Clip == EEncounterClip::Strike_R60;
}

/** The public tuning the presentation reads (FAssassinationTuning_Public; a client never reads a Server key). */
struct STEALTHGAMEPLAY_API FEncounterPresentationParams
{
	double StrikeContactSec = 0.40;
	double AttackAttemptLockoutSec = 1.75;
	double CounterDurationSec = 1.2;
	double DeathFallSec = 1.2;
	double BodyLingerSec = 6.0;
	double BodyRemovalGraceSec = 2.0;
	double FrontVariantHalfAngleDeg = 30.0;
	double StrikeWarpYawLimitDeg = 30.0;
	double StrikeWarpLimitCm = 240.0;
	double StrikeContactGapCm = 15.0;
	/** Cross-fade cap of a late (sought) strike or counter clip (ADR-0018 section 5.1: <= 0.1 s, never a snap). */
	double MaxCrossFadeSec = 0.10;
	/** Inertialised entry of an on-time Death_Fall (<= 0.15 s, story asn-020 engine notes); a late fall uses MaxCrossFadeSec. */
	double FallEntryBlendSec = 0.15;

	static FEncounterPresentationParams FromPublic(const FAssassinationTuning_Public& Public);
};

/** The authored clips' own timings (the presentation settings; the D54 placeholders until the authored clips land). */
struct FEncounterClipTimings
{
	/** Strike clip length and the clip time of its contact frame. */
	double StrikeLengthSec = 1.967;
	double StrikeClipContactSec = 0.9;
	/** Counter clip length (played over CounterDurationSec from the reversed strike's contact). */
	double CounterLengthSec = 3.467;
	/** Death_Fall clip length (played over DeathFallSec from DeathServerTime, then held). */
	double FallLengthSec = 2.167;
};

/** One Refresh's inputs: the pawn's replicated state and this machine's clock. */
struct FEncounterPresentationInput
{
	FActionState Action;
	FDeathState Death;
	/** EstimatedServerNow() on this machine (the server's own time on the server). */
	double Now = 0.0;
	/** False roughly the first seconds after travel (ADR-0002 section 9): present on receipt, never earlier. */
	bool bClockSynced = true;
	/** EstimatedServerNow() when this machine first saw the current FActionState Seq (-1 unknown). */
	double ActionReceiptNow = -1.0;
	/** EstimatedServerNow() when this machine first saw the FDeathState (-1 unknown). */
	double DeathReceiptNow = -1.0;
};

/** Where a body is in the one body sequence (VA-ASN-1, VA-ASN-2). */
enum class EEncounterBodyPhase : uint8
{
	/** No death write, or its DeathServerTime has not been reached on this machine. */
	Alive,
	/** Death_Fall plays: [DeathServerTime, + DeathFallSec). */
	Falling,
	/** The static final pose: [+ DeathFallSec, + BodyLingerSec). */
	Lingering,
	/** The removal window: hidden on the first frame out of view; cut at grace expiry. */
	Removal,
	/** Grace expired: the body is cut (if not already hidden). */
	Cut,
};

/** What Refresh must show now. */
struct FEncounterPresentationPlan
{
	/** The clip that owns the pose now (None: locomotion / the prototype brace stance). */
	EEncounterClip Clip = EEncounterClip::None;
	/** Identifies one clip instance: the FActionState Seq for a strike or counter, 0 for the fall. */
	uint32 InstanceKey = 0;
	/** Clip time the clip should be at now (seconds into the montage); negative while the clip is still to start. */
	double Position = -1.0;
	/** Montage rate for the current segment (the authored clip mapped onto the tuned timings). */
	double PlayRate = 1.0;
	/** Blend-in of a clip that starts now (a late start cross-fades within MaxCrossFadeSec). */
	double BlendInSec = 0.1;
	/** The clip started before this machine could present it (sought, not restarted). */
	bool bLate = false;
	/** The clip has run to its end: hold (the fall) or let it blend out (strike, counter). */
	bool bClipEnded = false;
	/** For a strike: the clip time the warp window ends (the contact frame). */
	double WarpWindowEndSec = 0.0;
	/** The death is presented (Now >= DeathServerTime): the role change and the victim's grade may fire. */
	bool bDeathPresented = false;
	EEncounterBodyPhase BodyPhase = EEncounterBodyPhase::Alive;
	/** Pawn collision is off client-locally (Now >= DeathServerTime + DeathFallSec). */
	bool bCollisionOff = false;
	/** First received after the linger ended: never shown (story asn-022 calls the evaluator with this). */
	bool bNeverShow = false;
	/** This machine presented on receipt because its clock is not synced yet (logged). */
	bool bUnsyncedFallback = false;
	/** Refresh must keep running every frame (a clip is scheduled or playing, or the body sequence is not over). */
	bool bNeedsTick = false;
	/** Server time the next scheduled change is due (pending clip start, contact, fall end ...); -1 when none. */
	double NextDueServerTime = -1.0;
};

/**
 * FEncounterPresentationCore::Evaluate (ADR-0018 section 5.1). Precedence: a valid FDeathState whose DeathServerTime has
 * been reached wins over any FActionState pose (the reversed attacker's lunge runs to contact, then Death_Fall). Strike:
 * the clip's contact frame lands at ContactServerTime: [ContactServerTime - StrikeContactSec, Contact] maps onto
 * [0, StrikeClipContactSec] and [Contact, StartServerTime + AttackAttemptLockoutSec] onto the recovery; with the bush
 * preface the clip starts BushStrikeEmergeSec after StartServerTime (the preface itself has no placeholder clip). Counter:
 * starts at its ContactServerTime, never on receipt. Fall: from DeathServerTime over DeathFallSec, then held. Unsynced
 * clock: the timeline is shifted to start at receipt, never earlier.
 */
struct STEALTHGAMEPLAY_API FEncounterPresentationCore
{
	static FEncounterPresentationPlan Evaluate(const FEncounterPresentationParams& Params, const FEncounterClipTimings& Clips, const FEncounterPresentationInput& Input);

	/** The strike entry clip of FActionState.Variant (by bearing; the bush preface bit does not change it). */
	static EEncounterClip StrikeClipOf(uint8 VariantBits);

	/** Server-time seconds from DeathServerTime at which the body phase starts (Falling 0, Lingering, Removal, Cut). */
	static double BodyPhaseStartSec(const FEncounterPresentationParams& Params, EEncounterBodyPhase Phase);
};
