#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Server/StealthQueuePayloads.h"
#include "View/NetPawnCommand.h"

/**
 * The server command log seam of the pawn command interface (story iif-095, ADR-0011 Validation Criteria). Two
 * observation points report every command that reaches the server: the output of stage-1 ingress (a human's command
 * after its checks) and the core's queue submit (every source). The harness's FStealthCommandLog installs the
 * observer and records the entries in order. Observing never alters ordering, timing or the payload.
 *
 * Compiled only under WITH_STEALTH_TEST_SEAMS: in Shipping the notify calls compile to nothing.
 */

/** Where an entry was observed. */
enum class EStealthCommandLogPoint : uint8
{
	/** The output of stage-1 ingress (passed; about to enter the component's entry point). */
	IngressOutput,
	/** The core's queue submit. */
	QueueSubmit,
};

/** One observed command. Ids and values only. */
struct FStealthCommandLogEntry
{
	EStealthCommandLogPoint Point = EStealthCommandLogPoint::QueueSubmit;
	ECommandKind Kind = ECommandKind::Traverse;
	FClientCommandSeq Seq;
	/** The server frame of the observation (GFrameCounter at ingress; the injected clock's tick at submit). */
	uint64 ArrivalTick = 0;
	FReplySinkId Sink;
	FMailboxId Mailbox;
	/** QueueSubmit only: the submitted payload. */
	FCommandPayload Payload;
	/** IngressOutput only: the wire arguments after ingress. */
	FNetCommandArgs Args;
};

#if WITH_STEALTH_TEST_SEAMS

namespace StealthCommandSeams
{
	using FObserver = TFunction<void(const FStealthCommandLogEntry&)>;

	/** Installs (or, with an empty function, removes) the single observer. Game thread only. */
	STEALTHGAMEPLAY_API void SetObserver(FObserver InObserver);

	/** True when an observer is installed (lets callers skip building an entry). */
	STEALTHGAMEPLAY_API bool HasObserver();

	/** Reports one entry to the observer, if any. */
	STEALTHGAMEPLAY_API void Notify(const FStealthCommandLogEntry& Entry);
}

#endif // WITH_STEALTH_TEST_SEAMS
