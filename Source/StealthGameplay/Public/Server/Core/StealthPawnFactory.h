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

	/** ADR-0010 3.1 steps 1-5. Null Pawn on refusal (no authority, wrong thread, no class, spawn collision). */
	FStealthSpawnResult SpawnSharedPawn(const FStealthSpawnRequest& Request);

	/** True for a pawn this factory created (the server-only creation token). */
	bool HasCreationToken(const AActor* Pawn) const;
	/** Pawns created since the world began. */
	int32 GetSpawnCount() const { return SpawnCount; }
	/** Shared Pawns in the world without a creation token (the debug guard; 0 in a healthy round). */
	int32 CountUntokenedPawns() const;

	/** The fault sink the factory reports misuse to. */
	const FStealthLogFaultSink& GetFaultSink() const { return FaultSink; }

private:
	IStealthPawnFactoryHooks* GetHooks() const;

	IStealthPawnFactoryHooks* Hooks = nullptr;
	TWeakObjectPtr<UObject> HooksOwner;
	TSubclassOf<AStealthPawn> DefaultPawnClass;
	TSet<FObjectKey> CreationTokens;
	int32 SpawnCount = 0;
	FStealthLogFaultSink FaultSink;
};
