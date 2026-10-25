# Story 080: AC-13 RejoinSnapshot (multi-process)

> **Epic**: Tuning Data & Config
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/tuning-data-config.md`
**Requirement**: `TR-tdc-011`, `TR-tdc-006` (text lives in `docs/architecture/tr-registry.yaml`); GDD AC-13 (CR-8, Edge Cases: rejoin)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0004: Tuning data pipeline and validator (section 7); the multi-process tier is ADR-0008 section 1 (T3)
**ADR Decision Summary**: The snapshot property is replicated with `COND_None` (not initial-only) precisely so a rejoiner receives the current snapshot. A rejoining participant keeps pawn continuity: its pawn is not hidden or re-spawned for tuning reasons, and its client issues no commands until its snapshot for the current epoch is valid. The total pawn count must not change through a disconnect or rejoin.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: Needs the Gauntlet host plus client topology of story 020 (ADR-0008 Verification 3 [U]) and `WITH_DEV_AUTOMATION_TESTS` or seam availability in cooked Development processes (Verification 7 [U]; the stubs here compile under `WITH_STEALTH_TEST_SEAMS`, `!UE_BUILD_SHIPPING`, so they must exist in the cooked processes). Rests on ADR-0004 Verification Required (1) [U] (legacy atomic delivery including to a rejoiner; spike story 071). How a client process disconnects and reconnects under Gauntlet is [U] topology detail not described in the inputs. Pawn continuity and the rejoin window are Session's rules (ADR-0005; Session rule 22).

**Control Manifest Rules (this layer)**:
- Forbidden: never prove multi-process criteria in in-process PIE only — source: ADR-0008
- Required: server tuning-ready bookkeeping is keyed by `ParticipantId`, never replicated, cleared at every epoch — source: ADR-0004
- Required: a rejoiner adopts its retained PlayerState in `InitPlayerState`; `OnDeactivated` does not destroy the PlayerState while admission is `RejoinOnly` — source: ADR-0005 (Session's half, stubbed here)
- Required: T2/T3 tests are bounded by frames or server time and assert and record the active replication system — source: ADR-0008

---

## Acceptance Criteria

*From GDD `design/gdd/tuning-data-config.md` AC-13, scoped to this story:*

- [ ] **AC-13 (CR-8, Edge rejoin)**: GIVEN a multi-process client that disconnects during `Active` and rejoins within Session's window, WHEN it reconnects, THEN all of the following hold:
  - it receives the current snapshot with the round's original `RoundIndex` and `ClientHash`;
  - no command from it is accepted before its snapshot is valid;
  - its pawn is neither despawned nor respawned for tuning reasons;
  - the other clients' `Active` tick count and state are unchanged against a control run.
  (`StealthGame.Tuning.MP.RejoinSnapshot`)

*Per amendment (story 080 REVISED): Session's rejoin window is the stub's `RejoinWindowSec`, and "no command accepted before the snapshot is valid" is asserted through the stub's `AcceptCommand` gate (story 052).*

---

## Implementation Notes

*Derived from ADR-0004 sections 5-7, GDD CR-8, CR-9 and Edge Cases (disconnect, rejoin), ADR-0008 sections 1, 2, 5, and global amendments G1, G4:*

- **Scenario and node.** `Source/StealthTestHarness/Private/Scenarios/RejoinSnapshotScenario.cpp` (named scenario `RejoinSnapshot`) and the Gauntlet node `Build/Scripts/StealthTuningRejoinNode.cs`. Test name `StealthGame.Tuning.MP.RejoinSnapshot`. One cooked Development listen host plus at least two client processes (the disconnecting client and an observer whose Active tick count is compared) and a control run without a disconnect. Result JSON per story 018, with `replicationSystem=legacy` asserted per story 020.
- **Stubs in cooked processes.** `FStubRound` and `FStubSession` (story 052, `Source/StealthFoundation/Public/Server/Testing/`, G1) must exist in the cooked Development processes (QA spec edge case). `FStubSession.RejoinWindowSec` supplies the window and `AcceptCommand(participant)` returns false until that participant's tuning is valid; the scenario issues a command from the rejoiner repeatedly from reconnect and asserts every call before snapshot validity is refused, then accepted after.
- **Observations required.** (1) The rejoiner's received snapshot has the round's original `RoundIndex` (not a new epoch) and its recomputed `ClientHash` equals the server's for that epoch; (2) `AcceptCommand` false until the snapshot is valid; (3) its pawn is neither despawned nor respawned for tuning reasons (compare the pawn's identity and spawn count before and after; the pawn class used by the stub Round is open, as in story 076); (4) the observer's Active tick count and state equal those of a control run with no disconnect (compare against the control from the same scenario; ticks measured on server time or frame count, never wall clock).
- **Rejoin after the window (QA edge case).** A rejoin after `RejoinWindowSec` is not a rejoin and must not receive a snapshot as one; assert it explicitly with a second client or a second pass.
- **Pawn-count rule.** GDD Edge Cases: the total pawn count must not change through any disconnect or rejoin (else pawns minus `NpcCount` would reveal the human count). Record the pawn count before disconnect and after rejoin in the capture; this is TDC's observation, the owning rule is a change request to Round and Crowd.
- **Epoch.** The snapshot property replicates with `COND_None`, so the rejoiner receives it on connect; the epoch is `RoundIndex` plus `Revision` from the snapshot, and the old `RoundIndex` must not change on rejoin. Disconnect drops the participant's epoch-keyed tuning-ready entry; Session's absolute deadline is not reset by the rejoin (Session's rule, stubbed).
- **Deferral and fallback.** DEFERRED: multi-process (nightly) per the QA spec, runnable once stories 020 and 073 are Done. It is not proven in in-process PIE.
- **Prototype.** None.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/tuning-data-config/story-073-gamestate-snapshot-public-only.md`: the replicated property and public-only checks
- `production/epics/tuning-data-config/story-071-snapshot-atomic-spike.md`: atomic delivery and a rejoiner in single-process PIE (spike T-1)
- `production/epics/tuning-data-config/story-076-no-pawn-before-ready.md`: no pawn for a first-entry participant before Ready (in-process)
- `production/epics/tuning-data-config/story-081-travel-epoch.md`: the epoch across `ServerTravel`
- `production/epics/tuning-data-config/story-052-round-session-stubs.md`: the stubs (`RejoinWindowSec`, `AcceptCommand`)
- `production/epics/tuning-data-config/story-020-gauntlet-multiprocess-smoke.md`: the Gauntlet host plus client smoke

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthTestHarness/Private/Scenarios/RejoinSnapshotScenario.cpp`
- `Build/Scripts/StealthTuningRejoinNode.cs`

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: a rejoining client receives the current snapshot, issues no accepted command before it is valid, keeps its pawn, and leaves other clients unchanged
  - Given: a client that disconnects during `Active` and rejoins within `FStubSession.RejoinWindowSec`; a control run without a disconnect (DEFERRED: multi-process; test `StealthGame.Tuning.MP.RejoinSnapshot`)
  - When: it reconnects
  - Then: it receives the current snapshot with the original `RoundIndex` and `ClientHash`; `AcceptCommand` is false until its snapshot is valid; its pawn is neither despawned nor respawned for tuning reasons; other clients' Active tick count and state equal the control run
  - Edge cases: a rejoin after the window is not a rejoin; the stubs must exist in the cooked processes

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: the T3 scenario `Source/StealthTestHarness/Private/Scenarios/RejoinSnapshotScenario.cpp` with node `Build/Scripts/StealthTuningRejoinNode.cs` (`StealthGame.Tuning.MP.RejoinSnapshot`) must exist and pass (nightly, deferred multi-process), with its result JSON retained, including the control run

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-073-gamestate-snapshot-public-only.md`, `production/epics/tuning-data-config/story-020-gauntlet-multiprocess-smoke.md`, `production/epics/tuning-data-config/story-052-round-session-stubs.md`
- Unlocks: None
- Open verification claims: ADR-0008 Verification 3 (Gauntlet topology) [U] and 7 (`WITH_DEV_AUTOMATION_TESTS` in cooked Development) [U]; ADR-0004 Verification Required (1) [U] (rejoiner receives the current snapshot). Open: how a client process disconnects and reconnects under the node, and the stub pawn class.
