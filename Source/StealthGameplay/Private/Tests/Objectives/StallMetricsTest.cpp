// Objectives Chain story oc-017 AC1 (AC-OC-23 [U]; F5): StealthGame.Objectives.StallMetrics.
// With ObjectiveStallGapDefectSec 300 and ObjectiveFinalDwellDefectSec 240 (the Tuning Knobs defaults, read from the schema
// registry's defaults), GIVEN fixture logs (team 0: P1 95, P2 260, final 420; team 1: P1 180, P2 330) THEN G_mutual 95,
// G_team(0) 165, G_team(1) 180, FinalDwell 160 and no defect; GIVEN no completion and an elimination at 400 s THEN StallDefect
// (G_mutual 400 > 300). Boundaries: a gap exactly at the threshold is not a defect (strictly greater), Round's StallFlag alone
// is a defect, a 1v0 dev round's exclusion flag (story oc-021) takes the round out of F5. Run both on explicit inputs and on
// the Round Event Log entries the core writes (ObjectiveStage, FinalCompleted, WinLatched).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "ObjectivesMetrics.h"
#include "Server/StealthTuningSubstructs.h"
#include "View/ObjectiveChainData.h"

namespace StallMetricsTestData
{
	constexpr double ActiveStart = 1000.0;

	FRoundLogEntry Stage(uint32 Team, EObjectiveStage StageKind, double SinceActive)
	{
		FRoundLogEntry Entry;
		Entry.Type = StageKind == EObjectiveStage::Final ? ERoundLogType::FinalCompleted : ERoundLogType::ObjectiveStage;
		Entry.ServerTime = ActiveStart + SinceActive;
		Entry.Data[0] = Team;
		Entry.Data[1] = static_cast<uint32>(StageKind);
		return Entry;
	}

