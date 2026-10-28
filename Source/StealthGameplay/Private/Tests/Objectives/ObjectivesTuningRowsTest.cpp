// Story oc-001 (production/epics/objectives-chain/story-001-objectives-tuning-rows-and-invariants.md), AC4 (GDD Tuning Knobs,
// ADR-0020 section 8): the ten Objectives-owned keys exist as ADR-0004 rows equal to the constant fixture copied from the GDD
// table (Server/Testing/StealthObjectivesTuningKnobs.h, never derived from the rows under test): default, hard range, safe
// range, audience and latch. ObjectiveInteractSec resolves to the one TDC row (declared once, not redeclared). The owner
// "Objectives" has exactly eleven rows (ten plus the canonical one): an eleventh own key fails. The eight ServerOnly names
// are absent from the client tuning face and from every client-module source (StealthPresentation and every View/ folder);
// a seeded client source that names one fails the same scan. The seeded client INCLUDE of the server-face header is
// ObjectivesIncludeLint's (story oc-022).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "ObjectivesSourceScan.h"
#include "Server/StealthTuningSchema.h"
#include "Server/Testing/StealthObjectivesTuningKnobs.h"
#include "UObject/UnrealType.h"
#include "View/StealthClientTuning.h"

namespace ObjectivesTuningRowsTestData
{
	/** Owner rows: the ten keys plus the TDC canonical ObjectiveInteractSec. */
	constexpr int32 ExpectedOwnerRows = 11;

	bool HasClientProperty(const FString& Name)
	{
		for (TFieldIterator<FProperty> Owner(FStealthClientTuning::StaticStruct()); Owner; ++Owner)
		{
			const FStructProperty* Sub = CastField<FStructProperty>(*Owner);
			if (Sub != nullptr && Sub->Struct->FindPropertyByName(FName(*Name)) != nullptr)
			{
				return true;
			}
		}
		return false;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectivesTuningRowsTest, "StealthGame.Objectives.ObjectivesTuningRows",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FObjectivesTuningRowsTest::RunTest(const FString& Parameters)
{
	using namespace ObjectivesTuningRowsTestData;
	const TConstArrayView<FTuningSchemaRow> Schema = GetTuningSchema();

	// Arrange: the owner's rows of the compiled schema.
	TArray<const FTuningSchemaRow*> OwnerRows;
	for (const FTuningSchemaRow& Row : Schema)
	{
		if (Row.Owner != nullptr && FCStringAnsi::Strcmp(Row.Owner, StealthObjectivesTuningKnobs::Owner) == 0)
		{
			OwnerRows.Add(&Row);
		}
	}

	// Assert: exactly ten own keys plus the canonical row.
	TestEqual(TEXT("owner Objectives has the ten keys plus ObjectiveInteractSec (an eleventh own key fails)"), OwnerRows.Num(), ExpectedOwnerRows);
	int32 Canonical = 0;
	for (const FTuningSchemaRow* Row : OwnerRows)
	{
		Canonical += FCStringAnsi::Strcmp(Row->Key, StealthObjectivesTuningKnobs::CanonicalKey) == 0 ? 1 : 0;
	}
	TestEqual(TEXT("ObjectiveInteractSec is one row (the TDC canonical row, not redeclared)"), Canonical, 1);

	for (const StealthObjectivesTuningKnobs::FKnob& Knob : StealthObjectivesTuningKnobs::Knobs)
	{
		const FString Key(Knob.Key);
		int32 Found = 0;
		for (const FTuningSchemaRow& Row : Schema)
		{
			Found += FCStringAnsi::Strcmp(Row.Key, Knob.Key) == 0 ? 1 : 0;
		}
		TestEqual(FString::Printf(TEXT("%s is declared exactly once"), *Key), Found, 1);
		const FTuningSchemaRow* const* RowPtr = OwnerRows.FindByPredicate([&Knob](const FTuningSchemaRow* R) { return FCStringAnsi::Strcmp(R->Key, Knob.Key) == 0; });
		if (!TestNotNull(FString::Printf(TEXT("%s is an Objectives row"), *Key), RowPtr))
		{
			continue;
		}
		const FTuningSchemaRow& Row = **RowPtr;
		TestEqual(FString::Printf(TEXT("%s default"), *Key), Row.Default, static_cast<double>(Knob.Default), 1e-6);
		TestEqual(FString::Printf(TEXT("%s hard min"), *Key), Row.HardMin, static_cast<double>(Knob.HardMin), 1e-6);
		TestEqual(FString::Printf(TEXT("%s hard max"), *Key), Row.HardMax, static_cast<double>(Knob.HardMax), 1e-6);
		TestEqual(FString::Printf(TEXT("%s safe min"), *Key), Row.SafeMin, static_cast<double>(Knob.SafeMin), 1e-6);
		TestEqual(FString::Printf(TEXT("%s safe max"), *Key), Row.SafeMax, static_cast<double>(Knob.SafeMax), 1e-6);
		TestEqual(FString::Printf(TEXT("%s audience (All = public)"), *Key), Row.bPublic, Knob.bPublic);
		TestEqual(FString::Printf(TEXT("%s latch"), *Key), static_cast<int32>(Row.Latch), static_cast<int32>(Knob.Latch));
		TestEqual(FString::Printf(TEXT("%s on the client face iff All"), *Key), HasClientProperty(Key), Knob.bPublic);
	}

	// The eight ServerOnly names never appear in client-module code.
	TArray<FString> ServerNames;
	for (const ANSICHAR* Name : StealthObjectivesTuningKnobs::ServerFaceKeys)
	{
		ServerNames.Add(FString(Name));
	}
	TestEqual(TEXT("eight ServerOnly keys"), ServerNames.Num(), 8);
	const TArray<StealthObjectivesSourceScan::FSource> Client = StealthObjectivesSourceScan::LoadClientSources();
	TestTrue(TEXT("the client-module scan found sources"), Client.Num() > 10);
	const TArray<FString> Findings = StealthObjectivesSourceScan::FindNames(Client, ServerNames);
	TestEqual(TEXT("no client-module source names a ServerOnly Objectives key"), Findings.Num(), 0);
	for (const FString& Finding : Findings)
	{
		AddError(Finding);
	}

	// Negative control: a seeded client source naming a server-face key is found.
	StealthObjectivesSourceScan::FSource Seeded;
	Seeded.Path = TEXT("StealthPresentation/Private/Seeded.cpp");
	Seeded.Text = TEXT("// bRivalSignalEnabled in a comment is fine\nconst bool b = Tuning.Objectives_Server.bRivalSignalEnabled;\n");
	TestEqual(TEXT("seeded: a client source reading bRivalSignalEnabled is reported once"), StealthObjectivesSourceScan::FindNames({ Seeded }, ServerNames).Num(), 1);
	return true;
}

#endif
