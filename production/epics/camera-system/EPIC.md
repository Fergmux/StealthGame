# Epic: Camera System

> **Layer**: Core
> **GDD**: design/gdd/camera-system.md
> **Architecture Module**: Camera (client) in `StealthGameplay/Core` (custom `APlayerCameraManager`, eye-origin visibility mask, aim ray, view state)
> **Status**: Ready
> **Stories**: 25 stories (see the Stories table below)
> **Engine Risk**: HIGH (highest among governing ADRs; engine 5.8, installed 5.8.4)
> **Control Manifest**: 2026-10-25e
> **Build Order**: 10 of 19 (layer order; the risk-first sequence is in the Build Order and Gates section)

## Overview

The Camera System is a client-local presentation layer that turns the owner's body into a view. It owns the two view modes (first and third person) with a local-only toggle and blend (nothing about camera mode is ever replicated), the third-person boom with collision and shoulder swap, the first-person eye camera and own-body presentation, the view envelope (-60/+40 degrees in both modes relative to body facing; the view never leaves the head-look clamp), the eye-origin visibility mask that makes the third-person view obey the body's line of sight (it hides mesh, shadow and ray-tracing presence of pawns the eye could not see, reading geometry only, with alpha 0 counting as not rendered for IIF, SCL and the strike hint), the shared `GetAimRay()` that Attack, the Strike hint and Pings consume, `GetViewState()` for the HUD and markers including no-pawn view states, the audio listener at the eye pivot, field-of-view and motion-comfort settings, and behaviour during actions, traversal, concealment, elimination and round end.

Per ADR-0015 the dither uses the custom primitive data slot `CPD_EyeMaskAlpha` with rate bands at every range (never a cutoff, never a per-pawn MID), and the eye-mask trace cost is bounded at 60 pawns by time slicing beyond 60 m. This is Pillar 2's guard: the third-person camera sees no more than body line of sight.

## Governing ADRs

| ADR | Decision Summary | Engine Risk |
|-----|------------------|-------------|
| ADR-0003: Replication, relevancy and the concealment cull hybrid | Uniform pawn net configuration for NPC and player, quantised server cull for non-allies plus a client-local hide, allowlist audit of every replicated property. | HIGH |
| ADR-0015: Camera system and LOS envelope | Client-local 1P/3P camera, -60/+40 view envelope, eye-origin visibility mask via a custom-primitive-data dither, one shared GetAimRay. | HIGH |
| ADR-0021: UI/HUD layering, CommonUI and Enhanced Input | UMG layer/ZOrder stack drawn after post-process, Common UI for menus only, client-local settings hooks, ping rings in HUD L0. | HIGH |
| ADR-0022: Rendering configuration and performance budgets | Lumen/Nanite/VSM configuration and CPU/GPU frame budgets for the RTX 3060-class 1080p60 target (GPU 14.6 ms). | HIGH |

All governing ADRs are Accepted (user, 2026-10-25). Several are conditionally accepted; the outstanding gates are in Build Order and Gates below.

## GDD Requirements

8 of 8 requirements are covered by an Accepted ADR. Untraced requirements: None.

| TR-ID | Requirement | ADR Coverage |
|-------|-------------|--------------|
| TR-camera-001 | Local-only toggle; nothing replicated about camera mode | ADR-0003, ADR-0015 ✅ |
| TR-camera-002 | View envelope −60/+40 in both modes relative to body facing | ADR-0015 ✅ |
| TR-camera-003 | 3P line-of-sight rule: boom geometry + eye-origin visibility mask hiding mesh, shadow and RT | ADR-0015, ADR-0022 ✅ |
| TR-camera-004 | Mask reads geometry only; α = 0 counts as not rendered for IIF, SCL and the strike hint | ADR-0015 ✅ |
| TR-camera-005 | One shared `GetAimRay` | ADR-0015 ✅ |
| TR-camera-006 | `GetViewState` for HUD/markers; no-pawn view states | ADR-0015, ADR-0021 ✅ |
| TR-camera-007 | Audio listener at the eye pivot | ADR-0015 ✅ |
| TR-camera-008 | Eye-mask trace cost bounded at 60 pawns (time slicing beyond 60 m) | ADR-0015 ✅ |

## Prototype Disposition

The code in `Source/StealthGame` is marked PROTOTYPE (`production/first-playable-report.md`). Production code follows ADR-0007 modules, and nothing is promoted until the ADR that governs it is built against.

**Replaces** the prototype camera in `StealthCharacter.cpp`: `ToggleCameraMode`/`bFirstPerson` (V key), the sample camera setup, and the `StealthDebugCam <0|1|2>` console command. First person currently shows a sliver of the player's own shoulder at the screen edge (known issue 10). There is no view envelope, no eye-origin mask and no `GetAimRay`: the prototype attack uses range and cone targeting. The sample's camera assets were resaved once in the prototype run (it broke an import) and were restored from the snapshot; never let the editor save them in 5.8.

## Build Order and Gates

