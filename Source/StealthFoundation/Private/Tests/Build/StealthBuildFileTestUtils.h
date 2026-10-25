#pragma once

// Shared helpers for the StealthGame.Build.* automation tests: they read the project's
// build-description files (.uproject, *.Build.cs, *.Target.cs) as text or JSON and compare
// what they declare. Test-only; compiled inside WITH_DEV_AUTOMATION_TESTS by the includers.

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformProcess.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace StealthBuildTest
{
	/** Absolute path of a file or folder given relative to the project directory. */
	inline FString ProjectPath(const FString& Relative)
	{
		return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), Relative));
	}

	/** Loads a project-relative text file. Returns false when it is missing or unreadable. */
	inline bool LoadProjectText(const FString& Relative, FString& OutText)
	{
		return FFileHelper::LoadFileToString(OutText, *ProjectPath(Relative));
	}

	/** Removes // line comments so commented-out declarations are not read as live ones. */
	inline FString StripLineComments(const FString& Text)
	{
		TArray<FString> Lines;
		Text.ParseIntoArrayLines(Lines, /*bCullEmpty*/ false);
		FString Out;
		for (const FString& Line : Lines)
		{
			const int32 Comment = Line.Find(TEXT("//"));
			Out += (Comment == INDEX_NONE ? Line : Line.Left(Comment));
			Out += TEXT("\n");
		}
		return Out;
	}

	/** Every double-quoted string literal in Text (quotes removed). */
	inline TSet<FString> ExtractQuotedTokens(const FString& Text)
	{
		TSet<FString> Tokens;
		int32 Open = Text.Find(TEXT("\""));
		while (Open != INDEX_NONE)
		{
			const int32 Close = Text.Find(TEXT("\""), ESearchCase::CaseSensitive, ESearchDir::FromStart, Open + 1);
			if (Close == INDEX_NONE)
			{
				break;
			}
			Tokens.Add(Text.Mid(Open + 1, Close - Open - 1));
			Open = Text.Find(TEXT("\""), ESearchCase::CaseSensitive, ESearchDir::FromStart, Close + 1);
		}
		return Tokens;
	}

	/**
	 * Quoted names in every statement that mentions Identifier (from the identifier up to the next
	 * ';'), for example all modules added to PublicDependencyModuleNames, whether by Add or AddRange.
	 * Comments are stripped first.
	 */
	inline TSet<FString> ExtractStatementTokens(const FString& SourceText, const FString& Identifier)
	{
		const FString Text = StripLineComments(SourceText);
		TSet<FString> Tokens;
		int32 At = Text.Find(Identifier, ESearchCase::CaseSensitive);
		while (At != INDEX_NONE)
		{
			const int32 End = Text.Find(TEXT(";"), ESearchCase::CaseSensitive, ESearchDir::FromStart, At);
			const FString Statement = End == INDEX_NONE ? Text.Mid(At) : Text.Mid(At, End - At);
			Tokens.Append(ExtractQuotedTokens(Statement));
			At = Text.Find(Identifier, ESearchCase::CaseSensitive, ESearchDir::FromStart, At + Identifier.Len());
		}
		return Tokens;
	}

	/** Empty when the sets are equal, else a readable "missing [..] extra [..]" description. */
	inline FString DescribeSetDifference(const TSet<FString>& Expected, const TSet<FString>& Actual)
	{
		const TArray<FString> Missing = Expected.Difference(Actual).Array();
		const TArray<FString> Extra = Actual.Difference(Expected).Array();
		if (Missing.IsEmpty() && Extra.IsEmpty())
		{
			return FString();
		}
		return FString::Printf(TEXT("missing [%s] extra [%s]"),
			*FString::Join(Missing, TEXT(", ")), *FString::Join(Extra, TEXT(", ")));
	}

	/**
	 * Path of the bash used to run the project's CI shell scripts: the STEALTH_BASH environment
	 * variable when set, else Git for Windows' bash. Never a bare "bash" from PATH, which on Windows
	 * can resolve to the WSL launcher. Empty when none is found.
	 */
	inline FString FindBash()
	{
		const FString FromEnv = FPlatformMisc::GetEnvironmentVariable(TEXT("STEALTH_BASH"));
		if (!FromEnv.IsEmpty() && FPaths::FileExists(FromEnv))
		{
			return FromEnv;
		}
		for (const TCHAR* EnvVar : { TEXT("ProgramW6432"), TEXT("ProgramFiles") })
		{
			const FString Root = FPlatformMisc::GetEnvironmentVariable(EnvVar);
			const FString Candidate = FPaths::Combine(Root, TEXT("Git/bin/bash.exe"));
			if (!Root.IsEmpty() && FPaths::FileExists(Candidate))
			{
				return Candidate;
			}
		}
		return FPaths::FileExists(TEXT("/bin/bash")) ? FString(TEXT("/bin/bash")) : FString();
	}

	/** Result of one script run. */
	struct FScriptRun
	{
		bool bLaunched = false;
		int32 ExitCode = -1;
		FString Output;
	};

	/** Runs a project-relative bash script with optional extra arguments and captures its output. */
	inline FScriptRun RunBashScript(const FString& Bash, const FString& RelativeScript, const FString& ExtraArgs = FString())
	{
		FScriptRun Run;
		const FString Script = ProjectPath(RelativeScript);
		const FString Args = FString::Printf(TEXT("\"%s\" %s"), *Script, *ExtraArgs);
		FString StdErr;
		Run.bLaunched = FPlatformProcess::ExecProcess(*Bash, *Args, &Run.ExitCode, &Run.Output, &StdErr);
		Run.Output += StdErr;
		return Run;
	}

	/** Parses the project's .uproject. Returns null when it cannot be read or parsed. */
	inline TSharedPtr<FJsonObject> LoadUProject()
	{
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *FPaths::GetProjectFilePath()))
		{
			return nullptr;
		}
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
		return FJsonSerializer::Deserialize(Reader, Root) ? Root : nullptr;
	}

	/** Every "Modules" entry of the .uproject whose Name equals ModuleName. */
	inline TArray<TSharedPtr<FJsonObject>> FindUProjectModuleEntries(const TSharedPtr<FJsonObject>& UProject, const FString& ModuleName)
	{
		TArray<TSharedPtr<FJsonObject>> Found;
		const TArray<TSharedPtr<FJsonValue>>* Modules = nullptr;
		if (UProject.IsValid() && UProject->TryGetArrayField(TEXT("Modules"), Modules))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Modules)
			{
				const TSharedPtr<FJsonObject>* Entry = nullptr;
				FString Name;
				if (Value.IsValid() && Value->TryGetObject(Entry) && (*Entry)->TryGetStringField(TEXT("Name"), Name) && Name == ModuleName)
				{
					Found.Add(*Entry);
				}
			}
		}
		return Found;
	}

	/**
	 * Shared body of the include-lint wrapper tests (G2): runs a lint selftest script, requires its
	 * overall PASS line, every named case "ok" and every named seeded observation echoed, then runs
	 * tools/ci/lint/gate.sh over the given rule scripts on the real tree and requires exit 0 and
	 * "GATE: PASS". Returns false when bash cannot be found.
	 */
	inline bool RunLintSelftestSuite(FAutomationTestBase& Test, const FString& SelftestScript, const TArray<FString>& RuleScripts,
		TConstArrayView<const TCHAR*> Cases, TConstArrayView<const TCHAR*> SeededObservations)
	{
		const FString Bash = FindBash();
		if (!Test.TestFalse(TEXT("A bash for the CI scripts was found (set STEALTH_BASH to override)"), Bash.IsEmpty()))
		{
			return false;
		}

		const FScriptRun Selftest = RunBashScript(Bash, SelftestScript);
		Test.AddInfo(Selftest.Output);
		if (Test.TestTrue(TEXT("Selftest script launched"), Selftest.bLaunched))
		{
			Test.TestEqual(TEXT("Selftest exit code"), Selftest.ExitCode, 0);
			Test.TestTrue(TEXT("Selftest reports PASS"), Selftest.Output.Contains(TEXT("SELFTEST RESULT: PASS")));
			for (const TCHAR* Case : Cases)
			{
				Test.TestTrue(FString::Printf(TEXT("Selftest case %s ok"), Case), Selftest.Output.Contains(FString::Printf(TEXT("SELFTEST %s ok"), Case)));
			}
			for (const TCHAR* Observation : SeededObservations)
			{
				Test.TestTrue(FString::Printf(TEXT("Seeded violation reported: '%s'"), Observation), Selftest.Output.Contains(Observation));
			}
		}

		FString GateArgs;
		for (const FString& Rule : RuleScripts)
		{
			GateArgs += FString::Printf(TEXT("\"%s\" "), *ProjectPath(Rule));
		}
		const FScriptRun RealTree = RunBashScript(Bash, TEXT("tools/ci/lint/gate.sh"), GateArgs);
		Test.AddInfo(RealTree.Output);
		if (Test.TestTrue(TEXT("Gate on the real tree launched"), RealTree.bLaunched))
		{
			Test.TestEqual(TEXT("Real tree: gate exit code 0 (zero findings)"), RealTree.ExitCode, 0);
			Test.TestTrue(TEXT("Real tree: GATE: PASS"), RealTree.Output.Contains(TEXT("GATE: PASS")));
		}
		return true;
	}
}
