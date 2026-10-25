#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"

#if WITH_STEALTH_TEST_SEAMS

#include "HAL/FileManager.h"
#include "Internationalization/Regex.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

/**
 * Class parity scan (AC-SCL-01, story SCL-004; ADR-0001 section 1, ADR-0010 "Kind-blind code",
 * ADR-0013 section 1): no observable path of the Shared Pawn, its movement component, its
 * AnimInstance or its Blueprints may read the controller kind, local control, the PlayerState or
 * Roster state, except the SCL R9 owner-local uses on the committed allowlist.
 *
 * This header holds the parts both scanners share: the committed fixture
 * (tests/fixtures/scl/class-parity-fixture.txt), identifier tokenising, the C++ source scanner and
 * the judge. The Blueprint graph walker lives in StealthGameEditor (it needs editor graph types)
 * and reports into the same FStealthParityRef list. Compiled only under WITH_STEALTH_TEST_SEAMS.
 *
 * Fail-closed: anything a scanner cannot read is recorded as NOT ASSESSED and the verdict fails.
 *
 * Usage:
 *   FStealthParityFixture Fixture;
 *   FString Error;
 *   if (!FStealthParityFixture::LoadFromProject(Fixture, Error)) { AddError(Error); }
 *   FStealthParityScanResult Result;
 *   StealthClassParity::ScanCppRoots(Fixture, Result);
 *   const FStealthParityVerdict Verdict = StealthClassParity::Judge(Result, Fixture);
 *   if (!Verdict.Passed()) { ... Verdict.Describe() ... }
 */

/** One forbidden-symbol occurrence: which root, in which function or graph, where, which token. */
struct FStealthParityRef
{
	/** C++ file (repo-relative) or Blueprint object path. */
	FString Root;
	/** Function ("AStealthPawn::Restart") or top-level graph name ("CanSprint"); "<file>" at file scope. */
	FString Scope;
	/** Line ("L42") or graph/node path, for messages only. */
	FString Location;
	/** The forbidden identifier. */
	FString Token;

	/** One tab-separated line: Root, Scope, Location, Token. */
	FString ToLine() const { return FString::Printf(TEXT("%s\t%s\t%s\t%s"), *Root, *Scope, *Location, *Token); }

	/** Parses ToLine() output; false when the line does not have four fields. */
	static bool FromLine(const FString& Line, FStealthParityRef& Out)
	{
		TArray<FString> Fields;
		Line.ParseIntoArray(Fields, TEXT("\t"), false);
		if (Fields.Num() != 4)
		{
			return false;
		}
		Out = { Fields[0], Fields[1], Fields[2], Fields[3] };
		return true;
	}
};

/** An R9 owner-local use that may read a forbidden symbol: (root, scope, token) and why. */
struct FStealthParityAllow
{
	FString Root;
	FString Scope;
	FString Token;
	FString Reason;
};

/** Which scanner a forbidden token applies to. */
enum class EStealthParityDomain : uint8 { Any, Cpp, Blueprint };

/** The committed scan fixture: forbidden symbols, the owner-local allowlist and the scan roots. */
struct FStealthParityFixture
{
	/** Repo-relative path of the committed fixture. */
	static constexpr const TCHAR* ProjectRelativePath = TEXT("tests/fixtures/scl/class-parity-fixture.txt");

	struct FForbidden
	{
		FString Token;
		bool bPrefix = false;
		EStealthParityDomain Domain = EStealthParityDomain::Any;
	};

	TArray<FForbidden> Forbidden;
	TArray<FStealthParityAllow> Allow;
	/** Repo-relative directories whose .h/.cpp files are scanned. */
	TArray<FString> CppRoots;
	/** Blueprint object paths walked by the editor scanner. */
	TArray<FString> BlueprintRoots;
	/** Names of the local-control subset (story SCL-005's AnimBP check). */
	TArray<FString> LocalControlTokens;

