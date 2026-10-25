# Story 013: AC 5 FlushSendsEnd

> **Epic**: Input & Interaction Framework
> **Status**: Blocked — BLOCKED: spike V5 (story 011) has not reported (V1, story 008, reported PASS 2026-10-25: production/qa/spikes/2026-10-25_V1-enhanced-input-edges-report.md)
> **Layer**: Core
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/input-interaction-framework.md`
**Requirement**: `TR-iif-003`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0011: Pawn command interface and input pipeline
**Other ADRs**: ADR-0021: UI/HUD layering, CommonUI and Enhanced Input
**ADR Decision Summary**: `UPawnCommandClient` tracks every outstanding Begin and, on a mapping-context swap, menu open, window focus loss, possession change or `ClientRestart`, sends End for every outstanding Begin before the change is applied and then requires a fresh press; Ends are idempotent, so a later engine `Completed` or `Canceled` for the same press is swallowed. A Common UI switch to `Menu` produces the Ends synchronously.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Highest of ADR-0011 (HIGH) and ADR-0021 (HIGH). Depends on verification items V1 (Enhanced Input edges and the flush yielding exactly one End with no re-fired `Started`, story 008) and V5 (Common UI Menu-mode switch yields a synchronous End before the router's next-tick `FlushPressedKeys`, story 011). Engine facts: E2 CVar `EnhancedInput.IgnoreHeldDigitalActionKeysOnFlush` (default true, marked for removal; second line only), E3 `FModifyContextOptions::bIgnoreAllPressedKeysUntilRelease`, E11 Common UI flushes on the next tick after `Menu`, E12 the ignore flag covers Boolean actions only.

**Control Manifest Rules (this layer)**:
- Required: Ends are idempotent per Begin; a Common UI switch to `Menu` produces the Ends synchronously; `UPawnCommandClient` sends End for every outstanding Begin before any context swap, menu open, focus loss, possession change or `ClientRestart`; context changes set `bIgnoreAllPressedKeysUntilRelease` — source: ADR-0011
- Required: Gameplay input is bound on `AStealthPlayerController`; held commands map `Started` to Begin and both `Completed` and `Canceled` to End — source: ADR-0011
- Forbidden: Never rate-limit `End` per pawn or drop it at the transport bucket when it matches an outstanding Begin — source: ADR-0011
- Forbidden: Never debounce releases client-side — source: ADR-0011
- Guardrail: Engine API constraint (5.8.4): `Completed` does not fire if a trigger reports `Ongoing` the same frame; Common UI flushes pressed keys on the next tick after `Menu` — source: ADR-0011, ADR-0021

This story delivers the release-safety behaviour of the client builder: when the client loses the ability to observe a release (focus lost, menu opened, mapping context swapped, pawn unpossessed), it sends End immediately for each outstanding Begin so the server never extends a hold the owner has abandoned. It also pins the owner's Move and Look to zero for as long as a menu is open, with an uninterrupted command stream, so the pawn brakes to rest instead of drifting or going silent (which would trip input starvation).

It is Blocked because it depends on two unreported verification spikes: V1 (story 008) and V5 (story 011). It also depends on 012 (edge handling), 094 (controller transport and builder shell) and 095 (server command log and outgoing-RPC spy) per the plan's additions. GDD relation: R4 (flush on swap, menu, focus, possession), R13 (contexts, "all held inputs are flushed and require a release"); ADR-0011 section 9 ("The builder owns release safety") and ADR-0021 section 7.

---

## Acceptance Criteria

*From GDD `design/gdd/input-interaction-framework.md`, scoped to this story:*

- [ ] AC 5 (R4, Integration, P `FlushSendsEnd`): **GIVEN** Prepare held **WHEN** (a) the window loses focus, (b) a menu opens, (c) the context swaps, or (d) the pawn is unpossessed **THEN** End is sent within one frame; **AND** the action restarts only on a fresh press; **AND** in case (b) the owner's `Move` and `Look` are zero for as long as the menu is open (no displacement after braking to rest, command stream uninterrupted).

---

## Implementation Notes

*Derived from ADR-0011 Decision §9 and Implementation Guidelines; ADR-0021 Decision §7:*

"The builder owns release safety. `UPawnCommandClient` (a client-only component on the controller) tracks every outstanding `Begin`. On a mapping-context swap, menu open, window focus loss, possession change or `ClientRestart`, it sends `End` for every outstanding `Begin` **before** the change is applied, then requires a fresh press. Ends are **idempotent**: once the builder has sent an End for a Begin, any later engine `Completed` or `Canceled` for that press is swallowed, so a flush never yields two Ends. Context changes use `FModifyContextOptions{bIgnoreAllPressedKeysUntilRelease = true}` (E3). The engine's held-key flush behaviour (E2) is a second line, not the contract, because its CVar is marked for removal." Menu path: `UStealthMenuStack::PushMenu` calls `UPawnCommandClient::EndAllOutstanding(EEndReason::MenuOpen)` synchronously, and the router's `OnActiveInputModeChanged` is bound as the second line (ADR-0021 §7, story 011).

**Blocked on V1 and V5:** this story may not start until the V1 and V5 reports (production/qa/spikes/) state PASS, or an ADR amendment supersedes the assumption. If V1 fails (scenario yields 0 or 2 terminal edges) the idempotent swallow still makes the builder correct, but the AC's "restarts only on a fresh press" relies on the flush behaviour verified by V1.

**Test design (QL-STORY-READY: GAPS; plan status Blocked on V1 and V5, verdict is on the criterion only):** (a) real OS focus loss cannot be injected reliably in PIE: invoke the focus-loss handler directly and mark the OS-level alt-tab check DEFERRED to a manual run (not BLOCKED); "within one frame" is unmeasured: assert a frame-counter delta of at most 1; "no displacement after braking to rest" has no number. Proposed rewrite: "End is sent no later than the frame in which the event is handled (frame-counter delta <= 1). While the menu is open for 3 s, `ServerMove` is sent every frame with zero input vector and unchanged control rotation; speed reaches 0 within the Locomotion baseline stop time and the pawn's position then changes by <= 0.1 cm." Edge cases: Interact and Run held instead of Prepare; two events in one frame yield one End; an End already sent by the builder swallows the engine's later Completed/Canceled (no second End). Needs the server command log seam (QL systemic gap 4, story 095) and the outgoing-RPC spy.

The menu-side "zero Move/Look while the menu is open" relies on the HUD/menu epic's menu stack (cross-epic, stories not yet written) and Locomotion's baseline stop time (shared-character-locomotion). Prototype replaced: `StealthPlayerController.*` input mode handling (the prototype switches input mode ad hoc); new builder code supersedes it. Tuning keys: none (the stop time is Locomotion's `WalkStopSec` / `StopMinSec`).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-012-edges-not-repeat.md`: the Begin/End edge handling this story builds on.
- `story-008-spike-v1-enhanced-input-edges.md` and `story-011-spike-v5-commonui-menu-end.md`: the spikes this story depends on.
- `story-092-common-ui-mid-brace-single-end.md`: the Common UI mid-brace single-End test (ADR Validation Criteria).
- `story-071-context-swap-on-state-and-possession.md`: the Active/Informant context swap logic and its gate.
- `story-044-debounce-merge.md`: the server-side debounce (End then Begin merge).

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- `Source/StealthGame/Private/PawnCommandClient.cpp` (outstanding-Begin tracking, `EndAllOutstanding`, idempotent swallow, router subscription)
- `Source/StealthGame/Public/PawnCommandClient.h`
- `Source/StealthTestHarness/Private/FlushSendsEndTest.cpp` (CQTest `NETWORK_TEST_CLASS(FlushSendsEnd, "StealthGame.Input.PIE")`, full name `StealthGame.Input.PIE.FlushSendsEnd.<Method>`; 2 PIE clients)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

