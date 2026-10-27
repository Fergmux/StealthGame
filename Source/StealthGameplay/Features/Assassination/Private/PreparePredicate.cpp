// Story asn-003: Prepare's CanBegin (ADR-0011 section 6; GDD F3). See Server/AssassinationPreparePredicate.h.

#include "Server/AssassinationPreparePredicate.h"

#include "Server/DefenceStateComponent.h"
#include "Server/StealthClock.h"
#include "View/Core/StealthPawn.h"

FPredicateResult UAssassinationPreparePredicate::Evaluate(const FDefenceStateCore* State, double NowServerTime, double CooldownSec)
{
	if (State != nullptr && !State->CanPrepare(NowServerTime, CooldownSec))
	{
		return FPredicateResult::Reject(ECommandRejectCode::Cooldown);
	}
	return FPredicateResult::Allow();
}

void UAssassinationPreparePredicate::Initialize(const IStealthClock* InClock, const FAssassinationTuning_Public& InTuning)
{
	Clock = InClock;
	Tuning = InTuning;
}

FPredicateResult UAssassinationPreparePredicate::CanBegin(const AStealthPawn& Pawn, const FCommandPayload& Cmd) const
{
	if (Clock == nullptr)
	{
		return FPredicateResult::Reject(ECommandRejectCode::NotNow);
	}
	const UDefenceStateComponent* Component = UDefenceStateComponent::Find(Pawn);
	return Evaluate(Component != nullptr ? &Component->GetCore() : nullptr, Clock->GetServerTimeSeconds(), Tuning.DefenceCooldownSec);
}