	/**
	 * Parses the fixture text. Lines: "forbid <token>", "forbid-cpp <token>", "forbid-bp <token>",
	 * "forbid-prefix <prefix>", "local-control <token>" (also forbidden),
	 * "allow <root>|<scope>|<token>|<reason>", "root-cpp <dir>", "root-bp <object path>";
	 * '#' starts a comment line. Unknown or malformed lines are returned in OutErrors.
	 */
	static FStealthParityFixture Parse(const FString& Text, TArray<FString>& OutErrors)
	{
		FStealthParityFixture F;
		TArray<FString> Lines;
		Text.ParseIntoArrayLines(Lines, true);
		for (int32 Index = 0; Index < Lines.Num(); ++Index)
		{
			const FString Line = Lines[Index].TrimStartAndEnd();
			if (Line.IsEmpty() || Line.StartsWith(TEXT("#")))
			{
				continue;
			}
			FString Key, Value;
			if (!Line.Split(TEXT(" "), &Key, &Value))
			{
				OutErrors.Add(FString::Printf(TEXT("fixture line %d has no value: %s"), Index + 1, *Line));
				continue;
			}
			Value.TrimStartAndEndInline();
			if (Key == TEXT("forbid")) { F.Forbidden.Add({ Value, false, EStealthParityDomain::Any }); }
			else if (Key == TEXT("forbid-cpp")) { F.Forbidden.Add({ Value, false, EStealthParityDomain::Cpp }); }
			else if (Key == TEXT("forbid-bp")) { F.Forbidden.Add({ Value, false, EStealthParityDomain::Blueprint }); }
			else if (Key == TEXT("forbid-prefix")) { F.Forbidden.Add({ Value, true, EStealthParityDomain::Any }); }
			else if (Key == TEXT("local-control"))
			{
				F.Forbidden.Add({ Value, false, EStealthParityDomain::Any });
				F.LocalControlTokens.Add(Value);
			}
			else if (Key == TEXT("root-cpp")) { F.CppRoots.Add(Value); }
			else if (Key == TEXT("root-bp")) { F.BlueprintRoots.Add(Value); }
			else if (Key == TEXT("allow"))
			{
				TArray<FString> Fields;
				Value.ParseIntoArray(Fields, TEXT("|"), false);
				if (Fields.Num() != 4 || Fields[3].TrimStartAndEnd().IsEmpty())
				{
					OutErrors.Add(FString::Printf(TEXT("fixture line %d: allow needs root|scope|token|reason: %s"), Index + 1, *Line));
					continue;
				}
				F.Allow.Add({ Fields[0].TrimStartAndEnd(), Fields[1].TrimStartAndEnd(), Fields[2].TrimStartAndEnd(), Fields[3].TrimStartAndEnd() });
			}
			else
			{
				OutErrors.Add(FString::Printf(TEXT("fixture line %d: unknown key '%s'"), Index + 1, *Key));
			}
		}
		return F;
	}

	/** Loads and parses the committed fixture; false (with OutError) when unreadable, malformed or empty. */
	static bool LoadFromProject(FStealthParityFixture& Out, FString& OutError)
	{
		const FString Path = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), ProjectRelativePath));
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *Path))
		{
			OutError = FString::Printf(TEXT("NOT ASSESSED: fixture unreadable: %s"), *Path);
			return false;
		}
		TArray<FString> Errors;
		Out = Parse(Text, Errors);
		if (!Errors.IsEmpty())
		{
			OutError = FString::Printf(TEXT("NOT ASSESSED: fixture malformed: %s"), *FString::Join(Errors, TEXT("; ")));
			return false;
		}
		if (Out.Forbidden.IsEmpty())
		{
			OutError = TEXT("NOT ASSESSED: fixture lists no forbidden symbol");
			return false;
		}
		return true;
	}

	/** True when Token is forbidden for Domain. */
	bool IsForbidden(const FString& Token, EStealthParityDomain Domain) const
	{
		for (const FForbidden& Entry : Forbidden)
		{
			if (Entry.Domain != EStealthParityDomain::Any && Entry.Domain != Domain)
			{
				continue;
			}
			if (Entry.bPrefix ? Token.StartsWith(Entry.Token, ESearchCase::CaseSensitive) : Token.Equals(Entry.Token, ESearchCase::CaseSensitive))
			{
				return true;
			}
		}
		return false;
	}

	/** The allowlist entry covering Ref, or nullptr. */
	const FStealthParityAllow* FindAllow(const FStealthParityRef& Ref) const
	{
		return Allow.FindByPredicate([&Ref](const FStealthParityAllow& A)
		{
			return A.Root == Ref.Root && A.Scope == Ref.Scope && A.Token == Ref.Token;
		});
	}
};

/** What one or more scanners saw: the forbidden-symbol references and what could not be assessed. */
struct FStealthParityScanResult
{
	TArray<FStealthParityRef> Refs;
	/** Each entry names an input that could not be read or classified; any entry fails the verdict. */
	TArray<FString> NotAssessed;
	/** Inputs actually scanned (files or Blueprints), so an empty scope is visible. */
	TArray<FString> Scanned;
};

