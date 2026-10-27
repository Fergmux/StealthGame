// Story iif-001 (production/epics/input-interaction-framework/story-001-command-reply-types.md), ADR-0011 Key
// Interfaces: the reflected command and reply types have exactly the shape the ADR fixes.
//   - ECommandRejectCode, EHoldOutcome, EActionType, ECancelReason (and the confirmed ECommandKind / ECommandReplyKind
//     sets): uint8, exact names, contiguous values from 0, exact count.
//   - FClientCommandSeq, FHoldStatus, FCommandReply, FNetPawnCommand, FNetCommandArgs: exact field names and types.
//     FCommandReply carries ADR-0018's StrikeResult / DefenceResult payloads since Assassination story asn-015 (the
//     reserved tags' members Strike and Defence; FStrikeResult and FDefenceResult moved into this header).
//   - No UObject-pointer field and no ECancelReason field in any reply or command struct; no identity-named field.
//   - A binary round trip (UScriptStruct::SerializeBin) of each struct is value-equal, including Seq 0, the
//     uint16/uint32 maxima and an invalid PawnRefGuid (which round-trips as invalid).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "IdentityNameChecker.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Server/Core/CancelReason.h"
#include "View/CommandReplyTypes.h"
#include "View/NetPawnCommand.h"

namespace CommandReplyTypesTestData
{
	/** One expected field: name and C++ type as reflection reports it. */
	struct FField
	{
		const TCHAR* Name;
		const TCHAR* Type;
	};

	/** The reflected enum's names (without the _MAX entry) and whether values are 0..N-1. */
	bool EnumShape(FAutomationTestBase& Test, const UEnum* Enum, const TArray<FString>& ExpectedNames)
	{
		if (!Test.TestNotNull(TEXT("enum is reflected"), Enum))
		{
			return false;
		}
		const int32 Count = Enum->NumEnums() - (Enum->ContainsExistingMax() ? 1 : 0);
		Test.TestEqual(FString::Printf(TEXT("%s value count"), *Enum->GetName()), Count, ExpectedNames.Num());
		for (int32 Index = 0; Index < FMath::Min(Count, ExpectedNames.Num()); ++Index)
		{
			Test.TestEqual(FString::Printf(TEXT("%s[%d] name"), *Enum->GetName(), Index), Enum->GetNameStringByIndex(Index), ExpectedNames[Index]);
			Test.TestEqual(FString::Printf(TEXT("%s[%d] value is contiguous"), *Enum->GetName(), Index), Enum->GetValueByIndex(Index), static_cast<int64>(Index));
		}
		return true;
	}

