# Story 011: Tombstones respect generations and ReleaseToCrowd needs a disconnected participant

> **Epic**: Participant State & Roster
> **Status**: Complete
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25g
> **Last Updated**: 2026-10-25

## Context

**GDD**: `design/gdd/participant-state-roster.md`
**Requirement**: `TR-roster-004`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0010: Shared Pawn: CMC locomotion, spawn factory and possession parity
**Also governed by**: ADR-0005: Session, seamless travel and match-scoped state stores
**ADR Decision Summary**: ADR-0010 section 5 says a disconnect never destroys a pawn: `PawnLeavingGame` hands the pawn to a crowd controller in the same frame and leaves the binding untouched, and Round later calls `ReleaseToCrowd` when grace expires. The Roster's part is the binding table: a tombstone must be keyed by object identity plus a generation so a recycled pawn never inherits it, and `ReleaseToCrowd` must be refused for a connected participant. ADR-0005 adds that the strike-limit path runs the same release inside the transition call (an internal path where the not-disconnected rule does not apply).
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: ADR-0010 (HIGH): post-cutoff `UMotionWarpingComponent::GetOwnerAdapter()` (Beta plugin); outstanding verification S8 (the `PawnLeavingGame` override keeps the pawn and a crowd AI controller possesses it in the same server frame), S14, H1, ST1, P1 and the traversal spike. This story tests only the Roster's binding bookkeeping through an injected liveness provider, so none of them is exercised. ADR-0005 (HIGH): S3, S4, S5 outstanding, untouched.

**Context paragraph**: This story makes tombstones generation-aware and adds `ReleaseToCrowd(Pawn)`. A `Retired` handle whose object returns with a new generation must resolve as an NPC, and releasing a Disconnected participant's `BoundLive` or `Retired` pawn must make it `Unbound` (an NPC) while releasing a Connected participant's pawn returns `Rejected_NotDisconnected`. It realises GDD R9 (`ReleaseToCrowd`, tombstone key, results on wrong input), R10 (`ResolveVictimKind` meaning of an NPC) and invariant I5.

**Control Manifest Rules (this layer)**:
- Required: Every queue item payload, staged-write payload, `FPawnHandle` and `FSubjectRef` is declared in `StealthFoundation/Public/Server/` (ADR-0002, ADR-0007)
- Required: Queue, Roster, tuning validator, seed mixer and appearance deck are plain C++ cores (`F…Core`) with no `UWorld` dependency, behind a subsystem facade (ADR-0007)
- Required: Authoritative match state lives only in the four game-instance stores, keyed by `ParticipantId`, never by controller or PlayerState pointer (ADR-0005)
- Forbidden: Never let Session spawn, free, hide or possess a pawn, or call `RequestTransition` (ADR-0005)
- Forbidden: Never pass Event Log content, `DealDigest`, server sub-seeds, bindings or mailbox order to `UE_LOG`/`UE_LOGF`/`ensureMsgf`/`checkf`/`ClientMessage`/`TRACE_*`/`CSV_*`/`SCOPE_CYCLE_COUNTER` names (ADR-0007)
- Guardrail: Queue drain + staging + teardown + Roster flush: <= 0.5 ms listen-host game thread (ADR-0002)
- Required (manifest 2026-10-25f, ADR-0005 Technical Setup 2026-10-25b): `FRosterCore::RequestTransition` is the only writer of lifecycle state with the R8 precedence; mutations on the server game thread with FIFO dispatch after the outermost call and EventSeq +1 per event; O(1) allocation-free queries through a pre-sized binding table; client queries return 0/false/NotTargetable(Stale) and fault (re-read at implementation; 2026-10-25g adds nothing for the Roster)

---

## Acceptance Criteria

*From GDD `design/gdd/participant-state-roster.md`, scoped to this story:*

- [x] **AC-PSR-24** [Core] `StealthGame.Roster.TombstoneGenerationAndRelease`: GIVEN a Retired handle, WHEN the same object returns with a new generation, THEN `Npc`. WHEN `ReleaseToCrowd` is called on a Disconnected participant's BoundLive or Retired pawn, THEN `Npc` and `IsPawnBound` = false. WHEN `ReleaseToCrowd` is called on a Connected participant's pawn, THEN `Rejected_NotDisconnected` (R9, I5).

---

## Implementation Notes

