#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Server/Core/Round/StealthRoundFormulas.h"
#include "Server/Core/Round/StealthRoundGateways.h"
#include "Server/Core/Round/StealthRoundTypes.h"
#include "Templates/Function.h"

class IStealthClock;

/**
 * The Preparing readiness sequence (GDD round-match-framework R6 P1-P7; ADR-0002 section 8.6; stories rmf-031..040).
 * Plain C++ step machine owned by FRoundMatchCore: it polls on the Round tick and waits on owner events (no engine
 * timer, no UWorld). Every engine contact goes through Round's gateways and the P6 spawner.
 *
 *   P1 params     OnTravelFailed(BadParams)                                      -> abort BadParams
 *   P2 tuning     Begin signals "Preparing entered"; OnTuningReady(TuningHash)    -> TDC Failed: TuningLoadFailed;
 *                 TDC's server self-check fault: TuningSnapshotFault (client hash mismatches never abort)
 *   P3 start work on the first OnTuningReady only: F6 at freeze, the Event Log header, RequestLayout(Seed, Hash) and
 *                 AllocateAppearances(every rostered id, NpcCount) exactly once  -> DeckInvalid / Error_DeckTooSmall:
 *                 DeckInvalid; Error_AlreadyDealt: AllocatorMisuse
 *   P4 layout     OnLayoutReady(LayoutId, Seed) with the round's Seed             -> OnLayoutAborted: LayoutAborted;
 *                 another seed: LayoutSeedMismatch
 *   P5 barrier    every participant Session has Connected delivered its single round acknowledgement for this
 *                 Preparing's nonce under its current ConnectionGeneration; stale acks (old nonce, replaced controller)
 *                 are dropped and change nothing (no deadline is started, extended or restarted). A participant that
 *                 misses Session's deadline is Disconnected and leaves the wait set; a rejoiner (new generation) must ack.
 *   P5b gate      >= MinPlayersPerTeamCount Connected per team (0 for an empty team with bAllowEmptyTeamDev) -> TeamAbsent
 *   P6 spawn      RequestStartPlacement once (PlacementInfeasible / NpcCountTooLow abort with that code), the F5 zone
 *                 share check (PlacementInvariant), the appearance copy to the Roster, then all D = players + NpcCount
 *                 pawns in one call of Tick, in sigma order (F5). A participant not Connected gets Ai control and a
 *                 closed spawn gate; a rejected BindPawn aborts BindFailed.
 *   P7 world-ready  every Connected client's world-ready report (second acknowledgement stage) or WorldReadyTimeoutSec,
 *                 then ActiveAtServerTime = Now + StartLeadSec; the first Tick at or after it (within the D22 1 ms due
 *                 tolerance) hands over to the Active-entry step (FRoundMatchCore, stories rmf-041/042).
 *   Watchdog      PreparingWatchdogSec until freeze, then F6's PreparingWatchdogEff from Preparing entry -> PreparingWatchdog
 */

/** ADR-0002 section 8.6's closed Preparing abort-code set (decision D48: story rmf-034 uses exactly this set). */
enum class ERoundPreparingAbortCode : uint8
{
	None,
	BadParams,
	TuningLoadFailed,
	TuningSnapshotFault,
	DeckInvalid,
	AllocatorMisuse,
	LayoutAborted,
	LayoutSeedMismatch,
	TeamAbsent,
	BindFailed,
	PlacementInvariant,
	PlacementInfeasible,
	NpcCountTooLow,
	PreparingWatchdog,
	/**
	 * Objectives' ValidateSelectedChain failed (an invalid chain, no cooked chain, or several with no selection rule;
	 * ADR-0020 section 2, AC-OC-01; appended by Objectives story oc-005, an amendment of ADR-0002 section 8.6's set).
	 */
	ObjectiveChainInvalid,
};

/** Number of abort codes (None excluded). */
inline constexpr int32 GRoundPreparingAbortCodeCount = 14;

/** Where the sequence is. */
enum class ERoundPreparingStep : uint8
{
	Idle,
	AwaitTuning,
	AwaitLayoutAndBarrier,
	AwaitWorldReady,
	AwaitActiveAt,
	Done,
};

/** This Preparing's acknowledgement nonce (ADR-0005 Key Interfaces FAckEpoch). */
struct FRoundAckEpoch
{
	uint32 Nonce = 0;

	bool operator==(const FRoundAckEpoch& Other) const { return Nonce == Other.Nonce; }
	bool operator!=(const FRoundAckEpoch& Other) const { return Nonce != Other.Nonce; }
};

/** One acknowledgement as Session forwards it (travel complete, or the world-ready second stage). */
struct FRoundTravelAck
{
	FParticipantId Participant;
	FRoundAckEpoch Epoch;
	/** The delivering connection's Session ConnectionGeneration (a replaced controller carries an older one). */
	uint32 ConnectionGeneration = 0;
};

/** One P6 spawn as Round issued it (for observation: frame, order, control and gate). */
struct FRoundSpawnRecord
{
	FRoundSpawnEntry Entry;
	uint64 ServerTickIndex = 0;
	ERoundSpawnResult Result = ERoundSpawnResult::Spawned;
};

