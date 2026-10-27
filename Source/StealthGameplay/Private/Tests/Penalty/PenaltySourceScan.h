#pragma once

#include "CoreMinimal.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

/**
 * Source scans of the Penalty feature folder for the StealthGame.Penalty.* lint tests (stories nkp-001, nkp-004): the
 * server-only guard include, the headless core, no literal limit, no logging or client channel. Each check is a pure
 * function over text, so a test also feeds it a seeded violation and watches it fail (ADR-0008 negative controls).
 */
namespace StealthPenaltySourceScan
{
	struct FSource
	{
		/** Path relative to the Penalty folder (forward slashes). */
		FString Path;
		FString Text;
	};

	/** One finding: file, 1-based line and what matched. */
	struct FFinding
	{
		FString Path;
		int32 Line = 0;
		FString What;
	};

	inline FString PenaltyRoot()
	{
		return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("Source/StealthGameplay/Features/Penalty")));
	}

	/** Every .h / .cpp / .inl under the Penalty folder (or SubDir of it), sorted by path. */
	inline TArray<FSource> LoadSources(const FString& SubDir = FString())
	{
		const FString Root = PenaltyRoot();
		const FString Dir = SubDir.IsEmpty() ? Root : FPaths::Combine(Root, SubDir);
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

	/** The text with comments and the contents of string and character literals blanked (newlines kept). */
	inline FString StripCommentsAndStrings(const FString& Text)
	{
		FString Out;
		Out.Reserve(Text.Len());
		enum class EState { Code, Line, Block, String, Char } State = EState::Code;
		for (int32 Index = 0; Index < Text.Len(); ++Index)
		{
			const TCHAR C = Text[Index];
			const TCHAR Next = Index + 1 < Text.Len() ? Text[Index + 1] : TEXT('\0');
			switch (State)
			{
			case EState::Code:
				if (C == TEXT('/') && Next == TEXT('/')) { State = EState::Line; Out.AppendChar(TEXT(' ')); }
				else if (C == TEXT('/') && Next == TEXT('*')) { State = EState::Block; Out.AppendChar(TEXT(' ')); ++Index; Out.AppendChar(TEXT(' ')); }
				else if (C == TEXT('"')) { State = EState::String; Out.AppendChar(C); }
				else if (C == TEXT('\'')) { State = EState::Char; Out.AppendChar(C); }
				else { Out.AppendChar(C); }
				break;
			case EState::Line:
				if (C == TEXT('\n')) { State = EState::Code; Out.AppendChar(C); }
				else { Out.AppendChar(TEXT(' ')); }
				break;
			case EState::Block:
				if (C == TEXT('*') && Next == TEXT('/')) { State = EState::Code; Out.AppendChar(TEXT(' ')); ++Index; Out.AppendChar(TEXT(' ')); }
				else { Out.AppendChar(C == TEXT('\n') ? C : TEXT(' ')); }
				break;
			case EState::String:
			case EState::Char:
				if (C == TEXT('\\')) { Out.AppendChar(TEXT(' ')); ++Index; Out.AppendChar(TEXT(' ')); }
				else if ((State == EState::String && C == TEXT('"')) || (State == EState::Char && C == TEXT('\''))) { State = EState::Code; Out.AppendChar(C); }
				else { Out.AppendChar(C == TEXT('\n') ? C : TEXT(' ')); }
				break;
			}
		}
		return Out;
	}

	inline bool IsIdentChar(TCHAR C)
	{
		return FChar::IsAlnum(C) || C == TEXT('_');
	}

	/** The target of the first #include outside comments ("" when there is none). */
	inline FString FirstInclude(const FString& Text)
	{
		TArray<FString> Lines;
		StripCommentsAndStrings(Text).ParseIntoArrayLines(Lines, /*bCullEmpty*/ false);
		TArray<FString> Raw;
		Text.ParseIntoArrayLines(Raw, false);
		for (int32 Index = 0; Index < Lines.Num(); ++Index)
		{
			if (Lines[Index].TrimStart().StartsWith(TEXT("#include")) && Raw.IsValidIndex(Index))
			{
				FString Target;
				const FString& Line = Raw[Index];
				int32 Open = INDEX_NONE;
				int32 Close = INDEX_NONE;
				if (Line.FindChar(TEXT('"'), Open) && Line.FindLastChar(TEXT('"'), Close) && Close > Open)
				{
					Target = Line.Mid(Open + 1, Close - Open - 1);
				}
				else if (Line.FindChar(TEXT('<'), Open) && Line.FindLastChar(TEXT('>'), Close) && Close > Open)
				{
					Target = Line.Mid(Open + 1, Close - Open - 1);
				}
				return Target;
			}
		}
		return FString();
	}

	/** Lines (comment- and string-free) where an identifier STARTS with one of Prefixes (e.g. UE_LOG matches UE_LOGF). */
	inline void FindIdentifierPrefixes(const FSource& Source, TConstArrayView<const TCHAR*> Prefixes, TArray<FFinding>& Out)
	{
		TArray<FString> Lines;
		StripCommentsAndStrings(Source.Text).ParseIntoArrayLines(Lines, false);
		for (int32 LineIndex = 0; LineIndex < Lines.Num(); ++LineIndex)
		{
			const FString& Line = Lines[LineIndex];
			for (const TCHAR* Prefix : Prefixes)
			{
				int32 From = 0;
				while (true)
				{
					const int32 At = Line.Find(Prefix, ESearchCase::CaseSensitive, ESearchDir::FromStart, From);
					if (At == INDEX_NONE)
					{
						break;
					}
					if (At == 0 || !IsIdentChar(Line[At - 1]))
					{
						Out.Add({ Source.Path, LineIndex + 1, Prefix });
					}
					From = At + 1;
				}
			}
		}
	}

	/**
	 * Lines that mention "Limit" in code and carry a decimal integer literal in L's hard range 2..5 (no literal limit in
	 * Penalty sources, nkp-001 AC2), plus any use of the prototype setting NpcKillLimit.
	 */
	inline void FindLiteralLimits(const FSource& Source, TArray<FFinding>& Out)
	{
		TArray<FString> Lines;
		StripCommentsAndStrings(Source.Text).ParseIntoArrayLines(Lines, false);
		for (int32 LineIndex = 0; LineIndex < Lines.Num(); ++LineIndex)
		{
			const FString& Line = Lines[LineIndex];
			if (!Line.Contains(TEXT("Limit"), ESearchCase::CaseSensitive))
			{
				continue;
			}
			TArray<FFinding> Prototype;
			static const TCHAR* const PrototypeSetting[] = { TEXT("NpcKillLimit") };
			FindIdentifierPrefixes(FSource{ Source.Path, Line }, PrototypeSetting, Prototype);
			if (Prototype.Num() > 0)
			{
				Out.Add({ Source.Path, LineIndex + 1, TEXT("NpcKillLimit") });
			}
			for (int32 Index = 0; Index < Line.Len(); ++Index)
			{
				if (!FChar::IsDigit(Line[Index]) || (Index > 0 && (IsIdentChar(Line[Index - 1]) || Line[Index - 1] == TEXT('.'))))
				{
					continue;
				}
				int32 End = Index;
				while (End < Line.Len() && (IsIdentChar(Line[End]) || Line[End] == TEXT('.')))
				{
					++End;
				}
				const FString Token = Line.Mid(Index, End - Index);
				if (Token.IsNumeric() && !Token.Contains(TEXT(".")))
				{
					const int32 Value = FCString::Atoi(*Token);
					if (Value >= 2 && Value <= 5)
					{
						Out.Add({ Source.Path, LineIndex + 1, FString::Printf(TEXT("literal %s on a Limit line"), *Token) });
					}
				}
				Index = End;
			}
		}
	}

	/** One line per finding, for a test message. */
	inline FString Describe(const TArray<FFinding>& Findings)
	{
		FString Out;
		for (const FFinding& Finding : Findings)
		{
			Out += FString::Printf(TEXT("%s:%d %s; "), *Finding.Path, Finding.Line, *Finding.What);
		}
		return Out;
	}
}
