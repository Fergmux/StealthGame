# Story 032: GetRelation answers the viewer-subject matrix symmetrically

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
**Also governed by**: ADR-0001: Crowd identity rules
**ADR Decision Summary**: ADR-0010 keeps every body one `AStealthPawn` class driven the same way for players and NPCs, so the only authority on "is this pawn a player, and whose" is the Roster's binding table, read through its server-side face. ADR-0001 makes the pawn-to-participant link owner-only on the wire and forbids any client code from deriving player-ness from pawn pointers, so `GetRelation` (Assassination's friendly-target rejection, Concealment's per-viewer hiding) is a ServerOnly query whose answer never reaches a client.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Post-cutoff APIs named by ADR-0010 are `UMotionWarpingComponent::GetOwnerAdapter()` (5.8; MotionWarping Beta in 5.8.4) and `UE_LOGF`; neither is used here. ADR-0001 adds `APawn::RemoteViewPitch16` (5.6) and requires spikes S1, S2, S14 (S2 and S14 PASS; S1 PASS for the three pointers only), none of which gates this pure-core story. No engine API beyond `UE_LOGF` is touched.

**Control Manifest Rules (this layer)** *(ADR-0010 rules sit in the Core-layer manifest section; this story reads the Foundation rules that apply to the Roster core)*:
- Required: Queue, Roster, tuning validator, seed mixer and appearance deck are plain C++ cores (`F…Core`) with no `UWorld` dependency, behind a subsystem facade — source: ADR-0007
- Required: A reflective or Blueprint request for a server-only value on a client returns invalid and uses `ensure`, never `check` — source: ADR-0007
- Required: Every Logic story ships T1 tests; T1 tests live in the owning module's `Private/Tests/<System>/` — source: ADR-0008
- Forbidden: Never branch on `IsPlayerControlled()`, `IsBotControlled()`, controller class, `PlayerState` presence, team or Roster state in pawn, component, AnimBP or audio code for anything another client can observe — source: ADR-0001
- Forbidden: Never put team, state, connection, appearance or pawn data on a PlayerState; never give NPCs a PlayerState — source: ADR-0001
- Guardrail: Queue drain + staging + teardown + Roster flush: at most 0.5 ms listen-host game thread (queries must stay O(1)) — source: ADR-0002

This story delivers `GetRelation(PawnA, PawnB)` (R10), which answers `Self`, `Teammate`, `Opponent` or `NotBothPlayers` for any pair of pawns. It exists so Assassination can reject friendly targets and Concealment can hide per viewer without either learning the binding table. It realises GDD R10 and reuses the tombstone rule of R9 so that a dying body keeps its relation until `EndPlay`.

---

## Acceptance Criteria

*From GDD `design/gdd/participant-state-roster.md`, scoped to this story:*

- [ ] **AC-PSR-27** [Core] `StealthGame.Roster.GetRelationMatrix`: GIVEN viewer pawn A (team 0) and the subjects (A, A), (A, teammate B), (A, opponent C), (A, Retired teammate D), (A, Retired opponent E), (A, informant teammate F), (A, NPC N), (A, null), and the reversed pairs, WHEN `GetRelation` is called, THEN `Self` / `Teammate` / `Opponent` / `Teammate` / `Opponent` / `Teammate` / `NotBothPlayers` / `NotBothPlayers`, and each reversed pair gives the same result (R10).

---

## Implementation Notes

*Derived from ADR-0010 Implementation Guidelines, ADR-0001 section 4 and GDD R9/R10:*

