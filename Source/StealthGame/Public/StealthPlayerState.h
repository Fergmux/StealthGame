#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "StealthTypes.h"
#include "StealthPlayerState.generated.h"

/**
 * Per-player match state. Server-authoritative; replicated to clients.
 *
 * KNOWN PROTOTYPE LEAK (ADR-001 follow-up): APawn::PlayerState and this actor replicate to every
 * client, so a modified client could tell which pawns are players. Hiding the pawn->PlayerState
 * link from non-team connections needs a replication filter (Iris / ReplicationGraph) and is not
 * done in the prototype. Nothing in the shipped UI uses it to reveal enemies.
 */
UCLASS()
class STEALTHGAME_API AStealthPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	AStealthPlayerState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Reset() override;

	/** Team index (0 or 1), or STEALTH_NO_TEAM before assignment. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Stealth")
	uint8 Team = STEALTH_NO_TEAM;

	/** Active or informant. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Stealth")
	EStealthLifeState LifeState = EStealthLifeState::Active;

	/** Civilians this player has killed this round. Owner-only (it is a private counter). */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Stealth")
	int32 NpcKills = 0;

	/** Server time of the last defence activation; negative if unused this round. Owner-only. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Stealth")
	double LastDefenceActivationTime = -1.0;

	/** Server time of this player's last ping (server-side spam guard). */
	double LastPingTime = -1000.0;

	UFUNCTION(BlueprintPure, Category = "Stealth")
	bool IsActivePlayer() const { return LifeState == EStealthLifeState::Active; }

	UFUNCTION(BlueprintPure, Category = "Stealth")
	bool IsInformant() const { return LifeState == EStealthLifeState::Informant; }

	/** Server: clears per-round state at the start of a round. */
	void ResetForRound();
};
