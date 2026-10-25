# Story 024: An accepted Start calls Session and Roster in the specified order and enters Preparing only after travel acceptance

> **Epic**: Round & Match Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-001` (Five-phase machine in a game-instance store surviving travel). Also `GDD AC 6a`, `R5` (effects, decided cross-GDD rule C2), `R4` (admission mode per phase).
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0005: Session, seamless travel and match-scoped state stores
**ADR Decision Summary**: `AGameModeBase` with four game-instance stores (Session, Roster, Round, Tuning preset) as the only authoritative match state, seamless `ServerTravel` with `?Seed=`, single round acknowledgement, admission modes, host commands on the game instance, no host migration. This story applies section 4 (Round's lock -> `ResetAll` -> `RequestRoundTravel` sequence, with `Preparing` set only on travel acceptance) and the Key Interfaces `SetAdmissionMode(EAdmissionMode)` (Round is the only caller), `CanRequestRoundTravel()` and `RequestRoundTravel(MapId, PresetId, RoundIndex)`.
**ADR Version**: ADR-0005 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: ADR-0005's post-cutoff item (5.6 `DispatchPostLogin` deprecation) is not touched. This story checks the call order against a fake Session whose travel acceptance can be deferred; the real `ServerTravel` (seamless travel, spike S3, and the transition map) is not exercised, and none of spikes S3, S4, S5 is touched. Travel acceptance tests on the real path run multi-process standalone (ADR-0005, ADR-0008).

**Control Manifest Rules (this layer)** (the Round-relevant rules sit in the manifest's Foundation Layer Rules, Session and travel and Forbidden; the Core Layer Rules section holds no Round-specific rule):
- Required: Every wait is bounded by a server-measured clock (ADR-0005); the Preparing entry waits on Session's travel acceptance, and Session's `Deadline` and `TravelLoadTimeoutSec` bound the travel.
- Forbidden: Never hard-travel every round (fallback only if spike S3 fails) (ADR-0005).
- Guardrail: Session: silence clocks and ack bookkeeping inside the 0.5 ms line; travel load bounded by `TravelLoadTimeoutSec` (60 s) (ADR-0005).

**Prototype disposition**: REPLACES `AStealthGameMode::StartRound` (`Source/StealthGame/Private/StealthGameMode.cpp`, line about 238), which resets round state and spawns in the same world with no admission lock, no Roster `ResetAll` and no travel. The prototype `StartRound` and its dev command `StealthStartRound` are retired by this and the Round composition-root stories; the command survives as `stealth.round.start` behind the net-mode check (TR-round-010).

---

## Acceptance Criteria

*From GDD `design/gdd/round-match-framework.md` AC 6a, scoped to this story:*

- [ ] GIVEN Start accepted, THEN a call-order spy records exactly `SetAdmissionMode(RejoinOnly)`, `SetTeamsLocked(true)`, `ResetAll(RoundsPlayedCount + 1)`, `RequestRoundTravel`, and the phase becomes `Preparing` only after the travel acceptance (decided rule C2).

---

## Implementation Notes

*Derived from GDD R5 effects and ADR-0005 section 4:*

- Rule (R5, decided cross-GDD rule C2: admission lock -> `ResetAll(RoundIndex)` -> travel -> `Preparing`), on acceptance and in order: (5b) `CanRequestRoundTravel()` (a Session query with no side effects; `false` refuses with `SessionRefused` before anything changes, story 025); (6) admission lock: `SetAdmissionMode(RejoinOnly)` and `SetTeamsLocked(true)`; (7) `ResetAll(RoundIndex)` with `RoundIndex = RoundsPlayedCount + 1`, the once-per-round roster reset (it runs here and not on `Preparing` entry because Session rule 11.2 requires the Roster round scope reset before `RequestRoundTravel`); (8) `RequestRoundTravel(MapId, PresetId, RoundIndex)`, where Session draws a fresh seed for every start; (9) only when Session accepts the travel, set phase `Preparing`.
- The spy records exactly four calls in this order: `SetAdmissionMode(RejoinOnly)`, `SetTeamsLocked(true)`, `ResetAll(RoundsPlayedCount + 1)`, `RequestRoundTravel`. No extra calls appear between the four (the `CanRequestRoundTravel()` query is a no-side-effect query; Open: confirm against GDD R5 and AC 6a whether the spy records queries or only mutating calls, and keep the spy's rule explicit). `ResetAll` receives the pre-increment `RoundsPlayedCount` plus 1; `RoundsPlayedCount` itself increments only at Active entry (story 042).
- The phase stays `Lobby` until acceptance and becomes `Preparing` only after it. The fake Session's acceptance is deferrable: the test issues Start, asserts the phase is `Lobby` with the four calls recorded, then releases acceptance and asserts `Preparing`. Phase changes push the admission mode (R4: Lobby `Open`; Preparing, Active, Ended `RejoinOnly`; Reset `Closed`); the pushed mode for `Preparing` is the one this story's lock call already set, and the full phase/admission table is story 028.
- Admission and travel are Round's calls on Session (`SetAdmissionMode` has Round as the only caller); Session never spawns, frees or possesses a pawn and never calls `RequestTransition` (ADR-0005 guideline 8). Where it lives: the Start orchestration unit in the Round Core (planned `Source/StealthGameplay/Private/Round/Start/`) operating on `FRoundPhaseCore` in `URoundMatchStore`; gateways are injected (story 003).
- Repeat index (R5): after an aborted start or rollback, `RoundsPlayedCount` has not advanced, so the next start calls `ResetAll(N+1)` again with the same index; this is asserted by story 025's rollback test, not here.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 022 (`production/epics/round-match-framework/story-022-start-validity.md`) and story 023 (`production/epics/round-match-framework/story-023-start-refuses-not-connected.md`): refusal checks before acceptance.
- Story 025 (`production/epics/round-match-framework/story-025-start-rollback.md`): Session refusal at `RequestRoundTravel`, rollback and `StartRolledBack`.
- Story 028 (`production/epics/round-match-framework/story-028-phase-admission-and-illegal-transitions.md`): the admission mode per phase and the illegal-transition table.
- Stories 031 to 034: everything inside Preparing (allocation, barrier, aborts).
- Session internals (travel, seed draw, deadlines) and Roster internals (`ResetAll`, epoch): epics `session-travel` and `participant-state-roster`.

---

## Files

*Planned files. Tests live under the engine test roots (`Source/<Module>/Private/Tests/`, see `tests/README.md`), not under `tests/`. `/dev-story` replaces this list with the files it actually changed.*

- `Source/StealthGameplay/Private/Tests/Round/StartOrderingTest.cpp` (class `FRoundStartOrderingTest`, test name `StealthGame.Round.StartOrdering`)
- `Source/StealthGameplay/Public/Round/` Start-orchestration header (new; Open: name set when implemented)
- `Source/StealthGameplay/Private/Round/Start/` Start-orchestration implementation (new)
- `Source/StealthGame/Private/StealthGameMode.cpp` and `Source/StealthGame/Public/StealthGameMode.h` (`StartRound`, `TryScheduleRoundStart`; retired/replaced by this story)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: An accepted Start calls Session and Roster in the specified order and enters Preparing only after travel acceptance.
  - Given: a valid layout, a call-order spy, and a fake Session whose travel acceptance can be deferred.
  - When: Start is issued, then travel acceptance is released.
  - Then: the spy records exactly `SetAdmissionMode(RejoinOnly)`, `SetTeamsLocked(true)`, `ResetAll(RoundsPlayedCount + 1)`, `RequestRoundTravel`, in that order.
  - Then: the phase is `Lobby` until acceptance and `Preparing` only after it.
  - Edge cases: no extra calls appear between the four; `ResetAll` receives the pre-increment count plus 1.

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Round/StartOrderingTest.cpp` — must exist and pass (automation test `StealthGame.Round.StartOrdering`, class `FRoundStartOrderingTest`)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-003-test-seam-fake-gateways.md`, `production/epics/round-match-framework/story-022-start-validity.md`
- Unlocks: `production/epics/round-match-framework/story-025-start-rollback.md`, `production/epics/round-match-framework/story-026-host-only-commands.md`, `production/epics/round-match-framework/story-027-reset-command-matrix.md`, `production/epics/round-match-framework/story-028-phase-admission-and-illegal-transitions.md`, `production/epics/round-match-framework/story-031-allocate-once.md`, `production/epics/round-match-framework/story-059-session-end-and-abort-during-travel.md`

Cross-epic: `production/epics/session-travel` (`SetAdmissionMode`, `CanRequestRoundTravel`, `RequestRoundTravel`; stories not yet created); `production/epics/participant-state-roster` (`SetTeamsLocked`, `ResetAll`; stories not yet created); `production/epics/tuning-data-config` (module skeleton and StealthTestHarness bootstrap; stories not yet created)
