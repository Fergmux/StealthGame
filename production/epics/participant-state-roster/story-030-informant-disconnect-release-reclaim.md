# Story 030: An informant's released pawn becomes an NPC and a reclaim adopts a different pawn

> **Epic**: Participant State & Roster
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

This story proves the informant disconnect path end to end in the core: when an `Informant` disconnects, Round releases its pawn to the crowd at once (an informant's pawn has no grace value), so the pawn resolves `Npc`, the participant keeps `Informant` state with no live pawn (`AwaitingPawn(T)` +1, `GetPawn(Id)` null); when the participant reclaims and a different Unbound pawn is bound, `AwaitingPawn(T)` falls by one and the new pawn resolves `Informant`. It exists so a disconnecting informant never leaves a retired, untargetable body or a visible hole in the crowd. It realises GDD rules R9 (`ReleaseToCrowd`, crowd adoption), R11 (informant disconnect, rejoin) and F2, and the Edge Case "If an `Informant` disconnects".

**GDD**: `design/gdd/participant-state-roster.md`
**Requirement**: `TR-roster-004` (also `TR-roster-010`)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0010: Shared Pawn: CMC locomotion, spawn factory and possession parity
**Also governed by**: ADR-0005: Session, seamless travel and match-scoped state stores
**ADR Decision Summary**: ADR-0010 commits to one `AStealthPawn` class where no connection event destroys, re-creates or re-spawns a pawn: a disconnect leaves the pawn bound and handed to a crowd controller (`PawnLeavingGame` override, spike S8), and a returning informant receives an adopted existing crowd pawn through `TakeOverCrowdPawn`, which calls `BindPawn` itself. ADR-0005 supplies the rejoin and reclaim rules: reclaim is decided by the Roster key alone and an informant stays an informant.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Core domain with Networking and Physics (HIGH per `architecture.md`). Post-cutoff APIs in the ADR: `UMotionWarpingComponent::GetOwnerAdapter()` (5.8; MotionWarping is Beta) and `UE_LOGF`; none are used by the Roster core. Verification required by the ADR: S8 (`PawnLeavingGame` hand-over, not yet reported), S14, H1, the traversal spike and the ADR-0002 E4 drain-spawn spike. This story is pure-core binding bookkeeping and none of them gates it; the engine-side hand-over is story 056 (blocked on S8).

**Control Manifest Rules (this layer)**:
- Required: Queue, Roster, tuning validator, seed mixer and appearance deck are plain C++ cores (`F…Core`) with no `UWorld` dependency, behind a subsystem facade — source: ADR-0007
- Required: Authoritative match state lives only in the four game-instance stores, keyed by `ParticipantId`, never by controller or PlayerState pointer — source: ADR-0005
- Required: Test names: `StealthGame.<System>.<Scenario>` (T1); T1 tests live in the owning module's `Private/Tests/<System>/` — source: ADR-0008
- Forbidden: Never pass Event Log content, `DealDigest`, server sub-seeds, bindings or mailbox order to `UE_LOG`/`UE_LOGF`/`ensureMsgf`/`checkf`/`ClientMessage`/`TRACE_*`/`CSV_*`/`SCOPE_CYCLE_COUNTER` names — source: ADR-0007
- Forbidden: Never let Session spawn, free, hide or possess a pawn, or call `RequestTransition` — source: ADR-0005
- Guardrail: Tests: per-commit pipeline <= 10 min — source: ADR-0008

---

## Acceptance Criteria

*From GDD `design/gdd/participant-state-roster.md`, scoped to this story:*

- [ ] **AC-PSR-48** [Core] `StealthGame.Roster.InformantDisconnectReleaseReclaim`: GIVEN an Informant with a live pawn, WHEN Session notifies `Disconnected` and Round calls `ReleaseToCrowd`, THEN:
  - the state stays Informant;
  - `AwaitingPawn(T)` rises by 1 and `GetPawn(Id)` = null;
  - the released pawn resolves `Npc`.

  WHEN the participant then reclaims and `BindPawn` adopts a different Unbound pawn, THEN `AwaitingPawn(T)` falls by 1 and that pawn resolves `Informant` (R9, R11, F2).

---

## Implementation Notes

*Derived from ADR-0010 Decision sections 3 and 5 and Implementation Guidelines (`TakeOverCrowdPawn` calls `BindPawn`; a disconnect never destroys a pawn), ADR-0005 section 5, and GDD rules R9, R11, F2:*

- No new API: this story composes `NotifyConnectionChanged(Id, Disconnected, ServerTimeMs)` (story 009), `ReleaseToCrowd(Pawn)` (story 011), the reclaim path of `RegisterParticipant` (story 029) and `BindPawn(Id, Pawn)` (story 010), and fixes any defect the composition exposes in `FRosterCore` (`Source/StealthFoundation/Private/Roster/StealthRosterCore.h/.cpp`).
- Release phase: the informant is Disconnected, so `ReleaseToCrowd(P1)` is accepted (it returns `Rejected_NotDisconnected` only for a Connected participant who is not `RoundEnded`, story 011). It erases the binding outright: P1 becomes `Unbound` and resolves `Npc` through `ResolveVictimKind`; the participant's state stays `Informant`; `GetPawn(Id)` is null; `AwaitingPawn(T)` rises by exactly 1 while `InformantCount(T)` is unchanged (F2).
- Reclaim and adopt phase: `RegisterParticipant` with the same key returns `Reclaimed` (story 029); the informant is still "awaiting pawn". `BindPawn(Id, P2)` with P2 a different, `Unbound`, live pawn returns `Applied`; `AwaitingPawn(T)` falls by exactly 1, and P2 resolves `Informant` (BoundLive + Informant). P2 must differ from P1: the test deliberately does not rebind P1, which is an NPC now.
- Only the Roster's side is tested. In the game, the adopted pawn comes from NPC-Kill Penalty & Informants calling Shared Character & Locomotion's `TakeOverCrowdPawn`, which performs the eligibility check and calls `BindPawn` on Penalty's behalf (R9, ADR-0010 section 3.2); that call chain and the `PawnLeavingGame` hand-over are other epics (`production/epics/npc-kill-penalty-informants/EPIC.md`, `production/epics/shared-character-locomotion/EPIC.md`). The test calls `BindPawn` directly, so no stub of those epics is needed. Open point (qa-lead): because the criterion rests on direct calls, it proves little about the Round, Session and Penalty hand-offs; the integration proof is stories 056 and 059, both blocked on spikes.
- No state event fires on the release or the bind (the informant stays `Informant`); only the reclaim emits `OnParticipantReclaimed` (story 029), so assert `EventSeq` only advances by that one event.
- Test construction: `FRosterCore` with injected fakes from `Private/Roster/StealthRosterDeps.h` (pawn-liveness provider reporting both pawns alive, authority true, game thread, seeded random source, fault sink). The informant with a `BoundLive` pawn comes from stories 014 and 010 (or a seam under `WITH_STEALTH_TEST_SEAMS`).
- Prototype disposition: no prototype counterpart. The prototype has neither informants nor crowd adoption, and nothing is ported.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 011 (Tombstones respect generations and ReleaseToCrowd needs a disconnected participant): `ReleaseToCrowd` itself and its `Rejected_NotDisconnected` result.
- Story 029 (Reclaim restores the same record for active and informant participants): the reclaim result and field-by-field restore.
- Story 009 (Session connection states project onto Connection fields only): `NotifyConnectionChanged`.
- Story 010 (BindPawn rejects an already-bound participant, an in-use pawn and a round-ended participant): `BindPawn` guards.
- Story 056 (A disconnect hand-over never decelerates faster than the crowd stop decel and never destroys the pawn): the engine hand-over (blocked on spike S8).
- Story 059 (A client rejoining within grace reclaims its record, pawn and informant status): the end-to-end multi-process rejoin (blocked on spikes S4 and S8).

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- `Source/StealthFoundation/Private/Roster/StealthRosterCore.h` (only if the composition exposes a defect)
- `Source/StealthFoundation/Private/Roster/StealthRosterCore.cpp` (only if the composition exposes a defect)
- `Source/StealthFoundation/Private/Tests/Roster/StealthRosterInformantDisconnectReleaseReclaimTest.cpp` (class `FRosterInformantDisconnectReleaseReclaimTest`, test name `StealthGame.Roster.InformantDisconnectReleaseReclaim`)
- ADR-0007 header naming: no new header is added; any new one uses the `Stealth` prefix and a project-unique file name.

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation. (At tiers where the QL-STORY-READY gate is skipped — `minimal`, or `lean`/`solo` review mode — no qa-lead specs exist, and this section reads: N/A — no qa-lead specs at this tier; implement against the Acceptance Criteria above.)*

**Automated test specs:**

- **AC-1**: an informant's released pawn becomes an NPC; a reclaim adopts a different pawn.
  - Given: an Informant with a BoundLive pawn P1, and an Unbound pawn P2.
  - When: NotifyConnectionChanged(Disconnected), then ReleaseToCrowd(P1). Then the key reclaims and BindPawn(Id, P2) runs.
  - Then: state stays Informant, AwaitingPawn(T) +1 and GetPawn(Id) null. P1 resolves Npc. After the bind AwaitingPawn(T) -1 and P2 resolves Informant.
  - Edge cases: P2 must differ from P1.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Roster/StealthRosterInformantDisconnectReleaseReclaimTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later). `qa.level` is `full`, so the test is required.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/participant-state-roster/story-029-reclaim-core.md`, `production/epics/participant-state-roster/story-011-tombstone-generation-and-release.md`
- Cross-epic: none directly; the `StealthFoundation` module skeleton from `production/epics/tuning-data-config/EPIC.md` reaches this story through story 001. The production adoption path belongs to `production/epics/npc-kill-penalty-informants/EPIC.md` and `production/epics/shared-character-locomotion/EPIC.md` (no stub needed here).
- Unlocks: `production/epics/participant-state-roster/story-056-disconnect-takeover-blend.md`
