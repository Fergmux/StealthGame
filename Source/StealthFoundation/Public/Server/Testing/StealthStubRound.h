#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"

#if WITH_STEALTH_TEST_SEAMS

#include "Server/StealthClock.h"
#include "Server/StealthTuningPipelineCore.h"
#include "Templates/Function.h"

/**
 * Test-only Round stub for the TDC acceptance tests (ADR-0008 section 4; story tdc-052). It stands in for
 * the Round & Match Framework until that epic lands and replaces it with its own integration tests; it is
 * not a production API. Compiled only under WITH_STEALTH_TEST_SEAMS.
 *
 * It records, in order, every call made on it:
 *   BeginResolve()              forwards to an injected callable (the real UStealthTuningService::BeginResolve
 *                               is story tdc-066, so the target is injected, never named)
 *   OnTuningReady(TuningHash)   the ready receiver (CR-7)
 *   OnTuningFailed(Reason)      the failed receiver (LoadFailed or SnapshotFault)
 *   EnterPhase(Phase)           a round phase transition (Lobby, Preparing, Playing, Ended)
 *   ReleasePawns()              releases first-entry pawns at one common tick read from the injected clock
 * and holds the Round Event Log header as an opaque byte buffer the test writes and reads (its field list
 * is story tdc-084's).
 */

/** Round phases the stub can be driven through (Round GDD names). */
enum class EStealthStubRoundPhase : uint8
{
	Lobby,
	Preparing,
	Playing,
	Ended,
};

/** The kind of a recorded call. */
enum class EStealthStubRoundCall : uint8
{
	BeginResolve,
	OnTuningReady,
	OnTuningFailed,
	EnterPhase,
	ReleasePawns,
};

/** One recorded call and its argument (only the field of its kind is meaningful). */
struct FStealthStubRoundCall
{
	EStealthStubRoundCall Kind = EStealthStubRoundCall::BeginResolve;
	uint64 TuningHash = 0;
	ETuningFailReason Reason = ETuningFailReason::LoadFailed;
	EStealthStubRoundPhase Phase = EStealthStubRoundPhase::Lobby;
	uint64 Tick = 0;

	bool operator==(const FStealthStubRoundCall& Other) const
	{
		return Kind == Other.Kind && TuningHash == Other.TuningHash && Reason == Other.Reason && Phase == Other.Phase && Tick == Other.Tick;
	}
};

class FStubRound
{
public:
	/** BeginResolveTarget is called by BeginResolve (may be empty); Clock supplies the pawn-release tick (may be null). */
	explicit FStubRound(TFunction<void()> InBeginResolveTarget = nullptr, const IStealthClock* InClock = nullptr)
		: BeginResolveTarget(MoveTemp(InBeginResolveTarget))
		, Clock(InClock)
	{
	}

	/** Records the call, then forwards to the injected target. */
	void BeginResolve()
	{
		FStealthStubRoundCall& Call = Calls.AddDefaulted_GetRef();
		Call.Kind = EStealthStubRoundCall::BeginResolve;
		if (BeginResolveTarget)
		{
			BeginResolveTarget();
		}
	}

	/** Ready receiver: records the hash. */
	void OnTuningReady(uint64 TuningHash)
	{
		FStealthStubRoundCall& Call = Calls.AddDefaulted_GetRef();
		Call.Kind = EStealthStubRoundCall::OnTuningReady;
		Call.TuningHash = TuningHash;
	}

	/** Failed receiver: records the reason; the stub returns to Lobby, as Round does (CR-6). */
	void OnTuningFailed(ETuningFailReason Reason)
	{
		FStealthStubRoundCall& Call = Calls.AddDefaulted_GetRef();
		Call.Kind = EStealthStubRoundCall::OnTuningFailed;
		Call.Reason = Reason;
		Phase = EStealthStubRoundPhase::Lobby;
	}

	/** Moves the stub to Phase and records it. */
	void EnterPhase(EStealthStubRoundPhase InPhase)
	{
		FStealthStubRoundCall& Call = Calls.AddDefaulted_GetRef();
		Call.Kind = EStealthStubRoundCall::EnterPhase;
		Call.Phase = InPhase;
		Phase = InPhase;
	}

	/** Releases every first-entry pawn at one common tick (the clock's, 0 without a clock); returns it. */
	uint64 ReleasePawns()
	{
		FStealthStubRoundCall& Call = Calls.AddDefaulted_GetRef();
		Call.Kind = EStealthStubRoundCall::ReleasePawns;
		Call.Tick = Clock != nullptr ? Clock->GetServerTickIndex() : 0;
		return Call.Tick;
	}

	/** The Round Event Log header, an opaque buffer (story tdc-084 defines its fields). */
	TArray<uint8>& GetEventLogHeader() { return EventLogHeader; }
	const TArray<uint8>& GetEventLogHeader() const { return EventLogHeader; }

	/** The current phase (Lobby at construction). */
	EStealthStubRoundPhase GetPhase() const { return Phase; }

	/** Every call made, in order. */
	const TArray<FStealthStubRoundCall>& GetCalls() const { return Calls; }

private:
	TFunction<void()> BeginResolveTarget;
	const IStealthClock* Clock = nullptr;
	EStealthStubRoundPhase Phase = EStealthStubRoundPhase::Lobby;
	TArray<FStealthStubRoundCall> Calls;
	TArray<uint8> EventLogHeader;
};

#endif // WITH_STEALTH_TEST_SEAMS
