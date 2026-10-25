# Story 061: MVP exit verification: final 5v5 evidence pass over the Presentation surfaces

> **Epic**: HUD
> **Status**: Blocked — BLOCKED: spikes UI-V1 (teammate-markers story 018), UI-V3 (story 012), UI-V4 (story 008) and ADR-0001 S2/S14 (IVP stories 002, 003) have not reported
> **Layer**: Presentation
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/hud.md`
**Requirement**: `TR-hud-002`, `TR-hud-005`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0021: UI/HUD layering, CommonUI and Enhanced Input (with ADR-0008 and ADR-0001)
**ADR Decision Summary**: ADR-0001 fixes the audience carriers and identity blindness that the leak suite proves; ADR-0008 provides the multi-client PIE harness, the determinism rule and the leak and parity suite that must be re-run green at a tagged commit; ADR-0021 fixes the HUD layer stack whose every Presentation surface present at MVP is shown on screen in the final pass. ADR-0001 S2 and S14 (rejoin and seamless-travel behaviour) are conditional gates on the reconnect and rebuild surfaces.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Blocked on ADR-0021 UI-V1, UI-V3 and UI-V4 (zero-lag projection, layer order with the camera fade, DPI rule and root scaler) and on ADR-0001 spikes S2 and S14 (IVP stories 002 and 003), which condition the rejoin, seamless-travel and restart behaviour the reconnect lines and the rebuild surfaces rely on. The capture build is a cooked Development standalone build (ADR-0022: performance and look claims come from cooked Development standalone, not PIE); the leak and parity suite is re-run in Development Editor PIE at the same tagged commit. ADR-0018 notes MotionWarping is Beta in 5.8.4 and the networking domain is HIGH risk. `UE_LOGF` is preferred over `UE_LOG` in 5.8.

**Control Manifest Rules (this layer)**:
- Required: Presentation reads only its own controller's carriers, the GameState public structs and client-local queries — source: ADR-0001, ADR-0021
- Forbidden: Never iterate actors, read a pawn's `PlayerState`/`Controller`/`Owner`, call `IsPlayerControlled`/`IsBotControlled`/`CanViewerKnow`, include a `Public/Server/` header, or write anything replicated from Presentation — source: ADR-0021, ADR-0001
- Guardrail: HUD: ≤ 0.3 ms CPU (AC-HUD-22), ≤ 0.5 ms with markers; UI GPU ≤ 0.5 ms at 1080p on the reference GPU (inside ADR-0022's frame budget) — source: ADR-0021

This is the MVP exit gate for the Presentation layer. It does not add behaviour: it plays a 5v5 round on a cooked Development standalone build, captures every Presentation surface from both teams' viewpoints, re-runs the leak and parity suite on the same tagged commit, and writes the exit record that cites every Presentation epic's evidence. It owns the final MVP evidence pass together with story 060.

---

## Acceptance Criteria

*From the HUD epic's Build Order and Gates and GDD `design/gdd/hud.md` (AC-HUD-01..07, -11, -12, -20, MVP items 9 and 10), scoped to this story:*

- [ ] **AC1** (derived, EPIC exit gate, `hud-mvp-exit-evidence.md`): **GIVEN** a 5v5 round on a cooked Development standalone build (commit hash recorded in `hud-mvp-exit-evidence.md`) **WHEN** it is played to an objective win **THEN** retained screenshots from a team-0 and a team-1 viewpoint show every Presentation surface present at MVP (HUD clusters, plates, card, feed incl. reconnect lines, banner with round summary, markers, ping rings), indexed in `production/qa/evidence/hud-mvp-exit-evidence.md`; MVP item 9 (round summary) and item 10 (reconnect lines) are each shown on screen.
- [ ] **AC2** (derived, ADR-0008 and the GDD leak and parity ACs): **GIVEN** the tagged commit of AC1 **WHEN** the leak and parity suite (AC-HUD-01..07, -11, -12, -20) is re-run in Development Editor PIE **THEN** it is green at the same tagged commit and its log is retained; any red is recorded as an open defect against the owning story, never waived.
- [ ] **AC3** (derived, EPIC exit record): **GIVEN** the evidence of the hud, teammate-markers and informant-pings (rings) epics **WHEN** the exit record is reviewed **THEN** it lists, per Presentation epic (hud, teammate-markers, informant-pings rings), the stories whose evidence it cites and the lead sign-off line; items owned elsewhere (the 10-player network pass) are named as out of scope.

---

## Implementation Notes

*Derived from the HUD EPIC, ADR-0008 and `.claude/docs/run-and-observe.md`:*

**Pass structure.** (1) Tag the commit and record its hash. (2) Build a cooked Development standalone and play a 5v5 round (ten clients on the multi-process harness, not in-process PIE with a lowered crowd) to an objective win, with at least one removal per team, one reconnect so the feed shows "{Ally} — reconnecting" and "{Ally} — back", ping use, and a banner with the round summary at `Ended`. (3) Capture from a team-0 viewpoint and a team-1 viewpoint at 1080p (names `hud-mvp-5v5-team0-1080p.png`, `hud-mvp-5v5-team1-1080p.png`, plus any surface-specific captures the index adds) showing: persistent clusters (brace, tally, objectives, latency icon where shown), plates and notices, the removal card, the feed including reconnect lines (MVP item 10 on screen), the banner with the two summary lines (MVP item 9 on screen), teammate markers (teammate-markers epic) and ping rings (informant-pings epic). (4) Index every capture in `production/qa/evidence/hud-mvp-exit-evidence.md` with team viewpoint, seed, build, tagged commit.

**Leak and parity suite (AC2).** Re-run in Development Editor PIE at the same tagged commit: AC-HUD-01 StrikeTargetParity, -02 InformantNpcParity, -03 TallyNotBeforePlate, -04 LookParity, -05 NoWarningBeforeContact (with the ADR-0021 §8 gate and the presenter bound), -06 OpponentNoTeamLines, -07 strike strip capture diff, -11 IncludeCheck, -12 NoClockNoCounts (story 053), -20 HostParity (story 046). Retain the full log (`production/qa/evidence/hud-mvp-exit-leak-parity-log.txt`). Determinism rule applies (60 Hz fixed step, identical seeds, dumps keyed by tick index, wall-clock fields excluded). Any red is recorded as an open defect against its owning story with the failing assertion; waivers are not allowed.

**Exit record (AC3).** `hud-mvp-exit-evidence.md` has a table per Presentation epic: hud (stories whose evidence is cited, including 023, 026, 028, 029, 036, 038, 043, 045, 046, 053, 054, 056, 060), teammate-markers (its evidence and exit stories), informant-pings rings (its ring UI evidence story `production/epics/informant-pings-team-comms/story-021-ring-ui-evidence-and-blind-sort.md`), each with the lead sign-off line. Items owned elsewhere are named out of scope: the 10-player network pass belongs to `production/epics/procedural-city-generator/EPIC.md` and is not repeated here.

**Traps.** In-process 10-world PIE with a lowered crowd is not a 5v5 round on a cooked standalone; use it only for the suite re-run, not the screenshot pass. Screenshots must come from a tagged commit, not a dirty tree. Do not cite per-story evidence taken at earlier commits as if current without noting the commit; prefer re-capture. The informant-pings rings and teammate-markers evidence stories are separate epics' work; this story cites them, it does not redo them.

**Decisions that apply.** D16, D20 as in story 060; the MVP inventory additions of 2026-10-25 (round summary HUD-E21, ping rings HUD-E22, reconnect lines HUD-R10a). Out of this story: the 10-player network pass, any fix of a defect found (filed against its owner).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/hud/story-060-hud-run-and-observe-2v2-evidence.md`: the 2v2 run-and-observe set this story builds on.
- `production/epics/hud/story-046-hud-host-parity-pie.md`, `story-053-hud-no-clock-no-counts-pie.md`, `story-028-hud-leak-suite-strike-and-look-parity.md`, `story-029-hud-leak-suite-no-warning-before-contact.md`: the individual suite members re-run here.
- `production/epics/hud/story-036-hud-team-feed-opponent-and-round-end-pie.md`, `story-038-hud-reconnect-lines-pie.md`, `story-043-hud-round-summary-pie.md`, `story-045-hud-rebuild-no-replay-pie.md`: the per-surface proofs.
- `production/epics/procedural-city-generator/EPIC.md`: owns the 10-player network pass.
- The teammate-markers and informant-pings epics' own evidence stories.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `tools/dev/hud_mvp_exit_pass.ps1` (tags the commit, launches the cooked standalone 5v5 session and the suite re-run; planned)
- Evidence: `production/qa/evidence/hud-mvp-exit-evidence.md`, `production/qa/evidence/hud-mvp-5v5-team0-1080p.png`, `production/qa/evidence/hud-mvp-5v5-team1-1080p.png`, `production/qa/evidence/hud-mvp-exit-leak-parity-log.txt`

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: 5v5 evidence pass
  - Given: a 5v5 round on a cooked Development standalone build, with the commit hash recorded in `hud-mvp-exit-evidence.md`.
  - When: it is played to an objective win.
  - Then: retained screenshots from a team-0 and a team-1 viewpoint show every Presentation surface present at MVP (HUD clusters, plates, card, feed including reconnect lines, banner with round summary, markers, ping rings), indexed in `hud-mvp-exit-evidence.md`. MVP item 9 (round summary) and item 10 (reconnect lines) are each shown on screen.
  - Edge cases: DEFERRED until spikes UI-V1, V3, V4 and IVP stories 002 and 003 report.
