# Story 004: Wire structs `FActionState` (208 bits) and `FDeathState` (65 bits), fixed width for every value

> **Epic**: Assassination & Defence Reversal
> **Status**: Ready
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/assassination-defence-reversal.md` (ASN-R21; AC-ASN-20 width clause)
**Requirement**: `TR-asn-003`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0018: Encounter data and outcome presentation timing (§1.1, §1.2, Key Interfaces)
**ADR Decision Summary**: `FActionState {Pose, Seq, Variant, StartServerTime, ContactServerTime, WarpPoint}` serialises to exactly 208 bits and `FDeathState {DeathServerTime, bDiedConcealed}` to exactly 65 bits for every value, through a custom `NetSerialize` and `Identical`, with no optional field, no conditional bit and no magnitude-packed vector, because a size is a side channel that would encode the outcome.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (networking; ADR-0018 Knowledge Risk)
**Engine Notes**: `TStructOpsTypeTraits::WithNetSerializer`/`WithIdentical` are pre-cutoff but their semantics matter: `NetSerialize`'s return value means "mapped" and must be `true`; `Identical` must compare every field including `Seq` (legacy change detection calls it through `Property->Identical`); `FArchive::SerializeInt` writes 32 bits on the base archive and is capped only by the net bit-writer, so the serialiser uses `SerializeBits` and the width is tested on an `FNetBitWriter` (ADR-0018 E1, E2, [V] read in the 5.8.4 install). Verification Required (1), the byte capture on a live bunch, is the Visibility Policy epic's spike (IVP story 008) and is the on-the-wire counterpart of this story's in-memory test.

**Control Manifest Rules (this layer)**:
- Required: `FActionState` serialises to exactly 208 bits (incl. `StartServerTime`) and `FDeathState` to exactly 65 bits (incl. `bDiedConcealed`) for every value; `FStrikeResult` carries `ContactServerTime` for every `Result` — source: ADR-0018
- Required: `FActionState.Seq` advances by one on every write of that pawn and is assigned by `WriteActionState` — source: ADR-0018
- Forbidden: Never use an optional, conditional or magnitude-packed field in `FActionState` or `FDeathState`; never let `Seq` carry, derive from or correlate with a `ClientCommandSeq`; no pawn reference on the struct — source: ADR-0018
- Guardrail: 26 + 9 bytes of replicated state per pawn

This story declares the wire types in Foundation `Public/View` (so IIF, Camera, HUD and Assassination all see one declaration, LP concern 2), with the fixed-width serialisers. `FFixedWarpOffset` is three `int16` in 0.1 cm units on world axes, the offset of the strike's warp target from the attacker's server capsule location. Replicated defaults are sentinels (`Seq` starts at 1, `DeathServerTime = −1`) so a late-opened actor that receives both in one bunch presents correctly (story 018). The reply structs `FStrikeResult` and `FDefenceResult` are declared here too but built by story 015.

---

## Acceptance Criteria

*From GDD `design/gdd/assassination-defence-reversal.md` (AC-ASN-20 width and shape clause) and ADR-0018 Validation Criteria `WireWidth`, scoped to this story:*

- [ ] **AC1** (AC-ASN-20 width clause, [Logic/BLOCKING] `StealthGame.Assassination.WireWidth`): **GIVEN** a fixed sweep of `FActionState` values (zero, maximum `WarpPoint` offset, minimum and maximum time bit patterns, 1000 seeded random values) and of `FDeathState` values **WHEN** each is serialised on an `FNetBitWriter` **THEN** every `FActionState` is exactly 208 bits and every `FDeathState` exactly 65 bits, `NetSerialize` returns `true`, and a round trip reproduces all six (resp. both) fields bit for bit.
- [ ] **AC2** (AC-ASN-20 shape clause + ADR `Identical`, [Logic/BLOCKING]): **GIVEN** `FActionState` and `FDeathState` **WHEN** a reflection check enumerates their properties and `Identical` is called on pairs that differ in exactly one field **THEN** `FActionState` has no property of pawn, actor, object or `FName` type, and `Identical` returns `false` for a difference in each of the six fields (including `Seq`) and `true` only for field-equal values.
- [ ] **AC3** (AC-ASN-20 negative control, ADR-0008 §3, [Logic/BLOCKING]): **GIVEN** a test-seam serialiser (`WITH_STEALTH_TEST_SEAMS`) that packs `WarpPoint` by magnitude **WHEN** the AC1 sweep runs against it **THEN** the width assertion fails; the production serialiser is restored afterwards and AC1 passes.

---

## Implementation Notes

*Derived from ADR-0018 §1 and Key Interfaces:*

Field layout: `Pose` (`EActionPose`, uint8, 8 bits; `None, Strike, Brace, Counter, Interact, BushEnter, BushExit`, append-only), `Seq` (uint16, 16 bits; default 1; wraps 65535 to 1), `Variant` (uint8, 8 bits; `Strike`: bits 0-1 entry variant 0 `Strike_F` / 1 `Strike_L60` / 2 `Strike_R60`, bit 2 `BushPreface`; every other pose 0), `StartServerTime` (double, 64), `ContactServerTime` (double, 64; −1 for non-strike, non-counter poses), `WarpPoint` (`FFixedWarpOffset`, 3 × int16 = 48). Total 208. `FDeathState`: `double DeathServerTime = −1.0` (64) and `bool bDiedConcealed` (1) = 65. `NetSerialize` writes with `SerializeBits` and always returns `true`; `WithNetSharedSerialization` may be set (no object references). `FFixedWarpOffset`'s members are `UPROPERTY`s for legacy change detection (ADR-0003 §4). The `OnRep_ActionState` / `OnRep_DeathState` property declarations on `AStealthPawn` and their allowlist rows belong to story 012; this story ships the types and tests only.

The `EStrikeResult { RivalEliminated, WrongTarget, NoContact, Reversed }`, `EDefenceResult { Reversed }`, `StrikeVariant` constants, `FStrikeResult` and `FDefenceResult` are declared in the same header, per ADR-0018 Key Interfaces; `FStrikeResult` has one shape (`ClientCommandSeq`, `Result`, `TallyAfter`, `bExiled`, `ContactServerTime`).

**Prototype replaced:** the prototype's separate replicated pawn bools `bDying` and `bPreparingDefence` (and `OnRep_PreparingDefence`) in `StealthCharacter.h` carry the outcome as individual properties; they are replaced by these two structs and removed by story 027.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-012-slot-write-outcome-blind-and-warp-point.md`: the replicated properties on the pawn, the writer, `WarpPoint` derivation, forced updates.
- `story-013-death-write-staging-and-after-latch.md`: when `FDeathState` is written.
- `story-015-owner-results-reply-entries-and-tally.md`: building and sending `FStrikeResult` and `FDefenceResult`.
- IVP story 008: the byte capture on a live legacy-driver bunch (Verification Required (1)).

