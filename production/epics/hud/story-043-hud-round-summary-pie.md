# Story 043: PIE proof of the round summary (AC-HUD-27, MVP item 9)

> **Epic**: HUD
> **Status**: Ready
> **Layer**: Presentation
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/hud.md`
**Requirement**: `TR-hud-005`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0021: UI/HUD layering and Common UI (the round summary decision), with ADR-0008: Test automation, leak and parity (the PIE harness)
**ADR Decision Summary**: The round summary (HUD-E21) is two string-table lines filled from the integer `FRoundResult.TeamSummary` carried in the public round result, shown with the end banner at its F10 time, only in `Ended`; no text travels on the wire. ADR-0008 provides the multi-client CQTest PIE harness with a fixed 60 Hz step, tick-indexed dumps with wall-clock fields excluded and the Round test seam used to inject results.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Tests are CQTest `NETWORK_TEST_CLASS` PIE tests in `StealthTestHarness` using the determinism rule of the GDD (fixed step 60 Hz, identical seeds). The HUD side exercises the same sampled-by-value presenter path on host and clients (no RepNotify dependency). The harness comes from IVP story 001 (ADR-0008). Strings are compared by exact characters (middle dots, em dashes). `UE_LOGF` is preferred over `UE_LOG` in 5.8.

**Control Manifest Rules (this layer)**:
- Required: The round summary (HUD-E21) is two string-table lines filled from the integer `FRoundResult.TeamSummary`, shown with the end banner at its F10 time, only in `Ended` — source: ADR-0021 (user decision 2026-10-25)
- Forbidden: Never carry text on the wire — source: ADR-0021
- Guardrail: HUD ≤ 0.3 ms CPU (AC-HUD-22), ≤ 0.5 ms with markers; UI GPU ≤ 0.5 ms at 1080p on the reference GPU — source: ADR-0021

This story is the PIE proof of MVP item 9. It runs a 2v2 round to its end and checks the summary and banner strings, timing and absence rules on every client, with the Round epic's test seam injecting the summary values the organic round cannot produce.

---

## Acceptance Criteria

*From GDD `design/gdd/hud.md` AC-HUD-27 and UX spec `design/ux/hud.md` UXA-HUD-20, scoped to this story:*

- [ ] **AC1** (AC-HUD-27, `StealthGame.HUD.PIE.RoundSummary`, with the fixture resolved by the qa-gate finding; decision logged): **GIVEN** a 2v2 PIE round that ends at a final-objective `Ended` won by team 0, with `FRoundResult.TeamSummary` values {steps 3, eliminated 1, wrong targets 2} for team 0 and {steps 2, eliminated 2, wrong targets 0} for team 1 injected through the Round test seam (round-match-framework story 054 fixture), because an organic 2v2 round cannot end by objective win with team 1 fully eliminated **WHEN** `Ended` arrives **THEN** each client shows the summary at its banner's F10 show time, own house first: team-0 clients "Your house — steps 3 of 3 · eliminated 1 · wrong targets 2" / "Rival house — steps 2 of 3 · eliminated 2 · wrong targets 0", team-1 clients the mirror; no summary string or count appears in any dump before `Ended`; a called-off round shows no summary; the strings match no time format.
- [ ] **AC2** (UXA-HUD-20, `StealthGame.HUD.PIE.RoundSummary` banner and summary): **GIVEN** the same round and a called-off round **WHEN** the dumps are read **THEN** the banner shows "Victory" or "Defeat", exactly one of "completed the final objective" or "every rival eliminated for the round", and "Your house {a} · Rival house {b}"; the summary is exactly the two approved lines, own house first, and is absent before `Ended` and in a called-off round.

---

## Implementation Notes

*Derived from ADR-0008 (PIE harness) and `design/gdd/hud.md` HUD-R14, AC-HUD-27:*

**What to build.** One CQTest PIE scenario file with `NETWORK_TEST_CLASS(RoundSummary, "StealthGame.HUD.PIE")`, full name `StealthGame.HUD.PIE.RoundSummary.<Method>`. The 2v2 fixture, the per-client `FHudViewModel::Dump()` capture and the tick-indexed comparison come from IVP story 001 and story 003 of this epic. No new HUD production code is expected here; a failing case is fixed in story 040 (logic) or 042 (widgets).

**The fixture resolution (decision logged).** AC-HUD-27's literal scenario (team 0 wins at the final objective while team 1 has both members eliminated) cannot arise organically: a round with both team-1 members eliminated ends by elimination, not by objective win. The qa-lead gate therefore resolved it as follows: run an ordinary 2v2 round to a final-objective `Ended`, and inject the two `FRoundResult.TeamSummary` values {steps 3, eliminated 1, wrong targets 2} and {steps 2, eliminated 2, wrong targets 0} through the Round test seam (round-match-framework `story-054-team-summary-at-ended.md`). The HUD is tested for display only: the strings, timing and absence rules are verbatim, the data origin is Round's (round-match-framework story 054; wrong targets are counted from the Event Log's `KillResolved` rows, `production/epics/npc-kill-penalty-informants/story-004-event-log-rows.md`). Do not add a HUD-side derivation to make the organic round produce the numbers, and do not weaken the strings.

**What each client must show.**
- Team-0 clients: "Your house — steps 3 of 3 · eliminated 1 · wrong targets 2" and "Rival house — steps 2 of 3 · eliminated 2 · wrong targets 0". Team-1 clients: the mirror (own house first). Both at the client's own banner F10 show time (a bystander shows at its `t_phase`; a team-0 viewer with a pending own plate waits per F10), so the test reads the dump at the tick the banner appears rather than at a fixed delay.
- Before `Ended`: scan every dump from round start and assert no summary string, no `steps … of 3` fragment and no summary count exists (the summary counts exist only in `Ended`).
- Called-off round: run a second round that is called off (`bCalledOff` in `Reset`) and assert no summary and no banner reason line (the called-off line is the only text).
- No time format: assert both strings match none of the clock patterns (`m:ss`, seconds suffix).

**Banner (UXA-HUD-20).** Assert "Victory" for team 0 and "Defeat" for team 1, exactly one reason phrase ("completed the final objective" for this objective win), and "Your house {a} · Rival house {b}" from `MatchScore`, using the approved characters of the UX string table (not the ASCII renderings).

**Evidence.** Capture `production/qa/evidence/hud-round-summary-1080p.png` showing the summary on both team viewpoints (a split or two captures named in the sign-off).

**Does not touch.** Summary and banner logic (story 040), the widgets (story 042), the Round-side `TeamSummary` computation (round-match-framework).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/hud/story-040-hud-end-banner-f10-and-round-summary-logic.md`: banner time, text and summary values (Logic proof).
- `production/epics/hud/story-042-hud-end-banner-summary-called-off-lobby-widgets.md`: the widgets and their layout screenshots.
- `production/epics/hud/story-053-hud-no-clock-no-counts-pie.md`: the sweep that no clock or count appears in any phase before `Ended`.
- `production/epics/round-match-framework/story-054-team-summary-at-ended.md`: computing and publishing `TeamSummary`.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthTestHarness/Private/HUD/RoundSummaryTest.cpp` (CQTest, `NETWORK_TEST_CLASS(RoundSummary, "StealthGame.HUD.PIE")`, `StealthGame.HUD.PIE.RoundSummary.*`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: AC-HUD-27 `RoundSummary` with injected summary
  - Given: 2v2 PIE ending at a final-objective `Ended`, with `FRoundResult.TeamSummary` values {steps 3, eliminated 1, wrong targets 2} and {2, 2, 0} injected through the Round test seam (round-match-framework story 054 fixture).
  - When: `Ended` arrives.
  - Then: each client shows the summary at its banner's F10 time, own house first. Team-0 clients show "Your house — steps 3 of 3 · eliminated 1 · wrong targets 2" and "Rival house — steps 2 of 3 · eliminated 2 · wrong targets 0". Team-1 clients show the mirror. No summary string or count appears in any dump before `Ended`. A called-off round shows no summary. The strings match no time format.
  - Edge cases: an organic 2v2 round cannot end by objective win with team 1 fully eliminated, hence the injection. Compare against the UX string table's exact characters.
- **AC-2**: UXA-HUD-20 banner and summary
  - Given: the same round, and a called-off round.
  - When: dumps are read.
  - Then: the banner shows "Victory" or "Defeat", exactly one of "completed the final objective" or "every rival eliminated for the round", and "Your house {a} · Rival house {b}". The summary is exactly the two approved lines, own house first, absent before `Ended` and in a called-off round.
  - Edge cases: `hud-round-summary-1080p.png` retained for both team viewpoints.

QL-STORY-READY: ADEQUATE (qa-lead, 2026-10-25; criteria revised after the first pass and re-judged)

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/HUD/RoundSummaryTest.cpp`, test `StealthGame.HUD.PIE.RoundSummary` OR a documented playtest; plus the retained capture `production/qa/evidence/hud-round-summary-1080p.png` (summary on both team viewpoints). Gate level: BLOCKING (the Integration default).

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/hud/story-040-hud-end-banner-f10-and-round-summary-logic.md`, `production/epics/hud/story-042-hud-end-banner-summary-called-off-lobby-widgets.md`; `production/epics/round-match-framework/story-054-team-summary-at-ended.md`; `production/epics/npc-kill-penalty-informants/story-004-event-log-rows.md`
- Unlocks: `production/epics/hud/story-060-hud-run-and-observe-2v2-evidence.md`, `production/epics/hud/story-061-hud-mvp-exit-5v5-presentation-verification.md`
