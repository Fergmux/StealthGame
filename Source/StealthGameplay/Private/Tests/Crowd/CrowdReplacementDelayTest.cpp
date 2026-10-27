// Story cnb-027 (production/epics/crowd-npc-behaviour/story-027-kill-vacancy-and-replacement.md): kill vacancy,
// victim-blind replacement delay and exit cancellation (GDD CNB-R9.2/9.3, F3, AC-CNB-08, AC-CNB-35; ADR-0016 section 6).
// The schedule core with a recording queue and an LSC stand-in host (exits are absorbed at once, sockets clear).
//   VictimBlindDelay  >= 1,236 kills each of player, NPC and informant victims (the class never reaches the schedule:
//                     the ledger input is the kill time only): for each pair of classes |d| <= 0.1, variance ratio in
//                     [0.8, 1.25] and two-sample KS D <= 0.1; the pooled mean is 16 +-2 s and p95 <= 32 s at defaults;
//                     no replacement earlier than CrowdReplacementMinDelaySec.
//   ExitCancelOnKill  200 kills at random times: each cancels exactly one exit scheduled within the next 60 s (or none
//                     when LSC R5.7's share bound binds, reported), the cancelled sockets are uniform (chi-square
//                     p > 0.01), every cancelled item resolves Voided(Revalidation) and is not rescheduled; an exiting
//                     NPC killed inside its alley adds a vacancy without counting a cancel.
//   NothingBeforeDeathWrite a kill noted with a death-write time in the future changes no exit before it and is not
//                     replaced before it + CrowdReplacementMinDelaySec.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "CrowdTestStats.h"
#include "Server/CrowdAmbientSchedule.h"
#include "Server/StealthTuningSubstructs.h"

namespace CrowdReplacementTestData
{
	struct FQueue : public ICrowdScheduleQueue
	{
		TArray<FCrowdScheduleItem> Items;
		TSet<uint32> Voided;
		TSet<uint32> ResolvedVoided;
		int32 Submitted = 0;
		virtual void Submit(const FCrowdScheduleItem& Item) override { Items.Add(Item); ++Submitted; }
		virtual void Void(uint32 ItemId) override { Voided.Add(ItemId); }
		void RunUntil(double Until, FCrowdAmbientSchedule& Schedule, ICrowdScheduleHost& Host, double& Now)
		{
			while (true)
			{
				int32 Best = INDEX_NONE;
				for (int32 Index = 0; Index < Items.Num(); ++Index)
				{
					if (Items[Index].Due <= Until && (Best == INDEX_NONE || Items[Index].Due < Items[Best].Due))
					{
						Best = Index;
					}
				}
				if (Best == INDEX_NONE)
				{
					break;
				}
				const FCrowdScheduleItem Item = Items[Best];
				Items.RemoveAtSwap(Best);
				Now = FMath::Max(Now, Item.Due);
				Schedule.Tick(Now);
				const bool bVoided = Voided.Contains(Item.Id);
				if (bVoided)
				{
					ResolvedVoided.Add(Item.Id);
				}
				Schedule.Resolve(Item.Id, bVoided ? ECrowdScheduleResult::VoidedRevalidation : ECrowdScheduleResult::Fired, Now, Host);
			}
			Now = Until;
			Schedule.Tick(Now);
		}
	};

	struct FHost : public ICrowdScheduleHost
	{
		FCrowdAmbientSchedule* Schedule = nullptr;
		TArray<int32> LastCycles;
		bool bAbsorbAtOnce = true;
		virtual bool StartAmbientExit(int32 Socket, int32 Cycle, double PickU, double Now) override
		{
			if (bAbsorbAtOnce)
			{
				Schedule->NoteAbsorbed(Cycle, Now);
			}
			else
			{
				LastCycles.Add(Cycle);
			}
			return true;
		}
		virtual bool IsSocketClear(int32 Socket, double Now) override { return true; }
		virtual bool RequestEmergence(int32 Socket, double Now) override { return true; }
	};

