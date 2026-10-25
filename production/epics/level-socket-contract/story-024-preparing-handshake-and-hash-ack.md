# Story 024: Preparing handshake and hash acknowledgement

> **Epic**: Level Socket Contract
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/level-socket-contract.md`
**Requirement**: `TR-lsc-001`, `TR-lsc-002`
*(At write time: TR-lsc-001 = Layout service world subsystem with registered providers; Round talks only to it; TR-lsc-002 = Integer Layout Manifest; clients build with no RNG and recompute `LayoutHash`)*

**ADR Governing Implementation**: ADR-0014: Layout service, seeded generation and runtime navmesh
**ADR Decision Summary**: In `Preparing` Round calls `RequestLayout(Seed, TuningHash, RoundEpoch)`; on a pass the service freezes, replicates the Manifest, writes `LayoutAttempt` and `LayoutHash` into Session's `FRoundReadiness` and fires `OnLayoutReady`; a client acks (Session's single readiness ack) only after building its geometry and recomputing `LayoutHash`; a mismatch resends once then disconnects with `ParamsMismatch`.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: Multi-client PIE via the ADR-0008 harness. **GDD defects flagged (QL-STORY-READY):** GDD AC-5 lists the ack tuple as (Seed, LayoutAttempt, LayoutHash, TuningHash) and fires `OnLayoutReady` after the acks, whereas R4.5 gives (Seed, LayoutAttempt, LocalLayoutHash, ClientHash, PoolHash) and R4.4 / the States table fire `OnLayoutReady` on entering `Published`; this story follows R4.4/R4.5 and the AC-5 wording needs correcting by the GDD owner.

**Why this story**: It pins the order of the round-start handshake that every later system waits on. Read first: R4 items 2-5, States table, AC-5, AC-31; ADR-0014 8.3-8.4.

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: Clients build only from a complete `FLayoutManifest`; Round and every feature reach layout only through the service (ADR-0014)
- Forbidden: Never let Round and the Generator include each other's headers (ADR-0014)
- Guardrail: zero layout traffic in `Active`; <= 45 s worst case in `Preparing` (ADR-0014)

---

## Acceptance Criteria

*From GDD AC-5 and AC-31 (revised after QL-STORY-READY: tuple and order per R4.4/R4.5, observation window defined), scoped to this story:*

- [ ] GIVEN a 2-client PIE on the graybox, WHEN Round enters `Preparing`, THEN the order is `RequestLayout` -> `Published` (`OnLayoutReady` fires once, on entering `Published`) -> Manifest on both clients -> each client's ack sent only after recomputing `LayoutHash`, echoing (Seed, LayoutAttempt, LocalLayoutHash, ClientHash, PoolHash); Round leaves `Preparing` only after the acks (`Handshake.PreparingOrder`)
- [ ] GIVEN a service stub that never publishes and a stubbed Session with no timers firing, THEN over 60 s of injected time Round stays in `Preparing` (`Handshake.RoundWaitsForLayout`)
- [ ] GIVEN client B on a different `GeneratorKitVersion`, THEN Session resends once and disconnects B with `ParamsMismatch`, `OnClientLayoutFailed(LayoutMismatch)` logs both hashes, and client A continues to `Active` (`Network.HashMismatchDisconnect`)

---

## Implementation Notes

- Publish order on the server (`Selecting -> Published`): freeze sockets and selection, write `FLayoutManifest`, write `LayoutAttempt` and `LayoutHash` into `FRoundReadiness`, broadcast `OnLayoutReady(LayoutId, Seed)` once.
- The client ack is Session's single readiness ack (rule 11.6); the contract adds only the condition "after local build and hash recompute". Clients that never ack are bounded by Session's travel deadline (the contract has no ack timer).
- `IStealthLayoutClientView` (`IsLocalLayoutBuilt`, `GetLocalLayoutHash`, `OnLocalLayoutBuilt`) is the client-local query; graybox clients echo the `GrayboxLayoutHash` read from their own packaged blob header.
- Edge: an ack before the local rebuild is a failure; `OnLayoutReady` count is exactly 1; client B matching after the resend is accepted.
- Round is a stub until the Round epic lands (the `RoundWaitsForLayout` test uses a never-publishing service stub).

---

## Out of Scope

- Story 009: Manifest replication; story 013: geometry build; story 025: spawn gate; Session readiness internals (`production/epics/session-travel/EPIC.md`)

---

## Files

- `Source/StealthGameplay/Private/Core/StealthLayoutService.cpp` (publish order)
- `Source/StealthGameplay/Public/View/Core/StealthLayoutClientView.h`
- `Source/StealthTestHarness/Private/LayoutHandshakeTest.cpp`
- `Source/StealthGameplay/Private/Tests/Layout/HandshakeStubTest.cpp`

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE after revision).*

- **AC-1** `Handshake.PreparingOrder`
  - Given: a 2-client PIE on the graybox.
  - When: Round enters Preparing.
  - Then: order RequestLayout, Published (`OnLayoutReady` once on entering Published), Manifest on both clients, each ack after recomputing LayoutHash echoing (Seed, LayoutAttempt, LocalLayoutHash, ClientHash, PoolHash); Round leaves Preparing only after both acks.
  - Edge cases: ack before the local rebuild is a failure; `OnLayoutReady` count = 1.
- **AC-2** `Handshake.RoundWaitsForLayout`
  - Given: a never-publishing service stub; Session stub with no timers.
  - When: 60 s of injected time pass.
  - Then: Round stays in Preparing.
  - Edge cases: no state change at 60.0 s.
- **AC-3** `Network.HashMismatchDisconnect`
  - Given: client B on a different `GeneratorKitVersion`.
  - When: acks arrive.
  - Then: Session resends once, disconnects B with `ParamsMismatch`; `OnClientLayoutFailed(LayoutMismatch)` logs both hashes; A continues to Active.
  - Edge cases: B matches after the resend.

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/LayoutHandshakeTest.cpp` and `Source/StealthGameplay/Private/Tests/Layout/HandshakeStubTest.cpp` — `StealthGame.Layout.Handshake.PreparingOrder`, `.RoundWaitsForLayout`, `StealthGame.Layout.Network.HashMismatchDisconnect` (CQTest PIE naming per ADR-0008 if used) must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/level-socket-contract/story-009-manifest-chunked-replication-late-join.md`, `production/epics/level-socket-contract/story-013-client-geometry-builder-and-decoration.md`, `production/epics/level-socket-contract/story-003-attempt-loop-timeout-budget-reseed.md`
- Cross-epic: `production/epics/session-travel/EPIC.md` (`FRoundReadiness`, readiness ack, `ParamsMismatch`), `production/epics/round-match-framework/EPIC.md` (stub until it exists)
- Unlocks: `production/epics/level-socket-contract/story-025-spawn-gate-and-no-view-before-active.md`
