# Story 016: The engine spectator state is banned: the grep finds no banned symbol and every banned call is a faulting no-op

> **Epic**: Information Visibility Policy
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

The engine's spectator path cycles the player array and sets the view target to a human's pawn, and it replicates `bIsSpectator` to everyone: each of those is a way to tell players from NPCs (leak L30). The design bans the state outright (IVP R13): every PlayerController stays in `NAME_Playing`, and pawnless viewers use a client-local camera actor. This story builds the two enforcement halves, a build-time grep check and a test that drives every banned call in a test world, so a later change cannot quietly re-enable spectating. It feeds the L30 row of the ship gate (CRITICAL, story-049) and ADR-0001 Validation Criteria.

**GDD**: `design/gdd/information-visibility-policy.md`
**Requirement**: `TR-ivp-008` (Engine spectator state banned; client-local no-pawn view)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0001: Crowd identity rules (section 4.5); also ADR-0008: Test and automation strategy for leak and parity tests (sections 1, 3, 4)
**ADR Decision Summary**: Every PlayerController stays in `NAME_Playing`; spectator RPCs fault and return; `bStartPlayersAsSpectators = false`; `SpectatorClass` replicates null; pawnless viewers use a client-local, non-replicated camera actor (ADR-0001 section 4.5, IVP R13; grep check AC-IVP-38).
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: ADR-0001 E17 [V]: the 5.8.4 `APlayerState` default set includes `bIsSpectator`, `bOnlySpectator`, `bIsInactive` (disabled or held constant false by the section 4.3 trims); the engine spectator RPCs and `ChangeState` paths are stock engine behaviour on `APlayerController`. No unreported spike owns a claim here. Whether the engine states can be made unreachable through overrides is verified by this story's test, not by a spike.

**Control Manifest Rules (this layer)**:
- Forbidden: **Never change the in-round player array; never enter the engine spectator state** — source: ADR-0001
- Required: **Every leak, parity, statistical and lint gate includes a negative control the suite detects** — source: ADR-0008
- Required: **Lints and analysis scripts emit observations only; pass/fail lives in the gate script** — source: ADR-0008
- Guardrail: **Tests**: per-commit pipeline ≤ 10 min — source: ADR-0008

---

## Acceptance Criteria

*From GDD `design/gdd/information-visibility-policy.md`, scoped to this story:*

- [ ] **AC-IVP-38** [Unit][Int] `StealthGame.Visibility.SpectatorStateBanned`: GIVEN game code and a test world, WHEN the grep check runs and a test calls `ChangeState(NAME_Spectating)`, `ChangeState(NAME_Inactive)`, `StartSpectatingOnly` and each banned spectator server RPC, THEN the grep finds no banned symbol, every call is a no-op with exactly one fault-sink report, the controller stays `NAME_Playing`, and `SpectatorClass` replicates as null. (R13, L30)

---

## Implementation Notes

*Derived from ADR-0001 section 4.5 and Implementation Guidelines, GDD R13 (the explicit lists) and ADR-0008 sections 1, 3 and 4:*

