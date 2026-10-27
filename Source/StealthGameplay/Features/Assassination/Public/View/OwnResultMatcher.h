#pragma once

// Story asn-024 (production/epics/assassination-defence-reversal/story-024-owner-result-matcher-and-plate-timing.md):
// the owner result matcher (ADR-0018 section 5.3, HUD F2), the plate timeline (HUD F3) and the brace square's end rule
// (UI-ASN-3). Plain C++, client-safe, no UObject: the owning player controller feeds it and samples it every frame.

#include "CoreMinimal.h"
#include "View/CommandReplyTypes.h"
#include "View/NetPawnCommand.h"
#include "View/StealthEncounterWire.h"

/** The five owner plates (GDD hud.md E07 / E08); one entrance and one timing for all five. */
enum class EOwnResultPlate : uint8
{
	/** FStrikeResult RivalEliminated. */
	TargetEliminated,
	/** FStrikeResult WrongTarget (byte-identical for an NPC and an informant victim). */
	WrongTarget,
	/** FStrikeResult NoContact (no stamp, no cochineal). */
	NoContact,
	/** FStrikeResult Reversed (the attacker; shown inside its OnRoleChangePresented sequence). */
	Reversed,
	/** FDefenceResult Reversed (the bound defender, at its Counter's ContactServerTime). */
	StrikeReversed,
};

/** Number of EOwnResultPlate values. */
inline constexpr int32 GOwnResultPlateCount = 5;

/** Log name of a plate. */
STEALTHGAMEPLAY_API const TCHAR* LexOwnResultPlate(EOwnResultPlate Plate);

/** The plate's text (string table ids Hud.Result.* / Hud.Result.Defence of design/ux/hud.md). */
STEALTHGAMEPLAY_API FText OwnResultPlateText(EOwnResultPlate Plate);

/** The attacker plate of a strike result. */
STEALTHGAMEPLAY_API EOwnResultPlate OwnResultPlateOf(EStrikeResult Result);

/** Where a shown result's time came from. */
enum class EOwnResultTimeSource : uint8
{
	/** FStrikeResult.ContactServerTime (the attacker: always first when present). */
	ResultContact,
	/** The matched FActionState write on the commanded pawn (the defender's Counter; the attacker's lost-time fallback). */
	MatchedActionState,
	/** Nothing matched within HudResultPlateMaxHoldBackSec of the arrival (a dropped write): shown at arrival + cap. */
	LostDataCap,
};

/** Log name of a time source. */
STEALTHGAMEPLAY_API const TCHAR* LexOwnResultTimeSource(EOwnResultTimeSource Source);

/** The matcher's tuning (UOwnResultPlateSettings::MatcherParams). */
struct FOwnResultMatcherParams
{
	/** The TDC hard minimum of HudResultPlateMaxHoldBackSec (decision D8): the cap never fires below the strike lead. */
	static constexpr double HardMinHoldBackSec = 1.25;

	/** HudResultPlateMaxHoldBackSec. */
	double MaxHoldBackSec = 1.25;

	/** The effective cap (never below the hard minimum). */
	double HoldBackSec() const { return FMath::Max(MaxHoldBackSec, HardMinHoldBackSec); }
};

/** One result that is due: shown by the HUD at ShowServerTime (in EstimatedServerNow()). */
struct FOwnResultDue
{
	EOwnResultPlate Plate = EOwnResultPlate::NoContact;
	ECommandReplyKind Kind = ECommandReplyKind::StrikeResult;
	/** The command the result answers (the Attack; the defender's Prepare(Begin)). */
	FClientCommandSeq Seq;
	/** The attacker's result (TallyAfter, bExiled); default for a defence result. */
	FStrikeResult Strike;
	/** The commanded pawn as an opaque key (never dereferenced): the handle captured at the press, else the fallback. */
	const void* PawnKey = nullptr;
	/** EstimatedServerNow() at the result's processing. */
	double ArrivalServerTime = 0.0;
	/** The contact time the plate answers; -1 when it was never learnt (LostDataCap). */
	double ContactServerTime = -1.0;
	/** t_show = min(max(t_c, t_arrival), t_arrival + HoldBack) (F2), or the cap when t_c is unknown. */
	double ShowServerTime = 0.0;
	EOwnResultTimeSource Source = EOwnResultTimeSource::ResultContact;
	/** The contact had passed when the result was processed: shown on arrival (never dropped for lateness). */
	bool bLate = false;
	/** A Reversed attacker plate released by its own OnRoleChangePresented(ReversedAgainst) (art bible 7.6 rule 7). */
	bool bInRoleChange = false;
};

