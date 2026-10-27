// Story asn-027 AC2 (production/epics/assassination-defence-reversal/story-027-port-rules-reference-tests-retire-prototype.md):
// the static scan of the prototype strike and defence retirement. Over every .h / .cpp / .inl / .cs file under Source/
// (comments and string literals blanked, so history notes and this file's own list do not count) no whole identifier of
// the retired list occurs, and no Config/*.ini line sets one of the five retired UStealthSettings keys. Denominator check:
// at least MinSourceFiles source files and at least one .ini are scanned, StealthCharacter.cpp among them. Negative
// control (ADR-0008 section 3): the same matcher finds a seeded use in code and a seeded ini key, and ignores the
// identifier inside a comment, inside a string and as a part of a longer identifier.
// The list extends IIF story 093's named prototype symbols (production/epics/input-interaction-framework/
// story-093-static-scan-and-prototype-retirement.md); 093's CI grep reuses it.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Tests/Penalty/PenaltySourceScan.h"

namespace PrototypeRetiredScan
{
	/** AC2's symbols, then the rest of this story's retirement (the prototype resolver's rules, enums and helpers). */
	const TCHAR* const RetiredIdentifiers[] = {
		TEXT("KillCharacter"), TEXT("StartDefence"), TEXT("StopDefence"), TEXT("SetDefenceStance"), TEXT("ApplyDeathPresentation"),
		TEXT("ServerAssassinate"), TEXT("ServerSetDefence"), TEXT("HandleAssassinationRequest"), TEXT("HandleDefenceRequest"),
		TEXT("bDying"), TEXT("bPreparingDefence"), TEXT("LastDefenceActivationTime"),
		TEXT("AttackerRecoveryTime"), TEXT("BodyLifetime"), TEXT("BlindStrikeRadius"), TEXT("AssassinationConeHalfAngle"), TEXT("AssassinationServerTolerance"),
		TEXT("ResolveAssassination"), TEXT("CanStartDefence"), TEXT("DefenceCooldownRemaining"), TEXT("ShouldEndDefence"),
		TEXT("EStealthAssassinationOutcome"), TEXT("EStealthTargetKind"), TEXT("IsPreparingDefence"), TEXT("FindAssassinationTarget"),
		TEXT("BeginEncounterDeath"), TEXT("OnRep_Dying"), TEXT("DefenceActivationTime"), TEXT("HandleEncounterCounterPose"),
	};

	/** The five retired UStealthSettings keys (GDD "Prototype keys retired"): none may be set in a Config ini. */
	const TCHAR* const RetiredIniKeys[] = {
		TEXT("AttackerRecoveryTime"), TEXT("BodyLifetime"), TEXT("BlindStrikeRadius"), TEXT("AssassinationConeHalfAngle"), TEXT("AssassinationServerTolerance"),
	};

	/** The project has well over this many source files; fewer means the scan root is wrong. */
	constexpr int32 MinSourceFiles = 200;

	struct FFinding
	{
		FString Path;
		int32 Line = 0;
		FString What;
	};

	/** Whole-identifier uses of Identifiers in Text's code (comments and string literals blanked). */
	void FindIdentifiers(const FString& Path, const FString& Text, TConstArrayView<const TCHAR*> Identifiers, TArray<FFinding>& Out)
	{
		using StealthPenaltySourceScan::IsIdentChar;
		TArray<FString> Lines;
		StealthPenaltySourceScan::StripCommentsAndStrings(Text).ParseIntoArrayLines(Lines, /*bCullEmpty*/ false);
		for (int32 LineIndex = 0; LineIndex < Lines.Num(); ++LineIndex)
		{
			const FString& Line = Lines[LineIndex];
			for (const TCHAR* Identifier : Identifiers)
			{
				const int32 Len = FCString::Strlen(Identifier);
				for (int32 From = 0;;)
				{
					const int32 At = Line.Find(Identifier, ESearchCase::CaseSensitive, ESearchDir::FromStart, From);
					if (At == INDEX_NONE)
					{
						break;
					}
					const bool bStart = At == 0 || !IsIdentChar(Line[At - 1]);
					const bool bEnd = At + Len >= Line.Len() || !IsIdentChar(Line[At + Len]);
					if (bStart && bEnd)
					{
						Out.Add({ Path, LineIndex + 1, Identifier });
					}
					From = At + 1;
				}
			}
		}
	}

	/** Ini lines that set one of Keys (any +, -, ., ! prefix; comments starting with ; or # ignored). */
	void FindIniKeys(const FString& Path, const FString& Text, TConstArrayView<const TCHAR*> Keys, TArray<FFinding>& Out)
	{
		TArray<FString> Lines;
		Text.ParseIntoArrayLines(Lines, /*bCullEmpty*/ false);
		for (int32 LineIndex = 0; LineIndex < Lines.Num(); ++LineIndex)
		{
			FString Line = Lines[LineIndex].TrimStartAndEnd();
			if (Line.IsEmpty() || Line.StartsWith(TEXT(";")) || Line.StartsWith(TEXT("#")))
			{
				continue;
			}
			while (!Line.IsEmpty() && (Line[0] == TEXT('+') || Line[0] == TEXT('-') || Line[0] == TEXT('.') || Line[0] == TEXT('!')))
			{
				Line.RightChopInline(1);
			}
			FString Key;
			FString Value;
			if (!Line.Split(TEXT("="), &Key, &Value))
			{
				continue;
			}
			Key.TrimStartAndEndInline();
			for (const TCHAR* Retired : Keys)
			{
				if (Key.Equals(Retired, ESearchCase::IgnoreCase))
				{
					Out.Add({ Path, LineIndex + 1, Retired });
				}
			}
		}
	}

