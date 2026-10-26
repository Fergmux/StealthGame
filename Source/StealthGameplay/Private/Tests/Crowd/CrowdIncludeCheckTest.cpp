// Story cnb-013 (production/epics/crowd-npc-behaviour/story-013-identity-and-command-path-gate.md), the static half of the
// identity-and-honesty gate (ADR-0001 section 2, ADR-0016 section 2; GDD CNB-R2, R3, AC-CNB-01, AC-CNB-02, AC-CNB-11).
//   IncludeCheck   the Crowd lint (tools/ci/lint/crowd-lint.sh) is watched failing on seeded violations (direct and
//                  TRANSITIVE Roster / Objectives-assignment / Visibility Policy fact includes, IsPlayer / GetTeam /
//                  FSiteAssignment names, MoveTo / RequestDirectMove / AddMovementInput / velocity writes / Attack) and the
//                  real Crowd tree has zero findings: zero Roster call sites, the driver selected by DriveBoundPawn versus an
//                  NPC spawn; the site-assignment sentinel fires on a deliberate crowd-scope query (story cnb-017).
//   NoStrikeVocabulary  AC-CNB-11 by construction: neither sink type has Attack, the restricted driver's sink has only
//                  Move and Look, and the trees run only the crowd activity task (no MoveTo task exists in them).
// The runtime half (an instrumented PIE run of the reference scene) is StealthGame.Crowd.PIE.CommandPath (harness).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "CrowdTestUtils.h"
#include "Server/CrowdAssetKeys.h"
#include "Server/CrowdSink.h"
#include "Server/CrowdStateTreeTypes.h"
#include "StateTree.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdIncludeCheckTest, "StealthGame.Crowd.Identity.IncludeCheck",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdIncludeCheckTest::RunTest(const FString& Parameters)
{
	const StealthCrowdTest::FScriptRun Run = StealthCrowdTest::RunBashScript(TEXT("tools/ci/lint/selftest/run-crowd-lint-selftest.sh"));
	if (!TestTrue(TEXT("the crowd lint selftest ran"), Run.bLaunched))
	{
		return false;
	}
	TestTrue(TEXT("seeded direct includes, identity names and forbidden APIs are reported"), Run.Output.Contains(TEXT("SELFTEST crowd-seeded ok")));
	TestTrue(TEXT("a transitive Roster include is reported"), Run.Output.Contains(TEXT("SELFTEST crowd-transitive ok")));
	TestTrue(TEXT("the real Crowd tree has zero findings (AC-CNB-01)"), Run.Output.Contains(TEXT("SELFTEST real-tree-zero-findings ok")));
	TestTrue(TEXT("the selftest passes"), Run.Output.Contains(TEXT("SELFTEST RESULT: PASS")));
	if (HasAnyErrors())
	{
		AddInfo(Run.Output);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdNoStrikeVocabularyTest, "StealthGame.Crowd.Identity.NoStrikeVocabulary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdNoStrikeVocabularyTest::RunTest(const FString& Parameters)
{
	TestFalse(TEXT("the NPC sink cannot write Attack"), StealthCrowdSink::THasAttack<FCrowdCommandSink>::value);
	TestFalse(TEXT("the restricted sink cannot write Attack"), StealthCrowdSink::THasAttack<FCrowdMotionSink>::value);
	TestFalse(TEXT("the restricted sink has no discrete command"), StealthCrowdSink::THasTraverse<FCrowdMotionSink>::value);
	// The crowd trees hold only the crowd activity task and its condition (no stock MoveTo task).
	const FCrowdAssetKeys Keys;
	for (const FSoftObjectPath& Path : { Keys.CrowdFullStateTreeAsset, Keys.CrowdRestrictedStateTreeAsset })
	{
		const UStateTree* Tree = Cast<UStateTree>(Path.TryLoad());
		if (!TestNotNull(FString::Printf(TEXT("%s loads"), *Path.ToString()), Tree))
		{
			continue;
		}
		int32 Foreign = 0;
		const FInstancedStructContainer& Nodes = Tree->GetNodes();
		for (int32 Index = 0; Index < Nodes.Num(); ++Index)
		{
			const UScriptStruct* Type = Nodes[Index].GetScriptStruct();
			Foreign += Type != FCrowdActivityTask::StaticStruct() && Type != FCrowdActivityCondition::StaticStruct() ? 1 : 0;
		}
		TestEqual(FString::Printf(TEXT("%s runs only crowd nodes (no MoveTo task)"), *Path.GetAssetName()), Foreign, 0);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
