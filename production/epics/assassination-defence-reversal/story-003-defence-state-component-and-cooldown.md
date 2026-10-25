# Story 003: Server-only defence state, cooldown from activation, and its lifecycle across possession

> **Epic**: Assassination & Defence Reversal
> **Status**: Ready
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/assassination-defence-reversal.md` (ASN-R1, ASN-R3, ASN-R18; Formulas F3; Edge Cases "Defender"; AC-ASN-14, AC-ASN-18, AC-ASN-42)
**Requirement**: `TR-asn-002`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0018: Encounter data and outcome presentation timing (§4.1)
**ADR Decision Summary**: `UEncounterService` owns the strike-role table beside a non-replicated per-pawn `UDefenceStateComponent` holding `LastPrepareActivation` and `LockoutEnd` for every pawn, NPC or player; all gameplay timing runs through the round's queue, never an engine timer.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0018 overall; the cooldown logic is a plain C++ core and carries no post-cutoff API)
**Engine Notes**: The component is a non-replicated `UActorComponent` identical on every pawn (ADR-0001 carrier model, Pillar-1 kind-blindness). The cooldown arithmetic lives in `FDefenceStateCore` with an injected clock and no `UWorld`, so the AC tests run headless. Server time is the double server clock (ADR-0002); never `FTimerManager`.

**Control Manifest Rules (this layer)**:
- Required: Defence state is server-only, not replicated, one component class on every pawn regardless of kind — source: ADR-0018, ADR-0001
- Required: Feature predicates are registered weakly from the composition root per `EActionType`; `Prepare`'s `CanBegin` returns `Reject(Cooldown)` iff F3 says the cooldown is running, else `Allow` — source: ADR-0011
- Forbidden: Never reset, extend or refund the cooldown on any event (re-press, possession change, Interact, camera toggle); never use `FTimerManager` or a feature tick for the window or cooldown — source: ADR-0002, ADR-0018
- Guardrail: One component per pawn, O(1) per query; two doubles of state

This story is the second half of the defence: the cooldown stamp. The window (the protected interval, its debounce and `CapEnd`) is the IIF's hold machinery; Assassination adds the stamp, the `Prepare` predicate and the possession rules of ASN-R18. `LastPrepareActivation` is written at `Prepare(Begin)` execution, so a tapped, released-early or never-used brace burns the full 60 s. `LockoutEnd` (the attacker lockout stamp) is kept on the same component but is written by story 008.

---

## Acceptance Criteria

*From GDD `design/gdd/assassination-defence-reversal.md` (AC-ASN-14, AC-ASN-18, AC-ASN-42), scoped to this story:*

- [ ] **AC1** (AC-ASN-14, [Logic/BLOCKING] `StealthGame.Assassination.CooldownFromActivation`): **GIVEN** a brace tapped at t = 100.0 (window 0.10 s) **WHEN** `Prepare(Begin)` is sent at 159.9 **THEN** it replies `Cooldown`, and at 160.0 (± 1 server tick) it executes; **AND** a brace held to its 4.0 s cap and one released at 0.5 s both stamp 100.0; 20 presses during a window change no stamp, end time or event.
- [ ] **AC2** (AC-ASN-18, [Logic/BLOCKING] `StealthGame.Assassination.DefenceStateSurvivesPossession`): **GIVEN** a stamp at t = 100 **WHEN** the pawn is rejoined, released to the crowd, and (separately) re-pawned by `PawnRecovery` at t = 130 **THEN** a `Prepare` at t = 140 replies `Cooldown` in all three cases; **AND** a `PawnRecovery` with an unknown stamp stamps t = 130.
- [ ] **AC3** (AC-ASN-42, [Logic/BLOCKING] `StealthGame.Assassination.DisconnectKeepsTimers`): **GIVEN** a pawn braced at t = 0 that disconnects at +1.0 s **WHEN** an Attack resolves at +3.9 s and (separately) at +4.0 s (± 1 server tick, keyed after the `CapEnd` item) **THEN** the first is a reversal and the second is a kill; **AND** GIVEN an attacker that disconnects mid-lockout **THEN** its lockout ends at t_exec + 1.75 and its result goes to the null sink.

---

## Implementation Notes

*Derived from ADR-0018 §4.1 and GDD ASN-R18, F3:*

`CanPrepare(t) = (T_act = none) ∨ (t − T_act ≥ C)` with `C = DefenceCooldownSec`; the boundary is inclusive, tests allow ± 1 server tick. `T_act` is `none` at round start and after `Reset`. The brace window itself ends at the IIF's `End` (arrival + `ReleaseDebounceSec`) or at `HoldTimer(CapEnd)` after `DefenceDurationSec` (4.0 s); both are IIF items (stories 044 and 045 there), and a pawn whose controller disconnects or is handed to AI hold sends no `End`, so `CapEnd` closes the window, which AC3 exercises through Round's key order, not wall-clock.

Possession rules (ASN-R18): rejoin into the same pawn keeps the component; a takeover pawn keeps its own stamp (irrelevant while `Informant`); `ReleaseToCrowd` keeps it, so Crowd's NPC braces honour the same cooldown; a `PawnRecovery` re-pawn of an `Active` player copies the lost pawn's stamp or stamps now when it is unknown (never a free brace). `Reset` clears every stamp (story 019 asserts it). The owner-only `CooldownEndServerTime` is written from this stamp by story 016.

`UEncounterService` owns the strike-role table (`Ready`, `Bracing`, `Countering`, `Striking`, `Cooling`, `Dead`) beside the component; `Cooling` and the lockout are independent timers (a pawn can be `Striking` while its brace cooldown runs).

**Prototype promoted as reference logic:** `FStealthRules::CanStartDefence`, `DefenceCooldownRemaining` and `ShouldEndDefence` (test `StealthGame.Rules.Defence`: cooldown measured from activation including unused attempts, 69.9 blocked and 70.0 allowed at a 60 s cooldown from t = 10) are the proven spec for AC1 and must be re-expressed on the component and the queue key, not copied. The prototype keeps the stamp on `AStealthPlayerState::LastDefenceActivationTime`; that is replaced by this per-pawn component (story 027 removes it).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-009-same-drain-ordering-and-braced-key-compare.md`: the braced-at-Attack key comparison (F2).
- `story-016-defence-owner-cooldown-component.md`: the owner-only `CooldownEndServerTime` on the PlayerController.
- `story-008-attacker-lockout-and-attack-lifecycle.md`: writing `LockoutEnd` and the lockout rules.
- IIF epic: `Prepare` window bounds, debounce merge, planted `Move` clamps (stories 042 to 048).

