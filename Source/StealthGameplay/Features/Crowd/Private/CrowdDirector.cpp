#include "Server/CrowdDirector.h"

#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "NavigationData.h"
#include "NavigationSystem.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Server/Core/StealthPawnControlQuery.h"
#include "Server/ActionHub.h"
#include "Server/CrowdCostMeter.h"
#include "Server/CrowdMotionProfileData.h"
#include "Server/CrowdStateTreeComponent.h"
#include "Server/CrowdStateTreeSchema.h"
#include "Server/Session/StealthSessionService.h"
#include "Server/StealthServerTuning.h"
#include "Server/StealthTuningInvariants.h"
#include "Server/StealthTuningVersion.h"
#include "Server/StealthCrowdController.h"
#include "StateTree.h"
#include "View/Core/StealthPawn.h"
#include "View/Core/StealthPawnMovementComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogStealthCrowd, Log, All);

#if !UE_BUILD_SHIPPING
/** Dev only (story cnb-025 evidence): log each R8 responder's drawn reaction to the server log (crowd facts only). */
static TAutoConsoleVariable<int32> CVarCrowdLogStrikeReactions(TEXT("Crowd.LogStrikeReactions"), 0,
	TEXT("1: log each strike's R8 responders (ordinal, distance, drawn reaction) to the server log. Development only."));
#endif

namespace CrowdDirectorLocal
{
	/** The graybox route-node grid spacing (cm) until the layout supplies CrowdRoute nodes. */
	constexpr double RouteGridCm = 500.0;
	/** Catchment of one graybox node (m^2) and its frontage (cm): capacity 2. */
	constexpr double RouteCatchmentM2 = 25.0;
	constexpr double RouteFrontageCm = 200.0;
	/** A node counts a pawn standing within this radius (cm) as an occupant (any kind). */
	constexpr double NodeOccupancyRadiusCm = 100.0;
	/** A leg goal lies within this factor of the drawn leg length. */
	constexpr double LegBandLow = 0.6;
	constexpr double LegBandHigh = 1.4;
	/** The fallback sub-seed when no session is open (dev worlds, harness tests): a fixed public constant. */
	constexpr uint64 NoSessionSubSeed = 0x5EEDC40DDEADBEEFull;
	/** Eye height above the actor location (capsule centre) for line of sight. */
	constexpr float EyeAboveCentreCm = 70.0f;
	/** Run start for the salient stimulus (the SCL IsRunning threshold, ServerOnly key RunSpeedThresholdCmPerSec). */
	constexpr double RunStartSpeedCm = 300.0;
	constexpr double RunEndSpeedCm = 275.0;
}

// ---- Tick function -------------------------------------------------------------------------------------------------

void FCrowdDirectorTickFunction::ExecuteTick(float DeltaTime, ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	if (Director != nullptr && TickType != LEVELTICK_ViewportsOnly)
	{
		Director->TickDirector(DeltaTime);
	}
}

// ---- Subsystem -----------------------------------------------------------------------------------------------------

bool UCrowdDirector::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World != nullptr && World->IsGameWorld() && ShouldCreateForNetMode(World->GetNetMode());
}

UCrowdDirector* UCrowdDirector::Get(const UWorld* World)
{
	return World != nullptr ? World->GetSubsystem<UCrowdDirector>() : nullptr;
}

bool UCrowdDirector::IsScheduledTickEnabled()
{
	const IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(TEXT("StateTree.Component.ScheduledTickEnabled"));
	return Variable != nullptr && Variable->GetBool();
}

void UCrowdDirector::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (InWorld.GetNetMode() == NM_Client)
	{
		return;
	}
	// E6: with the cvar off every crowd tree silently ticks every frame; assert it once at init.
	if (!IsScheduledTickEnabled())
	{
		bScheduledTickAssertFailed = true;
		UE_LOGF(LogStealthCrowd, Error, "StateTree.Component.ScheduledTickEnabled is off: crowd decision cadence is broken (ADR-0016 E6).");
	}
	TickFunction.Director = this;
	TickFunction.TickGroup = TG_PrePhysics;
	TickFunction.bCanEverTick = true;
	TickFunction.bStartWithTickEnabled = true;
	TickFunction.bTickEvenWhenPaused = false;
	TickFunction.RegisterTickFunction(InWorld.PersistentLevel);
	bTickRegistered = true;
	if (UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(&InWorld))
	{
		Nav->OnNavigationGenerationFinishedDelegate.AddUniqueDynamic(this, &UCrowdDirector::HandleNavGenerationFinished);
	}
	if (UActionHub* Hub = UActionHub::Get(&InWorld))
	{
		ActionStartedHandle = Hub->OnActionStarted.AddUObject(this, &UCrowdDirector::HandleActionStarted);
	}
}

void UCrowdDirector::HandleActionStarted(AStealthPawn* Pawn, EActionType Type)
{
	const uint32 Ordinal = StealthPawnControl::GetSpawnOrdinal(Pawn);
	if (Ordinal != StealthPawnControl::NoSpawnOrdinal && FindController(Ordinal) != nullptr)
	{
		ActionStartedAt.Add((static_cast<uint64>(Ordinal) << 8) | static_cast<uint64>(Type), GetWorld()->GetTimeSeconds());
	}
	if (Type == EActionType::Attack && Pawn != nullptr)
	{
		// The strike point: the striker's reach in front of it (the stimulus is the start, never the outcome).
		const FVector Point = Pawn->GetActorLocation() + Pawn->GetActorForwardVector() * 100.0;
		NoteStrikeStarted(Point, KeyOf(Pawn), GetWorld()->GetTimeSeconds());
	}
}

void UCrowdDirector::NoteStrikeStarted(const FVector& Point, uint64 SourceKey, double StimulusTime)
{
	CROWD_COST_SCOPE(Perception);
	if (!Core.IsValid() || !Core->IsActive())
	{
		return; // dormant or halted: no reactions (CNB-R12)
	}
	StealthCrowdBlindness::FCrowdScope Scope;
	const FCrowdTuning_Server& T = GetCrowdTuning();
	TArray<FCrowdStrikeCandidate> Candidates;
	for (const TPair<uint32, TWeakObjectPtr<AStealthCrowdController>>& Pair : Controllers)
	{
		const AStealthCrowdController* Controller = Pair.Value.Get();
		const APawn* Pawn = Controller != nullptr ? Controller->GetPawn() : nullptr;
		if (Pawn == nullptr || KeyOf(Pawn) == SourceKey || Controller->GetDriverKind() != ECrowdDriverKind::Full || !Controller->GetAgent().IsStarted()
			|| Controller->GetAgent().IsStrikeRefractory(StimulusTime, T))
		{
			continue;
		}
		const double Distance = FVector::Dist2D(Pawn->GetActorLocation(), Point);
		if (Distance <= T.CrowdStrikeReactRadiusCm)
		{
			Candidates.Add({ Pair.Key, Distance, 0.0 });
		}
	}
	StrikeFanOut.Begin(StimulusTime, Point, SourceKey, MoveTemp(Candidates), GetDraws());
}

