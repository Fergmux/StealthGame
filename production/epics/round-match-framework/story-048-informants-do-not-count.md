# Story 048: Killing or re-informing Informants triggers no evaluation and leaves the Active count unchanged

> **Epic**: Round & Match Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25f
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-005` (Objective and elimination wins; no draw, no clock); GDD AC 19; rules R15 ("Informants never count"; the zeroing condition requires `Old = Active`, `New = Informant`), F2 (`Zero(e)`).
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0002: Server resolution queue, server clock and round-end staging
**ADR Decision Summary**: One total-order queue per round keyed (ServerTickIndex, Seq), per-source mailboxes, due-time heap, drained once per frame by a custom `TG_PostPhysics` tick function, typed staged writes, single-assignment latch storage with Round's rule injected, the R9 two-step round end, and the RTT-compensated client `EstimatedServerNow()`. This story applies section 5: Round evaluates elimination inside the Roster flush of the causing slot, and only an applied `Active` to `Informant` event can zero a team.
**ADR Version**: ADR-0002 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (ADR-0002 knowledge risk)
**Engine Notes**: None touched. The test uses the fake Roster gateway and stub resolvers on the plain C++ queue core; no tick function, `UWorld` or post-cutoff API is exercised, so ADR-0002 verification items (1) to (3) do not apply.

**Control Manifest Rules (this layer)** (the Round-relevant rules sit in the Foundation Layer Rules, Session and travel and Forbidden Approaches):
- Required: `FRosterCore::RequestTransition` is the only writer of lifecycle `State`; the R8 result precedence is applied in order, with `NoOp_AlreadyInState` before `IllegalTransition`; `RoundEnd` and `Reset` exist only as the batch calls `EndRoundAll()` and `ResetAll(RoundIndex)` (ADR-0005, Technical Setup 2026-10-25b).
- Forbidden: Never mutate gameplay state outside the drain; never resolve inline in an RPC body or AI source (ADR-0002).
- Guardrail: Roster queries are O(1), at most 2 hash probes and 0 allocations per call (AC-PSR-28); queue drain, staging, teardown and Roster flush stay at or below 0.5 ms of listen-host game thread (ADR-0002, ADR-0005).

**Prototype disposition**: Replaces the head-count rule of `FStealthRules::EvaluateEliminationWinner` (`Source/StealthGame/Private/StealthRules.cpp`), which counts Active players per team and has no Informant state. Its test "No winner while both teams have active players" (`Source/StealthGame/Private/Tests/StealthRulesTests.cpp`) is the nearest reference, re-expressed against the F2 event predicate.

---

## Acceptance Criteria

*From GDD `design/gdd/round-match-framework.md` AC 19, scoped to this story:*

- [ ] GIVEN team 1 = {1 Active, 3 Informants}, WHEN an informant is killed (`VictimKind = Informant`) and an `Informant -> Informant` request returns `NoOp_AlreadyInState`, THEN no evaluation, no `WinLatched`, and team 1's Active count stays 1.

---

## Implementation Notes

*Derived from GDD R15, F2 and ADR-0005 (Roster transition rules):*

- **Only `Active` to `Informant` can zero a team.** `Zero(e)` requires `e.Applied`, `e.Old = Active` and `e.New = Informant`. Informants never count toward a team's Active total (R15), so neither of the two events here can satisfy it: a kill on an Informant's pawn (`VictimKind = Informant`) is logged `KillResolved` but changes no Roster lifecycle state (the participant stays `Informant`), and an `Informant -> Informant` request returns `NoOp_AlreadyInState`, which emits no event of its own and does not change `ActiveCountAfter`.
- **Two events, checked separately.** (1) Resolve an informant kill through a stub resolver (stories 004 and 008) in a slot; assert Round's elimination evaluation is not entered (no `Zero(e)` candidate), no `WinLatched`, and team 1's Active count stays 1. (2) Submit an `Informant -> Informant` `RequestTransition` through the fake Roster gateway and assert it returns `NoOp_AlreadyInState`, with the same three assertions. The fixture team is `{1 Active, 3 Informants}` from the constants file (story 005).
- **"No evaluation" is observable.** Use a spy on Round's elimination-evaluation entry (or on `TryTerminate`) to assert it was not called for either event, and a spy on `IRoundLogWriter::Append` for `WinLatched`. Keep the spy minimal; do not assert on Roster internals.
- **Uses stories 043 and 044.** The latch and F2 predicates exist; this story is a scenario over them. No new production logic is expected beyond a fixture helper in test support.
- **Where the code goes.** Test-only: `Source/StealthGameplay/Private/Tests/Round/InformantsDoNotCountTest.cpp`. The Informant state, penalty and respawn belong to other epics; Round only refuses to count them.
- Manifest 2026-10-25f: `FRosterCore::RequestTransition` is the only writer of lifecycle `State`, and `RoundEnd` and `Reset` exist only as the batch calls `EndRoundAll()` and `ResetAll(RoundIndex)` (`BatchOnlyReason` precedes `IllegalReason` in R8).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 044 (`production/epics/round-match-framework/story-044-elimination-truth-table.md`): the clause table, including `Old` and `New` flips.
- Story 043 (`production/epics/round-match-framework/story-043-no-draw-property.md`): the latch and uniqueness property.
- Story 047 (`production/epics/round-match-framework/story-047-empty-team-no-elimination.md`): the empty-rival case.
- Story 072: an Informant disconnect (`ReleaseToCrowd` once, no evaluation).
- Roster transitions, `NoOp_AlreadyInState` semantics and Penalty's informant respawn belong to the participant-state-roster epic and other feature epics.

---

## Files

*Planned files only. Tests live under the engine test roots (`tests/README.md`), not under `tests/`. `/dev-story` replaces this list with the files it actually changed.*

- `Source/StealthGameplay/Private/Tests/Round/InformantsDoNotCountTest.cpp` (class `FRoundInformantsDoNotCountTest`, test name `StealthGame.Round.InformantsDoNotCount`)
- `Source/StealthGame/Private/StealthRules.cpp` (`EvaluateEliminationWinner` head-count rule: retired/replaced by stories 043 and 044)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: Killing or re-informing Informants triggers no evaluation and leaves the Active count unchanged.
  - Given: team 1 = {1 Active, 3 Informants}.
  - When: an Informant is killed (`VictimKind = Informant`), then an `Informant -> Informant` request returns `NoOp_AlreadyInState`.
  - Then: there is no elimination evaluation and no `WinLatched` entry.
  - Then: team 1's Active count stays 1.
  - Edge cases: both events are checked separately.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Round/InformantsDoNotCountTest.cpp` (automation test `StealthGame.Round.InformantsDoNotCount`, class `FRoundInformantsDoNotCountTest`) — must exist and pass.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-044-elimination-truth-table.md`
- Unlocks: None
- Cross-epic: `production/epics/participant-state-roster` (`RequestTransition`, `NoOp_AlreadyInState` and counts, via the fake Roster gateway; stories not yet created), `production/epics/tuning-data-config` (module skeleton and StealthTestHarness bootstrap; stories not yet created)
