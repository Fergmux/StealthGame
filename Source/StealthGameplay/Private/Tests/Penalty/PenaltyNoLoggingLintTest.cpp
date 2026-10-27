// Story nkp-004 (production/epics/npc-kill-penalty-informants/story-004-event-log-rows.md), AC2 (derived, ADR-0019 section 7,
// PEN-R17, ADR-0007 / Round R19), lint half: a source scan of Source/StealthGameplay/Features/Penalty/** (comments and
// string contents excluded) finds no UE_LOG / UE_LOGF / UE_CLOG, no TRACE_* or CSV_* channel, no ClientMessage, no
// AddOnScreenDebugMessage and no Client RPC: nothing can carry a Penalty row, a field or the strike count off the server.
// The allowed-exception list is empty. Negative control (ADR-0007 verification): a seeded violating call is caught.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Server/Testing/StealthNegativeControl.h"
#include "Tests/Penalty/PenaltySourceScan.h"

namespace PenaltyNoLoggingLintTestData
{
	/** The forbidden identifier prefixes (an identifier that STARTS with one is a finding). */
	const TCHAR* const Forbidden[] = {
		TEXT("UE_LOG"), TEXT("UE_CLOG"), TEXT("TRACE_"), TEXT("CSV_"), TEXT("ClientMessage"), TEXT("AddOnScreenDebugMessage"),
	};

	/** The lint over one source: forbidden identifiers, plus a UFUNCTION declaring a Client RPC. */
	void Lint(const StealthPenaltySourceScan::FSource& Source, TArray<StealthPenaltySourceScan::FFinding>& Out)
	{
		using namespace StealthPenaltySourceScan;
		FindIdentifierPrefixes(Source, Forbidden, Out);
		TArray<FString> Lines;
		StripCommentsAndStrings(Source.Text).ParseIntoArrayLines(Lines, false);
		for (int32 Index = 0; Index < Lines.Num(); ++Index)
		{
			if (Lines[Index].Contains(TEXT("UFUNCTION"), ESearchCase::CaseSensitive) && Lines[Index].Contains(TEXT("Client"), ESearchCase::CaseSensitive))
			{
				Out.Add({ Source.Path, Index + 1, TEXT("Client RPC") });
			}
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPenaltyNoLoggingLintTest, "StealthGame.Penalty.NoLoggingLint",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPenaltyNoLoggingLintTest::RunTest(const FString& Parameters)
{
	using namespace StealthPenaltySourceScan;
	using namespace PenaltyNoLoggingLintTestData;

	const TArray<FSource> Sources = LoadSources();
	TestTrue(TEXT("the Penalty sources were found"), Sources.Num() >= 6);
	TArray<FFinding> Findings;
	for (const FSource& Source : Sources)
	{
		Lint(Source, Findings);
	}
	TestTrue(FString::Printf(TEXT("no log, trace, client message or client RPC in Penalty sources (%s)"), *Describe(Findings)), Findings.IsEmpty());

	FStealthNegativeControl Control(TEXT("SeededPenaltyLogCall"));
	FStealthNegativeControl::Report(*this, Control.Run([](FStealthCheckContext& Ctx)
	{
		TArray<FFinding> Seeded;
		Lint(FSource{ TEXT("Seeded.cpp"), TEXT("// UE_LOG in a comment is fine\nUE_LOGF(LogTemp, Log, \"%d\", Row.StrikeAfter);\n") }, Seeded);
		Ctx.TestTrue(TEXT("seeded source is clean"), Seeded.IsEmpty());
	}));
	return true;
}

#endif
