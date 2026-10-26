#include "Server/CrowdAssetKeys.h"

#include "Server/StealthServerTuning.h"
#include "Server/StealthTuningHasher.h"
#include "Server/StealthTuningSchema.h"
#include "UObject/UObjectGlobals.h"

namespace CrowdAssetKeysLocal
{
	StealthCrowdTuning::FStateTreeSchemaCheck GSchemaCheck = nullptr;
}

ECrowdAssetCheck StealthCrowdTuning::CheckAsset(const FSoftObjectPath& Path, const TCHAR* RequiredClassPath, bool bStateTree)
{
	if (Path.IsNull())
	{
		return ECrowdAssetCheck::Missing;
	}
	const UObject* Asset = Path.TryLoad();
	if (Asset == nullptr)
	{
		return ECrowdAssetCheck::Missing;
	}
	const UClass* Required = FindObject<UClass>(FTopLevelAssetPath(RequiredClassPath));
	if (Required == nullptr || !Asset->GetClass()->IsChildOf(Required))
	{
		return ECrowdAssetCheck::WrongClass;
	}
	if (bStateTree && (CrowdAssetKeysLocal::GSchemaCheck == nullptr || !CrowdAssetKeysLocal::GSchemaCheck(Asset)))
	{
		return ECrowdAssetCheck::WrongSchema;
	}
	return ECrowdAssetCheck::Valid;
}

void StealthCrowdTuning::SetStateTreeSchemaCheck(FStateTreeSchemaCheck Check)
{
	CrowdAssetKeysLocal::GSchemaCheck = Check;
}

bool StealthCrowdTuning::ValidateAssetKeys(const FCrowdAssetKeys& Keys, TArray<FString>* OutProblems)
{
	struct FEntry
	{
		const TCHAR* Key;
		const FSoftObjectPath* Path;
		const TCHAR* ClassPath;
		bool bStateTree;
	};
	const FEntry Entries[] = {
		{ TEXT("CrowdMotionProfileAsset"), &Keys.CrowdMotionProfileAsset, MotionProfileClassPath(), false },
		{ TEXT("CrowdFullStateTreeAsset"), &Keys.CrowdFullStateTreeAsset, StateTreeClassPath(), true },
		{ TEXT("CrowdRestrictedStateTreeAsset"), &Keys.CrowdRestrictedStateTreeAsset, StateTreeClassPath(), true },
	};
	bool bValid = true;
	for (const FEntry& Entry : Entries)
	{
		const ECrowdAssetCheck Result = CheckAsset(*Entry.Path, Entry.ClassPath, Entry.bStateTree);
		if (Result != ECrowdAssetCheck::Valid)
		{
			bValid = false;
			if (OutProblems != nullptr)
			{
				const TCHAR* Why = Result == ECrowdAssetCheck::Missing ? TEXT("missing") : (Result == ECrowdAssetCheck::WrongClass ? TEXT("wrong class") : TEXT("not on the crowd schema"));
				OutProblems->Add(FString::Printf(TEXT("%s (%s): %s"), Entry.Key, *Entry.Path->ToString(), Why));
			}
		}
	}
	return bValid;
}

uint64 StealthCrowdTuning::ComputeRoundTuningHash(const FStealthServerTuning& Values, const FCrowdAssetKeys& Keys, int32 SchemaVersion)
{
	const TConstArrayView<FTuningSchemaRow> Schema = GetTuningSchema();
	TArray<FTuningHashEntry> Entries;
	Entries.Reserve(Schema.Num() + 3);
	for (const FTuningSchemaRow& Row : Schema)
	{
		FTuningHashEntry& Entry = Entries.AddDefaulted_GetRef();
		Entry.Key = FAnsiStringView(Row.Key);
		Entry.TypeTag = Row.TypeTag;
		Entry.Value = Row.Get != nullptr ? Row.Get(Values) : Row.Default;
	}
	// The SoftRef values: FSoftObjectPath::GetAssetPath().ToString() (ADR-0004 amendment 2026-10-25b); kept alive here.
	const FString Paths[] = { Keys.CrowdFullStateTreeAsset.GetAssetPath().ToString(), Keys.CrowdMotionProfileAsset.GetAssetPath().ToString(), Keys.CrowdRestrictedStateTreeAsset.GetAssetPath().ToString() };
	const ANSICHAR* Names[] = { "CrowdFullStateTreeAsset", "CrowdMotionProfileAsset", "CrowdRestrictedStateTreeAsset" };
	for (int32 Index = 0; Index < 3; ++Index)
	{
		FTuningHashEntry& Entry = Entries.AddDefaulted_GetRef();
		Entry.Key = FAnsiStringView(Names[Index]);
		Entry.TypeTag = StealthTuningTypeTag::SoftRef;
		Entry.SoftPath = FStringView(Paths[Index]);
	}
	return FTuningHasher::HashEntries(SchemaVersion, Entries);
}
