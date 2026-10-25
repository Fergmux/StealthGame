# Story 075: AC-11 StaleEpochIgnored

> **Epic**: Tuning Data & Config
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/tuning-data-config.md`
**Requirement**: `TR-tdc-011` (text lives in `docs/architecture/tr-registry.yaml`); GDD AC-11 (CR-9)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0004: Tuning data pipeline and validator (section 7 and Implementation Guideline 12)
**ADR Decision Summary**: Server-side tuning-ready bookkeeping is keyed by `ParticipantId`, never replicated (a replicated "acked" flag would publish the human roster), and cleared at every epoch. The server compares a received `ClientHash` against its own for the same epoch (`RoundIndex` plus `Revision`); a stale-epoch hash never marks a participant Ready. Stale and duplicate handling beyond TDC's own data belongs to Session.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: Rests on the replicated snapshot of story 073 (ADR-0004 Verification Required (1) [U], spike story 071). The reflection half (no replicated property reveals ready state) is class-level reflection over `AStealthGameState` and `AStealthPlayerState`; no post-cutoff API. Whether the test needs PIE at all is open (see Implementation Notes).

**Control Manifest Rules (this layer)**:
- Required: server tuning-ready bookkeeping is keyed by `ParticipantId`, never replicated, cleared at every epoch — source: ADR-0004
- Forbidden: never put team, state, connection, appearance or pawn data on a PlayerState; never store anything authoritative on a PlayerController or PlayerState — source: ADR-0001, ADR-0005
- Required: ack records, kick list, silence clocks and connection maps are keyed by `ParticipantId` and never replicated — source: ADR-0005
- Required: every leak gate includes a negative control the suite detects — source: ADR-0008

---

## Acceptance Criteria

*From GDD `design/gdd/tuning-data-config.md` AC-11, scoped to this story:*

- [ ] **AC-11 (CR-9)**: GIVEN a `ClientHash` for a stale epoch (old `RoundIndex` or `Revision`), WHEN the server compares it, THEN it is reported as not matching the current epoch and never marks the participant tuning-`Ready`. AND no replicated property on any PlayerState or GameState reveals any participant's tuning-ready state (reflection). (`StealthGame.Tuning.StaleEpochIgnored`)

---

## Implementation Notes

*Derived from ADR-0004 section 7 and Implementation Guideline 12, GDD CR-9 and its Edge Cases, ADR-0008 section 2:*

- **Production code.** `Source/StealthFoundation/Private/Tuning/StealthTuningStore.cpp`: `GetServerClientHash(int32 RoundIndex, int32 Revision)` (the signature in the ADR Key Interfaces for `UStealthTuningStore`) returns the server's `ClientHash` only for the current epoch and reports a mismatch for any other. The tuning-ready bookkeeping is a `ParticipantId`-keyed map held in the game-instance store (never by connection, PlayerController or PlayerState, which are replaced at seamless travel and on rejoin), never replicated, and cleared when a new epoch begins. A participant becomes tuning-Ready only through a comparison against the current epoch's server hash that matches.
- **Acceptance path.** TDC does not re-implement Session's acknowledgement: Session reports an acknowledgement accepted and the participant becomes Ready (GDD CR-9, `OnParticipantTravelComplete`). In this epic the Session behaviour is the stub (`FStubSession`, story 052), so this story tests TDC's half: the server's comparison and bookkeeping, driven through the store API and the stub.
- **Test body.** (1) With the store at a current `(RoundIndex, Revision)`, submit a `ClientHash` computed for an old `RoundIndex`, then one for an old `Revision`; each must report not matching the current epoch and the participant must not be Ready. (2) Control: submit the correct hash for the current epoch; the participant is Ready (a test that never sees Ready proves nothing). (3) Reflection: diff the replicated property lists of `AStealthGameState` and every `PlayerState` class before and after the participant becomes Ready (and between the control and the stale cases); nothing may change or encode ready state, and no replicated property may be a per-participant ready flag.
- **Test form is open.** The AC names the test `StealthGame.Tuning.StaleEpochIgnored` (no `.PIE` segment) while the plan places its file at `Source/StealthTestHarness/Private/Tests/PIE/StaleEpochIgnoredTest.cpp`. A CQTest `NETWORK_TEST_CLASS(..., "StealthGame.Tuning.PIE")` would be named `StealthGame.Tuning.PIE.StaleEpochIgnored.<Method>`. Decide at implementation which form to use and record it: the store comparison and class reflection do not need a live PIE session, so a simple automation test keeping the AC's name is possible; if a PIE session is used, the full name gains `.PIE.` and the AC's test name is updated by note, not silently.
- **Epoch bookkeeping facts.** `Epoch` = `RoundIndex` plus `Revision`, both read from the snapshot (GDD CR-9). A client sends its echo only after it holds the current epoch's snapshot, so no early echo is queued (GDD Edge Cases). Duplicate acknowledgements and the disconnect path belong to Session.
- **Seams.** `FStubSession` (story 052) and a fault sink (story 008); a stale epoch reports through the fault sink if the store routes it there (ADR-0008 section 4 lists "stale epoch" among fault-sink reports); the test declares expected faults explicitly and asserts none unexpected at teardown.
- **Prototype.** None.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/tuning-data-config/story-074-client-hash-handoff.md`: the client-side single hand-off per epoch
- `production/epics/tuning-data-config/story-081-travel-epoch.md`: multi-process travel, old-epoch hash across a real `ServerTravel`, and bookkeeping empty at the start of the new epoch
- `production/epics/tuning-data-config/story-076-no-pawn-before-ready.md`: no pawn before Ready
- `production/epics/tuning-data-config/story-052-round-session-stubs.md`: the Session stub

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthFoundation/Private/Tuning/StealthTuningStore.cpp`
- `Source/StealthTestHarness/Private/Tests/PIE/StaleEpochIgnoredTest.cpp`

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: a stale-epoch `ClientHash` never marks a participant Ready, and no replicated property reveals ready state
  - Given: the server store at a current `(RoundIndex, Revision)` (test `StealthGame.Tuning.StaleEpochIgnored`)
  - When: a `ClientHash` for an old `RoundIndex`, then for an old `Revision`, is submitted; replicated properties of every PlayerState and GameState are diffed before and after Ready
  - Then: each is reported as not matching the current epoch; the participant is never marked tuning-Ready; no replicated property changes with ready state
  - Edge cases: the correct hash for the current epoch marks Ready (control)

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/Tests/PIE/StaleEpochIgnoredTest.cpp` (`StealthGame.Tuning.StaleEpochIgnored`, or its CQTest form per the open decision above) must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-074-client-hash-handoff.md`
- Unlocks: None
- Open verification claims: none from plan Note lines (the plan has none for 075). Open: the test form (simple automation test versus CQTest) and therefore the exact test name.
