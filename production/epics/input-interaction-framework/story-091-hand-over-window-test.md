# Story 091: Hand-over window test

> **Epic**: Input & Interaction Framework
> **Status**: Blocked — BLOCKED: spike V2 (story 009) has not reported
> **Layer**: Core
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/input-interaction-framework.md`
**Requirement**: `TR-iif-013`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

ADR-derived story (no numbered GDD acceptance criterion): the criterion is the ADR-0011 Validation Criteria "Hand-over window test".

**ADR Governing Implementation**: ADR-0011: Pawn command interface and input pipeline
**Other ADRs**: ADR-0010: Shared Pawn: CMC locomotion, spawn factory and possession parity
**ADR Decision Summary**: ADR-0011 section 2 makes `PossessionEpoch` server-owned and delivered owner-only by `ClientSetPossessionEpoch` before the engine's `ClientRestart`, and makes ingress stamp the controller's recorded `PossessedControlEpoch`, never the pawn's current control epoch, so a command from the old owner that arrives between a hand-over's commit and its possession step carries the old epoch and is voided at its slot. ADR-0010 sections 4.3-4.4 commit a hand-over inside a queue slot (bumping the control epoch at commit) and execute it at the next frame start, waiting for landing if the pawn is in traversal.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Conditioned by verification item **V2** (ADR-0011): `ClientSetPossessionEpoch` always arrives before the `ClientRestart` of the same possession on the owner, across takeover, rejoin, same-pawn re-possession after an `Unresponsive` hand-over and the engine's `ClientRetryClientRestart` and null-pawn resend paths (engine fact E6, `PlayerController.cpp:778-837`). V2 has not reported; the second half of this test (a command after `ClientSetPossessionEpoch` reaches the new pawn) depends on it being true. The traversal wait needs a seam to hold the possession step (ADR-0010 section 4.3).

**Control Manifest Rules (this layer)**:
- Required: `PossessionEpoch` is server-owned and delivered only by `ClientSetPossessionEpoch` before `ClientRestart`; ingress stamps the controller's recorded `PossessedControlEpoch`, never the pawn's current control epoch — source: ADR-0011
- Required: A live pawn's possession change is committed in a queue slot, bumps the control epoch, waits for landing if `IsInTraversal`, and executes in the hand-over pass at the next frame start; the last-command hold covers every frame until the new controller's first command or `HandoverHoldSec` — source: ADR-0010
- Required: A resolution-voided command produces exactly one reply, flushed at the end of the drain's resolution step in one owner-only RPC per connection — source: ADR-0011
- Forbidden: Never special-case `NM_ListenServer` in input ingress — source: ADR-0011

---

## Acceptance Criteria

*From ADR-0011 Validation Criteria ("Hand-over window test"), phrased as one testable criterion:*

- [ ] **GIVEN** a hand-over committed and its possession step not yet executed (including a wait for a traversal to land) **WHEN** the old owner sends a command in that window **THEN** it is voided `StaleControl` at its slot and exactly one `NotNow` reply is sent; **AND GIVEN** `ClientSetPossessionEpoch` has reached the new owner **WHEN** the new owner sends a command **THEN** it reaches the new pawn's mailbox and executes. *(Source: ADR-0011 Validation Criteria, "Hand-over window test".)*

---

## Implementation Notes

*Derived from ADR-0011 Decision sections 2 and 3.2 (step 0) and Implementation Guidelines; ADR-0010 Decision sections 4.3 and 4.4:*

**What this story builds.** A 2-client PIE test of the one-frame (or whole-traversal-wait) window between a hand-over's commit and its possession step. Ingress stamps the controller's recorded `PossessedControlEpoch` (recorded at `OnPossess`), not the pawn's current control epoch (which the commit already bumped). A command from the old owner in the window therefore carries the old epoch and resolves `Voided(StaleControl)` at the stage-2 step 0 check (control epoch of the payload equals the pawn's current one); the void gets exactly one `NotNow` reply (ADR-0011 section 8: a resolution-tier void replies once). After `ClientSetPossessionEpoch` reaches the new owner, its commands are stamped with the new epoch and reach the new pawn's mailbox. This is the end-to-end proof of ADR-0010's epoch design and ADR-0011's ingress rule; stories 025 (the ingress epoch gate) and 071 (context swap on possession) cover the other seams.

**Test shape (qa-lead).** The test name is a proposal, `StealthGame.Input.PIE.HandOverWindow` (the ADR gives no scenario name). Hold the possession step open (a seam in ADR-0010's hand-over pass, including a traversal wait), send a command from the old owner's controller, assert one `StaleControl` void and one `NotNow`; release the step, wait for `ClientSetPossessionEpoch` to reach the new owner, send a command, assert it executes on the new pawn. Edge cases: a window of one frame and a long traversal wait; two commands in the window give two replies (one each).

**Why Blocked.** The unblocking condition is the V2 report (story 009). If V2 fails (the epoch RPC can arrive after `ClientRestart` on some path), the ADR-0011 design needs amendment before this test is meaningful; do not start this story before that report exists.

**Prototype.** New code, no prototype counterpart (the prototype has no epoch or hand-over; `StealthPlayerController` possession is engine default).

**Tuning keys (defaults).** `HandoverHoldSec` (SCL-owned) bounds the last-command hold after the possession step. No IIF key.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/input-interaction-framework/story-025-possession-epoch-gate.md`: the ingress drop of a stale `PossessionEpoch` (unit-level logic).
- `production/epics/input-interaction-framework/story-009-spike-v2-possession-epoch-order.md`: the V2 report on RPC order across takeover, rejoin and retry paths.
- `production/epics/input-interaction-framework/story-071-context-swap-on-state-and-possession.md`: input contexts swap on possession.
- `production/epics/input-interaction-framework/story-094-controller-transport-skeleton.md`: the controller transport this test drives.

---

## Files

- `Source/StealthTestHarness/Private/HandOverWindowTest.cpp` — CQTest `NETWORK_TEST_CLASS(HandOverWindow, "StealthGame.Input.PIE")`, full name `StealthGame.Input.PIE.HandOverWindow.<Method>` (proposed name; planned)
- `Source/StealthGame/` (root module) — possession-step hold seam on the hand-over pass for tests (planned; ADR-0010 section 4.3)

---

## QA Test Cases

**Automated test specs:**

- **AC-1**: A command from the old owner in the hand-over window is voided `StaleControl` with one `NotNow`; a command after the epoch RPC reaches the new pawn (proposed name `StealthGame.Input.PIE.HandOverWindow`).
  - Given: a hand-over committed, the possession step held (including a traversal wait seam), the old owner's controller.
  - When: the old owner sends a command in that window.
  - Then: it is voided `StaleControl` at its slot; exactly one `NotNow` reply.
  - Given: `ClientSetPossessionEpoch` has reached the new owner.
  - When: the new owner sends a command.
  - Then: it reaches the new pawn's mailbox and executes.
  - Edge cases: a window of one frame and of a long traversal wait; two commands in the window give two replies (one each).

*QL-STORY-READY: ADEQUATE (qa-lead); also Blocked on V2 per the plan.*

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/HandOverWindowTest.cpp` (CQTest `StealthGame.Input.PIE.HandOverWindow`) must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/input-interaction-framework/story-025-possession-epoch-gate.md`, `production/epics/input-interaction-framework/story-009-spike-v2-possession-epoch-order.md`; cross-epic: shared-character-locomotion (stories not yet written; hand-over pass and control epoch, ADR-0010 sections 4.3-4.4); cross-epic: round-match-framework (stories not yet written; disconnect/rejoin policy items that commit hand-overs)
- Unlocks: None
