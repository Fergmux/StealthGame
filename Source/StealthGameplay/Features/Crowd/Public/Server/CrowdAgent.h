#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Server/CrowdDirectorCore.h"
#include "Server/CrowdDueTimeList.h"
#include "Server/CrowdFollower.h"
#include "Server/CrowdHerringTypes.h"
#include "Server/CrowdPathBudget.h"
#include "Server/CrowdSpatialHash.h"
#include "Server/CrowdStrikeStimulus.h"
#include "Server/CrowdTellSampler.h"
#include "Server/CrowdTypes.h"
#include "Server/CrowdVirtualInput.h"
#include "Server/Core/PawnCommandTypes.h"
#include "View/CommandReplyTypes.h"

struct FCrowdTuning_Server;
struct FLocomotionTuning_Public;

/**
 * What a crowd agent needs from its world (ADR-0016 sections 1, 4, 5). The Director implements it over the real world;
 * FCrowdSimWorld implements it headless for the statistical specs. Nothing here exposes a player, a binding, a team or
 * a participant state.
 */
class ICrowdAgentWorld
{
public:
	virtual ~ICrowdAgentWorld() = default;

	virtual const FCrowdTuning_Server& GetCrowdTuning() const = 0;
	virtual const FLocomotionTuning_Public& GetLocomotionTuning() const = 0;
	virtual FCrowdDraws& GetDraws() = 0;
	/** This frame's spatial hash (public facts, filtered by the perception filter). */
	virtual const FCrowdSpatialHash& GetHash() const = 0;
	/** The long-pause share the next pause draws with (F4 regulator), percent. */
	virtual double GetLongPausePct(double Now) const = 0;
	/** The pawn's visible stationary stretch age (the watchdog clock), seconds. */
	virtual double GetStationaryAge(uint32 Ordinal, double Now) const = 0;
	/** A goal about LegCm away by the density model (F1); false when none. */
	virtual bool PickLegGoal(uint32 Ordinal, const FVector& From, double LegCm, FVector& OutGoal) = 0;
	/**
	 * Asks for a path. True with OutCorners when it is known at once (a pre-baked route edge, or a sim's direct path);
	 * false when an async query was queued through the path-start budget (the result arrives via OnPathResult).
	 */
	virtual bool RequestPath(const FCrowdPathRequest& Request, TArray<FVector>& OutCorners) = 0;
	/** A pause context (stall, mural, view) within Radius of At: its look point, and whether it is goods (G3). */
	virtual bool FindPauseContext(const FVector& At, double Radius, FVector& OutLookPoint, bool& bOutGoods) const = 0;
	/** Line of sight from an eye point to a target point (budgeted by the Director; false when deferred or blocked). */
	virtual bool HasLineOfSight(uint32 Ordinal, const FVector& Eye, const FVector& Target) = 0;
	/** The activity mirror (ADR-0016 section 3). */
	virtual void ReportActivity(uint32 Ordinal, ECrowdActivity Activity) = 0;
	virtual FCrowdTellSampler& GetTellSampler() = 0;
	/** The pawn's drive-token epoch now (path results are accepted only at the epoch they were requested with). */
	virtual uint32 GetCurrentControlEpoch(uint32 Ordinal) const = 0;

	// ---- Red-herring sites (stories cnb-019 to cnb-022). The defaults are a world with no sites. ----------------------

