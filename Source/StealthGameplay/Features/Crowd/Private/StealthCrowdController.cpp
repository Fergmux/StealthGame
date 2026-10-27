#include "Server/StealthCrowdController.h"

#include "Server/CrowdCostMeter.h"

#include "Engine/World.h"
#include "Server/CrowdDirector.h"
#include "Server/CrowdStateTreeComponent.h"
#include "StateTree.h"
#include "View/Core/StealthPawn.h"

AStealthCrowdController::AStealthCrowdController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.DoNotCreateDefaultSubobject(TEXT("PathFollowingComponent")))
{
	// ADR-0016 section 1 / E4: engine defaults, asserted here so a change in either is visible.
	bWantsPlayerState = false;
	bStartAILogicOnPossess = false;
	bSetControlRotationFromPawnOrientation = false;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;
	CrowdStateTree = CreateDefaultSubobject<UCrowdStateTreeComponent>(TEXT("CrowdStateTree"));
	BrainComponent = CrowdStateTree;
}

AStealthPawn* AStealthCrowdController::GetDrivenPawn() const
{
	return Cast<AStealthPawn>(GetPawn());
}

void AStealthCrowdController::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	// At spawn, not only at BeginPlay: a world that never dispatches BeginPlay (no game mode) still orders the ticks.
	WireTickOrder();
}

void AStealthCrowdController::BeginPlay()
{
	Super::BeginPlay();
	WireTickOrder();
}

void AStealthCrowdController::WireTickOrder()
{
	if (CrowdStateTree == nullptr)
	{
		return;
	}
	if (UCrowdDirector* Found = UCrowdDirector::Get(GetWorld()))
	{
		// Tick order: Director -> this controller's StateTree component -> this controller (-> the pawn's CMC).
		Found->AddTickPrerequisiteTo(CrowdStateTree->PrimaryComponentTick);
	}
	PrimaryActorTick.AddPrerequisite(CrowdStateTree, CrowdStateTree->PrimaryComponentTick);
}

void AStealthCrowdController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UCrowdDirector* Found = Director.Get())
	{
		Found->DetachDriver(this);
	}
	Super::EndPlay(EndPlayReason);
}

bool AStealthCrowdController::VerifyTickOrder() const
{
	const UCrowdDirector* Found = UCrowdDirector::Get(GetWorld());
	if (Found == nullptr || CrowdStateTree == nullptr)
	{
		return false;
	}
	const bool bTreeAfterDirector = Found->IsTickPrerequisiteOf(CrowdStateTree->PrimaryComponentTick);
	bool bControllerAfterTree = false;
	for (const FTickPrerequisite& Prerequisite : const_cast<FActorTickFunction&>(PrimaryActorTick).GetPrerequisites())
	{
		bControllerAfterTree |= Prerequisite.Get() == &CrowdStateTree->PrimaryComponentTick;
	}
	return bTreeAfterDirector && bControllerAfterTree && PrimaryActorTick.TickGroup == TG_PrePhysics && CrowdStateTree->PrimaryComponentTick.TickGroup == TG_PrePhysics;
}

void AStealthCrowdController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	// The pending driver configuration (ADR-0016 section 7): the Director decides Full or Restricted from the ICrowdControl
	// call that registered it, never from a binding.
	if (UCrowdDirector* Found = UCrowdDirector::Get(GetWorld()))
	{
		Found->AttachDriver(this, Cast<AStealthPawn>(InPawn));
	}
}

void AStealthCrowdController::OnUnPossess()
{
	if (UCrowdDirector* Found = Director.Get())
	{
		Found->DetachDriver(this);
	}
	bDriverBound = false;
	bTreeRunning = false;
	if (CrowdStateTree != nullptr && CrowdStateTree->IsRunning())
	{
		CrowdStateTree->StopLogic(TEXT("UnPossess"));
	}
	Super::OnUnPossess();
}