void UCrowdDirector::ProcessStrikes(double Now)
{
	CROWD_COST_SCOPE(Perception);
	if (!StrikeFanOut.HasPending())
	{
		return;
	}
	const FCrowdTuning_Server& T = GetCrowdTuning();
	const int32 Budget = FMath::Max(T.CrowdRaysPerFrameLimit - RaysThisFrame, 0);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CrowdStrikeSight), false);
	const int32 Used = StrikeFanOut.Process(Budget, T.CrowdStrikeResponderLimit,
		[this, &Params](const FCrowdPendingStrike& Strike, const FCrowdStrikeCandidate& Candidate)
		{
			const AStealthCrowdController* Controller = FindController(Candidate.Ordinal);
			const APawn* Pawn = Controller != nullptr ? Controller->GetPawn() : nullptr;
			if (Pawn == nullptr)
			{
				return false;
			}
			FHitResult Hit;
			FCollisionQueryParams Query = Params;
			Query.AddIgnoredActor(Pawn);
			const FVector Eye = Pawn->GetActorLocation() + FVector(0.0, 0.0, CrowdDirectorLocal::EyeAboveCentreCm);
			const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Eye, Strike.Point + FVector(0.0, 0.0, CrowdDirectorLocal::EyeAboveCentreCm), ECC_Visibility, Query);
			return !bHit || Cast<APawn>(Hit.GetActor()) != nullptr;
		},
		[this, Now](const FCrowdPendingStrike& Strike, const FCrowdStrikeCandidate& Candidate)
		{
			if (AStealthCrowdController* Controller = FindController(Candidate.Ordinal))
			{
				Controller->GetAgent().OnStrikeStimulus(Strike.StimulusTime, Strike.Point, Strike.SourceKey, Candidate.DistanceCm, Now, *this);
#if !UE_BUILD_SHIPPING
				if (CVarCrowdLogStrikeReactions.GetValueOnGameThread() != 0)
				{
					UE_LOGF(LogStealthCrowd, Log, "R8 strike %llu responder %u at %.0f cm: %ls", Strike.StrikeIndex, Candidate.Ordinal, Candidate.DistanceCm,
						LexToString(Controller->GetAgent().GetDrawnReaction()));
				}
#endif
			}
		});
	RaysThisFrame += Used;
	MaxRaysInAFrame = FMath::Max(MaxRaysInAFrame, RaysThisFrame);
}

void UCrowdDirector::HandleNavGenerationFinished(ANavigationData* NavData)
{
	if (Core.IsValid() && Core->IsActive())
	{
		++NavRebuildsDuringRound;
	}
}

void UCrowdDirector::Deinitialize()
{
	if (UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		Nav->OnNavigationGenerationFinishedDelegate.RemoveDynamic(this, &UCrowdDirector::HandleNavGenerationFinished);
	}
	if (UActionHub* Hub = UActionHub::Get(GetWorld()); Hub != nullptr && ActionStartedHandle.IsValid())
	{
		Hub->OnActionStarted.Remove(ActionStartedHandle);
	}
	if (bTickRegistered)
	{
		TickFunction.UnRegisterTickFunction();
		bTickRegistered = false;
	}
	Controllers.Reset();
	Core.Reset();
	Super::Deinitialize();
}

void UCrowdDirector::AddTickPrerequisiteTo(FTickFunction& Tick)
{
	if (bTickRegistered)
	{
		Tick.AddPrerequisite(this, TickFunction);
	}
}

bool UCrowdDirector::IsTickPrerequisiteOf(const FTickFunction& Tick) const
{
	for (const FTickPrerequisite& Prerequisite : const_cast<FTickFunction&>(Tick).GetPrerequisites())
	{
		if (Prerequisite.Get() == &TickFunction)
		{
			return true;
		}
	}
	return false;
}

// ---- Configuration -------------------------------------------------------------------------------------------------

void UCrowdDirector::SetCrowdArea(const FVector& Center, float RadiusCm, float InMarketRadiusCm)
{
	CrowdCenter = Center;
	CrowdRadiusCm = RadiusCm;
	MarketRadiusCm = InMarketRadiusCm;
}

bool UCrowdDirector::SetTuningForRound(const FCrowdTuning_Server& InTuning)
{
	if (Core.IsValid() && Core->IsActive())
	{
		++RefusedTuningWrites;
		return false;
	}
	Tuning = InTuning;
	bTuningOverridden = true;
	Core.Reset();
	return true;
}

void UCrowdDirector::EnsureCore()
{
	if (Core.IsValid())
	{
		return;
	}
	if (!bTuningOverridden)
	{
		// The frozen snapshot: the schema defaults until the runtime tuning store lands (story cnb-003 deviation).
		Tuning = FCrowdTuning_Server();
	}
	Locomotion = FLocomotionTuning_Public();
	uint64 SubSeed = CrowdDirectorLocal::NoSessionSubSeed;
	if (const USessionService* Session = USessionService::Get(GetWorld()); Session != nullptr && Session->IsSessionOpen())
	{
		SubSeed = Session->GetServerSubSeed(ESeedDomain::CrowdBehaviour, 0);
	}
	Core = MakeUnique<FCrowdDirectorCore>(Tuning, SubSeed);
	Hash = FCrowdSpatialHash(Tuning.CrowdHashCellCm);
	Stall = FCrowdStallRegulator();
	PathBudget.Reset();
	RayBudget.Reset();
	InFlight.Reset();
	StartPlacements.Reset();
	Tells = FCrowdTellSampler();
	ConfigureTells();
	DecisionStats = FCrowdDecisionStats();
	NavRebuildsDuringRound = 0;
	RoundTuningHash = 0;
}