	/** Other owners' action timings (Objectives hold, Prepare cap and cooldown, bush re-enter cooldown). */
	virtual FCrowdActionTimings GetActionTimings() const { return FCrowdActionTimings(); }
	/** ICrowdHerringSites::FindTraversalNear of the world's sites. */
	virtual bool FindTraversalNear(uint32 Ordinal, const FVector& From, double MaxCm, bool bClimb, FCrowdTraversalSite& Out) { return false; }
	/** The SCL traversal dry run (CanBegin with the CrowdDryRunReachCm / FacingDeg margins). */
	virtual bool CanBeginTraversal(uint32 Ordinal, const FCrowdPerceivedPawn& Self, const FCrowdTraversalSite& Site) { return true; }
	/** True while the pawn is in the shared SCL traversal state. */
	virtual bool IsTraversing(uint32 Ordinal) const { return false; }
	virtual int32 GetCourtCount() const { return 0; }
	virtual bool GetCourtStandPoint(int32 Court, const FVector& From, FCrowdCourtSite& Out) { return false; }
	virtual bool FindBush(const FVector& From, TConstArrayView<FVector> Path, double RadiusCm, FCrowdBushSite& Out) { return false; }
	/**
	 * What became of the pawn's last discrete Begin of Type sent at SubmittedAt (the NPC's reply sink is null: a refused
	 * Begin is learned from the action hub). A world with no legality owner for Type answers Accepted.
	 */
	virtual ECrowdBeginOutcome GetBeginOutcome(uint32 Ordinal, EActionType Type, double SubmittedAt, double Now) { return ECrowdBeginOutcome::Accepted; }
	/** Ambient exits (story cnb-026): LSC's clear check passed and the pawn was absorbed (removed by the world). */
	virtual bool TryAbsorb(uint32 Ordinal, const FCrowdExitSite& Site, double Now) { return false; }
	/** True while some pawn's eye sample sees the exiting pawn inside the alley (LSC R5.2). */
	virtual bool IsExitWatched(uint32 Ordinal, const FCrowdExitSite& Site, double Now) { return false; }
	/** The exiting pawn gave up (watched for CrowdExitGiveUpSec): its cycle is abandoned. */
	virtual void OnAmbientExitAbandoned(uint32 Ordinal, const FCrowdExitSite& Site) {}
	/** Story cnb-029: a released pawn joined the full catalogue (the controller swaps to the full sink and tree). */
	virtual void OnAgentPromoted(uint32 Ordinal) {}
	/** Story cnb-029: an exile body takes the next scheduled exit slot (false when the world runs no schedule). */
	virtual bool ClaimExitSlot(uint32 Ordinal) { return false; }
	/** True when Point projects onto the navmesh (the takeover hold's heading check, story cnb-028). */
	virtual bool IsOnNavigation(const FVector& Point) const { return true; }
};

/** Where the agent's motion goes: the sink in the game, a recorder in specs. Only Move and Look exist here. */
class ICrowdAgentOutput
{
public:
	virtual ~ICrowdAgentOutput() = default;
	virtual bool Move(FVector2D Direction, EGait Gait) = 0;
	virtual bool Look(FRotator Rotation) = 0;

	/**
	 * The discrete commands of the full driver (FCrowdCommandSink; stories cnb-019 to cnb-022). The defaults refuse: a
	 * restricted driver's output has none, so a bound pawn can never issue one. There is no Attack (AC-CNB-11).
	 */
	virtual bool DryRun(EActionType Type, const FCourtFixtureId* Target) { return false; }
	virtual bool Traverse(const FTraversalHint& Hint) { return false; }
	virtual bool InteractBegin(FCourtFixtureId Target) { return false; }
	virtual bool InteractEnd() { return false; }
	virtual bool PrepareBegin() { return false; }
	virtual bool PrepareEnd() { return false; }
	virtual bool Hide(const FHideCommand& Command) { return false; }
};

/** The kind of a glance (CNB-R6). */
enum class ECrowdGlanceKind : uint8
{
	/** G2: a passer-by, 0.4-2.5 s. */
	PasserBy,
	/** G6: the red-herring long hold on a passer-by, 2.5-6 s (T13). */
	LongHold,
	/** G3: goods, pitched down. */
	Goods,
	/** G4: salient motion after the reaction delay. */
	Salient,
	/** G5: a conversation partner. */
	Partner,
	/** A long pause facing its context (stall, mural, view). */
	Context,
	/** R8: a reaction glance at a strike point (a fixed point, never the attacker). */
	Reaction,
};

/**
 * Observes an agent's decisions (specs and the dev evidence; null in production). Every callback carries only crowd
 * facts: an ordinal, opaque neighbour keys and times.
 */
