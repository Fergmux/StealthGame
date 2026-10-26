# Story 031: AllocateAppearances is called 0 times with tuning withheld and exactly once after OnTuningReady

> **Epic**: Round & Match Framework
> **Status**: Complete
> **Layer**: Core
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25f
> **Last Updated**: 2026-10-26

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-006` (Preparing readiness sequence, spawn gate and watchdog); GDD AC 8a; rule R6 steps P2 and P3.
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0005: Session, seamless travel and match-scoped state stores
**ADR Decision Summary**: `AGameModeBase` with four game-instance stores (Session, Roster, Round, Tuning preset) as the only authoritative match state, seamless `ServerTravel` with `?Seed=`, single round acknowledgement, admission modes, host commands on the game instance, no host migration. This story applies the Preparing sequencer (stated in ADR-0002 section 8.6, which owns the Round phase flow): P3 runs only on `OnTuningReady`, writes the Event Log header once, then calls `RequestLayout` and `AllocateAppearances(Players, NpcCount)` once each.
**ADR Version**: ADR-0005 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0005 knowledge risk)
**Engine Notes**: None touched. The test runs the Preparing sequencer against fake TDC and Character Pool gateways (story 003) on the fake clock (story 001); no engine API, post-cutoff API or ADR-0005 verification item (S3, S4, S5) is exercised.

**Control Manifest Rules (this layer)** (Core Layer Rules, Character pool and appearance):
- Required: The deal input is an opaque set of `FParticipantId`, sorted by the facade (ADR-0012); Round passes every rostered `ParticipantId`, connected or not.
- Forbidden: Never change `D` or any `c_a` after the deal (ADR-0012); a second `AllocateAppearances` call is a bug (`Error_AlreadyDealt`).
- Guardrail: Character pool deal at most 0.2 ms once per round (ADR-0012).

**Prototype disposition**: Replaces the prototype start-up path in `Source/StealthGame/Private/StealthGameMode.cpp` (`StartRound`, `TryScheduleRoundStart`); the production deal is Character Pool's `AllocateAppearances`, called once from Round after `OnTuningReady`.

---

## Acceptance Criteria

*From GDD `design/gdd/round-match-framework.md` AC 8a, scoped to this story:*

- [ ] GIVEN Preparing with tuning withheld, THEN `AllocateAppearances` is called 0 times; after `OnTuningReady` it is called exactly once.

---

## Implementation Notes

*Derived from ADR-0002 section 8.6 (Preparing sequencer), ADR-0005 and GDD R6:*

- **P2 then P3.** On entering `Preparing`, Round signals "Preparing entered" to TDC; TDC freezes and raises `OnTuningReady(TuningHash)`. Until then Round makes no deal and no layout request. On `OnTuningReady`, Round writes the Event Log header once, then in parallel calls `RequestLayout(Seed, TuningHash)` and `AllocateAppearances(Players, NpcCount)`, where `Players` is every rostered `ParticipantId`, connected or not.
- **Exactly once.** A repeated drain after `OnTuningReady` does not call `AllocateAppearances` again; a duplicate `OnTuningReady` is ignored. A second call to the real allocator would return `Error_AlreadyDealt`, which Round treats as a bug and aborts `AllocatorMisuse` (that abort is story 034).
- **Fakes.** The test uses the fake TDC gateway (withholds tuning until the test raises `OnTuningReady`) and the fake Character Pool gateway that counts `AllocateAppearances` calls; both come from story 003 and record calls in order. Time and `ServerTickIndex` are advanced only by the fake `IClock` (story 001).
- **Where the code goes.** The Preparing sequencer is a step machine in `FRoundPhaseCore`'s Preparing module under `Source/StealthGameplay/Private/Round/Preparing/`; it polls on the Round tick and waits on owner events (no engine timer).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 024 (`production/epics/round-match-framework/story-024-start-ordering.md`): entering `Preparing` after travel acceptance (this story's dependency).
- Story 032 (`production/epics/round-match-framework/story-032-preparing-barrier.md`): layout, acknowledgement and world-ready barrier.
- Story 034 (`production/epics/round-match-framework/story-034-preparing-aborts.md`): `DeckInvalid`, `Error_DeckTooSmall` and `AllocatorMisuse` aborts.
- Character Pool internals (the deal, `ParticipantToAppearance`, deck floor) and TDC internals (freeze, hashing, `OnTuningReady` source).

---

## Files

*Planned files only. Tests live under the engine test roots (`tests/README.md`), not under `tests/`. `/dev-story` replaces this list with the files it actually changed.*

- `Source/StealthGameplay/Public/Round/Preparing/PreparingSequencer.h`
- `Source/StealthGameplay/Private/Round/Preparing/PreparingSequencer.cpp`
- `Source/StealthGameplay/Private/Tests/Round/AllocateOnceTest.cpp` (class `FRoundAllocateOnceTest`, test name `StealthGame.Round.AllocateOnce`)
- `Source/StealthGame/Private/StealthGameMode.cpp` (prototype `StartRound` and `TryScheduleRoundStart`: retired/replaced by this story)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: AllocateAppearances is called 0 times with tuning withheld and exactly once after OnTuningReady.
  - Given: Preparing with a fake TDC gateway that withholds tuning.
  - When: the fake clock advances and drains run, then `OnTuningReady` is raised.
  - Then: the `AllocateAppearances` call count is 0 before `OnTuningReady`.
  - Then: the count is exactly 1 after it.
  - Edge cases: repeated drains after `OnTuningReady` do not call it again; a duplicate `OnTuningReady` is ignored.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Round/AllocateOnceTest.cpp` (automation test `StealthGame.Round.AllocateOnce`, class `FRoundAllocateOnceTest`) — must exist and pass.

**Status**: [x] Created and passing (`StealthGame.Round.AllocateOnce`, 2026-10-26)

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-003-test-seam-fake-gateways.md`, `production/epics/round-match-framework/story-024-start-ordering.md`
- Unlocks: `production/epics/round-match-framework/story-032-preparing-barrier.md`, `production/epics/round-match-framework/story-034-preparing-aborts.md`
- Cross-epic: `production/epics/tuning-data-config` (`OnTuningReady`, module skeleton and StealthTestHarness bootstrap; stories not yet created), `production/epics/character-pool-appearance` (`AllocateAppearances` allocator interface, via the fake gateway; stories not yet created)

---

## Completion Notes
**Completed**: 2026-10-26 (RMF Exit B batch: stories 030-047, Preparing, spawn order and win rules)
**Criteria**: 1/1 passing (0 deals while tuning is withheld; exactly one after OnTuningReady with every rostered id and NpcCount; duplicate OnTuningReady and later drains deal nothing)
**Deviations**: None
**Test Evidence**: `Source/StealthGameplay/Private/Tests/Round/AllocateOnceTest.cpp`; full suite: `production/qa/evidence/rmf-exit-b-preparing-wins-2026-10-26/full-suite.txt` (189/189 Success)
**Run result**: OBSERVED: production/qa/evidence/rmf-exit-b-preparing-wins-2026-10-26/
**Reviewed by**: lead-programmer focused review (sonnet, 15 findings, fixes in production/qa/evidence/rmf-exit-b-preparing-wins-2026-10-26/evidence.md) + self-review, RMF Exit B batch
