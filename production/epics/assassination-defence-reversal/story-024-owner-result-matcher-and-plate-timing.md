# Story 024: Result plates and the brace square are shown at the local contact frame, never on receipt

> **Epic**: Assassination & Defence Reversal
> **Status**: Ready
> **Layer**: Feature
> **Type**: UI
> **Estimate**: L
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/assassination-defence-reversal.md` (ASN-R19, ASN-R23; UI Requirements UI-ASN-1, -3, -4, -5, -7; AC-ASN-16 UI clause, AC-ASN-40, AC-ASN-49); `design/ux/hud.md` (stories cite the UX spec for layout; the HUD epic owns widgets)
**Requirement**: `TR-asn-005`, `TR-asn-010`, `TR-asn-011`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0018: Encounter data and outcome presentation timing (§5.3 owner results matcher, §5.4 tally and exile), with ADR-0002 §9 (`EstimatedServerNow()`) and ADR-0021 (HUD layering)
**ADR Decision Summary**: For the attacker the plate show time comes from `FStrikeResult.ContactServerTime`; the positional matcher (`FOwnResultMatcher`, pawn handle captured at the press) remains for the defender's `FDefenceResult` and as the lost-data fallback, with `t_show = min(max(t_c,local, t_arrival), t_arrival + HudResultPlateMaxHoldBackSec)`; the tally ticks at the plate's `t_show` and the exile card at `t_show + ExileBeatSec`; nothing is presented on receipt.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (UI and input are HIGH under the 5.8 Enhanced Input / Common UI unification; the matcher itself is plain C++)
**Engine Notes**: `FOwnResultMatcher` is client-local, one per local player, in `Public/View`; arrival order of the reply RPC and the `FActionState` property is unspecified (E6 [U]); both orders are forced in the net-emulation test (Verification Required (3)). Plate widgets are the HUD epic's (UMG/CommonUI, ADR-0021); this story supplies timing, data, the matcher and the presentation contract, and verifies the screens.

**Control Manifest Rules (this layer)**:
- Required: Result plates are matched to their strike positionally on the pawn handle captured at the press; for the attacker the result's own `ContactServerTime` is used first — source: ADR-0018
- Forbidden: Never present an outcome on receipt; never show the attacker's plate from an inferred contact time when `FStrikeResult.ContactServerTime` is present; never use arrival time except as the matcher's floor and lost-data cap — source: ADR-0018
- Forbidden: No strike hint, prompt, highlight or reticle for a bush or spot; no observer, enemy, informant or spectator UI for strikes, braces or reversals — source: GDD ASN-R20, UI-ASN-7
- Guardrail: `HudResultPlateMaxHoldBackSec` and `HudOwnRemovalHoldCapSec` have a hard minimum of 1.25 s in the TDC, so the lost-data fallback never fires below the strike lead

One entrance and one timing for every plate: `RivalEliminated` = "Target eliminated", `WrongTarget` = "Wrong target" (byte-identical for NPC and informant), `NoContact` = "No contact" (no stamp, no cochineal), `Reversed` = "Reversed"; the defender's "Strike reversed" plate is shown at its own Counter write's `ContactServerTime`. A reversal ends the brace window like any other end but at the contact time: the consumed brace's `FHoldStatus` carries `ServerEndTime = ContactServerTime` and the square is removed when local server time reaches it. A struck player's removal card and camera `Dying` start at its own `DeathServerTime` (story 021's dispatch). If a result arrives after its local contact frame (RTT beyond about 300 ms) the plate shows on arrival; nothing is shown before contact.

---

## Acceptance Criteria

*From GDD `design/gdd/assassination-defence-reversal.md` (AC-ASN-16, AC-ASN-40, AC-ASN-49), scoped to this story:*

- [ ] **AC1** (AC-ASN-16 UI clause, [UI/BLOCKING] `StealthGame.Assassination.KillPlateTellsOneThing`): **GIVEN** strikes that kill an Active rival, an NPC and an informant, a whiff and a reversed strike **THEN** the attacker sees "Target eliminated", "Wrong target", "Wrong target", "No contact" and "Reversed" respectively, each on its local contact frame with the same entrance and timing; evidence `asn-16-{rival,npc,informant,whiff,reversed}.png`.
- [ ] **AC2** (AC-ASN-49, [UI/BLOCKING] `StealthGame.Assassination.OutcomePresentedAtContact`, ASN-R23, UI-ASN-3/4/5): **GIVEN** PktLag 75 ms per direction and a defender that reverses a strike **THEN** its brace square is removed and the "Strike reversed" plate appears at its local `ContactServerTime` (± 1 frame), never at the arrival of `FHoldStatus` or `FDefenceResult`; **AND** a struck player's removal card and camera `Dying` start at its own `DeathServerTime` (± 1 frame), never at the arrival of its Roster state; evidence `asn-49-{defender,victim}-*.png` captured at receipt time and at contact.
- [ ] **AC4** (ADR-0018 §5.3 and Verification Required (3), added from the QA review, [Logic/BLOCKING] `OwnResultMatcherTest`): **GIVEN** a result and its `FActionState` write arriving in either order, a dropped strike write, a duplicate result, and an RTT beyond 300 ms **THEN** `t_show = min(max(t_c,local, t_arrival), t_arrival + HudResultPlateMaxHoldBackSec)` with `HudResultPlateMaxHoldBackSec ≥ 1.25 s`, the duplicate is dropped by `(PossessionEpoch, Seq)`, and a late result shows on arrival and never before contact.
- [ ] **AC3** (AC-ASN-40, [UI/BLOCKING] `StealthGame.Assassination.NoBushTargetingUI`, ASN-R8, IIF UI-4): **GIVEN** the attacker's camera ray on an occupied bush and on an empty bush **THEN** no Strike hint, prompt, highlight or reticle appears in either case, before or after a strike; evidence `asn-40-{occupied,empty}.png`.

---

## Implementation Notes

*Derived from ADR-0018 §5.3, §5.4 and GDD UI-ASN-1 to UI-ASN-7:*

`FOwnResultMatcher { OnCommandSent, OnStrikeResult, OnDefenceResult, OnActionStateObserved, PopDue }` (ADR-0018 Key Interfaces). On sending an `Attack` (and a `Prepare(Begin)` for the defence side) capture the commanded pawn handle (not "the currently possessed pawn": an exile releases it) and the last consumed `(Seq, ContactServerTime)`. A result is `Pending` until the first `FActionState` observation on that handle with `Pose = Strike` (defender: `Counter`), a `Seq` differing from the last consumed and a later `ContactServerTime`. Uniqueness holds because `AttackAttemptLockoutSec` (hard min 1.2 s) ≥ the longest execution-to-contact delay (≤ 1.2 s by I-ASN-3; W-ASN-10 warns on equality). Duplicates drop by `(PossessionEpoch, Seq)` order and a last-8 ring; late processing never drops an unshown, non-duplicate result in live play.

Plate timeline (HUD-R6, F3): 250 ms stepped in, `HudResultPlateHoldSec` 3.0 s hold, 600 ms out; IIF's 400 ms / 1.5 s / 600 ms applies to rejection plates only. UI-ASN-2's "Wrong targets" tally line (Penalty's element) ticks at the same frame as the `WrongTarget` plate. Rejection plates: `Attack` during the lockout replies `Busy`; `Prepare` during the cooldown replies `Cooldown` and shows no plate (the Cooling numeral says it); a `Rejected_Latched` Attack shows the ordinary "Not now". Accessibility: each owner-only audio cue has the plate as a text twin; no plate relies on colour alone; haptics are owner-only and outcome-uniform until contact.

The plates' widget implementations are in the HUD epic (stories not yet created); until they exist this story uses the HUD's view-model seam and a test widget. Screens and evidence follow `design/ux/hud.md`; the UX flag on this GDD asks that `/ux-design` for the HUD is run before the HUD epic.

**Prototype replaced:** the prototype's on-receipt `MessagePlayer` HUD messages ("Reversal!", "Your target was ready…") are replaced; evidence `production/qa/evidence/2026-10-24_client1-hud-message-npc-kill.png` and `…client1-npc-kill-confirmation.png` are the before-state for comparison.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-015-owner-results-reply-entries-and-tally.md`: building and sending the results.
- `story-021-role-change-dispatch-and-transient-grade.md`: the dispatch the removal card and camera subscribe to.
- HUD epic: widget layout, plate art, the removal card; Penalty epic: tally line content and the exile card.
- IIF stories 073 and 076: the strike hint parity and rejection plates themselves.

