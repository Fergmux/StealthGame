// Story nkp-013 (production/epics/npc-kill-penalty-informants/story-013-starvation-retries-and-widening.md), AC2 (derived,
// ADR-0019 section 4.3): GIVEN a fake LSC script of Deferred, a pick the pre-check excludes, Deferred, None and Picked,
// WHEN the request retries, THEN every pick carries the same RespawnRequestSeq; each Deferred and excluded retry is due at
// ServerNow + the next-frame offset, strictly after the drain's start and outside the queue's same-drain stamp window
// (never inline), and leaves PickStartServerTime and the stored widening level unchanged (the excluded pick's own level is
// not stored); the exclusion set survives consecutive next-frame retries and is cleared only when the regular
// EmergenceRetrySec retry (scheduled by None) fires. Edge: a new request (the informant killed again) gets a new seq.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Server/Queue/StealthResolutionQueueCore.h"
#include "Tests/Penalty/PenaltyTestRig.h"

namespace PenaltyRetryBookkeepingTestData
{
	inline constexpr uint64 PawnA = 700;
	inline constexpr uint64 PawnB = 701;
	/** The level the excluded pick reports: never stored (only None and committed picks are). */
	inline constexpr uint8 ExcludedPickLevel = 2;
	inline constexpr uint8 NoneLevel = 1;

