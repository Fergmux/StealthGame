#include "StealthGameState.h"
#include "StealthCityGenerator.h"

#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"

AStealthGameState::AStealthGameState()
{
	TeamStage.Init(1, STEALTH_NUM_TEAMS);
}

void AStealthGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AStealthGameState, RoundPhase);
	DOREPLIFETIME(AStealthGameState, TeamStage);
	DOREPLIFETIME(AStealthGameState, WinningTeam);
	DOREPLIFETIME(AStealthGameState, WinReason);
	DOREPLIFETIME(AStealthGameState, PhaseEndServerTime);
	DOREPLIFETIME(AStealthGameState, RoundSeed);
}

void AStealthGameState::ResetRoundState(int32 NewSeed)
{
	TeamStage.Init(1, STEALTH_NUM_TEAMS);
	WinningTeam = STEALTH_NO_TEAM;
	WinReason = EStealthWinReason::None;
	RoundSeed = NewSeed;
}

void AStealthGameState::BuildCityForSeed() const
{
	for (TActorIterator<AStealthCityGenerator> It(GetWorld()); It; ++It)
	{
		if (It->GetBuiltSeed() != RoundSeed)
		{
			It->Build(RoundSeed);
		}
	}
}

void AStealthGameState::OnRep_RoundSeed()
{
	BuildCityForSeed();
}
