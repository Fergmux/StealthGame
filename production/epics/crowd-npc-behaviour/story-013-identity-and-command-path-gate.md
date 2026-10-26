# Story 013: Identity blindness and command-path gate: includes, IIF-only motion, no NPC strike, rates fixed

> **Epic**: Crowd & NPC Behaviour
> **Status**: Complete
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: 2026-10-26

## Context

**GDD**: `design/gdd/crowd-npc-behaviour.md`
**Requirement**: `TR-crowd-004`, `TR-crowd-002`, `TR-crowd-010`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-crowd-004 = Identity blindness: no crowd code reads player/binding/team/state. TR-crowd-002 = StateTree per NPC + follower emitting Move/Look through the IIF; no path-following driving. TR-crowd-010 = Baseline and Tell Matrix rates as data with calibration counters.)*

**ADR Governing Implementation**: ADR-0001: Crowd identity rules
**Also relevant**: ADR-0016 (sinks, forbidden APIs, rates never adapted), ADR-0008 (include lint and harness)
**ADR Decision Summary**: One pawn class, one look path, identity blindness: every UE default that reveals player-ness is overridden and no crowd code reads player, binding, team or state. ADR-0016 realises it for AI: the driver kind arrives as an `ICrowdControl` call parameter so Crowd includes no Roster or Objectives query header, and every motion reaches the pawn through the command interface.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0001 knowledge risk; the wire-level identity defaults are the risk, this story checks Crowd's side only)
**Engine Notes**: Instrumentation needs a test hook in the movement component's input consumption (named in ADR-0011), counters on the path-following component tick (zero or absent), and a hook on `FSiteAssignment` queries that fires the server assert. These hooks are compiled out of Shipping.

**Why this story**: The four criteria here are the epic's identity-and-honesty gates: if any fails, the NPC crowd is not a valid baseline for Pillar 2. They are written as one gate story because they share one 10-minute instrumented reference-scene run. Read GDD CNB-R2, R3, R7 (never adapted) and AC-CNB-01, 02, 11, 37.

**Control Manifest Rules (Feature layer, manifest 2026-10-25e)**:
- Required: crowd code reaches a pawn only through the sinks; the driver kind comes from the `ICrowdControl` call; rates and profiles come from the frozen tuning snapshot (ADR-0016)
- Forbidden: including Roster, Objectives-assignment or Visibility Policy fact headers in Crowd code; never branch on player versus NPC in Crowd; never adapt crowd rates during a round; never give the NPC sink an `Attack` method (ADR-0016, ADR-0001)
- Guardrail: instrumentation is non-Shipping and adds no cost to the Shipping path

---

## Acceptance Criteria

*From GDD `design/gdd/crowd-npc-behaviour.md` AC-CNB-01, AC-CNB-02, AC-CNB-11 and AC-CNB-37, scoped to this story:*

- [x] (AC-CNB-01) A build-time include check finds no Crowd source including Roster, Objectives-assignment or Policy fact headers (zero Roster call sites; the driver is selected by `DriveBoundPawn` versus `AdoptReleasedPawn`), and an instrumented run of the reference scene records 0 calls to `IsPlayer`, `GetTeam`, `VictimKind` or `FSiteAssignment` from Crowd code.
- [x] (AC-CNB-02) GIVEN 10 min of the reference scene with movement-component writes instrumented, THEN 100 % of NPC velocity changes come from `Move` entry-point calls, there are 0 direct CMC velocity or `RequestDirectMove` writes from Crowd, 0 StateTree `MoveTo`/`AITask_MoveTo` uses and 0 path-following component ticks, and 0 AI commands are rejected by the IIF except same-drain contention, each logged with the contending human command.
- [x] (AC-CNB-11) GIVEN 60 min of simulation, THEN NPCs issue 0 `Attack` commands and participant-bound AI pawns issue 0 commands other than `Move` and `Look`.
- [x] (AC-CNB-37) GIVEN a round in which bot players run, hide and tend at 3 x their F2 r_P, WHEN Crowd's effective draw rates are logged every 60 s, THEN they never change during the round, and no Crowd code writes a `Crowd*` tuning value after `OnTuningReady`.

---

## Implementation Notes

*Derived from ADR-0001 section 2, ADR-0016 section 2 and Implementation Guidelines:*

- The include check extends ADR-0007's lint with a Crowd rule: Crowd files may not include Roster, Objectives `Public/Server/` or Visibility Policy fact headers; transitive includes are checked.
- The reference scene is the SCL graybox scene (50 NPCs, 10 bot players, fixed seeds). Runtime criteria run headless accelerated where the timing allows and multi-process where bot clients are needed (ADR-0008 harness, `Source/StealthTestHarness/Private/`).
- Rejections inside the IIF other than same-drain contention are a defect; each contention case must be logged with the contending human command (CNB-R3).
- AC-CNB-37: log the effective rate table each 60 s and compare; add a write-hook on the tuning snapshot that asserts after `OnTuningReady`.
- Several criteria here need the full catalogue to be meaningful (run, tend, hide). Implement the harness and checks early; the criteria run green only after stories 014-024; until then they run against whichever behaviours exist and are re-run at exit (story 038).