/** The judged outcome: hits outside the allowlist, allowed hits, unused allowlist entries. */
struct FStealthParityVerdict
{
	TArray<FStealthParityRef> Hits;
	TArray<FStealthParityRef> Allowed;
	TArray<FString> NotAssessed;
	/** Allowlist entries (for the judged roots) that matched nothing: stale entries fail the verdict. */
	TArray<FString> UnusedAllow;
	int32 ScannedCount = 0;

	/** Pass: something was scanned, no hit outside the allowlist, nothing unassessed, no stale entry. */
	bool Passed() const { return ScannedCount > 0 && Hits.IsEmpty() && NotAssessed.IsEmpty() && UnusedAllow.IsEmpty(); }

	/** Multi-line summary for test output. */
	FString Describe() const
	{
		FString Out = FString::Printf(TEXT("scanned %d, hits %d, allowed %d, not assessed %d, unused allowlist %d"),
			ScannedCount, Hits.Num(), Allowed.Num(), NotAssessed.Num(), UnusedAllow.Num());
		for (const FStealthParityRef& Hit : Hits) { Out += TEXT("\n  HIT ") + Hit.ToLine(); }
		for (const FString& Item : NotAssessed) { Out += TEXT("\n  NOT ASSESSED ") + Item; }
		for (const FString& Item : UnusedAllow) { Out += TEXT("\n  UNUSED ALLOW ") + Item; }
		return Out;
	}
};

namespace StealthClassParity
{
	/** Appends every C/C++-style identifier in Text to Out, in order. */
	inline void Tokenize(const FString& Text, TArray<FString>& Out)
	{
		const int32 Len = Text.Len();
		int32 Index = 0;
		while (Index < Len)
		{
			const TCHAR C = Text[Index];
			if (FChar::IsAlpha(C) || C == TEXT('_'))
			{
				const int32 Start = Index;
				while (Index < Len && (FChar::IsAlnum(Text[Index]) || Text[Index] == TEXT('_')))
				{
					++Index;
				}
				Out.Add(Text.Mid(Start, Index - Start));
			}
			else
			{
				++Index;
			}
		}
	}

	/** Replaces // and block comments with spaces, keeping newlines and string literals. */
	inline FString StripCppComments(const FString& Source)
	{
		FString Out;
		Out.Reserve(Source.Len());
		const int32 Len = Source.Len();
		bool bInString = false;
		TCHAR Quote = 0;
		for (int32 Index = 0; Index < Len; ++Index)
		{
			const TCHAR C = Source[Index];
			const TCHAR Next = Index + 1 < Len ? Source[Index + 1] : 0;
			if (bInString)
			{
				Out.AppendChar(C);
				if (C == TEXT('\\') && Next != 0) { Out.AppendChar(Next); ++Index; }
				else if (C == Quote) { bInString = false; }
				continue;
			}
			if (C == TEXT('"') || C == TEXT('\''))
			{
				bInString = true;
				Quote = C;
				Out.AppendChar(C);
			}
			else if (C == TEXT('/') && Next == TEXT('/'))
			{
				while (Index < Len && Source[Index] != TEXT('\n')) { ++Index; }
				if (Index < Len) { Out.AppendChar(TEXT('\n')); }
			}
			else if (C == TEXT('/') && Next == TEXT('*'))
			{
				Index += 2;
				while (Index < Len && !(Source[Index] == TEXT('*') && Index + 1 < Len && Source[Index + 1] == TEXT('/')))
				{
					Out.AppendChar(Source[Index] == TEXT('\n') ? TEXT('\n') : TEXT(' '));
					++Index;
				}
				++Index; // skip the closing '/'
			}
			else
			{
				Out.AppendChar(C);
			}
		}
		return Out;
	}