---

## Files

- `Source/StealthFoundation/Public/View/StealthEncounterWire.h` (`EActionPose`, `FFixedWarpOffset`, `FActionState`, `FDeathState`, result enums and structs, `TStructOpsTypeTraits`)
- `Source/StealthFoundation/Private/Net/StealthEncounterWire.cpp` (`NetSerialize`, `Identical`)
- `Source/StealthGameplay/Private/Tests/Assassination/WireWidthTest.cpp` (class `FAssassinationWireWidthTest`, name `StealthGame.Assassination.WireWidth`)
- `Source/StealthGameplay/Private/Tests/Assassination/WireShapeTest.cpp` (class `FAssassinationWireShapeTest`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

QL-STORY-READY: GAPS
- AC1: the 1000 seeded random values have no seed, and NaN and denormal doubles need a bitwise comparison; proposed rewrite (unconfirmed): values from a fixed constant seed `WireSweepSeed`; time fields include 0, -1, `DBL_MIN`, `DBL_MAX` and NaN bit patterns; `WarpPoint` includes (0,0,0), (32767,32767,32767) and (-32768,-32768,-32768); round trip compared by bit pattern.
- AC2: `Identical` is asserted only for `FActionState`; `FDeathState` (both fields) and its reflection check are missing; proposed rewrite (unconfirmed): `Identical` is `false` for a difference in `DeathServerTime` and in `bDiedConcealed`, `true` for field-equal values; neither struct has an actor, object, `FName` or pawn-typed property.
- AC3: "the width assertion fails" is not measurable; proposed rewrite (unconfirmed): using the negative-control helper the sweep reports at least one `FActionState` whose bit count differs from 208, the control is recorded as caught, and AC1 passes after the production serialiser is restored.

- **AC-1**: Fixed width and round trip
  - Given: the fixed sweep for both structs
  - When: each is written to an `FNetBitWriter` and read back
  - Then: `FActionState` is 208 bits and `FDeathState` 65 bits for every value; `NetSerialize` returns `true`; all fields round-trip bit for bit
  - Edge cases: NaN and negative-zero doubles; int16 extremes; `Seq` 65535 and 1; default sentinels (`Seq` 1, `DeathServerTime` -1) also 208 and 65
- **AC-2**: Shape and `Identical`
  - Given: both structs and pairs differing in exactly one field
  - When: reflection enumerates properties and `Identical` is called on each pair
  - Then: no pawn, actor, object or `FName` property; `Identical` is `false` for each of the six `FActionState` fields (including `Seq`) and both `FDeathState` fields, `true` for equal pairs
  - Edge cases: a `Seq`-only and a `bDiedConcealed`-only difference must register
- **AC-3**: Negative control detects magnitude packing
  - Given: the `WITH_STEALTH_TEST_SEAMS` serialiser that packs `WarpPoint` by magnitude
  - When: the AC1 sweep runs against it, then production is restored
  - Then: the width assertion fails for at least one value (control caught); with production restored AC1 passes
  - Edge cases: the seam must not leak into Shipping; the control fails loudly if not exercised

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `WireWidthTest.cpp` and `WireShapeTest.cpp` — must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: cross-epic `production/epics/tuning-data-config/story-001-foundation-module-skeleton.md`, `story-008-fault-sink-and-test-seams-define.md`, `story-010-negative-control-helper.md`; `production/epics/input-interaction-framework/story-001-command-reply-types.md` (`FClientCommandSeq`, `EActionPose` list ownership)
- Unlocks: `story-012-slot-write-outcome-blind-and-warp-point.md`, `story-013-death-write-staging-and-after-latch.md`, `story-015-owner-results-reply-entries-and-tally.md`
