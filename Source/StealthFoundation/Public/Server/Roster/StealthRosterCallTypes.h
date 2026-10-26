#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"

/**
 * Participant State & Roster call vocabulary shared beyond the plain core (GDD participant-state-roster R3, R8,
 * R11). Moved out of Private/Roster/StealthRosterCore.h (story psr-014) so the Round & Match Framework gateways
 * (StealthGameplay, consumer-owned, RMF-003 ruling 2026-10-25) can name the Roster results they receive.
 * Server-only: the codes describe server decisions and never reach a client.
 */

/** The Roster's projection of Session's ConnectionState (R3 `Connection`, R11 mapping table). */
enum class ERosterConnection : uint8
{
	Connected,
	Disconnected,
	Dormant,
};

/**
 * Lifecycle reasons (R8). InformantKilled (decision D10) is a removal/death reason carried only by
 * RetireBinding, never an entering reason: RequestTransition with it is always Rejected_IllegalReason.
 */
enum class ERosterReason : uint8
{
	None,
	Assassinated,
	ReversedAgainst,
	StrikeLimit,
	Disconnect,
	RoundEnd,
	Reset,
	InformantKilled,
};

/** Every Roster result code (R4, R5, R8, R9 and the GDD wrong-input table), shared by all calls. */
enum class ERosterResult : uint8
{
	Applied,
	Registered,
	Reclaimed,
	/** The call changed nothing because the target value was already in place (no event, no EventSeq). */
	NoOp_Unchanged,
	NoOp_AlreadyInState,
	NoOp_NotBound,
	NoOp_AlreadyRetired,
	Rejected_NotAuthority,
	Rejected_WrongThread,
	Rejected_UnknownParticipant,
	Rejected_StaleEpoch,
	Rejected_BatchOnlyReason,
	Rejected_IllegalReason,
	Rejected_IllegalTransition,
	Rejected_NotDisconnected,
	Rejected_StaleConnection,
	Rejected_TeamsLocked,
	Rejected_TeamsUnlocked,
	Rejected_RosterFull,
	Rejected_InvalidTeamSize,
	Rejected_TeamSizeBelowRoster,
	Rejected_InvalidBatch,
	Rejected_TeamFull,
	Rejected_UnassignedParticipants,
	Rejected_AlreadyBound,
	Rejected_PawnInUse,
	Rejected_WrongState,
};

/** True for every Rejected_* code. */
inline bool IsRosterRejection(ERosterResult Result) { return Result >= ERosterResult::Rejected_NotAuthority; }
