// Story cnb-026 (production/epics/crowd-npc-behaviour/story-026-ambient-schedule-and-emergence.md): the ambient schedule
// as queue items and witness-free emergence (GDD CNB-R9.1, F3, AC-CNB-07, AC-CNB-09; ADR-0016 section 6).
//   PoissonCycles        >= 1,000 exit gaps over 13 sockets: KS against Exp(mean 80 s / 13) p > 0.01 and chi-square of
//                        the socket choice against uniform p > 0.01; every item goes to the queue seam with the round epoch
//                        (no engine timer); a blocked entry retries at +EmergenceRetrySec; Rejected_Latched,
//                        Rejected_StaleEpoch and Voided items are never rescheduled.
//   SpawnPathAndRates    (AC-CNB-07) a 10-minute crowd simulation with 13 sockets and 20 scripted kills of mixed victim
//                        kinds (2 a minute): every mid-round spawn is a RequestEmergence and every removal an absorption or
//                        a kill; exits and entries average >= 0.5 per socket-minute; the population ledger balances.
//   EmergenceNeverWitnessed (AC-CNB-09) 10 minutes with 10 scripted viewers walking: no emergence or absorption happens
//                        while any pawn's eye sample sees it, and watched exits give up after CrowdExitGiveUpSec.
// The sim stands in for the Level Socket Contract (sockets, the clear check, RequestEmergence through the factory) and
// the Character Pool's DrawReplacement, neither of which is built yet.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "CrowdTestStats.h"
#include "Tests/Crowd/CrowdSimWorld.h"

namespace CrowdAmbientTestData
{
	/** A recording queue: items fire in due order unless voided; the round epoch is 1; Latch() latches the round. */
	struct FFakeQueue : public ICrowdScheduleQueue
	{
		TArray<FCrowdScheduleItem> Items;
		TSet<uint32> Voided;
		int32 Submitted = 0;
		int32 WrongEpoch = 0;
		bool bLatched = false;
		virtual void Submit(const FCrowdScheduleItem& Item) override
		{
			Items.Add(Item);
			++Submitted;
			WrongEpoch += Item.Epoch != 1u ? 1 : 0;
		}
		virtual void Void(uint32 ItemId) override { Voided.Add(ItemId); }
		/** Fires every item due by Until in order; returns the items resolved. */
		int32 RunUntil(double Until, FCrowdAmbientSchedule& Schedule, ICrowdScheduleHost& Host, double& Now)
		{
			int32 Resolved = 0;
			while (true)
			{
				Items.StableSort([](const FCrowdScheduleItem& A, const FCrowdScheduleItem& B) { return A.Due < B.Due; });
				if (Items.Num() == 0 || Items[0].Due > Until)
				{
					break;
				}
				const FCrowdScheduleItem Item = Items[0];
				Items.RemoveAt(0);
				Now = FMath::Max(Now, Item.Due);
				Schedule.Tick(Now);
				const ECrowdScheduleResult Result = Voided.Contains(Item.Id) ? ECrowdScheduleResult::VoidedRevalidation
					: (bLatched ? ECrowdScheduleResult::RejectedLatched : ECrowdScheduleResult::Fired);
				Schedule.Resolve(Item.Id, Result, Now, Host);
				++Resolved;
			}
			Now = Until;
			Schedule.Tick(Now);
			return Resolved;
		}
	};

	/** A host that always finds an exiter (absorbed at once) and a clear socket, except BlockNext entries. */
	struct FFakeHost : public ICrowdScheduleHost
	{
		FCrowdAmbientSchedule* Schedule = nullptr;
		int32 BlockNext = 0;
		TArray<double> BlockedAt, RetriedAt;
		int32 Emerged = 0;
		double* NowRef = nullptr;
		virtual bool StartAmbientExit(int32 Socket, int32 Cycle, double PickU, double Now) override
		{
			Schedule->NoteAbsorbed(Cycle, Now);
			return true;
		}
		virtual bool IsSocketClear(int32 Socket, double Now) override
		{
			if (BlockNext > 0)
			{
				--BlockNext;
				BlockedAt.Add(Now);
				return false;
			}
			return true;
		}
		virtual bool RequestEmergence(int32 Socket, double Now) override
		{
			++Emerged;
			return true;
		}
	};

