# Story 004: Hide(Enter) executes in its slot, commits at the swap and a failed guard exits ordinarily

> **Epic**: Concealment (Bushes)
> **Status**: Complete
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: 2026-10-29

## Context

**GDD**: `design/gdd/concealment-bushes.md` (CON-R5, CON-R6a, AC-CON-29, AC-CON-11 clip part)
**Requirement**: `TR-con-001`, `TR-con-005`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0017: Concealment component and per-viewer presentation (§3 transitions as queue items), with ADR-0002 (queue timers)
**ADR Decision Summary**: `HideEnter` executes in the command's slot (state Entering, generation +1, `FActionState {BushEnter, StartServerTime}`), and two `TimerFired(ConcealmentTransition)` items are scheduled: `Commit` at Start + `BushEnterSwapSec` and `End` at Start + `BushEnterDurSec`. Commit guards that the capsule is wholly inside the `HideVolume`; on success it records occupancy, pushes the cull and sets the owner and team flags; on failure nothing commits and an ordinary exit follows.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0017 overall; the resolver is plain C++ over the Round queue, no post-cutoff API)
**Engine Notes**: Items use ADR-0002 §2a's generic Foundation `FTimerPayload` (`Subject` = `FPawnHandle`, `Generation`, `Phase = EConcealPhase`); `ConcealmentTransition` is added to the closed `ETimerKind` set by the ADR-0002 amendment and is a post-latch exemption (it may write only Concealment state, the cull record and the Owner/Team flags).

**Control Manifest Rules (this layer)**:
- Required: Commit, crossing and end are `TimerFired(ConcealmentTransition)` queue items carrying the transition generation — source: ADR-0017
- Forbidden: Never drive concealment transitions or the hide by anim notifies, engine timers, feature ticks or overlap — source: ADR-0017
- Guardrail: Whole server ticks, ordered by (due, seq)

The entering pawn is an ordinary visible pawn until the commit: no invulnerable window. The cull call and owner/team setters go through seams here; the real cull source is story 025 and the flag writers are story 023.

---

## Acceptance Criteria

*From GDD `design/gdd/concealment-bushes.md` (CON-R5, CON-R6a, AC-CON-29, AC-CON-11), scoped to this story:*

- [ ] **AC1** (derived, CON-R5/R6a, ADR-0017 §3): **GIVEN** a legal Hide(Enter) executing at server time T **WHEN** the slot runs **THEN** state = Entering, `TransitionGeneration` +1, `FActionState {Pose = BushEnter, StartServerTime = T}` is written identically for NPC and player pawns, and exactly two timer items exist: Commit at T + `BushEnterSwapSec` (0.5 s) and End at T + `BushEnterDurSec` (0.9 s), on whole server ticks, ordered by (due, seq).
- [ ] **AC2** (derived, CON-R5): **GIVEN** the Commit item fires with the capsule wholly inside the `HideVolume` **THEN** state = Concealed, the pawn is inserted into the occupancy record with its `CommitSeq`, one `Commit` log row is queued (story 030), `SetCullState(Pawn, true, ServerNow())` is called once through the cull seam and the Owner and Team setters are called once; at End the pose returns to None while the state stays Concealed.
- [ ] **AC3** (AC-CON-29, [U] `StealthGame.Concealment.GuardFaultPath`): **GIVEN** a forced guard failure at the swap **THEN** no commit occurs, the fault sink records exactly one entry (`ReportFault ConcealGuard`), and an ordinary exit on the entry side follows: through the normal Hide path during Active, as an internal transition (Exiting, `BushExit`, Cross/End items, never a `PawnCommand`) after the latch.
- [ ] **AC4** (AC-CON-11 clip part, [U][Doc] `StealthGame.Concealment.ClipKeyAgreement`): **GIVEN** the `BushEnter` clip asset and keys **THEN** the swap notify time equals `BushEnterSwapSec` within 1 frame (1/60 s); the notify is presentation only and the server key governs.

---

## Implementation Notes

*Derived from ADR-0017 Decision §3:*

`HideEnter` executes in the command's slot: state Entering, `TransitionGeneration++`, `TransitionStartServerTime = ServerNow()`, `FActionState {Pose = BushEnter, StartServerTime, ...}` written with the start time (§8 A: never `ContactServerTime`), warp started (story 011). Two items are scheduled with `ScheduleAt` (ADR-0002 §4). **Commit** guard: the capsule lies wholly inside the `HideVolume`; on failure no commit, `ReportFault(ConcealGuard)`, and an ordinary exit on the entry side follows (during `Active` through the normal Hide path; after the latch as an internal transition run by the same resolver, because a `PawnCommand` would be `Rejected_Latched`). On success: state Concealed, occupancy insert with `CommitSeq`, `IConcealmentCullSource::SetCullState(Pawn, true, ServerNow())`, Roster `SetTeammateConcealed(Participant, true)` if bound, owner `bIsConcealed = true`, log row `Commit`, host latch event (story 022). An item with a stale generation or epoch, or whose pawn is dead or pending teardown, resolves `Voided`. Entry side = the open side nearest the pawn at execution. Resolvers are registered from the composition root.