---

## Files

- `Source/StealthGameplay/Public/Server/Features/Assassination/DefenceStateComponent.h` (`UDefenceStateComponent`, non-replicated)
- `Source/StealthGameplay/Private/Features/Assassination/DefenceStateCore.h` and `.cpp` (`FDefenceStateCore`, plain C++)
- `Source/StealthGameplay/Private/Features/Assassination/DefenceStateComponent.cpp`
- `Source/StealthGameplay/Private/Features/Assassination/PreparePredicate.cpp` (`CanBegin` for `Prepare`)
- `Source/StealthGameplay/Private/Tests/Assassination/CooldownFromActivationTest.cpp` (class `FAssassinationCooldownFromActivationTest`)
- `Source/StealthGameplay/Private/Tests/Assassination/DefenceStateSurvivesPossessionTest.cpp` (class `FAssassinationDefenceStateSurvivesPossessionTest`)
- `Source/StealthGameplay/Private/Tests/Assassination/DisconnectKeepsTimersTest.cpp` (class `FAssassinationDisconnectKeepsTimersTest`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

QL-STORY-READY: GAPS
- AC1: ± 1 tick is used around a 0.1 s margin (159.9 vs 160.0) with no tick length given, and "20 presses ... change no stamp, end time or event" gives neither the reply code nor what an "event" is; proposed rewrite (unconfirmed): state the server tick length (ADR-0002); assert the boundary on the queue key, not wall-clock; during a live window 20 `Prepare(Begin)` presses all reply with the IIF's no-op code (confirm with the IIF epic) and `LastPrepareActivation`, the `CapEnd` key and the Event Log row count are unchanged.
- AC2: rejoin and release times are not given and the unknown-stamp case asserts nothing after t = 130; proposed rewrite (unconfirmed): rejoin at 110, release at 120, recovery at 130; for the unknown-stamp case `Prepare` at 189.9 replies `Cooldown` and at 190.0 executes.
- AC3: Attacks at +3.9 and +4.0 are 0.1 s apart so a ± 1 tick tolerance cannot also say "keyed after `CapEnd`", and the lockout half has no observable; proposed rewrite (unconfirmed): assert by key: an Attack keyed before the `CapEnd` item is a reversal and one keyed after is a kill; for the disconnecting attacker an Attack at t_exec + 1.74 replies `Busy` and one at t_exec + 1.75 executes, and the null-sink spy receives exactly one `FStrikeResult`.

- **AC-1**: Cooldown runs from activation
  - Given: stamp 100.0 from a 0.10 s tap, `DefenceCooldownSec` 60, injected clock and tick length
  - When: `Prepare(Begin)` at 159.9 and 160.0; separately a 4.0 s capped hold and a 0.5 s release; 20 presses in a window
  - Then: 159.9 replies `Cooldown`, 160.0 executes; both holds stamp 100.0; presses change no stamp, `CapEnd` key or event row
  - Edge cases: boundary inclusive; an unused brace burns the full 60 s; no refund on re-press
- **AC-2**: Stamp survives possession changes
  - Given: stamp 100; runs for rejoin, `ReleaseToCrowd`, `PawnRecovery` at 130; plus a recovery with an unknown stamp
  - When: `Prepare` is sent at 140 in each known-stamp run
  - Then: all three reply `Cooldown`; the unknown-stamp recovery stamps 130, so `Prepare` at 189.9 replies `Cooldown` and at 190.0 executes
  - Edge cases: recovery never gives a free brace; `Reset` clears the stamp (story 019)
- **AC-3**: Disconnect keeps the window and lockout timers
  - Given: pawn braced at t = 0, controller disconnects at +1.0, `CapEnd` at +4.0
  - When: an Attack is keyed just before and just after the `CapEnd` item; an attacker disconnects mid-lockout
  - Then: earlier Attack is a reversal, later a kill; lockout ends at t_exec + 1.75 and the result goes to the null sink exactly once
  - Edge cases: no `End` is sent after disconnect so `CapEnd` alone closes the window; assert by key, not wall-clock

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: the three test files above under `Source/StealthGameplay/Private/Tests/Assassination/` — must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/assassination-defence-reversal/story-001-assassination-tuning-keys-and-invariants.md`; cross-epic `production/epics/input-interaction-framework/story-003-action-hub-and-predicate-registry.md`, `story-042-prepare-cooldown-stamped.md`, `story-050-hold-timer-key-order.md`; `production/epics/round-match-framework/story-008-resolver-contract.md`, `story-007-intake-ordering-rules.md`
- Unlocks: `story-009-same-drain-ordering-and-braced-key-compare.md`, `story-010-reversal-counter-and-chained-strikes.md`, `story-011-npc-brace-parity-never-strike-planted.md`, `story-016-defence-owner-cooldown-component.md`
