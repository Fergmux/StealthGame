// Story asn-004 (production/epics/assassination-defence-reversal/story-004-wire-structs-fixed-width.md), AC2: AC-ASN-20
// shape clause and ADR-0018 Identical. Reflection enumerates FActionState, FFixedWarpOffset and FDeathState: no property is
// an object, actor, pawn, class, interface, soft or weak reference, or FName. Identical is false for a pair differing in
// exactly one field (each of the six FActionState fields, Seq included; both FDeathState fields) and true only for
// field-equal values. The struct ops advertise the custom NetSerialize and Identical (so the legacy driver uses them).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "UObject/UnrealType.h"
#include "View/StealthEncounterWire.h"

#include <limits>

namespace AssassinationWireShapeTestData
{
	/** Names of the forbidden properties of Struct (recursing into nested structs). */
	void CollectForbidden(const UScriptStruct* Struct, const FString& Prefix, TArray<FString>& Out)
	{
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			const FProperty* Property = *It;
			const FString Name = Prefix + Property->GetName();
			if (Property->IsA<FObjectPropertyBase>() || Property->IsA<FInterfaceProperty>() || Property->IsA<FNameProperty>() || Property->IsA<FDelegateProperty>()
				|| Property->IsA<FMulticastDelegateProperty>())
			{
				Out.Add(Name);
			}
			else if (const FStructProperty* Nested = CastField<FStructProperty>(Property))
			{
				CollectForbidden(Nested->Struct, Name + TEXT("."), Out);
			}
		}
	}

	int32 CountProperties(const UScriptStruct* Struct)
	{
		int32 Count = 0;
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			++Count;
		}
		return Count;
	}

	bool Same(const FActionState& A, const FActionState& B) { return A.Identical(&B, 0); }
	bool Same(const FDeathState& A, const FDeathState& B) { return A.Identical(&B, 0); }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAssassinationWireShapeTest, "StealthGame.Assassination.WireShape",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAssassinationWireShapeTest::RunTest(const FString& Parameters)
{
	using namespace AssassinationWireShapeTestData;

	// ---- Reflection: no pawn, actor, object or FName property ----
	for (const UScriptStruct* Struct : { FActionState::StaticStruct(), FDeathState::StaticStruct(), FFixedWarpOffset::StaticStruct() })
	{
		TArray<FString> Forbidden;
		CollectForbidden(Struct, FString(), Forbidden);
		TestTrue(FString::Printf(TEXT("%s has no object, actor, pawn or FName property (found: %s)"), *Struct->GetName(), *FString::Join(Forbidden, TEXT(", "))), Forbidden.IsEmpty());
	}
	TestEqual(TEXT("FActionState has exactly six properties"), CountProperties(FActionState::StaticStruct()), 6);
	TestEqual(TEXT("FDeathState has exactly two properties"), CountProperties(FDeathState::StaticStruct()), 2);
	TestEqual(TEXT("FFixedWarpOffset has exactly three properties (UPROPERTY for legacy change detection)"), CountProperties(FFixedWarpOffset::StaticStruct()), 3);
	TestTrue(TEXT("FActionState advertises NetSerialize and Identical"), (FActionState::StaticStruct()->StructFlags & (STRUCT_NetSerializeNative | STRUCT_IdenticalNative)) == (STRUCT_NetSerializeNative | STRUCT_IdenticalNative));
	TestTrue(TEXT("FDeathState advertises NetSerialize and Identical"), (FDeathState::StaticStruct()->StructFlags & (STRUCT_NetSerializeNative | STRUCT_IdenticalNative)) == (STRUCT_NetSerializeNative | STRUCT_IdenticalNative));

	// ---- FActionState: Identical over each field ----
	FActionState Base;
	Base.Pose = EActionPose::Strike;
	Base.Seq = 7;
	Base.Variant = StrikeVariant::Strike_L60;
	Base.StartServerTime = 100.0;
	Base.ContactServerTime = 100.4;
	Base.WarpPoint.X = 1500;
	Base.WarpPoint.Y = -20;
	Base.WarpPoint.Z = 3;
	{
		const FActionState Copy = Base;
		TestTrue(TEXT("Identical is true for field-equal values"), Same(Base, Copy));
		TestTrue(TEXT("Identical is true for two defaults"), Same(FActionState(), FActionState()));
		TestFalse(TEXT("Identical is false against null"), Base.Identical(nullptr, 0));

		FActionState D = Base; D.Pose = EActionPose::Brace;
		TestFalse(TEXT("a Pose difference registers"), Same(Base, D));
		D = Base; D.Seq = 8;
		TestFalse(TEXT("a Seq-only difference registers"), Same(Base, D));
		D = Base; D.Variant = StrikeVariant::Strike_R60;
		TestFalse(TEXT("a Variant difference registers"), Same(Base, D));
		D = Base; D.StartServerTime = 100.0000001;
		TestFalse(TEXT("a StartServerTime difference registers"), Same(Base, D));
		D = Base; D.ContactServerTime = 100.5;
		TestFalse(TEXT("a ContactServerTime difference registers"), Same(Base, D));
		D = Base; D.WarpPoint.X = 1501;
		TestFalse(TEXT("a WarpPoint.X difference registers"), Same(Base, D));
		D = Base; D.WarpPoint.Y = -21;
		TestFalse(TEXT("a WarpPoint.Y difference registers"), Same(Base, D));
		D = Base; D.WarpPoint.Z = 4;
		TestFalse(TEXT("a WarpPoint.Z difference registers"), Same(Base, D));

		FActionState NaNA = Base; NaNA.StartServerTime = std::numeric_limits<double>::quiet_NaN();
		const FActionState NaNB = NaNA;
		TestTrue(TEXT("an equal NaN pattern is identical (bitwise)"), Same(NaNA, NaNB));
	}

	// ---- FDeathState ----
	{
		FDeathState A;
		A.DeathServerTime = 42.5;
		A.bDiedConcealed = false;
		FDeathState B = A;
		TestTrue(TEXT("FDeathState: Identical is true for field-equal values"), Same(A, B));
		B.DeathServerTime = 42.6;
		TestFalse(TEXT("FDeathState: a DeathServerTime difference registers"), Same(A, B));
		B = A; B.bDiedConcealed = true;
		TestFalse(TEXT("FDeathState: a bDiedConcealed-only difference registers"), Same(A, B));
	}

	// ---- Sentinels and helpers ----
	{
		const FActionState Default;
		TestEqual(TEXT("default Seq is the sentinel 1"), static_cast<int32>(Default.Seq), 1);
		TestEqual(TEXT("default ContactServerTime is -1"), Default.ContactServerTime, -1.0);
		TestEqual(TEXT("default DeathServerTime is -1"), FDeathState().DeathServerTime, -1.0);
		TestEqual(TEXT("Seq wraps 65535 to 1"), static_cast<int32>(FActionState::NextSeq(65535)), 1);
		TestEqual(TEXT("Seq advances by one"), static_cast<int32>(FActionState::NextSeq(1)), 2);
		const FFixedWarpOffset Offset = FFixedWarpOffset::FromOffsetCm(FVector(150.04, -0.05, 99999.0));
		TestTrue(TEXT("WarpPoint quantises to 0.1 cm and saturates at int16"), Offset.X == 1500 && Offset.Y == -1 && Offset.Z == MAX_int16);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