	/** Every file under Dir whose name ends with one of Extensions, sorted. */
	TArray<FString> FilesUnder(const FString& Dir, TConstArrayView<const TCHAR*> Extensions)
	{
		TArray<FString> All;
		IFileManager::Get().FindFilesRecursive(All, *Dir, TEXT("*.*"), /*Files*/ true, /*Directories*/ false);
		All.Sort();
		return All.FilterByPredicate([Extensions](const FString& File)
		{
			for (const TCHAR* Extension : Extensions)
			{
				if (File.EndsWith(Extension, ESearchCase::IgnoreCase))
				{
					return true;
				}
			}
			return false;
		});
	}

	FString Describe(const TArray<FFinding>& Findings)
	{
		FString Out;
		for (const FFinding& Finding : Findings)
		{
			Out += FString::Printf(TEXT("\n  %s:%d %s"), *Finding.Path, Finding.Line, *Finding.What);
		}
		return Out;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAssassinationPrototypeRetiredScanTest, "StealthGame.Assassination.PrototypeRetiredScan",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAssassinationPrototypeRetiredScanTest::RunTest(const FString& Parameters)
{
	using namespace PrototypeRetiredScan;

	// ---- Negative control: the matchers catch a seeded use and ignore comments, strings and longer identifiers ----
	{
		TArray<FFinding> Seeded;
		FindIdentifiers(TEXT("Seeded.cpp"), TEXT("void F() { Pawn->KillCharacter(); }\n// bDying in a comment\nconst TCHAR* S = TEXT(\"bPreparingDefence\");\nint32 BodyLifetimeSec = 0;\n"),
			RetiredIdentifiers, Seeded);
		TestTrue(FString::Printf(TEXT("control: exactly the seeded KillCharacter use is found (got %d)%s"), Seeded.Num(), *Describe(Seeded)),
			Seeded.Num() == 1 && Seeded[0].What == TEXT("KillCharacter") && Seeded[0].Line == 1);
		TArray<FFinding> SeededIni;
		FindIniKeys(TEXT("Seeded.ini"), TEXT("[/Script/StealthGame.StealthSettings]\n+BodyLifetime=12\n; BlindStrikeRadius=1\nBodyLifetimeSec=3\n"), RetiredIniKeys, SeededIni);
		TestTrue(FString::Printf(TEXT("control: exactly the seeded BodyLifetime ini key is found (got %d)"), SeededIni.Num()),
			SeededIni.Num() == 1 && SeededIni[0].What == TEXT("BodyLifetime") && SeededIni[0].Line == 2);
	}

	// ---- Source/: no retired identifier in code ----
	const FString SourceRoot = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("Source")));
	const TCHAR* const SourceExtensions[] = { TEXT(".h"), TEXT(".cpp"), TEXT(".inl"), TEXT(".cs") };
	const TArray<FString> SourceFiles = FilesUnder(SourceRoot, SourceExtensions);
	TArray<FFinding> Findings;
	int32 Scanned = 0;
	bool bSawCharacter = false;
	for (const FString& File : SourceFiles)
	{
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *File))
		{
			continue;
		}
		++Scanned;
		const FString Relative = File.RightChop(SourceRoot.Len() + 1).Replace(TEXT("\\"), TEXT("/"));
		bSawCharacter |= Relative == TEXT("StealthGame/Private/StealthCharacter.cpp");
		FindIdentifiers(Relative, Text, RetiredIdentifiers, Findings);
	}
	AddInfo(FString::Printf(TEXT("PrototypeRetiredScan: %d source files scanned under %s, %d retired identifiers, %d findings"), Scanned, *SourceRoot,
		static_cast<int32>(UE_ARRAY_COUNT(RetiredIdentifiers)), Findings.Num()));
	TestTrue(FString::Printf(TEXT("denominator: at least %d source files scanned (got %d)"), MinSourceFiles, Scanned), Scanned >= MinSourceFiles);
	TestTrue(TEXT("denominator: StealthGame/Private/StealthCharacter.cpp was scanned"), bSawCharacter);
	TestTrue(FString::Printf(TEXT("no retired prototype strike / defence identifier in code (%d found)%s"), Findings.Num(), *Describe(Findings)), Findings.Num() == 0);

	// ---- Config/: none of the five retired settings keys is set ----
	const FString ConfigRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectConfigDir());
	const TCHAR* const IniExtensions[] = { TEXT(".ini") };
	TArray<FFinding> IniFindings;
	int32 IniScanned = 0;
	for (const FString& File : FilesUnder(ConfigRoot, IniExtensions))
	{
		FString Text;
		if (FFileHelper::LoadFileToString(Text, *File))
		{
			++IniScanned;
			FindIniKeys(File.RightChop(ConfigRoot.Len()), Text, RetiredIniKeys, IniFindings);
		}
	}
	AddInfo(FString::Printf(TEXT("PrototypeRetiredScan: %d ini files scanned under %s, %d findings"), IniScanned, *ConfigRoot, IniFindings.Num()));
	TestTrue(TEXT("denominator: at least one Config ini scanned"), IniScanned > 0);
	TestTrue(FString::Printf(TEXT("no retired UStealthSettings key in Config (%d found)%s"), IniFindings.Num(), *Describe(IniFindings)), IniFindings.Num() == 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
