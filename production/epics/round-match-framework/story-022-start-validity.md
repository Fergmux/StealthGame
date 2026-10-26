# Story 022: Start refuses each invalid team layout with the specified code in check order and stays in Lobby

> **Epic**: Round & Match Framework
> **Status**: Complete
> **Layer**: Core
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: 2026-10-26

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-001` (Five-phase machine in a game-instance store surviving travel). The nearest registered requirement; Start validity is not a separate TR, so this mapping is approximate. Also `GDD AC 3`, `R5` (`StartRound` checks in order), `F3` (start validity).
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0005: Session, seamless travel and match-scoped state stores
**ADR Decision Summary**: `AGameModeBase` with four game-instance stores (Session, Roster, Round, Tuning preset) as the only authoritative match state, seamless `ServerTravel` with `?Seed=`, single round acknowledgement, admission modes, host commands on the game instance, no host migration. This story applies section 2 (Round's phase machine is `FRoundPhaseCore` in `URoundMatchStore`; Start reads Roster counts and Session connection state through gateways) and section 8 (Start is a host command on the game instance whose refusal goes to the host only).
**ADR Version**: ADR-0005 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: ADR-0005 post-cutoff item (5.6 `DispatchPostLogin` deprecation) is not touched. This story is the Start validity logic against fake Session, Roster and TDC gateways with a refusal-sink spy; no engine API and none of spikes S3, S4, S5 is touched.

**Control Manifest Rules (this layer)** (the Round-relevant rules sit in the manifest's Foundation Layer Rules, Session and travel; the Core Layer Rules section holds no Round-specific rule):
- Required: Host commands are registered on the game instance with the net-mode/host check (ADR-0005); the host check is Start's first refusal (`NotHost`).
- Forbidden: Never store anything authoritative on a PlayerController, PlayerState, GameMode or GameState; never carry match state across travel on them (ADR-0005).
- Guardrail: Tests: per-commit pipeline at most 10 min (ADR-0008); this is a headless T1 test with no world.

**Prototype disposition**: REPLACES the start gating in `AStealthGameMode::TryScheduleRoundStart` / `StartRound` (`Source/StealthGame/Private/StealthGameMode.cpp`) and the dev command `StealthStartRound`, which start on a player-count condition with no refusal codes and no F3. The command survives as the host command `stealth.round.start` behind the net-mode check (TR-round-010, story 026).

---

## Acceptance Criteria

*From GDD `design/gdd/round-match-framework.md` AC 3, scoped to this story:*

- [ ] GIVEN Lobby and a refusal-sink spy, WHEN Start is issued for each fixture layout, THEN: 5v3 -> `TeamsInvalid(Imbalance)`; a fake Roster gateway reporting 3 on one team with `TeamSize` 2 -> `Oversize` (defensive; the real Roster refuses such a team); `MinPlayersPerTeamCount` 2 with 2v1 -> `TooSmall`; 1v0 -> `TooSmall` without and accepted with `bAllowEmptyTeamDev`; 5v4, 5v5, 2v2 (`TeamSize` 2) -> accepted; a layout that is both Dormant-containing and imbalanced -> `ParticipantNotConnected` (check order). Each refusal reaches only the host sink and the phase stays `Lobby`.

---

## Implementation Notes

*Derived from GDD R5, F3 and ADR-0005 sections 2 and 8:*

- Return type (R5): `FStartRefusal {Code, ParticipantIds}` on refusal, shown to the host only, and Start then changes nothing. The checks run in this order and the first failure decides: (1) `NotHost`; (2) `WrongPhase` / `TravelInProgress`; (3) `ParticipantNotConnected` (lists ids; any rostered participant `Unresponsive`, `Connecting` or `Dormant`); (4) `TeamsInvalid(TooSmall | Imbalance | Oversize)` (F3 fails); (5) `PresetInvalid` / `DeckInfeasible` (best-effort TDC dry-run pre-checks, skipped with a warning if `PreviewPreset` is unavailable). Open: confirm against GDD R5 and F3 that this list is the full fixed check order the fixture asserts; the AC pins only the NotConnected-before-Imbalance pair, so the fixture takes the rest from the R5 list.
- F3: `StartValid = for all p: Team(p) != None AND Conn(p) = Connected; for t in {0,1}: m_t <= n_t <= TeamSize; abs(n0 - n1) <= MaxTeamImbalanceCount; n0 + n1 >= 1`, with `m_t = MinPlayersPerTeamCount` except that with `bAllowEmptyTeamDev` one team may have `m_t = 0`. Worked examples (F3): 5v4 valid; 5v3 invalid (`Imbalance`, 2 > 1); 2v2 with `TeamSize` 2 valid; 1v0 invalid unless `bAllowEmptyTeamDev` (imbalance 1 <= 1). Defaults: `MinPlayersPerTeamCount` 1, `MaxTeamImbalanceCount` 1, `TeamSize` 5; values come from tuning through the TDC gateway, never literals. Cross-field invariant (TDC): `MinPlayersPerTeamCount <= TeamSize`.
- `Oversize` is defensive only: the real Roster refuses to exceed `TeamSize`, so the test uses a fake Roster gateway that reports 3 on one team with `TeamSize` 2.
- R5 states an unassigned participant is auto-assigned first by R3, so it can never reach check 4 (asserted; there is no `Unassigned` sub-code).
- Each refusal goes only to the host sink (a spy in the test); no client sink is touched, and the phase stays Lobby with no gateway mutation (no `SetAdmissionMode`, `SetTeamsLocked`, `ResetAll` or `RequestRoundTravel` call; the accepted ordering is story 024).
- Where it lives: a Start-validation unit in the Round Core (planned `Source/StealthGameplay/Private/Round/Start/`), a pure function over `{counts[2], team-less count, connection states, TeamSize, MinPlayersPerTeamCount, MaxTeamImbalanceCount, bAllowEmptyTeamDev}` plus the sink-reporting wrapper. Layout fixtures live in the test constant files (story 005), not inline.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 023 (`production/epics/round-match-framework/story-023-start-refuses-not-connected.md`): `ParticipantNotConnected` naming exactly the Dormant, Unresponsive or Connecting set (this story asserts only its check order relative to `Imbalance`).
- Story 024 (`production/epics/round-match-framework/story-024-start-ordering.md`): the call order and phase change of an accepted Start.
- Story 025 (`production/epics/round-match-framework/story-025-start-rollback.md`): `SessionRefused` and rollback.
- Story 026 (`production/epics/round-match-framework/story-026-host-only-commands.md`): `NotHost` by console and `ServerExecRPC`.
- Story 018 (`production/epics/round-match-framework/story-018-team-assignment.md`): team assignment that precedes Start.
- TDC `PreviewPreset` and Character Pool deck floor internals (epics `tuning-data-config`, `character-pool-appearance`).

---

## Files

*Planned files. Tests live under the engine test roots (`Source/<Module>/Private/Tests/`, see `tests/README.md`), not under `tests/`. `/dev-story` replaces this list with the files it actually changed.*

- Source/StealthGameplay/Public/Server/Core/Round/StealthRoundStart.h, Private/Core/Round/StealthRoundStart.cpp (new: FRoundStartValidator, F3)
- Source/StealthFoundation/Public/Server/Testing/StealthRoundTestConstants.h (Exit B layouts and sizes)
- Source/StealthGameplay/Private/Tests/Round/StartValidityTest.cpp (new)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: Start refuses each invalid team layout with the specified code in check order and stays in Lobby.
  - Given: Lobby, a refusal-sink spy and the fixture layouts.
  - When: Start is issued for each layout.
  - Then: 5v3 gives `TeamsInvalid(Imbalance)`.
  - Then: a fake Roster reporting 3 on one team with `TeamSize` 2 gives `Oversize`.
  - Then: `MinPlayersPerTeamCount` 2 with 2v1 gives `TooSmall`.
  - Then: 1v0 gives `TooSmall` without `bAllowEmptyTeamDev` and is accepted with it.
  - Then: 5v4, 5v5 and 2v2 (`TeamSize` 2) are accepted.
  - Then: a layout that is both Dormant-containing and imbalanced gives `ParticipantNotConnected`.
  - Then: each refusal reaches only the host sink and the phase stays `Lobby`.
  - Edge cases: the AC pins only one check-order pair (NotConnected before Imbalance), so the full order comes from the GDD's check list (R5) as a fixture. Open: confirm the fixed check order against GDD R5. `Oversize` is defensive only.

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Round/StartValidityTest.cpp` — must exist and pass (automation test `StealthGame.Round.StartValidity`, class `FRoundStartValidityTest`)