class ICrowdAgentObserver
{
public:
	virtual ~ICrowdAgentObserver() = default;
	virtual void OnLegStarted(uint32 Ordinal, double Now, float Pace, double LegCm) {}
	virtual void OnPaceChanged(uint32 Ordinal, double Now, float Pace, ECrowdPaceCause Cause) {}
	virtual void OnPauseStarted(uint32 Ordinal, double Now, bool bLong, double LengthSec, bool bHasContext, const FVector& ContextPoint) {}
	virtual void OnPauseEnded(uint32 Ordinal, double Now, float LookYawDeg) {}
	virtual void OnWatchdog(uint32 Ordinal, double Now, double StretchAge) {}
	virtual void OnGlance(uint32 Ordinal, double Now, ECrowdGlanceKind Kind, uint64 TargetKey, double HoldSec) {}
	/** C3: a face-toward decision for a resting neighbour (initiator) or for being faced (reply). */
	virtual void OnConverseDecision(uint32 Ordinal, double Now, uint64 NeighbourKey, bool bReply, bool bDecided) {}
	virtual void OnConversationFormed(uint32 Ordinal, double Now, uint64 PartnerKey, double LengthSec) {}
	virtual void OnConversationEnded(uint32 Ordinal, double Now, uint64 PartnerKey, double PartnerLookShare, bool bTurnedAway) {}
	virtual void OnSalientGlanceScheduled(uint32 Ordinal, double StimulusTime, double StartTime) {}
	// Red herrings (stories cnb-019 to cnb-022).
	/** The tell sampler accepted a tell for this pawn (it is queued for its behaviour). */
	virtual void OnTellAccepted(uint32 Ordinal, double Now, ECrowdTell Tell) {}
	/** R8 (story cnb-024): the reaction a responder drew (after the 6 m fold) and the one it performed at StartTime. */
	virtual void OnStrikeReaction(uint32 Ordinal, double StimulusTime, double StartTime, ECrowdStrikeReaction Drawn, ECrowdStrikeReaction Performed, double DistanceCm) {}
	/** An accepted tell was performed (the visible moment) or dropped (the chain gap, or no chance within its wait). */
	virtual void OnTellPerformed(uint32 Ordinal, double Now, ECrowdTell Tell) {}
	virtual void OnTellDropped(uint32 Ordinal, double Now, ECrowdTell Tell) {}
	/** A run leg starts (T1, or an R8 panic run): the planned bout and the drawn destination. */
	virtual void OnRunLeg(uint32 Ordinal, double Now, double BoutSec, const FVector& Goal, bool bPanic) {}
	/** Traverse was sent after the dry run (or the dry run refused it). */
	virtual void OnTraverse(uint32 Ordinal, double Now, const FCrowdTraversalSite& Site, bool bClimb) {}
	virtual void OnTraversalRefused(uint32 Ordinal, double Now, uint16 PointId) {}
	/** T12: the reversal's stop starts (Move(0)). */
	virtual void OnReversal(uint32 Ordinal, double Now) {}
	/** T4: PrepareBegin was sent; bTriggered: at a flinch trigger (else an ambient startle). */
	virtual void OnBrace(uint32 Ordinal, double Now, bool bTriggered, double HoldSec, bool bCapped) {}
	/** T3: a court was drawn (ExcludedCourt: the court just refused, or INDEX_NONE). */
	virtual void OnDecoyCourtChosen(uint32 Ordinal, double Now, int32 Court, int32 ExcludedCourt) {}
	virtual void OnDecoyHold(uint32 Ordinal, double Now, int32 Court, double HoldSec, bool bComplete) {}
	virtual void OnDecoyRefused(uint32 Ordinal, double Now, int32 Court) {}
	/** T5: Hide(Enter) with the stay drawn at entry; Hide(Exit). */
	virtual void OnBushEnter(uint32 Ordinal, double Now, uint16 BushId, double StaySec) {}
	virtual void OnBushExit(uint32 Ordinal, double Now, uint16 BushId) {}
};

/**
 * FCrowdAgent: one driven pawn's behaviour (ADR-0016 sections 3 and 4; GDD CNB-R3, R5, R6, R12; stories cnb-009 to
 * cnb-016). Plain C++, owned by the crowd controller (and by FCrowdSimWorld in specs). It holds the follower, the
 * virtual input layer and the per-frame due-time list; its decisions run at the decision grid (Decide, called by the
 * StateTree task) and when a due entry wakes it. Every output is a Move or a Look through ICrowdAgentOutput.
 *
 * Catalogue in this build: C1 route walk, C2 pauses (with context facing), C3 conversation (partner-blind), C4 group
 * drift, C5 yield, C6 re-centre (SCL's idle turn-to-look), G2 glances, G3 goods, G4 salient glances, G5 partner look,
 * G6 long holds (the tell sampler's HeadHold clock), the stationary-stretch watchdog, and the red herrings of CNB-R7
 * (stories cnb-019 to cnb-022): T1 run legs, T2 vaults and climbs, T12 reversals, T4 braces, T3 decoy tends and T5
 * bush entries. Each accepted tell is performed through the same commands a human uses (Move with the Run gait,
 * Traverse, Prepare, Interact, Hide after the dry run), and only if no different tell was shown within the chain gap.
 */
