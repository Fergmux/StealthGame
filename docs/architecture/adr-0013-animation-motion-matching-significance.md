# ADR-0013: Animation: motion matching and significance budget

## Status
Accepted — 2026-10-25, by the user (blanket acceptance of ADR-0001..0023, recorded by the orchestrator). **Conditional:** every spike or verification item named in the earlier status text below must still report before implementation depends on it; a failed spike is handled by a superseding ADR, not by editing this one.

*Earlier status text (kept for its gating conditions):* Proposed — awaiting user acceptance. Acceptance is gated on spike **R3 run on RTX 3060-class target hardware** (architecture.md LP concern 9, AQ-18) and on the verification items ANIM-V1..V6 below; it also needs ADR-0001, ADR-0003, ADR-0004, ADR-0007, ADR-0008 and ADR-0010 Accepted. All current performance evidence comes from an RTX 5090 / 16-core desktop and is **not** acceptance evidence.

## Date
2026-10-24

## Last Verified
2026-10-24 (engine claims read against the UE 5.8.4 install at `C:/Program Files/Epic Games/UE_5.8`, including the PoseSearch and AnimationBudgetAllocator plugins; design against `shared-character-locomotion.md` R10-R12, R18, F7, F8, F11 and art bible 8.x.8, "In Review, reconciled 2026-10-24")

## Decision Makers
Autonomous design pipeline (`/architecture-decision`, rigor full, review mode full); technical-director (TD-ADR) and unreal-specialist validation; acceptance reserved for the user.

**Gate record (2026-10-24):** TD-ADR CONCERNS → REVISED; engine validation BLOCKING (authority root-motion timing under external control) → REVISED (authority keeps every pawn at rate 1). Details under `## Validation Criteria` → "Gate results".

## Summary
Fifty pawns share one motion-matched body, and animation cost must stay within 4 ms while no animation setting, rate or sound ever differs by controller kind, team or participant state. This ADR fixes one AnimBP / PoseSearch database set / chooser set, a client-local significance subsystem that tiers every rendered pawn A/B/C/D by figure screen height and Camera's eye-mask flag only, with caps 8/14/28 and a client-local random tie key, applied on non-authority clients through the engine's **external tick-rate control** and the motion-matching node's time-based `SearchThrottleTime` (authority processes keep every pawn at full rate so `ServerMove`-driven root motion stays correct) — with engine URO and the AnimationBudgetAllocator **not** used, because 5.8.4's default URO rule ticks `APlayerController`-instigated pawns at full rate (a player-keyed tell on the listen host); it also fixes the 4 ms definition, equal-for-all fallbacks, one step clock and one footstep concurrency for every pawn.

## Engine Compatibility

| Field | Value |
|-------|-------|
| **Engine** | Unreal Engine 5.8 (installed 5.8.4, CL 58902052) |
| **Domain** | Animation (with Audio for footsteps) |
| **Layer** | Core (architecture.md layer map: Shared Character & Locomotion, "Animation & significance (SCL R10-R12)") |
| **Knowledge Risk** | HIGH — Animation is HIGH risk for 5.4-5.8 (`VERSION.md`; 5.8 removed deprecated animation symbols); motion matching (PoseSearch), Chooser and the animation budget allocator are not covered by `docs/engine-reference/unreal/modules/animation.md`; Audio is MEDIUM (5.8 WASAPI default, relative-render-cost voice limit on by default) |
| **References Consulted** | `docs/engine-reference/unreal/VERSION.md`, `modules/animation.md`, `modules/audio.md`, `breaking-changes.md`, `deprecated-apis.md`; `docs/architecture/architecture.md` (Engine Knowledge Gap Summary, Core layer "Animation & significance", Data Flow §1-2, Required ADRs row ADR-013, Performance budget, Open Questions AQ-07, AQ-18); ADR-0001 §3, ADR-0003 §5.8, ADR-0010 (E4, §2, §4), ADR-0012 §4; art bible 5.4, 8.x.7, 8.x.8, 8.x.14, 8.x.16; engine and plugin source read this session (table below) |
| **Post-Cutoff APIs Used** | PoseSearch motion matching node `FAnimNode_MotionMatching::SearchThrottleTime`, `bShouldSearch`, `bUseInertialBlend`, `PoseReselectHistory` [V `PoseSearch/Source/Runtime/Public/PoseSearch/AnimNode_MotionMatching.h:115-154`]; PoseSearch and Chooser plugins at `VersionName 1.0` with no Beta/Experimental flag in their `.uplugin` [V]; `USkinnedMeshComponent` external tick-rate control (`EnableExternalTickRateControl`, `SetExternalTickRate`, `EnableExternalEvaluationRateLimiting`, `EnableExternalInterpolation`, `EnableExternalUpdate`, `SetExternalInterpolationAlpha`, `SetExternalDeltaTime`) [V `SkinnedMeshComponent.h:1011-1039`; consumed in `SkeletalMeshComponent.cpp:1920-1969, 2892-2909, 4499`]; `au.EnableRelativeRenderCostVoiceLimit` (default 1) [V `AudioDevice.cpp:203-208`] |
| **Verification Required** | **R3 (gate)** on an RTX 3060-class PC at 1080p, 60 characters (owner included; `NpcCount` 50 since 2026-10-25), Insights trace 60 s after 10 s warm-up: summed animation time (definition §5) p95 ≤ 4.0 ms, game-thread share p95 ≤ 2.0 ms, tier A ≤ 8, CMC proxy time and listen-host server CMC time reported separately; plus the same capture on the listen host with 9 remote clients. **ANIM-V1** external tick-rate control drives Game Animation Sample motion matching without visible pops at tier transitions (inertialised) and without breaking montages, root motion or notifies. **ANIM-V2** `SearchThrottleTime` bound from an AnimInstance property is honoured per instance (tier-B search count over 10 s equal at 30 and 120 fps ± 5 %, AC-SCL-44). **ANIM-V3** listen-host view parity: per-frame pose-update interval distributions of remote-player pawns and NPC pawns as rendered **on the host** do not differ (two-sample KS, α = 0.001), given ADR-0010 E4 (server pose of remote-controlled pawns advances with `ServerMove`). **ANIM-V4** the step clock holds visible plants within ±50 ms at tiers A-C (SCL OQ12). **ANIM-V5** footstep audibility parity under `au.EnableRelativeRenderCostVoiceLimit = 1` with 50 walking pawns: per-pawn played-step ratio independent of kind (χ², α = 0.001). **ANIM-V6** the Game Animation Sample's AnimBP and Blueprints compile on 5.8.4 after the deprecated-symbol removals and the database pruning (R10) |

**Engine facts this ADR relies on** ([V] read in 5.8.4 source this session; [D] engine reference; [U] unverified, owned by the named item):

