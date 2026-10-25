# ADR-0022: Rendering configuration and performance budgets

## Status
Accepted — 2026-10-25, by the user (blanket acceptance of ADR-0001..0023, recorded by the orchestrator). **Conditional:** every spike or verification item named in the earlier status text below must still report before implementation depends on it; a failed spike is handled by a superseding ADR, not by editing this one.

*Earlier status text (kept for its gating conditions):* Proposed — awaiting user acceptance. Acceptance is gated on spikes **R1, R2 and R3 run on RTX 3060-class target hardware at 1080p** (the art bible 8.x.14 mass-production gate; architecture.md AQ-07, AQ-18) and on the user resolving the project-config items listed under "Pending user items" (no `Config/` file is edited by this ADR). It also needs ADR-0012, ADR-0013, ADR-0014, ADR-0015 and ADR-0017 Accepted. Every performance number in this project so far comes from an RTX 5090 / 16-core desktop and is **development evidence, not acceptance evidence**.

## Date
2026-10-24

## Last Verified
2026-10-24 (engine claims read against the UE 5.8.4 install at `C:/Program Files/Epic Games/UE_5.8`; project config read from `Config/DefaultEngine.ini` (read-only); design against art bible 8.x.2-8.x.17, `shared-character-locomotion.md` R12, `procedural-city-generator.md` R-G9, `character-pool-appearance.md` CP-18, `camera-system.md` CS-R5)

## Decision Makers
Autonomous design pipeline (`/architecture-decision`, rigor full, review mode full); technical-director (TD-ADR) and unreal-specialist validation; acceptance reserved for the user. Performance-analyst signs the spike numbers and technical-artist runs the spikes (art bible 8.x.14); producer holds the mass-production gate.

**Gate record (2026-10-24):** TD-ADR CONCERNS → REVISED; engine validation MINOR NOTES → applied. Details under `## Validation Criteria` → "Gate results".

## Summary
Fifty identical-looking characters, a runtime-assembled Nanite city and Lumen must fit a 60 fps frame on an RTX 3060-class PC at 1080p without any rendering setting, cost or fallback ever differing by controller kind, team or viewer. This ADR fixes the project's rendering configuration (Substrate **off**, Lumen **software** GI and reflections with hardware ray tracing off, Virtual Shadow Maps on, MegaLights off, TSR at a fixed screen percentage with dynamic resolution off, Nanite for the kit and large opaque props only, classic skeletal meshes for characters, skin-cache mode chosen by spike R2 and applied uniformly to every LOD of every pool mesh, one masked-dither character master), the CPU game-thread, render-thread, GPU, memory, draw-call and package budget tables with their owners, the measurement protocol, and the ordered pre-decided fallbacks — all global, never per pawn.

## Engine Compatibility

| Field | Value |
|-------|-------|
| **Engine** | Unreal Engine 5.8 (installed 5.8.4, CL 58902052) |
| **Domain** | Rendering (with Core for the CPU budget table) |
| **Layer** | Presentation (architecture.md Required ADRs row ADR-022: "Presentation / cross-cutting"; the rendering configuration governs the client-local presentation path; the CPU budget table is cross-cutting and only allocates lines owned by other ADRs) |
| **Knowledge Risk** | HIGH — Rendering is HIGH risk for 5.4-5.8 (`VERSION.md`: Substrate production-ready and on for new projects since 5.7, MegaLights production-ready in 5.8, Lumen Lite Beta in 5.8); `docs/engine-reference/unreal/modules/rendering.md` is tutorial-level and stale on Nanite skeletal meshes and Nanite WPO (art bible 8.x.4, 8.x.3) |
| **References Consulted** | `docs/engine-reference/unreal/VERSION.md`, `modules/rendering.md`, `breaking-changes.md`, `deprecated-apis.md`, `current-best-practices.md`; `docs/architecture/architecture.md` (Engine Knowledge Gap Summary, Required ADRs row ADR-022, Architecture Principles → Performance budget and CPU table, Open Questions AQ-07, AQ-08, AQ-18); ADR-0001, ADR-0002, ADR-0003, ADR-0007, ADR-0008, ADR-0010, ADR-0012 (E3, E5, E11, E12, §5), ADR-0013 (§5), ADR-0014 (§3.2, R-6), ADR-0015 (§4.4, R-4), ADR-0016 (budget), ADR-0017 (E1, E2, §6); art bible 1.3.5, 8.x.1-8.x.17; `Config/DefaultEngine.ini` (read-only); engine source and config read this session (table below) |
| **Post-Cutoff APIs Used** | No new C++ API. Post-cutoff **settings** relied on: `r.Substrate` (ReadOnly, default 0 for existing projects) [V]; `r.MegaLights.EnableForProject` (default 0) [V]; Lumen Lite-class "Medium GI: Lumen Irradiance Volume" at `sg.GlobalIlluminationQuality=1` [V config, D name]; `r.PSOPrecache.ProxyCreationStrategy` (owned by ADR-0012) [V]; Nanite skinned meshes `r.Nanite.AllowSkinnedMeshes` (exists, not used) [V] |
| **Verification Required** | **R1 (gate)** kit Lumen + Nanite at 1,000 / 3,000 / 5,000 runtime ISM instances ≤ 5.0 ms GPU on a 3060 at 1080p; **R2 (gate)** skin cache Enabled vs Disabled on every pool LOD, 60 characters with VSM, character skinning + character shadows ≤ 2.5 ms GPU; **R3 (gate, owned by ADR-0013)** animation ≤ 4 ms, GT share ≤ 2.0 ms; **RC-1** the 10-connection listen-host capture on 3060-class hardware (GT ≤ 12.0 ms p95 by the owner table); **RC-2** whole-frame GPU on the client reference scene ≥ 2.0 ms headroom (art bible 8.x.16); **RC-3** masked-dither character master: GPU delta vs an opaque master ≤ 0.3 ms at 50 characters (depth prepass + VSM shadow pass), dither works on the classic skinned path, and a visual check of TSR ghosting/shimmer and dithered-shadow noise passes; **R4** (ADR-0014), **R5** (ADR-0017), **R6-R9** (this ADR, non-gating) |

**Engine and project facts this ADR relies on** ([V] read in 5.8.4 source/config or project config this session; [D] engine reference; [U] unverified, owned by the named item):

