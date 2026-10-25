# Epic: HUD

> **Layer**: Presentation
> **GDD**: design/gdd/hud.md
> **Architecture Module**: HUD in `StealthPresentation` (view-models as plain `USTRUCT`s, widget stack with the HUD-R4 ZOrders, `PresentedOwnState`; the L0 world-anchor leaf also paints Pings rings)
> **Status**: Ready
> **Stories**: 60 stories (see the Stories table below)
> **Engine Risk**: HIGH (highest among governing ADRs; engine 5.8, installed 5.8.4)
> **Control Manifest**: 2026-10-25f
> **Build Order**: 19 of 19 (layer order; the risk-first sequence is in the Build Order and Gates section)

## Overview

The HUD is the client-side presentation layer that turns the viewer's own private state, their team's shared state and the round's public state into a small set of screen-space plates, rings, cards and lines. It owns no gameplay rule and no data: every value it shows is already held by the viewing client through exactly three carriers (the Policy's host-parity rule): the owner data on the `APlayerController` (strike and defence results, Penalty's wrong-target tally, exile flag and respawn due time, IIF hold status and reply codes, the owner's own concealment flag), the viewer's C-TeamView (own state and team, teammates' states, Objectives' team view: stage card, own steps, `RivalPrelimCount`), and the GameState's `FRoundPublicState`. It is deliberately quiet: the player's wrong-target tally and brace indicator bottom-left, the team's stage card and race steps top centre, at most three team lines top right, one result plate after each strike lower centre, a one-line banner at round end, and an always-empty screen centre.

The epic delivers the UMG layer stack drawn after post-process (HUD-R4 ZOrders, safe-margin anchors), view-models, plate and card timing at the local contact frame from the RTT-compensated server clock, `PresentedOwnState`, round surfaces with a rebuild on `RoundEpoch` change, the client-local settings hooks (scale, reduce motion, high contrast; the full Settings system is Vertical Slice), the latency warning threshold (`HudLatencyWarnRttSec`), the L0 leaf that paints Pings rings and edge rings (HUD-E22), the text-only round summary (HUD-E21) and the reconnect feed lines, and the subject-kind parity rule enforced by the automated leak suite. Art bible 7.1's rule, nothing on screen is ever about a person who is not an ally, is enforced as testable leak rules (HUD-R2, HUD-R3).

## Governing ADRs

| ADR | Decision Summary | Engine Risk |
|-----|------------------|-------------|
| ADR-0001: Crowd identity rules | Four audience carriers (C-Public allowlist on PlayerState, owner-only C-TeamView on PlayerController, C-Server, GameState public structs); every UE default that reveals player-ness is overridden; one pawn class, one look path, identity blindness. | HIGH |
| ADR-0008: Test and automation strategy for leak and parity tests | Headless pure-C++ cores, multi-client and multi-process leak harness, raw-bunch capture, net-emulation profiles, per-commit canaries. | MEDIUM |
| ADR-0018: Encounter data and outcome presentation timing | Outcome-blind FActionState at the queue slot, contact-time pending writes, owner-only results, MotionWarping (Beta in 5.8.4). | HIGH |
| ADR-0021: UI/HUD layering, CommonUI and Enhanced Input | UMG layer/ZOrder stack drawn after post-process, Common UI for menus only, client-local settings hooks, ping rings in HUD L0. | HIGH |

All governing ADRs are Accepted (user, 2026-10-25). Several are conditionally accepted; the outstanding gates are in Build Order and Gates below.

## GDD Requirements

7 of 7 requirements are covered by an Accepted ADR. Untraced requirements: None.

| TR-ID | Requirement | ADR Coverage |
|-------|-------------|--------------|
| TR-hud-001 | Reads only the three carriers | ADR-0001 ✅ |
| TR-hud-002 | Subject-kind parity rule and automated leak suite | ADR-0008 ✅ |
| TR-hud-003 | Layer/ZOrder stack after post-process; safe-margin anchors | ADR-0021 ✅ |
| TR-hud-004 | Server-clock timing: plates at local contact frame, tally with plate, `PresentedOwnState` | ADR-0018, ADR-0021 ✅ |
| TR-hud-005 | Round surfaces from `FRoundPublicState`; rebuild on `RoundEpoch` change | ADR-0021 ✅ |
| TR-hud-006 | Client-local settings hooks (scale, reduce motion, high contrast) | ADR-0021 ✅ |
| TR-hud-007 | Latency warning threshold (`HudLatencyWarnRttSec`; duplicate retired 2026-10-25) | ADR-0021 ✅ |

