# Story 033: PoolHash rides in Session's single round acknowledgement, after warm-up

> **Epic**: Character Pool & Appearance
> **Status**: Blocked — BLOCKED: spike CPA-V5 (story 029) has not reported; whether `PoolHash` is byte-identical across cooked client, cooked host and PIE decides whether a mismatch check can be trusted at all.
> **Layer**: Core
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/character-pool-appearance.md`
**Requirement**: `TR-cpa-005`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-cpa-005 = All pool assets resident and precached before leaving `Preparing`; no first-draw load.)*

**ADR Governing Implementation**: ADR-0012: Character pool data and appearance allocation (acknowledgement per ADR-0005)
**ADR Decision Summary**: The client computes `PoolHash` locally and sends it as one field of Session's single round acknowledgement beside `ClientHash`, only once `OnPoolWarm` has fired; a mismatch follows Session's resend-once then `ParamsMismatch` path; the listen host's local acknowledgement (no RPC) also waits for its own `OnPoolWarm`; CPA adds no RPC, timer, kick rule or Reason code.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0012)
**Engine Notes**: The acknowledgement transport is ADR-0005's; Session is not coded yet, so tests run against an adapter fake until it lands (the integration with the real Session is then re-run).

**Why this story**: CP-18 and AC-CP-19: the pool-version check and the warm-up wait travel on the one acknowledgement; no CPA-owned message exists. Read GDD CP-18, AC-CP-19 and the session-travel epic's ack stories (`story-042-ack-arrival-order-independent.md`, `story-043-ack-mismatch-resend-once.md`).

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: Clients and the listen host warm all `A` pool rows before Session's acknowledgement; `PoolHash` is one field of the single acknowledgement (ADR-0012, ADR-0005)
- Forbidden: No CPA-owned RPC, timer or Reason code (`LINT-CPA-NoCpaRpcOrTimer`)
- Guardrail: n/a

---

## Acceptance Criteria

*From GDD AC-CP-19 and CP-18:*

- [ ] **AC-1** GIVEN a client with a wrong `PoolHash`, THEN the `PoolHash` field of Session's single round acknowledgement mismatches, Session resends once then disconnects it with `ParamsMismatch` (a matching hash on the resend is accepted).
- [ ] **AC-2** GIVEN a client that withholds its acknowledgement until `OnPoolWarm`, THEN it is not marked travel-complete before warm-up; the listen host's local acknowledgement waits for its own `OnPoolWarm`.
- [ ] **AC-3** GIVEN `LINT-CPA-NoCpaRpcOrTimer`, THEN no CPA-owned `UFUNCTION(Server/Client)` and no `FTimerHandle` exists in the character-pool code, and a seeded RPC fails the lint.

---

## Implementation Notes

- The `PoolHash` field is a change request on Session's ack struct (rule 11.6); coordinate with session-travel (`story-041-round-params-replicated-atomically.md`, `story-043-ack-mismatch-resend-once.md`).
- The timeout itself (`TravelLoadTimeoutSec`) is Session's test, not this story's.
- Test `StealthGame.CharacterPool.Net.PoolHashInSessionAck`; multi-client form per ADR-0008 naming (`StealthGame.CharacterPool.PIE.<Scenario>.<Method>`).

**Prototype disposition**: none.

---

## Out of Scope

- Session's resend and `ParamsMismatch` implementation (session-travel epic); Story 032: warm-up

---

## Files

- `Source/StealthGameplay/Private/View/CharacterPool/PoolAckContribution.cpp`
- `Source/StealthGameplay/Private/Tests/CharacterPool/PoolHashInSessionAckTest.cpp`

---

## QA Test Cases

*QL-STORY-READY: ADEQUATE (qa-lead; Session adapter is a fake until Session exists).*

**Test: wrong hash**
- Given: a client with a different `PoolHash`.
- When: the round acknowledgement is sent.
- Then: the `PoolHash` field mismatches, Session resends once, then disconnects with `ParamsMismatch`.
- Edge cases: a matching hash on the resend.

**Test: ack after warm-up**
- Given: a client withholding the acknowledgement until `OnPoolWarm`, and the listen host.
- When: travel completes.
- Then: the client is not marked travel-complete before warm-up; the host's local acknowledgement waits for its own `OnPoolWarm`.
- Edge cases: warm-up never completes (the Session timeout is Session's test).

**Test: no CPA RPC or timer**
- Given: `LINT-CPA-NoCpaRpcOrTimer`.
- When: it scans.
- Then: zero CPA-owned server/client UFUNCTIONs and zero `FTimerHandle`; a seeded RPC fails.
- Edge cases: none.

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthGameplay/Private/Tests/CharacterPool/PoolHashInSessionAckTest.cpp` — automation test `StealthGame.CharacterPool.Net.PoolHashInSessionAck` (OR documented playtest)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/character-pool-appearance/story-029-spike-cpa-v5-poolhash-cross-process.md`, `production/epics/character-pool-appearance/story-032-pool-warmup-subsystem-and-rig.md`, `production/epics/session-travel/story-043-ack-mismatch-resend-once.md`
- Unlocks: None
