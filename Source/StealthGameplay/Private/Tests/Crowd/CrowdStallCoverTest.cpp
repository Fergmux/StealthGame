// Story cnb-014 (production/epics/crowd-npc-behaviour/story-014-pauses-and-stall-cover.md): pauses, the stall-cover
// regulator and the stationary-stretch bound (GDD CNB-R5 C2 and watchdog, F4, AC-CNB-12, AC-CNB-13; ADR-0016 section 3).
//   StallCover     10 min of the headless reference scene sampled every 1 s: at least ceil(0.1 * N) NPCs are in a
//                  stationary stretch already >= 4 s old in >= 98 % of the samples, and the regulated long-pause share
//                  never exceeds 90 % (the per-viewer share is a PIE metric, reported by the run-and-observe evidence).
//   StretchBound   60 min: no visible stationary stretch of any NPC exceeds CrowdMaxIdleSec (pause, conversation and
//                  reaction-pause chains included); a stretch's clock is suspended while the pawn is concealed.
//   PauseMix       >= 1,000 pauses with the regulator off (CrowdStallCoverMinPct 0, so the C2 shares are the
//                  configured ones): the short share in the 99 % binomial interval of 0.55 with lengths in U(1, 3) s, the
//                  long share around 0.45 with lengths in U(4, 16) s; a long pause ends facing its context within 3 m
//                  (look direction within 30 degrees, a provisional QA tolerance) when one exists.
//   Watchdog       a pawn whose stretch reaches the bound receives a Routing leg in that tick.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "CrowdTestStats.h"
#include "Server/CrowdStallRegulator.h"
#include "Tests/Crowd/CrowdSimWorld.h"

namespace CrowdStallCoverTestData
{
	struct FPauseRecorder final : public ICrowdAgentObserver
	{
		struct FPause
		{
			uint32 Ordinal;
			bool bLong;
			double Length;
			bool bContext;
			FVector Context;
		};
		TArray<FPause> Pauses;
		TMap<uint32, int32> Open;
		int32 ContextChecked = 0;
		int32 ContextFacing = 0;
		/** Pauses that ended inside a G4 or G6 glance (they outrank the contextual look): reported, not checked. */
		int32 EndedInGlance = 0;
		/** Long context pauses that turned into a conversation (C3 outranks the context): reported, not checked. */
		int32 BecameConversation = 0;
		TSet<uint32> Conversed;
		TMap<uint32, double> GlanceUntil;
		virtual void OnConverseDecision(uint32 Ordinal, double Now, uint64, bool, bool bDecided) override
		{
			if (bDecided && Open.Contains(Ordinal))
			{
				Conversed.Add(Ordinal);
			}
		}
		virtual void OnGlance(uint32 Ordinal, double Now, ECrowdGlanceKind Kind, uint64 TargetKey, double HoldSec) override
		{
			if (Kind == ECrowdGlanceKind::LongHold || Kind == ECrowdGlanceKind::Salient)
			{
				GlanceUntil.Add(Ordinal, Now + HoldSec);
			}
		}
		TFunction<FVector(uint32)> LocationOf;
		virtual void OnPauseStarted(uint32 Ordinal, double Now, bool bLong, double LengthSec, bool bHasContext, const FVector& ContextPoint) override
		{
			Open.Add(Ordinal, Pauses.Add({ Ordinal, bLong, LengthSec, bHasContext, ContextPoint }));
		}
		virtual void OnPauseEnded(uint32 Ordinal, double Now, float LookYawDeg) override
		{
			if (const int32* Index = Open.Find(Ordinal))
			{
				const FPause& Pause = Pauses[*Index];
				const double* Until = GlanceUntil.Find(Ordinal);
				if (Pause.bLong && Pause.bContext && Conversed.Contains(Ordinal))
				{
					++BecameConversation;
				}
				else if (Pause.bLong && Pause.bContext && Until != nullptr && Now < *Until)
				{
					++EndedInGlance;
				}
				else if (Pause.bLong && Pause.bContext && LocationOf)
				{
					const FVector Here = LocationOf(Ordinal);
					const float Bearing = static_cast<float>(FMath::RadiansToDegrees(FMath::Atan2(Pause.Context.Y - Here.Y, Pause.Context.X - Here.X)));
					++ContextChecked;
					ContextFacing += FMath::Abs(StealthLocomotion::DeltaYawDeg(LookYawDeg, Bearing)) <= 30.0f ? 1 : 0;
				}
				Open.Remove(Ordinal);
				Conversed.Remove(Ordinal);
			}
		}
	};

