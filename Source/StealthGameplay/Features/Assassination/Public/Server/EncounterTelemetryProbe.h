#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Server/EncounterTypes.h"
#include "Server/EventLog/StealthTelemetrySink.h"
#include "Server/Queue/StealthQueueResolver.h"
#include "UObject/ObjectKey.h"

class FAttackResolverCore;
class IStealthClock;

/**
 * Assassination's server cost meter (story asn-026, AC-ASN-34; ADR-0018 Performance Implications, the 0.5 ms queue line;
 * the Insights scope Stealth.Budget.Assassination). Every PawnCommand / HoldTimer resolution of the encounter runs inside
 * one scope: an Insights CPU scope always, and outside Shipping a cycle accumulator a harness can read without an Insights
 * session (off by default: two FPlatformTime reads per scope when on). Game thread only.
 *
 * Usage (a harness):
 *   FAssassinationCostMeter::Get().bEnabled = true; ... resolve the frame's Attacks ...
 *   const double Ms = FAssassinationCostMeter::Get().TakeMs();
 */
struct STEALTHGAMEPLAY_API FAssassinationCostMeter
{
	/** Cycles spent in resolutions since the last TakeMs. */
	uint64 Cycles = 0;
	/** Resolutions measured since the last TakeMs. */
	int32 Calls = 0;
	bool bEnabled = false;

	static FAssassinationCostMeter& Get();

	/** The total since the last call in milliseconds; resets it. */
	double TakeMs();
};

/**
 * FEncounterTelemetryProbe (story asn-026; ADR-0023 section 2.1): Assassination's kind-blind probe call sites, wrapped around
 * the resolver and its action events so the core decides exactly as before:
 *  - as the IQueueResolver registered for PawnCommand and HoldTimer it forwards every item to FAttackResolverCore inside the
 *    Stealth.Budget.Assassination cost scope and then, in the same slot, records one StrikeResolved per Attack resolution the
 *    item produced (attacker, victim, outcome, the victim's brace age / time since its last brace / defence readiness at
 *    contact);
 *  - as the IEncounterActionEvents the core reports to it forwards every event to the hub and records one BraceEnded per
 *    brace the core ends (Reversed -> Reversal, Released -> Cancelled, Superseded -> Preempted, completed -> Expired; a
 *    brace still up at a round Reset -> Removed).
 * Every pawn takes the same path: nothing here reads a controller kind, team or participant state, and it records only
 * FSubjectRef pawn handles (the sink resolves them). It never logs a value. The sink is never null (the null object while
 * the Event Log's telemetry section does not exist). It holds no strike counter: per pawn only the open brace's start and
 * the last brace end, which the readiness fields need.
 *
 * Usage (UEncounterService::Initialize / RegisterOn):
 *   Probe = MakeUnique<FEncounterTelemetryProbe>();
 *   Probe->SetInnerEvents(HubEvents.Get()); Deps.Events = Probe.Get();
 *   Core = MakeUnique<FAttackResolverCore>(Deps, Params);
 *   Probe->Bind(Core.Get(), DefenceStore.Get(), Clock, Params.Public.DefenceCooldownSec, Init.Telemetry);
 *   Queue.RegisterResolver(EQueueItemType::PawnCommand, Probe.Get());
 */
class STEALTHGAMEPLAY_API FEncounterTelemetryProbe final : public IQueueResolver, public IEncounterActionEvents
{
public:
	/** The events the core's reports are forwarded to (the hub adapter; null forwards nothing). */
	void SetInnerEvents(IEncounterActionEvents* InInner) { Inner = InInner; }

	/** The resolver forwarded to, the defence store and clock the readiness fields read, and the sink (null: the null object). */
	void Bind(FAttackResolverCore* InCore, IEncounterDefenceStore* InDefence, const IStealthClock* InClock, double InDefenceCooldownSec, IStealthTelemetrySink* InSink);

	/** The sink in use (never null). */
	IStealthTelemetrySink& GetSink() const { return Sink != nullptr ? *Sink : FNullStealthTelemetrySink::Get(); }

	/** Round Reset: every brace still up ends (Removed) and the per-pawn records are dropped. */
	void OnRoundReset();

	// IQueueResolver
	virtual EResolveResult Resolve(const FQueueItem& Item, FQueueKey Key) override;
	virtual void OnRejectedLatched(const FQueueItem& Item, FQueueKey Key) override;

	// IEncounterActionEvents
	virtual void OnActionStarted(APawn* Pawn, EActionType Type) override;
	virtual void OnActionCancelled(APawn* Pawn, EActionType Type, ECancelReason Reason) override;
	virtual void OnActionCompleted(APawn* Pawn, EActionType Type) override;

private:
	struct FBraceRecord
	{
		TWeakObjectPtr<APawn> Pawn;
		double OpenStart = -1.0;
		double LastStart = -1.0;
		double LastEnd = -1.0;
		ETelemetryBraceEnd LastCause = ETelemetryBraceEnd::Expired;
		/** The slot (queue key) the last brace ended in. */
		FQueueKey LastEndKey;
	};

	void EndBrace(APawn* Pawn, ETelemetryBraceEnd Cause);
	void RecordStrike(const FAttackResolution& Resolution, FQueueKey Key);
	double Now() const;

	IEncounterActionEvents* Inner = nullptr;
	FAttackResolverCore* Core = nullptr;
	IEncounterDefenceStore* Defence = nullptr;
	const IStealthClock* Clock = nullptr;
	double DefenceCooldownSec = 60.0;
	IStealthTelemetrySink* Sink = nullptr;
	/** The key of the item being resolved (the slot the brace events of the core belong to). */
	FQueueKey CurrentKey;
	TMap<FObjectKey, FBraceRecord> Braces;
};
