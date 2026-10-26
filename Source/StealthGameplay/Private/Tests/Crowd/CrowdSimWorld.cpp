#include "Tests/Crowd/CrowdSimWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Server/CrowdMotionProfileData.h"
#include "Server/CrowdVirtualInput.h"
#include "Server/StealthTuningInvariants.h"

namespace StealthCrowdSim
{
	namespace Local
	{
		constexpr double EyeHeightCm = 160.0;
		constexpr double NodeOccupancyRadiusCm = 100.0;
		constexpr double LegBandLow = 0.6;
		constexpr double LegBandHigh = 1.4;
		constexpr double RunningSpeedCm = 300.0;
	}

	FWorld::FWorld(const FConfig& InConfig)
		: Config(InConfig)
		, Core(InConfig.Tuning, InConfig.SubSeed)
		, Hash(InConfig.Tuning.CrowdHashCellCm)
	{
		NextOrdinal = Config.FirstNpcOrdinal;
		Tells.SetEnabled(ECrowdTell::HeadHold, true);
		TArray<FCrowdRouteNode> Nodes;
		const int32 Steps = FMath::FloorToInt32(Config.HalfSizeCm / Config.NodeSpacingCm);
		for (int32 X = -Steps; X <= Steps; ++X)
		{
			for (int32 Y = -Steps; Y <= Steps; ++Y)
			{
				FCrowdRouteNode& Node = Nodes.AddDefaulted_GetRef();
				Node.Location = FVector(X * Config.NodeSpacingCm, Y * Config.NodeSpacingCm, 0.0);
				Node.Area = Node.Location.Size2D() <= Config.MarketRadiusCm ? ECrowdAreaType::Market : ECrowdAreaType::Causeway;
				Node.CatchmentM2 = (Config.NodeSpacingCm / 100.0) * (Config.NodeSpacingCm / 100.0);
				Node.FrontageCm = 200.0;
			}
		}
		Density.Initialize(Nodes, Config.NpcCount, Config.Tuning);
	}

	void FWorld::SetObserver(ICrowdAgentObserver* InObserver)
	{
		Observer = InObserver;
		for (TUniquePtr<FNpc>& Npc : Npcs)
		{
			Npc->Agent.SetObserver(InObserver);
		}
	}

	int32 FWorld::AddScripted(const FVector& Location, float YawDeg)
	{
		FScripted& Pawn = Scripted.AddDefaulted_GetRef();
		Pawn.Body.Key = NextKey++;
		Pawn.Body.Ordinal = NextOrdinal++;
		Pawn.Body.Location = Location;
		Pawn.Body.Motion.FacingYawDeg = YawDeg;
		Pawn.Body.Control = FRotator(0.0, YawDeg, 0.0);
		return Scripted.Num() - 1;
	}

	FNpc& FWorld::SpawnNpcAt(const FVector& Location, float YawDeg)
	{
		TUniquePtr<FNpc>& Npc = Npcs.Add_GetRef(MakeUnique<FNpc>());
		Npc->Body.Key = NextKey++;
		Npc->Body.Ordinal = NextOrdinal++;
		Npc->Body.Location = Location;
		Npc->Body.Motion.FacingYawDeg = YawDeg;
		Npc->Body.Control = FRotator(0.0, YawDeg, 0.0);
		const FCrowdAgentRecord& Record = Core.RegisterAgent(Npc->Body.Ordinal, ECrowdDriverKind::Full);
		Npc->Agent.Initialize(Npc->Body.Ordinal, ECrowdDriverKind::Full, FactsOf(Npc->Body), Record.GridPhase, UCrowdMotionProfileData::MakeProvisionalProfiles(), *this);
		Npc->Agent.SetObserver(Observer);
		if (Core.IsActive())
		{
			Core.JoinActiveRound(Npc->Body.Ordinal, Now, *this);
		}
		return *Npc;
	}

	void FWorld::SpawnNpcs()
	{
		const TArray<int32> Starts = Density.PlaceStarts(Config.NpcCount, Config.SubSeed ^ 0x5A5A5A5Aull, TArray<int32>());
		for (int32 Index = 0; Index < Starts.Num(); ++Index)
		{
			const FVector Spread = Index % 2 == 0 ? FVector::ZeroVector : FVector(60.0, 0.0, 0.0);
			SpawnNpcAt(Density.GetNodes()[Starts[Index]].Location + Spread, static_cast<float>((Index * 137) % 360));
		}
	}

