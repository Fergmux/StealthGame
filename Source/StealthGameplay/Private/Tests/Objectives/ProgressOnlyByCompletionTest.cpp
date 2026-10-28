// Story oc-004 (production/epics/objectives-chain/story-004-core-site-state-and-progress.md), AC1 / AC-OC-02
// ProgressOnlyByCompletion: GIVEN both teams at Stage1 (Prelim1 Live) WHEN team 0's Prelim1 completes (ApplyCountedCompletion,
// the only writer) THEN Prelim2(0) is Live in the same call and team 1's states are unchanged; WHEN a fake clock advances 60
// minutes with no completion (in many small steps, and in one big step) THEN no state changes and no event is emitted (no log
// entry, no view push). The core reads no clock at all: nothing keyed on elapsed time exists (SP-3). Edge: a completion at
// clock time 0 counts the same.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "ObjectivesTestRig.h"

namespace ObjectivesProgressOnlyByCompletionTestData
{
	constexpr double SixtyMinutesSec = 60.0 * 60.0;
	constexpr int32 SmallSteps = 3600;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectivesProgressOnlyByCompletionTest, "StealthGame.Objectives.ProgressOnlyByCompletion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FObjectivesProgressOnlyByCompletionTest::RunTest(const FString& Parameters)
{
	using namespace StealthObjectivesTest;
	using namespace ObjectivesProgressOnlyByCompletionTestData;

	// Completion drives progress, for the completing team only (at clock time 0).
	{
		FCoreRig Rig;
		Rig.Round.Now = 0.0;
		TArray<EObjectiveSiteState> Team1Before;
		for (int32 Def = 0; Def < Rig.Core.GetDefinitionCount(); ++Def)
		{
			Team1Before.Add(Rig.Core.GetSiteState(1, Def));
		}
		const FObjectivesAppliedCompletion Applied = Rig.Core.ApplyCountedCompletion(0, DefP1Team0);
		TestTrue(TEXT("team 0's Prelim1 completes"), Applied.bApplied);
		TestTrue(TEXT("it unlocks the next stage"), Applied.bUnlockedNext);
		TestEqual(TEXT("Prelim1(0) is Completed"), static_cast<int32>(Rig.Core.GetSiteState(0, DefP1Team0)), static_cast<int32>(EObjectiveSiteState::Completed));
		TestEqual(TEXT("Prelim2(0) is Live in the same call"), static_cast<int32>(Rig.Core.GetSiteState(0, DefP2Team0)), static_cast<int32>(EObjectiveSiteState::Live));
		TestEqual(TEXT("team 0 is at Stage2"), static_cast<int32>(Rig.Core.GetStageIndex(0)), static_cast<int32>(EObjectiveTeamStage::Stage2));
		for (int32 Def = 0; Def < Rig.Core.GetDefinitionCount(); ++Def)
		{
			TestEqual(FString::Printf(TEXT("team 1 row %d unchanged"), Def), static_cast<int32>(Rig.Core.GetSiteState(1, Def)), static_cast<int32>(Team1Before[Def]));
		}
		TestEqual(TEXT("team 1 stays at Stage1"), static_cast<int32>(Rig.Core.GetStageIndex(1)), static_cast<int32>(EObjectiveTeamStage::Stage1));
	}

	// Waiting earns nothing: 60 minutes in small steps, then in one step.
	for (const bool bSmallSteps : { true, false })
	{
		FCoreRig Rig;
		const TArray<EObjectiveSiteState> Before = Rig.Snapshot();
		const int32 Entries = Rig.Round.Entries.Num();
		const int32 Pushes = Rig.Views.Pushes.Num();
		if (bSmallSteps)
		{
			for (int32 Step = 0; Step < SmallSteps; ++Step)
			{
				Rig.Round.Now += SixtyMinutesSec / SmallSteps;
			}
		}
		else
		{
			Rig.Round.Now += SixtyMinutesSec;
		}
		const TCHAR* Mode = bSmallSteps ? TEXT("small steps") : TEXT("one step");
		TestTrue(FString::Printf(TEXT("%s: no state changes after 60 min"), Mode), Rig.Snapshot() == Before);
		TestEqual(FString::Printf(TEXT("%s: no event is emitted"), Mode), Rig.Round.Entries.Num(), Entries);
		TestEqual(FString::Printf(TEXT("%s: no view is pushed"), Mode), Rig.Views.Pushes.Num(), Pushes);
		TestEqual(FString::Printf(TEXT("%s: no rival release is scheduled"), Mode), Rig.Rival.Calls.Num(), 0);
		TestEqual(FString::Printf(TEXT("%s: both teams still Stage1"), Mode), static_cast<int32>(Rig.Core.GetStageIndex(0)) + static_cast<int32>(Rig.Core.GetStageIndex(1)),
			2 * static_cast<int32>(EObjectiveTeamStage::Stage1));
	}
	return true;
}

#endif
