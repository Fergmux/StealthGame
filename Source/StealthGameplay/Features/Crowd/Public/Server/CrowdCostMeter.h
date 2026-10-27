#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "HAL/PlatformTime.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

/**
 * The crowd's game-thread cost meter (ADR-0016 section 10, GDD F5, AC-CNB-26; spike story cnb-032). Every crowd entry
 * point on the game thread opens one scope of the six F5 names (Crowd.Director, Crowd.StateTree, Crowd.Follower,
 * Crowd.Perception, Crowd.NavQuery, Crowd.Dispatch); each is an Insights CPU scope AND, outside Shipping, a cycle
 * accumulator, so a cooked or -game process can report the summed crowd time per frame without an Insights session.
 * Top-level scopes (the Director's frame, a tree tick, a controller tick, a path callback) add to the frame total;
 * nested ones add to their own breakdown only, so nothing is counted twice. Compiled out of Shipping (AC-CNB-25: no crowd
 * instrumentation ships); the trace scope compiles out with the trace macros.
 */
enum class ECrowdCostScope : uint8
{
	Director,
	StateTree,
	Follower,
	Perception,
	NavQuery,
	Dispatch,
	Count,
};

#if !UE_BUILD_SHIPPING

/** The process-wide accumulators (game thread only). */
struct STEALTHGAMEPLAY_API FCrowdCostMeter
{
	/** Cycles per scope since the last TakeFrame. */
	uint64 Cycles[static_cast<int32>(ECrowdCostScope::Count)] = {};
	/** Cycles of top-level scopes (the frame's crowd total). */
	uint64 TopCycles = 0;
	/** Depth of open scopes (a nested scope does not add to TopCycles). */
	int32 Depth = 0;
	/** Off by default: a harness turns it on (two FPlatformTime reads per scope). */
	bool bEnabled = false;

	static FCrowdCostMeter& Get();

	/**
	 * The crowd debug sentinel (story cnb-036): a string compiled only outside Shipping, so the nightly Shipping-symbol
	 * check (tools/ci/shipping-symbol-check.sh, prefix STEALTH_TEST_SEAM_SENTINEL_) proves the crowd instrumentation is
	 * absent from a Shipping binary and present in Development (the positive control).
	 */
	static const TCHAR* DebugSentinel();

	/** Returns the totals since the last call in milliseconds (Total, then per scope) and resets them. */
	double TakeFrame(double OutScopesMs[static_cast<int32>(ECrowdCostScope::Count)]);
};

/** RAII: one crowd cost scope. */
class FCrowdCostScopeTimer
{
public:
	explicit FCrowdCostScopeTimer(ECrowdCostScope InScope)
		: Scope(InScope)
	{
		FCrowdCostMeter& Meter = FCrowdCostMeter::Get();
		if (Meter.bEnabled)
		{
			Start = FPlatformTime::Cycles64();
			++Meter.Depth;
		}
	}
	~FCrowdCostScopeTimer()
	{
		FCrowdCostMeter& Meter = FCrowdCostMeter::Get();
		if (Start != 0)
		{
			const uint64 Spent = FPlatformTime::Cycles64() - Start;
			Meter.Cycles[static_cast<int32>(Scope)] += Spent;
			if (--Meter.Depth == 0)
			{
				Meter.TopCycles += Spent;
			}
		}
	}

private:
	ECrowdCostScope Scope;
	uint64 Start = 0;
};

#define CROWD_COST_SCOPE(Name) TRACE_CPUPROFILER_EVENT_SCOPE(Crowd.Name); FCrowdCostScopeTimer UE_JOIN(CrowdCostScope_, __LINE__)(ECrowdCostScope::Name)

#else

#define CROWD_COST_SCOPE(Name) TRACE_CPUPROFILER_EVENT_SCOPE(Crowd.Name)

#endif