	void FWorld::StartRound()
	{
		BuildHash();
		Core.StartRound(Now, true, *this);
	}

	void FWorld::StartAgent(uint32 Ordinal, const FCrowdFirstActivity& First, double InNow)
	{
		FNpc* Npc = FindNpc(Ordinal);
		if (Npc == nullptr)
		{
			return;
		}
		const FCrowdFirstActivity* Override = FirstOverrides.Find(Ordinal);
		Npc->Agent.Start(Override != nullptr ? *Override : First, InNow, FactsOf(Npc->Body), *this, *Npc);
		Npc->NextDecision = Core.NextGridPoint(Ordinal, InNow);
	}

	FNpc* FWorld::FindNpc(uint32 Ordinal)
	{
		for (TUniquePtr<FNpc>& Npc : Npcs)
		{
			if (Npc->Body.Ordinal == Ordinal)
			{
				return Npc.Get();
			}
		}
		return nullptr;
	}

	FCrowdPerceivedPawn FWorld::FactsOf(const FBody& Body) const
	{
		FCrowdPerceivedPawn Facts;
		Facts.Key = Body.Key;
		Facts.Ordinal = Body.Ordinal;
		Facts.Location = Body.Location;
		Facts.VelocityCmPerSec = Body.Velocity();
		Facts.FacingYawDeg = Body.Motion.FacingYawDeg;
		Facts.EyeHeightCm = Local::EyeHeightCm;
		Facts.bPerceivable = Body.bPerceivable;
		Facts.bObstacle = Body.bObstacle;
		Facts.bRunning = Body.Motion.Speed > Local::RunningSpeedCm;
		return Facts;
	}

	void FWorld::BuildHash()
	{
		TArray<FCrowdPerceivedPawn> Pawns;
		Pawns.Reserve(Npcs.Num() + Scripted.Num());
		for (const TUniquePtr<FNpc>& Npc : Npcs)
		{
			Pawns.Add(FactsOf(Npc->Body));
		}
		for (const FScripted& Pawn : Scripted)
		{
			Pawns.Add(FactsOf(Pawn.Body));
		}
		Hash.Build(Pawns);
	}

	void FWorld::Integrate(FBody& Body)
	{
		FStealthMoveInput Input;
		Input.Magnitude = static_cast<float>(FMath::Min(Body.MoveIntent.Size(), 1.0));
		Input.InputYawDeg = static_cast<float>(FCrowdVirtualInput::WorldYawOfInput(Body.MoveIntent, Body.Control.Yaw));
		Input.Gait = Body.Gait;
		Input.bHasLookTarget = true;
		Input.LookTargetYawDeg = static_cast<float>(Body.Control.Yaw);
		FStealthLocomotionModel::Step(Config.Locomotion, Input, static_cast<float>(Config.StepSec), Body.Motion);
		Body.Location += Body.Velocity() * Config.StepSec;
		Body.Location.X = FMath::Clamp(Body.Location.X, -Config.HalfSizeCm, Config.HalfSizeCm);
		Body.Location.Y = FMath::Clamp(Body.Location.Y, -Config.HalfSizeCm, Config.HalfSizeCm);
	}

	void FWorld::Step()
	{
		++Frame;
		BuildHash();
		for (const TUniquePtr<FNpc>& Npc : Npcs)
		{
			Stall.Update(Npc->Body.Ordinal, Npc->Body.Motion.Speed, !Npc->Body.bPerceivable, Now);
		}
		// Decisions: the grid (the StateTree cadence) and wakes from due entries, before the controller tick.
		int32 Decisions = 0;
		for (TUniquePtr<FNpc>& Npc : Npcs)
		{
			if (!Npc->Agent.IsStarted())
			{
				continue;
			}
			const FCrowdPerceivedPawn Facts = FactsOf(Npc->Body);
			if (Now >= Npc->NextDecision - 1e-9)
			{
				Npc->Agent.Decide(Now, Facts, *this, *Npc);
				Npc->Agent.ClearWake();
				Npc->NextDecision = Core.NextGridPoint(Npc->Body.Ordinal, Now);
				++Decisions;
			}
			else if (Npc->Agent.WantsWake())
			{
				Npc->Agent.Decide(Now, Facts, *this, *Npc);
				Npc->Agent.ClearWake();
				++Decisions;
			}
		}
		MaxDecisionsPerFrame = FMath::Max(MaxDecisionsPerFrame, Decisions);
		for (TUniquePtr<FNpc>& Npc : Npcs)
		{
			Npc->Agent.Tick(Now, static_cast<float>(Config.StepSec), FactsOf(Npc->Body), *this, *Npc);
		}
		for (TUniquePtr<FNpc>& Npc : Npcs)
		{
			Integrate(Npc->Body);
		}
		for (FScripted& Pawn : Scripted)
		{
			Integrate(Pawn.Body);
		}
		Now += Config.StepSec;
	}