void AStealthCrowdController::BindDriver(AStealthPawn* InPawn, ECrowdDriverKind Kind, uint32 InOrdinal, uint32 ControlEpoch, UCrowdDirector* InDirector)
{
	Director = InDirector;
	DriverKind = Kind;
	Ordinal = InOrdinal;
	FullSink = FCrowdCommandSink(Kind == ECrowdDriverKind::Full ? InPawn : nullptr, ControlEpoch);
	MotionSink = FCrowdMotionSink(Kind == ECrowdDriverKind::Restricted ? InPawn : nullptr, ControlEpoch);
	bDriverBound = true;
	LastLook = InPawn != nullptr ? FRotator(0.0, InPawn->GetActorRotation().Yaw, 0.0) : FRotator::ZeroRotator;
}

void AStealthCrowdController::StartBehaviour(const FCrowdFirstActivity& First, double Now, double GridOrigin, double Period, UStateTree* Tree)
{
	UCrowdDirector* Found = Director.Get();
	if (Found == nullptr || !bDriverBound)
	{
		return;
	}
	FallbackGridOrigin = GridOrigin;
	FallbackPeriod = Period;
	NextFallbackDecision = GridOrigin;
	CrowdStateTree->SetDecisionGrid(GridOrigin, Period);
	if (Tree != nullptr)
	{
		CrowdStateTree->SetCrowdStateTree(Tree);
	}
	Agent.Start(First, Now, Found->GetSelfFacts(*this), *Found, *this);
	if (Tree != nullptr)
	{
		CrowdStateTree->StartLogic();
		bTreeRunning = CrowdStateTree->IsRunning();
	}
}

void AStealthCrowdController::HaltBehaviour()
{
	FullSink.SetHalted(true);
	MotionSink.SetHalted(true);
	Agent.Halt(*this);
	if (CrowdStateTree != nullptr && CrowdStateTree->IsRunning())
	{
		CrowdStateTree->StopLogic(TEXT("Halt"));
	}
	bTreeRunning = false;
}

void AStealthCrowdController::ReleaseForTakeover()
{
	FullSink.SetHalted(true);
	MotionSink.SetHalted(true);
	if (CrowdStateTree != nullptr && CrowdStateTree->IsRunning())
	{
		CrowdStateTree->StopLogic(TEXT("ReleasedForTakeover"));
	}
	bTreeRunning = false;
	bReleasedForTakeover = true;
}

void AStealthCrowdController::ResumeAfterReversal(uint32 Epoch, UStateTree* Tree)
{
	AStealthPawn* Body = GetDrivenPawn();
	FullSink = FCrowdCommandSink(DriverKind == ECrowdDriverKind::Full ? Body : nullptr, Epoch);
	MotionSink = FCrowdMotionSink(DriverKind == ECrowdDriverKind::Restricted ? Body : nullptr, Epoch);
	bReleasedForTakeover = false;
	if (Tree != nullptr && CrowdStateTree != nullptr)
	{
		// From its current activity: the agent kept its state; the tree re-enters the mirrored state.
		CrowdStateTree->SetCrowdStateTree(Tree);
		CrowdStateTree->StartLogic();
		bTreeRunning = CrowdStateTree->IsRunning();
	}
}

void AStealthCrowdController::PromoteToFull(UStateTree* Tree)
{
	AStealthPawn* Body = GetDrivenPawn();
	const uint32 Epoch = MotionSink.GetControlEpoch();
	DriverKind = ECrowdDriverKind::Full;
	FullSink = FCrowdCommandSink(Body, Epoch);
	MotionSink = FCrowdMotionSink(nullptr, Epoch);
	if (Tree != nullptr && CrowdStateTree != nullptr)
	{
		if (CrowdStateTree->IsRunning())
		{
			CrowdStateTree->StopLogic(TEXT("Promoted"));
		}
		CrowdStateTree->SetCrowdStateTree(Tree);
		CrowdStateTree->StartLogic();
		bTreeRunning = CrowdStateTree->IsRunning();
	}
}

bool AStealthCrowdController::Move(FVector2D Direction, EGait Gait)
{
	CROWD_COST_SCOPE(Dispatch);
	return DriverKind == ECrowdDriverKind::Full ? FullSink.Move(Direction, Gait) : MotionSink.Move(Direction, Gait);
}

bool AStealthCrowdController::Look(FRotator Rotation)
{
	CROWD_COST_SCOPE(Dispatch);
	return DriverKind == ECrowdDriverKind::Full ? FullSink.Look(Rotation) : MotionSink.Look(Rotation);
}

