# Story 055: The clock exchange RPCs are owner-only, timestamps-only and under 50 B/s

> **Epic**: Information Visibility Policy
> **Status**: Blocked — BLOCKED: needs the ServerClockPing/ClientClockPong RPC pair and EstimatedServerNow built by round-match-framework (ADR-0002 Exit A)
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

Client presentation schedules from `EstimatedServerNow()`, which is computed from an owner-only clock exchange (`ServerClockPing` / `ClientClockPong`) on `AStealthPlayerController` (ADR-0002 section 9). That RPC pair is new wire surface, so it needs a Pillar-1 audit: it may live only on the PlayerController, reach only the pinging owner, carry only timestamps, have a row under the existing "ConnectionState, ping, join time" Owner fact, and stay under 50 B/s per client. **This criterion is derived: the GDD has no acceptance criterion for `TR-ivp-012`; the story states it so the requirement is not left without a test.**

**GDD**: `design/gdd/information-visibility-policy.md`
**Requirement**: `TR-ivp-012` (Owner-only clock-exchange RPC pair carrying timestamps only, under the existing owner connection/ping fact; no other audience learns timing (R3))
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0002: Resolution queue, round-end staging (section 9, server clock and the client server-time estimator); ADR-0001: Crowd identity rules (section 5, Owner carrier); ADR-0008: Test and automation strategy for leak and parity tests (sections 1 T2, 6)
**ADR Decision Summary**: The client sends `ServerClockPing(T1)` (unreliable, every 0.5 s for the first 5 s after a controller begins play or travel, then every 2 s) and the server answers `ClientClockPong(T1, T2, T3)` (unreliable, owner only); the server rate-limits pings (at most 4 per second per connection). The pair carries timestamps only, with Owner audience under IVP's existing connection/ping fact; bandwidth is under 50 B/s per client after warm-up.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: ADR-0002 E3 (the engine's client server-time has no RTT compensation and stores a `float` delta) [V]; E5 (unreliable owner-only client/server RPCs on an `APlayerController` are delivered in the next dispatch with no ordering guarantee relative to replicated properties) [D], ordering [U] and not relied on; E8 (server RPC bodies run before `TimeSeconds` advances, so a world-time stamp in an RPC body is biased by up to one frame) [V]. Per-RPC byte attribution on a connection needs the capture mechanism chosen by story-001 [U, S1].

**Control Manifest Rules (this layer)**:
- Required: **Client presentation schedules only from `EstimatedServerNow()`; the clock-exchange RPCs carry only timestamps** — source: ADR-0002
- Forbidden: **Never send gameplay data in the clock-exchange RPCs** — source: ADR-0002
- Required: **Team-audience data travels only in a `UTeamViewComponentBase` subclass on the receiving teammate's own PlayerController; owner-audience data only on the owner's PlayerController** — source: ADR-0001
- Guardrail: **Clock exchange**: < 50 B/s per client after warm-up — source: ADR-0002

---

## Acceptance Criteria

*Derived criterion (the GDD has no acceptance criterion for TR-ivp-012; this one is stated by the story, QL-STORY-READY clarification applied):*

- [ ] **Derived (no GDD AC for TR-ivp-012):** GIVEN a listen server and 2 clients (three connections), WHEN each client runs the clock exchange for 60 s, THEN `ServerClockPing` and `ClientClockPong` exist only as RPCs on `AStealthPlayerController` (the R11 RPC-set audit finds none on any pawn and no multicast), each pong is delivered only to the pinging owner connection, both carry only timestamps (doubles T1..T3) and no gameplay field, the pair has an allowlist/R3 row under "ConnectionState, ping, join time" (Owner), and measured bandwidth is below 50 B/s per client after warm-up. Clarification (story-level, QL-STORY-READY): warm-up = the first 10 s of the 60 s window excluded; bandwidth = owner-connection bytes attributable to `ServerClockPing`/`ClientClockPong` only; the criterion is explicitly derived (no GDD AC for TR-ivp-012).

---

## Implementation Notes

*Derived from ADR-0002 section 9 and Key Interfaces, ADR-0001 section 5 and ADR-0008 sections 1 and 6:*

