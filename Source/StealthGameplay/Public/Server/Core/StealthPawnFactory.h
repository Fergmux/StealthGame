#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Server/Roster/StealthRosterCallTypes.h"
#include "Server/Roster/StealthRosterCore.h"
#include "Server/StealthLogFaultSink.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/ObjectKey.h"
#include "View/StealthRosterTypes.h"

#include "StealthPawnFactory.generated.h"

class AController;
class AStealthPawn;

/** Who will drive the pawn (ADR-0010 section 3). */
enum class EStealthPawnControlKind : uint8
{
	Ai,
	Player,
};

/** One SpawnSharedPawn call (ADR-0010 3.1). */
struct FStealthSpawnRequest
{
	/** The Shared Pawn class (the reparented sample Blueprint in production); null uses the factory's default class. */
	TSubclassOf<AStealthPawn> PawnClass;
	FTransform Transform;
	/** The dealt appearance card (ADR-0012); INDEX_NONE for none yet. */
	int32 AppearanceId = INDEX_NONE;
	EStealthPawnControlKind ControlKind = EStealthPawnControlKind::Ai;
	/** Set for a participant's pawn: BindPawn runs before FinishSpawning. */
	FParticipantId Participant;
	TOptional<FRosterEpochToken> ExpectedEpoch;
	/** Player: the participant's controller, possessed after FinishSpawning. */
	AController* PlayerController = nullptr;
};

/** What SpawnSharedPawn produced. */
struct FStealthSpawnResult
{
	AStealthPawn* Pawn = nullptr;
	/** The BindPawn result (NoOp_NotBound when no participant was given). */
	ERosterResult Bind = ERosterResult::NoOp_NotBound;
};

/**
 * The factory's calls into other systems' facades (ADR-0010 3.1 steps 2-3 and the AI controller), registered by the
 * composition root; tests stub them with the same interface.
 */
class IStealthPawnFactoryHooks
{
public:
	virtual ~IStealthPawnFactoryHooks() = default;

	/** Character Pool & Appearance: ApplyAppearance(AppearanceId) on the not-yet-finished pawn. */
	virtual void ApplyAppearance(AStealthPawn* Pawn, int32 AppearanceId) = 0;
	/** Participant State & Roster: BindPawn(Participant, Pawn) before the pawn is visible. */
	virtual ERosterResult BindPawn(FParticipantId Participant, AStealthPawn* Pawn, const TOptional<FRosterEpochToken>& ExpectedEpoch) = 0;
	/** Crowd: the AI controller that drives an Ai pawn (possessed by the factory). */
	virtual AController* CreateAiController(AStealthPawn* Pawn) = 0;
};

/**
 * UStealthPawnFactory: the ONLY code that spawns a Shared Pawn or possesses one (ADR-0010 section 3; SCL R2; story
 * SCL-006). Server-only world subsystem; every call checks authority and the game thread and reports misuse to the
 * fault sink. One call stack: deferred spawn, ApplyAppearance, BindPawn (if a participant), FinishSpawning, possess.
 * Every spawned pawn has Instigator == itself and carries a server-only creation token, so a debug guard (and
 * StealthGame.Locomotion.SingleSpawnSite) detects any pawn that came from elsewhere.
 *
 * Possession: FScopedPossessionParity (stories SCL-026/027) does not exist yet; the factory calls Possess directly and
 * AStealthPawn::Restart / PawnClientRestart already keep velocity and movement mode (SCL-001).
 */
