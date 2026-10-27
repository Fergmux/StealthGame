#pragma once

// The headless crowd simulation of the StealthGame.Crowd.* statistical specs (stories cnb-004 to cnb-018). It runs the
// real crowd code (FCrowdDirectorCore, FCrowdAgent with its follower, avoidance and virtual input, FCrowdSpatialHash,
// FCrowdStallRegulator, FCrowdTellSampler, FCrowdDensityModel) and the real SCL movement model
// (FStealthLocomotionModel::Step) at a fixed 60 Hz step on an open plane, with no UWorld, so a 10-minute reference scene
// runs in seconds and every run is deterministic. Scripted pawns (a "player" standing or walking a fixed vector) share
// the public-fact hash with the NPCs exactly as player-driven pawns do in the game: nothing marks their kind.
// Test-only.

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Server/CrowdAgent.h"
#include "Server/CrowdAmbientSchedule.h"
#include "Server/CrowdDensity.h"
#include "Server/CrowdDirectorCore.h"
#include "Server/CrowdStallRegulator.h"
#include "Server/CrowdTellSampler.h"
#include "Server/StealthTuningSubstructs.h"
#include "View/Core/StealthLocomotionModel.h"
#include "View/StealthTuningPublicSubstructs.h"

namespace StealthCrowdSim
{
	/** One simulated pawn body (NPC or scripted): the SCL model state and the public facts it shows. */
	struct FBody
	{
		uint64 Key = 0;
		uint32 Ordinal = 0;
		FVector Location = FVector::ZeroVector;
		FStealthMotionState Motion;
		/** The control rotation (an NPC's Look; a scripted pawn's camera). */
		FRotator Control = FRotator::ZeroRotator;
		FVector2D MoveIntent = FVector2D::ZeroVector;
		EGait Gait = EGait::Walk;
		bool bPerceivable = true;
		bool bObstacle = true;
		/** The shared traversal state (the sim's stand-in for SCL's): a straight crossing over Duration. */
		bool bTraversing = false;
		FVector TraverseFrom = FVector::ZeroVector;
		FVector TraverseTo = FVector::ZeroVector;
		double TraverseStart = 0.0;
		double TraverseDuration = 0.0;
		int64 LandingFrame = -1;
		/** Absorbed into an alley or killed: out of the world (no facts, no steps). */
		bool bGone = false;

		FVector Velocity() const
		{
			return FRotator(0.0, Motion.FacingYawDeg, 0.0).Vector() * Motion.Speed;
		}
	};

	/** A scripted (human-like) pawn: holds a Move intent and camera the test sets; never an agent. */
	struct FScripted
	{
		FBody Body;
	};

	/** A pause context fixture (a stall, mural or view). */
	struct FContext
	{
		FVector Point = FVector::ZeroVector;
		bool bGoods = false;
	};

	/** World configuration. */
	struct FConfig
	{
		int32 NpcCount = 50;
		uint64 SubSeed = 0x0BADC0FFEE0DDF00ull;
		/** Half-size of the square walkable plane (cm). */
		double HalfSizeCm = 4000.0;
		/** The route-node grid spacing (cm). */
		double NodeSpacingCm = 500.0;
		double StepSec = 1.0 / 60.0;
		/** Ordinal offset of the NPCs (scripted pawns take the ordinals below). */
		uint32 FirstNpcOrdinal = 0;
		FCrowdTuning_Server Tuning;
		FLocomotionTuning_Public Locomotion;
		/** Market radius around the origin (cm); outside is Causeway. */
		double MarketRadiusCm = 1300.0;
		/** The tells whose clocks run (bit per ECrowdTell). Default: G6 only, the catalogue of the earlier specs. */
		uint32 EnabledTellMask = 1u << static_cast<uint32>(ECrowdTell::HeadHold);
		static constexpr uint32 AllTells = (1u << static_cast<uint32>(ECrowdTell::Count)) - 1u;
	};

