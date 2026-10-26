#include "Server/CrowdSink.h"

#include "Server/ActionHub.h"
#include "Server/Core/StealthPawnCommandComponent.h"
#include "Server/Core/StealthPawnControlQuery.h"
#include "View/Core/StealthPawn.h"

// ---- Epoch source --------------------------------------------------------------------------------------------------

uint32 FCrowdFactoryEpochSource::GetControlEpoch(const AStealthPawn* Pawn) const
{
	return StealthPawnControl::GetControlEpoch(Pawn);
}

const FCrowdFactoryEpochSource& FCrowdFactoryEpochSource::Get()
{
	static const FCrowdFactoryEpochSource Instance;
	return Instance;
}

// ---- FCrowdMotionSink ----------------------------------------------------------------------------------------------

FCrowdMotionSink::FCrowdMotionSink(AStealthPawn* InPawn, uint32 InControlEpoch, const ICrowdEpochSource* InEpochSource)
	: Pawn(InPawn)
	, ControlEpoch(InControlEpoch)
	, EpochSource(InEpochSource != nullptr ? InEpochSource : &FCrowdFactoryEpochSource::Get())
{
}

bool FCrowdMotionSink::IsCurrent() const
{
	const AStealthPawn* Body = Pawn.Get();
	return Body != nullptr && EpochSource != nullptr && EpochSource->GetControlEpoch(Body) == ControlEpoch;
}

UPawnCommandComponent* FCrowdMotionSink::Admit(bool bAllowedWhenHalted)
{
	AStealthPawn* Body = Pawn.Get();
	if (Body == nullptr || EpochSource == nullptr || EpochSource->GetControlEpoch(Body) != ControlEpoch)
	{
		++Counters.StaleEpochDrop;
		return nullptr;
	}
	if (bHalted && !bAllowedWhenHalted)
	{
		++Counters.HaltDrop;
		return nullptr;
	}
	UPawnCommandComponent* Command = Body->GetPawnCommand();
	if (Command != nullptr)
	{
		++Counters.Passed;
	}
	return Command;
}

bool FCrowdMotionSink::Move(FVector2D Direction, EGait Gait)
{
	// After Halt only Move(0) passes (it clears the held intent; the factory's stop path brakes the pawn).
	UPawnCommandComponent* Command = Admit(/*bAllowedWhenHalted*/ Direction.IsNearlyZero());
	if (Command == nullptr)
	{
		return false;
	}
	Command->Move(Direction, Gait);
	return true;
}

bool FCrowdMotionSink::Look(FRotator Rotation)
{
	UPawnCommandComponent* Command = Admit(/*bAllowedWhenHalted*/ true);
	if (Command == nullptr)
	{
		return false;
	}
	Command->Look(Rotation);
	return true;
}

// ---- FCrowdCommandSink ---------------------------------------------------------------------------------------------

bool FCrowdCommandSink::Traverse(const FTraversalHint& Hint)
{
	UPawnCommandComponent* Command = Admit(false);
	if (Command == nullptr)
	{
		return false;
	}
	Command->Traverse(Hint);
	++Counters.DiscretePassed;
	return true;
}

bool FCrowdCommandSink::InteractBegin(FCourtFixtureId Target)
{
	UPawnCommandComponent* Command = Admit(false);
	if (Command == nullptr)
	{
		return false;
	}
	Command->InteractBegin(Target);
	++Counters.DiscretePassed;
	return true;
}

bool FCrowdCommandSink::InteractEnd()
{
	UPawnCommandComponent* Command = Admit(false);
	if (Command == nullptr)
	{
		return false;
	}
	Command->InteractEnd();
	++Counters.DiscretePassed;
	return true;
}

bool FCrowdCommandSink::PrepareBegin()
{
	UPawnCommandComponent* Command = Admit(false);
	if (Command == nullptr)
	{
		return false;
	}
	Command->PrepareBegin();
	++Counters.DiscretePassed;
	return true;
}

bool FCrowdCommandSink::PrepareEnd()
{
	UPawnCommandComponent* Command = Admit(false);
	if (Command == nullptr)
	{
		return false;
	}
	Command->PrepareEnd();
	++Counters.DiscretePassed;
	return true;
}

bool FCrowdCommandSink::Hide(const FHideCommand& Cmd)
{
	UPawnCommandComponent* Command = Admit(false);
	if (Command == nullptr)
	{
		return false;
	}
	Command->Hide(Cmd);
	++Counters.DiscretePassed;
	return true;
}

bool FCrowdCommandSink::DryRun(EActionType Type, const FCourtFixtureId* Target)
{
	const AStealthPawn* Body = Pawn.Get();
	const UActionHub* Hub = Body != nullptr ? UActionHub::Get(Body->GetWorld()) : nullptr;
	if (Body == nullptr || Hub == nullptr || !Hub->HasPredicate(Type))
	{
		// No legality owner registered in this build: nothing to dry-run against (the IIF still judges the command).
		return Body != nullptr;
	}
	FCommandPayload Payload;
	if (Target != nullptr)
	{
		Payload.Target = FSubjectRef::ForCourtSocket(Target->Value);
	}
	const FPredicateResult Result = Hub->ResolvePredicate(Type, *Body, Payload);
	if (Result.Verdict == EPredicateVerdict::Reject)
	{
		++Counters.DryRunRefused;
		return false;
	}
	return true;
}

void FCrowdCommandSink::BlockTarget(uint64 TargetKey, double Until)
{
	BlockedTargets.Add(TargetKey, Until);
}

bool FCrowdCommandSink::IsTargetBlocked(uint64 TargetKey, double Now) const
{
	const double* Until = BlockedTargets.Find(TargetKey);
	return Until != nullptr && Now < *Until;
}
