// Story nkp-021 (production/epics/npc-kill-penalty-informants/story-021-retire-prototype-penalty-code.md), AC3 (derived,
// ADR-0019 migration; ADR-0007 "replaced, not promoted"): StealthGame.Penalty.PrototypeRetired. The prototype module
// Source/StealthGame (every .h and .cpp under it, tests included) names none of the retired prototype penalty symbols:
// EliminatePlayer, ExilePlayer, RespawnInformant, the NpcKills counter, ShouldRemoveForNpcKills and the test name
// StealthGame.Rules.NpcKillRemoval. Negative control: the same scan over a seeded source that calls ExilePlayer reports it.
// The build half of AC3 is the gate itself (this test only runs in a build that compiled without them).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Server/Testing/StealthNegativeControl.h"

namespace PenaltyPrototypeRetiredTestData
{
	/** The retired prototype penalty symbols (story nkp-021 AC3's term set). */
	const TCHAR* const RetiredTerms[] = { TEXT("EliminatePlayer"), TEXT("ExilePlayer"), TEXT("RespawnInformant"), TEXT("NpcKills"), TEXT("ShouldRemoveForNpcKills"),
		TEXT("StealthGame.Rules.NpcKillRemoval") };

	/** "<file>:<line> <term>" for every retired term in Text. */
	void Scan(const FString& Path, const FString& Text, TArray<FString>& OutFindings)
	{
		TArray<FString> Lines;
		Text.ParseIntoArrayLines(Lines, false);
		for (int32 Index = 0; Index < Lines.Num(); ++Index)
		{
			for (const TCHAR* Term : RetiredTerms)
			{
				if (Lines[Index].Contains(Term, ESearchCase::CaseSensitive))
				{
					OutFindings.Add(FString::Printf(TEXT("%s:%d %s"), *Path, Index + 1, Term));
				}
			}
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPenaltyPrototypeRetiredTest, "StealthGame.Penalty.PrototypeRetired",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPenaltyPrototypeRetiredTest::RunTest(const FString& Parameters)
{
	using namespace PenaltyPrototypeRetiredTestData;
	const FString Root = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("Source/StealthGame"));
	TArray<FString> Files;
	IFileManager::Get().FindFilesRecursive(Files, *Root, TEXT("*.h"), true, false);
	IFileManager::Get().FindFilesRecursive(Files, *Root, TEXT("*.cpp"), true, false, false);
	TestTrue(FString::Printf(TEXT("the prototype module's sources were found (%d under %s)"), Files.Num(), *Root), Files.Num() > 20);

	TArray<FString> Findings;
	for (const FString& File : Files)
	{
		FString Text;
		if (TestTrue(FString::Printf(TEXT("%s is readable"), *File), FFileHelper::LoadFileToString(Text, *File)))
		{
			Scan(File, Text, Findings);
		}
	}
	for (const FString& Finding : Findings)
	{
		AddError(FString::Printf(TEXT("retired prototype penalty symbol still present: %s"), *Finding));
	}
	TestEqual(TEXT("Source/StealthGame names no retired prototype penalty symbol"), Findings.Num(), 0);

	FStealthNegativeControl Control(TEXT("SeededExilePlayerCall"));
	FStealthNegativeControl::Report(*this, Control.Run([](FStealthCheckContext& Ctx)
	{
		TArray<FString> Seeded;
		Scan(TEXT("Seeded.cpp"), TEXT("void F()\n{\n\tMode->ExilePlayer(Offender);\n}\n"), Seeded);
		Ctx.TestTrue(TEXT("the seeded source names no retired symbol"), Seeded.IsEmpty());
	}));
	return true;
}

#endif