---

## Files

- `Source/StealthGameplay/Public/View/Features/Assassination/OwnResultMatcher.h` (`FOwnResultMatcher`, plain C++, client-safe)
- `Source/StealthGameplay/Private/Features/Assassination/OwnResultMatcher.cpp`
- `Source/StealthGameplay/Private/Tests/Assassination/OwnResultMatcherTest.cpp` (class `FAssassinationOwnResultMatcherTest`: both arrival orders, dropped strike write, duplicates, late result, cap; headless)
- `Source/StealthTestHarness/Private/Assassination/OutcomePresentedAtContactTest.cpp` (`StealthGame.Assassination.PIE.OutcomePresentedAtContact`)
- `production/qa/evidence/asn-16-*.png`, `production/qa/evidence/asn-40-*.png`, `production/qa/evidence/asn-49-*.png` and sign-off `production/qa/evidence/asn-24-evidence.md`

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

QL-STORY-READY: GAPS
- AC1: "same entrance and timing" is not numeric; proposed rewrite (unconfirmed): each plate steps in over 250 ms, holds `HudResultPlateHoldSec` (3.0 s) and goes out over 600 ms, beginning at the attacker's local `ContactServerTime` ± 1 frame, with the same timeline for all five outcomes; the two "Wrong target" strings are byte-identical.
- AC2: "captured at receipt time and at contact" is not tied to a pass state; proposed rewrite (unconfirmed): the receipt-time capture shows no plate, no removal card and the square still present; the contact-time capture shows the plate and the square gone, at `ContactServerTime` / `DeathServerTime` ± 1 frame under 75 ms PktLag per direction.
- AC3: "no hint appears" is judged by eye; proposed rewrite (unconfirmed): the visible widget list and a pixel diff of the HUD region (excluding the world) are identical to an unaimed baseline frame.
- There is no AC for the matcher itself; proposed AC4 [Logic/BLOCKING] (unconfirmed): GIVEN a result and `FActionState` in either arrival order, a dropped strike write, a duplicate, and an RTT beyond 300 ms THEN `t_show = min(max(t_c,local, t_arrival), t_arrival + HudResultPlateMaxHoldBackSec)` with `HudResultPlateMaxHoldBackSec ≥ 1.25 s`, a duplicate is dropped by `(PossessionEpoch, Seq)`, and the plate shows on arrival if late and never before contact. (`OwnResultMatcherTest.cpp` in the Files list already carries this logic.)

