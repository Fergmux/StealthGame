#pragma once

// Shared fixture of the eye-mask core tests (Camera story cs-012): an async trace backend whose results land one frame after
// submission (the GDD's async case), with blocking decided by a scripted predicate; capsule-style probes for a target; a
// fixed 60 Hz frame clock; the view at the origin looking along +X.

#include "CoreMinimal.h"
#include "View/Core/StealthEyeMaskCore.h"
#include "View/Tuning/StealthCameraParams.h"

namespace StealthEyeMaskTest
{
	inline constexpr double Dt = 1.0 / 60.0;

	class FDelayedBackend final : public IEyeMaskTraceBackend
	{
	public:
		/** Blocked(From, To): the scripted geometry. */
		TFunction<bool(const FVector&, const FVector&)> Blocked = [](const FVector&, const FVector&) { return false; };
		int32 Frame = 0;
		int32 Submitted = 0;
		TArray<int32> SubmittedPerFrame;

		virtual FEyeMaskTraceHandle SubmitTrace(const FVector& From, const FVector& To) override
		{
			FPending& P = Pending.Add(++NextHandle);
			P.bBlocked = Blocked(From, To);
			P.LandFrame = Frame + 1;
			++Submitted;
			if (SubmittedPerFrame.Num() <= Frame)
			{
				SubmittedPerFrame.SetNumZeroed(Frame + 1);
			}
			++SubmittedPerFrame[Frame];
			return NextHandle;
		}
		virtual TOptional<bool> PollResult(FEyeMaskTraceHandle Handle) override
		{
			const FPending* P = Pending.Find(Handle);
			if (P == nullptr || Frame < P->LandFrame)
			{
				return {};
			}
			const bool bBlocked = P->bBlocked;
			Pending.Remove(Handle);
			return bBlocked;
		}

	private:
		struct FPending
		{
			bool bBlocked = false;
			int32 LandFrame = 0;
		};
		TMap<FEyeMaskTraceHandle, FPending> Pending;
		FEyeMaskTraceHandle NextHandle = 0;
	};

	/** Head, chest, pelvis and two lateral points of a standing capsule at Feet (never bones). */
	inline FEyeMaskProbes CapsuleProbes(const FVector& Feet)
	{
		FEyeMaskProbes Out;
		Out.Add(Feet + FVector(0.0, 0.0, 165.0));
		Out.Add(Feet + FVector(0.0, 0.0, 130.0));
		Out.Add(Feet + FVector(0.0, 0.0, 95.0));
		Out.Add(Feet + FVector(0.0, 35.0, 110.0));
		Out.Add(Feet + FVector(0.0, -35.0, 110.0));
		return Out;
	}

	inline FEyeMaskView ViewAlongX() { FEyeMaskView V; V.Origin = FVector(0.0, 0.0, 160.0); return V; }

	inline FStealthCameraParams Params() { return FStealthCameraParams::Defaults(); }
}
