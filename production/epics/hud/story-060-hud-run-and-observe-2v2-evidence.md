# Story 060: Run-and-observe: a 2v2 round played to an objective win with the full retained screenshot set

> **Epic**: HUD
> **Status**: Blocked — BLOCKED: spikes UI-V1 (teammate-markers story 018), UI-V3 (story 012) and UI-V4 (story 008) have not reported
> **Layer**: Presentation
> **Type**: UI
> **Estimate**: L
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/hud.md` · UX spec `design/ux/hud.md`
**Requirement**: `TR-hud-002`, `TR-hud-003`, `TR-hud-005`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0021: UI/HUD layering, CommonUI and Enhanced Input (with ADR-0008 for the PIE round driver)
**ADR Decision Summary**: The HUD is a stack of UMG layers (L0 world-anchored at 10, fade at 20, L1 persistent plates at 30, L2 lower-centre at 40, L3 cards and banners at 50, L4 status overlays at 60, menus at 100) built from view-models whose deterministic dump the tests read; the run-and-observe rule says a parse check is not a run, so every player-observable HUD story is also launched and observed with retained screenshots. This story is the one place the whole epic's visuals are observed together in a real round.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Blocked on ADR-0021 verification items UI-V1 (zero-lag projection: the ring, markers and prompt in the captures must be on their anchors; spike story `teammate-markers/story-018`), UI-V3 (layer order: the fade covers L0 and stays under plates and cards; spike story 012) and UI-V4 (DPI `H/1080` and the root scaler at 1080p and 16:10; spike story 008), because captures taken before those report could encode a wrong mechanism. Engine facts: `AddToPlayerScreen` is the Player slot (100) below `AddToViewport` (E1); a camera fade cannot cover UMG (E2); a DPI rule's result is cached per viewport size (E5). Procedure per engine is `.claude/docs/run-and-observe.md`. `UE_LOGF` is preferred over `UE_LOG` in 5.8.

**Control Manifest Rules (this layer)**:
- Required: Every HUD and marker layer is a UMG widget added with `AddToPlayerScreen` at its HUD-R4 ZOrder — source: ADR-0021
- Forbidden: Never use `AddToViewport` or the `AHUD` canvas for game UI (lint) — the canvas cannot be ordered against the UMG fade — source: ADR-0021
- Guardrail: HUD: ≤ 0.3 ms CPU (AC-HUD-22), ≤ 0.5 ms with markers; UI GPU ≤ 0.5 ms at 1080p on the reference GPU (inside ADR-0022's frame budget) — source: ADR-0021

This story owns the final MVP evidence pass for the HUD, together with story 061. Per the coding standard, a story that changes something player-observable is launched and observed before it closes and the observation is a retained screenshot; this story is where that is done for the epic as a whole.

---

## Acceptance Criteria

*From GDD `design/gdd/hud.md` (AC-HUD-23) and UX spec `design/ux/hud.md` (UXA-HUD-33, UXA-HUD-06), scoped to this story:*

- [ ] **AC1** (AC-HUD-23 `StealthGame.HUD.PIE.RunAndObserve`, UI/BLOCKING): **GIVEN** a 2v2 PIE round played to an objective win **WHEN** the session is observed **THEN** it produces retained screenshots of SA (all persistent clusters with the "Brace" label), a result plate of each outcome, the defence plate, the exile card, IA with countdown, IB, the ally feed line, the live-site ring with its ticks, the Victory and Defeat banners and the called-off line.
- [ ] **AC2** (UXA-HUD-33 and UXA-HUD-06): **GIVEN** the same session **WHEN** observed **THEN** retained screenshots also exist for the assassination card, a ping ring with its label and an edge ring, and the banners with the summary, each at 1080p and 16:10; and UXA-HUD-06 Self-Active shows exactly the brace indicator with "Brace" label and glyph, tally, stage card, own steps and rival steps as persistent elements (plus ally markers) and nothing else.

---

## Implementation Notes

*Derived from `.claude/docs/run-and-observe.md`, GDD AC-HUD-23 and UX spec UXA-HUD-33:*

**What to do.** Launch a real 2v2 listen-server PIE session (the `StealthBot` driver may drive the second team through real rule paths), play it to an objective win, and separately a called-off round, at 1920x1080 and at 16:10 (1920x1200), capturing the set below with the production HUD. Index every capture in `production/qa/evidence/hud-run-and-observe-evidence.md` (file name, state, resolution, build, seed, what to look for). The CQTest `StealthGame.HUD.PIE.RunAndObserve` drives the round and triggers captures at the right beats (plate show time, card hold, banner); a human reviews each capture.

**Capture checklist (AC-HUD-23 and UXA-HUD-33), each at 1080p and 16:10.**
1. Self-Active: every persistent cluster visible with the "Brace" label and the bound glyph (brace indicator E01, tally E02, stage card, own steps E04, rival steps E05, ally markers) and nothing else persistent.
2. A result plate of each outcome: "Target eliminated", "Wrong target", "No contact", "Reversed"; plus the defence plate "Strike reversed".
3. The exile card (title "Informant", subtitle "Too many wrong targets", line "You watch for your team now.") and the assassination card (subtitle "Eliminated for the round").
4. Informant Awaiting with the countdown ("Returning in {s}") and Informant Bodied (brackets, "Informants can only observe" plate when pressed).
5. An ally feed line ("{Ally} — eliminated for the round"; reconnect lines belong to story 038).
6. The live-site ring with its ticks (interact ring E12, story 048).
7. A ping ring with its sender label and an edge ring (E22, story 049).
8. The Victory and Defeat banners with the round summary lines, and the called-off line ("The round was called off").

**UXA-HUD-06 persistent-set check.** In the Self-Active capture, enumerate the persistent elements from the view-model dump alongside the screenshot: exactly the brace indicator (label and glyph), tally, stage card, own steps and rival steps, plus ally markers. The latency icon is allowed only when shown (own RTT over `HudLatencyWarnRttSec`), and never on the host. Anything else persistent fails the capture and is a defect against its widget story.

**Standard.** A capture counts only if it shows the real production HUD in a real round; mock-ups, editor widget previews and screenshots from before a story's own evidence do not substitute. Each capture is retained on disk (`production/qa/evidence/`), not just described; the index file records the commit or build id so story 061's final pass can re-use or replace them.

**Traps.** Captures taken before the three spikes report may need redoing (hence Blocked). Some states are brief (plate hold 3.0 s, card hold 4.0 s): capture at the dump-reported beat, not by eye. A 16:10 capture needs the 16:10 safe rectangle (5 % of each dimension). Do not stage captures by forcing state through test hooks that bypass the presenter; use real rule paths.

**Decisions that apply.** D11 and D29 (wording on card and plate), D16 (display face recorded by story 017, appears in every capture), D20 (rings and markers exempt from the centre-empty rule). Out of this story: layout matrix (056), accessibility captures (054), the 5v5 exit pass (061).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/hud/story-056-hud-layout-evidence-resolutions-and-budget.md`: layout at every resolution and scale.
- `production/epics/hud/story-054-hud-accessibility-captures-and-input-pass.md`: greyscale and colour-vision captures.
- `production/epics/hud/story-061-hud-mvp-exit-5v5-presentation-verification.md`: the 5v5 final evidence pass and the leak/parity suite re-run.
- `production/epics/hud/story-023-hud-strike-strip-test-and-plate-evidence.md`, `story-043-hud-round-summary-pie.md`, `story-038-hud-reconnect-lines-pie.md`: per-story evidence this story indexes rather than repeats.
- The widgets themselves (stories 019 to 048).

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthTestHarness/Private/HUD/RunAndObserveTest.cpp` (CQTest, `NETWORK_TEST_CLASS(RunAndObserve, "StealthGame.HUD.PIE")`, drives the round and triggers the captures)
- Evidence: `production/qa/evidence/hud-run-and-observe-evidence.md` (index of every capture) and the captures it lists under `production/qa/evidence/` (`hud-<case>-<resolution>.png`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: AC-HUD-23 run-and-observe
  - Setup: play a 2v2 PIE round to an objective win, plus a called-off round, at 1080p and 16:10.
  - Verify: retained captures exist for Self-Active (all persistent clusters, "Brace" label), a result plate of each outcome, the defence plate, the exile card, Informant Awaiting with countdown, Informant Bodied, the ally feed line, the live-site ring with ticks, the Victory and Defeat banners, and the called-off line.
  - Pass condition: every capture is present and indexed in `hud-run-and-observe-evidence.md`. DEFERRED (full round). Blocked on spikes UI-V1, V3 and V4.
- **AC-2**: UXA-HUD-33 and UXA-HUD-06
  - Setup: the same session.
  - Verify: captures also exist for the assassination card, a ping ring with its label, an edge ring, and banners with summary, at 1080p and 16:10. Self-Active shows exactly the brace indicator (label and glyph), tally, stage card, own steps and rival steps as persistent elements, plus ally markers.
  - Pass condition: captures present and the persistent set matches (the latency icon is allowed only when shown).

QL-STORY-READY: ADEQUATE

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: UI
**Required evidence**:
- UI: the retained screenshots indexed in `production/qa/evidence/hud-run-and-observe-evidence.md` (which is also the sign-off), at 1080p and 16:10
- Gate: BLOCKING (UI default; not waived at `qa.level: minimal`)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/hud/story-023-hud-strike-strip-test-and-plate-evidence.md`, `production/epics/hud/story-025-hud-removal-card-widget-and-preempt.md`, `production/epics/hud/story-034-hud-informant-presentation.md`, `production/epics/hud/story-036-hud-team-feed-opponent-and-round-end-pie.md`, `production/epics/hud/story-038-hud-reconnect-lines-pie.md`, `production/epics/hud/story-041-hud-objectives-cluster-widgets.md`, `production/epics/hud/story-042-hud-end-banner-summary-called-off-lobby-widgets.md`, `production/epics/hud/story-043-hud-round-summary-pie.md`, `production/epics/hud/story-047-hud-latency-icon.md`, `production/epics/hud/story-048-hud-interact-ring-and-context-prompt.md`, `production/epics/hud/story-049-hud-team-ping-rings-l0.md`, `production/epics/hud/story-056-hud-layout-evidence-resolutions-and-budget.md`
- Unlocks: `production/epics/hud/story-061-hud-mvp-exit-5v5-presentation-verification.md`
