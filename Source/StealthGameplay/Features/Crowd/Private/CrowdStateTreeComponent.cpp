#include "Server/CrowdStateTreeComponent.h"

#include "Engine/World.h"
#include "Server/CrowdDirector.h"
#include "StateTreeExecutionTypes.h"

namespace CrowdStateTreeComponentLocal
{
	/** A tree tick within this of a grid point counts as on the grid (one frame at 60 Hz is 16.7 ms). */
	constexpr double OnGridToleranceSec = 0.02;
	/** The smallest interval handed to the tick manager (it cannot schedule a zero interval). */
	constexpr double MinIntervalSec = 0.001;
	/** A grid tick may fire this much before its grid point (float cooldown arithmetic in the tick manager). */
	constexpr double EarlyToleranceSec = 0.0005;
}

UCrowdStateTreeComponent::UCrowdStateTreeComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// E14: brain components tick in TG_DuringPhysics unless moved; the crowd decides before the controller and the CMC.
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	// Created stopped: the Director starts every tree at OnRoundActive (CNB-R12).
	bStartLogicAutomatically = false;
	SetIsReplicatedByDefault(false);
}

void UCrowdStateTreeComponent::SetCrowdStateTree(UStateTree* Tree)
{
	SetStateTree(Tree);
}

void UCrowdStateTreeComponent::SetDecisionGrid(double InGridOrigin, double InPeriod)
{
	GridOrigin = InGridOrigin;
	Period = FMath::Max(InPeriod, 0.01);
	bHasGrid = true;
}

double UCrowdStateTreeComponent::NextGridPoint(double Now) const
{
	if (Now < GridOrigin)
	{
		return GridOrigin;
	}
	const double K = FMath::FloorToDouble((Now - GridOrigin) / Period) + 1.0;
	return GridOrigin + K * Period;
}

void UCrowdStateTreeComponent::StartLogic()
{
	Super::StartLogic();
	ImposeGrid();
}

void UCrowdStateTreeComponent::WakeNextFrame()
{
	bWakePending = true;
	ScheduleTickFrame(FStateTreeScheduledTick::MakeNextFrame());
}

void UCrowdStateTreeComponent::SendCrowdEvent(FGameplayTag Tag)
{
	SendStateTreeEvent(Tag);
	WakeNextFrame();
}

void UCrowdStateTreeComponent::ImposeGrid()
{
	if (!bHasGrid || !IsRunning())
	{
		return;
	}
	const UWorld* World = GetWorld();
	const double Now = World != nullptr ? World->GetTimeSeconds() : 0.0;
	const double Delay = FMath::Max(NextGridPoint(Now) - Now, CrowdStateTreeComponentLocal::MinIntervalSec);
	ScheduleTickFrame(FStateTreeScheduledTick::MakeCustomTickRate(static_cast<float>(Delay)));
}

void UCrowdStateTreeComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	const UWorld* World = GetWorld();
	const double Now = World != nullptr ? World->GetTimeSeconds() : 0.0;
	if (bHasGrid)
	{
		// One frame of lateness is on the grid: the tick manager fires a cooldown on the first frame that crosses it.
		const double FrameSec = World != nullptr ? static_cast<double>(World->GetDeltaSeconds()) : 0.0;
		const double Tolerance = FMath::Max(CrowdStateTreeComponentLocal::OnGridToleranceSec, FrameSec * 1.05);
		// The grid point this tick serves: the latest one at or before Now (a tick may land a hair early: the tick
		// manager's cooldowns are floats). Before the first grid point there is none, so the tick is off the grid.
		const double Previous = NextGridPoint(Now - Period);
		const double Late = Now - Previous;
		bLastTickOnGrid = Late >= -CrowdStateTreeComponentLocal::EarlyToleranceSec && Late <= Tolerance;
		if (!bLastTickOnGrid && !bWakePending && IsRunning())
		{
			// An off-grid tick nobody asked for (the first tick after StartLogic enabled the component): no decision.
			++SkippedOffGridTicks;
			ImposeGrid();
			return;
		}
		if (!bLastTickOnGrid && bWakePending && IsRunning())
		{
			// A wake or event: one off-grid decision within the Director's per-frame slack, else it waits a frame.
			UCrowdDirector* Director = UCrowdDirector::Get(World);
			if (Director != nullptr && !Director->AdmitWakeDecision())
			{
				ScheduleTickFrame(FStateTreeScheduledTick::MakeNextFrame());
				return;
			}
		}
	}
	++TreeTicks;
	LastTickFrame = GFrameCounter;
	bLastTickWake = bWakePending;
	WakeTicks += bWakePending ? 1 : 0;
	bWakePending = false;
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	// E12: the component overwrites its own schedule after every tree tick; re-impose the grid (the crowd tasks are not
	// considered for scheduling, so a "sleep" from the tree means no engine reason to tick, and the grid governs).
	ImposeGrid();
}
