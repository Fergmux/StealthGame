// NPC-Kill Penalty story nkp-018 (production/epics/npc-kill-penalty-informants/story-018-tally-countdown-and-exile-card-ui.md):
// StealthGame.Penalty.HudViewModel. The pure view-models of the minimal production Penalty HUD elements against the GDD:
//   - UI-NKP-1 / HUD F6 at L = 3: "Wrong targets: {n}" for n = 0..4, filled = min(n, L), the danger triangle exactly at
//     n >= L - 1 while Active (also for n > L), never while frozen as an informant (ruling D4); the limit never written;
//   - UI-NKP-3 / HUD F3: the card (D11 wording "Informant" / "Too many wrong targets" / "You watch for your team now.")
//     hidden before its entrance, eased in, holding, collapsing, gone, once;
//   - UI-NKP-4 / F7: "Returning in {s}" with s = ceil(T_due - now) in whole seconds, the static "Returning" at <= 0 (no
//     ellipsis, no count-up, no "searching"), hidden while not awaiting, without T_due, or before the fade completes.
// The tally view takes no victim kind, so an NPC kill and an informant kill at the same tally dump byte-equal by
// construction (AC-NKP-19's dump equality is also checked in PIE: StealthGame.Penalty.PIE.TallyWidgetParity).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "View/PenaltyHudViewModel.h"

