#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Server/EncounterTypes.h"
#include "Server/StealthPenaltySeams.h"
#include "Server/StrikeTallySource.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/WeakObjectPtrTemplates.h"

#include "StealthPenaltyService.generated.h"

class APawn;
class FPenaltyCore;
class FResolutionQueueCore;
class ICrowdControl;
class IRoundLogWriter;
class IStealthClock;
class IStealthFaultSink;
class UEncounterService;
class URosterStore;
struct FRosterEvent;
struct FPenaltyServiceAdapters;

/**
 * OnDueTimeChanged(P, T_due) (ADR-0019 section 6): server-only, raised on every change of a participant's DueServerTime
 * (set at scheduling and readmission; unset at embodiment, cancellation, round end and reset). The T_due carrier adapter
 * (FPenaltyDueAdapter, story nkp-011) is its consumer; nothing replicates from it directly.
 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnPenaltyDueTimeChanged, FParticipantId /*Participant*/, TOptional<double> /*DueServerTime*/);

/** What the composition root hands the service (Clock required; the rest optional, each documented). */
struct FPenaltyServiceInit
{
	const IStealthClock* Clock = nullptr;
	/** The round queue: the slot context of Penalty's slot-local flags and Event Log rows (null: no row is written). */
	FResolutionQueueCore* Queue = nullptr;
	/** Round's Event Log writer (FRoundMatchCore::GetRoundLogWriter()). */
	IRoundLogWriter* Log = nullptr;
	/** Overrides the world's Roster store (null: URosterStore::Get(World)). */
	URosterStore* Roster = nullptr;
	/** The encounter service whose OnPawnKilled Penalty subscribes to (null: no kill reaches Penalty). */
	UEncounterService* Encounters = nullptr;
	/** Overrides the tuning stand-in (null: the schema defaults, until the TDC store feeds features the frozen set). */
	const IPenaltyTuningSource* Tuning = nullptr;
	/** The Roster owner-view feed (null until roster story psr-038 builds SetOwnerTally: nothing is re-pushed). */
	IPenaltyOwnerViewFeed* OwnerView = nullptr;
	/** Crowd's adoption of the exile's released body (Round's ICrowdControl; null: step 3 is skipped). */
	ICrowdControl* Crowd = nullptr;
	/** Character Pool ReleaseCard and the factory's RequestHandOver (null: steps 2 and 4 are skipped). */
	IPenaltyPawnGateway* Pawns = nullptr;
	/**
	 * The respawn commit's gateways (story nkp-012: PickRespawnPawn, the factory takeover, Crowd's release and hand-back, the
	 * card; null: a fired respawn retries after EmergenceRetrySec and no informant is re-pawned).
	 */
	IPenaltyRespawnGateway* RespawnGateway = nullptr;
	IStealthFaultSink* Faults = nullptr;
};

/**
 * UPenaltyService: the NPC-Kill Penalty & Informants server facade (ADR-0019 section 1; ADR-0007 thin facade over
 * FPenaltyCore; stories nkp-001 to nkp-004). A world subsystem that is bound only on the server (Bind refuses a client
 * world); it owns the plain core and the production adapters and adds no rule of its own. Nothing in it is a replicated
 * property or reachable from Blueprint: StrikeCount never leaves the server, the tally reaches its owner only through
 * FStrikeResult and the owner re-push.
 *
 * Seams: implements IStrikeTallySource (ADR-0018 section 2.1 step 8); subscribes to UEncounterService's OnPawnKilled and
 * to the Roster's events (OnRosterReset, OnRosterRoundEnded, reclaim and connection changes). In the OnPawnKilled fan-out
 * it runs the exile of the L-th ungraced NPC kill (story nkp-005: Roster StrikeLimit, ReleaseCard, Crowd adoption, the
 * factory hand-over) and the informant kill's inline RetireBinding (story nkp-008) through IPenaltyBodyGateway over the
 * Roster store, Init.Crowd and Init.Pawns.
 *
 * Respawns (stories nkp-009, nkp-010, nkp-012): the IInformantBodyProvider calls arrive through OnInformantAwaitingBody /
 * OnInformantReadmitted / OnInformantPawnLost (the composition root registers a forwarding provider with Round); the service
 * registers the TimerFired(InformantRespawn) resolver on Init.Queue (FPenaltyRespawnQueueAdapter, which is also the core's
 * timer seam) and the commit runs through Init.RespawnGateway. OnDueTimeChanged feeds the T_due carrier (story nkp-011).
 *
 * Usage (the composition root, after the encounter service is initialised on the round queue):
 *   UPenaltyService* Penalty = World->GetSubsystem<UPenaltyService>();
 *   FPenaltyServiceInit Init; Init.Clock = Clock; Init.Queue = Queue; Init.Log = MatchCore->GetRoundLogWriter();
 *   Init.Encounters = Encounters; Init.Faults = Faults;
 *   Penalty->Bind(Init);            // and Penalty->OnRoundPreparing() at each Preparing; Unbind() before the queue goes
 */
