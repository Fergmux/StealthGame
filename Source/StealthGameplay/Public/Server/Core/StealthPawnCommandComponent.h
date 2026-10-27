#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Server/Core/PawnCommandCore.h"
#include "Server/Core/PawnCommandTypes.h"
#include "View/CourtFixtureId.h"
#include "View/Core/StealthGait.h"
#include "View/StealthEncounterWire.h"

#include "StealthPawnCommandComponent.generated.h"

/**
 * One FActionState write as its caller decides it (ADR-0011 section 7, ADR-0018 section 1.1): everything except Seq,
 * which UPawnCommandComponent::WriteActionState assigns itself. StartServerTime < 0 means "now" (the writer fills it).
 */
struct FActionStateWrite
{
	EActionPose Pose = EActionPose::None;
	/** Strike: StrikeVariant bits; 0 for every other pose. */
	uint8 Variant = 0;
	double StartServerTime = -1.0;
	/** Strike and Counter: the strike's contact time; -1 otherwise. */
	double ContactServerTime = -1.0;
	/** Strike: the quantised warp offset; zero otherwise. */
	FFixedWarpOffset WarpPoint;
};

/**
 * The boundary of the pawn command interface (ADR-0011 section 1; stories iif-002 and iif-005): the one
 * non-replicated component every AStealthPawn carries, created in the pawn's constructor. Its entry points are the
 * same functions for every source: the crowd AI calls them on the server, and a human's commands reach them through
 * the controller transport and stage-1 ingress (which opens an FScopedCommandOrigin around the call). Discrete entry
 * points execute nothing: they build an FCommandPayload and submit it through the plain FPawnCommandCore.
 *
 * Entry-point set decision (story iif-005, resolving the "listed by reflection" defect of GDD AC 1): every entry point
 * is a UFUNCTION() with no specifiers (so none is an RPC and reflection lists exactly these nine): Move, Look,
 * Traverse, InteractBegin, InteractEnd, PrepareBegin, PrepareEnd, Attack, Hide. Hide covers HideEnter and HideExit
 * through FHideCommand. Every other public function is a plain C++ method.
 *
 * No parameter, payload or member names a controller, a PlayerState or a team, and nothing here asks whether a player
 * or an AI drives the pawn (IIF R2; StealthGame.Input.NoCallerIdentityParameter).
 */
UCLASS(NotBlueprintType, NotBlueprintable, ClassGroup = Stealth)
class STEALTHGAMEPLAY_API UPawnCommandComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPawnCommandComponent();

	/** The default-subobject name on AStealthPawn. */
	static const FName ComponentName;

	// ---- Entry points (ADR-0011 section 1) -----------------------------------------------------------------

	/**
	 * Continuous: sets the Move intent (Direction in input space, X right and Y forward, magnitude <= 1, relative to the
	 * control yaw) re-applied every movement tick, so a source that calls it every frame (a held key) and one that calls
	 * it on change (the crowd AI) feed the movement component the same per-tick input. Never a queue item.
	 */
	UFUNCTION()
	void Move(FVector2D Direction, EGait Gait);

	/** Continuous: writes the pawn's control rotation (the AI's look path; a human's look rides ServerMove). Never a queue item. */
	UFUNCTION()
	void Look(FRotator Rotation);

	/** Discrete: submits a Traverse command. */
	UFUNCTION()
	void Traverse(const FTraversalHint& Hint);

	/** Held: submits InteractBegin for a court fixture. */
	UFUNCTION()
	void InteractBegin(FCourtFixtureId Target);

	/** Held: submits InteractEnd. */
	UFUNCTION()
	void InteractEnd();

	/** Held: submits PrepareBegin (the brace). */
	UFUNCTION()
	void PrepareBegin();

	/** Held: submits PrepareEnd. */
	UFUNCTION()
	void PrepareEnd();

	/** Instant: submits an Attack at the payload's world point (optional weak PawnRef). */
	UFUNCTION()
	void Attack(const FAttackPayload& Payload);

	/** Instant: submits HideEnter or HideExit. */
	UFUNCTION()
	void Hide(const FHideCommand& Cmd);

	// ---- Plain C++ (not entry points) ---------------------------------------------------------------------

	/** Binds the core's seams (composition root; tests). */
	void InitializeCore(const FPawnCommandCoreDeps& Deps) { Core.Initialize(Deps); }

	/**
	 * Server: the only writer of the owning pawn's FActionState (ADR-0011 section 7, ADR-0018 section 1.1; story asn-012).
	 * Assigns Seq (FActionState::NextSeq of the current one), fills StartServerTime (now when the write leaves it unset),
	 * writes the struct inside FStealthForcedNetUpdate(ActionStateWrite), and presents it on the server at once (the SCL
	 * pose, the local strike warp target; ADR-0018 E7). The same call for every pawn and every outcome; no-op off the
	 * server. Returns the state written (the current one when nothing was written).
	 */
	FActionState WriteActionState(const FActionStateWrite& Write);

	/** The plain core (read-only). */
	const FPawnCommandCore& GetCore() const { return Core; }

	/** The current Move intent and gait (the movement adapter and tests read them). */
	FVector2D GetMoveIntent() const { return MoveIntent; }
	EGait GetGaitIntent() const { return GaitIntent; }

	/** The world-space movement input the intent produces this frame (control yaw applied, magnitude kept). */
	FVector GetWorldMoveInput() const;

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	friend struct FScopedCommandOrigin;

	/** Builds the payload common to every discrete command (pawn handle, control epoch, sequence of the open origin). */
	FCommandPayload MakePayload() const;

	/** Submits through the core with the open origin (server-local when none is open). */
	void SubmitKind(ECommandKind Kind, const FCommandPayload& Payload);

	FPawnCommandCore Core;
	FVector2D MoveIntent = FVector2D::ZeroVector;
	EGait GaitIntent = EGait::Walk;

	/** The origin opened by ingress around one entry-point call (null otherwise). */
	const struct FScopedCommandOrigin* OpenOrigin = nullptr;
};

/**
 * Attaches an opaque origin to the entry-point calls made inside its scope (stage-1 ingress uses it for a human's
 * command; the AI never opens one, so its commands carry the server-local mailbox and the null sink). The origin is an
 * intake mailbox, a reply sink, the controller's recorded control epoch and the client sequence: no identity.
 */
struct STEALTHGAMEPLAY_API FScopedCommandOrigin
{
	FScopedCommandOrigin(UPawnCommandComponent& InComponent, const FCommandOrigin& InOrigin, const FClientCommandSeq& InSeq, uint32 InControlEpoch);
	~FScopedCommandOrigin();

	FScopedCommandOrigin(const FScopedCommandOrigin&) = delete;
	FScopedCommandOrigin& operator=(const FScopedCommandOrigin&) = delete;

	UPawnCommandComponent& Component;
	FCommandOrigin Origin;
	FClientCommandSeq Seq;
	uint32 ControlEpoch = 0;

private:
	const FScopedCommandOrigin* Previous = nullptr;
};
