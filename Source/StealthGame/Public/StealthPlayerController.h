#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Server/StealthQueuePayloads.h"
#include "View/CommandReplyTypes.h"
#include "View/Input/PawnCommandClient.h"
#include "View/NetPawnCommand.h"
#include "StealthPlayerController.generated.h"

class URosterTeamViewComponent;
class UPawnCommandClient;
class UDefenceOwnerComponent;

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

#if !UE_BUILD_SHIPPING
/** One possession event, recorded for spike V2 (story iif-009). Development builds only. */
struct FStealthPossessionTraceEvent
{
	enum class EKind : uint8
	{
		/** Server: OnPossess advanced ServerPossessionEpoch to Epoch. */
		ServerOnPossess,
		/** Owner: ClientSetPossessionEpoch(Epoch) arrived. */
		OwnerEpoch,
		/** Owner: ClientRestart ran; Epoch is OwnerPossessionEpoch at that moment. */
		OwnerRestart,
	};
	EKind Kind = EKind::ServerOnPossess;
	uint16 Epoch = 0;
	uint64 Frame = 0;
	double Seconds = 0.0;
	FString Controller;
	FString Pawn;
	/** Recorded in a client world (the owner of a remote connection). */
	bool bClientWorld = false;
	/** The pawn's network GUID value in the recording world (identical on server and owner), 0 when none. */
	uint64 PawnGuid = 0;
};
#endif

/**
 * Player controller: the pawn command transport (ADR-0011 section 2, story iif-094), receives server messages and team
 * pings for the HUD, carries the owner-only C-TeamView Roster component (story psr-062), and exposes development
 * console commands.
 *
 * Command transport (ADR-0011 Key Interfaces): gameplay input is bound HERE, not on the pawn (SetupInputComponent binds
 * every IA_* action through UEnhancedInputComponent into the client-only UPawnCommandClient); discrete commands travel
 * only in the reliable, validated ServerPawnCommand; owner replies return in one reliable ClientCommandReplies batch;
 * the server-owned possession epoch is sent by ClientSetPossessionEpoch in OnPossess before the engine's ClientRestart.
 * The client never names the pawn it commands. The listen host's controller calls IngestCommand(HostLocal) directly
 * (same ingress, same reply flush). IA_Ping uses ServerTeamPing, never ServerPawnCommand. Round control moved to the host-only
 * stealth.round.* commands on the game instance (stories rmf-026/027): StealthStartRound, StealthEndRound and
 * StealthSwitchTeam and their Server RPCs are retired, and no Round command lives on this class.
 */
UCLASS()
class STEALTHGAME_API AStealthPlayerController : public APlayerController, public IPawnCommandUplink
{
	GENERATED_BODY()

public:
	AStealthPlayerController();

	// ---- Pawn command transport (ADR-0011) ----------------------------------------------------------------

	/** Owner -> server: one discrete command. WithValidation refuses only structurally impossible payloads. */
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerPawnCommand(const FNetPawnCommand& Cmd);

	/** Server -> owner: every reply of one drain pass, in queue-key order (at most one call per drain). */
	UFUNCTION(Client, Reliable)
	void ClientCommandReplies(const TArray<FCommandReply>& Replies);

	/** Server -> owner: the possession epoch, sent in OnPossess before ClientRestart (V2). */
	UFUNCTION(Client, Reliable)
	void ClientSetPossessionEpoch(uint16 Epoch);

	/** Owner -> server: a teammate ping (IA_Ping; the informant-pings epic owns the handler). */
	UFUNCTION(Server, Reliable)
	void ServerTeamPing(FVector_NetQuantize Location);

	/** The client-only command builder. */
	UPawnCommandClient* GetCommandClient() const { return CommandClient; }

	/** Server: possessions so far (incremented in OnPossess). Owner: the last epoch the server delivered. */
	uint16 GetServerPossessionEpoch() const { return ServerPossessionEpoch; }
	uint16 GetOwnerPossessionEpoch() const { return OwnerPossessionEpoch; }

	/** Server: the control epoch recorded at OnPossess (stamped by ingress; the factory's epoch when ADR-0010 4.4 lands). */
	uint32 GetPossessedControlEpoch() const { return PossessedControlEpoch; }

	/** Server: this connection's reply sink. */
	FReplySinkId GetReplySink() const { return ReplySink; }

	/** Owner: replies received so far (development HUD and tests). */
	int32 GetReceivedReplyCount() const { return ReceivedReplyCount; }

