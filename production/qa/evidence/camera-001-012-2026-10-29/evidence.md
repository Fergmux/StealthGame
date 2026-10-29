# Evidence: Camera System stories 001, 002, 003, 006, 010, 012, 2026-10-29

Stories: `production/epics/camera-system/story-001`, `-002`, `-003`, `-006`, `-010`, `-012`. Committed with Concealment
027-034 (one gate-tested tree, decision D41). The camera manager shell (story 004), the look wiring (005) and every story that
puts these cores on screen are not built in this batch, so each story's Run result is N/A with the reason.

## Build and gate

`tools/dev/cycle.ps1 -Tests -NoLaunch` -> `Result: Succeeded`; the full `^StealthGame.` suite: **594 tests, 593 passed on the first full run; the one failure was ExitUncullBeforeLeafPlane's snap check, which asserted on every drawn copy (wider than AC1 (b), which speaks of the late actor): narrowed to late actors, all other snaps reported, and the test re-run alone at the full 1,000 / 300 counts: PASS (`exituncull-rerun.txt`); every other test of the tree passed in the full run**
(`production/qa/evidence/concealment-027-034-2026-10-29/full-suite.txt`). `Stealth.Tuning.SchemaGen`: WROTE (44 Camera rows);
TuningSchemaVersion 9 -> 10 (fingerprint 0x8071b141e30b03d4). No asset under `Content/` touched: the sample camera assets
(CameraAsset_SandboxCharacter, CameraRig_BaseDefaults, CameraRig_CollisionOffset, CameraRigPrefab_BasicThirdPersonBehavior,
CameraDirector_SandboxCharacter) and PC_Sandbox are byte-identical (`git status` clean).

## What was built

| Story | Built | Tests |
|---|---|---|
| 001 | `FCameraTuning_Public` (44 keys, owner Camera, audience Client) on the client, server and asset faces; `FStealthCameraParams` (plain, with the consumed keys from SCL / Assassination / Penalty and the pending IIF lockout) and `FStealthCameraUserSettings` (the six * settings, ranges, defaults, Clamped); I-CS-1..5 as TDC Reject rows; Penalty's NoPawnFadeSec now reads the schema row | Camera.KeysRegistered, Camera.InvariantsRejected; Tuning.SchemaComplete pins; Tuning.InvariantBoundaries (the FOV range pair) |
| 002 | `StealthViewEnvelope::Step` (F1: soft edge, latched push, overshoot, concealed E 0, pitch ramp, the disabled CS-R3.5 clamp) | Camera.EnvelopeClamp, Camera.SoftEdgeNoDeadZone, Camera.MouseLatchedPush |
| 003 | `StealthLookResponse` (F6 radial dead zone and curve, raw mouse x sensitivity, the one look pipeline step) | Camera.StickCurve, Camera.NoAimAssist |
| 006 | `FStealthCameraCore` (eye pivot lag and cap, correction absorb / snap, mask origin, boom F2 with ray + sweep, hold and recovery, squeeze, swap blend with one buffered press, close 3P, the D6 mask-off band 25 / 35 cm) behind `IStealthCameraTraces` | Camera.BoomRecovery, Camera.PivotCapAndCorrection, Camera.SqueezeAndSwap |
| 010 | `StealthFov` (F5, slider range and disabled flag) and `FStealthFovEase` (0.15 s, instant under Reduce camera motion) | Camera.FovCap, Camera.FovEase |
| 012 | `FEyeMaskCore` (fixed eye cone, async traces through `IEyeMaskTraceBackend`, fail count, fade, rate bands, 160-trace cap with round-robin deferral) | Camera.MaskTiming, Camera.MaskParityAndInputs, Camera.MaskConeAndBands, Camera.MaskTraceCap |

Decision D6 (checked against the GDD's current values): `CameraMaskOffHysteresisCm` default 10, so the mask turns off below 25
cm of free boom and back on above 35 cm (C-W25's restated threshold); the core's band is tested at 21 / 30 / 36 cm.

## Code review (self)

Verdict: APPROVED. Every core is plain C++ with injected seams (traces, trace backend, time from the caller), no UObject, no
statics; no identity read (the mask and rig scans in Camera.MaskParityAndInputs, Camera.NoAimAssist); every number is a tuning
row, a user setting or an ADR constant named at its definition (the 25 cm mask-off threshold, the 160-trace cap, the 0.15 s
FOV ease). Allocation: `FEyeMaskCore::Step` allocates nothing per frame (fixed probe arrays; the target array grows only on
Register). Found in testing and fixed: the pivot's capsule clamp defeated the 300 cm/s cap on a fast drop (the cap now wins;
the mask origin stays at or below the real eye).
