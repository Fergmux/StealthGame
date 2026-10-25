# Story 031: A player-only cull never ships: bConcealServerCull true requires NPC bush use and a passing AC-IVP-25 run

> **Epic**: Information Visibility Policy
> **Status**: Blocked — BLOCKED: depends on the cull implementation (story-024, itself blocked on S9) and on the AC-IVP-25 run record (story-040).
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

The concealment cull applies only to the pawns that are concealed. If NPCs never use bushes, every culled pawn is a player and the cull itself becomes a detector (L29, CRITICAL). The policy is therefore that `bConcealServerCull = true` is allowed only when NPC bush use is enabled and the latest AC-IVP-25 run (hide-posterior parity at `HidePosteriorFactor` at most 1.5) passed; otherwise the build uses client-side hide, the cull is invoked for no pawn, and the build is flagged internal-development-only so the release checklist rejects it. This story wires that switch and proves it against a stub run record. There is no automatic fallback if Crowd misses F5 (R7.6).

**GDD**: `design/gdd/information-visibility-policy.md`
**Requirement**: `TR-ivp-004` (Concealment hybrid: quantised server cull for non-allies + client presentation hide); also `TR-ivp-007` (Leak catalogue with CRITICAL ship gate; per-commit canaries)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0003: Replication, relevancy and the concealment cull hybrid (section 5.9); ADR-0008: Test automation and leak parity (section 8, gate wiring)
**ADR Decision Summary**: The cull applies identically to every pawn kind. `bConcealServerCull = false` (client-side hide only) exists only for internal development builds (L29 CRITICAL) and never ships by default; there is no automatic fallback if Crowd misses F5 (IVP R7.6).
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: No new engine API beyond ADR-0003's cull path (E5 explicit channel close, E10 closed channel stays closed) [V]; the switch only decides whether story-024's cull tick runs for any pawn. The cull path itself is gated on spike S9 (story-004).

**Control Manifest Rules (this layer)**:
- Required: **Every leak, parity, statistical and lint gate includes a negative control the suite detects** — source: ADR-0008
- Forbidden: **Never hide concealment client-side only or replicate a concealed flag** (internal development switch only); never use dormancy or distance culling for concealment — source: ADR-0003
- Forbidden: **Never compile negative-control types or test seams into Shipping** (`StealthTestHarness` or `WITH_STEALTH_TEST_SEAMS` only) — source: ADR-0008
- Guardrail: **Tests**: per-commit pipeline ≤ 10 min; nightly cook + T3 estimated 1-2 h [U] — source: ADR-0008

---

## Acceptance Criteria

*From GDD `design/gdd/information-visibility-policy.md` (AC-IVP-30), scoped to this story:*

- [ ] **AC-IVP-30** [Int][Doc] `StealthGame.Visibility.PlayerOnlyCullNeverShips`: GIVEN a build configuration, WHEN `bConcealServerCull = true`, THEN NPC bush use is enabled and the latest AC-IVP-25 run passed (at `HidePosteriorFactor` ≤ 1.5); otherwise the build uses client-side hide, the cull is invoked for no pawn, and the build is flagged internal-development-only (L29): the release checklist rejects it. (R7.6)

---

## Implementation Notes

*Derived from ADR-0003 section 5.9, ADR-0008 sections 3 and 8:*

- The switch is `bConcealServerCull` (ADR-0003 5.9). Its flag key and the AC-IVP-25 run-record format are defined by story-040 (the run record carries the pass/fail, the `HidePosteriorFactor` used and the commit); this story consumes them and must not invent a second format. Until story-040 lands, test against a stub record with the same fields.
- Behaviour to prove, for a stub run record: (a) `bConcealServerCull = true` with NPC bush use enabled and a latest AC-IVP-25 run that passed at `HidePosteriorFactor <= 1.5` is a valid shippable configuration; (b) `true` with a failed run, and `true` with a missing run, are not: the build must resolve to client-side hide, story-024's cull is invoked for no pawn, and the build carries the internal-development-only flag; (c) `bConcealServerCull = false` also produces the flag. A flagged build is rejected by the release checklist (story-049's gate reads the same flag; AC-IVP-44 requires `bConcealServerCull` true on any build leaving the team).
- The cull being invoked for no pawn is asserted at the story-024 seam (`SetCullState` is accepted but no record leaves `Visible`, or the cull tick has no work), in a test world.
- NPC bush use enabled is Crowd's setting (crowd-npc-behaviour epic); this story reads it through a consumer-owned interface and never names a Crowd type (ADR-0007).
- Negative control (ADR-0008 section 3): the true-with-failed-run and true-with-missing-run cases are the controls; the suite passes only if the guard rejects them. A harness-only variant that ignores the run record must be detected.
- The release-checklist rejection lives in story-049's gate script; this story provides the flag it reads and a checklist row.
- Prototype: Replaces: nothing (the prototype has only the client-side hide, `UpdateConcealmentVisibility`, which story-024 removes). Promotes: nothing.
- Cross-epic: needs crowd-npc-behaviour's NPC bush-use setting and concealment-bushes' entry log (via story-040).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- story-024: the cull implementation this switch gates
- story-040: the AC-IVP-25 statistical run, its record format and the flag key
- story-039: the `HidePosteriorFactor` ceiling check
- story-049: the external-build critical gate script and release checklist rows
- crowd-npc-behaviour epic: NPC bush use; concealment-bushes epic: the Concealment entry log

---

## Files

*The code and test files this story creates or changes (planned):*

- `Source/StealthFoundation/Public/Server/StealthVisibilityPolicy.h` (the `bConcealServerCull` resolution and internal-development-only flag, extends story-024)
- `Source/StealthFoundation/Private/Visibility/StealthVisibilityPolicy.cpp`
- `Source/StealthTestHarness/Private/Tests/PIE/PlayerOnlyCullNeverShipsTest.cpp` (T2, `NETWORK_TEST_CLASS(PlayerOnlyCullNeverShips, "StealthGame.Visibility.PIE")`, with the stub run record fixtures)
- a checklist row for the flagged build in the release checklist (location shared with story-047/story-049: `production/qa/`)

*Note: the `StealthFoundation` and `StealthTestHarness` module folders are created by tuning-data-config Exit A (they do not exist yet).*

---

## QA Test Cases

- **AC-1**: Player-only cull never ships (AC-IVP-30)
  - Given: a build configuration and a stub AC-IVP-25 run record.
  - When: `bConcealServerCull = true`, and again when it is false.
  - Then: if true, NPC bush use is enabled and the latest AC-IVP-25 run passed at `HidePosteriorFactor <= 1.5`; otherwise the build uses client-side hide, the cull is invoked for no pawn, and the build is flagged internal-development-only (L29); the release checklist rejects a flagged build.
  - Edge cases: the flag key and record format are defined by story-040; test true-with-failed-run and true-with-missing-run.

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/Tests/PIE/PlayerOnlyCullNeverShipsTest.cpp` (T2 test, must exist and pass) OR a documented playtest or spike report

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/information-visibility-policy/story-024-concealment-cull-audience.md`, `production/epics/information-visibility-policy/story-040-hide-posterior-parity.md`
- Unlocks: None
