// Story tdc-041 (production/epics/tuning-data-config/story-041-schema-generator-commandlet.md): the schema
// generator (console command Stealth.Tuning.SchemaGen; StealthGameEditor loads after commandlet lookup, so
// it is a console command, not a commandlet) produces byte-identical output on an unchanged tree, equal to
// the checked-in Source/StealthFoundation/Private/Tuning/StealthTuningSchema.generated.inl; the rows are
// constexpr with member-pointer accessors (the text never names offsetof); the order is byte-wise key order
// regardless of declaration order; LF line endings, no BOM, no absolute path. The test never writes the
// checked-in file. Editor test: it reads UPROPERTY metadata (WITH_METADATA), ADR-0004 section 2.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Server/StealthTuningSchema.h"
#include "Tuning/StealthTuningSchemaGen.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTuningSchemaGenDeterministicTest, "StealthGame.Tuning.SchemaGenDeterministic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTuningSchemaGenDeterministicTest::RunTest(const FString& Parameters)
{
	using namespace StealthTuningSchemaGen;

	// Two runs of the shared generator.
	FString First;
	FString Second;
	TArray<FString> Errors;
	const bool bFirst = Generate(First, Errors);
	const bool bSecond = Generate(Second, Errors);
	for (const FString& Error : Errors)
	{
		AddError(Error);
	}
	if (!TestTrue(TEXT("the generator succeeds twice"), bFirst && bSecond))
	{
		return false;
	}
	TestTrue(TEXT("two runs are byte-identical"), First.Equals(Second, ESearchCase::CaseSensitive));

	// Equal to the checked-in table, byte for byte (raw bytes: no BOM, LF only).
	TArray<uint8> FileBytes;
	if (TestTrue(TEXT("the checked-in table exists"), FFileHelper::LoadFileToArray(FileBytes, *CheckedInTablePath())))
	{
		const FTCHARToUTF8 Generated(*First);
		const bool bSameBytes = FileBytes.Num() == Generated.Length() && FMemory::Memcmp(FileBytes.GetData(), Generated.Get(), Generated.Length()) == 0;
		TestTrue(TEXT("the checked-in table equals the generator output byte for byte (regenerate with Stealth.Tuning.SchemaGen)"), bSameBytes);
		TestFalse(TEXT("the checked-in table has no UTF-8 BOM"), FileBytes.Num() >= 3 && FileBytes[0] == 0xEF && FileBytes[1] == 0xBB && FileBytes[2] == 0xBF);
		TestFalse(TEXT("the checked-in table has no CR"), FileBytes.Contains(static_cast<uint8>('\r')));
	}

	// Text properties: constexpr rows, member-pointer accessors, never offsetof, no absolute path.
	TestTrue(TEXT("rows are constexpr"), First.Contains(TEXT("inline constexpr FTuningSchemaRow Rows[]")));
	TestTrue(TEXT("accessors go through pointers-to-member"), First.Contains(TEXT("&Get<&FStealthServerTuning::")) && First.Contains(TEXT("&Set<&FStealthServerTuning::")));
	TestFalse(TEXT("the output never names offsetof"), First.Contains(TEXT("offsetof")));
	TestFalse(TEXT("the output has no CR"), First.Contains(TEXT("\r")));
	TestFalse(TEXT("the output holds no absolute project path"), First.Contains(FPaths::ConvertRelativePathToFull(FPaths::ProjectDir())));
	TestTrue(TEXT("the fingerprint is emitted after the table"), First.Contains(TEXT("inline constexpr uint64 SchemaFingerprint = 0x")));

	// Order: byte-wise key order, independent of declaration order: ReflectRows sorts by key, so reordering
	// struct members cannot change the output (the declaration order of the eight keys is not sorted).
	TArray<FGenRow> Rows;
	TArray<FString> RowErrors;
	if (TestTrue(TEXT("reflection succeeds"), ReflectRows(Rows, RowErrors)))
	{
		TestTrue(TEXT("at least the eight canonical keys"), Rows.Num() >= 8);
		for (int32 Index = 1; Index < Rows.Num(); ++Index)
		{
			TestTrue(FString::Printf(TEXT("row %d (%s) sorts after %s"), Index, *Rows[Index].Key, *Rows[Index - 1].Key),
				Rows[Index - 1].Key.Compare(Rows[Index].Key, ESearchCase::CaseSensitive) < 0);
		}
		int32 Previous = -1;
		for (const FGenRow& Row : Rows)
		{
			const int32 At = First.Find(FString::Printf(TEXT("{ \"%s\","), *Row.Key), ESearchCase::CaseSensitive);
			TestTrue(FString::Printf(TEXT("%s is rendered in row order"), *Row.Key), At > Previous);
			Previous = At;
		}
		// The compiled table (this build) carries the same keys in the same order.
		const TConstArrayView<FTuningSchemaRow> Compiled = GetTuningSchema();
		if (TestEqual(TEXT("the compiled table has the reflected row count"), Compiled.Num(), Rows.Num()))
		{
			for (int32 Index = 0; Index < Rows.Num(); ++Index)
			{
				TestEqual(FString::Printf(TEXT("compiled row %d key"), Index), FString(Compiled[Index].Key), Rows[Index].Key);
				TestEqual(FString::Printf(TEXT("compiled row %d default"), Index), Compiled[Index].Default, Rows[Index].Default);
			}
		}
	}

	// The table path is resolved from FPaths::ProjectDir(), never the process working directory (which the
	// editor does not guarantee), so the command gives the same result from any directory it is run in.
	{
		const FString ProjectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
		const FString TablePath = CheckedInTablePath();
		TestTrue(TEXT("the table path is absolute"), !FPaths::IsRelative(TablePath));
		TestTrue(TEXT("the table path is under the project directory"), TablePath.StartsWith(ProjectDir));
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