	/** Tracks every NPC's visible stationary stretch from movement state (the AC's own measure, independent of the regulator). */
	struct FStretchMeter
	{
		TMap<uint32, double> Start;
		/** The activities seen during each open stretch (bit per ECrowdActivity), for the longest stretch's report. */
		TMap<uint32, uint32> Seen;
		/** The activity changes of each open stretch ("t:activity"), for the longest stretch's report. */
		TMap<uint32, FString> Trace;
		TMap<uint32, ECrowdActivity> LastActivity;
		double Longest = 0.0;
		uint32 LongestActivities = 0;
		FString LongestTrace;
		void Sample(uint32 Ordinal, double Speed, bool bConcealed, double Now, double Dt, ECrowdActivity Activity = ECrowdActivity::Count)
		{
			double* Begin = Start.Find(Ordinal);
			if (bConcealed)
			{
				if (Begin != nullptr)
				{
					*Begin += Dt;
				}
				return;
			}
			if (Speed <= FCrowdStallRegulator::StationarySpeedCm)
			{
				uint32& Mask = Seen.FindOrAdd(Ordinal);
				Mask |= 1u << static_cast<uint32>(Activity);
				ECrowdActivity& Last = LastActivity.FindOrAdd(Ordinal, ECrowdActivity::Count);
				if (Begin == nullptr || Last != Activity)
				{
					Trace.FindOrAdd(Ordinal) += FString::Printf(TEXT(" %.2f:%d"), Begin != nullptr ? Now - *Begin : 0.0, static_cast<int32>(Activity));
					Last = Activity;
				}
				if (Begin == nullptr)
				{
					Start.Add(Ordinal, Now);
				}
				else if (Now - *Begin > Longest)
				{
					Longest = Now - *Begin;
					LongestActivities = Mask;
					LongestTrace = Trace.FindRef(Ordinal);
				}
			}
			else
			{
				Start.Remove(Ordinal);
				Seen.Remove(Ordinal);
				Trace.Remove(Ordinal);
				LastActivity.Remove(Ordinal);
			}
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdStallCoverTest, "StealthGame.Crowd.Pauses.StallCover",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdStallCoverTest::RunTest(const FString& Parameters)
{

	using namespace StealthCrowdSim;
	FConfig Config;
	FWorld World(Config);
	World.SpawnNpcs();
	World.StartRound();
	World.RunUntil(60.0); // warm-up: the stretch statistics settle after the round start
	int32 Samples = 0;
	int32 Covered = 0;
	double MaxLongPct = 0.0;
	const double Threshold = 4.0;
	const int32 Floor = FMath::CeilToInt32(Config.Tuning.CrowdStallCoverMinPct / 100.0 * World.NumNpcs());
	int32 MinCount = MAX_int32;
	int64 CountSum = 0;
	for (int32 Second = 0; Second < 600; ++Second)
	{
		World.RunUntil(60.0 + Second + 1.0);
		const int32 Count = World.GetStall().CountCovered(World.GetNow(), Threshold);
		MinCount = FMath::Min(MinCount, Count);
		CountSum += Count;
		Covered += Count >= Floor ? 1 : 0;
		++Samples;
		MaxLongPct = FMath::Max(MaxLongPct, World.GetLongPausePct(World.GetNow()));
	}
	const double Share = static_cast<double>(Covered) / Samples;
	AddInfo(FString::Printf(TEXT("STALLCOVER %d of %d samples with >= %d NPCs in a 4 s+ stretch (%.2f %%), lowest count %d, mean %.2f, highest regulated long share %.0f %%"),
		Covered, Samples, Floor, Share * 100.0, MinCount, static_cast<double>(CountSum) / Samples, MaxLongPct));
	TestTrue(TEXT(">= ceil(0.1 N) covered in >= 98 % of samples (AC-CNB-12)"), Share >= 0.98);
	TestTrue(TEXT("the regulator never sets the long share above 90 %"), MaxLongPct <= 90.0 + 1e-6);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdStretchBoundTest, "StealthGame.Crowd.Pauses.StretchBound",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdStretchBoundTest::RunTest(const FString& Parameters)
{
	using namespace CrowdStallCoverTestData;
	using namespace StealthCrowdSim;
	FConfig Config;
	FWorld World(Config);
	World.SpawnNpcs();
	World.StartRound();
	FStretchMeter Meter;
	const int32 Frames = 60 * 60 * 60;
	for (int32 Frame = 0; Frame < Frames; ++Frame)
	{
		World.Step();
		for (int32 Index = 0; Index < World.NumNpcs(); ++Index)
		{
			const FNpc& Npc = World.GetNpc(Index);
			Meter.Sample(Npc.Body.Ordinal, Npc.Body.Motion.Speed, !Npc.Body.bPerceivable, World.GetNow(), Config.StepSec, Npc.Agent.GetActivity());
		}
	}
	AddInfo(FString::Printf(TEXT("STRETCH longest visible stationary stretch over 60 min x %d NPCs: %.3f s (bound %.1f s), activities mask 0x%x, trace%s"), World.NumNpcs(), Meter.Longest, Config.Tuning.CrowdMaxIdleSec, Meter.LongestActivities, *Meter.LongestTrace));
	TestTrue(TEXT("no visible stationary stretch exceeds CrowdMaxIdleSec (AC-CNB-13)"), Meter.Longest <= Config.Tuning.CrowdMaxIdleSec);

	// The clock is suspended while concealed: 30 s concealed in the middle of a stand does not count.
	FCrowdStallRegulator Regulator;
	double Age = 0.0;
	for (int32 Frame = 0; Frame < 60 * 40; ++Frame)
	{
		const double Now = Frame / 60.0;
		const bool bConcealed = Now >= 5.0 && Now < 35.0;
		Age = Regulator.Update(1, 0.0, bConcealed, Now);
	}
	TestTrue(TEXT("40 s standing with 30 s concealed counts as about 10 s"), FMath::IsNearlyEqual(Age, 10.0, 0.05));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdPauseMixTest, "StealthGame.Crowd.Pauses.PauseMix",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdPauseMixTest::RunTest(const FString& Parameters)
{
	using namespace CrowdStallCoverTestData;
	using namespace StealthCrowdSim;
	FConfig Config;
	Config.Tuning.CrowdStallCoverMinPct = 0.0f; // the regulator off: the configured C2 shares
	FWorld World(Config);
	// Pause contexts (stalls with goods every 10 m along the grid): a long pause near one faces it.
	for (int32 X = -3; X <= 3; ++X)
	{
		for (int32 Y = -3; Y <= 3; ++Y)
		{
			World.AddContext({ FVector(X * 1000.0 + 150.0, Y * 1000.0, 0.0), (X + Y) % 2 == 0 });
		}
	}
	FPauseRecorder Recorder;
	Recorder.LocationOf = [&World](uint32 Ordinal) { const FNpc* Npc = World.FindNpc(Ordinal); return Npc != nullptr ? Npc->Body.Location : FVector::ZeroVector; };
	World.SetObserver(&Recorder);
	World.SpawnNpcs();
	World.StartRound();
	World.RunUntil(20.0 * 60.0);
	int32 Short = 0, Long = 0, ShortOut = 0, LongOut = 0;
	for (const FPauseRecorder::FPause& Pause : Recorder.Pauses)
	{
		if (Pause.bLong)
		{
			++Long;
			LongOut += Pause.Length < Config.Tuning.CrowdIdleMinSec || Pause.Length > Config.Tuning.CrowdIdleMaxSec ? 1 : 0;
		}
		else
		{
			++Short;
			ShortOut += Pause.Length < Config.Tuning.CrowdShortPauseMinSec || Pause.Length > Config.Tuning.CrowdShortPauseMaxSec ? 1 : 0;
		}
	}
	const int32 Total = Short + Long;
	double Low = 0.0, High = 0.0;
	const bool bShortShare = StealthCrowdStats::InBinomial99(Short, Total, 1.0 - Config.Tuning.CrowdLongPausePct / 100.0, &Low, &High);
	AddInfo(FString::Printf(TEXT("PAUSEMIX %d pauses: %d short (%.3f, 99 %% interval %.3f-%.3f), %d long; out-of-range lengths short %d long %d; context facing %d of %d (%d ended in a G4/G6 glance, %d became a conversation)"),
		Total, Short, Total > 0 ? static_cast<double>(Short) / Total : 0.0, Low, High, Long, ShortOut, LongOut, Recorder.ContextFacing, Recorder.ContextChecked, Recorder.EndedInGlance, Recorder.BecameConversation));
	TestTrue(TEXT(">= 1,000 pauses"), Total >= 1000);
	TestTrue(TEXT("the short share lies in the 99 % binomial interval of 0.55"), bShortShare);
	TestEqual(TEXT("short pauses in U(1, 3) s"), ShortOut, 0);
	TestEqual(TEXT("long pauses in U(4, 16) s"), LongOut, 0);
	TestTrue(TEXT("long pauses near a context were checked"), Recorder.ContextChecked > 20);
	TestEqual(TEXT("a long pause ends facing its context within 30 degrees"), Recorder.ContextFacing, Recorder.ContextChecked);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdWatchdogTest, "StealthGame.Crowd.Pauses.Watchdog",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdWatchdogTest::RunTest(const FString& Parameters)
{
	using namespace StealthCrowdSim;
	struct FWatch final : public ICrowdAgentObserver
	{
		int32 Fired = 0;
		double Age = 0.0;
		double LegAt = -1.0;
		double FiredAt = -1.0;
		virtual void OnWatchdog(uint32, double Now, double StretchAge) override { if (++Fired == 1) { Age = StretchAge; FiredAt = Now; } }
		virtual void OnLegStarted(uint32, double Now, float, double) override { if (FiredAt >= 0.0 && LegAt < 0.0) { LegAt = Now; } }
	} Watch;
	FConfig Config;
	Config.NpcCount = 1;
	// A pause longer than the bound: only the watchdog ends it.
	Config.Tuning.CrowdIdleMinSec = 25.0f;
	Config.Tuning.CrowdIdleMaxSec = 30.0f;
	Config.Tuning.CrowdLongPausePct = 60.0f;
	Config.Tuning.CrowdStallCoverMinPct = 0.0f;
	FWorld World(Config);
	World.SetObserver(&Watch);
	FNpc& Npc = World.SpawnNpcAt(FVector::ZeroVector, 0.0f);
	World.StartRound();
	World.RunUntil(120.0);
	AddInfo(FString::Printf(TEXT("WATCHDOG fired %d times, stretch %.2f s at fire, next leg %.3f s later"), Watch.Fired, Watch.Age, Watch.LegAt - Watch.FiredAt));
	TestTrue(TEXT("the watchdog fired"), Watch.Fired > 0);
	TestTrue(TEXT("before the stretch reaches CrowdMaxIdleSec"), Watch.Age < Config.Tuning.CrowdMaxIdleSec);
	TestTrue(TEXT("the Routing leg starts in that tick"), FMath::IsNearlyZero(Watch.LegAt - Watch.FiredAt, 1e-9));
	(void)Npc;
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