	void FWorld::RunUntil(double Until)
	{
		while (Now < Until)
		{
			Step();
		}
	}

	int32 FWorld::EmitSalient(const FBody& Source)
	{
		int32 Receivers = 0;
		for (TUniquePtr<FNpc>& Npc : Npcs)
		{
			if (Npc->Agent.IsStarted() && Npc->Body.Key != Source.Key && FVector::Dist2D(Npc->Body.Location, Source.Location) <= Config.Tuning.CrowdSalientRangeCm)
			{
				Npc->Agent.OnSalientStimulus(Now, Source.Key, Source.Location + FVector(0.0, 0.0, Local::EyeHeightCm), Now, *this);
				++Receivers;
			}
		}
		return Receivers;
	}

	double FWorld::GetLongPausePct(double InNow) const
	{
		const double Threshold = StealthTuningPendingOwnerKeys::SessionSoftTimeoutSec + 1.0;
		int32 Projected = 0;
		for (const TUniquePtr<FNpc>& Npc : Npcs)
		{
			Projected += Npc->Agent.GetRestEnd() > InNow + Threshold ? 1 : 0;
		}
		return Stall.EffectiveLongPausePct(InNow, Npcs.Num(), Config.Tuning, Threshold, Projected);
	}

	bool FWorld::PickLegGoal(uint32 Ordinal, const FVector& From, double LegCm, FVector& OutGoal)
	{
		if (RouteFixture)
		{
			return RouteFixture(Ordinal, From, OutGoal);
		}
		const TConstArrayView<FCrowdRouteNode> Nodes = Density.GetNodes();
		TArray<int32> Occupancy;
		Occupancy.SetNumZeroed(Nodes.Num());
		for (int32 Index = 0; Index < Nodes.Num(); ++Index)
		{
			const double Distance = FVector::Dist2D(Nodes[Index].Location, From);
			if (Distance < Local::LegBandLow * LegCm || Distance > Local::LegBandHigh * LegCm)
			{
				Occupancy[Index] = MAX_int32;
				continue;
			}
			TArray<const FCrowdPerceivedPawn*> Near;
			Hash.Query(Nodes[Index].Location, Local::NodeOccupancyRadiusCm, ECrowdQuery::Steering, 0, Near);
			Occupancy[Index] = Near.Num();
		}
		const int32 Pick = Density.PickDestination(Ordinal, Density.NearestNode(From), Occupancy, GetDraws());
		if (Pick == INDEX_NONE)
		{
			return false;
		}
		OutGoal = Nodes[Pick].Location;
		return true;
	}

	bool FWorld::RequestPath(const FCrowdPathRequest& Request, TArray<FVector>& OutCorners)
	{
		OutCorners.Reset();
		if (PathFixture)
		{
			PathFixture(Request.From, Request.To, OutCorners);
		}
		if (OutCorners.Num() == 0)
		{
			OutCorners.Add(Request.To);
		}
		return true;
	}

	bool FWorld::FindPauseContext(const FVector& At, double Radius, FVector& OutLookPoint, bool& bOutGoods) const
	{
		double Best = Radius * Radius;
		bool bFound = false;
		for (const FContext& Context : Contexts)
		{
			const double D = FVector::DistSquared2D(Context.Point, At);
			if (D <= Best)
			{
				Best = D;
				OutLookPoint = Context.Point;
				bOutGoods = Context.bGoods;
				bFound = true;
			}
		}
		return bFound;
	}
}

#endif // WITH_DEV_AUTOMATION_TESTS
