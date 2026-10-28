#pragma once

#include "CoreMinimal.h"

class SWidget;

/**
 * The three Penalty-fed HUD elements (NPC-Kill Penalty story nkp-018; GDD npc-kill-penalty-informants.md UI-NKP-1, UI-NKP-3,
 * UI-NKP-4; HUD GDD HUD-R7, HUD-R9, HUD-E02, HUD-E10, HUD-E11, F3, F6, F7) as plain view-models plus the minimal production
 * Slate overlay that paints them. Presentation builds only from View/ data (ADR-0007, ADR-0021; IVP AC-IVP-17): every input
 * is a number the owning client already holds (FStrikeResult.TallyAfter through the result plate, the public L, the owner
 * part's T_due, the client-local card-entrance time). Nothing here reads a victim kind, so the dumped tally state of an NPC
 * kill and an informant kill at the same tally is byte-equal by construction (AC-NKP-19).
 *
 * MINIMAL production widgets: the HUD epic restyles them (CommonUI, layout, scaling); the wording is the GDD's (decision D11:
 * the card reads "Informant" / "Too many wrong targets" / "You watch for your team now."; the word "exile" appears in no
 * player-facing string). Every element is screen-space; none relies on colour alone (squares plus numeral plus triangle).
 */

/** Art bible palette tokens the Penalty elements use (stand-in values until the HUD epic's style set). */
namespace PenaltyHudPalette
{
	/** Danger / finality (filled tally squares, the danger triangle). */
	STEALTHPRESENTATION_API FLinearColor Cochineal();
	/** The card's opaque title bar. */
	STEALTHPRESENTATION_API FLinearColor CochinealDeep();
	/** Text and square outlines. */
	STEALTHPRESENTATION_API FLinearColor Bone();
	/** Plate backgrounds (the countdown plate, the card body). */
	STEALTHPRESENTATION_API FLinearColor Obsidian();
}

/**
 * UI-NKP-1 / HUD-E02, F6: the line "Wrong targets: {n}" with L stepped squares filled up to min(n, L) and, at n >= L - 1 while
 * the presented own state is Active, a static upward danger triangle (it stays for n > L; nothing beyond it). Frozen while
 * Informant: numeral and squares keep their last value, no triangle (ruling D4). The limit is never written in words.
 */
struct STEALTHPRESENTATION_API FPenaltyTallyView
{
	bool bVisible = false;
	int32 Tally = 0;
	/** L (the squares encode it). */
	int32 Limit = 0;
	/** min(n, L). */
	int32 Filled = 0;
	bool bTriangle = false;
	/** True while the presented own state is Informant. */
	bool bFrozen = false;
	FText Text;
	FLinearColor FillColor = FLinearColor::Transparent;
	FLinearColor OutlineColor = FLinearColor::Transparent;
	FLinearColor TextColor = FLinearColor::Transparent;
	FLinearColor TriangleColor = FLinearColor::Transparent;

	/** The view for tally n, limit L and the presented own state. */
	static FPenaltyTallyView Make(int32 Tally, int32 Limit, bool bPresentedInformant);

	/** "text|filled/squares|triangle|frozen|colours": the widget state AC-NKP-19 compares between victim kinds. */
	FString Dump() const;
};

/** The removal card's F3 timeline (HUD keys HudRemovalCardInSec, HudRemovalCardHoldSec, HudRemovalCardOutSec). */
struct FPenaltyCardTiming
{
	double InSec = 0.6;
	double HoldSec = 4.0;
	/** The collapse into the informant brackets. */
	double OutSec = 0.8;
};

enum class EPenaltyCardPhase : uint8
{
	Hidden,
	/** Eased entrance from the card-entrance event. */
	Entering,
	Holding,
	/** Collapsing into the informant brackets (top left). */
	Collapsing,
};

/** UI-NKP-3 / HUD-E10: the exile card, shown once from the client-local card-entrance event. */
struct STEALTHPRESENTATION_API FPenaltyExileCardView
{
	EPenaltyCardPhase Phase = EPenaltyCardPhase::Hidden;
	/** Opacity 0..1 (eased in; 1 while holding; falls with the collapse). */
	float Alpha = 0.f;
	/** 0 at full size .. 1 collapsed into the informant brackets. */
	float Collapse = 0.f;
	FText Title;
	FText Subtitle;
	FText Line2;

	/** The card at ServerNow for a card that entered at EnteredServerTime (negative: never entered, Hidden). */
	static FPenaltyExileCardView Make(double EnteredServerTime, double ServerNow, const FPenaltyCardTiming& Timing);

	/** "phase|alpha|collapse|title|subtitle|line2". */
	FString Dump() const;
};

/**
 * UI-NKP-4 / HUD-E11, F7: "Returning in {s}" in whole-second steps, s = ceil(T_due - now), from the carried T_due (so it is
 * right after a reclaim or rejoin), and a static "Returning" once s <= 0 while the takeover is still picking (no ellipsis,
 * no count-up, no "searching" wording). Shown only to the awaiting owner and only after the fade completes.
 */
struct STEALTHPRESENTATION_API FPenaltyCountdownView
{
	bool bVisible = false;
	/** max(0, ceil(T_due - now)). */
	int32 Seconds = 0;
	FText Text;

	/**
	 * bAwaitingBody: an informant with no pawn; DueServerTime: the owner part's T_due (0: none); bFadeComplete: the neutral
	 * view is reached (the fall-and-fade or the exile's view release has completed).
	 */
	static FPenaltyCountdownView Make(bool bAwaitingBody, double DueServerTime, double ServerNow, bool bFadeComplete);

	FString Dump() const;
};

/** One frame of the Penalty HUD elements. */
struct FPenaltyHudFrame
{
	FPenaltyTallyView Tally;
	FPenaltyExileCardView Card;
	FPenaltyCountdownView Countdown;
};

/**
 * The minimal production overlay (a hit-test-invisible Slate leaf widget for the game viewport) that paints Provider's frame
 * every frame: the tally under the status panel (top left), the countdown in the Notice slot (centre) and the card in the
 * central band (<= 30 % of the screen), collapsing towards the informant brackets.
 *   Viewport->AddViewportWidgetForPlayer(LocalPlayer, MakePenaltyHudOverlay([WeakPC]() { return WeakPC->BuildPenaltyHudFrame(); }), 10);
 */
STEALTHPRESENTATION_API TSharedRef<SWidget> MakePenaltyHudOverlay(TFunction<FPenaltyHudFrame()> Provider);
