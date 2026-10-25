// Stories tdc-041 / tdc-043: the tuning schema generator and its console commands (see the header).

#include "Tuning/StealthTuningSchemaGen.h"

#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Server/StealthServerTuning.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogStealthTuningSchemaGen, Log, All);

namespace StealthTuningSchemaGen
{
	namespace
	{
		const TCHAR* const PublicSuffix = TEXT("_Public");
		const TCHAR* const ServerSuffix = TEXT("_Server");

		/** Strict number parse of a metadata value. */
		bool ParseNumber(const FString& Text, double& OutValue)
		{
			return !Text.IsEmpty() && LexTryParseString(OutValue, *Text);
		}

		bool ParseLatch(const FString& Text, ETuningLatch& OutLatch)
		{
			static const TPair<const TCHAR*, ETuningLatch> Names[] = {
				{ TEXT("Round"), ETuningLatch::Round },
				{ TEXT("Action"), ETuningLatch::Action },
				{ TEXT("Use"), ETuningLatch::Use },
				{ TEXT("Session"), ETuningLatch::Session },
			};
			for (const TPair<const TCHAR*, ETuningLatch>& Name : Names)
			{
				if (Text.Equals(Name.Key, ESearchCase::CaseSensitive))
				{
					OutLatch = Name.Value;
					return true;
				}
			}
			return false;
		}

		const TCHAR* LatchName(ETuningLatch Latch)
		{
			switch (Latch)
			{
			case ETuningLatch::Round: return TEXT("Round");
			case ETuningLatch::Action: return TEXT("Action");
			case ETuningLatch::Use: return TEXT("Use");
			case ETuningLatch::Session: return TEXT("Session");
			}
			return TEXT("Round");
		}

		const TCHAR* TypeTagName(uint8 TypeTag)
		{
			switch (TypeTag)
			{
			case StealthTuningTypeTag::Bool: return TEXT("Bool");
			case StealthTuningTypeTag::Int: return TEXT("Int");
			case StealthTuningTypeTag::Float: return TEXT("Float");
			case StealthTuningTypeTag::Enum: return TEXT("Enum");
			case StealthTuningTypeTag::SoftRef: return TEXT("SoftRef");
			default: return TEXT("Float");
			}
		}

		/**
		 * Fixed-format literal of a value of the row's type: ints and bools as "N.0", floats as the
		 * shortest fixed-point decimal that round-trips through float plus "f" (so the double field
		 * holds the float value exactly), never locale- or platform-dependent exponent output.
		 */
		FString FormatValue(uint8 TypeTag, double Value)
		{
			if (TypeTag != StealthTuningTypeTag::Float)
			{
				return FString::Printf(TEXT("%lld.0"), static_cast<long long>(Value));
			}
			const float AsFloat = static_cast<float>(Value);
			double Scale = 1.0;
			for (int32 Decimals = 0; Decimals <= 9; ++Decimals, Scale *= 10.0)
			{
				const double Rounded = FMath::RoundToDouble(static_cast<double>(AsFloat) * Scale) / Scale;
				if (static_cast<float>(Rounded) != AsFloat)
				{
					continue;
				}
				FString Text = FString::Printf(TEXT("%.12f"), Rounded);
				while (Text.EndsWith(TEXT("0")))
				{
					Text.LeftChopInline(1);
				}
				if (Text.EndsWith(TEXT(".")))
				{
					Text += TEXT("0");
				}
				return Text + TEXT("f");
			}
			return FString::Printf(TEXT("%.12ef"), static_cast<double>(AsFloat));
		}

