# Story 036: The dev-only transition log keeps the last N entries and is absent from Shipping

> **Epic**: Participant State & Roster
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/participant-state-roster.md`
**Requirement**: `TR-roster-003`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0002: Server resolution queue, server clock and round-end staging contract
**Also governed by**: ADR-0007: Code module layout, composition root and include boundaries
**ADR Decision Summary**: ADR-0002 fixes one server-side total order for every interacting state change, with the Roster flush and its events inside the drain, and keeps queue and Event Log content out of every logging path. ADR-0007 puts the Roster in a plain-C++ core with no `UWorld` dependency and forbids Event Log content, server-only state and identifiers from reaching `UE_LOGF`, ensure text or traces. A dev-only ring buffer of applied transitions is a ServerOnly debugging store that must follow both rules.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: ADR-0002 uses no post-cutoff API (`UE_LOGF`, 5.8, is preferred for diagnostics but the transition log never passes through logging). ADR-0007 notes the 5.8 C++20 floor and `UE_LOGF`. Verification items (drain-spawn spike, estimator net emulation, drain once per frame on the listen host) do not concern this story.

**Control Manifest Rules (this layer)**:
- Required: Queue, Roster, tuning validator, seed mixer and appearance deck are plain C++ cores (`F…Core`) with no `UWorld` dependency, behind a subsystem facade — source: ADR-0007
- Required: Hot-reload, console-command and debug code sits inside `WITH_EDITOR` / `!UE_BUILD_SHIPPING` blocks; debug commands are `#if !UE_BUILD_SHIPPING` — source: ADR-0004, ADR-0007
- Required: Every gameplay number is a field of a `_Public` or `_Server` tuning sub-struct; server code reads only `GetServerTuning()`/`FValidatedTuning` — source: ADR-0004
- Forbidden: Never pass Event Log content, `DealDigest`, server sub-seeds, bindings or mailbox order to `UE_LOG`/`UE_LOGF`/`ensureMsgf`/`checkf`/`ClientMessage`/`TRACE_*`/`CSV_*`/`SCOPE_CYCLE_COUNTER` names — source: ADR-0007
- Forbidden: Never compile negative-control types or test seams into Shipping (`StealthTestHarness` or `WITH_STEALTH_TEST_SEAMS` only) — source: ADR-0008
- Guardrail: Queue drain + staging + teardown + Roster flush: at most 0.5 ms listen-host game thread (appending an entry must be O(1)) — source: ADR-0002

This story delivers the dev-only ring buffer of applied transitions kept inside the Roster core for debugging and test assertions, sized by the `RosterTransitionLogDepth` knob. It exists so a failed round can be diagnosed from the last N transitions without any logging path and so tests can assert transition order. It realises the GDD Tuning Knobs row for `RosterTransitionLogDepth` (default 64, hard range 16-1024, ServerOnly, about 24 B per entry) and the ServerOnly log mentioned in R9.

---

## Acceptance Criteria

*From GDD `design/gdd/participant-state-roster.md`, scoped to this story:*

- [ ] **AC-PSR-56** [Core] `StealthGame.Roster.TransitionLogRing` (dev builds only): GIVEN `RosterTransitionLogDepth` = 16 and 20 applied transitions, WHEN the log is read, THEN it holds the last 16 in order, oldest dropped first, and a value outside 16-1024 is clamped; the log is absent from a Shipping configuration (compile-time check) and never replicated (Tuning Knobs).

---

## Implementation Notes

*Derived from ADR-0002, ADR-0007 sections 5-6 and GDD Tuning Knobs, R9:*