void UCrowdDirector::ConfigureTells()
{
	// Runs, reversals, braces and head holds need no site; the others only run where this world has their sites
	// (an accepted tell that could never be performed would only be dropped). Bushes also need Concealment's filter.
	// Show-anchored (F2 as amended 2026-10-27): a tell on its way holds the chain gap until it is shown.
	Tells.SetShowAnchored(true);
	Tells.SetEnabled(ECrowdTell::Run, true);
	Tells.SetEnabled(ECrowdTell::Reversal, true);
	Tells.SetEnabled(ECrowdTell::Brace, true);
	Tells.SetEnabled(ECrowdTell::HeadHold, true);
	const bool bTraversals = HerringSites != nullptr && HerringSites->HasTraversals();
	Tells.SetEnabled(ECrowdTell::Vault, bTraversals);
	Tells.SetEnabled(ECrowdTell::Climb, bTraversals);
	Tells.SetEnabled(ECrowdTell::DecoyTend, HerringSites != nullptr && HerringSites->GetCourtCount() > 0);
	Tells.SetEnabled(ECrowdTell::Bush, HerringSites != nullptr && HerringSites->HasBushes() && PerceptionFilter.IsValid());
}

void UCrowdDirector::ResetForNewRound()
{
	Core.Reset();
	EnsureCore();
}

bool UCrowdDirector::IsNavReady() const
{
	UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	return Nav != nullptr && Nav->GetDefaultNavDataInstance(FNavigationSystem::DontCreate) != nullptr && !Nav->IsNavigationBuildInProgress();
}

UStateTree* UCrowdDirector::LoadTree(ECrowdDriverKind Kind) const
{
	const FSoftObjectPath& Path = Kind == ECrowdDriverKind::Full ? AssetKeys.CrowdFullStateTreeAsset : AssetKeys.CrowdRestrictedStateTreeAsset;
	UStateTree* Tree = Cast<UStateTree>(Path.TryLoad());
	return UCrowdStateTreeSchema::IsCrowdTree(Tree) ? Tree : nullptr;
}

uint64 UCrowdDirector::KeyOf(const AActor* Actor) const
{
	return Actor != nullptr ? static_cast<uint64>(Actor->GetUniqueID()) : 0;
}

// ---- Controllers ---------------------------------------------------------------------------------------------------

void UCrowdDirector::AttachDriver(AStealthCrowdController* Controller, AStealthPawn* Pawn)
{
	if (Controller == nullptr || Pawn == nullptr)
	{
		return;
	}
	EnsureCore();
	if (Core->IsHalted() && Controllers.Num() == 0)
	{
		// The previous round ended and its pawns are gone: a new round's crowd.
		ResetForNewRound();
	}
	const uint32 Ordinal = StealthPawnControl::GetSpawnOrdinal(Pawn);
	if (Ordinal == StealthPawnControl::NoSpawnOrdinal)
	{
		return; // not a factory pawn: the crowd never drives it
	}
	// The driver kind is the ICrowdControl call's (DriveBoundPawn registered it), never a binding read.
	const ECrowdDriverKind Kind = PendingRestricted.Remove(FObjectKey(Pawn)) > 0 ? ECrowdDriverKind::Restricted : ECrowdDriverKind::Full;
	Controller->BindDriver(Pawn, Kind, Ordinal, StealthPawnControl::GetControlEpoch(Pawn), this);
	Controllers.Add(Ordinal, Controller);
	FCrowdAgentRecord& Record = Core->RegisterAgent(Ordinal, Kind);
	const UCrowdMotionProfileData* Profile = Cast<UCrowdMotionProfileData>(AssetKeys.CrowdMotionProfileAsset.TryLoad());
	const TArray<FCrowdDeviceProfile> Profiles = Profile != nullptr && Profile->Profiles.Num() > 0 ? Profile->Profiles : UCrowdMotionProfileData::MakeProvisionalProfiles();
	Controller->GetAgent().Initialize(Ordinal, Kind, GetSelfFacts(*Controller), Record.GridPhase, Profiles, *this);
	if (Core->IsActive())
	{
		Core->JoinActiveRound(Ordinal, GetWorld()->GetTimeSeconds(), *this);
	}
	FCrowdAdoptParams Adopt;
	if (PendingAdoptions.RemoveAndCopyValue(FObjectKey(Pawn), Adopt))
	{
		Controller->GetAgent().ScheduleAdoption(GetWorld()->GetTimeSeconds(), Adopt.RestrictedUntilServerTime, Adopt.bExitAtNextScheduledSlot, false, 0.0, *this);
	}
	else if (Core->IsHalted())
	{
		Controller->HaltBehaviour();
	}
}

void UCrowdDirector::DetachDriver(AStealthCrowdController* Controller)
{
	if (Controller == nullptr)
	{
		return;
	}
	const uint32 Ordinal = Controller->GetOrdinal();
	const TWeakObjectPtr<AStealthCrowdController>* Found = Controllers.Find(Ordinal);
	if (Found != nullptr && Found->Get() == Controller)
	{
		Controllers.Remove(Ordinal);
		if (Core.IsValid())
		{
			Core->UnregisterAgent(Ordinal);
		}
		Stall.Remove(Ordinal);
		Tells.RemoveNpc(Ordinal);
	}
}

AStealthCrowdController* UCrowdDirector::FindController(uint32 Ordinal) const
{
	const TWeakObjectPtr<AStealthCrowdController>* Found = Controllers.Find(Ordinal);
	return Found != nullptr ? Found->Get() : nullptr;
}

FCrowdPerceivedPawn UCrowdDirector::GetSelfFacts(const AStealthCrowdController& Controller) const
{
	const AStealthPawn* Pawn = Controller.GetDrivenPawn();
	if (Pawn == nullptr)
	{
		return FCrowdPerceivedPawn();
	}
	if (const FCrowdPerceivedPawn* Known = Hash.Find(KeyOf(Pawn)))
	{
		FCrowdPerceivedPawn Self = *Known;
		// The controller ticks after the Director: refresh the motion facts to this frame's values.
		Self.Location = Pawn->GetActorLocation();
		Self.VelocityCmPerSec = Pawn->GetVelocity();
		Self.FacingYawDeg = static_cast<float>(Pawn->GetActorRotation().Yaw);
		return Self;
	}
	FCrowdPerceivedPawn Self;
	Self.Key = KeyOf(Pawn);
	Self.Ordinal = Controller.GetOrdinal();
	Self.Location = Pawn->GetActorLocation();
	Self.VelocityCmPerSec = Pawn->GetVelocity();
	Self.FacingYawDeg = static_cast<float>(Pawn->GetActorRotation().Yaw);
	Self.EyeHeightCm = CrowdDirectorLocal::EyeAboveCentreCm;
	return Self;
}

