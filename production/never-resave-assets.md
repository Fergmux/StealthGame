# Never-resave assets (ADR-0009 §4)

Assets the UE 5.8 editor must not resave. Resaving the sample's GameplayCameras
assets in 5.8 broke the CameraDirector import on 2026-10-24; they were restored
from `_snapshots/2026-10-24_sample-blueprints-pre-resave/`. Never lock these for
editing; a pre-commit check (to be installed with the ADR-0008 gate) rejects a
commit that modifies them.

| Asset path | Reason |
|---|---|
| Content/Blueprints/Cameras/CameraAsset_SandboxCharacter.uasset | 5.8 resave broke CameraDirector import (restored from snapshot) |
| Content/Blueprints/Cameras/CameraDirector_SandboxCharacter.uasset | Same camera chain; do not resave |
| Content/Blueprints/Cameras/CameraRigPrefab_BasicThirdPersonBehavior.uasset | Restored from snapshot |
| Content/Blueprints/Cameras/CameraRig_BaseDefaults.uasset | Restored from snapshot |
| Content/Blueprints/Cameras/CameraRig_CollisionOffset.uasset | Restored from snapshot |

Rule: sample assets are never edited in place; duplicate or reparent with a
`_snapshots/` copy first (active.md, ADR-0009 §6).
