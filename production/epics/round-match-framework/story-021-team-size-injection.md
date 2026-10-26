# Story 021: A preset with a different TeamSize calls SetTeamSize once; refusals keep the previous preset; locked teams never call it

> **Epic**: Round & Match Framework
> **Status**: Complete
> **Layer**: Core
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25f
> **Last Updated**: 2026-10-26

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-001` (Five-phase machine in a game-instance store surviving travel). The nearest registered requirement; the team rules are not a separate TR, so this mapping is approximate. Also `GDD AC 39`, `R3` (`TeamSize`: a TDC key, Round's rule), `R22` (`stealth.round.preset`).
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0005: Session, seamless travel and match-scoped state stores
**ADR Decision Summary**: `AGameModeBase` with four game-instance stores (Session, Roster, Round, Tuning preset) as the only authoritative match state, seamless `ServerTravel` with `?Seed=`, single round acknowledgement, admission modes, host commands on the game instance, no host migration. This story applies section 2 (the Roster store is `URosterStore`/`FRosterCore`; Round injects the rule value into it) and section 8 (`stealth.round.preset` and `stealth.tuning.preset` are host commands on the game instance).
**ADR Version**: ADR-0005 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: ADR-0005's post-cutoff item (5.6 `DispatchPostLogin` deprecation) is not touched. This story is a headless rule over fake Roster and TDC gateways; no engine API and none of spikes S3, S4, S5 is touched.

**Control Manifest Rules (this layer)** (the Round-relevant rules sit in the manifest's Foundation Layer Rules, Tuning and Session and travel; the Core Layer Rules section holds no Round-specific rule):
- Required: Server code reads only `GetServerTuning()`/`FValidatedTuning` (ADR-0004); `TeamSize` is read from there and injected, never read from the raw asset.
- Forbidden: Never give a tuning accessor a pawn, controller, team, viewer or participant parameter (ADR-0004).
- Guardrail: Tests: per-commit pipeline at most 10 min (ADR-0008); this is a headless T1 test with no world.

**Prototype disposition**: REPLACES the prototype's `UStealthSettings::MaxPlayersPerTeam` read (`Source/StealthGame/Private/StealthGameMode.cpp`, `PostLogin`), which is a plain settings field with no preset and no Roster injection (`UStealthSettings` is replaced by the tuning pipeline, ADR-0007). Not promoted.

---

## Acceptance Criteria

*From GDD `design/gdd/round-match-framework.md` AC 39, scoped to this story:*

- [ ] GIVEN a preset with a different `TeamSize` in Lobby, THEN Round calls `SetTeamSize` once; GIVEN `Rejected_TeamSizeBelowRoster`, THEN the previous preset stays and the host sees the refusal; GIVEN locked teams, THEN `SetTeamSize` is never called.

---

## Implementation Notes

*Derived from GDD R3 and ADR-0005 sections 2 and 8:*

- Rule (R3): the `TeamSize` value lives in TDC's canonical table (owner column Round, latch `Round`); the rule (cap, balance, minimum, F3) is Round's. Round reads the lobby-time value through `GetServerTuning()` and injects it into the Roster: `BeginMatch(TeamSize)` once when the match is created, and `SetTeamSize(TeamSize)` in Lobby whenever `stealth.round.preset` changes it. A `Rejected_TeamSizeBelowRoster` refusal keeps the previous preset and shows the host the refusal; teams must be reassigned first. Round never changes `TeamSize` while teams are locked. Cross-field invariant (TDC): `MinPlayersPerTeamCount <= TeamSize`.
- Implementation: a preset-application unit in the Round Core (planned `Source/StealthGameplay/Private/Round/Teams/`) that, given the previously applied preset's `TeamSize` and the requested preset's `TeamSize` (from the TDC gateway, story 003), calls the Roster gateway's `SetTeamSize` exactly once when the values differ, none when equal, and none when teams are locked. On `Rejected_TeamSizeBelowRoster` it keeps the previous preset (the TDC preset selection is rolled back or not committed; Open: confirm against GDD R3 and R22 whether the rollback is a TDC call or a Round-held pointer) and sends the refusal to the host sink only (never a client).
- Phase rule: `stealth.round.preset` is Lobby only, else `WrongPhase` (R22; asserted by story 027's matrix). This story tests the Lobby rule and the locked-teams rule; the console registration is story 026/027.
- The test distinguishes three cases through the fake Roster's call recorder: different `TeamSize` (one call), refusal path (call made, previous preset retained, host sink received the refusal), locked teams (zero calls). A preset with the same `TeamSize` makes no call (edge case).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 018 (`production/epics/round-match-framework/story-018-team-assignment.md`): team assignment.
- Story 020 (`production/epics/round-match-framework/story-020-swap-team-rules.md`): swaps and reassigning teams after a refusal.
- Story 022 (`production/epics/round-match-framework/story-022-start-validity.md`): `PresetInvalid` / `DeckInfeasible` pre-checks and F3 at start.
- Tuning pipeline internals (preset store, validation, `PreviewPreset`, `stealth.tuning.preset`): epic `tuning-data-config`. Roster internals (`BeginMatch`, `SetTeamSize`, `Rejected_TeamSizeBelowRoster` production): epic `participant-state-roster`.

---

## Files

*Planned files. Tests live under the engine test roots (`Source/<Module>/Private/Tests/`, see `tests/README.md`), not under `tests/`. `/dev-story` replaces this list with the files it actually changed.*

- Source/StealthGameplay/Private/Core/Round/StealthRoundTeamRules.cpp (ApplyPresetTeamSize)
- Source/StealthGameplay/Private/Core/Round/StealthRoundMatchCore.cpp (OpenMatch BeginMatch(TeamSize); preset command)
- Source/StealthGameplay/Private/Tests/Round/TeamSizeInjectionTest.cpp (new)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: A preset with a different `TeamSize` calls `SetTeamSize` once, refusals keep the previous preset, and locked teams never call it.
  - Given: Lobby with a fake Roster gateway.
  - When: a preset with a different `TeamSize` is applied; the Roster returns `Rejected_TeamSizeBelowRoster`; teams are then locked and a preset is applied.
  - Then: `SetTeamSize` is called once.
  - Then: after `Rejected_TeamSizeBelowRoster` the previous preset stays and the host sink receives the refusal.
  - Then: with locked teams `SetTeamSize` is called 0 times.
  - Edge cases: a preset with the same `TeamSize` makes no call.

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Round/TeamSizeInjectionTest.cpp` — must exist and pass (automation test `StealthGame.Round.TeamSizeInjection`, class `FRoundTeamSizeInjectionTest`)

**Status**: [x] Created and passing (`StealthGame.Round.TeamSizeInjection`, 2026-10-26)

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-018-team-assignment.md`
- Unlocks: None

Cross-epic: `production/epics/participant-state-roster` (`SetTeamSize`, `BeginMatch`; stories not yet created); `production/epics/tuning-data-config` (`TeamSize` key, `GetServerTuning()`, module skeleton and StealthTestHarness bootstrap; stories not yet created)

---

## Completion Notes
**Completed**: 2026-10-26 (D53 integration batch: PSR 061-062, RMF 018-029, SCL 002/006)
**Criteria**: 1/1 passing
**Deviations**: Open point ruled: the previous preset is the Round-held applied TeamSize, committed only on Applied (no TDC rollback call). stealth.round.preset takes <name> <teamsize> until TDC exposes the preset's TeamSize by name.
**Test Evidence**: `Source/StealthGameplay/Private/Tests/Round/TeamSizeInjectionTest.cpp`; full suite: `production/qa/evidence/integration-spine-2026-10-26/full-suite.txt` (120/120 Success)
**Run result**: N/A: lobby-time host rule with no visible change
**Reviewed by**: lead-programmer focused review (sonnet) + self-review, D53 batch
