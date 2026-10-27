// Story nkp-003 (production/epics/npc-kill-penalty-informants/story-003-tally-rule-and-strike-result-supply.md), AC2 /
// AC-NKP-39 (PEN-R14.1, F1): L = 3, an Active attacker at k = 2 and tally 2; its next kill is (run a) an enemy informant:
// TallyAfter 3, bExiled false, k 2; (run b) an ungraced NPC: TallyAfter 3, bExiled true, k 3 and the exile due in that
// slot (the exile sequence itself is story nkp-005). The two FStrikeResult plates differ in bExiled only once
// ClientCommandSeq is normalised. Edge cases: a graced NPC kill at t = 2 gives tally 3 and no exile; with L = 1 on a test
// preset the first ungraced NPC kill exiles and an informant kill does not.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Tests/Penalty/PenaltyTestRig.h"

namespace PenaltyResidualBoundaryTestData
{
	/** The plate the resolver would send for this tally (same result, contact time and normalised sequence). */
	FStrikeResult Plate(const FStrikeTally& Tally)
	{
		FStrikeResult Out;
		Out.Result = EStrikeResult::WrongTarget;
		Out.TallyAfter = Tally.TallyAfter;
		Out.bExiled = Tally.bExiled;
		Out.ContactServerTime = 42.0;
		return Out;
	}

	/** A rig with L as given and P at k = L - 1, t = L - 1. */
	void ArrangeOneShort(StealthPenaltyTest::FRig& Rig)
	{
		for (int32 Index = 0; Index < Rig.Core->GetLatchedStrikeLimit() - 1; ++Index)
		{
			Rig.NpcKill(StealthPenaltyTest::P, 900 + Index);
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPenaltyResidualBoundaryTest, "StealthGame.Penalty.ResidualBoundary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPenaltyResidualBoundaryTest::RunTest(const FString& Parameters)
{
	using namespace StealthPenaltyTest;
	using namespace PenaltyResidualBoundaryTestData;

	// Run a: an enemy informant.
	FRig RunA;
	ArrangeOneShort(RunA);
	const int32 Limit = RunA.Core->GetLatchedStrikeLimit();
	const FStrikeTally TallyA = RunA.InformantKill(P);
	TestEqual(TEXT("run a: TallyAfter = L"), static_cast<int32>(TallyA.TallyAfter), Limit);
	TestFalse(TEXT("run a: bExiled false"), TallyA.bExiled);
	TestEqual(TEXT("run a: k = L - 1"), static_cast<int32>(RunA.Core->GetStrikeCount(P)), Limit - 1);

	// Run b: an ungraced NPC.
	FRig RunB;
	ArrangeOneShort(RunB);
	EPenaltyKillOutcome Outcome = EPenaltyKillOutcome::NoStrike;
	const FStrikeTally TallyB = RunB.Attack(P, FPenaltyKill{ Pawn(950), P, EVictimKind::Npc, EKillCause::Strike }, EStrikeResult::WrongTarget, &Outcome);
	TestEqual(TEXT("run b: TallyAfter = L"), static_cast<int32>(TallyB.TallyAfter), Limit);
	TestTrue(TEXT("run b: bExiled true"), TallyB.bExiled);
	TestEqual(TEXT("run b: k = L"), static_cast<int32>(RunB.Core->GetStrikeCount(P)), Limit);
	TestTrue(TEXT("run b: the exile is due in this slot"), Outcome == EPenaltyKillOutcome::StrikeAndExile);

	// The plates differ in bExiled only.
	FStrikeResult PlateA = Plate(TallyA);
	FStrikeResult PlateB = Plate(TallyB);
	TestTrue(TEXT("the plates differ"), PlateA != PlateB);
	PlateB.bExiled = PlateA.bExiled;
	TestTrue(TEXT("the plates are identical apart from bExiled"), PlateA == PlateB);

	// bExiled is slot-scoped: the next result of the same attacker carries false.
	const FStrikeTally Next = RunB.Attack(P, TOptional<FPenaltyKill>(), EStrikeResult::NoContact);
	TestFalse(TEXT("the next result never carries a stale bExiled"), Next.bExiled);

	// Edge: a graced NPC kill at t = L - 1 gives tally L and no exile.
	{
		FRig Rig;
		ArrangeOneShort(Rig);
		Rig.Core->RecordReleasedBody(Pawn(960), Rig.Clock.GetServerTimeSeconds());
		const FStrikeTally Graced = Rig.NpcKill(P, 960);
		TestEqual(TEXT("graced: tally L"), static_cast<int32>(Graced.TallyAfter), Limit);
		TestFalse(TEXT("graced: no exile"), Graced.bExiled);
		TestEqual(TEXT("graced: k = L - 1"), static_cast<int32>(Rig.Core->GetStrikeCount(P)), Limit - 1);
	}

	// Edge: L = 1 on a test preset (bTestAllowsStrikeLimitOne): the first ungraced NPC kill exiles, an informant kill does not.
	{
		const auto LimitOne = [](FFakeTuning& Tuning) { Tuning.Live.NpcKillStrikeLimit = 1; };
		FRig Informant(LimitOne);
		const FStrikeTally First = Informant.InformantKill(P);
		TestTrue(TEXT("L = 1: an informant kill gives tally 1 and no exile"), First.TallyAfter == 1 && !First.bExiled);
		FRig Npc(LimitOne);
		const FStrikeTally Exiling = Npc.NpcKill(P);
		TestTrue(TEXT("L = 1: the first NPC kill gives tally 1 and the exile"), Exiling.TallyAfter == 1 && Exiling.bExiled);
	}
	return true;
}

#endif
