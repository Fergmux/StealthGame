#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "StealthPlayerController.generated.h"

class URosterTeamViewComponent;

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
 * Player controller: receives server messages and team pings for the HUD, carries the owner-only C-TeamView Roster
 * component (story psr-062), and exposes development console commands. Round control moved to the host-only
 * stealth.round.* commands on the game instance (stories rmf-026/027): StealthStartRound, StealthEndRound and
 * StealthSwitchTeam and their Server RPCs are retired, and no Round command lives on this class.
 */
UCLASS()
class STEALTHGAME_API AStealthPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AStealthPlayerController();

	/** Server -> owning client: show a HUD message. */
	UFUNCTION(Client, Reliable)
	void ClientStealthMessage(const FText& Text, bool bDanger, float Duration);

	/** Server -> teammate clients: show a ping. */
	UFUNCTION(Client, Reliable)
	void ClientReceivePing(FVector_NetQuantize Location, const FString& SenderName);

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

	/** Development: logs the production Round phase and Roster counts (server) or the replicated public state (client). */
	UFUNCTION(Exec)
	void StealthDumpProd();

	/**
	 * A command a remote client sent through ServerExec runs inside FRoundRemoteExecScope, so Round's host check
	 * answers NotHost (stories rmf-026/027). Local console input is unchanged.
	 */
	virtual FString ConsoleCommand(const FString& Command, bool bWriteToLog = true) override;

	const TArray<FStealthHudMessage>& GetMessages() const { return Messages; }
	const TArray<FStealthPing>& GetPings() const { return Pings; }

	/** The owner-only C-TeamView Roster part (written only by the Roster push step). */
	URosterTeamViewComponent* GetTeamView() const { return TeamView; }

	/** Removes expired messages and pings. */
	void PruneExpired(double Now);

protected:
	UFUNCTION(Server, Reliable)
	void ServerStealthBot(int32 PlayerIndex, const FString& Action);

	/** C-TeamView, Roster part (ADR-0001): replicated to the owning connection only. */
	UPROPERTY(VisibleAnywhere, Category = "Stealth")
	TObjectPtr<URosterTeamViewComponent> TeamView;

	/** Overhead debug camera (local only). */
	UPROPERTY(Transient)
	TObjectPtr<AActor> DebugCamera;

	TArray<FStealthHudMessage> Messages;
	TArray<FStealthPing> Pings;
};