	/** A court fixture: four stand points around its centre (Objectives FixtureInteractSideCount 4). */
	struct FCourtFixture
	{
		FVector Center = FVector::ZeroVector;
		uint16 FixtureId = 0;
		int32 Interactors = 0;
		/** Test hook: refuse this many Begins that passed the dry run (a fixture that filled in between). */
		int32 RefuseNextBegins = 0;
	};

	/** A bush (Concealment): any number of pawns may share it; four open sides. */
	struct FBushFixture
	{
		uint16 BushId = 0;
		FVector Center = FVector::ZeroVector;
		double RadiusCm = 150.0;
	};

	/** One discrete command an NPC sent (the sim's command log: the IIF and Concealment entry logs). */
	struct FCommandRecord
	{
		enum class EKind : uint8 { DryRun, Traverse, InteractBegin, InteractEnd, PrepareBegin, PrepareEnd, HideEnter, HideExit };
		EKind Kind = EKind::DryRun;
		uint32 Ordinal = 0;
		double Time = 0.0;
		/** DryRun: the action type; Traverse: the point id; Interact: the fixture id; Hide: the bush id or side. */
		int32 Arg = 0;
		bool bAccepted = true;
	};

	class FWorld;

	/** One NPC: its agent and the output adapter that records the Move/Look calls (and the discrete commands). */
	struct FNpc : public ICrowdAgentOutput
	{
		FBody Body;
		FCrowdAgent Agent;
		FWorld* World = nullptr;
		double NextDecision = 0.0;
		int64 MoveCalls = 0;
		int64 LookCalls = 0;
		/** The court index this NPC interacts at (INDEX_NONE when none) and its last Begin's outcome. */
		int32 InteractCourt = INDEX_NONE;
		double LastBeginTime = -1.0;
		bool bLastBeginAccepted = true;
		double LastPrepareBegin = -1.0e9;
		int32 HiddenBush = INDEX_NONE;

		virtual bool DryRun(EActionType Type, const FCourtFixtureId* Target) override;
		virtual bool Traverse(const FTraversalHint& Hint) override;
		virtual bool InteractBegin(FCourtFixtureId Target) override;
		virtual bool InteractEnd() override;
		virtual bool PrepareBegin() override;
		virtual bool PrepareEnd() override;
		virtual bool Hide(const FHideCommand& Command) override;

		virtual bool Move(FVector2D Direction, EGait InGait) override
		{
			Body.MoveIntent = Direction.Size() > 1.0 ? Direction.GetSafeNormal() : Direction;
			Body.Gait = InGait;
			++MoveCalls;
			return true;
		}
		virtual bool Look(FRotator Rotation) override
		{
			Body.Control = Rotation;
			++LookCalls;
			return true;
		}
	};

	/** An emergence socket (LSC): the alley mouth and the absorb/emerge point inside it. */
	struct FSocketFixture
	{
		FVector Mouth = FVector::ZeroVector;
		FVector Inside = FVector::ZeroVector;
	};

	/** An emergence or absorption the sim performed (time, socket, ordinal), for the witness checks. */
	struct FSocketEvent
	{
		double Time = 0.0;
		int32 Socket = INDEX_NONE;
		uint32 Ordinal = 0;
		bool bEmergence = true;
		/** Some pawn's eye sample saw the socket at that instant (must never be true). */
		bool bWitnessed = false;
	};

	/** The simulation (ICrowdAgentWorld for its agents; the round queue and the LSC host of the ambient schedule). */
	class FWorld : public ICrowdAgentWorld, public ICrowdAgentStarter, public ICrowdScheduleQueue, public ICrowdScheduleHost
	{
	public:
		explicit FWorld(const FConfig& InConfig);

		/** Adds a scripted pawn at Location (before or after the start); returns its index. */
		int32 AddScripted(const FVector& Location, float YawDeg);
		FScripted& GetScripted(int32 Index) { return Scripted[Index]; }