- **AC-1**: Kill plate tells one thing
  - Setup: five strikes (Active rival, NPC, informant, whiff, reversed), the HUD view-model seam and a test widget
  - Verify: capture each plate at its local contact frame and log its entrance, hold and out times
  - Pass condition: the texts are "Target eliminated", "Wrong target", "Wrong target", "No contact" and "Reversed"; timings identical ± 1 frame; "No contact" has no stamp or cochineal; `asn-16-{rival,npc,informant,whiff,reversed}.png` retained
- **AC-2**: Outcome presented at contact
  - Setup: PktLag 75 ms per direction; a defender reverses a strike; a player victim is struck
  - Verify: capture at receipt of `FHoldStatus` or `FDefenceResult` or the Roster state, and at contact or `DeathServerTime`
  - Pass condition: the square is removed and the "Strike reversed" plate appears at the local `ContactServerTime` ± 1 frame, never at receipt; the removal card and `Dying` start at `DeathServerTime` ± 1 frame; `asn-49-{defender,victim}-*.png` retained
- **AC-3**: No bush targeting UI
  - Setup: the attacker's camera ray on an occupied bush and on an empty bush, before and after a strike
  - Verify: the widget tree and a screenshot at each state, compared against an aimed-at-nothing baseline
  - Pass condition: no hint, prompt, highlight or reticle in any of the four captures, with an identical widget list for occupied and empty; `asn-40-{occupied,empty}.png` retained

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: UI
**Required evidence**:
- UI: a retained screenshot of each screen touched in `production/qa/evidence/` (`asn-16-*`, `asn-40-*`, `asn-49-*`), none discarded
- Supporting: `OwnResultMatcherTest.cpp` and `OutcomePresentedAtContactTest.cpp`

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `story-015-owner-results-reply-entries-and-tally.md`, `story-016-defence-owner-cooldown-component.md`, `story-018-strike-clip-variants-warp-and-contact-scheduling.md`, `story-021-role-change-dispatch-and-transient-grade.md`; cross-epic HUD epic `production/epics/hud/EPIC.md` (no stories yet); `production/epics/input-interaction-framework/story-073-strike-hint-parity.md`, `story-076-rejection-plate.md`, `story-074-brace-protected-only-on-confirm.md`
- Unlocks: `story-017-no-warning-and-no-observer-cue.md`, HUD epic plate stories
