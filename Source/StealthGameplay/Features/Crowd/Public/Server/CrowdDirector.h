#pragma once

#include "StealthServerOnlyGuard.h"

#include "AI/Navigation/NavigationTypes.h"
#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "Server/Core/Round/StealthCrowdControl.h"
#include "Server/CrowdAgent.h"
#include "Server/CrowdAssetKeys.h"
#include "Server/CrowdDensity.h"
#include "Server/CrowdDirectorCore.h"
#include "Server/CrowdPathBudget.h"
#include "Server/CrowdPerceptionFilter.h"
#include "Server/CrowdSpatialHash.h"
#include "Server/CrowdStallRegulator.h"
#include "Server/CrowdStrikeStimulus.h"
#include "Server/CrowdTellSampler.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/WeakInterfacePtr.h"
#include "View/StealthTuningPublicSubstructs.h"

#include "CrowdDirector.generated.h"

class ANavigationData;
class AStealthCrowdController;
class AStealthPawn;
class UCrowdDirector;
class UStateTree;

/** The Director's own tick (ADR-0016 section 1): a world subsystem cannot choose its tick group. */
struct FCrowdDirectorTickFunction : public FTickFunction
{
	UCrowdDirector* Director = nullptr;
	virtual void ExecuteTick(float DeltaTime, ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent) override;
	virtual FString DiagnosticMessage() override { return TEXT("FCrowdDirectorTickFunction"); }
};

/** Per-frame decision accounting (AC-CNB-33b): the most decisions any frame held. */
struct FCrowdDecisionStats
{
	uint64 Frame = 0;
	int32 ThisFrame = 0;
	int32 MaxPerFrame = 0;
	int64 Total = 0;
};

/**
 * UCrowdDirector: the server-only Crowd Director (ADR-0016 section 1; GDD CNB-R1, R12, R15; stories cnb-004 onward). A
 * UWorldSubsystem facade over FCrowdDirectorCore (ledger, draws, phase gate, decision grid) plus the frame services the
 * agents share: the 2 m spatial hash of public pawn facts (filtered by ICrowdPerceptionFilter), the path-start budget
 * (FindPathAsync), the line-of-sight ray budget, the stall regulator and watchdog clocks, the tell sampler, the density
 * model and start placement, salient-stimulus fan-out and Halt. One FCrowdDirectorTickFunction in TG_PrePhysics,
 * authority only. Implements ICrowdControl (Round) and ITakeoverEligibilitySource (Layout, Penalty), registered weakly by
 * the composition root; no lower layer names this type. Never reads a Roster binding, a team, a participant state or a
 * site assignment: the driver kind arrives with the ICrowdControl call.
 */
UCLASS(NotBlueprintType, NotBlueprintable)
class STEALTHGAMEPLAY_API UCrowdDirector : public UWorldSubsystem, public ICrowdControl, public ITakeoverEligibilitySource, public ICrowdAgentWorld, public ICrowdAgentStarter
{
	GENERATED_BODY()

public:
	// ---- Subsystem ---------------------------------------------------------------------------------------------

	/** False on clients: the crowd is server-only. */
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	/** The net-mode rule ShouldCreateSubsystem applies (a client never creates the Director). */
	static bool ShouldCreateForNetMode(ENetMode NetMode) { return NetMode != NM_Client; }
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	static UCrowdDirector* Get(const UWorld* World);

	/** StateTree.Component.ScheduledTickEnabled (when off every crowd tree would silently tick every frame, E6). */
	static bool IsScheduledTickEnabled();
	/** True when the init-time ScheduledTickEnabled assertion failed (reported to the log once). */
	bool DidScheduledTickAssertFail() const { return bScheduledTickAssertFailed; }

	// ---- Composition root ---------------------------------------------------------------------------------------

