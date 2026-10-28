#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Server/Core/CommandPredicate.h"
#include "Server/Core/InteractTargetOwner.h"
#include "Server/ObjectivesSeams.h"
#include "Server/Queue/StealthQueueResolver.h"
#include "Server/StealthTuningSubstructs.h"
#include "Subsystems/WorldSubsystem.h"
#include "Templates/PimplPtr.h"
#include "View/ObjectiveChainData.h"
#include "View/StealthTuningPublicSubstructs.h"

#include "ObjectivesService.generated.h"

class FObjectivesCore;
class FResolutionQueueCore;
class IObjectivesChainSource;
class IStealthFaultSink;
class UObjectiveChainData;
class URosterStore;
struct FRosterEvent;
struct FStealthServerTuning;
enum class EChainValidation : uint8;
enum class EChainValidationClause : uint8;
enum class EObjectivesCompletion : uint8;

/** What the composition root injects at Bind (ADR-0020 Key Interfaces; every pointer outlives the binding). */
struct FObjectivesServiceInit
{
	const IObjectivesRosterView* Roster = nullptr;
	const IObjectivesSessionView* Session = nullptr;
	const IObjectivesPawnControlView* PawnControl = nullptr;
	IRoundObjectiveHooks* Round = nullptr;
	const IObjectivesCourtGeometry* Courts = nullptr;
	const IObjectivesAssignmentSource* Assignment = nullptr;
	const IObjectivesChainSource* Chains = nullptr;
	/** Where each team's view is written: the team members' controllers' UObjectiveTeamViewComponent (story oc-011). */
	IObjectivesViewTargets* ViewTargets = nullptr;
	/** Optional observer of every team-level view change (tests and dumps); never a second writer. */
	IObjectivesViewSink* Views = nullptr;
	/**
	 * The Roster whose events the service subscribes to (story oc-012): OnParticipantViewRebound (the rejoin rebind write) and
	 * OnRosterEvent's RosterReset (the clear). Null: no subscription (headless rigs call the handlers directly).
	 */
	URosterStore* RosterEvents = nullptr;
	IObjectivesRivalRelease* RivalRelease = nullptr;
	/** The round queue the TimerFired(StageSignal) resolver registers on (null: not registered). */
	FResolutionQueueCore* Queue = nullptr;
	IStealthFaultSink* Faults = nullptr;
	/** The frozen tuning (stand-in: the schema defaults until the TDC store feeds features the frozen set). */
	FObjectivesTuning_Public PublicTuning;
	FObjectivesTuning_Server ServerTuning;
#if !UE_BUILD_SHIPPING
	/** Non-Shipping: submits Round's DebugItem for DebugCompleteStage (URoundMatchStore -> FRoundMatchCore::SubmitDebugObjectiveStage). */
	TFunction<bool(uint8 /*Team*/)> SubmitDebugStage;
#endif
};

/** The service's view of the chain selection and validation (the service's result; Round's abort carries no clause). */
struct FObjectivesSelection
{
	/** True when exactly one cooked chain exists and it validates. */
	bool bValid = false;
	EChainValidation Result;
	EChainValidationClause Clause;
	int32 Row = INDEX_NONE;
	/** Cooked chains found (exactly one is selectable in MVP). */
	int32 CookedChainCount = 0;
};

/**
 * The server-only Objectives facade (ADR-0020 section 1; stories oc-005..oc-009). A UWorldSubsystem created on server game
 * worlds only (never on a client), NotBlueprintType, over the headless FObjectivesCore. It implements ICommandPredicate
 * (Interact), IInteractTargetOwner and IQueueResolver (TimerFired(StageSignal), the rival releases of stories oc-014/015).
 * It spawns no actor, has no tick function, no FTimerManager use and no latent action: it runs only inside queue slots, the
 * OnLayoutReady / OnRoundActive server hooks and the Roster's rebind and reset handlers. The composition root binds the seams and
 * registers it weakly; every registration and subscription it makes is undone by Unbind / Deinitialize.
 *
 * The team view (stories oc-010..oc-012): the service is the core's view sink and the ONLY writer of
 * UObjectiveTeamViewComponent::View. A team-level change (change-only in the core) is written to the component of each
 * current controller of the team's members (Active or Informant), again change-only per component; the Roster's
 * OnParticipantViewRebound writes that participant's team view inline (the empty default before Active entry); RosterReset
 * clears the store and writes the empty default to every member.
 *
 * Usage (composition root, server):
 *   UObjectivesService* Objectives = World->GetSubsystem<UObjectivesService>();
 *   Objectives->Bind(Init);                                              // seams, tuning, StageSignal resolver
 *   Hub->RegisterPredicate(EActionType::Interact, TWeakInterfacePtr<ICommandPredicate>(Objectives));
 *   Hub->RegisterInteractTargetOwner(TWeakInterfacePtr<IInteractTargetOwner>(Objectives));
 *   // Preparing: if (!Objectives->ValidateSelectedChain(Tuning).IsValid) Round->NotifyObjectiveChainInvalid();
 *   // OnLayoutReady: Objectives->HandleLayoutReady(LayoutId, Seed);  OnRoundActive: Objectives->HandleRoundActive(Epoch, t0);
 */