void UCrowdDirector::NoteDecision()
{
	if (DecisionStats.Frame != DirectorFrame)
	{
		DecisionStats.Frame = DirectorFrame;
		DecisionStats.ThisFrame = 0;
	}
	++DecisionStats.ThisFrame;
	++DecisionStats.Total;
	DecisionStats.MaxPerFrame = FMath::Max(DecisionStats.MaxPerFrame, DecisionStats.ThisFrame);
}

bool UCrowdDirector::AdmitWakeDecision()
{
	if (WakeFrame != DirectorFrame)
	{
		WakeFrame = DirectorFrame;
		WakesThisFrame = 0;
	}
	if (WakesThisFrame >= WakeDecisionsPerFrame)
	{
		++DeferredWakes;
		return false;
	}
	++WakesThisFrame;
	return true;
}

// ---- ICrowdControl -------------------------------------------------------------------------------------------------

void UCrowdDirector::StartRound()
{
	CROWD_COST_SCOPE(Director);
	StealthCrowdBlindness::FCrowdScope Scope;
	EnsureCore();
	if (Core->IsHalted())
	{
		ResetForNewRound();
	}
	TArray<FString> Problems;
	if (!StealthCrowdTuning::ValidateAssetKeys(AssetKeys, &Problems))
	{
		for (const FString& Problem : Problems)
		{
			UE_LOGF(LogStealthCrowd, Warning, "Crowd asset key invalid: %ls (the controllers use the fallback grid).", *Problem);
		}
	}
	FStealthServerTuning Snapshot;
	Snapshot.Crowd_Server = GetCrowdTuning();
	RoundTuningHash = StealthCrowdTuning::ComputeRoundTuningHash(Snapshot, AssetKeys, TuningSchemaVersion);
	if (Density.GetNodes().Num() == 0)
	{
		BuildRouteNodes();
	}
	BuildHash();
	ConfigureTells();
	Core->StartRound(GetWorld()->GetTimeSeconds(), IsNavReady(), *this);
	if (Core->IsNavNotReady())
	{
		UE_LOGF(LogStealthCrowd, Error, "Crowd StartRound: NavNotReady; every crowd pawn stays Dormant (ADR-0016 section 7).");
	}
}

void UCrowdDirector::StartAgent(uint32 Ordinal, const FCrowdFirstActivity& First, double Now)
{
	AStealthCrowdController* Controller = FindController(Ordinal);
	const FCrowdAgentRecord* Record = Core.IsValid() ? Core->FindAgent(Ordinal) : nullptr;
	if (Controller == nullptr || Record == nullptr)
	{
		return;
	}
	Controller->StartBehaviour(First, Now, Core->GetGridOrigin() + Record->GridPhase, Core->GetDecisionPeriod(), LoadTree(Record->Driver));
}

void UCrowdDirector::Halt()
{
	CROWD_COST_SCOPE(Director);
	EnsureCore();
	// Synchronously, in this call: latch every sink, clear Move, stop every tree (never by an event alone).
	Core->Halt();
	for (TPair<uint32, TWeakObjectPtr<AStealthCrowdController>>& Pair : Controllers)
	{
		if (AStealthCrowdController* Controller = Pair.Value.Get())
		{
			Controller->HaltBehaviour();
		}
	}
	PathBudget.Reset();
	RayBudget.Reset();
	// Reactions stop with the round (CNB-R12): no strike fanned out after Halt.
	StrikeFanOut.Reset();
}

void UCrowdDirector::DriveBoundPawn(AStealthPawn* Pawn)
{
	if (Pawn != nullptr)
	{
		PendingRestricted.Add(FObjectKey(Pawn));
	}
}

void UCrowdDirector::ReleaseControl(AStealthPawn* Pawn)
{
	// Hand-back (CNB-R10.3): Move(0), the tree stopped and the driver detached at the start of the next Director frame,
	// never during a traversal (the possession change itself is the factory's hand-over pass, ADR-0010). Idempotent.
	const uint32 Ordinal = StealthPawnControl::GetSpawnOrdinal(Pawn);
	if (FindController(Ordinal) != nullptr)
	{
		PendingReleases.AddUnique(Ordinal);
	}
	PendingRestricted.Remove(FObjectKey(Pawn));
}

void UCrowdDirector::AdoptReleasedPawn(AStealthPawn* Pawn, const FCrowdAdoptParams& Params)
{
	if (Pawn == nullptr)
	{
		return;
	}
	EnsureCore();
	const uint32 Ordinal = StealthPawnControl::GetSpawnOrdinal(Pawn);
	AStealthCrowdController* Controller = FindController(Ordinal);
	if (ReleasedForTakeover.Remove(Ordinal) > 0)
	{
		// The reversal of a TryReleaseNpc that did not bind (story cnb-030): B - 1, sinks unlatched at the pawn's current
		// epoch, the tree restarted from the agent's current activity.
		Core->NoteTakeoverReversed();
		if (Controller != nullptr)
		{
			const FCrowdAgentRecord* Record = Core->FindAgent(Ordinal);
			Controller->ResumeAfterReversal(StealthPawnControl::GetControlEpoch(Pawn), LoadTree(Record != nullptr ? Record->Driver : ECrowdDriverKind::Full));
		}
		return;
	}
	// ReleaseToCrowd (CNB-R10.4): counted into the ledger; nothing visible changes now.
	Core->NoteRelease();
	if (Controller == nullptr)
	{
		// Not crowd-driven yet: the restricted driver attaches first (the lockout), then the adoption applies.
		PendingRestricted.Add(FObjectKey(Pawn));
		PendingAdoptions.Add(FObjectKey(Pawn), Params);
		return;
	}
	const ICrowdPerceptionFilter* Filter = PerceptionFilter.Get();
	const bool bConcealed = Filter != nullptr && !Filter->IsPerceivable(Pawn);
	// The time already concealed is Concealment's to report (not built): 0 until it is (an unconditioned stay).
	Controller->GetAgent().ScheduleAdoption(GetWorld()->GetTimeSeconds(), Params.RestrictedUntilServerTime, Params.bExitAtNextScheduledSlot, bConcealed, 0.0, *this);
}

