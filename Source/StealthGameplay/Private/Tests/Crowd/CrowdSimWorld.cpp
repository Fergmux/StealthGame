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
		for (int32 Index = 0; Index < static_cast<int32>(ECrowdTell::Count); ++Index)
		{
			Tells.SetEnabled(static_cast<ECrowdTell>(Index), (Config.EnabledTellMask & (1u << Index)) != 0);
		}
		Tells.SetShowAnchored(true); // as the Director's (F2 as amended 2026-10-27)
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

	FNpc& FWorld::SpawnNpcAt(const FVector& Location, float YawDeg, ECrowdDriverKind Driver, float InitialSpeed, EGait InitialGait)
	{
		TUniquePtr<FNpc>& Npc = Npcs.Add_GetRef(MakeUnique<FNpc>());
		Npc->World = this;
		Npc->Body.Key = NextKey++;
		Npc->Body.Ordinal = NextOrdinal++;
		Npc->Body.Location = Location;
		Npc->Body.Motion.FacingYawDeg = YawDeg;
		Npc->Body.Control = FRotator(0.0, YawDeg, 0.0);
		// A pawn taken over mid-motion (story cnb-028) arrives moving.
		Npc->Body.Motion.Speed = InitialSpeed;
		Npc->Body.Gait = InitialGait;
		Npc->Body.Motion.LastGait = InitialGait;
		Npc->Body.MoveIntent = InitialSpeed > 0.0f ? FVector2D(0.0, 1.0) : FVector2D::ZeroVector;
		const FCrowdAgentRecord& Record = Core.RegisterAgent(Npc->Body.Ordinal, Driver);
		Npc->Agent.Initialize(Npc->Body.Ordinal, Driver, FactsOf(Npc->Body), Record.GridPhase, UCrowdMotionProfileData::MakeProvisionalProfiles(), *this);
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
		if (Sockets.Num() > 0)
		{
			Schedule.Start(Now, Sockets.Num(), Config.Tuning, 1u, *this, nullptr);
		}
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
		Facts.bTraversalLanding = Body.LandingFrame == Frame;
		return Facts;
	}

	void FWorld::BuildHash()
	{
		TArray<FCrowdPerceivedPawn> Pawns;
		Pawns.Reserve(Npcs.Num() + Scripted.Num());
		for (const TUniquePtr<FNpc>& Npc : Npcs)
		{
			if (!Npc->Body.bGone)
			{
				Pawns.Add(FactsOf(Npc->Body));
			}
		}
		for (const FScripted& Pawn : Scripted)
		{
			if (!Pawn.Body.bGone)
			{
				Pawns.Add(FactsOf(Pawn.Body));
			}
		}
		Hash.Build(Pawns);
	}

	void FWorld::Integrate(FBody& Body)
	{
		if (Body.bTraversing)
		{
			// The sim traversal: a straight crossing at constant speed; the landing is flagged for the next frame's facts.
			const double Alpha = FMath::Clamp((Now + Config.StepSec - Body.TraverseStart) / FMath::Max(Body.TraverseDuration, 0.01), 0.0, 1.0);
			const FVector Next = FMath::Lerp(Body.TraverseFrom, Body.TraverseTo, Alpha);
			Body.Motion.Speed = static_cast<float>(FVector::Dist2D(Next, Body.Location) / Config.StepSec);
			Body.Motion.FacingYawDeg = static_cast<float>((Body.TraverseTo - Body.TraverseFrom).Rotation().Yaw);
			Body.Location = Next;
			if (Alpha >= 1.0)
			{
				Body.bTraversing = false;
				Body.LandingFrame = Frame + 1;
			}
			return;
		}
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
		if (Schedule.IsRunning())
		{
			Schedule.Tick(Now);
			DispatchSchedule();
		}
		for (const TUniquePtr<FNpc>& Npc : Npcs)
		{
			if (!Npc->Body.bGone)
			{
				Stall.Update(Npc->Body.Ordinal, Npc->Body.Motion.Speed, !Npc->Body.bPerceivable, Now);
			}
		}
		// Decisions: the grid (the StateTree cadence) and wakes from due entries, before the controller tick.
		int32 Decisions = 0;
		for (TUniquePtr<FNpc>& Npc : Npcs)
		{
			if (!Npc->Agent.IsStarted() || Npc->Body.bGone)
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
			if (!Npc->Body.bGone)
			{
				Npc->Agent.Tick(Now, static_cast<float>(Config.StepSec), FactsOf(Npc->Body), *this, *Npc);
			}
		}
		for (TUniquePtr<FNpc>& Npc : Npcs)
		{
			if (!Npc->Body.bGone)
			{
				Integrate(Npc->Body);
			}
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

	// ---- Herring fixtures -----------------------------------------------------------------------------------------------

	namespace HerringLocal
	{
		constexpr double StandOffsetCm = 100.0;
		constexpr double StandOccupiedCm = 40.0;
		constexpr double VaultSec = 0.8;
		constexpr double ClimbSec = 1.4;
		constexpr double TraverseReachCm = 150.0;
		constexpr double TraverseFacingDeg = 45.0;

		double DistToPolyline(const FVector& P, const FVector& From, TConstArrayView<FVector> Path)
		{
			double Best = FVector::Dist2D(P, From);
			FVector A = From;
			for (const FVector& B : Path)
			{
				const FVector2D Ab(B.X - A.X, B.Y - A.Y);
				const double LenSq = Ab.SizeSquared();
				const double T = LenSq > UE_SMALL_NUMBER ? FMath::Clamp(FVector2D::DotProduct(FVector2D(P.X - A.X, P.Y - A.Y), Ab) / LenSq, 0.0, 1.0) : 0.0;
				Best = FMath::Min(Best, FVector::Dist2D(P, A + (B - A) * T));
				A = B;
			}
			return Best;
		}
	}

	int32 FWorld::AddCourt(const FVector& Center)
	{
		FCourtFixture& Court = Courts.AddDefaulted_GetRef();
		Court.Center = Center;
		Court.FixtureId = static_cast<uint16>(Courts.Num());
		return Courts.Num() - 1;
	}

	void FWorld::AddBush(const FVector& Center)
	{
		FBushFixture& Bush = Bushes.AddDefaulted_GetRef();
		Bush.Center = Center;
		Bush.BushId = static_cast<uint16>(Bushes.Num());
	}

	void FWorld::AddReferenceHerringSites()
	{
		// 14 courts (the reference court count), a bush every ~15 m and public coping of four heights every ~12 m.
		const double Half = Config.HalfSizeCm * 0.8;
		for (int32 Index = 0; Index < 14; ++Index)
		{
			const double Angle = 2.0 * UE_DOUBLE_PI * Index / 14.0;
			const double Radius = Half * (Index % 2 == 0 ? 0.45 : 0.85);
			AddCourt(FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0.0));
		}
		for (double X = -Half; X <= Half; X += 1500.0)
		{
			for (double Y = -Half; Y <= Half; Y += 1500.0)
			{
				AddBush(FVector(X + 250.0, Y + 250.0, 0.0));
			}
		}
		const double Heights[] = { 80.0, 120.0, 160.0, 200.0 };
		int32 Point = 1;
		for (double X = -Half; X <= Half; X += 1200.0)
		{
			for (double Y = -Half; Y <= Half; Y += 1200.0)
			{
				FCrowdTraversalSite Site;
				Site.PointId = static_cast<uint16>(Point);
				const bool bAlongX = (Point % 2) == 0;
				Site.Near = FVector(X, Y, 0.0);
				Site.Far = Site.Near + (bAlongX ? FVector(120.0, 0.0, 0.0) : FVector(0.0, 120.0, 0.0));
				Site.CopingHeightCm = Heights[Point % 4];
				AddTraversal(Site);
				++Point;
			}
		}
	}

	bool FWorld::FindTraversalNear(uint32 Ordinal, const FVector& From, double MaxCm, bool bClimb, FCrowdTraversalSite& Out)
	{
		double Best = MaxCm;
		bool bFound = false;
		for (const FCrowdTraversalSite& Site : Traversals)
		{
			const bool bClimbClass = Site.CopingHeightCm >= CrowdHerringConstants::ClimbMinCopingCm;
			if (bClimbClass != bClimb)
			{
				continue;
			}
			const double D = FVector::Dist2D(Site.Near, From);
			if (D <= Best)
			{
				Best = D;
				Out = Site;
				bFound = true;
			}
		}
		return bFound;
	}

	bool FWorld::CanBeginTraversal(uint32 Ordinal, const FCrowdPerceivedPawn& Self, const FCrowdTraversalSite& Site)
	{
		const double Facing = FMath::Abs(FMath::FindDeltaAngleDegrees(Self.FacingYawDeg, (Site.Far - Site.Near).Rotation().Yaw));
		const bool bAllowed = Site.PointId != RefuseTraversalPoint && FVector::Dist2D(Self.Location, Site.Near) <= HerringLocal::TraverseReachCm
			&& Facing <= HerringLocal::TraverseFacingDeg;
		TraversalDryRuns.Add({ Ordinal, Now, Site.PointId, bAllowed });
		return bAllowed;
	}

	bool FWorld::IsTraversing(uint32 Ordinal) const
	{
		for (const TUniquePtr<FNpc>& Npc : Npcs)
		{
			if (Npc->Body.Ordinal == Ordinal)
			{
				return Npc->Body.bTraversing;
			}
		}
		return false;
	}

	bool FWorld::BeginTraversal(FNpc& Npc, uint16 PointId)
	{
		for (const FCrowdTraversalSite& Site : Traversals)
		{
			if (Site.PointId == PointId)
			{
				Npc.Body.bTraversing = true;
				Npc.Body.TraverseFrom = Npc.Body.Location;
				Npc.Body.TraverseTo = Site.Far;
				Npc.Body.TraverseStart = Now;
				Npc.Body.TraverseDuration = Site.CopingHeightCm >= CrowdHerringConstants::ClimbMinCopingCm ? HerringLocal::ClimbSec : HerringLocal::VaultSec;
				return true;
			}
		}
		return false;
	}

	bool FWorld::GetCourtStandPoint(int32 Court, const FVector& From, FCrowdCourtSite& Out)
	{
		if (!Courts.IsValidIndex(Court))
		{
			return false;
		}
		const FCourtFixture& Fixture = Courts[Court];
		double Best = TNumericLimits<double>::Max();
		bool bFound = false;
		const FVector Offsets[] = { FVector(1, 0, 0), FVector(0, 1, 0), FVector(-1, 0, 0), FVector(0, -1, 0) };
		for (const FVector& Offset : Offsets)
		{
			const FVector Stand = Fixture.Center + Offset * HerringLocal::StandOffsetCm;
			// Physical occupancy only: any pawn standing on the point (any kind).
			TArray<const FCrowdPerceivedPawn*> Near;
			Hash.Query(Stand, HerringLocal::StandOccupiedCm, ECrowdQuery::Steering, 0, Near);
			if (Near.Num() > 0)
			{
				continue;
			}
			const double D = FVector::Dist2D(Stand, From);
			if (D < Best)
			{
				Best = D;
				Out.Court = Court;
				Out.Fixture = FCourtFixtureId(Fixture.FixtureId);
				Out.StandPoint = Stand;
				Out.LookPoint = Fixture.Center + FVector(0.0, 0.0, 100.0);
				bFound = true;
			}
		}
		return bFound;
	}

	bool FWorld::FindBush(const FVector& From, TConstArrayView<FVector> Path, double RadiusCm, FCrowdBushSite& Out)
	{
		double Best = TNumericLimits<double>::Max();
		bool bFound = false;
		for (const FBushFixture& Bush : Bushes)
		{
			// Geometry only: never the bush's occupancy.
			if (HerringLocal::DistToPolyline(Bush.Center, From, Path) > RadiusCm)
			{
				continue;
			}
			const double D = FVector::Dist2D(Bush.Center, From);
			if (D < Best)
			{
				Best = D;
				Out.BushId = Bush.BushId;
				Out.Center = Bush.Center;
				Out.Entry = Bush.Center + (From - Bush.Center).GetSafeNormal2D() * (Bush.RadiusCm + 20.0);
				Out.SidePoints.Reset();
				Out.SidePoints.Add(Bush.Center + FVector(Bush.RadiusCm + 60.0, 0.0, 0.0));
				Out.SidePoints.Add(Bush.Center + FVector(0.0, Bush.RadiusCm + 60.0, 0.0));
				Out.SidePoints.Add(Bush.Center + FVector(-(Bush.RadiusCm + 60.0), 0.0, 0.0));
				Out.SidePoints.Add(Bush.Center + FVector(0.0, -(Bush.RadiusCm + 60.0), 0.0));
				bFound = true;
			}
		}
		return bFound;
	}

	ECrowdBeginOutcome FWorld::GetBeginOutcome(uint32 Ordinal, EActionType Type, double SubmittedAt, double InNow)
	{
		const FNpc* Npc = FindNpc(Ordinal);
		if (Npc == nullptr || Npc->LastBeginTime < SubmittedAt - 1e-9)
		{
			return ECrowdBeginOutcome::Pending;
		}
		return Npc->bLastBeginAccepted ? ECrowdBeginOutcome::Accepted : ECrowdBeginOutcome::Refused;
	}

	// ---- The NPC's discrete commands (the sim's IIF, Objectives and Concealment stand-ins) ------------------------------

	bool FNpc::DryRun(EActionType Type, const FCourtFixtureId* Target)
	{
		bool bAllow = true;
		if (Type == EActionType::Interact && Target != nullptr)
		{
			for (const FCourtFixture& Court : World->GetCourts())
			{
				bAllow &= !(Court.FixtureId == Target->Value && Court.Interactors >= 4);
			}
		}
		else if (Type == EActionType::Prepare)
		{
			bAllow = World->GetNow() - LastPrepareBegin >= 60.0;
		}
		World->LogCommand({ FCommandRecord::EKind::DryRun, Body.Ordinal, World->GetNow(), static_cast<int32>(Type), bAllow });
		return bAllow;
	}

	bool FNpc::Traverse(const FTraversalHint& Hint)
	{
		const bool bStarted = World->BeginTraversal(*this, Hint.TraversalPointHint);
		World->LogCommand({ FCommandRecord::EKind::Traverse, Body.Ordinal, World->GetNow(), Hint.TraversalPointHint, bStarted });
		return true;
	}

	bool FNpc::InteractBegin(FCourtFixtureId Target)
	{
		LastBeginTime = World->GetNow();
		bLastBeginAccepted = false;
		TArray<FCourtFixture>& Courts = World->GetCourts();
		for (int32 Index = 0; Index < Courts.Num(); ++Index)
		{
			FCourtFixture& Court = Courts[Index];
			if (Court.FixtureId != Target.Value)
			{
				continue;
			}
			if (Court.RefuseNextBegins > 0)
			{
				--Court.RefuseNextBegins; // Reject(NotNow): a fifth Begin at a fixture that filled after the dry run
			}
			else if (Court.Interactors < 4)
			{
				++Court.Interactors;
				InteractCourt = Index;
				bLastBeginAccepted = true;
			}
		}
		World->LogCommand({ FCommandRecord::EKind::InteractBegin, Body.Ordinal, World->GetNow(), Target.Value, bLastBeginAccepted });
		return true;
	}

	bool FNpc::InteractEnd()
	{
		if (World->GetCourts().IsValidIndex(InteractCourt))
		{
			--World->GetCourts()[InteractCourt].Interactors;
		}
		InteractCourt = INDEX_NONE;
		World->LogCommand({ FCommandRecord::EKind::InteractEnd, Body.Ordinal, World->GetNow(), 0, true });
		return true;
	}

	bool FNpc::PrepareBegin()
	{
		LastPrepareBegin = World->GetNow();
		World->LogCommand({ FCommandRecord::EKind::PrepareBegin, Body.Ordinal, World->GetNow(), 0, true });
		return true;
	}

	bool FNpc::PrepareEnd()
	{
		World->LogCommand({ FCommandRecord::EKind::PrepareEnd, Body.Ordinal, World->GetNow(), 0, true });
		return true;
	}

	bool FNpc::Hide(const FHideCommand& Command)
	{
		TArray<FBushFixture>& Bushes = World->GetBushes();
		if (Command.Direction == EHideDirection::Enter)
		{
			for (int32 Index = 0; Index < Bushes.Num(); ++Index)
			{
				if (Bushes[Index].BushId == Command.BushId)
				{
					// Concealed (the sim's Concealment): out of NPC perception and steering, inside the bush.
					HiddenBush = Index;
					Body.bPerceivable = false;
					Body.bObstacle = false;
					Body.Location = Bushes[Index].Center;
					Body.Motion.Speed = 0.0f;
				}
			}
			World->LogCommand({ FCommandRecord::EKind::HideEnter, Body.Ordinal, World->GetNow(), Command.BushId, HiddenBush != INDEX_NONE });
			return true;
		}
		if (Bushes.IsValidIndex(HiddenBush))
		{
			const FBushFixture& Bush = Bushes[HiddenBush];
			const FVector Dirs[] = { FVector(1, 0, 0), FVector(0, 1, 0), FVector(-1, 0, 0), FVector(0, -1, 0) };
			Body.Location = Bush.Center + Dirs[Command.SideIndex % 4] * (Bush.RadiusCm + 60.0);
		}
		Body.bPerceivable = true;
		Body.bObstacle = true;
		World->LogCommand({ FCommandRecord::EKind::HideExit, Body.Ordinal, World->GetNow(), Command.SideIndex, true });
		HiddenBush = INDEX_NONE;
		return true;
	}

	// ---- Ambient schedule host (story cnb-026) ---------------------------------------------------------------------------

	namespace AmbientLocal
	{
		/** The eye-sample witness test: within this range and view half-angle a pawn sees a point (LOS open in the sim). */
		constexpr double WitnessRangeCm = 2500.0;
		constexpr double WitnessHalfAngleDeg = 60.0;
		/** The alley stand-in: its inside is seen only from within this range of the mouth and this angle of its axis. */
		constexpr double AlleySightCm = 1500.0;
		constexpr double AlleyAxisDeg = 20.0;
		/** Eligible exiters are within this distance of the socket path (40 m). */
		constexpr double ExitEligibleCm = 4000.0;
	}

	bool FWorld::IsWitnessed(const FVector& Point, uint64 ExceptKey) const
	{
		auto Sees = [&Point, ExceptKey](const FBody& Body)
		{
			if (Body.bGone || Body.Key == ExceptKey || !Body.bPerceivable)
			{
				return false;
			}
			const FVector To = Point - Body.Location;
			if (To.Size2D() > AmbientLocal::WitnessRangeCm)
			{
				return false;
			}
			return FMath::Abs(FMath::FindDeltaAngleDegrees(Body.Control.Yaw, To.Rotation().Yaw)) <= AmbientLocal::WitnessHalfAngleDeg;
		};
		for (const TUniquePtr<FNpc>& Npc : Npcs)
		{
			if (Sees(Npc->Body))
			{
				return true;
			}
		}
		for (const FScripted& Pawn : Scripted)
		{
			if (Sees(Pawn.Body))
			{
				return true;
			}
		}
		return false;
	}

	bool FWorld::IsAlleyWitnessed(int32 Socket, uint64 ExceptKey) const
	{
		const FSocketFixture& Fixture = Sockets[Socket];
		const FVector Axis = (Fixture.Mouth - Fixture.Inside).GetSafeNormal2D();
		auto Sees = [&Fixture, &Axis, ExceptKey](const FBody& Body)
		{
			if (Body.bGone || Body.Key == ExceptKey || !Body.bPerceivable)
			{
				return false;
			}
			const FVector FromMouth = Body.Location - Fixture.Mouth;
			const double Along = FVector::DotProduct(FromMouth.GetSafeNormal2D(), Axis);
			if (FromMouth.Size2D() > AmbientLocal::AlleySightCm || Along < FMath::Cos(FMath::DegreesToRadians(AmbientLocal::AlleyAxisDeg)))
			{
				return false; // the alley walls hide its inside from anywhere off its axis
			}
			const FVector To = Fixture.Inside - Body.Location;
			return FMath::Abs(FMath::FindDeltaAngleDegrees(Body.Control.Yaw, To.Rotation().Yaw)) <= AmbientLocal::WitnessHalfAngleDeg;
		};
		for (const TUniquePtr<FNpc>& Npc : Npcs)
		{
			if (Sees(Npc->Body))
			{
				return true;
			}
		}
		for (const FScripted& Pawn : Scripted)
		{
			if (Sees(Pawn.Body))
			{
				return true;
			}
		}
		return false;
	}

	void FWorld::DispatchSchedule()
	{
		// The round queue stand-in: TimerFired(CrowdSchedule) items in due order, resolved in their slot.
		QueueItems.StableSort([](const FCrowdScheduleItem& A, const FCrowdScheduleItem& B) { return A.Due < B.Due; });
		while (QueueItems.Num() > 0 && QueueItems[0].Due <= Now + 1e-9)
		{
			const FCrowdScheduleItem Item = QueueItems[0];
			QueueItems.RemoveAt(0);
			const ECrowdScheduleResult Result = VoidedItems.Contains(Item.Id) ? ECrowdScheduleResult::VoidedRevalidation
				: (!Core.IsActive() ? ECrowdScheduleResult::RejectedLatched : (Item.Epoch != 1u ? ECrowdScheduleResult::RejectedStaleEpoch : ECrowdScheduleResult::Fired));
			Schedule.Resolve(Item.Id, Result, Now, *this);
		}
	}

	bool FWorld::StartAmbientExit(int32 Socket, int32 Cycle, double PickU, double InNow)
	{
		const FSocketFixture& Fixture = Sockets[Socket];
		TArray<FNpc*> Eligible;
		for (TUniquePtr<FNpc>& Npc : Npcs)
		{
			if (!Npc->Body.bGone && Npc->Agent.IsStarted() && Npc->Agent.CanTakeExitSlot() && FVector::Dist2D(Npc->Body.Location, Fixture.Mouth) <= AmbientLocal::ExitEligibleCm)
			{
				Eligible.Add(Npc.Get());
			}
		}
		FNpc* Chosen = Eligible.Num() > 0 ? Eligible[FMath::Min(static_cast<int32>(PickU * Eligible.Num()), Eligible.Num() - 1)] : nullptr;
		// A released exile body takes the next slot (the slot's chosen NPC simply stays; story cnb-029).
		for (int32 Claim = 0; Claim < ExitClaims.Num(); ++Claim)
		{
			FNpc* Claimant = FindNpc(ExitClaims[Claim]);
			if (Claimant != nullptr && !Claimant->Body.bGone && Claimant->Agent.CanTakeExitSlot())
			{
				Chosen = Claimant;
				ExitClaims.RemoveAt(Claim);
				break;
			}
		}
		if (Chosen == nullptr)
		{
			return false;
		}
		FCrowdExitSite Site;
		Site.Socket = Socket;
		Site.Cycle = Cycle;
		Site.Mouth = Fixture.Mouth;
		Site.Inside = Fixture.Inside;
		Chosen->Agent.StartAmbientExit(InNow, FactsOf(Chosen->Body), *this, Site);
		ExitStartedAt.Add(Chosen->Body.Ordinal, InNow);
		++ExitsStarted;
		return true;
	}

	bool FWorld::IsSocketClear(int32 Socket, double InNow)
	{
		if (BlockNextEntries > 0)
		{
			--BlockNextEntries;
			return false;
		}
		return !IsAlleyWitnessed(Socket, 0);
	}

	bool FWorld::RequestEmergence(int32 Socket, double InNow)
	{
		FNpc& Npc = SpawnNpcAt(Sockets[Socket].Inside, static_cast<float>((Sockets[Socket].Mouth - Sockets[Socket].Inside).Rotation().Yaw));
		++Emergences;
		SocketEvents.Add({ InNow, Socket, Npc.Body.Ordinal, true, IsAlleyWitnessed(Socket, Npc.Body.Key) });
		return true;
	}

	bool FWorld::TryAbsorb(uint32 Ordinal, const FCrowdExitSite& Site, double InNow)
	{
		FNpc* Npc = FindNpc(Ordinal);
		if (Npc == nullptr || IsAlleyWitnessed(Site.Socket, Npc->Body.Key))
		{
			return false;
		}
		Npc->Body.bGone = true;
		++Absorptions;
		SocketEvents.Add({ InNow, Site.Socket, Ordinal, false, false });
		Schedule.NoteAbsorbed(Site.Cycle, InNow);
		return true;
	}

	bool FWorld::IsExitWatched(uint32 Ordinal, const FCrowdExitSite& Site, double InNow)
	{
		const FNpc* Npc = FindNpc(Ordinal);
		return Npc != nullptr && IsAlleyWitnessed(Site.Socket, Npc->Body.Key);
	}

	void FWorld::OnAmbientExitAbandoned(uint32 Ordinal, const FCrowdExitSite& Site)
	{
		if (const double* Started = ExitStartedAt.Find(Ordinal))
		{
			GiveUpAfterSec.Add(Now - *Started);
		}
		Schedule.NoteCycleAbandoned(Site.Cycle);
	}

	void FWorld::Halt()
	{
		Core.Halt();
		for (TUniquePtr<FNpc>& Npc : Npcs)
		{
			Npc->Agent.Halt(*Npc);
		}
		Schedule.Halt();
	}

	int32 FWorld::KillNpc(int32 Index)
	{
		FNpc& Npc = *Npcs[Index];
		const bool bInAlley = Npc.Agent.IsExiting() && Npc.Agent.GetHerringMode() == FCrowdAgent::EHerringMode::ExitWait;
		if (Npc.Agent.IsExiting() && !bInAlley)
		{
			Schedule.NoteCycleAbandoned(Npc.Agent.GetExitSite().Cycle); // killed on the way: its cycle closes
		}
		Npc.Body.bGone = true;
		return Schedule.NoteKill(Now, bInAlley, bInAlley ? Npc.Agent.GetExitSite().Cycle : INDEX_NONE);
	}

	int32 FWorld::KillScripted(int32 Index)
	{
		Scripted[Index].Body.bGone = true;
		return Schedule.NoteKill(Now, false);
	}

	int32 FWorld::EmitStrike(const FVector& Point, uint64 SourceKey)
	{
		TArray<FCrowdStrikeCandidate> Candidates;
		for (const TUniquePtr<FNpc>& Npc : Npcs)
		{
			if (!Npc->Agent.IsStarted() || Npc->Body.Key == SourceKey || Npc->Agent.IsStrikeRefractory(Now, Config.Tuning))
			{
				continue;
			}
			const double Distance = FVector::Dist2D(Npc->Body.Location, Point);
			if (Distance <= Config.Tuning.CrowdStrikeReactRadiusCm)
			{
				Candidates.Add({ Npc->Body.Ordinal, Distance, 0.0 });
			}
		}
		StrikeFanOut.Begin(Now, Point, SourceKey, MoveTemp(Candidates), GetDraws());
		int32 Responders = 0;
		StrikeFanOut.Process(MAX_int32, Config.Tuning.CrowdStrikeResponderLimit,
			[](const FCrowdPendingStrike&, const FCrowdStrikeCandidate&) { return true; },
			[this, &Responders](const FCrowdPendingStrike& Strike, const FCrowdStrikeCandidate& Candidate)
			{
				if (FNpc* Npc = FindNpc(Candidate.Ordinal))
				{
					Npc->Agent.OnStrikeStimulus(Strike.StimulusTime, Strike.Point, Strike.SourceKey, Candidate.DistanceCm, Now, *this);
					++Responders;
				}
			});
		return Responders;
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