- **Depends on (epics)**: input-interaction-framework (look input), shared-character-locomotion (eye pivot, body facing), tuning-data-config (client keys); consumed by IIF (`GetAimRay`), SCL tiering (eye-mask flag), Markers and HUD (`GetViewState`)
- **Gates and risks**: ADR-0015 is unconditional; ADR-0022 R1-R3 (rendering budget on 3060-class hardware) apply to the mask's shadow, Lumen and ray-tracing removal. Rendering is HIGH risk (hidden skinned-mesh shadow and RT behaviour, Substrate state unknown). The sample camera assets must never be resaved in the editor (first-playable report). The camera toggle and traversal replication are the second risk-first build-order step. The producer review flagged the days-per-AC ratio (0.10) as low: re-verify the AC type mix before story breakdown.
- **Estimate**: about 4 bottom-up days (human-paced yardstick, `production/estimates/2026-10-25_mvp-rebaseline.md`, Method B work packages); planning figure about 2.4 days after the uniform 0.6 calibration. Low confidence: no sprint history exists, so run the 2-week calibration spike and re-baseline.
- **Acceptance criteria in the GDD**: about 40 (counted as GIVEN-form criteria in the Acceptance Criteria section).
- **MVP build order**: risk-first per `design/gdd/systems-index.md`: graybox map and mannequin-variant pool first, then camera toggle and traversal replication, then the Meshy pool swap, then the generator.

## Stories

Granularity `balanced` (2-4 acceptance criteria per story; GDD criteria with several independent cases are split into sub-criteria; criteria derived from an ADR where the GDD has none are labelled "derived"). Story 011 is the mask spike (ADR-0015 OQ5/OQ10, ADR-0022 RC-3); 015 and 022 are Blocked on it, and 022 also on a RTX 3060-class machine and ADR-0022 R1-R3. QL-STORY-READY: 10 ADEQUATE (001, 003, 005, 010, 012, 013, 014, 018, 019, 022), 15 GAPS accepted as written (002, 004, 006, 007, 008, 009, 011, 015, 016, 017, 020, 021, 023, 024, 025; their QA Test Cases read "not yet defined", and the gap notes are in each story), 0 INADEQUATE. TR-camera-??? marks stories whose requirement has no registry entry (001, 003, 009, 010, 019).

| # | Story | Type | Status | ADR |
|---|-------|------|--------|-----|
| 001 | Camera tuning keys and invariants | Logic | Ready | ADR-0015 |
| 002 | View envelope pure function | Logic | Ready | ADR-0015 |
| 003 | Stick look response and no aim assist | Logic | Ready | ADR-0015 |
| 004 | Camera manager shell, controller defaults and remote-controller POV | Integration | Ready | ADR-0015 |
| 005 | Look wiring into control rotation | Integration | Ready | ADR-0015 |
| 006 | Eye pivot, boom and collision | Logic | Ready | ADR-0015 |
| 007 | ViewBlock channel and collision profiles | Config/Data | Ready | ADR-0015 |
| 008 | Toggle blend, Reduce camera motion and remembered mode | Integration | Ready | ADR-0015 |
| 009 | First-person own-head hide | Visual/Feel | Ready | ADR-0015 |
| 010 | Field of view | Logic | Ready | ADR-0015 |
| 011 | Camera mask spike (OQ5 / OQ10) | Integration | Ready | ADR-0015 |
| 012 | Eye mask core | Logic | Ready | ADR-0015 |
| 013 | Mask on the live world and IsEyeMaskVisible | Integration | Ready | ADR-0015 |
| 014 | Concealed and informant view | Integration | Ready | ADR-0015 |
| 015 | Mask apply path | Visual/Feel | Blocked (story 011 spike) | ADR-0015 |
| 016 | Aim ray | Integration | Ready | ADR-0015 |
| 017 | View state, neutral view and published fade | Integration | Ready | ADR-0015 |
| 018 | Dying hold and ViewRelease | Integration | Ready | ADR-0015 |
| 019 | Grade hooks | Integration | Ready | ADR-0015 |
| 020 | Audio listener at the eye | Integration | Ready | ADR-0015 |
| 021 | Camera mode not observable | Integration | Ready | ADR-0015 |
| 022 | Mask performance acceptance on 3060-class hardware | Integration | Blocked (story 011 spike; 3060-class capture, ADR-0022 R1-R3) | ADR-0015 |
| 023 | Playtests: scout parity, read parity and own-pawn finding | Integration | Ready | ADR-0015 |
| 024 | Presentation include check and Tell Matrix review | Logic | Ready | ADR-0015 |
| 025 | Retire the prototype camera | Integration | Ready | ADR-0015 |

## Definition of Done

- Every story implemented, reviewed and closed with `/story-done`
- Every acceptance criterion of design/gdd/camera-system.md verified
- Logic and Integration stories have passing tests in the test root (`Source/<Module>/Private/Tests/`)
- Visual/Feel and UI stories have retained screenshots in `production/qa/evidence/`
  (a lead sign-off too for Visual/Feel)
- Stories embed Control Manifest version 2026-10-25e and every governing ADR's conditional gate has reported before dependent work is marked Done
