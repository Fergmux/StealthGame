#include "StealthPlayerState.h"
#include "Net/UnrealNetwork.h"

AStealthPlayerState::AStealthPlayerState()
{
}

void AStealthPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AStealthPlayerState, Team);
	DOREPLIFETIME(AStealthPlayerState, LifeState);
	DOREPLIFETIME_CONDITION(AStealthPlayerState, NpcKills, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AStealthPlayerState, LastDefenceActivationTime, COND_OwnerOnly);
}

void AStealthPlayerState::Reset()
{
	Super::Reset();
	ResetForRound();
}

void AStealthPlayerState::ResetForRound()
{
	LifeState = EStealthLifeState::Active;
	NpcKills = 0;
	LastDefenceActivationTime = -1.0;
	LastPingTime = -1000.0;
}