	inline void AddEdgeSockets(StealthCrowdSim::FWorld& World, int32 Count, double HalfCm)
	{
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const double Angle = 2.0 * UE_DOUBLE_PI * Index / Count;
			const FVector Out(FMath::Cos(Angle), FMath::Sin(Angle), 0.0);
			const FVector Edge = Out * HalfCm * 0.95;
			World.AddSocket(Edge - Out * 150.0, Edge);
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdAmbientPoissonTest, "StealthGame.Crowd.Ambient.PoissonCycles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdAmbientPoissonTest::RunTest(const FString& Parameters)
{
	using namespace CrowdAmbientTestData;
	const FCrowdTuning_Server Tuning;
	FCrowdAmbientSchedule Schedule;
	FFakeQueue Queue;
	FFakeHost Host;
	Host.Schedule = &Schedule;
	double Now = 0.0;
	const int32 Sockets = 13;
	Schedule.Start(Now, Sockets, Tuning, 1u, Queue, [](uint32 Counter) { return 0xA5A5000000000000ull + Counter * 0x9E3779B97F4A7C15ull; });
	Queue.RunUntil(3.0 * 3600.0, Schedule, Host, Now);
	TArray<double> Gaps;
	const TConstArrayView<double> Times = Schedule.GetExitTimes();
	for (int32 Index = 1; Index < Times.Num() && Times[Index] <= Now; ++Index)
	{
		Gaps.Add(Times[Index] - Times[Index - 1]);
	}
	const double Mean = Tuning.CrowdAmbientCycleIntervalSec / Sockets;
	double D = 0.0;
	const double KsP = StealthCrowdStats::KsPValue(Gaps, [Mean](double X) { return 1.0 - FMath::Exp(-X / Mean); }, &D);
	TArray<double> Observed, Expected;
	int32 Total = 0;
	for (const int32 Count : Schedule.GetExitsPerSocket())
	{
		Observed.Add(Count);
		Total += Count;
	}
	Expected.Init(static_cast<double>(Total) / Sockets, Sockets);
	double Chi = 0.0;
	const double ChiP = StealthCrowdStats::ChiSquarePValue(Observed, Expected, &Chi);
	// The retry: the next two entries find the socket blocked.
	Host.BlockNext = 2;
	const int32 RetriesBefore = Schedule.GetRetries();
	Queue.RunUntil(Now + 120.0, Schedule, Host, Now);
	const int32 Retries = Schedule.GetRetries() - RetriesBefore;
	int32 RetryItemsAtPlusOne = 0;
	for (const FCrowdScheduleItem& Item : Queue.Items)
	{
		RetryItemsAtPlusOne += Item.Retries > 0 ? 1 : 0;
	}
	// Latched: everything resolves Rejected_Latched and nothing is rescheduled.
	Queue.bLatched = true;
	const int32 SubmittedBefore = Queue.Submitted;
	const int32 RejectedBefore = Schedule.GetRejectedItems();
	const int32 Pending = Queue.Items.Num();
	Schedule.Halt();
	Queue.RunUntil(Now + 3600.0, Schedule, Host, Now);
	AddInfo(FString::Printf(TEXT("AMBIENT %d exit gaps over %d sockets: mean %.2f s (expected %.2f), KS D %.4f p %.4f; sockets chi2 %.2f p %.4f; blocked-entry retries %d; latched: %d pending items rejected, %d submitted after"),
		Gaps.Num(), Sockets, Gaps.Num() > 0 ? (Times[Gaps.Num()] - Times[0]) / Gaps.Num() : 0.0, Mean, D, KsP, Chi, ChiP, Retries, Schedule.GetRejectedItems() - RejectedBefore,
		Queue.Submitted - SubmittedBefore));
	TestTrue(TEXT("at least 1,000 exit gaps"), Gaps.Num() >= 1000);
	TestTrue(TEXT("exit gaps are exponential with mean CrowdAmbientCycleIntervalSec / N_E (KS p > 0.01)"), KsP > 0.01);
	TestTrue(TEXT("each slot's socket is uniform (chi-square p > 0.01)"), ChiP > 0.01);
	TestEqual(TEXT("every item carries the round epoch"), Queue.WrongEpoch, 0);
	TestEqual(TEXT("a blocked entry retries (twice for two blocks)"), Retries, 2);
	TestTrue(TEXT("each retry was a new queue item"), Host.BlockedAt.Num() == 2);
	TestEqual(TEXT("Rejected_Latched items are never rescheduled"), Queue.Submitted - SubmittedBefore, 0);
	TestEqual(TEXT("every pending item resolved Rejected"), Schedule.GetRejectedItems() - RejectedBefore, Pending);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdAmbientSpawnPathTest, "StealthGame.Crowd.Ambient.SpawnPathAndRates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdAmbientSpawnPathTest::RunTest(const FString& Parameters)
{
	using namespace CrowdAmbientTestData;
	using namespace StealthCrowdSim;
	FConfig Config;
	Config.HalfSizeCm = 3000.0;
	FWorld World(Config);
	const int32 Sockets = 13;
	AddEdgeSockets(World, Sockets, Config.HalfSizeCm);
	World.SpawnNpcs();
	const int32 Viewers = 2;
	for (int32 Index = 0; Index < Viewers; ++Index)
	{
		World.AddScripted(FVector(200.0 * Index, 0.0, 0.0), 0.0f);
	}
	World.StartRound();
	const int32 Initial = World.NumNpcs();
	int32 Kills = 0, KilledNpcs = 0, KilledPlayers = 0;
	double NextKill = 30.0;
	while (World.GetNow() < 600.0)
	{
		World.Step();
		if (World.GetNow() >= NextKill && Kills < 20)
		{
			NextKill += 30.0;
			// Mixed victim kinds: an NPC, then a player-like pawn; the schedule only ever sees the time.
			if (Kills % 3 == 2 && KilledPlayers < Viewers)
			{
				World.KillScripted(KilledPlayers++);
			}
			else
			{
				for (int32 Index = 0; Index < World.NumNpcs(); ++Index)
				{
					if (!World.GetNpc(Index).Body.bGone)
					{
						World.KillNpc(Index);
						++KilledNpcs;
						break;
					}
				}
			}
			++Kills;
		}
	}
	int32 Alive = 0;
	for (int32 Index = 0; Index < World.NumNpcs(); ++Index)
	{
		Alive += World.GetNpc(Index).Body.bGone ? 0 : 1;
	}
	const double Minutes = 10.0;
	int32 Exits = 0, Entries = 0;
	for (const int32 Count : World.GetSchedule().GetExitsPerSocket())
	{
		Exits += Count;
	}
	for (const int32 Count : World.GetSchedule().GetEntriesPerSocket())
	{
		Entries += Count;
	}
	const int32 Spawned = World.NumNpcs() - Initial;
	AddInfo(FString::Printf(TEXT("SPAWNPATH %.0f min, %d sockets, %d kills (%d NPC, %d player): exit slots %d (%.3f per socket-minute; exiters found %d), absorptions %d, entries %d (%.3f per socket-minute), spawns %d all via RequestEmergence %d; ledger %d initial - %d absorbed - %d killed + %d emerged = %d alive"),
		Minutes, Sockets, Kills, KilledNpcs, KilledPlayers, Exits, Exits / (Sockets * Minutes), World.ExitsStarted, World.Absorptions, Entries, Entries / (Sockets * Minutes),
		Spawned, World.Emergences, Initial, World.Absorptions, KilledNpcs, World.Emergences, Alive));
	TestEqual(TEXT("20 scripted kills (2 a minute)"), Kills, 20);
	TestEqual(TEXT("every mid-round spawn went through RequestEmergence"), Spawned, World.Emergences);
	TestTrue(TEXT("exit slots average >= 0.5 per socket-minute"), Exits / (Sockets * Minutes) >= 0.5);
	TestTrue(TEXT("entries average >= 0.5 per socket-minute"), Entries / (Sockets * Minutes) >= 0.5);
	TestEqual(TEXT("the ledger balances: initial - absorbed - killed + emerged = alive"), Initial - World.Absorptions - KilledNpcs + World.Emergences, Alive);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdAmbientWitnessTest, "StealthGame.Crowd.Ambient.EmergenceNeverWitnessed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdAmbientWitnessTest::RunTest(const FString& Parameters)
{
	using namespace CrowdAmbientTestData;
	using namespace StealthCrowdSim;
	FConfig Config;
	Config.HalfSizeCm = 3000.0;
	FWorld World(Config);
	AddEdgeSockets(World, 13, Config.HalfSizeCm);
	World.SpawnNpcs();
	// Ten bot viewers walking routes that sweep the plane (their eyes are the witness samples, like every NPC's).
	const int32 Viewers = 10;
	for (int32 Index = 0; Index < Viewers; ++Index)
	{
		FScripted& Viewer = World.GetScripted(World.AddScripted(FVector(-2000.0 + 400.0 * Index, -1500.0 + 300.0 * Index, 0.0), static_cast<float>(Index * 36)));
		Viewer.Body.MoveIntent = FVector2D(0.0, 0.8);
	}
	World.StartRound();
	while (World.GetNow() < 600.0)
	{
		for (int32 Index = 0; Index < Viewers; ++Index)
		{
			FBody& Body = World.GetScripted(Index).Body;
			if (Body.Location.Size2D() > 2600.0)
			{
				Body.Control.Yaw = (-Body.Location).Rotation().Yaw + 25.0 * (Index % 3);
			}
		}
		World.Step();
	}
	int32 Witnessed = 0, Emergences = 0, Absorptions = 0;
	for (const FSocketEvent& Event : World.GetSocketEvents())
	{
		Witnessed += Event.bWitnessed ? 1 : 0;
		(Event.bEmergence ? Emergences : Absorptions) += 1;
	}
	double ShortestGiveUp = 1.0e9;
	for (const double After : World.GiveUpAfterSec)
	{
		ShortestGiveUp = FMath::Min(ShortestGiveUp, After);
	}
	AddInfo(FString::Printf(TEXT("WITNESS 10 min, %d viewers: %d emergences, %d absorptions, witnessed %d; exits started %d, given up (watched) %d, shortest give-up %.2f s after the exit started (CrowdExitGiveUpSec %.1f)"),
		Viewers, Emergences, Absorptions, Witnessed, World.ExitsStarted, World.GiveUpAfterSec.Num(), World.GiveUpAfterSec.Num() > 0 ? ShortestGiveUp : 0.0, World.GetCrowdTuning().CrowdExitGiveUpSec));
	TestTrue(TEXT("emergences and absorptions happen"), Emergences > 20 && Absorptions > 20);
	TestEqual(TEXT("no emergence or absorption within any pawn's eye sample"), Witnessed, 0);
	TestTrue(TEXT("watched exits give up"), World.GiveUpAfterSec.Num() > 0);
	TestTrue(TEXT("a watched exit gives up only after CrowdExitGiveUpSec"), ShortestGiveUp >= World.GetCrowdTuning().CrowdExitGiveUpSec - 1e-6);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
