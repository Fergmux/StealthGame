#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "StealthPlayerController.generated.h"

/** A message shown on the HUD (kill confirmation, removal notice, stage progress...). */
USTRUCT()
struct FStealthHudMessage
{
	GENERATED_BODY()

	UPROPERTY()
	FText Text;

	UPROPERTY()
	bool bDanger = false;

	double ExpireTime = 0.0;
};

/** A teammate's momentary location ping (informant communication; never tracks an enemy). */
USTRUCT()
struct FStealthPing
{
	GENERATED_BODY()

	UPROPERTY()
	FVector Location = FVector::ZeroVector;

	UPROPERTY()
	FString SenderName;

	double ExpireTime = 0.0;
};

/**
 * Player controller: receives server messages and team pings for the HUD, and exposes
 * development console commands (StealthStartRound, StealthEndRound, StealthSwitchTeam).
 */
UCLASS()
class STEALTHGAME_API AStealthPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	/** Server -> owning client: show a HUD message. */
	UFUNCTION(Client, Reliable)
	void ClientStealthMessage(const FText& Text, bool bDanger, float Duration);

	/** Server -> teammate clients: show a ping. */
	UFUNCTION(Client, Reliable)
	void ClientReceivePing(FVector_NetQuantize Location, const FString& SenderName);

	/** Console: start (or restart) a round immediately. */
	UFUNCTION(Exec)
	void StealthStartRound();

	/** Console: end the current round with no winner and return to waiting. */
	UFUNCTION(Exec)
	void StealthEndRound();

	/** Console: move this player to the other team (takes effect on the next spawn). */
	UFUNCTION(Exec)
	void StealthSwitchTeam();

	/**
	 * Development test driver (no-op in Shipping). Makes player PlayerIndex (0 = host) perform Action
	 * through the real server rule paths: strike_npc | strike_enemy | strike_informant | defend |
	 * interact | ping | report. Debug positioning teleports the pawn next to its target.
	 */
	UFUNCTION(Exec)
	void StealthBot(int32 PlayerIndex, const FString& Action);

	/** Development: 1 = overhead camera over the crowd, 0 = back to the pawn. */
	UFUNCTION(Exec)
	void StealthDebugCam(int32 Mode);

	const TArray<FStealthHudMessage>& GetMessages() const { return Messages; }
	const TArray<FStealthPing>& GetPings() const { return Pings; }

	/** Removes expired messages and pings. */
	void PruneExpired(double Now);

protected:
	UFUNCTION(Server, Reliable)
	void ServerStartRound();

	UFUNCTION(Server, Reliable)
	void ServerEndRound();

	UFUNCTION(Server, Reliable)
	void ServerSwitchTeam();

	UFUNCTION(Server, Reliable)
	void ServerStealthBot(int32 PlayerIndex, const FString& Action);

	/** Overhead debug camera (local only). */
	UPROPERTY(Transient)
	TObjectPtr<AActor> DebugCamera;

	TArray<FStealthHudMessage> Messages;
	TArray<FStealthPing> Pings;
};