	/** The perception filter (Concealment, ADR-0017); none means everything perceivable and bush herrings disabled. */
	void SetPerceptionFilter(TWeakInterfacePtr<ICrowdPerceptionFilter> Filter) { PerceptionFilter = Filter; }
	/**
	 * The red-herring sites (LSC TraversalPoints, Objectives courts, Concealment bushes; stories cnb-019 to cnb-022). Plain
	 * C++ and not owned: the composition root keeps the implementer alive for the world. None (this build) disables the
	 * vault, climb, decoy and bush clocks; runs, reversals, braces and head holds need no site.
	 */
	void SetHerringSites(ICrowdHerringSites* Sites) { HerringSites = Sites; }
	/** Which tells this world's crowd plays (the sampler's enabled set at round start). */
	bool IsTellEnabled(ECrowdTell Tell) const { return Tells.IsEnabled(Tell); }
	/** The graybox crowd area (until the layout service supplies the CrowdRoute graph and area types). */
	void SetCrowdArea(const FVector& Center, float RadiusCm, float MarketRadiusCm);
	/**
	 * Replaces the frozen Crowd tuning for the NEXT round (tests; production reads the schema defaults until the tuning
	 * store lands). Refused (false, counted) while a round's crowd is Active: no Crowd value changes after OnTuningReady
	 * (AC-CNB-37).
	 */
	bool SetTuningForRound(const FCrowdTuning_Server& Tuning);
	/** Tuning writes refused during an active round. */
	int32 GetRefusedTuningWrites() const { return RefusedTuningWrites; }
	/** The asset keys (StateTrees, motion profile). */
	void SetAssetKeys(const FCrowdAssetKeys& Keys) { AssetKeys = Keys; }

	// ---- ICrowdControl (Round) -------------------------------------------------------------------------------------

	virtual void StartRound() override;
	virtual void Halt() override;
	virtual void DriveBoundPawn(AStealthPawn* Pawn) override;
	virtual void ReleaseControl(AStealthPawn* Pawn) override;
	virtual void AdoptReleasedPawn(AStealthPawn* Pawn, const FCrowdAdoptParams& Params) override;
	virtual void RequestPawnStop(const struct FRoundLivePawn& Pawn, double BoundSec) override;

	// ---- ITakeoverEligibilitySource -------------------------------------------------------------------------------

	virtual ETakeoverEligibility GetTakeoverEligibility(const AStealthPawn* Pawn) const override;
	virtual bool TryReleaseNpc(AStealthPawn* Pawn, FEpochToken Expected) override;

	// ---- Controllers ------------------------------------------------------------------------------------------------

	/** OnPossess of a crowd controller: registers the agent with its pending driver kind and binds its sink. */
	void AttachDriver(AStealthCrowdController* Controller, AStealthPawn* Pawn);
	void DetachDriver(AStealthCrowdController* Controller);
	/** The pawn's public facts this frame (from the hash, or computed when the hash has not seen it yet). */
	FCrowdPerceivedPawn GetSelfFacts(const AStealthCrowdController& Controller) const;
	/** One tree or fallback decision happened this frame (AC-CNB-33b accounting). */
	void NoteDecision();
	/**
	 * Admits one off-grid (wake or event) tree decision in this Director frame. Grid decisions are bounded by the phase
	 * stratification (at most ceil(N / frames per period) + 1 a frame); wakes get the remaining slack of the story 004
	 * bound (WakeDecisionsPerFrame), and a refused wake waits a frame (or for its grid point). Never refuses with no core.
	 */
	bool AdmitWakeDecision();
	/** Off-grid decisions a frame (the bound's slack above the stratified grid). */
	static constexpr int32 WakeDecisionsPerFrame = 1;
	/** Wakes deferred by AdmitWakeDecision (a frame each). */
	int64 GetDeferredWakes() const { return DeferredWakes; }
	const FCrowdDecisionStats& GetDecisionStats() const { return DecisionStats; }
	/** Director frames ticked (the frame index of the decision accounting). */
	uint64 GetDirectorFrame() const { return DirectorFrame; }

	/** Makes Tick (a StateTree component's) run after the Director's tick; IsTickPrerequisiteOf checks it. */
	void AddTickPrerequisiteTo(FTickFunction& Tick);
	bool IsTickPrerequisiteOf(const FTickFunction& Tick) const;

	// ---- Start placement (story cnb-017) --------------------------------------------------------------------------

	/**
	 * The start transform of the Index-th NPC of the round's spawn list (F1 shares on the NpcRoutes stream, node
	 * capacity counting pawns of any kind). False when the crowd has no route nodes yet (the caller falls back).
	 */
	bool RequestStartPlacement(int32 Index, int32 NpcCount, FTransform& OutTransform);

	// ---- ICrowdAgentWorld -------------------------------------------------------------------------------------------

