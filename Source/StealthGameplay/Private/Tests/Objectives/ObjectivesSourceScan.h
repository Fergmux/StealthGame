#pragma once

// Source scans of the StealthGame.Objectives.* static checks (stories oc-001 AC4, oc-003 AC3, oc-007 AC1, oc-008 AC4): which
// files name a symbol, comment- and literal-blind, as pure functions over text so each test also feeds a seeded violation and
// watches it fail (ADR-0008 negative controls). The full include lint is story oc-022's ObjectivesIncludeLint. Test-only.

#include "CoreMinimal.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace StealthObjectivesSourceScan
{
	struct FSource
	{
		/** Path relative to Source/ (forward slashes). */
		FString Path;
		FString Text;
	};

	inline FString SourceRoot()
	{
		return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("Source")));
	}

	/** Every .h / .cpp / .inl under Source/<SubDir> (a folder relative to Source/), sorted by path. */
	inline TArray<FSource> LoadSources(const FString& SubDir)
	{
		const FString Root = SourceRoot();
		const FString Dir = FPaths::Combine(Root, SubDir);
		TArray<FString> Files;
		IFileManager::Get().FindFilesRecursive(Files, *Dir, TEXT("*.*"), /*Files*/ true, /*Directories*/ false);
		Files.Sort();
		TArray<FSource> Out;
		for (const FString& File : Files)
		{
			if (!File.EndsWith(TEXT(".h")) && !File.EndsWith(TEXT(".cpp")) && !File.EndsWith(TEXT(".inl")))
			{
				continue;
			}
			FSource Source;
			Source.Path = File.RightChop(Root.Len() + 1).Replace(TEXT("\\"), TEXT("/"));
			if (FFileHelper::LoadFileToString(Source.Text, *File))
			{
				Out.Add(MoveTemp(Source));
			}
		}
		return Out;
	}

	/**
	 * The client-module sources (ADR-0007): the StealthPresentation module and every View/ folder of the runtime modules
	 * (client-safe code compiled on clients). Server/ folders, tests and the composition root are not client modules.
	 */
	inline TArray<FSource> LoadClientSources()
	{
		TArray<FSource> Out;
		for (const TCHAR* Module : { TEXT("StealthFoundation"), TEXT("StealthGameplay"), TEXT("StealthPresentation") })
		{
			const bool bWholeModule = FCString::Strcmp(Module, TEXT("StealthPresentation")) == 0;
			for (FSource& Source : LoadSources(Module))
			{
				const bool bClient = bWholeModule || Source.Path.Contains(TEXT("/View/"));
				if (bClient && !Source.Path.Contains(TEXT("/Tests/")))
				{
					Out.Add(MoveTemp(Source));
				}
			}
		}
		return Out;
	}

	/** The text with // and block comments and the contents of string and character literals blanked (newlines kept). */
	inline FString StripCommentsAndLiterals(const FString& Text)
	{
		FString Out = Text;
		const int32 Len = Out.Len();
		int32 Index = 0;
		while (Index < Len)
		{
			const TCHAR C = Out[Index];
			const TCHAR Next = Index + 1 < Len ? Out[Index + 1] : TEXT('\0');
			if (C == TEXT('/') && Next == TEXT('/'))
			{
				while (Index < Len && Out[Index] != TEXT('\n'))
				{
					Out[Index++] = TEXT(' ');
				}
				continue;
			}
			if (C == TEXT('/') && Next == TEXT('*'))
			{
				while (Index < Len && !(Out[Index] == TEXT('*') && Index + 1 < Len && Out[Index + 1] == TEXT('/')))
				{
					if (Out[Index] != TEXT('\n'))
					{
						Out[Index] = TEXT(' ');
					}
					++Index;
				}
				if (Index < Len)
				{
					Out[Index++] = TEXT(' ');
					if (Index < Len)
					{
						Out[Index++] = TEXT(' ');
					}
				}
				continue;
			}
			if (C == TEXT('"') || C == TEXT('\''))
			{
				const TCHAR Quote = C;
				++Index;
				while (Index < Len && Out[Index] != Quote && Out[Index] != TEXT('\n'))
				{
					if (Out[Index] == TEXT('\\') && Index + 1 < Len)
					{
						Out[Index++] = TEXT(' ');
					}
					Out[Index++] = TEXT(' ');
				}
				++Index;
				continue;
			}
			++Index;
		}
		return Out;
	}

	inline bool IsIdentChar(TCHAR C)
	{
		return FChar::IsAlnum(C) || C == TEXT('_');
	}

	/** True when Name occurs in Text as a whole identifier. */
	inline bool ContainsIdentifier(const FString& Text, const FString& Name)
	{
		int32 From = 0;
		while (true)
		{
			const int32 At = Text.Find(Name, ESearchCase::CaseSensitive, ESearchDir::FromStart, From);
			if (At == INDEX_NONE)
			{
				return false;
			}
			const bool bStart = At == 0 || !IsIdentChar(Text[At - 1]);
			const int32 End = At + Name.Len();
			const bool bEnd = End >= Text.Len() || !IsIdentChar(Text[End]);
			if (bStart && bEnd)
			{
				return true;
			}
			From = At + 1;
		}
	}

	/**
	 * Lines (1-based) of Text whose code names Name OUTSIDE a "#if !UE_BUILD_SHIPPING" region (nested #if/#ifdef blocks are
	 * tracked; an #else of the guard leaves it). Empty: every occurrence is compiled out of Shipping.
	 */
	inline TArray<int32> FindOutsideShippingGuard(const FString& Text, const FString& Name)
	{
		TArray<FString> Lines;
		StripCommentsAndLiterals(Text).ParseIntoArrayLines(Lines, /*bCullEmpty*/ false);
		TArray<bool> Stack; // per open #if: true when it is the non-Shipping guard
		TArray<int32> Out;
		for (int32 Index = 0; Index < Lines.Num(); ++Index)
		{
			const FString Line = Lines[Index].TrimStartAndEnd();
			if (Line.StartsWith(TEXT("#if")))
			{
				Stack.Add(Line.Replace(TEXT(" "), TEXT("")) == TEXT("#if!UE_BUILD_SHIPPING"));
				continue;
			}
			if (Line.StartsWith(TEXT("#else")) || Line.StartsWith(TEXT("#elif")))
			{
				if (Stack.Num() > 0)
				{
					Stack.Last() = false;
				}
				continue;
			}
			if (Line.StartsWith(TEXT("#endif")))
			{
				if (Stack.Num() > 0)
				{
					Stack.Pop();
				}
				continue;
			}
			if (ContainsIdentifier(Line, Name) && !Stack.Contains(true))
			{
				Out.Add(Index + 1);
			}
		}
		return Out;
	}

	/** "path: name" for every source whose code (comments and literals blanked) names one of Names. */
	inline TArray<FString> FindNames(TConstArrayView<FSource> Sources, TConstArrayView<FString> Names)
	{
		TArray<FString> Findings;
		for (const FSource& Source : Sources)
		{
			const FString Code = StripCommentsAndLiterals(Source.Text);
			for (const FString& Name : Names)
			{
				if (ContainsIdentifier(Code, Name))
				{
					Findings.Add(FString::Printf(TEXT("%s: %s"), *Source.Path, *Name));
				}
			}
		}
		return Findings;
	}

	// --- Include scans (stories oc-016 AC2 and oc-022 AC2, ObjectivesIncludeLint) ----------------------------------------

	/** The quoted #include paths of Text (comments blanked first; angle-bracket includes are engine/system, never ours). */
	inline TArray<FString> ParseIncludes(const FString& Text)
	{
		TArray<FString> Lines;
		Text.ParseIntoArrayLines(Lines, /*bCullEmpty*/ false);
		TArray<FString> Out;
		bool bInBlock = false;
		for (const FString& Raw : Lines)
		{
			FString Line = Raw.TrimStartAndEnd();
			if (bInBlock)
			{
				const int32 End = Line.Find(TEXT("*/"));
				if (End == INDEX_NONE)
				{
					continue;
				}
				Line = Line.RightChop(End + 2).TrimStartAndEnd();
				bInBlock = false;
			}
			if (Line.StartsWith(TEXT("/*")) && !Line.Contains(TEXT("*/")))
			{
				bInBlock = true;
				continue;
			}
			if (!Line.StartsWith(TEXT("#include")))
			{
				continue;
			}
			const int32 Open = Line.Find(TEXT("\""));
			const int32 Close = Open != INDEX_NONE ? Line.Find(TEXT("\""), ESearchCase::CaseSensitive, ESearchDir::FromStart, Open + 1) : INDEX_NONE;
			if (Open != INDEX_NONE && Close > Open)
			{
				Out.Add(Line.Mid(Open + 1, Close - Open - 1));
			}
		}
		return Out;
	}

	/** The project source an include path names (a path ending in /IncludePath; the shortest match wins), or null. */
	inline const FSource* ResolveInclude(TConstArrayView<FSource> Project, const FString& IncludePath)
	{
		const FSource* Best = nullptr;
		const FString Suffix = TEXT("/") + IncludePath;
		for (const FSource& Source : Project)
		{
			if (Source.Path.EndsWith(Suffix) && (Best == nullptr || Source.Path.Len() < Best->Path.Len()))
			{
				Best = &Source;
			}
		}
		return Best;
	}

	/**
	 * Every forbidden include reached from Start through project headers (transitively): "start -> ... -> header: include".
	 * IsForbidden decides on the resolved header's path (or the raw include path when it is not a project file).
	 */
	inline TArray<FString> FindForbiddenIncludes(TConstArrayView<FSource> Project, const FSource& Start, TFunctionRef<bool(const FString& /*ResolvedPathOrInclude*/)> IsForbidden)
	{
		TArray<FString> Findings;
		TSet<FString> Visited;
		TArray<TPair<const FSource*, FString>> Stack;
		Stack.Emplace(&Start, Start.Path);
		while (Stack.Num() > 0)
		{
			const TPair<const FSource*, FString> Item = Stack.Pop();
			for (const FString& Include : ParseIncludes(Item.Key->Text))
			{
				const FSource* Resolved = ResolveInclude(Project, Include);
				const FString Path = Resolved != nullptr ? Resolved->Path : Include;
				if (IsForbidden(Path))
				{
					Findings.Add(FString::Printf(TEXT("%s: %s"), *Item.Value, *Include));
					continue;
				}
				if (Resolved != nullptr && Resolved->Path.EndsWith(TEXT(".h")) && !Visited.Contains(Resolved->Path))
				{
					Visited.Add(Resolved->Path);
					Stack.Emplace(Resolved, Item.Value + TEXT(" -> ") + Resolved->Path);
				}
			}
		}
		return Findings;
	}

	/** Every project source of the runtime modules and the presentation module (the include graph's universe). */
	inline TArray<FSource> LoadProjectSources()
	{
		TArray<FSource> Out;
		for (const TCHAR* Module : { TEXT("StealthFoundation"), TEXT("StealthGameplay"), TEXT("StealthPresentation"), TEXT("StealthGame") })
		{
			Out.Append(LoadSources(Module));
		}
		return Out;
	}
}
