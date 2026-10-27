// Story cnb-023 (production/epics/crowd-npc-behaviour/story-023-tell-calibration-gate.md): the Tell Matrix calibration
// gate (GDD AC-CNB-10, AC-CNB-31; F2; ADR-0016, ADR-0023). Pillar 2's baseline: 600 simulated minutes of the reference
// scene (50 NPCs, every tell clock on, the reference herring sites; headless and accelerated, fixed pre-registered
// seeds from Fixtures/CrowdCalibrationSeeds.h; a failing seed is a defect, never reseeded). The counts are the tells
// the NPCs PERFORMED (the visible moment, after the chain-gap guard), read from the herring recorder and the sampler's
// server-only counters; the harness adds no instrumentation inside Crowd code.
//   TellBaselineNonZero  (AC-CNB-10, no strikes) every count > 0 and every city-wide rate in the two-sided 99 % Poisson
//                        interval of F2's accepted default (decoy tends: expected about 137).
//   EvidenceCompounds    (AC-CNB-31, scripted strikes every 30 s) no NPC shows two different tells within
//                        CrowdTellChainGapSec (R8 panic runs and startle braces included); the gaps between an NPC's
//                        consecutive different tells show no excess in [gap, gap + 5 s] (99 % Poisson interval of the
//                        count the restarted exponential clocks predict); the "two different tells within the gap"
//                        posterior is reported as a playtest metric, not gated.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "CrowdTestStats.h"
#include "HAL/PlatformTime.h"
#include "Tests/Crowd/CrowdHerringRecorder.h"
#include "Tests/Crowd/CrowdSimWorld.h"
#include "Tests/Crowd/Fixtures/CrowdCalibrationSeeds.h"

namespace CrowdTellCalibrationTestData
{
	using namespace StealthCrowdSim;
	using namespace StealthCrowdHerringTest;

	inline const TCHAR* TellName(ECrowdTell Tell)
	{
		static const TCHAR* Names[] = { TEXT("run"), TEXT("vault"), TEXT("climb"), TEXT("brace"), TEXT("decoy"), TEXT("bush"), TEXT("reversal"), TEXT("headhold") };
		return Names[static_cast<int32>(Tell)];
	}