| # | Fact | Tag |
|---|---|---|
| E1 | **Engine URO is player-keyed.** `FAnimUpdateRateManager::AnimUpdateRateSetParams` computes `bHumanControlled = Owner->GetInstigatorController<APlayerController>() != nullptr` and, for such pawns, sets update and evaluation rate 1 whether rendered or not; other pawns are throttled by distance factor. On the listen host every remote player's pawn has a server-side `APlayerController`, so with URO on, the host renders players at full rate and NPCs throttled | [V] `Engine/Private/Components/SkinnedMeshComponent.cpp:205-210, 283-305` |
| E2 | Setting `bEnableUpdateRateOptimizations = false` plus `EnableExternalTickRateControl(true)` bypasses that rule: tick/evaluation/interpolation then follow `ExternalTickRate`, `bExternalUpdate`, `bExternalEvaluationRateLimited`, `bExternalInterpolate` and `ExternalInterpolationAlpha`; this is exactly how the AnimationBudgetAllocator drives components | [V] `SkinnedMeshComponent.h:983-1039`; `SkeletalMeshComponent.cpp:1920-1969, 2892-2909, 4499-4505`; `AnimationBudgetAllocator.cpp:478, 1195-1196, 1377-1378` |
| E3 | `EVisibilityBasedAnimTickOption` = `AlwaysTickPoseAndRefreshBones`, `AlwaysTickPose`, `OnlyTickMontagesAndRefreshBonesWhenPlayingMontages`, `OnlyTickMontagesWhenNotRendered`, `OnlyTickPoseWhenRendered` | [V] `SkinnedMeshComponent.h:96-115` |
| E4 | On the server, `ACharacter::UpdateMeshAllowAutonomousTickPose` sets `bOnlyAllowAutonomousTickPose` for a pawn whose remote role is `AutonomousProxy` with a net connection: its pose ticks only from the CMC as `ServerMove`s arrive (`MoveAutonomous` → `TickCharacterPose`), not from the mesh tick; ADR-0010 leaves this to the engine | [V] `Character.cpp:2443-2452`; `CharacterMovementComponent.cpp:10695-10705`; `SkeletalMeshComponent.cpp:1910`; ADR-0010 E4 |
| E5 | `FAnimNode_MotionMatching::SearchThrottleTime` = "minimum amount of time to wait between searching for a new pose segment" (seconds, pin-exposable); `bShouldSearch = false` searches only when the continuing pose is invalid | [V] `AnimNode_MotionMatching.h:132-154` |
| E6 | AnimationBudgetAllocator: `IAnimationBudgetAllocator::SetComponentSignificance(Component, Significance, bNeverSkip, bTickEvenIfNotRendered, bAllowReducedWork, bForceInterpolate)`, a load-adaptive `BudgetInMs`, reduced-work and emergency modes; requires `USkeletalMeshComponentBudgeted`; its default significance iterates the world's PlayerControllers | [V] `IAnimationBudgetAllocator.h:27-88`; `AnimationBudgetAllocatorParameters.h:18-157`; `SkeletalMeshComponentBudgeted.cpp:110` |
| E7 | `EMaxConcurrentResolutionRule` includes `StopFarthestThenOldest`, `StopLowestPriority`, `StopQuietest`; `FSoundConcurrencySettings::bLimitToOwner` exists; `au.EnableRelativeRenderCostVoiceLimit` defaults to 1 | [V] `Sound/SoundConcurrency.h:31-52, 93`; `AudioDevice.cpp:203-208` |
| E8 | 5.8 removed deprecated animation symbols from 5.0-5.6 | [D] `breaking-changes.md` |
| E9 | Motion matching cost per search at this database size is unknown | [U] R3 |
| E10 | Under external tick-rate control, `ShouldTickAnimation` returns `bExternalUpdate` and `TickPose` advances by `ExternalDeltaTime`, **including** the CMC's `TickCharacterPose` calls; `ServerMove`s run during net-driver dispatch before any tick group, so on authority external control would advance a remote-controlled pawn's root-motion montage by the wrong dt or freeze it (the drift ADR-0010 E4 warns about) | [V] `SkeletalMeshComponent.cpp:1921-1924, 1941-1950`; `CharacterMovementComponent.cpp:10705, 12027-12052` (TD/engine gate, B1) |
| E11 | `ExternalInterpolationAlpha` defaults to 0 and `ConsumeRootMotion` scales by it; interpolation of skipped frames happens only when evaluation rate-limiting is on; the AnimationBudgetAllocator sets alpha and an accumulated `ExternalDeltaTime` (× `CustomTimeDilation`) every frame; motion matching's `SearchThrottleTime` accumulates the update's delta time | [V] `SkinnedMeshComponent.cpp:514`; `SkeletalMeshComponent.cpp:2896-2911, 4499-4501`; `AnimationBudgetAllocator.cpp:453-459, 477`; `AnimNode_MotionMatching.cpp:217` (TD/engine gate, B2-B4) |
| E12 | The character mesh tick already has the CMC tick as a prerequisite, and simulated-proxy root motion ticks the pose from inside the CMC tick; `ShouldTickPose`'s visibility test uses the engine's `bRecentlyRendered` unless the tick option is `AlwaysTickPose*`; `FAnimNode_MotionMatching::bUseInertialBlend` defaults to false | [V] `Character.cpp:155`; `CharacterMovementComponent.cpp:2798, 2923`; `SkeletalMeshComponent.cpp:1909-1917`; `AnimNode_MotionMatching.h:~146` (TD/engine gate, B5-B7) |

## ADR Dependencies

| Field | Value |
|-------|-------|
| **Depends On** | ADR-0001 (§3 significance-only animation LOD invariant; one pawn class), ADR-0003 (re-open creates a new client actor; forced updates of `FActionState`/`TraversalState`), ADR-0004 (SCL tier/footstep keys as `Client`-audience tuning, invariants I-SCL-3, I-SCL-11), ADR-0007 (module placement, client-local presentation code), ADR-0008 (T1/T2 tests, perf capture harness), ADR-0010 (`AStealthPawn`, `UStealthMovementComponent`, E4 server pose ticking, published `IsInTraversal`/`IsRunning`). ADR-0015 (Camera: `IsEyeMaskVisible`) is a soft dependency: until it exists the rendered flag falls back to the engine's `WasRecentlyRendered` **in development builds only** |
| **Enables** | ADR-0017 (Concealment clip scheduling uses this AnimBP's action states and the step-clock suppression latch), ADR-0018 (one fall montage driven from `FDeathState`), ADR-0022 (the CPU budget table's 2.0 ms animation GT line and the GPU skinning line rely on this tiering) |
| **Blocks** | Shared Character & Locomotion animation stories (AnimBP reparenting and pruning, tier subsystem, step clock); the mass-production gate (art bible 8.x.14: R3 is one of the three gating spikes); Diegetic Audio footstep implementation |
| **Ordering Note** | R3 on 3060-class hardware must run before Accepted (AQ-18 names no owner yet: technical-artist runs, performance-analyst signs, per art bible 8.x.14). The tier subsystem and step clock can be built against the RTX 5090 machine meanwhile; their parity tests do not depend on hardware |