	virtual const FCrowdTuning_Server& GetCrowdTuning() const override;
	virtual const FLocomotionTuning_Public& GetLocomotionTuning() const override { return Locomotion; }
	virtual FCrowdDraws& GetDraws() override;
	virtual const FCrowdSpatialHash& GetHash() const override { return Hash; }
	virtual double GetLongPausePct(double Now) const override;
	virtual double GetStationaryAge(uint32 Ordinal, double Now) const override { return Stall.GetAge(Ordinal, Now); }
	virtual bool PickLegGoal(uint32 Ordinal, const FVector& From, double LegCm, FVector& OutGoal) override;
	virtual bool RequestPath(const FCrowdPathRequest& Request, TArray<FVector>& OutCorners) override;
	virtual bool FindPauseContext(const FVector& At, double Radius, FVector& OutLookPoint, bool& bOutGoods) const override;
	virtual bool HasLineOfSight(uint32 Ordinal, const FVector& Eye, const FVector& Target) override;
	virtual void ReportActivity(uint32 Ordinal, ECrowdActivity Activity) override;
	virtual FCrowdTellSampler& GetTellSampler() override { return Tells; }
	virtual uint32 GetCurrentControlEpoch(uint32 Ordinal) const override;
	virtual FCrowdActionTimings GetActionTimings() const override;
	virtual bool FindTraversalNear(uint32 Ordinal, const FVector& From, double MaxCm, bool bClimb, FCrowdTraversalSite& Out) override;
	virtual bool IsTraversing(uint32 Ordinal) const override;
	virtual int32 GetCourtCount() const override;
	virtual bool GetCourtStandPoint(int32 Court, const FVector& From, FCrowdCourtSite& Out) override;
	virtual bool FindBush(const FVector& From, TConstArrayView<FVector> Path, double RadiusCm, FCrowdBushSite& Out) override;
	virtual ECrowdBeginOutcome GetBeginOutcome(uint32 Ordinal, EActionType Type, double SubmittedAt, double Now) override;
	virtual bool IsOnNavigation(const FVector& Point) const override;
	virtual void OnAgentPromoted(uint32 Ordinal) override;
	virtual bool ClaimExitSlot(uint32 Ordinal) override;
	/** Pawns released by TryReleaseNpc and not yet adopted back (story cnb-030; diagnostics). */
	int32 GetReleasedForTakeoverCount() const { return ReleasedForTakeover.Num(); }
	/** Exile bodies waiting for the next scheduled exit slot (story cnb-029). */
	TConstArrayView<uint32> GetExitClaims() const { return ExitClaims; }
	/** ReleaseControl requests waiting for a frame start outside a traversal (story cnb-028). */
	int32 GetPendingReleaseCount() const { return PendingReleases.Num(); }

	// ---- ICrowdAgentStarter -------------------------------------------------------------------------------------------

	virtual void StartAgent(uint32 Ordinal, const FCrowdFirstActivity& First, double Now) override;

	// ---- Diagnostics (non-replicated, crowd facts only) -------------------------------------------------------------

	FCrowdDirectorCore* GetCore() { return Core.Get(); }
	const FCrowdDirectorCore* GetCore() const { return Core.Get(); }
	const FCrowdPathBudget& GetPathBudget() const { return PathBudget; }
	int32 GetMaxPathStartsInAFrame() const { return MaxPathStartsInAFrame; }
	int32 GetMaxRaysInAFrame() const { return MaxRaysInAFrame; }
	int64 GetPathQueriesBeforeLayoutReady() const { return PathQueriesBeforeLayout; }
	int32 GetNavRebuildsDuringRound() const { return NavRebuildsDuringRound; }
	int32 GetRouteNodeCount() const { return Density.GetNodes().Num(); }
	const FCrowdStallRegulator& GetStallRegulator() const { return Stall; }
	int32 NumControllers() const { return Controllers.Num(); }
	AStealthCrowdController* FindController(uint32 Ordinal) const;
	/** The round's Crowd tuning hash (schema keys + asset keys, story cnb-003). */
	uint64 GetRoundTuningHash() const { return RoundTuningHash; }

	/**
	 * R8 (story cnb-024): a strike started at Point by the pawn with SourceKey (OnActionStarted(Attack) only: kill, whiff
	 * and reversal alike). Queues the fan-out; responders are picked over the next frames within the ray budget.
	 */
	void NoteStrikeStarted(const FVector& Point, uint64 SourceKey, double StimulusTime);
	/** Strikes fanned out this round (diagnostics). */
	uint64 GetStrikeCount() const { return StrikeFanOut.GetStrikeCount(); }

