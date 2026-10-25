# Story 027: The transition matrix equals the checked-in CSV fixture for every combination

> **Epic**: Participant State & Roster
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

This story is the exhaustive safety net for `RequestTransition`: every combination of from-state, to-state, reason, epoch match, connection state and connection-generation match is requested, and each result must equal the value in a hand-derived, checked-in CSV fixture (`RosterTransitionMatrix.csv`). It exists because the result precedence (R8, eleven steps) is easy to get subtly wrong and the stories before it test only the headline cases. The fixture is derived by hand from R8, never generated from the core, so the test is an independent oracle. It realises GDD rule R8 (result precedence, `InformantKilled`, kick path), the legal-transition table, and F6.

**GDD**: `design/gdd/participant-state-roster.md`
**Requirement**: `TR-roster-002`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0005: Session, seamless travel and match-scoped state stores
**ADR Decision Summary**: The Roster is one of four game-instance stores and the only authoritative owner of participant records, bindings, tombstones, `MatchId`, `RoundEpoch` and `EventSeq`; its logic lives in the plain-C++ `FRosterCore` behind a thin `URosterStore` facade. Every `Connection` change is a projection written only by Session, and a kick that closes an `Expired` connection is a legal `Disconnect` (the Roster accepts `Disconnect` for a participant whose Session connection is `Expired`, Round CR-21, kick path).
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Networking domain (HIGH per VERSION.md). No new post-cutoff APIs; new logging uses `UE_LOGF` (ADR-0007). Verification required by ADR-0005 is behavioural: spikes S3, S4 and S5 (not yet reported). This story runs entirely inside the engine-free `FRosterCore` and is not gated by any of them.

**Control Manifest Rules (this layer)**:
- Required: Queue, Roster, tuning validator, seed mixer and appearance deck are plain C++ cores (`F…Core`) with no `UWorld` dependency, behind a subsystem facade — source: ADR-0007
- Required: Test names: `StealthGame.<System>.<Scenario>` (T1); T1 tests live in the owning module's `Private/Tests/<System>/` — source: ADR-0008
- Required: Every leak, parity, statistical and lint gate includes a negative control the suite detects — source: ADR-0008
- Forbidden: Never use wall-clock time as a test pass/fail predicate; never use wall-clock waits, unseeded randomness or shared mutable state in tests — source: ADR-0008
- Forbidden: Never disable, skip or reseed a failing test to pass a gate; never mark a gate Done that was never watched to fail — source: ADR-0008
- Guardrail: Tests: per-commit pipeline <= 10 min — source: ADR-0008

---

## Acceptance Criteria

*From GDD `design/gdd/participant-state-roster.md`, scoped to this story:*

- [ ] **AC-PSR-20** [Core] `StealthGame.Roster.Transition.ExhaustiveMatrix`: GIVEN every combination of from-state × to-state × reason × epoch match/mismatch × connection × generation match/mismatch, WHEN requested, THEN each result equals the expected value in a checked-in CSV fixture (`RosterTransitionMatrix.csv`, one row per combination, hand-derived from R8). The test also asserts the fixture's row count, and every cell outside the table's Applied/NoOp rows is a rejection (table, R8). The reason axis includes `InformantKilled` (D10, user-approved 2026-10-25), whose every row past precedence steps 1-5 is `Rejected_IllegalReason`; and the connection axis includes `Expired` projected as `Disconnected`, whose `Disconnect` rows with a matching generation are `Applied` (kick path, D9).

---

## Implementation Notes

*Derived from ADR-0005 section 5 (kick path, generation rules) and GDD rules R8 (precedence 1 to 11), R11, F6 and the legal-transition table:*