void UCrowdDirector::OnAgentPromoted(uint32 Ordinal)
{
	if (FCrowdAgentRecord* Record = Core.IsValid() ? Core->FindAgent(Ordinal) : nullptr)
	{
		Record->Driver = ECrowdDriverKind::Full;
	}
	if (AStealthCrowdController* Controller = FindController(Ordinal))
	{
		Controller->PromoteToFull(LoadTree(ECrowdDriverKind::Full));
	}
}

bool UCrowdDirector::ClaimExitSlot(uint32 Ordinal)
{
	// The ambient schedule gives its next exit slot to the first claimant (no schedule runs until LSC sockets exist).
	ExitClaims.AddUnique(Ordinal);
	return false;
}

void UCrowdDirector::RequestPawnStop(const FRoundLivePawn& LivePawn, double BoundSec)
{
	// Nothing for the crowd to do: Halt (earlier in the same round-end step) already latched every crowd sink and cleared
	// every AI-driven pawn's held Move, and the kind-blind stop-and-blend of every pawn is the factory's (ADR-0010 4.5).
	// The crowd never reads the record (it would carry a driver kind; and its header names the Roster call types).
}

// ---- ITakeoverEligibilitySource ------------------------------------------------------------------------------------

ETakeoverEligibility UCrowdDirector::GetTakeoverEligibility(const AStealthPawn* Pawn) const
{
	const uint32 Ordinal = StealthPawnControl::GetSpawnOrdinal(Pawn);
	const FCrowdAgentRecord* Record = Core.IsValid() ? Core->FindAgent(Ordinal) : nullptr;
	if (Record == nullptr)
	{
		return ETakeoverEligibility::Bound;
	}
	if (Record->Driver == ECrowdDriverKind::Restricted || ReleasedForTakeover.Contains(Ordinal))
	{
		return ETakeoverEligibility::Bound;
	}
	// AlleyExit: any pawn whose leg is an ambient exit, including an exile body waiting for its slot (CNB-R11): a
	// participant can never re-adopt its own released body.
	if (const AStealthCrowdController* Controller = FindController(Ordinal);
		Controller != nullptr && (Controller->GetAgent().IsExiting() || Controller->GetAgent().IsAwaitingExitSlot() || ExitClaims.Contains(Ordinal)))
	{
		return ETakeoverEligibility::AlleyExit;
	}
	switch (Record->Activity)
	{
	case ECrowdActivity::Tending: return ETakeoverEligibility::Tending;
	case ECrowdActivity::Bracing: return ETakeoverEligibility::Bracing;
	case ECrowdActivity::Hiding: return ETakeoverEligibility::Hiding;
	case ECrowdActivity::Emerging: return ETakeoverEligibility::Emerging;
	case ECrowdActivity::Exiting: return ETakeoverEligibility::Exiting;
	case ECrowdActivity::Reacting: return ETakeoverEligibility::Reacting;
	case ECrowdActivity::Conversing: return ETakeoverEligibility::Conversing;
	default: return ETakeoverEligibility::Eligible;
	}
}

bool UCrowdDirector::TryReleaseNpc(AStealthPawn* Pawn, FEpochToken Expected)
{
	// Story cnb-030 (CNB-R11): called inside Penalty's commit slot on the pawn PickRespawnPawn returned. Atomic in this
	// call: re-check eligibility, latch the sinks and stop the tree (no Move(0): the pawn keeps moving as it was), bump the
	// control epoch (every older command, path callback or debounced End is dropped from now on), and take the pawn out of
	// the crowd ledger (B + 1). The round epoch token is the caller's (Penalty validated it in its slot).
	if (Pawn == nullptr || !Core.IsValid() || Core->IsHalted() || GetTakeoverEligibility(Pawn) != ETakeoverEligibility::Eligible)
	{
		return false;
	}
	const uint32 Ordinal = StealthPawnControl::GetSpawnOrdinal(Pawn);
	AStealthCrowdController* Controller = FindController(Ordinal);
	if (Controller == nullptr)
	{
		return false;
	}
	Controller->ReleaseForTakeover();
	StealthPawnControl::BumpControlEpoch(Pawn);
	Core->NoteTakeover();
	ReleasedForTakeover.Add(Ordinal);
	return true;
}

// ---- ICrowdAgentWorld ----------------------------------------------------------------------------------------------

const FCrowdTuning_Server& UCrowdDirector::GetCrowdTuning() const
{
	return Core.IsValid() ? Core->GetTuning() : Tuning;
}

FCrowdDraws& UCrowdDirector::GetDraws()
{
	EnsureCore();
	return Core->GetDraws();
}

double UCrowdDirector::GetLongPausePct(double Now) const
{
	const int32 Live = Core.IsValid() ? Core->NumAgents() : 0;
	// P_th = SessionSoftTimeoutSec + 1 (F4); the Session key is a pending owner key (I-CNB-1 reads the same value).
	const double Threshold = StealthTuningPendingOwnerKeys::SessionSoftTimeoutSec + 1.0;
	// The F4 projection: pawns whose current rest still runs at Now + P_th (read from the agents' own state only).
	int32 Projected = 0;
	for (const TPair<uint32, TWeakObjectPtr<AStealthCrowdController>>& Pair : Controllers)
	{
		if (const AStealthCrowdController* Controller = Pair.Value.Get())
		{
			Projected += Controller->GetAgent().GetRestEnd() > Now + Threshold ? 1 : 0;
		}
	}
	return Stall.EffectiveLongPausePct(Now, Live, GetCrowdTuning(), Threshold, Projected);
}

