#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Server/Core/PawnCommandCore.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "View/NetPawnCommand.h"

class APawn;
class APlayerController;

/**
 * Stage-1 ingress of the pawn command interface (ADR-0011 section 3.1): the body of ServerPawnCommand, and the call the
 * listen host's own controller makes instead of the RPC (IngestCommand(HostLocal), the same checks and the same
 * reply flush; no NM_ListenServer branch anywhere here).
 *
 * Skeleton (stories iif-094 and iif-095): resolves the controller's OWN possessed pawn (the client never names the
 * pawn it commands), converts the wire arguments to the entry-point types, resolves PawnRefGuid through the NetDriver's
 * GUID cache (E7, spike V3) to a weak handle (a GUID naming the commanding pawn is stripped), reports the command log's
 * ingress point, and calls the matching entry point inside an FScopedCommandOrigin. The ordered silent-drop checks
 * (epoch and sequence, transport bucket, WorldPoint clamp, court-id check) are stories iif-020 to iif-025.
 *
 * Deviation from the ADR's 3-parameter sketch, recorded in story iif-094: the context also carries the controller's
 * recorded control epoch (ingress stamps it, never the pawn's current one) and the listen host's weak PawnRef (the host
 * has no package map, E7/V6). Neither is caller identity.
 */
struct FCommandIngressContext
{
	FCommandOrigin Origin;
	/** The controller's recorded PossessedControlEpoch (ADR-0011 section 2). */
	uint32 ControlEpoch = 0;
	/** Listen host only: the aim ray's pawn as a weak handle, used instead of a GUID. Null on the RPC path. */
	TWeakObjectPtr<APawn> HostLocalPawnRef;
};

/** What stage 1 did with a command (silent: never replied to; drops are counted by later stories). */
enum class ECommandIngressResult : uint8
{
	/** Passed and handed to the entry point. */
	Submitted,
	/** Structurally impossible (WithValidation should already have refused it). */
	DroppedMalformed,
	/** The controller has no command-capable pawn. */
	DroppedNoPawn,
	/** An Interact Begin naming an id that is not a court fixture (IIF R5: unknown id, no reply). */
	DroppedUnknownTarget,
};

/** Runs stage 1 for one command of Owner. Server only. */
STEALTHGAMEPLAY_API ECommandIngressResult IngestCommand(APlayerController& Owner, const FCommandIngressContext& Context, const FNetPawnCommand& Cmd);

/** Resolves a wire PawnRef GUID through the server NetDriver's GUID cache (ADR-0011 E7). Null when unresolvable. */
STEALTHGAMEPLAY_API APawn* ResolvePawnRefGuid(const UWorld* World, const FNetworkGUID& Guid);
