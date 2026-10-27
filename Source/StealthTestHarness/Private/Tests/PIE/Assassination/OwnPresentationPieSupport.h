#pragma once

// Shared helpers of the owner-presentation PIE tests (stories asn-024 and asn-017): each machine's owner presentation log
// (the HUD view-model changes of AStealthPlayerController), a per-machine frame tracker for the "+- 1 frame" bounds, and
// the HUD's element list of a machine.

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS && defined(ENABLE_PIE_NETWORK_TEST) && ENABLE_PIE_NETWORK_TEST && WITH_STEALTH_TEST_SEAMS

#include "StealthHUD.h"
#include "Tests/PIE/Assassination/PresentationPieSupport.h"

namespace StealthOwnPresentationPie
{
	/** The local controller of machine Index (into StealthAssassinationPie::Worlds()). */
	inline AStealthPlayerController* LocalOf(int32 Machine)
	{
		const TArray<UWorld*> All = StealthAssassinationPie::Worlds();
		return All.IsValidIndex(Machine) ? Cast<AStealthPlayerController>(All[Machine]->GetFirstPlayerController()) : nullptr;
	}

	/** Machine's owner presentation records of Event from index From on (From: a GetOwnPresentationLog().Num() snapshot). */
	inline TArray<FOwnPresentationRecord> RecordsOf(int32 Machine, EOwnPresentationEvent Event, int32 From = 0)
	{
		TArray<FOwnPresentationRecord> Out;
		if (const AStealthPlayerController* Local = LocalOf(Machine))
		{
			const TArray<FOwnPresentationRecord>& Log = Local->GetOwnPresentationLog();
			for (int32 Index = FMath::Max(0, From); Index < Log.Num(); ++Index)
			{
				if (Log[Index].Event == Event)
				{
					Out.Add(Log[Index]);
				}
			}
		}
		return Out;
	}

	/** The size of Machine's owner presentation log now. */
	inline int32 LogSizeOf(int32 Machine)
	{
		const AStealthPlayerController* Local = LocalOf(Machine);
		return Local != nullptr ? Local->GetOwnPresentationLog().Num() : 0;
	}

	/** The HUD element list of Machine's local player now. */
	inline TArray<FString> ElementsOf(int32 Machine)
	{
		TArray<FString> Out;
		if (const AStealthPlayerController* Local = LocalOf(Machine))
		{
			AStealthHUD::CollectOwnerElements(*Local, Out);
		}
		return Out;
	}

	/** The longest frame each machine ran (the "+- 1 frame" bound), sampled every test frame. */
	struct FFrameTracker
	{
		double Longest[8] = {};

		void Sample()
		{
			const TArray<UWorld*> All = StealthAssassinationPie::Worlds();
			for (int32 Machine = 0; Machine < All.Num() && Machine < 8; ++Machine)
			{
				Longest[Machine] = FMath::Max(Longest[Machine], static_cast<double>(All[Machine]->GetDeltaSeconds()));
			}
		}

		/** One frame of Machine plus a 3 ms clock-quantisation slack (never below one 60 Hz frame). */
		double FrameOf(int32 Machine) const
		{
			return FMath::Max(1.0 / 60.0, Machine >= 0 && Machine < 8 ? Longest[Machine] : 0.0) + 0.003;
		}
	};

	/** True when Time is on the frame of Due: never before it (1 ms) and no later than one frame after. */
	inline bool OnFrameOf(double Time, double Due, double Frame)
	{
		return Time + 0.001 >= Due && Time <= Due + Frame;
	}
}

#endif
