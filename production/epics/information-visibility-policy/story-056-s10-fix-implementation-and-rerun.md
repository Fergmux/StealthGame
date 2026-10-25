# Story 056: S10 fix, re-open guard, lost-pawn repair and warm-up implemented in the cull rig and S10 re-run to PASS

> **Epic**: Information Visibility Policy
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25g
> **Last Updated**: 2026-10-25

## Context

Spike S10 (story-005) reported FAIL: a cull followed by an un-cull before the client processes the close loses the pawn on ~1-2 % of such re-opens (the open re-binds the live actor, the late close destroys it); every re-created pawn renders one collapsed, un-evaluated frame; and pose and death times lag ~150 ms because the stub read the engine's `GetServerWorldTimeSeconds`. ADR-0003's amendment "S10 resolution 2026-10-25" decides the fix: a per-connection re-open guard that waits for the in-sequence acknowledgement of the previous close, bounded by `ReopenAckTimeoutMs` (350 ms) with a forced fresh channel; a client-reported lost-pawn repair (`NMT_ActorChannelFailure` → `APlayerController::NotifyActorChannelFailure` → fresh channel); a hidden-until-first-pose warm-up on every client pawn actor; and presentation times from ADR-0002 §9's estimator under its unchanged budget (p95 ≤ 15 ms, p99 ≤ 30 ms), which replaces S10's single 16.7 ms bound. Every part is identical for every pawn kind (Pillar 1). This story implements the fix in the S9/S10 cull rig and re-runs S10 until it passes; it is the gate that replaces S10 for every story that depends on ADR-0003 §5.6-§5.8.

**GDD**: `design/gdd/information-visibility-policy.md`
**Requirement**: `TR-ivp-004` (Concealment hybrid: quantised server cull for non-allies + client presentation hide)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0003: Replication, relevancy and the concealment cull hybrid (sections 5.6-5.8 and the amendment "S10 resolution 2026-10-25", S10-R1 to S10-R12); ADR-0002 §9 (estimator and accuracy budget; amendment "S10 resolution 2026-10-25")
**ADR Decision Summary**: A pawn is never re-opened to a connection while that connection's previous close for it is unacknowledged in sequence, except after `ReopenAckTimeoutMs`, when it re-opens on a fresh channel and any resulting lost pawn is repaired on the client's report; every client pawn actor stays hidden until a bone-transform finalisation after its presentation apply; presentation times come only from `EstimatedServerNow()`; the whole path is one function for every pawn kind.
**ADR Version**: 2026-10-25 (S10 resolution)

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: E10 (rev.) in-sequence close acknowledgement = server channel cleanup [V]; E11 (rev.) `AlwaysTickPose` does not refresh bones while not rendered, `RegisterOnBoneTransformsFinalizedDelegate` [V]; E18 `OnActorChannelOpen` on re-bind, `NMT_ActorChannelFailure` and `APlayerController::NotifyActorChannelFailure` [V]. Read the amendment's gate record for the engine-specialist notes before coding.

**Control Manifest Rules (this layer)**:
- Required: **A pawn is re-opened to a connection only after that connection's previous close for it is acknowledged in sequence, or after `ReopenAckTimeoutMs` on a fresh channel; the hold keys on the connection and channel only and adds no forced update** — source: ADR-0003 (S10 resolution)
- Required: **Every client pawn actor starts warm-up hidden and becomes visible only after a bone-transform finalisation that follows its presentation apply with the clock synced** — source: ADR-0003 (S10 resolution)
- Required: **Client presentation schedules only from `EstimatedServerNow()`** — source: ADR-0002
- Forbidden: **Never call `ForceNetRelevant()` or `ForceActorRelevantNextUpdate()` on a pawn** — source: ADR-0003

---

## Acceptance Criteria

*From ADR-0003 amendment "S10 resolution 2026-10-25", S10-R7 (the re-run gate):*

