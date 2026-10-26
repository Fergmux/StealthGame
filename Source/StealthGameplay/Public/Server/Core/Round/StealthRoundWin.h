#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Server/EventLog/StealthRoundLogTypes.h"
#include "Server/Roster/StealthRosterCallTypes.h"
#include "View/StealthQueueTypes.h"
#include "View/StealthRosterTypes.h"
#include "View/StealthRoundPublicState.h"

/**
 * Round's win vocabulary, the F2 predicates and the terminal latch record (GDD round-match-framework R15, R16, F2;
 * ADR-0002 section 8.1; stories rmf-043..047). Plain C++: pure predicates over value inputs, plus the
 * single-assignment latch record FRoundMatchCore wraps (it adds the queue Latch call, logging and effects).
 *
 * R16 (1): there is no draw. ERoundTerminalKind is {Win, Void} and FRoundResult's winner is a single team or none;
 * the reviewed enumerator lists are pinned by StealthGame.Round.NoDrawProperty.
 */

/** A terminal outcome is a win (one team) or a void (no award, never a shared result). Append only, reviewed. */
enum class ERoundTerminalKind : uint8
{
	Win,
	Void,
};

/** Number of ERoundTerminalKind values (the no-draw static check enumerates them). */
inline constexpr int32 GRoundTerminalKindCount = 2;

/** Number of ERoundEndReason values (StealthRoundLogTypes.h; the no-draw static check enumerates them). */
inline constexpr int32 GRoundEndReasonCount = 10;

/** One terminal outcome: TryTerminate's argument (R15). */
struct FRoundTerminalOutcome
{
	ERoundTerminalKind Kind = ERoundTerminalKind::Void;
	/** 0 or 1 for a win; 255 for a void. */
	uint8 WinningTeam = 255;
	/** Server-only reason (ObjectiveWin, Elimination, ForfeitElimination, DebugForced; VoidAbandoned, VoidKickedLastActive, VoidPawnLost). */
	ERoundEndReason EndReason = ERoundEndReason::VoidAbandoned;

	static FRoundTerminalOutcome Win(uint8 Team, ERoundEndReason Reason)
	{
		FRoundTerminalOutcome Out;
		Out.Kind = ERoundTerminalKind::Win;
		Out.WinningTeam = Team;
		Out.EndReason = Reason;
		return Out;
	}

	static FRoundTerminalOutcome Void(ERoundEndReason Reason)
	{
		FRoundTerminalOutcome Out;
		Out.Kind = ERoundTerminalKind::Void;
		Out.EndReason = Reason;
		return Out;
	}

	bool IsWin() const { return Kind == ERoundTerminalKind::Win; }

	/** R19: the public reason (1 objective, 2 elimination; forfeits fold into elimination; 0 for a void). */
	uint8 GetPublicReason() const
	{
		switch (EndReason)
		{
		case ERoundEndReason::ObjectiveWin:
		case ERoundEndReason::DebugForced:
			return 1;
		case ERoundEndReason::Elimination:
		case ERoundEndReason::ForfeitElimination:
			return 2;
		default:
			return 0;
		}
	}

	bool operator==(const FRoundTerminalOutcome& Other) const
	{
		return Kind == Other.Kind && WinningTeam == Other.WinningTeam && EndReason == Other.EndReason;
	}
	bool operator!=(const FRoundTerminalOutcome& Other) const { return !(*this == Other); }
};

/**
 * F2's event e: one applied single-participant transition as Round evaluates it (Roster OnParticipantStateChanged
 * plus Session's Kicked flag). A batch event (OnRosterRoundEnded, OnRosterReset) sets bBatch and is never evaluated.
 */
struct FRoundTransitionEvent
{
	bool bApplied = true;
	bool bBatch = false;
	FParticipantId Participant;
	ERosterTeam Team = ERosterTeam::None;
	EParticipantState Old = EParticipantState::Active;
	EParticipantState New = EParticipantState::Informant;
	ERosterReason Reason = ERosterReason::Assassinated;
	/** The disconnect was a host kick (Session Disconnected(Kicked)). */
	bool bKicked = false;
	/** Roster F1 snapshot right after e. */
	int32 ActiveCountAfter[2] = { 0, 0 };
};

/** F2's round-level inputs at the event (or at the completion). */
struct FRoundWinContext
{
	ERoundPhase Phase = ERoundPhase::Active;
	/** L: Terminated. */
	bool bTerminated = false;
	/** RosterCount(T): records on team T (Roster F3). */
	int32 RosterCount[2] = { 0, 0 };
	/** ConnectedActiveCount(T): members of T that are Active and Connected. */
	int32 ConnectedActiveCount[2] = { 0, 0 };
};

/** F2's item i for ObjectiveWin: an objective completion with the resolver's re-validated tender (Valid(i)). */
struct FRoundObjectiveCompletion
{
	/** Type = FinalCompletion; a preliminary (stage) completion never ends the round. */
	bool bFinal = false;
	/** The team the completion is for. */
	uint8 Team = 0;
	/** Valid(i): completer Active, Connected, human-controlled, on the team; the site live and not yet completed. */
	bool bCompleterActive = false;
	bool bCompleterConnected = false;
	bool bHumanControlled = false;
	bool bCompleterOnTeam = false;
	bool bSiteLive = false;

