#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Server/Core/Round/StealthRoundGateways.h"
#include "Server/Core/Round/StealthRoundTypes.h"
#include "Templates/Function.h"

/**
 * FRoundPhaseCore: Round's five-phase machine (GDD round-match-framework States and Transitions, R4; ADR-0005
 * section 2; story rmf-028). Plain C++, held by URoundMatchStore on the game instance so it survives travel.
 *
 * Legal transitions (table-driven, IsLegalTransition):
 *   Unknown -> Lobby (the match opens)     Lobby -> Preparing (Start accepted and travel accepted)
 *   Preparing -> Active                    Preparing -> Reset (abort)
 *   Active -> Ended (win)                  Active -> Reset (host abort if allowed, or Void)
 *   Ended -> Reset                         Reset -> Lobby
 *   any phase -> Lobby on OnSessionEnding (EnterLobbyOnSessionEnding; log flushed, nothing released)
 * "Lobby -> Lobby" (a refused or rolled-back Start) is not a phase write: the phase simply stays.
 * Everything else is refused and changes nothing: no phase write, no admission push, no PhaseChangeCount step.
 *
 * Every accepted write pushes Session's admission mode for the new phase (R4): Lobby Open; Preparing, Active,
 * Ended RejoinOnly; Reset Closed. PhaseChangeCount is monotonic for the whole match (the FRoundPublicState
 * re-seed source after travel, story rmf-029). RoundsPlayedCount advances on Active entry.
 */
class STEALTHGAMEPLAY_API FRoundPhaseCore
{
public:
	/** Called after every accepted phase write (old, new). */
	using FOnPhaseChanged = TFunction<void(ERoundPhase /*Old*/, ERoundPhase /*New*/)>;

	/** Session may be null only in tests that assert the guard alone; production always passes Round's Session gateway. */
	explicit FRoundPhaseCore(IRoundSessionGateway* InSession = nullptr) : Session(InSession) {}

	void SetSessionGateway(IRoundSessionGateway* InSession) { Session = InSession; }
	void SetOnPhaseChanged(FOnPhaseChanged InCallback) { OnPhaseChanged = MoveTemp(InCallback); }

	/** True when From -> To is a legal single phase change (the States and Transitions table). */
	static bool IsLegalTransition(ERoundPhase From, ERoundPhase To);

	/** R4: the admission mode Round pushes on entering Phase. */
	static ERoundAdmissionMode AdmissionFor(ERoundPhase Phase);

	/** Applies From -> To if legal: phase write, PhaseChangeCount + 1, admission push, callback. False and no change otherwise. */
	bool RequestPhase(ERoundPhase To, double ServerTime);

	/** A new match on the same game instance: back to Unknown with zeroed counts, score and result (no admission push). */
	void ResetForNewMatch();

	/** The OnSessionEnding rule: any phase returns to Lobby (no-op when already there). */
	bool EnterLobbyOnSessionEnding(double ServerTime);

	ERoundPhase GetPhase() const { return Phase; }
	uint32 GetPhaseChangeCount() const { return PhaseChangeCount; }
	uint32 GetRoundsPlayedCount() const { return RoundsPlayedCount; }
	/** RoundsPlayedCount + 1: the index of the round being started, prepared or played. */
	uint32 GetCurrentRoundIndex() const { return CurrentRoundIndex; }
	double GetPhaseEnteredServerTime() const { return PhaseEnteredServerTime; }

	/** Start sets the index of the round it is starting (R5 step 7). */
	void SetCurrentRoundIndex(uint32 RoundIndex) { CurrentRoundIndex = RoundIndex; }

	/** The last round's public result (kept through Reset into Lobby, R20) and the match score. */
	const FRoundResult& GetLastResult() const { return LastResult; }
	void SetLastResult(const FRoundResult& Result);
	int32 GetMatchScore(int32 Team) const { return (Team == 0 || Team == 1) ? MatchScore[Team] : 0; }
	bool WasLastRoundCalledOff() const { return bCalledOff; }
	void SetCalledOff(bool bInCalledOff) { bCalledOff = bInCalledOff; }

	/** The FRoundPublicState this core describes (ExpectedPawnCount and ActiveAtServerTime are set by Preparing). */
	FRoundPublicState MakePublicState() const;

	/** Preparing's public fields (P6/P7; 0 outside Preparing). */
	void SetPreparingPublicFields(int32 InExpectedPawnCount, double InActiveAtServerTime);

private:
	void Write(ERoundPhase To, double ServerTime);

	IRoundSessionGateway* Session = nullptr;
	FOnPhaseChanged OnPhaseChanged;

	ERoundPhase Phase = ERoundPhase::Unknown;
	uint32 PhaseChangeCount = 0;
	uint32 RoundsPlayedCount = 0;
	uint32 CurrentRoundIndex = 0;
	double PhaseEnteredServerTime = 0.0;
	FRoundResult LastResult;
	int32 MatchScore[2] = { 0, 0 };
	bool bCalledOff = false;
	int32 ExpectedPawnCount = 0;
	double ActiveAtServerTime = 0.0;
};
