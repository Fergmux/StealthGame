// Story psr-001 (production/epics/participant-state-roster/story-001-record-schema-whitelist.md):
// AC-PSR-01, the participant record carries exactly the fields of GDD R3 and no feature state (R1).
//
// Decisions recorded here (open point of the story):
// - Enumeration mechanism: an X-macro field list (STEALTH_ROSTER_RECORD_FIELDS in Roster/StealthRosterCore.h)
//   both declares the members and emits the name/type list (FParticipantRecord::GetFieldInfo); there is no
//   hand-kept second list next to the struct.
// - Roster-wide values MatchId, RoundEpoch and EventSeq are NOT record fields (they live on FRosterCore), so
//   they are on the deny side here. The pawn binding row is a record field (PawnBinding, the live pawn);
//   tombstones live in the core's binding table. The R6 owner-view cache is not part of the record.
// - Names follow R3 with UE naming applied: InRound is bInRound; Connection's DisconnectedAt is
//   DisconnectedAtMs (integer ms, R3) inside the composite Connection field.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Roster/StealthRosterCore.h"
#include "Server/Testing/StealthRoundTestConstants.h"

#include <type_traits>

namespace RecordSchemaTestData
{
	/** Expected-name fixture, hand-derived from GDD R3 (name, declared type; empty type = R3 gives none). */
	struct FExpectedField
	{
		const TCHAR* Name;
		const TCHAR* Type;
	};

	const FExpectedField ExpectedRecordFields[] = {
		{ TEXT("ParticipantId"), TEXT("FParticipantId") },
		{ TEXT("PublicHandle"), TEXT("uint32") },
		{ TEXT("IdentityKey"), TEXT("FIdentityKey") },
		{ TEXT("DisplayName"), TEXT("") },
		{ TEXT("Team"), TEXT("") },
		{ TEXT("Connection"), TEXT("FRosterConnection") },
		{ TEXT("AppearanceHistory"), TEXT("") },
		{ TEXT("bInRound"), TEXT("bool") },
		{ TEXT("State"), TEXT("") },
		{ TEXT("LastTransitionReason"), TEXT("") },
		{ TEXT("LastEventSeq"), TEXT("uint64") },
		{ TEXT("AssignedAppearanceId"), TEXT("") },
		{ TEXT("PawnBinding"), TEXT("") },
	};

	const FExpectedField ExpectedConnectionFields[] = {
		{ TEXT("Projection"), TEXT("") },
		{ TEXT("DisconnectedAtMs"), TEXT("int64") },
		{ TEXT("ConnectionGeneration"), TEXT("uint32") },
	};

	/** R1 deny-list fragments (case-insensitive) plus the roster-wide counters that must not sit on a record. */
	const TCHAR* const DenyFragments[] = {
		TEXT("strike"), TEXT("defence"), TEXT("defense"), TEXT("conceal"), TEXT("objective"),
		TEXT("MatchId"), RoundTestFixtures::HeaderFieldRoundEpoch, TEXT("Tally"), TEXT("RespawnDue"),
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRosterRecordSchemaWhitelistTest, "StealthGame.Roster.RecordSchemaWhitelist",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRosterRecordSchemaWhitelistTest::RunTest(const FString& Parameters)
{
	using namespace RecordSchemaTestData;

	const auto CheckFieldSet = [this](const TCHAR* Label, TConstArrayView<FRosterFieldInfo> Actual, TConstArrayView<FExpectedField> Expected)
	{
		TSet<FString> ActualNames;
		for (const FRosterFieldInfo& Field : Actual)
		{
			TestFalse(FString::Printf(TEXT("%s: field %s declared once"), Label, Field.Name), ActualNames.Contains(Field.Name));
			ActualNames.Add(Field.Name);
		}
		TSet<FString> ExpectedNames;
		for (const FExpectedField& Field : Expected)
		{
			ExpectedNames.Add(Field.Name);
		}
		for (const FString& Name : ActualNames)
		{
			TestTrue(FString::Printf(TEXT("%s: field %s is in the R3 whitelist"), Label, *Name), ExpectedNames.Contains(Name));
		}
		for (const FString& Name : ExpectedNames)
		{
			TestTrue(FString::Printf(TEXT("%s: R3 field %s exists"), Label, *Name), ActualNames.Contains(Name));
		}
		// Types where R3 gives them.
		for (const FExpectedField& Field : Expected)
		{
			if (*Field.Type == 0)
			{
				continue;
			}
			const FRosterFieldInfo* Found = Actual.FindByPredicate([&Field](const FRosterFieldInfo& F) { return FCString::Strcmp(F.Name, Field.Name) == 0; });
			if (Found)
			{
				TestEqual(FString::Printf(TEXT("%s: %s has R3 type %s"), Label, Field.Name, Field.Type), FString(Found->Type), FString(Field.Type));
			}
		}
		// R1: no feature state; roster-wide counters stay off the record.
		for (const FString& Name : ActualNames)
		{
			for (const TCHAR* Fragment : DenyFragments)
			{
				TestFalse(FString::Printf(TEXT("%s: field %s does not match deny fragment %s"), Label, *Name, Fragment),
					Name.Contains(Fragment, ESearchCase::IgnoreCase));
			}
		}
	};

	// Arrange + Act: enumerate through the record's own generated list.
	const TConstArrayView<FRosterFieldInfo> RecordFields = FParticipantRecord::GetFieldInfo();
	const TConstArrayView<FRosterFieldInfo> ConnectionFields = FRosterConnection::GetFieldInfo();

	// Assert: set equality, types, deny-list.
	TestEqual(TEXT("record field count equals R3"), RecordFields.Num(), static_cast<int32>(UE_ARRAY_COUNT(ExpectedRecordFields)));
	TestEqual(TEXT("connection field count equals R3"), ConnectionFields.Num(), static_cast<int32>(UE_ARRAY_COUNT(ExpectedConnectionFields)));
	CheckFieldSet(TEXT("record"), RecordFields, ExpectedRecordFields);
	CheckFieldSet(TEXT("connection"), ConnectionFields, ExpectedConnectionFields);

	// ParticipantId and IdentityKey are distinct struct types, not raw integers or strings (R2, R3).
	TestFalse(TEXT("FParticipantId and FIdentityKey are distinct types"), std::is_same_v<FParticipantId, FIdentityKey>);
	TestTrue(TEXT("FParticipantId is a class type"), std::is_class_v<FParticipantId>);
	TestTrue(TEXT("FIdentityKey is a class type"), std::is_class_v<FIdentityKey>);
	TestFalse(TEXT("FParticipantId is not convertible from a raw integer"), std::is_convertible_v<uint32, FParticipantId>);
	TestEqual(TEXT("FIdentityKey holds 128 bits"), static_cast<int32>(sizeof(FIdentityKey)), 16);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