	/**
	 * Scans one C++ file for forbidden identifiers outside comments. Scope is the innermost enclosing
	 * function ("Class::Function" as written at its definition), the class for inline members, or
	 * "<file>" at file scope.
	 */
	inline void ScanCppSource(const FString& RelPath, const FString& Source, const FStealthParityFixture& Fixture, TArray<FStealthParityRef>& Out)
	{
		const FString Code = StripCppComments(Source);
		TArray<FString> ScopeStack;
		int32 SegmentStart = 0; // start of the text since the last statement or block boundary
		int32 Line = 1;
		const int32 Len = Code.Len();

		auto NameForBlock = [](const FString& Segment) -> FString
		{
			// The block's owner: the first "Name(" or "A::B(" (a function definition; control keywords
			// give "" so the enclosing function stays the scope), unless a later
			// "class|struct|namespace Name" is closer to the brace (e.g. after a UCLASS() macro).
			static const FRegexPattern FunctionPattern(TEXT("(~?[A-Za-z_][A-Za-z0-9_]*(?:[ \\t\\r\\n]*::[ \\t\\r\\n]*~?[A-Za-z_][A-Za-z0-9_]*)*)[ \\t\\r\\n]*\\("));
			static const FRegexPattern TypePattern(TEXT("\\b(?:class|struct|namespace)[ \\t\\r\\n]+(?:[A-Z0-9_]+_API[ \\t\\r\\n]+)?([A-Za-z_][A-Za-z0-9_]*)"));
			FString FunctionName;
			int32 FunctionAt = INDEX_NONE;
			FRegexMatcher FunctionMatch(FunctionPattern, Segment);
			if (FunctionMatch.FindNext())
			{
				for (const TCHAR Ch : FunctionMatch.GetCaptureGroup(1))
				{
					if (!FChar::IsWhitespace(Ch)) { FunctionName.AppendChar(Ch); }
				}
				FunctionAt = FunctionMatch.GetMatchBeginning();
			}
			FString TypeName;
			int32 TypeAt = INDEX_NONE;
			FRegexMatcher TypeMatch(TypePattern, Segment);
			while (TypeMatch.FindNext())
			{
				TypeName = TypeMatch.GetCaptureGroup(1);
				TypeAt = TypeMatch.GetMatchBeginning();
			}
			if (TypeAt != INDEX_NONE && TypeAt > FunctionAt)
			{
				return TypeName;
			}
			static const TSet<FString> ControlKeywords = { TEXT("if"), TEXT("for"), TEXT("while"), TEXT("switch"), TEXT("catch"), TEXT("return"), TEXT("sizeof"), TEXT("decltype") };
			return ControlKeywords.Contains(FunctionName) ? FString() : FunctionName;
		};

		auto CurrentScope = [&ScopeStack]() -> FString
		{
			for (int32 S = ScopeStack.Num() - 1; S >= 0; --S)
			{
				if (!ScopeStack[S].IsEmpty()) { return ScopeStack[S]; }
			}
			return TEXT("<file>");
		};

		int32 Index = 0;
		while (Index < Len)
		{
			const TCHAR C = Code[Index];
			if (C == TEXT('\n')) { ++Line; ++Index; continue; }
			if (C == TEXT('"') || C == TEXT('\''))
			{
				// String or char literal: tokens inside still count (e.g. FindFunctionByName("IsLocallyControlled")).
				const TCHAR Quote = C;
				const int32 Start = Index++;
				while (Index < Len && Code[Index] != Quote)
				{
					if (Code[Index] == TEXT('\\')) { ++Index; }
					else if (Code[Index] == TEXT('\n')) { ++Line; }
					++Index;
				}
				++Index;
				TArray<FString> Inner;
				Tokenize(Code.Mid(Start, Index - Start), Inner);
				for (const FString& Token : Inner)
				{
					if (Fixture.IsForbidden(Token, EStealthParityDomain::Cpp))
					{
						Out.Add({ RelPath, CurrentScope(), FString::Printf(TEXT("L%d"), Line), Token });
					}
				}
				continue;
			}
			if (C == TEXT('{'))
			{
				ScopeStack.Add(NameForBlock(Code.Mid(SegmentStart, Index - SegmentStart)));
				SegmentStart = ++Index;
				continue;
			}
			if (C == TEXT('}'))
			{
				if (!ScopeStack.IsEmpty()) { ScopeStack.Pop(); }
				SegmentStart = ++Index;
				continue;
			}
			if (C == TEXT(';'))
			{
				SegmentStart = ++Index;
				continue;
			}
			if (FChar::IsAlpha(C) || C == TEXT('_'))
			{
				const int32 Start = Index;
				while (Index < Len && (FChar::IsAlnum(Code[Index]) || Code[Index] == TEXT('_'))) { ++Index; }
				const FString Token = Code.Mid(Start, Index - Start);
				if (Fixture.IsForbidden(Token, EStealthParityDomain::Cpp))
				{
					Out.Add({ RelPath, CurrentScope(), FString::Printf(TEXT("L%d"), Line), Token });
				}
				continue;
			}
			++Index;
		}
	}

