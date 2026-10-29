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
 * STAND-IN (until IIF stage 2, the IIF slot resolver): the non-Assassination discrete commands, resolved inside the same
 * queue slot after Assassination's action-slot gate. InteractBegin / InteractEnd go to the Interact hold stand-in
 * (FInteractHoldStandIn: the production Objectives path, story oc-005; it replaced the prototype objective handler, retired
 * by story oc-022); HideEnter and HideExit go to Concealment's resolver (UConcealmentService, stories con-004/005; every
 * pawn kind the same path); Traverse executes as a no-op (the sample Blueprint runs traversal). Without an Interact or a
 * Hide resolver those commands are Voided.
 */
class FStealthPrototypeCommandFallback final : public IQueueResolver
{
public:
	FStealthPrototypeCommandFallback(AStealthGameMode* InGameMode, IQueueResolver* InInteract, IQueueResolver* InHide)
		: GameMode(InGameMode), Interact(InInteract), Hide(InHide) {}

	virtual EResolveResult Resolve(const FQueueItem& Item, FQueueKey Key) override;

private:
	TWeakObjectPtr<AStealthGameMode> GameMode;
	IQueueResolver* Interact = nullptr;
	IQueueResolver* Hide = nullptr;
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
