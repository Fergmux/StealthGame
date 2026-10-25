# Story 028: Leak suite: strike-target parity and look parity for every kind of non-ally subject

> **Epic**: HUD
> **Status**: Ready
> **Layer**: Presentation
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/hud.md`
**Requirement**: `TR-hud-002`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0008: Test and automation strategy for leak and parity tests (with ADR-0001 for identity blindness and ADR-0021 for the view-model dump)
**ADR Decision Summary**: ADR-0008 requires leak and parity tests to be automated: the multi-client and multi-process harness compares what a viewer's client holds for two worlds that differ only in the subject's kind. ADR-0001 makes one pawn class and one look path so nothing about a non-ally pawn reveals player-ness; ADR-0021 gives the HUD a deterministic `FHudViewModel::Dump()` (stable field order, fixed float precision) used by the leak suite (AC-HUD-01..07).
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Runs in PIE at a fixed 60 Hz step, identical seeds, TAA off, dumps keyed by tick index with wall-clock fields excluded. The "contact" oracle is the strike anim-notify tick on the attacker's client, not the HUD's F2 mapping. The multi-process harness and `StealthTestHarness` module come from `production/epics/information-visibility-policy/story-001-spike-s1-multiprocess-raw-bunch-capture.md` (ADR-0008). The strike hint depends on the camera aim ray (story 030 injects a backend; PIE uses the real camera). ADR-0001 is HIGH-risk on replication: every UE default that reveals player-ness must be overridden. `UE_LOGF` is preferred over `UE_LOG` in 5.8.

**Control Manifest Rules (this layer)**:
- Required: Presentation reads only its own controller's carriers, the GameState public structs and client-local queries — source: ADR-0001, ADR-0021
- Forbidden: Never iterate actors, read a pawn's `PlayerState`/`Controller`/`Owner`, call `IsPlayerControlled`/`IsBotControlled`/`CanViewerKnow`, include a `Public/Server/` header, or write anything replicated from Presentation — source: ADR-0021, ADR-0001
- Guardrail: HUD ≤ 0.3 ms CPU (AC-HUD-22), ≤ 0.5 ms with markers; UI GPU ≤ 0.5 ms at 1080p on the reference GPU — source: ADR-0021

This is the first half of the HUD leak suite. HUD-R3 (subject-kind parity) says that for two worlds differing only in the kind or team of a non-ally subject the viewer interacts with or looks at, the view-model is identical on every frame before the owner-only result is due, and afterwards differs only in the result. The two tests here prove it for the viewer's own strike and for the viewer's own gaze.

---

## Acceptance Criteria

*From GDD `design/gdd/hud.md` AC-HUD-01 (as amended by the QA-gate finding; the GDD owner is to align the GDD text) and AC-HUD-04 (verbatim); UX restatement UXA-HUD-32:*

- [ ] **AC1** (AC-HUD-01 as amended, `StealthGame.HUD.PIE.StrikeTargetParity`): **GIVEN** a 2v2 PIE (attacker A on team 0; enemy players E1, E2 on team 1, so striking E1 never ends the round; an NPC N at the same distance and bearing in a second run), the determinism rule and a running brace cooldown on A, **WHEN** A strikes N in run 1 and E1 in run 2, **THEN** A's dumps are equal on every tick before `t_show` (including the strike-hint hide during `Striking`, brace indicator and sweep, notices and cue events), and from `t_show` differ only in the result plate's text and stamp, the result cue id and the tally cluster (squares, numeral, triangle if n >= L-1, tally and threshold ticks), which changes only in the WrongTarget run.
- [ ] **AC2** (AC-HUD-04, `StealthGame.HUD.PIE.LookParity`; restated for a tester by UXA-HUD-32): **GIVEN** the viewer's camera held for 3 s on, in turn, an NPC, an enemy player, an enemy informant, an empty spot and a bush with a concealed enemy, all within strike reach, **WHEN** the dumps are compared, **THEN** the NPC, enemy and informant dumps are identical (strike hint shown), and the empty-spot and concealed-bush dumps are identical (no hint).

---

## Implementation Notes

*Derived from ADR-0008, ADR-0001 and ADR-0021 §4, GDD HUD-R2, HUD-R3, HUD-R8 and the determinism rule:*

**What to build.** Two CQTest PIE scenarios in `StealthTestHarness`, `NETWORK_TEST_CLASS(StrikeTargetParity, "StealthGame.HUD.PIE")` and `NETWORK_TEST_CLASS(LookParity, "StealthGame.HUD.PIE")`. No production presenter code is written in this story. A leak found by these tests is a bug in the story that owns the leaking element, not something to patch in the test.

**StrikeTargetParity.** Two runs with identical seeds under the determinism rule (fixed 60 Hz, identical seeds, dumps keyed by tick index, wall-clock fields excluded, TAA off). A has a running brace cooldown (so the sweep and numeral are compared, not expected to freeze). Run 1: A strikes the NPC N. Run 2: A strikes E1 at the same distance and bearing (N replaced by E1; E2 stays so striking E1 does not end the round). Compare A's `FHudViewModel::Dump()` and cue events for each tick:
- every tick before `t_show` equal, including the strike-hint hide during `Striking` (1.75 s), brace indicator and sweep, notices and cue events;
- from `t_show`: differences allowed only in the result plate's text and stamp ("Target eliminated" with the stepped square versus "Wrong target" with the upward triangle), the result cue id, and the tally cluster (squares, numeral, triangle if `n >= L-1`, tally tick and threshold tick), and the tally cluster changes only in the WrongTarget run (the QA gate found the GDD's wording of AC-HUD-01 too narrow against F6 and HUD-R7; the amended text above governs this story and the GDD owner aligns AC-HUD-01 and HUD-R3).

**LookParity.** Hold the viewer's camera for 3 s on each subject in turn, all within strike reach: an NPC, an enemy player, an enemy informant, an empty spot and a bush with a concealed enemy. Compare dumps: the NPC, enemy and informant dumps are identical (strike hint shown), and the empty-spot and concealed-bush dumps are identical (no hint). The viewer sees only rendered pawns: a concealed enemy in a bush produces no actor on the viewer's client, so its dump equals the empty spot's.

**Rules under test.** HUD-R2 (nothing on screen identifies, counts or locates a non-ally person; the strike hint's trigger is identical for every rendered non-ally pawn), HUD-R3 (the leak rule), HUD-R8 (the hint predicate reads no team, controller, kind, concealment or `PlayerState` field), HUD-R7 (the tally moves with the plate).

**Traps.** The hint must be hidden during `Striking` for both runs identically; a hint that returns early for one kind is a leak. A "marked ally" is a pawn whose public handle is in the own roster; the ally case is not in this scenario (allies are excluded from the strike hint by story 030). Tests must not use `CanViewerKnow` or read server stores to decide expectations: they compare dumps only.

**Prototype.** The prototype HUD's team-field read (replicated on every pawn) is exactly the leak this suite catches; it is never promoted (EPIC prototype disposition).

**Out of this story.** The no-warning-before-contact cases (story 029), the strike strip pixel diff (story 023), and the include-boundary lint (story 004).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/hud/story-029-hud-leak-suite-no-warning-before-contact.md`: AC-HUD-05 and the restated no-warning clause of UXA-HUD-32.
- `production/epics/hud/story-023-hud-strike-strip-test-and-plate-evidence.md`: the AC-HUD-07 pixel diff of the HUD layers.
- `production/epics/hud/story-030-hud-strike-hint-predicate-and-lockout.md`: the hint predicate and lockout logic under test here.
- `production/epics/hud/story-022-hud-result-and-defence-plate-flow.md`: the plates, stamps and cue ids compared here.
- `production/epics/hud/story-004-hud-include-boundary-and-forbidden-api-lint.md`: the compile-time and grep guard that proves what the presenter reads.
- `production/epics/hud/story-036-hud-team-feed-opponent-and-round-end-pie.md`: opponent-visibility leak checks for the team feed.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthTestHarness/Private/HUD/StrikeTargetParityTest.cpp` (CQTest, `StealthGame.HUD.PIE.StrikeTargetParity.*`)
- `Source/StealthTestHarness/Private/HUD/LookParityTest.cpp` (CQTest, `StealthGame.HUD.PIE.LookParity.*`)
- `Source/StealthTestHarness/Private/HUD/HudPieFixtures.h` and `.cpp` (shared 2v2 fixture, dump-by-tick comparer; created by an earlier HUD PIE story if present, otherwise here)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: AC-HUD-01 `StrikeTargetParity` (as amended)
  - Given: 2v2 PIE, A on team 0, E1 and E2 on team 1, an NPC N at the same distance and bearing in a second run, a running brace cooldown on A, the determinism rule.
  - When: A strikes N (run 1) and E1 (run 2).
  - Then: every tick before `t_show` is equal (including strike-hint hide during `Striking`, brace indicator and sweep, notices, cues). From `t_show` the dumps differ only in the result plate text and stamp, the result cue id, and the tally cluster (squares, numeral, triangle if n >= L-1, tally and threshold ticks), which changes only in the WrongTarget run.
  - Edge cases: the GDD owner aligns AC-HUD-01 and HUD-R3 with F6.
- **AC-2**: AC-HUD-04 `LookParity`
  - Given: the camera held 3 s on an NPC, an enemy player, an enemy informant, an empty spot and a bush with a concealed enemy, all within strike reach.
  - When: the dumps are compared.
  - Then: the NPC, enemy and informant dumps are identical (strike hint shown). The empty-spot and concealed-bush dumps are identical (no hint).
  - Edge cases: none.

QL-STORY-READY: ADEQUATE (qa-lead, 2026-10-25; criteria revised after the first pass and re-judged)

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/HUD/StrikeTargetParityTest.cpp` (`StealthGame.HUD.PIE.StrikeTargetParity`) and `Source/StealthTestHarness/Private/HUD/LookParityTest.cpp` (`StealthGame.HUD.PIE.LookParity`) — must exist and pass (gate BLOCKING)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/hud/story-022-hud-result-and-defence-plate-flow.md`, `production/epics/hud/story-030-hud-strike-hint-predicate-and-lockout.md`, `production/epics/hud/story-004-hud-include-boundary-and-forbidden-api-lint.md`; `production/epics/information-visibility-policy/story-001-spike-s1-multiprocess-raw-bunch-capture.md`
- Unlocks: `production/epics/hud/story-023-hud-strike-strip-test-and-plate-evidence.md`, `production/epics/hud/story-029-hud-leak-suite-no-warning-before-contact.md`, `production/epics/hud/story-061-hud-mvp-exit-5v5-presentation-verification.md`
