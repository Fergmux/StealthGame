# Story 007: CanBegin for HideEnter and HideExit gives the same reply for every pawn kind and every occupancy

> **Epic**: Concealment (Bushes)
> **Status**: Complete
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: 2026-10-29

## Context

**GDD**: `design/gdd/concealment-bushes.md` (CON-R3, CON-R14, AC-CON-06, AC-CON-19 first half)
**Requirement**: `TR-con-002`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0017: Concealment component and per-viewer presentation (§2 commands and legality), with ADR-0011 (pawn command and predicate registry)
**ADR Decision Summary**: `Hide(Enter(BushId))` and `Hide(Exit(Side))` enter through `UPawnCommandComponent` for humans and Crowd alike; Concealment registers its `CanBegin` predicate for HideEnter/HideExit from the composition root. Legality is IIF class/locomotion legality (`NotNow`), slot (`Busy`), reach on an open side (`OutOfReach`) and own re-entry cooldown (`Cooldown`); occupancy, controller kind and team are never read.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0017 overall; plain C++ predicate)
**Engine Notes**: No engine API. Distances are inclusive (≤ 60 cm is in reach), measured horizontally from the capsule centre to the planter's outer edge; the 0.35 m curb lies inside the 2.0 x 2.0 m footprint.

**Control Manifest Rules (this layer)**:
- Required: Every Hide enters through `UPawnCommandComponent` — source: ADR-0017
- Forbidden: Never let Hide legality, replies or timing read occupancy, controller kind or team — source: ADR-0017
- Guardrail: Byte-identical reply at occupancy 0, 1 and 4 (a "full" reply would be an occupancy oracle)

The cooldown only stops same-bush flicker and applies to NPCs identically; it is not to be lengthened to fight bush-hopping (bounded by the rustle and by `BushSpacingCm`).

---

## Acceptance Criteria

*From GDD `design/gdd/concealment-bushes.md` (AC-CON-06, AC-CON-19), scoped to this story:*

- [ ] **AC1** (AC-CON-06, [U] `StealthGame.Concealment.EntryLegality`, table-driven, every row at occupancy 0, 1 and 4): reach 59 and 60 cm from an open side → `Allow`; 61 → `OutOfReach`; 30 cm from a closed side → `OutOfReach`; running, traversing, informant, `RoundEnded` → `NotNow`; slot held → `Busy`; 5.9 s after an exit → `Cooldown`, 6.0 and 6.1 s → `Allow`; 5.9 s after an **emergence** → `Cooldown`.
- [ ] **AC2** (AC-CON-06 tail and AC-CON-19 first half, [U] `StealthGame.Concealment.SamePathForNpcs`): **GIVEN** the same rows for an AI-driven pawn **THEN** the replies are identical; the reply is byte-identical on the same tick at occupancy 0, 1 and 4; both pawn kinds enter through the one IIF Hide entry point with identical legality, timings and cooldown; Concealment has no IsAI input and legality reads no controller kind, team or occupancy.

---

## Implementation Notes

*Derived from ADR-0017 Decision §2:*

Enter legality (CON-R3): IIF class/locomotion legality (not running, not traversing, not `RoundEnded`, Active — every unbound pawn counts as Active) → `NotNow`; IIF R9 slot free → `Busy`; capsule-centre horizontal distance ≤ `BushEntryReachCm` to the planter's outer edge on a side in the `EntryAxis` set → else `OutOfReach`; own re-entry cooldown `BushReenterCooldownSec` elapsed since the last exit/emergence completion → else `Cooldown`. Rejects carry a code the owner could compute itself. Occupancy is never read: there is no capacity limit. The entry side is the open side nearest the pawn at execution. Register through the ActionHub predicate registry (IIF story-003). Precedence when several guards fail is the IIF legality matrix's (IIF story-033). Crowd's bush-selection invariance (second half of AC-CON-19) is Crowd's `story-022-bush-herring.md`.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-004-hide-enter-execution-and-commit.md`, `story-005-hide-exit-execution-and-blocked-side.md`: execution.
- Crowd epic `story-022-bush-herring.md`: NPC bush selection invariance.
- IIF `story-052-no-brace-or-tend-in-bush.md`: the Prepare/Interact `NotNow` inside a bush.

---

## Files

- `Source/StealthGameplay/Features/Concealment/Public/Server/ConcealmentLegality.h`
- `Source/StealthGameplay/Features/Concealment/Private/ConcealmentLegality.cpp`
- `Source/StealthGameplay/Private/Tests/Concealment/EntryLegalityTest.cpp` (class `FConcealmentEntryLegalityTest`, name `StealthGame.Concealment.EntryLegality`)
- `Source/StealthGameplay/Private/Tests/Concealment/SamePathForNpcsTest.cpp` (class `FConcealmentSamePathForNpcsTest`, name `StealthGame.Concealment.SamePathForNpcs`)

---

## QA Test Cases

*Written by qa-lead at story creation (QL-STORY-READY: ADEQUATE).*

- **AC1**: EntryLegality (verbatim AC-CON-06)
  - Given: the table fixture, every row run at occupancy 0, 1 and 4.
  - When: `CanBegin(HideEnter)` is called.
  - Then: reach 59 and 60 cm Allow, 61 OutOfReach; 30 cm from a closed side OutOfReach; running, traversing, informant and RoundEnded NotNow; held slot Busy; 5.9 s after an exit Cooldown, 6.0 and 6.1 s Allow; 5.9 s after an emergence Cooldown.
  - Edge cases: the cooldown runs from completion (End) of the exit or emergence; guard precedence is the IIF legality matrix's.
- **AC2**: SamePathForNpcs
  - Given: the same rows for an AI-driven pawn.
  - When: replies are compared on the same tick.
  - Then: byte-identical to the human case and across occupancy 0, 1 and 4; the signature has no IsAI, controller, team or occupancy input; both kinds enter through the one IIF Hide entry point.
  - Edge cases: Crowd bush-selection invariance is Crowd story 022.

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `EntryLegalityTest.cpp` and `SamePathForNpcsTest.cpp` under `Source/StealthGameplay/Private/Tests/Concealment/` — must exist and pass

**Status**: [x] Created and passing (2026-10-29): StealthGame.Concealment.EntryLegality, StealthGame.Concealment.SamePathForNpcs; evidence production/qa/evidence/concealment-001-026-2026-10-29/

Run result: N/A - headless core rule (no player-observable surface of its own); the integrated behaviour is observed in the batch's 3-player PIE run (con-*.png)

---

## Dependencies

- Depends on: `story-001-concealment-tuning-keys-and-timing-validator.md`, `story-002-bush-runtime-from-manifest.md`, `story-005-hide-exit-execution-and-blocked-side.md` (cooldown source); IIF `story-003-action-hub-and-predicate-registry.md` and `story-033-legality-matrix.md` under `production/epics/input-interaction-framework/`; SCL `production/epics/shared-character-locomotion/story-013-isrunning-hysteresis.md`
- Unlocks: `story-011-server-warp-to-anchor-and-exit-motion.md` (PIE use)

## Completion Notes
**Completed**: 2026-10-29
**Criteria**: AC1-AC2 passing (every row at occupancy 0, 1 and 4; identical replies for unbound AI pawns).
**Deviations**: Interact refusal inside a bush is a STAND-IN predicate registered by the composition root (StealthBushGatedPredicate: Interact NotNow while in a bush or transition) until the IIF Interact owner reads Concealment directly.
**Test Evidence**: production/qa/evidence/concealment-001-026-2026-10-29/
**Reviewed by**: self code review 2026-10-29 (APPROVED; notes in the evidence)
