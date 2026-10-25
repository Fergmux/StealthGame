# Story 029: Reclaim restores the same record for active and informant participants

> **Epic**: Participant State & Roster
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

This story delivers the core of rejoin: when a key that matches an existing record registers again, the participant gets `Reclaimed` with the same `ParticipantId`, `PublicHandle`, `Team`, `State` and `AppearanceHistory`, `Connection` flips back to `Connected`, and exactly one `OnParticipantReclaimed` fires. An `Active` participant keeps its still-bound pawn; an `Informant` with no live pawn stays "awaiting pawn" (`AwaitingPawn(T)` still counts it) until `BindPawn` adopts an Unbound crowd pawn. It exists because reconnecting must never change state (an informant stays an informant) and must never mint a second record for the same human. It realises GDD rules R11 (Rejoin), R4 (registration check order, `Reclaimed`), R9 and F2, and the Edge Cases "If a participant reconnects" and "two live connections present the same `IdentityKey`".

**GDD**: `design/gdd/participant-state-roster.md`
**Requirement**: `TR-roster-010`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0005: Session, seamless travel and match-scoped state stores
**ADR Decision Summary**: A rejoin is a fresh login with a known key: Session admits it, the retained PlayerState is adopted by the new controller (no fresh PlayerState is ever spawned for a reclaim), and the Roster's `RegisterParticipant` returns `Reclaimed` for the existing record, deciding reclaim by the Roster key alone (never by address, and never through `AddInactivePlayer`/`FindInactivePlayer`). Authoritative state lives only in the four game-instance stores keyed by `ParticipantId`, and the newest connection for one key wins.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Networking domain (HIGH per VERSION.md). No new post-cutoff APIs; `DispatchPostLogin` (deprecated 5.6) is not used. Verification required by the ADR: spike S4 (reconnect: retained PlayerState re-attached, two clients on one machine with `-IdentityKey=`, supersede) has not reported. This story runs entirely inside the engine-free `FRosterCore` and is not gated by S4; the engine-side rejoin is stories 057 and 059.

**Control Manifest Rules (this layer)**:
- Required: A rejoiner adopts its retained PlayerState in `InitPlayerState`; `OnDeactivated` does not destroy the PlayerState while admission is `RejoinOnly` — source: ADR-0005
- Required: Authoritative match state lives only in the four game-instance stores, keyed by `ParticipantId`, never by controller or PlayerState pointer — source: ADR-0005
- Required: Queue, Roster, tuning validator, seed mixer and appearance deck are plain C++ cores (`F…Core`) with no `UWorld` dependency, behind a subsystem facade — source: ADR-0007
- Forbidden: Never derive from `AGameMode`; never use `AddInactivePlayer`/`FindInactivePlayer` or address-keyed reclaim — source: ADR-0005
- Forbidden: Never toggle `bIsInactive` or remove a PlayerState from the array between `Preparing` and the next travel; never spawn a fresh PlayerState for a reclaim — source: ADR-0005
- Guardrail: Tests: per-commit pipeline <= 10 min — source: ADR-0008

---

## Acceptance Criteria

*From GDD `design/gdd/participant-state-roster.md`, scoped to this story:*

- [ ] **AC-PSR-47** [Core] `StealthGame.Roster.ReclaimCore`: GIVEN an Active participant (Disconnected, pawn BoundLive) and an Informant (Disconnected, awaiting pawn), WHEN each key registers again, THEN:
  - each gets `Reclaimed` with the same id, handle, team, state and `AppearanceHistory`, and `Connection = Connected`;
  - one `OnParticipantReclaimed` fires for each;
  - the Active participant's pawn is unchanged;
  - the Informant's `AwaitingPawn` is still counted until `BindPawn` adopts an Unbound pawn.

  GIVEN the same key registers twice in one frame, THEN exactly one `Reclaimed` event fires (R11).

---

## Implementation Notes

*Derived from ADR-0005 section 5 (rejoin, supersede) and GDD rules R4, R11, R9, F2:*

