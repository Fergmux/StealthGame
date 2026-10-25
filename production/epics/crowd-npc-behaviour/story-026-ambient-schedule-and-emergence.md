# Story 026: Ambient schedule as queue items and witness-free emergence

> **Epic**: Crowd & NPC Behaviour
> **Status**: Ready
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/crowd-npc-behaviour.md`
**Requirement**: `TR-crowd-008`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-crowd-008 = Ambient emergence and replacement via shared sockets and DrawReplacement.)*

**ADR Governing Implementation**: ADR-0016: Crowd AI architecture
**Also relevant**: ADR-0002 (item type `TimerFired(CrowdSchedule)`, generic `FTimerPayload`, post-latch rules), ADR-0006 (`AmbientSchedule` domain 9, counter-as-Attempt), ADR-0014 (`RequestEmergence`, sockets, clear check)
**ADR Decision Summary**: Ambient exits, entries, replacements, retries and the exile-exit slot are interacting state changes, so they are scheduled as `TimerFired(CrowdSchedule)` items with the round's epoch token and resolved by the Director's resolver inside the drain. Inside the slot the resolver runs the LSC clear check and calls `RequestEmergence` and the pawn factory (`SpawnSharedPawn(..., Ai, ...)` with `DrawReplacement()` in the same tick) or absorbs the pawn (`RequestEmergence(AmbientExit)` + `RequestTeardown`). A failed clear check reschedules at +`EmergenceRetrySec`; `Rejected_Latched` and `Rejected_StaleEpoch` are never rescheduled.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: Items use ADR-0002's generic Foundation `FTimerPayload` (`Subject` = pawn or participant, `Generation` = schedule generation, `Seq` = event ordinal, `Phase` = event kind); Crowd declares no payload type. The `CrowdSchedule` kind and the AI-timing definition are applied in ADR-0002 (checked 2026-10-25), so this story is not blocked by an open amendment.

**Why this story**: Ambient cycles are what keep the NPC count and the street feeling alive without ever letting a player see a pawn appear. Their rate has a hard lower bound from the LSC floor (0.5 per socket per minute each way). Read GDD CNB-R9 items 1 and 3, F3 and AC-CNB-07, AC-CNB-09.

**Control Manifest Rules (Feature layer, manifest 2026-10-25e)**:
- Required: ambient schedule events are `TimerFired(CrowdSchedule)` queue items; emergence spawns go through `RequestEmergence` and `SpawnSharedPawn`; absorptions through `RequestTeardown`; a cancelled schedule item resolves `Voided(Revalidation)` by ledger re-check; schedule draws use `AmbientSchedule` counter-as-Attempt (ADR-0016)
- Forbidden: crowd timers on `FTimerManager`; spawning, hiding or destroying a pawn by any other path; rescheduling a `Rejected_Latched` or `Rejected_StaleEpoch` item (ADR-0016)
- Guardrail: at most one `SpawnSharedPawn` per frame inside the spawn budget; schedule work in `Crowd.Director`

---

## Acceptance Criteria

*From GDD `design/gdd/crowd-npc-behaviour.md` AC-CNB-07, AC-CNB-09 and CNB-R9, scoped to this story:*

- [ ] (AC-CNB-07) GIVEN a 10-minute simulation with 20 scripted kills of mixed victim kinds (2 per minute, under the 2.4/min exit-cancel capacity of F3), WHEN spawns and removals are counted, THEN 100 % of mid-round NPC spawns and removals go through `RequestEmergence` and the SCL factory, `DrawReplacement` is called in each spawn tick, the measured entry and exit rates averaged over sockets are each at least 0.5 per socket per minute, and after every event the CPA ledger invariants hold.
- [ ] Ambient cycles are a Poisson process: over at least 1,000 gaps a KS test at p > 0.01 against Exp(mean 80 s / 13 sockets) and chi-square p > 0.01 for uniform socket choice; items are `TimerFired(CrowdSchedule)` with the round epoch token and none are on `FTimerManager`; a failed clear check retries at +`EmergenceRetrySec`; `Rejected_Latched` and `Rejected_StaleEpoch` items are never rescheduled.
- [ ] (AC-CNB-09) GIVEN 10 min with 10 bot viewers walking random routes, WHEN every emergence and absorption is checked against every pawn's eye samples (LSC R5.2), THEN 0 occur within any pawn's line of sight, and watched exits give up after `CrowdExitGiveUpSec` and walk back out.

---

## Implementation Notes

*Derived from ADR-0016 section 6 and GDD CNB-R9:*

- Cycles: exit-first, one per `CrowdAmbientCycleIntervalSec` (80 s) per socket, i.e. 13 x 60 / 80 = 9.75 per minute city-wide, as a Poisson process (exponential gaps of mean 1/lambda city-wide, each slot to a socket drawn uniformly); the F3 replacement-delay model assumes exactly this. A socket in its `EmergenceSocketCooldownSec` takes the 1 s retry of LSC R5.
- An NPC chosen uniformly among eligible unbound NPCs within 40 m of a socket path takes "leave by the alley" as its next leg, walks in, and is absorbed (CPA card becomes `NpcVacant`); the entry follows after the exit-to-entry gap U(`CrowdCycleGapMinSec`, `CrowdCycleGapMaxSec`) = U(6, 20) s.
- Schedule draws: one `GetServerSubSeed(AmbientSchedule, counter)` per schedule event with a short secret stream for the draws inside it.
- Witness test: eye samples from every pawn's camera origin; a watched exit gives up after `CrowdExitGiveUpSec` (8 s) and walks back out.
- Emergence uses the `Emerging` state (story 012 skeleton) until past the socket's witness set.

**Prototype disposition** (`production/first-playable-report.md`, `Source/StealthGame`): Replaces `ScheduleNpcReplacement` in `Source/StealthGame/Private/StealthGameMode.cpp` (engine-timer replacement spawns). Deleted in story 039 with approval.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 027: kill-driven vacancy, replacement delay and exit cancellation
- Story 029: the exile exit slot (`bExitAtNextScheduledSlot`)
- The layout-service epoch (`production/epics/level-socket-contract/EPIC.md`): `RequestEmergence`, sockets and clear checks
- The character pool epoch (`production/epics/character-pool-appearance/EPIC.md`): `DrawReplacement` and the card ledger

---

## Files

- `Source/StealthGameplay/Features/Crowd/Private/CrowdAmbientSchedule.cpp`
- `Source/StealthGameplay/Features/Crowd/Public/Server/CrowdAmbientSchedule.h`
- `Source/StealthGameplay/Private/Tests/Crowd/CrowdAmbientScheduleTest.cpp`
- `Source/StealthTestHarness/Private/CrowdEmergenceWitnessTest.cpp`

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY, 2026-10-25). AC-2 revised after the GAPS verdict (KS and chi-square tests added).*

- **AC-1**: `StealthGame.Crowd.SpawnPathAndLedger`
  - Given: 10 min with 20 scripted kills (2 per minute)
  - When: spawns and removals are counted
  - Then: 100 % through `RequestEmergence` and the SCL factory; `DrawReplacement` called each spawn tick; entry and exit rates averaged over sockets each at least 0.5 per socket per minute; CPA ledger invariants hold after every event
  - Edge cases: spawn and absorption in the same tick
- **AC-2**: `StealthGame.Crowd.AmbientScheduleShape`
  - Given: at least 1,000 schedule gaps with fixed seeds
  - When: gaps, sockets and item kinds are logged
  - Then: KS p > 0.01 against Exp(80/13 s); chi-square p > 0.01 for uniform sockets; all items `TimerFired(CrowdSchedule)` with the round epoch token; none on `FTimerManager`; failed clear check retries at +`EmergenceRetrySec`; latched and stale-epoch items never rescheduled
  - Edge cases: socket in cooldown takes the 1 s retry
- **AC-3**: `StealthGame.Crowd.EmergenceNeverWitnessed`
  - Given: 10 min with 10 bot viewers walking random routes
  - When: each emergence and absorption is checked against every pawn's eye samples
  - Then: 0 within any pawn's line of sight; watched exits give up after `CrowdExitGiveUpSec` and walk back out
  - Edge cases: a viewer rounds the corner during the exit walk

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthGameplay/Private/Tests/Crowd/CrowdAmbientScheduleTest.cpp` and `Source/StealthTestHarness/Private/CrowdEmergenceWitnessTest.cpp` — must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/crowd-npc-behaviour/story-004-director-core-ledger-and-phase-gating.md`, `production/epics/crowd-npc-behaviour/story-017-density-and-start-placement.md`
- Unlocks: `production/epics/crowd-npc-behaviour/story-027-kill-vacancy-and-replacement.md`, `production/epics/crowd-npc-behaviour/story-029-release-to-crowd.md`, `production/epics/crowd-npc-behaviour/story-038-performance-pass-50-60-character.md`, `production/epics/crowd-npc-behaviour/story-039-retire-prototype-crowd.md`
