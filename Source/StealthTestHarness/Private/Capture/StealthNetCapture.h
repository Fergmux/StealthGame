#pragma once

// Raw-bunch capture for parity and leak spikes (ADR-0008 section 6; chosen by IVP spike S1,
// production/epics/information-visibility-policy/story-001-spike-s1-multiprocess-raw-bunch-capture.md).
//
// Mechanism: a harness-only UActorChannel subclass is swapped in for the "Actor" channel definition of
// every net driver when the process starts with `-StealthNetCapture`. Its ReceivedBunch override (client)
// and SendBunch override (server) bracket each bunch; inside the bracket an any-thread output device
// collects the engine's own replication log points (LogRepProperties VeryVerbose: ReceiveProperties /
// SendProperties parents; LogNetTraffic: "Replicate <actor>, bNetInitial, bNetOwner" and "Sent RPC";
// LogRepTraffic: "Received RPC"). One record per bunch goes to the harness record file:
//   rx  (client) {ch, actor, cls, guid, bits, open, close, rel, objs:[{o:<replicated object's class path>, p:[parents]}], rpc:[...]}
//   tx  (server) {conn, ch, actor, cls, guid, bits, open, close, rel, init, own, objs:[...]}
//   txrpc (server) {conn, actor, fn, bytes}
// The launcher must also pass -LogCmds="LogRepProperties VeryVerbose, LogNetTraffic Log, LogRepTraffic Log".
// Harness only: this module is excluded from Shipping and Test (ADR-0007), so the capture cannot ship.

#include "CoreMinimal.h"
#include "Engine/ActorChannel.h"
#include "StealthNetCapture.generated.h"

/** Actor channel that reports every received and sent bunch to the harness capture. */
UCLASS(Transient)
class UStealthCaptureActorChannel : public UActorChannel
{
	GENERATED_BODY()

public:
	UStealthCaptureActorChannel(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Brackets the engine's receive path so the replication log points inside it are attributed to this bunch. */
	virtual void ReceivedBunch(FInBunch& Bunch) override;

	/** Attributes the pending "Replicate" block (properties written for this connection) to this bunch. */
	virtual FPacketIdRange SendBunch(FOutBunch* Bunch, bool Merge) override;
};

namespace StealthHarness::Capture
{
	/** Swaps the Actor channel class on every net driver class default object and starts the log collector. */
	void Install();

	/** Stops the log collector. */
	void Uninstall();

	/** True after Install. */
	bool IsInstalled();
}