UCLASS(NotBlueprintType, NotBlueprintable)
class STEALTHGAMEPLAY_API UStealthPawnFactory : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	/** The factory of World, or null. */
	static UStealthPawnFactory* Get(const UWorld* World);

	/** Registers the hooks (held weakly through Owner). */
	void SetHooks(IStealthPawnFactoryHooks* InHooks, UObject* Owner);
	/** The class used when a request names none. */
	void SetDefaultPawnClass(TSubclassOf<AStealthPawn> InClass) { DefaultPawnClass = InClass; }

	/**
	 * ADR-0010 3.1 steps 1-5. Null Pawn on refusal (no authority, wrong thread, no class, spawn collision) and on a rejected
	 * BindPawn: the pawn is then destroyed before FinishSpawning and Result.Bind carries the rejection code (story SCL-007).
	 */
	FStealthSpawnResult SpawnSharedPawn(const FStealthSpawnRequest& Request);

	/** True for a pawn this factory created (the server-only creation token). */
	bool HasCreationToken(const AActor* Pawn) const;
	/** Pawns created since the world began. */
	int32 GetSpawnCount() const { return SpawnCount; }
	/** Shared Pawns in the world without a creation token (the debug guard; 0 in a healthy round). */
	int32 CountUntokenedPawns() const;

	/**
	 * The pawn's server-only control epoch (ADR-0010 section 4.4; consumed by the Crowd sinks, ADR-0016 section 2, story
	 * cnb-005): bumped at every possession this factory performs and by BumpControlEpoch (the hand-over commit). 0 for a
	 * pawn the factory never possessed. Never replicated.
	 */
	uint32 GetControlEpoch(const AActor* Pawn) const;

	/** Bumps Pawn's control epoch (the hand-over commit inside the drain, ADR-0010 4.3) and returns the new value. */
	uint32 BumpControlEpoch(const AActor* Pawn);

	/**
	 * ReleaseBody (ADR-0010; Assassination story asn-019's body sequence, the ONLY path that lets a controller go of a
	 * killed pawn): bumps the pawn's control epoch, then its controller unpossesses it. A crowd (non-player) controller is
	 * destroyed; a PlayerController is left pawnless in its normal playing state, never Spectating, and
	 * AStealthPlayerController keeps its view target while IsReleasingBody() (Camera's Dying -> NoPawn fade owns the
	 * view). Returns the former controller (null when none held it). Minimal implementation until the SCL hand-over
	 * pass (ADR-0010 4.3) executes it apply-only; the caller commits it inside its queue slot.
	 */
	AController* ReleaseBody(APawn* Pawn);

	/** Body teardown (story asn-019 BodyRemoval and Reset): released first when still possessed, then destroyed. */
	void RequestTeardown(APawn* Pawn);

	/**
	 * RequestHandOver(Pawn, Ai) (ADR-0010 4.3; NPC-Kill Penalty story nkp-005, the exile's step 4): the commit runs now,
	 * inside the caller's queue slot: the control epoch is bumped and the current controller lets go exactly as in
	 * ReleaseBody (a PlayerController is left pawnless in its playing state with its view target kept, never Spectating),
	 * so no command of the former owner reaches the body from here. The AI possession (the Crowd hook's controller)
	 * executes at PossessNotBeforeServerTime, the end of the body's strike lockout (amendment request to ADR-0010: never
	 * during a root-motion action pose); a body that has a death write or a controller by then is left alone. STAND-IN
	 * for the SCL hand-over pass: the deferred possession is a world timer until the pass exists. Only Kind = Ai is
	 * supported (a human takeover is Penalty story nkp-012's); returns false for anything else or without authority.
	 */
	bool RequestHandOver(APawn* Pawn, EStealthPawnControlKind Kind, double PossessNotBeforeServerTime);

	/**
	 * RequestHandOver(Pawn, Player) for an informant takeover (ADR-0010 sections 3.2, 4.3; NPC-Kill Penalty story nkp-012,
	 * called by the takeover after BindPawn and Crowd's ReleaseControl): the commit runs now (the control epoch is bumped, so
	 * every command stamped under the crowd's control is stale), and the possession executes at the next frame start: the
	 * crowd controller lets go (ReleaseBody) and PlayerController possesses the same pawn (no actor is created, the look and
	 * velocity are unchanged). A pawn that has a death write or is being destroyed by then is left alone. STAND-IN for the
	 * SCL hand-over pass (a next-tick world timer until the pass exists). False without authority or a controller.
	 */
	bool RequestHandOverToPlayer(APawn* Pawn, AController* PlayerController);

	/** Hand-overs requested by RequestHandOver whose AI possession has not executed yet (tests and dumps). */
	int32 GetPendingHandOverCount() const { return PendingHandOvers; }

	/** True while ReleaseBody runs its UnPossess (the controller's OnUnPossess keeps the view target then). */
	static bool IsReleasingBody() { return ReleasingBodyDepth > 0; }

	/** No spawn ordinal (a pawn this factory did not spawn). */
	static constexpr uint32 NoSpawnOrdinal = MAX_uint32;

	/**
	 * The server-only spawn ordinal (ADR-0010, Crowd CNB-R14): 0, 1, 2 ... in spawn order for every pawn this factory
	 * spawned, participants included, so counter-based crowd draws key on it. NoSpawnOrdinal for an unknown pawn.
	 */
	uint32 GetSpawnOrdinal(const AActor* Pawn) const;

	/** The fault sink the factory reports misuse to. */
	const FStealthLogFaultSink& GetFaultSink() const { return FaultSink; }

private:
	IStealthPawnFactoryHooks* GetHooks() const;

	IStealthPawnFactoryHooks* Hooks = nullptr;
	TWeakObjectPtr<UObject> HooksOwner;
	TSubclassOf<AStealthPawn> DefaultPawnClass;
	TSet<FObjectKey> CreationTokens;
	TMap<FObjectKey, uint32> ControlEpochs;
	TMap<FObjectKey, uint32> SpawnOrdinals;
	uint32 NextSpawnOrdinal = 0;
	int32 SpawnCount = 0;
	int32 PendingHandOvers = 0;
	FStealthLogFaultSink FaultSink;

	/** Nesting of ReleaseBody (game thread only). */
	static int32 ReleasingBodyDepth;
};
