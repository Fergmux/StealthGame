#pragma once

// The herring recorder of the StealthGame.Crowd.* red-herring specs (stories cnb-019 to cnb-023): an ICrowdAgentObserver
// that logs every performed tell and every herring event with its time, plus a per-step body sampler for the run bouts.
// Test-only.

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Server/CrowdAgent.h"

namespace StealthCrowdHerringTest
{
	struct FTellEvent
	{
		uint32 Ordinal = 0;
		double Time = 0.0;
		ECrowdTell Tell = ECrowdTell::Run;
	};

	struct FRunLeg
	{
		uint32 Ordinal = 0;
		double Time = 0.0;
		double BoutSec = 0.0;
		FVector Goal = FVector::ZeroVector;
		bool bPanic = false;
	};

	struct FBrace
	{
		uint32 Ordinal = 0;
		double Time = 0.0;
		bool bTriggered = false;
		double HoldSec = 0.0;
		bool bCapped = false;
	};

	struct FDecoyChoice
	{
		uint32 Ordinal = 0;
		double Time = 0.0;
		int32 Court = INDEX_NONE;
		int32 Excluded = INDEX_NONE;
	};

	struct FDecoyHold
	{
		uint32 Ordinal = 0;
		double Time = 0.0;
		int32 Court = INDEX_NONE;
		double HoldSec = 0.0;
		bool bComplete = false;
	};

	struct FBushEvent
	{
		uint32 Ordinal = 0;
		double Time = 0.0;
		uint16 BushId = 0;
		double StaySec = 0.0;
		bool bEnter = true;
	};

	/** Logs every herring callback (crowd facts only: ordinals, times, sites). */
	struct FRecorder : public ICrowdAgentObserver
	{
		TArray<FTellEvent> Performed;
		TArray<FTellEvent> AcceptedEvents;
		TArray<FTellEvent> Dropped;
		TArray<FRunLeg> Runs;
		TArray<FTellEvent> Reversals;
		TArray<TPair<FTellEvent, FCrowdTraversalSite>> Traversals;
		int32 TraversalRefusals = 0;
		TArray<FBrace> Braces;
		TArray<FDecoyChoice> DecoyChoices;
		TArray<FDecoyHold> DecoyHolds;
		TArray<FDecoyChoice> DecoyRefusals;
		TArray<FBushEvent> Bush;
		TArray<TPair<uint32, double>> LegStarts;

		virtual void OnTellPerformed(uint32 Ordinal, double Now, ECrowdTell Tell) override { Performed.Add({ Ordinal, Now, Tell }); }
		virtual void OnTellAccepted(uint32 Ordinal, double Now, ECrowdTell Tell) override { AcceptedEvents.Add({ Ordinal, Now, Tell }); }
		virtual void OnTellDropped(uint32 Ordinal, double Now, ECrowdTell Tell) override { Dropped.Add({ Ordinal, Now, Tell }); }
		virtual void OnRunLeg(uint32 Ordinal, double Now, double BoutSec, const FVector& Goal, bool bPanic) override { Runs.Add({ Ordinal, Now, BoutSec, Goal, bPanic }); }
		virtual void OnTraverse(uint32 Ordinal, double Now, const FCrowdTraversalSite& Site, bool bClimb) override
		{
			Traversals.Add({ FTellEvent{ Ordinal, Now, bClimb ? ECrowdTell::Climb : ECrowdTell::Vault }, Site });
		}
		virtual void OnTraversalRefused(uint32 Ordinal, double Now, uint16 PointId) override { ++TraversalRefusals; }
		virtual void OnReversal(uint32 Ordinal, double Now) override { Reversals.Add({ Ordinal, Now, ECrowdTell::Reversal }); }
		virtual void OnBrace(uint32 Ordinal, double Now, bool bTriggered, double HoldSec, bool bCapped) override { Braces.Add({ Ordinal, Now, bTriggered, HoldSec, bCapped }); }
		virtual void OnDecoyCourtChosen(uint32 Ordinal, double Now, int32 Court, int32 ExcludedCourt) override { DecoyChoices.Add({ Ordinal, Now, Court, ExcludedCourt }); }
		virtual void OnDecoyHold(uint32 Ordinal, double Now, int32 Court, double HoldSec, bool bComplete) override { DecoyHolds.Add({ Ordinal, Now, Court, HoldSec, bComplete }); }
		virtual void OnDecoyRefused(uint32 Ordinal, double Now, int32 Court) override { DecoyRefusals.Add({ Ordinal, Now, Court, INDEX_NONE }); }
		virtual void OnBushEnter(uint32 Ordinal, double Now, uint16 BushId, double StaySec) override { Bush.Add({ Ordinal, Now, BushId, StaySec, true }); }
		virtual void OnBushExit(uint32 Ordinal, double Now, uint16 BushId) override { Bush.Add({ Ordinal, Now, BushId, 0.0, false }); }
		virtual void OnLegStarted(uint32 Ordinal, double Now, float Pace, double LegCm) override { LegStarts.Add({ Ordinal, Now }); }

		int32 CountPerformed(ECrowdTell Tell) const
		{
			int32 Count = 0;
			for (const FTellEvent& Event : Performed)
			{
				Count += Event.Tell == Tell ? 1 : 0;
			}
			return Count;
		}
	};
}

#endif // WITH_DEV_AUTOMATION_TESTS
