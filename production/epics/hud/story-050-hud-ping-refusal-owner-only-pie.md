# Story 050: PIE proof that a concealed ping refusal is owner-only

> **Epic**: HUD
> **Status**: Ready
> **Layer**: Presentation
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/hud.md`
**Requirement**: `TR-hud-002`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0008: Test and automation strategy for leak and parity tests (with ADR-0021 for the refusal feedback path)
**ADR Decision Summary**: ADR-0008 fixes the multi-client PIE harness and the rule that a leak or parity test compares the observers' dumps, replicated-property logs and audio logs with a control run. ADR-0021 (Technical Setup 2026-10-25b) makes the concealed-ping refusal owner-only feedback: `FHudInputs.PingConcealedRefusalSeq` is sampled every frame from `FStealthClientViewHandles::PingView`, and a change submits one `NotNow` refusal to the existing `RejectPlateCooldownSec` limiter, which shows the "Not now" plate and plays the owner-only 2D `PingConcealedRefused` cue together or neither.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: A CQTest multi-client PIE test in `StealthTestHarness` (`NETWORK_TEST_CLASS`, test-standards CQTest exemption) on a listen server with the harness from the IVP epic's spike S1 story. The listen host reads the same carrier path as a client (no RepNotify dependence), so the control run and the press run must both use a fixed 60 Hz step. ADR-0021 has no UI-V item on this path; the cue is 2D on the UI bus (never spatialised), so the audio log of an observer is silent by construction and the test proves it. ADR-0018 notes MotionWarping is Beta in 5.8.4 and the networking domain is HIGH risk, which is why only owner-local state is read for the bush predicate. `UE_LOGF` is preferred over `UE_LOG` in 5.8 for the test's own diagnostics.

**Control Manifest Rules (this layer)**:
- Required: `FHudInputs.PingConcealedRefusalSeq` is sampled every frame from `FStealthClientViewHandles::PingView`; `FHudPresenterCore.LastSeenRefusalSeq` is set at `Bind`; a change submits one `NotNow` refusal to the existing `RejectPlateCooldownSec` limiter, which shows the "Not now" plate and plays the owner-only 2D `PingConcealedRefused` cue together or neither; the listen host uses the same path — source: ADR-0021 (Technical Setup 2026-10-25b)
- Forbidden: Presentation reaches ping refusals only through `FStealthClientViewHandles::PingView` (never `UTeamPingService`, never a world or component search for the handle); ring entries are read from the own controller's `UTeamPingViewComponent`; the presenter never reads concealment state — source: ADR-0007, ADR-0021 (Technical Setup 2026-10-25b)
- Guardrail: HUD: ≤ 0.3 ms CPU (AC-HUD-22), ≤ 0.5 ms with markers; UI GPU ≤ 0.5 ms at 1080p on the reference GPU (inside ADR-0022's frame budget) — source: ADR-0021

This is a proof story: the refusal wiring itself is the third AC of story 031 (limiters) and the Informant Pings epic's concealed-press refusal (its story 011). This story shows, end to end in a 2v2 PIE, that the one refused ping that gives feedback gives it to the pressing owner only, and that every other refusal stays silent.

---

## Acceptance Criteria

*From GDD `design/gdd/hud.md` (AC-HUD-31, HUD-R13 ping refusal, ruling D26) and Informant Pings PNG-R2, PNG-R3, scoped to this story:*

- [ ] **AC1** (AC-HUD-31, `StealthGame.HUD.PIE.PingRefusalOwnerOnly`): **GIVEN** a team-0 `Active` player concealed in a bush in a 2v2 PIE, **WHEN** it presses `IA_Ping` three times within 1 s, **THEN** its dumps show the "Not now" plate and one `PingConcealedRefused` cue event at the first press, no second plate or cue within `RejectPlateCooldownSec`, and no `ServerTeamPing` is sent; the team-0 ally's dumps and both team-1 clients' dumps, replicated-property logs and audio logs equal a control run without the presses.
- [ ] **AC2** (derived, HUD-R13 and PNG-R3, `StealthGame.HUD.PIE.PingRefusalOwnerOnly` second method): **GIVEN** the other refusals (no ground in range, empty budget, no body, the strike window) **WHEN** the owner presses `IA_Ping` **THEN** each stays silent with no plate and no cue; after `RejectPlateCooldownSec` the next press while concealed shows one new plate and cue.

---

## Implementation Notes

*Derived from ADR-0008 (harness, control-run rule) and ADR-0021 Technical Setup 2026-10-25b:*

**What to build.** One CQTest file with `NETWORK_TEST_CLASS(PingRefusalOwnerOnly, "StealthGame.HUD.PIE")` (full name `StealthGame.HUD.PIE.PingRefusalOwnerOnly.<Method>`), two methods for the two ACs. The scenario is a listen server with two team-0 and two team-1 clients; the test client of team 0 is placed in a bush (a concealed pawn) through the concealment test hooks of the concealment-bushes epic, then presses `IA_Ping` three times inside 1 s at fixed tick offsets. The body of the test lives here; production code changes are limited to fixing a gap the proof exposes (record any fix against the owning story).

**What the test asserts (the oracle).** For the pressing owner: the view-model dump of the first press shows the rejection plate with the string-table text `Hud.Reject.NotNow` ("Not now") and exactly one cue event `PingConcealedRefused` carrying its text twin, the plate and the cue appear together or not at all, presses two and three inside `RejectPlateCooldownSec` add neither, and the net-driver RPC hook counts zero `ServerTeamPing` calls (nothing is sent). For every observer (the team-0 ally and both team-1 clients): the HUD dump stream, the replicated-property log and the audio log equal a control run with the same seed and fixed 60 Hz step, and no ping ring appears anywhere. The control run is the same scenario with the presses removed ("unchanged" means equal to that control, GDD AC preamble); dumps are keyed by tick index with wall-clock fields excluded.

**Second method.** Drive each silent refusal in turn: a ground point out of range, an empty ping budget (token bucket from the pings epic), a press with no body, and a press inside the strike window (`AttackAttemptLockoutSec` after an own strike). Each must leave plate and cue absent in the owner's dump (these refusals show nothing at all, PNG-R3). Then wait out `RejectPlateCooldownSec` while concealed and press again: exactly one new "Not now" plate and one new cue. Use the limiter's own constant; do not hard-code its value in the test (read it through `GetClientTuning()` or the IIF key).

**Rules and numbers that apply.** HUD-R13: the refusal uses the `NotNow` limiter of the rejection plate (400 ms in, 1.5 s hold, 600 ms out, at most once per `RejectPlateCooldownSec` per code) and is the only refused ping with feedback (ruling D26). `FHudPresenterCore.LastSeenRefusalSeq` is set at `Bind`, so a late bind or rebuild never replays a refusal (HUD-R17). The concealment predicate is evaluated on the owner's client from the owner's own `IsConcealed` flag and bush-action state (Informant Pings story 011 passes `IsOwnPawnConcealedForPing()`); the presenter itself never reads concealment state, so do not add a concealment read to the HUD to make the test pass.

**Traps.** Do not rely on a RepNotify having fired on the host (the host's own writes fire none; sample by value). A control run that is not byte-identical in observer dumps usually means a wall-clock field leaked into the dump, not a real leak: fix the dump, then re-run. The test must run on both the host-as-owner and a remote-client-as-owner to keep host parity (story 046 covers the general parity). Observers' audio logs include the 2D UI bus: assert no `PingConcealedRefused` cue id and no ping tick appears in them.

**Decisions that apply.** D26 (refusal feedback is owner-only and the only one), D19 and D20 are not relevant here. Out of this story: the limiter implementation (031), the concealed-press client refusal and `ConcealedRefusalSeq` (pings epic story 011), the ping send path.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/hud/story-031-hud-notice-slot-priority-and-rate-limits.md`: the `PingConcealedRefusalSeq` consumption, the `NotNow` limiter and `LastSeenRefusalSeq` (Logic).
- `production/epics/hud/story-051-hud-cue-event-stream-and-owner-only-audio-hooks.md`: the cue id and the owner-only 2D hook.
- `production/epics/hud/story-003-hud-input-sampling-and-host-parity-path.md`: the `PingView` sampling path.
- `production/epics/hud/story-046-hud-host-parity-pie.md`: the general host-parity proof.
- Informant Pings epic: `production/epics/informant-pings-team-comms/story-011-concealed-press-refusal.md` (client refusal and the seq counter) and `story-012-strike-window-drop.md`.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthTestHarness/Private/HUD/PingRefusalOwnerOnlyTest.cpp` (CQTest, `StealthGame.HUD.PIE.PingRefusalOwnerOnly.*`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: AC-HUD-31 `PingRefusalOwnerOnly`
  - Given: a team-0 Active player concealed in a bush in 2v2 PIE.
  - When: it presses `IA_Ping` three times in 1 s.
  - Then: its dump shows the "Not now" plate and one `PingConcealedRefused` cue at the first press, no second plate or cue within `RejectPlateCooldownSec`, and no `ServerTeamPing` sent. The team-0 ally's and both team-1 clients' dumps, replicated-property logs and audio logs equal a control run.
  - Edge cases: none.
- **AC-2**: other refusals silent
  - Given: refusals for no ground in range, empty budget, no body and the strike window.
  - When: pressed.
  - Then: no plate and no cue. After `RejectPlateCooldownSec` the next concealed press shows one new plate and cue.
  - Edge cases: none.

QL-STORY-READY: ADEQUATE

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: the CQTest PIE test `Source/StealthTestHarness/Private/HUD/PingRefusalOwnerOnlyTest.cpp` (`StealthGame.HUD.PIE.PingRefusalOwnerOnly.*`)
- Gate: BLOCKING

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/hud/story-031-hud-notice-slot-priority-and-rate-limits.md`, `production/epics/hud/story-003-hud-input-sampling-and-host-parity-path.md`; Informant Pings epic `production/epics/informant-pings-team-comms/story-011-concealed-press-refusal.md` (PNG-R2, PNG-R3, AC-PNG-17) and `story-010-ia-ping-action-and-send-path.md`
- Unlocks: None within this epic (see the EPIC Definition of Done)
