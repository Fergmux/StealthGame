# Story 014: AC 29 CameraToggleLocal

> **Epic**: Input & Interaction Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/input-interaction-framework.md`
**Requirement**: `TR-iif-003`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0011: Pawn command interface and input pipeline
**ADR Decision Summary**: Camera toggle and shoulder swap are bound in `IMC_Common` and handled client-locally by Camera (ADR-0015) with the `CameraToggleLockoutSec` lockout; they are never commands, never reach `ServerPawnCommand`, and work mid-hold and mid-move. (ADR-0015 is outside this epic's ADR list and is cited as context only.)
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Input layer HIGH. The test needs no post-cutoff API: it injects an `IA_CameraToggle` press and asserts that the transport records no RPC. The lockout value is read from tuning, not hard-coded. No V1-V6 item gates this story.

**Control Manifest Rules (this layer)**:
- Required: Gameplay input is bound on `AStealthPlayerController`; server-meaningful actions are Boolean — source: ADR-0011
- Required: Camera toggle and shoulder swap are bound in `IMC_Common` and handled client-locally (never commands) — source: ADR-0011 Decision §9, ADR-0015
- Forbidden: Never add a Move/Look RPC or a Look event; never add a gameplay RPC on the pawn — source: ADR-0011
- Forbidden: Never add aim assist, snapping, magnetism, slowdown, zoom, shake or FOV punches to the camera — source: ADR-0015
- Guardrail: Zero camera network traffic — source: ADR-0015

This story pins the "camera toggle is local" rule (GDD R14): pressing the first/third-person toggle changes only the local view, sends nothing, does not disturb a hold in progress, and has a short local lockout so the camera cannot flicker. It protects the parity boundary (a camera toggle must not produce any server-visible event) and the hold contract (the toggle must not end a brace or a tend).

It depends on story 007 (the `IA_CameraToggle` asset and `IMC_Common`). The lockout itself is Camera's responsibility (ADR-0015, camera-system epic, stories not yet written), so this story uses a stub camera handler that counts toggles and verifies the input side: no RPC, hold unchanged, lockout honoured from `CameraToggleLockoutSec`.

---

## Acceptance Criteria

*From GDD `design/gdd/input-interaction-framework.md`, scoped to this story:*

- [ ] AC 29 (R14, Logic, U `CameraToggleLocal`): **GIVEN** an active hold **WHEN** V is pressed **THEN** the hold continues and zero RPCs are sent; **AND** a second press within `CameraToggleLockoutSec` (0.30 s) is ignored.

---

## Implementation Notes

*Derived from ADR-0011 Decision §9 ("Camera toggle and shoulder swap (IIF R14)") and GDD R14:*

"Camera toggle and shoulder swap (IIF R14) are bound in `IMC_Common` and handled client-locally by Camera (ADR-0015) with the `CameraToggleLockoutSec` lockout; they are never commands, never reach `ServerPawnCommand`, and work mid-hold and mid-move." GDD R14: "First/third-person toggle is a press, takes effect immediately, has a 300 ms local lockout, works mid-hold and mid-move, and is never sent through the command interface. ... If Camera's ADR needs a server-side view check, Camera owns that message, not the IIF." The body presentation and permissions are identical in both modes (Camera owns this), so nothing about the toggle may leak to observers.

Tuning key: `CameraToggleLockoutSec`, default 0.30 s, hard range 0-1, safe 0.25-0.5, Audience All, Latch Use, client-local use. Camera's blend interlock (Camera I-CS-1: `CameraToggleBlendSec` at most lockout minus one frame, reject, never clamp) sets the safe minimum 0.25; hard values below blend plus one frame are rejected by I-CS-1 whatever the blend (Tuning Knobs table).

**QL-STORY-READY gap (to pin in the implementation):** the lockout boundary and anchor are undefined in the AC (at exactly 0.30 s? measured from the last accepted press or the last ignored press?). Proposed rewrite: "A camera-toggle press < `CameraToggleLockoutSec` after the last accepted toggle is ignored; at >= the lockout it is accepted; ignored presses do not extend the lockout." The test uses the stub camera handler (counts toggles): press at t = 1.0 accepted (hold continues, no End sent, slot unchanged, 0 RPCs, stub counts 1); second press at 1.299 ignored; at 1.30 accepted; an ignored press at 1.15 does not move the lockout (1.31 accepted). Also test a lockout of 0.60 read from tuning, and a toggle mid-Move (Move stream unchanged). A toggle while a menu is open is not covered by the AC; the author states the intended behaviour.

Prototype: the prototype's `V` camera toggle (first-playable controls) lives in the character/controller and is superseded by the `IMC_Common` binding. Tuning keys by name: `CameraToggleLockoutSec` 0.30.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Camera epic (cross-epic, stories not yet written): the toggle and lockout implementation and the blend interlock I-CS-1.
- `story-007-input-assets-and-trigger-lint.md`: the `IA_CameraToggle` asset and `IMC_Common` membership.
- `story-015-default-bindings.md`: the default key (V, Y/Triangle) mapping.
- `story-013-flush-sends-end.md`: End on context swap (not caused by a camera toggle).

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- `Source/StealthGame/Private/PawnCommandClient.cpp` (camera toggle forwarded locally only; bound via `AStealthPlayerController`)
- `Source/StealthGameplay/Private/Tests/Input/CameraToggleLocalTest.cpp` (class `FInputCameraToggleLocalTest`, name `StealthGame.Input.CameraToggleLocal`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

*Test cases not yet defined — run /qa-plan to generate them.*

QL-STORY-READY: GAPS
- Lockout boundary and anchor are undefined (at exactly 0.30 s? measured from last accepted or last ignored press?), and the lockout itself is Camera's (ADR 9, ADR-0015), cross-epic.
- QA-lead proposed rewrite (unconfirmed; the GDD AC stays the authority): "A camera-toggle press < CameraToggleLockoutSec after the last accepted toggle is ignored; at >= the lockout it is accepted; ignored presses do not extend the lockout."
- Scenario sketch from the qa file (`CameraToggleLocal`, AC 29): owner with an active Prepare hold and recording transport, camera handler stubbed (counts toggles); `IA_CameraToggle` pressed at t = 1.0 gives hold continues (no End sent, slot unchanged), 0 RPCs of any kind, stub counts 1; second press at 1.299 ignored, 1.30 accepted, and an ignored press at 1.15 does not move the lockout (1.31 accepted). Edge cases: toggle mid-Move (Move stream unchanged); lockout value read from tuning (test with 0.60); toggle while a menu is open (not covered; author).

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Input/CameraToggleLocalTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/input-interaction-framework/story-007-input-assets-and-trigger-lint.md`; cross-epic: camera-system (stories not yet written; toggle and lockout owner, stub used until it lands)
- Unlocks: None
