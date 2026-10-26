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
	};

	class FWorld;

	/** One NPC: its agent and the output adapter that records the Move/Look calls. */
	struct FNpc : public ICrowdAgentOutput
	{
		FBody Body;
		FCrowdAgent Agent;
		double NextDecision = 0.0;
		int64 MoveCalls = 0;
		int64 LookCalls = 0;

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

	/** The simulation (ICrowdAgentWorld for its agents). */
	class FWorld : public ICrowdAgentWorld, public ICrowdAgentStarter
	{
	public:
		explicit FWorld(const FConfig& InConfig);

		/** Adds a scripted pawn at Location (before or after the start); returns its index. */
		int32 AddScripted(const FVector& Location, float YawDeg);
		FScripted& GetScripted(int32 Index) { return Scripted[Index]; }

		/** Spawns the NPCs (at density start placement) and registers them; Dormant until StartRound. */
		void SpawnNpcs();
		/** Spawns one NPC at an explicit location (fixture scenes). */
		FNpc& SpawnNpcAt(const FVector& Location, float YawDeg);
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

		/** Optional fixed routes: when set, PickLegGoal returns the next point of the NPC's own loop (corner tests). */
		void SetRouteFixture(TFunction<bool(uint32 Ordinal, const FVector& From, FVector& OutGoal)> InRoute) { RouteFixture = MoveTemp(InRoute); }
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
		ICrowdAgentObserver* Observer = nullptr;
		double Now = 0.0;
		int64 Frame = 0;
		int32 MaxDecisionsPerFrame = 0;
		uint32 NextOrdinal = 0;
		uint64 NextKey = 1;
	};
}

#endif // WITH_DEV_AUTOMATION_TESTS