- [ ] GIVEN the S10 rig with the fix implemented (re-open guard, timeout and fresh channel, lost-pawn repair, warm-up, a harness estimator implementing ADR-0002 §9), every (pawn, connection) pair observed until its unverified flag clears, WHEN it runs under StealthLeak1 and StealthLeak2 with 3 clients and ≥ 600 cycles per profile, with player-possessed and AI-possessed subjects cycling through the same rule (≥ 300 cycles per kind per profile, equal sample sizes; cases A same-step un-cull, C un-cull 2 server frames after the close, B kill + death-write re-open), THEN all of ADR-0003 S10-R7's PASS conditions hold and the verdict PASS is written to `production/qa/spikes/`: (1) 0 lost pawns, 0 crashes, 0 ensures, `UnexpectedChannelFailure` = 0, and every `failed to find/create ACTOR` warning or engine report log line is followed by a repair of that pawn within `L_gap`; (2) bounds: free re-opens with first-send delivery `G` p99 ≤ 250 ms, all free re-opens `G` p99 ≤ 520 ms (resend fraction reported); held re-opens: hold ≤ `ReopenAckTimeoutMs` + 1 server frame and, with first-send delivery, `G` p99 ≤ 650 ms; `G` max ≤ 1.0 s over cycles with at most one resend per hop (burst-loss cycles reported separately); `L_gap` max ≤ 1.25 s; `T_reopen` (open-bunch receipt → first visible posed frame) p99 ≤ 49 ms; (3) 0 un-posed first visible frames by a probe independent of the warm-up signal (head-bone height from the component-space transforms submitted for that frame and silhouette height on the render client, both ≥ 80 % of the standing reference), `WarmupTimeout` = 0, and a retained, lead-signed frame strip; (4) every re-open including repairs reuses the NetGUID (0 allocations); (5) parity: hold time, timeout, repair, `G` and warm-up frames as IVP F2 classifier features keep `AUC_hat ≤ 0.55` (upper bound ≤ 0.58), per-feature KS reported as descriptive only; (6) controls: guard off reproduces S10's lost pawns, warm-up off makes the probe in (3) detect the collapsed frame, and a fault-injection run (`ReopenAckTimeoutMs` = 200 plus an injected close delay) drives ≥ 100 timeout-and-repair cycles and still meets (1) and the `L_gap` bound; (7) clock: pose-time and death-elapsed error at the evaluation instant (time used by the evaluation − true server time at that instant) p95 ≤ 15 ms, p99 ≤ 30 ms after sync; (8) the hold is decomposed into close-sent → in-sequence acknowledgement, acknowledgement → open, and any `KeepProcessingActorChannelBunchesMap` delay; a timeout rate above 5 % of held re-opens is a CONCERN with its root cause, not a FAIL, when (1)-(7) pass.

---

## Implementation Notes

*Derived from ADR-0003 amendment S10-R2 to S10-R9 and ADR-0002 §9:*

- **Where.** Throwaway spike code like story-005: the rig `Source/StealthTestHarness/Private/Scenarios/StealthHarnessScenarioS10.cpp`, the presentation stub `Source/StealthTestHarness/Private/Spikes/Visibility/StealthSpikeActionStateComponent.{h,cpp}`, and the spike relevancy hook behind `!UE_BUILD_SHIPPING`. Use `AStealthPawn` (SCL-001) as the subject class if the rig can spawn it; otherwise the prototype pawn, and record which. The production `UVisibilityPolicy` implementation is stories 023-025's, which embed the amended rules; this story proves the mechanism.
- **Guard (S10-R2).** A per-(pawn, connection) pending-close entry at every rig close; the relevancy hook returns false while it exists; checked every server frame in the cull tick (`TG_PostUpdateWork`) through the closed channel's slot (`Connection->Channels[ChIndex] != ClosedChannel`, or same object not `Closing`); no forced update at release.
- **Timeout (S10-R3).** `ReopenAckTimeoutMs` = 350 as a harness setting (the tuning key does not exist yet); at timeout mark the entry stale, flag the pair unverified, keep watching the stale entry.
- **Repair (S10-R4).** Client: override `OnActorChannelOpen` on the subject class (count, last `ChIndex`, NetGUID); on `EndPlay(Destroyed)` with count ≥ 2 only queue a probe on the local PlayerController; on the next client tick report `NMT_ActorChannelFailure(LastOpenChIndex)` only if that channel is still open, not `Closing`, for the stored NetGUID and the server connection is open (guard every pointer). Server: `NotifyActorChannelFailure` on the rig's PlayerController only queues `{pawn, ChIndex, channel}`; the cull tick re-checks the slot (`Channels[ChIndex] == Chan && Chan->Actor == Pawn && !Chan->Closing`), relevance and the unverified flag, then closes and re-guards; valid reports above 4 per second per connection are deferred (FIFO, one per pawn), never dropped; invalid ones are counted as `UnexpectedChannelFailure`. Make a guard entry only if `Closing` is true right after `Close()` (else count `CloseNotSent`).
- **Warm-up (S10-R5).** Hidden from construction; in `PostNetInit` apply presentation, switch the mesh to `AlwaysTickPoseAndRefreshBones` at rate 1, show on the first `OnBoneTransformsFinalized` after the apply with the estimator synced; failsafe 3 frames; return to `AlwaysTickPose` after.
- **Clock (S10-R6).** A harness estimator implementing ADR-0002 §9 exactly (four-timestamp owner exchange, 16-sample min-delay filter, slew ≤ 50 ms/s, synced after 5 samples); presentation reads only it. The estimator's own error samples are recorded in the same run (input to the round-match-framework OQ11(b) clock-sync spike; this story does not close that spike).
- **Probes.** First-visible-frame probe independent of the warm-up signal (head-bone height from the submitted component-space transforms; silhouette height on the render client; ≥ 80 % of the standing reference); per re-open timestamps: un-cull, close sent, in-sequence ack, open sent, open receipt (first send or resend), actor spawn, pose evaluation (with the estimator time used), first visible frame; per network destroy and repair: destroy, probe, report receipt, repair close, fresh open, first visible frame (`L_gap`); per-subject kind label for the F2 parity features.
- Re-run with the original launch lines of story-005 plus the new flags; keep the previous runs' evidence untouched.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- story-023/024/025: the production `UVisibilityPolicy` cull record, override and cull tick (they embed the amended rules)
- story-027/044 and Concealment story-020: the production re-proof with the production AnimBP and the client registries (significance, audio, decals, markers)
- story-029: the production un-cull lead bound (AC-IVP-24)
- Round-match-framework OQ11(b) clock-sync spike and story 091: the estimator's own acceptance
- GDD wording changes requested by the amendment (IVP R7.5, F4, Tuning Knobs, leak catalogue; Concealment)

