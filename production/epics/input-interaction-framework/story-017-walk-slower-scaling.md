# Story 017: AC 76, client scaling clause: IA_WalkSlower scales keyboard Move to 0.8 / 0.9 / 0.8 / 1.0

> **Epic**: Input & Interaction Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/input-interaction-framework.md`
**Requirement**: `TR-iif-002`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0011: Pawn command interface and input pipeline
**ADR Decision Summary**: `IA_WalkSlower` (Axis1D, `IMC_Common`; Left Ctrl to 0.8, Left Alt to 0.9) scales the keyboard Move vector on the owning client before `ServerMove`; it is never a command, adds no RPC or replicated field, and the server sees only an ordinary Move magnitude that SCL quantises to the shared pace set. Gamepad input is untouched (ADR-0011 amendment, user decision 2026-10-25).
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Input layer HIGH. Enhanced Input's default accumulation of two mappings on one Axis1D action is `TakeHighestAbsoluteValue`, which would give 0.9 for Ctrl plus Alt held together; the required result is 0.8 (a minimum), so explicit minimum logic is needed (see Implementation Notes). The scaling is a pure function in plain C++ so it is unit-testable headless. No V1-V6 item gates this story.

**Control Manifest Rules (this layer)**:
- Required: `IA_WalkSlower` (`IMC_Common`; Left Ctrl 0.8, Left Alt 0.9) only scales the keyboard Move vector on the owning client before `ServerMove`; SCL quantises the received magnitude to the shared pace set — source: ADR-0011 (user decision 2026-10-25)
- Required: Human Move/Look travel only in `ServerMove` — source: ADR-0011
- Forbidden: Never send the walk-slower modifier to the server as a command or field, and never apply it to gamepad input — source: ADR-0011 (user decision 2026-10-25)
- Forbidden: Never add a Move/Look RPC or a Look event; never add a gameplay RPC on the pawn — source: ADR-0011
- Guardrail: Movement shaping (SCL) well under 5 percent of the 3.0 ms CMC game-thread line; the client scaling is a few arithmetic operations per input frame — source: ADR-0010

MVP item 11 (keyboard walk-slower), the client half. Keyboard Move is digital and always gave magnitude 1.0, so the crowd's slower paces 0.8 and 0.9 were NPC-only for keyboard players; the walk-slower modifier lets a keyboard player reproduce the whole pace set, which is a chosen deviation under Pillar 2 rather than a tell. This story builds only the owning-client scaling: given the keyboard Move vector, the held modifiers, whether Run is held and the device, produce the magnitude `ServerMove` will carry. The server-observed behaviour (received magnitude, SCL pace, acceleration shaping, only `ServerMove` sent, informant parity) is story 018; removing the sample's conflicting Ctrl jog toggle is story 019 (decision D37).

GDD AC 76 is split per the plan: this story takes the Ctrl/Alt/both/neither magnitudes, the run-with-Ctrl and the gamepad-unchanged clauses; 018 takes the server-observed clauses. The parent AC is quoted in full under Implementation Notes so nothing is lost. It depends on story 007 (`IA_WalkSlower` and `IMC_Common`). It unlocks 018, 019, 070 (release baseline stop) and 081 (look latency).

---

## Acceptance Criteria

*From GDD `design/gdd/input-interaction-framework.md`, AC 76 split (client scaling clauses only); the server-observed clauses are story 018:*

- [ ] AC 76, client clause (R15a, Logic, U `WalkSlowerScaling`; user decision 2026-10-25): **GIVEN** a keyboard walker holding W **WHEN** it holds Left Ctrl, then Left Alt, then both, then neither **THEN** the Move magnitude the owning client produces (the value `ServerMove` then carries) is 0.8, 0.9, 0.8 and 1.0, and holding Run with Left Ctrl gives run speed (no scale); **AND** the gamepad stick's magnitudes are unchanged by the modifier.

---

## Implementation Notes

*Derived from ADR-0011 Decision §9 and Amendments (user scope decision 2026-10-25); GDD R15a:*

**MVP item 11 and decision D37.** This story is part of MVP item 11. Decision D37 (user, 2026-10-25): the Game Animation Sample's Ctrl jog toggle is removed and Ctrl = walk-slower (story 019). Read `production/never-resave-assets.md` before any Content step: the sample's input assets and Blueprints (`Content/Input/IA_*`, `IMC_Sandbox`, the character Blueprint) and the five `Content/Blueprints/Cameras/*` assets listed there must not be re-saved; this story adds no Content beyond the assets authored in story 007 and edits none of the sample's.

Parent AC 76, quoted in full: "*R15a, Integration, P `WalkSlowerModifier`* (user decision 2026-10-25): **GIVEN** a keyboard walker holding W **WHEN** it holds Left Ctrl, then Left Alt, then both, then neither, for 3 s each **THEN** the server-received Move magnitude is 0.8, 0.9, 0.8 and 1.0, SCL's quantised pace is 0.8 / 0.9 / 0.8 / 1.0 (112 / 126 / 112 / 140 cm/s at defaults), every speed change follows the baseline acceleration or braking (no step above it), no RPC other than `ServerMove` is sent, and holding Run with Left Ctrl gives run speed; **AND** an informant gets the same paces; **AND** the gamepad stick's magnitudes are unchanged by the modifier."

GDD R15a: "The client-local input action `IA_WalkSlower` (an Axis1D action in `IMC_Common`, so informants have it too) scales the **keyboard** Move vector before it is sent: held Left Ctrl gives magnitude 0.8, held Left Alt 0.9, both 0.8, neither 1.0. It never reaches the server as a command, adds no replicated field and no RPC ... It has no effect while Run is held (Run has no scale), it does not touch gamepad input (the stick already covers the set), and pressing or releasing it mid-walk changes speed through SCL's baseline acceleration and braking, never a snap." ADR-0011 amendment: "`IA_WalkSlower` ... scales the **keyboard** Move vector on the owning client before `ServerMove`. It is never a command, adds no RPC or replicated field, and the server sees only an ordinary Move magnitude that SCL quantises to the shared pace set (IIF R15a, SCL R5.1). Gamepad input is untouched."

Implementation points (from the qa-lead spec): a pure function `(Move vector, source = keyboard, Ctrl, Alt, Run held, device) to Move vector` in plain C++ (planned in `StealthGameplay/Public/View/Core/`). Diagonal W+D with Ctrl is magnitude 0.8, not 1.13: clamp, not just multiply. "Both gives 0.8" is a minimum; Enhanced Input's default accumulation (`TakeHighestAbsoluteValue`) would give 0.9, so use explicit minimum logic and test that case. Releasing mid-walk returns 1.0 the same frame (smoothing is SCL's job, not the input layer's). With Run held, no scale (gait is Run). Gamepad and keyboard active together: the stick part is unchanged. No server command is produced.

**Magnitude constants:** the pace set {0.8, 0.9, 1.0} is an SCL R5.1 constant "held by no key" (SCL GDD line 66, decision D2), so IIF has no tuning key for the 0.8 and 0.9 values; define them once as named constants shared with SCL's quantiser (cross-epic: shared-character-locomotion, stories not yet written) rather than as tuning data. The qa-lead's "read from tuning data" applies to `WalkSpeedCmPerSec` (140) in story 018, not to these magnitudes. Tuning keys by name: none owned here; `WalkSpeedCmPerSec` (140, SCL) is consumed in 018.

Prototype replaced: prototype Shift-run and Ctrl-jog-toggle controls (`StealthCharacter`/sample input); new code, no direct prototype counterpart for the modifier.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-018-walk-slower-two-client.md`: server-received magnitude, SCL pace 112/126/112/140 cm/s, no step above baseline accel/brake, only `ServerMove` sent, informant paces (AC 76 server clauses).
- `story-019-remove-sample-ctrl-jog-toggle.md`: removing the sample's Ctrl jog toggle (D37).
- `story-007-input-assets-and-trigger-lint.md`: authoring `IA_WalkSlower` and `IMC_Common`.
- `story-015-default-bindings.md`: the default key table for walk-slower.
- Shared-character-locomotion epic (cross-epic, stories not yet written): SCL R5.1 pace quantisation and shaping.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- `Source/StealthGameplay/Public/View/Core/WalkSlowerScale.h` (pure scaling function)
- `Source/StealthGameplay/Private/View/Core/WalkSlowerScale.cpp`
- `Source/StealthGame/Private/PawnCommandClient.cpp` (applies the scaling to the keyboard Move vector before `ServerMove`)
- `Source/StealthGameplay/Private/Tests/Input/WalkSlowerScalingTest.cpp` (class `FInputWalkSlowerScalingTest`, name `StealthGame.Input.WalkSlowerScaling`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

**Logic — automated test spec** (QL-STORY-READY: ADEQUATE):

- **AC-1**: Walk-slower scaling, client clause (`WalkSlowerScaling`, AC 76 client clause)
  - Given: a pure function (Move vector, keyboard source, Ctrl, Alt, Run held, device) with magnitudes 0.8/0.9 (the SCL pace-set constants; see Implementation Notes).
  - When: W held and Ctrl, then Alt, then both, then neither.
  - Then: output magnitude 0.8, 0.9, 0.8, 1.0.
  - Given: W held with Run held, with Ctrl, Alt and both. Then: magnitude 1.0, Gait = Run (no scale).
  - Given: gamepad stick at 0.5 and 1.0 with Ctrl/Alt held. Then: unchanged. Gamepad and keyboard active together: stick part unchanged.
  - Edge cases: diagonal W+D with Ctrl is magnitude 0.8, not 1.13 (clamp, not just multiply); "both gives 0.8" is a min, but Enhanced Input's default accumulation (`TakeHighestAbsoluteValue`) would give 0.9, so explicit min logic needs this case; release mid-walk returns 1.0 the same frame (smoothing is SCL's); no server command is produced; read `production/never-resave-assets.md` before any Content edit.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Input/WalkSlowerScalingTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/input-interaction-framework/story-007-input-assets-and-trigger-lint.md`; cross-epic: shared-character-locomotion (stories not yet written; SCL R5.1 pace-set constants)
- Unlocks: `production/epics/input-interaction-framework/story-018-walk-slower-two-client.md`, `production/epics/input-interaction-framework/story-019-remove-sample-ctrl-jog-toggle.md`, `production/epics/input-interaction-framework/story-070-release-baseline-stop.md`, `production/epics/input-interaction-framework/story-081-look-latency.md`
