# Story 014: Active to Informant applies for each entering reason with the correct pawn disposition

> **Epic**: Participant State & Roster
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/participant-state-roster.md`
**Requirement**: `TR-roster-002`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0005: Session, seamless travel and match-scoped state stores
**Also governed by**: ADR-0002: Server resolution queue, server clock and round-end staging contract
**ADR Decision Summary**: ADR-0005 makes `FRosterCore::RequestTransition` the only writer of lifecycle state and fixes the strike-limit rule (architecture-review fix 2026-10-25): `RequestTransition(P, Informant, StrikeLimit, ExpectedEpoch)` releases, not retires, the live binding in the same call (the pawn becomes `Unbound`, with no separate `ReleaseToCrowd` by the caller), while every other `Active -> Informant` reason retires it. A kick that closes an `Expired` connection is a legal `Disconnect`. ADR-0002 resolves every interacting change in the round's single queue, so `RequestTransition` is called from a queue slot and its events are dispatched after the outermost call, with payload counts as authoritative snapshots.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: ADR-0005 (HIGH, networking): no new post-cutoff APIs; S3, S4 and S5 verification is outstanding but does not touch the plain-C++ transition logic. ADR-0002 (MEDIUM): no post-cutoff APIs; `UE_LOGF` is preferred for diagnostics but Event Log content never passes through logging. The core tests need no engine object.

**Context paragraph**: This is the central story of the epic. It introduces `RequestTransition(Id, NewState, Reason [, ExpectedEpoch] [, ExpectedConnectionGeneration] [, RetireAtServerTime])`, the result-code enum, R8's eleven-step result precedence as a skeleton, and the legal-transition table, then proves `Active -> Informant` for the four entering reasons with the correct pawn disposition (retired for `Assassinated`, `ReversedAgainst` and `Disconnect`; released for `StrikeLimit`). It realises GDD R8 (all of it), R9 (retire and release inside the call), R12 (`OnParticipantStateChanged` payload) and F6 (`AcceptDisconnect`), and is the base for stories 015 to 024, 027, 028 and 035.

**Control Manifest Rules (this layer)**:
- Required: Every interacting state change resolves as an item (or an inline follow-on in an item's slot) of the round's single queue; nothing outside the drain mutates gameplay state (ADR-0002)
- Required: Queue, Roster, tuning validator, seed mixer and appearance deck are plain C++ cores (`F…Core`) with no `UWorld` dependency, behind a subsystem facade (ADR-0007)
- Required: Authoritative match state lives only in the four game-instance stores, keyed by `ParticipantId`, never by controller or PlayerState pointer (ADR-0005)
- Forbidden: Never let Session spawn, free, hide or possess a pawn, or call `RequestTransition` (ADR-0005)
- Forbidden: Never mutate gameplay state outside the drain; never resolve inline in an RPC body or AI source (ADR-0002)
- Forbidden: Never put team, state, connection, appearance or pawn data on a PlayerState; never give NPCs a PlayerState (ADR-0001)
- Guardrail: Queue drain + staging + teardown + Roster flush: <= 0.5 ms listen-host game thread (ADR-0002)

---

## Acceptance Criteria

*From GDD `design/gdd/participant-state-roster.md`, scoped to this story:*

- [ ] **AC-PSR-14** [Core] `StealthGame.Roster.Transition.ActiveToInformant` (parameterised over Assassinated, ReversedAgainst, StrikeLimit, and Disconnect with Connection = Disconnected): GIVEN Active with a live pawn, WHEN the request is made, THEN it returns `Applied`. Before the call returns, the state is `Informant`, the reason and `LastEventSeq` are written, and the pawn is Retired, so `ResolveVictimKind` = `NotTargetable(AlreadyResolved)`; for `StrikeLimit` the pawn is instead `Unbound`, so `ResolveVictimKind` = `Npc`, and the participant has no live pawn (R8, Penalty CR-3). Exactly one `OnParticipantStateChanged` is then dispatched (R8).

---

## Implementation Notes

*Derived from ADR-0005 section 2 (strike-limit release inside the transition) and section 5 (Disconnect), ADR-0002 Implementation Guidelines, and GDD R8, R9, R12, F6:*

- **Signature and result codes.** `RequestTransition(Id, NewState, Reason, ExpectedEpoch = none, ExpectedConnectionGeneration = none, RetireAtServerTime = call time)`. Result enum (planned `ERosterTransitionResult`): `Applied`, `NoOp_AlreadyInState`, `Rejected_NotAuthority`, `Rejected_WrongThread`, `Rejected_UnknownParticipant`, `Rejected_StaleEpoch`, `Rejected_BatchOnlyReason`, `Rejected_IllegalReason`, `Rejected_IllegalTransition`, `Rejected_NotDisconnected`, `Rejected_StaleConnection`. `ExpectedEpoch` is the token `(MatchId, RoundEpoch)`; `ExpectedConnectionGeneration` is a `uint32`. `RetireAtServerTime` (ms) is the time the retire or release writes for teammates' C-TeamView entries (story 021 asserts it; this story only carries the parameter and defaults it to the injected clock time).
- **R8 result precedence skeleton (authoritative; the first failing check decides).** 1 `NotAuthority`, 2 `WrongThread` (shared precheck, story 003), 3 `UnknownParticipant`, 4 `StaleEpoch`, 5 `BatchOnlyReason` (`RoundEnd`, `Reset`), 6 `IllegalReason` (not an entering reason for `NewState`: Informant <- {Assassinated, ReversedAgainst, StrikeLimit, Disconnect}; RoundEnded <- {RoundEnd}; Active <- {Reset}; `InformantKilled` is never an entering reason), 7 `NoOp_AlreadyInState`, 8 `IllegalTransition`, 9 `NotDisconnected`, 10 `StaleConnection` (`Disconnect` and `ExpectedConnectionGeneration` missing or different), 11 `Applied`. Implement all eleven steps in this order now in one function; stories 015 to 019, 024 and 027 each add tests for individual steps. This story's test covers only the `Applied` path.
- **Legal-transition table.** Encode it as data (a `constexpr` table or a small switch) next to the function and make step 8 consult it: only `Active -> Informant` is a legal single-participant transition that can apply; `RoundEnded -> Informant` is `IllegalTransition`. The table is the object story 027's CSV fixture is checked against.
- **Applied effects, in one call, before it returns:** write `State = Informant`, `LastTransitionReason = Reason`, and `LastEventSeq` (the `EventSeq` of the `OnParticipantStateChanged` event enqueued by this call; story 008 assigns `EventSeq` at emission, so it is known inside the call, which satisfies "before the call returns"). Then dispose of the pawn: for `Assassinated`, `ReversedAgainst` and `Disconnect` call the internal retire (live binding becomes `Retired`, carrying `RetireAtServerTime`); for `StrikeLimit` call the internal release from story 011 (`ReleaseBindingInternal`: pawn becomes `Unbound`, the `Rejected_NotDisconnected` rule does not apply on this path). The participant then has no live pawn ("awaiting pawn" for AwaitingPawn counts). The Disconnect-with-Disconnected-connection case is `AcceptDisconnect` (F6): `Connection = Disconnected` (including an `Expired` session state projected as `Disconnected`, the kick path) and the generation equal.
- **Dispatch.** Exactly one `OnParticipantStateChanged{Id, Team, Old, New, Reason, EventSeq, RoundEpoch, ActiveCountAfter[2], InformantCountAfter[2]}` is enqueued through the story 008 queue and dispatched after the outermost call. The `ActiveCountAfter` and `InformantCountAfter` values are snapshots taken at emission (F1, F2); build small internal count helpers now (the public count queries and their conservation test are stories 025 and 034).
- **ResolveVictimKind in the test.** Assert through the story 013 function: a retired pawn gives `NotTargetable(AlreadyResolved)`, a released (strike-limit) pawn gives `Npc`.
- **Open point:** the AC's "`LastEventSeq` are written" before the call returns is inferred by the QA spec to mean the dispatched event's `EventSeq` (confirm in code review that `LastEventSeq` equals the event's `EventSeq` and not a pre-increment value).
- **Fixtures.** Active participants with live pawns come from the stories 002, 010 and (for Disconnected) 009 paths; the `Active` state itself is set through the core test seam until story 022 (`ResetAll`), because no production path creates `Active` yet. Disconnect case: `NotifyConnectionChanged(Disconnected)` and the matching `ExpectedConnectionGeneration`; the QA edge case notes step 10 rejects the case if the generation is omitted.
- **Penalty and Assassination callers.** `StrikeLimit` is requested by NPC-Kill Penalty & Informants, `Assassinated` and `ReversedAgainst` by Assassination, `Disconnect` by Round (production/epics/npc-kill-penalty-informants/EPIC.md, production/epics/assassination-defence-reversal/EPIC.md, production/epics/round-match-framework/EPIC.md); those callers are not needed to implement or test the Roster side.
- **Prototype disposition:** replaces the `EliminatePlayer`-style lifecycle bookkeeping in `Source/StealthGame/Private/StealthGameMode.cpp` (`AStealthGameMode::EliminatePlayer`, `CheckEliminationVictory`) and the life flags on `AStealthPlayerState`. The elimination predicate is re-expressed in story 035, not here.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 015 (A Disconnect transition is rejected while the participant is connected), Story 016 (The first entering reason is kept and later reasons are no-ops), Story 017 (No revival and no round-ended-to-informant transitions are possible), Story 018 (Unregistered and retired ids are rejected by every id-taking API) and Story 019 (RoundEnd and Reset reasons are rejected on single transitions): the individual rejection and no-op steps of the precedence
- Story 020 (Killing an informant retires its binding without a state event): `RetireBinding` as a public call
- Story 021 (RetireAtServerTime reaches teammate entries for each retiring reason): the value reaching the teammate-view projection
- Story 022 (ResetAll resets every non-dormant record with exactly one roster-reset event) and Story 023 (EndRoundAll ends all records once and is idempotent): the batch transitions
- Story 024 (Calls carrying an old round epoch or match id are rejected): the stale-epoch assertions
- Story 027 (The transition matrix equals the checked-in CSV fixture for every combination): the exhaustive matrix
- Story 028 (Re-entrant listener calls see live state and events dispatch FIFO with strict EventSeq): re-entrancy assertions
- Story 035 (The elimination trigger fires only for a strike-out win and never for disconnect or empty sides): the F4 trigger
- Story 036 (The dev-only transition log keeps the last N entries and is absent from Shipping): the dev transition log

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- `Source/StealthFoundation/Private/Roster/StealthRosterCore.h` (changed: `RequestTransition`, result-code enum, legal-transition table, `OnParticipantStateChanged` payload, internal retire and count helpers)
- `Source/StealthFoundation/Private/Roster/StealthRosterCore.cpp` (changed)
- `Source/StealthFoundation/Public/View/StealthRosterTypes.h` (changed only for wire-safe enums, for example the participant state enum used later by C-TeamView; server-only result codes stay in the core header)
- `Source/StealthFoundation/Private/Tests/Roster/StealthTransitionActiveToInformantTest.cpp` (new: class `FRosterTransitionActiveToInformantTest`, test name `StealthGame.Roster.Transition.ActiveToInformant`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation. (At tiers where the QL-STORY-READY gate is skipped — `minimal`, or `lean`/`solo` review mode — no qa-lead specs exist, and this section reads: N/A — no qa-lead specs at this tier; implement against the Acceptance Criteria above.)*

**For Logic / Integration stories — automated test specs:**

- **AC-1**: each entering reason moves Active to Informant with the right pawn disposition.
  - Given: an Active participant with a live pawn for each of Assassinated, ReversedAgainst, StrikeLimit, Disconnect. Disconnect uses a Disconnected record and a matching ExpectedConnectionGeneration.
  - When: RequestTransition(Informant, reason).
  - Then: Applied. Before the call returns, state Informant, reason and LastEventSeq written (inferred: the dispatched event's EventSeq; author to confirm). Pawn Retired and ResolveVictimKind gives NotTargetable(AlreadyResolved). For StrikeLimit the pawn is Unbound so Npc and GetPawn(Id) null. After the outermost call exactly one OnParticipantStateChanged.
  - Edge cases: the Disconnect case needs the generation supplied or step 10 rejects it.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: automated test `Source/StealthFoundation/Private/Tests/Roster/StealthTransitionActiveToInformantTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later). `qa.level` is full, so the test is required.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/participant-state-roster/story-009-connection-projection.md`, `production/epics/participant-state-roster/story-010-bind-pawn-guards.md`, `production/epics/participant-state-roster/story-011-tombstone-generation-and-release.md`
- Unlocks: `production/epics/participant-state-roster/story-015-transition-disconnect-requires-disconnected.md`, `production/epics/participant-state-roster/story-016-transition-first-reason-wins.md`, `production/epics/participant-state-roster/story-017-transition-no-revival-no-late-events.md`, `production/epics/participant-state-roster/story-018-transition-unknown-id-rejected.md`, `production/epics/participant-state-roster/story-019-batch-only-reasons-rejected-singly.md`, `production/epics/participant-state-roster/story-020-informant-kill-is-not-a-transition.md`, `production/epics/participant-state-roster/story-021-retire-at-server-time-source.md`, `production/epics/participant-state-roster/story-022-reset-all-batch.md`, `production/epics/participant-state-roster/story-023-end-round-all-batch-idempotent.md`, `production/epics/participant-state-roster/story-024-stale-epoch-rejected.md`, `production/epics/participant-state-roster/story-028-reentrant-listener-ordering.md`, `production/epics/participant-state-roster/story-029-reclaim-core.md`, `production/epics/participant-state-roster/story-031-legality-class-and-adoption.md`, `production/epics/participant-state-roster/story-035-elimination-trigger-core.md`, `production/epics/participant-state-roster/story-036-transition-log-ring.md`, `production/epics/participant-state-roster/story-045-pie-elimination-leak-diff.md`, `production/epics/participant-state-roster/story-055-simultaneous-resolution.md`
