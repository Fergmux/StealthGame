// Story nkp-013 (production/epics/npc-kill-penalty-informants/story-013-starvation-retries-and-widening.md), AC1 / AC-NKP-14
// (PEN-R10; ADR-0019 section 4.3): GIVEN a fake clock and a fake LSC gateway with no eligible pawn (its widening level
// follows the request's own clock: 0, then 1 from RespawnStarvationSec, then 2 from twice that), WHEN a respawn is due and the
// clock runs to 60 s past T_due, THEN PickRespawnPawn is retried every EmergenceRetrySec with the same RequestSeq; Penalty
// stores the level the fake reports (0, 1 at exactly 10.0 s, 2 at exactly 20.0 s) and never lowers it; past level 2 it keeps
// waiting (one pending retry, no RespawnCancelled row: no timeout and no view-only fallback); when the fake finally returns a
// pawn the InformantRespawn row carries WideningLevel 2; the only body is the picked crowd pawn (no spawn).
// Round's LateJoinTakeoverTimeoutSec view-only fallback is not built in Round (no such symbol exists); Penalty has no path
// to it: the request never ends without a body while the participant waits, and the Penalty sources name no spawn, no
// late-join fallback and no RespawnStarved row (LSC's row, asserted once per request by LSC's integration run, never
// Penalty's). Negative control: a seeded spawn call is caught by the same scan.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Server/Testing/StealthNegativeControl.h"
#include "Tests/Penalty/PenaltySourceScan.h"
#include "Tests/Penalty/PenaltyTestRig.h"

namespace PenaltyStarvationWideningTestData
{
	/** The fake LSC's RespawnStarvationSec (LSC R5b.5's 10 s; LSC owns the key, the fake owns this fixture value). */
	inline constexpr double StarvationSec = 10.0;
	/** The fake LSC answers None until this long after the request's first pick, then Picked(PawnX). */
	inline constexpr double StarvedForSec = 60.0;
	inline constexpr uint64 PawnX = 700;
	/** Safety bound on the drive loop (61 picks are expected). */
	inline constexpr int32 MaxFires = 200;

	/** The fake LSC's widening: level 0 until StarvationSec after the first pick, 1 until twice that, then 2. */
	uint8 LevelAt(double Elapsed)
	{
		return Elapsed >= 2.0 * StarvationSec ? 2 : (Elapsed >= StarvationSec ? 1 : 0);
	}

	FPenaltyRespawnPick Answer(EPenaltyPickStatus Status, uint8 Level, uint64 Pawn = 0)
	{
		FPenaltyRespawnPick Pick;
		Pick.Status = Status;
		Pick.WideningLevel = Level;
		Pick.Pawn = FPenaltyPawnKey(Pawn);
		return Pick;
	}

