#pragma once

// The rival-signal rig of stories oc-014 / oc-015 (test-only): the slot-level FQueueRig with the PRODUCTION scheduler (the
// service schedules its own TimerFired(StageSignal) on the real queue; no recording stub), a spy resolver in front of the
// service that records every StageSignal payload it resolves, and seed search helpers over the pinned sub-seed list.

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "ObjectivesQueueRig.h"
#include "ObjectivesRivalSignal.h"

namespace StealthObjectivesTest
{
	/** Records each TimerFired(StageSignal) item and the drain time, then forwards to the service. */
	class FStageSignalSpy : public IQueueResolver
	{
	public:
		IQueueResolver* Service = nullptr;
		const FStealthQueueTestRig* Q = nullptr;
		TArray<FTimerPayload> Payloads;
		TArray<double> Times;
		TArray<EResolveResult> Results;

		virtual EResolveResult Resolve(const FQueueItem& Item, FQueueKey Key) override
		{
			if (const FTimerPayload* Timer = Item.Payload.TryGet<FTimerPayload>())
			{
				Payloads.Add(*Timer);
				Times.Add(Q->Clock.GetServerTimeSeconds());
			}
			const EResolveResult Result = Service->Resolve(Item, Key);
			Results.Add(Result);
			return Result;
		}
	};

	/** FQueueRig whose service schedules on the queue itself, with the spy in front of it. */
	struct FRivalRig : public FQueueRig
	{
		FStageSignalSpy Spy;

		explicit FRivalRig(TFunctionRef<void(FObjectivesServiceInit&)> Configure = [](FObjectivesServiceInit&) {})
		{
			Init.RivalRelease = nullptr; // production: UObjectivesService::ScheduleRivalRelease
			Configure(Init);
			Service->Bind(Init);
			Spy.Service = Service;
			Spy.Q = &Q;
			Q.Queue->UnregisterTimerResolver(ETimerKind::StageSignal);
			Q.Queue->RegisterTimerResolver(ETimerKind::StageSignal, &Spy);
		}

		uint8 RivalCount(int32 Team) const { return Core().GetRivalPrelimCount(Team); }

		/** Advances in Step frames until the clock reaches Time (the frame that reaches it drains it). */
		void RunUntil(double Time, double Step = 0.1)
		{
			while (Q.Clock.GetServerTimeSeconds() < Time - 1e-9)
			{
				Q.Tick(FMath::Min(Step, Time - Q.Clock.GetServerTimeSeconds()));
			}
		}
	};

	/** The first pinned sub-seed (SplitMix64 of 1..20000) whose drawn delay satisfies Want; 0 when none. */
	inline uint64 FindSubSeed(TFunctionRef<bool(int32 DeltaMs)> Want, int32 MinMs = 30000, int32 MaxMs = 165000)
	{
		for (uint64 Index = 1; Index <= 20000; ++Index)
		{
			uint64 Z = Index * 0x9E3779B97F4A7C15ull;
			Z = (Z ^ (Z >> 30)) * 0xBF58476D1CE4E5B9ull;
			Z = (Z ^ (Z >> 27)) * 0x94D049BB133111EBull;
			Z ^= Z >> 31;
			if (Z != 0 && Want(FObjectivesRivalSignal::DrawRivalDelayMsFromSubSeed(Z, MinMs, MaxMs)))
			{
				return Z;
			}
		}
		return 0;
	}
}

#endif