---

## QA Test Cases

- **AC-1**: S10 re-run with the fix
  - Given: the S10 rig with the guard, timeout, repair, warm-up and harness estimator; per-pair observation until the unverified flag clears; NPC and player-possessed subjects (≥ 300 cycles per kind per profile).
  - When: StealthLeak1 and StealthLeak2 runs, 3 clients, ≥ 600 cycles each, cases A, B and C.
  - Then: criteria (1)-(7) of the acceptance criterion hold; (8) recorded.
  - Edge cases: the controls in (6) are part of the run, not optional: guard off must reproduce S10's lost pawns (proves the loss probe), warm-up off must show the collapsed frame to the independent probe, and the fault-injection run must exercise ≥ 100 timeout-and-repair cycles. If the `OnBoneTransformsFinalized` callback does not fire for a hidden mesh under external tick control (E11 [U]), record it and use an explicit `TickAnimation` + `RefreshBoneTransforms` in warm-up mode instead, then re-run. (Tier: T3; visual evidence retained.)

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: a written spike report `production/qa/spikes/<date>_S10-rerun.md` with the verdict, plus analysis JSON and launch lines under `production/qa/evidence/story-056-s10-fix-implementation-and-rerun/`.
- Visual: a retained, lead-signed first-visible-frame strip in `production/qa/evidence/` (no collapsed frame).

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/information-visibility-policy/story-005-spike-s10-reopen-behaviour-and-netguid.md` (Complete; the rig and the FAIL baseline)
- Unlocks: `production/epics/information-visibility-policy/story-024-concealment-cull-audience.md`, `production/epics/information-visibility-policy/story-027-concealment-local-hide-at-swap.md`, `production/epics/information-visibility-policy/story-028-blind-spot-attack-on-culled.md`, `production/epics/information-visibility-policy/story-029-uncull-lead-bound.md`, `production/epics/information-visibility-policy/story-044-raw-bunch-schema-parity.md`, `production/epics/concealment-bushes/story-020-fresh-actor-latch-and-ally-hidden-read.md`, `production/epics/concealment-bushes/story-025-cull-push-wiring.md`, `production/epics/concealment-bushes/story-026-per-viewer-visibility-pie.md`, `production/epics/concealment-bushes/story-027-uncull-before-leaf-plane.md`, `production/epics/concealment-bushes/story-028-enter-swap-hides-body-per-viewer.md`, `production/epics/concealment-bushes/story-032-ownership-change-while-concealed.md`, `production/epics/concealment-bushes/story-033-blind-strike-parity-and-kill-timing.md`, `production/epics/assassination-defence-reversal/story-022-culled-victim-opens-falling-and-late-actors.md`, `production/epics/assassination-defence-reversal/story-023-concealed-body-stays-hidden.md`, `production/epics/teammate-markers/story-029-feed-handoff-and-null-ref-on-real-connection.md`, `production/epics/teammate-markers/story-030-informant-return-timing.md` (each for its S10 gate only; their other gates stay)
- If the re-run FAILs: the amendment's escalation (client net-driver alternative, S10-R10) is decided by a new `/architecture-decision`; dependents stay Blocked on this story.