class STEALTHGAMEPLAY_API FCrowdAgent
{
public:
	/**
	 * The watchdog fires this long before CrowdMaxIdleSec, so the leg's first motion lands inside the bound: a leg that
	 * starts facing away from a conversation partner turns in place and clears the partner first (measured up to 1.6 s
	 * in the 60-minute StretchBound run).
	 */
	static constexpr double WatchdogLeadSec = 2.5;
	/** At or below this speed the pawn is at rest for conversation (cm/s). */
	static constexpr double RestSpeedCm = 5.0;
	/** A moving pawn for G2 (cm/s). */
	static constexpr double MovingSpeedCm = 20.0;
	/** The retry of a G2 glance with no target (s). */
	static constexpr double GlanceRetrySec = 1.0;

	/** Binds the pawn: ordinal, driver kind, device profile, lane offset and steering phase. */
	void Initialize(uint32 InOrdinal, ECrowdDriverKind InDriver, const FCrowdPerceivedPawn& Self, double GridPhaseSec, TConstArrayView<FCrowdDeviceProfile> Profiles,
		ICrowdAgentWorld& World);

	/** OnRoundActive (or a join): starts the first activity and issues the first command (a Look and a Move) now. */
	void Start(const FCrowdFirstActivity& First, double Now, const FCrowdPerceivedPawn& Self, ICrowdAgentWorld& World, ICrowdAgentOutput& Output);

	/** Every controller tick: due entries, follower, gaze, virtual input, then Move and Look through Output. */
	void Tick(double Now, float Dt, const FCrowdPerceivedPawn& Self, ICrowdAgentWorld& World, ICrowdAgentOutput& Output);

	/** A decision (the grid tick or a wake). Returns true when the activity changed. */
	bool Decide(double Now, const FCrowdPerceivedPawn& Self, ICrowdAgentWorld& World, ICrowdAgentOutput& Output);

	/** True when a due entry or an event asked for a decision before the next grid point. */
	bool WantsWake() const { return bWake; }
	void ClearWake() { bWake = false; }

	/** A path result from the Director (the leg-id and epoch gate is applied here). Returns true when accepted. */
	bool OnPathResult(const FCrowdPathResult& Result, ICrowdAgentWorld& World);

	/** A salient stimulus (run start, traversal landing) the Director saw with line of sight within range, at StimulusTime. */
	void OnSalientStimulus(double StimulusTime, uint64 SourceKey, const FVector& Where, double Now, ICrowdAgentWorld& World);

	/**
	 * R8 (story cnb-024): this pawn was picked as a responder of a strike at Point StimulusTime (line of sight checked by
	 * the fan-out). Draws the reaction and its delay from the stimulus time; nothing about the strike's outcome is known.
	 */
	void OnStrikeStimulus(double StimulusTime, const FVector& Point, uint64 SourceKey, double DistanceCm, double Now, ICrowdAgentWorld& World);
	/** True inside CrowdReactRefractorySec of the last strike this pawn responded to (the fan-out skips it). */
	bool IsStrikeRefractory(double Now, const FCrowdTuning_Server& Tuning) const;
	/** The R8 reaction drawn for the last strike this pawn responded to (diagnostics). */
	ECrowdStrikeReaction GetDrawnReaction() const { return PendingReaction; }

	/** Halt: terminal; Move(0), no further decisions, the head stays where it is. */
	void Halt(ICrowdAgentOutput& Output);

	/** Forces a new leg now (the watchdog, an abandoned behaviour). */
	void ForceLeg(double Now, const FCrowdPerceivedPawn& Self, ICrowdAgentWorld& World, ICrowdAgentOutput& Output);

