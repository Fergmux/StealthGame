// Story nkp-005 (production/epics/npc-kill-penalty-informants/story-005-exile-sequence-in-attack-slot.md), AC1 /
// AC-NKP-05: GIVEN an Active participant with L - 1 strikes, WHEN its next ungraced NPC kill resolves, THEN in the same
// queue slot the gateways record, in order, RequestTransition(P, Informant, StrikeLimit, epoch) once, ReleaseCard(body),
// AdoptReleasedPawn(body, {lockout end, bExitAtNextScheduledSlot}) and RequestHandOver(body, Ai, epoch); ReleasedBodies[body]
// is the slot time and ExcludePawn the body; the body is never Retired; bExiled is true in that Attack's FStrikeResult and in
// no other; StrikeCounted carries that slot's key; nothing schedules a respawn from this stack (Round's
// OnInformantAwaitingBody does, story nkp-009: the RespawnScheduled half of the criterion is asserted there).
// Parameterised L = 2..5 (the latched value from the fake tuning), plus L = 1 from a test preset: TDC's F-1 with
// bTestAllowsStrikeLimitOne keeps 1 (the flag lowers the hard minimum), without it 1 is clamped to 2.
// Edges: a graced kill at k = L - 1 does not exile; a rejected transition faults once and runs nothing else.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Server/StealthTuningPipelineCore.h"
#include "Server/StealthTuningSchema.h"
#include "Tests/Penalty/PenaltyTestRig.h"

namespace PenaltyExileOnLthNpcKillTestData
{
	/** P's body (the pawn the exile releases). */
	inline constexpr uint64 BodyOfP = 77;