	void StructShape(FAutomationTestBase& Test, const UScriptStruct* Struct, const TArray<FField>& Expected)
	{
		TArray<FString> Actual;
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			Actual.Add(FString::Printf(TEXT("%s:%s"), *It->GetName(), *It->GetCPPType()));
		}
		TArray<FString> Wanted;
		for (const FField& Field : Expected)
		{
			Wanted.Add(FString::Printf(TEXT("%s:%s"), Field.Name, Field.Type));
		}
		Test.TestTrue(FString::Printf(TEXT("%s fields (got %s)"), *Struct->GetName(), *FString::Join(Actual, TEXT(", "))), Actual == Wanted);
	}

	/** True when any field (recursive) of Struct is an enum property of ECancelReason. */
	bool HasCancelReasonField(const UStruct* Struct)
	{
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			if (const FEnumProperty* EnumProp = CastField<FEnumProperty>(*It))
			{
				if (EnumProp->GetEnum() == StaticEnum<ECancelReason>())
				{
					return true;
				}
			}
			if (const FByteProperty* ByteProp = CastField<FByteProperty>(*It))
			{
				if (ByteProp->Enum == StaticEnum<ECancelReason>())
				{
					return true;
				}
			}
			if (const FStructProperty* Nested = CastField<FStructProperty>(*It))
			{
				if (HasCancelReasonField(Nested->Struct))
				{
					return true;
				}
			}
		}
		return false;
	}

	template <typename T>
	T RoundTrip(const T& Value)
	{
		TArray<uint8> Bytes;
		FMemoryWriter Writer(Bytes);
		T Copy = Value;
		T::StaticStruct()->SerializeBin(Writer, &Copy);
		T Out;
		FMemoryReader Reader(Bytes);
		T::StaticStruct()->SerializeBin(Reader, &Out);
		return Out;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInputCommandReplyTypesTest, "StealthGame.Input.CommandReplyTypes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FInputCommandReplyTypesTest::RunTest(const FString& Parameters)
{
	using namespace CommandReplyTypesTestData;

	// Enums: uint8, names, contiguous values, exact counts (matching the G*Count constants).
	static_assert(sizeof(ECommandRejectCode) == 1 && sizeof(EHoldOutcome) == 1 && sizeof(EActionType) == 1 && sizeof(ECancelReason) == 1
		&& sizeof(ECommandKind) == 1 && sizeof(ECommandReplyKind) == 1, "command interface enums are uint8");
	EnumShape(*this, StaticEnum<ECommandRejectCode>(), { TEXT("NotNow"), TEXT("Cooldown"), TEXT("Busy"), TEXT("OutOfReach") });
	EnumShape(*this, StaticEnum<EHoldOutcome>(), { TEXT("Active"), TEXT("Completed"), TEXT("Cancelled") });
	EnumShape(*this, StaticEnum<EActionType>(), { TEXT("Interact"), TEXT("Prepare"), TEXT("Attack"), TEXT("HideEnter"), TEXT("HideExit") });
	EnumShape(*this, StaticEnum<ECancelReason>(),
		{ TEXT("Released"), TEXT("Moved"), TEXT("Superseded"), TEXT("Illegal"), TEXT("OutOfRange"), TEXT("TargetInvalid"), TEXT("PawnRemoved"), TEXT("Reversed") });
	EnumShape(*this, StaticEnum<ECommandKind>(),
		{ TEXT("Traverse"), TEXT("InteractBegin"), TEXT("InteractEnd"), TEXT("PrepareBegin"), TEXT("PrepareEnd"), TEXT("Attack"), TEXT("HideEnter"), TEXT("HideExit") });
	EnumShape(*this, StaticEnum<ECommandReplyKind>(), { TEXT("Rejected"), TEXT("HoldStatus"), TEXT("StrikeResult"), TEXT("DefenceResult") });
	TestEqual(TEXT("GCommandRejectCodeCount"), GCommandRejectCodeCount, 4);
	TestEqual(TEXT("GHoldOutcomeCount"), GHoldOutcomeCount, 3);
	TestEqual(TEXT("GActionTypeCount"), GActionTypeCount, 5);
	TestEqual(TEXT("GCancelReasonCount"), GCancelReasonCount, 8);
	TestEqual(TEXT("GCommandKindCount"), GCommandKindCount, 8);
	TestEqual(TEXT("GCommandReplyKindCount"), GCommandReplyKindCount, 4);

	// Struct shapes.
	StructShape(*this, FClientCommandSeq::StaticStruct(), { { TEXT("PossessionEpoch"), TEXT("uint16") }, { TEXT("Seq"), TEXT("uint32") } });
	StructShape(*this, FHoldStatus::StaticStruct(),
		{ { TEXT("Seq"), TEXT("FClientCommandSeq") }, { TEXT("ActionType"), TEXT("EActionType") }, { TEXT("Outcome"), TEXT("EHoldOutcome") },
		  { TEXT("ServerStartTime"), TEXT("double") }, { TEXT("ServerEndTime"), TEXT("double") } });
	StructShape(*this, FCommandReply::StaticStruct(),
		{ { TEXT("Kind"), TEXT("ECommandReplyKind") }, { TEXT("Seq"), TEXT("FClientCommandSeq") }, { TEXT("Code"), TEXT("ECommandRejectCode") },
		  { TEXT("Hold"), TEXT("FHoldStatus") }, { TEXT("Strike"), TEXT("FStrikeResult") }, { TEXT("Defence"), TEXT("FDefenceResult") } });
	StructShape(*this, FStrikeResult::StaticStruct(),
		{ { TEXT("ClientCommandSeq"), TEXT("FClientCommandSeq") }, { TEXT("Result"), TEXT("EStrikeResult") }, { TEXT("TallyAfter"), TEXT("uint8") },
		  { TEXT("bExiled"), TEXT("bool") }, { TEXT("ContactServerTime"), TEXT("double") } });
	StructShape(*this, FDefenceResult::StaticStruct(), { { TEXT("Result"), TEXT("EDefenceResult") } });
	StructShape(*this, FNetPawnCommand::StaticStruct(),
		{ { TEXT("Kind"), TEXT("ECommandKind") }, { TEXT("Seq"), TEXT("FClientCommandSeq") }, { TEXT("Args"), TEXT("FNetCommandArgs") } });
	StructShape(*this, FNetCommandArgs::StaticStruct(),
		{ { TEXT("CourtFixtureId"), TEXT("FCourtFixtureId") }, { TEXT("BushId"), TEXT("uint16") }, { TEXT("SideIndex"), TEXT("uint8") },
		  { TEXT("WorldPoint"), TEXT("FVector_NetQuantize10") }, { TEXT("PawnRefGuidValue"), TEXT("uint64") },
		  { TEXT("TraversalPointHint"), TEXT("uint16") }, { TEXT("ClientTimestamp"), TEXT("float") } });

	// No UObject pointer, no cancel reason, no identity anywhere in the wire and reply structs.
	const UScriptStruct* WireStructs[] = { FClientCommandSeq::StaticStruct(), FHoldStatus::StaticStruct(), FCommandReply::StaticStruct(),
		FNetPawnCommand::StaticStruct(), FNetCommandArgs::StaticStruct(), FCourtFixtureId::StaticStruct(), FStrikeResult::StaticStruct(),
		FDefenceResult::StaticStruct() };
	for (const UScriptStruct* Struct : WireStructs)
	{
		const TArray<FString> ObjectFields = StealthIdentityCheck::FindObjectFields(Struct);
		TestTrue(FString::Printf(TEXT("%s has no UObject-pointer field (%s)"), *Struct->GetName(), *FString::Join(ObjectFields, TEXT(", "))), ObjectFields.IsEmpty());
		TestFalse(FString::Printf(TEXT("%s has no ECancelReason field"), *Struct->GetName()), HasCancelReasonField(Struct));
		const TArray<FString> Identity = StealthIdentityCheck::FindStructViolations(Struct);
		TestTrue(FString::Printf(TEXT("%s has no identity-named field (%s)"), *Struct->GetName(), *FString::Join(Identity, TEXT(", "))), Identity.IsEmpty());
	}

	// Value round trips, boundaries included.
	FClientCommandSeq Zero;
	FClientCommandSeq Max;
	Max.PossessionEpoch = MAX_uint16;
	Max.Seq = MAX_uint32;
	TestTrue(TEXT("Seq 0 round-trips"), RoundTrip(Zero) == Zero);
	TestTrue(TEXT("uint16 / uint32 maxima round-trip"), RoundTrip(Max) == Max);

	FHoldStatus Hold;
	Hold.Seq = Max;
	Hold.ActionType = EActionType::Prepare;
	Hold.Outcome = EHoldOutcome::Cancelled;
	Hold.ServerStartTime = 12.5;
	Hold.ServerEndTime = 13.25;
	TestTrue(TEXT("FHoldStatus round-trips"), RoundTrip(Hold) == Hold);
	TestTrue(TEXT("FCommandReply (HoldStatus) round-trips"), RoundTrip(FCommandReply::MakeHoldStatus(Hold)) == FCommandReply::MakeHoldStatus(Hold));
	const FCommandReply Rejected = FCommandReply::MakeRejected(Max, ECommandRejectCode::OutOfReach);
	TestTrue(TEXT("FCommandReply (Rejected) round-trips"), RoundTrip(Rejected) == Rejected);

	FNetPawnCommand Command;
	Command.Kind = ECommandKind::Attack;
	Command.Seq = Max;
	Command.Args.WorldPoint = FVector_NetQuantize10(FVector(100.0, -250.5, 30.0));
	Command.Args.SetPawnRefGuid(FNetworkGUID::CreateFromIndex(42, false));
	TestTrue(TEXT("FNetPawnCommand round-trips"), RoundTrip(Command) == Command);

	FNetPawnCommand NoRef;
	NoRef.Kind = ECommandKind::HideExit;
	NoRef.Args.SideIndex = 3;
	const FNetPawnCommand NoRefBack = RoundTrip(NoRef);
	TestTrue(TEXT("an invalid PawnRefGuid round-trips as invalid"), NoRefBack == NoRef && !NoRefBack.Args.GetPawnRefGuid().IsValid());
	TestTrue(TEXT("the GUID value survives"), Command.Args.GetPawnRefGuid() == FNetworkGUID::CreateFromIndex(42, false));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