		/** Spawns the NPCs (at density start placement) and registers them; Dormant until StartRound. */
		void SpawnNpcs();
		/** Spawns one NPC at an explicit location (fixture scenes); Restricted registers a participant-bound driver. */
		FNpc& SpawnNpcAt(const FVector& Location, float YawDeg, ECrowdDriverKind Driver = ECrowdDriverKind::Full, float InitialSpeed = 0.0f, EGait InitialGait = EGait::Walk);
		/** OnRoundActive. */
		void StartRound();

		/** One fixed step. */
		void Step();
		/** Steps until Now >= Until. */
		void RunUntil(double Until);

		double GetNow() const { return Now; }
		int32 NumNpcs() const { return Npcs.Num(); }
		FNpc& GetNpc(int32 Index) { return *Npcs[Index]; }
		FCrowdDirectorCore& GetCore() { return Core; }
		FCrowdStallRegulator& GetStall() { return Stall; }
		FCrowdDensityModel& GetDensity() { return Density; }
		void AddContext(const FContext& Context) { Contexts.Add(Context); }
		void SetObserver(ICrowdAgentObserver* InObserver);
		/** Decisions per frame (max over the run). */
		int32 GetMaxDecisionsPerFrame() const { return MaxDecisionsPerFrame; }
		int64 GetFrame() const { return Frame; }

		/** The first activity an NPC starts the round with (instead of its drawn one; fixture scenes). */
		void OverrideFirstActivity(uint32 Ordinal, const FCrowdFirstActivity& First) { FirstOverrides.Add(Ordinal, First); }
		/** A salient stimulus (a run start) by Source at its location now: every started NPC within CrowdSalientRangeCm
		 *  (line of sight is open in the sim) receives it, as the Director's fan-out does. Returns the receivers. */
		int32 EmitSalient(const FBody& Source);
		/**
		 * A strike started at Point by SourceKey now (R8, story cnb-024): the real fan-out (FCrowdStrikeFanOut) over every
		 * started NPC within CrowdStrikeReactRadiusCm outside its refractory window (line of sight is open in the sim).
		 * Returns the responders.
		 */
		int32 EmitStrike(const FVector& Point, uint64 SourceKey);

		/** Optional fixed routes: when set, PickLegGoal returns the next point of the NPC's own loop (corner tests). */
		void SetRouteFixture(TFunction<bool(uint32 Ordinal, const FVector& From, FVector& OutGoal)> InRoute) { RouteFixture = MoveTemp(InRoute); }
		/** Ambient sockets (story cnb-026): the schedule starts with the round when any exist. */
		void AddSocket(const FVector& Mouth, const FVector& Inside) { Sockets.Add({ Mouth, Inside }); }
		FCrowdAmbientSchedule& GetSchedule() { return Schedule; }
		const TArray<FSocketEvent>& GetSocketEvents() const { return SocketEvents; }
		/** A kill at the scheduled death time Now (any victim): the NPC (or scripted pawn) leaves the world; the schedule
		 *  gets the time only. Returns the vacancy index. */
		int32 KillNpc(int32 Index);
		int32 KillScripted(int32 Index);
		/** The eye-sample witness test of LSC R5.2: some pawn other than Except sees Point (range and view cone). */
		bool IsWitnessed(const FVector& Point, uint64 ExceptKey) const;
		/**
		 * The alley's witness test: the inside of an alley is occluded by its walls, so only a pawn standing in front of the
		 * mouth (within 20 degrees of the alley's axis, 15 m) and looking into it sees it.
		 */
		bool IsAlleyWitnessed(int32 Socket, uint64 ExceptKey) const;
		/** Exits given up (watched), with the time watched from the exit start. */
		TArray<double> GiveUpAfterSec;
		TMap<uint32, double> ExitStartedAt;
		int32 ExitsStarted = 0;
		int32 Emergences = 0;
		int32 Absorptions = 0;
		/** Test hook: entries see the socket blocked this many times before it clears. */
		int32 BlockNextEntries = 0;

