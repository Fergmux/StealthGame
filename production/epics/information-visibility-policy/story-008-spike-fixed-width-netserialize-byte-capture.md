# Story 008: Spike, fixed-width NetSerialize structs are sent with identical bit sizes on the legacy driver

> **Epic**: Information Visibility Policy
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

Outcome-bearing wire structs (`FActionState` for a kill, a whiff and a reversal; `FDeathState`) must serialise to the same number of bits for every value, otherwise their size is a side channel that reveals the encounter outcome. ADR-0003 section 4 specifies a fixed-width `NetSerialize` (`WithNetSerializer` plus `WithIdentical`), but engine fact E12 ("a struct with a custom `NetSerialize` is sent as a whole when any field changes, with exactly the bits it writes, on the legacy driver") is unverified. This spike captures the bunches and confirms or refutes it, using the same capture mechanism story-001 selects.

**GDD**: `design/gdd/information-visibility-policy.md`
**Requirement**: `TR-ivp-003` (UE default overrides of R5: pawn links, Instigator, possession ForceNetUpdate, RemoteViewPitch16, spectator)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0003: Replication, relevancy and the concealment cull hybrid (section 4, E12)
**ADR Decision Summary**: `FActionState {Pose, Seq, Variant, StartServerTime, ContactServerTime, WarpPoint}` and `FDeathState {DeathServerTime, bDiedConcealed}` implement `NetSerialize` with `WithNetSerializer` and write the same number of bits for every value: no magnitude-packed vector, no optional field, no conditional bit; the struct is declared once in ADR-0018's Key Interfaces.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: E12 [U, byte-capture spike]; legacy change detection compares UPROPERTYs, so `FFixedWarpOffset` needs UPROPERTY members or `WithIdentical`. Iris serialisers are ADR-0024's. This spike owns the fixed-width byte capture.

**Control Manifest Rules (this layer)**:
- Required: **Outcome-bearing replicated structs serialise to a fixed bit width** — source: ADR-0003
- Forbidden: **Never skip `FActionState.StartServerTime` in an `FActionState` write** — source: ADR-0003, ADR-0017, ADR-0018
- Required: **Every leak, parity, statistical and lint gate includes a negative control the suite detects** — source: ADR-0008

---

## Acceptance Criteria

*From the story's spike definition (ADR-0003 Verification Required, byte capture; E12):*

- [ ] GIVEN a throwaway USTRUCT with the ADR-0003 section 4 layout (Pose 8, Seq 16, Variant 8, StartServerTime 64, ContactServerTime 64, WarpPoint three fixed-width offsets; and FDeathState {double, bDiedConcealed}) using WithNetSerializer and WithIdentical on the legacy driver, WHEN the bunches for a kill, a whiff and a reversal (and the three FDeathState variants) are captured on a non-owner client, THEN the bit sizes are identical for every value and E12 ("sent as a whole with exactly the bits it writes") is recorded as verified or refuted in production/qa/spikes/.

---

## Implementation Notes

*Derived from ADR-0003 section 4 and Validation Criteria (byte capture):*

- Use a throwaway struct with the section 4 layout (the production `FActionState` is declared by ADR-0018, Assassination epic, in `StealthFoundation/Public/View/`); the point of the spike is the engine behaviour, not the final type.
- Capture mechanism: the one chosen in story-001 (including its negative control); run every enumerated value set (kill, whiff, reversal, and the three `FDeathState` variants) and compare bit sizes of the received bunches.
- A refuted E12 is still a valid spike outcome: record how the legacy driver actually sends the struct and file the follow-up for ADR-0003/ADR-0018 (a superseding ADR, not an edit).
- Prototype: none relevant (`bDying`/`bPreparingDefence` bools are replaced by `FActionState` in ADR-0018).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- story-001: the capture mechanism
- Assassination epic: the production `FActionState`/`FDeathState` and their serialisers (ADR-0018)
- story-044: the production raw-bunch schema parity gate

---

## Files

- `production/qa/spikes/<date>_fixed-width-byte-capture.md` (the verdict)
- Throwaway struct and capture scenario (prototype or `Source/StealthTestHarness/Private/Scenarios/`)

---

## QA Test Cases

- **AC-1**: fixed-width NetSerialize byte capture (E12)
  - Given: a throwaway USTRUCT with the ADR-0003 section 4 layout (Pose 8, Seq 16, Variant 8, StartServerTime 64, ContactServerTime 64, WarpPoint) plus FDeathState, using WithNetSerializer and WithIdentical on the legacy driver.
  - When: bunches for a kill, a whiff, a reversal and the 3 FDeathState variants are captured on a non-owner.
  - Then: bit sizes are identical for every value; E12 is recorded as verified or refuted in production/qa/spikes/.
  - Edge cases: run all enumerated value sets; a refuted result is still a valid spike outcome. (Tier: T3.)

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: a written spike report `production/qa/spikes/<date>_fixed-width-byte-capture.md` with the captured bit sizes (a documented measurement).

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/information-visibility-policy/story-001-spike-s1-multiprocess-raw-bunch-capture.md`
- Unlocks: None in this epic; closes the "fixed-width byte capture" verification of ADR-0003 for the assassination-defence-reversal epic (ADR-0018 encounter data)
