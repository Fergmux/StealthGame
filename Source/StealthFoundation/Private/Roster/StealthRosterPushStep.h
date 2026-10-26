#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Server/Roster/StealthRosterCore.h"
#include "Templates/Function.h"
#include "UObject/WeakObjectPtr.h"
#include "View/StealthRosterTeamView.h"

class APawn;

/**
 * The Roster push step (ADR-0001 sections 5-6; GDD participant-state-roster R6; story psr-062): the ONLY writer of
 * URosterTeamViewComponent. It listens to the store's events, keeps a dirty set per team, and Flush() (once per
 * server frame, called by URosterStore's tick) writes each registered owner's view only when it changed. Own-team
 * data only; no opponent entry or count.
 *
 * bReconnecting (ruling D23, D24): set when a teammate's projection becomes Disconnected while Round's phase is
 * Active; cleared only by an event, never by a timer: rejoin (Connected) -> Rejoined; grace expiry (the
 * Disconnect transition) -> GraceExpired; a kick, removal, elimination or round end -> Cleared. A superseding
 * rejoin emits no projection change, so the flag stays until the rejoin completes.
 *
 * RetireAtServerTime is non-zero only in the retiring update: the flush after an Active -> Informant change carries
 * the retired pawn and its time once, and the next flush clears them.
 */
class FRosterPushStep final : public IRosterEventListener
{
public:
	/** Maps a Roster pawn handle to the engine pawn (the store's handle table); null when unknown or gone. */
	using FPawnResolver = TFunction<APawn*(const FRosterPawnHandle&)>;

	FRosterPushStep(const FRosterCore& InCore, const IRosterRoundPhase& InRoundPhase, FPawnResolver InResolver);

	/** Binds Id's owner view (one component per controller). Re-registering replaces it; the next flush writes it. */
	void RegisterView(FParticipantId Id, URosterTeamViewComponent* View);
	void UnregisterView(FParticipantId Id);

	// IRosterEventListener
	virtual void OnRosterEvent(const FRosterEvent& Event) override;

	/** Writes every dirty owner whose view changed; returns the number of components written. Call once per server frame. */
	int32 Flush();

	/** The teammate-facing reconnect flag of Id (for tests and dumps). */
	bool IsReconnecting(FParticipantId Id) const;
	EReconnectEndCause GetReconnectEndCause(FParticipantId Id) const;

	/** Forgets every view, flag and dirty mark (EndMatch). */
	void Reset();

private:
	struct FReconnectState
	{
		bool bReconnecting = false;
		EReconnectEndCause Cause = EReconnectEndCause::None;
	};

	void MarkTeamDirty(ERosterTeam Team);
	void ClearReconnect(FParticipantId Id, EReconnectEndCause Cause);
	FTeamView BuildView(const FParticipantRecord& Owner) const;

	const FRosterCore& Core;
	const IRosterRoundPhase& RoundPhase;
	FPawnResolver Resolver;

	TMap<FParticipantId, TWeakObjectPtr<URosterTeamViewComponent>> Views;
	TMap<FParticipantId, FReconnectState> Reconnect;
	/** Participants whose next flush is their retiring update (Active -> Informant this frame). */
	TSet<FParticipantId> RetiringThisFrame;
	/** Participants whose retiring fields must be cleared by the following flush. */
	TSet<FParticipantId> RetiringClearNextFrame;
	bool bDirty[2] = { false, false };
	bool bAllDirty = false;
};
