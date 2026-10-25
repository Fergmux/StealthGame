# Story 021: Camera mode not observable

> **Epic**: Camera System
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/camera-system.md`
**Requirement**: `TR-camera-001`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0015: Camera system and LOS envelope (with ADR-0003 for the uniform pawn net configuration)
**ADR Decision Summary**: Camera mode is invisible to every other machine, the listen host included: mode, boom, FOV and shoulder side are client settings; only the standard control rotation reaches the server and it is identical in both modes because the envelope is identical; no RPC, replicated property, component, pose, animation tier, footstep or sound depends on the camera.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0015 overall; this is the integration proof across stories 004-010)
**Engine Notes**: A 2-client PIE test with a recorded 60 s input track and a fixed `RoundSeed`; the comparison covers `ServerMove` payload bytes, observer-side replicated properties, pose, tier and step events. Use the harness's raw-bunch capture (ADR-0008). Related SCL assertion `shared-character-locomotion/story-042-camera-toggle-changes-nothing.md` (AC-SCL-30) covers the pose side of toggling from SCL's angle; this story is the camera's own run and adds shoulder swaps and FOV changes. No ADR-0015 verification item gates it.

**Control Manifest Rules (this layer)**:
- Required: Nothing about camera mode is replicated or sent; the camera sends only the standard control rotation inside `ServerMove` — source: ADR-0015, ADR-0003
- Forbidden: Never add a camera-mode field, RPC or replicated property — source: ADR-0015
- Guardrail: Zero RPCs other than movement from the camera code

This is the Pillar 2 proof: a spy cannot tell whether an opponent is in first or third person from anything observable. It runs after the camera features exist.

---

## Acceptance Criteria

*From GDD `design/gdd/camera-system.md` (AC-CS-01 and the last line of AC-CS-32), scoped to this story:*

- [ ] **AC1** (AC-CS-01, [N] `ModeNotObservable`): **GIVEN** two clients, a fixed `RoundSeed` and NPC stream, and a scripted 60 s input track for client A (walk, look, strike a dummy) **WHEN** the track is run once in 1P and once in 3P, with 20 toggles, 10 shoulder swaps and 3 FOV changes added in the 3P run **THEN** the `ServerMove` payload bytes from A are identical between the runs frame for frame, client B's recorded replicated properties, pose, animation tier and step events for A's pawn are identical, and zero RPCs other than movement are sent by A's camera code.
- [ ] **AC2** (AC-CS-32 last line, [N]): **GIVEN** Reduce camera motion on **WHEN** AC1's runs are repeated **THEN** AC1 still passes.

---

## Implementation Notes

*Derived from ADR-0015 Decision §1, §7 and ADR-0008:*

Use the `StealthTestHarness` CQTest PIE support (`StealthGame.Camera.PIE.ModeNotObservable.*`), listen server plus 1 client, and the input injection through the Look axis after sensitivity (GDD AC preamble). Record per-frame `ServerMove` bytes at the client send path and compare byte-for-byte; record client B's received properties for A's pawn through the replication-system assertion from tuning-data-config `story-018-replication-system-assertion.md`. A fixed dt of 1/60 and 30 settle frames apply. A failure here is a Pillar 2 defect, not a flaky test: investigate before retrying (`.claude/rules/test-standards.md`).

Prototype replaced: the prototype's `bFirstPerson` local-only toggle was replicated nowhere, but its first-person camera attached to a mesh socket and so changed the observed head pose; this test is the regression guard.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-004-camera-manager-shell-remote-pov.md`: AC-CS-27 and AC-CS-02 (traffic and component parity).
- SCL `story-042-camera-toggle-changes-nothing.md`: pose-bone parity for toggling.
- IIF `story-014-camera-toggle-local.md`: the toggle press and lockout.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthTestHarness/Private/Camera/ModeNotObservableTest.cpp` (CQTest, `StealthGame.Camera.PIE.ModeNotObservable.*`)
- `Source/StealthTestHarness/Private/Camera/ModeNotObservableInputTrack.h` (recorded input track as a `constexpr` array)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

*Test cases not yet defined — run /qa-plan to generate them.*

QL-STORY-READY: GAPS
- AC1 (a): "zero RPCs other than movement are sent by A's camera code" cannot be attributed from the wire, and the track includes a strike, which sends an Attack RPC. QA-lead proposed rewrite (unconfirmed): A's full RPC sequence is identical across the runs, and no RPC is declared on a camera class (reflection).
- AC1 (b): the Attack payload's `WorldPoint` and `TargetPawn` come from `GetAimRay`, whose origin differs in 1P and 3P, yet client B's observed pose for the strike must be identical. The author confirms the scripted strike's result is mode-invariant.
- AC1 (c): frame-for-frame byte equality needs a deterministic harness and a stated capture hook (net-driver send hook or saved-move pack level).
- Scenario sketches: AC1 2 clients, fixed `RoundSeed` and NPC stream, 60 s track run once in 1P and once in 3P with 20 toggles, 10 swaps and 3 FOV changes (toggles during a strike montage included): `ServerMove` bytes from A identical per frame, client B's recorded properties, pose, animation tier and step events for A identical. AC2 the same with Reduce camera motion on in both runs.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: `ModeNotObservableTest.cpp` under `Source/StealthTestHarness/Private/Camera/` OR a documented playtest

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/camera-system/story-005-look-wiring-control-rotation.md`, `story-008-toggle-blend-reduce-motion-remembered-mode.md`, `story-010-field-of-view.md`, `story-015-mask-apply-path.md`; cross-epic: tuning-data-config `story-018-replication-system-assertion.md`, `story-019-cqtest-headless-proof.md`
- Unlocks: `production/epics/camera-system/story-023-playtests-scout-read-own-pawn.md`