		/** Reads the value of one key from a default-constructed server face (C++ initialiser = schema default). */
		bool ReadDefault(const FProperty* Key, const void* KeyContainer, uint8& OutTypeTag, double& OutValue, FString& OutError)
		{
			if (const FBoolProperty* Bool = CastField<FBoolProperty>(Key))
			{
				OutTypeTag = StealthTuningTypeTag::Bool;
				OutValue = Bool->GetPropertyValue_InContainer(KeyContainer) ? 1.0 : 0.0;
				return true;
			}
			if (const FIntProperty* Int = CastField<FIntProperty>(Key))
			{
				OutTypeTag = StealthTuningTypeTag::Int;
				OutValue = static_cast<double>(Int->GetPropertyValue_InContainer(KeyContainer));
				return true;
			}
			if (const FFloatProperty* Float = CastField<FFloatProperty>(Key))
			{
				OutTypeTag = StealthTuningTypeTag::Float;
				OutValue = static_cast<double>(Float->GetPropertyValue_InContainer(KeyContainer));
				return true;
			}
			if (Key->IsA<FEnumProperty>() || (Key->IsA<FByteProperty>() && CastField<FByteProperty>(Key)->Enum != nullptr))
			{
				OutError = TEXT("enum keys need the EnumValues row column, which is added with the first enum key (story tdc-038 open point)");
				return false;
			}
			if (Key->IsA<FSoftObjectProperty>())
			{
				OutError = TEXT("SoftRef keys (type tag 4) need the AllowedClass/bAllowNull row columns (ADR-0004 amendment 2026-10-25b), not generated yet");
				return false;
			}
			OutError = FString::Printf(TEXT("type %s is not a tuning type (bool, int32, float, uint8 enum; CR-3)"), *Key->GetCPPType());
			return false;
		}

		/** Reads and validates the range metadata of one key. */
		bool ReadRanges(const FProperty* Key, FGenRow& Row, TArray<FString>& OutErrors)
		{
#if WITH_METADATA
			if (Row.TypeTag == StealthTuningTypeTag::Bool)
			{
				Row.HardMin = 0.0;
				Row.HardMax = 1.0;
				Row.SafeMin = 0.0;
				Row.SafeMax = 1.0;
				return true;
			}
			bool bOk = true;
			const TPair<const TCHAR*, double*> Fields[] = {
				{ TEXT("ClampMin"), &Row.HardMin },
				{ TEXT("ClampMax"), &Row.HardMax },
				{ TEXT("SafeMin"), &Row.SafeMin },
				{ TEXT("SafeMax"), &Row.SafeMax },
			};
			for (const TPair<const TCHAR*, double*>& Field : Fields)
			{
				double Value = 0.0;
				if (!ParseNumber(Key->GetMetaData(Field.Key), Value))
				{
					OutErrors.Add(FString::Printf(TEXT("%s: missing or non-numeric meta %s"), *Row.Key, Field.Key));
					bOk = false;
					continue;
				}
				if (Row.TypeTag == StealthTuningTypeTag::Int && Value != FMath::RoundToDouble(Value))
				{
					OutErrors.Add(FString::Printf(TEXT("%s: meta %s must be an integer for an int key"), *Row.Key, Field.Key));
					bOk = false;
					continue;
				}
				// A float key's bounds are float values widened, like its default.
				*Field.Value = Row.TypeTag == StealthTuningTypeTag::Float ? static_cast<double>(static_cast<float>(Value)) : Value;
			}
			return bOk;
#else
			OutErrors.Add(TEXT("UPROPERTY metadata is not available in this build (WITH_METADATA is 0)"));
			return false;
#endif
		}
	}

