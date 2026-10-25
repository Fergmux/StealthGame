# Story 031: GetLegalityClass answers per pawn state and BindPawn adopts an unbound pawn

> **Epic**: Participant State & Roster
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/participant-state-roster.md`
**Requirement**: `TR-roster-004`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0010: Shared Pawn: CMC locomotion, spawn factory and possession parity
**ADR Decision Summary**: ADR-0010 makes `UStealthPawnFactory` the only creator and possessor of Shared Pawns, and its `TakeOverCrowdPawn` owns the eligibility check and then calls the Roster's `BindPawn` itself, atomically, creating no actor and no NetGUID (crowd adoption). The movement rules learn whether a pawn's participant is an Informant only through one injected query, `IGaitLegalitySource`, which on the server calls Roster `GetLegalityClass(Pawn)`; an Unbound pawn answers `Active` so the answer never exposes the binding. The prototype's `IsInformant()` read from the replicated PlayerState is replaced, not ported.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Post-cutoff APIs named by ADR-0010 are `UMotionWarpingComponent::GetOwnerAdapter()` (5.8; MotionWarping is Beta in 5.8.4) and `UE_LOGF`; neither is used here. ADR-0010 verification items (traversal spike, S8 hand-over, S14, H1, ST1, P1, ADR-0002 E4 drain-spawn spike) do not gate this pure-core story, which touches no engine API beyond `UE_LOGF`.

**Control Manifest Rules (this layer)** *(ADR-0010 rules sit in the Core-layer manifest section; this story reads the Foundation rules that apply to the Roster core)*:
- Required: Queue, Roster, tuning validator, seed mixer and appearance deck are plain C++ cores (`F…Core`) with no `UWorld` dependency, behind a subsystem facade — source: ADR-0007
- Required: A reflective or Blueprint request for a server-only value on a client returns invalid and uses `ensure`, never `check` — source: ADR-0007
- Required: Every Logic story ships T1 tests; T1 tests live in the owning module's `Private/Tests/<System>/` — source: ADR-0008
- Forbidden: Never branch on `IsPlayerControlled()`, `IsBotControlled()`, controller class, `PlayerState` presence, team or Roster state in pawn, component, AnimBP or audio code for anything another client can observe — source: ADR-0001
- Forbidden: Never call `RestartPlayer` or `SpawnDefaultPawnFor`; every pawn comes from `UStealthPawnFactory` — source: ADR-0005, ADR-0010
- Guardrail: C-TeamView pushes change-only, at most one per frame, within the 50 KB/s per-client budget (this story adds no push) — source: ADR-0001

This story delivers the server-side action-legality query `GetLegalityClass` (R10) and proves the crowd-adoption bind path (R9): an awaiting informant adopts an existing Unbound pawn, with no fresh pawn involved. It exists so the Input & Interaction Framework and the movement gait clamp can ask "what may this pawn do?" without learning whether the pawn is bound. It realises GDD R9 (crowd adoption), R10 (`GetLegalityClass`), F2 (`AwaitingPawn`) and invariant I3.

---

## Acceptance Criteria

*From GDD `design/gdd/participant-state-roster.md`, scoped to this story:*

- [ ] **AC-PSR-51** [Core] `StealthGame.Roster.LegalityClassAndAdoption`: GIVEN Unbound, BoundLive-Active, BoundLive-Informant and Retired pawns, WHEN `GetLegalityClass` is called, THEN it returns `Active` / `Active` / `Informant` / the participant's state. GIVEN an awaiting informant and an Unbound pawn, WHEN `BindPawn` adopts it, THEN `Applied`, and no fresh-spawn path is involved (R9, R10).

---

## Implementation Notes

*Derived from ADR-0010 Implementation Guidelines (sections 2, 3.2), ADR-0007 section 5 and GDD R9/R10:*

- **Where it lives.** `GetLegalityClass(FPawnHandle)` is a const method on `FRosterCore` (`Source/StealthFoundation/Private/Roster/StealthRosterCore.h/.cpp`). It returns the participant-state enum (`Active`, `Informant`, `RoundEnded`) declared in `Public/View/StealthRosterTypes.h` (story 001), the same value type ADR-0010's `IGaitLegalitySource::GetLegalityClass` returns. The `URosterStore` facade (`Public/Server/StealthRosterService.h`, `Private/Roster/StealthRosterStore.cpp`) converts `const APawn*` to the opaque handle `{ObjectKey, Generation}` and delegates; the core never sees a `UWorld` or an `APawn`.
- **Rule (R10).** Look the handle up in the binding table (story 010). `BoundLive(Id)` or `Retired(Id)` returns that participant's current `State`. An `Unbound` pawn returns `Active`, so the answer never exposes the binding. A Retired handle resolves through its tombstone only when the generation matches (I5, story 011); do not duplicate tombstone logic, call the shared lookup.
- **Allocation-free.** The method is on the query hot path described by R10; use the same table lookup as `ResolveVictimKind` and `IsPawnBound` (budget asserted by story 033).
- **Adoption is `BindPawn`, not new code.** The adoption half exercises story 010's `BindPawn` with the crowd-adoption fixture: a participant in `Informant` with no live pawn (counted in `AwaitingPawn(T)`, F2) and a pre-existing `Unbound` pawn handle. Assert `Applied`, assert the handle now bound to the participant equals the handle of the pre-existing Unbound pawn (the Roster mints no handle), and assert `AwaitingPawn(T)` falls by 1.
- **No eligibility logic in the core.** Eligibility (`Unbound`, not Disabled, no action pose, not in traversal and so on) belongs to `UStealthPawnFactory::TakeOverCrowdPawn` in `StealthGameplay` (ADR-0010 section 3.2); the Roster only binds. Do not add any of it here.
- **Fixtures.** Build the `Informant` participant with `RequestTransition` (story 014) so the pawn is Retired through the real path. The `RoundEnded` fixture for the Retired case: story 023 (`EndRoundAll`) is not a declared dependency, so build it through the same core test seam story 010 uses (`WITH_STEALTH_TEST_SEAMS`, never compiled into Shipping), and switch to `EndRoundAll` once story 023 lands.
- **Client behaviour** of `GetLegalityClass` is pinned by story 004 (`ClientCallsRejected`); do not define it here.
- **Open point:** R10 does not say what `GetLegalityClass` returns for a null, destroyed or pending-kill pawn. The conservative reading is that the handle resolves as Unbound and the answer is `Active` (no binding exposed). Confirm with the design owner before asserting it; the AC does not cover it.
- **Open point:** the QA gap below says "no fresh-spawn path is involved" has no observable in a pure-core test. This story asserts handle identity instead; the spawn-path guarantee is covered by story 043 (`player-pawn-bound-before-visible`).
- **Prototype disposition:** no prototype counterpart in the roster core. The prototype's `AStealthCharacter::IsInformant()` and `IsNpc()` (`Source/StealthGame/Private/StealthCharacter.cpp`, reading the replicated PlayerState) are the client-readable pattern this query replaces; their consumers (the movement gait clamp in `StealthMovementComponent.cpp`) move to `IGaitLegalitySource` in the shared-character-locomotion epic, not in this story.
- **Header naming (ADR-0007).** Stealth prefix, project-unique names, all under `View/` or `Server/`: `StealthRosterCore.h`, `StealthRosterDeps.h`, `StealthRosterTypes.h`, `StealthRosterService.h`.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 010 (BindPawn rejects an already-bound participant, an in-use pawn and a round-ended participant): owns `BindPawn`, the binding table and its guards.
- Story 011 (Tombstones respect generations and ReleaseToCrowd needs a disconnected participant): owns tombstone generation handling.
- Story 013 (ResolveVictimKind answers the full pawn-state matrix and never yields Npc for NotTargetable): owns the victim-kind query.
- Story 014 (Active to Informant applies for each entering reason with the correct pawn disposition): owns `RequestTransition`.
- Story 004 (Every roster call on a client is rejected or answers not-targetable and faults): owns the client answer for this query.
- Story 033 (Hot-path queries allocate nothing and probe at most twice per call): owns the allocation and probe budget.
- Story 043 (Player pawns are bound before BeginPlay and a failed bind destroys the pawn before FinishSpawning): owns the fresh-spawn path.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- `Source/StealthFoundation/Private/Roster/StealthRosterCore.h` (add `GetLegalityClass`)
- `Source/StealthFoundation/Private/Roster/StealthRosterCore.cpp`
- `Source/StealthFoundation/Public/Server/StealthRosterService.h` (facade exposure of `GetLegalityClass`)
- `Source/StealthFoundation/Private/Roster/StealthRosterStore.cpp`
- `Source/StealthFoundation/Private/Tests/Roster/StealthRosterLegalityClassAdoptionTest.cpp` (class `FRosterLegalityClassAndAdoptionTest`, test name `StealthGame.Roster.LegalityClassAndAdoption`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation. (At tiers where the QL-STORY-READY gate is skipped — `minimal`, or `lean`/`solo` review mode — no qa-lead specs exist, and this section reads: N/A — no qa-lead specs at this tier; implement against the Acceptance Criteria above.)*

*Test cases not yet defined — run /qa-plan to generate them.*

**Flagged gap (QL-STORY-READY: GAPS):** "no fresh-spawn path is involved" has no observable in a [Core] test (the core has no spawn dependency). "The participant's state" for a Retired pawn does not say which state. Needs: assert the adopted pawn's handle equals the pre-existing Unbound handle; move spawn-path coverage to story 043.

**Provisional spec (qa-lead, pending gap resolution):**

- **AC-1**: GetLegalityClass answers per pawn state; BindPawn adopts an unbound pawn.
  - Given: Unbound, BoundLive-Active, BoundLive-Informant, Retired pawns, and an awaiting informant with an Unbound pawn.
  - When: GetLegalityClass called on each; then BindPawn adopts.
  - Then: Active, Active, Informant, and the participant's state (use Informant, plus RoundEnded after EndRoundAll). BindPawn returns Applied and the bound handle equals the adopted pawn.
  - Edge cases: an Unbound pawn returns Active so the binding is not exposed.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Roster/StealthRosterLegalityClassAdoptionTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later). `qa.level` is full, so the test is required.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/participant-state-roster/story-010-bind-pawn-guards.md`, `production/epics/participant-state-roster/story-014-transition-active-to-informant.md`
- Unlocks: None
