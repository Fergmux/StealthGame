# Spike V1: Enhanced Input edge behaviour on this project's contexts (ADR-0011 verification item V1)

> **Story:** `production/epics/input-interaction-framework/story-008-spike-v1-enhanced-input-edges.md` · **Date:** 2026-10-25 · **Verdict:** **PASS** (scenarios 1, 2, 3, 5, 6 PASS; 4 DEFERRED by design; one control variant outside the six, S3d, FAILS as the ADR predicts and is recorded as a finding)

## Setup

| Item | Value |
|---|---|
| Engine build | 5.8.4-58902052+++UE5+Release-5.8 (CL 58902052) |
| CVar `EnhancedInput.IgnoreHeldDigitalActionKeysOnFlush` | `true` (engine default; the project sets nothing) |
| Contexts under test | `/Game/StealthGame/Input/IMC_Common` (priority 0) and `IMC_Active` (priority 1) from story iif-007; `IMC_Informant` as the swap target; a transient `IMC_SpikeV1Menu` (one UI action on Escape) standing in for ADR-0021's `IMC_Menu` |
| Action | `IA_Interact` (Boolean, no trigger: the implicit default), keyboard `E` and gamepad `Gamepad_FaceButton_Left` |
| Runs | N = 20 per scenario per key (360 presses in all) |
| Harness | `Source/StealthTestHarness/Private/Tests/Spikes/SpikeV1EnhancedInputEdgesTest.cpp`, automation test `StealthSpike.Input.V1EnhancedInputEdges` (throwaway, outside the `^StealthGame.` gate filter). Standalone single-player PIE on `L_Prototype` with `AGameModeBase` (so the prototype pawn's legacy bindings are not loaded), `UEnhancedInputLocalPlayerSubsystem::ClearAllMappings()` then the two project contexts (decision D37: the sample's `IMC_Sandbox` with its Ctrl jog toggle is never added). Key events are simulated through `APlayerController::InputKey`; every step waits 3 frames; context changes use the default deferred rebuild (not `bForceImmediately`). |
| Builder probe | A minimal stand-in for `UPawnCommandClient` (stories 094/012 deliver the real one), mirroring ADR-0011 section 9: Begin on the first `Started`; End on the first `Completed` or `Canceled`, or on an explicit flush; any later terminal edge for that press is swallowed. |
| Raw data | `production/qa/evidence/story-008-spike-v1-enhanced-input-edges/v1-results.json` (one record per press) |

Run: `UnrealEditor-Cmd.exe StealthGame.uproject -ExecCmds="Automation RunTests StealthSpike.Input.V1; Quit" -unattended -nullrhi -stdout` → `Result={Success}`, 360 runs.

## Results (per press; every one of the 20 runs per row gave the identical counts)

| Scenario | Key | Started | Completed | Canceled | Builder Begin / End / swallowed | Verdict |
|---|---|---|---|---|---|---|
| (1) Plain press and release | E / pad | 1 | 1 | 0 | 1 / 1 / 0 | PASS |
| (2) Context add while held (`IMC_Informant` at priority 5, ignore-held true) | E / pad | 1 | 1 | 0 | 1 / 1 / 0 | PASS |
| (3a) Context remove while held (`IMC_Active`, ignore-held **true**) | E / pad | 1 | 1 | 0 | 1 / 1 / 0 | PASS |
| (3b) Context remove while held (`IMC_Active`, ignore-held **false**) | E / pad | 1 | 1 | 0 | 1 / 1 / 0 | PASS |
| (3c) Held across remove and re-add of the same mapping (ignore-held true) | E / pad | 1 | 1 | 0 | 1 / 1 / 0 | PASS |
| (3d) control: held across remove and re-add, ignore-held **false** | E / pad | **2** | **2** | 0 | 2 / 2 / 0 | **FAIL** (finding) |
| (4) Window focus loss | — | — | — | — | — | **DEFERRED** (PIE focus is not representative; needs an interactive standalone run) |
| (5) Menu open (menu context at priority 100 + `PlayerController::FlushPressedKeys`) | E / pad | 1 | 1 | 0 | 1 / 1 / 0 | PASS |
| (6a) Builder flush, then swap `IMC_Active`→`IMC_Informant`, release, swap back | E / pad | 1 | 1 | 0 | 1 / **1** / 1 | PASS |
| (6b) Builder flush, swap `IMC_Active`→`IMC_Informant`→`IMC_Active` while held, release | E / pad | 1 | 1 | 0 | 1 / **1** / 1 | PASS |

Keyboard and gamepad behave identically in every scenario.

## Findings

1. **Exactly one terminal edge per release** in every required scenario, for both keys: the engine always ended a
   held press with `Completed` (never `Canceled`, never zero, never two), including on context removal and on the
   menu-style key flush.
2. **The builder's flush yields exactly one End and no re-fired `Started`** (6a, 6b): the End is the builder's own;
   the engine's later `Completed` for the same press arrives and is swallowed (1 per press). This confirms ADR-0011
   section 9's idempotent-End rule is needed, not just belt and braces: without it each flushed press would produce
   two Ends. The swallow stays mandatory regardless of this PASS.
3. **`bIgnoreAllPressedKeysUntilRelease` is what prevents a re-fired Begin** when a context holding a held key is
   removed and re-added (3c true: 1 Started; 3d false: a second `Started` and a second Begin on the re-add, i.e. a
   second Begin/End pair for one physical press). ADR-0011 already requires context changes to set the flag; this is
   the evidence for that rule. Stories 013 and 071 must keep it set on every add and remove.
4. Adding an unrelated higher-priority context while held (2) did not interrupt the press: `Triggered` kept firing
   until release (8 per press vs 4 for a plain press). Removing the context that maps the held key (3a/3b) ended the
   press about two frames later (deferred rebuild) with one `Completed`, and the physical release then produced
   nothing.

## Limits of this spike

- The menu scenario (5) simulates the menu with a higher-priority context and `FlushPressedKeys`; the real Common UI
  `UStealthMenuStack::PushMenu` path (activatable `IMC_Menu`, input-mode change) is spike V5, story iif-011.
- Focus loss (4) is DEFERRED to an interactive standalone run, as the story specifies (not BLOCKED).
- Simulated key events through `InputKey` bypass the Slate/viewport routing; the edges measured are those of the
  Enhanced Input pipeline, which is what ADR-0011 relies on.
- The probe is throwaway; the production builder is `UPawnCommandClient` (stories 094 and 012).

## Consequence

V1 reports **PASS**. Stories 013 (FlushSendsEnd) and 071 (ContextSwapOnStateAndPossession) are no longer blocked by
V1 (071 still waits for V2, story 009). No ADR amendment is needed.