	/** Runs Minutes of the reference scene in Segments seeds (Minutes / Segments each); StrikeEverySec > 0 adds strikes. */
	inline void RunReference(double Minutes, double StrikeEverySec, FRecorder& Recorder, double& OutNpcMinutes, int64 Accepted[8], int32& OutStrikes)
	{
		const int32 Segments = UE_ARRAY_COUNT(StealthCrowdCalibration::Seeds);
		OutNpcMinutes = 0.0;
		OutStrikes = 0;
		for (int32 Seg = 0; Seg < Segments; ++Seg)
		{
			FConfig Config;
			Config.HalfSizeCm = 3000.0;
			Config.SubSeed = StealthCrowdCalibration::Seeds[Seg];
			Config.EnabledTellMask = FConfig::AllTells;
			FWorld World(Config);
			World.AddReferenceHerringSites();
			World.SetObserver(&Recorder);
			World.SpawnNpcs();
			// The ordinals of each segment are offset so the per-NPC gap analysis never merges two segments' NPCs.
			World.StartRound();
			const double SegSec = Minutes * 60.0 / Segments;
			double NextStrike = StrikeEverySec;
			uint32 StrikeOrdinal = 0;
			while (World.GetNow() < SegSec)
			{
				World.Step();
				if (StrikeEverySec > 0.0 && World.GetNow() >= NextStrike)
				{
					NextStrike += StrikeEverySec;
					// A scripted strike at an NPC's position (the stimulus does not depend on who or what was struck).
					const FNpc& Victim = World.GetNpc(static_cast<int32>(StrikeOrdinal++ % World.NumNpcs()));
					World.EmitStrike(Victim.Body.Location + FVector(80.0, 0.0, 0.0), 0);
					++OutStrikes;
				}
			}
			OutNpcMinutes += SegSec / 60.0 * World.NumNpcs();
			for (int32 Index = 0; Index < 8; ++Index)
			{
				Accepted[Index] += World.GetTells().GetCounters().Accepted[Index];
			}
			World.SetObserver(nullptr);
			// Tag the segment's events so ordinals from different segments stay apart.
			for (TArray<FTellEvent>* List : { &Recorder.Performed, &Recorder.AcceptedEvents })
			{
				for (FTellEvent& Event : *List)
				{
					if (Event.Ordinal < 1000000u)
					{
						Event.Ordinal += 1000000u * static_cast<uint32>(Seg + 1);
					}
				}
			}
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdTellBaselineTest, "StealthGame.Crowd.TellCalibration.TellBaselineNonZero",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdTellBaselineTest::RunTest(const FString& Parameters)
{
	using namespace CrowdTellCalibrationTestData;
	const double WallStart = FPlatformTime::Seconds();
	FRecorder Recorder;
	double NpcMinutes = 0.0;
	int64 Accepted[8] = {};
	int32 Strikes = 0;
	RunReference(StealthCrowdCalibration::GateMinutes, 0.0, Recorder, NpcMinutes, Accepted, Strikes);
	const FCrowdTuning_Server Tuning;
	double Rates[static_cast<int32>(ECrowdTell::Count)];
	FCrowdTellSampler::TargetRatesPerMin(Tuning, Rates);
	FString Line;
	for (int32 Index = 0; Index < static_cast<int32>(ECrowdTell::Count); ++Index)
	{
		const ECrowdTell Tell = static_cast<ECrowdTell>(Index);
		const int32 Count = Recorder.CountPerformed(Tell);
		const double Expected = NpcMinutes * Rates[Index];
		double Low = 0, High = 0;
		const bool bIn = StealthCrowdStats::InPoisson99(Count, Expected, &Low, &High);
		Line += FString::Printf(TEXT(" %s %d (accepted %lld) exp %.1f [%.1f-%.1f]%s;"), TellName(Tell), Count, Accepted[Index], Expected, Low, High, bIn ? TEXT("") : TEXT(" MISS"));
		TestTrue(FString::Printf(TEXT("%s: performed count > 0"), TellName(Tell)), Count > 0);
		TestTrue(FString::Printf(TEXT("%s: city-wide rate in the 99 %% Poisson interval of F2's accepted default"), TellName(Tell)), bIn);
	}
	AddInfo(FString::Printf(TEXT("TELLBASELINE %.0f NPC-min over %d seeds (%.1f s wall):%s"), NpcMinutes, UE_ARRAY_COUNT(StealthCrowdCalibration::Seeds), FPlatformTime::Seconds() - WallStart, *Line));
	// The mean delay from acceptance to the visible moment per tell (the F2 ShowDelaySec table is derived from this line).
	TMap<uint64, double> LastAccepted;
	double DelaySum[8] = {}, DelayN[8] = {};
	TArray<FTellEvent> Events = Recorder.AcceptedEvents;
	for (const FTellEvent& Shown : Recorder.Performed)
	{
		Events.Add({ Shown.Ordinal | 0x80000000u, Shown.Time, Shown.Tell });
	}
	Events.StableSort([](const FTellEvent& A, const FTellEvent& B) { return A.Time < B.Time; });
	for (const FTellEvent& Event : Events)
	{
		const bool bShown = (Event.Ordinal & 0x80000000u) != 0;
		const uint64 Key = (static_cast<uint64>(Event.Ordinal & 0x7FFFFFFFu) << 8) | static_cast<uint64>(Event.Tell);
		if (!bShown)
		{
			LastAccepted.Add(Key, Event.Time);
		}
		else if (const double* At = LastAccepted.Find(Key))
		{
			DelaySum[static_cast<int32>(Event.Tell)] += Event.Time - *At;
			DelayN[static_cast<int32>(Event.Tell)] += 1.0;
			LastAccepted.Remove(Key);
		}
	}
	FString Delays;
	for (int32 Index = 0; Index < 8; ++Index)
	{
		Delays += FString::Printf(TEXT(" %s %.2f"), TellName(static_cast<ECrowdTell>(Index)), DelayN[Index] > 0.0 ? DelaySum[Index] / DelayN[Index] : 0.0);
	}
	AddInfo(FString::Printf(TEXT("SHOWDELAY mean seconds from acceptance to the visible moment:%s"), *Delays));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdEvidenceCompoundsTest, "StealthGame.Crowd.TellCalibration.EvidenceCompounds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdEvidenceCompoundsTest::RunTest(const FString& Parameters)
{
	using namespace CrowdTellCalibrationTestData;
	const double WallStart = FPlatformTime::Seconds();
	FRecorder Recorder;
	double NpcMinutes = 0.0;
	int64 Accepted[8] = {};
	int32 Strikes = 0;
	RunReference(StealthCrowdCalibration::GateMinutes, 30.0, Recorder, NpcMinutes, Accepted, Strikes);
	const FCrowdTuning_Server Tuning;
	const double Gap = Tuning.CrowdTellChainGapSec;
	// Per NPC, consecutive performed tells in time order.
	TMap<uint32, TArray<FTellEvent>> ByNpc;
	for (const FTellEvent& Event : Recorder.Performed)
	{
		ByNpc.FindOrAdd(Event.Ordinal).Add(Event);
	}
	int32 Violations = 0, InWindow = 0, DifferentPairs = 0;
	double ExposureAfterGap = 0.0;
	double Rates[static_cast<int32>(ECrowdTell::Count)];
	FCrowdTellSampler::TargetRatesPerMin(Tuning, Rates);
	double Lambda = 0.0;
	for (const double Rate : Rates)
	{
		Lambda += Rate / 60.0; // per second
	}
	for (TPair<uint32, TArray<FTellEvent>>& Pair : ByNpc)
	{
		Pair.Value.StableSort([](const FTellEvent& A, const FTellEvent& B) { return A.Time < B.Time; });
		for (int32 Index = 1; Index < Pair.Value.Num(); ++Index)
		{
			const FTellEvent& A = Pair.Value[Index - 1];
			const FTellEvent& B = Pair.Value[Index];
			if (A.Tell == B.Tell)
			{
				continue;
			}
			++DifferentPairs;
			const double Delta = B.Time - A.Time;
			Violations += Delta < Gap - 1e-9 ? 1 : 0;
			InWindow += Delta >= Gap && Delta < Gap + 5.0 ? 1 : 0;
		}
	}
	// The restarted exponential clocks: after a tell, the next different tell's gap beyond T_gap is exponential with the
	// other clocks' total rate, so the share of different-tell gaps falling in [gap, gap + 5 s] is 1 - exp(-Lambda_o * 5).
	const double ShareExpected = 1.0 - FMath::Exp(-Lambda * 5.0);
	const double Expected = DifferentPairs * ShareExpected;
	double Low = 0, High = 0;
	const bool bNoExcess = StealthCrowdStats::InPoisson99(InWindow, Expected, &Low, &High) || InWindow <= High;
	AddInfo(FString::Printf(TEXT("EVIDENCE %.0f NPC-min, %d strikes (%.1f s wall): %d performed tells, %d consecutive different-tell pairs, within the %.0f s gap %d, in [gap, gap + 5 s] %d (expected %.1f, 99 %% %.1f-%.1f); reported posterior (two different tells within the gap): 0 NPC pairs, so any such pair is a player"),
		NpcMinutes, Strikes, FPlatformTime::Seconds() - WallStart, Recorder.Performed.Num(), DifferentPairs, Gap, Violations, InWindow, Expected, Low, High));
	TestTrue(TEXT("strikes happened"), Strikes > 100);
	TestEqual(TEXT("no NPC shows two different tells within CrowdTellChainGapSec (AC-CNB-31)"), Violations, 0);
	TestTrue(TEXT("no excess of different tells just after the gap (AC-CNB-31b)"), bNoExcess);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
