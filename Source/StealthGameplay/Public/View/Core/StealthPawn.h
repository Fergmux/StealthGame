#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "View/Core/StealthLocomotionModel.h"
#include "View/StealthEncounterWire.h"
#include "StealthPawn.generated.h"

class AStealthPawn;
class UGameplayTasksComponent;
class UMotionWarpingComponent;
class UPawnCommandComponent;
class UStealthMotionWarpingComponent;
class UStealthMovementComponent;

/** The reasons a pawn's net update may be forced (ADR-0003 section 2; every other force is the 15 Hz cadence's). */
enum class EForcedUpdateReason : uint8
{
	ActionStateWrite,
	TraversalStateWrite,
	DeathWrite,
};

/**
 * The scoped forced net update of a pawn (ADR-0003 section 2, E8, E16; ADR-0018 sections 1.1 and 3.2): the only way a
 * gameplay write forces a pawn's replication, at most once per pawn per frame (a second force in the same frame is
 * dropped: the first already sends this frame's state). Constructed by UPawnCommandComponent::WriteActionState
 * (ActionStateWrite) and AStealthPawn::ApplyDeathWrite (DeathWrite).
 *
 * Deviation (stories asn-012/013): the AStealthPawn::ForceNetUpdate override that drops every unscoped caller is IIF
 * story 069's; until it lands, unscoped engine and prototype callers still force.
 */
struct STEALTHGAMEPLAY_API FStealthForcedNetUpdate
{
	explicit FStealthForcedNetUpdate(AStealthPawn& Pawn, EForcedUpdateReason Reason);
};

#if WITH_STEALTH_TEST_SEAMS
namespace StealthPawnTestSeams
{
	/** Story asn-014 wire check: machines without authority skip the local strike warp target (so a target found there came over the wire). */
	extern STEALTHGAMEPLAY_API bool bSkipRemoteStrikeWarpTarget;
}
#endif

/**
 * The Shared Pawn (ADR-0010 section 2; SCL R1 One class, R3 Possession parity). Every body in the
 * city, NPC or player, is one AStealthPawn on one movement component class with the same default
 * subobjects. Nothing here reads the controller kind, the PlayerState or Roster state (ADR-0001);
 * the class parity scan (StealthGame.Locomotion.ClassParity) enforces that on this file.
 *
 * Header placement: Public/View/Core/ (client-safe), per the ADR-0010 Key Interfaces amendment
 * (architecture review 2026-10-25, Conflict 4). Pawns are created only by UStealthPawnFactory
 * (story SCL-006); nothing may add a component to a pawn at run time.
 */
UCLASS()
class STEALTHGAMEPLAY_API AStealthPawn : public ACharacter
{
	GENERATED_BODY()

public:
	/** Sets UStealthMovementComponent as the movement subobject and builds the fixed subobject set. */
	AStealthPawn(const FObjectInitializer& ObjectInitializer);

	/**
	 * The body constants of every pawn, read from the class default object (R17.1): capsule radius and half-height
	 * (the reparented sample character's values, recorded in these class defaults), MaxStepHeight and the walkable
	 * floor angle. The Level Socket Contract and the Generator consume this (CR-4).
	 */
	static FStealthBodyConstants GetPublishedBodyConstants();

	/** Capsule radius recorded from the Game Animation Sample character (R17.1, measured in the 5.8.4 project). */
	static constexpr float SampleCapsuleRadiusCm = 30.0f;
	/** Capsule half-height recorded from the Game Animation Sample character (R17.1, measured in the 5.8.4 project). */
	static constexpr float SampleCapsuleHalfHeightCm = 86.0f;

	/** Name of the non-replicated gameplay tasks subobject (also found by AAIController on possession). */
	static const FName GameplayTasksComponentName;

	/** Name of the motion-warping subobject. */
	static const FName MotionWarpingComponentName;

	/**
	 * The pawn command interface boundary (ADR-0011 section 1): exactly one non-replicated UPawnCommandComponent per
	 * pawn, created here. Forward-declared only: this View header never includes the Server component header.
	 */
	UPawnCommandComponent* GetPawnCommand() const;

	/**
	 * Recalculates the base eye height only (ADR-0010 E2). Deliberately skips ACharacter::Restart and
	 * APawn::Restart, which stop movement, consume pending input and reset the movement mode: a
	 * possession change must not be visible in the body's motion.
	 */
	virtual void Restart() override;

	/**
	 * Resets client prediction data, then runs APawn::PawnClientRestart directly, skipping
	 * ACharacter's StopMovementImmediately so the owner's velocity is kept across a possession.
	 */
	virtual void PawnClientRestart() override;

	/** No-op: the factory possesses every pawn; no engine path may spawn a controller for one. */
	virtual void SpawnDefaultController() override;

	/** The Shared Pawn's movement component (never null on a constructed pawn). */
	UStealthMovementComponent* GetStealthMovement() const;

	/** The non-replicated gameplay tasks component every pawn carries. */
	UGameplayTasksComponent* GetGameplayTasks() const { return GameplayTasks; }

	/**
	 * The motion-warping component (WarpTargets never replicate). Null on the D53 prototype adapter pawns, which use the
	 * sample Blueprint's own component until ADR-0010's Blueprint cleanup: callers must null-check.
	 */
	UStealthMotionWarpingComponent* GetMotionWarping() const { return SharedMotionWarping; }