class STEALTHGAMEPLAY_API FRoundPreparingSequencer
{
public:
	/** The owner's reactions (FRoundMatchCore sets them). */
	struct FCallbacks
	{
		/** An R6 failure or the watchdog: the owner runs the one abort path (story rmf-034). */
		TFunction<void(ERoundPreparingAbortCode)> OnAbort;
		/** P7 reached ActiveAtServerTime: the owner runs the unbound guard and the Active-entry step. */
		TFunction<void()> OnReadyForActive;
		/** ExpectedPawnCount and ActiveAtServerTime for FRoundPublicState. */
		TFunction<void(int32 /*ExpectedPawnCount*/, double /*ActiveAtServerTime*/)> OnPublicFields;
		/** F6 at freeze chose T_needed over PreparingWatchdogSec (logged as a warning by the owner). */
		TFunction<void(const StealthRoundFormulas::FWatchdogBound&)> OnWatchdogWarning;
		/** P3: write the Event Log header once (before the layout request and the deal). */
		TFunction<void(uint64 /*TuningHash*/)> OnWriteHeader;
	};

	FRoundPreparingSequencer(const FRoundGateways& InGateways, const IStealthClock* InClock);

	void SetCallbacks(FCallbacks InCallbacks) { Callbacks = MoveTemp(InCallbacks); }
	void SetGateways(const FRoundGateways& InGateways) { Gateways = InGateways; }

	/** Preparing entered (after travel acceptance): P2 "Preparing entered" goes to TDC. */
	void Begin(const FRoundRuleParams& InParams, int32 InSeed, FRoundAckEpoch InEpoch);
	/** Stops the sequence (abort, Reset, session end); clears every per-round record. */
	void Cancel();

	bool IsRunning() const { return Step != ERoundPreparingStep::Idle && Step != ERoundPreparingStep::Done; }
	ERoundPreparingStep GetStep() const { return Step; }

	// --- Owner events --------------------------------------------------------------------------------------------------

	/** P1: Session OnTravelFailed(BadParams). */
	void OnTravelFailed();
	/** P2 -> P3 (a duplicate is ignored). */
	void OnTuningReady(uint64 TuningHash);
	/** P2: TDC Failed. */
	void OnTuningFailed();
	/** P2: TDC's server self-check fault (never a client hash mismatch). */
	void OnTuningSnapshotFault();

	/** Objectives' chain validation failed while Preparing runs: abort ObjectiveChainInvalid (story oc-005). */
	void OnObjectiveChainInvalid();
	/** P4. */
	void OnLayoutReady(uint32 LayoutId, int32 LayoutSeed);
	void OnLayoutAborted();
	/** P5: Session's single round acknowledgement; false when dropped (stale) or not needed. */
	bool OnParticipantTravelComplete(const FRoundTravelAck& Ack);
	/** P7: the second acknowledgement stage; false when dropped or not expected yet. */
	bool OnParticipantWorldReady(const FRoundTravelAck& Ack);
	/** Session moved the participant to Connected: a gated pawn opens (Level Socket Contract R4.8). */
	void OnParticipantConnected(FParticipantId Participant);

	/** One Round tick: watchdog, barrier and P6, P7, the Active hand-over. */
	void Tick();

	// --- Observation ---------------------------------------------------------------------------------------------------

	FRoundAckEpoch GetAckEpoch() const { return Epoch; }
	int32 GetSeed() const { return Seed; }
	int32 GetAllocateCallCount() const { return AllocateCalls; }
	bool IsTuningFrozen() const { return bTuningReady; }
	bool IsLayoutReady() const { return bLayoutReady; }
	const StealthRoundFormulas::FWatchdogBound& GetWatchdogBound() const { return WatchdogBound; }
	double GetWatchdogDeadline() const { return WatchdogDeadline; }
	int32 GetExpectedPawnCount() const { return ExpectedPawnCount; }
	double GetActiveAtServerTime() const { return ActiveAtServerTime; }
	const TArray<FRoundSpawnRecord>& GetSpawnRecords() const { return SpawnRecords; }
	/** Participants Disconnected (or Dormant) at P6: bound, Ai-driven pawns whose grace starts at Active (F4). */
	const TArray<FParticipantId>& GetDisconnectedAtSpawn() const { return DisconnectedAtSpawn; }
	/** The epoch the P6 spawns were bound under (unset before P6). */
	const FEpochToken& GetSpawnEpoch() const { return SpawnEpoch; }
	/** Participants whose spawn gate is still closed. */
	bool IsGateClosed(FParticipantId Participant) const { return Gated.Contains(Participant.Value); }
	/** True when every Connected participant has a valid acknowledgement (the P5 barrier). */
	bool IsBarrierSatisfied() const;

private:
	double Now() const;
	void Abort(ERoundPreparingAbortCode Code);
	bool IsAckCurrent(const FRoundTravelAck& Ack) const;
	bool AreAllConnectedWorldReady() const;
	/** P5b and P6; false when it aborted. */
	bool RunStartSpawn();

	FRoundGateways Gateways;
	const IStealthClock* Clock = nullptr;
	FCallbacks Callbacks;

	FRoundRuleParams Params;
	ERoundPreparingStep Step = ERoundPreparingStep::Idle;
	int32 Seed = 0;
	FRoundAckEpoch Epoch;
	double EnteredAt = 0.0;
	double WatchdogDeadline = 0.0;
	StealthRoundFormulas::FWatchdogBound WatchdogBound;

	bool bTuningReady = false;
	uint64 TuningHash = 0;
	int32 AllocateCalls = 0;
	bool bLayoutReady = false;
	uint32 LayoutId = 0;

	/** ParticipantId -> the ConnectionGeneration its accepted acknowledgement carried. */
	TMap<uint32, uint32> Acked;
	TMap<uint32, uint32> WorldReady;

	int32 ExpectedPawnCount = 0;
	double WorldReadyDeadline = 0.0;
	double ActiveAtServerTime = 0.0;
	TArray<FRoundSpawnRecord> SpawnRecords;
	TArray<FParticipantId> DisconnectedAtSpawn;
	TSet<uint32> Gated;
	FEpochToken SpawnEpoch;
};
