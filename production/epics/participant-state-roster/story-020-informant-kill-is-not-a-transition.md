# Story 020: Killing an informant retires its binding without a state event

> **Epic**: Participant State & Roster
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

This story delivers `RetireBinding`, the call NPC-Kill Penalty & Informants makes when an informant's body is killed. Killing an informant is deliberately not a lifecycle transition: the participant stays `Informant`, no state event fires and no `EventSeq` is consumed (so nothing observable differs from an NPC kill, the P1 informant-kill invariant), but the participant's live pawn is cleared and `AwaitingPawn(T)` rises by one until a replacement pawn is adopted. It realises GDD rules R9 (`RetireBinding` and its wrong-input results), R10 (informant-kill invariant) and F2 (`AwaitingPawn`), plus the "Not a transition" paragraph of the States section.

**GDD**: `design/gdd/participant-state-roster.md`
**Requirement**: `TR-roster-004`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0010: Shared Pawn: CMC locomotion, spawn factory and possession parity
**Also governed by**: ADR-0005: Session, seamless travel and match-scoped state stores
**ADR Decision Summary**: Every body in the city is one `AStealthPawn`, and the Roster binding is the only thing that distinguishes a player's body from an NPC, so it is managed through a small set of server-only calls: `BindPawn`, `RetireBinding`, `ReleaseToCrowd` and `ClearBinding`. A binding is `Unbound`, `BoundLive` or `Retired` (a tombstone keyed by object identity plus generation). The factory binds before a pawn is visible, and no connection or kill event destroys or re-spawns a Shared Pawn; a killed informant's replacement is an adopted existing crowd pawn.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Core domain with Networking and Physics (HIGH per `architecture.md`). Post-cutoff APIs in the ADR: `UMotionWarpingComponent::GetOwnerAdapter()` (5.8; MotionWarping is Beta) and `UE_LOGF`; none are used by the Roster core. Verification required by the ADR: traversal spike, S8 (`PawnLeavingGame` hand-over, not yet reported), S14, H1 and the ADR-0002 E4 drain-spawn spike. This story is pure-core binding bookkeeping and none of those gates it.

**Control Manifest Rules (this layer)**:
- Required: The Roster part carries per teammate `{Handle, State, PawnRef, bIsConcealed, RetireAtServerTime}`; the retire time comes through `RetireBinding(Pawn, RetireAtServerTime)`; the Roster push step is the only writer — source: ADR-0001, ADR-0019
- Required: Queue, Roster, tuning validator, seed mixer and appearance deck are plain C++ cores (`F…Core`) with no `UWorld` dependency, behind a subsystem facade — source: ADR-0007
- Required: Test names: `StealthGame.<System>.<Scenario>` (T1); T1 tests live in the owning module's `Private/Tests/<System>/` — source: ADR-0008
- Forbidden: Never pass Event Log content, `DealDigest`, server sub-seeds, bindings or mailbox order to `UE_LOG`/`UE_LOGF`/`ensureMsgf`/`checkf`/`ClientMessage`/`TRACE_*`/`CSV_*`/`SCOPE_CYCLE_COUNTER` names — source: ADR-0007
- Forbidden: Never compile negative-control types or test seams into Shipping (`StealthTestHarness` or `WITH_STEALTH_TEST_SEAMS` only) — source: ADR-0008
- Guardrail: Identity policy: C-TeamView pushes change-only, <= 1 per frame, within the 50 KB/s per-client budget — source: ADR-0001

---

## Acceptance Criteria

*From GDD `design/gdd/participant-state-roster.md`, scoped to this story:*

- [ ] **AC-PSR-21** [Core] `StealthGame.Roster.InformantKillIsNotATransition`: GIVEN an Informant with a live pawn, WHEN `RetireBinding(pawn)` is called, THEN the state stays `Informant`, no state event fires, and `AwaitingPawn(T)` is +1. Calling `RetireBinding` again returns `NoOp_AlreadyRetired` (table, F2, R9).

---

## Implementation Notes

*Derived from ADR-0010 Decision sections 3 to 5 and Implementation Guidelines, ADR-0005 section 2, and GDD rules R9, R10, F2:*