bool UCrowdDirector::PickLegGoal(uint32 Ordinal, const FVector& From, double LegCm, FVector& OutGoal)
{
	using namespace CrowdDirectorLocal;
	const TConstArrayView<FCrowdRouteNode> Nodes = Density.GetNodes();
	if (Nodes.Num() == 0)
	{
		return false;
	}
	// Occupancy (any kind) and the leg-length band: a node outside the band is marked full for this pick.
	TArray<int32> Occupancy;
	Occupancy.SetNumZeroed(Nodes.Num());
	for (int32 Index = 0; Index < Nodes.Num(); ++Index)
	{
		const double Distance = FVector::Dist2D(Nodes[Index].Location, From);
		if (Distance < LegBandLow * LegCm || Distance > LegBandHigh * LegCm)
		{
			Occupancy[Index] = MAX_int32;
			continue;
		}
		TArray<const FCrowdPerceivedPawn*> Near;
		Hash.Query(Nodes[Index].Location, NodeOccupancyRadiusCm, ECrowdQuery::Steering, 0, Near);
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

bool UCrowdDirector::RequestPath(const FCrowdPathRequest& Request, TArray<FVector>& OutCorners)
{
	// No CrowdRoute edges exist until the layout service lands: every leg is an async query through the budget.
	EnsureCore();
	PathBudget.Enqueue(Request);
	return false;
}

bool UCrowdDirector::FindPauseContext(const FVector& At, double Radius, FVector& OutLookPoint, bool& bOutGoods) const
{
	// The layout publishes no stall, mural or view records yet: no context (the GDD's "no facing requirement").
	return false;
}

bool UCrowdDirector::HasLineOfSight(uint32 Ordinal, const FVector& Eye, const FVector& Target)
{
	CROWD_COST_SCOPE(Perception);
	if (RaysThisFrame >= GetCrowdTuning().CrowdRaysPerFrameLimit)
	{
		return false; // deferred: the caller tries again later (G2 retries; stimuli queue with their time)
	}
	++RaysThisFrame;
	MaxRaysInAFrame = FMath::Max(MaxRaysInAFrame, RaysThisFrame);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CrowdSight), false);
	if (const AStealthCrowdController* Controller = FindController(Ordinal))
	{
		Params.AddIgnoredActor(Controller->GetPawn());
	}
	FHitResult Hit;
	// The crowd sight channel is a project collision change not made yet: the visibility channel; a pawn hit is not a
	// blocker (pawns never block crowd sight).
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Eye, Target, ECC_Visibility, Params);
	return !bHit || Cast<APawn>(Hit.GetActor()) != nullptr;
}

void UCrowdDirector::ReportActivity(uint32 Ordinal, ECrowdActivity Activity)
{
	if (Core.IsValid())
	{
		Core->SetActivity(Ordinal, Activity);
	}
}

uint32 UCrowdDirector::GetCurrentControlEpoch(uint32 Ordinal) const
{
	const AStealthCrowdController* Controller = FindController(Ordinal);
	return Controller != nullptr ? StealthPawnControl::GetControlEpoch(Controller->GetPawn()) : 0u;
}

FCrowdActionTimings UCrowdDirector::GetActionTimings() const
{
	// Other owners' values (never copied into a Crowd key): the schema defaults until the runtime tuning store lands.
	FCrowdActionTimings Timings;
	Timings.ObjectiveInteractSec = FObjectivesTuning_Public().ObjectiveInteractSec;
	Timings.PrepareCapSec = FAssassinationTuning_Public().DefenceDurationSec;
	Timings.PrepareCooldownSec = FAssassinationTuning_Public().DefenceCooldownSec;
	Timings.HandoverHoldSec = FLocomotionTuning_Server().HandoverHoldSec;
	Timings.BushReenterCooldownSec = FConcealmentTuning_Server().BushReenterCooldownSec; // registered by story con-001
	return Timings;
}

bool UCrowdDirector::IsOnNavigation(const FVector& Point) const
{
	const UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	FNavLocation OnNav;
	return Nav == nullptr || Nav->ProjectPointToNavigation(Point, OnNav, FVector(50.0, 50.0, 250.0));
}

bool UCrowdDirector::FindTraversalNear(uint32 Ordinal, const FVector& From, double MaxCm, bool bClimb, FCrowdTraversalSite& Out)
{
	return HerringSites != nullptr && HerringSites->FindTraversalNear(From, MaxCm, bClimb, Out);
}

bool UCrowdDirector::IsTraversing(uint32 Ordinal) const
{
	const AStealthCrowdController* Controller = FindController(Ordinal);
	const AStealthPawn* Pawn = Controller != nullptr ? Controller->GetDrivenPawn() : nullptr;
	const UStealthMovementComponent* Movement = Pawn != nullptr ? Pawn->GetStealthMovement() : nullptr;
	return Movement != nullptr && Movement->IsInTraversal();
}

int32 UCrowdDirector::GetCourtCount() const
{
	return HerringSites != nullptr ? HerringSites->GetCourtCount() : 0;
}

bool UCrowdDirector::GetCourtStandPoint(int32 Court, const FVector& From, FCrowdCourtSite& Out)
{
	return HerringSites != nullptr && HerringSites->GetCourtStandPoint(Court, From, Out);
}

bool UCrowdDirector::FindBush(const FVector& From, TConstArrayView<FVector> Path, double RadiusCm, FCrowdBushSite& Out)
{
	return HerringSites != nullptr && PerceptionFilter.IsValid() && HerringSites->FindBush(From, Path, RadiusCm, Out);
}

ECrowdBeginOutcome UCrowdDirector::GetBeginOutcome(uint32 Ordinal, EActionType Type, double SubmittedAt, double Now)
{
	const UActionHub* Hub = UActionHub::Get(GetWorld());
	if (Hub == nullptr || !Hub->HasPredicate(Type))
	{
		// No legality owner for the action in this build: nothing can refuse it.
		return ECrowdBeginOutcome::Accepted;
	}
	if (const double* Started = ActionStartedAt.Find((static_cast<uint64>(Ordinal) << 8) | static_cast<uint64>(Type)); Started != nullptr && *Started >= SubmittedAt)
	{
		return ECrowdBeginOutcome::Accepted;
	}
	// The command drains in the next queue slot: no start a few frames after the submit is a refusal (NotNow).
	return Now - SubmittedAt > 0.1 ? ECrowdBeginOutcome::Refused : ECrowdBeginOutcome::Pending;
}

// ---- Start placement -----------------------------------------------------------------------------------------------

