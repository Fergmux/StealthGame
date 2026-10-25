# Story 033: Hot-path queries allocate nothing and probe at most twice per call

> **Epic**: Participant State & Roster
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/participant-state-roster.md`
**Requirement**: `TR-roster-005`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0010: Shared Pawn: CMC locomotion, spawn factory and possession parity
**ADR Decision Summary**: ADR-0010 makes the Roster binding table the single source of "is this pawn a player", which Assassination, Concealment and the movement gait clamp query many times per frame, so those queries must stay O(1) and allocation-free (Roster R10). The factory and every possession path run in one server call stack, and nothing in the Roster may add per-call cost that scales with pawn count. ADR-0002 sets the shared listen-host budget (queue drain, staging, teardown and Roster flush at most 0.5 ms), which these queries sit inside.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Post-cutoff APIs named by ADR-0010 are `UMotionWarpingComponent::GetOwnerAdapter()` (5.8; Beta in 5.8.4) and `UE_LOGF`; neither is used. The allocation counter needs a hook into the engine allocator; that API is not covered by `docs/engine-reference/unreal/` and must be verified against the 5.8.4 headers before writing the test (see Implementation Notes).

**Control Manifest Rules (this layer)** *(ADR-0010 rules sit in the Core-layer manifest section; this story reads the Foundation rules that apply to the Roster core)*:
- Required: Queue, Roster, tuning validator, seed mixer and appearance deck are plain C++ cores (`F…Core`) with no `UWorld` dependency, behind a subsystem facade — source: ADR-0007
- Required: Every Logic story ships T1 tests; T1 tests live in the owning module's `Private/Tests/<System>/` — source: ADR-0008
- Required: Test names follow `StealthGame.<System>.<Scenario>` (T1) — source: ADR-0008
- Forbidden: Never use wall-clock time as a test pass/fail predicate (only as a failure ceiling); never use wall-clock waits, unseeded randomness or shared mutable state in tests — source: ADR-0008
- Forbidden: Never compile negative-control types or test seams into Shipping (`StealthTestHarness` or `WITH_STEALTH_TEST_SEAMS` only) — source: ADR-0008
- Guardrail: Queue drain + staging + teardown + Roster flush: at most 0.5 ms listen-host game thread — source: ADR-0002

This story proves the R10 promise that the hot-path queries (`ResolveVictimKind`, `IsPawnBound`, `GetRelation`) are O(1) and allocation-free, by measuring allocations and hash-table probes across 10,000 calls at 200 and then 2,000 pawn handles. It exists because those queries run inside every attack, concealment and gait check, and an allocation or a long probe chain would break the frame budget. It realises GDD R10 ("O(1), allocation-free on the hot path").

---

## Acceptance Criteria

*From GDD `design/gdd/participant-state-roster.md`, scoped to this story:*

- [ ] **AC-PSR-28** [Core] `StealthGame.Roster.QueryHotPathBudget`: GIVEN 10 participants and 200 pawn handles, WHEN 10,000 calls each are made to `ResolveVictimKind`, `IsPawnBound` and `GetRelation`, THEN a counting allocator records 0 allocations, and an instrumented probe counter shows at most 2 hash-table probes per call at both 200 and 2,000 handles (R10, "O(1), allocation-free"). A wall-clock timing is recorded as advisory only.

---

## Implementation Notes

*Derived from ADR-0007 section 5, ADR-0008 (T1 rules) and GDD R10:*

- **Binding table.** The binding table behind `ResolveVictimKind`, `IsPawnBound` and `GetRelation` (stories 010, 013, 032) must be a pre-sized open hash structure keyed by the pawn handle `{ObjectKey, Generation}`. It reserves capacity at `BeginMatch`/construction so a query never grows it; queries only read. Scratch memory used by the liveness provider call and the lookup is pre-allocated before the measured loop.
- **Probe counter.** Add an instrumented probe counter on the table, compiled only under `WITH_STEALTH_TEST_SEAMS` (never into Shipping; ADR-0008). Count every slot examined by a lookup. The test reads it after each call, not once at the end, so the maximum is observable.
- **Probe bound (pinned reading).** The qa-lead reading is "at most 2 probes per handle lookup, maximum over all calls, at both 200 and 2,000 handles". `GetRelation` performs two handle lookups, so under this reading it may examine up to 4 slots in total. Choose a table scheme that makes a 2-probe worst case achievable by construction (for example two-candidate-slot cuckoo-style placement) rather than relying on a low load factor, and record the chosen table design and load factor in the test comment. The GDD wording is "per call"; see the Open point.
- **Counting allocator.** Count allocations through a hook installed around the measured loops only. The engine allocator hook API is not in the engine reference docs, so verify the exact mechanism against the 5.8.4 headers in `C:/Program Files/Epic Games/UE_5.8` before coding; if no clean hook exists, wrap the core's own container allocator for the test seam instead. Either way the assertion is exactly 0 allocations across all 30,000 calls.
- **Handle counts.** Run the same 10,000 calls per query with 200 handles and then 2,000 handles. Keep 10 participants, with handles spread over BoundLive, Retired and Unbound entries so the table sees hits and misses.
- **Wall clock is advisory.** Record elapsed time in the test output; never assert on it (manifest: never use wall-clock time as a pass/fail predicate).
- **Open point:** the GDD says "at most 2 hash-table probes per call", but `GetRelation` makes two lookups. The qa-lead gap asks for the bound to be restated as per handle lookup. Get a ruling (game-designer with qa-lead) and, if the GDD wording is kept, amend the AC; until then implement the per-lookup reading.
- **Open point:** the maximum versus mean question and the table design/load factor are not defined by the GDD; this story fixes maximum (worst call) and leaves the table design to the implementer, recorded in the test.
- **Prototype disposition:** no prototype counterpart in the roster core. The prototype finds strike targets by scanning every actor with `TActorIterator` (`Source/StealthGame/Private/StealthGameMode.cpp`), the O(n) pattern this table replaces.
- **Header naming (ADR-0007).** Stealth prefix, project-unique names: `StealthRosterCore.h`, `StealthRosterDeps.h`.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 013 (ResolveVictimKind answers the full pawn-state matrix and never yields Npc for NotTargetable): owns the query's correctness; this story measures it.
- Story 032 (GetRelation answers the viewer-subject matrix symmetrically): owns `GetRelation` correctness.
- Story 031 (GetLegalityClass answers per pawn state and BindPawn adopts an unbound pawn): `GetLegalityClass` is not in this AC's measured set.
- Story 010 (BindPawn rejects an already-bound participant, an in-use pawn and a round-ended participant): owns the binding table's contents and writes.
- Story 004 (Every roster call on a client is rejected or answers not-targetable and faults): client-side query behaviour.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- `Source/StealthFoundation/Private/Roster/StealthRosterCore.h` (binding table capacity reservation, probe counter seam)
- `Source/StealthFoundation/Private/Roster/StealthRosterCore.cpp`
- `Source/StealthFoundation/Private/Roster/StealthRosterDeps.h` (liveness provider must not allocate on the query path)
- `Source/StealthFoundation/Private/Tests/Roster/StealthRosterQueryHotPathBudgetTest.cpp` (class `FRosterQueryHotPathBudgetTest`, test name `StealthGame.Roster.QueryHotPathBudget`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation. (At tiers where the QL-STORY-READY gate is skipped — `minimal`, or `lean`/`solo` review mode — no qa-lead specs exist, and this section reads: N/A — no qa-lead specs at this tier; implement against the Acceptance Criteria above.)*

*Test cases not yet defined — run /qa-plan to generate them.*

**Flagged gap (QL-STORY-READY: GAPS):** "at most 2 hash-table probes per call" is ambiguous. GetRelation does two lookups so the cap is met only by exactly one probe per lookup. It does not say maximum or mean, nor the table design or load factor. Needs: "at most 2 probes per handle lookup (maximum over all calls), at both 200 and 2,000 handles".

**Provisional spec (qa-lead, pending gap resolution):**

- **AC-1**: hot-path queries allocate nothing and probe a bounded number of times.
  - Given: 10 participants, 200 pawn handles (then 2,000), counting allocator, probe counter on the handle table.
  - When: 10,000 calls each of ResolveVictimKind, IsPawnBound, GetRelation, scratch memory pre-allocated.
  - Then: 0 allocations. Probes per call stay within the pinned bound at both sizes.
  - Edge cases: wall-clock time is advisory only, never asserted.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Roster/StealthRosterQueryHotPathBudgetTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later). `qa.level` is full, so the test is required. The advisory wall-clock timing is recorded in the test output only.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/participant-state-roster/story-013-resolve-victim-kind-matrix.md`, `production/epics/participant-state-roster/story-032-get-relation-matrix.md`
- Unlocks: None