UCLASS(NotBlueprintType, NotBlueprintable)
class STEALTHGAMEPLAY_API UObjectivesService : public UWorldSubsystem, public ICommandPredicate, public IInteractTargetOwner, public IQueueResolver,
	public IObjectivesViewSink
{
	GENERATED_BODY()

public:
	UObjectivesService();

	/** Server game worlds only (authority; never a pure client). */
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Deinitialize() override;

	/** The service of World, or null (client worlds have none). */
	static UObjectivesService* Get(const UWorld* World);

	/** Binds the seams and the tuning and registers the StageSignal resolver. False (nothing bound) without Round hooks. */
	bool Bind(const FObjectivesServiceInit& Init);

	/** Undoes Bind: the resolver registration is dropped and the store cleared. */
	void Unbind();

	bool IsBound() const { return bBound; }

	/** Round's queue is about to be destroyed: forget it without touching it (its registrations go with it). */
	void ForgetQueue() { Queue = nullptr; bStageSignalRegistered = false; }

	/** Replaces the frozen tuning (a publish: a running hold keeps its D; the next Begin reads the new value). */
	void SetTuning(const FObjectivesTuning_Public& PublicTuning, const FObjectivesTuning_Server& ServerTuning);

	// --- Lifecycle hooks (story oc-005) -------------------------------------------------------------------------------

	/**
	 * Preparing (ADR-0020 section 2): selects the one cooked chain and validates it with the round's tuning. Zero chains, more
	 * than one (no selection rule: never a silent pick) or an invalid chain is ObjectiveChainInvalid; the caller maps it to
	 * Round's PreparingFailed(ObjectiveChainInvalid) and nothing is bound afterwards.
	 */
	FObjectivesSelection ValidateSelectedChain(const FStealthServerTuning& Tuning);

	/** OnLayoutReady: binds the selected chain to the assignment (story oc-003). False when not selected or the bind failed. */
	bool HandleLayoutReady(uint64 LayoutId, int32 Seed);

	/**
	 * OnRoundActive (inside Round's Active-entry step): stores t0, both teams' Prelim1 sites Live, views pushed. A call before
	 * HandleLayoutReady reports one fault and changes nothing; a second call for the same epoch is ignored.
	 */
	bool HandleRoundActive(FEpochToken Epoch, double ActivePhaseStart);

	/** Round reached Ended: Objectives state is frozen. */
	void HandleRoundEnded();

	/** Round Reset: the store is cleared (as HandleRosterReset, without the view clear: the Roster's reset precedes it). */
	void HandleRoundReset();

	// --- The team view (stories oc-010..oc-012) -------------------------------------------------------------------------

	/** IObjectivesViewSink (the core's change-only push): writes View to every member of Team, then tells the observer. */
	virtual void PushTeamView(uint8 Team, const FObjectiveTeamView& View) override;

	/** Writes View to the component of each current controller of Team's members, change-only. Returns the components written. */
	int32 WriteTeamView(uint8 Team, const FObjectiveTeamView& View);

	/**
	 * Roster OnParticipantViewRebound (ADR-0020 section 6, ADR-0005 RebindView; story oc-012): writes Participant's team view
	 * inline to its new controller's component (the empty default before Active entry; nothing for a participant without a
	 * team). Only that participant's component is written. True when the component's value changed.
	 */
	bool HandleViewRebound(FParticipantId Participant);

	/**
	 * Roster OnRosterReset (story oc-012): clears the store (site states, claims, view caches, rival counts; the pending rival
	 * list of story oc-015 when it exists) and writes the empty default to every member's component, so no client keeps a
	 * site across rounds.
	 */
	void HandleRosterReset();

	/** Team's current view as the core builds it (server only: the clients' copy is their own component; tests and dumps). */
	FObjectiveTeamView BuildTeamView(int32 Team) const;

	/** Component writes made by the service (change-only writes that changed a value; tests). */
	int32 GetViewWriteCount() const { return ViewWriteCount; }

#if !UE_BUILD_SHIPPING
	/**
	 * Non-Shipping DebugObjectiveCompletion (ADR-0020 section 3; Round R13/R22): enqueues its own Round DebugItem slot, whose
	 * effect (ApplyDebugStage) calls the same ApplyCountedCompletion as a counted tend. False when Round refused the item.
	 */
	bool DebugCompleteStage(uint8 Team);

	/** The DebugItem slot's effect (Round's OnObjectiveDebugStage): Team's Live rows complete as if tended. */
	bool ApplyDebugStage(uint8 Team, FQueueKey Key);

	/** Non-Shipping drain-end claim check (ADR-0020 5.1): claims per fixture vs the IIF's Interacting slots per fixture. */
	int32 CheckClaimConsistency(const TMap<FCourtFixtureId, int32>& InteractingSlots);
#endif

	// --- ICommandPredicate (Interact; story oc-007; defined in ObjectivesInteractPredicate.cpp) -------------------------

	/** Reject(NotNow) for an Informant / RoundEnded pawn or a full fixture; otherwise Allow at every court. Never AllowNoEffect. */
	virtual FPredicateResult CanBegin(const AStealthPawn& Pawn, const FCommandPayload& Cmd) const override;

	// --- IInteractTargetOwner (story oc-006) --------------------------------------------------------------------------

	virtual double GetInteractHoldSec() const override;
	virtual void OnInteractExecuted(const AStealthPawn& Pawn, FCourtFixtureId Court, FQueueKey Key) override;
	virtual void OnInteractEnded(const AStealthPawn& Pawn, FCourtFixtureId Court, EInteractEnd End, FQueueKey Key) override;
	virtual void OnInteractCompleteLatched(const AStealthPawn& Pawn, FCourtFixtureId Court, FQueueKey Key) override;

	// --- IQueueResolver (TimerFired(StageSignal); stories oc-014/015 fill it) ------------------------------------------

	/** No release is ever scheduled before story oc-015: every StageSignal item is Voided (nothing published). */
	virtual EResolveResult Resolve(const FQueueItem& Item, FQueueKey Key) override;

	// --- Reads (Round's TeamSummary, the composition root's stand-ins, tests) ------------------------------------------

	/** Preliminary stages Team completed this round (IRoundObjectiveProgress). */
	int32 GetCompletedPreliminaryStages(int32 Team) const;

	/** Team's StageIndex (None before Active entry). Server-only: the team view (story oc-010) is the client's copy. */
	EObjectiveTeamStage GetObjectiveTeamStage(int32 Team) const;

#if !UE_BUILD_SHIPPING
	/** DEV ONLY (the StealthBot driver): the court of Team's first Live row, or an unset id. Never reaches a client. */
	FCourtFixtureId GetLiveCourtForDev(int32 Team) const;
#endif

	/** The last interact end's treatment (tests and dumps). */
	EObjectivesCompletion GetLastCompletion() const { return LastCompletion; }

	const UObjectiveChainData* GetSelectedChain() const { return SelectedChain.Get(); }
	uint64 GetLayoutId() const { return LayoutId; }
	bool IsStageSignalRegistered() const { return bStageSignalRegistered; }

	/** The headless core (read-only; tests and the composition root's stand-ins). */
	const FObjectivesCore& GetCore() const { return *Core; }

private:
	void ReportFault(int64 Detail) const;
	void HandleRosterEvent(const FRosterEvent& Event);
	void HandleViewRebound_Event(FParticipantId Participant);
	void Unsubscribe();

	TPimplPtr<FObjectivesCore> Core;

	UPROPERTY(Transient)
	TObjectPtr<const UObjectiveChainData> SelectedChain;

	const IObjectivesRosterView* Roster = nullptr;
	const IObjectivesAssignmentSource* Assignment = nullptr;
	const IObjectivesChainSource* Chains = nullptr;
	IObjectivesViewTargets* ViewTargets = nullptr;
	IObjectivesViewSink* ViewObserver = nullptr;
	TWeakObjectPtr<URosterStore> RosterEvents;
	FDelegateHandle RosterEventHandle;
	FDelegateHandle ViewReboundHandle;
	int32 ViewWriteCount = 0;
	FResolutionQueueCore* Queue = nullptr;
	IStealthFaultSink* Faults = nullptr;
#if !UE_BUILD_SHIPPING
	TFunction<bool(uint8)> SubmitDebugStage;
#endif
	uint64 LayoutId = 0;
	uint32 ActiveEpoch = 0;
	EObjectivesCompletion LastCompletion;
	bool bBound = false;
	bool bChainSelected = false;
	bool bStageSignalRegistered = false;
};
