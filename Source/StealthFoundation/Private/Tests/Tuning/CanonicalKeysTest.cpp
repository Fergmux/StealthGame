// Story tdc-040 (production/epics/tuning-data-config/story-040-canonical-keys-and-faces.md): the eight
// canonical keys (GDD tuning-data-config.md, Canonical Tunables) are each declared once as a UPROPERTY with
// the table's type, default (C++ initialiser), hard range (ClampMin/ClampMax), safe range (SafeMin/SafeMax),
// audience (the _Public/_Server sub-struct), latch, owner and unit (custom meta; unit = the CR-15.2 name
// suffix, since the table has no Unit column). FStealthClientTuning (Public/View, _Public only) and
// FStealthServerTuning (Public/Server, public + server) are distinct types, no ServerOnly key is reachable
// from the client face, and UStealthTuningData declares exactly the server face's members.
// The expected values below are committed constants copied from the GDD table (the Default pin; the full
// pin of every key to the schema is story tdc-044). Metadata exists only with WITH_METADATA (editor
// builds); the metadata clauses are compiled only there.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Server/StealthServerTuning.h"
#include "Server/StealthTuningData.h"
#include "UObject/UnrealType.h"
#include "View/StealthClientTuning.h"

#include <type_traits>

static_assert(!std::is_same_v<FStealthClientTuning, FStealthServerTuning>, "client and server tuning faces are distinct types");

namespace CanonicalKeysTestData
{
	enum class EType : uint8 { Int, Float };

	struct FCanonicalKey
	{
		const TCHAR* Key;
		EType Type;
		double Default;
		const TCHAR* HardMin;
		const TCHAR* HardMax;
		const TCHAR* SafeMin;
		const TCHAR* SafeMax;
		bool bPublic; // Audience All (true) or ServerOnly (false)
		const TCHAR* Latch;
		const TCHAR* Owner;
		const TCHAR* Unit;
	};

	/** GDD Canonical Tunables, 2026-10-25 (NpcCount default 50 by user decision). Owner ids: Round, Crowd, Assassination, Penalty, Objectives. */
	const FCanonicalKey Expected[] = {
		{ TEXT("TeamSize"), EType::Int, 5.0, TEXT("1"), TEXT("5"), TEXT("2"), TEXT("5"), true, TEXT("Round"), TEXT("Round"), TEXT("Size") },
		{ TEXT("NpcCount"), EType::Int, 50.0, TEXT("6"), TEXT("100"), TEXT("45"), TEXT("50"), false, TEXT("Round"), TEXT("Crowd"), TEXT("Count") },
		{ TEXT("AssassinationRangeCm"), EType::Float, 200.0, TEXT("100"), TEXT("250"), TEXT("150"), TEXT("250"), true, TEXT("Use"), TEXT("Assassination"), TEXT("Cm") },
		{ TEXT("DefenceDurationSec"), EType::Float, 4.0, TEXT("3.0"), TEXT("5.0"), TEXT("3.0"), TEXT("5.0"), true, TEXT("Action"), TEXT("Assassination"), TEXT("Sec") },
		{ TEXT("DefenceCooldownSec"), EType::Float, 60.0, TEXT("30"), TEXT("120"), TEXT("45"), TEXT("75"), true, TEXT("Action"), TEXT("Assassination"), TEXT("Sec") },
		{ TEXT("NpcKillStrikeLimit"), EType::Int, 3.0, TEXT("2"), TEXT("5"), TEXT("2"), TEXT("4"), true, TEXT("Round"), TEXT("Penalty"), TEXT("Limit") },
		{ TEXT("ObjectiveInteractSec"), EType::Float, 3.0, TEXT("2.0"), TEXT("6.0"), TEXT("2.9"), TEXT("4.5"), true, TEXT("Action"), TEXT("Objectives"), TEXT("Sec") },
		{ TEXT("TargetRoundMinutes"), EType::Float, 12.0, TEXT("5"), TEXT("30"), TEXT("8"), TEXT("15"), false, TEXT("Round"), TEXT("Round"), TEXT("Minutes") },
	};

	/** One declaration of a key: the sub-struct member holding it and the key property. */
	struct FFound
	{
		const FStructProperty* Owner = nullptr;
		const FProperty* Key = nullptr;
	};

	/** Every declaration of every key name in Face's sub-structs. */
	TMultiMap<FString, FFound> CollectKeys(const UScriptStruct* Face)
	{
		TMultiMap<FString, FFound> Out;
		for (TFieldIterator<FProperty> OwnerIt(Face); OwnerIt; ++OwnerIt)
		{
			const FStructProperty* Owner = CastField<FStructProperty>(*OwnerIt);
			if (Owner == nullptr)
			{
				continue;
			}
			for (TFieldIterator<FProperty> KeyIt(Owner->Struct); KeyIt; ++KeyIt)
			{
				Out.Add(KeyIt->GetName(), FFound{ Owner, *KeyIt });
			}
		}
		return Out;
	}

