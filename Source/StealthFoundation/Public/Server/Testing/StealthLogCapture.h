#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"

#if WITH_STEALTH_TEST_SEAMS

#include "Containers/StringView.h"
#include "HAL/CriticalSection.h"
#include "Misc/AutomationTest.h"
#include "Misc/OutputDevice.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/ScopeLock.h"
#include "String/Find.h"

#include <atomic>

/**
 * Test-only log capture (ADR-0008 section 4, story tdc-009). Compiled only under
 * WITH_STEALTH_TEST_SEAMS.
 *
 * An RAII scope: the constructor registers the capture with GLog, the destructor removes it, so
 * lines logged after the scope ends are not recorded. Only Warning lines of one category
 * (LogStealthFoundation by default) are kept. Nested scopes each record independently.
 *
 * Usage in a test:
 *   FStealthLogCapture Capture;
 *   ... act (the system under test logs "... NpcCount ... 3 ... 2 ...") ...
 *   const FStealthLogMatchResult R = Capture.ExpectExactlyOneWarning(TEXT("NpcCount"), TEXT("3"), TEXT("2"));
 *   TestTrue(R.Describe(), R.Passed());
 *
 * Matching: a warning matches when its message contains Key, Authored and Effective each as a
 * whole token (a query for NpcCount does not match NpcCountMax; a query for 3 does not match 13
 * or 3.5). The rendered format of authored and effective values in the real validation warnings
 * is owned by story tdc-046; until then tests match on token occurrences only.
 */

/** Outcome of an "exactly one warning" query, returned so a self-test can inspect failures. */
struct FStealthLogMatchResult
{
	/** The query, for messages. */
	FString Key;
	FString Authored;
	FString Effective;
	/** Every captured warning that matched, in capture order. */
	TArray<FString> Matches;

	/** True when exactly one captured warning matched. */
	bool Passed() const { return Matches.Num() == 1; }

	/** One-line description for a test message. */
	FString Describe() const
	{
		return FString::Printf(TEXT("expected exactly one LogStealthFoundation warning naming key '%s' authored '%s' effective '%s'; found %d"),
			*Key, *Authored, *Effective, Matches.Num());
	}
};

/** Records the Warning lines of one log category while it is in scope. */
class FStealthLogCapture : public FOutputDevice
{
public:
	/** Starts capturing Warning lines of Category (LogStealthFoundation by default). */
	explicit FStealthLogCapture(FName InCategory = FName(TEXT("LogStealthFoundation")))
		: Category(InCategory)
	{
		// GLog may replay its backlog into a newly added device; nothing logged before the scope
		// counts, so the device is armed only after registration returns.
		GLog->AddOutputDevice(this);
		bArmed = true;
	}

	virtual ~FStealthLogCapture() override
	{
		Stop();
	}

	FStealthLogCapture(const FStealthLogCapture&) = delete;
	FStealthLogCapture& operator=(const FStealthLogCapture&) = delete;

	/** Stops capturing early (the destructor does the same). Captured lines stay readable. */
	void Stop()
	{
		if (bArmed)
		{
			bArmed = false;
			GLog->RemoveOutputDevice(this);
		}
	}

	virtual void Serialize(const TCHAR* Message, ELogVerbosity::Type Verbosity, const FName& InCategory) override
	{
		if (!bArmed || Message == nullptr || InCategory != Category
			|| (Verbosity & ELogVerbosity::VerbosityMask) != ELogVerbosity::Warning)
		{
			return;
		}
		FScopeLock Lock(&LinesLock);
		Lines.Emplace(Message);
	}

	// Lines may arrive from any thread; Serialize locks, so GLog may call it directly.
	virtual bool CanBeUsedOnAnyThread() const override { return true; }
	virtual bool CanBeUsedOnMultipleThreads() const override { return true; }

	/** Every captured warning message, in capture order. */
	TArray<FString> GetWarnings() const
	{
		FScopeLock Lock(&LinesLock);
		return Lines;
	}

