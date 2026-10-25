# Story 008: Client ping-point rule

> **Epic**: Informant Pings & Team Communication
> **Status**: Ready
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/informant-pings-team-comms.md` (PNG-R3 steps 1-3, F2 client half, edge cases on bush, pawn, sky and third person; AC-PNG-15)
**Requirement**: `TR-png-005` (also `TR-png-006` client half and `TR-png-001` never names a pawn)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0015: Camera system and LOS envelope (§5 `GetAimRay()`), with ADR-0011 §9 client gate and ADR-0017 (bush foliage ignores `ViewBlock`)
**ADR Decision Summary**: `GetAimRay()` on the camera's client view facade is the single source of aim: first person uses the camera-centre ray from the lens; third person uses the centre ray from the point nearest the eye pivot to find an aim point, then a re-trace from the pivot to that point plus 10 cm. The ping uses only the world query on `ViewBlock` (pawns, bush foliage, glass and decoration ignored), then a downward trace of at most 300 cm to the first walkable surface; range is measured from the eye pivot on both machines.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0015 overall; the traces use stock `LineTraceSingleByChannel`, pre-cutoff)
**Engine Notes**: The `ViewBlock` channel constant is the single one from `StealthCollisionChannels.h` (camera story 007). Take the walkable-floor angle from the movement component's `GetWalkableFloorAngle()`/`GetWalkableFloorZ()` rather than a literal. Cost: at most 3 traces per press (aim, re-trace, drop). The function is pure over an injected trace interface so the cases run headless.

**Control Manifest Rules (this layer)**:
- Required: the ping point is the world query of `GetAimRay()` on `ViewBlock`, dropped at most 300 cm to the first walkable surface; with no hit or no walkable ground nothing is sent — source: ADR-0011, ADR-0015
- Required: range is measured from the sender pawn's eye pivot on both machines, never from the camera — source: ADR-0011, ADR-0015
- Forbidden: Never add lock-on, snapping, magnetism or aim assist; never use the pawn query or read a `PawnRef` for a ping — source: ADR-0011, ADR-0015

---

## Acceptance Criteria

*From GDD `design/gdd/informant-pings-team-comms.md` (AC-PNG-15, PNG-R3, F2), scoped to this story:*

- [ ] **AC1** (AC-PNG-15 a, b, `[Logic]` `StealthGame.Pings.PointRule`): **GIVEN** the ping-point function with an injected `ViewBlock` trace **WHEN** (a) a first-person aim ray passes through a pawn and meets a wall 4 m behind it at 150 cm height, (b) a ray meets a bush `HideVolume` and the ground behind it **THEN** (a) `P` is the ground at the wall base, the pawn is never hit and no pawn id is read, (b) `P` is the geometry beyond the bush on the ground, identical for an occupied and an empty bush.
- [ ] **AC2** (AC-PNG-15 c): **GIVEN** a ray meeting a facade at 450 cm height with no walkable surface within 300 cm below, or no hit (sky), or a point beyond `PingRangeCm` from the eye pivot **WHEN** the press is evaluated **THEN** nothing is sent; the downward trace starts 1 cm along the hit normal, runs at most 300 cm on `ViewBlock` and accepts the first surface within the walkable-floor angle.
- [ ] **AC3** (AC-PNG-15 d, e): **GIVEN** a third-person camera whose camera-centre ray clears a corner that the eye-pivot re-trace does not **WHEN** the point is computed **THEN** `P` is the first surface the eye-pivot re-trace (to the aim point extended 10 cm) meets; **GIVEN** a ground point 6001 cm from the eye pivot but 6184 cm from the camera **THEN** the press is refused on the client (6001 > 6000), and a point 5999 cm from the pivot but 6184 cm from the camera is sent.

---

## Implementation Notes

*Derived from ADR-0015 Decision §5 and GDD PNG-R3:*

Do not reuse camera's `WorldPoint` (it clamps to strike range and snaps to a `HideVolume` entry). Reuse only the ray construction of `GetAimRay()` through `IStealthCameraView` and run the ping's own query: (1) the `ViewBlock` world trace in CS-R8's form; (2) from the hit, offset 1 cm along the hit normal and trace straight down at most 300 cm to the first surface whose normal is within the walkable-floor angle (a ground hit already is); (3) `P`, then the range test `|P - E| <= PingRangeCm` with `E` the capsule-derived eye pivot (CS-R4). Because `ViewBlock` ignores pawns and foliage, the result is identical for an occupied and an empty bush, and no pawn id is ever read. The function lives in `Public/View/` (client-local helper, no `Server/` include) as `FTeamPingPointRule` with the trace and eye pivot injected. Enhanced Input and the press handling are story 010; the gate that consumes the result is story 009.

Prototype replaced: `AStealthCharacter::OnPingPressed` (a forward camera trace falling back to a fixed-distance end point, then `ServerPing(bHit ? Hit.ImpactPoint : End)` with a client cooldown). Retired in story 027.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Camera epic story 016 (`GetAimRay`) and story 007 (the `ViewBlock` channel and profiles).
- Story 009: the gate order and the bucket mirror.
- Story 010: binding the key and sending the RPC.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Public/View/Features/Pings/StealthTeamPingPointRule.h`
- `Source/StealthGameplay/Private/Features/Pings/StealthTeamPingPointRule.cpp`
- `Source/StealthGameplay/Private/Tests/Pings/PingPointRuleTest.cpp` (class `FPingsPointRuleTest`, name `StealthGame.Pings.PointRule`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

**Test: AC1 pawns and foliage ignored (AC-PNG-15 a, b)**
- Given: the ping-point function with an injected `ViewBlock` trace; (a) a ray passes a pawn and meets a wall 4 m behind at 150 cm; (b) a ray meets a bush `HideVolume` and the ground behind.
- When: compute `P`.
- Then: (a) `P` is the ground at the wall base, the pawn is never hit, no pawn id is read (spy); (b) `P` is the ground beyond the bush, identical for an occupied and an empty bush.
- Edge cases: a wall at exactly 150 cm against the 300 cm drop limit.

**Test: AC2 nothing-sent cases (AC-PNG-15 c)**
- Given: a facade at 450 cm with no walkable surface within 300 cm below; sky (no hit); a point beyond `PingRangeCm`.
- When: the press is evaluated.
- Then: nothing is sent in each case; the downward trace starts 1 cm along the hit normal, is <= 300 cm, on `ViewBlock`, and accepts the first walkable surface (the angle is the movement component's configured walkable-floor Z, not a literal).
- Edge cases: a surface at 299 and 301 cm below the start; a slope just inside and outside the walkable angle; range exactly 6000.

**Test: AC3 eye-pivot re-trace and range (AC-PNG-15 d, e)**
- Given: a third-person camera whose camera-centre ray clears a corner the pivot re-trace does not; ground points 6001 and 5999 cm from the pivot (both 6184 from the camera).
- When: compute and evaluate.
- Then: `P` is the first surface the pivot re-trace meets (aim point + 10 cm); 6001 refused client-side; 5999 sent.
- Edge cases: first person has no re-trace.

QL-STORY-READY: ADEQUATE

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Pings/PingPointRuleTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: cross-epic `production/epics/camera-system/story-016-aim-ray.md`, `production/epics/camera-system/story-007-viewblock-channel-collision-profiles.md`
- Unlocks: `production/epics/informant-pings-team-comms/story-009-client-gate-and-bucket-mirror.md`, `story-010-ia-ping-action-and-send-path.md`
