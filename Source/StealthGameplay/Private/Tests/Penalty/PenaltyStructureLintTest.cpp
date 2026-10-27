// Story nkp-001 (production/epics/npc-kill-penalty-informants/story-001-penalty-core-store-and-service-shell.md), AC3
// (derived, ADR-0019 section 1 and Implementation Guidelines 1-2; the QA case's ServerOnlyShell): reflection over
// UPenaltyService finds no Replicated / ReplicatedUsing / conditioned property, the service is NotBlueprintType and
// FPenaltyRecord is not a USTRUCT; every Penalty Public/Server header includes StealthServerOnlyGuard.h first (negative
// control: a seeded header without it is caught); and FPenaltyCore runs with no UWorld, UObject or engine singleton (it is
// built and driven here on plain fakes, and its own sources name none of them).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Server/StealthPenaltyService.h"
#include "Server/Testing/StealthNegativeControl.h"
#include "Tests/Penalty/PenaltySourceScan.h"
#include "Tests/Penalty/PenaltyTestRig.h"
#include "UObject/UnrealType.h"

namespace PenaltyStructureLintTestData
{
	/** The guard check over one header's text (the same function the seeded control runs). */
	bool HasGuardFirst(const FString& HeaderText)
	{
		return StealthPenaltySourceScan::FirstInclude(HeaderText) == TEXT("StealthServerOnlyGuard.h");
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPenaltyStructureLintTest, "StealthGame.Penalty.StructureLint",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPenaltyStructureLintTest::RunTest(const FString& Parameters)
{
	using namespace StealthPenaltySourceScan;
	using namespace PenaltyStructureLintTestData;

	// Reflection: no replicated property anywhere on the service (its own or inherited), not Blueprint-visible.
	const UClass* Service = UPenaltyService::StaticClass();
	for (TFieldIterator<FProperty> It(Service, EFieldIteratorFlags::IncludeSuper); It; ++It)
	{
		TestFalse(FString::Printf(TEXT("UPenaltyService::%s is not replicated"), *It->GetName()), It->HasAnyPropertyFlags(CPF_Net | CPF_RepNotify));
	}
#if WITH_METADATA
	TestFalse(TEXT("UPenaltyService is not Blueprintable"), Service->GetBoolMetaData(TEXT("IsBlueprintBase")));
	TestFalse(TEXT("UPenaltyService is NotBlueprintType (no BlueprintType metadata)"), Service->GetBoolMetaData(TEXT("BlueprintType")));
	TestTrue(TEXT("UPenaltyService carries NotBlueprintType"), Service->GetBoolMetaData(TEXT("NotBlueprintType")));
#endif
	TestNull(TEXT("FPenaltyRecord is not a USTRUCT"), FindFirstObject<UScriptStruct>(TEXT("PenaltyRecord"), EFindFirstObjectOptions::None));
	TestNull(TEXT("FPenaltyPawnKey is not a USTRUCT"), FindFirstObject<UScriptStruct>(TEXT("PenaltyPawnKey"), EFindFirstObjectOptions::None));

	// Every Public/Server header includes the server-only guard first.
	const TArray<FSource> Headers = LoadSources(TEXT("Public/Server"));
	TestTrue(TEXT("the Penalty Public/Server headers were found"), Headers.Num() >= 3);
	for (const FSource& Header : Headers)
	{
		TestTrue(FString::Printf(TEXT("%s includes StealthServerOnlyGuard.h first (found \"%s\")"), *Header.Path, *FirstInclude(Header.Text)), HasGuardFirst(Header.Text));
	}
	FStealthNegativeControl Control(TEXT("HeaderWithoutGuard"));
	FStealthNegativeControl::Report(*this, Control.Run([](FStealthCheckContext& Ctx)
	{
		Ctx.TestTrue(TEXT("seeded header has the guard first"), HasGuardFirst(TEXT("#pragma once\n// StealthServerOnlyGuard.h comes later\n#include \"CoreMinimal.h\"\n#include \"StealthServerOnlyGuard.h\"\n")));
	}));

	// The core's own sources name no engine world, object or singleton.
	for (const FSource& Source : LoadSources(TEXT("Private")))
	{
		if (!Source.Path.Contains(TEXT("StealthPenaltyCore.")))
		{
			continue;
		}
		TArray<FFinding> Findings;
		static const TCHAR* const EngineNames[] = { TEXT("UWorld"), TEXT("UObject"), TEXT("GEngine"), TEXT("GWorld"), TEXT("GetWorld"), TEXT("NewObject") };
		FindIdentifierPrefixes(Source, EngineNames, Findings);
		TestTrue(FString::Printf(TEXT("%s names no UWorld / UObject / singleton (%s)"), *Source.Path, *Describe(Findings)), Findings.IsEmpty());
	}

	// Headless: the core is built and driven on plain fakes (no world exists in this test).
	{
		using namespace StealthPenaltyTest;
		FRig Rig;
		Rig.NpcKill(P);
		TestEqual(TEXT("the headless core counts a strike"), static_cast<int32>(Rig.Core->GetStrikeCount(P)), 1);
		TestEqual(TEXT("the headless core counts the tally"), static_cast<int32>(Rig.Core->GetWrongTargetTally(P)), 1);
	}
	return true;
}

#endif
