# Story 022: Mask performance acceptance on 3060-class hardware

> **Epic**: Camera System
> **Status**: Blocked — BLOCKED: (1) spike story 011 (ADR-0015 OQ5/OQ10) has not reported; (2) ADR-0022 gate spikes R1-R3 and the named RTX 3060-class reference machine do not exist yet (every performance number so far is from an RTX 5090 and is development evidence; AQ-18 has no owner)
> **Layer**: Core
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/camera-system.md`
**Requirement**: `TR-camera-008`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0015: Camera system and LOS envelope; budget lines from ADR-0022: Rendering configuration and performance budgets
**ADR Decision Summary**: The eye-mask trace cost is bounded at 60 pawns: the same line-of-sight traces at every range in rate bands (60 Hz within `EyeMaskNearCm`, 15 Hz to `EyeMaskRangeCm`, 5 Hz beyond), never a distance cutoff; game-thread target ≤ 0.5 ms (async submission and result handling, ≤ 160 traces per frame cap), inside architecture.md's "camera eye-mask traces + misc 1.0 ms" line. A failure goes to the technical-director, never fixed by dropping the line-of-sight test.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Physics trace work on task threads is about 1.5 ms worst case (295 traces at about 5 µs; unverified, measured by story 011 and here). If the async path misses the line, ADR-0015 §4.3.6 allows synchronous `LineTraceTestByChannel` with early exit for the near band; fewer probes for distant pawns is a tuning change subject to the creative director's limit that 3P reveal must not be noticeably worse than 1P culling. Evidence from the RTX 5090 development machine is **not acceptance evidence** (ADR-0013 manifest rule: never cite RTX 5090 numbers as acceptance evidence).

**Control Manifest Rules (this layer)**:
- Required: The mask runs at every range with distance changing only the rate; round-robin deferral by registry index — source: ADR-0015
- Forbidden: Never drop the line-of-sight test to meet budget; never cite RTX 5090 numbers as acceptance evidence — source: ADR-0015, ADR-0013
- Guardrail: Mask game-thread mean ≤ 0.5 ms, p99 ≤ 1.0 ms at 59 near pawns, 1080p, RTX 3060-class reference machine

---

## Acceptance Criteria

*From GDD `design/gdd/camera-system.md` (AC-CS-13, split by assertion), scoped to this story:*

- [ ] **AC1** (AC-CS-13, [P] `MaskPerf`): **GIVEN** 59 dummies within 20 m (the `NpcCount` 50 worst case: 60 characters less the owner), all inside the eye cone and behind walls (5 traces each, 295 per frame), on a named RTX 3060-class reference machine at 1080p **WHEN** the mask runs for 60 s **THEN** its mean game-thread cost is ≤ 0.5 ms.
- [ ] **AC2** (AC-CS-13): **THEN** its p99 per-frame cost is ≤ 1.0 ms over the same 60 s.
- [ ] **AC3** (AC-CS-13): **THEN** the evidence file records the GPU, CPU, RAM, resolution and settings of the reference machine; **AND** a failure goes to technical-director (OQ10) and is never fixed by dropping the line-of-sight test.

---

## Implementation Notes

*Derived from ADR-0015 Performance Implications and Risk R-2:*

Measure with Unreal Insights game-thread timing around the mask tick and result handling, identical scene to story 011 AC3 (59 dummies, all in the cone, blocked), 60 s. The test fails if the thresholds are exceeded (test-standards: performance tests specify thresholds and fail if exceeded). Report in `production/qa/evidence/camera-mask-perf-evidence.md` with the trace summary and the machine description. If the near-band synchronous fallback is in use (story 011 AC3 decision), measure that configuration.

This story stays Blocked until a 3060-class machine is available and ADR-0022's R1-R3 spikes have reported, because the mask's cost is only meaningful next to the frame it shares with Lumen and skinning (ADR-0022 §4.2). If the owner decides to accept development-machine evidence for this line, record that as a decision in `production/session-logs/decision-log.md` and unblock explicitly.

Prototype replaced: none.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-011-mask-spike-oq5-oq10.md`: the development-machine spike.
- `story-012-eye-mask-core.md`, `story-013-mask-live-world-is-eye-mask-visible.md`: the mask being measured.
- ADR-0022 R1-R3 captures and the whole-frame RC-1/RC-2 captures: their owners.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Private/Tests/Camera/MaskPerfTest.cpp` (functional performance test, name `StealthGame.Camera.MaskPerf`, thresholds as constants)
- `production/qa/evidence/camera-mask-perf-evidence.md` (machine description, Insights summary)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

QL-STORY-READY: ADEQUATE (all three criteria are deferred to the named reference machine; no RTX 3060-class machine is recorded yet, `project.yaml` names only the class)

Test: AC1 `MaskPerf` mean (AC-CS-13)
  Given: the named machine at 1080p with 59 dummies within 20 m, in the cone and behind walls (295 traces per frame).
  When: it runs for 60 s, with the mask scope defined as submission, result handling and apply.
  Then: game-thread mean ≤ 0.5 ms.
  Edge cases: none.

Test: AC2 `MaskPerf` p99
  Given: the same run (about 3600 frames).
  When: per-frame costs are sorted.
  Then: p99 per-frame cost ≤ 1.0 ms.
  Edge cases: none.

Test: AC3 `MaskPerfEvidence`
  Given: the evidence file.
  When: it is reviewed.
  Then: it records GPU, CPU, 1080p, 59 dummies within 20 m behind walls and the observed 295 traces per frame; on failure it records an escalation to technical-director; no distance-cutoff constant exists (story 012 AC3).
  Edge cases: none.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: `MaskPerfTest.cpp` OR a documented playtest, plus `production/qa/evidence/camera-mask-perf-evidence.md` from the 3060-class machine

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/camera-system/story-011-mask-spike-oq5-oq10.md` (blocker), `story-013-mask-live-world-is-eye-mask-visible.md`, `story-015-mask-apply-path.md`; external: an RTX 3060-class reference machine and ADR-0022 R1-R3
- Unlocks: epic Definition of Done for TR-camera-008
