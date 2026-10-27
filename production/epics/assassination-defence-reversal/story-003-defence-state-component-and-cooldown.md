# Story 003: Server-only defence state, cooldown from activation, and its lifecycle across possession

> **Epic**: Assassination & Defence Reversal
> **Status**: Complete
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: 2026-10-27

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

- `Source/StealthGameplay/Features/Assassination/Public/Server/DefenceStateCore.h` (new; FDefenceStateCore, public so later stories include it)
- `Source/StealthGameplay/Features/Assassination/Private/DefenceStateCore.cpp` (new)
- `Source/StealthGameplay/Features/Assassination/Public/Server/DefenceStateComponent.h` (new; UDefenceStateComponent)
- `Source/StealthGameplay/Features/Assassination/Private/DefenceStateComponent.cpp` (new)
- `Source/StealthGameplay/Features/Assassination/Public/Server/AssassinationPreparePredicate.h` (new)
- `Source/StealthGameplay/Features/Assassination/Private/PreparePredicate.cpp` (new)
- `Source/StealthGame/Private/Composition/StealthCompositionRoot.cpp`, `Source/StealthGame/Public/Server/StealthCompositionRoot.h` (Prepare predicate registered)
- `Source/StealthGameplay/Private/Tests/Assassination/AssassinationTestWorld.h` (new test fixture)
- `Source/StealthGameplay/Private/Tests/Assassination/CooldownFromActivationTest.cpp`
- `Source/StealthGameplay/Private/Tests/Assassination/DefenceStateSurvivesPossessionTest.cpp`
- `Source/StealthGameplay/Private/Tests/Assassination/DisconnectKeepsTimersTest.cpp`

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

**Status**: [x] Created and passing (2026-10-27)

---

## Dependencies

- Depends on: `production/epics/assassination-defence-reversal/story-001-assassination-tuning-keys-and-invariants.md`; cross-epic `production/epics/input-interaction-framework/story-003-action-hub-and-predicate-registry.md`, `story-042-prepare-cooldown-stamped.md`, `story-050-hold-timer-key-order.md`; `production/epics/round-match-framework/story-008-resolver-contract.md`, `story-007-intake-ordering-rules.md`
- Unlocks: `story-009-same-drain-ordering-and-braced-key-compare.md`, `story-010-reversal-counter-and-chained-strikes.md`, `story-011-npc-brace-parity-never-strike-planted.md`, `story-016-defence-owner-cooldown-component.md`

## Completion Notes
**Completed**: 2026-10-27
**Criteria**: 3/3 passing, one clause partly deferred (AC1 tap at 100.0: 159.9 and one tick before 160.0 reply Cooldown, 160.0 and one tick after execute; held-to-cap and 0.5 s release both stamp 100.0; 20 presses in a live window all reply Cooldown with stamp, window keys and CapEnd due time unchanged; the predicate object on a spawned pawn with a fake clock. AC2 rejoin, ReleaseToCrowd and PawnRecovery each keep the stamp (Prepare at 140 replies Cooldown); unknown-stamp recovery stamps 130 (189.9 Cooldown, 190.0 executes); Reset clears. AC3 disconnect at +1.0 leaves the window to CapEnd: an Attack keyed before CapEnd reads braced, one after reads not braced; a disconnecting attacker's lockout ends at t_exec + 1.75 (1.74 locked, 1.75 free); replies to the released sink and the null sink are discarded. DEFERRED to story 015: "the null-sink spy receives exactly one FStrikeResult", which needs the result builder) — `StealthGame.Assassination.CooldownFromActivation`, `...DefenceStateSurvivesPossession`, `...DisconnectKeepsTimers`
**Deviations**: ADVISORY — `DefenceStateCore.h` is public (`Features/Assassination/Public/Server/`) rather than Private, because the component exposes it and stories 008/009/016 include it; files live under `Features/Assassination/` (Crowd layout). ADVISORY — the component is created server-side on the first defence write (`UDefenceStateComponent::FindOrAdd`) instead of being a default subobject of `AStealthPawn` (Core may not include a Feature header, ADR-0007); a pawn without it reads as T_act none, identically for every pawn kind. ADVISORY (stand-in) — the IIF hold machinery (IIF 042-048) is not built: `FDefenceStateCore` keeps a minimal window record (Begin key, first End key) with `IsWindowOpenAt(k_A)` implementing F2's key clauses; story 009 adds Slot(V) = Bracing and the IIF later owns the debounce/CapEnd items. ADVISORY — `UAssassinationPreparePredicate` is registered for `EActionType::Prepare` from `UStealthCompositionRoot`; until the in-slot resolver stamps (stories 007-011) it always answers Allow, so play is unchanged. Tuning stand-in: schema defaults until the TDC store feeds features. `UEncounterService`'s strike-role table is not part of this story's ACs and is left to stories 007-009. Cross-epic IIF 003/042/050 and RMF 007/008 proceeded with risk accepted.
**Test Evidence**: the three test files above under `Source/StealthGameplay/Private/Tests/Assassination/`; run in `production/qa/evidence/assassination-defence-reversal-001-004-2026-10-27/full-suite.txt`
**Run result**: N/A — no production path stamps a Prepare yet, so the registered predicate answers Allow exactly as the absent predicate did; nothing a player sees changes
**Reviewed by**: self, no independent review (focused /code-review pass by the implementing agent; unattended batch)
