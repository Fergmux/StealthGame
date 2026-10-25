# Story 024: Calls carrying an old round epoch or match id are rejected

> **Epic**: Participant State & Roster
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

This story proves the epoch token `(MatchId, RoundEpoch)` guards every timer and deferred call: a grace timer, respawn timer or queued request created in an earlier round, or in an earlier match, returns `Rejected_StaleEpoch` and changes nothing. The match id matters because `RoundEpoch` restarts at 0 on each `BeginMatch`, so without a random `MatchId` a leftover timer from a previous match would match the new match's epoch. It realises GDD rules R3 (`MatchId`, `RoundEpoch`), R4 (`BeginMatch` draws a new `MatchId`), R8 (precedence step 4), R9 (`BindPawn` epoch guard) and F6.

**GDD**: `design/gdd/participant-state-roster.md`
**Requirement**: `TR-roster-002` (also `TR-roster-010`)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0005: Session, seamless travel and match-scoped state stores
**Also governed by**: ADR-0002: Server resolution queue, server clock and round-end staging contract
**ADR Decision Summary**: The Roster is one of four game-instance stores and the only authoritative owner of `MatchId`, `RoundEpoch`, `EventSeq` and the participant records; `BeginMatch` runs when the host opens the session and clears the store. Session and Round keep state keyed by `ParticipantId`, never by controller or PlayerState, and every wait is bounded by a server-measured clock; ADR-0002's queue items and timers carry the epoch token so stale ones are dropped at the slot.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Networking domain (HIGH per VERSION.md). No new post-cutoff APIs; new logging uses `UE_LOGF` (ADR-0007). Verification required by ADR-0005 is behavioural: spikes S3, S4 and S5 (not yet reported). The `MatchId` draw uses the injected random source (ADR-0006), so no engine API is involved. This story runs entirely inside the engine-free `FRosterCore` and is not gated by any spike.

**Control Manifest Rules (this layer)**:
- Required: Tests inject entropy through `IEntropySource` — source: ADR-0006
- Required: Authoritative match state lives only in the four game-instance stores, keyed by `ParticipantId`, never by controller or PlayerState pointer — source: ADR-0005
- Required: Test names: `StealthGame.<System>.<Scenario>` (T1); T1 tests live in the owning module's `Private/Tests/<System>/` — source: ADR-0008
- Forbidden: Never use `FMath::Rand`, `FMath::FRand`, `FMath::RandRange`, `FRandomStream`, `std::rand`, `<random>` engines or time-seeded randomness in gameplay or server code — source: ADR-0006
- Forbidden: Never use wall-clock time as a test pass/fail predicate; never use wall-clock waits, unseeded randomness or shared mutable state in tests — source: ADR-0008
- Guardrail: Tests: per-commit pipeline <= 10 min — source: ADR-0008

---

## Acceptance Criteria

*From GDD `design/gdd/participant-state-roster.md`, scoped to this story:*

- [ ] **AC-PSR-19** [Core] `StealthGame.Roster.Transition.StaleEpochRejected`: GIVEN `ExpectedEpoch = N`, WHEN `ResetAll` runs and then `RequestTransition` or `BindPawn` is called with N, THEN `Rejected_StaleEpoch` and nothing changes. GIVEN `ExpectedEpoch = (M1, 1)` captured in one match, WHEN `BeginMatch` and one `ResetAll` run and the call is made, THEN `Rejected_StaleEpoch`, because the `MatchId` differs (R8, R9, F6).

---

## Implementation Notes

*Derived from ADR-0005 section 2 (`BeginMatch` lifetime) and ADR-0006 (injected entropy), and GDD rules R3, R4, R8, R9, F6:*