*Derived from ADR-0010 section 5 (disconnect never destroys a pawn), ADR-0005 section 2 (strike-limit release inside the transition) and GDD R9, R10, I5:*

- **Tombstone key.** A tombstone is keyed by the pawn's `ObjectKey` plus the `Generation` it had when retired (R9). A lookup with the same `ObjectKey` and a different generation must miss, so the pawn resolves as `Unbound` (an NPC). I5 says a tombstone never becomes live or unbound except via `ClearBinding`, `ReleaseToCrowd`, `InvalidateAllBindings` or `ResetAll`; the generation mismatch is a *miss*, not a mutation of the tombstone.
- **Resolution used by the test.** `ResolveVictimKind` is story 013; here assert through `IsPawnBound` and a minimal internal `GetBindingStatus` helper (planned name) that story 013 builds on, plus the `Npc` answer as the AC states it. Keep the helper O(1) and allocation-free, since story 033 asserts that.
- **ReleaseToCrowd(Pawn).** Erases a live **or** retired binding outright so the pawn becomes `Unbound`. Result codes: `Applied` (pawn was bound or retired), `NoOp_NotBound` only if the GDD's wrong-input table implies one for it (it lists `ReleaseToCrowd` only for `Rejected_NotDisconnected`; do not invent others), `Rejected_NotDisconnected` when the bound participant is `Connected` and not `RoundEnded` (R9: a connected player's pawn can never be handed to the crowd). The shared precheck (authority, thread) applies first.
- **Disconnected means the Connection projection.** Use the participant's `Connection` from story 009's `NotifyConnectionChanged` to build the Disconnected and Connected fixtures (that is why this story depends on 009). `Dormant` participants are not part of this AC.
- **Retired fixture.** `RetireBinding` is story 020 and the transition that retires a binding is story 014 (which depends on this story), so build the `Retired` handle through the core test seam from story 010 (`WITH_STEALTH_TEST_SEAMS`, never in Shipping) until then.
- **Internal release path.** `RequestTransition(..., StrikeLimit)` runs its own release in the same call and the `Rejected_NotDisconnected` rule does not apply to that path (R8, ADR-0005). Structure `ReleaseToCrowd` as a public wrapper with the connection check plus a private `ReleaseBindingInternal` that story 014 calls for `StrikeLimit`.
- **Same-call-stack rule.** For a disconnect expiry Round calls `RequestTransition(..., Disconnect)` and `ReleaseToCrowd` in one call stack so a rejoin cannot land between them (R9); that is Round's discipline, tested in story 054.
- **I5 recycled pawn.** A pawn object that is recycled after a `ClearBinding` or release must never inherit an old tombstone: the new generation guarantees it, which is the second clause of the QA edge case.
- **Prototype disposition:** No prototype counterpart (the prototype destroyed a disconnected player's character via the engine default `PawnLeavingGame`).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 009 (Session connection states project onto Connection fields only): the `Connection` projection used to build fixtures
- Story 010 (BindPawn rejects an already-bound participant, an in-use pawn and a round-ended participant): the binding table and `BindPawn`
- Story 012 (Clearing a live binding during Active faults, but level-teardown clears are silent): `ClearBinding` and `InvalidateAllBindings`
- Story 013 (ResolveVictimKind answers the full pawn-state matrix and never yields Npc for NotTargetable): the full `ResolveVictimKind` evaluation order
- Story 014 (Active to Informant applies for each entering reason with the correct pawn disposition): the `StrikeLimit` internal release
- Story 020 (Killing an informant retires its binding without a state event): `RetireBinding`
- Story 030 (An informant's released pawn becomes an NPC and a reclaim adopts a different pawn): informant release and re-adoption
- Story 056 (A disconnect hand-over never decelerates faster than the crowd stop decel and never destroys the pawn): the real hand-over blend (Blocked on spike S8)

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- `Source/StealthFoundation/Private/Roster/StealthRosterCore.h` (`FRosterCore`, `FParticipantRecord` and its X-macro field list, result/event/call enums, core test seams under `WITH_STEALTH_TEST_SEAMS`)
- `Source/StealthFoundation/Private/Roster/StealthRosterCore.cpp`
- `Source/StealthFoundation/Private/Roster/StealthRosterDeps.h` (pawn handle, `IRosterPawnLiveness`, `IRosterAuthority`, `IRosterThreadCheck`, `IRosterRandom`, `IRosterRoundPhase` (D43), `FRosterCoreDeps`; clock and fault sink reuse `IStealthClock`/`IStealthFaultSink`)
- `Source/StealthFoundation/Public/View/StealthRosterTypes.h` (`FParticipantId`, `FIdentityKey`, `EParticipantState`, `ERosterTeam`)
- `Source/StealthFoundation/Public/Server/StealthFaultSink.h` (appended `RosterNotAuthority`=5, `RosterWrongThread`=6, `RosterLiveBindingCleared`=7, `RosterUnrecognisedState`=8)
- `Source/StealthFoundation/Private/Tests/Roster/StealthRosterTestFixture.h` (shared fakes and fixture; reuses `FFakeStealthClock`, `FStealthRecordingFaultSink`)
- `Source/StealthFoundation/Private/Tests/Roster/StealthTombstoneGenerationAndReleaseTest.cpp` (test `StealthGame.Roster.TombstoneGenerationAndRelease`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation. (At tiers where the QL-STORY-READY gate is skipped — `minimal`, or `lean`/`solo` review mode — no qa-lead specs exist, and this section reads: N/A — no qa-lead specs at this tier; implement against the Acceptance Criteria above.)*

**For Logic / Integration stories — automated test specs:**

- **AC-1**: tombstones respect generations; ReleaseToCrowd needs a disconnected participant.
  - Given: a Retired handle {Key, Gen1}; Disconnected and Connected participants (via NotifyConnectionChanged).
  - When: the same object returns as {Key, Gen2}; ReleaseToCrowd on a Disconnected participant's BoundLive and Retired pawns; ReleaseToCrowd on a Connected participant's pawn.
  - Then: Gen2 handle resolves Npc. After release the pawn resolves Npc and IsPawnBound false. The Connected call returns Rejected_NotDisconnected, binding unchanged.
  - Edge cases: a recycled pawn never inherits the tombstone (I5).

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: automated test `Source/StealthFoundation/Private/Tests/Roster/StealthTombstoneGenerationAndReleaseTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later). `qa.level` is full, so the test is required.

**Status**: [x] Created and passing (`StealthGame.Roster.TombstoneGenerationAndRelease`, 2026-10-25)

---

## Dependencies

- Depends on: `production/epics/participant-state-roster/story-009-connection-projection.md`, `production/epics/participant-state-roster/story-010-bind-pawn-guards.md`
- Unlocks: `production/epics/participant-state-roster/story-013-resolve-victim-kind-matrix.md`, `production/epics/participant-state-roster/story-014-transition-active-to-informant.md`, `production/epics/participant-state-roster/story-022-reset-all-batch.md`, `production/epics/participant-state-roster/story-030-informant-disconnect-release-reclaim.md`

---

## Completion Notes
**Completed**: 2026-10-25 (participant-state-roster batch 001-014: one core built and closed together)
**Criteria**: 1/1 passing (AC-PSR-24: {K,Gen2} of a Retired {K,Gen1} resolves Npc and is unbound while the tombstone stays; ReleaseToCrowd on a Disconnected participant's BoundLive and Retired pawns -> Npc, IsPawnBound false, GetPawn null; a Connected participant's pawn Rejected_NotDisconnected with the binding unchanged; I5 recycled object binds fresh)
**Decisions**: The tombstone miss by generation is structural (table keyed by object + generation). `ReleaseToCrowd` on an Unbound pawn returns NoOp_NotBound (the GDD lists no code; mirrors ClearBinding). Private `ReleaseBindingInternal` serves the StrikeLimit path (story 014).
**Deviations**: None. Manifest Version updated to 2026-10-25g.
**Test Evidence**: `production/qa/evidence/story-011-tombstone-generation-and-release/full-suite.txt` (build `Result: Succeeded`; 84/84 `StealthGame.` tests Success, 70 existing + 14 Roster) and `stamp.txt`
**Run result**: N/A — plain-C++ server core with no world, actor or screen; nothing a player can see until the facade (story 061) and the C-TeamView push step (story 062) exist
**Reviewed by**: focused code review (self-applied, unattended run): one defect fixed (StrikeLimit release aliasing, story 014); `TryReclaim`, `DisposeLivePawnOnElimination` and `EmitStateChanged` extracted to keep methods short; `RequestTransition` kept as one function by design (the R8 precedence in one place)