## Context

### Problem Statement
The prototype runs the Game Animation Sample's motion matching on every pawn with engine defaults: 1 player + 40 NPCs at GT 9.5 ms and 50 characters at 89.6 fps / GT 11.2 ms on an RTX 5090 (first playable report) — fast hardware, no crowd AI, no queue, no concealment probes, no 10 real connections, and no throttling. On the 3060-class reference the same setup will not fit the 2.0 ms animation game-thread share of the 12 ms listen-host CPU budget. Throttling is required, and every throttling mechanism the engine offers is a potential Pillar-1 tell: the default URO rule in 5.8.4 explicitly ticks `APlayerController`-instigated pawns at full rate (E1), and the AnimationBudgetAllocator's default significance is computed from player controllers and its rates adapt to load and internal ordering (E6). SCL R11 requires a deterministic tiering keyed by screen height and the eye-mask "rendered" flag only, with caps and a client-local tie key; R18 requires footsteps from one clock independent of animation tier; R12 requires a 4 ms budget whose definition (game thread or summed) is still open (SCL OQ4, art bible 8.x.15). This ADR fixes the mechanism.

### Constraints
- One pawn class, one AnimBP, kind-blind code (ADR-0001 §1/§3, SCL R1, R10); no AI-keyed animation LOD, budget, audio concurrency or tie-break.
- Fallbacks lower fidelity for every pawn equally; never per pawn, team or viewer (art bible 8.x.16 "never used as a fallback").
- Tier thresholds, caps, hysteresis, hold, search intervals, D-visible rate and footstep cadence are existing SCL tuning keys (`Client` audience, Round latch, ADR-0004); this ADR adds **no** tuning key (a new key would be a schema change).
- Characters are classic non-Nanite skeletal meshes with 4 LODs, one `ACharacter` each, not instanced, no Mass (art bible 8.x.4, 8.x.9).
- The listen host is both server and a player's client; its rendering must obey the same parity rules as any client (SCL R11.5, R14).
- Headless (`-nullrhi`) automation must be able to test tier logic without a renderer (ADR-0008).

### Requirements
- One AnimBP class, one PoseSearch database set, one chooser set, one look-at solver, one pose set; `ActionPose`-driven states; one fall sequence for every victim (SCL R10, ASN-R17).
- Tiers A/B/C/D-visible/D-culled from `h` (F7) and the eye-mask flag only; promotion immediate, demotion after `AnimTierMinHoldSec`; caps 8/14/28 with overflow down one tier; ties by a client-local random `TierTieKey`; owner's pawn tiered like any other; no rendered pawn unposed or frozen (SCL R11, F7, F8).
- Search cadence by time, not by update count (R10, R11.3).
- Server (listen host or dedicated) poses every pawn with identical settings; gameplay never depends on the host's view (R11.5).
- `C_anim ≤ 4.0 ms` at 60 characters on the reference hardware with an explicit definition (R12, F8).
- One step clock from replicated movement, independent of tier, with identical audio concurrency and voice-limit treatment for every pawn (R18, F11; TD concern 8).

## Decision

### 1. One animation asset set

- **AnimBP.** One AnimBP class derived from the Game Animation Sample's character AnimBP, parented to a C++ base `UStealthAnimInstance` (`StealthGameplay/Core/Animation`). It is assigned on the single Blueprint child of `AStealthPawn` (ADR-0010 §2). No other AnimBP, linked anim layer set or post-process AnimBP is selected per pawn; linked layers, if used, are the same instances on every pawn.
- **Motion matching.** One PoseSearch database set and one chooser set (Chooser tables select databases by gait/state inputs that come only from movement, `HeadLook`, `TraversalState` and `FActionState`). The sample's sprint, crouch, jump and unused overlay databases are removed (SCL R10); the pruned set covers walk 112-140 cm/s (play rate 0.9-1.1 plus stride warping), run 450 cm/s, idle variants, starts, stops, pivots, the shared contact reaction and traversal.
- **Inputs the AnimBP may read** (exhaustive): replicated movement (velocity, acceleration from CMC, movement mode), `HeadLook`, `RemoteViewPitch16`, `TraversalState`, `FActionState`, `FDeathState`, the step-clock phase (§6), the per-pawn idle timers (seeded identically for every pawn kind), the tier feature flags (§3) and `EstimatedServerNow()` for clip seeking (ADR-0002). It **must not** read controller, `IsPlayerControlled`, `IsLocallyControlled` (except the owner-local first-person head hide, SCL R9), `PlayerState`, team or Roster state. `AC-SCL-01` scans the AnimBP graph and `UStealthAnimInstance`.
- **Action poses and the fall.** `FActionState.Pose` drives the AnimBP states; Strike/Counter clips are scheduled from `ContactServerTime`/`WarpPoint` and seeked by `EstimatedServerNow()` (ADR-0018 owns timing); **one** fall montage plays from `FDeathState.DeathServerTime` for every victim kind, seeked so late arrival or relevancy re-open joins at the right frame (ASN-R17; ADR-0018 owns removal and possession through the fall). Presentation is state-derived and idempotent; no multicast RPC exists (ADR-0003).

### 2. Significance: `UAnimSignificanceSubsystem` (client-local, every process that renders)

A `UWorldSubsystem` (`StealthGameplay/Public/View/Animation/`; created on clients, the listen host and standalone; not on a dedicated server) that owns tiering. It registers one custom `FTickFunction` in `TG_PrePhysics` and adds it as a **prerequisite of every Shared Pawn's CMC tick and mesh tick** (the mesh tick already depends on the CMC, and simulated-proxy root motion ticks the pose from inside the CMC tick, E12; pawns register on `BeginPlay`, unregister on `EndPlay`; a relevancy re-open re-registers the new actor, ADR-0003 §5.8). Pawns whose `AppearanceId` has not resolved have no skeletal mesh (ADR-0012 §4): they are skipped and take no cap slot. Once per frame, before any pawn's animation ticks:

