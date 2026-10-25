# Sprint 1 — 2026-10-26 to 2026-11-06

> Milestone: [M1 MVP-Core](../milestones/M1-mvp-core.md) (Sprint 1 of 7). Generated 2026-10-25 by `/sprint-plan new` (unattended; review mode full).
> Producer gate PR-SPRINT: **CONCERNS**, handled in autonomous mode by option B (adjust scope); the scope below is the adjusted plan. Decision logged in `production/session-logs/decision-log.md`.
> QA plan: none exists. Run `/qa-plan sprint` before implementation begins (autonomous option A, recommended).

## Sprint Goal

"Calibration + bootstrap": land the Tuning Data & Config Exit A bootstrap (module skeleton, harness, lints, CI, VCS checks, stories 001-028), run the conditioning spikes that unblock the most stories, remove the ADR-0013 identity tell (`IsLocallyControlled` branch), and measure how many planning-days of work one developer plus agents actually completes per working day, so the 0.6 estimate factor can be replaced by data.

## Goal Prerequisites

| Needed to show the goal | Status |
|-------------------------|--------|
| Project, engine and start map to run the harness and prototype in | exists: `StealthGame.uproject`, UE 5.8.4, `GameDefaultMap=/Game/StealthGame/Maps/L_Prototype` (`Config/DefaultEngine.ini`), `Content/StealthGame/Maps/L_Prototype.umap` |
| Local build and test cycle | exists: `tools/dev/cycle.ps1`; 12 automation tests pass (technical-setup gate) |
| ADR-0007 module skeleton and test runner (needed by SCL-001, IIF-007, ST-001 and every test story) | built by TDC-001, TDC-002, TDC-003, TDC-004, TDC-019 |
| Net-emulation profiles (needed by SCL-045 and the session spikes) | built by TDC-017 |
| Self-hosted CI runner registered (needed to observe TDC-022 on real CI) | **missing**: a user action named in TDC-022, not a story |
| SCL-003 (tuning keys registered), needed by SCL-004 for the AC-SCL-01 "every SCL tuning key has exactly one value" clause | **missing**: SCL-003 depends on the TDC Exit B key pipeline (stories 038 onward), not in this sprint |
| Factory, controller and possession path for spike IIF-009 (V2) | **missing**: Round & Match and SCL factory stories are not yet created |
| IVP cull mechanism for spike IIF-010 (V3) | **missing**: exists only after IVP-004 and later stories |
| HUD menu root and `UCommonGameViewportClient` configuration for spike IIF-011 (V5) | **missing**: the HUD epic has no stories |
| 60-character graybox street (shared masked-dither character master, graybox map) for spike CAM-011 | **missing**: Character Pool & Appearance and Level Socket Contract have no stories; only `L_Prototype.umap` exists |
| 3060-class reference machine for budgets | **missing / unconfirmed**: all captures in this sprint are labelled development evidence |

Each missing item is copied into Risks. For each, the three fixes open to the user: (1) pull in the Ready story that builds it (SCL-003 would also need the TDC Exit B stories it depends on, and takes the sprint far past the `balanced` 6-10 story range, which this 46-story sprint already exceeds by explicit scope); (2) write the story first with `/create-stories` (Round, Character Pool, Level Socket Contract and HUD stories do not exist yet; `/create-stories` could also split SCL-004 into a scan story and a key-uniqueness story); (3) reword the goal to promise only what the sprint can show, which is what this plan does: the four spikes whose prerequisite is missing (IIF-009, IIF-010, IIF-011, CAM-011) are listed under "Blocked" and not scheduled. No story was invented, widened or edited.

## Capacity

