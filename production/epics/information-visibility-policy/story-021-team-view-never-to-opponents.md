# Story 021: No opponent connection ever receives a C-TeamView entry, PawnRef, IsConcealed flag or fallback position of the other team

> **Epic**: Information Visibility Policy
> **Status**: Blocked — BLOCKED: spike story-003 (rejoin/PawnRecovery) has not reported; needs C-TeamView components (participant-state-roster), the takeover path (npc-kill-penalty-informants) and rejoin (session-travel)
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

The locked rule is that teammate markers exist only on teammates' clients and enemies must not be able to obtain them, enforced by the absence of data rather than a client-side filter. That works only if team data travels exclusively in C-TeamView components on the receiving teammate's own PlayerController, whose relevancy is owner-only. This story proves it end to end: in a 2v2 with an instrumented per-connection replication log, through an elimination, a takeover, a disconnect and a rejoin, no opponent connection (Active or Informant) ever receives a team entry, a `PawnRef`, an `IsConcealed` flag or a fallback position of the other team. It is the integration proof of the ADR-0001 carrier model and the Pillar 1 guarantee for marker data.

**GDD**: `design/gdd/information-visibility-policy.md`
**Requirement**: `TR-ivp-002` (One carrier kind per audience); also `TR-ivp-009` (Teammate fallback position feed on C-TeamView)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0001: Crowd identity rules (section 5, carrier model); also ADR-0003: Replication, relevancy and the concealment cull hybrid (sections 6, 7); ADR-0008: Test and automation strategy for leak and parity tests (sections 1, 3)
**ADR Decision Summary**: Team-audience data travels only in a `UTeamViewComponentBase` subclass on each teammate's own `AStealthPlayerController`; the PlayerController is relevant only to its owner, so every part reaches exactly one connection, and an opponent's client holds no entry and no feed for another team (enforced by absence of data, not by a client filter).
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: ADR-0001 E6 [V] (`AController` sets `bOnlyRelevantToOwner = true`, so a PlayerController and its components reach exactly one connection); E11 [V] (`bReplicateUsingRegisteredSubObjectList` and the registered sub-object list host the C-TeamView parts). The rejoin and `PawnRecovery` legs depend on S14's open condition (story-003) and on seamless travel (multi-process only, `net.AllowPIESeamlessTravel` is off in single-process PIE). ADR-0003 section 7: the fallback feed is built by the Visibility Policy and carried on C-TeamView to teammate connections only; [U] `UChannel::OpenAcked` is checked in story-032.

**Control Manifest Rules (this layer)**:
- Required: **Team-audience data travels only in a `UTeamViewComponentBase` subclass on the receiving teammate's own PlayerController; owner-audience data only on the owner's PlayerController** — source: ADR-0001
- Forbidden: **Never use a team-filtered actor, a per-team replica actor or a team relevancy rule** — source: ADR-0001
- Forbidden: **Never add a teammate connection field other than the one `bReconnecting` bit (no ping, time, reason or address), and never send it to a non-teammate** — source: ADR-0001, ADR-0005 (user decision 2026-10-25)
- Guardrail: **Identity policy**: C-TeamView pushes change-only, ≤ 1 per frame, within the 50 KB/s per-client budget — source: ADR-0001

---

## Acceptance Criteria

*From GDD `design/gdd/information-visibility-policy.md`, scoped to this story:*

- [ ] **AC-IVP-19** [PIE-3] `StealthGame.Visibility.TeamViewNeverToOpponents`: GIVEN a 2v2 with an instrumented replication log through an elimination, a takeover, a disconnect and a rejoin, WHEN each connection's received data is inspected, THEN no opponent connection (Active or Informant) ever receives a C-TeamView entry, `PawnRef`, `IsConcealed` flag or fallback position of the other team. (R4, R8, R9a)

---

## Implementation Notes

*Derived from ADR-0001 section 5 and ADR-0003 sections 6 and 7, ADR-0008 sections 1, 3:*

