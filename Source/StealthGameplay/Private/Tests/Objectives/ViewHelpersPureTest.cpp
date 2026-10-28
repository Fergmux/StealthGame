// Objectives Chain story oc-016 AC2 (ADR-0020 section 9 [U]): StealthGame.Objectives.ViewHelpersPure.
// IsOwnLiveSite, GetStageCardRow, BuildClueText (and RingIsMarigold) are pure functions in View/ObjectiveViewHelpers.h: results
// depend only on the team view, the cooked chain asset and client tuning; the same inputs give the same outputs; an empty view
// returns no live site and no card; the header and everything it includes (transitively, through project headers) includes no
// server header (a Server/ path or the server-only guard), and a seeded server include fails the scan (negative control).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "ObjectivesSourceScan.h"
#include "ObjectivesTestRig.h"
#include "View/ObjectiveViewHelpers.h"

namespace ViewHelpersPureTestData
{
	bool IsServerHeader(const FString& Path)
	{
		return Path.Contains(TEXT("/Server/")) || Path.StartsWith(TEXT("Server/")) || Path.EndsWith(TEXT("StealthServerOnlyGuard.h"));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectivesViewHelpersPureTest, "StealthGame.Objectives.ViewHelpersPure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FObjectivesViewHelpersPureTest::RunTest(const FString& Parameters)
{
	using namespace StealthObjectivesTest;
	using namespace ViewHelpersPureTestData;
	TStrongObjectPtr<UObjectiveChainData> Chain(MakeCacaoChain());

	// An empty view: no live site, no card.
	const FObjectiveTeamView Empty;
	TestFalse(TEXT("empty view: no live site"), ObjectiveViewHelpers::IsOwnLiveSite(Empty, FCourtFixtureId(CourtP1Team0)));
	TestFalse(TEXT("empty view: no card"), ObjectiveViewHelpers::GetStageCardRow(Empty, Chain.Get()).IsValid());

	// A Stage1 view of team 0 (as the core builds it).
	FCoreRig Rig;
	const FObjectiveTeamView View = Rig.Core.BuildTeamView(0);
	TestTrue(TEXT("own live site"), ObjectiveViewHelpers::IsOwnLiveSite(View, FCourtFixtureId(CourtP1Team0)));
	TestFalse(TEXT("an opponent site is not own-live"), ObjectiveViewHelpers::IsOwnLiveSite(View, FCourtFixtureId(CourtP1Team1)));
	TestFalse(TEXT("a locked own site is not own-live"), ObjectiveViewHelpers::IsOwnLiveSite(View, FCourtFixtureId(CourtP2Team0)));
	const ObjectiveViewHelpers::FStageCardRow Card = ObjectiveViewHelpers::GetStageCardRow(View, Chain.Get());
	TestTrue(TEXT("the card is the row of LiveSites[0]"), Card.IsValid() && Card.DefIndex == DefP1Team0);
	TestTrue(TEXT("its text is the cooked chain's row text"), Card.Title.EqualTo(Chain->Definitions[DefP1Team0].CardTitle) && Card.Line.EqualTo(Chain->Definitions[DefP1Team0].CardLine));
	const ObjectiveViewHelpers::FStageCardRow Again = ObjectiveViewHelpers::GetStageCardRow(View, Chain.Get());
	TestTrue(TEXT("same inputs, same card"), Again.DefIndex == Card.DefIndex && Again.Title.EqualTo(Card.Title));
	FObjectiveTeamView OtherSerial = View;
	OtherSerial.ChainSerial = static_cast<uint16>(View.ChainSerial + 1);
	TestFalse(TEXT("a view naming another chain gets no card"), ObjectiveViewHelpers::GetStageCardRow(OtherSerial, Chain.Get()).IsValid());
	TestFalse(TEXT("no chain asset: no card"), ObjectiveViewHelpers::GetStageCardRow(View, nullptr).IsValid());
	FClueTag Clue;
	Clue.AreaType = static_cast<uint8>(EClueAreaType::Causeway);
	Clue.FeatureKind = static_cast<uint8>(EClueFeatureKind::Stairway);
	TestTrue(TEXT("same clue, same text"), ObjectiveViewHelpers::BuildClueText(Clue).EqualTo(ObjectiveViewHelpers::BuildClueText(Clue)));

	// The include scan: View/ObjectiveViewHelpers.h and its transitive project includes reach no server header.
	const TArray<StealthObjectivesSourceScan::FSource> Project = StealthObjectivesSourceScan::LoadProjectSources();
	const StealthObjectivesSourceScan::FSource* Header = StealthObjectivesSourceScan::ResolveInclude(Project, TEXT("View/ObjectiveViewHelpers.h"));
	if (!TestNotNull(TEXT("the helper header is in the tree"), Header))
	{
		return false;
	}
	const TArray<FString> Findings = StealthObjectivesSourceScan::FindForbiddenIncludes(Project, *Header, [](const FString& Path) { return IsServerHeader(Path); });
	AddInfo(FString::Printf(TEXT("OC16 include scan of %s: %d finding(s) %s"), *Header->Path, Findings.Num(), *FString::Join(Findings, TEXT("; "))));
	TestEqual(TEXT("the helper header includes no server header, directly or transitively"), Findings.Num(), 0);
	const StealthObjectivesSourceScan::FSource* Cpp = StealthObjectivesSourceScan::ResolveInclude(Project, TEXT("Private/ObjectiveViewHelpers.cpp"));
	if (TestNotNull(TEXT("the helper source is in the tree"), Cpp))
	{
		TestEqual(TEXT("the helper source includes no server header"), StealthObjectivesSourceScan::FindForbiddenIncludes(Project, *Cpp, [](const FString& Path) { return IsServerHeader(Path); }).Num(), 0);
	}
	// Negative control: the same header with one seeded server include (transitively through a seeded View header).
	StealthObjectivesSourceScan::FSource Seeded = *Header;
	Seeded.Text += TEXT("\n#include \"View/SeededHelper.h\"\n");
	TArray<StealthObjectivesSourceScan::FSource> SeededProject = Project;
	StealthObjectivesSourceScan::FSource SeededHelper;
	SeededHelper.Path = TEXT("StealthGameplay/Public/View/SeededHelper.h");
	SeededHelper.Text = TEXT("#pragma once\n#include \"Server/ObjectivesSeams.h\"\n");
	SeededProject.Add(SeededHelper);
	TestEqual(TEXT("control: a seeded transitive server include fails the scan"), StealthObjectivesSourceScan::FindForbiddenIncludes(SeededProject, Seeded, [](const FString& Path) { return IsServerHeader(Path); }).Num(), 1);
	return true;
}

#endif
