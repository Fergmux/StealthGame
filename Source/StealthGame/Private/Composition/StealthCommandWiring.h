#pragma once

#include "CoreMinimal.h"
#include "Server/BodyRemovalObserver.h"
#include "Server/BushSpotQuery.h"
#include "Server/Core/PawnCommandCore.h"
#include "Server/Core/ReplySink.h"
#include "Server/Queue/StealthQueueResolver.h"
#include "Server/Queue/StealthResolutionQueueCore.h"
#include "Server/StealthClock.h"
#include "Server/StrikeTallySource.h"

class AStealthBush;
class AStealthGameMode;
class URoundMatchStore;
class URoundRosterStoreGateway;
class UStealthMatchAdapters;
class UWorld;

/**
 * The composition root's command-path adapters (ADR-0007 section 4; Assassination stories asn-012 / asn-013 play
 * integration). They replace the D53 FStealthPrototypeCommandQueue, which resolved Attack / Prepare / Interact at once
 * through the prototype game mode: every PawnCommand now goes to Round's single queue (URoundMatchStore::GetRoundQueue())
 * and is resolved in its slot by Assassination's UEncounterService (Attack, Prepare) or by the stand-in fallback below
 * (Interact, Hide, Traverse) until the IIF stage-2 resolver exists.
 */

/** The server world's time and frame counter (the same values as Round's store clock). */
class FStealthWorldCommandClock final : public IStealthClock
{
public:
	explicit FStealthWorldCommandClock(const UWorld* InWorld) : World(InWorld) {}
	virtual double GetServerTimeSeconds() const override;
	virtual uint64 GetServerTickIndex() const override { return GFrameCounter; }

private:
	TWeakObjectPtr<const UWorld> World;
};

/**
 * IPawnCommandQueue over Round's queue: a participant's command goes into its own mailbox with the participant's Session
 * ConnectionGeneration captured at push (a command sent before a reconnect is Dropped_StaleConnection at its slot), and
 * an AI or server-local one into the server-local mailbox. The queue is looked up per call (Round recreates it per match).
 * With no queue (no match core yet) the command is dropped and counted.
 */
class FStealthRoundCommandQueue final : public IPawnCommandQueue
{
public:
	FStealthRoundCommandQueue(URoundMatchStore* InRound, UStealthMatchAdapters* InGenerations);

	virtual void Submit(FMailboxId Mailbox, const FQueueItem& Item) override;
	virtual void ScheduleAt(double DueServerTime, const FQueueItem& Item) override;

	/** Commands handed to the round queue / dropped for want of one (dev HUD, StealthDumpProd). */
	int32 GetSubmittedCount() const { return SubmittedCount; }
	int32 GetDroppedCount() const { return DroppedCount; }

private:
	TWeakObjectPtr<URoundMatchStore> Round;
	TWeakObjectPtr<UStealthMatchAdapters> Generations;
	int32 SubmittedCount = 0;
	int32 DroppedCount = 0;
};

/** The epoch stamped on submitted commands: the Roster's (MatchId, RoundEpoch), the same source the queue compares with. */
class FStealthRoundEpochSource final : public IQueueEpochSource
{
public:
	explicit FStealthRoundEpochSource(URoundRosterStoreGateway* InRoster) : Roster(InRoster) {}
	virtual FEpochToken GetCurrentEpoch() const override;

private:
	TWeakObjectPtr<URoundRosterStoreGateway> Roster;
};

/**
 * STAND-IN (Concealment epic not built; ADR-0017 FBushRuntime replaces it): IBushSpotQuery over the prototype bushes.
 * BushId = 1-based index of the world's AStealthBush actors sorted by name; footprint = the bush volume's 2D box; spot
 * zone = footprint grown by BushSpotToleranceCm; occupants = concealed AStealthCharacters standing in the footprint
 * (AStealthCharacter::IsConcealed, the prototype overlap-driven concealment).
 */
class FStealthPrototypeBushSpotQuery final : public IBushSpotQuery
{
public:
	FStealthPrototypeBushSpotQuery(UWorld* InWorld, double InToleranceCm) : World(InWorld), ToleranceCm(InToleranceCm) {}

	virtual bool FindSpotZone(const FVector& WorldPoint, uint16& OutBushId) const override;
	virtual void GetSpotOccupants(uint16 BushId, const APawn* Attacker, TArray<APawn*>& OutOccupants) const override;
	virtual bool IsConcealed(const APawn* Pawn) const override;
	virtual bool GetFootprint(uint16 BushId, FBox2D& OutFootprint) const override;

private:
	void GetBushes(TArray<const AStealthBush*>& Out) const;

	TWeakObjectPtr<UWorld> World;
	double ToleranceCm = 0.0;
};

/**
 * STAND-IN (until IIF stage 2, the IIF slot resolver): the non-Assassination discrete commands, resolved inside the same
 * queue slot after Assassination's action-slot gate. InteractBegin / InteractEnd call the prototype objective handler
 * (AStealthGameMode::HandleInteractRequest); Traverse, HideEnter and HideExit execute as no-ops (the sample Blueprint
 * runs traversal; prototype concealment is overlap-driven). Holds: none use the queue yet (Voided).
 */
class FStealthPrototypeCommandFallback final : public IQueueResolver
{
public:
	explicit FStealthPrototypeCommandFallback(AStealthGameMode* InGameMode) : GameMode(InGameMode) {}

	virtual EResolveResult Resolve(const FQueueItem& Item, FQueueKey Key) override;

private:
	TWeakObjectPtr<AStealthGameMode> GameMode;
};

/**
 * STAND-IN (NPC-Kill Penalty epic not built; its IStrikeTallySource replaces it; story asn-015): the attacker's
 * AStealthPlayerState counters the prototype game mode keeps in the OnPawnKilled fan-out (AStealthGameMode::
 * HandleEncounterPawnKilled): WrongTargetTally (+1 for every WrongTarget kill, an NPC and an informant alike, PEN-R14)
 * and the exile flag set when an NPC kill reached NpcKillLimit in the same slot (consumed here, so bExiled is true only on
 * the triggering kill's result). An NPC attacker (no player state): {0, false}.
 */
class FStealthPrototypeStrikeTally final : public IStrikeTallySource
{
public:
	virtual FStrikeTally GetTallyForResult(const APawn* Attacker, EStrikeResult Result) override;
};

/**
 * STAND-IN (Concealment epic not built; its FBushRuntime observer replaces it; story asn-019): the prototype keeps no
 * killed-concealed owner / team flags, so a committed body removal changes nothing here (it is logged at Verbose).
 */
class FStealthPrototypeBodyRemovalObserver final : public IBodyRemovalObserver
{
public:
	virtual void OnBodyRemovalCommitted(APawn* Body) override;
};

/**
 * The reply step's transport (ADR-0011 section 8; story asn-015): one connection's batch goes to the AStealthPlayerController
 * whose reply sink it is, through its reliable owner-only ClientCommandReplies (the listen host's own controller runs it
 * locally). A sink no controller holds any more discards.
 */
class FStealthControllerReplyTransport final : public IReplyTransport
{
public:
	explicit FStealthControllerReplyTransport(UWorld* InWorld) : World(InWorld) {}

	virtual void SendCommandReplies(FReplySinkId Sink, const TArray<FCommandReply>& Replies) override;

private:
	TWeakObjectPtr<UWorld> World;
};
