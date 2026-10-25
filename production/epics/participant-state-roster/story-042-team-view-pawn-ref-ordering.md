# Story 042: PawnRef resolves in either arrival order of pawn and team-view entry

> **Epic**: Participant State & Roster
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/participant-state-roster.md`
**Requirement**: `TR-roster-006`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0001: Crowd identity rules
**ADR Decision Summary**: ADR-0001 lets an ally learn "that pawn is my teammate" only from the ally's own C-TeamView entry, whose `PawnRef` is typed `APawn` (never `AStealthPawn`, because Foundation may not depend on StealthGameplay). Engine pointers on pawns are owner-only, so the entry is the only link. A teammate whose pawn is outside relevancy has an unresolved reference and is served by the fallback position feed (built by the Visibility Policy, ADR-0003). `PawnRef`s are cleared at `InvalidateAllBindings` and re-pushed after each controller swap and on reclaim.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: ADR-0001 post-cutoff items: `APawn::RemoteViewPitch16` (5.6) and the `net.Iris.UseIrisReplication` default of 0 (legacy driver). The behaviour under test is the engine's handling of an object reference whose NetGUID is not yet mapped (resolved when the actor's channel arrives); it is verified at run time by this test and is not covered by a spike. Verification items S1, S2, S14 (S2 and S14 PASS; S1 PASS for the three pointers only). The component needs `bReplicateUsingRegisteredSubObjectList` on the controller (ADR-0020 E2).

**Control Manifest Rules (this layer)**:
- Required: Team-audience data travels only in a `UTeamViewComponentBase` subclass on the receiving teammate's own PlayerController — source: ADR-0001
- Required: Client code tolerates a relevancy destroy and initialises a re-created actor from replicated state only — source: ADR-0003
- Required: T2/T3 tests are bound by frames or server time and assert and record the active replication system — source: ADR-0008
- Forbidden: Never read `Owner` or `PlayerState` on a pawn the owner does not currently possess; client code never derives player-ness or team from pawn pointers — source: ADR-0001
- Forbidden: Never use wall-clock time as a test pass/fail predicate (only as a failure ceiling); never use wall-clock waits in tests — source: ADR-0008
- Forbidden: Never compile negative-control types or test seams into Shipping (`StealthTestHarness` or `WITH_STEALTH_TEST_SEAMS` only) — source: ADR-0008
- Guardrail: C-TeamView pushes change-only, at most 1 per frame, within the 50 KB/s per-client budget — source: ADR-0001

This story proves that a teammate's `PawnRef` in the client's C-TeamView resolves to the right pawn once that pawn becomes relevant, whichever of the two replicates first, and that an unresolved reference causes no crash or fault. It exists because the pawn actor and the controller's component replicate on independent channels, so arrival order is not guaranteed. It realises GDD R6 (`PawnRef` resolution) and the Edge Case "if replication order differs: the client's teammate `PawnRef` resolves when the pawn becomes relevant".

---

## Acceptance Criteria

*From GDD `design/gdd/participant-state-roster.md`, scoped to this story:*

- [ ] **AC-PSR-40d** [Int] `StealthGame.Roster.TeamViewPawnRefOrdering`: GIVEN a test hook that forces a teammate pawn to arrive before, and then after, its C-TeamView entry, WHEN both orders run, THEN `PawnRef` resolves once the pawn is relevant in both and nothing crashes (R11, Edge Cases).

---

## Implementation Notes

*Derived from ADR-0001 sections 4.1 and 5, ADR-0008 (T2 rules) and GDD R6, Edge Cases:*

- **`PawnRef` type.** `FTeamViewTeammateEntry::PawnRef` is `TObjectPtr<APawn>` (declared by story 037 in `Public/View/StealthRosterTeamView.h`); the client resolves it purely through the engine's object-reference mapping. Do not add identity data to the pawn, and do not search world pawns to find teammates (presentation never iterates pawns; ADR-0001 section 4.6).
- **Client-safe accessor.** Provide one accessor on the client (for example on `URosterTeamViewComponent`) that returns the `PawnRef` if resolved and null otherwise, never asserting or faulting on an unresolved reference; consumers (markers, HUD) treat null as "not relevant yet" and the fallback feed (ADR-0003) covers position. Before relevance the reference stays unresolved with no error.
- **Test hook (test seam only).** The hook forces the two arrival orders: (1) the pawn's channel opens first and the team-view entry afterwards, (2) the entry first and the pawn afterwards. Implement it under `WITH_STEALTH_TEST_SEAMS` (never compiled into Shipping; ADR-0008), for example by holding back either the pawn's relevancy or the delivery of the component update by a controlled number of server frames.
- **Net peers needed.** Arrival order is only observable with a real client connection. If the test cannot be written in one automation world with a client, relocate it to `Source/StealthTestHarness/Private/Roster/` and use CQTest `NETWORK_TEST_CLASS(TeamViewPawnRefOrdering, "StealthGame.Roster.PIE")` (ADR-0008: T2 code lives in the harness); record the move in the Files list. See the Open point.
- **Waiting and limits.** Wait on the condition "`PawnRef` resolved" with a timeout counted in server frames; hitting the timeout fails the test. Assert and record that the active replication system is the legacy driver.
- **Fixed net profile.** If run through the harness, use the fixed emulation for gating runs (PktLag 100 ms, no lag variance, PktLoss 0 %); the PktLoss 2 % and variance run is the advisory Soak.
- **Open point:** the plan puts [Int] tests in `Source/StealthFoundation/Private/Tests/Roster/`, but the two-order check needs two net peers, which ADR-0008 assigns to the harness. The hook mechanism (relevancy hold versus delayed component update) is also not specified; choose one and record it.
- **Prototype disposition:** no prototype counterpart. The prototype finds teammates by iterating pawns and reading their PlayerState on the client (`Source/StealthGame/Private/StealthHUD.cpp`, teammate markers), the pattern this reference replaces; it is not ported.
- **Header naming (ADR-0007).** Stealth prefix, project-unique names: `StealthRosterTeamView.h` (the accessor), test-only seam header under `Private/Tests/Roster/`.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 037 (A maximal C-TeamView net-serializes in at most 96 bytes): owns the `FTeamViewTeammateEntry` struct and its serializer.
- Story 041 (A replaced controller or reclaim re-pushes C-TeamView within one server frame): owns the server-side re-push of `PawnRef`s.
- Story 046 (Non-owned pawns carry null PlayerState, Controller and Owner for players and NPCs alike): PIE proof of the pawn pointer conditions.
- Teammate Markers and the Visibility Policy fallback feed (outside this epic; ADR-0003): positioning of teammates beyond relevancy.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- `Source/StealthFoundation/Public/View/StealthRosterTeamView.h` (client-safe `PawnRef` accessor on `URosterTeamViewComponent`)
- `Source/StealthFoundation/Private/Roster/StealthRosterTeamView.cpp`
- `Source/StealthFoundation/Private/Tests/Roster/StealthRosterTeamViewPawnRefOrderingTest.cpp` (class `FRosterTeamViewPawnRefOrderingTest`, test name `StealthGame.Roster.TeamViewPawnRefOrdering`; may move to `Source/StealthTestHarness/Private/Roster/` as a CQTest network test, see Implementation Notes)
- `Source/StealthFoundation/Private/Tests/Roster/StealthRosterArrivalOrderSeam.h` (test-only arrival-order hook, `WITH_STEALTH_TEST_SEAMS`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation. (At tiers where the QL-STORY-READY gate is skipped — `minimal`, or `lean`/`solo` review mode — no qa-lead specs exist, and this section reads: N/A — no qa-lead specs at this tier; implement against the Acceptance Criteria above.)*

**Automated test specs (Integration):**

- **AC-1**: PawnRef resolves in either arrival order.
  - Given: a test hook forcing the teammate pawn to arrive before, then after, its C-TeamView entry.
  - When: both orders run.
  - Then: PawnRef resolves to that pawn once it is relevant, in both orders, within a frame-counted timeout. The run completes without a crash or fault.
  - Edge cases: before relevance the reference stays unresolved with no error.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthFoundation/Private/Tests/Roster/StealthRosterTeamViewPawnRefOrderingTest.cpp` (or the harness path if relocated) — automated test must exist and pass (OR a documented playtest; `qa.level` is full, so the automated test is required).

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/participant-state-roster/story-037-team-view-serialized-size.md`
- Unlocks: None
