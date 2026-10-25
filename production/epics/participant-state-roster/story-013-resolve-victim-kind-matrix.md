# Story 013: ResolveVictimKind answers the full pawn-state matrix and never yields Npc for NotTargetable

> **Epic**: Participant State & Roster
> **Status**: Complete
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25g
> **Last Updated**: 2026-10-25

## Context

**GDD**: `design/gdd/participant-state-roster.md`
**Requirement**: `TR-roster-005`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0010: Shared Pawn: CMC locomotion, spawn factory and possession parity
**Also governed by**: ADR-0001: Crowd identity rules
**ADR Decision Summary**: ADR-0010 makes every body one `AStealthPawn` with a kind-blind possession and hand-over pass, and publishes the server state the Information Visibility Policy reads, so the only place player-ness exists is the server-side Roster binding. ADR-0001 (Pillar 1: an enemy cannot tell a player from an NPC) requires everything an attacker perceives after killing an informant to be identical to an NPC kill, which is why `ResolveVictimKind` must be exact: a second attack on an already-resolved body must never read as an NPC kill and cost a strike.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: ADR-0010 (HIGH): post-cutoff `UMotionWarpingComponent::GetOwnerAdapter()` (Beta plugin); outstanding verification S8, S14, H1, ST1, P1, the traversal spike and the ADR-0002 E4 spike. ADR-0001 (HIGH): `APawn::RemoteViewPitch16` (5.6) and the legacy NetDriver pin; S1 (pawn parity, partial FAIL per the 2026-10-25 spike report), S2 and S14 are carried by the PIE stories, not this one. The matrix is evaluated by the plain-C++ core over an injected liveness provider, so no engine API is exercised.

**Context paragraph**: `ResolveVictimKind(Pawn)` is the "NPC or player?" answer Assassination uses to fill `OnPawnKilled.VictimKind`. This story implements its evaluation order over the nine pawn states the GDD lists and proves a `NotTargetable` input is never mapped to `Npc`. It realises GDD R10 (`ResolveVictimKind` and its evaluation order, the informant-kill invariant) and builds on the binding table and tombstones from stories 010 and 011.

**Control Manifest Rules (this layer)**:
- Required: Queue, Roster, tuning validator, seed mixer and appearance deck are plain C++ cores (`F…Core`) with no `UWorld` dependency, behind a subsystem facade (ADR-0007)
- Required: Every body is an `AStealthPawn`: one class, one component set, no per-instance replication or net-config difference (ADR-0001)
- Required: A reflective or Blueprint request for a server-only value on a client returns invalid and uses `ensure`, never `check` (ADR-0007)
- Forbidden: Never branch on `IsPlayerControlled()`, `IsBotControlled()`, controller class, `PlayerState` presence, team or Roster state in pawn, component, AnimBP or audio code for anything another client can observe (ADR-0001)
- Forbidden: Never put `BlueprintCallable`, `BlueprintPure`, `BlueprintReadOnly`, `BlueprintReadWrite` or `BlueprintAssignable` on a member of a server type (ADR-0007)
- Guardrail: Queue drain + staging + teardown + Roster flush: <= 0.5 ms listen-host game thread (ADR-0002)
- Required (manifest 2026-10-25f, ADR-0005 Technical Setup 2026-10-25b): `FRosterCore::RequestTransition` is the only writer of lifecycle state with the R8 precedence; mutations on the server game thread with FIFO dispatch after the outermost call and EventSeq +1 per event; O(1) allocation-free queries through a pre-sized binding table; client queries return 0/false/NotTargetable(Stale) and fault (re-read at implementation; 2026-10-25g adds nothing for the Roster)

---

## Acceptance Criteria

*From GDD `design/gdd/participant-state-roster.md`, scoped to this story:*