	bool ReflectRows(TArray<FGenRow>& OutRows, TArray<FString>& OutErrors)
	{
		OutRows.Reset();
		const int32 ErrorsBefore = OutErrors.Num();
		FStealthServerTuning Defaults;
		const UScriptStruct* Face = FStealthServerTuning::StaticStruct();

		for (TFieldIterator<FProperty> OwnerIt(Face); OwnerIt; ++OwnerIt)
		{
			const FStructProperty* OwnerProp = CastField<FStructProperty>(*OwnerIt);
			if (OwnerProp == nullptr)
			{
				OutErrors.Add(FString::Printf(TEXT("FStealthServerTuning::%s is not a tuning sub-struct"), *OwnerIt->GetName()));
				continue;
			}
			const FString StructName = OwnerProp->Struct->GetStructCPPName();
			const bool bPublic = StructName.EndsWith(PublicSuffix, ESearchCase::CaseSensitive);
			if (!bPublic && !StructName.EndsWith(ServerSuffix, ESearchCase::CaseSensitive))
			{
				OutErrors.Add(FString::Printf(TEXT("%s: a tuning sub-struct name must end in _Public or _Server"), *StructName));
				continue;
			}
			const void* OwnerValue = OwnerProp->ContainerPtrToValuePtr<void>(&Defaults);

			for (TFieldIterator<FProperty> KeyIt(OwnerProp->Struct); KeyIt; ++KeyIt)
			{
				const FProperty* KeyProp = *KeyIt;
				FGenRow Row;
				Row.Key = KeyProp->GetName();
				Row.OwnerMember = OwnerProp->GetName();
				Row.OwnerStruct = StructName;
				Row.bPublic = bPublic;

				if (!FCString::IsPureAnsi(*Row.Key) || Row.Key.Len() > 255 || Row.Key.IsEmpty())
				{
					OutErrors.Add(FString::Printf(TEXT("%s: a key must be 1 to 255 ASCII characters (F-2)"), *Row.Key));
					continue;
				}
				FString TypeError;
				if (!ReadDefault(KeyProp, OwnerValue, Row.TypeTag, Row.Default, TypeError))
				{
					OutErrors.Add(FString::Printf(TEXT("%s: %s"), *Row.Key, *TypeError));
					continue;
				}
				if (!ReadRanges(KeyProp, Row, OutErrors))
				{
					continue;
				}
#if WITH_METADATA
				if (!ParseLatch(KeyProp->GetMetaData(TEXT("Latch")), Row.Latch))
				{
					OutErrors.Add(FString::Printf(TEXT("%s: meta Latch must be Round, Action, Use or Session"), *Row.Key));
					continue;
				}
				Row.Owner = KeyProp->GetMetaData(TEXT("Owner"));
				Row.Unit = KeyProp->GetMetaData(TEXT("Unit"));
				if (Row.Owner.IsEmpty() || Row.Unit.IsEmpty() || !FCString::IsPureAnsi(*Row.Owner) || !FCString::IsPureAnsi(*Row.Unit))
				{
					OutErrors.Add(FString::Printf(TEXT("%s: meta Owner and Unit are required (ASCII)"), *Row.Key));
					continue;
				}
				if (KeyProp->HasMetaData(TEXT("Sentinel")))
				{
					Row.bHasSentinel = ParseNumber(KeyProp->GetMetaData(TEXT("Sentinel")), Row.Sentinel);
					if (!Row.bHasSentinel)
					{
						OutErrors.Add(FString::Printf(TEXT("%s: meta Sentinel is not numeric"), *Row.Key));
						continue;
					}
				}
#endif
				OutRows.Add(MoveTemp(Row));
			}
		}

		// Deterministic order: byte-wise (ordinal, case-sensitive) key order, the F-2 hash order and the
		// snapshot wire order. Never the reflection field order.
		OutRows.Sort([](const FGenRow& A, const FGenRow& B)
		{
			return A.Key.Compare(B.Key, ESearchCase::CaseSensitive) < 0;
		});
		for (int32 Index = 1; Index < OutRows.Num(); ++Index)
		{
			if (OutRows[Index].Key.Equals(OutRows[Index - 1].Key, ESearchCase::IgnoreCase))
			{
				OutErrors.Add(FString::Printf(TEXT("%s: declared more than once (keys are globally unique, CR-15.2)"), *OutRows[Index].Key));
			}
		}
		return OutErrors.Num() == ErrorsBefore;
	}

	uint64 Fingerprint(const TArray<FGenRow>& Rows)
	{
		TArray<TArray<ANSICHAR>> KeyStorage;
		KeyStorage.Reserve(Rows.Num());
		TArray<FTuningSchemaRow> Schema;
		Schema.Reserve(Rows.Num());
		for (const FGenRow& Gen : Rows)
		{
			TArray<ANSICHAR>& Key = KeyStorage.AddDefaulted_GetRef();
			Key.Append(TCHAR_TO_ANSI(*Gen.Key), Gen.Key.Len());
			Key.Add('\0');
			FTuningSchemaRow& Row = Schema.AddDefaulted_GetRef();
			Row.Key = Key.GetData();
			Row.TypeTag = Gen.TypeTag;
			Row.bPublic = Gen.bPublic;
			Row.Latch = Gen.Latch;
		}
		return ComputeTuningSchemaFingerprint(Schema);
	}

