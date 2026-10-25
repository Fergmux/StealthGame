# Story 024: Presentation include check and Tell Matrix review

> **Epic**: Camera System
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/camera-system.md`
**Requirement**: `TR-camera-004`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0015: Camera system and LOS envelope (with ADR-0007 §3 include lint)
**ADR Decision Summary**: The mask and rig read only geometry and the local controller's own possessed pawn; the camera module includes no server query header and calls `GetPlayerState`, `GetController`, `IsPlayerControlled` or `GetOwner` only on the local controller's own pawn. Crowd's virtual look calls the same `EvaluateViewEnvelope`, and the Tell Matrix has a "camera mode / toggle" row reading "no visible deviation".
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0015 overall; this story is a static scan and a document review)
**Engine Notes**: No engine API. The scan is a T0 lint in the ADR-0007/0008 framework: it parses `#include` lines and greps identifiers in the camera module sources.

**Control Manifest Rules (this layer)**:
- Required: Include lint (ADR-0007 §3) and AC-IVP-17 apply to the camera module — source: ADR-0007, ADR-0015
- Forbidden: The camera never includes a `Public/Server/` header or a server query header (Policy, Roster, Round, LSC, Penalty, Objectives) — source: ADR-0015
- Guardrail: Lint runs in the per-commit gate in well under a second

---

## Acceptance Criteria

*From GDD `design/gdd/camera-system.md` (AC-CS-39, AC-CS-23), scoped to this story:*

- [ ] **AC1** (AC-CS-39, [U] `PresentationIncludeCheck`, IVP AC-IVP-17): **GIVEN** the camera module **WHEN** the include and grep check runs **THEN** it includes no server query header, and it calls `GetPlayerState`, `GetController`, `IsPlayerControlled` or `GetOwner` only on the local controller's own possessed pawn.
- [ ] **AC2** (AC-CS-23, doc review, ADVISORY, not automated, `TellMatrixRow`): **GIVEN** the Baseline and Tell Matrix **WHEN** it is reviewed (design review of Crowd, and code review of Crowd's virtual-look call site) **THEN** it has a "camera mode / toggle" row reading "no visible deviation" pointing to Camera, and Crowd's virtual look calls `EvaluateViewEnvelope`.

---

## Implementation Notes

*Derived from ADR-0007 §3, ADR-0015 Guideline 4 and GDD AC-CS-23:*

Extend the T0 lint set (tuning-data-config `story-011-t0-lint-framework-and-gate.md`) with a camera rule: sources under the camera folders (`StealthGameplay/Core/Camera`, `Public/View/Core/StealthPlayerCameraManager.h` and friends) must not include any `Public/Server/` path and must not reference a banned identifier unless the call is on the local controller's possessed pawn (an allowlist of the specific call sites, each commented). The negative control (tuning-data-config `story-010-negative-control-helper.md`) proves the lint fails on a seeded violation; watch it fail before trusting it (`.claude/rules/test-standards.md`). AC2 is verified by reading `design/gdd/crowd-npc-behaviour.md` (its Tell Matrix) and the Crowd code review of the virtual-look call; record the finding in `production/qa/evidence/camera-tell-matrix-review.md`. It is advisory, retagged 2026-10-25 (D6).

Prototype replaced: none.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- crowd-npc-behaviour epic: Crowd's virtual look implementation and its own Tell Matrix tests.
- `story-012-eye-mask-core.md` AC2: the mask-code static scan for team/binding reads.
- teammate-markers / hud presentation lints: their own AC-TMK-06, AC-PSR-42.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `tools/lint/` camera include rule (path set by the T0 lint framework story)
- `Source/StealthGameplay/Private/Tests/Camera/PresentationIncludeCheckTest.cpp` (class `FCameraPresentationIncludeCheckTest`, name `StealthGame.Camera.PresentationIncludeCheck`)
- `production/qa/evidence/camera-tell-matrix-review.md`

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

*Test cases not yet defined — run /qa-plan to generate them.*

QL-STORY-READY: GAPS
- AC1: "no server query header" names no header list. QA-lead proposed rewrite (unconfirmed): a checked-in forbidden-header list taken from the ADR-0007 §2 server-only folders.
- AC1: "only on the local controller's own possessed pawn" cannot be proven by grep. Proposed rewrite: grep for `GetPlayerState`, `GetController`, `IsPlayerControlled` and `GetOwner` call sites; each must be in an allowlist file with a justification, and the call sites are code-reviewed. Edge cases: indirect includes, macros, Blueprint nodes.
- Manual check AC2 (advisory doc review): read the Crowd GDD Tell Matrix and Crowd's virtual-look call site; the Matrix has a "camera mode / toggle" row reading "no visible deviation" pointing to Camera, and the call site calls `EvaluateViewEnvelope`. The reviewer records both checks; mark DEFERRED if the Crowd GDD row or code is not yet written.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `PresentationIncludeCheckTest.cpp` under `Source/StealthGameplay/Private/Tests/Camera/` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later); AC2 is advisory and recorded in `production/qa/evidence/camera-tell-matrix-review.md`

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/camera-system/story-013-mask-live-world-is-eye-mask-visible.md` (the camera module has code to scan); cross-epic: tuning-data-config `story-011-t0-lint-framework-and-gate.md`, `story-010-negative-control-helper.md`; crowd-npc-behaviour (virtual look, Tell Matrix)
- Unlocks: None