	FRoundLogEntry End(double SinceActive)
	{
		FRoundLogEntry Entry;
		Entry.Type = ERoundLogType::WinLatched;
		Entry.ServerTime = ActiveStart + SinceActive;
		return Entry;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectivesStallMetricsTest, "StealthGame.Objectives.StallMetrics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FObjectivesStallMetricsTest::RunTest(const FString& Parameters)
{
	using namespace StallMetricsTestData;
	const FObjectivesTuning_Server Knobs; // the schema registry's defaults
	const double Gap = Knobs.ObjectiveStallGapDefectSec;
	const double Dwell = Knobs.ObjectiveFinalDwellDefectSec;
	TestTrue(TEXT("the registry defaults are 300 / 240 s"), Gap == 300.0 && Dwell == 240.0);

	// The fixture round from the log.
	TArray<FRoundLogEntry> Log;
	Log.Add(Stage(0, EObjectiveStage::Prelim1, 95.0));
	Log.Add(Stage(1, EObjectiveStage::Prelim1, 180.0));
	Log.Add(Stage(0, EObjectiveStage::Prelim2, 260.0));
	Log.Add(Stage(1, EObjectiveStage::Prelim2, 330.0));
	Log.Add(Stage(0, EObjectiveStage::Final, 420.0));
	Log.Add(End(420.0));
	const FObjectivesStallMetrics M = FObjectivesMetrics::ComputeStall(FObjectivesMetrics::StallInputFromLog(Log, ActiveStart, ActiveStart + 999.0, false), Gap, Dwell);
	AddInfo(FString::Printf(TEXT("OC17 fixture: G_mutual %.1f, G_team %.1f / %.1f, FinalDwell %.1f, defect %d"), M.GMutual, M.GTeam[0], M.GTeam[1], M.FinalDwell, M.bStallDefect ? 1 : 0));
	TestEqual(TEXT("G_mutual 95"), M.GMutual, 95.0);
	TestEqual(TEXT("G_team(0) 165"), M.GTeam[0], 165.0);
	TestEqual(TEXT("G_team(1) 180"), M.GTeam[1], 180.0);
	TestEqual(TEXT("FinalDwell 160"), M.FinalDwell, 160.0);
	TestFalse(TEXT("no defect"), M.bStallDefect);

	// No completion, an elimination at 400 s: StallDefect (G_mutual 400 > 300).
	TArray<FRoundLogEntry> Idle;
	Idle.Add(End(400.0));
	const FObjectivesStallMetrics Stalled = FObjectivesMetrics::ComputeStall(FObjectivesMetrics::StallInputFromLog(Idle, ActiveStart, ActiveStart + 999.0, false), Gap, Dwell);
	TestEqual(TEXT("idle: G_mutual 400"), Stalled.GMutual, 400.0);
	TestEqual(TEXT("idle: each team's gap is the whole round"), Stalled.GTeam[0], 400.0);
	TestEqual(TEXT("idle: FinalDwell undefined"), Stalled.FinalDwell, -1.0);
	TestTrue(TEXT("idle: StallDefect"), Stalled.bStallDefect);

	// Boundaries: exactly at the gap threshold is not a defect; one above is; FinalDwell above its threshold is.
	FObjectivesStallInput AtGap;
	AtGap.TEnd = 300.0;
	TestFalse(TEXT("G_mutual exactly 300 is not a defect"), FObjectivesMetrics::ComputeStall(AtGap, Gap, Dwell).bStallDefect);
	AtGap.TEnd = 300.5;
	TestTrue(TEXT("G_mutual 300.5 is a defect"), FObjectivesMetrics::ComputeStall(AtGap, Gap, Dwell).bStallDefect);
	FObjectivesStallInput LongDwell;
	LongDwell.Completions[0] = { 100.0, 200.0, 441.0 };
	LongDwell.Completions[1] = { 150.0, 250.0, 350.0 };
	LongDwell.Prelim2Time[0] = 200.0;
	LongDwell.Prelim2Time[1] = 300.0;
	LongDwell.TEnd = 441.0;
	const FObjectivesStallMetrics Dwelt = FObjectivesMetrics::ComputeStall(LongDwell, Gap, Dwell);
	TestTrue(TEXT("FinalDwell 241 > 240 is a defect"), Dwelt.FinalDwell == 241.0 && Dwelt.bStallDefect);
	LongDwell.TEnd = 440.0;
	LongDwell.Completions[0] = { 100.0, 200.0, 440.0 };
	TestFalse(TEXT("FinalDwell exactly 240 is not"), FObjectivesMetrics::ComputeStall(LongDwell, Gap, Dwell).bStallDefect);
	FObjectivesStallInput Flagged = AtGap;
	Flagged.TEnd = 10.0;
	Flagged.bStallFlag = true;
	TestTrue(TEXT("Round's StallFlag alone is a defect"), FObjectivesMetrics::ComputeStall(Flagged, Gap, Dwell).bStallDefect);

	// A debug completion is not a playtest completion (R22); a 1v0 dev round's flag excludes the round.
	TArray<FRoundLogEntry> Debug = Idle;
	FRoundLogEntry DebugStage = Stage(0, EObjectiveStage::Prelim1, 200.0);
	DebugStage.Flags |= RoundLogFlags::Debug;
	Debug.Add(DebugStage);
	TestEqual(TEXT("a debug completion is skipped"), FObjectivesMetrics::ComputeStall(FObjectivesMetrics::StallInputFromLog(Debug, ActiveStart, ActiveStart + 999.0, false), Gap, Dwell).GMutual, 400.0);
	TArray<FRoundLogEntry> Excluded = Idle;
	FRoundLogEntry Pacing;
	Pacing.Type = ERoundLogType::ObjectivePacing;
	Pacing.Code = 1;
	Excluded.Add(Pacing);
	const FObjectivesStallMetrics Out = FObjectivesMetrics::ComputeStall(FObjectivesMetrics::StallInputFromLog(Excluded, ActiveStart, ActiveStart + 999.0, false), Gap, Dwell);
	TestTrue(TEXT("an excluded round is out of F5 (no defect reported)"), Out.bExcluded && !Out.bStallDefect);
	TestFalse(TEXT("a round without the flag counts as normal"), Stalled.bExcluded);
	return true;
}

#endif
