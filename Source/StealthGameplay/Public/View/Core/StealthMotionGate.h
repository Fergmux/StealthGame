#pragma once

#include "CoreMinimal.h"
#include "UObject/NameTypes.h"

class UActorComponent;
class UClass;

/**
 * A feature-owned gate on one pawn's simulated moves (ADR-0017 section 4, the ADR-0010 saved-move extension; Concealment
 * story con-008). Declared in Core so the Shared Pawn's movement component can consult it without naming a feature: a
 * feature component on the pawn registers itself with UStealthMovementComponent::SetMotionGate.
 *
 * Per simulated move (server ServerMove, owner prediction, AI and host ticks) the movement component asks for the move's
 * state byte at the move's server time, records it in the saved move, and on a correction replay restores the recorded
 * byte instead of asking again (the replayed move keeps its own state, never the current ServerNow's). ApplyMoveState
 * is called for every move with the byte in effect; the gate writes collision only when the byte changes.
 */
class IStealthMotionGate
{
public:
	virtual ~IStealthMotionGate() = default;

	/** The gate's state for a move simulated at MoveServerTime (0 = open: Outside). */
	virtual uint8 EvaluateMoveState(double MoveServerTime) = 0;

	/** Applies State (collision responses; change-only). */
	virtual void ApplyMoveState(uint8 State) = 0;

	/** True when commanded Move input produces no displacement in State. */
	virtual bool IsMoveBlocked(uint8 State) const = 0;
};

/**
 * Feature components every AStealthPawn creates as default subobjects (ADR-0017 section 1: "UConcealmentComponent on every
 * pawn, same class, creation order and flags"). A feature registers its component class at module load through a static
 * registrar (the registry is read by AStealthPawn's constructor), so Core never includes a feature header (ADR-0007 rule
 * (d)). Registration order is the creation order; the same on every machine and every pawn.
 *
 * Usage (feature .cpp, file scope):
 *   static StealthPawnFeatureComponents::FRegistrar GConcealmentRegistrar(TEXT("Concealment"), &UConcealmentComponent::StaticClass);
 */
namespace StealthPawnFeatureComponents
{
	using FClassGetter = UClass* (*)();

	struct FEntry
	{
		FName Name;
		FClassGetter GetClass = nullptr;
	};

	/** Adds one component class (called by FRegistrar at static initialisation). */
	STEALTHGAMEPLAY_API void Register(FName Name, FClassGetter GetClass);

	/** Every registered component, in registration order. */
	STEALTHGAMEPLAY_API const TArray<FEntry>& GetEntries();

	struct FRegistrar
	{
		FRegistrar(const TCHAR* Name, FClassGetter GetClass) { Register(FName(Name), GetClass); }
	};
}
