// Story rmf-005 (production/epics/round-match-framework/story-005-test-fixture-constants.md):
// [STATIC] scan, BLOCKING in the test run. No RoundLogHeaderFields name, RoundTestSeeds secret or fixture code
// string appears in a test source outside the test constant file (Server/Testing/StealthRoundTestConstants.h).
//
// Globs (decision D48: the Foundation glob is recursive):
//   Source/*/Private/Tests/Round/*.cpp   and   Source/StealthFoundation/Private/Tests/**/*.cpp
// A glob matching zero files fails. Raw text is scanned, comments included (RMF Exit A ruling):
//   - a header field name or a code string counts when it appears as a whole quoted string literal;
//   - a secret word counts when its 16 hex digits appear anywhere, in any case.
// The banned lists are read from the constant file itself (compiled in), so a new field cannot leave the scan
// stale. A hit is reported as file, line and category only, never the matched secret. The planted-violation
// directory tests/static-fixtures/fixture-constants-violation/ (outside both globs, never compiled) proves the scan
// can fail: it is run as a negative control and must be detected.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Server/Testing/StealthNegativeControl.h"
#include "Server/Testing/StealthRoundTestConstants.h"

namespace FixtureConstantsTestData
{
	/** The allowlisted constant files (relative to the project directory); never scanned. */
	const TCHAR* const ConstantFiles[] = {
		TEXT("Source/StealthFoundation/Public/Server/Testing/StealthRoundTestConstants.h"),
	};

	const TCHAR* const PlantedViolationDir = TEXT("tests/static-fixtures/fixture-constants-violation");

	struct FScanHit
	{
		FString File;
		int32 Line = 0;
		FString Category;
	};

