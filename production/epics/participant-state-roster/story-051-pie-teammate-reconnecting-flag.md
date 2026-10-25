# Story 051: A teammate's reconnecting flag and end cause follow the event-only clear rules in all six cases

> **Epic**: Participant State & Roster
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/participant-state-roster.md`
**Requirement**: `TR-roster-006`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0001: Crowd identity rules
**Also governed by**: ADR-0005: Session, seamless travel and match-scoped state stores
**ADR Decision Summary**: ADR-0001 puts Team-audience data only in C-TeamView components on each teammate's own PlayerController: the Roster part carries per teammate `{PublicHandle, State, PawnRef, IsConcealed, RetireAtServerTime, bReconnecting, ReconnectEndCause}`, pushed on change only, at most once per server frame, all changes in a frame coalesced. `bReconnecting` (one bit) is derived by the Roster push step from Session's existing `Connection` projection and cleared only by an event (Rejoined, GraceExpired or Cleared) written together with `ReconnectEndCause` in the same update, never by a timer (ruling D23). ADR-0005 supplies the projection (`NotifyConnectionChanged`, with `bKicked`), the supersede rule (a supersede is a disconnect at the supersede instant, D24) and the rejoin by retained-PlayerState adoption.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Post-cutoff: `APawn::RemoteViewPitch16` (5.6) [V]; `net.Iris.UseIrisReplication` default 0 (legacy NetDriver, Iris pinned off) [V]. ADR-0005 notes the 5.6 `DispatchPostLogin` deprecation (use `PostLogin`/`OnPostLogin`). Verification required: S1, S2, S14 (ADR-0001) and S3, S4, S5 (ADR-0005); the harness startup assertion that the game NetDriver runs the legacy replication system. This story is a single-process PIE test with a stub Session, so S3/S4 are not direct inputs; the rejoin cases against a real Session are covered again by Stories 057 and 059.

**Control Manifest Rules (this layer)**:
- Required: the Roster teammate entry carries Team-audience `bReconnecting`, derived only by the Roster push step from Session's `Connection` projection (set when the projection becomes `Disconnected` during `Active`; cleared only by a Roster event, Rejoined, GraceExpired or Cleared, never by a timer; ruling D23) — source: ADR-0001, ADR-0005 (user decision 2026-10-25)
- Required: Team-audience data travels only in a `UTeamViewComponentBase` subclass on the receiving teammate's own PlayerController; the Roster push step is the only writer — source: ADR-0001
- Forbidden: Never add a teammate connection field other than the one `bReconnecting` bit (no ping, time, reason or address), and never send it to a non-teammate — source: ADR-0001, ADR-0005 (user decision 2026-10-25)
- Forbidden: Never use a team-filtered actor, a per-team replica actor or a team relevancy rule — source: ADR-0001
- Guardrail: C-TeamView pushes change-only, at most one per frame, within the 50 KB/s per-client budget — source: ADR-0001

---

## Acceptance Criteria

*From GDD `design/gdd/participant-state-roster.md`, scoped to this story:*

- [ ] **AC-PSR-53** [PIE-3][Dep:Session] `StealthGame.Roster.PIE.TeammateReconnectingFlag` (R6, user decision 2026-10-25; rewritten for the event-only clear, ruling D23, cross-review S-B1 2026-10-25). Every case reads the team-0 ally's C-TeamView entry `{bReconnecting, ReconnectEndCause}`; a clear is accepted only if both fields change in the same net update:
  - (a) **Active ally, rejoin then expiry.** GIVEN a 2v2 Active round, WHEN a team-0 player disconnects, rejoins inside `DisconnectGraceSec`, disconnects again and lets the grace expire, THEN the entry shows `true/None` from the update after Session's `Disconnected` projection, `false/Rejoined` at the rejoin completion, `true/None` again on the second disconnect and `false/GraceExpired` at the grace-expiry transition (the same update as the `Active -> Informant` change); the flag is false in `Preparing`, `Ended` and after `OnRosterReset` (cause `None`); team-1 clients receive no byte for either field (their replicated-property logs equal a control run); the entry carries no time or reason field.
  - (b) **Informant ally disconnected at grace expiry.** GIVEN the ally was already an `Informant` and disconnects, WHEN server time passes `max(DisconnectedAt, ActivePhaseStart) + G_eff` and `3 x G_eff`, THEN the entry stays `true/None` with no update at either time (no timer clears it); WHEN it then rejoins, THEN `false/Rejoined`; the HUD prints "back" only then (AC-HUD-28).
  - (c) **Active ally killed while disconnected.** GIVEN an `Active` ally disconnected and AI-driven, WHEN a rival strike makes it an `Informant` (`Assassinated`), THEN the entry keeps `true/None` through the transition update and through its later `NoOp_AlreadyInState` grace expiry, and clears only at a rejoin (`Rejoined`) or `EndRoundAll` (`Cleared`).
  - (d) **Superseding rejoin outlasting `G_eff`.** GIVEN a live ally whose key reconnects (supersede, Round R17.3, decision D24), WHEN the rejoin handshake is held open beyond `G_eff` counted from the supersede instant, THEN the entry is `true/None` from the supersede instant until the grace-expiry transition, then `false/GraceExpired`; the rejoin completing afterwards writes nothing and prints no "back"; AND GIVEN the rejoin completes before `G_eff`, THEN `false/Rejoined` at completion, never earlier.
  - (e) **Kicked ally.** GIVEN a host kick of an `Active` ally (Session `Disconnected(Kicked)` with `bKicked`), THEN the entry is never `true`, or if it was set by the preceding `Disconnected` it clears in the same update as `Cleared`; no "reconnecting" line follows, and the grace-expiry conversion later writes no further change.
  - (f) **Round end.** GIVEN an ally flagged `true/None` at the round-end step, THEN `EndRoundAll` writes `false/Cleared` in the same forced net update as the `RoundEnded` records.

---

## Implementation Notes

*Derived from ADR-0001 section 5 (Roster part) and Implementation Guidelines, ADR-0005 sections 2 and 5, and GDD R6 (`bReconnecting`, `ReconnectEndCause`), R11 (projection details, kick path, duplicate connection), R12:*

- This story is the lead test for MVP item 10 (teammate-reconnecting indicator) and ruling D23. It asserts the writer behaviour that earlier stories deliver: the push step (Story 038 and Story 041) derives flag and cause from Session's `NotifyConnectionChanged` projection (Story 009, including the optional `bKicked`) and from the Roster's own transition and batch calls (`RequestTransition` Disconnect, `EndRoundAll`, `ResetAll`, `RemoveParticipant`, Story 014, 022, 023, 029). No new writer exists. If any case needs a push-step rule that no earlier story implemented (for example the Round-phase gate "while the Round phase is Active", or the "already Disconnected when Active begins" set), implement it here in the push step and say so in the Files list.
- Rules to implement and observe (R6): set when the teammate's `Connection` projection becomes `Disconnected` while the Round phase is `Active` and its `State` is `Active` or `Informant` (also at the moment `Active` begins for a participant already `Disconnected`); cleared only by one of three events, each written with `ReconnectEndCause` in the same net update: (1) Rejoined (the rejoin completes: `NotifyConnectionChanged(Connected)` with `OnParticipantRejoined`, for an Active or an Informant alike), (2) GraceExpired (an applied `Active -> Informant` with Reason `Disconnect`; never applies to an Informant), (3) Cleared (record removed, host-kicked via `bKicked`, or `EndRoundAll`). `OnRosterReset` rebuilds the view (flag false, cause `None`). Nothing else clears it; in particular a killed disconnected Active ally stays flagged and a superseding rejoin keeps the flag until the rejoin completes.
- `ReconnectEndCause` is a uint8 enum `{None = 0, Rejoined = 1, GraceExpired = 2, Cleared = 3}`, Team audience, `None` while the flag is true, and keeps its value until the flag next becomes true (which writes `None` in the same update). One bit plus a two-bit cause packed into the entry's flags byte (no growth, ADR-0001 amendments); no time, reason, ping or address field (Story 037 owns the size budget).
- Test harness: PIE 2v2 with a stub Session (until `production/epics/session-travel/EPIC.md` lands), an injected server clock (the Roster's integer-millisecond clock) and a dev grace value to keep runs short (`DisconnectGraceSec` below 10 is dev-preset only). Property-level replicated logs on a team-0 client and a team-1 client; a clear is accepted only if both fields change in the same net update; the team-1 log must equal a control run.
- Case notes from QA: in (e) accept "never true" or "cleared in the same update as `Cleared`". The HUD "back" line is owned by AC-HUD-28 (HUD epic) and is not asserted here beyond the cause value. Gating runs use fixed net emulation (PktLag 100 ms, no variance, PktLoss 0 %) with frame or millisecond timeouts; a non-gating [Soak] run is advisory.
- Prototype disposition: no prototype counterpart (nothing like it existed; the prototype `StealthPlayerState.h` replicated team and life state instead).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 037 (Maximal C-TeamView net-serializes in at most 96 bytes): the `FTeamView` struct, `NetSerialize` and the size budget.
- Story 009 (Session connection states project onto Connection fields only): the `NotifyConnectionChanged` projection and `ConnectionGeneration`.
- Story 029 (Reclaim restores the same record for active and informant participants): `Reclaimed` core behaviour.
- Story 054 (Disconnect grace expires on the injected clock and stale-connection timers are rejected): grace timing and stale-generation rejection.
- Story 059 (A client rejoining within grace reclaims its record, pawn and informant status): real multi-process rejoin.
- Story 041 (Replaced controller or reclaim re-pushes C-TeamView within one server frame): re-push on controller swap.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthTestHarness/Private/Roster/StealthRosterTeammateReconnectingFlagTest.cpp` (CQTest `NETWORK_TEST_CLASS(TeammateReconnectingFlag, "StealthGame.Roster.PIE")`, exempt from the `F<System><Scenario>Test` class-name rule)
- `Source/StealthTestHarness/Private/Roster/StealthRosterSessionStub.h` (stub Session driving `NotifyConnectionChanged`, supersede and kick)
- `Source/StealthFoundation/Private/Roster/StealthRosterCore.h` and `.cpp` (only if a push-step derivation rule is still missing; shared file)
- `Source/StealthFoundation/Public/View/StealthRosterTeamView.h` (entry fields `bReconnecting`, `ReconnectEndCause`; defined by Story 037, only checked here)
- Header names follow ADR-0007: `Stealth` prefix, unique project-wide.

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation. (At tiers where the QL-STORY-READY gate is skipped — `minimal`, or `lean`/`solo` review mode — no qa-lead specs exist, and this section reads: N/A — no qa-lead specs at this tier; implement against the Acceptance Criteria above.)*

**For Logic / Integration stories — automated test specs:**

- **AC-1**: a teammate's reconnecting flag and end cause follow the event-only clear rules in all six cases.
  - Given: PIE 2v2 with a stub Session, an injected server clock, a dev grace value to keep runs short, property-level logs on team-0 and team-1 clients.
  - When: cases (a) to (f) run.
  - Then: each case matches the AC's flag and cause timeline. A clear is accepted only if both fields change in the same net update. Team-1 logs equal a control run. The entry carries no time or reason field.
  - Edge cases: in (e) accept "never true" or "cleared in the same update as Cleared". The HUD "back" line is owned by AC-HUD-28.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: automated test `Source/StealthTestHarness/Private/Roster/StealthRosterTeammateReconnectingFlagTest.cpp` (`StealthGame.Roster.PIE.TeammateReconnectingFlag`) OR a documented playtest; `qa.level` is full, so the automated test is required.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/participant-state-roster/story-037-team-view-serialized-size.md`, `production/epics/participant-state-roster/story-009-connection-projection.md`, `production/epics/participant-state-roster/story-029-reclaim-core.md`
- Cross-epic: `production/epics/session-travel/EPIC.md` ([Dep:Session]); the test runs against a stub Session until it lands
- Unlocks: None
