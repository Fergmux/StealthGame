# Story 012: The slot writes one outcome-blind `FActionState`, with a `WarpPoint` that never reads a hidden pawn

> **Epic**: Assassination & Defence Reversal
> **Status**: Ready
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/assassination-defence-reversal.md` (ASN-R16, ASN-R21; AC-ASN-20 slot clause, AC-ASN-47)
**Requirement**: `TR-asn-003`, `TR-asn-004`, `TR-asn-009`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0018: Encounter data and outcome presentation timing (§1.1, §2.1 step 2, §2.2, §7)
**ADR Decision Summary**: At the Attack's slot only the attacker's `FActionState` (and its movement and root-motion state) changes on the wire, identical in shape for a kill, a whiff and a reversal; `WarpPoint` is derived only from an honoured `PawnRef`'s extrapolation or the submitted `WorldPoint` (clamped to the footprint edge for a bush), never from a culled or concealed pawn's position; a forced net update is issued at the slot for every outcome.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (networking; forced updates and serialisers are 5.5+ sensitive)
**Engine Notes**: `OnRep_ActionState` is a single RepNotify on `AStealthPawn`; the listen host does not receive RepNotify for its own server writes (ADR-0018 E7) so the host path calls the same handler directly (story 018). Forced updates go through `FStealthForcedNetUpdate(ActionStateWrite)`, at most once per pawn per frame (ADR-0003); never `ForceNetUpdate()` directly. Capture and byte-level checks use the ADR-0008 harness (`LeakNetProfile`, fixed zero-variance delay). Verification Required (1) byte capture is IVP story 008.

**Control Manifest Rules (this layer)**:
- Required: Only `UPawnCommandComponent::WriteActionState` writes `FActionState`, assigns `Seq` itself and fills `StartServerTime` in every write — source: ADR-0011, ADR-0018
- Required: At the Attack's slot only the attacker's `FActionState` and movement/root-motion state change on the wire — source: ADR-0018
- Forbidden: Never read a culled or concealed pawn's position for `WarpPoint`; no optional or magnitude-packed field; no replicated effect actor, no multicast — source: ADR-0018, ADR-0003
- Guardrail: One forced update per strike; the attacker's property block has the same bit length for every outcome

`FActionState` for the strike: `Pose = Strike`, `Variant` (entry variant from the bearing, plus the `BushPreface` bit for a bush strike), `StartServerTime` = the slot time, `ContactServerTime` = slot time + `StrikeContactSec` (+ `BushStrikeEmergeSec` for `BushPreface`), `WarpPoint` = the quantised offset of the warp target from the attacker's server capsule location. The server's own lunge warps to the same `WarpPoint` for every outcome and never re-tracks the victim (story 014). Accepted tell (P2): after a blind open-spot hit the body may fall up to `r_c + OpenSpotToleranceCm` from the attacker's hands.

---

## Acceptance Criteria

*From GDD `design/gdd/assassination-defence-reversal.md` (AC-ASN-20 slot-window clause, AC-ASN-47), scoped to this story:*

- [ ] **AC1** (AC-ASN-20 slot window, [Integration/BLOCKING][Net] `StealthGame.Assassination.OutcomeBlindWire`): **GIVEN** a kill, a whiff and a reversal with the same variant, `WarpPoint` and attacker path, all other pawns stationary, under no loss and a fixed zero-variance packet delay **WHEN** the server send capture to a third-team bystander connection is logged per tick from the slot up to but excluding the tick that executes the death write **THEN** the three runs have the same set of dirty replicated properties (the attacker's `FActionState`, `ReplicatedMovement` and `RepRootMotion`, nothing on any other pawn), the attacker's property block has the same bit length in all three runs, the attacker's `FActionState` is sent in the slot tick (forced update) in all three runs, its values are identical across the runs (same variant, `StartServerTime`, `ContactServerTime`, `WarpPoint`, so no field encodes the outcome), and no actor is spawned on the server in the window (spawn count 0).
- [ ] **AC2** (AC-ASN-20 negative control, ADR-0008 §3, [Integration/BLOCKING][Net]): **GIVEN** a seam build that writes the outcome into `Variant` **WHEN** AC1 runs against it **THEN** AC1 fails; the production build passes.
- [ ] **AC3** (AC-ASN-47, [Integration/BLOCKING][Net] `StealthGame.Assassination.WarpPointBlind`): **GIVEN** `BushStrike`s at the same `WorldPoint` into an empty bush, an ally-only bush, a bush with a concealed NPC and one with a concealed rival, and open `TargetSpot` strikes at the same point with and without a culled pawn there **THEN** within each group `WarpPoint` is identical (derived from the `WorldPoint` and the footprint only) and never equals an occupant's position; a strike with an honoured `PawnRef` uses the victim's extrapolated position.

---

## Implementation Notes

*Derived from ADR-0018 §2.2 (`WarpPoint` derivation) and §7.1:*

Let `P_A` be the attacker's server capsule location at the slot. **Honoured `PawnRef`** (hit or reach-failed whiff): target = F1's extrapolated victim capsule centre `P_V(T_v)` (story 002). **Open spot** (hit or whiff): target = the submitted `WorldPoint`. **Bush spot** (hit or whiff): target = the `WorldPoint` clamped to the bush footprint edge on the attacker's side (footprint from ADR-0017's Manifest-derived `FBushRuntime`, identical on server and clients). **Clamp, all cases:** horizontal `|target − P_A|_xy ≤ 2·r_c + StrikeContactGapCm + StrikeWarpLimitCm` and `|Δz| ≤ StrikeVerticalReachCm`; the bearing is not clamped here (the warp yaw is capped by the animation at `StrikeWarpYawLimitDeg`). Quantise `target − P_A` to 0.1 cm per axis, round half away from zero. No culled or concealed pawn's position ever enters the value.

Variant choice (ASN-R16): `Strike_F` for |θ| < 30°, otherwise `Strike_L60` or `Strike_R60` by the sign of θ (θ = ±30° exactly takes the side variant), where θ is the bearing of the victim (or the `WorldPoint` for a whiff) relative to body facing; the choice never depends on outcome. `Seq` is assigned by `WriteActionState`, never by the caller. Replicate `FActionState` as `ReplicatedUsing=OnRep_ActionState`, `All` audience, with an allowlist row and a Visibility Policy row (default-deny wire). The forced update is the single `ActionStateWrite` reason of ADR-0003 §2.

The attacker's only other wire changes in the window are replicated movement and `RepRootMotion`, because the strike clip is a root-motion montage whose asset differs only by `Variant` (already public). Holds of the victim are cancelled in the slot as server state (IIF cancel reason `PawnRemoved`) but the IIF must not write the victim's `FActionState` for that cancel (the death write supersedes it, story 013); AC1's "nothing on any other pawn" assertion catches a violation.

**Prototype replaced:** `Server*` RPCs on the pawn and the separate replicated bools (`ServerAssassinate`, `bDying`) are replaced by one command entry and one struct; the prototype's immediate `ApplyDeathPresentation` at the slot is the exact behaviour this story's blindness forbids.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-004-wire-structs-fixed-width.md`: the struct types and serialisers.
- `story-013-death-write-staging-and-after-latch.md`: the death write and AC-ASN-20's death-write clause.
- `story-014-warp-targets-not-replicated-lunge-outcome-blind.md`: the server lunge and motion-warping replication.
- `story-018-strike-clip-variants-warp-and-contact-scheduling.md`: client-side clip, warp and scheduling.

