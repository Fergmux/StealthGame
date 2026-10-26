# Story 020: SwapTeam is refused only when the target team is full; pair exchange is atomic; swaps outside Lobby return WrongPhase

> **Epic**: Round & Match Framework
> **Status**: Complete
> **Layer**: Core
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: 2026-10-26

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-001` (Five-phase machine in a game-instance store surviving travel). The nearest registered requirement; the team rules are not a separate TR, so this mapping is approximate. Also `GDD AC 38`, `R3` (host `SwapTeam`, pair exchange, no rebalance after the lock), `R22` (`stealth.round.swapteam`, `stealth.round.remove`).
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0005: Session, seamless travel and match-scoped state stores
**ADR Decision Summary**: `AGameModeBase` with four game-instance stores (Session, Roster, Round, Tuning preset) as the only authoritative match state, seamless `ServerTravel` with `?Seed=`, single round acknowledgement, admission modes, host commands on the game instance, no host migration. This story applies section 8 (host commands are registered on the game instance with a net-mode and host check) as the entry that reaches the swap rule, and section 2 (Round owns the rule, the Roster owns the data).
**ADR Version**: ADR-0005 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: ADR-0005 uses only long-standing hooks; the post-cutoff item (5.6 `DispatchPostLogin` deprecation) is not touched. This story tests the swap rules headlessly against a fake Roster; the console registration through `IConsoleManager` on the game instance is story 026/027's concern. Spikes S3, S4, S5 are not touched.

**Control Manifest Rules (this layer)** (the Round-relevant rules sit in the manifest's Foundation Layer Rules, Session and travel and Tests; the Core Layer Rules section holds no Round-specific rule):
- Required: Every Logic story ships T1 tests, which live in the owning module's `Private/Tests/<System>/` (ADR-0008).
- Forbidden: Never store anything authoritative on a PlayerController, PlayerState, GameMode or GameState; never carry match state across travel on them (ADR-0005).
- Guardrail: Tests: per-commit pipeline at most 10 min (ADR-0008); this is a headless T1 test with no world.

**Prototype disposition**: REPLACES `AStealthGameMode::SwitchTeam` and the dev console command `StealthSwitchTeam` on `AStealthPlayerController` (`Source/StealthGame/Private/StealthGameMode.cpp` line about 203 and `StealthPlayerController.cpp`). The prototype `SwitchTeam` flips the caller's own team on the PlayerState with no capacity check, destroys and restarts the pawn, and is reached from a client-owned controller (`ServerSwitchTeam` through `StealthSwitchTeam`), which ADR-0005 forbids for host commands. The command survives only as the host command `stealth.round.swapteam` behind the net-mode check (TR-round-010, story 026).

---

## Acceptance Criteria

*From GDD `design/gdd/round-match-framework.md` AC 38, scoped to this story:*

- [ ] GIVEN Lobby, THEN a single `SwapTeam` is refused iff the target holds `TeamSize`; a 5/5 pair exchange succeeds atomically; any swap outside Lobby returns `WrongPhase`; after the lock a disconnect changes no count and a Dormant slot is freed only by `remove`.

---

## Implementation Notes

*Derived from GDD R3, R22 and ADR-0005 sections 2 and 8:*

- Rule (R3): "The host may move one participant with `SwapTeam(Id)` (a single move, refused only if the target team already holds `TeamSize`) or exchange two (a batch validated against the final state, so a 5/5 swap succeeds)." Any team layout is legal in Lobby; F3 is checked only at `StartRound` (story 022). Teams are match-scoped: they change only in Lobby. "Teams are never rebalanced after the lock": a disconnect mid-round moves no one, and a Dormant record does not free a slot until the host removes it.
- Single swap: refuse iff the count of the target team equals `TeamSize` (a swap to a team one below `TeamSize` succeeds). The refusal reaches only the host sink, never a client. The atomic mutation is a Roster call (`AssignTeams`); Round validates, the Roster owns the data.
- Pair exchange: validated against the final state in one batch, so with `TeamSize` 5 and a 5/5 layout, exchanging one member from each team succeeds, and no intermediate observed count exceeds `TeamSize` (the test asserts through the fake Roster's recorded states that the exchange is one atomic call, not two sequential swaps). Open: confirm against GDD R3 and R22 the pair-exchange entry point, because the AC does not name it (R22 lists `stealth.round.swapteam <participant>` for one participant only).
- Phase rule (R22): `swapteam`, `remove` and `preset` return `WrongPhase` outside Lobby. The test issues swaps in Preparing, Active, Ended and Reset and asserts `WrongPhase` with no Roster call.
- After the lock (`SetTeamsLocked(true)`, from story 024's start ordering, here set directly on the fake): a disconnect (Roster state to Dormant) changes no team count; a Dormant slot is freed only by `RemoveParticipant` through `stealth.round.remove` (legal while teams are unlocked, R5 check 3). The test asserts the counts are unchanged after the disconnect and that only the explicit remove decrements the count.
- Gateways are injected (story 003), no singleton access; fixtures (`TeamSize` 5, 5/5 layout) live in the constant files (story 005).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 018 (`production/epics/round-match-framework/story-018-team-assignment.md`): automatic assignment of a new key.
- Story 019 (`production/epics/round-match-framework/story-019-team-reclaim-keeps-team.md`): reclaim keeps the team.
- Story 021 (`production/epics/round-match-framework/story-021-team-size-injection.md`): `TeamSize` injection and `Rejected_TeamSizeBelowRoster`.
- Story 022 (`production/epics/round-match-framework/story-022-start-validity.md`): Start refusal codes (F3 at start).
- Story 026 (`production/epics/round-match-framework/story-026-host-only-commands.md`) and story 027 (`production/epics/round-match-framework/story-027-reset-command-matrix.md`): console registration, `NotHost`, the full command matrix per phase.
- Roster internals (`AssignTeams`, `RemoveParticipant`, tombstones): epic `participant-state-roster`.

---

## Files

*Planned files. Tests live under the engine test roots (`Source/<Module>/Private/Tests/`, see `tests/README.md`), not under `tests/`. `/dev-story` replaces this list with the files it actually changed.*

- Source/StealthGameplay/Private/Core/Round/StealthRoundTeamRules.cpp (SwapTeam, ExchangeTeams, RemoveParticipant)
- Source/StealthGame/Private/StealthGameMode.cpp, StealthPlayerController.cpp/.h (prototype SwitchTeam / StealthSwitchTeam / ServerSwitchTeam retired)
- Source/StealthGameplay/Private/Tests/Round/SwapTeamRulesTest.cpp (new)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: `SwapTeam` is refused only when the target is full, pair exchange is atomic, and swaps outside Lobby return `WrongPhase`.
  - Given: a Lobby fake Roster and `TeamSize` 5.
  - When: single swaps, a 5/5 pair exchange, swaps in other phases, and disconnects after the lock are issued.
  - Then: a single swap is refused iff the target team holds `TeamSize`.
  - Then: the 5/5 pair exchange succeeds atomically, with no observed count above `TeamSize`.
  - Then: any swap outside Lobby returns `WrongPhase`.
  - Then: after the lock a disconnect changes no count, and a Dormant slot is freed only by `remove`.
  - Edge cases: Open: confirm against GDD R3 and R22 the pair-exchange entry point, because the AC does not name it. A swap to a team one below `TeamSize` succeeds.

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Round/SwapTeamRulesTest.cpp` — must exist and pass (automation test `StealthGame.Round.SwapTeamRules`, class `FRoundSwapTeamRulesTest`)

**Status**: [x] Created and passing (`StealthGame.Round.SwapTeamRules`, 2026-10-26)

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-018-team-assignment.md`
- Unlocks: None

Cross-epic: `production/epics/participant-state-roster` (`AssignTeams`, `SetTeamsLocked`, `RemoveParticipant`; stories not yet created); `production/epics/tuning-data-config` (module skeleton and StealthTestHarness bootstrap; stories not yet created)

---

## Completion Notes
**Completed**: 2026-10-26 (D53 integration batch: PSR 061-062, RMF 018-029, SCL 002/006)
**Criteria**: 1/1 passing
**Deviations**: Open point ruled: the pair exchange is stealth.round.swapteam <participant> <participant> (one argument = the single move).
**Test Evidence**: `Source/StealthGameplay/Private/Tests/Round/SwapTeamRulesTest.cpp`; full suite: `production/qa/evidence/integration-spine-2026-10-26/full-suite.txt` (120/120 Success)
**Run result**: N/A: host console rule with no on-screen change beyond the team label already covered by the integration screenshots
**Reviewed by**: lead-programmer focused review (sonnet) + self-review, D53 batch