- Add `RetireBinding(Pawn, RetireAtServerTime [, Reason])` to `FRosterCore` (`Source/StealthFoundation/Private/Roster/StealthRosterCore.h/.cpp`), using the binding table (Unbound / BoundLive / Retired) introduced in story 010. `BoundLive` becomes `Retired` and the participant's live pawn is cleared; the tombstone is keyed by `{ObjectKey, Generation}` so a recycled pawn cannot inherit it (R9, I5). The call stays server-only: authority and game-thread checks as for every mutator.
- Wrong inputs (R9): `RetireBinding` on an `Unbound` pawn returns `NoOp_NotBound`; on a `Retired` pawn returns `NoOp_AlreadyRetired`. Neither changes anything nor raises an `ensure`.
- For an informant kill Penalty passes `Reason = InformantKilled` (D10). The Roster writes that reason only to the ServerOnly log: no state change, no `EventSeq`, nothing replicated. Open point: R9 does not name which log; the dev-only transition log of story 036 is the nearest existing structure. Do not add a replicated field or a persistent record for it, and do not assert it in this test (the AC does not).
- No event: `RetireBinding` raises none, so `EventSeq` is identical before and after. The `OnBindingLost` event (R9, R12) belongs to `ClearBinding` of a live binding (story 012), not to a retire.
- F2: after the retire the informant has no live pawn, so `AwaitingPawn(T)` increases by exactly 1 while `InformantCount(T)` is unchanged and `AwaitingPawn(T) <= InformantCount(T)`.
- The `RetireAtServerTime` argument is stored for the teammate view projection; its propagation is story 021 and the carrier is stories 037 and 038. This story only needs to accept it.
- Test construction: `FRosterCore` with injected fakes from `Private/Roster/StealthRosterDeps.h` (pawn-liveness provider, authority true, game thread, seeded random source, fault sink). Build the `Informant` with a `BoundLive` pawn through story 014 (`Active -> Informant` retires the pawn, so use a fresh `BindPawn` for the informant once story 010 allows it) or a core test seam compiled only under `WITH_STEALTH_TEST_SEAMS`. Record `EventSeq` and attach an event spy before the call.
- Prototype disposition: no prototype counterpart. The prototype has no tombstone or informant-kill concept, and nothing is ported.
- Cross-epic note: the caller is NPC-Kill Penalty & Informants (`production/epics/npc-kill-penalty-informants/EPIC.md`) inside its queue slot; this story tests the Roster side only and needs no stub.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 010 (BindPawn rejects an already-bound participant, an in-use pawn and a round-ended participant): the binding table and `BindPawn`.
- Story 011 (Tombstones respect generations and ReleaseToCrowd needs a disconnected participant): tombstone generation checks and `ReleaseToCrowd`.
- Story 012 (Clearing a live binding during Active faults, but level-teardown clears are silent): `ClearBinding` and `OnBindingLost`.
- Story 014 (Active to Informant applies for each entering reason with the correct pawn disposition): retiring a binding as part of a state transition.
- Story 021 (RetireAtServerTime reaches teammate entries for each retiring reason): the `RetireAtServerTime` value reaching teammate entries.
- Story 030 (An informant's released pawn becomes an NPC and a reclaim adopts a different pawn): the later adoption of a replacement pawn.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- `Source/StealthFoundation/Private/Roster/StealthRosterCore.h`
- `Source/StealthFoundation/Private/Roster/StealthRosterCore.cpp`
- `Source/StealthFoundation/Public/Server/StealthRosterService.h` (facade forwarding `RetireBinding`; the header includes `StealthServerOnlyGuard.h` first, per ADR-0007)
- `Source/StealthFoundation/Private/Roster/StealthRosterStore.cpp` (facade implementation)
- `Source/StealthFoundation/Private/Tests/Roster/StealthRosterInformantKillIsNotATransitionTest.cpp` (class `FRosterInformantKillIsNotATransitionTest`, test name `StealthGame.Roster.InformantKillIsNotATransition`)
- ADR-0007 header naming: `Stealth` prefix and project-unique file names; no new header is added beyond the shared roster headers.

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation. (At tiers where the QL-STORY-READY gate is skipped — `minimal`, or `lean`/`solo` review mode — no qa-lead specs exist, and this section reads: N/A — no qa-lead specs at this tier; implement against the Acceptance Criteria above.)*

**Automated test specs:**

- **AC-1**: killing an informant retires its binding without a state event.
  - Given: an Informant (InRound) with a BoundLive pawn, EventSeq recorded, event spy.
  - When: RetireBinding(pawn, T, InformantKilled) runs twice.
  - Then: state stays Informant, no state event (EventSeq unchanged, per R9), AwaitingPawn(T) +1. Second call returns NoOp_AlreadyRetired.
  - Edge cases: RetireBinding on an Unbound pawn returns NoOp_NotBound.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Roster/StealthRosterInformantKillIsNotATransitionTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later). `qa.level` is `full`, so the test is required.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/participant-state-roster/story-014-transition-active-to-informant.md`
- Cross-epic: none directly; the `StealthFoundation` module skeleton from `production/epics/tuning-data-config/EPIC.md` reaches this story through story 001.
- Unlocks: None
