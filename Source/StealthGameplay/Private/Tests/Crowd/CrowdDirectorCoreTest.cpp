// Story cnb-004 (production/epics/crowd-npc-behaviour/story-004-director-core-ledger-and-phase-gating.md): the Crowd
// Director core (ADR-0016 sections 1, 3, 7; GDD CNB-R1, R12, AC-CNB-33).
//   ServerOnlyHeadless    the Director is never created on a client; FCrowdDirectorCore runs with no UWorld; the
//                         StateTree.Component.ScheduledTickEnabled assertion reads the cvar (and sees it turned off).
//   PhaseGating           nothing is started before OnRoundActive; at it every NPC starts in one server step; first pause
//                         and leg lengths pooled over 20 fixed-seed rounds pass a KS test at p > 0.01 (no unison).
//   DecisionSpread        no frame holds more than ceil(N / (FPS / CrowdDecisionPerSec)) + 2 grid decisions (60 s, 60 Hz).
//   Ledger                NpcCount - V - B + Rel after scripted kills, takeovers and releases, recording kill times only;
//                         NavNotReady keeps every pawn Dormant.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "CrowdTestStats.h"
#include "HAL/IConsoleManager.h"
#include "Server/CrowdDirector.h"
#include "Server/CrowdDirectorCore.h"

namespace CrowdDirectorCoreTestData
{
	constexpr uint64 FixtureSubSeed = 0x00DECAF000000004ull;
	constexpr int32 Npcs = 50;
	constexpr int32 Rounds = 20;

	/** Records every StartAgent call (the "command log" of the round start). */
	struct FRecordingStarter final : public ICrowdAgentStarter
	{
		TArray<TPair<uint32, FCrowdFirstActivity>> Calls;
		TSet<double> Steps;
		virtual void StartAgent(uint32 Ordinal, const FCrowdFirstActivity& First, double Now) override
		{
			Calls.Add({ Ordinal, First });
			Steps.Add(Now);
		}
	};

