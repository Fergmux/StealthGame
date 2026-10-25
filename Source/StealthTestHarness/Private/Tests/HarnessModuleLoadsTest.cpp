// Story tdc-004 (production/epics/tuning-data-config/story-004-test-harness-module-shipping-excluded.md):
// the harness module is loaded in a Development editor. A test hosted in the harness is only
// discovered when the module loaded, so on its own it cannot observe a load failure; the
// non-vacuous load assertion is repeated by StealthGame.Build.HarnessModuleExclusions (hosted in
// StealthFoundation), and the cooked-Development observation belongs to story tdc-020.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Modules/ModuleManager.h"
#include "StealthTestHarnessModule.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHarnessModuleLoadsInDevelopmentTest, "StealthGame.Harness.ModuleLoadsInDevelopment",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHarnessModuleLoadsInDevelopmentTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("StealthTestHarness is loaded in the Development editor"), FModuleManager::Get().IsModuleLoaded(TEXT("StealthTestHarness")));
	TestEqual(TEXT("LogStealthHarness category name"), LogStealthHarness.GetCategoryName(), FName(TEXT("LogStealthHarness")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