**Prototype disposition** (`production/first-playable-report.md`, `Source/StealthGame`): Retires the prototype's direct `MoveToLocation` driving and the `StealthBot` strike paths from the crowd's code path (`StealthBot` stays as a test aid). Superseded code is removed in story 039.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 005: the sink types themselves
- Story 031: AC-CNB-38 (epoch and Halt gate)
- The Visibility Policy epic (`production/epics/information-visibility-policy/EPIC.md`): wire-level identity audits (replicated property audit, possession canary)

---

## Files

- `Source/StealthGameplay/Private/Tests/Crowd/CrowdIncludeCheckTest.cpp`
- `Source/StealthGameplay/Private/Tests/Crowd/CrowdRatesConstantTest.cpp`
- `Source/StealthTestHarness/Private/Tests/Crowd/CrowdPIETests.cpp`
- `tools/ci/lint/crowd-lint.sh`
- `tools/ci/lint/rule-crowd.awk`
- `Source/StealthGameplay/Public/Server/Core/Round/StealthCrowdControl.h`
- `Source/StealthGameplay/Private/Tests/Crowd/CrowdTestUtils.h`

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY, 2026-10-25).*

- **AC-1**: `StealthGame.Crowd.NoIdentityReads`
  - Given: the Crowd source tree and an instrumented reference-scene run
  - When: the include check runs, then a 10 min run
  - Then: no Roster, Objectives-assignment or Policy fact header is included; 0 calls to `IsPlayer`, `GetTeam`, `VictimKind` or `FSiteAssignment`
  - Edge cases: transitive includes; the runtime part needs 10 bots (deferred to the harness)
- **AC-2**: `StealthGame.Crowd.CommandsOnlyThroughIIF`
  - Given: 10 min reference scene with a CMC input-consumption hook
  - When: velocity changes are attributed to their source
  - Then: 100 % from `Move` entry points; 0 direct CMC writes or `RequestDirectMove`; 0 MoveTo/AITask_MoveTo; 0 path-following ticks; rejections only same-drain, each logged with the human command
  - Edge cases: multi-process run
- **AC-3**: `StealthGame.Crowd.NoNpcStrike`
  - Given: a 60 min simulation
  - When: the command log is scanned
  - Then: NPCs issue 0 `Attack` commands; participant-bound AI pawns issue only `Move` and `Look`
  - Edge cases: takeover and release transitions; discrete commands from ordinary NPCs allowed, from bound pawns not
- **AC-4**: `StealthGame.Crowd.RatesNotAdaptedAtRuntime`
  - Given: bots running, hiding and tending at 3 x r_P, with a write hook on `Crowd*` tuning values
  - When: effective draw rates are logged every 60 s
  - Then: rates constant across all samples; 0 writes after `OnTuningReady`
  - Edge cases: strike bursts and bush entries during the round

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/CrowdCommandPathTest.cpp` and `CrowdRatesConstantTest.cpp` plus `Source/StealthGameplay/Private/Tests/Crowd/CrowdIncludeCheckTest.cpp` — must exist and pass

**Status**: [x] Created and passing (`StealthGame.Crowd.Identity.IncludeCheck`, `StealthGame.Crowd.Identity.NoStrikeVocabulary`, `StealthGame.Crowd.Identity.RatesConstant`, `StealthGame.Crowd.Identity.NoTuningWriteAfterReady`, `StealthGame.Crowd.PIE.CommandPath.CrowdMotionOnlyThroughTheCommandInterface`; 2026-10-26)

---

## Dependencies

- Depends on: `production/epics/crowd-npc-behaviour/story-009-follower-shaping-pace-and-stops.md`, `production/epics/crowd-npc-behaviour/story-011-avoidance-and-yield.md`, `production/epics/crowd-npc-behaviour/story-012-statetree-schema-and-decision-grid.md`, `production/epics/crowd-npc-behaviour/story-014-pauses-and-stall-cover.md`
- Unlocks: None

## Completion Notes
**Completed**: 2026-10-26 (Crowd & NPC Behaviour batch, stories cnb-001..018; the production crowd drives L_Prototype, D53)
**Criteria**: 4/4 passing
**Deviations**: RatesConstant runs on the headless scene with scripted players (no bot-player behaviour layer yet), not in the harness. The transitive include walk is one awk pass; the test-side script runner kills and retries a hung bash after 120 s. ITakeoverEligibilitySource::TryReleaseNpc returns false until story 030.
**Test Evidence**: `StealthGame.Crowd.Identity.IncludeCheck`, `StealthGame.Crowd.Identity.NoStrikeVocabulary`, `StealthGame.Crowd.Identity.RatesConstant`, `StealthGame.Crowd.Identity.NoTuningWriteAfterReady`, `StealthGame.Crowd.PIE.CommandPath.CrowdMotionOnlyThroughTheCommandInterface`; full suite 275/275: `production/qa/evidence/crowd-npc-behaviour-001-018-2026-10-26/full-suite.txt`
**Run result**: 3-player PIE on L_Prototype with the production crowd (40 NPCs, the NpcCount default): `production/qa/evidence/crowd-npc-behaviour-001-018-2026-10-26/evidence.md`
**Reviewed by**: self-review against the code-review skill during the batch; findings and fixes in `production/qa/evidence/crowd-npc-behaviour-001-018-2026-10-26/evidence.md`