## Prototype Disposition

The code in `Source/StealthGame` is marked PROTOTYPE (`production/first-playable-report.md`). Production code follows ADR-0007 modules, and nothing is promoted until the ADR that governs it is built against.

**Replaces** `Source/StealthGame/Private/StealthHUD.cpp` / `Public/StealthHUD.h` (281 + 40 lines: an `AHUD` canvas pass) and the message plumbing in `StealthPlayerController.*` and `StealthGameMode.cpp` (`MessagePlayer`, `MessageController`, `MessageTeam`, `MessageAll`). The prototype HUD showed kill confirmation, civilian count, defence state and cooldown, stage progress, enemy-stage alert, round banner and teammate markers, seen on clients and passing the first-playable networked tests; the production HUD is UMG and carrier-driven (ADR-0021). The prototype leaks (it reads a team field replicated on every pawn) are not promoted.

## Build Order and Gates

- **Depends on (epics)**: participant-state-roster, round-match-framework (`FRoundPublicState`, client `EstimatedServerNow()`), information-visibility-policy, assassination-defence-reversal and npc-kill-penalty-informants (owner data), objectives-chain (team view), concealment-bushes, input-interaction-framework (reply sink), tuning-data-config, camera-system (view state), session-travel (reconnect), teammate-markers (soft)
- **Gates and risks**: ADR-0021 conditional gate: UI-V1..UI-V4 and the HUD and Teammate Markers GDD re-reviews. UI is a HIGH-risk domain for 5.8 (Enhanced Input and Common UI unified). Carries the HUD halves of MVP items 9 (round-summary lines, HUD-E21, AC-HUD-27) and 10 (reconnect feed lines, AC-HUD-28 a-h), both gated on the Round Event Log and Session rejoin existing. UI stories require retained screenshots in `production/qa/evidence/`. Days-per-AC (0.52) is UI-evidence-heavy; confirm at story breakdown. As the last Presentation epic it also owns the cross-cutting **MVP exit verification** (a final 5v5 evidence pass over the Presentation surfaces; the 10-player network pass itself is owned by procedural-city-generator).
- **Estimate**: about 12 bottom-up days (human-paced yardstick, `production/estimates/2026-10-25_mvp-rebaseline.md`, Method B work packages); planning figure about 7.2 days after the uniform 0.6 calibration. Low confidence: no sprint history exists, so run the 2-week calibration spike and re-baseline.
- **Acceptance criteria in the GDD**: about 23 (counted as GIVEN-form criteria in the Acceptance Criteria section).
- **MVP build order**: risk-first per `design/gdd/systems-index.md`: graybox map and mannequin-variant pool first, then camera toggle and traversal replication, then the Meshy pool swap, then the generator.

## Stories

Granularity `balanced` (2-4 acceptance criteria per story). Numbered 001-061; **story 010 was dropped before writing** (the UI-V1 zero-lag projection spike is owned by `production/epics/teammate-markers/story-018-spike-ui-v1-zero-lag-projection.md`; HUD stories 011, 048, 049, 060 and 061 depend on it) and its number is not reused. Spike stories in this epic: 008 (UI-V4, DPI) and 012 (UI-V3, layer order); UI-V2 is `input-interaction-framework/story-011-spike-v5-commonui-menu-end.md`. Stories carry Control Manifest 2026-10-25f. TR-hud-008 (HUD cost, AC-HUD-22) exists in `docs/architecture/tr-registry.yaml` but not in the requirements table above; it is covered by stories 006 and 058.

This epic owns the final MVP evidence pass (stories 060 and 061), the round-summary display (MVP item 9: stories 040, 042, 043) and the reconnect line display (MVP item 10: stories 037, 038). QL-STORY-READY (qa-lead, 2026-10-25): 26 stories were first returned GAPS, their criteria were revised, and all 60 stories were re-judged ADEQUATE. GDD change requests raised by the gate for the GDD owner (no GDD was edited): align AC-HUD-01, AC-HUD-07 and HUD-R3 with F6 (the tally cluster differs in the WrongTarget run); clarify HUD-R17 against the F2 replay window (stories 015 and 044 resolve it per rebuild trigger); AC-HUD-27 needs a fixture because an organic 2v2 round cannot end by objective win with team 1 fully eliminated; AC-HUD-21 wording (countdown only in Informant Awaiting; feed contextual).

