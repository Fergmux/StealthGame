# Story 021: RetireAtServerTime reaches teammate entries for each retiring reason

> **Epic**: Participant State & Roster
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

This story makes the Roster write the correct `RetireAtServerTime` into the model that the C-TeamView push step reads, so teammates' markers and HUD can time the retire of a dying body from the kill's contact time instead of from packet arrival. `RequestTransition` takes an optional `RetireAtServerTime`: Assassination passes the kill's `ContactServerTime` for `Assassinated` and `ReversedAgainst`, Penalty passes the exiling attack's slot time for `StrikeLimit` (the release path), and for `Disconnect` it is omitted and defaults to the call's server time. It realises GDD rule R8 (the `RetireAtServerTime` paragraph), R9 (`RetireBinding` time) and R6 (teammate entry field), and is asserted against the core's teammate-view projection because the carrier itself arrives in stories 037 and 038.

**GDD**: `design/gdd/participant-state-roster.md`
**Requirement**: `TR-roster-002`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0005: Session, seamless travel and match-scoped state stores
**Also governed by**: ADR-0001: Crowd identity rules; ADR-0010: Shared Pawn: CMC locomotion, spawn factory and possession parity
**ADR Decision Summary**: The Roster is one of four game-instance stores and the only authoritative owner of participant records and bindings; `FRosterCore::RequestTransition(P, Informant, StrikeLimit, ExpectedEpoch)` releases rather than retires the live binding in the same call (the pawn becomes `Unbound`, the card stays `NpcLive`), while every other `Active -> Informant` reason retires it. Team-audience data reaches clients only through the owner-only C-TeamView carrier written by the Roster push step (ADR-0001), so the retire time must be produced by the Roster core and never by a feature. The retire time comes through `RetireBinding(Pawn, RetireAtServerTime)`, and is non-zero only in the retiring update.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Networking domain (HIGH per VERSION.md). No new post-cutoff APIs; new logging uses `UE_LOGF` (ADR-0007). Verification required by the ADR is behavioural: spikes S3, S4 and S5 (not yet reported); ADR-0001 spikes S1 (pawn parity partly FAIL, handed to ADR-0003 and ADR-0013 work), S2 (PASS) and S14 (PASS) concern the replicated carriers, not this core. This story runs entirely inside the engine-free `FRosterCore` and is not gated by any of them.

