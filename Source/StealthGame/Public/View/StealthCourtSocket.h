#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "View/CourtFixtureId.h"
#include "StealthCourtSocket.generated.h"

class UStaticMeshComponent;

/**
 * STAND-IN for the Level Socket Contract's ObjectiveCourt socket (GDD level-socket-contract.md R3; ADR-0014 section 7.4) on
 * L_Prototype, until the LSC layout service publishes court fixtures. It replaced the prototype AStealthObjective (Objectives
 * Chain story oc-022, 2026-10-28; Config/DefaultEngine.ini redirects the class, so the level's placed actors load as this).
 *
 * Placement only: a court is public geometry. The actor is NOT replicated (the level places it on every machine and the
 * prototype city generator moves it identically everywhere from the replicated round seed), has no tick, no completion or
 * interactor state and no gameplay: the fixture shows nothing stateful (identical relief on every court, OC-15). The
 * Objectives Chain owns every objective rule server-side (UObjectivesService); this actor never spawns, completes or reveals
 * anything.
 *
 * Court ids: a court's FCourtFixtureId is its 1-based index among the world's court sockets sorted by name. The level-placed
 * actors have the same names on the server and on every client, so the server's assignment adapter, the IIF stand-in's reach
 * test and a client's HUD ring agree on the ids.
 *
 * LayoutTeamHint / LayoutStageHint are the prototype layout's placement inputs (the generator puts the stage-3 socket at the
 * temple front and team T's sockets on its side), read only by that generator and by the composition root's server-only
 * assignment stand-in. They are level data, not replicated state; LSC Selection replaces them with a secret role draw
 * (ADR-0014; the prototype map's roles are therefore derivable from its layout: a recorded stand-in limitation).
 */
UCLASS()
class STEALTHGAME_API AStealthCourtSocket : public AActor
{
	GENERATED_BODY()

public:
	AStealthCourtSocket();

	/** Prototype layout hint: the team side the generator places this socket on (255, STEALTH_NO_TEAM, for the temple socket). */
	UPROPERTY(EditAnywhere, Category = "Stealth|Layout", meta = (DisplayName = "Layout Team Hint"))
	uint8 OwningTeam = 255;

	/** Prototype layout hint: 1-2 for side sockets, 3 (STEALTH_FINAL_STAGE) for the temple-front socket. */
	UPROPERTY(EditAnywhere, Category = "Stealth|Layout", meta = (ClampMin = "1", ClampMax = "3", DisplayName = "Layout Stage Hint"))
	int32 Stage = 1;

	/** The world's court sockets sorted by name (index + 1 is the court id). */
	static void GetCourts(const UWorld* World, TArray<const AStealthCourtSocket*>& OutCourts);

	/** The court socket of Court in World, or null. */
	static const AStealthCourtSocket* FindCourt(const UWorld* World, FCourtFixtureId Court);

	/** The nearest court within RangeCm (2D) of Location, or an unset id (the IIF prompt-target stand-in; public geometry). */
	static FCourtFixtureId FindCourtInReach(const UWorld* World, const FVector& Location, double RangeCm);

protected:
	/** The court fixture (one stone block, identical on every court). */
	UPROPERTY(VisibleAnywhere, Category = "Stealth")
	TObjectPtr<UStaticMeshComponent> Fixture;
};
