#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "StealthTypes.h"
#include "Server/Core/Round/StealthRoundPublicStateMirror.h"
#include "View/StealthRoundPublicState.h"
#include "StealthGameState.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnRoundPublicStateChanged, const FRoundPublicState& /*State*/);

/**
 * Round state replicated to every client. Contains no per-player identity information.
 *
 * RoundPublicState (story rmf-029, GDD R20) is the production Round's public state: one property, one RepNotify,
 * replicated unconditionally (every viewer gets identical bytes). Phase 0 is an explicit Unknown, so a late joiner
 * whose first value equals the class default still runs the handler; the server runs the same handler explicitly
 * after each write (a server receives no RepNotify). The prototype fields below are kept for the prototype HUD.
 */
UCLASS()
class STEALTHGAME_API AStealthGameState : public AGameStateBase, public IRoundPublicStateSink
{
	GENERATED_BODY()

public:
	AStealthGameState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;

	// IRoundPublicStateSink
	virtual void WriteRoundPublicState(const FRoundPublicState& State) override;

	/** The production Round's public state (replicated, unconditional). */
	const FRoundPublicState& GetRoundPublicState() const { return RoundPublicState; }

	/** Times the public-state handler ran on this machine (late-joiner and host-call checks, story rmf-029). */
	int32 GetRoundPublicStateHandlerCalls() const { return RoundPublicStateHandlerCalls; }
	/** The copy the handler last diffed against. */
	const FRoundPublicState& GetPreviousRoundPublicState() const { return PreviousRoundPublicState; }
	/** True when the last handler run saw a PhaseChangeCount gap (a skipped phase). */
	bool DidHandlerSeePhaseSkip() const { return bHandlerSawPhaseSkip; }

	/** Raised on every machine after the handler ran. */
	FOnRoundPublicStateChanged OnRoundPublicStateChanged;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Stealth")
	EStealthRoundPhase RoundPhase = EStealthRoundPhase::WaitingToStart;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Stealth")
	uint8 WinningTeam = STEALTH_NO_TEAM;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Stealth")
	EStealthWinReason WinReason = EStealthWinReason::None;

	/** Server time at which the pending round start or restart happens (for the HUD countdown). */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Stealth")
	double PhaseEndServerTime = 0.0;

	/** Seed of the current round; every machine builds the city layout from it. */
	UPROPERTY(ReplicatedUsing = OnRep_RoundSeed, BlueprintReadOnly, Category = "Stealth")
	int32 RoundSeed = 0;

	/** Server: puts both teams back at stage 1 and clears the result. */
	void ResetRoundState(int32 NewSeed);

	/** Builds the city for RoundSeed on this machine (no-op without an AStealthCityGenerator). */
	void BuildCityForSeed() const;

protected:
	UFUNCTION()
	void OnRep_RoundSeed();

	UFUNCTION()
	void OnRep_RoundPublicState();

	/** The one handler (client RepNotify and the host's explicit call): diffs against the previous copy. */
	void HandleRoundPublicState();

	UPROPERTY(ReplicatedUsing = OnRep_RoundPublicState)
	FRoundPublicState RoundPublicState;

	FRoundPublicState PreviousRoundPublicState;
	int32 RoundPublicStateHandlerCalls = 0;
	bool bHandlerSawPhaseSkip = false;
};