UCLASS(NotBlueprintable, NotBlueprintType)
class STEALTHGAMEPLAY_API UPenaltyService : public UWorldSubsystem, public IStrikeTallySource
{
	GENERATED_BODY()

public:
	/** Game worlds only (the editor world never has one). */
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Deinitialize() override;

	/** Builds the adapters and the core and subscribes. False (nothing built) without a clock or on a client world. */
	bool Bind(const FPenaltyServiceInit& Init);
	/** Unsubscribes and releases the core (call before the round queue or the encounter core is released). */
	void Unbind();
	bool IsBound() const { return Core.IsValid(); }

	/** Round entered Preparing: the core latches L and the release grace (TDC rule CR-11 Latching). */
	void OnRoundPreparing();
	/** The participant's C-TeamView was rebound after a rejoin (Roster OnViewRebound, story psr-038): the owner re-push. */
	void OnViewRebound(FParticipantId Participant);

	// IInformantBodyProvider (ADR-0019 section 1; forwarded by the composition root's provider registered with Round)

	/** Round, after its elimination check and never when latched: schedule (or Absent) for an Active -> Informant cause. */
	void OnInformantAwaitingBody(FParticipantId Participant, ERespawnCause Cause, double LostServerTime);
	/** Round R18: a returning informant after its Session acknowledgement (Awaiting, or the commit now). */
	void OnInformantReadmitted(FParticipantId Participant);
	/** Round R17.9 informant case: an embodied informant's body lost without a kill (D = 0, the commit now). */
	void OnInformantPawnLost(FParticipantId Participant);

	/** Server-only (ADR-0019 Key Interfaces): the participant's sub-state, and T_due while one is pending. */
	EInformantLifeState GetLifeState(FParticipantId Participant) const;
	TOptional<double> GetDueServerTime(FParticipantId Participant) const;

	/** Raised on every DueServerTime change (server-only; the T_due carrier adapter binds it). */
	FOnPenaltyDueTimeChanged OnDueTimeChanged;

	// IStrikeTallySource
	virtual FStrikeTally GetTallyForResult(const APawn* Attacker, EStrikeResult Result) override;
	virtual FStrikeTally GetTallyForStrike(const APawn* Attacker, EStrikeResult Result, const FClientCommandSeq& Seq) override;

	/** The core (valid between Bind and Unbind; server-only reads for tests and dumps). */
	FPenaltyCore* GetCore() const { return Core.Get(); }

#if WITH_STEALTH_TEST_SEAMS
	/** Test seam (story nkp-021's PIE tests; never Shipping): the server-only strike count k of Participant (0 unbound). */
	uint8 GetStrikeCountForTest(FParticipantId Participant) const;
	/** Test seam: the server-side wrong-target tally t of Participant (0 unbound). */
	uint8 GetWrongTargetTallyForTest(FParticipantId Participant) const;
#endif

	/** The opaque, stable key of a pawn (its object index and serial number); unset for null. */
	static FPenaltyPawnKey MakePawnKey(const APawn* Pawn);

private:
	void HandlePawnKilled(APawn* Victim, APawn* Killer, EVictimKind Kind, EKillCause Cause);
	void HandleRosterEvent(const FRosterEvent& Event);
	FParticipantId ResolveParticipant(const APawn* Pawn) const;

	/** The attacker's lockout end from its action state (Assassination's defence state; -1 when it has none). */
	static double ReadLockoutEnd(const APawn* Pawn);

	TSharedPtr<FPenaltyServiceAdapters> Adapters;
	TSharedPtr<FPenaltyCore> Core;
	TWeakObjectPtr<URosterStore> Roster;
	TWeakObjectPtr<UEncounterService> Encounters;
	/** The delegate subscribed to (compared before removal: a released encounter core has none). */
	FOnPawnKilled* BoundPawnKilled = nullptr;
	FDelegateHandle PawnKilledHandle;
	FDelegateHandle RosterEventHandle;
};