**Status**: [x] Created and passing (`StealthGame.Round.StartValidity`, 2026-10-26)

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-003-test-seam-fake-gateways.md`, `production/epics/round-match-framework/story-005-test-fixture-constants.md`, `production/epics/round-match-framework/story-018-team-assignment.md`
- Unlocks: `production/epics/round-match-framework/story-023-start-refuses-not-connected.md`, `production/epics/round-match-framework/story-024-start-ordering.md`, `production/epics/round-match-framework/story-026-host-only-commands.md`

Cross-epic: `production/epics/participant-state-roster` (`RosterCount`, participant states; stories not yet created); `production/epics/session-travel` (connection states `Unresponsive`, `Connecting`, `Dormant`; stories not yet created); `production/epics/tuning-data-config` (`TeamSize`, `MinPlayersPerTeamCount`, `MaxTeamImbalanceCount`, `bAllowEmptyTeamDev`; module skeleton and StealthTestHarness bootstrap; stories not yet created)

---

## Completion Notes
**Completed**: 2026-10-26 (D53 integration batch: PSR 061-062, RMF 018-029, SCL 002/006)
**Criteria**: 1/1 passing
**Deviations**: F3 sub-code order inside check 4 is Oversize, TooSmall, Imbalance (the fixtures are unambiguous either way). DeckInfeasible has no gateway yet (skipped like an Unavailable dry run).
**Test Evidence**: `Source/StealthGameplay/Private/Tests/Round/StartValidityTest.cpp`; full suite: `production/qa/evidence/integration-spine-2026-10-26/full-suite.txt` (120/120 Success)
**Run result**: N/A: refusal path is host-log only; the accepted path is observed in integration-spine-2026-10-26
**Reviewed by**: lead-programmer focused review (sonnet) + self-review, D53 batch
