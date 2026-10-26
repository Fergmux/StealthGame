#pragma once

#include "StealthServerOnlyGuard.h"

#include "AIController.h"
#include "CoreMinimal.h"
#include "Server/CrowdAgent.h"
#include "Server/CrowdSink.h"

#include "StealthCrowdController.generated.h"

class AStealthPawn;
class UCrowdDirector;
class UCrowdStateTreeComponent;
class UStateTree;

/**
 * AStealthCrowdController: the one AAIController class of every AI-driven pawn: NPCs, participant-bound pawns and
 * PawnLeavingGame holds (ADR-0016 sections 1, 2, 4.4; ADR-0010 names it; story cnb-006).
 *   - No PathFollowingComponent (DoNotCreateDefaultSubobject, E3): nothing drives the pawn by path following.
 *   - bWantsPlayerState and bStartAILogicOnPossess false (engine defaults, asserted).
 *   - UpdateControlRotation is overridden to write the last Look (yaw AND pitch) into the control rotation: no focal
 *     point, no FaceRotation (E1: the stock version zeroes pitch unless the focus is a pawn). This is the only place
 *     crowd code writes the control rotation; the pawn's per-tick head filter consumes it exactly as a human's.
 *   - Tick steps the agent (due-time list, follower, virtual input; Move and Look through the sink) BEFORE Super::Tick,
 *     so UpdateControlRotation writes this frame's Look.
 *   - Tick prerequisites: the Director's tick -> this controller's StateTree component -> this controller -> the pawn's
 *     CMC (AController::AddPawnTickDependency); VerifyTickOrder asserts them.
 *   - Possession is ADR-0010's: this class never calls Possess. OnPossess asks the Director for the pending driver
 *     configuration (Full for an NPC, Restricted when Round registered DriveBoundPawn).
 */
UCLASS(NotBlueprintType, NotBlueprintable)
class STEALTHGAMEPLAY_API AStealthCrowdController : public AAIController, public ICrowdAgentOutput
{
	GENERATED_BODY()

public:
	AStealthCrowdController(const FObjectInitializer& ObjectInitializer);

	virtual void Tick(float DeltaTime) override;
	virtual void UpdateControlRotation(float DeltaTime, bool bUpdatePawn = true) override;
	virtual void SetControlRotation(const FRotator& NewRotation) override;

	// ICrowdAgentOutput (the sink of the current driver).
	virtual bool Move(FVector2D Direction, EGait Gait) override;
	virtual bool Look(FRotator Rotation) override;

	/** Binds the agent and the sink of a driver (called by the Director in OnPossess / adoption). */
	void BindDriver(AStealthPawn* InPawn, ECrowdDriverKind Kind, uint32 Ordinal, uint32 ControlEpoch, UCrowdDirector* InDirector);

	/** The Director's start (OnRoundActive or a join): starts the tree on its grid and the agent's first activity. */
	void StartBehaviour(const FCrowdFirstActivity& First, double Now, double GridOrigin, double Period, UStateTree* Tree);

	/** Halt: latches the sink, clears Move, stops the tree directly (never by an event alone). */
	void HaltBehaviour();

	/** A decision from the tree's activity task (the grid tick or a wake). */
	void DecideFromTree();
	/** A state's enter task: the activity mirror. */
	void OnTreeStateEntered(uint8 TreeActivity);
	/** The agent's activity as the tree's enum value. */
	uint8 GetTreeActivity() const { return static_cast<uint8>(Agent.GetActivity()); }

	FCrowdAgent& GetAgent() { return Agent; }
	const FCrowdAgent& GetAgent() const { return Agent; }
	UCrowdStateTreeComponent* GetCrowdStateTree() const { return CrowdStateTree; }
	AStealthPawn* GetDrivenPawn() const;
	uint32 GetOrdinal() const { return Ordinal; }
	ECrowdDriverKind GetDriverKind() const { return DriverKind; }
	/** The current sink's counters (StaleEpochDrop, HaltDrop). */
	const FCrowdSinkCounters& GetSinkCounters() const;
	/** The last Look written into the control rotation. */
	FRotator GetLastLook() const { return LastLook; }

	/** True when the tick prerequisites hold: Director -> StateTree component -> this controller. */
	bool VerifyTickOrder() const;

	/** Decisions made without the tree (the documented fallback when no crowd tree asset loads). */
	int64 GetFallbackDecisions() const { return FallbackDecisions; }

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	/** Director -> this controller's StateTree component -> this controller (idempotent; at spawn and at BeginPlay). */
	void WireTickOrder();
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY()
	TObjectPtr<UCrowdStateTreeComponent> CrowdStateTree;

	TWeakObjectPtr<UCrowdDirector> Director;
	FCrowdAgent Agent;
	FCrowdCommandSink FullSink;
	FCrowdMotionSink MotionSink;
	ECrowdDriverKind DriverKind = ECrowdDriverKind::Full;
	uint32 Ordinal = MAX_uint32;
	bool bDriverBound = false;
	bool bTreeRunning = false;
	FRotator LastLook = FRotator::ZeroRotator;
	bool bHasLook = false;
	double FallbackGridOrigin = 0.0;
	double FallbackPeriod = 0.25;
	double NextFallbackDecision = 0.0;
	int64 FallbackDecisions = 0;
};