	bool IsValid() const { return bCompleterActive && bCompleterConnected && bHumanControlled && bCompleterOnTeam && bSiteLive; }
};

/**
 * The final completion's validity (F2 Valid(i), R13, R15; story rmf-049): one closed code per failure, shared by Round's
 * DeclareObjectiveWin path and the Objectives resolver. Values are stable: append only.
 */
enum class ERoundCompletionValidity : uint8
{
	Valid,
	/** The completer is not a rostered participant. */
	NotRostered,
	/** The completer is not on the completing team. */
	WrongTeam,
	/** The completer is an Informant (or out of the round). */
	CompleterInformant,
	/** The completer disconnected earlier in this same server tick (the disconnect's key precedes the completion). */
	CompleterDisconnectedThisTick,
	/** The completer is Disconnected. */
	CompleterDisconnected,
	/** The completer's pawn is AI-driven (grace, Unresponsive hand-over, or a control change since the hold's Begin). */
	CompleterAiDriven,
	/** The site is not live or already completed. */
	SiteNotLive,
};

/**
 * R13 "human-controlled": the pawn's control epoch at the hold's Begin and now, and whether a hand-over is pending
 * (ADR-0020 section 5.4 (c), ADR-0010 section 4.4). Supplied by the pawn-control view; never a controller or PlayerState.
 */
struct FRoundCompleterControl
{
	uint32 BeginControlEpoch = 0;
	uint32 CurrentControlEpoch = 0;
	bool bHandOverPending = false;

	bool IsHumanControlled() const { return BeginControlEpoch == CurrentControlEpoch && !bHandOverPending; }
};

/** The completer facts Valid(i) reads (Round fills them from the Roster, Session and its own takeover table). */
struct FRoundCompleterFacts
{
	bool bRostered = false;
	bool bOnTeam = false;
	EParticipantState State = EParticipantState::RoundEnded;
	bool bConnected = false;
	bool bDisconnectedThisTick = false;
	bool bAiDriven = false;
	FRoundCompleterControl Control;
};

namespace StealthRoundWin
{
	/** F2 Valid(i) over the completer facts: the first failing clause's code, in the enum's order, else Valid. */
	STEALTHGAMEPLAY_API ERoundCompletionValidity ValidateCompleter(const FRoundCompleterFacts& Facts);

	/** The reason partition (F2): Decision, Expiry, Kick are disjoint. */
	STEALTHGAMEPLAY_API bool IsDecision(const FRoundTransitionEvent& E);
	STEALTHGAMEPLAY_API bool IsExpiry(const FRoundTransitionEvent& E);
	STEALTHGAMEPLAY_API bool IsKick(const FRoundTransitionEvent& E);

	/** Zero(e) = e.Applied AND Old Active AND New Informant AND Phase Active AND NOT L AND ActiveCountAfter[e.Team] = 0 AND RosterCount(e.Team) >= 1 (and not a batch event). */
	STEALTHGAMEPLAY_API bool IsZeroing(const FRoundTransitionEvent& E, const FRoundWinContext& Context);

	/**
	 * F2 over one transition event: EliminationWin(W, e) (EndReason Elimination for a decision, ForfeitElimination for an
	 * expiry), Void(Abandoned) or Void(KickedLastActive); unset when no predicate holds.
	 */
	STEALTHGAMEPLAY_API TOptional<FRoundTerminalOutcome> EvaluateTransition(const FRoundTransitionEvent& E, const FRoundWinContext& Context);

	/** F2 ObjectiveWin(t, i) = Phase Active AND NOT L AND i final AND Valid(i): Win(t, ObjectiveWin); unset otherwise. */
	STEALTHGAMEPLAY_API TOptional<FRoundTerminalOutcome> EvaluateObjectiveCompletion(const FRoundObjectiveCompletion& Completion, const FRoundWinContext& Context);
}

/**
 * The single-assignment latch record (R15). TryTerminate succeeds only in Active while not terminated, storing the
 * outcome and TerminalKey; every later attempt returns false and counts as suppressed. Reset clears it for the next round.
 * FRoundMatchCore is its only writer and adds the queue's Latch(TerminalKey), the WinLatched / LateWinSuppressed entries
 * and the effects.
 */
class STEALTHGAMEPLAY_API FRoundTerminalLatch
{
public:
	bool TryTerminate(const FRoundTerminalOutcome& Outcome, FQueueKey Key, ERoundPhase Phase);

	bool IsTerminated() const { return bTerminated; }
	const FRoundTerminalOutcome& GetOutcome() const { return Outcome; }
	FQueueKey GetTerminalKey() const { return TerminalKey; }
	int32 GetSuppressedCount() const { return SuppressedCount; }

	void Reset();

private:
	bool bTerminated = false;
	FRoundTerminalOutcome Outcome;
	FQueueKey TerminalKey;
	int32 SuppressedCount = 0;
};
