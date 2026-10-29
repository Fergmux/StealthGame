#include "StealthGameState.h"
#include "Composition/StealthConcealmentWiring.h"
#include "StealthCityGenerator.h"

#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"

AStealthGameState::AStealthGameState()
{
}

void AStealthGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AStealthGameState, RoundPhase);
	DOREPLIFETIME(AStealthGameState, WinningTeam);
	DOREPLIFETIME(AStealthGameState, WinReason);
	DOREPLIFETIME(AStealthGameState, PhaseEndServerTime);
	DOREPLIFETIME(AStealthGameState, RoundSeed);
	// R20: unconditional (no owner or team condition): every viewer receives identical bytes.
	DOREPLIFETIME(AStealthGameState, RoundPublicState);
}

void AStealthGameState::BeginPlay()
{
	Super::BeginPlay();
	// Clients also read the struct at begin-play (it may have arrived with the initial bunch).
	if (!HasAuthority() && RoundPublicState.Phase != ERoundPhase::Unknown && RoundPublicStateHandlerCalls == 0)
	{
		HandleRoundPublicState();
	}
}

void AStealthGameState::WriteRoundPublicState(const FRoundPublicState& State)
{
	RoundPublicState = State;
	// Every Round mutation forces a net update (the GameState otherwise sends at its own rate, up to 100 ms late).
	ForceNetUpdate();
	HandleRoundPublicState();
}

void AStealthGameState::OnRep_RoundPublicState()
{
	HandleRoundPublicState();
}

void AStealthGameState::HandleRoundPublicState()
{
	++RoundPublicStateHandlerCalls;
	// One RepNotify can carry a phase and a score change; PhaseChangeCount detects a skipped phase.
	bHandlerSawPhaseSkip = PreviousRoundPublicState.Phase != ERoundPhase::Unknown
		&& RoundPublicState.PhaseChangeCount > PreviousRoundPublicState.PhaseChangeCount + 1;
	PreviousRoundPublicState = RoundPublicState;
	OnRoundPublicStateChanged.Broadcast(RoundPublicState);
}

void AStealthGameState::ResetRoundState(int32 NewSeed)
{
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
	// Concealment story con-002: every machine builds the same bush runtime from the (stand-in) Manifest's BushSites once
	// the seeded layout placed the bushes; the server validates V-BUSH-1 again at the layout-ready step.
	StealthConcealmentLayout::RebuildBushRuntime(GetWorld());
}

void AStealthGameState::OnRep_RoundSeed()
{
	BuildCityForSeed();
}