| # | Story | Type | Status | ADR |
|---|-------|------|--------|-----|
| 001 | [HUD tuning keys with ranges, client audience and cross-field invariants](story-001-hud-tuning-keys-and-invariants.md) | Config/Data | Ready | ADR-0021 |
| 002 | [FHudViewModel clusters, deterministic Dump() and the headless FHudPresenterCore shell](story-002-hud-view-model-and-presenter-core-shell.md) | Logic | Ready | ADR-0021 |
| 003 | [UStealthHudPresenter sampling carriers every frame by value, identically for host and client](story-003-hud-input-sampling-and-host-parity-path.md) | Integration | Ready | ADR-0021 |
| 004 | [Build-time include and grep check for the HUD module (AC-HUD-11) plus the ADR-0021 lints](story-004-hud-include-boundary-and-forbidden-api-lint.md) | Logic | Ready | ADR-0001 |
| 005 | [ST_Hud string table with the approved wording and the language-rule lint](story-005-hud-string-table-and-language-lint.md) | Logic | Ready | ADR-0021 |
| 006 | [HUD layer stack at the HUD-R4 ZOrders, created once, hit-test-invisible, with no input effects](story-006-hud-layer-stack-root-and-input-isolation.md) | Integration | Ready | ADR-0021 |
| 007 | [Layout resolver (F1): safe rectangle, slot positions, text floors and the A_persist lint](story-007-hud-layout-resolver-and-persistent-area-budget.md) | Logic | Ready | ADR-0021 |
| 008 | [Spike UI-V4: custom DPI scaling rule and root scaler on 5.8.4](story-008-spike-ui-v4-dpi-scaling-rule.md) | Integration | Ready | ADR-0021 |
| 009 | [UStealthDpiScalingRule (H/1080) and the layer-root scaler applying s_ui once](story-009-hud-dpi-rule-and-root-scaler.md) | Integration | Blocked | ADR-0021 |
| 011 | [SStealthWorldAnchorLeaf shell with constant-pixel anchors and the neutral-view collapse](story-011-hud-world-anchor-leaf-shell.md) | Integration | Blocked | ADR-0021 |
| 012 | [Spike UI-V3: fade layer order under real plates and probes](story-012-spike-ui-v3-layer-order-with-camera-fade.md) | Integration | Ready | ADR-0021 |
| 013 | [HUD fade layer renders Camera's published fade; AC-HUD-24 LayerOrder](story-013-hud-fade-layer-and-layer-order-test.md) | Integration | Blocked | ADR-0021 |
| 014 | [PresentedOwnState (HUD-R6a), the viewer state table and the visibility matrix](story-014-hud-presented-own-state-and-viewer-state-machine.md) | Logic | Ready | ADR-0021 |
| 015 | [F2 result show time, matching fallback and duplicate/late rules](story-015-hud-result-show-time-f2.md) | Logic | Ready | ADR-0018 |
| 016 | [F3 plate and card timelines sampled from now - t0, including Reduce motion and read hold](story-016-hud-plate-timeline-f3-and-reduce-motion.md) | Logic | Ready | ADR-0021 |
| 017 | [Shared visual system: art-director display face, plate style, contrast and high contrast](story-017-hud-shared-visual-system-font-face-and-plate-style.md) | UI | Ready | ADR-0021 |
| 018 | [Brace indicator states and the F5 cooldown numeral and sweep](story-018-hud-brace-indicator-state-logic.md) | Logic | Ready | ADR-0018 |
| 019 | [Brace indicator widget with persistent label, bound glyph and cooling sweep](story-019-hud-brace-indicator-widget.md) | UI | Ready | ADR-0021 |
| 020 | [F6 tally display and the HUD-R7 hold, re-push gate and client-local cache](story-020-hud-wrong-target-tally-logic.md) | Logic | Ready | ADR-0018 |
| 021 | [Tally widget: squares, numeral, danger triangle and informant frozen state](story-021-hud-wrong-target-tally-widget.md) | UI | Ready | ADR-0021 |
| 022 | [Result and defence plates wired to F2/F3 with per-outcome text, stamp and cue id](story-022-hud-result-and-defence-plate-flow.md) | Integration | Ready | ADR-0018 |
| 023 | [AC-HUD-07 strike strip capture test and per-outcome plate screenshots](story-023-hud-strike-strip-test-and-plate-evidence.md) | UI | Ready | ADR-0008 |
| 024 | [F4 own removal time, card gating and the once-only OnRemovalCardEntered event](story-024-hud-removal-card-timing-f4-logic.md) | Logic | Ready | ADR-0018 |
| 025 | [Removal card widget, approved wording and the stepped preempt of the Result slot](story-025-hud-removal-card-widget-and-preempt.md) | UI | Ready | ADR-0021 |
| 026 | [PIE proof of removal card timing and the informant-killed and hold-cap cases](story-026-hud-removal-card-pie-timing.md) | Integration | Ready | ADR-0018 |
| 027 | [PIE proof that the tally never moves before its plate and informant kills equal NPC kills](story-027-hud-tally-moves-with-plate-and-informant-parity-pie.md) | Integration | Ready | ADR-0008 |
| 028 | [Leak suite: strike-target parity and look parity for every kind of non-ally subject](story-028-hud-leak-suite-strike-and-look-parity.md) | Integration | Ready | ADR-0008 |
| 029 | [Leak suite: no HUD warning before the viewer's own contact frame](story-029-hud-leak-suite-no-warning-before-contact.md) | Integration | Ready | ADR-0008 |
| 030 | [Strike hint predicate (HUD-R8) and lockout suppression](story-030-hud-strike-hint-predicate-and-lockout.md) | Logic | Ready | ADR-0021 |
| 031 | [Result and Notice slot priority, one-shot drops and the rejection, informant and ping-refusal limiters](story-031-hud-notice-slot-priority-and-rate-limits.md) | Logic | Ready | ADR-0021 |
| 032 | [Rejection plate (L1, beside the brace row), informant plate, third-person hint and strike hint widgets](story-032-hud-rejection-informant-and-hint-widgets.md) | UI | Ready | ADR-0021 |
| 033 | [F7 returning countdown text and its Notice-slot rules](story-033-hud-returning-countdown-f7.md) | Logic | Ready | ADR-0021 |
| 034 | [Informant presentation: brackets, frozen tally, countdown slot and the informant view contract](story-034-hud-informant-presentation.md) | Integration | Ready | ADR-0021 |
| 035 | [F8 feed queue and the HUD-R10 ally removal lines](story-035-hud-team-feed-queue-f8-and-removal-lines.md) | Logic | Ready | ADR-0001 |
| 036 | [PIE proof that opponents never receive team lines and that no line is created after Ended](story-036-hud-team-feed-opponent-and-round-end-pie.md) | Integration | Ready | ADR-0001 |
| 037 | [HUD-R10a reconnect line rules from observed bReconnecting edges and ReconnectEndCause](story-037-hud-reconnect-lines-logic.md) | Logic | Ready | ADR-0001 |
| 038 | [PIE proof of every reconnect line case (AC-HUD-28 a-h)](story-038-hud-reconnect-lines-pie.md) | Integration | Blocked | ADR-0001 |
| 039 | [F9 objectives cluster update: own steps, rival steps and the monotonic rival count](story-039-hud-objectives-cluster-f9-logic.md) | Logic | Ready | ADR-0001 |
| 040 | [F10 banner show time, banner text and the HUD-E21 round summary lines (MVP item 9, display half)](story-040-hud-end-banner-f10-and-round-summary-logic.md) | Logic | Ready | ADR-0021 |
| 041 | [Stage card, own steps and rival steps widgets at the top-centre cluster](story-041-hud-objectives-cluster-widgets.md) | UI | Ready | ADR-0021 |
| 042 | [End banner with round summary, called-off line and lobby line widgets](story-042-hud-end-banner-summary-called-off-lobby-widgets.md) | UI | Ready | ADR-0021 |
| 043 | [PIE proof of the round summary (AC-HUD-27, MVP item 9)](story-043-hud-round-summary-pie.md) | Integration | Ready | ADR-0021 |
| 044 | [HUD-R17 reset and rebuild: discard transients, rebuild persistent elements, no replay](story-044-hud-reset-and-rebuild-logic.md) | Logic | Ready | ADR-0021 |
| 045 | [PIE proof of rebuild without replay across disconnect, restart and RoundEpoch change](story-045-hud-rebuild-no-replay-pie.md) | Integration | Blocked | ADR-0001 |
| 046 | [PIE proof of listen-host parity of the settled view-models and ordered event lists](story-046-hud-host-parity-pie.md) | Integration | Ready | ADR-0001 |
| 047 | [Latency icon with hysteresis, three-step stair shape and host exclusion](story-047-hud-latency-icon.md) | UI | Ready | ADR-0021 |
| 048 | [Interact ring and context prompt on the L0 leaf (HUD-E12, HUD-E13)](story-048-hud-interact-ring-and-context-prompt.md) | Integration | Blocked | ADR-0021 |
| 049 | [Team ping rings, edge rings and sender labels painted by the L0 leaf (HUD-E22)](story-049-hud-team-ping-rings-l0.md) | Integration | Blocked | ADR-0021 |
| 050 | [PIE proof that a concealed ping refusal is owner-only](story-050-hud-ping-refusal-owner-only-pie.md) | Integration | Ready | ADR-0008 |
| 051 | [HUD cue events with timestamps, owner-only 2D UI-bus, never spatialised](story-051-hud-cue-event-stream-and-owner-only-audio-hooks.md) | Logic | Ready | ADR-0021 |
| 052 | [Client-local settings hooks honoured at fixed MVP defaults (scale, text, Reduce motion, high contrast, read hold)](story-052-hud-accessibility-settings-hooks.md) | UI | Ready | ADR-0021 |
| 053 | [PIE proof that no displayed string is a clock, an opponent count or a banned word](story-053-hud-no-clock-no-counts-pie.md) | Integration | Ready | ADR-0008 |
| 054 | [Greyscale and colour-vision captures plus the keyboard-only and gamepad-only round](story-054-hud-accessibility-captures-and-input-pass.md) | UI | Ready | ADR-0021 |
| 055 | [Photosensitivity analysis of the exile and assassinated-victim sequences (AC-HUD-18)](story-055-hud-photosensitivity-removal-sequence.md) | UI | Ready | ADR-0021 |
| 056 | [Layout captures at every resolution and scale, and the A_persist from the real widgets](story-056-hud-layout-evidence-resolutions-and-budget.md) | UI | Blocked | ADR-0021 |
| 057 | [In-session menu coexistence and the display-only L4 status host](story-057-hud-menu-coexistence-and-l4-status-host.md) | Integration | Ready | ADR-0021 |
| 058 | [HUD cost capture against the 0.3 ms / 0.2 ms budget](story-058-hud-performance-capture.md) | Integration | Blocked | ADR-0021 |
| 059 | [Retire the prototype AStealthHUD canvas and the message plumbing](story-059-hud-retire-prototype-canvas-and-message-plumbing.md) | Integration | Ready | ADR-0021 |
| 060 | [Run-and-observe: a 2v2 round played to an objective win with the full retained screenshot set](story-060-hud-run-and-observe-2v2-evidence.md) | UI | Blocked | ADR-0021 |
| 061 | [MVP exit verification: final 5v5 evidence pass over the Presentation surfaces](story-061-hud-mvp-exit-5v5-presentation-verification.md) | Integration | Blocked | ADR-0021 |

**60 stories: 19 Logic, 26 Integration, 14 UI, 0 Visual/Feel, 1 Config/Data; 11 Blocked.**

## Definition of Done

- Every story implemented, reviewed and closed with `/story-done`
- Every acceptance criterion of design/gdd/hud.md verified
- Logic and Integration stories have passing tests in the test root (`Source/<Module>/Private/Tests/`)
- Visual/Feel and UI stories have retained screenshots in `production/qa/evidence/`
  (a lead sign-off too for Visual/Feel)
- Stories embed Control Manifest version 2026-10-25e and every governing ADR's conditional gate has reported before dependent work is marked Done
