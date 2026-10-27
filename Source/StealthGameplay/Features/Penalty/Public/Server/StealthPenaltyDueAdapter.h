#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Delegates/IDelegateInstance.h"
#include "Server/StealthPenaltySeams.h"
#include "Templates/Function.h"
#include "UObject/WeakObjectPtrTemplates.h"

class UPenaltyService;

/**
 * FPenaltyDueAdapter: the T_due carrier adapter (ADR-0019 section 6, option (a) decided by the user 2026-10-25; GDD
 * PEN-R15; story nkp-011). It holds no copy of T_due: on every OnDueTimeChanged(P, T_due) it writes the value (0 when
 * unset) to P's C-TeamView owner part through the Roster's owner-view feed SetOwnerRespawnDue(P, T_due, ExpectedEpoch),
 * exactly once per change and never while the round is latched (ADR-0002's latch gate freezes owner writes after the
 * deciding slot; at round end the delegate still fires unset and nothing is written). The Roster is the only writer of
 * the owner part; T_due therefore reaches only its owner, never a pawn, never a reply entry.
 *
 * Registered at the composition root with the Roster's feed (URosterStore::SetOwnerRespawnDue, the T_due half of roster
 * story psr-038) and the round queue's latch; the Logic tests bind a recording feed (StealthGame.Penalty.DueAdapter).
 *
 * Usage:
 *   FPenaltyDueAdapter Adapter(&Feed, [Queue] { return Queue->IsLatched(); }, [Store] { return Store->GetEpochToken().RoundEpoch; });
 *   Adapter.Bind(*PenaltyService);   // ... Adapter.Unbind() before the feed or the queue goes
 */
class STEALTHGAMEPLAY_API FPenaltyDueAdapter
{
public:
	/** Feed: the Roster owner-view feed. IsLatched: Round's latch (no write while true). CurrentEpoch: the ExpectedEpoch. */
	FPenaltyDueAdapter(IPenaltyOwnerViewFeed* InFeed, TFunction<bool()> InIsLatched, TFunction<uint32()> InCurrentEpoch);
	~FPenaltyDueAdapter();

	FPenaltyDueAdapter(const FPenaltyDueAdapter&) = delete;
	FPenaltyDueAdapter& operator=(const FPenaltyDueAdapter&) = delete;

	/** Subscribes to Service.OnDueTimeChanged (a second Bind moves the subscription). */
	void Bind(UPenaltyService& Service);
	void Unbind();

	/** The write rule itself (the delegate's handler; the Logic tests call it directly). */
	void OnDueTimeChanged(FParticipantId Participant, TOptional<double> DueServerTime);

	/** Writes made (observation for tests and dumps). */
	int32 GetWriteCount() const { return WriteCount; }

private:
	IPenaltyOwnerViewFeed* Feed = nullptr;
	TFunction<bool()> IsLatched;
	TFunction<uint32()> CurrentEpoch;
	TWeakObjectPtr<UPenaltyService> Service;
	FDelegateHandle Handle;
	int32 WriteCount = 0;
};