	/**
	 * The motion warping component the pawn's root-motion clips warp with: the Shared Pawn's own one, else the sample
	 * Blueprint's (the D53 prototype adapter, whose WarpTargets replication PostInitializeComponents turns off).
	 */
	UMotionWarpingComponent* GetWarpingForStrike() const;

	/** True when this machine's strike warping component holds a StrikeContact target (story asn-014 audit). */
	bool HasStrikeWarpTarget() const;

	/** Motion-warping components on this pawn that could send WarpTargets (neither the Shared class nor unreplicated); 0 expected. */
	int32 CountWarpingThatCanReplicateTargets() const;

	// ---- Encounter state (ADR-0018 sections 1.1, 1.2; stories asn-012, asn-013) ----------------------------------

	/** The local warp target a Strike's root-motion clip warps to (set on every machine from FActionState, never replicated). */
	static const FName StrikeWarpTargetName;

	/** The outcome-blind action state (audience All). Written only by UPawnCommandComponent::WriteActionState. */
	const FActionState& GetActionState() const { return ActionState; }

	/** The death state (audience All). Written once, by the death write (ApplyDeathWrite). */
	const FDeathState& GetDeathState() const { return DeathState; }

	/** True once the death write reached this pawn (DeathServerTime set). */
	bool HasDeathWrite() const { return DeathState.DeathServerTime >= 0.0; }

	/**
	 * Server: the pawn half of an applied DeathWrite (ADR-0018 section 3.2 steps 1-2, story asn-013): sets FDeathState
	 * inside FStealthForcedNetUpdate(DeathWrite) and records the movement freeze. Applied once (a second write is ignored
	 * and returns false). Called only by the Assassination death-write applier; decides nothing.
	 */
	bool ApplyDeathWrite(const FDeathState& State);

	/**
	 * The server time the death freeze takes effect for this pawn now: its own ContactServerTime while its Strike's lunge
	 * is still before contact (the reversed attacker: the freeze never interrupts the lunge), else Now.
	 */
	double GetDeathFreezeServerTime(double Now) const;

	/** The SCL view of an FActionState pose (the two enums order their values differently). */
	static EStealthActionPose ToSclActionPose(EActionPose Pose);

	/** Forced net updates sent (observation for tests; at most one per frame). */
	uint32 GetForcedNetUpdateCount() const { return ForcedNetUpdateCount; }

	/** Fired on every machine after a new FActionState is presented (RepNotify on clients, direct on the server). */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnEncounterStateChanged, AStealthPawn* /*Pawn*/);
	FOnEncounterStateChanged OnActionStateChanged;
	/** Fired on every machine after the FDeathState write is presented. */
	FOnEncounterStateChanged OnDeathStateChanged;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	/** Turns off the WarpTargets replication of any non-Shared motion warping component (ADR-0018 section 7.2, story asn-014). */
	virtual void PostInitializeComponents() override;

protected:
	/**
	 * Presents the current FActionState on this machine (ADR-0018 E7: RepNotify on clients; the server write and the
	 * listen host call it directly): the SCL pose, and for a Strike the local warp target, set once per write at the
	 * pawn's location plus WarpPoint and never re-tracked. Client clip scheduling is story asn-018's (override here).
	 */
	virtual void HandleActionStateChanged();

	/** Presents the FDeathState on this machine: the movement freeze (the fall and linger are stories asn-019/020). */
	virtual void HandleDeathStateChanged();

	UFUNCTION()
	void OnRep_ActionState();

	UFUNCTION()
	void OnRep_DeathState();

private:
	friend class UPawnCommandComponent;
	friend struct FStealthForcedNetUpdate;

	/** ADR-0018 section 1.1: one ReplicatedUsing property, 208 bits for every value. */
	UPROPERTY(ReplicatedUsing = OnRep_ActionState)
	FActionState ActionState;

	/** ADR-0018 section 1.2: one ReplicatedUsing property, 65 bits for every value. */
	UPROPERTY(ReplicatedUsing = OnRep_DeathState)
	FDeathState DeathState;

	/** The Seq whose strike warp target this machine already set (re-set, never re-track). */
	uint16 StrikeWarpSeq = 0;
	uint64 LastForcedNetUpdateFrame = MAX_uint64;
	uint32 ForcedNetUpdateCount = 0;

private:
	/** Default subobject "PawnCommand" (ADR-0011): non-replicated, the same on every pawn. */
	UPROPERTY(VisibleAnywhere, Category = "Stealth|Pawn")
	TObjectPtr<UPawnCommandComponent> PawnCommand;

	/** Default subobject: AAIController adopts it on possession instead of adding one (ADR-0010). */
	UPROPERTY(VisibleAnywhere, Category = "Stealth|Pawn")
	TObjectPtr<UGameplayTasksComponent> GameplayTasks;

	/**
	 * Default subobject "MotionWarping": motion warping with WarpTargets replication disabled (ADR-0018 section 7.2).
	 * The C++ member is not named MotionWarping, and the subobject is optional, so the reparented sample Blueprint
	 * (which still carries its own SCS component and variable of that name until ADR-0010's Blueprint cleanup) compiles
	 * against this class: the D53 prototype adapter AStealthCharacter opts out of it (DoNotCreateDefaultSubobject).
	 */
	UPROPERTY(VisibleAnywhere, Category = "Stealth|Pawn")
	TObjectPtr<UStealthMotionWarpingComponent> SharedMotionWarping;

	/** Restart has given this body its first movement mode (see Restart). */
	bool bMovementModeInitialised = false;
};