- Create `RosterTransitionMatrix.csv` by hand from R8's eleven-step precedence and the legal-transition table; it is the oracle and must never be produced by running the core. One row per combination with columns for the inputs (from-state, to-state, reason, epoch match, connection, generation match) and the expected result code. Authority is true and the thread is the game thread for every row, so steps 1 and 2 are exercised by stories 003 and 004, and step 3 (unknown id) by story 018; the matrix rows use a registered id.
- Axes (author to confirm, see open points): from-state {Active, Informant, RoundEnded}; to-state {Active, Informant, RoundEnded}; reason {Assassinated, ReversedAgainst, StrikeLimit, Disconnect, RoundEnd, Reset, InformantKilled}; epoch {match, mismatch}; connection {Connected, Disconnected, and `Expired` projected as `Disconnected`}; generation {match, mismatch, and "missing" because R8 step 10 says "missing or differs"}. The expected result per row follows R8: step 4 stale epoch, 5 batch-only, 6 illegal reason (`InformantKilled` is illegal for every target), 7 no-op, 8 illegal transition, 9 not disconnected, 10 stale connection, 11 applied.
- Applied only for `Active -> Informant` with an entering reason; for `Disconnect` also `Connection == Disconnected` with a matching generation. The `Expired`-projected row must be built the way Session would produce it: a record that went `Disconnected` (generation bumped once) then `Expired` via `NotifyConnectionChanged`, which projects `Disconnected` and bumps nothing; its `Disconnect` row with a matching generation is `Applied` (kick path, D9). Build this precondition from the real call history, not by setting the field.
- Assert the fixture's row count (a constant checked in beside the fixture and recomputed from the axis sizes in the test) and that every row outside the Applied and NoOp results is a rejection result code.
- Open point (qa-lead gap, unresolved): the AC does not enumerate the connection axis values (whether `Dormant` is included), whether "missing generation" is a third generation value, whether the reason axis has six or seven values, or the expected row count. Fix these explicitly when the CSV is authored, record the axis lists and the count in the fixture header, and flag them to the GDD owner.
- Open point (unit-test rule): loading a CSV is file I/O, which the unit-test standards forbid ("Unit tests do not call external APIs, databases, or file I/O"). Options: embed the CSV text in the test source (kept byte-identical to the checked-in file and verified by a separate check), generate a header from the CSV at build time, or use a harness-approved fixture path. The file location is not fixed by the plan; a natural place is `Source/StealthFoundation/Private/Tests/Roster/` beside the test. Decide and record the choice when implementing.
- The driver builds each precondition through core seams (compiled only under `WITH_STEALTH_TEST_SEAMS`) or through the real calls from stories 009, 014 and 022; reuse one helper so the 3x3x7x2x3x3 expansion stays a loop, not 1,000 hand-written cases.
- Test construction: `FRosterCore` with injected fakes from `Private/Roster/StealthRosterDeps.h` (authority true, game thread, seeded random source, fault sink, injected ms clock).
- Prototype disposition: no prototype counterpart.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 015 (A Disconnect transition is rejected while the participant is connected): the headline `Rejected_NotDisconnected` case.
- Story 016 (The first entering reason is kept and later reasons are no-ops): the headline no-op case.
- Story 017 (No revival and no round-ended-to-informant transitions are possible): the headline illegal-reason and illegal-transition cases.
- Story 018 (Unregistered and retired ids are rejected by every id-taking API): unknown-id handling.
- Story 019 (RoundEnd and Reset reasons are rejected on single transitions): the headline batch-only case.
- Story 024 (Calls carrying an old round epoch or match id are rejected): the headline stale-epoch cases.
- Story 054 (Disconnect grace expires on the injected clock and stale-connection timers are rejected): the timing of real grace timers.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- `Source/StealthFoundation/Private/Roster/StealthRosterCore.h` (only if the matrix exposes a precedence defect)
- `Source/StealthFoundation/Private/Roster/StealthRosterCore.cpp` (only if the matrix exposes a precedence defect)
- `RosterTransitionMatrix.csv` (new fixture, hand-derived from R8; final location and loading mechanism to be decided per the open point above, proposed under `Source/StealthFoundation/Private/Tests/Roster/`)
- `Source/StealthFoundation/Private/Tests/Roster/StealthRosterTransitionExhaustiveMatrixTest.cpp` (class `FRosterTransitionExhaustiveMatrixTest`, test name `StealthGame.Roster.Transition.ExhaustiveMatrix`)
- ADR-0007 header naming: no new header is added; any new one uses the `Stealth` prefix and a project-unique file name.

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation. (At tiers where the QL-STORY-READY gate is skipped — `minimal`, or `lean`/`solo` review mode — no qa-lead specs exist, and this section reads: N/A — no qa-lead specs at this tier; implement against the Acceptance Criteria above.)*

*Test cases not yet defined — run /qa-plan to generate them.*

**Flagged gap (QL-STORY-READY: GAPS)**: the connection axis is not enumerated (Connected, Disconnected, Dormant?). "Expired projected as Disconnected" is a history condition, not an input value. The generation axis has match and mismatch but no "missing" (R8 step 10 says "missing or differs"). The reason axis is ambiguous (six or seven with InformantKilled) and the expected row count is not given. Needs each axis's values and the row count.

**Provisional spec (qa-lead, pending gap resolution):**

- **AC-1**: transition matrix equals the CSV fixture for every combination.
  - Given: RosterTransitionMatrix.csv (hand-derived from R8), one row per combination, a driver that builds each precondition through core seams.
  - When: each row requested.
  - Then: each result equals the CSV value. Row count equals the checked-in count. Every cell outside the Applied/NoOp rows is a rejection. InformantKilled rows past step 5 are Rejected_IllegalReason. An Expired-projected Disconnect with a matching generation is Applied.
  - Edge cases: CSV loading is file I/O, against the unit-test rule: embed or generate it, or use a harness-approved path.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Roster/StealthRosterTransitionExhaustiveMatrixTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later). `qa.level` is `full`, so the test is required. The `RosterTransitionMatrix.csv` fixture must also exist.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/participant-state-roster/story-015-transition-disconnect-requires-disconnected.md`, `production/epics/participant-state-roster/story-016-transition-first-reason-wins.md`, `production/epics/participant-state-roster/story-017-transition-no-revival-no-late-events.md`, `production/epics/participant-state-roster/story-018-transition-unknown-id-rejected.md`, `production/epics/participant-state-roster/story-019-batch-only-reasons-rejected-singly.md`, `production/epics/participant-state-roster/story-024-stale-epoch-rejected.md`
- Cross-epic: none directly; the `StealthFoundation` module skeleton from `production/epics/tuning-data-config/EPIC.md` reaches this story through story 001.
- Unlocks: None
