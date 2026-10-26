#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Server/Core/PawnCommandTypes.h"
#include "View/CommandReplyTypes.h"
#include "View/CourtFixtureId.h"
#include "View/Core/StealthGait.h"

#include <type_traits>

class AStealthPawn;
class UPawnCommandComponent;

/**
 * Where a sink reads the pawn's current control epoch (ADR-0010 section 4.4). Production: UStealthPawnFactory of the
 * pawn's world (FCrowdFactoryEpochSource). Specs inject a fake.
 */
class ICrowdEpochSource
{
public:
	virtual ~ICrowdEpochSource() = default;
	virtual uint32 GetControlEpoch(const AStealthPawn* Pawn) const = 0;
};

/** The production epoch source: UStealthPawnFactory::GetControlEpoch of the pawn's world. */
class STEALTHGAMEPLAY_API FCrowdFactoryEpochSource final : public ICrowdEpochSource
{
public:
	virtual uint32 GetControlEpoch(const AStealthPawn* Pawn) const override;

	/** The shared instance (stateless). */
	static const FCrowdFactoryEpochSource& Get();
};

/** Drop counters of one sink (diagnostics and the AC-CNB-38 gate; plain integers, never replicated). */
struct FCrowdSinkCounters
{
	/** Calls dropped because the token's epoch is not the pawn's current epoch (or the pawn is gone). */
	int32 StaleEpochDrop = 0;
	/** Calls dropped by the Halt filter (anything but Move(0) and Look after Halt). */
	int32 HaltDrop = 0;
	/** Calls that reached the pawn command component. */
	int32 Passed = 0;
	/** Discrete commands refused by the dry run (never sent). */
	int32 DryRunRefused = 0;
	/** Discrete commands (Traverse, Interact, Prepare, Hide) that reached the command component. */
	int32 DiscretePassed = 0;
};

/**
 * FCrowdMotionSink: the restricted driver's sink (ADR-0016 section 2; GDD CNB-R3, R10.1; story cnb-005). It exposes
 * ONLY Move and Look: a participant-bound pawn's driver holds this type, so it cannot issue any other command by
 * construction. Every call
 *   1. drops when the pawn is gone or the drive token's ControlEpoch is not the pawn's current epoch (StaleEpochDrop),
 *   2. after the Halt latch passes only Move(0) and Look (HaltDrop),
 *   3. otherwise calls the pawn's UPawnCommandComponent entry point, and nothing else (no AddMovementInput, no
 *      SetActorRotation, no velocity write, no MoveTo).
 * Per call cost: a weak-pointer resolve and two integer compares.
 */
class STEALTHGAMEPLAY_API FCrowdMotionSink
{
public:
	FCrowdMotionSink() = default;
	/** Pawn and the epoch the driver was created with. EpochSource defaults to the pawn factory's. */
	FCrowdMotionSink(AStealthPawn* InPawn, uint32 InControlEpoch, const ICrowdEpochSource* InEpochSource = nullptr);

	/** Continuous Move (input space, X right / Y forward of the control yaw, |Direction| <= 1). False when dropped. */
	bool Move(FVector2D Direction, EGait Gait);

	/** Continuous Look: the control rotation (yaw and pitch). False when dropped. */
	bool Look(FRotator Rotation);

	/** The Halt latch (ADR-0016 section 7): set by the Director's Halt, synchronously. */
	void SetHalted(bool bInHalted) { bHalted = bInHalted; }
	bool IsHalted() const { return bHalted; }

	/** The token's epoch and pawn (read-only). */
	uint32 GetControlEpoch() const { return ControlEpoch; }
	AStealthPawn* GetPawn() const { return Pawn.Get(); }

	/** True when a call made now would pass the epoch check (the pawn exists and its epoch equals the token's). */
	bool IsCurrent() const;

	const FCrowdSinkCounters& GetCounters() const { return Counters; }

protected:
	/** The epoch gate and the Halt filter; returns the command component when the call may pass. */
	UPawnCommandComponent* Admit(bool bAllowedWhenHalted);

	TWeakObjectPtr<AStealthPawn> Pawn;
	uint32 ControlEpoch = 0;
	bool bHalted = false;
	const ICrowdEpochSource* EpochSource = nullptr;
	FCrowdSinkCounters Counters;
};

/**
 * FCrowdCommandSink: the full driver's sink. A motion sink plus the discrete commands Traverse, InteractBegin/End,
 * PrepareBegin/End and Hide, which enter the pawn's command component with the server-local mailbox and the null reply
 * sink (IIF R2, R5). There is no Attack method: an NPC strike cannot be written (T6, AC-CNB-11). After Halt every
 * discrete call is dropped.
 */
class STEALTHGAMEPLAY_API FCrowdCommandSink : public FCrowdMotionSink
{
public:
	using FCrowdMotionSink::FCrowdMotionSink;

	bool Traverse(const FTraversalHint& Hint);
	bool InteractBegin(FCourtFixtureId Target);
	bool InteractEnd();
	bool PrepareBegin();
	bool PrepareEnd();
	bool Hide(const FHideCommand& Command);

	/**
	 * The dry run before a discrete command (CNB-R3): the IIF's side-effect-free legality predicate for Type. True when
	 * the command may be sent (Allow or AllowNoEffect, or no predicate registered for the type in this build); false
	 * counts DryRunRefused and the caller abandons the behaviour. The margins (CrowdDryRunReachCm / FacingDeg) are
	 * applied by the caller's target selection.
	 */
	bool DryRun(EActionType Type, const FCourtFixtureId* Target = nullptr);

	/** A target refused by the dry run or the IIF is not retried before Until (CrowdRetryBlockSec later). */
	void BlockTarget(uint64 TargetKey, double Until);
	bool IsTargetBlocked(uint64 TargetKey, double Now) const;

private:
	TMap<uint64, double> BlockedTargets;
};

namespace StealthCrowdSink
{
	/** Compile-time proof that a sink type has no Attack member (AC-CNB-11 by construction). */
	template <typename T, typename = void>
	struct THasAttack : std::false_type {};
	template <typename T>
	struct THasAttack<T, std::void_t<decltype(&T::Attack)>> : std::true_type {};

	/** Compile-time proof that the motion sink has no discrete command. */
	template <typename T, typename = void>
	struct THasTraverse : std::false_type {};
	template <typename T>
	struct THasTraverse<T, std::void_t<decltype(&T::Traverse)>> : std::true_type {};
}

static_assert(!StealthCrowdSink::THasAttack<FCrowdMotionSink>::value, "The restricted sink must have no Attack method (ADR-0016 section 2)");
static_assert(!StealthCrowdSink::THasAttack<FCrowdCommandSink>::value, "The NPC sink must have no Attack method (ADR-0016 section 2, AC-CNB-11)");
static_assert(!StealthCrowdSink::THasTraverse<FCrowdMotionSink>::value, "The restricted sink exposes only Move and Look (CNB-R10.1)");
