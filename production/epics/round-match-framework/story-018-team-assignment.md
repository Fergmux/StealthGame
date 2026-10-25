# Story 018: A new key joins the smaller team; ties go to team RoundsPlayedCount parity

> **Epic**: Round & Match Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25f
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-001` (Five-phase machine in a game-instance store surviving travel). The nearest registered requirement; the team rules are not a separate TR, so this mapping is approximate. Also `GDD AC 36`, `R3` (Assignment in Lobby only).
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0005: Session, seamless travel and match-scoped state stores
**ADR Decision Summary**: `AGameModeBase` with four game-instance stores (Session, Roster, Round, Tuning preset) as the only authoritative match state, seamless `ServerTravel` with `?Seed=`, single round acknowledgement, admission modes, host commands on the game instance, no host migration. This story applies section 2: Round's data lives in `URoundMatchStore` (`FRoundPhaseCore`, plain C++), Roster data in `URosterStore` (`FRosterCore`), both keyed by `ParticipantId`, and the team rule is Round's while the data (`AssignTeams`) is the Roster's.
**ADR Version**: ADR-0005 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: ADR-0005 uses only long-standing `AGameModeBase`/`APlayerController`/`UNetDriver` hooks; the post-cutoff item is the 5.6 deprecation of `DispatchPostLogin` (use `PostLogin`/`OnPostLogin`). This story is a plain-C++ rule over fake Roster and Session gateways (headless, no `UWorld`), so none of ADR-0005's verification spikes S3 (seamless travel), S4 (reconnect) or S5 (listen-host loss) is touched; `OnParticipantRegistered` is the Roster event the rule subscribes to.

**Control Manifest Rules (this layer)** (the Round-relevant rules sit in the manifest's Foundation Layer Rules, Session and travel; the Core Layer Rules section holds no Round-specific rule):
- Required: Authoritative match state lives only in the four game-instance stores, keyed by `ParticipantId`, never by controller or PlayerState pointer (ADR-0005).
- Forbidden: Never put team, state, connection, appearance or pawn data on a PlayerState; never give NPCs a PlayerState (ADR-0001).
- Guardrail: Tests: per-commit pipeline at most 10 min (ADR-0008); this is a headless T1 test with no world.

**Prototype disposition**: REPLACES the team assignment in `AStealthGameMode::PostLogin` (`Source/StealthGame/Private/StealthGameMode.cpp`, the `PlayersPerTeam` count and `FStealthRules::ChooseTeamForNewPlayer(PlayersPerTeam, MaxPlayersPerTeam)` call). The prototype picks the smaller team and breaks ties toward team 0, with no `RoundsPlayedCount` parity and no Roster. `FStealthRules::ChooseTeamForNewPlayer` and its test in `StealthRulesTests.cpp` are reference only, re-expressed against the Roster gateway. Production code follows ADR-0007 modules; the prototype is not promoted wholesale.

---

## Acceptance Criteria

*From GDD `design/gdd/round-match-framework.md` AC 36, scoped to this story:*

- [ ] GIVEN Lobby with 2/1, WHEN a new key registers, THEN it joins the team of 1; GIVEN a 2/2 tie, THEN it joins team 0 at `RoundsPlayedCount` 0 and team 1 at `RoundsPlayedCount` 1.

---

## Implementation Notes

*Derived from GDD R3 and ADR-0005 section 2:*

- Rule (R3): when `OnParticipantRegistered` fires for a **new** key in Lobby, Round assigns the participant to the team with fewer rostered members via one atomic `AssignTeams` call on the Roster; on a tie the favoured team alternates by round (`RoundsPlayedCount mod 2`), so an odd head-count does not always enlarge team 0; at `RoundsPlayedCount = 0` the tie favours team 0, so the first registrant (normally the host) is on team 0. There are exactly two teams (`0`, `1`).
- Where it lives: a team-assignment unit in the Round Core (planned `Source/StealthGameplay/Private/Round/Teams/`), operating on the `FRoundPhaseCore` data held by `URoundMatchStore`. It reads counts through the Roster gateway (`RosterCount`) and writes only through `AssignTeams`; it never writes team data itself (the Roster owns the data) and never stores a team on a PlayerController or PlayerState (ADR-0005 guideline 3).
- Assignment applies in Lobby only. A registration in any other phase is not assigned here (a joiner between rounds in `Preparing`..`Reset` is a rejoin path, R18, other stories).
- Gateways are injected (story 003) with no singleton access: the test supplies a fake Roster in Lobby with a 2/1 layout and a 2/2 layout and records `AssignTeams` calls in order. `RoundsPlayedCount` comes from the round store core, set by the test.
- Prefer a pure function `ChooseTeamForNewKey(Counts[2], RoundsPlayedCount)` plus the event handler that calls the gateway, so the arithmetic is unit-testable without events.
- Edge cases from the spec: exactly one assignment call per registration; the existing members' teams are unchanged. `TeamSize` capacity (a full team) is story 020/022's concern; this story does not refuse a registration.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 019 (`production/epics/round-match-framework/story-019-team-reclaim-keeps-team.md`): a reclaimed Dormant key keeps its team and triggers no `AssignTeams` call.
- Story 020 (`production/epics/round-match-framework/story-020-swap-team-rules.md`): host `SwapTeam`, pair exchange, `WrongPhase` outside Lobby.
- Story 021 (`production/epics/round-match-framework/story-021-team-size-injection.md`): injecting `TeamSize` into the Roster.
- Story 022 (`production/epics/round-match-framework/story-022-start-validity.md`): F3 start validity at `StartRound`.
- Roster internals (records, `AssignTeams` implementation, `OnParticipantRegistered` raising): epic `participant-state-roster`.

---

## Files

*Planned files. Tests live under the engine test roots (`Source/<Module>/Private/Tests/`, see `tests/README.md`), not under `tests/`. `/dev-story` replaces this list with the files it actually changed.*

- `Source/StealthGameplay/Private/Tests/Round/TeamAssignmentTest.cpp` (class `FRoundTeamAssignmentTest`, test name `StealthGame.Round.TeamAssignment`)
- `Source/StealthGameplay/Public/Round/` team-assignment header (new; Open: name set when implemented)
- `Source/StealthGameplay/Private/Round/Teams/` team-assignment implementation (new)
- `Source/StealthGame/Private/StealthGameMode.cpp` (`PostLogin` team choice; retired/replaced by this story's rule, removal lands with the Round composition-root story)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: A new key joins the smaller team, and ties go to the team given by `RoundsPlayedCount` parity.
  - Given: a fake Roster in Lobby, with a 2/1 layout and a 2/2 layout.
  - When: a new key registers.
  - Then: in 2/1 it joins the team of 1.
  - Then: in 2/2 it joins team 0 at `RoundsPlayedCount` 0 and team 1 at `RoundsPlayedCount` 1.
  - Edge cases: one assignment call per registration; the existing members' teams are unchanged.

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Round/TeamAssignmentTest.cpp` — must exist and pass (automation test `StealthGame.Round.TeamAssignment`, class `FRoundTeamAssignmentTest`)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-003-test-seam-fake-gateways.md`, `production/epics/round-match-framework/story-005-test-fixture-constants.md`
- Unlocks: `production/epics/round-match-framework/story-019-team-reclaim-keeps-team.md`, `production/epics/round-match-framework/story-020-swap-team-rules.md`, `production/epics/round-match-framework/story-021-team-size-injection.md`, `production/epics/round-match-framework/story-022-start-validity.md`

Cross-epic: `production/epics/participant-state-roster` (`AssignTeams`, `OnParticipantRegistered`, `RosterCount`; stories not yet created); `production/epics/tuning-data-config` (module skeleton and StealthTestHarness bootstrap; stories not yet created)
