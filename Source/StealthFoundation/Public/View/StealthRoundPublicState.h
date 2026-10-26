#pragma once

#include "CoreMinimal.h"

#include "StealthRoundPublicState.generated.h"

/**
 * Round & Match Framework's public, client-safe wire types (GDD round-match-framework R20; ADR-0002 section 8.10;
 * ADR-0007 section 2: View/ types compile on both sides; story rmf-029).
 *
 * FRoundPublicState is one replicated property on the GameState with one RepNotify, replicated unconditionally:
 * every viewer receives identical bytes. Nothing in it is per-team before Ended (opponent counts, grace deadlines,
 * queue keys and the Event Log never appear here; the field audit is story rmf-080).
 */

/** The five Round phases plus the explicit "never received" value 0 (R20: the late-joiner RepNotify rule). */
UENUM()
enum class ERoundPhase : uint8
{
	/** Class default: a late joiner whose first replicated value is a real phase always runs the handler. */
	Unknown = 0,
	Lobby = 1,
	Preparing = 2,
	Active = 3,
	Ended = 4,
	Reset = 5,
};

/**
 * One team's round summary (R9 (a) TeamSummary[t]; user decision 2026-10-25, HUD-E21; story rmf-054). Team aggregates
 * only: three saturating counts, no participant id and no time (IVP R3 RoundTeamSummary row). Computed in the latch
 * step and published only in the round-end step; all zero in every earlier FRoundPublicState of the round.
 */
USTRUCT()
struct STEALTHFOUNDATION_API FRoundTeamSummary
{
	GENERATED_BODY()

	/** 0-3: the team's completed stages, the final counted only for the objective winner (Objectives' server store). */
	UPROPERTY()
	uint8 StepsCompleted = 0;

	/** Members of the team whose State is Informant in the latch record, every cause alike (forfeits included). */
	UPROPERTY()
	uint8 EliminatedCount = 0;

	/** The team's WrongTarget KillResolved rows this round (NPC and informant kills alike), saturating at 255. */
	UPROPERTY()
	uint8 WrongTargetCount = 0;

	bool IsZero() const { return StepsCompleted == 0 && EliminatedCount == 0 && WrongTargetCount == 0; }

	bool operator==(const FRoundTeamSummary& Other) const
	{
		return StepsCompleted == Other.StepsCompleted && EliminatedCount == Other.EliminatedCount && WrongTargetCount == Other.WrongTargetCount;
	}
	bool operator!=(const FRoundTeamSummary& Other) const { return !(*this == Other); }
};

/** The public end-of-round result (R20: kept as LastResult through Reset into the lobby). R9 (a): no duration field. */
USTRUCT()
struct STEALTHFOUNDATION_API FRoundResult
{
	GENERATED_BODY()

	/** 0 or 1; 255 for none (an abort or void). */
	UPROPERTY()
	uint8 WinningTeam = 255;

	/** The public end reason: 0 none, 1 objective, 2 elimination (server-only reasons fold into these two, R19). */
	UPROPERTY()
	uint8 PublicReason = 0;

	/** The RoundIndex the result belongs to (0 = no result yet). */
	UPROPERTY()
	uint32 RoundIndex = 0;

	/** R9 (a) TeamSummary[2], indexed by team (story rmf-054). */
	UPROPERTY()
	FRoundTeamSummary TeamSummary[2];

	bool operator==(const FRoundResult& Other) const
	{
		return WinningTeam == Other.WinningTeam && PublicReason == Other.PublicReason && RoundIndex == Other.RoundIndex
			&& TeamSummary[0] == Other.TeamSummary[0] && TeamSummary[1] == Other.TeamSummary[1];
	}
	bool operator!=(const FRoundResult& Other) const { return !(*this == Other); }
};

/** Round's public state (R20). Written only by the Round mirror on the server; read by every client and the host UI. */
USTRUCT()
struct STEALTHFOUNDATION_API FRoundPublicState
{
	GENERATED_BODY()

	/** The phase; Unknown (0) only before the first write reaches this viewer. */
	UPROPERTY()
	ERoundPhase Phase = ERoundPhase::Unknown;

	/** RoundsPlayedCount + 1 for the round being prepared or played (0 before the first start). */
	UPROPERTY()
	uint32 RoundIndex = 0;

	/** Incremented on every phase write; never moves backward, also across travel (held in the Round phase core). */
	UPROPERTY()
	uint32 PhaseChangeCount = 0;

	/** Server time the phase was entered, for Lobby, Preparing, Ended and Reset only; 0 during Active. */
	UPROPERTY()
	double PhaseEnteredServerTime = 0.0;

	/** Rounds won per team this match. */
	UPROPERTY()
	int32 MatchScore[2] = { 0, 0 };

	/** The last round was called off (abort or void). */
	UPROPERTY()
	bool bCalledOff = false;

	/** Pawns the round start expects (players plus NPCs); 0 outside Preparing. */
	UPROPERTY()
	int32 ExpectedPawnCount = 0;

	/** Server time Active begins; non-zero only in Preparing after P7. */
	UPROPERTY()
	double ActiveAtServerTime = 0.0;

	/** The result of the last ended round; kept through Reset into Lobby. */
	UPROPERTY()
	FRoundResult LastResult;

	bool operator==(const FRoundPublicState& Other) const
	{
		return Phase == Other.Phase && RoundIndex == Other.RoundIndex && PhaseChangeCount == Other.PhaseChangeCount
			&& PhaseEnteredServerTime == Other.PhaseEnteredServerTime && MatchScore[0] == Other.MatchScore[0]
			&& MatchScore[1] == Other.MatchScore[1] && bCalledOff == Other.bCalledOff && ExpectedPawnCount == Other.ExpectedPawnCount
			&& ActiveAtServerTime == Other.ActiveAtServerTime && LastResult == Other.LastResult;
	}
	bool operator!=(const FRoundPublicState& Other) const { return !(*this == Other); }
};

/** Short display name of a phase (development HUD and dumps). */
STEALTHFOUNDATION_API const TCHAR* LexRoundPhase(ERoundPhase Phase);

/** FRoundResult::PublicReason values (R15 last bullet: exactly two public win types; a forfeit is an elimination). */
namespace RoundPublicReason
{
	inline constexpr uint8 None = 0;
	inline constexpr uint8 Objective = 1;
	inline constexpr uint8 Elimination = 2;
}

/**
 * The banner reason text of a public reason (HUD-E18's approved phrasings; R15: a forfeit shows the elimination text,
 * never a separate forfeit phrase, R20). Example: GetRoundEndReasonText(RoundPublicReason::Elimination) is "every
 * rival eliminated for the round". Empty for RoundPublicReason::None.
 */
STEALTHFOUNDATION_API FText GetRoundEndReasonText(uint8 PublicReason);
