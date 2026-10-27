# Story 034: Pipeline parity: AIController versus bot-client input path, nightly

> **Epic**: Crowd & NPC Behaviour
> **Status**: Blocked — BLOCKED (2026-10-27): AC-CNB-27 needs a nightly multi-process run of at least 1,236 30 s windows per pipeline (about 10.3 h each, >= 10 bot clients) on the self-hosted CI runner, which is not registered (a human item), and the bot-client Enhanced Input replay of FCrowdVirtualInputFrame streams through the ADR-0008 T3 harness is not built; the recording side exists (story 010, CrowdVirtualInput Recording spec)
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: 2026-10-27

## Context

**GDD**: `design/gdd/crowd-npc-behaviour.md`
**Requirement**: `TR-crowd-011`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-crowd-011 = Follower drives Move/Look through a virtual input layer with device profiles; a recordable input stream is replayable through a bot client for parity tests (CNB-R3.4, F6, AC-CNB-27/28).)*

**ADR Governing Implementation**: ADR-0016: Crowd AI architecture
**Also relevant**: ADR-0008 (multi-process harness T3, nightly runs, bot clients), ADR-0011 (Enhanced Input path)
**ADR Decision Summary**: The virtual input layer's per-step output `FCrowdVirtualInputFrame` can be recorded (non-Shipping) and replayed through a bot client's Enhanced Input path by the ADR-0008 harness; that is how AC-CNB-27 isolates pipeline differences between an AIController-driven pawn and a client-driven one.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: Multi-process nightly run with at least 10 bot clients (about 10.3 h per pipeline for 1,236+ 30 s windows each); features are computed on an observer client from replicated pawn state, not from server internals. Enhanced Input flush CVar name is `EnhancedInput.IgnoreHeldDigitalActionKeysOnFlush` in 5.8.4 (not the stale engine-reference name).

**Why this story**: If an AI-driven pawn and a human-driven pawn given the same inputs differ in any observable motion feature, the pipeline itself is a tell no profile can fix. This is the parity gate that proves the input-emulation decision (ADR-0016 section 4). It also supplies the per-step result story 033 consumes. Read GDD F6 and AC-CNB-27 (`md-section.sh extract ... "Formulas"` F6).

**Control Manifest Rules (Feature layer, manifest 2026-10-25e)**:
- Required: AC-CNB-27 and AC-CNB-21 run at the default and at each cut-order step; the recordable stream is non-Shipping only (ADR-0016)
- Forbidden: filtering motion with a turn-smoothing filter to pass the test (fallback only, Alternative 3, needs a superseding decision); reading server internals in the feature computation (computed on an observer client) (ADR-0016)
- Guardrail: the nightly job runs inside the self-hosted CI runner budget (tests/README.md)

---

## Acceptance Criteria

*From GDD `design/gdd/crowd-npc-behaviour.md` AC-CNB-27 and ADR-0016 section 4 item 6, scoped to this story:*

- [ ] (AC-CNB-27) GIVEN the same CNB command streams fed through the AIController and through a bot client's Enhanced Input path for at least 1,236 30 s windows each (about 10.3 h per pipeline: a nightly multi-process run with at least 10 bot clients), WHEN the nine F6 features are computed on an observer client, THEN every feature passes |d| <= 0.1, variance ratio in [0.8, 1.25] and KS D <= 0.1.
- [ ] A recorded `FCrowdVirtualInputFrame` stream replayed through the bot client path via the ADR-0008 T3 harness yields observer-client `Move` and `Look` equal to the recorded stream (identical quantised `Move`, `Look` within 0.01 degrees), and the replay job appears in the nightly run configuration.
- [ ] The parity run is repeated at the default and at every cut-order step that story 033 enables, with the per-step results recorded.

---

## Implementation Notes

*Derived from ADR-0016 section 4 item 6 and GDD F6:*

- Recording: the follower writes `FCrowdVirtualInputFrame {ServerTime, CameraYaw, CameraPitch, DeviceInput, Gait}` per step (story 010) behind `#if !UE_BUILD_SHIPPING`. The bot client reads a frame file and feeds Enhanced Input values at the same server times.
- The nine F6 features and thresholds are in the GDD (F6); compute them on an observer client from replicated pawn state so the comparison matches what a human watcher sees.
- Nightly: add the job to the CI definition (`.github/workflows/tests.yml`, self-hosted Windows runner); the full 10.3 h run per pipeline is nightly-only, with a short smoke variant (a few windows) on every push.
- Per-step results feed story 033's record file `production/qa/evidence/crowd-cost-step-parity-<date>.md`.
- This story is Integration, not Logic: it needs real processes and network replication, as the QL review noted.

**Prototype disposition** (`production/first-playable-report.md`, `Source/StealthGame`): Reference only. `StealthBot` (console-driven bot, `StealthBot <player#> ...`) shows an existing bot driver, but it works through server rule paths, not Enhanced Input; the production bot client is the ADR-0008 T3 harness. Nothing promoted.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 010: the layer and the recordable frame type
- Story 033: the cost cuts being re-checked
- Story 035: the classifier gate on human data
- The harness epoch (ADR-0008 stories in the session and input epochs): the T3 harness itself

---

## Files

- `Source/StealthTestHarness/Private/CrowdPipelineParityTest.cpp`
- `Source/StealthTestHarness/Private/CrowdVirtualInputReplayTest.cpp`
- `.github/workflows/tests.yml` (nightly job entry)

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY, 2026-10-25). AC-2 revised after the GAPS verdict (replay tolerance and nightly entry defined); type corrected to Integration.*

- **AC-1**: `StealthGame.Crowd.MP.PipelineParity`
  - Given: the same command streams through the AIController and through a bot client's Enhanced Input path, at least 1,236 30 s windows each, at least 10 bot clients
  - When: the nine F6 features are computed on an observer client
  - Then: every feature |d| <= 0.1, variance ratio in [0.8, 1.25], KS D <= 0.1
  - Edge cases: nightly only (deferred); a failing feature names the window set
- **AC-2**: `StealthGame.Crowd.MP.VirtualInputReplay`
  - Given: a recorded `FCrowdVirtualInputFrame` stream
  - When: it is replayed through the bot client path via the T3 harness
  - Then: observer-client Move equal to the recorded quantised Move and Look within 0.01 degrees; the job is in the nightly config
  - Edge cases: first and last frame alignment
- **AC-3**: `StealthGame.Crowd.MP.PipelineParityPerStep`
  - Given: each cut-order step enabled by story 033
  - When: the parity run is repeated
  - Then: results are recorded per step and each step passes AC-1
  - Edge cases: no step enabled, only the default recorded

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/CrowdPipelineParityTest.cpp` and `CrowdVirtualInputReplayTest.cpp` with the nightly run log retained — must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/crowd-npc-behaviour/story-005-command-sinks-epoch-and-halt-filter.md`, `production/epics/crowd-npc-behaviour/story-010-virtual-input-layer.md`
- Unlocks: `production/epics/crowd-npc-behaviour/story-033-cost-cut-order.md`, `production/epics/crowd-npc-behaviour/story-035-classifier-gate.md`
