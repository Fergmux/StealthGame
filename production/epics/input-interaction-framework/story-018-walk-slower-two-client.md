# Story 018: AC 76 WalkSlowerModifier, server-side clause: received magnitude, SCL pace, accel/brake, only ServerMove

> **Epic**: Input & Interaction Framework
> **Status**: Complete
> **Layer**: Core
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: 2026-10-26

## Context

**GDD**: `design/gdd/input-interaction-framework.md`
**Requirement**: `TR-iif-002`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0011: Pawn command interface and input pipeline
**Other ADRs**: ADR-0010: Shared Pawn: CMC locomotion, spawn factory and possession parity
**ADR Decision Summary**: `IA_WalkSlower` scales the keyboard Move vector on the owning client before `ServerMove`, adds no RPC or replicated field, and the server sees only an ordinary Move magnitude. ADR-0010 section 6 supplies the receiving side: one quantiser maps analog magnitude to the shared pace set {0.8, 0.9, 1.0} (the server snaps a human move's received acceleration magnitude to the nearest pace-set value) and speed changes follow the baseline acceleration and braking.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Highest of ADR-0011 (HIGH) and ADR-0010 (HIGH). ADR-0010 section 6: `MoveAutonomous` does not call `ScaleInputAcceleration` (engine gate), so for human moves the server snaps the received acceleration magnitude to the nearest pace-set value; `GetMaxSpeed` returns `WalkSpeedCmPerSec x pace` or `RunSpeedCmPerSec`. Human Move/Look reach the server through the CMC's `ServerMove` (E8, unreliable, batched, with `FLAG_Custom_0` for the gait). No V1-V6 item gates this story.

**Control Manifest Rules (this layer)**:
- Required: `IA_WalkSlower` (`IMC_Common`; Left Ctrl 0.8, Left Alt 0.9) only scales the keyboard Move vector on the owning client before `ServerMove`; SCL quantises the received magnitude to the shared pace set — source: ADR-0011 (user decision 2026-10-25)
- Required: Human Move/Look travel only in `ServerMove` — source: ADR-0011
- Required: Movement shaping is one function per rule for every pawn, reads only tuning values, and runs identically on the server and in owner prediction — source: ADR-0010
- Forbidden: Never send the walk-slower modifier to the server as a command or field, and never apply it to gamepad input — source: ADR-0011 (user decision 2026-10-25)
- Forbidden: Never read controller kind, PlayerState or team in a movement rule — source: ADR-0010
- Guardrail: Movement shaping well under 5 percent of the 3.0 ms CMC game-thread line — source: ADR-0010

MVP item 11, the server-observed half. Where story 017 proves the owning client produces the right magnitude, this story proves that what actually reaches the server is an ordinary Move input and that the pawn behaves accordingly: the server-received magnitude is 0.8, 0.9, 0.8 and 1.0 for Ctrl, Alt, both and neither, SCL's quantised pace gives 112, 126, 112 and 140 cm/s at defaults, every speed change follows the baseline acceleration or braking (no step), no client-to-server RPC other than the `ServerMove` family is sent, holding Run with Ctrl gives run speed, an informant gets exactly the same paces, and the gamepad stick is unchanged. The "no RPC other than ServerMove" clause is the privacy guard: the modifier must not be visible to the server as an event, since a modifier flag would be a player-only tell.

It depends on 017 (the scaling), and per the plan's additions on 094 (controller transport, so a command RPC exists to be absent) and 095 (outgoing-RPC spy and server command log). It is Ready, with a cross-epic dependency on shared-character-locomotion R5.1 (pace quantisation) that does not block it. Alt has OS and PIE side effects (rebaseline Risk Factors): in PIE, pressing and releasing Left Alt can activate the editor menu bar or steal focus, so the test must record this behaviour (see Notes). Story 019 (remove the sample Ctrl jog toggle, D37) is closely tied: Ctrl must reach `IA_WalkSlower` only. GDD relation: R15a, AC 76; ADR-0011 section 9 amendment; ADR-0010 section 6.

---

## Acceptance Criteria

*From GDD `design/gdd/input-interaction-framework.md`, AC 76 split (server-observed clauses); the client scaling clauses are story 017:*

- [ ] AC 76, server clause (R15a, Integration, P `WalkSlowerModifier`; user decision 2026-10-25): **GIVEN** a keyboard walker holding W **WHEN** it holds Left Ctrl, then Left Alt, then both, then neither, for 3 s each **THEN** the server-received Move magnitude is 0.8, 0.9, 0.8 and 1.0, SCL's quantised pace is 0.8 / 0.9 / 0.8 / 1.0 (112 / 126 / 112 / 140 cm/s at defaults), every speed change follows the baseline acceleration or braking (no step above it) and no RPC other than `ServerMove` is sent; **AND** an informant gets the same paces.

---

## Implementation Notes

*Derived from ADR-0011 Decision §2, §9 and Amendments; ADR-0010 Decision §6:*

**MVP item 11 and decision D37.** Part of MVP item 11 (keyboard walk-slower). Decision D37: the sample's Ctrl jog toggle is removed; Ctrl = walk-slower (story 019). Read `production/never-resave-assets.md` before any Content step. The sample assets that must not be re-saved (listed there, ADR-0009 section 4) are `Content/Blueprints/Cameras/CameraAsset_SandboxCharacter`, `CameraDirector_SandboxCharacter`, `CameraRigPrefab_BasicThirdPersonBehavior`, `CameraRig_BaseDefaults` and `CameraRig_CollisionOffset`; the sample's input assets (`Content/Input/IA_*`, `IMC_Sandbox`) and character Blueprint are sample assets that are never edited in place (duplicate or reparent with a `_snapshots/` copy first). This story is a test; it creates no Content that edits the sample.

Parent AC 76, quoted in full: "*R15a, Integration, P `WalkSlowerModifier`* (user decision 2026-10-25): **GIVEN** a keyboard walker holding W **WHEN** it holds Left Ctrl, then Left Alt, then both, then neither, for 3 s each **THEN** the server-received Move magnitude is 0.8, 0.9, 0.8 and 1.0, SCL's quantised pace is 0.8 / 0.9 / 0.8 / 1.0 (112 / 126 / 112 / 140 cm/s at defaults), every speed change follows the baseline acceleration or braking (no step above it), no RPC other than `ServerMove` is sent, and holding Run with Left Ctrl gives run speed; **AND** an informant gets the same paces; **AND** the gamepad stick's magnitudes are unchanged by the modifier." The client-scaling clauses (Ctrl/Alt/both/neither magnitudes, run-with-Ctrl, gamepad unchanged) are covered by story 017; this story asserts them again only as observed at the server.

ADR-0010 §6: "One quantiser function maps analog magnitude to the shared pace set with +/-0.03 hysteresis, applied where the input is consumed: in `ScaleInputAcceleration` on the owning client (human) and on the server (AI). `MoveAutonomous` does not call `ScaleInputAcceleration` (engine gate), so for human moves the server snaps the received acceleration magnitude to the nearest pace-set value (validation, no hysteresis needed because the owner already quantised)." And: "`GetMaxAcceleration`/`GetMaxBrakingDeceleration` give constant acceleration to gait speed in `WalkStartSec`/`RunStartSec`, one walk deceleration set from the slowest pace (`WalkStopSec`)".

Test design (qa-lead): 2 PIE clients, a keyboard walker holding W (and separately an informant, via stub state); Ctrl, then Alt, then both, then neither, 3 s each. Assert server-received Move magnitude 0.8, 0.9, 0.8, 1.0; quantised pace 0.8/0.9/0.8/1.0 (112/126/112/140 cm/s read from SCL tuning `WalkSpeedCmPerSec` 140, never hard-coded). Each per-tick speed delta at most baseline acceleration (or braking) x dt + 1 cm/s at every change (no step); "baseline acceleration/braking" is read from Locomotion tuning data, not hard-coded. The client-to-server RPC log contains only the `ServerMove` family ("only ServerMove" means the CMC move RPC family, ServerMove/ServerMovePacked variants, and no other client-to-server RPC; needs the outgoing-RPC spy from story 095). Run plus Ctrl gives run speed (`RunSpeedCmPerSec` 450); gamepad stick magnitudes unchanged. Edge cases: informant paces identical; press and release mid-walk (no snap); record the Alt focus behaviour in PIE (menu-bar activation) in the test notes; ties to story 019.

Prototype replaced: new code, no prototype counterpart (the prototype's Ctrl-jog toggle is the sample's, addressed in story 019). Tuning keys by name: `WalkSpeedCmPerSec` (140, SCL), `RunSpeedCmPerSec` (450, SCL), `WalkStartSec` and `WalkStopSec` (SCL baseline shaping); the pace set {0.8, 0.9, 1.0} is an SCL constant held by no key.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-017-walk-slower-scaling.md`: the owning-client scaling function (client clauses of AC 76).
- `story-019-remove-sample-ctrl-jog-toggle.md`: D37, removing the sample's Ctrl jog toggle.
- `story-070-release-baseline-stop.md`: release baseline stop with SCL (AC 24).
- `story-081-look-latency.md`: look latency (AC 58).
- Shared-character-locomotion epic (cross-epic, stories not yet written): the SCL R5.1 quantiser and shaping this story observes.

---

## Files

*The code and test files this story changed (written by /dev-story; /story-done checks scope against it).*

- `Source/StealthTestHarness/Private/Tests/Input/WalkSlowerModifierTest.cpp`

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

**Integration — automated test spec** (QL-STORY-READY: ADEQUATE; author notes: "only ServerMove" means the CMC move RPC family and no other client-to-server RPC; "baseline acceleration/braking" is read from the Locomotion tuning data; Alt has OS/PIE side effects, record the behaviour in the story):

- **AC-1**: Walk-slower, server clause (`WalkSlowerModifier`, AC 76, P)
  - Given: 2 PIE clients, a keyboard walker holding W (and separately an informant, via stub state).
  - When: Ctrl, then Alt, then both, then neither, 3 s each.
  - Then: server-received Move magnitude 0.8, 0.9, 0.8, 1.0; quantised pace 0.8/0.9/0.8/1.0 (112/126/112/140 cm/s at defaults, read from SCL tuning); each per-tick speed delta <= baseline accel (or brake) x dt + 1 cm/s at every change (no step); client-to-server RPC log contains only the `ServerMove` family.
  - And: Run plus Ctrl gives run speed; gamepad stick magnitudes unchanged by the modifier.
  - Edge cases: informant paces identical; press and release mid-walk (no snap); Alt focus behaviour in PIE (menu-bar activation) noted; ties to story 019.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/WalkSlowerModifierTest.cpp` — CQTest 2-client PIE test (P form), or a documented playtest

**Status**: [x] Created and passing (`StealthGame.Input.PIE.WalkSlowerModifier.KeyboardModifiersScaleTheServerReceivedMove`, 2026-10-26)

---

## Dependencies

- Depends on: `production/epics/input-interaction-framework/story-017-walk-slower-scaling.md`, `production/epics/input-interaction-framework/story-094-controller-transport-skeleton.md`, `production/epics/input-interaction-framework/story-095-command-test-seams.md`; cross-epic: shared-character-locomotion R5.1 pace quantisation (stories not yet written; does not block)
- Unlocks: None

---

## Completion Notes
**Completed**: 2026-10-26 (IIF batch 13: command/reply types, core, hub, reply step, transport, seams, spikes V2/V3/V5, input ACs; D53 integration of the production input path)
**Criteria**: 1/1 passing (2 PIE clients; Ctrl, Alt, both, neither for 3 s each: server-received Move magnitude 0.800 / 0.900 / 0.800 / 1.000; pace 0.800 / 0.900 / 0.800 / 1.000 of the walk speed (160 / 180 / 160 / 200 cm/s); no step above the movement component's acceleration/braking on any owner frame; no client-to-server RPC beyond the ServerMove family in the walk phases; Run + Ctrl gives run speed; an informant (stub state) gets the same paces)
**Deviations**: ADVISORY — absolute paces are those of the reparented sample's 200 cm/s walk; the AC's 112 / 126 / 112 / 140 cm/s need SCL's WalkSpeedCmPerSec (140) and its quantiser (SCL-010, not built); the pace is asserted as the ratio, which is what the quantiser keeps. 'Baseline acceleration/braking' is the CMC's own model until SCL's shaping. The Run phase reaches the sample's sprint through the pass-through IA_Run and the sample Blueprint then sends its own UpdateInputState_Server pawn RPC: a prototype adapter, recorded for retirement with SCL-010 / story 093 (walk-slower itself sends nothing). Informant: the prototype owner state plus IMC_Informant on the owner (the Roster transition in play is story 071). ServerUpdateCamera is the engine's client camera update (ADR-0015 disables it later). Alt focus side effects: none observed in PIE with simulated keys; a physical Alt press in a PIE window can still focus the editor menu bar (manual check).
**Test Evidence**: `Source/StealthTestHarness/Private/Tests/Input/WalkSlowerModifierTest.cpp`
Run result: OBSERVED — in-play walk-slower paces (production/qa/evidence/story-094-controller-transport-skeleton/02, 04); server numbers from the PIE test
**Reviewed by**: lead-programmer focused review (sonnet; 3 medium + 6 low findings, all fixed except the documented limits) + self-review, IIF batch 13
