# Smoke check: Camera collision profiles (cs-007, ADR-0015 section 4.1)

- **Date**: 2026-10-29
- **Story**: `production/epics/camera-system/story-007-viewblock-channel-collision-profiles.md` (TR-camera-003, TR-camera-004)
- **Type / gate**: Config/Data, ADVISORY
- **Config change**: `Config/DefaultEngine.ini` `[/Script/Engine.CollisionProfile]`: Pawn and CharacterMesh add Camera Ignore;
  new StealthKit, StealthProp (Block / Block), StealthFoliage, StealthDecoration (Ignore / Ignore), StealthGlass (ViewBlock
  Ignore, Camera Block). ViewBlock stays ECC_GameTraceChannel2 (`StealthCollision::ViewBlock`).

## Tests

| Test name | Covers | Result |
|---|---|---|
| `StealthGame.Camera.CollisionProfiles` | AC1: one ViewBlock trace channel, default Block, on the constant's channel; AC2: every table row's ViewBlock / Camera response, explicit rows written in CustomResponses | Success (full suite 2026-10-29, 597 / 597) |
| `StealthGame.Camera.CollisionProfileValidator` | AC3: a drifted kit mesh is Invalid naming the asset and `Camera: Ignore`; matching, ungoverned and seen-through Custom bodies pass | Success (full suite 2026-10-29) |

Listed in `tests/smoke/critical-paths.md` (Config Integrity, entry 9).