	/** L = 1 through TDC's own F-1 on a bTestAllowsStrikeLimitOne preset (0 when the row is missing). */
	int32 TestPresetLimitOne(bool bFlag)
	{
		for (const FTuningSchemaRow& Row : GetTuningSchema())
		{
			if (FCStringAnsi::Strcmp(Row.Key, TuningStrikeLimitKey) == 0)
			{
				FTuningF1Options Options;
				Options.bTestAllowsStrikeLimitOne = bFlag;
				ETuningF1Action Action = ETuningF1Action::Kept;
				return static_cast<int32>(FTuningPipelineCore::ApplyF1(Row, 1.0, Options, Action));
			}
		}
		return 0;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPenaltyExileOnLthNpcKillTest, "StealthGame.Penalty.ExileOnLthNpcKill",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPenaltyExileOnLthNpcKillTest::RunTest(const FString& Parameters)
{
	using namespace StealthPenaltyTest;
	using namespace PenaltyExileOnLthNpcKillTestData;

	const int32 LimitOne = TestPresetLimitOne(true);
	TestEqual(TEXT("the test preset (bTestAllowsStrikeLimitOne) keeps L = 1"), LimitOne, 1);
	TestTrue(TEXT("without the flag F-1 never gives L = 1"), TestPresetLimitOne(false) > 1);

	for (const int32 Limit : { LimitOne, 2, 3, 4, 5 })
	{
		const FString Case = FString::Printf(TEXT("L = %d"), Limit);

		// Arrange: L latched (the fake tuning's value at construction), the gateways recorded, P at k = L - 1.
		FRig Rig([Limit](FFakeTuning& Tuning) { Tuning.Live.NpcKillStrikeLimit = Limit; }, /*bWithGateways*/ true);
		TArray<FStrikeTally> Earlier;
		for (int32 Index = 0; Index < Limit - 1; ++Index)
		{
			Earlier.Add(Rig.NpcKillWithBody(P, Pawn(BodyOfP), 900 + Index));
		}
		TestEqual(Case + TEXT(": arranged k = L - 1"), static_cast<int32>(Rig.Core->GetStrikeCount(P)), Limit - 1);
		TestEqual(Case + TEXT(": nothing called before the L-th kill"), Rig.Bodies.Calls.Num(), 0);

		// Act: the L-th ungraced NPC kill, then a later Attack by Q (a whiff) in its own slot.
		EPenaltyKillOutcome Outcome = EPenaltyKillOutcome::NoStrike;
		FQueueKey ExileKey;
		double ExileTime = 0.0;
		const FStrikeTally Exiling = Rig.NpcKillWithBody(P, Pawn(BodyOfP), 990, &Outcome, &ExileKey, &ExileTime);
		const FStrikeTally Later = Rig.Attack(Q, TOptional<FPenaltyKill>(), EStrikeResult::NoContact); // a whiff: no kill, no strike (any L)

		// Assert: the four calls, in order, once each, all inside the exile's slot.
		TestTrue(Case + TEXT(": StrikeAndExile"), Outcome == EPenaltyKillOutcome::StrikeAndExile);
		TestEqual(Case + TEXT(": call order"), Rig.Bodies.Sequence(), FString(TEXT("RequestTransition ReleaseCard AdoptReleasedPawn RequestHandOver")));
		const double LockoutEnd = ExileTime + FRig::LockoutSec();
		if (TestEqual(Case + TEXT(": four calls"), Rig.Bodies.Calls.Num(), 4))
		{
			const TArray<FRecordingBodies::FCall>& C = Rig.Bodies.Calls;
			TestTrue(Case + TEXT(": RequestTransition(P, StrikeLimit) with the round epoch"), C[0].Participant == P && C[0].Epoch == FixtureEpoch);
			TestTrue(Case + TEXT(": ReleaseCard(P's body)"), C[1].Body == Pawn(BodyOfP));
			TestTrue(Case + TEXT(": AdoptReleasedPawn(body, lockout end, exit at the next scheduled slot)"), C[2].Body == Pawn(BodyOfP)
				&& FMath::IsNearlyEqual(C[2].Time, LockoutEnd, 1e-9) && C[2].bExit);
			TestTrue(Case + TEXT(": RequestHandOver(body, Ai, epoch) not before the lockout end"), C[3].Body == Pawn(BodyOfP) && C[3].Epoch == FixtureEpoch
				&& FMath::IsNearlyEqual(C[3].Time, LockoutEnd, 1e-9));
			for (const FRecordingBodies::FCall& Call : C)
			{
				TestTrue(Case + TEXT(": ") + Call.Name + TEXT(" ran inside the exile's slot"), Call.bInSlot && Call.SlotKey == ExileKey);
			}
		}
		TestEqual(Case + TEXT(": the body is never Retired"), Rig.Bodies.Count(TEXT("RetireBinding")), 0);
		const TOptional<double> Released = Rig.Core->FindReleaseTime(Pawn(BodyOfP));
		TestTrue(Case + TEXT(": ReleasedBodies[body] = the slot time"), Released.IsSet() && FMath::IsNearlyEqual(Released.GetValue(), ExileTime, 1e-9));
		const FPenaltyRecord* Record = Rig.Core->FindRecord(P);
		TestTrue(Case + TEXT(": ExcludePawn = the body"), Record != nullptr && Record->ExcludePawn == Pawn(BodyOfP));
		TestTrue(Case + TEXT(": bExiled in the exiling Attack's result"), Exiling.bExiled);
		TestFalse(Case + TEXT(": bExiled in no later result"), Later.bExiled);
		for (const FStrikeTally& Tally : Earlier)
		{
			TestFalse(Case + TEXT(": bExiled in no earlier result"), Tally.bExiled);
		}
		const TArray<FRoundLogEntry> Counted = Rig.Log.OfType(ERoundLogType::StrikeCounted);
		TestTrue(Case + TEXT(": a StrikeCounted row with the exile's slot key"), Counted.ContainsByPredicate([ExileKey](const FRoundLogEntry& E) { return E.Key == ExileKey; }));
		TestEqual(Case + TEXT(": no respawn is scheduled from the exile's stack (Round's provider call does it)"), Rig.Respawns.Calls.Num(), 0);
		TestEqual(Case + TEXT(": no fault"), Rig.Faults.GetRecords().Num(), 0);
	}

	// Edge: a graced kill at k = L - 1 does not exile (the victim is a body released within the grace).
	{
		FRig Rig(nullptr, /*bWithGateways*/ true);
		const int32 Limit = Rig.Core->GetLatchedStrikeLimit();
		for (int32 Index = 0; Index < Limit - 1; ++Index)
		{
			Rig.NpcKillWithBody(P, Pawn(BodyOfP), 900 + Index);
		}
		Rig.Core->RecordReleasedBody(Pawn(555), Rig.Clock.GetServerTimeSeconds());
		EPenaltyKillOutcome Outcome = EPenaltyKillOutcome::NoStrike;
		const FStrikeTally Graced = Rig.NpcKillWithBody(P, Pawn(BodyOfP), 555, &Outcome);
		TestTrue(TEXT("graced at k = L - 1: no strike, no exile"), Outcome == EPenaltyKillOutcome::Graced && !Graced.bExiled);
		TestEqual(TEXT("graced at k = L - 1: no exile call"), Rig.Bodies.Calls.Num(), 0);
	}

	// Edge: a rejected transition faults once and runs nothing else (no release, no bExiled).
	{
		FRig Rig(nullptr, /*bWithGateways*/ true);
		Rig.Bodies.TransitionAnswer = ERosterResult::Rejected_StaleEpoch;
		const int32 Limit = Rig.Core->GetLatchedStrikeLimit();
		FStrikeTally Last;
		for (int32 Index = 0; Index < Limit; ++Index)
		{
			Last = Rig.NpcKillWithBody(P, Pawn(BodyOfP), 900 + Index);
		}
		TestEqual(TEXT("rejected: only the transition was asked"), Rig.Bodies.Sequence(), FString(TEXT("RequestTransition")));
		TestFalse(TEXT("rejected: no bExiled"), Last.bExiled);
		TestFalse(TEXT("rejected: no released body"), Rig.Core->FindReleaseTime(Pawn(BodyOfP)).IsSet());
		TestTrue(TEXT("rejected: one ExileTransitionRejected fault"), Rig.Faults.GetRecords().Num() == 1
			&& Rig.Faults.GetRecords()[0].Detail == PenaltyFaultDetail::ExileTransitionRejected);
	}
	return true;
}

#endif
