#pragma once

#include "StealthServerOnlyGuard.h"

#include "Components/StateTreeAIComponent.h"
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "CrowdStateTreeComponent.generated.h"

/**
 * UCrowdStateTreeComponent: the crowd's StateTree component (ADR-0016 section 3; engine facts E5, E6, E12, E14; story
 * cnb-012). A UStateTreeAIComponent subclass so it can call the protected ScheduleTickFrame and override TickComponent:
 * it ticks in TG_PrePhysics (after the Director, before the controller) and, after every tree tick, re-imposes the
 * pawn's phase-aligned decision grid t = Phase + k / CrowdDecisionPerSec with
 * ScheduleTickFrame(MakeCustomTickRate(NextGridPoint - Now)), unless the tree asked to tick every frame. An event or a
 * due-time wake costs one off-grid tick (MakeNextFrame) and the pawn returns to its grid; any other off-grid tick the
 * engine runs (the first tick after StartLogic enables the component, which would put every tree on one frame) is skipped
 * without a tree tick. "On the grid" allows one frame of lateness (the tick manager fires a cooldown on the frame that
 * crosses it). Created stopped; the Director
 * starts it at OnRoundActive and stops it directly at Halt (never by an event alone, E15).
 */
UCLASS(NotBlueprintType, NotBlueprintable, ClassGroup = Stealth)
class STEALTHGAMEPLAY_API UCrowdStateTreeComponent : public UStateTreeAIComponent
{
	GENERATED_BODY()

public:
	UCrowdStateTreeComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void StartLogic() override;

	/** The grid this component keeps: origin (round start + phase) and period. */
	void SetDecisionGrid(double InGridOrigin, double InPeriod);

	/** Asks for one tick next frame (an event or a due-time wake), then back to the grid. */
	void WakeNextFrame();

	/** Sends a crowd StateTree event and wakes the tree next frame for it (the one off-grid tick an event costs). */
	void SendCrowdEvent(FGameplayTag Tag);

	/** Off-grid engine ticks skipped (the first tick after StartLogic, engine-requested ticks): no decision runs. */
	int64 GetSkippedOffGridTicks() const { return SkippedOffGridTicks; }

	/** The next grid point strictly after Now. */
	double NextGridPoint(double Now) const;

	/** Tree ticks this component ran (tests count decisions per frame). */
	int64 GetTreeTickCount() const { return TreeTicks; }
	/** Frame number of the last tree tick. */
	uint64 GetLastTickFrame() const { return LastTickFrame; }
	/** True when the last tree tick was on the grid (not a wake). */
	bool WasLastTickOnGrid() const { return bLastTickOnGrid; }
	/** True when the last tree tick ran for a wake or an event (WakeNextFrame, SendCrowdEvent). */
	bool WasLastTickWake() const { return bLastTickWake; }
	/** Tree ticks that ran for a wake or an event. */
	int64 GetWakeTickCount() const { return WakeTicks; }

	/** Uses StateTree (no StartLogic). */
	void SetCrowdStateTree(UStateTree* Tree);

private:
	void ImposeGrid();

	double GridOrigin = 0.0;
	double Period = 0.25;
	bool bHasGrid = false;
	bool bWakePending = false;
	int64 TreeTicks = 0;
	int64 SkippedOffGridTicks = 0;
	int64 WakeTicks = 0;
	bool bLastTickWake = false;
	uint64 LastTickFrame = 0;
	bool bLastTickOnGrid = false;
};