	FString RenderTable(const TArray<FGenRow>& Rows)
	{
		FString Text;
		Text += TEXT("// GENERATED FILE, DO NOT EDIT. Written by the console command Stealth.Tuning.SchemaGen\n");
		Text += TEXT("// (StealthGameEditor, story tdc-041) from the UPROPERTY declarations of the tuning sub-structs\n");
		Text += TEXT("// (ADR-0004 section 2). Regenerate with\n");
		Text += TEXT("//   UnrealEditor-Cmd StealthGame.uproject -ExecCmds=\"Stealth.Tuning.SchemaGen,QUIT_EDITOR\" -unattended -nullrhi\n");
		Text += TEXT("// or, when this table no longer compiles, tools/ci/regenerate-tuning-schema.ps1 (story tdc-042).\n");
		Text += TEXT("// Commit it with every tuning field change; bump TuningSchemaVersion when the fingerprint changes.\n");
		Text += TEXT("// Rows are in byte-wise (ordinal, case-sensitive) key order: the F-2 hash order and the snapshot\n");
		Text += TEXT("// wire order. Row fields: Key, TypeTag, Get, Set, Default, HardMin, HardMax, SafeMin, SafeMax,\n");
		Text += TEXT("// bPublic, Latch, Owner, Unit, bHasSentinel, Sentinel. Included only by StealthTuningSchema.cpp.\n");
		Text += TEXT("\n");
		Text += TEXT("namespace StealthTuningGenerated\n");
		Text += TEXT("{\n");
		Text += TEXT("\tusing StealthTuningAccess::Get;\n");
		Text += TEXT("\tusing StealthTuningAccess::Set;\n");
		Text += TEXT("\n");
		Text += TEXT("\tinline constexpr FTuningSchemaRow Rows[] = {\n");
		for (const FGenRow& Row : Rows)
		{
			const FString Members = FString::Printf(TEXT("&FStealthServerTuning::%s, &%s::%s"), *Row.OwnerMember, *Row.OwnerStruct, *Row.Key);
			Text += FString::Printf(TEXT("\t\t{ \"%s\", StealthTuningTypeTag::%s, &Get<%s>, &Set<%s>, %s, %s, %s, %s, %s, %s, ETuningLatch::%s, \"%s\", \"%s\", %s, %s },\n"),
				*Row.Key, TypeTagName(Row.TypeTag), *Members, *Members,
				*FormatValue(Row.TypeTag, Row.Default),
				*FormatValue(Row.TypeTag, Row.HardMin), *FormatValue(Row.TypeTag, Row.HardMax),
				*FormatValue(Row.TypeTag, Row.SafeMin), *FormatValue(Row.TypeTag, Row.SafeMax),
				Row.bPublic ? TEXT("true") : TEXT("false"), LatchName(Row.Latch), *Row.Owner, *Row.Unit,
				Row.bHasSentinel ? TEXT("true") : TEXT("false"),
				*FormatValue(Row.bHasSentinel ? Row.TypeTag : StealthTuningTypeTag::Int, Row.bHasSentinel ? Row.Sentinel : 0.0));
		}
		Text += TEXT("\t};\n");
		// Extension point (story tdc-043): values derived from the whole row set follow the table.
		Text += TEXT("\n");
		Text += TEXT("\t/** Schema fingerprint over names, types, audiences, latches and enum values (story tdc-043). */\n");
		Text += FString::Printf(TEXT("\tinline constexpr uint64 SchemaFingerprint = 0x%016llxull;\n"), static_cast<unsigned long long>(Fingerprint(Rows)));
		Text += TEXT("}\n");
		return Text;
	}

	bool Generate(FString& OutText, TArray<FString>& OutErrors)
	{
		TArray<FGenRow> Rows;
		if (!ReflectRows(Rows, OutErrors))
		{
			return false;
		}
		OutText = RenderTable(Rows);
		return true;
	}

