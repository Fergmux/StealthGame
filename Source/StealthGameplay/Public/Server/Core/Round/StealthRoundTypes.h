#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "View/StealthRosterTypes.h"
#include "View/StealthRoundPublicState.h"

/**
 * Round & Match Framework server vocabulary for the Exit B phase, team and start units (GDD round-match-framework
 * R3, R4, R5, R8-R10, R22; ADR-0005 sections 2 and 8; stories rmf-018..028). Plain C++: no UObject.
 */

/** R5 Start refusal codes, in the fixed check order (the first failure decides). */
enum class ERoundStartRefusalCode : uint8
{
	None,
	/** (1) the caller is not the host. */
	NotHost,
	/** (2) the phase is not Lobby. */
	WrongPhase,
	/** (2) a travel is already pending. */
	TravelInProgress,
	/** (3) a rostered participant is Unresponsive, Connecting or Dormant (retryable; lists the ids). */
	ParticipantNotConnected,
	/** (4) F3 fails (sub-code in FStartRefusal::TeamsReason). */
	TeamsInvalid,
	/** (5) TDC's best-effort preset dry run says the preset is invalid. */
	PresetInvalid,
	/** (5) the appearance deck cannot be dealt (best effort). */
	DeckInfeasible,
	/** Session refused: CanRequestRoundTravel() false, or RequestRoundTravel refused (rollback). */
	SessionRefused,
};

/** F3 failure sub-codes (R5 check 4). There is no Unassigned sub-code: R3 assigns everyone first. */
enum class ERoundTeamsInvalidReason : uint8
{
	None,
	TooSmall,
	Imbalance,
	Oversize,
};

/** R5's refusal: shown to the host only; Start then changes nothing. */
struct FStartRefusal
{
	ERoundStartRefusalCode Code = ERoundStartRefusalCode::None;
	ERoundTeamsInvalidReason TeamsReason = ERoundTeamsInvalidReason::None;
	/** ParticipantNotConnected: exactly the offending ids. */
	TArray<FParticipantId> ParticipantIds;

	bool IsRefused() const { return Code != ERoundStartRefusalCode::None; }

	static FStartRefusal Accepted() { return FStartRefusal(); }
	static FStartRefusal Make(ERoundStartRefusalCode InCode, ERoundTeamsInvalidReason InReason = ERoundTeamsInvalidReason::None)
	{
		FStartRefusal Out;
		Out.Code = InCode;
		Out.TeamsReason = InReason;
		return Out;
	}
};

/** The R22 host commands (stealth.round.<name>). Append only. */
enum class ERoundHostCommand : uint8
{
	Start,
	Reset,
	Abort,
	SwapTeam,
	Remove,
	Preset,
	DebugWin,
	DebugEliminate,
	DumpLog,
};

/** What a host command did (returned to the host's caller only, never to a client). */
enum class ERoundCommandResult : uint8
{
	/** The command ran (or, for Start, was accepted). */
	Executed,
	/** Legal and changed nothing (reset in Lobby or Reset; a preset with the same TeamSize). */
	NoOp,
	/** Parked until the Ended dwell elapses, then executed (R9). */
	Held,
	/** The caller is not the host (R2, R22). */
	NotHost,
	/** The command is not legal in this phase (R22). */
	WrongPhase,
	/** Refused by its rule (a full target team; a Start refusal; a Roster rejection), reported to the host sink. */
	Refused,
	/** The command needs a component that is not present (no resolution queue for a debug command). */
	Unavailable,
};

/**
 * Round rule inputs (values from tuning through the TDC gateway, never literals in the cores). The defaults are the
 * GDD canonical defaults; keys not yet registered in the TDC schema are filled by the composition root.
 */
struct FRoundRuleParams
{
	/** Players per team (TDC Round key, latch Round). */
	int32 TeamSize = 5;
	/** F3 m_t (R5). */
	int32 MinPlayersPerTeamCount = 1;
	/** F3 imbalance bound (R5). */
	int32 MaxTeamImbalanceCount = 1;
	/** Development: one team may be empty (F3 m_t = 0). */
	bool bAllowEmptyTeamDev = false;
	/** R22: the host may abort an Active round. */
	bool bHostMayAbortActive = false;
	/** R9: a reset in Ended is held until this long after the round end. */
	double EndedMinDwellSec = 3.0;
	/** R5 step 8 travel arguments. */
	FName MapId;
	FName PresetId;
};

/** Who issued a host command (R22: net mode listen server or standalone, a local controller, ParticipantId == HostId). */
struct FRoundCommandCaller
{
	/** The world's net mode is listen server or standalone. */
	bool bServerNetMode = false;
	/** The command came from a local controller (not a remote client's ServerExec). */
	bool bLocalController = false;
	FParticipantId Caller;
	FParticipantId HostId;
	/** The server process acting for the host (dev auto-start, end-of-round reset): no participant needed (dedicated server). */
	bool bServerAutomation = false;

	bool IsHost() const { return bServerNetMode && bLocalController && (bServerAutomation || (Caller.IsValid() && Caller == HostId)); }

	/** The server itself acting for the host (composition-root automation such as the dev auto-start). */
	static FRoundCommandCaller ServerAsHost(FParticipantId InHostId)
	{
		FRoundCommandCaller Out;
		Out.bServerNetMode = true;
		Out.bLocalController = true;
		Out.Caller = InHostId;
		Out.HostId = InHostId;
		Out.bServerAutomation = true;
		return Out;
	}
};

/** Arguments of a host command (R22): a participant, a second participant (pair exchange), a team, a preset. */
struct FRoundCommandArgs
{
	FParticipantId Participant;
	FParticipantId Other;
	int32 Team = 0;
	/** Preset: the requested preset's TeamSize (read from TDC by the caller) and its id. */
	int32 PresetTeamSize = 0;
	FName PresetId;
};

/** Host-only sink of refusals (never a client sink, R5). */
class IRoundHostSink
{
public:
	virtual ~IRoundHostSink() = default;

	virtual void OnStartRefused(const FStartRefusal& Refusal) = 0;
	virtual void OnCommandRefused(ERoundHostCommand Command, ERoundCommandResult Result, uint8 Detail) = 0;
};