	/** Scans every .h/.cpp under the fixture's C++ roots; a missing root is NOT ASSESSED. */
	inline void ScanCppRoots(const FStealthParityFixture& Fixture, FStealthParityScanResult& Result)
	{
		for (const FString& Root : Fixture.CppRoots)
		{
			const FString Dir = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), Root));
			if (!FPaths::DirectoryExists(Dir))
			{
				Result.NotAssessed.Add(FString::Printf(TEXT("C++ root missing: %s"), *Root));
				continue;
			}
			TArray<FString> Files;
			IFileManager::Get().FindFilesRecursive(Files, *Dir, TEXT("*.h"), true, false);
			IFileManager::Get().FindFilesRecursive(Files, *Dir, TEXT("*.cpp"), true, false, false); // keep the .h list
			Files.Sort();
			if (Files.IsEmpty())
			{
				Result.NotAssessed.Add(FString::Printf(TEXT("C++ root has no source files: %s"), *Root));
			}
			for (const FString& File : Files)
			{
				FString Rel = File;
				FPaths::MakePathRelativeTo(Rel, *FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
				FString Source;
				if (!FFileHelper::LoadFileToString(Source, *File))
				{
					Result.NotAssessed.Add(FString::Printf(TEXT("unreadable: %s"), *Rel));
					continue;
				}
				Result.Scanned.Add(Rel);
				ScanCppSource(Rel, Source, Fixture, Result.Refs);
			}
		}
	}

	/**
	 * Judges a scan. Allowlist entries whose root is in RootsInScope and that matched nothing are
	 * reported as unused (a stale entry hides nothing today but would hide a new read tomorrow).
	 */
	inline FStealthParityVerdict Judge(const FStealthParityScanResult& Result, const FStealthParityFixture& Fixture, const TArray<FString>& RootsInScope)
	{
		FStealthParityVerdict Verdict;
		Verdict.ScannedCount = Result.Scanned.Num();
		Verdict.NotAssessed = Result.NotAssessed;
		TSet<int32> UsedAllow;
		for (const FStealthParityRef& Ref : Result.Refs)
		{
			if (const FStealthParityAllow* Allow = Fixture.FindAllow(Ref))
			{
				Verdict.Allowed.Add(Ref);
				UsedAllow.Add(static_cast<int32>(Allow - Fixture.Allow.GetData()));
			}
			else
			{
				Verdict.Hits.Add(Ref);
			}
		}
		for (int32 A = 0; A < Fixture.Allow.Num(); ++A)
		{
			const FStealthParityAllow& Entry = Fixture.Allow[A];
			const bool bInScope = RootsInScope.ContainsByPredicate([&Entry](const FString& Root) { return Entry.Root.StartsWith(Root); });
			if (bInScope && !UsedAllow.Contains(A))
			{
				Verdict.UnusedAllow.Add(FString::Printf(TEXT("%s|%s|%s"), *Entry.Root, *Entry.Scope, *Entry.Token));
			}
		}
		return Verdict;
	}

	/** Serialises references as a committed report (one ToLine per line, sorted, '#' header). */
	inline FString WriteReport(const FString& Header, TArray<FStealthParityRef> Refs)
	{
		Refs.Sort([](const FStealthParityRef& A, const FStealthParityRef& B) { return A.ToLine() < B.ToLine(); });
		FString Out = TEXT("# ") + Header + TEXT("\n# Root\tScope\tLocation\tToken\n");
		for (const FStealthParityRef& Ref : Refs)
		{
			Out += Ref.ToLine() + TEXT("\n");
		}
		return Out;
	}

	/** Parses WriteReport output; malformed lines go to OutErrors. */
	inline TArray<FStealthParityRef> ReadReport(const FString& Text, TArray<FString>& OutErrors)
	{
		TArray<FStealthParityRef> Refs;
		TArray<FString> Lines;
		Text.ParseIntoArrayLines(Lines, true);
		for (const FString& Line : Lines)
		{
			if (Line.StartsWith(TEXT("#"))) { continue; }
			FStealthParityRef Ref;
			if (FStealthParityRef::FromLine(Line, Ref)) { Refs.Add(Ref); }
			else { OutErrors.Add(Line); }
		}
		return Refs;
	}
}

#endif // WITH_STEALTH_TEST_SEAMS