/**
 * FOwnResultMatcher (ADR-0018 section 5.3 Key Interfaces; HUD F2): one per local player, client-local.
 *
 * Inputs: OnCommandSent (an Attack, a Prepare(Begin): the commanded pawn handle and its FActionState at the press, so a
 * previous write is never matched), OnStrikeResult / OnDefenceResult (the reply entries, with their processing time),
 * OnActionStateObserved (the owner's sampled FActionState of every watched pawn, any order relative to the results: UE
 * does not order an RPC against property replication, E6), OnRejected (a voided command is never matched) and
 * OnReversedAgainstPresented (the own-role clock).
 *
 * Rules:
 *  - attacker: t_c = FStrikeResult.ContactServerTime whenever present (never an inferred time); defender: the Counter
 *    write on the handle captured at the Prepare(Begin) whose Seq differs from the last consumed and whose
 *    ContactServerTime is later; a result with no time stays Pending until that write is observed;
 *  - t_show = min(max(t_c, t_arrival), t_arrival + HoldBack); a result whose write never arrives shows at
 *    t_arrival + HoldBack (HoldBack >= 1.25 s); a late result shows on arrival; nothing shows before its contact;
 *  - duplicates and older sequences drop by (Kind, PossessionEpoch, Seq) order and a last-8 ring;
 *  - a Reversed attacker plate is released only inside its pawn's OnRoleChangePresented(ReversedAgainst) sequence (the
 *    grade, the removal card and the plate are one sequence), or by the cap when the death state never arrives.
 *
 * Usage (the owning controller, every frame):
 *   Matcher.OnActionStateObserved(Pawn, Pawn->GetActionState(), ServerNow);
 *   for (const FOwnResultDue& Due : Matcher.PopDue(ServerNow)) { ShowPlate(Due); }
 */
class STEALTHGAMEPLAY_API FOwnResultMatcher
{
public:
	/** An opaque pawn handle (never dereferenced). */
	using FPawnKey = const void*;

	/** Size of the duplicate ring. */
	static constexpr int32 DuplicateRingSize = 8;

	explicit FOwnResultMatcher(const FOwnResultMatcherParams& InParams = FOwnResultMatcherParams()) : Params(InParams) {}

	void SetParams(const FOwnResultMatcherParams& InParams) { Params = InParams; }
	const FOwnResultMatcherParams& GetParams() const { return Params; }

	/** A command left this client: Attack and PrepareBegin are recorded with the commanded pawn and its state at the press. */
	void OnCommandSent(ECommandKind Kind, const FClientCommandSeq& Seq, FPawnKey Pawn, const FActionState& PawnStateAtPress);

	/** The command Seq was rejected (voided): it will produce no result and no write. */
	void OnRejected(const FClientCommandSeq& Seq);

	/**
	 * The attacker's result was processed at ArrivalServerTime. FallbackPawn (the currently possessed pawn) is used only
	 * when the press was not recorded. False when dropped as a duplicate or an older sequence.
	 */
	bool OnStrikeResult(const FStrikeResult& Result, double ArrivalServerTime, FPawnKey FallbackPawn);

	/** The defender's result for its Prepare(Begin) Seq. False when dropped. */
	bool OnDefenceResult(const FClientCommandSeq& Seq, const FDefenceResult& Result, double ArrivalServerTime, FPawnKey FallbackPawn);

	/** The owner observed State on Pawn at ServerNow (repeat observations of one write are ignored). */
	void OnActionStateObserved(FPawnKey Pawn, const FActionState& State, double ServerNow);

	/** OnRoleChangePresented(ReversedAgainst) fired for Pawn at ServerNow. */
	void OnReversedAgainstPresented(FPawnKey Pawn, double ServerNow);

	/** Removes and returns every result due at ServerNow, in show-time order. */
	TArray<FOwnResultDue> PopDue(double ServerNow);

	/** True while a result waits for its show time. */
	bool HasPending() const { return !Pending.IsEmpty(); }

	/** The pawns whose FActionState the owner should sample (pending presses and pending results). */
	TArray<FPawnKey> GetWatchedPawns() const;

