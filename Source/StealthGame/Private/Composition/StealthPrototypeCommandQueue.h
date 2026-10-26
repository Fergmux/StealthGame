#pragma once

#include "CoreMinimal.h"
#include "Server/Core/PawnCommandCore.h"
#include "Server/StealthClock.h"

class AStealthGameMode;
class UWorld;

/**
 * D53 integration adapters for the pawn command interface (stories iif-094/095 integration; decision D53): the
 * production command path (UPawnCommandClient -> ServerPawnCommand -> IngestCommand -> UPawnCommandComponent ->
 * FPawnCommandCore) runs in play, and these two seams stand in for what the Round queue epic has not delivered yet.
 *
 * FStealthWorldCommandClock: the server world's time and frame counter (ADR-0002 ServerNow until the queue facade's
 * clock lands).
 *
 * FStealthPrototypeCommandQueue: an IPawnCommandQueue that resolves each PawnCommand item IMMEDIATELY through the
 * prototype game mode's rule handlers (strike, defence, objective interaction), so the prototype actions keep working
 * through the production entry path until the Round queue facade (UResolutionQueue) and the IIF stage-2 resolver
 * replace it. Traverse and Hide are logged no-ops (the reparented sample Blueprint still runs traversal; prototype
 * concealment is overlap-driven). Throwaway: deleted when ADR-0002's facade is wired.
 */
class FStealthWorldCommandClock final : public IStealthClock
{
public:
	explicit FStealthWorldCommandClock(const UWorld* InWorld) : World(InWorld) {}
	virtual double GetServerTimeSeconds() const override;
	virtual uint64 GetServerTickIndex() const override { return GFrameCounter; }

private:
	TWeakObjectPtr<const UWorld> World;
};

class FStealthPrototypeCommandQueue final : public IPawnCommandQueue
{
public:
	explicit FStealthPrototypeCommandQueue(AStealthGameMode* InGameMode);

	virtual void Submit(FMailboxId Mailbox, const FQueueItem& Item) override;
	virtual void ScheduleAt(double DueServerTime, const FQueueItem& Item) override {}

	/** Commands resolved since construction (dev HUD / StealthDumpProd). */
	int32 GetResolvedCount() const { return ResolvedCount; }

private:
	TWeakObjectPtr<AStealthGameMode> GameMode;
	int32 ResolvedCount = 0;
};
