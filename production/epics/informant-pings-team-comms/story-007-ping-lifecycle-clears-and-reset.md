# Story 007: Ping lifecycle: clears, reset and travel

> **Epic**: Informant Pings & Team Communication
> **Status**: Ready
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/informant-pings-team-comms.md` (PNG-R10; edge cases on round end and reconnect; AC-PNG-08)
**Requirement**: `TR-png-019`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0002: Server resolution queue, server clock and round-end staging (§8 round-end step, `ITeamPingReset`); binding events from ADR-0007 (Technical Setup 2026-10-25 and 2026-10-25b) and ADR-0005
**ADR Decision Summary**: Round clears the team ping parts in the round-end step only through the Core consumer-owned interface `ITeamPingReset::ClearAllTeamPings()`, implemented by `UTeamPingService` and registered weakly by the composition root; Round never names a Pings type. The service clears its view parts (never the token bucket) on Session's `OnRoundTravelStarted` and resets the bucket and parts at the Roster's `OnRosterReset`.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (ADR-0002; seamless travel facts are ADR-0005's HIGH-risk surface)
**Engine Notes**: `InvalidateAllBindings` is a travel step, not an event, so the clear binds to `OnRoundTravelStarted`. Registrations are weak (`TWeakInterfacePtr<ITeamPingReset>`) and cleared in `Deinitialize`; a dead registration is a fault-sink entry, never a crash (ADR-0007 §4). Seamless-travel behaviour itself is exercised by the session-travel epic's spikes; this story relies on their events, not on their conclusions.

**Control Manifest Rules (this layer)**:
- Required: Round clears the team ping parts in the round-end step only through `ITeamPingReset::ClearAllTeamPings()`, registered weakly by the composition root — source: ADR-0007, ADR-0002
- Required: the view parts (never the token bucket) clear at `OnRoundTravelStarted`; the bucket and parts reset at `OnRosterReset` — source: ADR-0007 (Technical Setup 2026-10-25b)
- Forbidden: Round must never name a Pings type (consumer-owned interface only) — source: ADR-0007

---

## Acceptance Criteria

*From GDD `design/gdd/informant-pings-team-comms.md` (PNG-R10, AC-PNG-08), scoped to this story:*

- [ ] **AC1** (AC-PNG-08 a, `[Integration]` `StealthGame.Pings.PIE.LifecycleNoReplay`): **GIVEN** live rings on a client **WHEN** the round ends by an objective win **THEN** every ring is hidden on that client's `Ended` entry tick and the server part is empty after the round-end step, cleared only through `ITeamPingReset::ClearAllTeamPings()` (Round names no Pings type).
- [ ] **AC2** (AC-PNG-08 b, c): **GIVEN** live rings **WHEN** the client disconnects and rejoins inside the grace, and separately when a new round starts, **THEN** no ring present before the rebuild reappears and a ping accepted after the rebuild shows normally.
- [ ] **AC3** (ADR-0007 Technical Setup, derived): **GIVEN** `OnRoundTravelStarted`, `OnRosterReset` and a dead weak `ITeamPingReset` registration **WHEN** each occurs **THEN** the view parts clear at travel start but the token bucket does not, bucket and parts both reset at `OnRosterReset`, and a dead registration is a fault-sink entry, not a crash.

---

## Implementation Notes

*Derived from ADR-0002 §8, ADR-0007 Technical Setup 2026-10-25 / 2026-10-25b and GDD PNG-R10:*

`UTeamPingService` implements `ITeamPingReset` (`UINTERFACE(meta=(CannotImplementInterfaceInBlueprint))`, in `StealthGameplay/Public/Server/Core/`, consumer-owned by Round) and subscribes to the Roster's `OnRosterReset` and Session's `OnRoundTravelStarted` (Feature -> Foundation, no inversion needed). Round's round-end step (R9 (a)-(h)) calls `ClearAllTeamPings()` through the weak registration made by the composition root in `InitGame`/`OnWorldBeginPlay` beside `ICrowdControl`. A sender who disconnects leaves its live slot to expire client-side (story 006 AC4); only the three events above clear slots. The client half of AC1 (hide on `Ended`) and the rebuild marking are implemented by the ring model (story 016); this story owns the end-to-end PIE test across both halves.

Round-boundary ordering: the clear must run in the round-end step, after the latch, so a ping RPC in flight when the latch lands is dropped at validation step 1 (story 005) and the cleared parts never see it.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 016: the model's hide on `Ended`, `LastShownSeq` rebuild marking and clear-on-travel (client side logic and its unit tests).
- Story 004: the bucket arithmetic and keying.
- round-match-framework epic: the round-end step sequencing and `ITeamPingReset` declaration site (`production/epics/round-match-framework/story-058-reset-clears.md`).

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Public/Server/Core/StealthTeamPingReset.h` (`ITeamPingReset`; only if the round-match-framework epic has not declared it)
- `Source/StealthGameplay/Private/Features/Pings/StealthTeamPingService.cpp` (event subscriptions, `ClearAllTeamPings`)
- `Source/StealthGame/Private/` composition root (weak registration)
- `Source/StealthTestHarness/Private/Pings/LifecycleNoReplayTest.cpp` (CQTest PIE, `NETWORK_TEST_CLASS(LifecycleNoReplay, "StealthGame.Pings.PIE")`)
- `Source/StealthGameplay/Private/Tests/Pings/PingResetBindingsTest.cpp` (class `FPingsResetBindingsTest`, name `StealthGame.Pings.ResetBindings`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

**Test: AC1 round-end clear (AC-PNG-08 a)**
- Given: live rings on a client; an objective-win round end.
- When: the round ends.
- Then: every ring hidden on that client's `Ended` entry tick; the server part empty after the round-end step; cleared only via `ITeamPingReset::ClearAllTeamPings()` and Round names no Pings type (static scan).
- Edge cases: a ring created on the same tick as `Ended` is hidden.

**Test: AC2 rejoin and new round do not replay (AC-PNG-08 b, c)**
- Given: live rings; (b) the client disconnects and rejoins inside the grace, (c) a new round starts.
- When: the HUD rebuilds.
- Then: no pre-rebuild ring reappears; a ping accepted after the rebuild shows normally.
- Edge cases: a ring still inside its 8 s lifetime at the rebuild is still not shown.

**Test: AC3 travel, roster reset, dead registration**
- Given: a bucket partly spent and live parts; one dead weak `ITeamPingReset` registration.
- When: `OnRoundTravelStarted`, then `OnRosterReset`.
- Then: travel clears parts but not the bucket; reset clears parts and refills the bucket; the dead registration writes a fault-sink entry and does not crash.
- Edge cases: travel with an empty part; reset with a dead registration first in the list.

QL-STORY-READY: ADEQUATE

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/Pings/LifecycleNoReplayTest.cpp` and `Source/StealthGameplay/Private/Tests/Pings/PingResetBindingsTest.cpp` OR a documented playtest

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/informant-pings-team-comms/story-004-ping-token-bucket-core.md`, `story-006-server-team-ping-handler-and-host-parity.md`; cross-epic `production/epics/round-match-framework/story-058-reset-clears.md`, `production/epics/participant-state-roster/story-022-reset-all-batch.md`, session-travel epic (`OnRoundTravelStarted`)
- Unlocks: `production/epics/informant-pings-team-comms/story-016-ring-model-and-age.md` (end-to-end), `story-013-opponent-sees-nothing.md`
