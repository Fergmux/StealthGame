#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Server/CrowdDirectorCore.h"
#include "Server/CrowdDueTimeList.h"
#include "Server/CrowdFollower.h"
#include "Server/CrowdPathBudget.h"
#include "Server/CrowdSpatialHash.h"
#include "Server/CrowdTellSampler.h"
#include "Server/CrowdTypes.h"
#include "Server/CrowdVirtualInput.h"

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
};

/** Where the agent's motion goes: the sink in the game, a recorder in specs. Only Move and Look exist here. */
class ICrowdAgentOutput
{
public:
	virtual ~ICrowdAgentOutput() = default;
	virtual bool Move(FVector2D Direction, EGait Gait) = 0;
	virtual bool Look(FRotator Rotation) = 0;
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
};

/**
 * FCrowdAgent: one driven pawn's behaviour (ADR-0016 sections 3 and 4; GDD CNB-R3, R5, R6, R12; stories cnb-009 to
 * cnb-016). Plain C++, owned by the crowd controller (and by FCrowdSimWorld in specs). It holds the follower, the
 * virtual input layer and the per-frame due-time list; its decisions run at the decision grid (Decide, called by the
 * StateTree task) and when a due entry wakes it. Every output is a Move or a Look through ICrowdAgentOutput.
 *
 * Catalogue in this build: C1 route walk, C2 pauses (with context facing), C3 conversation (partner-blind), C4 group
 * drift, C5 yield, C6 re-centre (SCL's idle turn-to-look), G2 glances, G3 goods, G4 salient glances, G5 partner look,
 * G6 long holds (the tell sampler's HeadHold clock), the stationary-stretch watchdog. Red-herring behaviours beyond G6,
 * reactions, tending, bushes and exits arrive with stories cnb-019 onward.
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

	ICrowdAgentObserver* Observer = nullptr;
#if !UE_BUILD_SHIPPING
	TOptional<FRotator> LookOverride;
#endif
};
