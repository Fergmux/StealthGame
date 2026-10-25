# Story 011: T_due carrier adapter writes the countdown end to the owner part only

> **Epic**: NPC-Kill Penalty & Informants
> **Status**: Ready
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/npc-kill-penalty-informants.md`
**Requirement**: `TR-pen-006`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0019: Informant lifecycle and crowd-pawn takeover (§6, decided by the user 2026-10-25: option (a)), with ADR-0001 (C-TeamView owner part, `FTeamViewOwnerPart`)
**ADR Decision Summary**: Penalty always computes and stores `DueServerTime` and raises `OnDueTimeChanged(P, TOptional<double> T_due)` on every change (set at scheduling and readmission; unset at embodiment, cancellation, round end and reset). The carrier adapter writes `T_due` (double; 0 when unset) into the owner part through the Roster server face `SetOwnerRespawnDue(P, T_due)`, never while `IsLatched()`; it survives reconnects as replicated state and HUD shows a live countdown from it. No reply entry or client derivation may carry it.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Risk is the highest of the governing ADRs: ADR-0001 is HIGH (owner-only replicated state on a `PlayerController`, legacy NetDriver with Iris pinned off). The owner part is replicated state, so it survives a reconnect through the engine's normal re-delivery to a new owner (ADR-0001 E12, verified in legacy source; S14 confirmed it at run time 2026-10-25 for the prototype's possession paths, rejoin and PawnRecovery not yet testable). `T_due` is a new Owner-audience field and needs its Visibility Policy fact and allowlist row (information-visibility-policy epic).

**Control Manifest Rules (this layer)**:
- Required: `T_due` reaches only its owner, only through the C-TeamView owner part — source: ADR-0019
- Forbidden: never write the tally to C-TeamView on a strike; `T_due` is never on the pawn, never in a reply entry — source: ADR-0019
- Guardrail: one owner-part write per scheduling event, none per frame — source: ADR-0019

The countdown end reveals nothing to anyone but the waiting informant (an opponent learning "an informant is waiting until T" would be a leak), so the carrier is the owner-only part and the write rules are strict: only on change, never while latched, always cleared when the wait ends.

**Prototype replaced:** none (the prototype showed an informant a fixed message "Returning to your team's spawn" with no countdown and no carrier).

---

## Acceptance Criteria

*From ADR-0019 §6 and GDD PEN-R15 (all three derived: the GDD's AC-NKP-31 covers the on-screen countdown in story 018), scoped to this story:*

- [ ] **AC1** (derived, ADR-0019 §6): **GIVEN** a respawn scheduled or a readmission **WHEN** `OnDueTimeChanged(P, T_due)` fires **THEN** the adapter calls Roster `SetOwnerRespawnDue(P, T_due, ExpectedEpoch)` exactly once with the double `T_due`; **AND** at embodiment, disconnect-cancellation and reset it writes 0 (unset); at round end `OnDueTimeChanged(P, unset)` still fires but the adapter makes no write because the round is latched (AC2). *(ADR-0019 §6 prints `SetOwnerRespawnDue(P, T_due)`; the GDD PEN-R15 form with `ExpectedEpoch` is used here: correct the ADR text.)*
- [ ] **AC2** (derived, ADR-0019 §6 and PEN-R3/R15): **GIVEN** `IsLatched()` is true **WHEN** `OnDueTimeChanged` fires **THEN** no `SetOwnerRespawnDue` call is made; **AND** across three ordinary strikes no owner-part write of the tally or `T_due` occurs.
- [ ] **AC3** (derived, ADR-0019 §6 and IVP Owner audience): **GIVEN** an awaiting informant in a 3v3 PIE **WHEN** every connection's received properties are logged **THEN** only the owner connection receives `T_due` (on the C-TeamView owner part, never on the pawn), a reclaim before `T_due` carries the same `T_due` (after `T_due` it is recomputed by story 010), and the reflected RPC signatures and reply structs carry no `DueServerTime`.

---

## Implementation Notes

*Derived from ADR-0019 Decision §6:*

`OnDueTimeChanged` is a server-only multicast delegate on `UPenaltyService` (`FOnDueTimeChanged(FParticipantId, TOptional<double>)`, declared in `Public/Server/`). Story 009 and 010 raise it at every change of `DueServerTime`; this story adds the **adapter**, a small class registered at the composition root that binds the delegate and calls the Roster's owner-view feed `SetOwnerRespawnDue(P, T_due, ExpectedEpoch)` (roster `story-038-owner-view-feed.md`; `T_due = 0` clears). The Roster is the only writer of the owner part and its push step orders the precedence; the adapter holds no copy.

Never write while `IsLatched()`: ADR-0002's latch gate also freezes owner writes after the deciding slot, and a stale write after a reset is `Rejected_StaleEpoch`. The unset events: takeover `Applied` (embodiment), `RespawnCancelled`, `OnRosterRoundEnded`, `OnRosterReset`. A reclaim mid-timer leaves `T_due` unchanged; the owner part still carries it (replicated state), and the countdown resumes on the owner's client from it (the HUD side is story 018).

The leak check (AC3) uses the PIE-3 harness differential capture: owner connection receives the `T_due` field on the owner part; allies and opponents receive nothing carrying it; it appears on no pawn and no reply struct. Add the `T_due` field to the IVP leak-inventory (information-visibility-policy's allowlist row) if it is not there; the harness boot assertion that fails on an unknown replicated property is the safety net.

Seam fakes: for the Logic part (AC1, AC2) the Roster feed is a recording fake; AC3 runs the real Roster and carriers.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-009-respawn-due-time-scheduling-and-cancellation.md`: computing `T_due` and raising `OnDueTimeChanged`.
- `story-018-tally-countdown-and-exile-card-ui.md`: drawing the countdown (HUD owns the widget).
- `story-016-strike-count-never-leaves-and-removal-audiences.md`: the `StrikeCount` and removal-audience captures.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Private/Features/Penalty/StealthPenaltyDueAdapter.h` and `StealthPenaltyDueAdapter.cpp`
- `Source/StealthGameplay/Public/Server/Features/Penalty/StealthPenaltyService.h` (`FOnDueTimeChanged`, if not added by story 009)
- `Source/StealthGame/Private/` composition root registration of the adapter
- `Source/StealthGameplay/Private/Tests/Penalty/PenaltyDueAdapterTest.cpp` (class `FPenaltyDueAdapterTest`, name `StealthGame.Penalty.DueAdapter`)
- `Source/StealthTestHarness/Private/Penalty/DueOwnerOnlyTest.cpp` (CQTest, `StealthGame.Penalty.PIE.DueOwnerOnly.*`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC1** (derived, `StealthGame.Penalty.DueTimeAdapter`)
  - Given: a fake Roster that records `SetOwnerRespawnDue(P, T_due, ExpectedEpoch)` with the adapter bound to `OnDueTimeChanged`.
  - When: a schedule or readmission fires with 115.0; then embodiment, disconnect-cancellation and reset; then round end.
  - Then: exactly one call per change with the double and the current epoch; embodiment, disconnect and reset each write 0; at round end `OnDueTimeChanged(P, unset)` fires and 0 calls are made.
  - Edge cases: Picking writes nothing (T_due stays in the past and HUD shows the static "Returning"); reset must run with `IsLatched()` false: confirm Round clears the latch first.
- **AC2** (derived, `StealthGame.Penalty.DueTimeAdapterLatched`)
  - Given: `IsLatched()` is true.
  - When: `OnDueTimeChanged` fires with a value and with unset; separately three ordinary strikes.
  - Then: 0 `SetOwnerRespawnDue` calls; the strikes cause 0 `SetOwnerTally` and 0 `SetOwnerRespawnDue` calls.
  - Edge cases: the latch flipping mid-drain.
- **AC3** (derived, `StealthGame.Penalty.PIE.TDueOwnerOnly.<Method>`)
  - Given: an awaiting informant in 3v3 PIE.
  - When: every connection's received properties are logged and the owner disconnects and reclaims before `T_due`.
  - Then: only the owner connection receives `T_due`, on the C-TeamView owner part, never on the pawn; after the reclaim the value is the same; reflected RPC signatures and reply structs carry no `DueServerTime`.
  - Edge cases: a reclaim after `T_due` is recomputed (story 010); disconnect writes 0 and readmission rewrites it.

QL-STORY-READY: ADEQUATE

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: the CQTest PIE test under `Source/StealthTestHarness/Private/Penalty/` and the unit test under `Source/StealthGameplay/Private/Tests/Penalty/` OR a documented playtest

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/npc-kill-penalty-informants/story-009-respawn-due-time-scheduling-and-cancellation.md`, `story-010-absent-path-readmission-and-lost-body.md`; cross-epic: participant-state-roster `story-038-owner-view-feed.md` (`SetOwnerRespawnDue`), information-visibility-policy (the `T_due` Owner fact and allowlist row), tuning-data-config `story-019-cqtest-headless-proof.md`
- Unlocks: `production/epics/npc-kill-penalty-informants/story-018-tally-countdown-and-exile-card-ui.md`
