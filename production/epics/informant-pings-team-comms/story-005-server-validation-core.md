# Story 005: Server validation core: order, drop reasons and range

> **Epic**: Informant Pings & Team Communication
> **Status**: Ready
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/informant-pings-team-comms.md` (PNG-R2, PNG-R5, F2; AC-PNG-04)
**Requirement**: `TR-png-002` (also `TR-png-009` validation order and `TR-png-006` server half)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0011: Pawn command interface and input pipeline (Technical Setup 2026-10-25 `ServerTeamPing` validation order); structure from ADR-0007 §5 (plain-C++ cores) and ADR-0002 (the handler runs outside the queue)
**ADR Decision Summary**: The server handler checks, in order, (1) phase `Active` and Round's latch not set, (2) sender `State` `Active` or `Informant`, bound pawn, not concealed, (3) finite point within `PingRangeCm + PingRangeSlackCm` of the sender pawn's eye pivot, (4) the token bucket; drops are silent and counted by reason (`Phase`, `Latch`, `State`, `Pawn`, `Concealed`, `Range`, `NonFinite`, `Bucket`); only an accepted ping spends a token; acceptance stamps `ServerTime = ServerNow()` and advances the sender's wrapping `PingSeq`.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0011 overall; this story is headless C++ with injected gateways, no engine API)
**Engine Notes**: `FTeamPingCore` takes Roster, Round, clock and the concealment predicate through small injected interfaces so `StealthGame.Pings.*` specs run with no `UWorld`. Use `FVector` math in `double` (UE 5 LWC) and `FMath::IsFinite` (or `!ContainsNaN()` plus a finite check) for the non-finite case.

**Control Manifest Rules (this layer)**:
- Required: validate in order phase/latch, state/pawn/concealment, range, bucket; the first failure drops silently with its reason counted; failures before the bucket spend no token — source: ADR-0011
- Required: no line-of-sight check; range from the sender pawn's eye pivot on both machines, never the camera — source: ADR-0011, ADR-0015
- Forbidden: Never answer a dropped `ServerTeamPing` with a reply, client RPC, replicated field or client-visible log row — source: ADR-0011
- Guardrail: handler cost sits inside the queue-drain 0.5 ms line — source: ADR-0002, ADR-0022

---

## Acceptance Criteria

*From GDD `design/gdd/informant-pings-team-comms.md` (AC-PNG-04, PNG-R5, F2), scoped to this story:*

- [ ] **AC1** (AC-PNG-04, `[Logic]` `StealthGame.Pings.Validation`): **GIVEN** the headless `FTeamPingCore` with injected Roster, Round and clock and a full bucket at the start of each case (each case a separate fixture) **WHEN** pings arrive with phase `Preparing`, phase `Active` after the latch, sender `RoundEnded`, an informant with no bound pawn, a point 6151 cm from the eye pivot, a point 6149 cm from the eye pivot, and a NaN point **THEN** only the 6149 cm ping is accepted (counted as accepted); the other six are dropped with the reasons Phase, Latch, State, Pawn, Range and NonFinite respectively, each counted once, no reply is produced, and every dropped case's bucket still holds 2 tokens. *(The GDD's AC-PNG-04 lists a second `Range` in this sequence; the accepted ping is not a drop, so six drops carry six reasons. Flagged for the GDD owner.)*
- [ ] **AC2** (PNG-R5, PNG-R2): **GIVEN** a ping that fails several steps (State and Range) **WHEN** validated **THEN** only the first failing step in the order phase/latch, state/pawn/concealment, range, bucket is counted and no later step runs; **GIVEN** accepted pings from an `Active` and an `Informant` sender **THEN** the entries have identical shape (no role branch).
- [ ] **AC3** (F2): **GIVEN** the eye pivot at (0, 0, 165) **WHEN** a point at (5900, 1000, 0) (5986 cm) arrives **THEN** it is accepted, and one at (6200, 0, 0) (6202 cm) is dropped; the distance is measured from the eye pivot at validation, never from the camera; there is no line-of-sight check (a point behind a wall within range is accepted).
- [ ] **AC4** (TR-png-009): **GIVEN** an accepted ping **THEN** `ServerTime` equals `ServerNow()` at acceptance and the sender's `PingSeq` (uint8, per participant) advances by one, wrapping 255 to 0, and resets at `OnRosterReset`.

---

## Implementation Notes

*Derived from ADR-0011 Technical Setup 2026-10-25 and ADR-0007 §5:*

`FTeamPingCore::Submit(ParticipantId, FNetTeamPing, Now) -> FPingDecision { Accepted | Dropped(Reason) }` runs the four steps in order; the concealment test is an injected `TFunction<bool(ParticipantId)>` (Concealment's `IsInBushOrTransition`, bound at the composition root; story 011 owns the concealed behaviour and its tests, so here a stub returning false is enough and one case asserts that a stub returning true yields `Concealed` ahead of range). Drop reasons are an enum counted in a server-only stats struct that story 024 forwards to telemetry; the core never logs through `UE_LOG`. A drop never spends a token: call the bucket's `TrySpend` (story 004) only after steps 1-3 pass. Entries for Active and Informant senders are built by the same code path. The core returns the entry to write; story 006 writes it into the parts.

Rate-limit fairness is the bucket's; do not add a second limiter. The listen host and a remote client use the same function (story 006).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 004: the bucket arithmetic.
- Story 006: the RPC, the service, writing the slots into the team's parts.
- Story 011: the concealment predicate's behaviour for each bush state and the client refusal feedback.
- Story 012: the strike-window drop in a real match (this story only proves `State` and `Pawn` drops at core level).
- Story 024: forwarding the counters to telemetry.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Private/Features/Pings/StealthTeamPingCore.h` and `StealthTeamPingCore.cpp`
- `Source/StealthGameplay/Private/Tests/Pings/PingValidationTest.cpp` (class `FPingsValidationTest`, name `StealthGame.Pings.Validation`)
- `Source/StealthGameplay/Private/Tests/Pings/PingValidationOrderTest.cpp` (class `FPingsValidationOrderTest`, name `StealthGame.Pings.ValidationOrder`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

**Test: AC1 validation cases**
- Given: the headless `FTeamPingCore` with injected Roster, Round and clock; a fresh full bucket per case.
- When: pings arrive with phase `Preparing`; `Active` after the latch; sender `RoundEnded`; an informant with no bound pawn; 6151 cm; 6149 cm; NaN.
- Then: only 6149 is accepted; the six drops carry Phase, Latch, State, Pawn, Range, NonFinite; no reply; dropped buckets still hold 2 tokens.
- Edge cases: +Inf and -NaN also NonFinite; the server limit is 6000 + 150, so 6150 exactly is not asserted.

**Test: AC2 first failing step only; no role branch**
- Given: a ping failing State and Range; accepted pings from an `Active` and an `Informant` sender.
- When: validate.
- Then: only the State drop is counted and no later step runs; the two entries have identical shape.
- Edge cases: Range plus Bucket failure counts Range; Phase plus State counts Phase.

**Test: AC3 range measured from the eye pivot**
- Given: eye pivot (0, 0, 165).
- When: points (5900, 1000, 0) [5986 cm] and (6200, 0, 0) [6202 cm]; a point behind a wall within range.
- Then: first accepted, second dropped, third accepted; the camera position is never read; no line-of-sight call is made.
- Edge cases: a camera far from the pivot does not change the result.

**Test: AC4 timestamp and sequence**
- Given: accepted pings from one sender.
- When: accept 256+ pings in sequence; fire `OnRosterReset`.
- Then: `ServerTime` equals `ServerNow()` at acceptance; `PingSeq` +1 each, 255 wraps to 0; the seq resets at reset.
- Edge cases: a dropped ping does not advance `PingSeq`.

QL-STORY-READY: GAPS (revised) — AC1's reason list was corrected as above on the qa-lead's proposal.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Pings/PingValidationTest.cpp` and `PingValidationOrderTest.cpp` — must exist and pass (`/story-done` checks that they EXIST; pass/fail is established by `/gate-check` and `/smoke-check`)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/informant-pings-team-comms/story-001-ping-tuning-keys-and-invariant.md`, `story-002-team-ping-wire-types-and-slot-component.md`, `story-004-ping-token-bucket-core.md`
- Unlocks: `production/epics/informant-pings-team-comms/story-006-server-team-ping-handler-and-host-parity.md`, `story-011-concealed-press-refusal.md`, `story-012-strike-window-drop.md`, `story-024-ping-telemetry-statistics.md`