	TUniquePtr<FCrowdDirectorCore> MakeCore(uint64 SubSeed, int32 Count)
	{
		TUniquePtr<FCrowdDirectorCore> Core = MakeUnique<FCrowdDirectorCore>(FCrowdTuning_Server(), SubSeed);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Core->RegisterAgent(static_cast<uint32>(Index), ECrowdDriverKind::Full);
		}
		return Core;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdDirectorServerOnlyHeadlessTest, "StealthGame.Crowd.Director.ServerOnlyHeadless",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdDirectorServerOnlyHeadlessTest::RunTest(const FString& Parameters)
{
	TestFalse(TEXT("no Director on a client"), UCrowdDirector::ShouldCreateForNetMode(NM_Client));
	TestTrue(TEXT("a Director on a listen server"), UCrowdDirector::ShouldCreateForNetMode(NM_ListenServer));
	TestTrue(TEXT("a Director on a dedicated server"), UCrowdDirector::ShouldCreateForNetMode(NM_DedicatedServer));
	TestTrue(TEXT("a Director standalone"), UCrowdDirector::ShouldCreateForNetMode(NM_Standalone));

	// The core needs no UWorld.
	FCrowdDirectorCore Core(FCrowdTuning_Server(), CrowdDirectorCoreTestData::FixtureSubSeed);
	Core.RegisterAgent(7, ECrowdDriverKind::Full);
	TestEqual(TEXT("a headless core holds an agent"), Core.NumAgents(), 1);
	TestEqual(TEXT("and is Dormant"), static_cast<int32>(Core.GetPhase()), static_cast<int32>(ECrowdDirectorPhase::Dormant));

	// The ScheduledTickEnabled assertion reads the real cvar: on by default, and the check sees it turned off.
	IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(TEXT("StateTree.Component.ScheduledTickEnabled"));
	if (TestNotNull(TEXT("the StateTree cvar exists"), Variable))
	{
		const bool bWas = Variable->GetBool();
		TestTrue(TEXT("ScheduledTickEnabled is on"), UCrowdDirector::IsScheduledTickEnabled());
		Variable->Set(false, ECVF_SetByCode);
		TestFalse(TEXT("the assertion sees it off"), UCrowdDirector::IsScheduledTickEnabled());
		Variable->Set(bWas, ECVF_SetByCode);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdDirectorPhaseGatingTest, "StealthGame.Crowd.Director.PhaseGating",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdDirectorPhaseGatingTest::RunTest(const FString& Parameters)
{
	using namespace CrowdDirectorCoreTestData;
	TArray<double> Pauses;
	TArray<double> Legs;
	const FCrowdTuning_Server Tuning;
	for (int32 Round = 0; Round < Rounds; ++Round)
	{
		// Arrange: 50 registered NPCs, Dormant.
		TUniquePtr<FCrowdDirectorCore> Core = MakeCore(FixtureSubSeed + static_cast<uint64>(Round) * 0x9E3779B97F4A7C15ull, Npcs);
		FRecordingStarter Starter;
		TestEqual(TEXT("nothing is started before OnRoundActive"), Starter.Calls.Num(), 0);
		for (const uint32 Ordinal : Core->GetOrdinals())
		{
			TestEqual(TEXT("every pawn is Dormant before OnRoundActive"), static_cast<int32>(Core->GetActivity(Ordinal)), static_cast<int32>(ECrowdActivity::Dormant));
		}
		// Act: OnRoundActive at t = 12.5 s.
		TestTrue(TEXT("the crowd starts"), Core->StartRound(12.5, /*bNavReady*/ true, Starter));
		// Assert: all 50 in that one step.
		TestEqual(TEXT("all 50 NPCs issue their first command at OnRoundActive"), Starter.Calls.Num(), Npcs);
		TestEqual(TEXT("in one server step"), Starter.Steps.Num(), 1);
		for (const TPair<uint32, FCrowdFirstActivity>& Call : Starter.Calls)
		{
			(Call.Value.bPause ? Pauses : Legs).Add(Call.Value.bPause ? Call.Value.PauseSec : Call.Value.LegCm);
		}
	}
	// No unison: the first pauses follow the C2 mixture, the first legs U(CrowdLegMinCm, CrowdLegMaxCm).
	const double LongShare = Tuning.CrowdLongPausePct / 100.0;
	double PauseD = 0.0;
	const double PauseP = StealthCrowdStats::KsPValue(Pauses, [&Tuning, LongShare](double X)
	{
		return (1.0 - LongShare) * StealthCrowdStats::UniformCdf(X, Tuning.CrowdShortPauseMinSec, Tuning.CrowdShortPauseMaxSec)
			+ LongShare * StealthCrowdStats::UniformCdf(X, Tuning.CrowdIdleMinSec, Tuning.CrowdIdleMaxSec);
	}, &PauseD);
	double LegD = 0.0;
	const double LegP = StealthCrowdStats::KsPValue(Legs, [&Tuning](double X) { return StealthCrowdStats::UniformCdf(X, Tuning.CrowdLegMinCm, Tuning.CrowdLegMaxCm); }, &LegD);
	AddInfo(FString::Printf(TEXT("PHASEGATE %d first pauses (KS D %.4f, p %.4f), %d first legs (KS D %.4f, p %.4f), pause share %.3f (F4 %.3f)"),
		Pauses.Num(), PauseD, PauseP, Legs.Num(), LegD, LegP, static_cast<double>(Pauses.Num()) / (Pauses.Num() + Legs.Num()), FCrowdDirectorCore(Tuning, 1).GetFirstPauseShare()));
	TestTrue(TEXT("first pause lengths pass KS at p > 0.01 (AC-CNB-33a)"), PauseP > 0.01);
	TestTrue(TEXT("first leg lengths pass KS at p > 0.01 (AC-CNB-33a)"), LegP > 0.01);
	TestTrue(TEXT("both kinds of first activity occur"), Pauses.Num() > 50 && Legs.Num() > 200);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdDirectorDecisionSpreadTest, "StealthGame.Crowd.Director.DecisionSpread",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdDirectorDecisionSpreadTest::RunTest(const FString& Parameters)
{
	using namespace CrowdDirectorCoreTestData;
	TUniquePtr<FCrowdDirectorCore> Core = MakeCore(FixtureSubSeed, Npcs);
	FRecordingStarter Starter;
	Core->StartRound(0.0, true, Starter);
	constexpr double Fps = 60.0;
	const int32 Bound = Core->DecisionBoundPerFrame(Fps);
	TMap<uint32, double> Next;
	for (const uint32 Ordinal : Core->GetOrdinals())
	{
		Next.Add(Ordinal, Core->NextGridPoint(Ordinal, 0.0, /*bInclusive*/ true));
	}
	int32 MaxInFrame = 0;
	for (int32 Frame = 1; Frame <= 60 * 60; ++Frame)
	{
		const double FrameEnd = Frame / Fps;
		int32 InFrame = 0;
		for (TPair<uint32, double>& Pair : Next)
		{
			// Every grid point that falls inside this frame is one decision in this frame.
			while (Pair.Value < FrameEnd)
			{
				++InFrame;
				Pair.Value = Core->NextGridPoint(Pair.Key, Pair.Value);
			}
		}
		MaxInFrame = FMath::Max(MaxInFrame, InFrame);
	}
	AddInfo(FString::Printf(TEXT("DECISIONSPREAD max %d decisions in a frame, bound %d (N %d, 60 Hz, %.1f/s)"), MaxInFrame, Bound, Npcs, FCrowdTuning_Server().CrowdDecisionPerSec));
	TestTrue(TEXT("no frame holds more than ceil(N / (FPS / f)) + 2 decisions (AC-CNB-33b)"), MaxInFrame <= Bound);
	TestEqual(TEXT("the bound at 50 NPCs, 60 Hz, 4 Hz is 6"), Bound, 6);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdDirectorLedgerTest, "StealthGame.Crowd.Director.Ledger",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdDirectorLedgerTest::RunTest(const FString& Parameters)
{
	using namespace CrowdDirectorCoreTestData;
	FCrowdDirectorCore Core(FCrowdTuning_Server(), FixtureSubSeed);
	const int32 NpcCount = FCrowdTuning_Server().NpcCount;
	TestEqual(TEXT("the ledger starts at NpcCount"), Core.GetLiveNpcCount(), NpcCount);
	// A scripted sequence: two kills (any victim kind: the time is the only input), an ambient exit, a served entry,
	// a takeover, its reversal, another takeover, a release.
	Core.NoteKill(61.5);
	Core.NoteKill(75.0);
	Core.NoteAmbientExit();
	Core.NoteVacancyServed();
	Core.NoteTakeover();
	Core.NoteTakeoverReversed();
	Core.NoteTakeover();
	Core.NoteRelease();
	TestEqual(TEXT("V"), Core.GetVacancies(), 2);
	TestEqual(TEXT("B"), Core.GetTakenOver(), 1);
	TestEqual(TEXT("Rel"), Core.GetReleased(), 1);
	TestEqual(TEXT("live = NpcCount - V - B + Rel"), Core.GetLiveNpcCount(), NpcCount - 2 - 1 + 1);
	TestEqual(TEXT("the ledger recorded the two kill times only"), Core.GetKillTimes().Num(), 2);
	TestEqual(TEXT("kill time 1"), Core.GetKillTimes()[0], 61.5);

	// NavNotReady: StartRound refuses, nobody starts, everyone stays Dormant.
	TUniquePtr<FCrowdDirectorCore> NoNav = MakeCore(FixtureSubSeed, 10);
	FRecordingStarter Starter;
	TestFalse(TEXT("StartRound refuses without a navmesh"), NoNav->StartRound(5.0, /*bNavReady*/ false, Starter));
	TestTrue(TEXT("NavNotReady is asserted"), NoNav->IsNavNotReady());
	TestEqual(TEXT("no pawn started"), Starter.Calls.Num(), 0);
	TestEqual(TEXT("the Director stays Dormant"), static_cast<int32>(NoNav->GetPhase()), static_cast<int32>(ECrowdDirectorPhase::Dormant));
	// Halt is terminal: the mirror reads Halted for everyone.
	TestTrue(TEXT("the crowd then starts once the navmesh is ready"), NoNav->StartRound(6.0, true, Starter));
	NoNav->Halt();
	TestEqual(TEXT("Halt latches every activity mirror"), static_cast<int32>(NoNav->GetActivity(3)), static_cast<int32>(ECrowdActivity::Halted));
	NoNav->SetActivity(3, ECrowdActivity::Routing);
	TestEqual(TEXT("nothing leaves Halted"), static_cast<int32>(NoNav->GetActivity(3)), static_cast<int32>(ECrowdActivity::Halted));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