bool UCrowdDirector::RequestStartPlacement(int32 Index, int32 NpcCount, FTransform& OutTransform)
{
	StealthCrowdBlindness::FCrowdScope Scope;
	EnsureCore();
	if (Index == 0 || Density.GetNodes().Num() == 0)
	{
		// The round's layout was just built (Preparing): the route nodes follow it.
		BuildRouteNodes();
	}
	if (Density.GetNodes().Num() == 0)
	{
		return false;
	}
	if (StartPlacements.Num() == 0 || Index == 0)
	{
		uint64 SubSeed = CrowdDirectorLocal::NoSessionSubSeed ^ 0x5u;
		if (const USessionService* Session = USessionService::Get(GetWorld()); Session != nullptr && Session->IsSessionOpen())
		{
			SubSeed = Session->GetServerSubSeed(ESeedDomain::NpcRoutes, 0);
		}
		// Pawns of any kind already standing count against node capacity.
		BuildHash();
		TArray<int32> Occupancy;
		Occupancy.SetNumZeroed(Density.GetNodes().Num());
		for (const FCrowdPerceivedPawn& Pawn : Hash.GetPawns())
		{
			const int32 Node = Density.NearestNode(Pawn.Location);
			if (Node != INDEX_NONE && FVector::Dist2D(Density.GetNodes()[Node].Location, Pawn.Location) <= CrowdDirectorLocal::NodeOccupancyRadiusCm)
			{
				++Occupancy[Node];
			}
		}
		const TArray<int32> Nodes = Density.PlaceStarts(FMath::Max(NpcCount, Index + 1), SubSeed, Occupancy);
		StartPlacements.Reset();
		for (int32 Placed = 0; Placed < Nodes.Num(); ++Placed)
		{
			const FVector Location = Density.GetNodes()[Nodes[Placed]].Location;
			// A small spread inside the node so two occupants never stand on one point (NpcRoutes-only, assignment-blind).
			const double Angle = 2.0 * UE_DOUBLE_PI * static_cast<double>(Placed % 8) / 8.0;
			const FVector Spread = FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0) * (Placed % 2 == 0 ? 0.0 : 60.0);
			StartPlacements.Add(FTransform(FRotator(0.0, static_cast<double>((Placed * 137) % 360), 0.0), Location + Spread + FVector(0.0, 0.0, 100.0)));
		}
	}
	if (!StartPlacements.IsValidIndex(Index))
	{
		return false;
	}
	OutTransform = StartPlacements[Index];
	return true;
}

void UCrowdDirector::BuildRouteNodes()
{
	using namespace CrowdDirectorLocal;
	UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (Nav == nullptr || Nav->GetDefaultNavDataInstance(FNavigationSystem::DontCreate) == nullptr)
	{
		return;
	}
	TArray<FCrowdRouteNode> Nodes;
	const int32 Steps = FMath::CeilToInt32(CrowdRadiusCm / RouteGridCm);
	for (int32 X = -Steps; X <= Steps; ++X)
	{
		for (int32 Y = -Steps; Y <= Steps; ++Y)
		{
			const FVector Point = CrowdCenter + FVector(X * RouteGridCm, Y * RouteGridCm, 0.0);
			if (FVector::Dist2D(Point, CrowdCenter) > CrowdRadiusCm)
			{
				continue;
			}
			FNavLocation OnNav;
			if (!Nav->ProjectPointToNavigation(Point, OnNav, FVector(RouteGridCm * 0.4, RouteGridCm * 0.4, 500.0)))
			{
				continue;
			}
			FCrowdRouteNode& Node = Nodes.AddDefaulted_GetRef();
			Node.Location = OnNav.Location;
			// Graybox area types until the layout manifest supplies them: the central plaza is Market, the rest Causeway.
			Node.Area = FVector::Dist2D(OnNav.Location, CrowdCenter) <= MarketRadiusCm ? ECrowdAreaType::Market : ECrowdAreaType::Causeway;
			Node.CatchmentM2 = RouteCatchmentM2;
			Node.FrontageCm = RouteFrontageCm;
		}
	}
	Density.Initialize(Nodes, GetCrowdTuning().NpcCount, GetCrowdTuning());
}

// ---- Frame ---------------------------------------------------------------------------------------------------------

void UCrowdDirector::BuildHash()
{
	CROWD_COST_SCOPE(Perception);
	TArray<FCrowdPerceivedPawn> Pawns;
	const ICrowdPerceptionFilter* Filter = PerceptionFilter.Get();
	for (TActorIterator<AStealthPawn> It(GetWorld()); It; ++It)
	{
		const AStealthPawn* Pawn = *It;
		if (Pawn == nullptr || Pawn->IsActorBeingDestroyed())
		{
			continue;
		}
		FCrowdPerceivedPawn& Facts = Pawns.AddDefaulted_GetRef();
		Facts.Key = KeyOf(Pawn);
		Facts.Ordinal = StealthPawnControl::GetSpawnOrdinal(Pawn);
		Facts.Location = Pawn->GetActorLocation();
		Facts.VelocityCmPerSec = Pawn->GetVelocity();
		Facts.FacingYawDeg = static_cast<float>(Pawn->GetActorRotation().Yaw);
		Facts.EyeHeightCm = CrowdDirectorLocal::EyeAboveCentreCm;
		Facts.bPerceivable = Filter == nullptr || Filter->IsPerceivable(Pawn);
		Facts.bObstacle = Filter == nullptr || Filter->IsSteeringObstacle(Pawn);
		const double Speed = Facts.Speed2D();
		bool& bRunning = WasRunning.FindOrAdd(Facts.Key);
		const bool bNowRunning = bRunning ? Speed > CrowdDirectorLocal::RunEndSpeedCm : Speed > CrowdDirectorLocal::RunStartSpeedCm;
		Facts.bRunning = bNowRunning;
		// The run start is a salient event (G4): marked for this frame's fan-out.
		Facts.bRunStart = bNowRunning && !bRunning;
		bRunning = bNowRunning;
		// A traversal that ended this frame is a landing (G4, and the T4 flinch trigger within 3 m).
		const UStealthMovementComponent* Movement = Pawn->GetStealthMovement();
		const bool bNowTraversing = Movement != nullptr && Movement->IsInTraversal();
		bool& bWasTraversing = WasTraversing.FindOrAdd(Facts.Key);
		Facts.bTraversalLanding = bWasTraversing && !bNowTraversing;
		bWasTraversing = bNowTraversing;
	}
	Hash.Build(Pawns);
}

