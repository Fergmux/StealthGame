# Story 043: Player pawns are bound before BeginPlay and a failed bind destroys the pawn before FinishSpawning

> **Epic**: Participant State & Roster
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/participant-state-roster.md`
**Requirement**: `TR-roster-004`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0010: Shared Pawn: CMC locomotion, spawn factory and possession parity
**ADR Decision Summary**: ADR-0010 makes `UStealthPawnFactory` the only code that spawns a Shared Pawn, in one call stack: deferred spawn, `ApplyAppearance`, `Roster.BindPawn` (when a participant is set), `FinishSpawning`, then possess. On any bind rejection the factory destroys the pawn before `FinishSpawning`, so no client ever receives the actor and the bind is never network-visible; `HandleStartingNewPlayer`, `RestartPlayer` and `SpawnDefaultPawnFor` are dead paths. This removes any window in which a live player pawn is unbound and would read as an NPC.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Post-cutoff APIs named by ADR-0010: `UMotionWarpingComponent::GetOwnerAdapter()` (5.8; MotionWarping Beta in 5.8.4) and `UE_LOGF`; neither is used here. Verification items relevant here: the ADR-0002 E4 spike (a CMC pawn spawned inside `TG_PostPhysics` behaves on its first tick; if it fails, factory spawns move to ADR-0002's post-drain pass with the sequence unchanged), S14. This story runs against a stub of the factory, so neither is verified here.

**Control Manifest Rules (this layer)** *(ADR-0010 rules sit in the Core-layer manifest section; this story reads the Foundation rules that apply)*:
- Required: `HandleStartingNewPlayer` is a no-op; every pawn comes from `UStealthPawnFactory` — source: ADR-0005, ADR-0010
- Required: Authoritative match state lives only in the four game-instance stores, keyed by `ParticipantId`, never by controller or PlayerState pointer — source: ADR-0005
- Required: Every Integration story ships a T2 or T3 test or a documented playtest — source: ADR-0008
- Forbidden: Never call `RestartPlayer` or `SpawnDefaultPawnFor` — source: ADR-0005, ADR-0010
- Forbidden: Never compile negative-control types or test seams into Shipping (`StealthTestHarness` or `WITH_STEALTH_TEST_SEAMS` only) — source: ADR-0008
- Guardrail: Queue drain + staging + teardown + Roster flush: at most 0.5 ms listen-host game thread (a bind inside a slot must be O(1)) — source: ADR-0002

This story proves, against a stub spawn factory, that the Roster's `BindPawn` contract supports the R9 fresh-spawn path: ten player pawns are bound by their `BeginPlay`, and a pawn whose bind is forced to fail is destroyed before `FinishSpawning`, never receives `BeginPlay`, and leaves no entry in the binding table. It exists because a live-but-unbound player pawn would answer as an NPC and make an attack read as an NPC kill. It realises GDD R9 ("a player pawn is bound before it can be seen or targeted", fresh-spawn path) and the Edge Case "if a player pawn exists for one frame before `BindPawn`".

---

## Acceptance Criteria

*From GDD `design/gdd/participant-state-roster.md`, scoped to this story:*

- [ ] **AC-PSR-23** [Int] `StealthGame.Roster.PlayerPawnBoundBeforeVisible`: GIVEN the Shared Character spawn path, WHEN 10 player pawns spawn and one bind is forced to fail, THEN at each player pawn's `BeginPlay` `IsPawnBound` = true, and the failed one is destroyed before `FinishSpawning` (R9).

---

## Implementation Notes

*Derived from ADR-0010 section 3.1 and Implementation Guidelines, ADR-0005 guideline 2 and GDD R9:*

- **Stub until the real factory lands.** `UStealthPawnFactory` is in `StealthGameplay` (shared-character-locomotion epic); Foundation tests cannot include it. Write a test-local stub factory (a test-only helper in `Private/Tests/Roster/`) that performs exactly ADR-0010 section 3.1's sequence against a real `URosterStore`/`FRosterCore`: deferred spawn of a stub pawn, `BindPawn(Participant, Pawn, ExpectedEpoch)`, on rejection destroy before finishing, otherwise `FinishSpawning`. The stub pawn records `IsPawnBound` from its `BeginPlay`. When the real factory exists, switch the test to drive it; the assertions do not change. Peer epic: `production/epics/shared-character-locomotion/EPIC.md`.
- **What the Roster must guarantee here.** (1) `BindPawn` accepts a deferred-spawned pawn that has not yet received `FinishSpawning`: the injected pawn-liveness provider must report a freshly spawned, not-yet-begun actor as valid (not destroyed, not pending kill), otherwise `BindPawn` gives `Rejected_PawnInUse`. (2) A rejected bind leaves the binding table untouched (no partial entry), so the destroyed pawn is not in the table (assertion in the AC). (3) `IsPawnBound(Pawn)` is true as soon as `BindPawn` returns `Applied`, before `FinishSpawning`, hence at `BeginPlay`.
- **Forcing a failure.** The AC says one bind is forced to fail; the spec does not say how. Use any input that makes `BindPawn` return a rejection (for example a stale `ExpectedEpoch` giving `Rejected_StaleEpoch`, or a participant already holding a live pawn giving `Rejected_AlreadyBound`) and record the choice. The assertion is the same: destroyed before `FinishSpawning`, no `BeginPlay`, not bound.
- **Single spawn site.** R9 allows exactly one named spawn site; the default pawn paths (`SpawnDefaultPawnAtTransform`, `RestartPlayer`) stay dead (ADR-0005 guideline 2; ADR-0010 section 3.3). Do not add a second creation path to make the test pass.
- **Round-boundary and replication.** The bind is never network-visible because replication starts on the next net tick, after `FinishSpawning` (R9). This story does not test the wire.
- **Prototype disposition:** replaces the prototype's `RestartPlayer`-based spawn and its spawn paths in `Source/StealthGame/Private/StealthGameMode.cpp` (`RestartPlayer(Player)` and the NPC `SpawnActor`), which bind nothing; those are the patterns the factory replaces (ADR-0010 Implementation Guidelines). The real factory is built by the shared-character-locomotion epic, not here.
- **Header naming (ADR-0007).** Stealth prefix, project-unique names: `StealthRosterService.h`, plus a test-only `StealthRosterSpawnFactoryStub.h` under `Private/Tests/Roster/`.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 010 (BindPawn rejects an already-bound participant, an in-use pawn and a round-ended participant): owns `BindPawn` and its guards.
- Story 031 (GetLegalityClass answers per pawn state and BindPawn adopts an unbound pawn): owns the crowd-adoption bind path.
- Story 049 (Player and NPC pawns replicate identically and bind, transition or possession changes no setting): blocked parity proof.
- Story 056 (A disconnect hand-over never decelerates faster than the crowd stop decel and never destroys the pawn): hand-over.
- The real `UStealthPawnFactory`, `AStealthPawn` and the possession hand-over pass: shared-character-locomotion epic.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- `Source/StealthFoundation/Private/Roster/StealthRosterCore.cpp` (only if `BindPawn` needs a fix so a rejected bind leaves no partial entry)
- `Source/StealthFoundation/Private/Roster/StealthRosterDeps.h` (liveness provider semantics for a deferred-spawn pawn)
- `Source/StealthFoundation/Private/Tests/Roster/StealthRosterSpawnFactoryStub.h` (test-only stub factory and stub pawn)
- `Source/StealthFoundation/Private/Tests/Roster/StealthRosterPawnBoundBeforeVisibleTest.cpp` (class `FRosterPlayerPawnBoundBeforeVisibleTest`, test name `StealthGame.Roster.PlayerPawnBoundBeforeVisible`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation. (At tiers where the QL-STORY-READY gate is skipped — `minimal`, or `lean`/`solo` review mode — no qa-lead specs exist, and this section reads: N/A — no qa-lead specs at this tier; implement against the Acceptance Criteria above.)*

**Automated test specs (Integration):**

- **AC-1**: player pawns bound before BeginPlay; a failed bind destroys the pawn before FinishSpawning.
  - Given: stubbed UStealthPawnFactory, 10 spawns, one bind forced to fail, a probe recording IsPawnBound in BeginPlay.
  - When: the 10 spawns run.
  - Then: for each of the nine successes IsPawnBound is true in BeginPlay. The failed pawn is destroyed, never receives FinishSpawning or BeginPlay, and is not in the binding table.
  - Edge cases: runs against a stub until the shared-character-locomotion epic lands.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthFoundation/Private/Tests/Roster/StealthRosterPawnBoundBeforeVisibleTest.cpp` — automated test must exist and pass (OR a documented playtest; `qa.level` is full, so the automated test is required).

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/participant-state-roster/story-010-bind-pawn-guards.md`
- Depends on (cross-epic): `production/epics/shared-character-locomotion/EPIC.md` ([Dep:Shared Character] spawn factory `UStealthPawnFactory`; the test runs against a stub of its interface until that epic lands)
- Unlocks: `production/epics/participant-state-roster/story-049-pie-pawn-replication-parity.md`