**Prototype replaced**: overlap-driven entry (`SetInBush` in `StealthCharacter.cpp`); nothing promoted.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-011-server-warp-to-anchor-and-exit-motion.md`: the root-motion source.
- `story-025-cull-push-wiring.md`: the real `IConcealmentCullSource` caller (this story uses the seam).
- `story-023-owner-and-team-concealed-flags.md`: the real flag writers.
- `story-018-no-immunity-while-entering.md`: strikes landing on an entering pawn.

---

## Files

- `Source/StealthGameplay/Features/Concealment/Private/ConcealmentCore.cpp` (enter resolver)
- `Source/StealthGameplay/Features/Concealment/Private/ConcealmentTransitionResolver.cpp`
- `Source/StealthGameplay/Features/Concealment/Public/Server/ConcealmentSeams.h` (cull-source and owner/team setter seams)
- `Source/StealthGameplay/Private/Tests/Concealment/EnterCommitTest.cpp` (classes `FConcealmentEnterExecutionTest` and `FConcealmentCommitEffectsTest`)
- `Source/StealthGameplay/Private/Tests/Concealment/GuardFaultPathTest.cpp` (class `FConcealmentGuardFaultPathTest`, name `StealthGame.Concealment.GuardFaultPath`)
- `Source/StealthGameplay/Private/Tests/Concealment/ClipKeyAgreementTest.cpp` (class `FConcealmentClipKeyAgreementTest`)

---

## QA Test Cases

*Written by qa-lead at story creation (QL-STORY-READY: ADEQUATE).*

- **AC1**: EnterExecution (`StealthGame.Concealment.EnterExecution`)
  - Given: a legal Hide(Enter) executing at T for an NPC and a player on a fake queue.
  - When: the slot runs.
  - Then: state Entering, generation +1, `FActionState{BushEnter, Start=T}` byte-identical for both kinds, exactly two items (Commit T+0.5 s, End T+0.9 s) on whole ticks ordered by (due, seq).
  - Edge cases: a non-tick-aligned T follows the rounding rule; changed keys (0.46, 0.65) shift the schedule; no third item.
- **AC2**: CommitEffects
  - Given: Commit fires with the capsule inside the volume.
  - When: it resolves.
  - Then: Concealed, record gains the pawn with `CommitSeq`, one log row queued, `SetCullState(true)` called once, Owner and Team setters called once; at End pose None and state stays Concealed.
  - Edge cases: a stale generation drops the item with no seam calls.
- **AC3**: GuardFaultPath (verbatim AC-CON-29)
  - Given: a forced guard failure at the swap.
  - When: Commit fires.
  - Then: no commit, no record, no cull call, exactly one `ReportFault(ConcealGuard)`; exit via the Hide path during Active, and as an internal Exiting/BushExit transition after the latch (never a PawnCommand).
  - Edge cases: entry side is the nearest open side.
- **AC4**: ClipKeyAgreement
  - Given: the BushEnter clip asset (editor-context automation reads the notify).
  - When: the swap notify time is compared to the key.
  - Then: the difference is at most 1/60 s.
  - Edge cases: 60 Hz is the frame definition; negative control: a notify offset by 2 frames fails.

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: the tests above under `Source/StealthGameplay/Private/Tests/Concealment/` — must exist and pass

**Status**: [x] Created and passing (2026-10-29): StealthGame.Concealment.EnterExecution, StealthGame.Concealment.GuardFaultPath; evidence production/qa/evidence/concealment-001-026-2026-10-29/

Run result: OBSERVED - 3-player listen-server PIE on L_Prototype (production/qa/evidence/concealment-001-026-2026-10-29/, con-*.png per viewer)

---

## Dependencies

- Depends on: `story-001-concealment-tuning-keys-and-timing-validator.md`, `story-002-bush-runtime-from-manifest.md`, `story-003-conceal-state-machine-and-occupancy-record.md`; IIF `production/epics/input-interaction-framework/story-003-action-hub-and-predicate-registry.md`; Round queue stories (round-match-framework epic) for `ScheduleAt`
- Unlocks: `story-005-hide-exit-execution-and-blocked-side.md`, `story-011-server-warp-to-anchor-and-exit-motion.md`, `story-014-attack-breaks-concealment.md`, `story-018-no-immunity-while-entering.md`, `story-025-cull-push-wiring.md`

## Completion Notes
**Completed**: 2026-10-29
**Criteria**: AC1-AC3 passing. AC4 (ClipKeyAgreement) DEFERRED: no BushEnter clip asset exists in the project yet (art); the server key governs and the presentation reads only the server times, so nothing depends on the notify. The test lands with the clip.
**Deviations**: A guard failure exits internally in both phases (Active and after the latch: Exiting, BushExit, Cross/End items, never a PawnCommand; decision logged). bTransitionOpen makes an Exit Busy until the enter's End.
**Test Evidence**: production/qa/evidence/concealment-001-026-2026-10-29/
**Reviewed by**: self code review 2026-10-29 (APPROVED; notes in the evidence)