	/** Owner: the latest received reply entries (at most MaxKeptReplies, oldest first; tests and story asn-024's matcher). */
	const TArray<FCommandReply>& GetReceivedReplies() const { return ReceivedReplies; }

	/**
	 * Owner: the "Wrong targets" tally the HUD shows: FStrikeResult.TallyAfter of the latest result plate, changed only at
	 * that plate's show time (HUD-R7; story asn-015 stand-in until story asn-024).
	 */
	int32 GetShownWrongTargetTally() const;

	/** The owner-only cooldown end (story asn-016): the only defence datum a client holds. */
	UDefenceOwnerComponent* GetDefenceOwner() const { return DefenceOwner; }

#if !UE_BUILD_SHIPPING
	/** Every possession event of every AStealthPlayerController in this process (spike V2); cleared by the spike. */
	static TArray<FStealthPossessionTraceEvent>& GetPossessionTrace();

	/** GFrameCounter of every FlushPressedKeys on any AStealthPlayerController (spike V5); cleared by the spike. */
	static TArray<uint64>& GetFlushPressedKeysTrace();
#endif

	/** Records the flush for spike V5 (development builds), then flushes. */
	virtual void FlushPressedKeys() override;

	// IPawnCommandUplink
	virtual void SendPawnCommand(const FNetPawnCommand& Cmd, APawn* HostLocalPawnRef) override;
	virtual void SendTeamPing(const FVector& Location) override;

	virtual void SetupInputComponent() override;
	virtual void OnPossess(APawn* InPawn) override;
	/** A body release (UStealthPawnFactory::IsReleasingBody, story asn-019) keeps the view target on the falling body. */
	virtual void OnUnPossess() override;
	virtual void ClientRestart_Implementation(APawn* NewPawn) override;
	/** Owner: an unpossession or a new pawn arrived by replication; outstanding Begins are ended first (story iif-013 d). */
	virtual void OnRep_Pawn() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	/** Server: forwards to a non-Shipping spike hook (THROWAWAY, IVP S10 re-run story-056, ADR-0003 S10-R4); otherwise the engine default (nothing). */
	virtual void NotifyActorChannelFailure(UActorChannel* ActorChan) override;

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

	/**
	 * Development: injects one simulated key event into this player's Enhanced Input (as a real key would arrive), so a
	 * PIE session driven over the MCP can exercise the production input path. Down: 1 press, 0 release. No-op in Shipping.
	 */
	UFUNCTION(Exec)
	void StealthInputKey(const FString& KeyName, int32 Down);

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

	/** Builds the stage-1 context (mailbox, reply sink, recorded control epoch) and runs IngestCommand. Server. */
	void IngestOwnCommand(const FNetPawnCommand& Cmd, APawn* HostLocalPawnRef);

	/** The client-only command builder (ADR-0011 section 9); inert on a non-local controller. */
	UPROPERTY(VisibleAnywhere, Category = "Stealth")
	TObjectPtr<UPawnCommandClient> CommandClient;

	uint16 ServerPossessionEpoch = 0;
	uint16 OwnerPossessionEpoch = 0;
	uint32 PossessedControlEpoch = 0;
	FReplySinkId ReplySink;
	int32 ReceivedReplyCount = 0;
	TArray<FCommandReply> ReceivedReplies;
	int32 ShownWrongTargetTally = 0;
	/** The round (FRoundPublicState::RoundIndex) of the shown tally: a new round shows 0 (HUD-R7). */
	uint32 ShownTallyRoundIndex = 0;

	/** Owner, STAND-IN for story asn-024's plates: the result's HUD message at its contact time (DefenceResult at once). */
	void PresentOwnerResult(const FCommandReply& Reply);

	/** The owner-only cooldown end (story asn-016), replicated to the owning connection only. */
	UPROPERTY(VisibleAnywhere, Category = "Stealth")
	TObjectPtr<UDefenceOwnerComponent> DefenceOwner;

	/** C-TeamView, Roster part (ADR-0001): replicated to the owning connection only. */
	UPROPERTY(VisibleAnywhere, Category = "Stealth")
	TObjectPtr<URosterTeamViewComponent> TeamView;

	/** Overhead debug camera (local only). */
	UPROPERTY(Transient)
	TObjectPtr<AActor> DebugCamera;

	TArray<FStealthHudMessage> Messages;
	TArray<FStealthPing> Pings;
};