		/** Herring fixtures (stories cnb-019 to cnb-022). */
		void AddTraversal(const FCrowdTraversalSite& Site) { Traversals.Add(Site); }
		int32 AddCourt(const FVector& Center);
		void AddBush(const FVector& Center);
		/** A grid of fixtures over the plane (a reference scene with every herring site). */
		void AddReferenceHerringSites();
		TArray<FCourtFixture>& GetCourts() { return Courts; }
		TArray<FBushFixture>& GetBushes() { return Bushes; }
		const TArray<FCommandRecord>& GetCommandLog() const { return CommandLog; }
		void LogCommand(const FCommandRecord& Record) { CommandLog.Add(Record); }
		/** Test hook: the traversal dry run refuses this point id. */
		uint16 RefuseTraversalPoint = 0;
		/** Every traversal dry run asked (ordinal, time, point id, verdict). */
		struct FTraversalDryRun { uint32 Ordinal; double Time; uint16 PointId; bool bAllowed; };
		TArray<FTraversalDryRun> TraversalDryRuns;
		/** Starts the sim traversal of Npc over the site with PointId (Traverse). */
		bool BeginTraversal(FNpc& Npc, uint16 PointId);
		FCrowdTellSampler& GetTells() { return Tells; }

		/** Optional path corners (a fixture route graph): returns the corners of From -> To. */
		void SetPathFixture(TFunction<void(const FVector& From, const FVector& To, TArray<FVector>& OutCorners)> InPath) { PathFixture = MoveTemp(InPath); }

		// ICrowdAgentWorld
		virtual const FCrowdTuning_Server& GetCrowdTuning() const override { return Config.Tuning; }
		virtual const FLocomotionTuning_Public& GetLocomotionTuning() const override { return Config.Locomotion; }
		virtual FCrowdDraws& GetDraws() override { return Core.GetDraws(); }
		virtual const FCrowdSpatialHash& GetHash() const override { return Hash; }
		virtual double GetLongPausePct(double InNow) const override;
		virtual double GetStationaryAge(uint32 Ordinal, double InNow) const override { return Stall.GetAge(Ordinal, InNow); }
		virtual bool PickLegGoal(uint32 Ordinal, const FVector& From, double LegCm, FVector& OutGoal) override;
		virtual bool RequestPath(const FCrowdPathRequest& Request, TArray<FVector>& OutCorners) override;
		virtual bool FindPauseContext(const FVector& At, double Radius, FVector& OutLookPoint, bool& bOutGoods) const override;
		virtual bool HasLineOfSight(uint32 Ordinal, const FVector& Eye, const FVector& Target) override { ++RaysAsked; return true; }
		virtual void ReportActivity(uint32 Ordinal, ECrowdActivity Activity) override { Core.SetActivity(Ordinal, Activity); }
		virtual FCrowdTellSampler& GetTellSampler() override { return Tells; }
		virtual uint32 GetCurrentControlEpoch(uint32 Ordinal) const override { return 1u; }
		virtual bool FindTraversalNear(uint32 Ordinal, const FVector& From, double MaxCm, bool bClimb, FCrowdTraversalSite& Out) override;
		virtual bool CanBeginTraversal(uint32 Ordinal, const FCrowdPerceivedPawn& Self, const FCrowdTraversalSite& Site) override;
		virtual bool IsTraversing(uint32 Ordinal) const override;
		virtual int32 GetCourtCount() const override { return Courts.Num(); }
		virtual bool GetCourtStandPoint(int32 Court, const FVector& From, FCrowdCourtSite& Out) override;
		virtual bool FindBush(const FVector& From, TConstArrayView<FVector> Path, double RadiusCm, FCrowdBushSite& Out) override;
		virtual ECrowdBeginOutcome GetBeginOutcome(uint32 Ordinal, EActionType Type, double SubmittedAt, double InNow) override;
		virtual bool TryAbsorb(uint32 Ordinal, const FCrowdExitSite& Site, double InNow) override;
		virtual void OnAgentPromoted(uint32 Ordinal) override
		{
			if (FCrowdAgentRecord* Record = Core.FindAgent(Ordinal))
			{
				Record->Driver = ECrowdDriverKind::Full;
			}
			Promoted.Add(TPair<uint32, double>(Ordinal, Now));
		}
		virtual bool ClaimExitSlot(uint32 Ordinal) override { ExitClaims.AddUnique(Ordinal); return true; }
		/** Round's Halt in the round-end step (story cnb-031): the core latch, every agent halted, the schedule stopped. */
		void Halt();
		TArray<TPair<uint32, double>> Promoted;
		TArray<uint32> ExitClaims;
		virtual bool IsExitWatched(uint32 Ordinal, const FCrowdExitSite& Site, double InNow) override;
		virtual void OnAmbientExitAbandoned(uint32 Ordinal, const FCrowdExitSite& Site) override;
		// ICrowdScheduleQueue (the round queue stand-in) and ICrowdScheduleHost (the LSC stand-in)
		virtual void Submit(const FCrowdScheduleItem& Item) override { QueueItems.Add(Item); }
		virtual void Void(uint32 ItemId) override { VoidedItems.Add(ItemId); }
		virtual bool StartAmbientExit(int32 Socket, int32 Cycle, double PickU, double InNow) override;
		virtual bool IsSocketClear(int32 Socket, double InNow) override;
		virtual bool RequestEmergence(int32 Socket, double InNow) override;
		virtual bool IsOnNavigation(const FVector& Point) const override
		{
			return FMath::Abs(Point.X) <= Config.HalfSizeCm && FMath::Abs(Point.Y) <= Config.HalfSizeCm;
		}
		// ICrowdAgentStarter
		virtual void StartAgent(uint32 Ordinal, const FCrowdFirstActivity& First, double InNow) override;