- [x] **AC-PSR-26** [Core] `StealthGame.Roster.ResolveVictimKindMatrix`: GIVEN pawns that are BoundLive+Active, BoundLive+Informant, Unbound, Retired, destroyed, pending-kill, null, BoundLive with a RoundEnded participant, and Retired with a RoundEnded participant, WHEN `ResolveVictimKind` is called, THEN `Player` / `Informant` / `Npc` / `AlreadyResolved` / `Stale` / `Stale` / `Stale` / `RoundOver` / `AlreadyResolved`, and no `NotTargetable` input ever yields `Npc` (R10 evaluation order).

---

## Implementation Notes

*Derived from ADR-0010 (published server state, kind-blind pawn), ADR-0001 (informant-kill invariant) and GDD R10:*

- **Result type.** A small enum plus reason: `Player`, `Informant`, `Npc`, `NotTargetable(AlreadyResolved | Stale | RoundOver)`. Keep it a plain value type (no allocation), declared in the core header; it is server-only and never reaches a client directly (R10), so it does not belong in `Public/View/`.
- **Evaluation order (R10, fixed).** (1) a null, destroyed or pending-kill pawn gives `Stale` (liveness provider; a null handle is `Stale`); (2) a `Retired` pawn gives `AlreadyResolved`, **including one whose participant is now `RoundEnded`**; (3) a `BoundLive` pawn whose participant is `RoundEnded` gives `RoundOver`; (4) `BoundLive` + `Active` gives `Player`, `BoundLive` + `Informant` gives `Informant`; (5) a valid live `Unbound` pawn gives `Npc`. Implement it as one function with this order so the nine matrix rows are direct consequences.
- **NotTargetable never maps to Npc.** The default branch for any state the function does not recognise must be a fault-sink report plus `NotTargetable(Stale)`, never `Npc` (the permissive answer). A second attack on an already-resolved body can therefore never cost a strike.
- **Authority.** The client-side answer (`NotTargetable(Stale)` plus fault) is story 004; this story's function is the authority-true path and should call the shared query precheck added there if it exists, otherwise leave the call point.
- **Hot path.** This is a hot-path query: O(1), no allocation. Do not build strings, containers or log in the function body; story 033 measures allocations and probes.
- **Informant-kill invariant.** `ResolveVictimKind` returns `Informant` for a BoundLive informant so Assassination and Penalty can treat the kill identically to an NPC kill on the attacker's side (R10 informant-kill invariant); that behaviour lives in those epics, not here.
- **Fixtures.** Nine pawns through the liveness provider and the binding seams; `RoundEnded` participants and `Retired` bindings come from the core test seam (`WITH_STEALTH_TEST_SEAMS`, never in Shipping) until stories 023 and 020 land. Open point: none in the QA spec beyond the order caveat above.
- **Prototype disposition:** No prototype counterpart; the prototype decided player-vs-NPC from `PlayerState` presence (`IsNpc() = (PlayerState == null)`), the known leak retired by stories 040 and 046. `FStealthRules` has no equivalent query.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 004 (Every roster call on a client is rejected or answers not-targetable and faults): the client-side `NotTargetable(Stale)` answer
- Story 010 (BindPawn rejects an already-bound participant, an in-use pawn and a round-ended participant) and Story 011 (Tombstones respect generations and ReleaseToCrowd needs a disconnected participant): the binding table and tombstones
- Story 020 (Killing an informant retires its binding without a state event): `RetireBinding`
- Story 032 (GetRelation answers the viewer-subject matrix symmetrically): `GetRelation`
- Story 033 (Hot-path queries allocate nothing and probe at most twice per call): the allocation and probe budget for this query
- Story 031 (GetLegalityClass answers per pawn state and BindPawn adopts an unbound pawn): `GetLegalityClass`
- Story 050 (Killing an informant produces the same attacker-side RPCs, HUD and cues as killing an NPC): the attacker-side parity check

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- `Source/StealthFoundation/Private/Roster/StealthRosterCore.h` (`FRosterCore`, `FParticipantRecord` and its X-macro field list, result/event/call enums, core test seams under `WITH_STEALTH_TEST_SEAMS`)
- `Source/StealthFoundation/Private/Roster/StealthRosterCore.cpp`
- `Source/StealthFoundation/Private/Roster/StealthRosterDeps.h` (pawn handle, `IRosterPawnLiveness`, `IRosterAuthority`, `IRosterThreadCheck`, `IRosterRandom`, `IRosterRoundPhase` (D43), `FRosterCoreDeps`; clock and fault sink reuse `IStealthClock`/`IStealthFaultSink`)
- `Source/StealthFoundation/Public/View/StealthRosterTypes.h` (`FParticipantId`, `FIdentityKey`, `EParticipantState`, `ERosterTeam`)
- `Source/StealthFoundation/Public/Server/StealthFaultSink.h` (appended `RosterNotAuthority`=5, `RosterWrongThread`=6, `RosterLiveBindingCleared`=7, `RosterUnrecognisedState`=8)
- `Source/StealthFoundation/Private/Tests/Roster/StealthRosterTestFixture.h` (shared fakes and fixture; reuses `FFakeStealthClock`, `FStealthRecordingFaultSink`)
- `Source/StealthFoundation/Private/Tests/Roster/StealthResolveVictimKindMatrixTest.cpp` (test `StealthGame.Roster.ResolveVictimKindMatrix`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation. (At tiers where the QL-STORY-READY gate is skipped — `minimal`, or `lean`/`solo` review mode — no qa-lead specs exist, and this section reads: N/A — no qa-lead specs at this tier; implement against the Acceptance Criteria above.)*

**For Logic / Integration stories — automated test specs:**

- **AC-1**: full pawn-state matrix resolves correctly; NotTargetable is never Npc.
  - Given: nine pawns built through the liveness provider and binding seams.
  - When: ResolveVictimKind called on each.
  - Then: BoundLive+Active Player; BoundLive+Informant Informant; Unbound Npc; Retired NotTargetable(AlreadyResolved); destroyed, pending-kill, null NotTargetable(Stale); BoundLive+RoundEnded NotTargetable(RoundOver); Retired+RoundEnded NotTargetable(AlreadyResolved). No NotTargetable input yields Npc.
  - Edge cases: R10 order is Stale, then AlreadyResolved, then RoundOver.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: automated test `Source/StealthFoundation/Private/Tests/Roster/StealthResolveVictimKindMatrixTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later). `qa.level` is full, so the test is required.

**Status**: [x] Created and passing (`StealthGame.Roster.ResolveVictimKindMatrix`, 2026-10-25)

---

## Dependencies

- Depends on: `production/epics/participant-state-roster/story-011-tombstone-generation-and-release.md`
- Unlocks: `production/epics/participant-state-roster/story-032-get-relation-matrix.md`, `production/epics/participant-state-roster/story-033-query-hot-path-budget.md`

---

## Completion Notes
**Completed**: 2026-10-25 (participant-state-roster batch 001-014: one core built and closed together)
**Criteria**: 1/1 passing (AC-PSR-26: the nine-row matrix Player / Informant / Npc / AlreadyResolved / Stale x3 / RoundOver / AlreadyResolved; no NotTargetable row yields Npc; no fault on the server)
**Decisions**: One function in the fixed R10 order; any unrecognised state reports RosterUnrecognisedState and answers NotTargetable(Stale), never Npc. The destroyed row is bound to an Active participant and the pending-kill row is unbound, so Stale is shown to beat both Player and Npc.
**Deviations**: None. Manifest Version updated to 2026-10-25g.
**Test Evidence**: `production/qa/evidence/story-013-resolve-victim-kind-matrix/full-suite.txt` (build `Result: Succeeded`; 84/84 `StealthGame.` tests Success, 70 existing + 14 Roster) and `stamp.txt`
**Run result**: N/A — plain-C++ server core with no world, actor or screen; nothing a player can see until the facade (story 061) and the C-TeamView push step (story 062) exist
**Reviewed by**: focused code review (self-applied, unattended run): one defect fixed (StrikeLimit release aliasing, story 014); `TryReclaim`, `DisposeLivePawnOnElimination` and `EmitStateChanged` extracted to keep methods short; `RequestTransition` kept as one function by design (the R8 precedence in one place)