namespace PenaltyHudViewModelTestData
{
	/** The schema default L (UI-NKP-1's AC uses L = 3). */
	constexpr int32 Limit = 3;
	/** T_due 15 s ahead (AC-NKP-31). */
	constexpr double Now = 100.0;
	constexpr double Due = 115.0;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPenaltyHudViewModelTest, "StealthGame.Penalty.HudViewModel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPenaltyHudViewModelTest::RunTest(const FString& Parameters)
{
	using namespace PenaltyHudViewModelTestData;

	// UI-NKP-1: the tally line at each value while Active, and frozen.
	const bool ExpectedTriangle[] = { false, false, true, true, true };
	for (int32 N = 0; N <= 4; ++N)
	{
		const FPenaltyTallyView View = FPenaltyTallyView::Make(N, Limit, false);
		TestEqual(FString::Printf(TEXT("tally %d: the line"), N), View.Text.ToString(), FString::Printf(TEXT("Wrong targets: %d"), N));
		TestEqual(FString::Printf(TEXT("tally %d: L squares"), N), View.Limit, Limit);
		TestEqual(FString::Printf(TEXT("tally %d: min(n, L) filled"), N), View.Filled, FMath::Min(N, Limit));
		TestEqual(FString::Printf(TEXT("tally %d: the triangle at n >= L - 1"), N), View.bTriangle, ExpectedTriangle[N]);
		TestFalse(FString::Printf(TEXT("tally %d: the limit is never written"), N), View.Text.ToString().Contains(TEXT("/")));
		TestTrue(FString::Printf(TEXT("tally %d: non-colour backups (numeral in the text)"), N), View.Text.ToString().EndsWith(FString::FromInt(N)));
		AddInfo(FString::Printf(TEXT("NKP018 tally %d: %s"), N, *View.Dump()));
	}
	const FPenaltyTallyView Frozen = FPenaltyTallyView::Make(3, Limit, true);
	TestTrue(TEXT("frozen: visible as a record"), Frozen.bVisible && Frozen.bFrozen);
	TestEqual(TEXT("frozen: the exile tally of 3"), Frozen.Text.ToString(), FString(TEXT("Wrong targets: 3")));
	TestEqual(TEXT("frozen: squares filled"), Frozen.Filled, 3);
	TestFalse(TEXT("frozen: no danger triangle (ruling D4)"), Frozen.bTriangle);
	TestEqual(TEXT("the same tally dumps identically on two calls (no hidden input: the victim kind is not one)"),
		FPenaltyTallyView::Make(2, Limit, false).Dump(), FPenaltyTallyView::Make(2, Limit, false).Dump());
	TestTrue(TEXT("positive control: different tallies dump differently"), FPenaltyTallyView::Make(1, Limit, false).Dump() != FPenaltyTallyView::Make(2, Limit, false).Dump());

	// UI-NKP-3: the card timeline and wording.
	const FPenaltyCardTiming Timing;
	TestEqual(TEXT("card: hidden before any entrance"), static_cast<int32>(FPenaltyExileCardView::Make(-1.0, Now, Timing).Phase), static_cast<int32>(EPenaltyCardPhase::Hidden));
	TestEqual(TEXT("card: hidden before its entrance time"), static_cast<int32>(FPenaltyExileCardView::Make(Now + 0.1, Now, Timing).Phase), static_cast<int32>(EPenaltyCardPhase::Hidden));
	const FPenaltyExileCardView Entering = FPenaltyExileCardView::Make(Now, Now + Timing.InSec * 0.5, Timing);
	TestEqual(TEXT("card: entering"), static_cast<int32>(Entering.Phase), static_cast<int32>(EPenaltyCardPhase::Entering));
	TestTrue(TEXT("card: eased in (partly opaque mid-entrance)"), Entering.Alpha > 0.f && Entering.Alpha < 1.f);
	const FPenaltyExileCardView Holding = FPenaltyExileCardView::Make(Now, Now + Timing.InSec + 1.0, Timing);
	TestEqual(TEXT("card: holding"), static_cast<int32>(Holding.Phase), static_cast<int32>(EPenaltyCardPhase::Holding));
	TestEqual(TEXT("card: title"), Holding.Title.ToString(), FString(TEXT("Informant")));
	TestEqual(TEXT("card: subtitle"), Holding.Subtitle.ToString(), FString(TEXT("Too many wrong targets")));
	TestEqual(TEXT("card: second line"), Holding.Line2.ToString(), FString(TEXT("You watch for your team now.")));
	for (const FText& Line : { Holding.Title, Holding.Subtitle, Holding.Line2 })
	{
		TestFalse(TEXT("card: 'exile' appears in no player-facing string (D11)"), Line.ToString().Contains(TEXT("exile"), ESearchCase::IgnoreCase));
	}
	const FPenaltyExileCardView Collapsing = FPenaltyExileCardView::Make(Now, Now + Timing.InSec + Timing.HoldSec + Timing.OutSec * 0.5, Timing);
	TestEqual(TEXT("card: collapsing into the brackets"), static_cast<int32>(Collapsing.Phase), static_cast<int32>(EPenaltyCardPhase::Collapsing));
	TestTrue(TEXT("card: half collapsed"), FMath::IsNearlyEqual(Collapsing.Collapse, 0.5f, 0.01f));
	TestEqual(TEXT("card: gone after the collapse"), static_cast<int32>(FPenaltyExileCardView::Make(Now, Now + Timing.InSec + Timing.HoldSec + Timing.OutSec + 0.01, Timing).Phase),
		static_cast<int32>(EPenaltyCardPhase::Hidden));

	// UI-NKP-4: the countdown.
	const FPenaltyCountdownView At15 = FPenaltyCountdownView::Make(true, Due, Now, true);
	TestTrue(TEXT("countdown: visible for the awaiting owner after the fade"), At15.bVisible);
	TestEqual(TEXT("countdown: 'Returning in 15' at T_due - 15 s"), At15.Text.ToString(), FString(TEXT("Returning in 15")));
	TestEqual(TEXT("countdown: ceil (14.2 s left reads 15)"), FPenaltyCountdownView::Make(true, Due, Now + 0.8, true).Text.ToString(), FString(TEXT("Returning in 15")));
	TestEqual(TEXT("countdown: whole-second step (13.9 s left reads 14)"), FPenaltyCountdownView::Make(true, Due, Now + 1.1, true).Seconds, 14);
	TestEqual(TEXT("countdown: 0.4 s left reads 1"), FPenaltyCountdownView::Make(true, Due, Due - 0.4, true).Text.ToString(), FString(TEXT("Returning in 1")));
	TestEqual(TEXT("countdown: a static 'Returning' at 0"), FPenaltyCountdownView::Make(true, Due, Due, true).Text.ToString(), FString(TEXT("Returning")));
	const FPenaltyCountdownView Picking = FPenaltyCountdownView::Make(true, Due, Due + 7.0, true);
	TestEqual(TEXT("countdown: still the static 'Returning' while picking past T_due (no count-up)"), Picking.Text.ToString(), FString(TEXT("Returning")));
	TestFalse(TEXT("countdown: no ellipsis or 'searching' wording"), Picking.Text.ToString().Contains(TEXT(".")) || Picking.Text.ToString().Contains(TEXT("search"), ESearchCase::IgnoreCase));
	// After a reclaim at time t the display equals ceil(T_due - t): it is a function of the carried T_due and now alone.
	TestEqual(TEXT("countdown: after a reclaim at t = T_due - 6.5 it reads ceil(6.5) = 7"), FPenaltyCountdownView::Make(true, Due, Due - 6.5, true).Seconds, 7);
	TestFalse(TEXT("countdown: hidden before the fade completes"), FPenaltyCountdownView::Make(true, Due, Now, false).bVisible);
	TestFalse(TEXT("countdown: hidden while not awaiting a body (embodied, Active, an ally or an opponent)"), FPenaltyCountdownView::Make(false, Due, Now, true).bVisible);
	TestFalse(TEXT("countdown: hidden without T_due (0: none)"), FPenaltyCountdownView::Make(true, 0.0, Now, true).bVisible);
	return true;
}

#endif
