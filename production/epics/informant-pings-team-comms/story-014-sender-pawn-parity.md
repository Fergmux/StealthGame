# Story 014: Sender pawn parity

> **Epic**: Informant Pings & Team Communication
> **Status**: Ready
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/informant-pings-team-comms.md` (PNG-R4, PNG-R9 pawn bullets; AC-PNG-02)
**Requirement**: `TR-png-018`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0008: Test and automation strategy for leak and parity tests (Technical Setup 2026-10-25b: Pings parity rows), with ADR-0001 (one command path, pawn carries only `All` facts)
**ADR Decision Summary**: Headless pure-C++ cores, a multi-client and multi-process leak harness, raw-bunch capture and net-emulation profiles, with per-commit canaries and a nightly tier. The Pings rows: T2 `StealthGame.Pings.PIE.PawnParity` (AC-PNG-02) and nightly T3 `StealthGame.Pings.MP.PawnParity` (raw-bunch byte parity; the ping payload is one quantised `Point`), each watched failing against injected-leak negative controls.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (ADR-0008; relies on the legacy NetDriver's replication of pawns)
**Engine Notes**: Gauntlet multi-process nodes are named `StealthGame.Pings.MP.PawnParity`; the cooked Development build is required for the raw-bunch capture (the harness module is in Development, never Shipping). The leak harness startup assertion (legacy replication system) must hold.

**Control Manifest Rules (this layer)**:
- Required: Pings leak and parity rows T2 `StealthGame.Pings.PIE.PawnParity` and nightly T3 `StealthGame.Pings.MP.PawnParity`, each watched failing against injected-leak negative controls — source: ADR-0008
- Forbidden: Never change the sender's pawn for a ping (no RPC, field, pose, gesture, `ForceNetUpdate`, world sound or effect); never name a pawn in the payload — source: ADR-0011, ADR-0001
- Forbidden: Never compile negative-control types or test seams into Shipping — source: ADR-0008

---

## Acceptance Criteria

*From GDD `design/gdd/informant-pings-team-comms.md` (AC-PNG-02, PNG-R4, PNG-R9), scoped to this story:*

- [ ] **AC1** (AC-PNG-02, `[Integration]` `StealthGame.Pings.PIE.PawnParity`): **GIVEN** a sender standing still and, in a second run, walking a fixed path **WHEN** it pings 5 times **THEN** the sender pawn's replicated state as received by an opponent client (`FActionState`, movement, head-look, net-update times, relevancy) is byte-identical to the control run without pings, no `ForceNetUpdate` occurs on it, and no resolution-queue item or IIF command is created.
- [ ] **AC2** (AC-PNG-02 payload): **GIVEN** the captured `ServerTeamPing` payload **THEN** it is exactly one quantised `Point` (no pawn id, no other field).
- [ ] **AC3** (ADR-0008 T3 nightly): **GIVEN** the nightly test `StealthGame.Pings.MP.PawnParity` (raw-bunch byte parity) **WHEN** run with its injected-leak negative control **THEN** the control fails and the clean run passes.

---

## Implementation Notes

*Derived from ADR-0008 Technical Setup 2026-10-25b:*

AC1 and AC2 run in one PIE test with a deterministic seed and a scripted walk; compare the opponent client's received pawn state per tick against a control run. For `ForceNetUpdate`, count calls through the pawn net-update spy that the IVP epic's pawn-parity tests use (`participant-state-roster/story-049-pie-pawn-replication-parity.md` is the analogue) and assert zero attributable to a ping. AC2 inspects the captured RPC bunch payload (raw-bunch capture) for a single `FVector_NetQuantize10`. AC3 adds the nightly Gauntlet multi-process node; its injected leak is a test-only seam that makes the handler call `ForceNetUpdate` on the sender pawn or add a pawn id to the payload, enabled only behind `WITH_STEALTH_TEST_SEAMS`. A leak test that has only ever been seen passing is not accepted (watch each negative control fail).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 013: the opponent-sees-nothing run (parts, widgets, sounds, RPC lists).
- IVP and roster epics: general pawn-replication parity for player vs NPC pawns.
- Story 006: the handler itself.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthTestHarness/Private/Pings/PawnParityTest.cpp` (CQTest PIE, `NETWORK_TEST_CLASS(PawnParity, "StealthGame.Pings.PIE")`)
- `Source/StealthTestHarness/Private/Pings/PawnParityMpNode.cpp` (Gauntlet node `StealthGame.Pings.MP.PawnParity`)
- `Source/StealthTestHarness/Private/Pings/PingLeakNegativeControls.cpp` (shared with story 013; behind `WITH_STEALTH_TEST_SEAMS`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

**Test: AC1 pawn byte parity**
- Given: a sender standing still, and in a second run walking a fixed path; a control without pings.
- When: it pings 5 times.
- Then: the pawn state as received by an opponent client (`FActionState`, movement, head-look, net-update times, relevancy) is byte-identical to the control; no `ForceNetUpdate`; no queue item or IIF command.
- Edge cases: fixed 60 Hz step and fixed seed.

**Test: AC2 payload shape**
- Given: the captured `ServerTeamPing` payload.
- When: inspect.
- Then: exactly one quantised `Point`; no pawn id or other field.
- Edge cases: the payload size is constant.

**Test: AC3 nightly raw-bunch parity**
- Given: `StealthGame.Pings.MP.PawnParity` with its injected-leak control.
- When: run nightly.
- Then: the control fails, the clean run passes.
- Edge cases: nightly-only (DEFERRED from per-PR runs).

QL-STORY-READY: ADEQUATE

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/Pings/PawnParityTest.cpp` and `PawnParityMpNode.cpp` OR a documented playtest

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/informant-pings-team-comms/story-006-server-team-ping-handler-and-host-parity.md`, `story-010-ia-ping-action-and-send-path.md`; cross-epic `production/epics/information-visibility-policy/story-001-spike-s1-multiprocess-raw-bunch-capture.md`, `production/epics/tuning-data-config/story-020-gauntlet-multiprocess-smoke.md`
- Unlocks: None