	/** Captured warnings that name Key, Authored and Effective, each as a whole token. */
	TArray<FString> FindWarnings(FStringView Key, FStringView Authored, FStringView Effective) const
	{
		return GetWarnings().FilterByPredicate([Key, Authored, Effective](const FString& Line)
		{
			return ContainsToken(Line, Key) && ContainsToken(Line, Authored) && ContainsToken(Line, Effective);
		});
	}

	/** Number of captured warnings naming Key with Authored and Effective. */
	int32 CountWarnings(FStringView Key, FStringView Authored, FStringView Effective) const
	{
		return FindWarnings(Key, Authored, Effective).Num();
	}

	/** "Exactly one warning naming Key with Authored and Effective"; inspect Passed() on the result. */
	FStealthLogMatchResult ExpectExactlyOneWarning(FStringView Key, FStringView Authored, FStringView Effective) const
	{
		FStealthLogMatchResult Result;
		Result.Key = FString(Key);
		Result.Authored = FString(Authored);
		Result.Effective = FString(Effective);
		Result.Matches = FindWarnings(Key, Authored, Effective);
		return Result;
	}

	/**
	 * True when Token occurs in Line as a whole token: the characters around it are not letters,
	 * digits or '_', a '.' is a boundary unless a digit follows it (so 3 matches "3." at the end of
	 * a sentence but not "3.5"), and a numeric token preceded by '-' or a digit-'.' is not matched.
	 */
	static bool ContainsToken(FStringView Line, FStringView Token)
	{
		if (Token.IsEmpty())
		{
			return false;
		}
		const auto IsWordChar = [](TCHAR C) { return FChar::IsAlnum(C) || C == TEXT('_'); };
		const bool bNumericToken = FChar::IsDigit(Token[0]) || Token[0] == TEXT('.');
		int32 From = 0;
		while (From + Token.Len() <= Line.Len())
		{
			const int32 At = UE::String::FindFirst(Line.RightChop(From), Token, ESearchCase::CaseSensitive);
			if (At == INDEX_NONE)
			{
				return false;
			}
			const int32 Start = From + At;
			const int32 End = Start + Token.Len();
			bool bBoundaryBefore = Start == 0 || !IsWordChar(Line[Start - 1]);
			if (bBoundaryBefore && Start > 0 && bNumericToken)
			{
				const TCHAR Before = Line[Start - 1];
				bBoundaryBefore = Before != TEXT('-') && !(Before == TEXT('.') && Start > 1 && FChar::IsDigit(Line[Start - 2]));
			}
			bool bBoundaryAfter = End == Line.Len() || !IsWordChar(Line[End]);
			if (bBoundaryAfter && End < Line.Len() && Line[End] == TEXT('.'))
			{
				bBoundaryAfter = !(End + 1 < Line.Len() && FChar::IsDigit(Line[End + 1]));
			}
			if (bBoundaryBefore && bBoundaryAfter)
			{
				return true;
			}
			From = Start + 1;
		}
		return false;
	}

private:
	const FName Category;
	std::atomic<bool> bArmed = false;
	mutable FCriticalSection LinesLock;
	TArray<FString> Lines;
};

/**
 * Base class for automation tests that assert warnings with FStealthLogCapture. Declare the test
 * with IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FMyTest, FStealthLogCaptureTestBase, "...", Flags).
 *
 * Why it exists: a warning a test declares with AddExpectedMessage is demoted to Verbose by the
 * automation framework's GWarn filter before any log device sees it, so a capture would never
 * record it. Tests that use the capture therefore do not declare expected messages; instead this
 * base class stops the framework from recording log warnings as test warnings, and the capture's
 * own assertions are the check. Log errors still fail the test.
 */
class FStealthLogCaptureTestBase : public FAutomationTestBase
{
public:
	FStealthLogCaptureTestBase(const FString& InName, const bool bInComplexTask)
		: FAutomationTestBase(InName, bInComplexTask)
	{
	}

	virtual bool SuppressLogWarnings() override { return true; }
};

#endif // WITH_STEALTH_TEST_SEAMS