// The discrete commands exist only on the full driver's sink: a restricted driver refuses every one (CNB-R10.1).
bool AStealthCrowdController::DryRun(EActionType Type, const FCourtFixtureId* Target)
{
	return DriverKind == ECrowdDriverKind::Full && FullSink.DryRun(Type, Target);
}

bool AStealthCrowdController::Traverse(const FTraversalHint& Hint)
{
	return DriverKind == ECrowdDriverKind::Full && FullSink.Traverse(Hint);
}

bool AStealthCrowdController::InteractBegin(FCourtFixtureId Target)
{
	return DriverKind == ECrowdDriverKind::Full && FullSink.InteractBegin(Target);
}

bool AStealthCrowdController::InteractEnd()
{
	return DriverKind == ECrowdDriverKind::Full && FullSink.InteractEnd();
}

bool AStealthCrowdController::PrepareBegin()
{
	return DriverKind == ECrowdDriverKind::Full && FullSink.PrepareBegin();
}

bool AStealthCrowdController::PrepareEnd()
{
	return DriverKind == ECrowdDriverKind::Full && FullSink.PrepareEnd();
}

bool AStealthCrowdController::Hide(const FHideCommand& Command)
{
	return DriverKind == ECrowdDriverKind::Full && FullSink.Hide(Command);
}

const FCrowdSinkCounters& AStealthCrowdController::GetSinkCounters() const
{
	return DriverKind == ECrowdDriverKind::Full ? FullSink.GetCounters() : MotionSink.GetCounters();
}

void AStealthCrowdController::DecideFromTree()
{
	if (UCrowdDirector* Found = Director.Get())
	{
		Found->NoteDecision();
		Agent.Decide(Found->GetWorld()->GetTimeSeconds(), Found->GetSelfFacts(*this), *Found, *this);
		Agent.ClearWake();
	}
}

void AStealthCrowdController::OnTreeStateEntered(uint8 TreeActivity)
{
	if (UCrowdDirector* Found = Director.Get())
	{
		Found->ReportActivity(Ordinal, static_cast<ECrowdActivity>(TreeActivity));
	}
}

void AStealthCrowdController::Tick(float DeltaTime)
{
	CROWD_COST_SCOPE(Follower);
	UCrowdDirector* Found = Director.Get();
	if (Found != nullptr && bDriverBound && Agent.IsStarted() && !bReleasedForTakeover)
	{
		const double Now = GetWorld()->GetTimeSeconds();
		// The due-time list and the follower step BEFORE Super::Tick, so UpdateControlRotation writes this frame's Look.
		Agent.Tick(Now, DeltaTime, Found->GetSelfFacts(*this), *Found, *this);
		if (bTreeRunning && CrowdStateTree->IsRunning())
		{
			if (Agent.WantsWake())
			{
				CrowdStateTree->WakeNextFrame();
			}
		}
		else if (Agent.WantsWake() || Now >= NextFallbackDecision)
		{
			// Fallback (ADR-0016 section 3): no crowd tree runs, the controller keeps the same grid itself.
			++FallbackDecisions;
			Found->NoteDecision();
			Agent.Decide(Now, Found->GetSelfFacts(*this), *Found, *this);
			Agent.ClearWake();
			if (Now >= NextFallbackDecision)
			{
				const double K = FMath::FloorToDouble((Now - FallbackGridOrigin) / FallbackPeriod) + 1.0;
				NextFallbackDecision = FallbackGridOrigin + K * FallbackPeriod;
			}
		}
	}
	Super::Tick(DeltaTime);
}

void AStealthCrowdController::SetControlRotation(const FRotator& NewRotation)
{
	LastLook = NewRotation;
	bHasLook = true;
	Super::SetControlRotation(NewRotation);
}

void AStealthCrowdController::UpdateControlRotation(float DeltaTime, bool bUpdatePawn)
{
	// E1: the stock AAIController version zeroes pitch unless the focus is a pawn, and turns the pawn with FaceRotation.
	// The crowd writes the last Look, yaw AND pitch, and nothing else: no focal point, no FaceRotation.
	if (bHasLook)
	{
		Super::SetControlRotation(LastLook);
	}
}