	/** Results dropped as duplicates or older sequences. */
	int32 GetDroppedCount() const { return DroppedCount; }

	/** Forgets everything (a Round Reset, a new local player). */
	void Reset();

private:
	struct FCommandRecord
	{
		ECommandKind Kind = ECommandKind::Attack;
		FClientCommandSeq Seq;
		FPawnKey Pawn = nullptr;
		uint16 BaselineSeq = 0;
		double BaselineContact = -1.0;
	};

	struct FObservation
	{
		FPawnKey Pawn = nullptr;
		uint16 Seq = 0;
		EActionPose Pose = EActionPose::None;
		double ContactServerTime = -1.0;
		bool bConsumed = false;
	};

	struct FPendingResult
	{
		FOwnResultDue Due;
		EActionPose WantPose = EActionPose::Strike;
		/** The contact time is known (from the result or a matched write); else Pending at the cap. */
		bool bTimeKnown = false;
		bool bHasCommand = false;
		uint16 BaselineSeq = 0;
		double BaselineContact = -1.0;
		bool bAwaitRoleChange = false;
		bool bRoleChangeSeen = false;
	};

	struct FConsumed
	{
		uint16 Seq = 0;
		double ContactServerTime = -1.0;
	};

	bool AcceptKey(ECommandReplyKind Kind, const FClientCommandSeq& Seq);
	void AddPending(FPendingResult&& Result);
	bool Qualifies(const FPendingResult& Result, const FObservation& Observation) const;
	void Match(FPendingResult& Result, FObservation& Observation);
	void SetShowTime(FPendingResult& Result, double ContactServerTime, EOwnResultTimeSource Source) const;
	void TryMatch(FPendingResult& Result);
	void CheckRoleChange(FPendingResult& Result) const;
	const FCommandRecord* FindCommand(const FClientCommandSeq& Seq) const;
	double LastConsumedContact(FPawnKey Pawn) const;

	FOwnResultMatcherParams Params;
	TArray<FCommandRecord> Commands;
	TArray<FObservation> Observations;
	TArray<FPendingResult> Pending;
	TMap<FPawnKey, FConsumed> LastConsumed;
	TMap<FPawnKey, uint16> LastObservedSeq;
	TMap<FPawnKey, double> ReversedAgainstAt;
	/** The last accepted (Kind, Seq) keys, newest last. */
	TArray<TPair<ECommandReplyKind, FClientCommandSeq>> AcceptedRing;
	TMap<ECommandReplyKind, FClientCommandSeq> NewestAccepted;
	int32 DroppedCount = 0;
};

/** The F3 timeline parameters (UOwnResultPlateSettings::TimelineParams). */
struct FOwnResultPlateTimelineParams
{
	double InSec = 0.25;
	int32 InSteps = 4;
	double HoldSec = 3.0;
	double ReadHoldMult = 1.0;
	double OutSec = 0.6;

	/** In + Hold x mult + Out. */
	double TotalSec() const { return InSec + HoldSec * ReadHoldMult + OutSec; }
};

/** A plate's timeline phase. */
enum class EOwnResultPlatePhase : uint8
{
	Pending,
	Entering,
	Holding,
	Exiting,
	Gone,
};

/** One sample of a plate's timeline. */
struct FOwnResultPlateSample
{
	EOwnResultPlatePhase Phase = EOwnResultPlatePhase::Pending;
	/** 0..1; stepped while entering (no motion), a fade while exiting. */
	float Opacity = 0.0f;
};

/** HUD F3: one timeline for every plate, sampled from now - t_show (never from a widget animation). */
struct STEALTHGAMEPLAY_API FOwnResultPlateTimeline
{
	/** The sample at Elapsed seconds after the show time (negative: Pending). */
	static FOwnResultPlateSample Sample(const FOwnResultPlateTimelineParams& Params, double Elapsed);
};

/** UI-ASN-3: the brace square of the owner's pawn. */
struct STEALTHGAMEPLAY_API FOwnBraceIndicator
{
	/**
	 * True while the square is shown: the pose is Brace, or it is the Counter of a reversal whose ContactServerTime has not
	 * been reached (a reversal ends the brace at contact, never at the Counter's or the FDefenceResult's arrival).
	 */
	static bool IsShown(const FActionState& OwnState, double ServerNow);
};