	FString ProjectPath(const FString& Relative)
	{
		return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), Relative));
	}

	bool IsAllowlisted(const FString& FullPath)
	{
		for (const TCHAR* Constant : ConstantFiles)
		{
			if (FPaths::IsSamePath(FullPath, ProjectPath(Constant)))
			{
				return true;
			}
		}
		return false;
	}

	/** Glob 1: Source/<module>/Private/Tests/Round/*.cpp for every module (non-recursive inside Round/). */
	TArray<FString> CollectRoundGlob()
	{
		TArray<FString> Files;
		TArray<FString> Modules;
		IFileManager::Get().FindFiles(Modules, *FPaths::Combine(ProjectPath(TEXT("Source")), TEXT("*")), false, true);
		Modules.Sort();
		for (const FString& Module : Modules)
		{
			const FString Dir = FPaths::Combine(ProjectPath(TEXT("Source")), Module, TEXT("Private/Tests/Round"));
			TArray<FString> Names;
			IFileManager::Get().FindFiles(Names, *FPaths::Combine(Dir, TEXT("*.cpp")), true, false);
			Names.Sort();
			for (const FString& Name : Names)
			{
				Files.Add(FPaths::Combine(Dir, Name));
			}
		}
		return Files;
	}

	/** Glob 2: Source/StealthFoundation/Private/Tests/**\/*.cpp (recursive, D48), or any directory recursively. */
	TArray<FString> CollectRecursive(const FString& Dir, const TCHAR* Pattern)
	{
		TArray<FString> Files;
		IFileManager::Get().FindFilesRecursive(Files, *Dir, Pattern, true, false);
		Files.Sort();
		return Files;
	}

	/** Scans one file's raw text; appends hits (category only, never the matched value). */
	void ScanFile(const FString& Path, TArray<FScanHit>& OutHits)
	{
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *Path))
		{
			OutHits.Add({ Path, 0, TEXT("unreadable") });
			return;
		}
		TArray<FString> Lines;
		Text.ParseIntoArrayLines(Lines, false);

		TArray<FString> QuotedHeaderFields;
		for (const TCHAR* Field : RoundTestFixtures::RoundLogHeaderFields)
		{
			QuotedHeaderFields.Add(FString::Printf(TEXT("\"%s\""), Field));
		}
		TArray<FString> QuotedCodes;
		for (const TCHAR* Code : RoundTestFixtures::FixtureCodeStrings)
		{
			QuotedCodes.Add(FString::Printf(TEXT("\"%s\""), Code));
		}
		TArray<FString> SecretWords;
		for (const RoundTestFixtures::FRoundTestSecret& Secret : RoundTestFixtures::RoundTestSeeds)
		{
			for (const uint64 Word : { Secret.Word0, Secret.Word1, Secret.MatchNonce })
			{
				SecretWords.Add(FString::Printf(TEXT("%016llX"), Word));
			}
		}

		for (int32 LineIndex = 0; LineIndex < Lines.Num(); ++LineIndex)
		{
			const FString& Line = Lines[LineIndex];
			for (const FString& Quoted : QuotedHeaderFields)
			{
				if (Line.Contains(Quoted, ESearchCase::CaseSensitive))
				{
					OutHits.Add({ Path, LineIndex + 1, TEXT("RoundLogHeaderFields name") });
				}
			}
			for (const FString& Quoted : QuotedCodes)
			{
				if (Line.Contains(Quoted, ESearchCase::CaseSensitive))
				{
					OutHits.Add({ Path, LineIndex + 1, TEXT("fixture code string") });
				}
			}
			for (const FString& Word : SecretWords)
			{
				if (Line.Contains(Word, ESearchCase::IgnoreCase))
				{
					OutHits.Add({ Path, LineIndex + 1, TEXT("RoundTestSeeds secret") });
				}
			}
		}
	}

	/** Scans Files minus the allowlist; returns the hits. */
	TArray<FScanHit> ScanFiles(const TArray<FString>& Files)
	{
		TArray<FScanHit> Hits;
		for (const FString& File : Files)
		{
			if (!IsAllowlisted(File))
			{
				ScanFile(File, Hits);
			}
		}
		return Hits;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoundFixtureConstantsTest, "StealthGame.Round.FixtureConstants",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRoundFixtureConstantsTest::RunTest(const FString& Parameters)
{
	using namespace FixtureConstantsTestData;

	// Arrange: both globs; each must match at least one file.
	const TArray<FString> RoundFiles = CollectRoundGlob();
	const TArray<FString> FoundationFiles = CollectRecursive(ProjectPath(TEXT("Source/StealthFoundation/Private/Tests")), TEXT("*.cpp"));
	TestTrue(FString::Printf(TEXT("glob Source/*/Private/Tests/Round/*.cpp matches files (%d)"), RoundFiles.Num()), RoundFiles.Num() > 0);
	TestTrue(FString::Printf(TEXT("glob Source/StealthFoundation/Private/Tests/**/*.cpp matches files (%d)"), FoundationFiles.Num()), FoundationFiles.Num() > 0);
	TestTrue(TEXT("the constant file exists"), FPaths::FileExists(ProjectPath(ConstantFiles[0])));

	TArray<FString> Scanned = RoundFiles;
	for (const FString& File : FoundationFiles)
	{
		Scanned.AddUnique(File);
	}

	// Act: scan.
	const TArray<FScanHit> Hits = ScanFiles(Scanned);

	// Assert: zero hits (each hit is an error naming file, line and category: the observation list).
	for (const FScanHit& Hit : Hits)
	{
		AddError(FString::Printf(TEXT("fixture literal outside the constant file: %s:%d (%s)"), *Hit.File, Hit.Line, *Hit.Category));
	}
	AddInfo(FString::Printf(TEXT("scanned %d test sources, %d findings"), Scanned.Num(), Hits.Num()));

	// Negative control: the planted-violation directory (outside both globs) must be detected.
	const FString PlantedDir = ProjectPath(PlantedViolationDir);
	const TArray<FString> PlantedFiles = CollectRecursive(PlantedDir, TEXT("*.*"));
	TestTrue(TEXT("the planted-violation fixture exists"), PlantedFiles.Num() > 0);
	for (const FString& Planted : PlantedFiles)
	{
		TestFalse(TEXT("the planted fixture is outside the scanned globs"), Scanned.Contains(Planted));
	}
	FStealthNegativeControl Control(TEXT("PlantedFixtureLiteral"));
	const FStealthNegativeControlResult Result = Control.Run([&PlantedFiles](FStealthCheckContext& Ctx)
	{
		const TArray<FScanHit> PlantedHits = ScanFiles(PlantedFiles);
		Ctx.TestTrue(TEXT("planted directory scans clean"), PlantedHits.Num() == 0 && PlantedFiles.Num() > 0);
	});
	FStealthNegativeControl::Report(*this, Result);
	// Each category is detectable on its own.
	{
		const TArray<FScanHit> PlantedHits = ScanFiles(PlantedFiles);
		TestTrue(TEXT("planted header field literal detected"), PlantedHits.ContainsByPredicate([](const FScanHit& H) { return H.Category == TEXT("RoundLogHeaderFields name"); }));
		TestTrue(TEXT("planted code string detected"), PlantedHits.ContainsByPredicate([](const FScanHit& H) { return H.Category == TEXT("fixture code string"); }));
		TestTrue(TEXT("planted secret word detected"), PlantedHits.ContainsByPredicate([](const FScanHit& H) { return H.Category == TEXT("RoundTestSeeds secret"); }));
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