	ECrowdActivity GetActivity() const { return Activity; }
	/** When the current rest (a pause, or a formed conversation) ends; negative when not resting (the F4 projection). */
	double GetRestEnd() const
	{
		return Activity == ECrowdActivity::Pausing ? PauseEnd : (Activity == ECrowdActivity::Conversing && bConversationFormed ? ConversationEnd : -1.0);
	}
	ECrowdDriverKind GetDriver() const { return Driver; }
	uint32 GetOrdinal() const { return Ordinal; }
	bool IsStarted() const { return bStarted; }
	const FCrowdFollower& GetFollower() const { return Follower; }
	const FCrowdVirtualInput& GetVirtualInput() const { return VirtualInput; }
	FCrowdVirtualInput& GetVirtualInputMutable() { return VirtualInput; }
	const FCrowdDueTimeList& GetDueList() const { return DueList; }
	/** The last Move and Look issued (diagnostics). */
	FVector2D GetLastMove() const { return LastMove; }
	FRotator GetLastLook() const { return LastLook; }
	/** The current conversation partner key (0 when none). */
	uint64 GetPartnerKey() const { return PartnerKey; }

	void SetObserver(ICrowdAgentObserver* InObserver) { Observer = InObserver; }

#if !UE_BUILD_SHIPPING
	/** Test hook (non-Shipping): every Look this agent emits is Override while set (the observer pitch PIE test). */
	void SetLookOverride(const TOptional<FRotator>& Override) { LookOverride = Override; }
#endif

	/** Enter/leave a tell's due behaviour from outside (the tell sampler's accepted HeadHold makes the next G2 long). */
	void QueueHeadHold() { bHeadHoldPending = true; }

	/** Story cnb-026: may this pawn take an ambient exit slot now (eligible: full driver, nothing in progress). */
	bool CanTakeExitSlot() const;
	/** Story cnb-026: "leave by the alley" as the next leg. */
	void StartAmbientExit(double Now, const FCrowdPerceivedPawn& Self, ICrowdAgentWorld& World, const FCrowdExitSite& Site);
	bool IsExiting() const { return Mode == EHerringMode::ExitWalk || Mode == EHerringMode::ExitWait; }
	const FCrowdExitSite& GetExitSite() const { return ExitSite; }
	/** True once the world absorbed the pawn (it issues nothing more). */
	bool IsAbsorbed() const { return bAbsorbed; }

	/**
	 * Story cnb-029 (ReleaseToCrowd): the pawn joins the full catalogue at its next leg boundary once RestrictedUntil has
	 * passed (a concealed pawn at once, staying in the bush for the conditioned stay); bExitAtNextSlot: it then takes the
	 * next scheduled exit slot.
	 */
	void ScheduleAdoption(double Now, double RestrictedUntil, bool bExitAtNextSlot, bool bConcealed, double ConcealedSec, ICrowdAgentWorld& World);
	bool IsAdoptionPending() const { return bAdoptPending; }
	/** True from an exile body's promotion until its exit slot starts (AlleyExit eligibility). */
	bool IsAwaitingExitSlot() const { return bAwaitingExitSlot; }
	/** The remaining bush stay of a pawn concealed for ElapsedSec, conditioned on that time (log-normal, uncapped). */
	static double ConditionedStaySec(double U, double ElapsedSec, double MedianSec, double Sigma);

	/** True while a restricted driver's takeover hold runs (story cnb-028). */
	bool IsInHandoverHold() const { return HoldUntil >= 0.0; }

