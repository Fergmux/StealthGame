# Story 004: Every roster call on a client is rejected or answers not-targetable and faults

> **Epic**: Participant State & Roster
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/participant-state-roster.md`
**Requirement**: `TR-roster-005`, `TR-roster-001`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0010: Shared Pawn: CMC locomotion, spawn factory and possession parity
**Also governed by**: ADR-0005: Session, seamless travel and match-scoped state stores
**ADR Decision Summary**: ADR-0010 makes every body one `AStealthPawn` and routes every spawn, bind and possession through the server-only `UStealthPawnFactory`, which asserts authority and the game thread on every call and reports misuse to the fault sink; a client must never be able to obtain a permissive "this is an NPC" answer. ADR-0005 states that the four stores exist but stay empty on a client process (a client may later host) and that every mutating API asserts authority. This story is the Roster's side of that rule: on a client every mutator rejects, every query answers the non-permissive value, and each call is faulted.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: ADR-0010 (HIGH): post-cutoff `UMotionWarpingComponent::GetOwnerAdapter()` (MotionWarping is Beta in 5.8.4) and `UE_LOGF`; spikes S8, S14, H1, ST1, P1 and the traversal spike are outstanding but concern possession and movement, not this story. This story is plain C++ over an injected authority provider (the production adapter asks the net mode and is facade work). ADR-0005 (HIGH): S3, S4, S5 outstanding, untouched here.

**Context paragraph**: GDD R2 says a mutating or binding query called on a client returns `Rejected_NotAuthority` / `NotTargetable(Stale)` and reports to the fault sink, and never returns the permissive `Npc`; R10 adds that the count queries return 0 or false there. This story adds the authority-false behaviour to every Roster entry point that exists so far and fixes the per-call result table, so a client (or the listen host's presentation code calling the wrong API) can never learn player-ness from the Roster.

**Control Manifest Rules (this layer)**:
- Required: Authoritative match state lives only in the four game-instance stores, keyed by `ParticipantId`, never by controller or PlayerState pointer (ADR-0005)
- Required: A reflective or Blueprint request for a server-only value on a client returns invalid and uses `ensure`, never `check` (ADR-0007)
- Required: Registrations are weak (`TWeakInterfacePtr`/`TWeakObjectPtr`), cleared in `Deinitialize`/`EndPlay`; a dead registration is a fault-sink report, never a crash (ADR-0007)
- Forbidden: Never put team, state, connection, appearance or pawn data on a PlayerState; never give NPCs a PlayerState (ADR-0001)
- Forbidden: Never put `BlueprintCallable`, `BlueprintPure`, `BlueprintReadOnly`, `BlueprintReadWrite` or `BlueprintAssignable` on a member of a server type (ADR-0007)
- Guardrail: Session: silence clocks and ack bookkeeping inside the 0.5 ms line (ADR-0005)

---

## Acceptance Criteria

*From GDD `design/gdd/participant-state-roster.md`, scoped to this story:*

- [ ] **AC-PSR-02** [Core] `StealthGame.Roster.ClientCallsRejected`: GIVEN authority = false, WHEN every mutating call, `ResolveVictimKind`, `IsPawnBound`, `GetLegalityClass` and every count query (`CountActive`, `CountInformant`, `CountRoster`, `IsTeamEliminated`) is invoked, THEN:
  - the results are `Rejected_NotAuthority` / `NotTargetable(Stale)` / false / 0;
  - the fault sink records one fault per call;
  - the store is unchanged;
  - no call returns `Npc` or a real count (R2, R10).

---

## Implementation Notes

*Derived from ADR-0010 Implementation Guidelines (factory asserts authority and thread, reports misuse to the fault sink), ADR-0005 section 2 and GDD R2, R10:*

- **Authority provider.** `IRosterAuthority` (story 001 deps) answers "is this the authority". The shared precheck added in story 003 returns `Rejected_NotAuthority` first (R8 precedence 1); this story adds the **query-side** equivalent: every query that has a server-only answer (`ResolveVictimKind`, `IsPawnBound`, `GetLegalityClass`, `CountActive`, `CountInformant`, `CountRoster`, `IsTeamEliminated`) checks authority first and returns the client answer, faulting once per call.
- **Per-call result table (pin it in the test and in a header comment).** Mutators that return a result code return `Rejected_NotAuthority`. `ResolveVictimKind` returns `NotTargetable(Stale)`. `IsPawnBound` and `IsTeamEliminated` return false. `CountActive`, `CountInformant`, `CountRoster` return 0. `GetLegalityClass` has no stated client value in the GDD; see the open point. No query returns `Npc`, `Player`, `Informant` or a real count on a client (R2, R10). Mutators without a result type in the GDD (`InvalidateAllBindings`, `RebindView`) are listed in the open point.
- **One fault per call.** The fault sink receives exactly one fault, with the expected code, per rejected call (not one per test); the test asserts the sink size grows by one per call. Never rely on `ensure` (GDD Tags).
- **Store unchanged.** Reuse the core snapshot helper (story 003); after the whole sequence the snapshot equals the pre-sequence snapshot, and restoring authority shows the original counts.
- **Which calls exist now.** At this point in the epic the entry points that exist are registration, `RemoveParticipant`, `BeginMatch` and the story 003 precheck entry points; the remaining mutators and queries named by the AC arrive in later stories. Implement the client behaviour through the shared precheck so each later story inherits it, and have this story's test enumerate the call list from one table that later stories extend (do not hardcode a second list).
- **Open point (QL-STORY-READY GAPS):** the AC says "every mutating call" without enumerating it and the pipe-separated result list does not say which result belongs to which call. `GetLegalityClass` has no client value in that list (must be pinned; it must not return a value that reveals the binding, per R10, which makes `Active` the only non-leaking answer for an Unbound pawn, but R10 does not state the client value). `InvalidateAllBindings` and `RebindView` have no result type. Resolve with a per-call expected-result table before the test is finalised.
- **Prototype disposition:** No prototype counterpart. The prototype has no authority-gated roster; its `IsNpc() = (PlayerState == null)` client-side derivation (the known leak) is retired by stories 040 and 046.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 002 (BeginMatch and registration assign ids, handles and reject a full roster): registration behaviour on the authority
- Story 003 (Mutation from a worker thread is rejected and faulted): the thread check and the shared precheck helper
- Story 010 (BindPawn rejects an already-bound participant, an in-use pawn and a round-ended participant), Story 011 (Tombstones respect generations and ReleaseToCrowd needs a disconnected participant) and Story 013 (ResolveVictimKind answers the full pawn-state matrix and never yields Npc for NotTargetable): the server-side bodies of the binding calls and `ResolveVictimKind`
- Story 025 (Undersized rosters count correctly and lock and reset preconditions hold) and Story 034 (A scripted 200-step sequence keeps F1-F3 and I1-I3 equal to a brute-force recount): server-side count semantics
- Story 031 (GetLegalityClass answers per pawn state and BindPawn adopts an unbound pawn): the authority-true behaviour of `GetLegalityClass`

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- `Source/StealthFoundation/Private/Roster/StealthRosterCore.h` (changed: client-side answers for queries, per-call result table)
- `Source/StealthFoundation/Private/Roster/StealthRosterCore.cpp` (changed)
- `Source/StealthFoundation/Private/Roster/StealthRosterDeps.h` (changed: fault codes for client calls if not complete)
- `Source/StealthFoundation/Private/Tests/Roster/StealthClientCallsRejectedTest.cpp` (new: class `FRosterClientCallsRejectedTest`, test name `StealthGame.Roster.ClientCallsRejected`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation. (At tiers where the QL-STORY-READY gate is skipped — `minimal`, or `lean`/`solo` review mode — no qa-lead specs exist, and this section reads: N/A — no qa-lead specs at this tier; implement against the Acceptance Criteria above.)*

*Test cases not yet defined — run /qa-plan to generate them.*

**Flagged gap (QL-STORY-READY: GAPS):** "every mutating call" is not enumerated and "Rejected_NotAuthority / NotTargetable(Stale) / false / 0" does not say which result goes with which call. GetLegalityClass has no client value in that list. Several mutators have no result type (InvalidateAllBindings, RebindView). Needs a per-call expected-result table incl. the GetLegalityClass client answer.

**Provisional spec (qa-lead, pending gap resolution):**

- **AC-1**: on a client, calls rejected or not-targetable, faulted, change nothing.
  - Given: populated store, then authority provider = false, fault sink.
  - When: each enumerated mutator, ResolveVictimKind, IsPawnBound, GetLegalityClass, CountActive, CountInformant, CountRoster, IsTeamEliminated called once.
  - Then: mutators return Rejected_NotAuthority; ResolveVictimKind returns NotTargetable(Stale); IsPawnBound and IsTeamEliminated false; counts 0. Sink holds one fault per call; store snapshot unchanged. No call returns Npc or a real count.
  - Edge cases: GetLegalityClass value to be pinned; restoring authority shows the original counts.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: automated test `Source/StealthFoundation/Private/Tests/Roster/StealthClientCallsRejectedTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later). `qa.level` is full, so the test is required.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/participant-state-roster/story-002-begin-match-and-register.md`, `production/epics/participant-state-roster/story-003-off-game-thread-mutation-rejected.md`
- Unlocks: None