void UCrowdDirector::FanOutSalient(double Now)
{
	CROWD_COST_SCOPE(Perception);
	const FCrowdTuning_Server& T = GetCrowdTuning();
	for (const FCrowdPerceivedPawn& Source : Hash.GetPawns())
	{
		if (!(Source.bRunStart || Source.bTraversalLanding) || !Source.bPerceivable)
		{
			continue;
		}
		TArray<const FCrowdPerceivedPawn*> Near;
		Hash.Query(Source.Location, T.CrowdSalientRangeCm, ECrowdQuery::Perception, Source.Key, Near);
		for (const FCrowdPerceivedPawn* Watcher : Near)
		{
			if (FindController(Watcher->Ordinal) == nullptr)
			{
				continue;
			}
			FCrowdRayRequest Ray;
			Ray.Requester = Watcher->Ordinal;
			Ray.Tag = static_cast<uint32>(Source.Key);
			Ray.From = Watcher->Location + FVector(0.0, 0.0, Watcher->EyeHeightCm);
			Ray.To = Source.Location + FVector(0.0, 0.0, Source.EyeHeightCm);
			Ray.StimulusTime = Now;
			RayBudget.Request(Ray);
		}
	}
	const int32 Budget = FMath::Max(T.CrowdRaysPerFrameLimit - RaysThisFrame, 0);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CrowdStimulusSight), false);
	const int32 Done = RayBudget.Process(Budget,
		[this, &Params](const FVector& From, const FVector& To)
		{
			FHitResult Hit;
			const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, From, To, ECC_Visibility, Params);
			return !bHit || Cast<APawn>(Hit.GetActor()) != nullptr;
		},
		[this, Now](const FCrowdRayRequest& Ray, bool bVisible)
		{
			if (!bVisible)
			{
				return;
			}
			if (AStealthCrowdController* Controller = FindController(Ray.Requester))
			{
				// The deferred ray keeps its stimulus time: the reaction delay runs from the stimulus.
				Controller->GetAgent().OnSalientStimulus(Ray.StimulusTime, Ray.Tag, Ray.To, Now, *this);
			}
		});
	RaysThisFrame += Done;
	MaxRaysInAFrame = FMath::Max(MaxRaysInAFrame, RaysThisFrame);
}

void UCrowdDirector::StartPaths()
{
	CROWD_COST_SCOPE(NavQuery);
	UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	ANavigationData* NavData = Nav != nullptr ? Nav->GetDefaultNavDataInstance(FNavigationSystem::DontCreate) : nullptr;
	if (NavData == nullptr || !IsNavReady())
	{
		// No query before the navmesh is ready (OnLayoutReady): requests wait, never drop.
		return;
	}
	const int32 Started = PathBudget.StartThisFrame(GetCrowdTuning().CrowdPathStartsPerFrameLimit, [this, Nav, NavData](const FCrowdPathRequest& Request)
	{
		FPathFindingQuery Query(this, *NavData, Request.From, Request.To);
		const uint32 QueryId = Nav->FindPathAsync(FNavAgentProperties::DefaultProperties, Query,
			FNavPathQueryDelegate::CreateUObject(this, &UCrowdDirector::OnPathFound), EPathFindingMode::Regular);
		if (QueryId != INVALID_NAVQUERYID)
		{
			InFlight.Add(QueryId, Request);
			InFlightStartFrame.Add(QueryId, GFrameCounter);
		}
	});
	MaxPathStartsInAFrame = FMath::Max(MaxPathStartsInAFrame, Started);
}

void UCrowdDirector::OnPathFound(uint32 QueryId, ENavigationQueryResult::Type Result, FNavPathSharedPtr Path)
{
	CROWD_COST_SCOPE(NavQuery);
	FCrowdPathRequest Request;
	if (!InFlight.RemoveAndCopyValue(QueryId, Request))
	{
		return;
	}
	uint64 StartFrame = 0;
	if (InFlightStartFrame.RemoveAndCopyValue(QueryId, StartFrame) && PathLatencyFrames.Num() < 100000)
	{
		PathLatencyFrames.Add(static_cast<int32>(GFrameCounter - StartFrame));
	}
	FCrowdPathResult Out;
	Out.Ordinal = Request.Ordinal;
	Out.LegId = Request.LegId;
	Out.ControlEpoch = Request.ControlEpoch;
	Out.bValid = Result == ENavigationQueryResult::Success && Path.IsValid() && Path->IsValid();
	Out.bPartial = Path.IsValid() && Path->IsPartial();
	if (Out.bValid)
	{
		const TArray<FNavPathPoint>& Points = Path->GetPathPoints();
		for (int32 Index = 1; Index < Points.Num(); ++Index)
		{
			Out.Corners.Add(Points[Index].Location);
		}
	}
	if (AStealthCrowdController* Controller = FindController(Request.Ordinal))
	{
		Controller->GetAgent().OnPathResult(Out, *this);
	}
}

void UCrowdDirector::TickDirector(float DeltaTime)
{
	CROWD_COST_SCOPE(Director);
	StealthCrowdBlindness::FCrowdScope Scope;
	++DirectorFrame;
	if (!Core.IsValid())
	{
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	RaysThisFrame = 0;
	// Hand-backs at the frame start, never during a traversal (CNB-R10.3).
	for (int32 Index = PendingReleases.Num() - 1; Index >= 0; --Index)
	{
		AStealthCrowdController* Controller = FindController(PendingReleases[Index]);
		if (Controller == nullptr)
		{
			PendingReleases.RemoveAt(Index);
		}
		else if (!IsTraversing(PendingReleases[Index]))
		{
			Controller->HaltBehaviour();
			PendingReleases.RemoveAt(Index);
		}
	}
	if (Controllers.Num() == 0)
	{
		// Nothing to perceive for; pending path requests still wait on the navmesh gate.
		StartPaths();
		return;
	}
	// The spatial hash once per frame from public facts; the stretch clocks from movement state.
	BuildHash();
	for (const TPair<uint32, TWeakObjectPtr<AStealthCrowdController>>& Pair : Controllers)
	{
		const AStealthCrowdController* Controller = Pair.Value.Get();
		const APawn* Pawn = Controller != nullptr ? Controller->GetPawn() : nullptr;
		if (Pawn == nullptr)
		{
			continue;
		}
		const FCrowdPerceivedPawn* Facts = Hash.Find(KeyOf(Pawn));
		Stall.Update(Pair.Key, Pawn->GetVelocity().Size2D(), Facts != nullptr && !Facts->bPerceivable, Now);
	}
	if (Core->IsActive())
	{
		ProcessStrikes(Now);
		FanOutSalient(Now);
	}
	StartPaths();
}