		/** Public facts of a body (as the hash sees them). */
		FCrowdPerceivedPawn FactsOf(const FBody& Body) const;
		/** The NPC with Ordinal (null when none). */
		FNpc* FindNpc(uint32 Ordinal);

		int64 RaysAsked = 0;

	private:
		void BuildHash();
		void Integrate(FBody& Body);

		FConfig Config;
		FCrowdDirectorCore Core;
		FCrowdSpatialHash Hash;
		FCrowdStallRegulator Stall;
		FCrowdTellSampler Tells;
		FCrowdDensityModel Density;
		TArray<TUniquePtr<FNpc>> Npcs;
		TArray<FScripted> Scripted;
		TArray<FContext> Contexts;
		TMap<uint32, FCrowdFirstActivity> FirstOverrides;
		TFunction<bool(uint32, const FVector&, FVector&)> RouteFixture;
		TFunction<void(const FVector&, const FVector&, TArray<FVector>&)> PathFixture;
		TArray<FCrowdTraversalSite> Traversals;
		TArray<FCourtFixture> Courts;
		TArray<FBushFixture> Bushes;
		TArray<FCommandRecord> CommandLog;
		TArray<FSocketFixture> Sockets;
		FCrowdAmbientSchedule Schedule;
		TArray<FCrowdScheduleItem> QueueItems;
		TSet<uint32> VoidedItems;
		TArray<FSocketEvent> SocketEvents;
		void DispatchSchedule();
		FCrowdStrikeFanOut StrikeFanOut;
		ICrowdAgentObserver* Observer = nullptr;
		double Now = 0.0;
		int64 Frame = 0;
		int32 MaxDecisionsPerFrame = 0;
		uint32 NextOrdinal = 0;
		uint64 NextKey = 1;
	};
}

#endif // WITH_DEV_AUTOMATION_TESTS