- **AC-2**: leak and parity suite re-run
  - Given: the tagged commit of AC1.
  - When: AC-HUD-01..07, -11, -12 and -20 run in Development Editor PIE.
  - Then: all are green and the log is retained. Any red is recorded as an open defect against the owning story, never waived.
  - Edge cases: none.
- **AC-3**: exit record
  - Given: the evidence of the hud, teammate-markers and informant-pings rings epics.
  - When: the exit record is reviewed.
  - Then: it lists, per Presentation epic, the stories whose evidence it cites, and the lead sign-off line. The 10-player network pass is named as out of scope (procedural-city-generator epic).
  - Edge cases: none.

QL-STORY-READY: ADEQUATE (qa-lead, 2026-10-25; criteria revised after the first pass and re-judged)

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: the retained exit record `production/qa/evidence/hud-mvp-exit-evidence.md`, the screenshots `hud-mvp-5v5-team0-1080p.png` and `hud-mvp-5v5-team1-1080p.png`, and the retained leak/parity suite log (the existing suite tests are the automated half; this story adds no new test file)
- Gate: BLOCKING

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/hud/story-060-hud-run-and-observe-2v2-evidence.md`, `production/epics/hud/story-028-hud-leak-suite-strike-and-look-parity.md`, `production/epics/hud/story-029-hud-leak-suite-no-warning-before-contact.md`, `production/epics/hud/story-036-hud-team-feed-opponent-and-round-end-pie.md`, `production/epics/hud/story-038-hud-reconnect-lines-pie.md`, `production/epics/hud/story-043-hud-round-summary-pie.md`, `production/epics/hud/story-045-hud-rebuild-no-replay-pie.md`, `production/epics/hud/story-046-hud-host-parity-pie.md`, `production/epics/hud/story-053-hud-no-clock-no-counts-pie.md`; teammate-markers epic and informant-pings-team-comms epic evidence stories (`production/epics/informant-pings-team-comms/story-021-ring-ui-evidence-and-blind-sort.md`); `production/epics/procedural-city-generator/EPIC.md` (owns the 10-player network pass; this story does not repeat it)
- Unlocks: None (epic exit)
