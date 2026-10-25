#pragma once

// THROWAWAY spike code for the session-travel spikes S3 (story 003), S4 (story 004) and the net-emulation
// measurement (story 002), in the shape ADR-0005 sections 1, 2, 4 and 5 prescribe:
//   - four game-instance stores (stand-ins for Session, Roster, Round and Tuning-preset), the only
//     authoritative match state, keyed by ParticipantId / IdentityKey, never by controller or PlayerState;
//   - a game mode with seamless travel on that re-binds the replaced controller and PlayerState from the
//     stores on arrival (HandleSeamlessTravelPlayer, the host through PostSeamlessTravel, E19), re-binds a
//     fresh login by key (hard travel), and adopts a retained PlayerState on a rejoin (Login stashes the
//     pending reclaim, the controller's InitPlayerState adopts it, E15);
//   - a PlayerState that never broadcasts the engine's entered/left/renamed messages (E10) and is kept,
//     not destroyed, when its controller leaves mid-round (OnDeactivated, E9);
//   - an owner-only C-TeamView struct on the PlayerState and a single owner-only acknowledgement RPC.
// It derives from the prototype game classes so the prototype round, crowd and pawns keep running. Selected
// with `?game=/Script/StealthTestHarness.StealthSpikeSessionGameMode` by the harness scenarios only.
// Stores exist only in harness processes (-StealthHarnessScenario=); PIE and the editor never create them.

#include "CoreMinimal.h"
#include "StealthGameMode.h"
#include "StealthPlayerController.h"
#include "StealthPlayerState.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "StealthSpikeSession.generated.h"

class APawn;

namespace StealthSpikeSession
{
	/** Client: set when a non-seamless travel starts (the connection will drop and come back); cleared on reconnect. */
	bool& ClientExpectsReconnect();

	/** The game-mode URL option of the spike game mode. */
	inline const TCHAR* GameOption() { return TEXT("?game=/Script/StealthTestHarness.StealthSpikeSessionGameMode"); }
}

/** The owner-only team view of one participant (C-TeamView stand-in). */
USTRUCT()
struct FStealthSpikeTeamView
{
	GENERATED_BODY()

	UPROPERTY()
	int32 ParticipantId = -1;

	UPROPERTY()
	uint8 Team = 255;

	/** Participant ids of the team-mates, sorted. */
	UPROPERTY()
	TArray<int32> TeammateIds;

	/** Roster round epoch the view was written for. */
	UPROPERTY()
	int32 RoundEpoch = 0;

	/** CRC of the fields above (the acknowledgement carries it). */
	UPROPERTY()
	uint32 Hash = 0;

	/** Recomputes Hash from the other fields. */
	void Seal();
};

/** Base of the four spike stores: a serialised content string and its CRC, for the before/after comparison. */
UCLASS(Abstract)
class UStealthSpikeStoreBase : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** Only harness processes create the stores. */
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	/** Canonical text of the match-scoped content (no pointers, no transient bookkeeping). */
	virtual FString DescribeContent() const PURE_VIRTUAL(UStealthSpikeStoreBase::DescribeContent, return FString(););

	/** CRC32 of DescribeContent(). */
	uint32 ContentHash() const;
};

/** Session stand-in: identity keys, connection states, match nonce, supersede and kick counters. */
UCLASS()
class UStealthSpikeSessionStore : public UStealthSpikeStoreBase
{
	GENERATED_BODY()

public:
	enum class EConnection : uint8 { Connected, Disconnected };

	struct FEntry
	{
		int32 ParticipantId = -1;
		EConnection State = EConnection::Connected;
		int32 Generation = 0;
	};

	virtual FString DescribeContent() const override;

	/** Begins the match once (first login): draws the match nonce. */
	void BeginMatchIfNeeded();

	TMap<FString, FEntry> Entries;
	FGuid MatchNonce;
	int32 SupersedeCount = 0;

	// Transient bookkeeping, never part of the content: retained PlayerStates and bodies of disconnected
	// participants, and the acknowledgements of the current epoch (ParticipantId -> view hash).
	TMap<FString, TWeakObjectPtr<APlayerState>> RetainedPlayerStates;
	TMap<FString, TWeakObjectPtr<APawn>> RetainedPawns;
	TMap<int32, uint32> Acks;
	int32 AckEpoch = 0;
	/** True from a hard (fallback) travel's start until the world is back: connection closes then are travel, not leaves. */
	bool bHardTravelInProgress = false;
};

/** Roster stand-in: participant records, match id, round epoch, event sequence. */
UCLASS()
class UStealthSpikeRosterStore : public UStealthSpikeStoreBase
{
	GENERATED_BODY()

public:
	struct FRecord
	{
		int32 Id = -1;
		FString KeyPrefix;
		FString Name;
		uint8 Team = 255;
	};