- T2 test `NETWORK_TEST_CLASS(ClockExchangeOwnerOnlyAudit, "StealthGame.Visibility.PIE")` (full name `StealthGame.Visibility.PIE.ClockExchangeOwnerOnlyAudit.<Method>`) in `Source/StealthTestHarness/Private/Tests/PIE/`, listen server plus 2 clients, `WithGameMode(AStealthGameMode::StaticClass())`, fixture map loaded in a first step, legacy replication system asserted and recorded. Run the exchange for 60 s of server time (frames or server time bound, wall clock only as a failure ceiling).
- RPC-set audit: reflect over every replicated class and assert `ServerClockPing(double T1)` (Server, Unreliable, rate-limited) and `ClientClockPong(double T1, double T2, double T3)` (Client, Unreliable) exist only on `AStealthPlayerController`, no pawn declares either, and neither is a multicast. Reuse the R11 RPC-set audit from story-015 (it already checks the absence of multicast RPCs on the pawn).
- Audience check: on each of the two non-pinging connections, record that no pong arrives (the QA edge case: check the pong on the two non-pinging connections as well as the owner). A pong is delivered only to the pinging owner connection (PlayerControllers are relevant only to their owner, ADR-0001 E6).
- Payload check: both RPCs carry only the double timestamps (T1 for the ping; T1, T2, T3 for the pong) and no gameplay field.
- Allowlist / R3 row: the pair needs a row under the existing "ConnectionState, ping, join time" Owner fact in the R3 matrix and the allowlist (story-013's file). ADR-0002 section 9 records that an IVP change request adds the RPC pair to the leak catalogue; raise that change request as part of this story if it is still open.
- Bandwidth: measure owner-connection bytes attributable only to `ServerClockPing` and `ClientClockPong` after the first 10 s warm-up of the 60 s window; the result must be below 50 B/s per client. Attributing bytes to specific RPCs on a connection needs the capture mechanism from story-001; if the in-process T2 capture cannot attribute them, run this check as a T3 scenario under `Source/StealthTestHarness/Private/Scenarios/` instead and record the choice.
- The accuracy budget (`|EstimatedServerNow - ServerNow|` p95 <= 15 ms, etc.) is round-match-framework's own acceptance under ADR-0002 section 9, not part of this story.
- Prototype: not applicable (no clock exchange exists). Replaces: none (new).
- Cross-epic needs: round-match-framework epic (the RPC pair and `EstimatedServerNow`, ADR-0002 Exit A), session-travel epic (`AStealthPlayerController`), tuning-data-config Exit A (T2 harness).
- Unblocks when: round-match-framework delivers the RPC pair and `EstimatedServerNow` on `AStealthPlayerController`.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- story-001: choosing the capture mechanism that allows per-RPC byte attribution
- story-015: the replicated-property and RPC-set audit machinery
- story-013: the allowlist file content (this story adds one row)
- round-match-framework epic: implementing the RPC pair, the estimator and its accuracy budget
- story-021 and story-053: team-audience carriers (the clock pair is Owner audience)

---

## Files

*Planned (module folders are created by tuning-data-config Exit A and do not exist yet):*

- `Source/StealthTestHarness/Private/Tests/PIE/ClockExchangeOwnerOnlyAuditTest.cpp` (T2 test)
- `Source/StealthFoundation/Private/Tests/Data/ReplicationAllowlist.json` (one added row for the RPC pair, with both owners' sign-off)

---

## QA Test Cases

- **AC-1**: Derived criterion, clock exchange owner-only audit (T2, derived criterion)
  - Given: a listen server and 2 clients (three connections).
  - When: each client runs the clock exchange for 60 s.
  - Then: `ServerClockPing` and `ClientClockPong` exist only as RPCs on `AStealthPlayerController` (the R11 RPC-set audit finds none on any pawn and no multicast); each pong is delivered only to the pinging owner connection; both carry only timestamps (doubles T1..T3); the pair has an allowlist/R3 row under "ConnectionState, ping, join time" (Owner); owner-connection bytes for these two RPCs only, after the first 10 s of warm-up, are below 50 B/s per client; the story states the criterion is derived (no GDD AC for TR-ivp-012).
  - Edge cases: attributing bytes to specific RPCs on a connection needs the capture mechanism from story-001; check the pong on the two non-pinging connections.

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: T2 test under `Source/StealthTestHarness/Private/Tests/PIE/` (or a T3 scenario if byte attribution needs it), OR a documented playtest/spike report

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/information-visibility-policy/story-001-spike-s1-multiprocess-raw-bunch-capture.md`, `production/epics/information-visibility-policy/story-015-replicated-property-audit.md`; round-match-framework epic (`ServerClockPing`/`ClientClockPong` and `EstimatedServerNow`, ADR-0002 Exit A); session-travel epic (`AStealthPlayerController`); tuning-data-config Exit A (T2 harness)
- Unlocks: None
