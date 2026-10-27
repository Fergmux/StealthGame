// Story asn-026: Assassination's telemetry probe call sites and cost scope. See Server/EncounterTelemetryProbe.h.

#include "Server/EncounterTelemetryProbe.h"

#include "HAL/PlatformTime.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Server/AttackResolverCore.h"
#include "Server/DefenceStateCore.h"
#include "Server/StealthClock.h"

FAssassinationCostMeter& FAssassinationCostMeter::Get()
{
	static FAssassinationCostMeter Meter;
	return Meter;
}

double FAssassinationCostMeter::TakeMs()
{
	const double Ms = FPlatformTime::ToMilliseconds64(Cycles);
	Cycles = 0;
	Calls = 0;
	return Ms;
}

namespace EncounterTelemetryProbeLocal
{
	/** RAII: one resolution inside Stealth.Budget.Assassination (the cycle accumulator outside Shipping). */
	class FCostScope
	{
	public:
		FCostScope()
		{
#if !UE_BUILD_SHIPPING
			if (FAssassinationCostMeter::Get().bEnabled)
			{
				Start = FPlatformTime::Cycles64();
			}
#endif
		}
		~FCostScope()
		{
#if !UE_BUILD_SHIPPING
			if (Start != 0)
			{
				FAssassinationCostMeter& Meter = FAssassinationCostMeter::Get();
				Meter.Cycles += FPlatformTime::Cycles64() - Start;
				++Meter.Calls;
			}
#endif
		}

	private:
		uint64 Start = 0;
	};

	ETelemetryStrikeOutcome ToTelemetry(EAttackOutcome Outcome)
	{
		switch (Outcome)
		{
		case EAttackOutcome::Kill: return ETelemetryStrikeOutcome::Kill;
		case EAttackOutcome::Reversal: return ETelemetryStrikeOutcome::Reversal;
		default: return ETelemetryStrikeOutcome::Whiff;
		}
	}

	FSubjectRef Ref(APawn* Pawn)
	{
		return Pawn != nullptr ? FSubjectRef::ForPawn(FPawnHandle{ Pawn }) : FSubjectRef();
	}
}

void FEncounterTelemetryProbe::Bind(FAttackResolverCore* InCore, IEncounterDefenceStore* InDefence, const IStealthClock* InClock, double InDefenceCooldownSec, IStealthTelemetrySink* InSink)
{
	Core = InCore;
	Defence = InDefence;
	Clock = InClock;
	DefenceCooldownSec = InDefenceCooldownSec;
#if STEALTH_WITH_PLAYTEST_TELEMETRY
	Sink = InSink;
#else
	Sink = nullptr; // Shipping: the null object, whatever is passed (ADR-0023 section 1)
#endif
}

double FEncounterTelemetryProbe::Now() const
{
	return Clock != nullptr ? Clock->GetServerTimeSeconds() : 0.0;
}

EResolveResult FEncounterTelemetryProbe::Resolve(const FQueueItem& Item, FQueueKey Key)
{
	if (Core == nullptr)
	{
		return EResolveResult::Voided;
	}
	EResolveResult Result = EResolveResult::Voided;
	const int32 Before = Core->GetResolutions().Num();
	CurrentKey = Key;
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(Stealth.Budget.Assassination);
		EncounterTelemetryProbeLocal::FCostScope Cost;
		Result = Core->Resolve(Item, Key);
	}
	// The owner's decision point (ADR-0023 section 2.1): after the outcome is decided, in the same slot.
	const TArray<FAttackResolution>& Resolutions = Core->GetResolutions();
	for (int32 Index = FMath::Min(Before, Resolutions.Num()); Index < Resolutions.Num(); ++Index)
	{
		RecordStrike(Resolutions[Index], Key);
	}
	CurrentKey = FQueueKey();
	return Result;
}

void FEncounterTelemetryProbe::OnRejectedLatched(const FQueueItem& Item, FQueueKey Key)
{
	if (Core != nullptr)
	{
		Core->OnRejectedLatched(Item, Key);
	}
}

