// Story tdc-043 (production/epics/tuning-data-config/story-043-schema-fingerprint-version-bump.md): the F-2
// SchemaVersion bump rule made mechanical (ADR-0004 section 2). The freshly generated schema fingerprint
// (reflection, not the checked-in table) must equal the TuningSchemaHistory entry of the current
// TuningSchemaVersion, and no two history entries may share a fingerprint. The policy check runs on the real
// header and on fixtures: (a) matching history passes; (b) a seeded field change (a key renamed in the
// reflected rows) with the version untouched fails; (c) the same change after a bump plus history append
// passes; (d) two versions sharing one fingerprint fail. (b) and (d) are negative controls. Known limit:
// removal of an old history entry is not detected (a G3-style shrink-only check would be needed).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Algo/Reverse.h"
#include "Server/StealthTuningSchema.h"
#include "Server/StealthTuningVersion.h"
#include "Server/Testing/StealthNegativeControl.h"
#include "Tuning/StealthTuningSchemaGen.h"

namespace SchemaVersionBumpedTestData
{
	/**
	 * The bump policy. True when History has exactly one entry for CurrentVersion, its fingerprint equals
	 * Fingerprint, versions are unique and ascending with CurrentVersion the newest, and no two entries
	 * share a fingerprint. Problems are appended to Ctx.
	 */
	bool CheckHistory(int32 CurrentVersion, TConstArrayView<FTuningSchemaHistoryEntry> History, uint64 Fingerprint, FStealthCheckContext& Ctx)
	{
		int32 CurrentEntries = 0;
		for (int32 Index = 0; Index < History.Num(); ++Index)
		{
			const FTuningSchemaHistoryEntry& Entry = History[Index];
			if (Entry.Version == CurrentVersion)
			{
				++CurrentEntries;
				Ctx.TestTrue(FString::Printf(TEXT("the generated fingerprint 0x%016llx must equal the history entry of version %d (0x%016llx); it differs, so a tuning field changed without bumping TuningSchemaVersion and appending history"),
					static_cast<unsigned long long>(Fingerprint), CurrentVersion, static_cast<unsigned long long>(Entry.Fingerprint)), Entry.Fingerprint == Fingerprint);
			}
			if (Index > 0)
			{
				Ctx.TestTrue(FString::Printf(TEXT("history versions ascend (%d after %d)"), Entry.Version, History[Index - 1].Version), Entry.Version > History[Index - 1].Version);
			}
			for (int32 Other = 0; Other < Index; ++Other)
			{
				Ctx.TestTrue(FString::Printf(TEXT("versions %d and %d share fingerprint 0x%016llx"), History[Other].Version, Entry.Version, static_cast<unsigned long long>(Entry.Fingerprint)),
					History[Other].Fingerprint != Entry.Fingerprint);
			}
		}
		Ctx.TestTrue(FString::Printf(TEXT("history holds exactly one entry for the current version %d"), CurrentVersion), CurrentEntries == 1);
		Ctx.TestTrue(TEXT("the current version is the newest history entry"), History.Num() > 0 && History.Last().Version == CurrentVersion);
		return !Ctx.HasFailed();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTuningSchemaVersionBumpedTest, "StealthGame.Tuning.SchemaVersionBumped",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTuningSchemaVersionBumpedTest::RunTest(const FString& Parameters)
{
	using namespace SchemaVersionBumpedTestData;
	using namespace StealthTuningSchemaGen;

	TArray<FGenRow> Rows;
	TArray<FString> Errors;
	if (!TestTrue(TEXT("the generator reflects the schema"), ReflectRows(Rows, Errors)))
	{
		for (const FString& Error : Errors)
		{
			AddError(Error);
		}
		return false;
	}
	const uint64 Generated = Fingerprint(Rows);
	const TConstArrayView<FTuningSchemaHistoryEntry> History = MakeArrayView(TuningSchemaHistory);

	// (a) The real header: the generated fingerprint is recorded for the current version.
	{
		FStealthCheckContext Ctx;
		CheckHistory(TuningSchemaVersion, History, Generated, Ctx);
		for (const FString& Failure : Ctx.GetFailures())
		{
			AddError(Failure);
		}
		TestEqual(TEXT("the compiled table's fingerprint equals the generated one (table regenerated)"), GetTuningSchemaFingerprint(), Generated);
		TestEqual(TEXT("the runtime fingerprint function agrees on the compiled rows"), ComputeTuningSchemaFingerprint(GetTuningSchema()), Generated);
	}

	// Seeded field change: rename one reflected key (a fixture copy, never the real structs).
	TArray<FGenRow> Changed = Rows;
	Changed[0].Key += TEXT("Renamed");
	const uint64 ChangedFingerprint = Fingerprint(Changed);
	TestNotEqual(TEXT("a renamed key changes the fingerprint"), ChangedFingerprint, Generated);
	{
		TArray<FGenRow> LatchChanged = Rows;
		LatchChanged[0].Latch = LatchChanged[0].Latch == ETuningLatch::Round ? ETuningLatch::Use : ETuningLatch::Round;
		TestNotEqual(TEXT("a latch change changes the fingerprint"), Fingerprint(LatchChanged), Generated);
		TArray<FGenRow> AudienceChanged = Rows;
		AudienceChanged[0].bPublic = !AudienceChanged[0].bPublic;
		TestNotEqual(TEXT("an audience change changes the fingerprint"), Fingerprint(AudienceChanged), Generated);
		TArray<FGenRow> TypeChanged = Rows;
		TypeChanged[0].TypeTag = TypeChanged[0].TypeTag == StealthTuningTypeTag::Int ? StealthTuningTypeTag::Float : StealthTuningTypeTag::Int;
		TestNotEqual(TEXT("a type change changes the fingerprint"), Fingerprint(TypeChanged), Generated);
		TArray<FGenRow> DefaultChanged = Rows;
		DefaultChanged[0].Default += 1.0;
		DefaultChanged[0].HardMax += 1.0;
		TestEqual(TEXT("a default or range change alone is outside the fingerprint (open confirmation, story tdc-043)"), Fingerprint(DefaultChanged), Generated);
		TArray<FGenRow> Reordered = Rows;
		Algo::Reverse(Reordered);
		TestEqual(TEXT("row order does not change the fingerprint"), Fingerprint(Reordered), Generated);
	}

	const FTuningSchemaHistoryEntry Version1 = { 1, Generated };
	const FTuningSchemaHistoryEntry Version2 = { 2, ChangedFingerprint };

	// (b) Negative control: the field changed, the version was not bumped.
	{
		FStealthNegativeControl Control(TEXT("FieldChangedWithoutVersionBump"));
		const FTuningSchemaHistoryEntry Unbumped[] = { Version1 };
		FStealthNegativeControl::Report(*this, Control.Run([&](FStealthCheckContext& Ctx) { CheckHistory(1, Unbumped, ChangedFingerprint, Ctx); }));
	}

	// (c) After the bump and history append: passes.
	{
		const FTuningSchemaHistoryEntry Bumped[] = { Version1, Version2 };
		FStealthCheckContext Ctx;
		TestTrue(TEXT("bumped version with appended history passes"), CheckHistory(2, Bumped, ChangedFingerprint, Ctx));
		// A bump without appending history still fails.
		FStealthNegativeControl Control(TEXT("VersionBumpedWithoutHistory"));
		const FTuningSchemaHistoryEntry NotAppended[] = { Version1 };
		FStealthNegativeControl::Report(*this, Control.Run([&](FStealthCheckContext& Inner) { CheckHistory(2, NotAppended, ChangedFingerprint, Inner); }));
	}

	// (d) Negative control: two versions share one fingerprint.
	{
		FStealthNegativeControl Control(TEXT("TwoVersionsShareFingerprint"));
		const FTuningSchemaHistoryEntry Duplicate[] = { Version1, { 2, Generated } };
		FStealthNegativeControl::Report(*this, Control.Run([&](FStealthCheckContext& Ctx) { CheckHistory(2, Duplicate, Generated, Ctx); }));
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