1. **Inputs per pawn** (only these): view-space depth `z` and vertical FOV of the local view (previous frame's final camera), and `IsEyeMaskVisible(Pawn)` from Camera (ADR-0015; α > 0, geometry-only). `h = H_fig / (2 · z · tan(FOV_v / 2))` with `H_fig = 175 cm`, `h = 1` if `z < 30 cm`, `h = 0` if not eye-mask-visible within the last 0.2 s (F7). The engine's `WasRecentlyRendered` is **not** an input (SCL R11.1), except as the development-only fallback while ADR-0015 is unbuilt.
2. **Raw tier** from the thresholds with relative hysteresis `η`: promote at once when `h ≥ T(1 + η)`; demote only when `h < T(1 − η)` and the pawn has held its tier for `AnimTierMinHoldSec`.
3. **Caps** (F8): sort candidates by `h` descending, ties by `TierTieKey` (then by a per-process registration counter only to make the sort total); fill A up to `AnimTierACapCount`, overflow to B; B up to `AnimTierBCapCount`, overflow to C; C up to `AnimTierCCapCount`, overflow to D-visible. Caps override holds. The owner's own pawn takes part like any pawn (R11.4).
4. **`TierTieKey`**: a `uint32` drawn for each pawn **when its client actor is first registered**, from a client-local `FIntStream` (ADR-0006) seeded once per world from the local `IEntropySource`; never derived from spawn index, NetGUID, actor name or registration order, never replicated. A re-opened actor draws a new key.
5. **Apply** the tier (§3) to the pawn's mesh and `UStealthAnimInstance`. The work is O(n log n) for n ≤ 60 pawns, budgeted at ≤ 0.05 ms game thread (inside the animation line).

The subsystem is a thin facade over `FAnimTierCore` (plain C++, no `UWorld`; ADR-0007), which takes `(h, rendered, current tier, held time, TierTieKey)` per pawn plus the frozen client tuning and returns tiers, so AC-SCL-26/28 run headless.

### 3. What each tier does (same table for every pawn)

Engine URO is **off** on every Shared Pawn mesh on every process (`bEnableUpdateRateOptimizations = false`, class default; E1). On **non-authority clients**, every pawn mesh is under **external tick-rate control** (`EnableExternalTickRateControl(true)`, set at `BeginPlay` when `!HasAuthority()`; E2) with `VisibilityBasedAnimTickOption = AlwaysTickPose`, so the engine's `bRecentlyRendered` test is never a gate and external control is the only one (E12, SCL R11.1). On authority processes external control is **off** (§4). Each frame the subsystem applies, per pawn: `SetExternalTickRate(k)`; `EnableExternalEvaluationRateLimiting(k > 1)` and `EnableExternalInterpolation(k > 1)` (interpolation needs rate-limiting on, E11); `EnableExternalUpdate(true)` on the pawn's update frame, else false; `SetExternalDeltaTime(accumulated frame dt × CustomTimeDilation since the last update)` so animation speed and the time-based search throttle are correct (E11); and `SetExternalInterpolationAlpha(1)` at rate 1, else `1 / FramesUntilNextUpdate` (root motion is scaled by alpha, which defaults to 0, E11). This mirrors the AnimationBudgetAllocator's own bookkeeping (E11) without its load-adaptive policy.

| Tier | Update / evaluation (external control) | Motion-matching `SearchThrottleTime` | Graph features (`UStealthAnimInstance` flags) | Mesh LOD |
|---|---|---|---|---|
| A (h ≥ 19 %) | rate 1 (every frame), no interpolation | 0 (search every update) | full: foot placement, look-at, offset root, overlays | engine screen-size LOD (LOD0) |
| B (8-19 %) | rate 2 (skip 1), evaluation rate-limited, interpolated | `AnimSearchIntervalTierBSec` (0.067 s) | foot placement only while `h ≥ 11 %`; look-at; offset root | LOD1→LOD2 |
| C (3-8 %) | rate 4 (skip 3), interpolated | `AnimSearchIntervalTierCSec` (0.133 s) | look-at and offset root only; fingers/twist stripped by LOD | LOD2→LOD3 |
| D-visible (< 3 %, rendered) | rate `max(1, floor(FrameHz / AnimTierDVisibleMinHz))` so pose updates ≥ 8 Hz; interpolated | `1 / AnimTierDVisibleMinHz` | locomotion and stops only: no head-look, no foot IK | LOD3 |
| D-culled (not rendered, client) | no pose tick or evaluation (`bExternalUpdate = false` every frame), so montages do **not** advance while culled (E10); on promotion `EnableExternalUpdate(true)` that frame with the montage position **re-derived from replicated state** (`FActionState`/`FDeathState`/`TraversalState` seeked at `EstimatedServerNow()`, ADR-0002), so the pose is evaluated correctly in its first rendered frame, inertialised from the last pose | — | — | — |

- **Phase spreading.** A pawn at rate `k` updates on frames where `(FrameCounter + TierTieKey) mod k = 0` (spreads cost; client-local; kind-blind).
- **Inertialised transitions.** Tier changes never snap: `bUseInertialBlend = true` is set **explicitly** on the motion-matching node (the engine default is false, E12) and the AnimBP has an inertialization node at the root; interpolation alpha comes from the external controller. Whether `bResetOnBecomingRelevant` resets the node after skipped updates is checked in ANIM-V1.
- **State overrides that are kind-blind, with precedence.** A pawn playing a root-motion montage, in traversal (`IsInTraversal`) or in a `PoseLocked` action is forced to rate 1 while that state lasts, on every client, whatever its controller, because root-motion extraction and contact timing need every frame (the condition is replicated state, so all clients agree). **The rate-1 override wins over every tier, including D-culled.**
- Mesh LOD selection stays the engine's screen-size rule from the identical per-asset `ScreenSize` table (art bible 8.5), the same key family as `h`; nothing forces or biases a LOD per pawn.

### 4. Authority: server and listen host

- **No external tick-rate control on authority** (revised after the engine gate, B1/E10). `ServerMove`s tick a remote-controlled pawn's pose during net-driver dispatch, before any tick group, so no tick prerequisite can order them, and external control would advance root-motion montages by the wrong dt or freeze them. On any process with authority over the pawns (listen host, dedicated server, standalone), every Shared Pawn mesh therefore has URO **and** external control off and `VisibilityBasedAnimTickOption = AlwaysTickPoseAndRefreshBones` (E3): every pawn updates every frame (rate 1), so montages, root motion and notifies run whatever the host looks at (SCL R11.5). Project code never sets `bOnlyAllowAutonomousTickPose` (ADR-0010): the engine's role-driven `ServerMove` ticking (ADR-0010 E4) runs unmodified for remote-controlled pawns, and this ADR owns the remedy ADR-0010's risk table hands to it (below).
- **What tiering still does on the listen host** (its rendering is a player's view): the subsystem computes tiers from the host's camera exactly as on a client and applies the **search throttle, the graph feature flags and nothing else** (mesh LOD remains the engine's screen-size rule). Update and evaluation rates are not throttled on authority. Escalation if R3's host capture misses the budget: override the virtual `USkeletalMeshComponent::TickPose` in a pawn mesh subclass so `bIsAutonomousTickPose` calls use the move's dt, then re-enable external control on the host — requires ADR-0010's agreement to a mesh-component subclass and the traversal spike (recorded, not chosen).
- **Gameplay never reads animation.** No gameplay rule may depend on an anim notify, a bone/socket transform or a pose curve; all gameplay timing comes from server time and the queue (ADR-0002) and from replicated state. Notifies are presentation only (footsteps do not use them, §6). This makes the host's rendering throttling irrelevant to rules.
- **Remote-controlled pawns on the host (ADR-0010 E4).** Their server pose advances with `ServerMove`s (at the client's send rate, with the client's dt), not with the mesh tick. Because the host *renders* that pose, an honest host player could see a different update cadence on players than on NPCs. ANIM-V3 measures it. If it fails, the remedy must be uniform: the preferred candidate is to drive the **host's rendered pose** for every pawn from the same interpolated presentation path (a visual-only pose interpolation on the host keyed to frame time, identical for all pawns); a kind-dependent setting is never a remedy (ADR-0010 risk table). Until ANIM-V3 passes, host-view animation parity is **provisional** and Pillar-1 evidence excludes the host's own view (as CPA and IVP already do for host trust).
- **Dedicated server (Alpha).** No rendering and no tiering: every pawn at rate 1 with the throttles of tier D-visible applied to search and features (no camera); revisited in ADR-0024.

### 5. The 4 ms definition and the performance gate

- **Definition (closes SCL OQ4 and art bible 8.x.15):** `C_anim` = per frame, the **sum** of (a) game-thread time of every Shared Pawn skeletal mesh component's tick, animation update dispatch/completion and `RefreshBoneTransforms` dispatch, (b) worker-thread time of parallel animation update and evaluation tasks for those components (including PoseSearch searches and chooser evaluation), and (c) `UAnimSignificanceSubsystem` and the step clocks. It **excludes** CMC movement for simulated proxies and for server-simulated AI pawns (reported separately, SCL R12) and GPU skinning (ADR-0022's 2.5 ms GPU line). Measured with Unreal Insights (`-trace=cpu,frame`, named scopes on (c)), p95 over 60 s after a 10 s warm-up.
- **Budgets:** `C_anim ≤ 4.0 ms` (SCL R12, F8) **and** its game-thread part (a)+(c) ≤ **2.0 ms**, the animation line of architecture.md's listen-host CPU table. Both must hold on the client reference scene and on the listen host with 9 remote clients.
- **Gate:** spike R3 on RTX 3060-class hardware (art bible 8.x.14 pass line "≤ 4 ms total, ≤ 8 characters in tier A") is a precondition for acceptance; RTX 5090 numbers are recorded as development evidence only. AC-SCL-29 is the repeatable test.
- **Fallbacks if R3 fails, in order, each applied to every pawn and every client identically** (none changes tier thresholds per pawn, never keyed to who is on screen; art bible 8.x.16): (1) prune the databases further and raise `PoseReselectHistory`/search throttles for tiers B and C within their safe ranges; (2) lower the caps within safe ranges (`AnimTierACapCount` 8→6, B 14→12) — a tuning change validated by I-SCL-11; (3) disable foot placement below tier A for everyone; (4) switch tier C and D-visible to a non-motion-matching locomotion state (blend-space-free sequence player driven by speed and the step-clock phase) for everyone. A fallback that changes a key's **default or range** goes through `/propagate-design-change`; a fallback that needs a new key is a schema change for the user.

### 6. Footsteps: one step clock, one concurrency

- **`UStealthStepClockComponent`** (on `AStealthPawn`, every pawn, every process that plays audio): implements SCL R18 and F11 from movement only — distance travelled by the **rendered (smoothed) position** of the pawn (simulated proxies after SCL R14 smoothing; the host uses the same smoothed position for remote-client pawns, so `ServerMove` burstiness does not leak into cadence), divided by `stride(v)`; start step after `FootstepStartDelaySec`, settling step below `PivotSpeedCmPerSec`, one step per 60° of in-place pivot; suppressed while `Falling`, in traversal, in `PoseLocked`, while the client-local concealment presentation latch `bPresentationHidden` is set (SCL R17.2), and absent entirely for pawns culled from this client (no actor). It never reads animation state, tier, controller, team or Roster state. Its phase is published to `UStealthAnimInstance` (§1) so stride warping and distance matching follow the clock at tiers A-C (R18.1); resync slews over ≥ 3 strides.
- **Event.** `FStepEvent {Pawn, Foot, SurfaceType, Gait}` — no controller, team, role or `AppearanceId` field (CPA CP-16) — fires a single `UFootstepAudioSubsystem` entry point, which plays one MetaSound source per surface type at the foot socket (socket position at tier D comes from the last evaluated pose; tier-independent fallback: the capsule bottom offset by foot side).
- **Concurrency and voice limiting (TD concern 8).** Every footstep from every pawn uses **one** `USoundConcurrency` asset (`SC_Footsteps`: resolution `StopFarthestThenOldest`, one max-count value, `bLimitToOwner = false`), **one** sound class and submix, **one** attenuation asset, and **identical `Priority`** and virtualization mode. The owner's own steps use the same assets (a 2D/3D mix choice on the owner's client is owner-local, SCL R18.4). Because `au.EnableRelativeRenderCostVoiceLimit` is on by default in 5.8 (E7), all footstep sources must have identical render cost: the same MetaSound graph per surface type with no per-pawn parameter. Culling and stealing therefore depend on distance, age and loudness only. ANIM-V5 verifies parity with 50 walking pawns.
- **Server AI hearing**, if any, is a separate authoritative noise event (Crowd, ADR-0016), never derived from client audio.

### Architecture Diagram

```
 CLIENT / LISTEN HOST (render)                                 every frame
 ┌───────────────────────────────────────────────────────────────────────────────┐
 │ Camera (ADR-0015): view z, FOV, IsEyeMaskVisible(Pawn)   Tuning client face   │
 │            │                                              (tier keys, ADR-0004)│
 │            ▼                                                  │               │
 │ UAnimSignificanceSubsystem  [TG_PrePhysics, prerequisite of every pawn mesh]  │
 │   FAnimTierCore: h → raw tier (hysteresis, demotion hold) → caps 8/14/28      │
 │                  ties: TierTieKey (client-local FIntStream, never replicated) │
 │            │ per pawn, identical table                                        │
 │            ▼                                                                  │
 │ USkeletalMeshComponent (URO OFF, external tick-rate control ON)               │
 │   ExternalTickRate · ExternalUpdate · Interpolation alpha                     │
 │ UStealthAnimInstance: SearchThrottleTime, feature flags, step phase           │
 │   one AnimBP · one PoseSearch DB set · one chooser set · one fall montage     │
 └───────────────────────────────────────────────────────────────────────────────┘
 AUTHORITY: URO off, external control OFF, AlwaysTickPoseAndRefreshBones, rate 1 for every pawn;
            host tiering applies only search throttle + feature flags; gameplay never reads anim.

 replicated movement ─► smoothed render position ─► UStealthStepClockComponent (F11)
        ─► FStepEvent{Pawn,Foot,Surface,Gait} ─► UFootstepAudioSubsystem
        ─► one MetaSound per surface · SC_Footsteps (StopFarthestThenOldest) · one priority
```

### Key Interfaces

```cpp
// --- StealthGameplay/Public/View/Animation (client-local presentation) ----------
enum class EAnimTier : uint8 { A, B, C, DVisible, DCulled };
struct FAnimTierInput  { float H; bool bRendered; EAnimTier Current; float HeldSec; uint32 TierTieKey; };
struct FAnimTierParams { float TA, TB, TC, Eta, MinHoldSec; int32 CapA, CapB, CapC; };  // from client tuning
class FAnimTierCore {                                    // plain C++, headless-testable (AC-SCL-26/28)
public:
    static void Assign(TArrayView<FAnimTierInput> Pawns, const FAnimTierParams&, TArrayView<EAnimTier> Out);
    static float ScreenHeight(float DepthCm, float FovVDeg);   // F7, h = 1 below 30 cm
};
class UAnimSignificanceSubsystem : public UWorldSubsystem {   // not created on a dedicated server
public:
    void RegisterPawn(AStealthPawn*);   void UnregisterPawn(AStealthPawn*);   // BeginPlay / EndPlay
    EAnimTier GetTier(const AStealthPawn*) const;               // debug and tests only
};
UCLASS() class UStealthAnimInstance : public UAnimInstance {
public:
    UPROPERTY(BlueprintReadOnly) float SearchThrottleTimeSec = 0.f;   // bound to the MM node pin
    UPROPERTY(BlueprintReadOnly) bool bFootPlacement = true, bLookAt = true, bOffsetRoot = true;
    UPROPERTY(BlueprintReadOnly) float StepPhase = 0.f;              // from the step clock
    // No property derived from controller, PlayerState, team or Roster state (AC-SCL-01).
};

// --- On AStealthPawn (ADR-0010 owns the class) -----------------------------------
UPROPERTY() TObjectPtr<UStealthStepClockComponent> StepClock;        // every pawn
struct FStepEvent { TWeakObjectPtr<AStealthPawn> Pawn; uint8 Foot; TEnumAsByte<EPhysicalSurface> Surface; uint8 Gait; };
class UFootstepAudioSubsystem : public UWorldSubsystem { public: void PlayStep(const FStepEvent&); };
```

### Implementation Guidelines
1. Every Shared Pawn **must** use the one AnimBP, the one PoseSearch database set and the one chooser set; no code or asset may select an animation asset, layer or graph by controller, team, participant state or `AppearanceId`.
2. Engine URO **must** be off (`bEnableUpdateRateOptimizations = false`) for every Shared Pawn mesh on every process; external tick-rate control **must** be on for every pawn on non-authority clients (with `AlwaysTickPose`, accumulated `ExternalDeltaTime` and alpha set every frame) and **must** be off on authority processes; `USkeletalMeshComponentBudgeted`, the AnimationBudgetAllocator and the SignificanceManager **must not** be used for pawns (ADR revision required to adopt them).
3. Tier inputs **must** be only `h` (view depth and FOV) and Camera's eye-mask flag; tie-breaks **must** use the client-local `TierTieKey`; nothing that varies by kind (spawn index, NetGUID, actor name, registration order, controller) may enter a tier decision.
4. Motion-matching search cadence **must** be time-based (`SearchThrottleTime` in seconds from the tier keys), never a frame count.
5. On authority every pawn **must** use `AlwaysTickPoseAndRefreshBones` at rate 1 and project code **must never** set `bOnlyAllowAutonomousTickPose`; root-motion, traversal and `PoseLocked` states **must** run at rate 1 on every client, overriding every tier including D-culled; a D-culled pawn's montage position **must** be re-derived from replicated state at promotion; gameplay rules **must never** read anim notifies, bone/socket transforms or curves.
6. Performance fallbacks **must** apply to every pawn and client identically; changing thresholds, caps or features for some pawns, dropping character shadows, or adapting to who is on screen is forbidden.
7. Footsteps **must** come only from `UStealthStepClockComponent` (never from anim notifies) using the rendered smoothed position; every footstep **must** use `SC_Footsteps`, one sound class, one attenuation, one priority, and no per-pawn parameter.
8. `C_anim` **must** be measured as defined in §5 (GT + worker sum, p95, Insights) and reported with the GT share; RTX 5090 numbers **must not** be cited as acceptance evidence.
9. The development-only `WasRecentlyRendered` fallback for the rendered flag **must** be compiled out of Shipping and removed when ADR-0015's eye-mask flag exists.

## Alternatives Considered

### Alternative 1: Engine URO with default settings (or `LODToFrameSkipMap`)
- **Description**: Leave `bEnableUpdateRateOptimizations` on and tune `BaseVisibleDistanceFactorThesholds` or a LOD-to-frame-skip map.
- **Pros**: Zero code; battle-tested.
- **Cons**: 5.8.4's `AnimUpdateRateSetParams` ticks any pawn whose instigator controller is an `APlayerController` at full rate (E1): on the listen host every remote player animates at full rate while NPCs are throttled; no caps; no tie key; rates by distance factor, not the GDD's `h`.
- **Rejection Reason**: A built-in player-keyed animation tell (ADR-0001 §3 forbids AI-keyed LOD).

### Alternative 2: AnimationBudgetAllocator with a custom significance function
- **Description**: Make every pawn mesh a `USkeletalMeshComponentBudgeted`, feed `SetComponentSignificance(h)` and let the allocator fit a `BudgetInMs`.
- **Pros**: Mature time-budgeting, interpolation and reduced-work logic; already uses external tick-rate control (E2).
- **Cons**: Rates adapt to measured load, so which pawns are throttled changes with frame cost rather than the GDD's fixed tiers and caps; tie order and reduced-work selection follow internal ordering that this project does not control (registration order can correlate with first-export order); its default significance iterates PlayerControllers (E6); requires changing the pawn's mesh component class; not enabled by default.
- **Rejection Reason**: Cannot express the 8/14/28 caps or a client-local tie key deterministically; kept as a recorded option only if R3 shows the custom subsystem's bookkeeping is itself too costly (an ADR revision).

### Alternative 3: SignificanceManager plugin
- **Description**: Register pawns with `USignificanceManager` and compute significance there.
- **Pros**: Standard registry with per-object significance callbacks.
- **Cons**: Adds a plugin and a registry for what is a 60-element sort; still needs the same tier application code.
- **Rejection Reason**: No benefit over the plain-C++ core.

### Alternative 4: Anim-notify footsteps
- **Description**: Footstep notifies on locomotion clips (the sample's approach).
- **Pros**: Steps match foot plants exactly at full fidelity.
- **Cons**: Notifies blend unreliably under motion matching and vanish when the pose is not evaluated (tier D, culled), so audibility would depend on tier — and tier depends on the viewer's camera, which differs per pawn's screen position; LOD would change what is heard (SCL R18, logged technical-artist decision).
- **Rejection Reason**: Breaks tier-independent audio parity.

### Alternative 5: Throttle by fixed distance bands instead of screen height
- **Description**: Tiers from world distance only.
- **Pros**: Simpler.
- **Cons**: FOV changes (first person, slider) break the art bible's coupling of tier and mesh LOD to screen size; the GDD fixes `h`.
- **Rejection Reason**: Contradicts SCL F7 and art bible 8.x.8.

## Consequences

### Positive
- The engine's player-keyed URO path is removed for every pawn, closing a Pillar-1 tell that would have been invisible in code review.
- Tiering is deterministic, testable headless and identical for every pawn; caps make the worst case of F8 an upper bound rather than an estimate.
- Footstep audibility is independent of tier, controller and viewer, and voice stealing depends only on distance, age and loudness.
- The 4 ms question (SCL OQ4, art bible 8.x.15) is closed with a definition that matches the CPU budget table.

### Negative
- The project owns tick-rate bookkeeping the engine would otherwise do (phase spreading, interpolation alpha, promotion updates): ~300-500 lines plus tests.
- The listen host updates every pawn at rate 1 (only search throttle and feature flags are tiered there), so the host's animation cost is higher than a client's; R3's host capture must show it fits the 4.0 ms / 2.0 ms GT lines, else the `TickPose` override escalation (§4) is needed.
- Host-view animation parity depends on ANIM-V3 because the engine's `ServerMove`-driven pose for remote players (ADR-0010 E4) is outside this ADR's control.
- Search throttling and interpolation reduce motion quality at tiers B-D for everyone; acceptable by design (art bible 8.x.8).

## Risks
- **R1 — R3 fails on 3060-class hardware.** *Mitigation*: the ordered uniform fallbacks of §5; the CPU line is also protected by the CMC and AI lines being reported separately; escalation is an ADR-0022 budget re-allocation, never a per-pawn rule.
- **R2 — External tick-rate control interacts badly with the sample's AnimBP** (pops at rate changes, montage or root-motion drift). *Mitigation*: ANIM-V1; external control on non-authority clients only (E10); alpha and accumulated dt set every frame (E11); rate-1 override for root-motion/traversal/PoseLocked states; montage re-derivation at D-culled promotion; explicit inertial blending.
- **R9 — Host animation cost at rate 1 for every pawn.** *Mitigation*: R3 host capture; escalation is the `TickPose` override (§4) with ADR-0010's agreement, never a kind-dependent setting.
- **R3 — Host-view cadence leak (ANIM-V3).** *Mitigation*: uniform host presentation interpolation; until it passes, host view is excluded from Pillar-1 evidence and listed as a listen-host limit alongside host trust.
- **R4 — Eye-mask flag timing (ADR-0015 not yet written).** The subsystem uses the previous frame's flag; a pawn becoming visible could draw one frame with a stale pose. *Mitigation*: Camera's mask fades in from α = 0, so the first visible frame is nearly transparent; AC-SCL-46 checks "posed in its first rendered frame"; ADR-0015 must publish the flag before `TG_PrePhysics` of the next frame.
- **R5 — Motion-matching database too sparse at 112 cm/s** (foot sliding). *Mitigation*: stride warping and play rate 0.9-1.1; else raise `WalkSpeedScaleMinMult` for every pawn (SCL edge case).
- **R6 — Step clock vs visible plants (ANIM-V4, SCL OQ12).** *Mitigation*: fallback in the GDD — tier-A pawns fire from a foot-contact curve snapped to the clock phase, identically for every pawn.
- **R7 — Relative render-cost voice limiting (ANIM-V5).** Different MetaSound graphs per surface could have different render cost; surfaces correlate with location, not kind, so this is not a kind tell, but it is measured.
- **R8 — 5.8 deprecated-symbol removals in sample Blueprints (ANIM-V6).** *Mitigation*: compile and fix during AnimBP pruning; the prototype already compiles on 5.8.4 (first playable report).

## GDD Requirements Addressed

| GDD System | Requirement | How This ADR Addresses It |
|------------|-------------|--------------------------|
| shared-character-locomotion.md | R10 one AnimBP, one MM database set, one chooser set, one look-at, one pose set; `ActionPose`-driven; time-based search cadence; debug draws stripped (TR-scl-006, provisional) | §1 asset set and input list; §3 `SearchThrottleTime` in seconds with accumulated dt |
| shared-character-locomotion.md | R11.1-R11.3 tiers by `h` and eye-mask flag only, hysteresis, demotion-only hold, caps with overflow, `TierTieKey`, per-tier features, no rendered pawn frozen, inertialised | §2 subsystem and `FAnimTierCore`; §3 tier table |
| shared-character-locomotion.md | R11.4 owner tiered like any pawn | §2 step 3 |
| shared-character-locomotion.md | R11.5 server always-ticks pose identically; host renders with client tiering | §4 |
| shared-character-locomotion.md | R11.6 budget allocator only with this significance | §Alternatives 2 (not adopted; Guideline 2) |
| shared-character-locomotion.md | R12 ≤ 4 ms at 60 characters on 3060-class; CMC reported separately; fallbacks equal for all | §5 definition, gate, fallbacks |
| shared-character-locomotion.md | F7 tier formula; F8 cost budget and I-SCL-11 | §2; §5 (I-SCL-11 enforced by ADR-0004) |
| shared-character-locomotion.md | R18, F11 one step clock independent of tier; suppression; event fields | §6 |
| art-bible.md 8.x.8 | tier table, D split, time-based search, step sync ±50 ms, budget math, tools present | §3, §5, §6 |
| art-bible.md 8.x.14 / 8.x.16 | R3 gate; fallbacks never per pawn | §5 |
| assassination-defence-reversal.md | ASN-R17 one body sequence for every victim | §1 one fall montage from `FDeathState` (removal and possession: ADR-0018) |
| information-visibility-policy.md / ADR-0001 §3 | no AI-keyed animation LOD, budget or audio concurrency | E1 finding; §3 URO off; §6 one concurrency |

## Performance Implications
- **CPU**: target `C_anim ≤ 4.0 ms` (GT + workers) and GT ≤ 2.0 ms at 60 characters; F8 estimate 3.04 ms + `C_fixed` ≤ 0.9 → ≤ 3.94 ms (ASSUMED until R3; 0.06 ms margin). Tier subsystem ≤ 0.05 ms GT. Step clocks: 50 × ~1 µs. The listen host updates every pawn at rate 1 (only search throttle and feature flags are tiered on authority, §4), so its cost exceeds a client's F8 figure; it is measured, not estimated, in R3's host capture.
- **Memory**: one database set; pruning the sample databases reduces memory versus the prototype; no per-pawn animation assets.
- **Load Time**: unchanged (databases load with the pawn class).
- **Network**: none (tiering and footsteps are client-local; nothing replicated is added).

## Migration Plan
The prototype's `AStealthCharacter` runs the sample AnimBP with engine defaults (URO default, all databases, notify footsteps where the sample has them). Steps: (1) reparent the AnimBP onto `UStealthAnimInstance` and prune databases (ANIM-V6); (2) turn URO off on every process and add external control on non-authority clients with all pawns at rate 1 (baseline capture); (3) add `FAnimTierCore` + subsystem and tests (AC-SCL-26/27/28/44/46); (4) step clock + footstep subsystem, remove notify footsteps (AC-SCL-31, ANIM-V4/V5); (5) R3 on 3060-class hardware and the listen-host capture; apply fallbacks if needed. Prototype code is promoted only after this ADR is Accepted (ADR-0007).

## Validation Criteria
- Headless T1: AC-SCL-26 (tier formula), AC-SCL-28 (caps and tie key: player pawn tier-A frequency equals an NPC's within ±3 % over 10,000 draws; Spearman |ρ| < 0.1 between `TierTieKey` and spawn order), AC-SCL-01 (static scan: no kind reads in AnimBP/anim instance/step clock), reflection check that every pawn mesh has URO off and external control on.
- PIE / integration: AC-SCL-27 (tier parity A/B), AC-SCL-44 (asset-set parity; tier-B search count equal at 30 and 120 fps ± 5 %), AC-SCL-46 (no rendered pawn below 8 Hz; posed in first rendered frame), AC-SCL-30 (camera toggle changes nothing), AC-SCL-31 (footstep parity), ANIM-V1..V6.
- Performance: AC-SCL-29 / R3 on RTX 3060-class at 1080p (client scene and listen host with 9 remote clients) against §5 budgets.
- Playtest: AC-SCL-33 blind observer walk (≤ 60 % correct).

**Gate results (2026-10-24):**
- **TD-ADR: CONCERNS → REVISED** (autonomous: "Revise flagged items", logged). Applied: §4's false claim about `ServerMove` pose ticking replaced; ADR-0010 E4 stance stated (project code never sets `bOnlyAllowAutonomousTickPose`; this ADR owns ADR-0010's risk-table remedy); D-culled montages re-derived from replicated state at promotion (GDD sync item 5); rate-1 override precedence over every tier; sync items for architecture.md and ADR-0001 §3 and TR IDs added; pawns without a resolved mesh skipped.
- **Engine validation (unreal-specialist lens): BLOCKING → REVISED.** B1 (external control on authority mis-times `ServerMove`-driven root motion) resolved by fix (a): authority runs URO and external control off, every pawn at rate 1, tiering limited to search throttle and features; fix (b) (`TickPose` override) recorded as escalation. B2-B7 applied: alpha and accumulated `ExternalDeltaTime` every frame; rate-limiting for every tier with rate > 1; subsystem a prerequisite of the CMC tick too; clients use `AlwaysTickPose`; `bUseInertialBlend` set explicitly; `bResetOnBecomingRelevant` checked in ANIM-V1. Remaining [U]: per-instance `SearchThrottleTime` at runtime (ANIM-V2), host-side smoothed position for remote-client pawns used by the step clock (§6), concurrency enum details not re-read by the gate (verified by the author, E7). Not re-run (autonomous run); `/architecture-review` is the check.

## Related
- Depends on: ADR-0001 (§3), ADR-0003 (§5.8), ADR-0004, ADR-0007, ADR-0008, ADR-0010 (E4, §2); soft: ADR-0015 (eye-mask flag).
- Enables: ADR-0017, ADR-0018, ADR-0022.
- ADR-0012 (no `AppearanceId`-keyed audio or animation selection; pool meshes share the Manny skeleton so one AnimBP drives all).
- Design: `design/gdd/shared-character-locomotion.md` R10-R12, R18, F7, F8, F11, OQ4, OQ12; `design/art/art-bible.md` 5.4, 8.x.8, 8.x.14, 8.x.16; `design/gdd/assassination-defence-reversal.md` ASN-R17; `production/first-playable-report.md` (RTX 5090 evidence).
- **GDD sync items (not applied; GDDs owned by other agents in this run):** (1) SCL R11.3 / art bible 8.x.8 say "URO skip 1 / skip 3": this ADR implements the same skips through external tick-rate control with engine URO **off**, because engine URO is player-keyed in 5.8.4 (E1); the wording "URO" should read "update-rate skip". (2) SCL R11.6 / art bible 8.x.8 permit the AnimationBudgetAllocator with the significance function; this ADR does not adopt it (Alternative 2). (3) SCL OQ4 and art bible 8.x.15 ("is 4 ms GT or summed?") are answered by §5: summed GT + workers ≤ 4.0 ms with a GT share ≤ 2.0 ms. (4) SCL R11.5 "always-tick" is read with ADR-0010's scoping (E4): authority uses `AlwaysTickPoseAndRefreshBones` at rate 1 for every pawn, but remote-controlled pawns' server pose advances with `ServerMove`s; host-view parity is ANIM-V3. (5) SCL R11.3 "D-culled: montages still tick" — under external control nothing ticks while culled; this ADR re-derives montage position from replicated state at promotion (same visible result). (6) architecture.md Core-layer "Animation & significance" row and Required ADRs row ADR-013 list URO and the AnimationBudgetAllocator/SignificanceManager as candidate mechanisms; this ADR rules both out (E1, Alternatives 2-3). (7) ADR-0001 §3 "the server ticks pose for every pawn identically" needs the same E4 scoping as ADR-0010. (8) TRs covered (provisional): TR-scl-006, TR-scl-007, TR-scl-008, TR-scl-011, TR-asn-007 (shared with ADR-0018).

User decisions (2026-10-25); decision unchanged in substance, Status unchanged (Accepted).
- user decision 2026-10-25: `NpcCount` default 50 (was ~40, user-locked default changed by the user), so 5v5 has 60 characters; gate R3, the requirement and the CPU estimate are restated at 60 characters (SCL F8: 3.04 ms + `C_fixed`, ≤ 3.94 ms; `C_fixed` ≤ 0.54 ms at the cap tops). Working values, re-verify in R3.
