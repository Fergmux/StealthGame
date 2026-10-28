// The replicated-property allowlist's shared parse and per-class audit (ADR-0003 section 8, ADR-0008 section 4). Added by
// Objectives story oc-011 (production/epics/objectives-chain/story-011-team-view-component-and-replication.md) as the
// stand-in of IVP stories ivp-013 / ivp-015. See Server/Testing/StealthReplicationAllowlist.h.

#include "Server/Testing/StealthReplicationAllowlist.h"

#if WITH_STEALTH_TEST_SEAMS

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Net/UnrealNetwork.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"

namespace StealthReplicationAllowlistLocal
{
	bool ReadStrings(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, TArray<FString>& Out)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!Object->TryGetArrayField(Field, Values))
		{
			return false;
		}
		for (const TSharedPtr<FJsonValue>& Value : *Values)
		{
			Out.Add(Value->AsString());
		}
		return true;
	}
}

FString StealthReplicationAllowlist::GetDefaultPath()
{
	return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("Source/StealthFoundation/Private/Tests/Data/ReplicationAllowlist.json")));
}

bool StealthReplicationAllowlist::Parse(const FString& JsonText, TArray<FReplicationAllowlistRow>& OutRows, FString& OutError)
{
	using namespace StealthReplicationAllowlistLocal;
	OutRows.Reset();
	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(JsonText), Root) || !Root.IsValid())
	{
		OutError = TEXT("the allowlist is not valid JSON");
		return false;
	}
	const TArray<TSharedPtr<FJsonValue>>* Sections = nullptr;
	if (!Root->TryGetArrayField(TEXT("sections"), Sections))
	{
		OutError = TEXT("the allowlist has no sections array");
		return false;
	}
	for (const TSharedPtr<FJsonValue>& SectionValue : *Sections)
	{
		const TSharedPtr<FJsonObject> Section = SectionValue->AsObject();
		const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
		if (!Section.IsValid() || !Section->TryGetArrayField(TEXT("rows"), Rows))
		{
			OutError = TEXT("a section has no rows array");
			return false;
		}
		const FString SectionName = Section->GetStringField(TEXT("name"));
		for (const TSharedPtr<FJsonValue>& RowValue : *Rows)
		{
			const TSharedPtr<FJsonObject> Row = RowValue->AsObject();
			FReplicationAllowlistRow& Out = OutRows.AddDefaulted_GetRef();
			Out.Section = SectionName;
			const bool bFields = Row.IsValid() && Row->TryGetStringField(TEXT("class"), Out.Class) && Row->TryGetStringField(TEXT("property"), Out.Property)
				&& Row->TryGetStringField(TEXT("condition"), Out.Condition) && Row->TryGetStringField(TEXT("audience"), Out.Audience)
				&& Row->TryGetStringField(TEXT("carrier"), Out.Carrier) && ReadStrings(Row, TEXT("owners"), Out.Owners);
			if (!bFields)
			{
				OutError = FString::Printf(TEXT("a row of section '%s' misses a required field (class, property, condition, audience, carrier, owners)"), *SectionName);
				return false;
			}
			ReadStrings(Row, TEXT("policyRows"), Out.PolicyRows);
		}
	}
	return true;
}

bool StealthReplicationAllowlist::Load(TArray<FReplicationAllowlistRow>& OutRows, FString& OutError)
{
	FString Text;
	const FString Path = GetDefaultPath();
	if (!FFileHelper::LoadFileToString(Text, *Path))
	{
		OutError = FString::Printf(TEXT("cannot read the allowlist at %s"), *Path);
		return false;
	}
	return Parse(Text, OutRows, OutError);
}

FString StealthReplicationAllowlist::ConditionName(int32 Condition)
{
	static const TCHAR* const Names[] = {
		TEXT("COND_None"), TEXT("COND_InitialOnly"), TEXT("COND_OwnerOnly"), TEXT("COND_SkipOwner"), TEXT("COND_SimulatedOnly"),
		TEXT("COND_AutonomousOnly"), TEXT("COND_SimulatedOrPhysics"), TEXT("COND_InitialOrOwner"), TEXT("COND_Custom"),
		TEXT("COND_ReplayOrOwner"), TEXT("COND_ReplayOnly"), TEXT("COND_SimulatedOnlyNoReplay"), TEXT("COND_SimulatedOrPhysicsNoReplay"),
		TEXT("COND_SkipReplay"), TEXT("COND_Dynamic"), TEXT("COND_Never"),
	};
	return Condition >= 0 && Condition < UE_ARRAY_COUNT(Names) ? FString(Names[Condition]) : FString::Printf(TEXT("COND_%d"), Condition);
}

void StealthReplicationAllowlist::AuditClass(const UClass* Class, TConstArrayView<FReplicationAllowlistRow> Rows, TArray<FString>& OutFindings)
{
	if (Class == nullptr)
	{
		OutFindings.Add(TEXT("no class to audit"));
		return;
	}
	const FString ClassName = FString(Class->GetPrefixCPP()) + Class->GetName();
	TArray<FLifetimeProperty> Lifetime;
	if (const UObject* Default = Class->GetDefaultObject())
	{
		Default->GetLifetimeReplicatedProps(Lifetime);
	}
	TSet<FString> Replicated;
	for (TFieldIterator<FProperty> It(Class, EFieldIteratorFlags::ExcludeSuper); It; ++It)
	{
		const FProperty* Property = *It;
		if (!Property->HasAnyPropertyFlags(CPF_Net))
		{
			continue;
		}
		const FString Name = Property->GetName();
		Replicated.Add(Name);
		const FLifetimeProperty* Registered = Lifetime.FindByPredicate([Property](const FLifetimeProperty& L) { return L.RepIndex == Property->RepIndex; });
		const FString Actual = Registered != nullptr ? ConditionName(static_cast<int32>(Registered->Condition)) : FString(TEXT("(not registered)"));
		int32 Matches = 0;
		for (const FReplicationAllowlistRow& Row : Rows)
		{
			if (Row.Class == ClassName && Row.Property == Name)
			{
				++Matches;
				if (Row.Condition != Actual)
				{
					OutFindings.Add(FString::Printf(TEXT("%s::%s: the row says %s, the class registers %s"), *ClassName, *Name, *Row.Condition, *Actual));
				}
			}
		}
		if (Matches != 1)
		{
			OutFindings.Add(FString::Printf(TEXT("%s::%s: %d allowlist rows (exactly 1 required)"), *ClassName, *Name, Matches));
		}
	}
	for (const FReplicationAllowlistRow& Row : Rows)
	{
		if (Row.Class == ClassName && !Replicated.Contains(Row.Property))
		{
			OutFindings.Add(FString::Printf(TEXT("%s::%s: a row names a property the class does not replicate"), *ClassName, *Row.Property));
		}
	}
}

#endif // WITH_STEALTH_TEST_SEAMS
