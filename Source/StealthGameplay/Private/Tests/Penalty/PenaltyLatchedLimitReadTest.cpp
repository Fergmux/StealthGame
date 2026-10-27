// Story nkp-001 (production/epics/npc-kill-penalty-informants/story-001-penalty-core-store-and-service-shell.md), AC2
// (derived from AC-NKP-40's first half, PEN-R4, TDC rule CR-11 Latching): with L = 3 latched at Preparing, a live change
// of NpcKillStrikeLimit to 4 during Active leaves Penalty's latched-limit read at 3 for the rest of the round (the exile
// is still due on the third strike; zero reads of the live knob during the round) and the next round's Preparing latches
// 4; and a source search finds no literal limit in the Penalty sources (negative control: a seeded literal is found).
// The exile consequence (story nkp-005 AC4) is not asserted here.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Server/Testing/StealthNegativeControl.h"
#include "Tests/Penalty/PenaltySourceScan.h"
#include "Tests/Penalty/PenaltyTestRig.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPenaltyLatchedLimitReadTest, "StealthGame.Penalty.LatchedLimitRead",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPenaltyLatchedLimitReadTest::RunTest(const FString& Parameters)
{
	using namespace StealthPenaltyTest;
	using namespace StealthPenaltySourceScan;

	const int32 Latched = DefaultLimit();
	const int32 Changed = Latched + 1;

	// Arrange: L latched at Preparing.
	FRig Rig;
	Rig.Core->OnRoundPreparing();
	const int32 ReadsAtPreparing = Rig.Tuning.Reads;
	TestEqual(TEXT("L latched at Preparing"), Rig.Core->GetLatchedStrikeLimit(), Latched);

	// Act: the live value changes during Active; the round plays on to the latched limit.
	Rig.Tuning.Live.NpcKillStrikeLimit = Changed;
	bool bExiledAtLatched = false;
	for (int32 Strike = 1; Strike <= Latched; ++Strike)
	{
		const FStrikeTally Tally = Rig.NpcKill(P, 900 + Strike);
		TestEqual(FString::Printf(TEXT("strike %d: the latched read is still %d"), Strike, Latched), Rig.Core->GetLatchedStrikeLimit(), Latched);
		bExiledAtLatched = Tally.bExiled;
		TestEqual(FString::Printf(TEXT("strike %d: bExiled only on the latched L-th strike"), Strike), Tally.bExiled, Strike == Latched);
	}

	// Assert: the live knob was never read during the round.
	TestTrue(TEXT("the exile is due on the latched limit's strike, not the live one"), bExiledAtLatched);
	TestEqual(TEXT("zero reads of the live knob during the round"), Rig.Tuning.Reads, ReadsAtPreparing);

	// Act + Assert: the next round's Preparing latches the new value.
	Rig.Core->OnRosterReset(FixtureEpoch + 1);
	TestEqual(TEXT("the reset alone does not re-latch"), Rig.Core->GetLatchedStrikeLimit(), Latched);
	Rig.Core->OnRoundPreparing();
	TestEqual(TEXT("the next Preparing latches the new value"), Rig.Core->GetLatchedStrikeLimit(), Changed);
	TestEqual(TEXT("one read per Preparing"), Rig.Tuning.Reads, ReadsAtPreparing + 1);

	// No literal limit in the Penalty sources.
	const TArray<FSource> Sources = LoadSources();
	TestTrue(TEXT("the Penalty sources were found"), Sources.Num() >= 6);
	TArray<FFinding> Findings;
	for (const FSource& Source : Sources)
	{
		FindLiteralLimits(Source, Findings);
	}
	TestTrue(FString::Printf(TEXT("no literal limit in Penalty sources (%s)"), *Describe(Findings)), Findings.IsEmpty());

	// Negative control: a seeded literal limit and the prototype setting are both found.
	FStealthNegativeControl Control(TEXT("SeededLiteralLimit"));
	FStealthNegativeControl::Report(*this, Control.Run([](FStealthCheckContext& Ctx)
	{
		TArray<FFinding> Seeded;
		FindLiteralLimits(FSource{ TEXT("Seeded.cpp"), TEXT("const int32 StrikeLimit = 3; // the user-locked value\nif (k >= Settings->NpcKillLimit) {}\n") }, Seeded);
		Ctx.TestTrue(TEXT("seeded source has no literal limit"), Seeded.IsEmpty());
	}));
	return true;
}

#endif