	/** The red herring in progress (diagnostics and tests). */
	enum class EHerringMode : uint8
	{
		None,
		RunLeg,
		ReversalStop,
		TraverseApproach,
		Traversing,
		BraceHold,
		DecoyWalk,
		DecoyBeginCheck,
		DecoyHold,
		BushWalk,
		BushHidden,
		/** R8: a reaction pause facing the strike point. */
		ReactPause,
		/** CNB-R9.1: walking into the alley of an exit slot, then waiting for the clear check. */
		ExitWalk,
		ExitWait,
	};
	EHerringMode GetHerringMode() const { return Mode; }
	/** True while a T4 brace is due (the flinch window). */
	bool IsBraceDue() const { return BraceDueUntil >= 0.0; }

private:
	void SetActivity(ECrowdActivity NewActivity, ICrowdAgentWorld& World);
	void BeginLeg(double Now, const FCrowdPerceivedPawn& Self, ICrowdAgentWorld& World, double LegCm);
	void BeginPause(double Now, const FCrowdPerceivedPawn& Self, ICrowdAgentWorld& World, double ForcedLength = -1.0);
	void EndPause(double Now);
	/** C3 at rest: initiate toward a resting neighbour, or answer one that faces this pawn. */
	bool TryConversation(double Now, const FCrowdPerceivedPawn& Self, ICrowdAgentWorld& World);
	void UpdateConversation(double Now, const FCrowdPerceivedPawn& Self, ICrowdAgentWorld& World);
	void EndConversation(double Now, bool bTurnedAway, const FCrowdPerceivedPawn& Self, ICrowdAgentWorld& World);
	void ProcessDue(double Now, const FCrowdPerceivedPawn& Self, ICrowdAgentWorld& World);
	void StartGlance(double Now, const FCrowdPerceivedPawn& Self, ICrowdAgentWorld& World);
	FCrowdGazeIntent ComputeGaze(double Now, const FCrowdPerceivedPawn& Self, ICrowdAgentWorld& World);
	void EmitInput(double Now, float Dt, const FCrowdFollowerOutput& Motion, const FCrowdGazeIntent& Gaze, ICrowdAgentWorld& World, ICrowdAgentOutput& Output);
	static float YawTo(const FVector& From, const FVector& To);
	/** Delay of a goods look into a long pause at a goods context. */
	static double GoodsDelay();
	/** True when Other's body faces Self within AngleDeg. */
	static bool Faces(const FCrowdPerceivedPawn& Other, const FVector& SelfLocation, double AngleDeg);

	// ---- Red herrings (stories cnb-019 to cnb-022; CrowdHerringRunTasks.cpp, CrowdHerringTraverseTasks.cpp,
	// CrowdReversalTasks.cpp, CrowdBraceTasks.cpp, CrowdDecoyTendTasks.cpp, CrowdBushTasks.cpp) ----------------------------

	/** Starts a leg to an explicit goal (corners from the path source, then Tail appended). */
	void BeginLegTo(double Now, const FCrowdPerceivedPawn& Self, ICrowdAgentWorld& World, const FVector& Goal, bool bKeepPace, EGait Gait = EGait::Walk,
		double ReleaseAt = -1.0, TConstArrayView<FVector> Tail = TConstArrayView<FVector>());
	/** An accepted tell from the sampler: queued for its behaviour. */
	void OnTellAccepted(ECrowdTell Tell, double Now, ICrowdAgentWorld& World);
	/** Every tick before the follower: pending tells start, herrings in progress advance. */
	void TickHerrings(double Now, const FCrowdPerceivedPawn& Self, ICrowdAgentWorld& World, ICrowdAgentOutput& Output);
	/** The leg ended at rest while a herring owned it; true when the herring consumed the arrival (no pause). */
	bool OnHerringArrival(double Now, const FCrowdPerceivedPawn& Self, ICrowdAgentWorld& World, ICrowdAgentOutput& Output);
	/**
	 * The visible moment of a tell: shown unless a different tell was shown within the chain gap or another tell's
	 * herring is already on its way to its visible moment. False defers (the tell stays pending; it is dropped only when
	 * its wait runs out). bFromClock false: an R8 reaction tell.
	 */
	bool TryPerformTell(ECrowdTell Tell, double Now, ICrowdAgentWorld& World, bool bFromClock = true);
	/** True when Tell may not be shown now (TryPerformTell's test, without showing it). */
	bool MustDeferTell(ECrowdTell Tell, double Now, ICrowdAgentWorld& World) const;
	/** The tell a herring in progress is walking toward (Count when none). */
	ECrowdTell TellInProgress() const;
	/** A herring walk that failed before its visible moment: the tell is pending again (its wait counts from acceptance). */
	void RequeueInProgress(ECrowdTell Tell);
	void DropPending(ECrowdTell Tell, double Now, ICrowdAgentWorld& World);
	bool IsPending(ECrowdTell Tell) const { return PendingSince[static_cast<int32>(Tell)] >= 0.0; }
	/** True when the agent may start a herring now (full driver, not halted, nothing else in progress, not conversing). */
	bool CanStartHerring() const;
	/** Leaves a pause for a herring (the pause's due entries are cancelled). */
	void LeaveRestForHerring(double Now);