---

## Files

- `Source/StealthGameplay/Public/View/Core/StealthPawn.h` (`FActionState ActionState` with `OnRep_ActionState`; only if the pawn skeleton story has not added it)
- `Source/StealthGameplay/Private/Features/Assassination/StrikeWarpPointCore.h` and `.cpp` (`FStrikeWarpPointCore`: derivation, clamp, quantisation)
- `Source/StealthGameplay/Private/Features/Assassination/EncounterService.cpp` (slot step 2: `WriteActionState` call)
- `Source/StealthTestHarness/Private/Assassination/OutcomeBlindWireTest.cpp` (`StealthGame.Assassination.PIE.OutcomeBlindWire`)
- `Source/StealthTestHarness/Private/Assassination/WarpPointBlindTest.cpp` (`StealthGame.Assassination.PIE.WarpPointBlind`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

QL-STORY-READY: GAPS
- AC3: "never equals an occupant's position" has no minimum separation so it can pass vacuously, and the extrapolated-position comparison has no tolerance; proposed rewrite (unconfirmed): within each group `WarpPoint` is byte-identical; each occupant is placed ≥ 50 cm from the `WorldPoint` so equality is distinguishable, and `WarpPoint` differs from every occupant offset by more than 0.1 cm (the quantum); an honoured `PawnRef` gives `WarpPoint` = quantised(F1 `P_V(T_v)` − `P_A`) exactly.
- AC1: adequate; state that the three runs script the same slot server time, otherwise `StartServerTime` differs by construction.
- AC2: adequate; a retained failing log is required (story evidence covers it).

- **AC-1**: Outcome-blind wire
  - Given: a kill, a whiff and a reversal with the same variant, `WarpPoint` and attacker path, others stationary, no loss, fixed zero-variance delay, and a bystander connection capture
  - When: the capture is logged per tick from the slot up to but excluding the death-write tick
  - Then: dirty-property sets are identical (attacker `FActionState`, `ReplicatedMovement`, `RepRootMotion`, nothing else), the attacker's bit length is equal, `FActionState` is sent in the slot tick, values are identical, 0 actors spawn
  - Edge cases: victim hold cancelled with no IIF `FActionState` write; the host path
- **AC-2**: Negative control
  - Given: a `WITH_STEALTH_TEST_SEAMS` build that writes the outcome into `Variant`
  - When: AC1 runs on the seam build and on the production build
  - Then: AC1 fails on the seam build and passes on production
  - Edge cases: the seam must be excluded from Shipping
- **AC-3**: `WarpPoint` blind
  - Given: `BushStrike`s at one `WorldPoint` into an empty, ally-only, concealed-NPC and concealed-rival bush, plus open-spot strikes with and without a culled pawn, and one honoured-`PawnRef` strike
  - When: each strike's slot write is captured
  - Then: `WarpPoint` is identical within each group, never equals an occupant's offset, and the honoured-`PawnRef` case uses the extrapolated victim position
  - Edge cases: `WorldPoint` outside the footprint (clamped to the attacker-side edge); clamp at `|Δz| = StrikeVerticalReachCm`; θ = ±30° exactly takes the side variant

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: the two harness tests above under `LeakNetProfile` with a fixed zero-variance delay — must exist and pass; the send capture log is retained with the run

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `story-002-reach-f1-pure-function.md`, `story-004-wire-structs-fixed-width.md`, `story-006-bush-spot-rule-and-victim-pick.md`, `story-007-attack-outcomes-resolved-in-slot.md`; cross-epic `production/epics/input-interaction-framework/story-069-force-net-update-on-write.md`, `story-082-action-pose-parity-observed.md`; `production/epics/information-visibility-policy/story-008-spike-fixed-width-netserialize-byte-capture.md`, `story-015-replicated-property-audit.md`; `production/epics/tuning-data-config/story-017-net-emulation-profiles.md`
- Unlocks: `story-013-death-write-staging-and-after-latch.md`, `story-014-warp-targets-not-replicated-lunge-outcome-blind.md`, `story-017-no-warning-and-no-observer-cue.md`, `story-018-strike-clip-variants-warp-and-contact-scheduling.md`