- Extend `FRosterCore::RegisterParticipant` (`Source/StealthFoundation/Private/Roster/StealthRosterCore.h/.cpp`): the lookup by `IdentityKey` comes first (registration check order: `Rejected_NotAuthority`, then the reclaim match, then `Rejected_TeamsLocked`, then `Rejected_RosterFull`; story 005 covers the locked case). A match returns `Reclaimed(Id)` and restores nothing, because nothing was lost: `ParticipantId`, `PublicHandle`, `Team`, `State`, `AppearanceHistory` stay exactly as they were; `Connection` becomes `Connected`.
- Connection fields on reclaim: `Connection = Connected`. Per R11 `ConnectionGeneration` increases only on a change of the projection into `Disconnected`, so a reclaim does not bump it and leaves `DisconnectedAt` alone. Open point (source conflict): ADR-0005 section 5 says "Every `Connection` change bumps the participant's `ConnectionGeneration`", which contradicts the GDD (R3, R11, AC-PSR-46). Follow the GDD, and flag the ADR wording for correction. Whether `NotifyConnectionChanged(Connected)` or `RegisterParticipant` flips `Connection` back is a question for story 009's projection; keep one writer for the fields (Session's notify) where possible and let `Reclaimed` reflect that.
- Active case: the pawn stays `BoundLive`; `GetPawn(Id)` is unchanged. Informant case: no live pawn, `AwaitingPawn(T)` (F2) still counts the informant until `BindPawn(Id, UnboundPawn)` adopts a crowd pawn; the adoption itself is a plain `BindPawn` (R9 crowd-adoption path), no spawn is involved.
- `AppearanceHistory` must have entries to prove it is kept: run with `AppearanceHistoryDepth > 0` and `RecordAppearance` entries recorded beforehand (default depth is 0, which would make the check vacuous). The `RecordAppearance` drop-oldest behaviour is not covered by any AC; do not add assertions for it here.
- Events: exactly one `OnParticipantReclaimed` per reclaim, consuming one `EventSeq` through the story-008 queue. State-changing side effects are none (reconnecting never changes lifecycle state).
- Open point (qa-lead gap, needs a ruling): the second clause ("the same key registers twice in one frame") does not say what the second call returns. A pure core has no frame, so define "in one frame" as two `RegisterParticipant` calls with no push step or clock advance between them. The GDD only pins the event count (exactly one). Candidates: the second call returns `Reclaimed` again with no event, or a no-op result; pin the return value with the GDD owner before asserting it, and meanwhile assert only that exactly one `OnParticipantReclaimed` fires and `EventSeq` rose by exactly one.
- The feature-state rule (strikes, defence and concealment are not reset by a reclaim; features key by `ParticipantId`) is an obligation on those features and is not tested here. `RebindView` and the C-TeamView re-push are story 041.
- Test construction: `FRosterCore` with injected fakes from `Private/Roster/StealthRosterDeps.h` (authority true, game thread, seeded random source, fault sink). Disconnect the two participants through `NotifyConnectionChanged` (story 009); the Informant via `RequestTransition` (story 014) then `ReleaseToCrowd`/retire as needed to be pawn-less.
- Prototype disposition: no prototype counterpart. The prototype has no identity key or reclaim, and nothing is ported.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 009 (Session connection states project onto Connection fields only): the `NotifyConnectionChanged` projection and `ConnectionGeneration` rule.
- Story 008 (Registration, team, removal and reclaim each fire exactly one sequenced event): the Dormant-record reclaim case and the event queue.
- Story 005 (A locked roster rejects registration, removal and team changes but reclaims known keys): reclaim while teams are locked.
- Story 030 (An informant's released pawn becomes an NPC and a reclaim adopts a different pawn): reclaim after `ReleaseToCrowd`.
- Story 041 (A replaced controller or reclaim re-pushes C-TeamView within one server frame): `RebindView` and the re-push.
- Story 054 (Disconnect grace expires on the injected clock and stale-connection timers are rejected): grace timing.
- Story 057 (Two client processes on one machine with distinct identity keys get distinct records): the multi-process same-machine keys case (blocked on spike S4).
- Story 059 (A client rejoining within grace reclaims its record, pawn and informant status): the end-to-end multi-process rejoin (blocked on spikes S4 and S8).

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- `Source/StealthFoundation/Private/Roster/StealthRosterCore.h`
- `Source/StealthFoundation/Private/Roster/StealthRosterCore.cpp` (reclaim path of `RegisterParticipant`)
- `Source/StealthFoundation/Public/View/StealthRosterTypes.h` (result type for `Registered`/`Reclaimed`, if not already defined by story 002)
- `Source/StealthFoundation/Private/Tests/Roster/StealthRosterReclaimCoreTest.cpp` (class `FRosterReclaimCoreTest`, test name `StealthGame.Roster.ReclaimCore`)
- ADR-0007 header naming: `Stealth` prefix and project-unique file names; `Public/View/` headers never include a `Server/` header.

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation. (At tiers where the QL-STORY-READY gate is skipped — `minimal`, or `lean`/`solo` review mode — no qa-lead specs exist, and this section reads: N/A — no qa-lead specs at this tier; implement against the Acceptance Criteria above.)*

*Test cases not yet defined — run /qa-plan to generate them.*

**Flagged gap (QL-STORY-READY: GAPS)**: the "same key registers twice in one frame" gives no result for the second call (Reclaimed with no event, or a no-op code). A pure core has no frame so "in one frame" is undefined. Needs the second call's return value pinned and "frame" defined as two calls before any push step or clock advance.

**Provisional spec (qa-lead, pending gap resolution):**

- **AC-1**: reclaim restores the same record for Active and Informant participants.
  - Given: a Disconnected Active participant with a BoundLive pawn, a Disconnected Informant awaiting a pawn, AppearanceHistoryDepth > 0 with entries recorded.
  - When: each key registers again, then an Unbound pawn is bound to the Informant.
  - Then: each gets Reclaimed with the same id, handle, team, state, AppearanceHistory, Connection = Connected. One OnParticipantReclaimed for each. Active pawn unchanged. AwaitingPawn still counts the Informant until BindPawn adopts.
  - Edge cases: the same key twice gives exactly one event.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Roster/StealthRosterReclaimCoreTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later). `qa.level` is `full`, so the test is required.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/participant-state-roster/story-009-connection-projection.md`, `production/epics/participant-state-roster/story-010-bind-pawn-guards.md`, `production/epics/participant-state-roster/story-014-transition-active-to-informant.md`
- Cross-epic: none directly; the `StealthFoundation` module skeleton from `production/epics/tuning-data-config/EPIC.md` reaches this story through story 001.
- Unlocks: `production/epics/participant-state-roster/story-030-informant-disconnect-release-reclaim.md`, `production/epics/participant-state-roster/story-041-team-view-repush-on-controller-swap.md`, `production/epics/participant-state-roster/story-051-pie-teammate-reconnecting-flag.md`, `production/epics/participant-state-roster/story-054-disconnect-grace-timing.md`, `production/epics/participant-state-roster/story-057-mp-same-machine-clients-distinct.md`