| # | Fact | Tag |
|---|---|---|
| E1 | `r.Substrate` is a ReadOnly CVar with default **0**; "Existing project NOT using Substrate will continue to NOT use Substrate"; new 5.7+ projects get it enabled by the project-creation path (`GameProjectUtils`), not by the CVar default. The project had **no** `r.Substrate` key, so Substrate is **off** (the 2026-10-25 config pass added a defensive `r.Substrate=False`, `DefaultEngine.ini:23`; behaviour unchanged) | [V] `Runtime/RenderCore/Private/RenderUtils.cpp:2068-2083`; `RendererSettings.h:1257-1261` (`bEnableSubstrate`, `ConfigRestartRequired`); `Config/DefaultEngine.ini` (no key) |
| E2 | `r.SkinCache.DefaultBehavior`: engine default **1 (Inclusive)**; the project sets **0 (Exclusive)** — every skeletal mesh is excluded unless it opts in; per-LOD `ESkinCacheUsage {Auto, Disabled, Enabled}` on `FSkeletalMeshLODInfo`; a mesh with Support Ray Tracing is forced inclusive; an excluded mesh is skinned in the vertex shader of every pass that draws it (depth, base, each shadow view) | [V] `Engine/Private/GPUSkinCache.cpp:119-123`; `SkinnedAssetCommon.h:33-49`; `Config/DefaultEngine.ini:13-14`; ADR-0012 E5/E12; art bible 8.x.7 |
| E3 | `r.RayTracing` default **0** (was absent from project config; the 2026-10-25 config pass pinned it off, `DefaultEngine.ini:22-24`), so hardware ray tracing is off; `r.Lumen.HardwareRayTracing` defaults to 1 but Lumen "will fall back to Software Ray Tracing otherwise"; enabling `r.RayTracing` requires skin-cache support and an editor restart | [V] `Renderer/Private/DeferredShadingRenderer.cpp:278-282`; `Lumen/LumenHardwareRayTracingCommon.cpp:17-21`; `RendererSettings.h:617-621, 673-677` |
| E4 | Project rendering config today: `r.DynamicGlobalIlluminationMethod=1` (Lumen), `r.ReflectionMethod=1` (Lumen), `r.Shadow.Virtual.Enable=1`, `r.GenerateMeshDistanceFields=True`, `r.Lumen.TraceMeshSDFs=1`, `r.Nanite.ProjectEnabled=True`, `r.AllowStaticLighting=0`, `r.AntiAliasingMethod=4` (TSR, also the engine default), `r.MSAACount=8` (inert under deferred + TSR), motion blur off, `r.GPUSkin.UnlimitedBoneInfluences=True`, `r.SkinCache.CompileShaders=True`, DX12 with SM5 + SM6, `DefaultGraphicsPerformance=Maximum`, Desktop | [V] `Config/DefaultEngine.ini:4-21, 23-35`; `Engine/Private/SceneView.cpp:237-238` |
| E5 | `r.MegaLights.EnableForProject` default **0**; the 5.8.4 help text still says "requires Hardware Ray Tracing … Experimental", while the 5.8 release notes call it production-ready | [V] `Renderer/Private/MegaLights/MegaLights.cpp:27-29`; [D] `modules/rendering.md` |
| E6 | Texture streaming pool by `sg.TextureQuality`: 400 / 600 / 800 / **1000 MB** at levels 0-3; level 3 sets `r.Streaming.LimitPoolSizeToVRAM=0` | [V] `Engine/Config/BaseScalability.ini` `[TextureQuality@0..3]` |
| E7 | `sg.GlobalIlluminationQuality`: level 3 Lumen screen-probe gather downsample 16; level 2 downsample 32 **and** `r.Lumen.TraceMeshSDFs.Allow=0` (mesh-SDF detail traces off), a smaller surface-cache atlas (4096 → 3584) and a different irradiance format; level 1 "Medium GI: Lumen Irradiance Volume" (`r.Lumen.FinalGatherMethod=0`) — the Lumen Lite-class mode; level 0 disables Lumen diffuse. `sg.ReflectionQuality` ≤ 1 disables Lumen reflections. `sg.ViewDistanceQuality` 2 and 3 keep `r.SkeletalMeshLODBias=0`; level 1 raises it to 1 | [V] `BaseScalability.ini:110-124, 318-414, 514-546` (TD/engine gate); [D] Lumen Lite (Beta) in `VERSION.md` |
| E8 | Dynamic resolution `r.DynamicRes.OperationMode` defaults to **0 (disabled)** | [V] `Engine/Private/UnrealEngine.cpp:539-542` |
| E9 | Nanite skinned meshes exist (`r.Nanite.AllowSkinnedMeshes` default 1, `USkinnedMeshComponent::bDisallowNanite`); production status and motion-matching interaction unknown | [V] `Engine/Private/Rendering/NaniteResources.cpp:107-109`; art bible 8.x.4; [U] spike R6 |
| E10 | `r.Nanite.Foliage` defaults to 0; Nanite + WPO status contradictory between the reference ("no WPO") and source (`bEvaluateWorldPositionOffset` exists) | [V/U] art bible 8.x.3; spike R9 |
| E11 | PSO precache delays proxy creation while a component's PSOs compile (`r.PSOPrecache.ProxyCreationStrategy=1`, ReadOnly, default); component precache is off under `GIsEditor` | [V] ADR-0012 E3, E11 |
| E12 | A primitive hidden with `SetVisibility(false)` gets no render proxy (no mesh, shadow, Lumen, ray-tracing presence) while `bCastHiddenShadow`, `bAffectIndirectLightingWhileHidden` and the ray-tracing far-field flag are false | [V] ADR-0015 §4.4; ADR-0017 E1, E2 |
| E13 | Custom primitive data on a skinned mesh updates GPU-Scene data without re-creating the proxy | [V] ADR-0015 §4.4 (`SkeletalMeshSceneProxy.cpp:363`) |
| E14 | A dither-to-zero fade needs a Masked blend mode (opacity mask from the `DitherTemporalAA` material function, present in engine content). With Nanite enabled the renderer forces a full depth prepass (`DDM_AllOpaque`) that draws masked materials; `r.EarlyZPassOnlyMaterialMasking` (default 1, read-only) evaluates the mask only in the prepass and the base pass depth-tests for equality; in shadow-depth passes masked materials lose the position-only stream and run a pixel shader in VSM. Temporal dither under TSR can ghost/shimmer on moving skinned meshes and dithered shadows are noisy | [V] `RenderCore/Private/RenderUtils.cpp:534-545, 681-698`; `Renderer/Private/RendererScene.cpp:137-143, 4446-4451`; `ShadowDepthRendering.cpp:476, 576`; `Content/Functions/Engine_MaterialFunctions02/Utility/DitherTemporalAA.uasset` (TD/engine gate); RC-3 measures cost and look |
| E15 | `r.CustomDepth` exists (0 disables the feature); whether any retained Game Animation Sample content uses custom depth is unknown | [V] `Engine/Private/SceneUtils.cpp:17-19`; [U] sample-content audit |
| E16 | No skin-cache scene memory limit exists in 5.8 (the constructor's limit argument is `UE_DEPRECATED(5.7, "SkinCache no longer requires a memory limit")`); `r.SkinCache.PrintMemorySummary` reports usage | [V] `Engine/Public/GPUSkinCache.h:135` (TD/engine gate) |
| E17 | Desktop screen percentage defaults to `r.ScreenPercentage.Default.Desktop.Mode = BasedOnDisplayResolution`; the default resolution preset `ResolutionQuality=0` means "use the project default", so internal resolution follows display size unless manual mode is pinned | [V] `Engine/Private/LegacyScreenPercentageDriver.cpp:18`; `BaseScalability.ini:39-41` (TD/engine gate) |
| E18 | CVar set-by priority: Scalability (0x02) < GameSetting < ProjectSetting < SystemSettingsIni (0x05) < DeviceProfile < ConsoleVariablesIni, so a `DefaultEngine.ini [SystemSettings]` value overrides any `sg.*` group; `sg.TextureQuality=3` also sets `r.Streaming.LimitPoolSizeToVRAM=0`. `UGameUserSettings` persists `sg.*` in the user-editable `Saved/Config/.../GameUserSettings.ini` | [V] `Core/Public/HAL/IConsoleManager.h:155-187`; `BaseScalability.ini:774-783`; exact load order of saved user settings [U] |
| E19 | `UPrimitiveComponent` has `LDMaxDrawDistance`, `BoundsScale`, `LightingChannels`, `bRayTracingFarField`, `bAllowCullDistanceVolume` (a Cull Distance Volume sets a component's cached max draw distance from its bounds size); `USkeletalMeshLODSettings` is a `UDataAsset` | [V] `PrimitiveComponent.h:337, 461, 722, 830, 970`; `Engine/Classes/Engine/SkeletalMeshLODSettings.h:123` (TD/engine gate) |

## ADR Dependencies

| Field | Value |
|-------|-------|
| **Depends On** | ADR-0001 (kind-blind invariants: one pawn class, significance-only LOD), ADR-0012 (pool residency and warm-up, the pool validator that enforces the skin-cache value chosen here, `ProxyCreationStrategy=1`), ADR-0013 (the 4 ms animation definition and its ≤ 2.0 ms GT share, animation fallbacks), ADR-0014 (kit ISM + Nanite assembly, static mobility, instance caps), ADR-0015 (CPD dither slot `CPD_EyeMaskAlpha`, hide via `SetVisibility`), ADR-0017 (hidden-primitive flag set, spike R5), ADR-0008 (harness for the reference captures and the reflection audits) |
| **Enables** | ADR-0021 (UMG HUD 0.5 ms GT / 0.5 ms GPU lines and the "HUD after post-process at native resolution" rule under fallback F2); every content story that authors materials (Substrate decision closes AQ-08); ADR-0024 (dedicated server: removes the listen host's render cost from the server budget) |
| **Blocks** | The art mass-production gate (characters 2-8, kit variants beyond one each — art bible 8.x.14, OI-8, OI-13); character-master and kit-master material authoring (needs the Substrate and blend-mode decisions); any story that claims a performance acceptance criterion against the 3060-class reference |
| **Ordering Note** | R1, R2 and the RC-1/RC-2 captures need target-class hardware that the project does not have (AQ-18 has no owner yet). The configuration decisions (Substrate off, HWRT off, Nanite scope, uniform skin-cache value, masked master) can be implemented against the RTX 5090 machine now; only the **budget pass lines** wait for the 3060-class captures |

## Context

### Problem Statement
The game's core promise (Pillar 1) is that nobody can tell players from 40-odd NPCs by looking. That makes rendering a fairness surface as well as a cost: any setting that differs by pawn kind, team or viewer (an LOD bias, a skin-cache flag, a fallback that triggers when "important" pawns are on screen) is a tell. At the same time the scene is heavy for the reference GPU: up to 50 skinned characters with VSM shadows, a runtime-assembled Nanite city of up to 5,000 instances lit by Lumen, foliage and bushes with WPO sway, and a listen host that both simulates and renders. Five configuration questions are open or contradictory in the project today (architecture.md Engine Knowledge Gap: Substrate unknown, skin cache Exclusive, Nanite + WPO contradictory, MegaLights, hardware RT), the art bible's GPU allocation and memory/draw-call ceilings are proposals, and the CPU game-thread table exists only in architecture.md. Mass production of characters and kit is gated on spikes that need these numbers fixed first.

### Constraints
- **Pillar 1 (kind-blind):** every pawn uses the same class defaults, assets, material, LOD rule and render flags; fallbacks apply to every pawn and every client identically; nothing keys on who is on screen (art bible 8.x.8, 8.x.16; ADR-0001).
- **Engine:** UE 5.8.4; the project is a rebased Game Animation Sample (existing project, legacy materials; E1); DX12 desktop only.
- **Hardware evidence:** only RTX 5090 numbers exist (1 player + 40 NPCs: GT 9.5 ms, RT 4.0 ms, GPU 2.1 ms; 50 characters: 89.6 fps average, GT 11.2 ms, 0 % misses at 60 — graybox box-kit city, no crowd AI, no queue, no concealment probes, no 10 real connections). Reference target RTX 3060-class 1080p60 is **ASSUMED** (architecture.md).
- **Write scope (this run):** no `Config/`, `.uproject` or `project.yaml` edit; every config change below is a pending user item.
- **Data-driven principle:** gameplay numbers are tuning keys (ADR-0004); rendering settings are **not** gameplay tuning — they ship as engine config (`DefaultEngine.ini`, `DefaultScalability.ini`, device profiles) identical for every client, and client-local preferences never become tuning (architecture principle 4).

### Requirements
- Frame 16.6 ms (60 fps) at 1080p on the reference GPU; ≥ 2.0 ms GPU headroom (art bible 8.x.16).
- Animation ≤ 4 ms GT + workers, GT share ≤ 2.0 ms (SCL R12, ADR-0013); listen-host GT ≤ 12.0 ms by owner (architecture.md CPU table, LP concern 9).
- Kit GPU (Lumen + Nanite) ≤ 5.0 ms at ≤ 5,000 instances; non-Nanite draws ≈ 300-500 of the 1,500 ceiling from the Generator (R-G9).
- Pool assets resident and PSO/mip-warm before reveal; no first-draw pop for any appearance (CP-18, ADR-0012).
- A masked pawn in 3P is drawn with no mesh, shadow or RT/GI presence; the dither fade uses one per-pawn parameter path with no per-pawn material instance; `CastShadow` is never toggled (CS-R5b, ADR-0015).
- Pre-decided fallbacks that never reopen art direction and never touch character parity (art bible 8.x.16).

## Decision

### 1. Project rendering configuration (one configuration, every client)

| Setting | Decision | Today [V] | Change needed |
|---|---|---|---|
| Substrate | **Off** for the MVP and Vertical Slice; re-evaluated only at a milestone review with an R-SUB capture (Blendable GBuffer cost vs legacy on the character and kit masters) | off (no key, default 0, E1) | Pin `r.Substrate=0` explicitly in `DefaultEngine.ini` so an editor "upgrade project" prompt or template merge cannot flip it silently (**pending user**) |
| Global illumination / reflections | **Lumen software ray tracing** (mesh-SDF detail traces on), Lumen reflections | as decided (E4) | none |
| Hardware ray tracing | **Off** (`r.RayTracing=0`) — no RT shadows, no HWRT Lumen, no MegaLights; pool meshes have Support Ray Tracing off | off by default (E3) | Pin `r.RayTracing=0` explicitly (**pending user**, defensive) |
| MegaLights | **Off** (`r.MegaLights.EnableForProject=0`); the art direction is one sun + sky with few local lights | off (default, E5) | none |
| Shadows | **Virtual Shadow Maps** for sun and local lights; the kit is `Static` after assembly so its VSM pages cache (ADR-0014) | on (E4) | none |
| Anti-aliasing / resolution | **TSR**, **manual** screen-percentage mode with `sg.ResolutionQuality=100` at the reference preset (the engine default follows display resolution, E17), **dynamic resolution off** (`r.DynamicRes.OperationMode=0`) — internal resolution changes only by a pre-decided global fallback step (F2). Display resolution still differs between clients (a 1440p player's figures have more pixels): an **accepted residual** beside Pillar 1 — it is the player's hardware, not a per-pawn rule; budgets and readability checks are at 1080p | TSR, dynres off (E4, E8) | Pin `r.ScreenPercentage.Default.Desktop.Mode` to manual (**pending user**); remove `r.MSAACount=8` when config is next touched (believed inert under deferred + TSR [U]; **pending user**, cosmetic) |
| Nanite | **On** for kit modules and large opaque props (> 2k tris, no WPO, no masked); **off** for characters (classic skeletal, 4 LODs), bushes, decorative foliage (WPO sway), small props (< 1.5k tris). Nanite skeletal (R6) and Nanite WPO/foliage (R9) are not adopted | project enabled (E4) | none |
| Skin cache | **One value for every LOD of every pool mesh**, set per asset (`FSkeletalMeshLODInfo::SkinCacheUsage`), with the project default left at Exclusive. **Provisional value: `Enabled`** (skin once per frame, reuse across depth, base and VSM passes). Spike **R2** compares all-`Enabled` vs all-`Disabled` and the faster under the 2.5 ms pass line becomes the value; ADR-0012's pool validator enforces it and rejects `Auto` or mixed values | Exclusive default, no pool asset yet (E2) | none (per-asset); flipping the project default is **not** chosen |
| Unlimited bone influences | Keep `r.GPUSkin.UnlimitedBoneInfluences=True` until spike **R8**; if R8 shows any measurable cost on ≤ 4-influence meshes, turn it off (**pending user**, config + shader recompile) | on (E4) | conditional |
| Custom depth / stencil | No pawn, bush or prop primitive may set `bRenderCustomDepth` or a stencil value (art bible 8.x.11); set `r.CustomDepth=0` project-wide once a sample-content audit shows nothing retained uses it (E15) | engine default | **pending user** after audit |
| Texture streaming | On; pool size **1,000 MB** = the engine's `sg.TextureQuality=3` value (E6), pinned for every client; `r.Streaming.LimitPoolSizeToVRAM=1` so 6-8 GB cards are not over-committed (TQ 3 sets it to 0, E18); virtual textures off for the pool; pool textures pre-streamed by ADR-0012's warm-up | engine default at TQ 3 | Pin `r.Streaming.PoolSize=1000` and `r.Streaming.LimitPoolSizeToVRAM=1` in `DefaultEngine.ini [SystemSettings]`, which outranks every `sg.*` group (E18) (**pending user**) |
| PSO | Component PSO precaching on (engine default) with `r.PSOPrecache.ProxyCreationStrategy=1` (ADR-0012 asserts it); a bundled PSO cache is added only if RC-1/RC-2 record first-use hitches after warm-up | defaults (E11) | none now |
| Static lighting | Off (`r.AllowStaticLighting=0`); no lightmap UVs | as decided (E4) | none |

**Scalability preset (fixed for the MVP).** Budgets are defined and measured at one shipped preset, the **reference preset**: every `sg.*` group at 3 (Epic) at 1080p, manual screen percentage 100, TSR. The client applies it on **every launch** through `UGameUserSettings` — `SetOverallScalabilityLevel(3)`, the resolution-quality value, then `ApplySettings(false)`, ignoring values saved in the user-editable `GameUserSettings.ini` (E18) — from client-local presentation code in `StealthPresentation` (ADR-0007), and the MVP exposes **no** user control over groups that change what is visible — view distance, foliage, shadows, global illumination, reflections, effects. A later settings screen (VS) may expose only resolution, frame-rate cap, V-sync and display options unless a new ADR shows a visibility-neutral split. Reason: a player who lowers shadow or foliage quality sees pawns differently from the others (a fairness asymmetry the Pillar-1 leak tests cannot see); fallbacks below change the preset for **every** client together.

### 2. Kind-blind rendering invariants (enforced, not advisory)

1. **One render-flag set on every pawn primitive** (class defaults of `AStealthPawn` and every attachable pawn primitive, ADR-0010). Pinned values: `bRenderCustomDepth=false`, `CustomDepthStencilValue=0`, `LDMaxDrawDistance=0` and `MinDrawDistance=0` (no per-pawn draw-distance cull), **`bAllowCullDistanceVolume=false`** (a Cull Distance Volume would otherwise set each component's cull distance from its bounds, so appearances could cull differently, E19), `BoundsScale=1`, `ForcedLodModel=0`, no min-LOD override, empty per-component `SkinCacheUsage` array (ADR-0012), `bEnableUpdateRateOptimizations=false` (ADR-0013); the hidden-primitive flags `bCastHiddenShadow`, `bAffectIndirectLightingWhileHidden` and `bRayTracingFarField` stay false as ADR-0017 §6 requires. **Audit:** `StealthGame.Pawn.RenderFlagParity` (ADR-0008 T1) compares **every render-relevant `UPROPERTY` by reflection** on `UPrimitiveComponent`, `UMeshComponent`, `USkinnedMeshComponent` and `USkeletalMeshComponent` (rendering, LOD, lighting, shadow, culling, ray-tracing and streaming properties — e.g. `CastShadow`, `bCastDynamicShadow`, `bCastContactShadow`, `bCastFarShadow`, `bReceivesDecals`, `bRenderInDepthPass`, `bVisibleInRayTracing`, `bAffectDistanceFieldLighting`, `bNeverDistanceCull`, `LightingChannels`, `StreamingDistanceMultiplier`, `MinLodModel`/`bOverrideMinLod`) across the pawn CDO, a spawned NPC pawn and a spawned player pawn, minus an explicit allowlist of properties that legitimately differ per instance (transform, custom primitive data, visibility/hidden state, and the mesh/material asset references that ADR-0012 governs). **Ownership:** this audit owns cross-pawn *equality* of every render property; the *values* of the three hidden-primitive flags stay asserted by the ADR-0003 allowlist audit that ADR-0017 §6 names, and `RenderFlagParity` does not re-assert them.
2. **One asset rule on every pool row** (ADR-0012 validator, extended by this ADR): Nanite support off, Support Ray Tracing off, the skin-cache value of §1 on every LOD, one shared `USkeletalMeshLODSettings` asset for LOD screen sizes and reduction settings, ≤ 4 influences, one material slot with an MIC of `M_Character_Master`.
3. **Settings are global only.** No code path sets a CVar, scalability group, LOD bias, screen percentage or material parameter per pawn, per team, per viewer or per on-screen composition. The only per-pawn rendering inputs are the camera mask's `CPD_EyeMaskAlpha` and visibility (ADR-0015) and Concealment's client latch (ADR-0017), both derived from geometry/carriers, never from kind.
4. **No load-adaptive scaling.** Anything that feeds back from **measured cost or frame time** is forbidden: dynamic resolution, the animation budget allocator (ADR-0013), adaptive significance budgets, frame-time-driven quality or LOD changes. They make cost-driven differences correlate with what is in view. **Allowed:** ADR-0013's deterministic per-tier count caps (A ≤ 8, B ≤ 14, C ≤ 28; overflow ordered by figure screen height with the client-local random `TierTieKey` for ties) and its fallback (2) that lowers those caps globally — fixed functions of geometry and a random key, identical for every pawn kind, not feedback from cost.
5. **No instanced or Mass characters** (art bible 8.x.9): `MassGameplay`/`MassCrowd` stay disabled; instanced skinned meshes are not used for pawns.
6. **No outlines, stencil or custom-depth effects** for any gameplay reading (art bible 8.x.11); teammate markers are screen-space UI (ADR-0021).

### 3. The shared character master material (`M_Character_Master`)

- **Blend mode Masked, Default Lit, legacy (non-Substrate) shading**, opacity mask = `DitherTemporalAA` of `CPD_EyeMaskAlpha` (custom primitive data slot reserved by ADR-0015; default value 1.0 = fully drawn), opacity-mask clip 0.5; no translucency, no WPO, no pixel depth offset, no static switches that multiply permutations, `Used with Skeletal Mesh` only (no morph targets, no spline meshes). This supersedes the word "opaque" in art bible 8.x.6 for the character master only (GDD sync item below): the master is opaque **in appearance** at α = 1 and needs Masked only so the camera's fade can dither every pawn identically.
- **Budget:** ≤ 150 pixel instructions, ≤ 4 texture samples (art bible 8.x.6, ASSUMED); the masked path's extra cost is verified by **RC-3** (≤ 0.3 ms GPU at 60 characters vs an opaque twin built only for the measurement). If RC-3 fails, the fallback is a **global** decision between (a) accepting the cost inside the 2.5 ms character line or (b) shortening the fade to a hard cut (α ∈ {0, 1}) with an opaque master — a camera-design change routed to ADR-0015 / Camera GDD owners, never a per-pawn material swap.
- Attachable pawn props and VFX materials reserve the same CPD slot (ADR-0015).
- Kit master: opaque, Nanite-friendly, no masked/WPO/PDO, ≤ 120 instructions; foliage/bush master: masked leaves at LOD2 cards only, shared WPO sway function, ≤ 140 instructions, non-Nanite (art bible 8.x.6, 8.x.11). Rustle WPO on bush leaf tiers (ADR-0017) counts in the non-Nanite base-pass line.

### 4. Budget tables

All values are **ASSUMED allocations** at the reference preset on an RTX 3060-class PC at 1080p until the named capture replaces them. "p95" = 95th percentile of per-frame values over 60 s after a 10 s warm-up (ADR-0013's protocol).

**4.1 Frame.** 16.6 ms (60 fps), every thread and the GPU independently.

**4.2 CPU game thread on the listen host (binding case; LP concern 9).** Sum ≤ **12.0 ms p95**, leaving ≥ 4.6 ms for render-thread sync, GC and spikes. This ADR **allocates** the lines; each owner ADR implements and measures its own:

| Line | GT budget (ms) | Owner ADR | Notes |
|---|---|---|---|
| Animation, GT share (the 4 ms line counts GT + workers) | 2.0 | ADR-0013 | `C_anim` ≤ 4.0 total. **Listen host (closeout D3, 2026-10-25):** the host tiers every non-locally-controlled pawn with the same significance bands and uniform `TickPose` escalation clients use (no host-only rate-1 posing for remote pawns); the line is confirmed in R3 with the listen host plus 9 remote clients |
| CMC: ~50 server-simulated NPCs + simulated proxies | 3.0 | ADR-0010 | reported separately from animation (SCL R12); sized for ~40 NPCs, projects to ~3.75 ms at 50 — working value, re-verify at 60 characters in R3/RC-1 |
| Crowd AI + nav (server) | 1.5 | ADR-0016 | p99 1.5, mean ≤ 0.8 |
| Queue drain, resolvers, Roster, staged writes, CPA ledger | 0.5 | ADR-0002 (ADR-0012's 0.2 is inside it) | ADR-0011's Input & Interaction Framework (IIF: command ingress, stage 2, condition sweep) and the `ServerTeamPing` handler are charged here and only here (amendment 2026-10-25b) |
| Replication (NetDriver, 10 connections, ~60 pawns, cull) | 1.5 | ADR-0003 | re-verify at 60 pawns |
| Camera eye-mask traces (3P) + concealment/objectives misc (IIF excluded; it is in the Queue/Roster line) | 1.0 | ADR-0015 (0.5 eye mask) + ADR-0017/0020 (0.5 shared) | Eye-mask worst case is **295 async traces per frame** (59 other characters x 5 probes at `NpcCount` 50, camera GDD F3), about **1.5 ms** of physics work at ~5 us per trace, almost all on task threads: the **GT share target stays 0.5 ms** (submission + result handling, `EyeMaskTracesPerFrameMax` 160, so the worst case drains in two frames); typical about 75 traces. If the spike shows GT over 0.5 ms the ADR-0015 §4.3.6 synchronous fallback applies (C3) |
| UMG HUD + markers | 0.5 | ADR-0021 | |
| Engine/world tick, layout runtime, telemetry, other | 2.0 | **ADR-0022** (ADR-0014's 0.1 layout line and ADR-0023's 0.15 telemetry line are inside it) | the residual line; GC is outside the 12.0 |
| **Total** | **12.0** | | |

A **client** (non-host) has no server lines; its GT ceiling is the same 12.0 ms p95 and its measured composition is reported, not allocated. A line that overruns borrows from no other line: the overrun triggers that owner's fallbacks (§5), and only a re-allocation recorded in this ADR moves milliseconds between owners.

**4.3 Render thread and RHI thread.** ≤ **12.0 ms p95** each on the client reference scene (parallel to the game thread; 5090 evidence RT 4.0 ms). The draw-call ceiling below is the main lever.

**4.4 GPU, client reference scene (art bible 8.x.16, adopted as the allocation).**

| Pass group | GPU (ms) | Gate |
|---|---|---|
| Lumen scene, GI, reflections, Nanite visibility/raster/shading (kit ≤ 5,000 instances) | 5.0 | R1 |
| Character skinning + character shadow passes (60 characters, VSM) — includes the masked-master depth/shadow overhead (RC-3) | 2.5 | R2 |
| Non-Nanite base pass + shadows: characters' base pass, props, foliage, bushes (≤ 800k tris) | 2.0 | RC-2 |
| VSM for the kit | 1.2 | RC-2 |
| Sky, atmosphere, haze | 0.5 | RC-2 |
| Post: grade, vignette, tone map, TSR | 1.8 | RC-2 |
| UMG HUD (drawn after post-process, native resolution) | 0.5 | ADR-0021 |
| Prepass, HZB, translucency, misc | 0.9 | RC-2 |
| **Total allocated** | **14.4** | |
| **Required headroom** | **≥ 2.0** (2.2 allocated) | pass line of RC-2 |

The pass line is the headroom row, not any single line (art bible 8.x.16).

**4.5 Geometry, draw calls, memory, disk.**

| Budget | Value | Status | Measured by |
|---|---|---|---|
| Draw calls, main view, non-Nanite (characters, props, foliage, bushes) | ≤ 1,500 (Generator share ≈ 300-500, R-G9) | PROPOSED (not in project config) | `stat RHI` / `stat SceneRendering` mesh draw calls, RC-2 |
| Nanite unique materials in view | ≤ 12 | ASSUMED | Nanite material visualisation, R1 |
| Non-Nanite tris on screen before shadows | ≤ 800k (characters ≈ 266k design-density case, OI-13) | ASSUMED | RC-2 |
| Runtime kit + decoration instances | ≤ 5,000 (target ≤ 4,000) | ASSUMED (ADR-0014 enforces the cap) | R1 |
| Texture streaming pool | ≤ 1.0 GB (`r.Streaming.PoolSize=1000`) | PROPOSED; equals the engine TQ 3 default (E6) | `stat Streaming`, no over-budget frames in RC-2 |
| Character textures (pool, resident) | ≤ 40 MB (≈ 32-34 MB for 8 models) | VERIFIED-P (art bible) | ADR-0012 validator |
| Kit / bush+foliage / props textures | ≤ 96 / ≤ 20 / ≤ 40 MB | ASSUMED | content audit |
| Total GPU memory | reported only (no ceiling until R1/R2) | open | `rhi.DumpMemory` in RC-2 |
| Cooked package, installed | ≤ 3 GB | PROPOSED (producer to confirm) | R7 + first package |

The three PROPOSED rows (draw calls, streaming pool, package) are this ADR's working ceilings; recording them in project config (`technical-preferences.md` / `project.yaml` performance keys) is a **pending user item**.

### 5. Pre-decided fallbacks (ordered; global; never per pawn)

Applied identically to every client and every pawn, decided now so a failed spike does not reopen art direction or Pillar 1. Engaging one is the technical-director's decision on the technical-artist's / performance-analyst's evidence; each is a shipped-config change (pending user while this run cannot write `Config/`).

**GPU (art bible 8.x.16):**
- **F1 — Lumen one step:** `sg.GlobalIlluminationQuality` and `sg.ReflectionQuality` 3 → 2 for every client, with a project `DefaultScalability.ini` override of `[GlobalIlluminationQuality@2]` that **keeps `r.Lumen.TraceMeshSDFs.Allow=1`**: the engine's level 2 also turns mesh-SDF detail traces off and shrinks the surface cache (E7), which would change the thin-wall GI behaviour the kit is authored against (art bible 8.x.10). With the override the step changes the screen-probe downsample (16 → 32), surface-cache atlas and irradiance format. Expected about 1 ms of the 5.0 ms line; R1 measures both variants, and if the override saves too little the technical-director may take engine level 2 as written, in which case kit thin-wall GI leaks are re-checked with the OI-6 re-measure. `sg.ViewDistanceQuality` **never goes below 2**, because level 1 sets `r.SkeletalMeshLODBias=1` (E7) and would change character LODs — on the never-fallback list. OI-6 lighting outcomes are re-measured after the step.
- **F2 — internal resolution:** screen percentage 100 → 85, then → 77, TSR upscaling; HUD stays native (drawn after post-process); thumbnail readability (art bible 3.2) re-verified at the new internal resolution.
- **F3 candidate (not pre-decided; needs art-director sign-off because it changes the look):** `sg.GlobalIlluminationQuality=1` (Lumen Irradiance Volume, the Lumen Lite-class mode, E7) with `sg.ReflectionQuality=1` (Lumen reflections off).

**CPU game thread (owner fallbacks, in this order):** (C1) ADR-0013 §5 animation fallbacks; (C2) ADR-0016's global-only crowd AI cuts; (C3) ADR-0015 §4.3 synchronous-trace fallback for the eye mask; (C4) only if C1-C3 do not close the gap: a lower NPC count default — a **design change** routed through `/propagate-design-change` and the game-designer, never an automatic runtime reduction.

**Never used as a fallback:** character LOD thresholds, animation tiers or triangle ceilings for some pawns; dropping character shadows; per-viewer or per-team settings; dynamic resolution; any adaptive scaling keyed to who or how many are on screen (art bible 8.x.16; §2.4).

### 6. Measurement protocol and gates

- **Reference scenes.** *Client scene:* a golden Generator seed at the 5,000-instance cap, 60 characters (owner + 9 remote players + 50 NPCs) in the densest legal street view (OI-13 rule once written), sun at the OI-4 azimuth, reference preset. *Listen-host scene:* the same seed with the host playing and **9 real client connections** (multi-process, ADR-0008 T3), crowd AI, queue, concealment and camera mask live.
- **Harness.** `StealthTestHarness` scenario `-StealthHarnessScenario=PerfReference` (uses ADR-0008's T3 multi-process launcher but runs **on demand on reference hardware**, not as a nightly gate — an ADR-0008 amendment recorded under Related) drives a scripted camera path and writes a JSON summary; capture with Unreal Insights `-trace=cpu,gpu,frame,bookmark` (named scopes per owner line of §4.2) plus the CSV profiler for p95; `stat unit`, `stat gpu` and `ProfileGPU` for spot checks. Cooked **Development** standalone builds only — PIE and editor numbers (e.g. the 43 ms 3-instance PIE GT) are not evidence, and component PSO precache is off in the editor (E11). The prototype's `-StealthPerfSeconds=N` hook is development-only.
- **Gates.** R1 + R2 + R3 lift the mass-production gate (art bible 8.x.14). RC-1 + RC-2 + RC-3 are this ADR's acceptance captures. A failed spike triggers the fallbacks of §5 and is re-run under the fallback before the gate lifts. Numbers are recorded in `production/qa/perf/` with hardware, driver, build and seed.
- **Non-gating spikes owned here:** R6 (Nanite skeletal: adopt only if ≥ 20 % cheaper and no motion-matching regression), R7 (Nanite disk footprint vs 3 GB), R8 (unlimited bone influences), R9 (Nanite WPO / foliage). R4 is ADR-0014's; R5 is ADR-0017's.

### Architecture Diagram

```
                       ONE shipped configuration (DefaultEngine.ini + reference scalability preset)
                       Substrate off · Lumen SW · HWRT off · VSM · MegaLights off · TSR 100% · dynres off
                                               │ identical on every client (no per-pawn/per-viewer path)
         ┌─────────────────────────────────────┼──────────────────────────────────────┐
         ▼                                     ▼                                      ▼
  Kit (ADR-0014)                       Characters (ADR-0012/0013)              Bushes/foliage/props
  ISM per mesh×quarter, Nanite,        classic SK, 4 LODs, shared LOD           non-Nanite, HISM/ISM,
  Static after assembly, ≤5,000        settings, skin-cache value (R2) on       WPO sway, LOD2 cards
  GPU line 5.0 ms (R1)                 every LOD, M_Character_Master (Masked     part of 2.0 ms line
                                       dither, CPD_EyeMaskAlpha) — 2.5 ms (R2)
                                               │
                     per-pawn inputs allowed:  CPD alpha + SetVisibility (Camera ADR-0015)
                                               client latch (Concealment ADR-0017)
                                               ── nothing else ──
  Budgets: GT 12.0 (owner table §4.2) │ RT/RHI ≤ 12.0 │ GPU 14.4 + ≥2.0 headroom │ memory/draw/package §4.5
  Over budget ─► §5 fallbacks F1→F2 (GPU), C1→C3 (CPU), global only ─► re-run capture
```

### Key Interfaces

This ADR defines configuration and budgets, not runtime APIs. The contracts other code depends on:

- **`StealthGame.Pawn.RenderFlagParity`** (T1 reflection audit, ADR-0008): compares the §2.1 field list across the pawn CDO, a spawned NPC pawn and a spawned player pawn, and across every attachable pawn primitive class; fails on any difference.
- **Pool validator extension** (`StealthGameEditor`, ADR-0012): rejects a pool row whose mesh has Nanite enabled, Support Ray Tracing on, a skin-cache value other than the §1 value on any LOD, a LOD-settings asset other than the shared one, more than one material slot, or a material not derived from `M_Character_Master`.
- **Material lint** (T0, ADR-0008): fails when a material parameter, static switch, material function or MPC entry name contains `Team`, `Role`, `IsPlayer`, `IsNPC`, `Ally` or `Local` (art bible 8.x.6), or when any pawn/bush/prop primitive asset sets custom depth or stencil.
- **Reference preset applier** (`StealthPresentation`, client-local): applies the reference preset on every launch (ignoring saved user values) and after any fallback config change; asserts at startup (non-Shipping `ensure`; Shipping `UE_LOGF(LogStealthPresentation, Error, …)` per ADR-0007) that `r.Substrate`, `r.RayTracing`, `r.DynamicRes.OperationMode`, the screen-percentage mode and value, `r.Streaming.PoolSize`, `r.Streaming.LimitPoolSizeToVRAM` and the `sg.*` groups have the shipped values.
- **Insights scopes**: one named CPU scope per §4.2 line (`Stealth.Budget.<Line>`) declared by each owner so RC-1 sums lines without guessing.

### Implementation Guidelines

1. Every pawn primitive **must** take its render flags from the class default; code **must never** set a render flag, LOD bias, skin-cache usage or material on one pawn based on kind, team, binding or viewer.
2. Every pool mesh **must** carry the same skin-cache value on every LOD (provisional `Enabled`; R2 decides); `Auto` and per-component overrides **must not** be used.
3. Characters, bushes, decorative foliage and small props **must not** enable Nanite; kit modules and large opaque props (> 2k tris, no WPO, no masked) **must**.
4. `M_Character_Master` **must** be Masked with the `CPD_EyeMaskAlpha` dither as its only opacity input; no per-pawn MID **may** be created for any pawn.
5. Code **must never** enable dynamic resolution, MegaLights, hardware ray tracing or Substrate, or change a scalability group at runtime outside the shipped preset applier.
6. Performance claims **must** come from cooked Development standalone captures on the reference preset; PIE/editor numbers **must not** be cited as budget evidence.
7. A fallback **must** be applied to every client through shipped config, never through gameplay tuning (ADR-0004) or a per-client heuristic.
8. Each owner ADR's code **must** declare its `Stealth.Budget.<Line>` Insights scope.
9. Config keys this ADR pins are changed only by a user-approved config change recorded against this ADR.

## Alternatives Considered

### Alternative 1: Substrate on (Blendable or Adaptive GBuffer)
- **Description**: Enable `r.Substrate=1` now, as Epic recommends for new projects, and author the character, kit and foliage masters as Substrate slabs.
- **Pros**: Aligns with the 5.7+ direction ("legacy material system: migrate to Substrate"); avoids a later migration; richer layering if the art direction ever needs it.
- **Cons**: The project is an existing project whose sample content uses legacy materials (E1); enabling it is an editor-restart, whole-project shader rebuild with auto-conversion of every sample material; instruction budgets (art bible 8.x.6) and the masked-dither path (ADR-0015 R-4) are verified only on the legacy path; Adaptive GBuffer costs more per pixel and its 3060-class cost is unmeasured; the "Held Breath" art direction needs no multi-layer materials.
- **Rejection Reason**: Cost and risk with no art-direction benefit at MVP scope; recorded for re-evaluation with an R-SUB capture at a milestone review.

### Alternative 2: Flip the project skin-cache default to Inclusive (engine default)
- **Description**: Set `r.SkinCache.DefaultBehavior=1` so every skeletal mesh uses the skin cache unless it opts out.
- **Pros**: One config line; matches engine defaults; no per-asset setting to police.
- **Cons**: A config change outside this run's scope; affects every skeletal mesh including retained sample content; uniformity across pool meshes would still need the validator (an asset can opt out); it pre-empts R2, which may show the uncached path is cheaper at these vertex counts.
- **Rejection Reason**: Per-asset uniform value plus validator gives the same parity guarantee, keeps the decision with R2, and needs no config change.

### Alternative 3: Hardware ray tracing (HWRT Lumen, optionally MegaLights)
- **Description**: Enable `r.RayTracing=1`; Lumen uses hardware traces; MegaLights becomes available.
- **Pros**: Higher-quality GI/reflections; skinned meshes can appear in ray-traced Lumen.
- **Cons**: Ray-tracing scene updates cost scales with instances ("significant scene update costs for scenes with more than 100k instances", E3) and with 50 skinned meshes (forces them into the skin cache); 3060-class RT throughput is the weakest part of that card; a new leak surface for hidden pawns (ADR-0017 R5 would have to cover RT far-field and reflections); MegaLights is unnecessary for a sun-lit city.
- **Rejection Reason**: Budget and leak-surface cost with no gameplay benefit; software Lumen is the art bible's stated basis (8.x.10).

### Alternative 4: Load-adaptive scaling (dynamic resolution and/or AnimationBudgetAllocator-style budgets)
- **Description**: Let the engine lower internal resolution or animation work when the frame is heavy.
- **Pros**: Holds frame rate automatically on weaker hardware.
- **Cons**: Cost-driven changes correlate with what is on screen; the animation allocator is player-keyed by default (ADR-0013 E1/E6); dynamic resolution changes figure pixel heights frame to frame, defeating the thumbnail-readability verification (art bible 3.2) and making the visual read differ between clients with different loads.
- **Rejection Reason**: Violates §2.4 and art bible 8.x.16 ("any adaptive scaling keyed to who is on screen"); fixed global steps (F2) achieve the saving deterministically.

### Alternative 5: Nanite skeletal meshes for characters
- **Description**: Enable Nanite on pool meshes (supported in 5.8.4, E9).
- **Pros**: Automatic LOD, potentially cheaper VSM rendering of characters.
- **Cons**: Production status and motion-matching interaction unknown; 12k-tri characters are below where Nanite pays off; CPD dither and masked behaviour on Nanite skinned are [U] (ADR-0015 R-4).
- **Rejection Reason**: Unproven; spike R6 may revisit with a ≥ 20 % win and no regression.

### Alternative 6: Opaque character master with per-pawn material swap during fades
- **Description**: Keep the master opaque and swap a pawn to a masked variant only while 0 < α < 1.
- **Pros**: No masked cost on fully visible pawns.
- **Cons**: A material swap re-creates the proxy and requests PSOs at fade time (hitch risk, E11), creates per-pawn material state, and makes the cost of a fade a per-pawn event.
- **Rejection Reason**: ADR-0015 already rejected per-pawn MIDs for Pillar reasons; one masked master is simpler and uniform; RC-3 bounds its cost.

## Consequences

### Positive
- The five open rendering questions (Substrate, skin cache, HWRT, MegaLights, Nanite scope) are closed with explicit owners and evidence paths; AQ-08 resolves.
- One configuration and one flag set for every pawn, enforced by tests and validators rather than review, closes the rendering half of Pillar 1's "no kind-keyed branch" principle.
- The CPU, GPU, memory and draw-call budgets sum to their frames, each line has an owner, and every fallback is decided before a spike fails.
- Budgets are measured one way (cooked Development, reference preset, p95, Insights scopes), so captures from different owners are comparable.

### Negative
- Substrate-off keeps the project on the legacy material path that Epic is moving away from; a later migration (if ever) costs a shader rebuild and re-validation of every master.
- The masked character master costs some depth-prepass and shadow-pass work on every pawn every frame, even when no fade is running (bounded by RC-3).
- No user-facing quality options for visibility-affecting groups in the MVP: players on weaker hardware cannot trade shadows/foliage for frame rate (they can only change resolution once a settings screen exists).
- All budgets remain ASSUMED until 3060-class captures exist, and nobody owns that hardware yet (AQ-18); the mass-production gate may stall.
- Several config pins depend on the user (no `Config/` write in this run).

## Risks
- **R-1 Listen-host GT over 12 ms.** 5090 evidence is already 11.2 ms without AI, queue, probes or real connections. *Mitigation*: owner fallbacks C1-C3 in order; the dedicated server (ADR-0024) removes rendering from the authority at Alpha; C4 (lower NPC default) is a design decision, never automatic.
- **R-2 Lumen + 5,000 runtime ISM instances over 5.0 ms (R1).** *Mitigation*: F1, then F2; target ≤ 4,000 instances (R-G9); ADR-0014's `Movable` + VSM-invalidation-off fallback if runtime `Static` ISM misbehaves with caching (ADR-0014 R-6).
- **R-3 Skin-cache mode (R2) and VSM.** Either mode could miss 2.5 ms at 60 characters. *Mitigation*: R2 picks the faster; LOD screen sizes (shared settings asset) lower tri counts for distant pawns uniformly; F2.
- **R-4 Masked master cost or dither artefacts (RC-3).** *Mitigation*: global choice in §3 (absorb or hard cut), never per pawn.
- **R-5 Skin-cache memory.** 5.8 has no skin-cache memory limit (E16), so no mesh silently falls back to the uncached path; the cost is memory. *Mitigation*: R2 records skin-cache memory at 60 characters with `r.SkinCache.PrintMemorySummary` into the §4.5 GPU-memory row.
- **R-10 Dither artefacts under TSR** (E14): temporal dither can ghost or shimmer on moving skinned meshes and dithered shadows are noisy. *Mitigation*: RC-3 includes a visual check (a moving pawn fading in a crowded street, retained in `production/qa/evidence/`); if it fails, the §3 global choice applies.
- **R-6 PSO hitches after warm-up** for kit, foliage or VFX permutations not covered by ADR-0012's pool warm-up. *Mitigation*: RC-1/RC-2 count hitches > 33 ms after warm-up; a bundled PSO cache is added if any are found.
- **R-7 Sample content pulls in custom depth, Substrate-only or RT-only features** (E15). *Mitigation*: sample-content audit before the first package (art bible 8.x.12).
- **R-8 Target hardware assumption wrong** (players' cards weaker or the 3060 assumption replaced). *Mitigation*: budgets are re-based by re-running RC-1/RC-2 on the new reference; the fallback order stays.
- **R-9 Editor or template upgrade flips Substrate/RT** because their values are implicit defaults today. *Mitigation*: explicit pins (pending user) and the startup assert of the preset applier.

## GDD Requirements Addressed

TR IDs (architecture.md traceability, ADR-022 row): **TR-cpa-005** (CP-18 row), **TR-scl-008** (SCL R12 row), **TR-citygen-004** (R-G9 row), **TR-con-006** (art bible 8.x.10-8.x.11 bush/hidden-flag row), **TR-camera-003** (CS-R5 row).

| GDD System | Requirement | How This ADR Addresses It |
|------------|-------------|--------------------------|
| art-bible.md 8.x.2 | Budget summary: frame, animation, AI, tris, texture pool ≤ 1.0 GB, draw calls ≤ 1,500, Nanite materials ≤ 12, instances ≤ 5,000, package ≤ 3 GB | §4.4-4.5 adopt them with status (VERIFIED-P / ASSUMED / PROPOSED) and a measurement tool each; PROPOSED rows flagged for project config |
| art-bible.md 8.x.3-8.x.4 | Nanite for kit and large props; characters, bushes, foliage, small props off; Nanite skeletal not assumed | §1 Nanite row; Implementation Guideline 3; R6/R9 non-gating |
| art-bible.md 8.x.5 | Texture memory per category; streaming on; VT off for the pool | §1 streaming row; §4.5 |
| art-bible.md 8.x.6 | One character master, ≤ 150 instructions, no keyed branches; Substrate state to be confirmed | §3 (Masked dither master — GDD sync item); Substrate off (§1); material lint (Key Interfaces) |
| art-bible.md 8.x.7 | Skin cache conflict; unlimited influences flag | §1 skin-cache row (uniform per-asset value, R2 decides); R8 for the influences flag |
| art-bible.md 8.x.8-8.x.9 | Tiering keyed by screen height only; no Mass/instanced characters | §2.4-2.5; animation itself owned by ADR-0013 |
| art-bible.md 8.x.10-8.x.11 | Kit ISM/Nanite/Static/Lumen SW constraints; bush concealment flags; no custom depth/stencil | §1 (Lumen SW, VSM, Static kit), §2.1 flag set, §2.6; ADR-0014/0017 own the mechanisms |
| art-bible.md 8.x.14-8.x.16 | Spikes R1-R9; mass-production gate R1-R3; whole-frame GPU allocation; pre-decided fallbacks F1/F2; never-fallback list | §4.4, §5, §6 |
| shared-character-locomotion.md R12 | Animation ≤ 4 ms; CMC proxies and server AI movement reported separately; fallbacks equal for every pawn | §4.2 (2.0 GT animation line, 3.0 CMC line), §5 C1; ADR-0013 owns the definition |
| procedural-city-generator.md R-G9 | Kit GPU ≤ 5.0 ms; ≤ 5,000 instances; non-Nanite draws ≈ 300-500 of 1,500; client build ≤ 3 s | §4.4 R1 line, §4.5 rows; build-time budgets stay with ADR-0014 |
| character-pool-appearance.md CP-18 | All pool entries resident, PSO and mip-warm, skin cache ready before reveal; no pop | §1 PSO and streaming rows, skin-cache uniformity enforced by the validator; mechanism owned by ADR-0012 |
| camera-system.md CS-R5 | Masked pawn fully hidden (no mesh/shadow/RT/GI); dither via per-pawn value with one path; never toggle `CastShadow` | §2.1 flag set, §3 master material with `CPD_EyeMaskAlpha`; HWRT off removes the RT path entirely; mechanism owned by ADR-0015 |
| architecture.md (LP concern 9) | CPU game-thread budget table by owner, ≤ 12 ms on the listen host | §4.2 |

## Performance Implications
- **CPU**: This ADR allocates rather than consumes. Its own code (preset applier, asserts) runs at startup only. The masked master adds no CPU cost.
- **GPU**: Masked master adds depth-prepass/shadow work on characters (RC-3 ≤ 0.3 ms); HWRT off and MegaLights off remove their costs; the skin-cache choice is R2's.
- **Memory**: Streaming pool 1.0 GB; pool resident ≤ 40 MB; skin cache (if Enabled) adds per-pawn skinned vertex buffers (R2 records it).
- **Load Time**: No change beyond ADR-0012's warm-up; Substrate-off avoids a project-wide shader rebuild.
- **Network**: None.

## Migration Plan
The prototype (`Source/StealthGame`, `L_Prototype`, `M_CrowdTint`) is feasibility evidence only and is not migrated. When production content starts: (1) the user applies the pending config pins; (2) `M_Character_Master` is authored Masked per §3 before any pool MIC; (3) the pool validator extension and `RenderFlagParity` audit land with the first pool asset (ADR-0012 story); (4) the `PerfReference` harness scenario lands with ADR-0008's T3 harness; (5) R1/R2/R3 and RC-1..RC-3 run on 3060-class hardware; numbers replace the ASSUMED values in this ADR by revision, and the mass-production gate lifts.

## Validation Criteria
- RC-1: listen-host GT p95 ≤ 12.0 ms with every §4.2 line within its budget (Insights scopes), 9 real clients, cooked Development, 3060-class.
- RC-2: client GPU total ≤ 14.6 ms p95 (≥ 2.0 ms headroom), RT and RHI ≤ 12.0 ms p95, non-Nanite draw calls ≤ 1,500, no streaming-pool over-budget frames, zero frames > 33 ms after warm-up.
- RC-3: masked master overhead ≤ 0.3 ms at 60 characters; dither visible and stable on the classic skinned path, with no objectionable TSR ghosting/shimmer or shadow noise in a retained capture (visual check signed by the technical-artist).
- R1, R2 pass lines (art bible 8.x.14) on 3060-class hardware; R3 per ADR-0013.
- `StealthGame.Pawn.RenderFlagParity` passes; pool validator rejects a seeded mixed-skin-cache row, a Nanite-enabled row and an RT-enabled row (each watched to fail once); material lint fails on a seeded `IsPlayer` parameter.
- Startup assert reports the shipped values for every pinned key in a cooked build.

**Gate results (2026-10-24, one combined sub-agent call):** TD-ADR **CONCERNS** (10 items) → all **REVISED** in this draft: F1 rewritten (engine level 2 also drops mesh-SDF traces; an override keeps them; view distance never below 2); §2.4 narrowed to cost-feedback scaling with ADR-0013's count caps explicitly allowed; R-5 rewritten (no skin-cache memory limit in 5.8); screen percentage pinned to manual with display resolution as an accepted residual; render-flag audit by reflection with an allowlist and `bAllowCullDistanceVolume=false`; one owner per audit (equality here, hidden-flag values in ADR-0003/0017); the Shipping "telemetry counter" replaced by `UE_LOGF`; TR IDs added; R2 redefinition and the `PerfReference` ADR-0008 amendment recorded as sync items. Engine validation (unreal-specialist lens) **MINOR NOTES**: E14 upgraded to [V] and a TSR dither visual check added to RC-3; E7 completed; E16-E19 added; the preset applier re-applies the preset on every launch; streaming pins moved to `[SystemSettings]`. Remaining [U]: `r.MSAACount` inertness, TSR history at the F2 steps, saved-user-settings load order.

## Related
- Depends on: ADR-0001, ADR-0008, ADR-0012, ADR-0013, ADR-0014, ADR-0015, ADR-0017. Allocates GT lines owned by ADR-0002, ADR-0003, ADR-0010, ADR-0011, ADR-0016, ADR-0020, ADR-0021. Enables ADR-0021, ADR-0024.
- Design: `design/art/art-bible.md` 8.x.2-8.x.17; `design/gdd/shared-character-locomotion.md` R12; `design/gdd/procedural-city-generator.md` R-G9; `design/gdd/character-pool-appearance.md` CP-18; `design/gdd/camera-system.md` CS-R5.
- **GDD/doc sync items (not applied by this ADR):** (1) art bible 8.x.6 calls `M_Character_Master` "opaque"; this ADR makes it **Masked** (dither only) — art-director to amend 8.x.6 and 8.x.11's "no custom depth" note stays; (2) art bible 8.x.15 "Substrate on or off" and "draw-call budget and memory ceiling" are answered here (off; PROPOSED ceilings adopted); (3) architecture.md AQ-08 resolves to "off"; (4) `docs/engine-reference/unreal/modules/rendering.md` remains stale on Nanite skeletal/WPO (engine-reference is out of this run's write scope); (5) art bible 8.x.14 defines spike R2 as `r.SkinCache.DefaultBehavior` 0 vs 1 — this ADR redefines it as per-asset `SkinCacheUsage` `Enabled` vs `Disabled` on every pool LOD with the project default left Exclusive (amend 8.x.14 and 8.x.7); (6) art bible 8.x.16 F1 assumed one plain scalability step — this ADR keeps mesh-SDF traces on through a `[GlobalIlluminationQuality@2]` override and never lowers view distance below level 2; (7) **ADR-0008 amendment**: T3 gains an on-demand `PerfReference` scenario run on reference hardware, outside the nightly gate; (8) ADR-0017 §6 / ADR-0003: their allowlist audit keeps the hidden-flag *values*, `RenderFlagParity` owns cross-pawn equality — cross-reference both ways.
- **Pending user items (config; none applied):** pin `r.Substrate=0` and `r.RayTracing=0`; pin `r.Streaming.PoolSize=1000` and `r.Streaming.LimitPoolSizeToVRAM=1` in `DefaultEngine.ini [SystemSettings]`; manual screen-percentage mode; the `DefaultScalability.ini [GlobalIlluminationQuality@2]` override (only when F1 is engaged); `r.CustomDepth=0` after a sample-content audit; remove inert `r.MSAACount=8`; record draw calls ≤ 1,500 / texture pool ≤ 1.0 GB / package ≤ 3 GB in project config; conditional `r.GPUSkin.UnlimitedBoneInfluences=False` after R8; owner and hardware for the 3060-class captures (AQ-18).

## Amendments (2026-10-25 config pass)
- Config applied (phase 2, user-authorised) in `Config/DefaultEngine.ini`: `r.Substrate=False` and `r.RayTracing=False` in `[/Script/Engine.RendererSettings]` (the section the Project Settings UI writes for these `ConfigRestartRequired` renderer properties; `r.Substrate` is `ECVF_ReadOnly`, so it stays in the section Epic's templates use rather than `[SystemSettings]`); `[SystemSettings]` `r.ScreenPercentage.Default.Desktop.Mode=0` (`EScreenPercentageMode::Manual`), `r.Streaming.PoolSize=1000`, `r.Streaming.LimitPoolSizeToVRAM=1`. Startup log confirms each (`Set CVar [[r.Substrate:0]]`, `[[r.RayTracing:0]]`, `[[r.ScreenPercentage.Default.Desktop.Mode:0]]`, `[[r.Streaming.PoolSize:1000]]`, `[[r.Streaming.LimitPoolSizeToVRAM:1]]`).
- Not applied (conditional, per their own triggers): `r.CustomDepth=0` (sample-content audit first), removing `r.MSAACount=8`, `r.GPUSkin.UnlimitedBoneInfluences=False` (spike R8), the `DefaultScalability.ini [GlobalIlluminationQuality@2]` override (only if F1 engages), `r.DynamicRes.OperationMode=0` (engine default already off).

User decisions (2026-10-25); decision unchanged in substance, Status unchanged (Accepted).
- user decision 2026-10-25: `NpcCount` default 50 (was ~40, user-locked default changed by the user), so 5v5 has 60 characters; reference scenes, R2, RC-3 and the character GPU line are measured at 60 characters (pass lines unchanged, conservative). The listen-host GT total 12.0 ms and the CMC 3.0 ms line were sized for 40-50 characters: the prototype slope (~0.19 ms per character) projects ~13.1 ms GT at 60 on the RTX 5090 before tiering and optimisation, so both are working values to re-verify in R3/RC-1 (technical-director + performance-analyst). The pawn replication line in SCL F9 rises to ≤ 36 KB/s per client (working value).

Closeout 2026-10-25 (re-review follow-ups, `production/design-closeout/2026-10-25_rereview-followups.md`); decision unchanged, Status unchanged (Accepted).
- §4.2 eye-mask row: mask cost stated for the 295-trace worst case (59 x 5, about 1.5 ms physics, GT share target 0.5 ms unchanged, two-frame drain at the 160 cap), consistent with ADR-0015 and the camera GDD F3/OQ10 (camera re-review xdoc).
- §4.2 animation row: closeout D3 adopted: the listen host tiers every non-locally-controlled pawn with the clients' significance bands and uniform `TickPose` escalation (no host-only rate-1 posing); R3 (listen host + 9 remote clients) confirms or measures it (SCL OQ16).

Cross-review fix 2026-10-25 (`design/gdd/gdd-cross-review-2026-10-25.md` S-W3; decision unchanged, Status unchanged):
- The one-frame round-end step (Round R9: Roster net updates, GameState, ping clear, crowd halt, about 60 pawn stops) carries a budget of ≤ 2.0 ms added listen-host game-thread time in that frame; the R3 listen-host capture must include one round end (Round AC 65, ADR-0002).

Technical Setup 2026-10-25b (architecture-review closure, `docs/architecture/architecture-review-2026-10-25b.md`); decision unchanged, Status unchanged (Accepted).
- §4.2 (LOW inconsistency; ping handler has no named line): the Informant Pings `ServerTeamPing` handler (a token-bucket check, the range check, and one write to the sender's team ping part on C-TeamView) MUST be charged to the "Queue drain, resolvers, Roster, staged writes, CPA ledger" line (0.5 ms, ADR-0002), because it is RPC ingress plus a staged team-part write like the other command paths. Its cost is negligible (a few microseconds per ping; the bucket limits it to `PingBucketSize` / `PingBucketPerSec`) and gets no new line; the line's measured scope in RC-1 includes it.
- §4.2 (LOW inconsistency; IIF double count): ADR-0011 counts the Input & Interaction Framework (IIF) inside the Queue/Roster 0.5 ms line, while this table also listed it under "Camera eye-mask traces + concealment/IIF/objectives misc". IIF MUST be charged to the Queue/Roster 0.5 ms line only; the 1.0 ms misc line is now eye mask (0.5 ms, ADR-0015) plus concealment and objectives misc (0.5 ms, ADR-0017/0020) and excludes IIF. The double count is removed with no change in any line value, so the 12.0 ms total is unchanged (2.0 + 3.0 + 1.5 + 0.5 + 1.5 + 1.0 + 0.5 + 2.0 = 12.0). The owner column of the misc line drops ADR-0011 accordingly.
