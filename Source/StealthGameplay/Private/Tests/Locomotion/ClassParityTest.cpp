// Story SCL-004 (production/epics/shared-character-locomotion/story-004-class-parity-scan.md): the C++
// half of the class parity scan (AC-SCL-01; ADR-0001 section 1, ADR-0010 "Kind-blind code",
// ADR-0013 section 1). Scans the Shared Pawn, movement and AnimInstance sources named by the
// committed fixture (tests/fixtures/scl/class-parity-fixture.txt) for the forbidden symbols, outside
// comments, with the R9 owner-local allowlist named by file and function. The Blueprint half is
// StealthGame.Locomotion.ClassParity.Blueprint in StealthGameEditor.
//
// AC-SCL-01 clause "every SCL tuning key has exactly one value (no per-kind variant)" needs the SCL
// tuning schema (story SCL-003, TDC Exit B), which does not exist yet: it is reported here as NOT
// ASSESSED (carryover), never as passing.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Server/Testing/StealthClassParityScan.h"
#include "Server/Testing/StealthNegativeControl.h"

namespace ClassParityTestData
{
	/** Names the fixture must forbid (ADR-0013 section 1 plus IsBotControlled, the SCL-004 QA clarification). */
	const TCHAR* const RequiredForbidden[] = {
		TEXT("IsLocallyControlled"), TEXT("IsPlayerControlled"), TEXT("IsBotControlled"), TEXT("GetController"), TEXT("GetPlayerState"), TEXT("PlayerState")
	};

	/** A seeded violation: an observable branch on local control inside a pawn function. */
	const TCHAR* const SeededSource =
		TEXT("// IsLocallyControlled in a comment is not a read\n")
		TEXT("float AStealthPawn::GetGaitScale() const\n")
		TEXT("{\n")
		TEXT("    return IsLocallyControlled() ? 1.0f : 0.5f;\n")
		TEXT("}\n");

	/** Scope tracking: qualified function names, inline members and file scope. */
	const TCHAR* const ScopeSource =
		TEXT("UCLASS()\n")
		TEXT("class STEALTHGAMEPLAY_API AProbe : public ACharacter\n")
		TEXT("{\n")
		TEXT("    APlayerState* Cached;\n")
		TEXT("};\n")
		TEXT("void AProbe::Tick(float Dt)\n")
		TEXT("{\n")
		TEXT("    if (Dt > 0.f) { GetController(); }\n")
		TEXT("}\n");
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLocomotionClassParitySourceTest, "StealthGame.Locomotion.ClassParity.Source",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FLocomotionClassParitySourceTest::RunTest(const FString& Parameters)
{
	using namespace ClassParityTestData;

	// Arrange: the committed fixture, checked for integrity first.
	FStealthParityFixture Fixture;
	FString Error;
	if (!FStealthParityFixture::LoadFromProject(Fixture, Error))
	{
		AddError(Error);
		return false;
	}
	for (const TCHAR* Name : RequiredForbidden)
	{
		TestTrue(FString::Printf(TEXT("Fixture forbids %s"), Name), Fixture.IsForbidden(Name, EStealthParityDomain::Cpp));
	}
	TestTrue(TEXT("Fixture names C++ roots"), !Fixture.CppRoots.IsEmpty());

	// The scanner: scope tracking and comment stripping, on fixed snippets.
	{
		TArray<FStealthParityRef> Refs;
		StealthClassParity::ScanCppSource(TEXT("Probe.h"), ScopeSource, Fixture, Refs);
		TestTrue(TEXT("Scanner: class member type read is scoped to the class"),
			Refs.ContainsByPredicate([](const FStealthParityRef& R) { return R.Token == TEXT("APlayerState") && R.Scope == TEXT("AProbe") && R.Location == TEXT("L4"); }));
		TestTrue(TEXT("Scanner: call inside an if block is scoped to the enclosing function"),
			Refs.ContainsByPredicate([](const FStealthParityRef& R) { return R.Token == TEXT("GetController") && R.Scope == TEXT("AProbe::Tick") && R.Location == TEXT("L8"); }));

		TArray<FStealthParityRef> CommentOnly;
		StealthClassParity::ScanCppSource(TEXT("Comment.cpp"), TEXT("// GetController\n/* IsPlayerControlled */\nvoid F() {}\n"), Fixture, CommentOnly);
		TestEqual(TEXT("Scanner: names inside comments are not reads"), CommentOnly.Num(), 0);
	}

	// Negative control: a seeded local-control branch must fail the same judge.
	const FStealthNegativeControl Control(TEXT("SeededCppLocalControlBranch"));
	Control.RunAndReport(*this, [&](FStealthCheckContext& Ctx)
	{
		FStealthParityScanResult Seeded;
		Seeded.Scanned.Add(TEXT("Seeded.cpp"));
		StealthClassParity::ScanCppSource(TEXT("Seeded.cpp"), SeededSource, Fixture, Seeded.Refs);
		const FStealthParityVerdict Verdict = StealthClassParity::Judge(Seeded, Fixture, { TEXT("Seeded.cpp") });
		Ctx.TestTrue(FString::Printf(TEXT("seeded source passes (%s)"), *Verdict.Describe()), Verdict.Passed());
	});

	// Act: scan the real roots.
	FStealthParityScanResult Result;
	StealthClassParity::ScanCppRoots(Fixture, Result);
	const FStealthParityVerdict Verdict = StealthClassParity::Judge(Result, Fixture, Fixture.CppRoots);

	// Assert: zero forbidden reads outside the allowlist; every root read; no stale allowlist entry.
	AddInfo(FString::Printf(TEXT("C++ files scanned: %s"), *FString::Join(Result.Scanned, TEXT(", "))));
	TestTrue(FString::Printf(TEXT("AC-SCL-01 C++ scan: no forbidden read outside the R9 allowlist (%s)"), *Verdict.Describe()), Verdict.Passed());

	// AC-SCL-01 second clause: carried over, explicitly.
	AddInfo(TEXT("NOT ASSESSED (carryover): AC-SCL-01 'every SCL tuning key has exactly one value' needs the SCL tuning schema from story SCL-003 (TDC Exit B); this test does not claim it."));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