- **Where it lives.** `GetRelation(FPawnHandle A, FPawnHandle B)` on `FRosterCore` (`Source/StealthFoundation/Private/Roster/StealthRosterCore.h/.cpp`), returning an `ERosterRelation` enum (`Self`, `Teammate`, `Opponent`, `NotBothPlayers`) declared in `Public/View/StealthRosterTypes.h`. The `URosterStore` facade maps `const APawn*` to the opaque handle `{ObjectKey, Generation}`. The return enum is a plain value type; the query itself lives in `Public/Server/` (server-only guard).
- **Algorithm.** Resolve each handle through the binding table: `BoundLive(Id)` or `Retired(Id)` yields a participant, `Unbound` or null yields none. Either side without a participant gives `NotBothPlayers`. Same participant on both sides gives `Self`. Otherwise compare `Team`: equal gives `Teammate`, different gives `Opponent`. Informants are still participants, so an informant teammate is `Teammate`. No state check is involved (a RoundEnded participant keeps its relation).
- **Retired pawns resolve through the tombstone** (R10, R9): a dying body keeps its relation until `EndPlay`/`ClearBinding`. A tombstone only matches when the handle generation matches (I5, story 011); a recycled object with a new generation is Unbound and therefore `NotBothPlayers`.
- **Symmetry.** Implement the relation as a pure function of the two resolved participants so `GetRelation(A,B) == GetRelation(B,A)` holds by construction; the test still asserts every reversed pair, including `(null, A)`.
- **Hot path.** The method does two handle lookups and nothing else; it must not allocate (R10; the budget is asserted by story 033).
- **Consumers are not built here.** Assassination (friendly-target rejection) and Concealment (per-viewer hiding) call this query from their own epics.
- **Open point:** R10 and the AC are silent on a pair where one pawn is destroyed or pending-kill while still `BoundLive`, and on a participant whose `Team` is still `None`. The AC fixtures use only assigned teams and live or Retired pawns; do not invent behaviour for the others without a design ruling.
- **Prototype disposition:** no direct prototype counterpart. The prototype decides friend or foe by comparing `AStealthCharacter::GetTeam()` values, which read the replicated PlayerState (`Source/StealthGame/Private/StealthCharacter.cpp`, and the target matching in `Source/StealthGame/Private/StealthGameMode.cpp`, which scans actors with `TActorIterator`). Those are replaced by this server-only query; they are not ported.
- **Header naming (ADR-0007).** Stealth prefix, project-unique names: `StealthRosterCore.h`, `StealthRosterTypes.h`, `StealthRosterService.h`.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 013 (ResolveVictimKind answers the full pawn-state matrix and never yields Npc for NotTargetable): owns the victim-kind query and the lookup helper this story reuses.
- Story 033 (Hot-path queries allocate nothing and probe at most twice per call): owns the allocation and probe budget, including `GetRelation`.
- Story 031 (GetLegalityClass answers per pawn state and BindPawn adopts an unbound pawn): owns the action-legality query.
- Story 011 (Tombstones respect generations and ReleaseToCrowd needs a disconnected participant): owns tombstone generation handling.
- Story 004 (Every roster call on a client is rejected or answers not-targetable and faults): owns the client-side answer for server queries.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- `Source/StealthFoundation/Private/Roster/StealthRosterCore.h` (add `GetRelation`)
- `Source/StealthFoundation/Private/Roster/StealthRosterCore.cpp`
- `Source/StealthFoundation/Public/View/StealthRosterTypes.h` (add `ERosterRelation`)
- `Source/StealthFoundation/Public/Server/StealthRosterService.h` (facade exposure)
- `Source/StealthFoundation/Private/Roster/StealthRosterStore.cpp`
- `Source/StealthFoundation/Private/Tests/Roster/StealthRosterGetRelationMatrixTest.cpp` (class `FRosterGetRelationMatrixTest`, test name `StealthGame.Roster.GetRelationMatrix`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation. (At tiers where the QL-STORY-READY gate is skipped — `minimal`, or `lean`/`solo` review mode — no qa-lead specs exist, and this section reads: N/A — no qa-lead specs at this tier; implement against the Acceptance Criteria above.)*

**Automated test specs (Logic):**

- **AC-1**: GetRelation answers the viewer-subject matrix symmetrically.
  - Given: A (team 0), teammate B, opponent C, Retired teammate D, Retired opponent E, informant teammate F, NPC N, a null pawn.
  - When: GetRelation on (A,A), (A,B), (A,C), (A,D), (A,E), (A,F), (A,N), (A,null) and all reversed pairs.
  - Then: Self, Teammate, Opponent, Teammate, Opponent, Teammate, NotBothPlayers, NotBothPlayers; each reversed pair returns the same value.
  - Edge cases: Retired pawns resolve through the tombstone.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Roster/StealthRosterGetRelationMatrixTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later). `qa.level` is full, so the test is required.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/participant-state-roster/story-013-resolve-victim-kind-matrix.md`
- Unlocks: `production/epics/participant-state-roster/story-033-query-hot-path-budget.md`
