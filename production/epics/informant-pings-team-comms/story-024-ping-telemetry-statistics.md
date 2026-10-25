# Story 024: Ping telemetry statistics

> **Epic**: Informant Pings & Team Communication
> **Status**: Ready
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/informant-pings-team-comms.md` (PNG-R5 drop statistics, OQ6; AC-PNG-13 feed)
**Requirement**: `TR-png-023`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0023: Playtest telemetry hooks (Technical Setup 2026-10-25: Pings statistics and `PingAccepted` rows)
**ADR Decision Summary**: Telemetry lives in a separate section of the Round Event Log store, written by kind-blind probes and detectors at Feature call sites and exported once after termination by one off-game-thread writer; ping statistics (accepted, and drops by reason split by role class) and `PingAccepted` rows go only to the server telemetry section, never to clients, never per person in a client-visible form, never through `UE_LOG`.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: LOW (ADR-0023; plain C++ counters and a file export)
**Engine Notes**: The section is compiled out of Shipping and is a null object when `-StealthTelemetry=0`; its storage is separate and never evicts Event Log ring slots (ADR-0023 §1, §4). The Shipping compile-out check needs a Shipping build and is deferred to the build-verification story of the owning epic.

**Control Manifest Rules (this layer)**:
- Required: ping statistics (accepted, and drops by reason `Phase`/`Latch`/`State`/`Pawn`/`Concealed`/`Range`/`NonFinite`/`Bucket`, split by role class) and `PingAccepted` rows go only to the server telemetry section — source: ADR-0023
- Forbidden: Never make a probe conditional on controller kind, team or participant state; never export ping data per person; never record telemetry client-side; never pass ping data to `UE_LOG` or any log macro — source: ADR-0023, ADR-0007

---

## Acceptance Criteria

*From GDD `design/gdd/informant-pings-team-comms.md` (PNG-R5, OQ6, AC-PNG-13 feed) and ADR-0023; AC2 and AC3 revised at QL-STORY-READY, scoped to this story:*

- [ ] **AC1** (TR-png-023): **GIVEN** accepted and dropped pings **WHEN** handled **THEN** server-only counters record accepted pings and drops by reason (Phase, Latch, State, Pawn, Concealed, Range, NonFinite, Bucket) split by role class, and `PingAccepted` rows (placement, sender role class) go only to the telemetry section of the Event Log store; a dropped ping creates no `PingAccepted` row.
- [ ] **AC2** (ADR-0023, no client reach): **GIVEN** the build and a client session **WHEN** inspected **THEN** ping statistics reach no client (no replicated field, RPC or client-visible log), pass through no `UE_LOG` or log-macro argument, no exported or client-visible row field carries `ParticipantId`, `SenderHandle` or `DisplayName`, with `-StealthTelemetry=0` the section is a null object, and the telemetry storage never evicts Event Log ring slots under a flood; the Shipping compile-out check runs on a Shipping build (DEFERRED).
- [ ] **AC3** (ADR-0023 kind-blind probes): **GIVEN** the same ping fixture run from a host controller, a remote client and a bot controller, on team 0 and on team 1 **WHEN** the counters and `PingAccepted` rows are compared **THEN** the deltas are identical in every combination, and the role-class split changes only when the Roster `State` changes.

---

## Implementation Notes

*Derived from ADR-0023 §2, §3, §4 and Technical Setup 2026-10-25:*

Add the probe calls at the single acceptance and drop points of `FTeamPingCore` (story 005) and forward through the telemetry sink interface owned by the telemetry/Event Log work (round-match-framework epic); the Pings code never names an Event Log type in a log macro (ADR-0007 lint (i)). Role class is the Roster `State` (Active or Informant) read server-side, never controller kind. `PingAccepted` carries placement (the point, as needed for OQ6) and the role class, never a participant id; the analysis in story 025 reads only this export. Memory: part of the 1.6 MB telemetry budget (ADR-0023).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 005: the validation and its decision points (this story hooks them).
- Story 025: the offline analysis (follow-up share and walker coverage).
- round-match-framework / ADR-0023 owner: the section, the sink, the export writer and the Shipping compile-out.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Private/Features/Pings/StealthTeamPingCore.cpp` (probe calls)
- `Source/StealthGameplay/Private/Features/Pings/StealthTeamPingTelemetry.h` and `StealthTeamPingTelemetry.cpp` (probe adapter)
- `Source/StealthGameplay/Private/Tests/Pings/PingTelemetryStatsTest.cpp` (class `FPingsTelemetryStatsTest`, name `StealthGame.Pings.TelemetryStats`)
- `Source/StealthTestHarness/Private/Pings/TelemetryKindBlindTest.cpp` (CQTest PIE, `NETWORK_TEST_CLASS(TelemetryKindBlind, "StealthGame.Pings.PIE")`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

**Test: AC1 counters and rows**
- Given: accepted and dropped pings of every reason, from Active and Informant senders.
- When: handled.
- Then: server-only counters for accepted and for drops by reason (Phase, Latch, State, Pawn, Concealed, Range, NonFinite, Bucket) split by role class; `PingAccepted` rows (placement, role class) only in the Event Log telemetry section.
- Edge cases: a dropped ping creates no `PingAccepted` row.

**Test: AC2 no client reach (Shipping part DEFERRED)**
- Given: the build and a client session.
- When: scan replicated fields, RPCs and client-visible logs; scan call sites for `UE_LOG` and log-macro arguments; run with `-StealthTelemetry=0`; fill the Event Log ring.
- Then: nothing reaches a client; no log macro argument carries ping statistics; no `ParticipantId`/`SenderHandle`/`DisplayName` in client-visible or exported rows; with the flag off the section is a null object; separate storage never evicts Event Log ring slots.
- Edge cases: a telemetry flood does not evict Event Log slots.

**Test: AC3 probe neutrality**
- Given: the fixture run from a host controller, a remote client and a bot controller, on team 0 and team 1.
- When: run in all six combinations.
- Then: counters identical across controller kind and team.
- Edge cases: a Roster `State` change moves counts between role classes only.

QL-STORY-READY: GAPS (revised) — "per-person" was defined as the id/handle/name list and the probe-neutrality review became a six-combination test, on the qa-lead's proposals (the author should confirm the id/handle/name list is the intended definition).

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthGameplay/Private/Tests/Pings/PingTelemetryStatsTest.cpp` and `Source/StealthTestHarness/Private/Pings/TelemetryKindBlindTest.cpp` OR a documented playtest

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/informant-pings-team-comms/story-005-server-validation-core.md`, `story-006-server-team-ping-handler-and-host-parity.md`; cross-epic round-match-framework epic (Event Log store, telemetry section and sink)
- Unlocks: `production/epics/informant-pings-team-comms/story-025-playtest-metrics-follow-up-and-walkers.md`