- **Grep half (T0/T1).** A grep check over game code bans `ASpectatorPawn`, `ViewAPlayer`, `SetIsSpectator`, `StartSpectatingOnly` and `ChangeState(NAME_Spectating|NAME_Inactive)` (GDD R13 lists these). Lints emit observations only (file:line findings); the gate script decides pass or fail (ADR-0008 section 1, T0). The grep scope is game code, so the test file that deliberately calls the banned functions must be excluded from the scope (otherwise the test would fail its own gate); record the exclusion rule with the lint. Per ADR-0008 section 3 a T0 lint ships one seeded violation per rule: add a seeded fixture the grep must flag.
- **Test-world half.** In a test world with a controller, call each of these separately and assert: `ChangeState(NAME_Spectating)`, `ChangeState(NAME_Inactive)`, `StartSpectatingOnly`, and each banned spectator server RPC. The banned RPC list is the explicit one in GDD R13: `ServerViewNextPlayer`, `ServerViewPrevPlayer`, `ServerViewSelf`, `ServerSetSpectatorLocation`, `ServerSetSpectatorWaiting`, `ServerRestartPlayer`. The same R13 sentence names `BeginSpectatingState`, `BeginInactiveState`, `SetSpectatorPawn` and the `PawnPendingDestroy` state change as faulting no-ops; cover them as extra cases if cheap, but the acceptance list above is the gate. Every call must be a no-op with exactly one `IStealthFaultSink` report (counting stub, ADR-0008 section 4), the controller must remain `NAME_Playing`, and `SpectatorClass` must replicate as null.
- **What to build in production.** The game PlayerController forces `NAME_Playing` on login and accepts no other state; the spectator server RPCs fault and return; the GameMode sets `bStartPlayersAsSpectators = false` (and `MustSpectate`/`CanSpectate`/`PlayerCanRestart` false, per R13); `bIsSpectator`, `bOnlySpectator` and `bIsInactive` are never written. The controller and GameMode are session-travel's (`AStealthPlayerController`, `AStealthGameMode`); until they land, this story's test runs against a harness controller stand-in and the grep check lands first.
- **Placement.** The grep wrapper and the call checks are named `StealthGame.Visibility.SpectatorStateBanned` (T1 naming); the test-world calls need a controller, so that half sits under `Source/StealthTestHarness/` while the grep wrapper sits in the owning module's `Private/Tests/Visibility/`. The split is decided in the story within ADR-0008 section 1 (T1 in the owning module, T2 and T3 code in the harness).
- **Prototype disposition.** None promoted. The prototype has no spectator guard; nothing to replace.
- **Spike gates.** None.
- **Cross-epic.** session-travel delivers `AStealthPlayerController` and `AStealthGameMode` (login forcing `NAME_Playing`, `SpectatorOnly` URL option stripping, Session rule 6); round-match-framework, participant-state-roster and the camera epic own the pawnless and `Ended` viewing states that use the client-local camera instead.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- story-020: frozen in-round player array (AC-IVP-22)
- story-019: possession canary
- story-034: presentation include and banned-symbol check (AC-IVP-17)
- session-travel epic: the production controller and GameMode login logic (this story lands the check and a stand-in)
- camera epic: the client-local no-pawn camera actor

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `tools/ci/lint/` (spectator banned-symbol grep script, seeded-violation fixture, exclusion rule for test files)
- `Source/StealthFoundation/Private/Tests/Visibility/SpectatorStateBannedTest.cpp` (class `FVisibilitySpectatorStateBannedTest`, test `StealthGame.Visibility.SpectatorStateBanned`; grep wrapper)
- `Source/StealthTestHarness/` (harness controller stand-in and the test-world call checks, until `AStealthPlayerController` exists)

---

## QA Test Cases

- **AC-1**: Spectator state banned (AC-IVP-38)
  - Given: game code and a test world with a controller.
  - When: the grep check runs; a test calls `ChangeState(NAME_Spectating)`, `ChangeState(NAME_Inactive)`, `StartSpectatingOnly` and each banned spectator server RPC.
  - Then: the grep finds no banned symbol; every call is a no-op with exactly 1 fault-sink report; the controller stays `NAME_Playing`; `SpectatorClass` replicates as null.
  - Edge cases: the banned-symbol and banned-RPC lists are the explicit ones (GDD R13, summarised in ADR-0001 section 4.5); test each RPC separately. (Tier: T1 plus test world.)

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: a T2 test under `Source/StealthTestHarness/` (the test-world call checks) OR a documented playtest/spike report; the grep wrapper is a T1 test at `Source/StealthFoundation/Private/Tests/Visibility/SpectatorStateBannedTest.cpp`

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: tuning-data-config Exit A (module skeleton, include lint, ADR-0008 harness); session-travel epic story delivering `AStealthPlayerController` and `AStealthGameMode` with `bStartPlayersAsSpectators = false` (the grep check and a harness controller stand-in can land first)
- Unlocks: None
