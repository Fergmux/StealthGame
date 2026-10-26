// Story iif-016 (production/epics/input-interaction-framework/story-016-no-toggle-setting-mvp.md), GDD AC 31 MVP clause
// (R16, Logic, NoToggleSettingMvp): GIVEN the MVP build THEN no toggle setting exists.
// Pinned search surface (QL-STORY-READY rewrite): no property of a UDeveloperSettings or UGameUserSettings class of the
// project's modules, no UEnhancedInputUserSettings property and no StealthGame console variable has a name containing
// both a hold/toggle term (Toggle, HoldMode, HoldToggle) and one of Prepare, Interact, Run; and the command builder has
// no press/press-to-Begin/End path (UPawnCommandClient declares no such property, and FPawnCommandClientCore sends an
// End only from a terminal edge or a flush). IA_CameraToggle is a binding, not a setting (allowlisted by name).
// Seeded negative: a fake "bToggleHoldPrepare" name must fail the matcher.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/DeveloperSettings.h"
#include "GameFramework/GameUserSettings.h"
#include "HAL/IConsoleManager.h"
#include "UObject/UObjectIterator.h"
#include "UserSettings/EnhancedInputUserSettings.h"
#include "View/Input/PawnCommandClient.h"

namespace NoToggleSettingMvpTestData
{
	const TCHAR* const ToggleTerms[] = { TEXT("Toggle"), TEXT("HoldMode"), TEXT("HoldToggle") };
	const TCHAR* const CommandTerms[] = { TEXT("Prepare"), TEXT("Interact"), TEXT("Run") };
	const TCHAR* const Allowlist[] = { TEXT("IA_CameraToggle"), TEXT("CameraToggleLockoutSec") };

	bool IsToggleSettingName(const FString& Name)
	{
		for (const TCHAR* Allowed : Allowlist)
		{
			if (Name == Allowed)
			{
				return false;
			}
		}
		bool bToggle = false;
		for (const TCHAR* Term : ToggleTerms)
		{
			bToggle |= Name.Contains(Term, ESearchCase::IgnoreCase);
		}
		bool bCommand = false;
		for (const TCHAR* Term : CommandTerms)
		{
			bCommand |= Name.Contains(Term, ESearchCase::IgnoreCase);
		}
		return bToggle && bCommand;
	}

	bool IsProjectClass(const UClass* Class)
	{
		return Class->GetPathName().StartsWith(TEXT("/Script/Stealth"));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInputNoToggleSettingMvpTest, "StealthGame.Input.NoToggleSettingMvp",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FInputNoToggleSettingMvpTest::RunTest(const FString& Parameters)
{
	using namespace NoToggleSettingMvpTestData;

	TArray<FString> Hits;
	int32 ClassesScanned = 0;
	for (TObjectIterator<UClass> It; It; ++It)
	{
		const UClass* Class = *It;
		const bool bSettingsClass = (Class->IsChildOf(UDeveloperSettings::StaticClass()) || Class->IsChildOf(UGameUserSettings::StaticClass())) && IsProjectClass(Class);
		const bool bUserSettings = Class->IsChildOf(UEnhancedInputUserSettings::StaticClass());
		const bool bBuilder = Class == UPawnCommandClient::StaticClass();
		if (!bSettingsClass && !bUserSettings && !bBuilder)
		{
			continue;
		}
		++ClassesScanned;
		for (TFieldIterator<FProperty> Prop(Class); Prop; ++Prop)
		{
			if (IsToggleSettingName(Prop->GetName()))
			{
				Hits.Add(FString::Printf(TEXT("%s.%s"), *Class->GetName(), *Prop->GetName()));
			}
		}
	}
	IConsoleManager::Get().ForEachConsoleObjectThatStartsWith(FConsoleObjectVisitor::CreateLambda([&Hits](const TCHAR* Name, IConsoleObject*)
	{
		if (IsToggleSettingName(Name))
		{
			Hits.Add(Name);
		}
	}), TEXT("Stealth"));

	TestTrue(TEXT("the scan covered the builder and at least one settings class"), ClassesScanned >= 2);
	TestTrue(FString::Printf(TEXT("no hold/toggle setting exists for Prepare, Interact or Run (%s)"), *FString::Join(Hits, TEXT(", "))), Hits.IsEmpty());

	// Seeded negative and the allowlist.
	TestTrue(TEXT("a seeded bToggleHoldPrepare fails the matcher"), IsToggleSettingName(TEXT("bToggleHoldPrepare")));
	TestTrue(TEXT("a seeded RunHoldMode fails the matcher"), IsToggleSettingName(TEXT("RunHoldMode")));
	TestFalse(TEXT("IA_CameraToggle is a binding, not a setting"), IsToggleSettingName(TEXT("IA_CameraToggle")));

	// Press/press never becomes Begin/End: a second Started while held sends nothing (hold-only in the MVP).
	TArray<ECommandKind> Sent;
	FPawnCommandClientCore Core([&Sent](const FNetPawnCommand& Cmd) { Sent.Add(Cmd.Kind); });
	Core.OnHeldStarted(FPawnCommandClientCore::EHeld::Prepare);
	Core.OnHeldStarted(FPawnCommandClientCore::EHeld::Prepare);
	TestTrue(TEXT("a second press while held is not an End (no toggle translation)"), Sent == TArray<ECommandKind>({ ECommandKind::PrepareBegin }));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
