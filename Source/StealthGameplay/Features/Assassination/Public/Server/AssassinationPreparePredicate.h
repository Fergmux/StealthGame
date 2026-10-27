#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Server/Core/CommandPredicate.h"
#include "UObject/Object.h"
#include "View/StealthTuningPublicSubstructs.h"

#include "AssassinationPreparePredicate.generated.h"

class FDefenceStateCore;
class IStealthClock;

/**
 * Prepare's CanBegin (ADR-0011 section 6, control manifest: Reject(Cooldown) iff F3 says the cooldown is running, else
 * Allow; story asn-003). Registered weakly per EActionType::Prepare from the composition root. Side-effect-free: the AI
 * dry run and the slot call the same function, and it never creates the defence component (a pawn without one has
 * T_act none).
 *
 * Tuning: DefenceCooldownSec is read from the Assassination tuning this object holds, set by the composition root from
 * the frozen set (SetTuning). Until the TDC tuning store feeds features (TDC epic), the root passes the schema defaults,
 * the same stand-in the Crowd director uses.
 *
 * Usage (composition root, server):
 *   UAssassinationPreparePredicate* Prepare = NewObject<UAssassinationPreparePredicate>(this);
 *   Prepare->Initialize(Clock, Tuning.Assassination_Public);
 *   Hub->RegisterPredicate(EActionType::Prepare, TWeakInterfacePtr<ICommandPredicate>(Prepare));
 */
UCLASS(NotBlueprintable, NotBlueprintType)
class STEALTHGAMEPLAY_API UAssassinationPreparePredicate : public UObject, public ICommandPredicate
{
	GENERATED_BODY()

public:
	/** The pure verdict: Reject(Cooldown) iff State has a stamp and Now - T_act < CooldownSec, else Allow. */
	static FPredicateResult Evaluate(const FDefenceStateCore* State, double NowServerTime, double CooldownSec);

	/** Binds the server clock (not owned; must outlive this object) and the Assassination tuning. */
	void Initialize(const IStealthClock* InClock, const FAssassinationTuning_Public& InTuning);

	/** Replaces the tuning (the next round's frozen set). */
	void SetTuning(const FAssassinationTuning_Public& InTuning) { Tuning = InTuning; }

	/** ICommandPredicate. No clock bound: Reject(NotNow) (a wiring defect; nothing executes without a clock). */
	virtual FPredicateResult CanBegin(const AStealthPawn& Pawn, const FCommandPayload& Cmd) const override;

private:
	const IStealthClock* Clock = nullptr;
	FAssassinationTuning_Public Tuning;
};
