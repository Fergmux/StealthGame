#pragma once

// The tuning schema generator (ADR-0004 section 2; stories tdc-041, tdc-043). Reflects the tuning
// sub-structs of FStealthServerTuning and renders the checked-in table
// Source/StealthFoundation/Private/Tuning/StealthTuningSchema.generated.inl. One shared function
// produces the text for the console command, the tests and (story tdc-044) the in-memory drift check,
// so there is no second source of truth. Editor-only: UPROPERTY metadata exists only with
// WITH_METADATA (= WITH_EDITORONLY_DATA).
//
// Console commands (StealthGameEditor loads at PostEngineInit, after commandlet lookup, so these are
// console commands, not a commandlet):
//   Stealth.Tuning.SchemaGen     writes the table when it differs; summary line
//                                "STEALTH TUNING SCHEMA GEN: WROTE|UNCHANGED|FAIL ..."
//   Stealth.Tuning.SchemaVerify  writes nothing; PASS only when this build is not a bootstrap build,
//                                the compiled table is non-empty and equals the reflection, and the
//                                checked-in file equals the generated text; summary line
//                                "STEALTH TUNING SCHEMA VERIFY: PASS|FAIL ..."
// Headless: UnrealEditor-Cmd StealthGame.uproject -ExecCmds="Stealth.Tuning.SchemaGen,QUIT_EDITOR"
//           -unattended -nullrhi -stdout

#include "CoreMinimal.h"
#include "Server/StealthTuningSchema.h"

namespace StealthTuningSchemaGen
{
	/** One reflected key, before rendering. */
	struct FGenRow
	{
		FString Key;
		uint8 TypeTag = StealthTuningTypeTag::Float;
		/** Member of FStealthServerTuning holding the key's sub-struct, for example Round_Public. */
		FString OwnerMember;
		/** C++ name of that sub-struct, for example FRoundTuning_Public. */
		FString OwnerStruct;
		double Default = 0.0;
		double HardMin = 0.0;
		double HardMax = 0.0;
		double SafeMin = 0.0;
		double SafeMax = 0.0;
		bool bPublic = false;
		ETuningLatch Latch = ETuningLatch::Round;
		FString Owner;
		FString Unit;
		bool bHasSentinel = false;
		double Sentinel = 0.0;
	};

	/** Reflects every key of FStealthServerTuning's sub-structs, sorted by key bytes. False (with reasons) on any declaration error. */
	bool ReflectRows(TArray<FGenRow>& OutRows, TArray<FString>& OutErrors);

	/** Schema fingerprint of reflected rows (the same function the runtime uses, ComputeTuningSchemaFingerprint). */
	uint64 Fingerprint(const TArray<FGenRow>& Rows);

	/** Renders the complete .inl text: LF line endings, no timestamp, no path, fixed number format. */
	FString RenderTable(const TArray<FGenRow>& Rows);

	/** ReflectRows + RenderTable. False (with reasons) when the declarations are invalid. */
	bool Generate(FString& OutText, TArray<FString>& OutErrors);

	/** Absolute path of the checked-in table, from FPaths::ProjectDir() (never the working directory). */
	FString CheckedInTablePath();

	/** Loads the checked-in table as UTF-8 text. False when missing. */
	bool LoadCheckedInTable(FString& OutText);
}
