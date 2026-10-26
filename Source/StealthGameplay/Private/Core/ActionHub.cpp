#include "Server/ActionHub.h"

#include "Engine/World.h"
#include "View/Core/StealthPawn.h"

namespace ActionHubPrivate
{
	const FName FaultSource(TEXT("ActionHub"));

	bool IsInRange(EActionType Type) { return static_cast<int32>(Type) < GActionTypeCount; }
}

bool UActionHub::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && World->GetNetMode() != NM_Client && Super::ShouldCreateSubsystem(Outer);
}

UActionHub* UActionHub::Get(const UWorld* World)
{
	return World ? World->GetSubsystem<UActionHub>() : nullptr;
}

IStealthFaultSink& UActionHub::GetFaultSink() const
{
	return InjectedFaultSink ? *InjectedFaultSink : static_cast<IStealthFaultSink&>(LogFaultSink);
}

bool UActionHub::RegisterPredicate(EActionType Type, TWeakInterfacePtr<ICommandPredicate> Predicate)
{
	if (!ActionHubPrivate::IsInRange(Type) || !Predicate.IsValid())
	{
		GetFaultSink().Report(EStealthFault::CommandNoPredicate, ActionHubPrivate::FaultSource, static_cast<int64>(Type));
		return false;
	}
	// Re-registering replaces (story iif-003 pins "replace": the signature has no other reading).
	Predicates[static_cast<int32>(Type)] = Predicate;
	return true;
}

void UActionHub::UnregisterPredicate(EActionType Type)
{
	if (ActionHubPrivate::IsInRange(Type))
	{
		Predicates[static_cast<int32>(Type)].Reset();
	}
}

bool UActionHub::HasPredicate(EActionType Type) const
{
	return ActionHubPrivate::IsInRange(Type) && Predicates[static_cast<int32>(Type)].IsValid();
}

FPredicateResult UActionHub::ResolvePredicate(EActionType Type, const AStealthPawn& Pawn, const FCommandPayload& Cmd) const
{
	if (!HasPredicate(Type))
	{
		// A missing or destroyed predicate is a fault and resolves Reject(NotNow) (ADR-0011 section 6).
		GetFaultSink().Report(EStealthFault::CommandNoPredicate, ActionHubPrivate::FaultSource, static_cast<int64>(Type));
		return FPredicateResult::Reject(ECommandRejectCode::NotNow);
	}
	return Predicates[static_cast<int32>(Type)]->CanBegin(Pawn, Cmd);
}