*Test cases not yet defined — run /qa-plan to generate them.*

QL-STORY-READY: GAPS
- "Within one frame" is unmeasured, case (a) real OS focus loss cannot be injected reliably in PIE (mark DEFERRED, not BLOCKED, for the OS-level check), and "no displacement after braking to rest" has no number.
- QA-lead proposed rewrite (unconfirmed; the GDD AC stays the authority): "End is sent no later than the frame in which the event is handled (frame-counter delta <= 1). While the menu is open for 3 s, ServerMove is sent every frame with zero input vector and unchanged control rotation; speed reaches 0 within the Locomotion baseline stop time and the pawn's position then changes by <= 0.1 cm."
- Scenario sketch from the qa file (`FlushSendsEnd`, AC 5, P): 2 PIE clients, owner holding Prepare (brace active); (a) focus-loss handler invoked (OS alt-tab DEFERRED to a manual run), (b) menu pushed, (c) IMC swapped, (d) pawn unpossessed; for each, 1 End on the server command log with frame delta <= 1 and the brace cancels Released; a key still physically held after the event sends nothing and a fresh press sends Begin (new Seq); in (b) with the menu open 3 s with Move held and mouse moved, server-received Move vector is zero every frame, control rotation unchanged, no ServerMove gap, position settled within 0.1 cm. Edge cases: Interact and Run held instead of Prepare; two events in one frame yield one End; builder End swallows the engine's later Completed/Canceled; needs the server command log seam (story 095).

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/FlushSendsEndTest.cpp` — CQTest 2-client PIE test (net-emulation profile not required by the AC; run without emulation, plus once with PktLag 50 ms/direction, 1 % loss), or a documented playtest

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/input-interaction-framework/story-012-edges-not-repeat.md`, `production/epics/input-interaction-framework/story-008-spike-v1-enhanced-input-edges.md`, `production/epics/input-interaction-framework/story-011-spike-v5-commonui-menu-end.md`, `production/epics/input-interaction-framework/story-094-controller-transport-skeleton.md`, `production/epics/input-interaction-framework/story-095-command-test-seams.md`; cross-epic: HUD / UI epic menu stack (stories not yet written); cross-epic: shared-character-locomotion baseline stop time (stories not yet written)
- Unlocks: `production/epics/input-interaction-framework/story-071-context-swap-on-state-and-possession.md`, `production/epics/input-interaction-framework/story-092-common-ui-mid-brace-single-end.md`, `production/epics/input-interaction-framework/story-093-static-scan-and-prototype-retirement.md`