	FString CheckedInTablePath()
	{
		return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("Source/StealthFoundation/Private/Tuning/StealthTuningSchema.generated.inl")));
	}

	bool LoadCheckedInTable(FString& OutText)
	{
		return FFileHelper::LoadFileToString(OutText, *CheckedInTablePath());
	}

	namespace
	{
		void LogErrors(const TCHAR* Summary, const TArray<FString>& Errors)
		{
			for (const FString& Error : Errors)
			{
				UE_LOGF(LogStealthTuningSchemaGen, Error, "%ls", *Error);
			}
			UE_LOGF(LogStealthTuningSchemaGen, Error, "%ls: FAIL (%d error(s))", Summary, Errors.Num());
		}

		/** Stealth.Tuning.SchemaGen: writes the table when the generated text differs from the file. */
		void RunSchemaGen()
		{
			const TCHAR* const Summary = TEXT("STEALTH TUNING SCHEMA GEN");
			FString Text;
			TArray<FString> Errors;
			if (!Generate(Text, Errors))
			{
				LogErrors(Summary, Errors);
				return;
			}
			const FString Path = CheckedInTablePath();
			FString Existing;
			if (FFileHelper::LoadFileToString(Existing, *Path) && Existing.Equals(Text, ESearchCase::CaseSensitive))
			{
				UE_LOGF(LogStealthTuningSchemaGen, Display, "%ls: UNCHANGED %ls", Summary, *Path);
				return;
			}
			if (!FFileHelper::SaveStringToFile(Text, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
			{
				UE_LOGF(LogStealthTuningSchemaGen, Error, "%ls: FAIL could not write %ls", Summary, *Path);
				return;
			}
			UE_LOGF(LogStealthTuningSchemaGen, Display, "%ls: WROTE %ls", Summary, *Path);
		}

		/** Stealth.Tuning.SchemaVerify: compiled table, reflection and checked-in file agree; writes nothing. */
		void RunSchemaVerify()
		{
			const TCHAR* const Summary = TEXT("STEALTH TUNING SCHEMA VERIFY");
			TArray<FString> Errors;
			TArray<FGenRow> Rows;
			if (!ReflectRows(Rows, Errors))
			{
				LogErrors(Summary, Errors);
				return;
			}
			const TConstArrayView<FTuningSchemaRow> Compiled = GetTuningSchema();
			if (IsTuningSchemaBootstrapBuild())
			{
				Errors.Add(TEXT("this build compiled the empty bootstrap table (STEALTH_TUNING_SCHEMA_BOOTSTRAP=1 left set)"));
			}
			if (Compiled.Num() != Rows.Num())
			{
				Errors.Add(FString::Printf(TEXT("compiled table has %d row(s), reflection has %d"), Compiled.Num(), Rows.Num()));
			}
			else
			{
				for (int32 Index = 0; Index < Rows.Num(); ++Index)
				{
					if (!Rows[Index].Key.Equals(ANSI_TO_TCHAR(Compiled[Index].Key), ESearchCase::CaseSensitive))
					{
						Errors.Add(FString::Printf(TEXT("row %d: compiled key %s, reflected key %s"), Index, *FString(Compiled[Index].Key), *Rows[Index].Key));
					}
				}
			}
			if (GetTuningSchemaFingerprint() != Fingerprint(Rows))
			{
				Errors.Add(TEXT("compiled fingerprint differs from the reflected fingerprint"));
			}
			FString Existing;
			if (!LoadCheckedInTable(Existing))
			{
				Errors.Add(FString::Printf(TEXT("checked-in table missing: %s"), *CheckedInTablePath()));
			}
			else if (!Existing.Equals(RenderTable(Rows), ESearchCase::CaseSensitive))
			{
				Errors.Add(TEXT("checked-in table differs from the generated text (run Stealth.Tuning.SchemaGen)"));
			}
			if (!Errors.IsEmpty())
			{
				LogErrors(Summary, Errors);
				return;
			}
			UE_LOGF(LogStealthTuningSchemaGen, Display, "%ls: PASS rows=%d bootstrap=0 fingerprint=0x%016llx", Summary, Rows.Num(),
				static_cast<unsigned long long>(GetTuningSchemaFingerprint()));
		}

		FAutoConsoleCommand SchemaGenCommand(
			TEXT("Stealth.Tuning.SchemaGen"),
			TEXT("Story tdc-041: regenerates Source/StealthFoundation/Private/Tuning/StealthTuningSchema.generated.inl from the tuning UPROPERTY declarations (writes only when it differs)."),
			FConsoleCommandDelegate::CreateStatic(&RunSchemaGen));

		FAutoConsoleCommand SchemaVerifyCommand(
			TEXT("Stealth.Tuning.SchemaVerify"),
			TEXT("Story tdc-042: checks the compiled schema table, the reflection and the checked-in table agree and that this is not a bootstrap build (writes nothing)."),
			FConsoleCommandDelegate::CreateStatic(&RunSchemaVerify));
	}
}
