# Story 019: A reclaimed Dormant key keeps its team and triggers no AssignTeams call

> **Epic**: Round & Match Framework
> **Status**: Complete
> **Layer**: Core
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25f
> **Last Updated**: 2026-10-26

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-001` (Five-phase machine in a game-instance store surviving travel). The nearest registered requirement; the team rules are not a separate TR, so this mapping is approximate. Also `GDD AC 37`, `R3` (a returning known key keeps its team).
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0005: Session, seamless travel and match-scoped state stores
**ADR Decision Summary**: `AGameModeBase` with four game-instance stores (Session, Roster, Round, Tuning preset) as the only authoritative match state, seamless `ServerTravel` with `?Seed=`, single round acknowledgement, admission modes, host commands on the game instance, no host migration. This story applies the identity part: a participant is keyed by `ParticipantId` (from `IdentityKey`), the Roster record is match-scoped, and a rejoiner adopts its retained PlayerState rather than a new one, so a reclaim carries the old team.
**ADR Version**: ADR-0005 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: ADR-0005 post-cutoff item is the 5.6 `DispatchPostLogin` deprecation (use `PostLogin`/`OnPostLogin`), not touched here. This story is a headless rule over a fake Roster; ADR-0005 verification spike S4 (reconnect: retained PlayerState re-attached to a new controller, `-IdentityKey=`, supersede) owns the real reclaim and is not touched.

**Control Manifest Rules (this layer)** (the Round-relevant rules sit in the manifest's Foundation Layer Rules, Session and travel; the Core Layer Rules section holds no Round-specific rule):
- Required: A rejoiner adopts its retained PlayerState in `InitPlayerState`; `OnDeactivated` does not destroy the PlayerState while admission is `RejoinOnly` (ADR-0005).
- Forbidden: Never toggle `bIsInactive` or remove a PlayerState from the array between `Preparing` and the next travel; never spawn a fresh PlayerState for a reclaim (ADR-0005).
- Guardrail: Tests: per-commit pipeline at most 10 min (ADR-0008); this is a headless T1 test with no world.

**Prototype disposition**: REPLACES the prototype's team choice in `AStealthGameMode::PostLogin` (`Source/StealthGame/Private/StealthGameMode.cpp`), which runs for every login, counts teams from the PlayerState array and stores the team on the PlayerState; it has no Dormant record or reclaim concept. Not promoted.

---

## Acceptance Criteria

*From GDD `design/gdd/round-match-framework.md` AC 37, scoped to this story:*

- [ ] GIVEN a Dormant key on the larger team, WHEN it is reclaimed, THEN it keeps its team and no `AssignTeams` call is made.

---

## Implementation Notes

*Derived from GDD R3 and ADR-0005 section 2:*

- Rule (R3): "A returning known key (`OnParticipantReclaimed`, a Dormant record) keeps the team it had: the Roster record is match-scoped, so teams do not drift when someone crashes and returns." Teams are never rebalanced after the lock, and also not on a reclaim in Lobby: the reclaim handler must not call `AssignTeams` even when the reclaimed participant's team is the larger one.
- Implementation: in the team-assignment unit from story 018, the `OnParticipantReclaimed` handler is a no-op for team data (it must not route through the new-key path of `OnParticipantRegistered`). The test distinguishes the two events: a new key calls `AssignTeams` exactly once (story 018); a reclaimed key calls it 0 times.
- Test shape: a fake Roster (story 003) holds a Dormant key on the larger team (for example a 3/2 layout with the Dormant key on the 3 side); the test raises the reclaim event and asserts the key's team is unchanged, the `AssignTeams` call count is 0, and neither team's count changed (no rebalance).
- A Dormant slot is freed only by an explicit host `remove` (R5 check 3, R3), covered by story 020; this story does not free or move a slot.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 018 (`production/epics/round-match-framework/story-018-team-assignment.md`): assignment of a new key.
- Story 020 (`production/epics/round-match-framework/story-020-swap-team-rules.md`): swaps, and "a Dormant slot is freed only by `remove`" after the lock.
- Rejoin and re-admission after expiry (R17, R18; stories 061 to 063, 074).
- Roster internals (Dormant records, `OnParticipantReclaimed` raising) and Session internals (identity key, retained PlayerState): epics `participant-state-roster` and `session-travel`.

---

## Files

*Planned files. Tests live under the engine test roots (`Source/<Module>/Private/Tests/`, see `tests/README.md`), not under `tests/`. `/dev-story` replaces this list with the files it actually changed.*

- Source/StealthGameplay/Public/Server/Core/Round/StealthRoundTeamRules.h (OnParticipantReclaimed: no Roster call)
- Source/StealthGameplay/Private/Tests/Round/TeamReclaimKeepsTeamTest.cpp (new)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: A reclaimed Dormant key keeps its team and triggers no `AssignTeams` call.
  - Given: a Dormant key on the larger team of a fake Roster.
  - When: the key is reclaimed.
  - Then: it keeps its team.
  - Then: the `AssignTeams` call count is 0.
  - Edge cases: the reclaim does not rebalance the teams even though the team is the larger one.

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Round/TeamReclaimKeepsTeamTest.cpp` — must exist and pass (automation test `StealthGame.Round.TeamReclaimKeepsTeam`, class `FRoundTeamReclaimKeepsTeamTest`)

**Status**: [x] Created and passing (`StealthGame.Round.TeamReclaimKeepsTeam`, 2026-10-26)

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-018-team-assignment.md`
- Unlocks: None

Cross-epic: `production/epics/participant-state-roster` (`OnParticipantReclaimed`, Dormant records; stories not yet created); `production/epics/tuning-data-config` (module skeleton and StealthTestHarness bootstrap; stories not yet created)

---

## Completion Notes
**Completed**: 2026-10-26 (D53 integration batch: PSR 061-062, RMF 018-029, SCL 002/006)
**Criteria**: 1/1 passing
**Deviations**: None
**Test Evidence**: `Source/StealthGameplay/Private/Tests/Round/TeamReclaimKeepsTeamTest.cpp`; full suite: `production/qa/evidence/integration-spine-2026-10-26/full-suite.txt` (120/120 Success)
**Run result**: N/A: a reclaim is a server rule with no visible change in L_Prototype (no Session rejoin yet)
**Reviewed by**: lead-programmer focused review (sonnet) + self-review, D53 batch
