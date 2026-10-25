# Story 053: PIE proof that no displayed string is a clock, an opponent count or a banned word

> **Epic**: HUD
> **Status**: Ready
> **Layer**: Presentation
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/hud.md`
**Requirement**: `TR-hud-002`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0008: Test and automation strategy for leak and parity tests (with ADR-0001 for the language and identity-blindness rules)
**ADR Decision Summary**: ADR-0008 gives the PIE leak harness: fixed-step multi-client runs whose per-client view-model dumps are inspected for anything that would reveal a non-ally person or the round's hidden state. ADR-0001 makes identity blindness a project rule; the HUD language rule (no clock, no opponent count, no banned word) is its text-level enforcement, linted statically over `ST_Hud` (story 005) and proved at runtime by this story over every string actually displayed.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: CQTest multi-client PIE on the StealthTestHarness module with fixed 60 Hz step; the strings are collected from the deterministic `FHudViewModel::Dump()` of each client (no UMG read-back), so the test does not depend on any ADR-0021 UI-V item. ADR-0018 notes MotionWarping is Beta in 5.8.4 and the networking domain is HIGH risk, relevant because the eliminations in the 3-minute round are real strikes. `UE_LOGF` is preferred over `UE_LOG` in 5.8.

**Control Manifest Rules (this layer)**:
- Required: Player-facing text comes from string tables — source: ADR-0021
- Forbidden: Never carry text on the wire — source: ADR-0021
- Guardrail: HUD: ≤ 0.3 ms CPU (AC-HUD-22), ≤ 0.5 ms with markers; UI GPU ≤ 0.5 ms at 1080p on the reference GPU (inside ADR-0022's frame budget) — source: ADR-0021

Story 005 lints the string table; this story proves the running HUD never displays anything the table's templates do not allow, which also catches strings assembled from data (for example a display name that is a number).

---

## Acceptance Criteria

*From GDD `design/gdd/hud.md` (AC-HUD-12) and UX spec `design/ux/hud.md` (UXA-HUD-21), scoped to this story:*

- [ ] **AC1** (AC-HUD-12, `StealthGame.HUD.NoClockNoCounts`, made mechanical by the QA gate): **GIVEN** an Active round of 3 minutes with eliminations on both teams, **WHEN** every displayed string is collected, **THEN** none matches a time format (`\d+:\d\d` outside "Brace in"), an opponent count, "left", "remaining", or a banned word (dead, lives, revive, spectator, ghost, tracked, registered, detected, spotted). Mechanical form: every collected string is checked against a whitelist (strings with digits must match the Active-round templates Tally.Line, Brace.Cooling, Countdown, Banner.Score) and any other digit-bearing string fails, as does a match for a time format outside Brace.Cooling and Countdown, "left", "remaining" or a banned word; Summary strings are `Ended`-only and out of this collection window; fixtures use digit-free display names (a name containing a digit would fail the whitelist).
- [ ] **AC2** (UXA-HUD-21, `StealthGame.HUD.NoClockNoCounts`): **GIVEN** the same 3-minute Active round with eliminations on both teams **WHEN** the same collection runs **THEN** no displayed string matches a time format outside "Brace in m:ss" and the returning countdown, an opponent count, "left", "remaining", or a banned word (dead, lives, revive, spectator, ghost, tracked, registered, detected, spotted).

---

## Implementation Notes

*Derived from ADR-0008 and the GDD language rule (HUD-R2, AC-HUD-12):*

**What to build.** A CQTest file `NETWORK_TEST_CLASS(NoClockNoCounts, "StealthGame.HUD.PIE")` (full name `StealthGame.HUD.PIE.NoClockNoCounts.*`; the GDD's short name `StealthGame.HUD.NoClockNoCounts` is the AC alias) that runs a 2v2 or larger PIE round for 180 s of `Active` game time at a fixed 60 Hz step, produces eliminations on both teams (a strike win on each side, an exile through three wrong targets, and at least one reversal so Result, defence, card, removal-line and countdown strings all occur), and collects every string that any client's dump displays: persistent clusters, plates, notices, feed lines, card, banner. The collection window ends at `Ended`; Summary strings (`Hud.Summary.Own` / `.Rival`) exist only at `Ended` and are outside it.

**The mechanical check (QA gate wording).** Build the whitelist from the Active-round templates of the approved table (`design/ux/hud.md` lines 257-290): `Hud.Tally.Line` ("Wrong targets: {n}"), `Hud.Brace.Cooling` ("Brace in {m}:{ss}"), `Hud.Countdown` ("Returning in {s}"), `Hud.Banner.Score` ("Your house {a} · Rival house {b}"). Rule 1: any collected string that contains a digit must match one of these templates (by regex derived from the template) or the test fails. Rule 2: a time format `\d+:\d\d` is allowed only inside `Brace.Cooling` and `Countdown` instances. Rule 3: none of "left", "remaining" (whole word, case-insensitive) or the banned words dead, lives, revive, spectator, ghost, tracked, registered, detected, spotted appears anywhere. Rule 4: a digit-bearing string that fits a template but whose number is an opponent count is not detectable by regex, so the fixtures use digit-free display names and the `Wrong targets: {n}` value is cross-checked against the owner's own tally (it is never the opponents'). The `{ally}` names in the feed lines come from C-Public `DisplayName`; "Ally" when unresolved.

**Why the whitelist, not only a deny-list.** The deny-list in AC-HUD-12 would pass an unexpected "7 players" or a surprise "3:00" in a new template; the whitelist makes a new digit-bearing template an explicit decision (it must be added here and in story 005's allowlist together). Keep the two allowlists in one shared data file in `Source/StealthPresentation/Private/Tests/HUD/` so they cannot drift (story 005 owns the file; import it, do not copy).

**UXA-HUD-21.** The same collection satisfies the UX wording: only "Brace in m:ss" and the returning countdown may look like a clock, and nothing says how many opponents remain, "left" or "remaining".

**Traps.** A display name containing a digit would fail the whitelist and is a fixture error, not a product bug (the fixtures say so). Do not widen the whitelist to make a run pass. The 3-minute window is game time under the fixed step, not wall-clock. Dumps are keyed by tick index; wall-clock fields are excluded. The banner and its score appear at the very end of an Active round when an objective win occurs; include the banner only if the round ends inside the window, and keep Summary strings out.

**Decisions that apply.** D11 and D29 (the approved wording the templates come from). Out of this story: the static string-table lint and the reflection test (story 005), the banner and summary logic (story 040), the feed queue (story 035).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/hud/story-005-hud-string-table-and-language-lint.md`: the static `ST_Hud` lint and allowlist file.
- `production/epics/hud/story-040-hud-end-banner-f10-and-round-summary-logic.md`: banner text and summary lines (logic).
- `production/epics/hud/story-035-hud-team-feed-queue-f8-and-removal-lines.md`: the feed lines the collection reads.
- `production/epics/hud/story-043-hud-round-summary-pie.md`: the `Ended`-only summary strings.
- `production/epics/hud/story-036-hud-team-feed-opponent-and-round-end-pie.md`: opponent-facing feed parity.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthTestHarness/Private/HUD/NoClockNoCountsTest.cpp` (CQTest, `StealthGame.HUD.PIE.NoClockNoCounts.*`; GDD alias `StealthGame.HUD.NoClockNoCounts`)
- `Source/StealthPresentation/Private/Tests/HUD/HudLanguageAllowlist.h` (shared whitelist and banned-word list owned by story 005; imported here)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: AC-HUD-12 `NoClockNoCounts`, mechanical form
  - Given: a 3-minute Active round with eliminations on both teams, fixtures using digit-free display names.
  - When: every displayed string is collected from the dumps.
  - Then: any digit-bearing string must match an Active-round template (Tally.Line, Brace.Cooling, Countdown, Banner.Score); any other digit-bearing string fails. A time format outside Brace.Cooling and Countdown fails, as does "left", "remaining" or a banned word (dead, lives, revive, spectator, ghost, tracked, registered, detected, spotted).
  - Edge cases: Summary strings are `Ended`-only and out of this collection window.
- **AC-2**: UXA-HUD-21
  - Given: the same 3-minute round.
  - When: the same collection runs.
  - Then: no time format outside "Brace in m:ss" and the returning countdown, no opponent count, "left", "remaining" or banned word.
  - Edge cases: none.

QL-STORY-READY: ADEQUATE (qa-lead, 2026-10-25; criteria revised after the first pass and re-judged)

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: the CQTest PIE test `Source/StealthTestHarness/Private/HUD/NoClockNoCountsTest.cpp` (`StealthGame.HUD.PIE.NoClockNoCounts.*`)
- Gate: BLOCKING

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/hud/story-005-hud-string-table-and-language-lint.md`, `production/epics/hud/story-040-hud-end-banner-f10-and-round-summary-logic.md`, `production/epics/hud/story-035-hud-team-feed-queue-f8-and-removal-lines.md`
- Unlocks: `production/epics/hud/story-061-hud-mvp-exit-5v5-presentation-verification.md`
