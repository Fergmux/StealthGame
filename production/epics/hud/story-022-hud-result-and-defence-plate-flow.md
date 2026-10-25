# Story 022: Result and defence plates wired to F2/F3 with per-outcome text, stamp and cue id

> **Epic**: HUD
> **Status**: Ready
> **Layer**: Presentation
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/hud.md`
**Requirement**: `TR-hud-004`, `TR-hud-002`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0018: Encounter data and outcome presentation timing (§5.3 owner results and the positional matcher, §5.4 tally and exile), with ADR-0021 (§1 L2 lower-centre Result slot, §4 presenter and string tables)
**ADR Decision Summary**: A strike's owner result is presented at the owner's own contact frame, identically for every outcome and victim kind: `t_show = min(max(t_c,local, t_arrival), t_arrival + HudResultPlateMaxHoldBackSec)`, with the attacker's `t_c` from `FStrikeResult.ContactServerTime` and the defender's from the owner's own Counter `FActionState` write (positional matcher), and a late result never dropped in live play. The Result slot lives on L2 (`UStealthHudTransientLayer`, ZOrder 40, `HitTestInvisible`); plate text comes from the string table and no text travels on the wire.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Integration test in the `StealthTestHarness` module using CQTest `NETWORK_TEST_CLASS` multi-client PIE (ADR-0008 T2; the harness comes from information-visibility-policy story 001). ADR-0018 HIGH networking domain applies: `FStrikeResult` and `FDefenceResult` are owner-only reply entries, `FActionState` is replicated pawn data, and UE does not order an RPC against property replication (E6), so the test must exercise both arrival orders; MotionWarping is Beta in 5.8.4 and drives the strike clip whose anim-notify tick is the test's contact oracle. The listen host gets no RepNotify for its own writes, so the host case runs the same sampled path (no event-style handlers). The result widgets are UMG on L2 (E1, `HitTestInvisible`). Use `UE_LOGF` for diagnostics.

**Control Manifest Rules (this layer)**:
- Required: Presentation timing uses only `EstimatedServerNow()`; plate and card timelines are sampled from `now − t0` in C++ — source: ADR-0021, ADR-0002
- Forbidden: Never depend on a RepNotify having fired; never drive widgets event-style from RepNotify/RPC handlers — the host gets no RepNotify for its own writes — source: ADR-0021
- Guardrail: HUD ≤ 0.3 ms CPU (AC-HUD-22), ≤ 0.5 ms with markers; UI GPU ≤ 0.5 ms at 1080p on the reference GPU — source: ADR-0021

This story assembles the pieces built so far into the player-visible result feedback: the F2 timer (story 015), the F3 timeline (016), the plate style (017) and the sampled inputs (003) become one pipeline that shows the right plate at the right frame. It is the first end-to-end proof of HUD-R3, the parity rule: all five plates share size, slot, entrance, hold and exit, differ only in text and glyph, and none shows before the owner's contact frame, so a result plate can never tell a bystander or the owner anything the strike animation has not already shown. It also wires the defender's special case, where the result arrives up to the contact delay before the thing it describes.

---

## Acceptance Criteria

*From GDD `design/gdd/hud.md` (HUD-R6, F2, F3, HUD-R9 late-result rule) and `design/ux/hud.md` (UXA-HUD-18, E07, E08), scoped to this story:*

- [ ] **AC1** (UXA-HUD-18, `StealthGame.HUD.PIE.ResultPlateFlow` per-outcome plate): **GIVEN** each `FStrikeResult` outcome and `FDefenceResult` **WHEN** shown **THEN** the Result slot plate carries exactly "Target eliminated" + stepped-square stamp, "Wrong target" + upward Cochineal triangle, "No contact" (Bone only), "Reversed" + triangle, or "Strike reversed" + stepped-square stamp; all five have the same size, slot, entrance, hold and exit and show no earlier than the owner's contact frame.
- [ ] **AC2** (derived, HUD-R6 defender rule and F2, `StealthGame.HUD.PIE.ResultPlateFlow` defender hold): **GIVEN** a defender's `FDefenceResult` **THEN** the plate is held Pending until the defender's own Counter `FActionState` write is observed, shown at F2's time from it, with fallback arrival + `HudResultPlateMaxHoldBackSec` for lost data; it never names the attacker or its team.
- [ ] **AC3** (derived, VA-HUD-6 / CR-HUD-15 and D9, `StealthGame.HUD.PIE.ResultPlateFlow` cue and late result): **GIVEN** each outcome **THEN** it fires one distinct owner-only result cue id at `t_show` and none before (cue stream, story 051); a late result in live play still shows on processing and an exile card follows it (D9).

---

## Implementation Notes

*Derived from ADR-0018 §5.3-5.4, ADR-0021 §1 and §4, GDD HUD-R6, UX E07/E08:*

**What to build.** (1) The Result-slot widget on L2: `UStealthResultPlateWidget` in `Source/StealthPresentation/Private/Hud/Widgets/`, child of `UStealthHudTransientLayer`'s Result slot (Z4, positioned by the layout resolver of story 007). One plate 300 x 64 whose width is computed from the widest result string of the active locale, never from the shown string, so its size is outcome-independent (HUD-R3); left a 40 x 40 stamp or glyph slot (always reserved), right the text in Primary 22 px Semibold Bone, centred in the remaining width. It applies `FHudResultSlotView` (text key, glyph id, alpha, `bVisible`) on change only. (2) The result-slot logic in the core: `FHudResultSlot` in `Source/StealthPresentation/Private/Hud/` takes the F2 schedule from `FHudResultTimer` (015), maps each result to its plate content, runs the F3 timeline (`FHudTimeline`, 016) from `t0 = t_show` and exposes the slot content and alpha through `FHudViewModel`. (3) The cue request: at `t_show` the slot raises one owner-only cue id in the cue event stream (the stream itself is story 051; this story adds the producer call and the per-outcome ids).

**Content (UX E07/E08, D11, D29).**

| Outcome | Text key and text | Glyph slot |
|---|---|---|
| `RivalEliminated` | `Hud.Result.RivalEliminated` "Target eliminated" | Bone stepped-square stamp |
| `WrongTarget` | `Hud.Result.WrongTarget` "Wrong target" | Cochineal Bright upward triangle (byte-identical for an NPC and an informant victim) |
| `NoContact` | `Hud.Result.NoContact` "No contact" | empty (a miss is not finality, no Cochineal) |
| `Reversed` (attacker) | `Hud.Result.Reversed` "Reversed" | Cochineal Bright upward triangle; the removal card follows and preempts |
| Defence (defender) | `Hud.Result.Defence` "Strike reversed" | Bone stepped-square stamp |

"Target eliminated" is the D29 wording (renamed from the earlier rival-kill phrase; "exile" is reserved for the strike-limit removal). Use the exact characters of the UX string table. The defence plate never names the attacker or its team: the plate text and the dump contain no attacker name, team label or kind; the test asserts that the plate string and the dump contain none of the attacker's `DisplayName` or a team token.

**Timeline and parity.** One shared timeline for all five (HUD-R6): `Pending` until `t_show`, then 250 ms stepped in (four 62.5 ms steps), 3.0 s hold times `HudReadHoldMult`, 600 ms out, total 3.85 s (F3, story 016). The IIF UI-5 timing (400 ms, 1.5 s, 600 ms) is for rejection plates only and must not be used here. A plate still in the slot when the card or a round clear takes it leaves over `HudPreemptExitSec` (the preempt rule is story 016's; the card side is story 024/025). An informant never receives a result plate (the viewer-state gating of story 014 hides the slot).

**The defender hold.** `FDefenceResult` arrives with the resolving drain, up to `StrikeContactSec` (0.4, plus `BushStrikeEmergeSec` 0.35 from a bush) before contact, and carries no time. The result stays `Pending` until the defender's own `FActionState` Counter write (`Pose = Counter`, written at the death write, `ContactServerTime − StrikeNetLeadSec`) is observed on the captured pawn handle, then shows at F2's time computed from that write's `ContactServerTime`. A stale previous-strike `FActionState` is ignored (story 015 owns the matching rule; this story wires it to the plate). If no matching write is seen by `t_arrival + HudResultPlateMaxHoldBackSec` the plate shows at that cap (always at or after contact by I-HUD-1). Any reply to a defender command during `Countering` (`Busy`) is held to the defender plate's show time (the notice-slot half is story 031; this story only exposes the show time as the release input).

**Late results (D9, PEN-R16).** A late, never-shown result is never dropped for lateness in live play: processed after its show time it shows on processing, and for an exile (`bExiled`) the removal card follows at `t_show + ExileBeatSec` (the card arithmetic is story 024; this story must supply `t_show` of the WrongTarget plate to it and make sure the plate is shown, not skipped, when late).

**Cue ids.** One distinct owner-only 2D result cue id per outcome (five ids), fired at `t_show` and none before; never a cue for a received result before the plate. The tally tick fires with the plate (story 051/021). The cue id names are fixed in story 051's table; here use named constants in a header `HudCueIds.h` that story 051 completes.

**Test.** `NETWORK_TEST_CLASS(ResultPlateFlow, "StealthGame.HUD.PIE")` with methods per outcome (`TargetEliminated`, `WrongTarget`, `NoContact`, `Reversed`, `StrikeReversed`), `DefenderHold`, `DefenderHoldLostData`, `LateResult`, `CueIds`. Determinism per the GDD AC preamble: fixed step 60 Hz, identical seeds, contact oracle from the strike / Death_Fall / Counter anim-notify tick on that client (not the HUD's own F2). Run both arrival orders of result and `FActionState` (E6). Run on a remote client and on the listen host.

**Prototype.** The message plumbing in `Source/StealthGame/Private/StealthPlayerController.*` and `StealthGameMode.cpp` (`MessagePlayer`, `MessageController`, `MessageTeam`, `MessageAll`) and the canvas kill-confirmation text in `StealthHUD.cpp` are replaced by this pipeline; nothing is promoted. Removal itself is story 059.

**Out of this story:** the F2 arithmetic (015) and F3 (016), the tally move (020/021), the card (024/025/026), the leak suite over many runs (028, 029), the plate screenshots (023).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/hud/story-015-hud-result-show-time-f2.md` and `production/epics/hud/story-016-hud-plate-timeline-f3-and-reduce-motion.md`: the pure show-time and timeline logic.
- `production/epics/hud/story-023-hud-strike-strip-test-and-plate-evidence.md`: the strike-strip capture diff and per-outcome screenshots.
- `production/epics/hud/story-024-hud-removal-card-timing-f4-logic.md`, `production/epics/hud/story-025-hud-removal-card-widget-and-preempt.md` and `production/epics/hud/story-026-hud-removal-card-pie-timing.md`: the card that follows an exile and preempts the plate.
- `production/epics/hud/story-028-hud-leak-suite-strike-and-look-parity.md` and `production/epics/hud/story-029-hud-leak-suite-no-warning-before-contact.md`: the parity and no-warning leak suites.
- `production/epics/hud/story-051-hud-cue-event-stream-and-owner-only-audio-hooks.md`: the cue stream and owner-only routing.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthPresentation/Private/Hud/HudResultSlot.h`
- `Source/StealthPresentation/Private/Hud/HudResultSlot.cpp`
- `Source/StealthPresentation/Private/Hud/HudCueIds.h`
- `Source/StealthPresentation/Private/Hud/Widgets/StealthResultPlateWidget.h`
- `Source/StealthPresentation/Private/Hud/Widgets/StealthResultPlateWidget.cpp`
- `Source/StealthPresentation/Private/Hud/Widgets/StealthHudTransientLayer.cpp` (add the Result slot child and apply `FHudResultSlotView`)
- `Source/StealthTestHarness/Private/HUD/ResultPlateFlowTest.cpp` (CQTest, `StealthGame.HUD.PIE.ResultPlateFlow.*`)

The Result plate widget Blueprint (layout and style only) is named in prose only; no `Content/` paths are planned here.

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: Per-outcome plate
  - Given: each `FStrikeResult` outcome and an `FDefenceResult`.
  - When: the plate is shown.
  - Then: Target eliminated + stepped-square stamp; Wrong target + upward Cochineal triangle; No contact (Bone only); Reversed + triangle; Strike reversed + stepped-square stamp. All five share size, slot, entrance, hold and exit, and none shows before the owner's contact tick.
  - Edge cases: the contact tick comes from the anim notify, not the HUD's F2.
- **AC-2**: Defender hold
  - Given: a defender's `FDefenceResult`.
  - When: the Counter `FActionState` write is observed, or never is.
  - Then: the plate stays Pending, then shows at the F2 time from it. If never observed, it shows at arrival + `HudResultPlateMaxHoldBackSec`. Plate text never contains the attacker's name or team label.
  - Edge cases: a stale previous-strike `FActionState` is ignored.
- **AC-3**: Cue and late result
  - Given: each outcome, and a result delayed past its show time.
  - When: the cue stream is read.
  - Then: one distinct owner-only cue id per outcome at `t_show` and none earlier. A late result shows on processing, and an exile card follows it (D9).
  - Edge cases: the harness is multi-client PIE (IVP story 001).

QL-STORY-READY: ADEQUATE

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: the CQTest PIE test `Source/StealthTestHarness/Private/HUD/ResultPlateFlowTest.cpp` (`StealthGame.HUD.PIE.ResultPlateFlow`) must exist and pass, or a documented playtest. Gate level BLOCKING.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/hud/story-003-hud-input-sampling-and-host-parity-path.md`, `production/epics/hud/story-015-hud-result-show-time-f2.md`, `production/epics/hud/story-016-hud-plate-timeline-f3-and-reduce-motion.md`, `production/epics/hud/story-017-hud-shared-visual-system-font-face-and-plate-style.md`; cross-epic: assassination-defence-reversal `production/epics/assassination-defence-reversal/story-015-owner-results-reply-entries-and-tally.md`, `production/epics/assassination-defence-reversal/story-010-reversal-counter-and-chained-strikes.md`
- Unlocks: `production/epics/hud/story-023-hud-strike-strip-test-and-plate-evidence.md`, `production/epics/hud/story-025-hud-removal-card-widget-and-preempt.md`, `production/epics/hud/story-026-hud-removal-card-pie-timing.md`, `production/epics/hud/story-027-hud-tally-moves-with-plate-and-informant-parity-pie.md`, `production/epics/hud/story-028-hud-leak-suite-strike-and-look-parity.md`, `production/epics/hud/story-029-hud-leak-suite-no-warning-before-contact.md`, `production/epics/hud/story-051-hud-cue-event-stream-and-owner-only-audio-hooks.md`, `production/epics/hud/story-058-hud-performance-capture.md`, `production/epics/hud/story-059-hud-retire-prototype-canvas-and-message-plumbing.md`