- **What to build.** A 2v2 scenario with an instrumented per-connection replication log (what each connection actually received: component properties on each PlayerController, pawn changes, GameState structs). Drive an elimination, a takeover, a disconnect and a rejoin; inspect every connection's log, and assert that no opponent connection, including an informant opponent and the window around the rejoin, ever received a C-TeamView entry, a `PawnRef`, an `IsConcealed` flag or a fallback position belonging to the other team. Check informant opponents explicitly.
- **Tiers.** In-process T2 (`NETWORK_TEST_CLASS(TeamViewNeverToOpponents, "StealthGame.Visibility.PIE")`, `WithGameMode(AStealthGameMode)`) for elimination, takeover and disconnect; the rejoin leg needs the T3 path (`StealthGame.Visibility.MP.TeamViewNeverToOpponents`), since T2 cannot represent seamless travel.
- **Negative control (added by the story, ADR-0008 section 3).** An injected opponent receipt of a team entry (a harness-only variant that delivers one C-TeamView entry to an opponent connection) must be detected by the same inspection; the suite passes only when the control is flagged. The criterion text stays the GDD's.
- **Parts in scope.** Every `UTeamViewComponentBase` subclass present when the story runs: the Roster part (`URosterTeamViewComponent`), the Visibility Policy's `UTeamPositionFeedComponent` (story-032) and any feature part that exists (`UObjectiveTeamViewComponent`, `UTeamPingViewComponent`); later parts are added by their epics under the same inspection.
- **Prototype disposition.** Replaced, never promoted: `AStealthHUD::DrawTeammateMarkers` iterating `TActorIterator<AStealthCharacter>` and `AStealthCharacter::IsTeammateOfLocalViewer()` (`Source/StealthGame/Private/StealthHUD.cpp:171-174`) derive teams on the client from world iteration; they are replaced by the carrier model and the Teammate Markers epic.
- **Spike gates.** story-003 (S14 rejoin and `PawnRecovery`). story-019 (canary harness) and story-032 (fallback feed) are dependencies.
- **Unblocks when.** story-003 reports; participant-state-roster delivers the C-TeamView components, npc-kill-penalty-informants the takeover path, session-travel the rejoin.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- story-032: the fallback feed and its byte budget
- story-033: teammate marker behaviour on a concealed ally
- story-022: public structs carrying no per-team value
- story-053: ping, reconnecting and round-result scope additions
- story-003: S14 spike on rejoin and `PawnRecovery` possession
- teammate-markers epic: marker widgets

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthTestHarness/Private/Tests/PIE/TeamViewNeverToOpponentsTest.cpp` (`NETWORK_TEST_CLASS(TeamViewNeverToOpponents, "StealthGame.Visibility.PIE")`)
- `Source/StealthTestHarness/Private/Scenarios/` and `Build/Scripts/` (T3 scenario `StealthGame.Visibility.MP.TeamViewNeverToOpponents` for the rejoin leg)
- Harness-only instrumented per-connection replication log and the injected-receipt negative control, inside `StealthTestHarness`

---

## QA Test Cases

- **AC-1**: Team view never reaches opponents (AC-IVP-19)
  - Given: a 2v2 with an instrumented per-connection replication log.
  - When: an elimination, a takeover, a disconnect and a rejoin occur.
  - Then: no opponent connection, Active or Informant, ever receives a C-TeamView entry, `PawnRef`, `IsConcealed` flag or fallback position of the other team.
  - Edge cases: check informant opponents explicitly and the window around rejoin; negative control (added): an injected opponent receipt of a team entry must be detected. (Tier: T2, T3 for rejoin.)

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: a T2 test under `Source/StealthTestHarness/Private/Tests/PIE/` plus the T3 rejoin scenario under `Source/StealthTestHarness/Private/Scenarios/` OR a documented playtest/spike report

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/information-visibility-policy/story-003-spike-s14-s1-rejoin-and-pawnrecovery-possession.md`, `production/epics/information-visibility-policy/story-019-possession-canary.md`, `production/epics/information-visibility-policy/story-032-team-fallback-feed-budget.md`; participant-state-roster epic stories delivering the C-TeamView components; npc-kill-penalty-informants epic story delivering the takeover path; session-travel epic story delivering rejoin
- Unlocks: None