	void StartRunLeg(double Now, const FCrowdPerceivedPawn& Self, ICrowdAgentWorld& World, bool bPanic, double BoutSec, const FVector* AwayDir = nullptr);
	void StartStrikeReaction(double Now, const FCrowdPerceivedPawn& Self, ICrowdAgentWorld& World, ICrowdAgentOutput& Output);
	// ---- Participant-bound driving (story cnb-028; CrowdBoundDriver.cpp) ----------------------------------------------
	/** The takeover hold of a restricted driver: Move(current heading, Walk) for HandoverHoldSec, or Move(0). */
	void StartHandoverHold(double Now, const FCrowdPerceivedPawn& Self, ICrowdAgentWorld& World);
	void TickAmbientExit(double Now, float Dt, const FCrowdPerceivedPawn& Self, ICrowdAgentWorld& World);
	/** The adoption's promotion (at a leg boundary, or at once in a bush). True when it happened. */
	bool TryPromote(double Now, const FCrowdPerceivedPawn& Self, ICrowdAgentWorld& World);
	/** True while the hold owns this tick (it emitted the Move and Look). */
	bool TickHandoverHold(double Now, float Dt, const FCrowdPerceivedPawn& Self, ICrowdAgentWorld& World, ICrowdAgentOutput& Output);
	void StartReversal(double Now, const FCrowdPerceivedPawn& Self, ICrowdAgentWorld& World);
	bool StartTraversal(double Now, const FCrowdPerceivedPawn& Self, ICrowdAgentWorld& World, bool bClimb);
	void TickTraversal(double Now, const FCrowdPerceivedPawn& Self, ICrowdAgentWorld& World, ICrowdAgentOutput& Output);
	bool FlinchTriggered(const FCrowdPerceivedPawn& Self, ICrowdAgentWorld& World) const;
	bool BraceFitsStretch(double Now, double HoldSec, ICrowdAgentWorld& World) const;
	void PerformBrace(double Now, const FCrowdPerceivedPawn& Self, ICrowdAgentWorld& World, ICrowdAgentOutput& Output, bool bTriggered);
	void EndBrace(double Now, const FCrowdPerceivedPawn& Self, ICrowdAgentWorld& World, ICrowdAgentOutput& Output);
	bool ChooseCourt(double Now, const FCrowdPerceivedPawn& Self, ICrowdAgentWorld& World, int32 ExcludedCourt);
	void TickDecoy(double Now, const FCrowdPerceivedPawn& Self, ICrowdAgentWorld& World, ICrowdAgentOutput& Output);
	void ArriveAtCourt(double Now, const FCrowdPerceivedPawn& Self, ICrowdAgentWorld& World, ICrowdAgentOutput& Output);
	bool StartBushWalk(double Now, const FCrowdPerceivedPawn& Self, ICrowdAgentWorld& World);
	void ArriveAtBush(double Now, const FCrowdPerceivedPawn& Self, ICrowdAgentWorld& World, ICrowdAgentOutput& Output);
	void TickBush(double Now, const FCrowdPerceivedPawn& Self, ICrowdAgentWorld& World, ICrowdAgentOutput& Output);

	uint32 Ordinal = 0;
	ECrowdDriverKind Driver = ECrowdDriverKind::Full;
	ECrowdActivity Activity = ECrowdActivity::Dormant;
	bool bStarted = false;
	bool bHalted = false;
	bool bWake = false;
	uint32 NextLegId = 1;
	FCrowdFollower Follower;
	FCrowdVirtualInput VirtualInput;
	FCrowdDueTimeList DueList;
	FVector2D LastMove = FVector2D::ZeroVector;
	FRotator LastLook = FRotator::ZeroRotator;
	bool bMoveSent = false;
	/** Where the pending path request started (the corners of its result start there). */
	FVector PathFrom = FVector::ZeroVector;
	/** The leg was abandoned (failed or partial query, blocked ladder): the next decision draws a new destination. */
	bool bLegAbandoned = false;

	// Pause state (C2).
	double PauseEnd = 0.0;
	bool bPauseLong = false;
	bool bPauseHasContext = false;
	bool bPauseGoods = false;
	FVector PauseContextPoint = FVector::ZeroVector;
	bool bConversationChecked = false;

