// Stories tdc-038 / tdc-041 / tdc-042 / tdc-043: the runtime schema table (ADR-0004 section 2).
// The rows come from the checked-in StealthTuningSchema.generated.inl, written by the editor
// generator (Stealth.Tuning.SchemaGen). Under STEALTH_TUNING_SCHEMA_BOOTSTRAP=1 (set only by
// tools/ci/regenerate-tuning-schema.ps1 through StealthFoundation.Build.cs) the table is empty, so the
// editor still builds after a tuning field was renamed and the generator can rewrite the file;
// SchemaComplete fails on an empty table, so a bootstrap build can never pass CI.

#include "Server/StealthTuningSchema.h"

#include "Server/StealthFnv1a64.h"

#if !(defined(STEALTH_TUNING_SCHEMA_BOOTSTRAP) && STEALTH_TUNING_SCHEMA_BOOTSTRAP)
#include "Tuning/StealthTuningAccess.h"
#include "Tuning/StealthTuningSchema.generated.inl"
#endif

TConstArrayView<FTuningSchemaRow> GetTuningSchema()
{
#if defined(STEALTH_TUNING_SCHEMA_BOOTSTRAP) && STEALTH_TUNING_SCHEMA_BOOTSTRAP
	return TConstArrayView<FTuningSchemaRow>();
#else
	return MakeArrayView(StealthTuningGenerated::Rows);
#endif
}

uint64 GetTuningSchemaFingerprint()
{
#if defined(STEALTH_TUNING_SCHEMA_BOOTSTRAP) && STEALTH_TUNING_SCHEMA_BOOTSTRAP
	return 0;
#else
	return StealthTuningGenerated::SchemaFingerprint;
#endif
}

bool IsTuningSchemaBootstrapBuild()
{
#if defined(STEALTH_TUNING_SCHEMA_BOOTSTRAP) && STEALTH_TUNING_SCHEMA_BOOTSTRAP
	return true;
#else
	return false;
#endif
}

uint64 ComputeTuningSchemaFingerprint(TConstArrayView<FTuningSchemaRow> Rows)
{
	TArray<const FTuningSchemaRow*> Sorted;
	Sorted.Reserve(Rows.Num());
	for (const FTuningSchemaRow& Row : Rows)
	{
		Sorted.Add(&Row);
	}
	Sorted.Sort([](const FTuningSchemaRow& A, const FTuningSchemaRow& B)
	{
		return FCStringAnsi::Strcmp(A.Key, B.Key) < 0;
	});

	uint64 State = StealthFnv1a64::OffsetBasis;
	for (const FTuningSchemaRow* Row : Sorted)
	{
		const int32 Len = FCStringAnsi::Strlen(Row->Key);
		const uint8 LenByte = static_cast<uint8>(Len);
		const uint8 Tail[5] = {
			Row->TypeTag,
			static_cast<uint8>(Row->bPublic ? 1 : 0),
			static_cast<uint8>(Row->Latch),
			0, 0, // u16le enum value count: no enum key exists yet (EnumValues column, tdc-038 open point)
		};
		StealthFnv1a64::Update(State, &LenByte, 1);
		StealthFnv1a64::Update(State, reinterpret_cast<const uint8*>(Row->Key), Len);
		StealthFnv1a64::Update(State, Tail, UE_ARRAY_COUNT(Tail));
	}
	return State;
}
