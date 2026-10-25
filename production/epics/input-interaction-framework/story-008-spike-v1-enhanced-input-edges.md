# Story 008: Spike V1 reports Enhanced Input edge behaviour on this project's contexts

> **Epic**: Input & Interaction Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/input-interaction-framework.md`
**Requirement**: `TR-iif-003`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0011: Pawn command interface and input pipeline
**ADR Decision Summary**: Discrete commands are Enhanced Input edges: `Started` maps to Begin and both `Completed` and `Canceled` map to End, the client builder owns release safety by sending an End for every outstanding Begin before any context swap, menu open, focus loss, possession change or `ClientRestart`, and Ends are idempotent per Begin. Verification item V1 confirms the engine edge behaviour this relies on.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: This story IS verification item V1 of ADR-0011: "Enhanced Input edge behaviour on this project's contexts: `Started` on press and exactly one `Completed` or `Canceled` per release, on context add/remove, focus loss and menu open, with the client's outstanding-Begin flush (R4) producing exactly one End per outstanding Begin and no re-fire of `Started` after a flush (E2, E3)." Engine facts under test: E1 (`ETriggerEvent` semantics; `Completed` does not fire if any trigger reports Ongoing the same frame), E2 (CVar `EnhancedInput.IgnoreHeldDigitalActionKeysOnFlush`, default true, "will be removed in a future release"), E3 (`FModifyContextOptions::bIgnoreAllPressedKeysUntilRelease`, default true), E12 (covers Boolean actions only; a release already queued that tick is not ignored). Facts are read from 5.8.4 source; the exactly-one-terminal-edge claim on flush is unconfirmed until this report.

**Control Manifest Rules (this layer)**:
- Required: Gameplay input is bound on `AStealthPlayerController`; held commands map `Started` to Begin and both `Completed` and `Canceled` to End; server-meaningful actions are Boolean — source: ADR-0011
- Required: Ends are idempotent per Begin; `UPawnCommandClient` sends End for every outstanding Begin before any context swap, menu open, focus loss, possession change or `ClientRestart`; context changes set `bIgnoreAllPressedKeysUntilRelease` — source: ADR-0011
- Forbidden: Never use Hold/HoldAndRelease/Tap/Pulse/Chorded triggers for server-meaningful actions — source: ADR-0011
- Guardrail: Engine API constraint (5.8.4): the flush CVar is `EnhancedInput.IgnoreHeldDigitalActionKeysOnFlush` (default true, marked for removal); `bIgnoreAllPressedKeysUntilRelease` covers Boolean actions only; `Completed` does not fire if a trigger reports `Ongoing` the same frame — source: ADR-0011, ADR-0021

This is a verification spike, not feature code. It produces the V1 report that decides whether stories 013 (FlushSendsEnd) and 071 (ContextSwapOnStateAndPossession) may proceed: both are Blocked ("spike V1 (story 008) has not reported") until this reports PASS or an ADR amendment lands. The spike exercises this project's own contexts (`IMC_Common`, `IMC_Active` from story 007) against six scenarios, counting `Started`, `Completed` and `Canceled` for one held Boolean action.

It sits at the start of the risk-first order because a failed V1 is handled by a superseding ADR, not by editing ADR-0011 ("a failed spike is handled by a superseding ADR"). Even on PASS the idempotent-End swallow stays mandatory (ADR §9). GDD relation: R4 (edges, flush on swap), R13 (contexts, "its exactly-one-`Completed` behaviour on flush is spike V1").

---

## Acceptance Criteria

*ADR-derived spike. Source: ADR-0011 Engine Compatibility, Verification Required item V1; criterion text from the plan:*

- [ ] **GIVEN** this project's `IMC_Common` and `IMC_Active` contexts and one held Boolean action **WHEN** the six V1 scenarios (plain press and release, context add while held, context remove while held with and without `bIgnoreAllPressedKeysUntilRelease`, window focus loss, menu open, the builder's outstanding-Begin flush) are each run N times **THEN** a report at `production/qa/spikes/` records engine build, contexts, CVar value and per-scenario `Started`/`Completed`/`Canceled` counts, shows `Started` on press and exactly one terminal edge (`Completed` or `Canceled`) per release in every scenario, and shows the builder's flush yielding exactly one End and no re-fired `Started`, stating PASS or FAIL per scenario and overall.

---

## Implementation Notes

*Derived from ADR-0011 Engine Compatibility V1, Decision §9 and Implementation Guidelines:*

Why the builder does not trust the engine: "The builder owns release safety ... On a mapping-context swap, menu open, window focus loss, possession change or `ClientRestart`, it sends `End` for every outstanding `Begin` **before** the change is applied, then requires a fresh press. Ends are **idempotent**: once the builder has sent an End for a Begin, any later engine `Completed` or `Canceled` for that press is swallowed, so a flush never yields two Ends. ... The engine's held-key flush behaviour (E2) is a second line, not the contract, because its CVar is marked for removal." The spike measures whether the engine behaves well enough that the swallow rarely triggers, and records any scenario where it yields 0 or 2 terminal edges as a FAIL finding for that scenario.

**Spike procedure (from the qa-lead spec, N author to confirm):** record engine build (5.8.4 CL 58902052), the contexts under test (`IMC_Common`, `IMC_Active` from story 007), the value of CVar `EnhancedInput.IgnoreHeldDigitalActionKeysOnFlush` used, and per-scenario counts for one held Boolean action over N repetitions (propose N = 20). Scenarios: (1) plain press and release; (2) context add while held; (3) context remove while held, with and without `FModifyContextOptions::bIgnoreAllPressedKeysUntilRelease`; (4) window focus loss, DEFERRED to an interactive standalone run because PIE focus is not representative (mark DEFERRED, not BLOCKED); (5) menu open; (6) the builder's outstanding-Begin flush. Also cover both keyboard and gamepad key for one action, and a key held across a context swap that removes and re-adds the same mapping. The production builder (`UPawnCommandClient`) is delivered by story 094 and 012; this spike uses a minimal throwaway probe that mirrors the ADR §9 flush so it can run before them, and says so in the report. Spikes and tests must not resave sample assets; read `production/never-resave-assets.md` before any Content step.

Pass rule: every scenario shows `Started` on press and exactly one terminal edge per release across N runs, and scenario (6) yields exactly one End and no re-fired `Started`. Any 0 or 2 terminal edges is a FAIL finding for that scenario. The report states PASS or FAIL per scenario and overall; stories 013 and 071 stay Blocked until PASS or an ADR amendment.

Prototype: new code, no prototype counterpart. Tuning keys: none.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-007-input-assets-and-trigger-lint.md`: authoring the contexts and the lint (prerequisite of this spike).
- `story-011-spike-v5-commonui-menu-end.md`: V5, the Common UI Menu-mode synchronous End.
- `story-012-edges-not-repeat.md`: the production edge handling (AC 4).
- `story-013-flush-sends-end.md`: the production flush behaviour (AC 5), Blocked on this spike.
- `story-071-context-swap-on-state-and-possession.md`: the production context swap, Blocked on this spike and V2.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- `production/qa/spikes/V1-enhanced-input-edges-report.md` (the report; final dated name chosen at write time)
- `Source/StealthTestHarness/Private/SpikeV1EnhancedInputEdgesTest.cpp` (throwaway probe harness; not a gate test)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