- The epoch token is `(MatchId, RoundEpoch)` with `MatchId` a random 64-bit drawn from the injected, per-test seeded random source at each `BeginMatch` (never `FMath::Rand`). `RequestTransition` and `BindPawn` accept an optional `ExpectedEpoch`; when supplied and different from the live token the result is `Rejected_StaleEpoch` and nothing changes.
- Precedence: `RequestTransition` checks the epoch at step 4, after authority, thread and unknown id and before batch-only and illegal-reason. `BindPawn` checks `Rejected_StaleEpoch` after authority, thread and unknown id and before `Rejected_WrongState`, `Rejected_AlreadyBound` and `Rejected_PawnInUse`. So after a `ResetAll` an old-epoch `BindPawn` for an `Active` participant without a pawn returns `StaleEpoch` (not `WrongState`), which is the stale-respawn guard in R9.
- Case 1: capture `ExpectedEpoch = (M1, N)`, run `ResetAll` (epoch becomes `(M1, N+1)`), then call `RequestTransition` and `BindPawn` with the captured token. Both return `Rejected_StaleEpoch`; the store snapshot is unchanged.
- Case 2: capture `(M1, 1)`, run `BeginMatch` again (new `MatchId` M2, `RoundEpoch` back to 0, store cleared), re-register a participant so an id exists in the new match, run one `ResetAll` (epoch `(M2, 1)`), then call with the captured `(M1, 1)`. `RoundEpoch` is 1 in both matches, so only the `MatchId` difference makes it stale. The test must use a seed whose two `MatchId` draws differ.
- Participant ids restart at 0 after `BeginMatch` (R4), so use an id that exists in the new match, otherwise `UnknownParticipant` (step 3) masks the result.
- The Roster only compares tokens; it does not own timers. Round's grace timer, Penalty's respawn timer and the like are other epics.
- Test construction: `FRosterCore` with injected fakes from `Private/Roster/StealthRosterDeps.h` (authority true, game thread, seeded random source via the injected interface, fault sink). `ResetAll` is story 022; teams must be locked.
- Prototype disposition: no prototype counterpart.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 002 (BeginMatch and registration assign ids, handles and reject a full roster): `BeginMatch` itself and the `MatchId` draw.
- Story 022 (ResetAll resets every non-dormant record with exactly one roster-reset event): the `RoundEpoch` increment.
- Story 054 (Disconnect grace expires on the injected clock and stale-connection timers are rejected): `Rejected_StaleConnection` and the grace timer.
- Story 026 (Late binds and transitions after a round boundary are rejected): the round-boundary scenarios that use the stale epoch result.
- Story 018 (Unregistered and retired ids are rejected by every id-taking API): the unknown-id result that outranks this one.
- Story 038 (SetOwnerTally and SetOwnerRespawnDue feed only the owner view with the full precedence): `Rejected_StaleEpoch` for the owner-feed calls.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- `Source/StealthFoundation/Private/Roster/StealthRosterCore.h` (only if the epoch check needs correction)
- `Source/StealthFoundation/Private/Roster/StealthRosterCore.cpp` (only if the epoch check needs correction)
- `Source/StealthFoundation/Public/View/StealthRosterTypes.h` (epoch token type, if not already defined by story 014 or 010)
- `Source/StealthFoundation/Private/Tests/Roster/StealthRosterTransitionStaleEpochRejectedTest.cpp` (class `FRosterTransitionStaleEpochRejectedTest`, test name `StealthGame.Roster.Transition.StaleEpochRejected`)
- ADR-0007 header naming: `Stealth` prefix and project-unique file names; `Public/View/` headers never include a `Server/` header.

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation. (At tiers where the QL-STORY-READY gate is skipped — `minimal`, or `lean`/`solo` review mode — no qa-lead specs exist, and this section reads: N/A — no qa-lead specs at this tier; implement against the Acceptance Criteria above.)*

**Automated test specs:**

- **AC-1**: calls carrying an old round epoch or match id are rejected.
  - Given: ExpectedEpoch = (M1, N) captured, seeded random source whose draws differ per match.
  - When: ResetAll runs, then RequestTransition and BindPawn carry the old epoch. Separately, with (M1, 1) captured, BeginMatch runs again (M2) and one ResetAll follows, then the call is made with a participant id that exists in the new match.
  - Then: Rejected_StaleEpoch in each case, nothing changes. The second case is stale because MatchId differs although RoundEpoch is 1 in both.
  - Edge cases: "N" is the RoundEpoch paired with the current MatchId. Epoch (step 4) outranks WrongState so BindPawn gives StaleEpoch.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Roster/StealthRosterTransitionStaleEpochRejectedTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later). `qa.level` is `full`, so the test is required.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/participant-state-roster/story-022-reset-all-batch.md`, `production/epics/participant-state-roster/story-014-transition-active-to-informant.md`, `production/epics/participant-state-roster/story-010-bind-pawn-guards.md`
- Cross-epic: none directly; the `StealthFoundation` module skeleton from `production/epics/tuning-data-config/EPIC.md` reaches this story through story 001.
- Unlocks: `production/epics/participant-state-roster/story-026-round-boundary-late-events.md`, `production/epics/participant-state-roster/story-027-transition-exhaustive-matrix.md`, `production/epics/participant-state-roster/story-054-disconnect-grace-timing.md`
