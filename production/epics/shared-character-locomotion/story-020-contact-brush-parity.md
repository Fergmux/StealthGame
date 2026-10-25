# Story 020: Contact brush reaction is the same for every pair of pawns

> **Epic**: Shared Character & Locomotion
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: [set by /dev-story when implementation begins]

## Context

**GDD**: `design/gdd/shared-character-locomotion.md` (R5.8 Contact reaction, R10 shared contact reaction; AC-SCL-47)
**Requirement**: `TR-scl-006`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0013: Animation: motion matching and significance budget (section 1; the shared contact reaction is part of the one animation asset set)
**ADR Decision Summary**: One AnimBP, one database set, one chooser set; the contact reaction is presentation driven by replicated movement on each client with no RPC and no replicated field, played identically for NPC-NPC, NPC-player and player-player contacts at relative speed at or above `ContactBrushMinRelSpeedCmPerSec` (40).
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: The reaction is an additive upper-body montage or layer with no displacement and no gait change. Pawn-pawn capsules block and physics push is off (R16), so contact detection comes from capsule overlap/hit events computed locally on each client from replicated positions.

**Control Manifest Rules (this layer)**:
- Required: presentation is state-derived and idempotent; no multicast RPC exists on the pawn.
- Forbidden: never add a gameplay RPC on the pawn; never select an animation by controller, team or `AppearanceId`; gameplay rules never read anim notifies.
- Guardrail: stays inside the 4.0 ms animation line.

---

## Acceptance Criteria

*From GDD `design/gdd/shared-character-locomotion.md`, scoped to this story:*

- [ ] **AC-SCL-47** [P] `ContactBrushParity` (R5.8): GIVEN NPC-NPC, NPC-player and player-player pairs of pawns walking into capsule contact at a relative speed of 60 cm/s (≥ `ContactBrushMinRelSpeedCmPerSec` 40), and a control pair at 30 cm/s, WHEN each pair touches, THEN both pawns of every ≥ 40 cm/s pair play the same additive reaction (same asset, same start within 1 frame of contact on each client), neither moves more than 1 cm from the contact, no gait or `IsRunning` change occurs, no RPC or replicated field is added, and the 30 cm/s pair plays none.

---

## Implementation Notes

*Derived from GDD R5.8 and ADR-0013 section 1:*

- Detect contact locally on every client from the pawns' replicated movement (capsule contact at relative speed at or above the key) and trigger the additive reaction from `UStealthAnimInstance`; both pawns of the pair play the same reaction asset. The reaction is part of the pruned animation set (story 041), authored as upper-body additive only.
- Assert "no RPC or replicated field is added" by comparing the replicated-property set and RPC list before and after (the story-032 allowlist); assert "no gait or `IsRunning` change" from the published state.
- The animation asset is a Content step (never-resave hazards apply).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 032: allowlist enforcement.
- Story 041: pruning the database set.
- Crowd avoidance and yielding (Crowd epic).

---

## Files

- `Source/StealthGameplay/Private/Core/Animation/StealthAnimInstance.cpp` (contact reaction trigger)
- `Source/StealthGameplay/Private/Tests/Locomotion/ContactBrushParityTest.cpp` (planned)
- Content: shared contact reaction asset (by explicit path via MCP)

---

## QA Test Cases

*QL-STORY-READY: ADEQUATE. Test `StealthGame.Locomotion.ContactBrushParity`.*

Test: contact brush parity (AC-SCL-47)
  Given: NPC-NPC, NPC-player and player-player pairs walking into capsule contact at a relative speed of 60 cm/s (≥ `ContactBrushMinRelSpeedCmPerSec` 40), plus a 30 cm/s control pair
  When: each pair touches
  Then: both pawns of every ≥ 40 cm/s pair play the same additive reaction (same asset, start within 1 frame of contact on each client); neither moves more than 1 cm from the contact; no gait or `IsRunning` change occurs; no RPC or replicated field is added; the 30 cm/s pair plays none
  Edge cases: no emulation profile is assumed; the reaction asset must exist in the pruned database set (story 041)

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthGameplay/Private/Tests/Locomotion/ContactBrushParityTest.cpp`, test name `StealthGame.Locomotion.ContactBrushParity` — must exist and pass; retained screenshot of the brush reaction in `production/qa/evidence/` (player-observable animation)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/shared-character-locomotion/story-005-animbp-islocallycontrolled-removal.md`, `story-010-speed-tiers.md`
- Unlocks: None