- Unit: planning-days (one developer's focused day with agents doing the implementation). Yardstick, uncalibrated and already agent-compressed by the 0.6 factor in `production/estimates/2026-10-25_mvp-rebaseline.md`: implementation story S 0.25 / M 0.5 / L 1.0; spike story S 0.5 / M 1.0 / L 2.0. TDC-003 and SCL-005 are raised to 1.0 each on the producer's review (CoreRedirects for the module split; retained screenshot and 5.8.4 Blueprint compile for the AnimBP change).
- Total days: 10 (2026-10-26 to 2026-11-06)
- Buffer (20%): 2 days reserved for unplanned work (UE rebuilds after module changes, editor crashes, asset-resave recovery)
- Available: 8 days
- Must Have 8.0 days (100 % of available, 0.0 buffer beyond the 2 reserved), Should Have 9.0 days, Nice to Have 11.5 days, Blocked 4.0 days (not scheduled). Only Must is a commitment. Should is planned work that is cut first; Nice to Have is an ordered pull-forward queue that nobody expects to finish.
- Single-machine constraint: UE builds, PIE and multi-process runs are serial and one editor instance exists. Script-only stories (T0 lint, hooks, CI, VCS checks) can run without the editor while a build is running. Per D41 one `/dev-story` session may batch adjacent small stories of one epic into one commit listing all story IDs.

### Calibration protocol (the sprint's measurement deliverable)

- Record for every story: planned days, actual wall-clock days, number of build/test cycles, and whether a human decision was needed. Write it to `production/sprints/sprint-01-calibration.md` as stories close (a data file, not a report).
- **Day-5 checkpoint (end of 2026-10-30):** velocity = completed planning-days divided by 5. If velocity is below 0.8: stop starting Should stories. If below 0.6: also defer IVP-001 first, then SCL-004 and SCL-005, to Sprint 2 (the producer asked for an explicit Must-cut rule).
- **Sprint close (2026-11-06):** replace the 0.6 factor with the measured ratio and reissue the MVP-remaining estimate (`production/estimates/`). Note that the rebaseline's suggested calibration (a production Roster plus Round slice with its ACs) is not in this sprint: bootstrap and spike work calibrates differently from feature work, so a second calibration point is taken in Sprint 2 or 3 from the first Roster or Round stories.

## Tasks

Estimates are planning-days. Dependencies use story IDs (`TDC` = tuning-data-config, `SCL` = shared-character-locomotion, `IVP` = information-visibility-policy, `IIF` = input-interaction-framework, `ST` = session-travel, `CAM` = camera-system). "Acceptance Criteria" is the headline; the story file holds the full list.

### Must Have (Critical Path)

Committed. 14 stories, 8.0 days. TDC-019 settles an unverified item (ADR-0008 V1, CQTest PIE networking under `-nullrhi`) and the SCL chain waits on it: schedule it by day 3. IVP-001 needs the machine exclusively: hard 2-day timebox, run it when no build is running.

| ID | Task | Agent/Owner | Est. Days | Dependencies | Acceptance Criteria |
|----|------|-------------|-----------|-------------|-------------------|
| TDC-001 | [Foundation module skeleton and server-only guard header](production/epics/tuning-data-config/story-001-foundation-module-skeleton.md) | lead-programmer | 0.25 | None | Test StealthGame.Build.FoundationLayout passes (module loaded, .uproject entry, Build.cs deps, guard header, folders) |
| TDC-002 | [Gameplay and Presentation module skeletons with the presentation guard define](production/epics/tuning-data-config/story-002-gameplay-presentation-module-skeletons.md) | lead-programmer | 0.25 | tdc-001 | Gameplay and Presentation modules build; private STEALTH_PRESENTATION_MODULE define reaches every Presentation TU |
| TDC-003 | [Split the prototype module into the ADR-0007 composition root](production/epics/tuning-data-config/story-003-split-prototype-composition-root.md) | lead-programmer | 1.0 | tdc-001, tdc-002 | Prototype split into the ADR-0007 composition root; builds and the 12 existing tests still pass; Blueprint references survive (check CoreRedirects) |
| TDC-004 | [StealthTestHarness module excluded from Shipping and Test](production/epics/tuning-data-config/story-004-test-harness-module-shipping-excluded.md) | lead-programmer | 0.25 | tdc-001 | StealthTestHarness module excluded from Shipping and Test targets |
| TDC-007 | [IStealthClock and an injectable fake clock](production/epics/tuning-data-config/story-007-stealth-clock-interface.md) | lead-programmer | 0.25 | tdc-001, tdc-008 | IStealthClock plus injectable fake clock; logic test passes |
| TDC-008 | [IStealthFaultSink and the WITH_STEALTH_TEST_SEAMS define](production/epics/tuning-data-config/story-008-fault-sink-and-test-seams-define.md) | lead-programmer | 0.25 | tdc-001 | IStealthFaultSink and WITH_STEALTH_TEST_SEAMS define; logic test passes |
| TDC-009 | [Log-capture hook asserting warnings by key](production/epics/tuning-data-config/story-009-log-capture-hook.md) | lead-programmer | 0.25 | tdc-001, tdc-008 | Log-capture hook asserts warnings by key; logic test passes |
| TDC-011 | [T0 lint framework: observation emitter and gate script](production/epics/tuning-data-config/story-011-t0-lint-framework-and-gate.md) | devops-engineer | 0.5 | None | T0 lint framework emits observations and gate script runs; G2 wrapper test passes |
| TDC-012 | [Include lint rules (a)-(c): header placement, guard first, view must not include server](production/epics/tuning-data-config/story-012-include-lint-rules-a-c.md) | devops-engineer | 0.5 | tdc-011, tdc-001, tdc-002 | Include lint rules (a)-(c) pass on the tree and fail on seeded violations |
| TDC-019 | [CQTest listen server with three clients runs headless under -nullrhi](production/epics/tuning-data-config/story-019-cqtest-headless-proof.md) | qa-lead | 0.5 | tdc-003, tdc-004 | CQTest listen server plus 3 clients runs headless under -nullrhi (settles ADR-0008 V1); schedule by day 3 |
| SCL-001 | [One Shared Pawn class with an identical default subobject set](production/epics/shared-character-locomotion/story-001-pawn-class-default-subobjects.md) | gameplay-programmer | 0.5 | tdc-001..004, tdc-019 | One shared pawn class with identical default subobjects; logic test passes (prerequisite of SCL-004/005, pulled in) |
| SCL-004 | [Class parity scan: no observable path branches on controller kind, with a negative control](production/epics/shared-character-locomotion/story-004-class-parity-scan.md) | gameplay-programmer | 0.5 | scl-001 (and SCL-003, not in sprint) | Class parity scan with negative control passes; the key-uniqueness clause of AC-SCL-01 stays open until SCL-003 (carryover) |
| SCL-005 | [Remove the sample AnimBP's IsLocallyControlled branch for every pawn](production/epics/shared-character-locomotion/story-005-animbp-islocallycontrolled-removal.md) | gameplay-programmer | 1.0 | scl-004 (scan half), scl-001 | ADR-0013: zero IsLocallyControlled reads in AnimBP and UStealthAnimInstance, scan negative control fails on unmodified sample, sample BPs compile on 5.8.4, retained screenshot |
| IVP-001 | [Spike S1, multi-process raw-bunch capture shows no identity pointers to non-owners](production/epics/information-visibility-policy/story-001-spike-s1-multiprocess-raw-bunch-capture.md) | network-programmer | 2.0 | None (prototype build) | Spike S1 report in production/qa/spikes: multi-process raw-bunch capture shows no identity pointers to non-owners (PASS or amendment); 2-day timebox, machine exclusive |

### Should Have

Planned, not committed: first to be cut at the day-5 checkpoint. 14 stories, 9.0 days. TDC-017 first (SCL-045 needs it); TDC-024 before TDC-020.

| ID | Task | Agent/Owner | Est. Days | Dependencies | Acceptance Criteria |
|----|------|-------------|-----------|-------------|-------------------|
| TDC-005 | [Compile-probe runner with the ADR-0007 guard probe and its positive control](production/epics/tuning-data-config/story-005-compile-probe-runner-and-guard-probe.md) | devops-engineer | 0.5 | tdc-001, tdc-002, tdc-003 | Compile-probe runner proves the guard #error fires, with positive control |
| TDC-006 | [Shipping-symbol check: no harness module and no test-seam symbols in Shipping](production/epics/tuning-data-config/story-006-shipping-symbol-check.md) | devops-engineer | 0.5 | tdc-004, tdc-008 | Shipping-symbol check finds no harness module or test-seam symbols in Shipping |
| TDC-010 | [Negative-control helper: the suite passes only if the inner check fails](production/epics/tuning-data-config/story-010-negative-control-helper.md) | qa-lead | 0.25 | tdc-001, tdc-008 | Negative-control helper: suite passes only if the inner check fails |
| TDC-013 | [Include lint rules (d)-(f): core/features direction, foundation upward includes, unique header names](production/epics/tuning-data-config/story-013-include-lint-rules-d-f.md) | devops-engineer | 0.25 | tdc-011 | Include lint rules (d)-(f) pass and fail on seeded violations |
| TDC-014 | [Include lint rules (g)-(h): server UCLASS specifiers and presentation forbidden APIs](production/epics/tuning-data-config/story-014-include-lint-rules-g-h.md) | devops-engineer | 0.5 | tdc-011 | Include lint rules (g)-(h) pass and fail on seeded violations |
| TDC-015 | [Include lint rules (i)-(j): log arguments and generator random isolation](production/epics/tuning-data-config/story-015-include-lint-rules-i-j.md) | devops-engineer | 0.25 | tdc-011 | Include lint rules (i)-(j) pass and fail on seeded violations |
| TDC-016 | [Seeded violation per lint rule watched to fail](production/epics/tuning-data-config/story-016-lint-seeded-violation-controls.md) | qa-lead | 0.25 | tdc-012..015 | Each lint rule has a seeded violation watched to fail |
| TDC-017 | [Net emulation profiles StealthLeak1, StealthLeak2, StealthClean](production/epics/tuning-data-config/story-017-net-emulation-profiles.md) | network-programmer | 0.25 | None | Net emulation profiles StealthLeak1, StealthLeak2, StealthClean exist; RTT measured and recorded (needed by SCL-045) |
| TDC-018 | [Harness replication-system assertion and result JSON](production/epics/tuning-data-config/story-018-replication-system-assertion.md) | qa-lead | 0.5 | tdc-004, tdc-007 (and tdc-008, undeclared) | Harness replication-system assertion writes result JSON |
| TDC-020 | [Gauntlet host plus client smoke scenario with legacy-driver assertion](production/epics/tuning-data-config/story-020-gauntlet-multiprocess-smoke.md) | qa-lead | 1.0 | tdc-018, tdc-019 (and tdc-024, undeclared) | Gauntlet host plus client smoke scenario with legacy-driver assertion; records ADR-0008 V3/V7 and ADR-0007 V2 observations |
| TDC-021 | [Per-commit gate script: T0 lint, build, prefix-filtered automation run](production/epics/tuning-data-config/story-021-per-commit-gate-script.md) | devops-engineer | 0.5 | tdc-011, tdc-003 | Per-commit gate script runs T0 lint, build and prefix-filtered automation; under the 10-minute budget or measured |
| TDC-024 | [VCS conformance check for ignore and attribute rules](production/epics/tuning-data-config/story-024-vcs-conformance-check.md) | devops-engineer | 0.25 | None | VCS conformance check for ignore and attribute rules passes (Build/ not ignored) |
| SCL-045 | [Net-emulation and traversal spike report (unblocks Exit B and starvation)](production/epics/shared-character-locomotion/story-045-net-emulation-traversal-spike.md) | ue-replication-specialist | 2.0 | tdc-017 | Net-emulation and traversal spike report: traversal under 100 ms / 1 % loss, PASS or fallback recorded |
| IVP-004 | [Spike S9, the explicit channel close lands at the deadline and the channel stays closed](production/epics/information-visibility-policy/story-004-spike-s9-cull-latency-and-explicit-channel-close.md) | network-programmer | 2.0 | None | Spike S9 report: explicit channel close lands at the deadline and stays closed |

### Nice to Have

**Stretch.** Ordered pull-forward queue, 14 stories, 11.5 days (about twice what the sprint can hold). IIF-007 and ST-001 are prerequisites pulled in because requested stories depend on them (IIF-008; ST-002 to ST-004).

| ID | Task | Agent/Owner | Est. Days | Dependencies | Acceptance Criteria |
|----|------|-------------|-----------|-------------|-------------------|
| TDC-022 | [CI workflow runs the T0 lint and the same gate](production/epics/tuning-data-config/story-022-ci-workflow-t0-lint-step.md) | devops-engineer | 0.25 | tdc-021 | CI workflow runs the T0 lint and the same gate (real runner run unobserved: user action) |
| TDC-023 | [Nightly workflow: compile probes, T3 suites, Shipping-symbol check](production/epics/tuning-data-config/story-023-nightly-workflow.md) | devops-engineer | 0.5 | tdc-005, tdc-006, tdc-020, tdc-021 | Nightly workflow: compile probes, T3 suites, Shipping-symbol check |
| TDC-025 | [Tracked pre-commit hook framework running the T0 lint](production/epics/tuning-data-config/story-025-pre-commit-hook-framework.md) | devops-engineer | 0.25 | tdc-011 | Tracked pre-commit hook framework runs the T0 lint |
| TDC-026 | [Pre-commit check rejects changes to never-resave assets](production/epics/tuning-data-config/story-026-never-resave-commit-check.md) | devops-engineer | 0.25 | tdc-025 | Pre-commit check rejects changes to never-resave assets |
| TDC-027 | [Pre-commit check compares changed assets with git lfs locks](production/epics/tuning-data-config/story-027-lfs-lock-commit-check.md) | devops-engineer | 0.5 | tdc-025 | Pre-commit check compares changed assets with git lfs locks; records real lock behaviour once |
| TDC-028 | [commit-msg hook enforces Conventional Commits with a story or ADR reference](production/epics/tuning-data-config/story-028-commit-message-check.md) | devops-engineer | 0.25 | tdc-025 | commit-msg hook enforces Conventional Commits with story or ADR reference |
| IVP-005 | [Spike S10, a re-opened pawn is a new actor that presents correctly from replicated state](production/epics/information-visibility-policy/story-005-spike-s10-reopen-behaviour-and-netguid.md) | network-programmer | 1.0 | ivp-004 | Spike S10 report: re-opened pawn is a new actor presenting correctly from replicated state |
| IVP-006 | [Spike S12, the cull delay and un-cull lead are recomputed from measured L99](production/epics/information-visibility-policy/story-006-spike-s12-cull-delay-vs-measured-l99.md) | network-programmer | 0.5 | ivp-004, ivp-005 | Spike S12 report: cull delay and un-cull lead recomputed from measured L99 |
| IIF-007 | [IA_* actions and IMC contexts exist and the trigger lint passes](production/epics/input-interaction-framework/story-007-input-assets-and-trigger-lint.md) | gameplay-programmer | 0.5 | tdc-003 | IA_* actions and IMC contexts exist and the trigger lint passes (prerequisite of IIF-008, pulled in) |
| IIF-008 | [Spike V1 reports Enhanced Input edge behaviour on this project's contexts](production/epics/input-interaction-framework/story-008-spike-v1-enhanced-input-edges.md) | gameplay-programmer | 1.0 | iif-007 | Spike V1 report: Enhanced Input edge behaviour on the project contexts |
| ST-001 | [PIE harness hook forces client disconnect, rejoin, supersede and host kick](production/epics/session-travel/story-001-harness-connection-event-hook.md) | network-programmer | 0.5 | None (skeleton: tdc-003 or prototype path) | PIE harness hook forces client disconnect, rejoin, supersede and host kick (prerequisite of ST-002..004, pulled in) |
| ST-002 | [Net-emulation spike measures RTT, packet rate, burst outages and map load time](production/epics/session-travel/story-002-net-emulation-measurement-spike.md) | network-programmer | 2.0 | st-001 | Net-emulation spike measures RTT, packet rate, burst outages and map load time |
| ST-003 | [Spike S3: stores and views survive two seamless travels in multi-process standalone](production/epics/session-travel/story-003-spike-s3-seamless-travel-persistence.md) | network-programmer | 2.0 | st-001 | Spike S3: stores and views survive two seamless travels in multi-process standalone (fake Roster allowed) |
| ST-004 | [Spike S4: retained PlayerState adopted on rejoin with no visible change to other clients](production/epics/session-travel/story-004-spike-s4-rejoin-playerstate-adoption.md) | network-programmer | 2.0 | st-001 | Spike S4: retained PlayerState adopted on rejoin with no visible change to other clients |

### Blocked (requested, not schedulable this sprint)

Requested for Sprint 1 but the prerequisite is missing (see Goal Prerequisites). Listed in `production/sprint-status.yaml` as `blocked`, not counted as committed or stretch. Estimates shown are for planning the later sprint.

| ID | Task | Agent/Owner | Est. Days | Blocked by | Acceptance Criteria |
|----|------|-------------|-----------|-----------|-------------------|
| IIF-009 | [Spike V2 reports ClientSetPossessionEpoch always precedes ClientRestart](production/epics/input-interaction-framework/story-009-spike-v2-possession-epoch-order.md) | gameplay-programmer | 1.0 | Needs factory, controller and possession path (Round and SCL stories not yet written) | Spike V2 report: ClientSetPossessionEpoch always precedes ClientRestart |
| IIF-010 | [Spike V3 reports server NetGUID resolution of a culled pawn](production/epics/input-interaction-framework/story-010-spike-v3-netguid-server-resolution.md) | gameplay-programmer | 1.0 | Needs the IVP cull mechanism (ivp-004 and later stories) and the shared pawn | Spike V3 report: server NetGUID resolution of a culled pawn |
| IIF-011 | [Spike V5 reports Common UI Menu-mode switch yields a synchronous End](production/epics/input-interaction-framework/story-011-spike-v5-commonui-menu-end.md) | ue-umg-specialist | 1.0 | iif-007; needs a HUD menu root and UCommonGameViewportClient config (HUD epic has no stories) | Spike V5 report: Common UI Menu-mode switch yields a synchronous End |
| CAM-011 | [Camera mask spike (OQ5 / OQ10)](production/epics/camera-system/story-011-mask-spike-oq5-oq10.md) | gameplay-programmer | 1.0 | Needs the 60-character graybox street (Character Pool and Level Socket Contract epics have no stories) | Camera mask spike report OQ5/OQ10 with retained screenshots |

## Carryover from Previous Sprint

| Task | Reason | New Estimate |
|------|--------|-------------|
| None: this is Sprint 1 (no previous sprint, no velocity history) | n/a | n/a |

Expected carryover into Sprint 2 regardless of velocity: the AC-SCL-01 key-uniqueness clause of SCL-004 (needs SCL-003), and the rest of the TDC Exit A stories that fall below the Must line.

## Risks

| Risk | Probability | Impact | Mitigation |
|------|------------|--------|------------|
| Velocity is uncalibrated; Must uses 100 % of available days, so any slip eats the 2-day buffer | High | Must misses by 1-3 days | Day-5 checkpoint with an explicit Must-cut rule (above); Should and Nice are cut first |
| TDC-019 (CQTest PIE networking under `-nullrhi`, ADR-0008 V1) is unverified; the SCL chain (TDC-001 to 003 to 019, then SCL-001, 004, 005) waits on it | Medium | SCL stories lose their test runner; ADR-0008 amendment | Schedule TDC-019 by day 3; if it fails, record the ADR-0008 amendment request and run SCL logic tests in the editor-context runner |
| **Missing prerequisite:** SCL-003 (needs TDC Exit B) so SCL-004's key-uniqueness clause and the story as a whole cannot fully close; SCL-005 depends on SCL-004 | High | SCL-004 closes partially; SCL-005's closing evidence rests on the scan half | Treat SCL-005's dependency as satisfied by SCL-004's scan half; carry the key-uniqueness clause to the sprint that builds SCL-003; consider `/create-stories` to split SCL-004 |
| **Missing prerequisite:** IIF-009, IIF-010, IIF-011, CAM-011 cannot start (see Goal Prerequisites) | Certain | Four requested spikes not delivered this sprint | Listed under Blocked; re-plan when Round/SCL factory, IVP cull, HUD menu root and the graybox street stories exist |
| **Missing prerequisite:** self-hosted CI runner not registered | High | TDC-022 closes on the workflow file only; real CI run unobserved | User action; recorded in TDC-022 |
| **Missing prerequisite:** 3060-class reference hardware | Medium | Spike measurements are development evidence only | Label every capture with the machine; budgets stay ASSUMED |
| TDC-003 (prototype split) may break Blueprint references (CoreRedirects) and `.uasset` resave hazards (`production/never-resave-assets.md`) | Medium | Lost day, asset corruption | Estimate raised to 1.0; no `Content/*.uasset` is resaved by any story in this plan; never-resave check (TDC-026) is stretch, so apply the rule by hand |
| SCL-005 changes player-observable animation: needs a retained screenshot (run-and-observe) and 5.8.4 compile of the sample Blueprints after the 5.8 animation API removals (ANIM-V6); touches Blueprint assets | Medium | Evidence missing or asset edits blocked | Estimate raised to 1.0; sample assets are never edited in place (`production/never-resave-assets.md`, ADR-0009 s6): duplicate or reparent with a `_snapshots/` copy first; the AnimBP is not on the never-resave list, but record a blocker rather than resave anything that is |
| IVP-001 (S1) needs the machine for 2 days and cannot overlap build-heavy TDC work | Medium | Serializes the sprint | Hard 2-day timebox, scheduled in a build-free window (days 6-7) |
| TDC-020 relies on TDC-024 (Build/ not ignored), undeclared; it also carries three unverified items | Medium | Smoke scenario cannot run | TDC-024 moved into Should; TDC-020 is the weakest Should candidate |
| Story count (46) is far above the `balanced` range (6-10) | Certain | Plan is a queue, not a commitment | Only Must (14) is committed; D41 batching applies |
| Spike results may fail ADR conditions (ADR-0003, 0013, 0022 are Accepted conditional on spikes) | Medium | Dependent stories need rework | Each spike closes with PASS, FAIL plus fallback, or an amendment request; never by skipping |

## Dependencies on External Factors

- Unreal Engine 5.8.4 installed locally; one editor instance; `tools/dev/cycle.ps1` closes a running editor first.
- GitHub remote and Git LFS lock behaviour (TDC-027, stretch); self-hosted runner registration (user action, TDC-022).
- No Content/ asset changes are planned by this sprint other than the SCL-005 AnimBP change, done on a snapshotted duplicate per `production/never-resave-assets.md`.
- Human review time (single developer) for ADR amendment requests that spike results may raise.

## Suggested Sequence (indicative)

1. Days 1-3: TDC-001, 002, 004, 008 (module skeletons), then TDC-003 (the split, 1.0 day, full rebuild), TDC-007, 009; TDC-011 and TDC-012 in parallel (script-only); **TDC-019 by end of day 3**.
2. Days 3-5: SCL-001 (needs the test runner), SCL-004. Day-5 checkpoint. Start Should in this window if velocity allows: TDC-017 first (SCL-045 needs it), TDC-024, TDC-010, TDC-013 to 016.
3. Days 6-7: IVP-001 in a build-free window (2-day timebox).
4. Days 8-9: SCL-005 (editor work, screenshot evidence); Should as capacity allows: SCL-045, IVP-004, TDC-005, 006, 018, 020, 021.
5. Day 10: calibration write-up, sprint close, QA sign-off request.

Pull-forward order for Nice to Have (only if Must and Should are done): TDC-025 to 028 (hooks), IVP-005, IVP-006, IIF-007 then IIF-008, ST-001 then ST-002 to ST-004, TDC-022, TDC-023.

## Definition of Done for this Sprint

- [ ] All Must Have tasks completed (SCL-004 counts as done when its scan half passes; the key-uniqueness clause is recorded as carryover)
- [ ] All tasks pass acceptance criteria
- [ ] QA plan exists (`production/qa/qa-plan-sprint-01-2026-10-26.md`, from `/qa-plan sprint`): **not yet written; run before implementation begins**
- [ ] All Logic/Integration stories have passing unit/integration tests
- [ ] Smoke check passed (`/smoke-check sprint`)
- [ ] QA sign-off report: APPROVED or APPROVED WITH CONDITIONS (`/team-qa sprint`)
- [ ] No S1 or S2 bugs in delivered features
- [ ] Design documents updated for any deviations
- [ ] Code reviewed and merged
- [ ] Every spike story has its report in `production/qa/spikes/` with PASS, FAIL plus fallback, or an amendment request
- [ ] `production/sprints/sprint-01-calibration.md` filled in and the MVP-remaining estimate reissued with the measured factor
- [ ] SCL-005 evidence screenshot retained in `production/qa/evidence/`
