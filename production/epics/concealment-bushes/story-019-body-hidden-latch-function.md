# Story 019: EvaluateBodyHidden is a pure, fail-closed, state-derived function

> **Epic**: Concealment (Bushes)
> **Status**: Ready
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/concealment-bushes.md` (CON-R6b, CON-R6a, AC-CON-08 last sentence as a unit case)
**Requirement**: `TR-con-006`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0017: Concealment component and per-viewer presentation (§5 the client latch)
**ADR Decision Summary**: `EvaluateBodyHidden(FActionState, FDeathState, bAllyIsConcealed, bIsOwner, Now, FStealthClientTuning)` is the one pure function behind the client latch: owner always false; set while a BushEnter pose is at or past its swap, while the ally flag is true, while a BushExit or bush-emerge pose is before its crossing, or when `bDiedConcealed`; clear only when no set rule holds and the last evidence is a passed crossing or there is none. A pose change to None, a death pose or a strike pose never clears it.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0017 overall; this function is plain C++ with no UObject dependency)
**Engine Notes**: Reads `FActionState.StartServerTime` (approved 2026-10-25; `FActionState` is exactly 208 bits) and `FDeathState.bDiedConcealed` (exactly 65 bits), never `ContactServerTime`. `Now` is `EstimatedServerNow()` (ADR-0002 §9); anim notifies must never gate the hide (E6).

**Control Manifest Rules (this layer)**:
- Required: Client presentation hides and shows bodies only through `EvaluateBodyHidden` from `EstimatedServerNow()` — source: ADR-0017
- Forbidden: Never read a hide-pose start from `ContactServerTime`; read `FActionState.StartServerTime` — source: ADR-0017, ADR-0018
- Guardrail: Pure function, no allocation, no UObject access

Fail-closed means any unknown or ambiguous evidence leaves the body hidden; the function is the single source of truth every client (and the listen host) shares.

---

## Acceptance Criteria

*From GDD `design/gdd/concealment-bushes.md` (CON-R6b, AC-CON-08), scoped to this story:*

- [ ] **AC1** (CON-R6b set/clear, [U] `StealthGame.Concealment.BodyHiddenLatchTable`): **GIVEN** a table of (`FActionState`, `FDeathState`, `bAllyIsConcealed`, `bIsOwner`, Now) **THEN** an owner is always false; the set rules are: BushEnter with Now ≥ Start + `BushEnterSwapSec`, `bAllyIsConcealed` true, BushExit with Now < Start + `BushExitCrossSec`, bush-emerge strike with Now < Start + `BushStrikeEmergeSec`, `bDiedConcealed`; the latch clears only when no set rule holds and the last evidence is a passed crossing or there is none (pose None and ally flag false); a pose change to None, a death pose or a strike pose never clears it.
- [ ] **AC2** (CON-R6b rule 4 precedence): **GIVEN** BushEnter before its swap **WHEN** IsConcealed is false, then true before the pose sets, then death occurs before or after the commit **THEN** it waits for the swap, sets at once on IsConcealed true, a death before the commit cancels the pending swap (ordinary visible kill), and a death after the commit never clears.
- [ ] **AC3** (AC-CON-08 last sentence, unit form): **GIVEN** an ally client watching the whole enter, with the pose going BushEnter → None at +0.9 s **WHEN** Now is swept at frame resolution (1/60 s) **THEN** the function returns hidden at every Now from the swap onward, so no body frame appears.

---

## Implementation Notes

*Derived from ADR-0017 Decision §5 and Key Interfaces:*

```
bool EvaluateBodyHidden(const FActionState&, const FDeathState&, bool bAllyIsConcealed,
                        bool bIsOwner, double Now, const FStealthClientTuning&);
```

The tuning comes through the ADR-0004 client face (`FConcealmentTuning_Public` and `FAssassinationTuning_Public` for `BushStrikeEmergeSec` and `StrikeContactSec`): no Concealment-specific client tuning type. Boundary rows must be exact: at Now = Start + swap the body is hidden; at Now = Start + crossing it is shown. The function has no UObject dependency so it runs headless and on the listen host with `Now == ServerNow()` (CON-R16). The fresh-actor handling (hidden at construction, evaluated in `PostNetInit`, the unresolved-ally-evidence rule) is story 020.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-020-fresh-actor-latch-and-ally-hidden-read.md`: fresh actor, `PostNetInit`, `IsAllyBodyHidden`.
- `story-022-presentation-hide-application-and-host-parity.md`: what hiding applies to the mesh and the host path.

---

## Files

- `Source/StealthGameplay/Features/Concealment/Public/View/BodyHiddenLatch.h`
- `Source/StealthGameplay/Features/Concealment/Private/BodyHiddenLatch.cpp`
- `Source/StealthGameplay/Private/Tests/Concealment/BodyHiddenLatchTest.cpp` (class `FConcealmentBodyHiddenLatchTest`, name `StealthGame.Concealment.BodyHiddenLatchTable`)

---

## QA Test Cases

*Written by qa-lead at story creation (QL-STORY-READY: ADEQUATE).*

- **AC1**: BodyHiddenLatchTable
  - Given: a table of (FActionState, FDeathState, bAllyIsConcealed, bIsOwner, Now).
  - When: `EvaluateBodyHidden` is called per row.
  - Then: an owner is always false; the set rules hold as listed; a pose change to None, a death pose or a strike pose never clears the latch.
  - Edge cases: boundary rows at exactly Start + Swap (hidden) and Start + Cross (shown).
- **AC2**: Rule4Precedence
  - Given: BushEnter before its swap.
  - When: IsConcealed is false, then true before the pose sets, then death occurs before or after commit.
  - Then: it waits for the swap, hides at once on IsConcealed true, a death before commit cancels the pending swap, a death after commit never clears.
- **AC3**: AllyNoBodyFrame
  - Given: an ally watching the whole enter, with the pose going BushEnter to None at +0.9 s.
  - When: Now is swept at frame resolution (1/60 s).
  - Then: hidden at every Now from the swap onward, so no body frame appears.

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `BodyHiddenLatchTest.cpp` under `Source/StealthGameplay/Private/Tests/Concealment/` — must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `story-001-concealment-tuning-keys-and-timing-validator.md` (client tuning face); the Foundation `FActionState`/`FDeathState` struct definitions (ADR-0018; assassination-defence-reversal epic stories not yet written, so use the Foundation definitions)
- Unlocks: `story-020-fresh-actor-latch-and-ally-hidden-read.md`, `story-022-presentation-hide-application-and-host-parity.md`
