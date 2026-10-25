# Story 004: Penalty Event Log rows, server-only and never through UE_LOG

> **Epic**: NPC-Kill Penalty & Informants
> **Status**: Ready
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/npc-kill-penalty-informants.md`
**Requirement**: `TR-pen-008`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0019: Informant lifecycle and crowd-pawn takeover (§7), with ADR-0002 (Event Log ring and flags) and ADR-0007 (Event Log never reaches logging)
**ADR Decision Summary**: Penalty writes server-only rows, `PostRoundEligible` unless stated: `StrikeCounted{Killer, StrikeAfter, TallyAfter, Graced}`, `RespawnScheduled{Participant, Cause, t_lost, T_due}`, `InformantRespawn{Participant, PickLatencySec, WideningLevel}`, `RespawnCancelled{Participant, Reason}`, `PawnLost{Participant, Cause}`. LSC writes `RespawnStarved`; Round writes `StrikeLimit` with the transition. Nothing passes through `UE_LOG`/`UE_LOGF`, the output log, a client message or a trace channel.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: No engine API: rows go to Round's Event Log writer (ADR-0002 §8, Round R19). `UE_LOGF` is the 5.8 preferred logger and is exactly what these rows must never use; the include/source lint named in ADR-0007 enforces it.

**Control Manifest Rules (this layer)**:
- Required: Event Log rows are written server-side inside a queue slot with the Event Log flags the owning ADRs name — source: ADR-0002, ADR-0019
- Forbidden: Event Log content never reaches `UE_LOG`/`UE_LOGF`, the output log, `ClientMessage`, crash dumps or trace channels — source: ADR-0007 (Round R19)
- Guardrail: rows are fixed-size structs, no allocation inside the slot — source: ADR-0002

Penalty's rows are what the post-round reveal, telemetry (story 022) and tests read. This story defines the row types and the writer call sites' shape; the call sites themselves are completed by the stories that produce each event (002, 005, 009, 010, 012, 013), each of which calls the writer interface created here.

**Prototype replaced:** none (the prototype has no Event Log; it wrote `UE_LOG` lines). The prototype's penalty `UE_LOG` calls in `StealthGameMode.cpp` go in story 021.

---

## Acceptance Criteria

*From GDD `design/gdd/npc-kill-penalty-informants.md` (AC-NKP-22) and ADR-0019 §7 / ADR-0007 (derived item marked), scoped to this story:*

- [ ] **AC1** (AC-NKP-22 as clarified at QL-STORY-READY, [L] `StealthGame.Penalty.EventLogRows`): **GIVEN** an exile, an informant kill, a respawn, a cancellation and an informant body lost to KillZ **THEN** the in-memory sink holds each Penalty row once, with the slot key and exactly the ADR-0019 §7 fields: `StrikeCounted{Killer, StrikeAfter, TallyAfter, Graced}`, `RespawnScheduled{Participant, Cause, t_lost, T_due}`, `InformantRespawn{Participant, PickLatencySec, WideningLevel}`, `RespawnCancelled{Participant, Reason}`, `PawnLost{Participant, Cause}`; the `StrikeLimit` row is Round's (written with the transition) and is asserted in the integration runs of stories 005 and 007 with the real Round, not in this Penalty fixture; the no-`UE_LOG` clause of the GDD AC is AC2's lint (PEN-R17).
- [ ] **AC2** (derived, ADR-0019 §7 and PEN-R17): **GIVEN** a stub Event Log sink that records the queue-slot context **WHEN** each Penalty row is written **THEN** its name and `PostRoundEligible` flag equal the PEN-R17 table (all five Penalty rows are `PostRoundEligible`), every write happens inside a queue slot, and a source lint finds no `UE_LOG`/`UE_LOGF` that takes an Event Log row or field as an argument, and no `ClientMessage`, client RPC or trace channel carrying one, in Penalty sources.

---

## Implementation Notes

*Derived from ADR-0019 Decision §7 and ADR-0007:*

Define the five Penalty row payload structs next to the Penalty service (server-only header in `Public/Server/Features/Penalty/`, guard include first) and register them with Round's Event Log writer through the writer interface Round exposes (`round-match-framework` `story-011-event-log-content.md`; the ring and header are Round's). Fields exactly as the ADR §7 list; `Cause` is `ERespawnCause` (declared by Round beside `IInformantBodyProvider`: `Assassinated`, `ReversedAgainst`, `StrikeLimit`, `InformantKilled`, `PawnLost`, `Readmitted`). `Reason` for `RespawnCancelled` is an enum (`Disconnect`, `RoundEnded`, `Reset`, `Stale`) and never free text. `StrikeLimit` is Round's existing row, written with the transition (story 005 only asserts it exists); `RespawnStarved` is LSC's row (story 013 asserts it appears, it does not write it).

All rows are `PostRoundEligible` unless the GDD says otherwise (PEN-R17 states none otherwise). Fixed-size structs, no `FString` fields (no allocation in the slot, nothing text-shaped to leak). The lint names for the Event Log writer are in ADR-0007 ("no voice/chat modules or plugins… Event Log lint names"): check them in the 5.8 project's lint config before naming the row types.

The source lint for AC2 is a Logic test that scans `Source/StealthGameplay/*/Features/Penalty/**` for `UE_LOG`, `UE_LOGF`, `TRACE_*`, `ClientMessage` and `GEngine->AddOnScreenDebugMessage`; the allowed exception list is empty. Rows must be written only from within a slot: the stub sink asserts a slot-context flag set by the stub queue.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Each producing story (`story-002`, `story-005`, `story-009`, `story-010`, `story-012`, `story-013`): the call sites for the row it produces; this story creates the row types, the writer interface and the lint.
- `story-022-telemetry-and-playtest-report.md`: aggregation of the rows.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Public/Server/Features/Penalty/StealthPenaltyEventRows.h`
- `Source/StealthGameplay/Private/Features/Penalty/StealthPenaltyEventLog.cpp`
- `Source/StealthGameplay/Private/Tests/Penalty/PenaltyEventLogRowsTest.cpp` (class `FPenaltyEventLogRowsTest`, name `StealthGame.Penalty.EventLogRows`)
- `Source/StealthGameplay/Private/Tests/Penalty/PenaltyNoLoggingLintTest.cpp` (class `FPenaltyNoLoggingLintTest`, name `StealthGame.Penalty.NoLoggingLint`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC1** (AC-NKP-22, `StealthGame.Penalty.EventLogRows`)
  - Given: `FPenaltyCore`, fake gateways and a fresh in-memory sink for each of five scenario fixtures: exile, informant kill, respawn commit, cancellation, KillZ loss.
  - When: each scenario runs in its own slot.
  - Then (slot key and exact ADR-0019 section 7 fields on every row): exile: one `StrikeCounted` and one `RespawnScheduled{P, StrikeLimit, t_lost, T_due}`; informant kill: one `StrikeCounted` with `StrikeAfter` unchanged and one `RespawnScheduled{InformantKilled}`; respawn commit: `InformantRespawn{P, PickLatencySec = now - T_due, WideningLevel}`; cancellation: `RespawnCancelled{P, Disconnect}`; KillZ loss: `PawnLost{P, Cause}`; no `StrikeLimit` row in this fixture.
  - Edge cases: "each row once" is per fixture (`RespawnScheduled` appears in two scenarios); confirm `StrikeCounted` is written for every NPC or informant kill (ADR-0019 section 2.1 and the `Graced` field imply it); compare `T_due` and `t_lost` exactly against the fake clock; rows depend on code from stories 005, 009, 010 and 012.
- **AC2** (derived, `StealthGame.Penalty.EventLogRowSchema`, automation test plus source lint)
  - Given: a stub sink that records slot context and flag, and the PEN-R17 table as a fixture.
  - When: all five Penalty rows are written through their real code paths and the lint scans the Penalty sources.
  - Then: each name equals the table and each `PostRoundEligible` is true; every write has a non-null slot context; the lint finds no `UE_LOG`/`UE_LOGF` with a row or field argument and no `ClientMessage`, client RPC or trace channel carrying one.
  - Edge cases: watch a seeded violating call fail the lint once (ADR-0007 verification); a direct write outside a slot must be rejected.

QL-STORY-READY: ADEQUATE

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: the two tests under `Source/StealthGameplay/Private/Tests/Penalty/` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/npc-kill-penalty-informants/story-001-penalty-core-store-and-service-shell.md`; cross-epic: round-match-framework `story-011-event-log-content.md` (the writer and `PostRoundEligible` flag)
- Unlocks: `production/epics/npc-kill-penalty-informants/story-005-exile-sequence-in-attack-slot.md`, `story-009-respawn-due-time-scheduling-and-cancellation.md`, `story-022-telemetry-and-playtest-report.md`