**Control Manifest Rules (this layer)**:
- Required: Penalty supplies tally and `T_due` only through `SetOwnerTally` / `SetOwnerRespawnDue`, and the retire time comes through `RetireBinding(Pawn, RetireAtServerTime)`; the Roster push step is the only writer — source: ADR-0001, ADR-0019
- Required: `RetireAtServerTime` is non-zero only in the retiring update (the kill's `ContactServerTime` or the release slot time) — source: ADR-0001, ADR-0018, ADR-0019
- Required: Queue, Roster, tuning validator, seed mixer and appearance deck are plain C++ cores (`F…Core`) with no `UWorld` dependency, behind a subsystem facade — source: ADR-0007
- Forbidden: Never add a teammate connection field other than the one `bReconnecting` bit (no ping, time, reason or address), and never send it to a non-teammate — source: ADR-0001, ADR-0005
- Forbidden: Never use wall-clock time as a test pass/fail predicate; never use wall-clock waits, unseeded randomness or shared mutable state in tests — source: ADR-0008
- Guardrail: Identity policy: C-TeamView pushes change-only, <= 1 per frame, within the 50 KB/s per-client budget — source: ADR-0001

---

## Acceptance Criteria

*From GDD `design/gdd/participant-state-roster.md`, scoped to this story:*

- [ ] **AC-PSR-57** [Core] `StealthGame.Roster.RetireAtServerTimeSource` (R8, cross-review C-W31 2026-10-25): GIVEN an Active attacker and victim with live bindings, WHEN `RequestTransition(victim, Informant, Assassinated, ..., RetireAtServerTime = T_c)` is `Applied`, THEN the victim's binding is `Retired` and every teammate's C-TeamView entry for it carries `RetireAtServerTime = T_c`; the same for `ReversedAgainst` (the attacker's entry) and for `StrikeLimit` (the exiling slot time, with the pawn released instead of retired); for `Disconnect` with the argument omitted the entry carries the call's server time; a call for `Assassinated` with the argument omitted is flagged by the contract test as a defect (the entry would carry the call's server time).

---

## Implementation Notes

*Derived from ADR-0005 section 2 (strike-limit release inside the transition), ADR-0001 carrier model, and GDD rules R8, R9, R6:*

- Extend `FRosterCore::RequestTransition` (`Source/StealthFoundation/Private/Roster/StealthRosterCore.h/.cpp`) with the optional `RetireAtServerTime` argument (server time in integer ms, 0 or an optional wrapper meaning "omitted"; the exact encoding is chosen here and must let the core distinguish "omitted" from a supplied value). On `Applied`, the value is passed to the retire performed inside the call, and written to the retiring participant's entry in the core's teammate-view projection (the model the C-TeamView push step reads). Assert against that projection, not against a carrier (the plan notes that the carrier arrives with story 038).
- Per reason: `Assassinated` retires the victim's binding and every teammate entry for the victim carries `T_c`; `ReversedAgainst` retires the attacker's binding and the attacker's entry carries `T_c`; `StrikeLimit` releases (the pawn becomes `Unbound`, no tombstone) and the entry carries the supplied exiling slot time; `Disconnect` omits the argument and the entry carries the call's server time from the injected clock (ms). The value is ignored for a transition that retires and releases nothing, and it adds no result code (R8).
- Omitted-argument default: when `Assassinated` or `ReversedAgainst` arrives with the argument omitted, the core uses the call's server time (injected clock). That is the contract defect AC-PSR-57 names; the Roster cannot distinguish an omitted argument from a supplied one at the caller. Open point: the AC's last clause ("flagged by the contract test as a defect") is not an assertion the Roster can make. This story asserts the default (entry carries the injected clock time, not `T_c`); the "flag" itself belongs in the Assassination caller's contract test (Assassination & Defence Reversal epic). The AC text is unchanged.
- The teammate-view projection read accessor is introduced here (name chosen in this story, ServerOnly, no `UObject`) so stories 037, 038 and 041 can map it onto the `FTeamView` carrier without changing semantics. `RetireAtServerTime` is non-zero only in the retiring update; later updates for that participant must not re-send it.
- Do not add any teammate connection field (ADR-0001 forbidden list); the entry holds only `{Handle, State, PawnRef, bIsConcealed, RetireAtServerTime}` plus the 2026-10-25 reconnect fields handled by story 051.
- Test construction: `FRosterCore` with injected fakes from `Private/Roster/StealthRosterDeps.h` (injected ms clock set to a value different from `T_c`, authority true, game thread, seeded random source, fault sink). Two participants on one team pair with a third teammate to confirm "every teammate entry"; live bindings via story 010.
- Prototype disposition: no prototype counterpart.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 014 (Active to Informant applies for each entering reason with the correct pawn disposition): the transition skeleton and pawn disposition per reason.
- Story 020 (Killing an informant retires its binding without a state event): `RetireBinding` for an informant kill.
- Story 037 (A maximal C-TeamView net-serializes in at most 96 bytes): the `FTeamView` struct and `NetSerialize`.
- Story 038 (SetOwnerTally and SetOwnerRespawnDue feed only the owner view with the full precedence): the push step and owner feed.
- Story 041 (A replaced controller or reclaim re-pushes C-TeamView within one server frame): the re-push to a controller.
- Story 051 (A teammate's reconnecting flag and end cause follow the event-only clear rules in all six cases): the `bReconnecting` and `ReconnectEndCause` entry fields.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- `Source/StealthFoundation/Private/Roster/StealthRosterCore.h`
- `Source/StealthFoundation/Private/Roster/StealthRosterCore.cpp`
- `Source/StealthFoundation/Public/View/StealthRosterTypes.h` (only if a client-safe time or entry type is needed; no server type may be declared in `View/`)
- `Source/StealthFoundation/Private/Tests/Roster/StealthRosterRetireAtServerTimeSourceTest.cpp` (class `FRosterRetireAtServerTimeSourceTest`, test name `StealthGame.Roster.RetireAtServerTimeSource`)
- ADR-0007 header naming: `Stealth` prefix and project-unique file names; `Public/View/` headers never include a `Server/` header.

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation. (At tiers where the QL-STORY-READY gate is skipped — `minimal`, or `lean`/`solo` review mode — no qa-lead specs exist, and this section reads: N/A — no qa-lead specs at this tier; implement against the Acceptance Criteria above.)*

*Test cases not yet defined — run /qa-plan to generate them.*

**Flagged gap (QL-STORY-READY: GAPS)**: "a call for Assassinated with the argument omitted is flagged by the contract test as a defect" is not an assertion. The Roster cannot tell an omitted argument from a supplied one; omission is visible only in Assassination's caller. Needs: assert the Roster default (entry carries the injected clock time, not T_c) and move the "flag" to Assassination's AC.

**Provisional spec (qa-lead, pending gap resolution):**

- **AC-1**: RetireAtServerTime reaches teammate entries for each retiring reason.
  - Given: an Active attacker and victim on one team pair with live bindings, injected clock, the teammate-view projection (not the carrier, per story 038).
  - When: RequestTransition(victim, Informant, Assassinated, ..., T_c); the same for ReversedAgainst and StrikeLimit; Disconnect with the argument omitted.
  - Then: Assassinated: binding Retired and every teammate entry carries T_c. ReversedAgainst: the attacker's entry carries T_c. StrikeLimit: entry carries the supplied slot time and the pawn is released, not retired. Disconnect: entry carries the call's clock time.
  - Edge cases: Assassinated with the argument omitted gives the clock time, not T_c.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Roster/StealthRosterRetireAtServerTimeSourceTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later). `qa.level` is `full`, so the test is required.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/participant-state-roster/story-014-transition-active-to-informant.md`
- Cross-epic: none directly; the `StealthFoundation` module skeleton from `production/epics/tuning-data-config/EPIC.md` reaches this story through story 001. The contract-test flag for an omitted `Assassinated` argument belongs to `production/epics/assassination-defence-reversal/EPIC.md`.
- Unlocks: None
