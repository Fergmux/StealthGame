#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Server/Queue/StealthQueueResolver.h"
#include "Server/StealthPenaltySeams.h"
#include "Templates/Function.h"

class FPenaltyCore;
class FResolutionQueueCore;

/**
 * FPenaltyRespawnQueueAdapter: Penalty's InformantRespawn timers on Round's due-time heap (ADR-0019 sections 1, 3.2;
 * ADR-0002 sections 2a, 4; stories nkp-009 and nkp-012). Two faces of one engine owner, adding no rule:
 *
 *   IPenaltyRespawnTimers  ScheduleRespawnItem -> FResolutionQueueCore::ScheduleAt(T_due, TimerFired(InformantRespawn,
 *                          FTimerPayload {Subject = P, Seq = RespawnRequestSeq, Generation = 0, Phase = 0}), Expected =
 *                          {MatchId, RoundEpoch}); CancelRespawnItem -> FResolutionQueueCore::CancelTimer.
 *   IQueueResolver         the TimerFired(InformantRespawn) resolver (one per kind, registered by UPenaltyService::Bind):
 *                          FPenaltyCore::ResolveRespawnItem re-validates and runs the commit. The queue itself rejects an
 *                          item of an earlier epoch (Rejected_StaleEpoch) and, after the latch, Rejected_Latched.
 *
 * Plain C++ (IQueueResolver's I-class has a non-pure _getUObject, so a non-UObject may implement it). The Logic tests
 * build one over a real FResolutionQueueCore (FStealthQueueTestRig) to drive the real heap order.
 *
 * Usage:
 *   FPenaltyRespawnQueueAdapter Adapter(Queue, [Store] { return Store->GetEpochToken().MatchId; });
 *   Deps.Timers = &Adapter; ... Adapter.SetCore(Core.Get()); Queue->RegisterTimerResolver(ETimerKind::InformantRespawn, &Adapter);
 */
class STEALTHGAMEPLAY_API FPenaltyRespawnQueueAdapter final : public IPenaltyRespawnTimers, public IQueueResolver
{
public:
	/** MatchId: the MatchId half of every item's Expected token (the Roster's match; the core carries the RoundEpoch). */
	FPenaltyRespawnQueueAdapter(FResolutionQueueCore* InQueue, TFunction<uint64()> InMatchId);

	/** The core whose ResolveRespawnItem a fired item reaches (null: a fired item voids). */
	void SetCore(FPenaltyCore* InCore) { Core = InCore; }
	FResolutionQueueCore* GetQueue() const { return Queue; }

	// IPenaltyRespawnTimers
	virtual uint64 ScheduleRespawnItem(double DueServerTime, const FPenaltyRespawnItem& Item) override;
	virtual void CancelRespawnItem(uint64 Handle) override;

	// IQueueResolver (ETimerKind::InformantRespawn)
	virtual EResolveResult Resolve(const FQueueItem& Item, FQueueKey Key) override;

private:
	FResolutionQueueCore* Queue = nullptr;
	TFunction<uint64()> MatchId;
	FPenaltyCore* Core = nullptr;
};