	FPenaltyRespawnPick Answer(EPenaltyPickStatus Status, uint8 Level = 0, uint64 Pawn = 0)
	{
		FPenaltyRespawnPick Pick;
		Pick.Status = Status;
		Pick.WideningLevel = Level;
		Pick.Pawn = FPenaltyPawnKey(Pawn);
		return Pick;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPenaltyRetryBookkeepingTest, "StealthGame.Penalty.RetryBookkeeping",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPenaltyRetryBookkeepingTest::RunTest(const FString& Parameters)
{
	using namespace StealthPenaltyTest;
	using namespace PenaltyRetryBookkeepingTestData;

	// Arrange: P exiled (StrikeLimit) and awaiting; the scripted LSC and a pre-check that rejects the first candidate.
	FRig Rig(nullptr, false, /*bWithRespawnGateway*/ true);
	Rig.Roster.States.Add(P, EParticipantState::Informant);
	Rig.Roster.Teams.Add(P, ERosterTeam::Team0);
	Rig.Gateway.Picks = { Answer(EPenaltyPickStatus::Deferred), Answer(EPenaltyPickStatus::Picked, ExcludedPickLevel, PawnA), Answer(EPenaltyPickStatus::Deferred),
		Answer(EPenaltyPickStatus::None, NoneLevel), Answer(EPenaltyPickStatus::Picked, NoneLevel, PawnB) };
	Rig.Gateway.Eligible = { false };
	Rig.Core->OnInformantAwaitingBody(P, ERespawnCause::StrikeLimit, Rig.Clock.GetServerTimeSeconds());
	const double Due = Rig.Core->GetDueServerTime(P).Get(0.0);
	const double RetrySec = Rig.Core->GetLatchedParams().EmergenceRetrySec;
	const double NextFrame = FPenaltyCore::NextFrameRetryOffsetSec();

	struct FAfter
	{
		double FireTime = 0.0;
		double RetryDue = 0.0;
		double PickStart = 0.0;
		uint8 Level = 0;
		int32 Exclusions = 0;
	};
	TArray<FAfter> After;

	// Act: fire the item and the four retries (the fifth pick commits).
	for (int32 Fire = 0; Fire < 4; ++Fire)
	{
		Rig.Fire(P);
		const TArray<FRecordingRespawns::FCall> Pending = Rig.Respawns.PendingFor(P);
		const FPenaltyRecord* Record = Rig.Core->FindRecord(P);
		FAfter& A = After.AddDefaulted_GetRef();
		A.FireTime = Rig.Clock.GetServerTimeSeconds();
		A.RetryDue = Pending.Num() == 1 ? Pending[0].DueServerTime : -1.0;
		A.PickStart = Record != nullptr ? Record->PickStartServerTime : -1.0;
		A.Level = Record != nullptr ? Record->LastWideningLevel : 255;
		A.Exclusions = Record != nullptr ? Record->RequestExclusions.Num() : -1;
	}
	Rig.Fire(P);

	// Assert: one request, five picks.
	const TArray<FFakeRespawnGateway::FCall> Picks = Rig.Gateway.Named(TEXT("Pick"));
	TestEqual(TEXT("five picks"), Picks.Num(), 5);
	bool bSameSeq = Picks.Num() == 5 && Picks[0].RequestSeq != 0;
	for (const FFakeRespawnGateway::FCall& Pick : Picks)
	{
		bSameSeq &= Pick.RequestSeq == Picks[0].RequestSeq;
	}
	TestTrue(TEXT("every retry reuses the same RespawnRequestSeq"), bSameSeq);

	// Deferred, excluded, Deferred: next-frame retries, strictly after the drain start and outside its stamp window.
	const TCHAR* const Kinds[] = { TEXT("Deferred"), TEXT("excluded"), TEXT("Deferred again") };
	for (int32 Index = 0; Index < 3 && After.IsValidIndex(Index); ++Index)
	{
		const FAfter& A = After[Index];
		TestTrue(FString::Printf(TEXT("%s: retry at ServerNow + the next-frame offset"), Kinds[Index]), FMath::IsNearlyEqual(A.RetryDue, A.FireTime + NextFrame, 1e-9));
		TestTrue(FString::Printf(TEXT("%s: due strictly after the drain start plus the queue's same-drain window (never inline)"), Kinds[Index]),
			A.RetryDue > A.FireTime + FResolutionQueueCore::DueEpsilonSec);
		TestTrue(FString::Printf(TEXT("%s: PickStartServerTime unchanged (T_due)"), Kinds[Index]), FMath::IsNearlyEqual(A.PickStart, Due, 1e-9));
		TestEqual(FString::Printf(TEXT("%s: the stored level unchanged (0)"), Kinds[Index]), static_cast<int32>(A.Level), 0);
	}
	TestTrue(TEXT("Deferred: nothing excluded"), After.IsValidIndex(0) && After[0].Exclusions == 0);
	TestTrue(TEXT("excluded: the rejected candidate joins the exclusion set"), After.IsValidIndex(1) && After[1].Exclusions == 1);
	TestTrue(TEXT("the exclusion set survives the next Deferred retry"), After.IsValidIndex(2) && After[2].Exclusions == 1);

	// None: the regular retry; its level is stored; the exclusion set clears when it fires.
	TestTrue(TEXT("None: retry at ServerNow + EmergenceRetrySec"), After.IsValidIndex(3) && FMath::IsNearlyEqual(After[3].RetryDue, After[3].FireTime + RetrySec, 1e-9));
	TestTrue(TEXT("None: PickStartServerTime still T_due"), After.IsValidIndex(3) && FMath::IsNearlyEqual(After[3].PickStart, Due, 1e-9));
	TestTrue(TEXT("None: the reported level is stored"), After.IsValidIndex(3) && After[3].Level == NoneLevel);
	TestTrue(TEXT("the excluded pawn is in Exclude for the picks of the next-frame retries"), Picks.Num() == 5 && Picks[1].Exclude.Num() == 0
		&& Picks[2].Exclude.Contains(FPenaltyPawnKey(PawnA)) && Picks[3].Exclude.Contains(FPenaltyPawnKey(PawnA)));
	TestTrue(TEXT("the regular retry's pick has an empty Exclude (the set cleared when it fired)"), Picks.Num() == 5 && Picks[4].Exclude.Num() == 0);
	TestTrue(TEXT("committed: Embodied in PawnB"), Rig.Core->GetLifeState(P) == EInformantLifeState::Embodied && Rig.Gateway.Named(TEXT("Takeover")).Num() == 1
		&& Rig.Gateway.Named(TEXT("Takeover"))[0].Pawn == FPenaltyPawnKey(PawnB));
	const TArray<FPenaltyInformantRespawnRow> Rows = Rig.Rows<FPenaltyInformantRespawnRow>();
	TestTrue(TEXT("the row's level is the committed request's level (the excluded pick's level 2 never counted)"), Rows.Num() == 1 && Rows[0].WideningLevel == NoneLevel);

	// Edge: a new request (the informant body killed) gets a new RespawnRequestSeq.
	Rig.Roster.LivePawns.Add(P);
	Rig.KillInformant(P, Q, PawnB, Rig.Clock.GetServerTimeSeconds());
	Rig.Fire(P);
	const TArray<FFakeRespawnGateway::FCall> AllPicks = Rig.Gateway.Named(TEXT("Pick"));
	TestTrue(TEXT("edge: the next request's pick carries a new seq"), AllPicks.Num() == 6 && AllPicks[5].RequestSeq != 0 && AllPicks[5].RequestSeq != Picks[0].RequestSeq);
	return true;
}

#endif
