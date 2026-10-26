#include "Server/Core/Round/StealthRoundPhaseCore.h"

bool FRoundPhaseCore::IsLegalTransition(ERoundPhase From, ERoundPhase To)
{
	switch (From)
	{
	case ERoundPhase::Unknown: return To == ERoundPhase::Lobby;
	case ERoundPhase::Lobby: return To == ERoundPhase::Preparing;
	case ERoundPhase::Preparing: return To == ERoundPhase::Active || To == ERoundPhase::Reset;
	case ERoundPhase::Active: return To == ERoundPhase::Ended || To == ERoundPhase::Reset;
	case ERoundPhase::Ended: return To == ERoundPhase::Reset;
	case ERoundPhase::Reset: return To == ERoundPhase::Lobby;
	default: return false;
	}
}

ERoundAdmissionMode FRoundPhaseCore::AdmissionFor(ERoundPhase Phase)
{
	switch (Phase)
	{
	case ERoundPhase::Lobby: return ERoundAdmissionMode::Open;
	case ERoundPhase::Reset: return ERoundAdmissionMode::Closed;
	default: return ERoundAdmissionMode::RejoinOnly;
	}
}

bool FRoundPhaseCore::RequestPhase(ERoundPhase To, double ServerTime)
{
	if (!IsLegalTransition(Phase, To))
	{
		return false;
	}
	Write(To, ServerTime);
	return true;
}

void FRoundPhaseCore::ResetForNewMatch()
{
	Phase = ERoundPhase::Unknown;
	PhaseChangeCount = 0;
	RoundsPlayedCount = 0;
	CurrentRoundIndex = 0;
	PhaseEnteredServerTime = 0.0;
	LastResult = FRoundResult();
	MatchScore[0] = MatchScore[1] = 0;
	bCalledOff = false;
	ExpectedPawnCount = 0;
	ActiveAtServerTime = 0.0;
}

bool FRoundPhaseCore::EnterLobbyOnSessionEnding(double ServerTime)
{
	if (Phase == ERoundPhase::Lobby)
	{
		return false;
	}
	Write(ERoundPhase::Lobby, ServerTime);
	return true;
}

void FRoundPhaseCore::Write(ERoundPhase To, double ServerTime)
{
	const ERoundPhase Old = Phase;
	Phase = To;
	++PhaseChangeCount;
	PhaseEnteredServerTime = To == ERoundPhase::Active ? 0.0 : ServerTime;
	if (To == ERoundPhase::Active)
	{
		RoundsPlayedCount = CurrentRoundIndex;
	}
	if (To != ERoundPhase::Preparing)
	{
		ExpectedPawnCount = 0;
		ActiveAtServerTime = 0.0;
	}
	if (Session != nullptr)
	{
		Session->SetAdmissionMode(AdmissionFor(To));
	}
	if (OnPhaseChanged)
	{
		OnPhaseChanged(Old, To);
	}
}

void FRoundPhaseCore::SetLastResult(const FRoundResult& Result)
{
	LastResult = Result;
	if (Result.WinningTeam == 0 || Result.WinningTeam == 1)
	{
		++MatchScore[Result.WinningTeam];
	}
}

void FRoundPhaseCore::SetPreparingPublicFields(int32 InExpectedPawnCount, double InActiveAtServerTime)
{
	ExpectedPawnCount = InExpectedPawnCount;
	ActiveAtServerTime = InActiveAtServerTime;
}

FRoundPublicState FRoundPhaseCore::MakePublicState() const
{
	FRoundPublicState State;
	State.Phase = Phase;
	State.RoundIndex = CurrentRoundIndex;
	State.PhaseChangeCount = PhaseChangeCount;
	State.PhaseEnteredServerTime = PhaseEnteredServerTime;
	State.MatchScore[0] = MatchScore[0];
	State.MatchScore[1] = MatchScore[1];
	State.bCalledOff = bCalledOff;
	State.ExpectedPawnCount = ExpectedPawnCount;
	State.ActiveAtServerTime = ActiveAtServerTime;
	State.LastResult = LastResult;
	return State;
}
