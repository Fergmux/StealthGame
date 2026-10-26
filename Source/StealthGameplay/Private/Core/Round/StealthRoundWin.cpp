#include "Server/Core/Round/StealthRoundWin.h"

namespace StealthRoundWin
{
	namespace
	{
		int32 TeamIndex(ERosterTeam Team)
		{
			return Team == ERosterTeam::Team0 ? 0 : (Team == ERosterTeam::Team1 ? 1 : INDEX_NONE);
		}
	}

	ERoundCompletionValidity ValidateCompleter(const FRoundCompleterFacts& Facts)
	{
		if (!Facts.bRostered)
		{
			return ERoundCompletionValidity::NotRostered;
		}
		if (!Facts.bOnTeam)
		{
			return ERoundCompletionValidity::WrongTeam;
		}
		if (Facts.State != EParticipantState::Active)
		{
			return ERoundCompletionValidity::CompleterInformant;
		}
		if (Facts.bDisconnectedThisTick)
		{
			return ERoundCompletionValidity::CompleterDisconnectedThisTick;
		}
		if (!Facts.bConnected)
		{
			return ERoundCompletionValidity::CompleterDisconnected;
		}
		if (Facts.bAiDriven || !Facts.Control.IsHumanControlled())
		{
			return ERoundCompletionValidity::CompleterAiDriven;
		}
		return ERoundCompletionValidity::Valid;
	}

	bool IsDecision(const FRoundTransitionEvent& E)
	{
		return E.Reason == ERosterReason::Assassinated || E.Reason == ERosterReason::ReversedAgainst || E.Reason == ERosterReason::StrikeLimit;
	}

	bool IsExpiry(const FRoundTransitionEvent& E)
	{
		return E.Reason == ERosterReason::Disconnect && !E.bKicked;
	}

	bool IsKick(const FRoundTransitionEvent& E)
	{
		return E.Reason == ERosterReason::Disconnect && E.bKicked;
	}

	bool IsZeroing(const FRoundTransitionEvent& E, const FRoundWinContext& Context)
	{
		const int32 Team = TeamIndex(E.Team);
		// F2 proof item 5: batch events (OnRosterRoundEnded, OnRosterReset) are never evaluated.
		return !E.bBatch && E.bApplied && Team != INDEX_NONE && E.Old == EParticipantState::Active && E.New == EParticipantState::Informant
			&& Context.Phase == ERoundPhase::Active && !Context.bTerminated && E.ActiveCountAfter[Team] == 0 && Context.RosterCount[Team] >= 1;
	}

	TOptional<FRoundTerminalOutcome> EvaluateTransition(const FRoundTransitionEvent& E, const FRoundWinContext& Context)
	{
		if (!IsZeroing(E, Context))
		{
			return TOptional<FRoundTerminalOutcome>();
		}
		const int32 W = 1 - TeamIndex(E.Team);
		// A team with RosterCount 0 never wins by elimination (F2 proof item 6: a 1v0 dev round has only ObjectiveWin).
		if (Context.RosterCount[W] < 1)
		{
			return TOptional<FRoundTerminalOutcome>();
		}
		if (IsDecision(E))
		{
			if (E.ActiveCountAfter[W] >= 1)
			{
				return FRoundTerminalOutcome::Win(static_cast<uint8>(W), ERoundEndReason::Elimination);
			}
			return TOptional<FRoundTerminalOutcome>();
		}
		if (IsExpiry(E))
		{
			return Context.ConnectedActiveCount[W] >= 1
				? FRoundTerminalOutcome::Win(static_cast<uint8>(W), ERoundEndReason::ForfeitElimination)
				: FRoundTerminalOutcome::Void(ERoundEndReason::VoidAbandoned);
		}
		if (IsKick(E))
		{
			return FRoundTerminalOutcome::Void(ERoundEndReason::VoidKickedLastActive);
		}
		return TOptional<FRoundTerminalOutcome>();
	}

	TOptional<FRoundTerminalOutcome> EvaluateObjectiveCompletion(const FRoundObjectiveCompletion& Completion, const FRoundWinContext& Context)
	{
		// R15: preliminary stage completions do not end the round.
		if (!Completion.bFinal || Context.Phase != ERoundPhase::Active || Context.bTerminated || !Completion.IsValid() || Completion.Team > 1)
		{
			return TOptional<FRoundTerminalOutcome>();
		}
		return FRoundTerminalOutcome::Win(Completion.Team, ERoundEndReason::ObjectiveWin);
	}
}

bool FRoundTerminalLatch::TryTerminate(const FRoundTerminalOutcome& InOutcome, FQueueKey Key, ERoundPhase Phase)
{
	if (bTerminated || Phase != ERoundPhase::Active)
	{
		++SuppressedCount;
		return false;
	}
	bTerminated = true;
	Outcome = InOutcome;
	TerminalKey = Key;
	return true;
}

void FRoundTerminalLatch::Reset()
{
	bTerminated = false;
	Outcome = FRoundTerminalOutcome();
	TerminalKey = FQueueKey();
	SuppressedCount = 0;
}