void FEncounterTelemetryProbe::RecordStrike(const FAttackResolution& Resolution, FQueueKey Key)
{
	using namespace EncounterTelemetryProbeLocal;
	FTelemetryStrikeResolved Event;
	Event.Key = Key;
	Event.Attacker = Ref(Resolution.Attacker);
	Event.Victim = Ref(Resolution.Victim);
	Event.Outcome = ToTelemetry(Resolution.Outcome);
	const double Contact = Resolution.ContactServerTime;
	if (Resolution.Victim != nullptr)
	{
		if (const FBraceRecord* Brace = Braces.Find(FObjectKey(Resolution.Victim)))
		{
			// Braced at contact: still up, or ended by this very strike's reversal in this slot.
			const bool bEndedByThis = Brace->OpenStart < 0.0 && Brace->LastEndKey == Key && Brace->LastCause == ETelemetryBraceEnd::Reversal;
			const double Start = Brace->OpenStart >= 0.0 ? Brace->OpenStart : (bEndedByThis ? Brace->LastStart : -1.0);
			Event.VictimBraceAgeSec = Start >= 0.0 ? static_cast<float>(Contact - Start) : -1.f;
			Event.VictimSinceBraceEndSec = Start < 0.0 && Brace->LastEnd >= 0.0 ? static_cast<float>(Contact - Brace->LastEnd) : -1.f;
		}
		const FDefenceStateCore* State = Defence != nullptr ? Defence->Find(Resolution.Victim) : nullptr;
		Event.bVictimDefenceReady = State == nullptr || State->CanPrepare(Contact, DefenceCooldownSec);
	}
	// Objectives does not publish its court table to Assassination yet: the court fields stay at their "unknown" values.
	GetSink().Record(Event);
}

void FEncounterTelemetryProbe::OnActionStarted(APawn* Pawn, EActionType Type)
{
	if (Type == EActionType::Prepare && Pawn != nullptr)
	{
		FBraceRecord& Brace = Braces.FindOrAdd(FObjectKey(Pawn));
		Brace.Pawn = Pawn;
		Brace.OpenStart = Now();
		Brace.LastStart = Brace.OpenStart;
	}
	if (Inner != nullptr)
	{
		Inner->OnActionStarted(Pawn, Type);
	}
}

void FEncounterTelemetryProbe::OnActionCancelled(APawn* Pawn, EActionType Type, ECancelReason Reason)
{
	if (Type == EActionType::Prepare)
	{
		EndBrace(Pawn, Reason == ECancelReason::Reversed ? ETelemetryBraceEnd::Reversal
			: (Reason == ECancelReason::Superseded ? ETelemetryBraceEnd::Preempted
			: (Reason == ECancelReason::Released ? ETelemetryBraceEnd::Cancelled : ETelemetryBraceEnd::Removed)));
	}
	if (Inner != nullptr)
	{
		Inner->OnActionCancelled(Pawn, Type, Reason);
	}
}

void FEncounterTelemetryProbe::OnActionCompleted(APawn* Pawn, EActionType Type)
{
	if (Type == EActionType::Prepare)
	{
		EndBrace(Pawn, ETelemetryBraceEnd::Expired);
	}
	if (Inner != nullptr)
	{
		Inner->OnActionCompleted(Pawn, Type);
	}
}

void FEncounterTelemetryProbe::EndBrace(APawn* Pawn, ETelemetryBraceEnd Cause)
{
	FBraceRecord* Brace = Pawn != nullptr ? Braces.Find(FObjectKey(Pawn)) : nullptr;
	if (Brace == nullptr || Brace->OpenStart < 0.0)
	{
		return; // no brace up (a stale end): nothing to record
	}
	FTelemetryBraceEnded Event;
	Event.ServerTime = Now();
	Event.Subject = EncounterTelemetryProbeLocal::Ref(Pawn);
	Event.StartTime = Brace->OpenStart;
	Event.Cause = Cause;
	Brace->LastEnd = Event.ServerTime;
	Brace->LastCause = Cause;
	Brace->LastEndKey = CurrentKey;
	Brace->OpenStart = -1.0;
	GetSink().Record(Event);
}

void FEncounterTelemetryProbe::OnRoundReset()
{
	for (TPair<FObjectKey, FBraceRecord>& Pair : Braces)
	{
		if (Pair.Value.OpenStart >= 0.0)
		{
			FTelemetryBraceEnded Event;
			Event.ServerTime = Now();
			Event.Subject = EncounterTelemetryProbeLocal::Ref(Pair.Value.Pawn.Get());
			Event.StartTime = Pair.Value.OpenStart;
			Event.Cause = ETelemetryBraceEnd::Removed;
			GetSink().Record(Event);
		}
	}
	Braces.Reset();
}