	/** Engine frames between each FindPathAsync start and its delivery (spike cnb-001 AC-3; diagnostics). */
	TConstArrayView<int32> GetPathLatencyFrames() const { return PathLatencyFrames; }

	/** One Director frame (public for the tick function). */
	void TickDirector(float DeltaTime);

private:
	/** Counts a navmesh generation that finishes while the round is Active (the frozen-navmesh check, ADR-0014). */
	UFUNCTION()
	void HandleNavGenerationFinished(ANavigationData* NavData);

	/** UActionHub::OnActionStarted (server, inside the queue slot): Begin outcomes and, for Attack, the strike stimulus. */
	void HandleActionStarted(AStealthPawn* Pawn, EActionType Type);

private:
	void EnsureCore();
	/** The sampler's enabled tells: a tell with no site in this world never runs its clock. */
	void ConfigureTells();
	void ResetForNewRound();
	bool IsNavReady() const;
	void BuildRouteNodes();
	void BuildHash();
	void FanOutSalient(double Now);
	/** The strike fan-out's line-of-sight tests and responder calls within this frame's ray budget. */
	void ProcessStrikes(double Now);
	void StartPaths();
	void OnPathFound(uint32 QueryId, ENavigationQueryResult::Type Result, FNavPathSharedPtr Path);
	UStateTree* LoadTree(ECrowdDriverKind Kind) const;
	uint64 KeyOf(const AActor* Actor) const;

	TUniquePtr<FCrowdDirectorCore> Core;
	FCrowdTuning_Server Tuning;
	FLocomotionTuning_Public Locomotion;
	FCrowdAssetKeys AssetKeys;
	FCrowdSpatialHash Hash;
	FCrowdPathBudget PathBudget;
	FCrowdRayBudget RayBudget;
	FCrowdStallRegulator Stall;
	FCrowdTellSampler Tells;
	FCrowdDensityModel Density;
	FCrowdStrikeFanOut StrikeFanOut;
	/** Bound pawns handed back by ReleaseControl: detached at the start of a Director frame, never during a traversal. */
	TArray<uint32> PendingReleases;
	/** TryReleaseNpc's released set (a later AdoptReleasedPawn(Pawn, {}) of one of them is the reversal). */
	TSet<uint32> ReleasedForTakeover;
	/** Adoptions of pawns not driven by a crowd controller yet (applied when the controller attaches). */
	TMap<FObjectKey, FCrowdAdoptParams> PendingAdoptions;
	TArray<uint32> ExitClaims;
	FCrowdDirectorTickFunction TickFunction;
	FCrowdDecisionStats DecisionStats;
	TWeakInterfacePtr<ICrowdPerceptionFilter> PerceptionFilter;
	ICrowdHerringSites* HerringSites = nullptr;
	/** The last OnActionStarted time per (ordinal, action) of crowd-driven pawns (Begin outcomes). */
	TMap<uint64, double> ActionStartedAt;
	TMap<uint64, bool> WasTraversing;
	FDelegateHandle ActionStartedHandle;
	TMap<uint32, TWeakObjectPtr<AStealthCrowdController>> Controllers;
	TSet<FObjectKey> PendingRestricted;
	TMap<uint32, FCrowdPathRequest> InFlight;
	TMap<uint32, uint64> InFlightStartFrame;
	TArray<int32> PathLatencyFrames;
	TMap<uint64, bool> WasRunning;
	TArray<FTransform> StartPlacements;
	FVector CrowdCenter = FVector::ZeroVector;
	float CrowdRadiusCm = 3500.0f;
	float MarketRadiusCm = 1300.0f;
	int32 RaysThisFrame = 0;
	uint64 DirectorFrame = 0;
	int32 RefusedTuningWrites = 0;
	uint64 WakeFrame = 0;
	int32 WakesThisFrame = 0;
	int64 DeferredWakes = 0;
	int32 MaxRaysInAFrame = 0;
	int32 MaxPathStartsInAFrame = 0;
	int64 PathQueriesBeforeLayout = 0;
	int32 NavRebuildsDuringRound = 0;
	uint64 RoundTuningHash = 0;
	bool bTuningOverridden = false;
	bool bScheduledTickAssertFailed = false;
	bool bTickRegistered = false;
};
