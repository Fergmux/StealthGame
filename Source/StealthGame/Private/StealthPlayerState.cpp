#include "StealthPlayerState.h"
#include "Net/UnrealNetwork.h"

#if !UE_BUILD_SHIPPING
#include "Spikes/StealthIdentitySpike.h" // THROWAWAY SPIKE hook (ADR-0001 S1/S2/S14, 2026-10-25)
#endif

AStealthPlayerState::AStealthPlayerState()
{
}

void AStealthPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AStealthPlayerState, Team);
	DOREPLIFETIME(AStealthPlayerState, LifeState);
	// NpcKills is server only (story asn-015); the defence stamp is the pawn's server-only UDefenceStateComponent (asn-016).

#if !UE_BUILD_SHIPPING
	StealthSpike::ApplyPlayerStateTrims(StaticClass(), OutLifetimeProps); // SPIKE: no-op unless stealth.Spike.IdentityTrims is set
#endif
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
	WrongTargetTally = 0;
	bExiledByPendingResult = false;
	LastPingTime = -1000.0;
}