	virtual FString DescribeContent() const override;

	/** The record of Key, created (with Team) when unknown. */
	FRecord& FindOrRegister(const FString& Key, const FString& Name, uint8 Team);

	/** The record of Key, or null. */
	const FRecord* Find(const FString& Key) const;

	/** The owner-only team view of participant Id at the current epoch. */
	FStealthSpikeTeamView BuildTeamView(int32 Id) const;

	TMap<FString, FRecord> Records;
	FGuid MatchId;
	int32 RoundEpoch = 0;
	int32 EventSeq = 0;
	int32 NextId = 0;
};

/** Round stand-in: rounds played and match score. */
UCLASS()
class UStealthSpikeRoundStore : public UStealthSpikeStoreBase
{
	GENERATED_BODY()

public:
	virtual FString DescribeContent() const override;

	int32 RoundsPlayed = 0;
	int32 Score[2] = { 0, 0 };
};

/** Tuning-preset stand-in: the session-latched values copied at match begin. */
UCLASS()
class UStealthSpikeTuningStore : public UStealthSpikeStoreBase
{
	GENERATED_BODY()

public:
	virtual FString DescribeContent() const override;

	FString PresetId = TEXT("Default");
	TMap<FString, float> Latched;
};

/** PlayerState: owner-only C-TeamView, no engine welcome/leave/rename messages, retained mid-round. */
UCLASS()
class AStealthSpikePlayerState : public AStealthPlayerState
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool ShouldBroadCastWelcomeMessage(bool bExiting = false) override { return false; }
	virtual void OnDeactivated() override;

	UFUNCTION()
	void OnRep_TeamView();

	/** Owner-only team view (written by the server's rebind from the Roster store). */
	UPROPERTY(ReplicatedUsing = OnRep_TeamView)
	FStealthSpikeTeamView TeamView;

	/** Server: set by the game mode at Logout when the participant's PlayerState must outlive its controller. */
	bool bRetainWhenDeactivated = false;
};

/** PlayerController: adopts a retained PlayerState, keeps its pawn when it leaves, acknowledges team views. */
UCLASS()
class AStealthSpikePlayerController : public AStealthPlayerController
{
	GENERATED_BODY()

public:
	virtual void InitPlayerState() override;
	virtual void PawnLeavingGame() override;
	virtual void UpdatePing(float InPing) override;
	virtual void PreClientTravel(const FString& PendingURL, ETravelType TravelType, bool bIsSeamlessTravel) override;
	virtual void ClientReceiveLocalizedMessage_Implementation(TSubclassOf<ULocalMessage> Message, int32 Switch, APlayerState* RelatedPlayerState_1,
		APlayerState* RelatedPlayerState_2, UObject* OptionalObject) override;

	/** Client -> server (owner-only): the team view of Epoch with Hash arrived. The participant comes from this connection. */
	UFUNCTION(Server, Reliable)
	void ServerSpikeAck(int32 Epoch, uint32 Hash);

	/** True when InitPlayerState adopted a retained PlayerState instead of spawning one. */
	bool bAdoptedRetained = false;
};

/** Game mode: seamless travel on, store-driven re-bind, rejoin adoption and supersede (ADR-0005 shape). */
UCLASS()
class AStealthSpikeSessionGameMode : public AStealthGameMode
{
	GENERATED_BODY()

public:
	AStealthSpikeSessionGameMode();

	virtual APlayerController* Login(UPlayer* NewPlayer, ENetRole InRemoteRole, const FString& Portal, const FString& Options, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
	virtual FString InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId, const FString& Options, const FString& Portal) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	virtual void HandleSeamlessTravelPlayer(AController*& C) override;
	virtual void PostSeamlessTravel() override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;

	/** The PlayerState Login stashed for the controller being spawned (consumed by its InitPlayerState). */
	TWeakObjectPtr<APlayerState> TakePendingReclaim();

	/** Records an acknowledgement from Controller's participant (server). */
	void ReceiveAck(APlayerController* Controller, int32 Epoch, uint32 Hash);

	/** Writes the view fields and the team view of the participant behind Controller from the stores. */
	void RebindView(APlayerController* Controller);

	/** The identity key of Controller: its login option, or the reserved "host" for the listen host's player. */
	static FString KeyOf(const APlayerController* Controller);

	/** The identity key bound to PlayerState's participant (through its team view), or "" (usable after the connection is gone). */
	FString KeyOfPlayerState(const APlayerState* PlayerState) const;

	/** True while a round is in progress (PlayerStates of leavers are retained). */
	bool IsRoundPhase() const;

private:
	TWeakObjectPtr<APlayerState> PendingReclaim;
	TWeakObjectPtr<APawn> PendingPawn;
	TSet<TObjectKey<APlayerController>> Reclaimed;
};