- **Ring.** A fixed-capacity ring buffer member of `FRosterCore`, written once per applied single-participant transition (inside the same call that writes `LastEventSeq`), append is O(1) and allocation-free after construction. Reading returns entries oldest first. Capacity is the injected depth.
- **Depth injection and clamping.** The core is plain C++ and takes the depth as a constructor argument; the `URosterStore` facade reads `RosterTransitionLogDepth` through `GetServerTuning()` (ADR-0004) and passes it in. The tuning pipeline's F-1 step clamps to the key's hard range (16-1024); the core also clamps defensively so that raw input below 16 gives 16 and above 1024 gives 1024, which is what the AC asserts. The test passes the depth directly (16), so it does not need the tuning key to exist yet.
- **Dev-only.** Guard the member, its writes and its read accessor with `#if !UE_BUILD_SHIPPING` (define a project macro such as `WITH_STEALTH_ROSTER_TRANSITION_LOG` from it in one place). The compile-time check for "absent from a Shipping configuration" is a build-level check, not an editor automation assertion: see the Open point.
- **Never replicated, never logged.** The ring is a plain C++ member of the core, with no `UPROPERTY`, `USTRUCT` or `UCLASS`, and the facade subsystem is not a replicated object. Do not route entries through `UE_LOG`/`UE_LOGF`, `ensureMsgf` text or traces (ADR-0007 section 6; Event Log content rule).
- **Entry content.** The GDD does not define an entry; the knob row says about 24 B per entry. The minimum that makes the log useful for the AC and for debugging is the transition itself, drawn from the `OnParticipantStateChanged` payload fields (`EventSeq`, participant id, old state, new state, reason, `RoundEpoch`). Keep it a trivially copyable struct so the byte estimate holds.
- **Open point:** the entry layout is not defined by the GDD (qa-lead gap); the fields above are a proposal to confirm with the design owner.
- **Open point:** "absent from a Shipping configuration (compile-time check)" and "never replicated" have no mechanism in the AC. Proposed: a Shipping-configuration compile of `StealthFoundation` in the gate script that fails if the macro is defined, plus a reflection assertion that no replicated property exists on the facade. Decide with the qa-lead; this story's automated test covers the ring behaviour and the clamp.
- **Open point:** the 20 transitions in the AC span two rounds with `ResetAll` between (qa-lead spec); confirm that a `ResetAll` does not clear the ring (it is a debug history across rounds), otherwise the "last 16" would shrink.
- **Prototype disposition:** no prototype counterpart.
- **Header naming (ADR-0007).** Stealth prefix, project-unique names: `StealthRosterCore.h`, `StealthRosterService.h`.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 014 (Active to Informant applies for each entering reason with the correct pawn disposition): owns `RequestTransition` and the apply path that this story hooks.
- Story 020 (Killing an informant retires its binding without a state event): R9 writes the `InformantKilled` reason to the ServerOnly log without a state event; whether that case also enters this ring is not required by AC-PSR-56 and is left to story 020.
- Story 008 (Registration, team, removal and reclaim each fire exactly one sequenced event): owns the event queue; the ring records transitions, not events.
- Story 027 (The transition matrix equals the checked-in CSV fixture for every combination): may use the ring to read results but owns the matrix.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- `Source/StealthFoundation/Private/Roster/StealthRosterCore.h` (ring member, depth constructor argument, read accessor, all under `!UE_BUILD_SHIPPING`)
- `Source/StealthFoundation/Private/Roster/StealthRosterCore.cpp`
- `Source/StealthFoundation/Private/Roster/StealthRosterStore.cpp` (facade passes `RosterTransitionLogDepth` from `GetServerTuning()`)
- `Source/StealthFoundation/Private/Tests/Roster/StealthRosterTransitionLogRingTest.cpp` (class `FRosterTransitionLogRingTest`, test name `StealthGame.Roster.TransitionLogRing`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation. (At tiers where the QL-STORY-READY gate is skipped — `minimal`, or `lean`/`solo` review mode — no qa-lead specs exist, and this section reads: N/A — no qa-lead specs at this tier; implement against the Acceptance Criteria above.)*

*Test cases not yet defined — run /qa-plan to generate them.*

**Flagged gap (QL-STORY-READY: GAPS):** a log entry's content is not defined. "Absent from a Shipping configuration (compile-time check)" is a build check, not an editor automation test. "Never replicated" has no mechanism. Needs a Shipping-configuration compile step and a reflection check that the log has no replicated property.

**Provisional spec (qa-lead, pending gap resolution):**

- **AC-1**: the dev-only transition log keeps the last N entries.
  - Given: RosterTransitionLogDepth = 16, 20 applied transitions (two rounds, ResetAll between).
  - When: the log is read.
  - Then: it holds the last 16 in order, oldest dropped first. Out-of-range depths (below 16, above 1024) clamp to the nearest bound.
  - Edge cases: the Shipping and replication clauses depend on the mechanism chosen.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Roster/StealthRosterTransitionLogRingTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later). `qa.level` is full, so the test is required.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/participant-state-roster/story-014-transition-active-to-informant.md`
- Depends on (cross-epic): `production/epics/tuning-data-config/EPIC.md` (the `RosterTransitionLogDepth` key, hard range 16-1024, is a Tuning Data & Config row; until it exists the core takes the depth as a plain argument)
- Unlocks: None