	/** Identifiers no Penalty source may use: a spawn, the prototype / harness respawn, Round's late-join fallback, LSC's row. */
	const TCHAR* const ForbiddenPrefixes[] = { TEXT("SpawnActor"), TEXT("SpawnInformant"), TEXT("SpawnHarness"), TEXT("SpawnPlayerPawn"),
		TEXT("LateJoin"), TEXT("RespawnStarved") };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPenaltyStarvationWideningTest, "StealthGame.Penalty.StarvationWidening",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPenaltyStarvationWideningTest::RunTest(const FString& Parameters)
{
	using namespace StealthPenaltyTest;
	using namespace PenaltyStarvationWideningTestData;

	{
		// Arrange: P exiled at 100.0 (StrikeLimit: no fall), so T_due = 100 + InformantRespawnDelaySec; a fake LSC on the
		// rig's clock that starves the request for 60 s.
		FRig Rig(nullptr, false, /*bWithRespawnGateway*/ true);
		Rig.Roster.States.Add(P, EParticipantState::Informant);
		Rig.Roster.Teams.Add(P, ERosterTeam::Team0);
		const double RetrySec = Rig.Core->GetLatchedParams().EmergenceRetrySec;
		TestTrue(TEXT("fixture: EmergenceRetrySec is positive"), RetrySec > 0.0);
		struct FSeen
		{
			double Time = 0.0;
			uint32 Seq = 0;
			uint8 Level = 0;
		};
		TArray<FSeen> Seen;
		double FirstPick = -1.0;
		Rig.Gateway.PickFn = [&Rig, &Seen, &FirstPick](uint32 RequestSeq, TConstArrayView<FPenaltyPawnKey>)
		{
			const double Now = Rig.Clock.GetServerTimeSeconds();
			FirstPick = FirstPick < 0.0 ? Now : FirstPick;
			const double Elapsed = Now - FirstPick;
			Seen.Add({ Now, RequestSeq, LevelAt(Elapsed) });
			return Elapsed >= StarvedForSec ? Answer(EPenaltyPickStatus::Picked, LevelAt(Elapsed), PawnX) : Answer(EPenaltyPickStatus::None, LevelAt(Elapsed));
		};
		Rig.Core->OnInformantAwaitingBody(P, ERespawnCause::StrikeLimit, Rig.Clock.GetServerTimeSeconds());
		const double Due = Rig.Core->GetDueServerTime(P).Get(0.0);

		// Act: fire the item, then every retry, until P is embodied; after each pick read what Penalty stored.
		TArray<uint8> Stored;
		bool bAlwaysWaitingBeforeTheBody = true;
		bool bPickStartFixed = true;
		for (int32 Fire = 0; Fire < MaxFires && Rig.Core->GetLifeState(P) != EInformantLifeState::Embodied; ++Fire)
		{
			Rig.Fire(P);
			const FPenaltyRecord* Record = Rig.Core->FindRecord(P);
			if (Rig.Core->GetLifeState(P) == EInformantLifeState::Embodied || Record == nullptr)
			{
				break;
			}
			Stored.Add(Record->LastWideningLevel);
			bAlwaysWaitingBeforeTheBody &= Record->LifeState == EInformantLifeState::Picking && Rig.Respawns.PendingFor(P).Num() == 1;
			bPickStartFixed &= FMath::IsNearlyEqual(Record->PickStartServerTime, Due, 1e-9);
		}

		// Assert: the cadence and the request.
		const int32 ExpectedPicks = static_cast<int32>(StarvedForSec / RetrySec) + 1;
		TestEqual(TEXT("one pick at T_due and one per EmergenceRetrySec until the body (61)"), Seen.Num(), ExpectedPicks);
		bool bSameSeq = Seen.Num() > 0 && Seen[0].Seq != 0;
		bool bCadence = Seen.Num() > 0 && FMath::IsNearlyEqual(Seen[0].Time, Due, 1e-9);
		for (int32 Index = 1; Index < Seen.Num(); ++Index)
		{
			bSameSeq &= Seen[Index].Seq == Seen[0].Seq;
			bCadence &= FMath::IsNearlyEqual(Seen[Index].Time - Seen[Index - 1].Time, RetrySec, 1e-9);
		}
		TestTrue(TEXT("every pick carries the request's RespawnRequestSeq"), bSameSeq);
		TestTrue(TEXT("the first pick at T_due, then exactly EmergenceRetrySec apart"), bCadence);
		TestTrue(TEXT("PickStartServerTime = the first fire (T_due), unchanged by the retries"), bPickStartFixed);

		// Assert: the stored level follows the fake's (0, 1 at 10.0 s, 2 at 20.0 s), never decreasing.
		const int32 At9 = static_cast<int32>(9.0 / RetrySec);
		const int32 At10 = static_cast<int32>(StarvationSec / RetrySec);
		const int32 At19 = static_cast<int32>(19.0 / RetrySec);
		const int32 At20 = static_cast<int32>(2.0 * StarvationSec / RetrySec);
		TestTrue(TEXT("a stored level for every waiting pick"), Stored.Num() == ExpectedPicks - 1 && Stored.IsValidIndex(At20));
		if (Stored.IsValidIndex(At20))
		{
			TestEqual(TEXT("level 0 at 9 s"), static_cast<int32>(Stored[At9]), 0);
			TestEqual(TEXT("level 1 at exactly RespawnStarvationSec (10.0 s)"), static_cast<int32>(Stored[At10]), 1);
			TestEqual(TEXT("level 1 at 19 s"), static_cast<int32>(Stored[At19]), 1);
			TestEqual(TEXT("level 2 at exactly twice RespawnStarvationSec (20.0 s)"), static_cast<int32>(Stored[At20]), 2);
		}
		bool bMatchesFake = true;
		bool bMonotonic = true;
		for (int32 Index = 0; Index < Stored.Num(); ++Index)
		{
			bMatchesFake &= Seen.IsValidIndex(Index) && Stored[Index] == Seen[Index].Level;
			bMonotonic &= Index == 0 || Stored[Index] >= Stored[Index - 1];
		}
		TestTrue(TEXT("the stored level is the fake LSC's level at every pick"), bMatchesFake);
		TestTrue(TEXT("the stored level never decreases"), bMonotonic);

		// Assert: past level 2 the participant keeps waiting, then takes the picked crowd pawn; nothing else.
		TestTrue(TEXT("while starved: Picking with exactly one pending retry after every pick (no timeout, no view-only fallback)"), bAlwaysWaitingBeforeTheBody);
		TestTrue(TEXT("no RespawnCancelled row (the request never ended without a body)"), Rig.Rows<FPenaltyRespawnCancelledRow>().Num() == 0);
		TestTrue(TEXT("Embodied after the fake's pawn"), Rig.Core->GetLifeState(P) == EInformantLifeState::Embodied);
		const TArray<FFakeRespawnGateway::FCall> Takeovers = Rig.Gateway.Named(TEXT("Takeover"));
		TestTrue(TEXT("one takeover, of the pawn the pick returned (Penalty has no other way to a body: no spawn)"),
			Takeovers.Num() == 1 && Takeovers[0].Pawn == FPenaltyPawnKey(PawnX) && Takeovers[0].Participant == P);
		const TArray<FPenaltyInformantRespawnRow> Rows = Rig.Rows<FPenaltyInformantRespawnRow>();
		TestTrue(TEXT("one InformantRespawn row with WideningLevel 2"), Rows.Num() == 1 && Rows[0].WideningLevel == 2);
		TestTrue(TEXT("the InformantRespawn latency is the starved time (now - T_due = 60 s)"), Rows.Num() == 1 && FMath::IsNearlyEqual(Rows[0].PickLatencySec, StarvedForSec, 1e-6));
		bool bOnlyPenaltyRows = true;
		for (const FRoundLogEntry& Entry : Rig.Log.Entries)
		{
			bOnlyPenaltyRows &= Entry.Type == ERoundLogType::RespawnScheduled || Entry.Type == ERoundLogType::InformantRespawn;
		}
		TestTrue(TEXT("Penalty wrote only RespawnScheduled and InformantRespawn (RespawnStarved is LSC's row, never Penalty's)"), bOnlyPenaltyRows);
		TestEqual(TEXT("no fault"), Rig.Faults.GetRecords().Num(), 0);
	}

	// Edge: a lower level reported later never lowers the stored level (the row keeps the highest level reached).
	{
		FRig Rig(nullptr, false, true);
		Rig.Roster.States.Add(P, EParticipantState::Informant);
		Rig.Gateway.Picks = { Answer(EPenaltyPickStatus::None, 2), Answer(EPenaltyPickStatus::None, 1), Answer(EPenaltyPickStatus::Picked, 1, PawnX) };
		Rig.Core->OnInformantAwaitingBody(P, ERespawnCause::StrikeLimit, Rig.Clock.GetServerTimeSeconds());
		Rig.Fire(P);
		TestEqual(TEXT("edge: level 2 stored"), static_cast<int32>(Rig.Core->FindRecord(P)->LastWideningLevel), 2);
		Rig.Fire(P);
		TestEqual(TEXT("edge: a later level 1 keeps 2"), static_cast<int32>(Rig.Core->FindRecord(P)->LastWideningLevel), 2);
		Rig.Fire(P);
		const TArray<FPenaltyInformantRespawnRow> Rows = Rig.Rows<FPenaltyInformantRespawnRow>();
		TestTrue(TEXT("edge: the row carries the highest level reached (2)"), Rows.Num() == 1 && Rows[0].WideningLevel == 2);
	}

	// No spawn, no late-join fallback, no RespawnStarved row anywhere in the Penalty sources (comments and strings stripped).
	{
		using namespace StealthPenaltySourceScan;
		const TArray<FSource> Sources = LoadSources();
		TestTrue(TEXT("the Penalty sources were found"), Sources.Num() >= 5);
		TArray<FFinding> Findings;
		for (const FSource& Source : Sources)
		{
			FindIdentifierPrefixes(Source, ForbiddenPrefixes, Findings);
		}
		TestTrue(FString::Printf(TEXT("no spawn, late-join fallback or RespawnStarved in Penalty (%s)"), *Describe(Findings)), Findings.IsEmpty());

		FStealthNegativeControl Control(TEXT("SeededSpawnInPenalty"));
		FStealthNegativeControl::Report(*this, Control.Run([](FStealthCheckContext& Ctx)
		{
			FSource Seeded;
			Seeded.Path = TEXT("Private/Seeded.cpp");
			Seeded.Text = TEXT("void F() { World->SpawnActor<APawn>(Class, Where, Rot); }\n");
			TArray<FFinding> SeededFindings;
			FindIdentifierPrefixes(Seeded, ForbiddenPrefixes, SeededFindings);
			Ctx.TestTrue(TEXT("seeded spawn has no finding"), SeededFindings.IsEmpty());
		}));
	}
	return true;
}

#endif
