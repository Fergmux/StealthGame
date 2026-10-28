// Objectives Chain story oc-022 AC2 (AC-OC-25, ADR-0020 Guidelines [U]; the single owner of this lint): StealthGame.Objectives.
// ObjectivesIncludeLint. GIVEN an include scan over the client modules (every View/ folder and the StealthPresentation module),
// Crowd (Features/Crowd), the IIF (Server/Core, Private/Core, View/Input) and Presentation, THEN none includes an Objectives
// Public/Server/ header, directly or transitively through another project header (the Objectives View/ helper and struct headers
// are allowed); a seeded include (direct, and transitive through a seeded View header) fails the scan. Stories oc-001, oc-003,
// oc-005 and oc-016 cite this test rather than duplicating it.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "ObjectivesSourceScan.h"

namespace ObjectivesIncludeLintTestData
{
	using StealthObjectivesSourceScan::FSource;

	bool IsObjectivesServerHeader(const FString& Path)
	{
		return Path.Contains(TEXT("Features/Objectives/Public/Server/"));
	}

	/** The scanned set: client modules, Crowd, IIF and Presentation (tests excluded). */
	bool IsScanned(const FString& Path)
	{
		if (Path.Contains(TEXT("/Tests/")))
		{
			return false;
		}
		return Path.StartsWith(TEXT("StealthPresentation/")) || Path.Contains(TEXT("/View/")) || Path.Contains(TEXT("Features/Crowd/"))
			|| Path.Contains(TEXT("StealthGameplay/Public/Server/Core/")) || Path.Contains(TEXT("StealthGameplay/Private/Core/"));
	}

	TArray<FString> Lint(TConstArrayView<FSource> Project)
	{
		TArray<FString> Findings;
		for (const FSource& Source : Project)
		{
			if (IsScanned(Source.Path))
			{
				Findings.Append(StealthObjectivesSourceScan::FindForbiddenIncludes(Project, Source, [](const FString& Path) { return IsObjectivesServerHeader(Path); }));
			}
		}
		return Findings;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectivesIncludeLintTest, "StealthGame.Objectives.ObjectivesIncludeLint",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FObjectivesIncludeLintTest::RunTest(const FString& Parameters)
{
	using namespace ObjectivesIncludeLintTestData;
	const TArray<FSource> Project = StealthObjectivesSourceScan::LoadProjectSources();
	int32 Scanned = 0;
	for (const FSource& Source : Project)
	{
		Scanned += IsScanned(Source.Path) ? 1 : 0;
	}
	const TArray<FString> Findings = Lint(Project);
	AddInfo(FString::Printf(TEXT("OC22 include lint: %d scanned of %d project sources, %d finding(s) %s"), Scanned, Project.Num(), Findings.Num(), *FString::Join(Findings, TEXT("; "))));
	TestTrue(TEXT("the lint scanned the client, Crowd, IIF and Presentation sources"), Scanned > 50);
	TestEqual(TEXT("no scanned source reaches an Objectives Public/Server/ header"), Findings.Num(), 0);

	// The View/ helper and struct headers are allowed (they are scanned and clean, not exempt).
	TestTrue(TEXT("the Objectives View helpers are in the scanned set"), Project.ContainsByPredicate([](const FSource& S)
	{
		return S.Path.EndsWith(TEXT("Objectives/Public/View/ObjectiveViewHelpers.h")) && IsScanned(S.Path);
	}));

	// Negative controls: a direct include from Crowd, and a transitive one through a seeded View header.
	TArray<FSource> Direct = Project;
	FSource CrowdSeed;
	CrowdSeed.Path = TEXT("StealthGameplay/Features/Crowd/Private/SeededCrowd.cpp");
	CrowdSeed.Text = TEXT("#include \"Server/ObjectivesService.h\"\n");
	Direct.Add(CrowdSeed);
	TestEqual(TEXT("control: a seeded direct include from Crowd fails"), Lint(Direct).Num(), 1);
	TArray<FSource> Transitive = Project;
	FSource ViewSeed;
	ViewSeed.Path = TEXT("StealthGameplay/Public/View/SeededViewHeader.h");
	ViewSeed.Text = TEXT("#pragma once\n#include \"Server/ObjectivesSeams.h\"\n");
	FSource PresentationSeed;
	PresentationSeed.Path = TEXT("StealthPresentation/Private/SeededWidget.cpp");
	PresentationSeed.Text = TEXT("#include \"View/SeededViewHeader.h\"\n");
	Transitive.Add(ViewSeed);
	Transitive.Add(PresentationSeed);
	TestTrue(TEXT("control: a transitive include through a View header fails (both the header and its includer)"), Lint(Transitive).Num() >= 2);
	return true;
}

#endif
