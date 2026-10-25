# SCL-005 evidence: the sample AnimBP's IsLocallyControlled branch removed for every pawn

> **Story**: `production/epics/shared-character-locomotion/story-005-animbp-islocallycontrolled-removal.md`
> **Date**: 2026-10-25 · **Engine**: UE 5.8.4 · **QA plan**: `production/qa/qa-plan-sprint-01-2026-10-26.md` (SCL-005, Logic + Visual)
> **Sign-off**: pending (lead-programmer; technical-artist for the animation result). Recorded by the unattended build agent.

## What changed (Content step, by explicit path over the Unreal MCP)

| Asset | Graph | Before (sample) | After |
|---|---|---|---|
| `Content/Blueprints/ABP_SandboxCharacter.uasset` | `Get_AOValue` | `Select(Index = SandboxCharacter.IsLocallyControlled; true: SandboxCharacter.GetControlRotation; false: SandboxCharacter.GetBaseAimRotation)` feeds the aim-offset delta | `SandboxCharacter.GetBaseAimRotation` wired straight into the delta for every pawn (property access nodes `_1` IsLocallyControlled and `_0` GetControlRotation and the `Select` deleted). `GetBaseAimRotation` is the engine's replicated path (controller view on authority/owner, `RemoteViewPitch` on proxies) |
| `Content/Blueprints/ABP_SandboxCharacter.uasset` | class | parent `UAnimInstance` | parent `UStealthAnimInstance` (`StealthGameplay`, no members yet; scanned by the C++ half) |
| `Content/Blueprints/CBP_SandboxCharacter.uasset` | `CanSprint` | `Select(Index = IsLocallyControlled; true: GetPendingMovementInputVector; false: CharacterMovement.GetCurrentAcceleration)` gives the direction compared with the actor forward | `GetCurrentAcceleration` wired straight in for every pawn (call nodes `_4` and `_23` and the `Select` deleted) |

Only these two packages were saved (`AssetTools.save_assets` with the two explicit paths). Before saving, the never-resave set was checked clean (`is_dirty` false for `PC_Sandbox`, the five `Cameras/` assets and `BP_CrowdCharacter`); `git status Content` afterwards lists only the two files.

**Snapshots**: before `_snapshots/2026-10-25_pre-scl005/ABP_SandboxCharacter.uasset` (SHA-1 `039368e7378676e48eecbf73e07984c21d353950`) and `CBP_SandboxCharacter.uasset` (SHA-1 `98e55e91aee97d9925729a780596a531157e5a1f`), taken with `Copy-Item` before any MCP read; after: the committed `Content/Blueprints/` versions.

## Checklist (QA plan SCL-005)

- [x] **ANIM-V6, compiles on 5.8.4**: `ABP_SandboxCharacter` and `CBP_SandboxCharacter` compiled in the editor after the edit and the reparent with no error (`CBP_SandboxCharacter` keeps its two pre-existing warnings: `ActivatePersistentGlobalCameraRig` / `ActivatePersistentBaseCameraRig` deprecated). Both also load and compile in the headless `-nullrhi` test run and in PIE with no `LogBlueprint` error.
- [x] **Zero local-control reads** in the AnimBP graph (all graphs incl. state machines; no linked layers exist; the parent is native) and `UStealthAnimInstance`: `StealthGame.Locomotion.AnimBpNoLocalControlRead` and `StealthGame.Locomotion.ClassParity.Source` pass (`test-run.txt` in `production/qa/evidence/story-005-animbp-islocallycontrolled-removal/`).
- [x] **Negative control**: the walk of the unmodified sample (`tests/fixtures/scl/sample-animbp-negative-control.txt`, recorded by the first, failing run: `production/qa/evidence/story-004-class-parity-scan/negative-control-run.txt`) still fails: "control detected: UnmodifiedSampleAnimBp" and "control detected: UnmodifiedSampleBlueprints".
- [x] **Launched in PIE** (listen host + 1 client, `L_Prototype`, round running): host frames show the remote player's body (client 1, `BP_CrowdCharacter_C_3`) next to AI crowd bodies, all on the one AnimBP, no animation error: `02-host-remote-player-close.png`, `03-host-remote-player-beside-npc-1.png`, `-3.png`, `01-host-remote-player-and-crowd.png`; `04-client-own-view.png` shows the client's own view (its shadow shows the same held post-strike pose the host renders). The host pawn and one NPC were moved with `ActorTools.set_actor_transform` in the PIE server world to frame the shot; the remote player's pose is the prototype's held post-strike pose after `StealthBot 1 strike_npc`, rendered identically on host and owner.
- [x] **No sample asset resaved in place without a snapshot**; no never-resave asset touched.

## Limits (stated, not hidden)

- A still cannot show motion parity; the removed branch changed the aim-offset source and the sprint-direction source, which are only visible in motion and under aim. The advisory side-by-side playtest (QA plan "Playtest Requirements") and ANIM-V3 host-view parity (stories SCL-039, SCL-044) remain the motion checks.
- `EventBeginPlay` in `CBP_SandboxCharacter` still binds `OnCharacterMovementUpdated` only when `GetLocalRole == SimulatedProxy`, captured once (spike S14). Audited: it is role-keyed, not kind-keyed (on the listen host every non-owned body, NPC or player, has the same role; on a client every other body is a simulated proxy), so it is not a host tell today; the stale binding after a takeover is carried to the possession stories (SCL-006 factory, SCL-028 informant takeover), which must re-run role-dependent setup on role change.
- `CBP_SandboxCharacter` is still parented to the prototype `AStealthCharacter` (whose C++ reads PlayerState); the reparent onto `AStealthPawn` is a later Content step, after which the C++ scan roots already cover the new parent.
