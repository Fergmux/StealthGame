// Story oc-008 (production/epics/objectives-chain/story-008-completion-gate-and-stage-effects.md), AC4 (ADR-0020 section 3)
// DebugCompleteStage:
//   (a) non-Shipping DebugCompleteStage(Team) enqueues its own Round DebugItem slot (a distinct queue key) whose effect calls
//       the same ApplyCountedCompletion as a counted tend: the effects equal StageCompletionEffects' row 1 (Completed, next stage
//       Live, ObjectiveStage logged (Debug-flagged), team view pushed, rival release scheduled); repeated, the final declares
//       the win; afterwards (nothing Live) and before Active entry (everything Locked) it has no effect;
//   (b) a source scan: every declaration and definition of DebugCompleteStage / ApplyDebugStage / ApplyDebugStageCompletion /
//       SubmitDebugObjectiveStage sits inside #if !UE_BUILD_SHIPPING in Objectives and the composition root (Round's
//       SubmitDebugObjectiveStage compiles in every configuration and answers Unavailable in Shipping, as SubmitDebugItem), a seeded
//       unguarded definition fails the same scan;
//   (c) the Shipping build/link log is retained in production/qa/evidence/oc-shipping-build.md (not part of this test).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "ObjectivesQueueRig.h"
#include "ObjectivesSourceScan.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectivesDebugCompleteStageTest, "StealthGame.Objectives.DebugCompleteStage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FObjectivesDebugCompleteStageTest::RunTest(const FString& Parameters)
{
	using namespace StealthObjectivesTest;

	// Before Active entry: no effect.
	{
		FQueueRig Rig;
		Rig.Service->ValidateSelectedChain(FStealthServerTuning());
		Rig.Service->HandleLayoutReady(1, 0);
		TestFalse(TEXT("before Active entry (all Locked): no effect"), Rig.Service->ApplyDebugStage(0, FQueueKey(1, 1)));
	}

	FQueueRig Rig;
	TestTrue(TEXT("Active"), Rig.ToActive());
	const int32 Pushes = Rig.Views.PushesFor(0);
	TestTrue(TEXT("DebugCompleteStage(0) is accepted"), Rig.Service->DebugCompleteStage(0));
	TestEqual(TEXT("nothing happens before the DebugItem's slot"), Rig.Logged(ERoundLogType::ObjectiveStage), 0);
	Rig.Q.Tick(0.1);
	TestEqual(TEXT("its own DebugItem slot ran"), Rig.DebugKeys.Num(), 1);
	TestEqual(TEXT("team 0's P1 is Completed"), static_cast<int32>(Rig.Core().GetSiteState(0, DefP1Team0)), static_cast<int32>(EObjectiveSiteState::Completed));
	TestEqual(TEXT("team 0's P2 is Live"), static_cast<int32>(Rig.Core().GetSiteState(0, DefP2Team0)), static_cast<int32>(EObjectiveSiteState::Live));
	const TArray<FRoundLogEntry> Stages = Rig.LoggedOf(ERoundLogType::ObjectiveStage);
	if (TestEqual(TEXT("one ObjectiveStage"), Stages.Num(), 1))
	{
		TestTrue(TEXT("logged with the DebugItem slot's key"), Rig.DebugKeys.Num() == 1 && Stages[0].Key == Rig.DebugKeys[0]);
		TestTrue(TEXT("Debug-flagged (excluded from playtest metrics)"), (Stages[0].Flags & RoundLogFlags::Debug) != 0);
	}
	TestEqual(TEXT("team 0's view pushed"), Rig.Views.PushesFor(0), Pushes + 1);
	TestEqual(TEXT("the rival release scheduled"), Rig.Rival.Calls.Num(), 1);
	TestEqual(TEXT("team 1 untouched"), static_cast<int32>(Rig.Core().GetSiteState(1, DefP1Team1)), static_cast<int32>(EObjectiveSiteState::Live));

	// Prelim2, then the final: the win is declared in the DebugItem slot.
	Rig.Service->DebugCompleteStage(0);
	Rig.Q.Tick(0.1);
	Rig.Service->DebugCompleteStage(0);
	Rig.Q.Tick(0.1);
	TestEqual(TEXT("the final's debug completion declares the win once"), Rig.Round.Wins.Num(), 1);
	TestEqual(TEXT("FinalCompleted logged"), Rig.Logged(ERoundLogType::FinalCompleted), 1);
	TestFalse(TEXT("nothing Live any more (Completed): no effect"), Rig.Service->ApplyDebugStage(0, FQueueKey(9, 9)));

	// (b) The non-Shipping guard of every debug entry point.
	const TCHAR* Names[] = { TEXT("DebugCompleteStage"), TEXT("ApplyDebugStage"), TEXT("ApplyDebugStageCompletion"), TEXT("SubmitDebugObjectiveStage") };
	for (const TCHAR* Folder : { TEXT("StealthGameplay/Features/Objectives"), TEXT("StealthGame/Private/Composition"), TEXT("StealthGame/Public/Server") })
	{
		for (const StealthObjectivesSourceScan::FSource& Source : StealthObjectivesSourceScan::LoadSources(Folder))
		{
			for (const TCHAR* Name : Names)
			{
				const TArray<int32> Outside = StealthObjectivesSourceScan::FindOutsideShippingGuard(Source.Text, Name);
				TestEqual(FString::Printf(TEXT("%s: %s only inside #if !UE_BUILD_SHIPPING"), *Source.Path, Name), Outside.Num(), 0);
			}
		}
	}
	TestEqual(TEXT("seeded: an unguarded DebugCompleteStage is found"),
		StealthObjectivesSourceScan::FindOutsideShippingGuard(TEXT("bool UObjectivesService::DebugCompleteStage(uint8 Team) { return true; }\n"), TEXT("DebugCompleteStage")).Num(), 1);
	return true;
}

#endif