	/** Reads a key's default from a default-constructed face. */
	double ReadDefault(const FFound& Found, const void* Face)
	{
		const void* OwnerValue = Found.Owner->ContainerPtrToValuePtr<void>(Face);
		if (const FIntProperty* Int = CastField<FIntProperty>(Found.Key))
		{
			return static_cast<double>(Int->GetPropertyValue_InContainer(OwnerValue));
		}
		if (const FFloatProperty* Float = CastField<FFloatProperty>(Found.Key))
		{
			return static_cast<double>(Float->GetPropertyValue_InContainer(OwnerValue));
		}
		return TNumericLimits<double>::Lowest();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTuningCanonicalKeysTest, "StealthGame.Tuning.CanonicalKeys",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTuningCanonicalKeysTest::RunTest(const FString& Parameters)
{
	using namespace CanonicalKeysTestData;

	const TMultiMap<FString, FFound> ServerKeys = CollectKeys(FStealthServerTuning::StaticStruct());
	const TMultiMap<FString, FFound> ClientKeys = CollectKeys(FStealthClientTuning::StaticStruct());
	FStealthServerTuning ServerDefaults;

	for (const FCanonicalKey& Want : Expected)
	{
		TArray<FFound> Declarations;
		ServerKeys.MultiFind(Want.Key, Declarations);
		if (!TestEqual(FString::Printf(TEXT("%s is declared exactly once on the server face"), Want.Key), Declarations.Num(), 1))
		{
			continue;
		}
		const FFound& Found = Declarations[0];
		const FString OwnerStruct = Found.Owner->Struct->GetStructCPPName();

		// Type and default.
		const bool bTypeOk = Want.Type == EType::Int ? Found.Key->IsA<FIntProperty>() : Found.Key->IsA<FFloatProperty>();
		TestTrue(FString::Printf(TEXT("%s type is %s"), Want.Key, Want.Type == EType::Int ? TEXT("int32") : TEXT("float")), bTypeOk);
		TestEqual(FString::Printf(TEXT("%s default (C++ initialiser)"), Want.Key), ReadDefault(Found, &ServerDefaults), Want.Default, 1e-6);

		// Audience is structural: the sub-struct suffix, and presence on the client face.
		TestTrue(FString::Printf(TEXT("%s sits in a %s sub-struct (%s)"), Want.Key, Want.bPublic ? TEXT("_Public") : TEXT("_Server"), *OwnerStruct),
			OwnerStruct.EndsWith(Want.bPublic ? TEXT("_Public") : TEXT("_Server"), ESearchCase::CaseSensitive));
		TArray<FFound> OnClient;
		ClientKeys.MultiFind(Want.Key, OnClient);
		TestEqual(FString::Printf(TEXT("%s on the client face: %s"), Want.Key, Want.bPublic ? TEXT("once") : TEXT("absent (ServerOnly)")), OnClient.Num(), Want.bPublic ? 1 : 0);

#if WITH_METADATA
		const TPair<const TCHAR*, const TCHAR*> Meta[] = {
			{ TEXT("ClampMin"), Want.HardMin }, { TEXT("ClampMax"), Want.HardMax },
			{ TEXT("SafeMin"), Want.SafeMin }, { TEXT("SafeMax"), Want.SafeMax },
			{ TEXT("Latch"), Want.Latch }, { TEXT("Owner"), Want.Owner }, { TEXT("Unit"), Want.Unit },
		};
		for (const TPair<const TCHAR*, const TCHAR*>& Pair : Meta)
		{
			const FString Actual = Found.Key->GetMetaData(Pair.Key);
			double ActualNumber = 0.0;
			double WantNumber = 0.0;
			const bool bNumeric = LexTryParseString(WantNumber, Pair.Value) && LexTryParseString(ActualNumber, *Actual);
			if (bNumeric)
			{
				TestEqual(FString::Printf(TEXT("%s meta %s"), Want.Key, Pair.Key), ActualNumber, WantNumber, 1e-9);
			}
			else
			{
				TestEqual(FString::Printf(TEXT("%s meta %s"), Want.Key, Pair.Key), Actual, FString(Pair.Value));
			}
		}
		TestTrue(FString::Printf(TEXT("%s unit is its name suffix (CR-15.2)"), Want.Key), FString(Want.Key).EndsWith(Want.Unit, ESearchCase::CaseSensitive));
		TestFalse(FString::Printf(TEXT("%s has no Config flag"), Want.Key), Found.Key->HasAnyPropertyFlags(CPF_Config));
#endif
	}

	// No ServerOnly key is reachable from the client face, and every client sub-struct is _Public.
	for (TFieldIterator<FProperty> It(FStealthClientTuning::StaticStruct()); It; ++It)
	{
		const FStructProperty* Owner = CastField<FStructProperty>(*It);
		const bool bPublicStruct = Owner != nullptr && Owner->Struct->GetStructCPPName().EndsWith(TEXT("_Public"), ESearchCase::CaseSensitive);
		TestTrue(FString::Printf(TEXT("client face member %s is a _Public sub-struct"), *It->GetName()), bPublicStruct);
	}

	// The preset class declares exactly the server face's members (same names, same struct types).
	{
		TMap<FString, const UScriptStruct*> FaceMembers;
		for (TFieldIterator<FProperty> It(FStealthServerTuning::StaticStruct()); It; ++It)
		{
			const FStructProperty* Owner = CastField<FStructProperty>(*It);
			FaceMembers.Add(It->GetName(), Owner ? Owner->Struct.Get() : nullptr);
		}
		int32 PresetMembers = 0;
		for (TFieldIterator<FProperty> It(UStealthTuningData::StaticClass(), EFieldIteratorFlags::ExcludeSuper); It; ++It)
		{
			++PresetMembers;
			const FStructProperty* Owner = CastField<FStructProperty>(*It);
			const UScriptStruct* const* FaceType = FaceMembers.Find(It->GetName());
			TestTrue(FString::Printf(TEXT("preset member %s is a server-face member of the same type"), *It->GetName()),
				FaceType != nullptr && Owner != nullptr && *FaceType == Owner->Struct.Get());
		}
		TestEqual(TEXT("the preset declares as many members as the server face"), PresetMembers, FaceMembers.Num());
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
