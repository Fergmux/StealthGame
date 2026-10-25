# Story 038: SetOwnerTally and SetOwnerRespawnDue feed only the owner view with the full precedence

> **Epic**: Participant State & Roster
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/participant-state-roster.md`
**Requirement**: `TR-roster-006`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0001: Crowd identity rules
**Also governed by**: ADR-0005: Session, seamless travel and match-scoped state stores
**ADR Decision Summary**: ADR-0001 fixes the owner-only `T_due`, `WrongTargetTally` and `LastResultSeq` as fields of the C-TeamView owner part (option (a), user decision 2026-10-25): Owner audience, on the PlayerController carrier family, never on the pawn, written only on reclaim, rejoin and `OnRosterReset` (never per strike), with Penalty supplying the values through the Roster server face and the Roster's push step staying the only writer. ADR-0005 keeps the Roster's authoritative state in the plain-C++ `FRosterCore` behind the `URosterStore` facade, with every mutating API asserting authority and the game thread.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: ADR-0001 post-cutoff items: `APawn::RemoteViewPitch16` (5.6) and the `net.Iris.UseIrisReplication` default of 0; neither is used here. Verification items S1, S2, S14 (S2 and S14 PASS; S1 PASS for the three pointers only). ADR-0005 adds spikes S3, S4, S5 (not yet reported). This story is core-only: the push step is observed through a spy, so no replication behaviour is verified here.

**Control Manifest Rules (this layer)**:
- Required: The Roster part carries owner `{Handle, Team, State, RoundEpoch, WrongTargetTally, LastResultSeq, T_due}` (last three Owner-only); Penalty supplies tally and `T_due` only through `SetOwnerTally` / `SetOwnerRespawnDue`; the Roster push step is the only writer — source: ADR-0001, ADR-0019
- Required: `WrongTargetTally`/`LastResultSeq` are written to C-TeamView only on reclaim, rejoin and `OnRosterReset` — source: ADR-0001, ADR-0018, ADR-0019
- Required: Owner-audience data travels only on the owner's PlayerController — source: ADR-0001
- Forbidden: Never carry `T_due` anywhere but the owner's C-TeamView owner part (no reply entry, pawn field or client derivation) — source: ADR-0001, ADR-0019
- Forbidden: Never store anything authoritative on a PlayerController, PlayerState, GameMode or GameState — source: ADR-0005
- Guardrail: C-TeamView pushes change-only, at most 1 per frame, within the 50 KB/s per-client budget — source: ADR-0001

This story delivers the Roster's owner-view feed: the two server calls `SetOwnerTally` and `SetOwnerRespawnDue` that write a participant's three Owner-only display fields into the push step's view cache, with their result precedence, saturation and coalescing rules. It exists so NPC-Kill Penalty can drive the HUD tally and the informant countdown without ever touching a replicated property itself. It realises the GDD R6 owner-view feed (second-round review 2026-10-25) and R12 (these calls emit no event and consume no `EventSeq`).

---

## Acceptance Criteria

*From GDD `design/gdd/participant-state-roster.md`, scoped to this story:*

- [ ] **AC-PSR-54** [Core] `StealthGame.Roster.OwnerViewFeed` (R6 owner-view feed, second-round review 2026-10-25): GIVEN a participant `InRound`, WHEN `SetOwnerTally(Id, 2, 7, epoch)` is called, THEN it returns `Applied`, the push step's owner view carries (2, 7) and no teammate entry or opponent view changes, no event fires and `EventSeq` is unchanged. WHEN the same call repeats, THEN `NoOp_Unchanged`; WHEN `LastResultSeq` 6 is sent, THEN `Rejected_StaleSequence`; WHEN a tally of 300 is sent, THEN it reads 255. WHEN `SetOwnerRespawnDue(Id, T, epoch)` is called and then `SetOwnerRespawnDue(Id, 0, epoch)`, THEN the owner view carries T and then 0. WHEN authority = false, the thread is not the game thread, the id is unknown, the epoch is stale or the record is not `InRound`, THEN the results are `Rejected_NotAuthority` / `Rejected_WrongThread` / `Rejected_UnknownParticipant` / `Rejected_StaleEpoch` / `Rejected_NotInRound` in that precedence and the cache is unchanged. WHEN `ResetAll` runs, THEN all three fields read 0; WHEN the participant reclaims, THEN they are unchanged. Three `Applied` calls in one frame produce one coalesced view update.

---

## Implementation Notes

*Derived from ADR-0001 sections 5-6, ADR-0005 section 2 and GDD R6 (owner-view feed), R12:*

- **Signatures (GDD R6).** `SetOwnerTally(Id, Tally, LastResultSeq, ExpectedEpoch)` and `SetOwnerRespawnDue(Id, T_due, ExpectedEpoch)` on `FRosterCore`, exposed on `URosterStore` (`Public/Server/StealthRosterService.h`). Both are server-only and game-thread. The `Tally` parameter type must be wider than `uint8` (for example `int32`) so that 300 can be passed and saturates to 255; the stored field is `uint8` (`WrongTargetTally`).
- **View cache, not the record.** Keep the three values in a per-participant view cache owned by the core and consumed by the push step (`FRosterOwnerViewCache {uint8 Tally; LastResultSeq; double T_due;}`), outside `FParticipantRecord`, so R1, R3 and AC-PSR-01 (story 001) are unaffected. The cache is a display copy: Penalty owns the values.
- **Result precedence (R6).** `Rejected_NotAuthority`, `Rejected_WrongThread`, `Rejected_UnknownParticipant`, `Rejected_StaleEpoch`, `Rejected_NotInRound` (the record is not `InRound`), `Rejected_StaleSequence` (`SetOwnerTally` only: `LastResultSeq` lower than the stored one within the epoch), `NoOp_Unchanged` (identical values), then `Applied`. A rejection leaves the cache unchanged. `T_due = 0` clears the countdown.
- **Reuse, do not re-derive.** The first four checks are the same steps as R8 precedence 1-4 and BindPawn; use the shared helper written for `RequestTransition` (story 014) rather than a second copy of the epoch token comparison `(MatchId, RoundEpoch)`.
- **No event, no EventSeq.** These calls emit nothing and consume no `EventSeq` (R12); the test asserts `EventSeq` unchanged. They mark the participant's owner view dirty for the push step only.
- **Push step seam and coalescing.** The Roster's push step is the only writer of the replicated part and writes changes at most once per server frame, all coalesced (R6). In the pure core, expose the dirty set through a consume-style accessor that the test treats as the "push step spy": after three `Applied` calls the test calls it once and expects one update for the owner. An `Applied` write reaches only the owner's controller (owner view), never a teammate entry or another participant's view.
- **Lifecycle.** `ResetAll` (story 022) zeroes tally, `LastResultSeq` and `T_due`; reclaim and rejoin do not touch the cache. For the reclaim assertion use the reclaim path available from story 008 (a Dormant key reclaims) or story 029 once landed.
- **Call discipline is not enforced here.** "Never per strike" (Penalty PEN-R3, CR-19) is Penalty's call discipline and is tested there (AC-NKP-27); the Roster's part is that an `Applied` write reaches only the owner and is coalesced.
- **Open point:** the exact shape of the push-step accessor (what "the push step's owner view" is in a pure core) is not specified by the GDD or ADRs; the implementer defines it and records it in the test so stories 041 and 045 can reuse it. Story 021 refers to the same "teammate-view projection" in the core.
- **Prototype disposition:** supersedes the prototype's owner-only `AStealthPlayerState::NpcKills` and `LastDefenceActivationTime` (`COND_OwnerOnly` fields in `Source/StealthGame/Private/StealthPlayerState.cpp`); the tally replacing the strike counter is confirmed (ADR-0001 amendments, Penalty OQ-NKP-1). Not ported: values move into the C-TeamView owner part.
- **Header naming (ADR-0007).** Stealth prefix, project-unique names: `StealthRosterCore.h`, `StealthRosterTypes.h`, `StealthRosterService.h`, `StealthRosterTeamView.h`.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 037 (A maximal C-TeamView net-serializes in at most 96 bytes): owns the replicated struct and its serializer.
- Story 041 (A replaced controller or reclaim re-pushes C-TeamView within one server frame): owns `RebindView` and the replication of the cache to a new controller.
- Story 022 (ResetAll resets every non-dormant record with exactly one roster-reset event): owns `ResetAll` itself; this story only asserts the cache is cleared by it.
- Story 029 (Reclaim restores the same record for active and informant participants): owns the reclaim path.
- Story 018 (Unregistered and retired ids are rejected by every id-taking API): owns the general unknown-id rule; this story applies it to the two calls.
- Story 045 (Opponents receive nothing but allowlisted movement updates after a kill of a player, NPC or informant) and story 047 (No opponent count reaches a client before the round ends): PIE proof that the owner part never reaches an opponent.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- `Source/StealthFoundation/Private/Roster/StealthRosterCore.h` (owner view cache, `SetOwnerTally`, `SetOwnerRespawnDue`, dirty accessor)
- `Source/StealthFoundation/Private/Roster/StealthRosterCore.cpp`
- `Source/StealthFoundation/Public/Server/StealthRosterService.h` (facade exposure)
- `Source/StealthFoundation/Private/Roster/StealthRosterStore.cpp`
- `Source/StealthFoundation/Public/View/StealthRosterTypes.h` (result codes `Rejected_StaleSequence`, `Rejected_NotInRound`, `NoOp_Unchanged` if not already present)
- `Source/StealthFoundation/Private/Tests/Roster/StealthRosterOwnerViewFeedTest.cpp` (class `FRosterOwnerViewFeedTest`, test name `StealthGame.Roster.OwnerViewFeed`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation. (At tiers where the QL-STORY-READY gate is skipped — `minimal`, or `lean`/`solo` review mode — no qa-lead specs exist, and this section reads: N/A — no qa-lead specs at this tier; implement against the Acceptance Criteria above.)*

**Automated test specs (Logic):**

- **AC-1**: owner-view feed reaches only the owner view with the full precedence.
  - Given: an InRound participant, push-step spy, EventSeq recorded.
  - When: SetOwnerTally(Id,2,7,e) runs and repeats; LastResultSeq 6 sent; a tally of 300 sent (wider parameter type); SetOwnerRespawnDue(Id,T,e) then (Id,0,e); failure inputs in precedence order; ResetAll; reclaim; three Applied calls in one push step.
  - Then: Applied with owner view (2,7), no teammate or other entry changed, no event, EventSeq unchanged. Repeat gives NoOp_Unchanged. Seq 6 gives Rejected_StaleSequence. 300 reads 255. Respawn due reads T then 0. Failures give NotAuthority, WrongThread, UnknownParticipant, StaleEpoch, NotInRound in that order, cache unchanged. ResetAll zeroes all three fields; reclaim keeps them. The three calls give one coalesced update.
  - Edge cases: the tally parameter type must hold 300.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Roster/StealthRosterOwnerViewFeedTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later). `qa.level` is full, so the test is required.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/participant-state-roster/story-037-team-view-serialized-size.md`, `production/epics/participant-state-roster/story-022-reset-all-batch.md`
- Unlocks: `production/epics/participant-state-roster/story-041-team-view-repush-on-controller-swap.md`
