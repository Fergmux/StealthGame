// Story nkp-004 (production/epics/npc-kill-penalty-informants/story-004-event-log-rows.md), AC2 (derived, ADR-0019 section 7,
// PEN-R17), schema half: each Penalty row's entry type equals the PEN-R17 table (StrikeCounted, RespawnScheduled,
// InformantRespawn, RespawnCancelled, PawnLost) and its release class is PostRoundEligible; every write the stub sink sees
// happened inside a queue slot (the stub records the slot context at the write); a direct write outside a slot is refused
// (one QueueMisuse fault, nothing appended), including the core's own StrikeCounted when its strike ran outside a slot; a
// writer without a log faults DeadRegistration and never crashes. The lint half is StealthGame.Penalty.NoLoggingLint.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Server/EventLog/StealthRoundEventLog.h"
#include "Tests/Penalty/PenaltyTestRig.h"

namespace PenaltyEventLogRowSchemaTestData
{
	/** The PEN-R17 table as a fixture: row -> entry type, every one PostRoundEligible. */
	struct FTableRow
	{
		const TCHAR* Name;
		ERoundLogType Type;
	};
	const FTableRow Table[] = {
		{ TEXT("StrikeCounted"), ERoundLogType::StrikeCounted },
		{ TEXT("RespawnScheduled"), ERoundLogType::RespawnScheduled },
		{ TEXT("InformantRespawn"), ERoundLogType::InformantRespawn },
		{ TEXT("RespawnCancelled"), ERoundLogType::RespawnCancelled },
		{ TEXT("PawnLost"), ERoundLogType::PawnLost },
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPenaltyEventLogRowSchemaTest, "StealthGame.Penalty.EventLogRowSchema",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPenaltyEventLogRowSchemaTest::RunTest(const FString& Parameters)
{
	using namespace StealthPenaltyTest;
	using namespace PenaltyEventLogRowSchemaTestData;

	// Names and flags equal the table.
	const ERoundLogType RowTypes[] = { FPenaltyStrikeCountedRow::Type, FPenaltyRespawnScheduledRow::Type, FPenaltyInformantRespawnRow::Type,
		FPenaltyRespawnCancelledRow::Type, FPenaltyPawnLostRow::Type };
	for (int32 Index = 0; Index < static_cast<int32>(UE_ARRAY_COUNT(Table)); ++Index)
	{
		TestTrue(FString::Printf(TEXT("%s: the row's entry type"), Table[Index].Name), RowTypes[Index] == Table[Index].Type);
		TestTrue(FString::Printf(TEXT("%s: PostRoundEligible"), Table[Index].Name), FRoundEventLog::GetReleaseClass(Table[Index].Type) == ERoundLogReleaseClass::PostRoundEligible);
	}

	// Every write inside a slot: the real StrikeCounted path and the writer for the other four.
	{
		FRig Rig;
		Rig.NpcKill(P);
		FPenaltyEventLog Writer(&Rig.Log, &Rig.Slot, &Rig.Clock, &Rig.Faults);
		Writer.Write(FPenaltyRespawnScheduledRow{ P, ERespawnCause::Assassinated, 1.0, 16.0 }, FixtureEpoch);
		Writer.Write(FPenaltyInformantRespawnRow{ P, 0.5, 0 }, FixtureEpoch);
		Writer.Write(FPenaltyRespawnCancelledRow{ P, ERespawnCancelReason::RoundEnded }, FixtureEpoch);
		Writer.Write(FPenaltyPawnLostRow{ P, EPenaltyPawnLostCause::Destroyed }, FixtureEpoch);
		TestEqual(TEXT("five rows written"), Rig.Log.Entries.Num(), 5);
		TestFalse(TEXT("every write had a slot context"), Rig.Log.InSlotAtWrite.Contains(false));
		TestTrue(TEXT("every entry carries a stamped slot key"), !Rig.Log.Entries.ContainsByPredicate([](const FRoundLogEntry& E) { return !E.Key.IsSet(); }));
	}

	// A direct write outside a slot is refused.
	{
		FRig Rig;
		Rig.Slot.bInSlot = false;
		FPenaltyEventLog Writer(&Rig.Log, &Rig.Slot, &Rig.Clock, &Rig.Faults);
		TestFalse(TEXT("outside a slot: refused"), Writer.Write(FPenaltyPawnLostRow{ P, EPenaltyPawnLostCause::KillZ }, FixtureEpoch));
		// The core's own row when its strike ran outside a slot (a misuse; the strike itself still counts).
		Rig.Core->OnPawnKilled(FPenaltyKill{ Pawn(900), P, EVictimKind::Npc, EKillCause::Strike });
		Rig.Core->GetTallyForResult(P, EStrikeResult::WrongTarget, FRig::Seq(1));
		TestEqual(TEXT("outside a slot: nothing appended"), Rig.Log.Entries.Num(), 0);
		const FStealthFaultTeardownResult Teardown = Rig.Faults.EvaluateTeardown({ { EStealthFault::QueueMisuse } });
		TestTrue(FString::Printf(TEXT("outside a slot: QueueMisuse faults only (%s)"), *Teardown.Describe()), Teardown.IsClean());
		TestEqual(TEXT("outside a slot: one fault per refused write"), Rig.Faults.GetRecords().Num(), 2);
	}

	// No log writer: DeadRegistration, no crash.
	{
		FRig Rig;
		FPenaltyEventLog Writer(nullptr, &Rig.Slot, &Rig.Clock, &Rig.Faults);
		TestFalse(TEXT("no writer: refused"), Writer.Write(FPenaltyRespawnCancelledRow{ P, ERespawnCancelReason::Reset }, FixtureEpoch));
		TestTrue(TEXT("no writer: one DeadRegistration"), Rig.Faults.GetRecords().Num() == 1 && Rig.Faults.GetRecords()[0].Fault == EStealthFault::DeadRegistration
			&& Rig.Faults.GetRecords()[0].Detail == PenaltyFaultDetail::NoLogWriter);
	}
	return true;
}

#endif