	// Conversation state (C3, G5).
	uint64 PartnerKey = 0;
	bool bConversationFormed = false;
	double ConversationEnd = 0.0;
	double PartnerLookShare = 0.7;
	double PartnerLookTime = 0.0;
	double ConversationTime = 0.0;
	bool bPartnerLookOn = true;
	double PartnerLookSwitch = 0.0;
	/** Neighbours already decided for the current rest (a decision is drawn once per neighbour per rest). */
	TArray<uint64, TInlineAllocator<4>> DecidedNeighbours;

	// Gaze state (G2-G6).
	ECrowdGlanceKind GlanceKind = ECrowdGlanceKind::PasserBy;
	bool bGlanceActive = false;
	uint64 GlanceTargetKey = 0;
	FVector GlancePoint = FVector::ZeroVector;
	float GlancePitch = 0.0f;
	bool bHeadHoldPending = false;
	FVector SalientPoint = FVector::ZeroVector;
	uint64 SalientKey = 0;
	double LastSalientTime = -1.0e9;

	// Red-herring state (stories cnb-019 to cnb-022).
	EHerringMode Mode = EHerringMode::None;
	/** Accepted tells waiting for their behaviour (the time each was accepted; negative when none is pending). */
	double PendingSince[static_cast<int32>(ECrowdTell::Count)] = { -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0 };
	FVector ReversalTarget = FVector::ZeroVector;
	FCrowdTraversalSite TraversalSite;
	bool bTraversalClimb = false;
	double TraversalStarted = 0.0;
	bool bTraversalSent = false;
	TArray<FVector, TInlineAllocator<4>> TraversalTail;
	TMap<uint16, double> BlockedTraversals;
	double BraceDueUntil = -1.0;
	double BraceEnd = 0.0;
	double LastBraceTime = -1.0e9;
	double BraceHoldPlanned = -1.0;
	/** The next brace is an R8 startle (it anchors the clocks' gap; it does not come from the T4 clock). */
	bool bBraceIsStartle = false;
	/** When the tell of the herring walk in progress was accepted. */
	double InProgressAcceptedAt = -1.0;
	bool bBraceCapPlanned = false;
	ECrowdActivity BraceResume = ECrowdActivity::Routing;
	FVector BraceResumeGoal = FVector::ZeroVector;
	bool bBraceResumeLeg = false;
	FCrowdCourtSite DecoySite;
	double DecoyBeginAt = 0.0;
	double DecoyHoldEnd = 0.0;
	int32 DecoyAttempts = 0;
	FCrowdBushSite BushSite;
	double BushExitAt = 0.0;
	double LastBushExit = -1.0e9;
	// R8 reaction state (story cnb-024).
	double LastStrikeReaction = -1.0e9;
	ECrowdStrikeReaction PendingReaction = ECrowdStrikeReaction::None;
	bool bStrikeReactionDue = false;
	FVector StrikePoint = FVector::ZeroVector;
	uint64 StrikeSourceKey = 0;
	double StrikeStimulusTime = 0.0;
	double StrikeDistanceCm = 0.0;
	double ReactEnd = 0.0;
	// Ambient exit (story cnb-026).
	FCrowdExitSite ExitSite;
	double ExitWatchedSec = 0.0;
	bool bAbsorbed = false;
	double LastHerringTick = -1.0;
	// ReleaseToCrowd (story cnb-029).
	bool bAdoptPending = false;
	double PromoteAt = 0.0;
	bool bExileExitPending = false;
	bool bAwaitingExitSlot = false;
	bool bReleasedConcealed = false;
	double ReleasedConcealedStaySec = 0.0;
	// Takeover hold (story cnb-028).
	double HoldUntil = -1.0;
	FVector2D HoldDirection = FVector2D::ZeroVector;
	float HoldMagnitude = 0.0f;
	/** G2 never picks the strike source until this time (no responder's gaze tracks the attacker). */
	uint64 AvoidGlanceKey = 0;
	double AvoidGlanceUntil = -1.0;

	ICrowdAgentObserver* Observer = nullptr;
#if !UE_BUILD_SHIPPING
	TOptional<FRotator> LookOverride;
#endif
};