**Integration — spike specification** (QL-STORY-READY: ADEQUATE):

- **AC-1**: Spike V1 reports Enhanced Input edge behaviour on this project's contexts
  - Given: `IMC_Common` and `IMC_Active` (story 007), one held Boolean action, N = 20 runs per scenario (N author to confirm). Report must record: engine build (5.8.4 CL 58902052), contexts under test, CVar `EnhancedInput.IgnoreHeldDigitalActionKeysOnFlush` value used, per-scenario counts of Started/Completed/Canceled.
  - When: scenarios (1) plain press/release; (2) context add while held; (3) context remove while held, with and without `FModifyContextOptions::bIgnoreAllPressedKeysUntilRelease`; (4) window focus loss (DEFERRED to an interactive standalone run); (5) menu open; (6) the builder's outstanding-Begin flush.
  - Then: every scenario shows Started on press and exactly one terminal edge (Completed or Canceled) per release across N runs, and (6) yields exactly one End and no re-fired Started. Any 0 or 2 terminal edges is recorded as a FAIL finding for that scenario (the idempotent End swallow stays mandatory regardless, ADR 9). The report states PASS / FAIL per scenario and overall; stories 013 and 071 stay Blocked until PASS or an ADR amendment.
  - Edge cases: both keyboard and gamepad key for one action; key held across a context swap that removes and re-adds the same mapping.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: the spike report `production/qa/spikes/V1-enhanced-input-edges-report.md` (a documented result, retained) OR the probe harness test `Source/StealthTestHarness/Private/SpikeV1EnhancedInputEdgesTest.cpp`

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/input-interaction-framework/story-007-input-assets-and-trigger-lint.md`
- Unlocks: `production/epics/input-interaction-framework/story-013-flush-sends-end.md`, `production/epics/input-interaction-framework/story-071-context-swap-on-state-and-possession.md`