	/** A fixed deterministic sequence in [0, 1) (the test's kill times; never the crowd's streams). */
	struct FLcg
	{
		uint64 State;
		explicit FLcg(uint64 Seed) : State(Seed) {}
		double Next() { State = State * 6364136223846793005ull + 1442695040888963407ull; return static_cast<double>(State >> 11) / 9007199254740992.0; }
	};

	inline double Mean(const TArray<double>& V) { double S = 0; for (double X : V) { S += X; } return V.Num() ? S / V.Num() : 0.0; }
	inline double Var(const TArray<double>& V) { const double M = Mean(V); double S = 0; for (double X : V) { S += (X - M) * (X - M); } return V.Num() > 1 ? S / (V.Num() - 1) : 0.0; }
	inline double KsTwoSample(TArray<double> A, TArray<double> B)
	{
		A.Sort();
		B.Sort();
		int32 I = 0, J = 0;
		double D = 0.0;
		while (I < A.Num() && J < B.Num())
		{
			const double X = FMath::Min(A[I], B[J]);
			while (I < A.Num() && A[I] <= X) { ++I; }
			while (J < B.Num() && B[J] <= X) { ++J; }
			D = FMath::Max(D, FMath::Abs(static_cast<double>(I) / A.Num() - static_cast<double>(J) / B.Num()));
		}
		return D;
	}
	inline TFunction<uint64(uint32)> SubSeeds(uint64 Base) { return [Base](uint32 Counter) { return Base ^ (static_cast<uint64>(Counter) * 0xD6E8FEB86659FD93ull); }; }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdReplacementDelayTest, "StealthGame.Crowd.Replacement.VictimBlindDelay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdReplacementDelayTest::RunTest(const FString& Parameters)
{
	using namespace CrowdReplacementTestData;
	const FCrowdTuning_Server Tuning;
	FCrowdAmbientSchedule Schedule;
	FQueue Queue;
	FHost Host;
	Host.Schedule = &Schedule;
	double Now = 0.0;
	Schedule.Start(Now, 13, Tuning, 1u, Queue, SubSeeds(0xDE1A700000000001ull));
	FLcg Lcg(0x27C0FFEEull);
	const int32 PerClass = 1236;
	TArray<int32> ClassOf;
	// Kills about one every 2 minutes (exponential spacing; F3's single-vacancy delay), the class round-robin.
	for (int32 Kill = 0; Kill < 3 * PerClass; ++Kill)
	{
		const double At = Now - 120.0 * FMath::Loge(1.0 - Lcg.Next());
		Queue.RunUntil(At, Schedule, Host, Now);
		Schedule.NoteKill(Now, false);
		ClassOf.Add(Kill % 3);
	}
	Queue.RunUntil(Now + 600.0, Schedule, Host, Now);
	TArray<double> ByClass[3];
	TArray<double> Pooled;
	int32 Early = 0, Unserved = 0;
	const TConstArrayView<FCrowdKillVacancy> Vacancies = Schedule.GetVacancies();
	for (int32 Index = 0; Index < Vacancies.Num(); ++Index)
	{
		if (Vacancies[Index].ServedTime < 0.0)
		{
			++Unserved;
			continue;
		}
		const double D = Vacancies[Index].ServedTime - Vacancies[Index].KillTime;
		Early += D < Tuning.CrowdReplacementMinDelaySec - 1e-9 ? 1 : 0;
		ByClass[ClassOf[Index]].Add(D);
		Pooled.Add(D);
	}
	const double P95 = StealthCrowdStats::Quantile(Pooled, 0.95);
	int32 PairFails = 0;
	FString Pairs;
	const TCHAR* Names[] = { TEXT("player"), TEXT("NPC"), TEXT("informant") };
	for (int32 A = 0; A < 3; ++A)
	{
		for (int32 B = A + 1; B < 3; ++B)
		{
			const double Sd = FMath::Sqrt(0.5 * (Var(ByClass[A]) + Var(ByClass[B])));
			const double Cohen = (Mean(ByClass[A]) - Mean(ByClass[B])) / FMath::Max(Sd, 1e-9);
			const double Ratio = Var(ByClass[A]) / FMath::Max(Var(ByClass[B]), 1e-9);
			const double Ks = KsTwoSample(ByClass[A], ByClass[B]);
			const bool bOk = FMath::Abs(Cohen) <= 0.1 && Ratio >= 0.8 && Ratio <= 1.25 && Ks <= 0.1;
			PairFails += bOk ? 0 : 1;
			Pairs += FString::Printf(TEXT(" %s-%s d %.3f var ratio %.3f KS %.3f;"), Names[A], Names[B], Cohen, Ratio, Ks);
		}
	}
	AddInfo(FString::Printf(TEXT("REPLACEMENT %d kills (%d per class), served %d, unserved %d, fallbacks %d: pooled mean %.2f s, p95 %.2f s, earlier than %.0f s %d;%s"),
		Vacancies.Num(), PerClass, Pooled.Num(), Unserved, [&]() { int32 F = 0; for (const FCrowdKillVacancy& V : Vacancies) { F += V.bFallback ? 1 : 0; } return F; }(),
		Mean(Pooled), P95, Tuning.CrowdReplacementMinDelaySec, Early, *Pairs));
	TestTrue(TEXT("at least 1,236 kills per class were replaced"), ByClass[0].Num() >= PerClass - 5 && ByClass[1].Num() >= PerClass - 5 && ByClass[2].Num() >= PerClass - 5);
	TestEqual(TEXT("every class pair: |d| <= 0.1, variance ratio in [0.8, 1.25], KS D <= 0.1"), PairFails, 0);
	TestTrue(TEXT("the pooled mean is 16 +-2 s (F3)"), FMath::Abs(Mean(Pooled) - 16.0) <= 2.0);
	TestTrue(TEXT("p95 <= 32 s"), P95 <= 32.0);
	TestEqual(TEXT("no replacement earlier than CrowdReplacementMinDelaySec"), Early, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdExitCancelTest, "StealthGame.Crowd.Replacement.ExitCancelOnKill",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdExitCancelTest::RunTest(const FString& Parameters)
{
	using namespace CrowdReplacementTestData;
	const FCrowdTuning_Server Tuning;
	FCrowdAmbientSchedule Schedule;
	FQueue Queue;
	FHost Host;
	Host.Schedule = &Schedule;
	double Now = 0.0;
	const int32 Sockets = 13;
	Schedule.Start(Now, Sockets, Tuning, 1u, Queue, SubSeeds(0xC0CA7000000002ull));
	FLcg Lcg(0x35ull);
	const int32 Kills = 200;
	for (int32 Kill = 0; Kill < Kills; ++Kill)
	{
		Queue.RunUntil(Now + 10.0 + 100.0 * Lcg.Next(), Schedule, Host, Now);
		Schedule.NoteKill(Now, false);
	}
	// An exiting NPC killed inside its alley: a vacancy and no cancel.
	Host.bAbsorbAtOnce = false;
	Queue.RunUntil(Now + 30.0, Schedule, Host, Now);
	const int32 CancelsBefore = Schedule.GetCancelledExits();
	const int32 AlleyVacancy = Host.LastCycles.Num() > 0 ? Schedule.NoteKill(Now, true, Host.LastCycles.Last()) : INDEX_NONE;
	const int32 CancelsAfterAlley = Schedule.GetCancelledExits() - CancelsBefore;
	Queue.RunUntil(Now + 120.0, Schedule, Host, Now);
	int32 Cancelled = 0, OutOfWindow = 0, Unresolved = 0, Refused = 0;
	for (int32 Index = 0; Index < Kills; ++Index)
	{
		const FCrowdKillVacancy& Vacancy = Schedule.GetVacancies()[Index];
		if (Vacancy.CancelledExit == 0)
		{
			++Refused;
			continue;
		}
		++Cancelled;
		const double Due = Schedule.GetExitDueOf(Vacancy.CancelledExit);
		OutOfWindow += Due < Vacancy.KillTime || Due > Vacancy.KillTime + FCrowdAmbientSchedule::CancelWindowSec ? 1 : 0;
		Unresolved += Queue.ResolvedVoided.Contains(Vacancy.CancelledExit) ? 0 : 1;
	}
	TArray<double> Observed, Expected;
	Observed.SetNumZeroed(Sockets);
	for (const int32 Socket : Schedule.GetCancelledSockets())
	{
		Observed[Socket] += 1.0;
	}
	Expected.Init(static_cast<double>(Schedule.GetCancelledSockets().Num()) / Sockets, Sockets);
	double Chi = 0.0;
	const double P = StealthCrowdStats::ChiSquarePValue(Observed, Expected, &Chi);
	AddInfo(FString::Printf(TEXT("EXITCANCEL %d kills: one exit cancelled for %d (outside the 60 s window %d), none for %d (LSC R5.7 bound %d), cancelled items not resolved Voided %d; sockets chi2 %.2f p %.4f; alley kill: vacancy %d, cancels counted %d"),
		Kills, Cancelled, OutOfWindow, Refused, Schedule.GetCancelsRefusedByBound(), Unresolved, Chi, P, AlleyVacancy, CancelsAfterAlley));
	TestEqual(TEXT("each kill cancels exactly one exit scheduled within the next 60 s (or the bound refused it)"), Cancelled + Refused, Kills);
	TestEqual(TEXT("every cancelled exit lay in [t_k, t_k + 60 s]"), OutOfWindow, 0);
	TestEqual(TEXT("a kill without a cancel is always the LSC R5.7 share bound"), Refused, Schedule.GetCancelsRefusedByBound());
	TestEqual(TEXT("every cancelled item resolved Voided(Revalidation)"), Unresolved, 0);
	TestTrue(TEXT("cancelled exits are uniform over sockets (chi-square p > 0.01)"), P > 0.01);
	TestTrue(TEXT("an alley kill adds a vacancy"), AlleyVacancy != INDEX_NONE && Schedule.GetVacancies()[AlleyVacancy].bInAlley);
	TestEqual(TEXT("an alley kill counts no cancel"), CancelsAfterAlley, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdKillDeathWriteTest, "StealthGame.Crowd.Replacement.NothingBeforeDeathWrite",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdKillDeathWriteTest::RunTest(const FString& Parameters)
{
	using namespace CrowdReplacementTestData;
	const FCrowdTuning_Server Tuning;
	FCrowdAmbientSchedule Schedule;
	FQueue Queue;
	FHost Host;
	Host.Schedule = &Schedule;
	double Now = 0.0;
	Schedule.Start(Now, 13, Tuning, 1u, Queue, SubSeeds(0xDEA7Bull));
	int32 EarlyCancels = 0, EarlyServes = 0;
	for (int32 Kill = 0; Kill < 100; ++Kill)
	{
		Queue.RunUntil(Now + 37.0, Schedule, Host, Now);
		// The strike lands now; the victim's death write is scheduled 0.4 s later (ADR-0018): t_k is that time.
		const double DeathWrite = Now + 0.4;
		const int32 Vacancy = Schedule.NoteKill(DeathWrite, false);
		const FCrowdKillVacancy& V = Schedule.GetVacancies()[Vacancy];
		EarlyCancels += V.CancelledExit != 0 && Schedule.GetExitDueOf(V.CancelledExit) < DeathWrite ? 1 : 0;
	}
	Queue.RunUntil(Now + 300.0, Schedule, Host, Now);
	for (const FCrowdKillVacancy& V : Schedule.GetVacancies())
	{
		EarlyServes += V.ServedTime >= 0.0 && V.ServedTime < V.KillTime + Tuning.CrowdReplacementMinDelaySec ? 1 : 0;
	}
	AddInfo(FString::Printf(TEXT("DEATHWRITE 100 kills with a future death write: exits cancelled before it %d, replacements before it + %.0f s %d"), EarlyCancels, Tuning.CrowdReplacementMinDelaySec, EarlyServes));
	TestEqual(TEXT("no exit before the death write is changed by the kill"), EarlyCancels, 0);
	TestEqual(TEXT("no replacement before the death write + CrowdReplacementMinDelaySec"), EarlyServes, 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
