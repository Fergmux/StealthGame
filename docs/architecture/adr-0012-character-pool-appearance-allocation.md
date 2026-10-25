# ADR-0012: Character pool data and appearance allocation

## Status
Accepted — 2026-10-25, by the user (blanket acceptance of ADR-0001..0023, recorded by the orchestrator). **Conditional:** every spike or verification item named in the earlier status text below must still report before implementation depends on it; a failed spike is handled by a superseding ADR, not by editing this one.

*Earlier status text (kept for its gating conditions):* Proposed — awaiting user acceptance. Cannot be Accepted before ADR-0001, ADR-0002, ADR-0003, ADR-0004, ADR-0005, ADR-0006, ADR-0007, ADR-0008 and ADR-0010 are Accepted and the verification spikes CPA-V1 to CPA-V5 (Engine Compatibility) have reported.

## Date
2026-10-24

## Last Verified
2026-10-24 (engine claims read against the UE 5.8.4 install at `C:/Program Files/Epic Games/UE_5.8`; design against `character-pool-appearance.md` in its "NEEDS REVISION, re-review pending; reconciled 2026-10-24" state)

## Decision Makers
Autonomous design pipeline (`/architecture-decision`, rigor full, review mode full); technical-director (TD-ADR) and unreal-specialist validation (gate record below); acceptance reserved for the user.

**Gate record (2026-10-24):** TD-ADR CONCERNS → REVISED; engine validation MINOR NOTES (applied). Details under `## Validation Criteria` → "Gate results".

## Summary
Character Pool & Appearance (CPA) needs a cooked pool of at most 24 appearances, a team-blind balanced deck dealt once per round and conserved through kills, takeovers and disconnects, and a look that is identical, resident and pop-free for every pawn and viewer. This ADR fixes the mechanism: one hard-referenced `UCharacterPoolData` primary data asset held resident by a game-instance subsystem; a plain-C++ `FAppearanceDeckCore` (deal, valid-subset table, card ledger, replacement draw) behind a server-only `UAppearanceAllocator` world subsystem, drawing only from ADR-0006 `FSecretStream`s; `AppearanceId` as one `uint8` initial-only RepNotify property on `AStealthPawn` set during the factory's deferred spawn and applied by one idempotent function that never loads; a client warm-up rig that precaches PSOs and forces residency for **all** pool rows before Session's single acknowledgement carries `PoolHash`.

## Engine Compatibility

| Field | Value |
|-------|-------|
| **Engine** | Unreal Engine 5.8 (installed 5.8.4, CL 58902052) |
| **Domain** | Core (data assets, server logic) with Rendering (residency, PSO precache, skin cache) and Networking (one replicated property) |
| **Layer** | Core (architecture.md layer map: Character Pool & Appearance, system 4) |
| **Knowledge Risk** | HIGH — Rendering and Networking are HIGH-risk domains for 5.4-5.8 (`VERSION.md`); the PSO precache component API and the 5.8 streaming signatures are post-cutoff; the engine reference has no material on PSO precache, texture streaming control or skin-cache per-LOD settings |
| **References Consulted** | `docs/engine-reference/unreal/VERSION.md`, `modules/rendering.md`, `modules/networking.md`, `breaking-changes.md`, `deprecated-apis.md`, `current-best-practices.md`; `docs/architecture/architecture.md` (Engine Knowledge Gap Summary, Module Ownership, Data Flow §1/§5, API Boundaries, Required ADRs, Performance budget, Open Questions); ADR-0001, ADR-0003, ADR-0004, ADR-0005, ADR-0006, ADR-0007, ADR-0008 (read by section); engine source read this session (table below) |
| **Post-Cutoff APIs Used** | `UPrimitiveComponent::PrecachePSOs` / `IsPSOPrecaching()` / `CheckPSOPrecachingAndBoostPriority()` [V `PrimitiveComponent.h:1025-1044`]; `r.PSOPrecache.ProxyCreationStrategy` (0 always create, **1 delay proxy until PSOs compiled — default**, 2 fallback material) [V `PSOPrecache.cpp:35-41`]; `UStreamableRenderAsset::HasPendingInitOrStreaming()` (the `bool` overload is `UE_DEPRECATED(5.8)`) [V `StreamableRenderAsset.h:75-80`]; `USkeletalMeshComponent::SetSkeletalMeshAsset` (5.1+) [V `SkeletalMeshComponent.h:369`] |
| **Verification Required** | **CPA-V1** a Shared Pawn whose mesh component starts with no skeletal mesh and receives `SetSkeletalMeshAsset` + `SetMaterial(0, MIC)` from the initial `OnRep` (possibly before `BeginPlay`) initialises its AnimBP instance and evaluates motion matching on its first rendered frame, with no frame drawn with another mesh. **CPA-V2** in a cooked Development client, after the warm-up rig reports complete, zero pawn proxies are delayed by `ProxyCreationStrategy = 1` between reveal and reveal + 10 s (count via `IsPSOPrecaching()` on every pawn mesh at first registration); behaviour in PIE recorded separately (PIE compiles shaders on demand). **CPA-V3** after warm-up, zero pool-asset package loads between reveal and reveal + 10 s (AC-CP-15c) and every pool texture `IsFullyStreamedIn()` at reveal. **CPA-V4** a `COND_InitialOnly` property is re-sent on relevancy re-entry after the concealment cull's explicit channel close (ADR-0003 §5.8, S10) and after a takeover flips the controller on a stable NetGUID (AC-CP-18b). **CPA-V5** `PoolHash` is byte-identical on a cooked client, a cooked listen host and PIE for the same content, and differs after any pool row or referenced package changes |

**Engine facts this ADR relies on** ([V] read in the 5.8.4 source this session; [D] engine reference; [U] unverified, owned by the named spike):

| # | Fact | Tag |
|---|---|---|
| E1 | `COND_InitialOnly` = "only attempt to send on the initial bunch" | [V] `CoreUObject/Public/UObject/CoreNetTypes.h:19` |
| E2 | `USkeletalMeshComponent::SetSkeletalMeshAsset(NewMesh)` calls `SetSkeletalMesh(NewMesh, /*bReinitPose*/ false)`; `USkinnedMeshComponent::SetSkeletalMesh` is deprecated since 5.1 in favour of the skeletal-mesh-component overload | [V] `SkeletalMeshComponent.h:369, 2198`; `SkinnedMeshComponent.h:1184` |
| E3 | PSO precache: components precache all possibly used PSOs at PostLoad/registration when `r.PSOPrecaching = 1` (default 1) and `r.PSOPrecache.Components = 1` (default 1, read-only); with `ProxyCreationStrategy = 1` (default) the render proxy is **not created** while the component's PSOs are still compiling; strategy 2 draws a fallback material | [V] `RHI/Private/PipelineStateCache.cpp:211-217`; `Engine/Private/PSOPrecache.cpp:18-41` |
| E4 | `UMeshComponent::PrestreamTextures(Seconds, bPrioritizeCharacterTextures, CinematicTextureGroups)` exists; `UStreamableRenderAsset::SetForceMipLevelsToBeResident(Seconds, Mask)`, `IsFullyStreamedIn()` and `HasPendingInitOrStreaming()` exist for textures and skeletal meshes | [V] `MeshComponent.h:152`; `StreamableRenderAsset.h:75, 181, 216` |
| E5 | `ESkinCacheUsage { Auto, Disabled, Enabled }` is a **per-LOD** setting (`FSkeletalMeshLODInfo::SkinCacheUsage`, default `Auto`); the project sets `r.SkinCache.DefaultBehavior=0` (Exclusive: every mesh excluded unless it opts in) | [V] `SkinnedAssetCommon.h:33-44, 204`; `GPUSkinCache.cpp:119`; `Config/DefaultEngine.ini:14` |
| E6 | `AActor::PostNetInit()` is virtual; on a client the initial RepNotifies run before `PostNetInit`/`BeginPlay`; `SetSkeletalMesh` calls `InitAnim` when animation is enabled, and native components are registered in `PostSpawnInitialize` (before `FinishSpawning`), so setting the mesh from null initialises the AnimBP instance | [V] `Actor.h:2958`; `DataChannel.cpp:3518` vs `3537`; `SkeletalMeshComponent.cpp:1164, 3568-3571`; `Actor.cpp:4333-4337` (TD/engine gate) |
| E7 | Asset Registry `FAssetPackageData::GetPackageSavedHash()` exists (editor-side content digest); `EPrimaryAssetCookRule::AlwaysCook` exists; a `UPrimaryDataAsset`'s default primary asset type is its native class name unless `GetPrimaryAssetId` is overridden | [V] `AssetData.h:1044`; `AssetManagerTypes.h:53`; `DataAsset.cpp:73-98` (TD/engine gate) |
| E10 | A relevancy close keeps no replicator (only a dormancy close does) and `COND_InitialOnly` properties are force-compared on an initial send, so a re-opened channel re-sends `AppearanceId`; pawns are `DORM_Never` (ADR-0003 §1) | [V] `DataChannel.cpp:2445-2473`; `RepLayout.cpp:1561, 1605` (TD/engine gate) |
| E11 | PSO precache for a skinned component is requested when its mesh or material is set, whether or not it is visible; the scene proxy is not created while its PSOs compile (`ProxyCreationStrategy = 1`, `ECVF_ReadOnly`); component precache is **off whenever `GIsEditor`** (PIE shows nothing) | [V] `SkinnedMeshComponent.cpp:614, 2804`; `MeshComponent.cpp:74-76, 110`; `PSOPrecache.cpp:35-41, 141` (TD/engine gate) |
| E12 | `IsFullyStreamedIn()` is always false for a texture whose optional mips are not mounted; components carry their own per-LOD `SkinCacheUsage` override array; `bSupportRayTracing` forces a mesh into the skin cache | [V] `StreamableRenderAsset.h:~214`; `SkinnedMeshComponent.h:325`; `GPUSkinCache.cpp:121` (TD/engine gate) |
| E8 | `UPrimaryDataAsset` exists; `FStreamableManager::RequestSyncLoad` / `RequestAsyncLoad` exist | [V] `Engine/Classes/Engine/DataAsset.h:47`; `StreamableManager.h:756-786` |
| E9 | Listen-server authority receives no RepNotify for its own writes | [D] (ADR-0003 constraints) |

## ADR Dependencies

| Field | Value |
|-------|-------|
| **Depends On** | ADR-0001 (one pawn class; `AppearanceId` is the only per-instance look datum; one look path, CP-16), ADR-0002 (`FParticipantId`, `FEpochToken`, ledger mutations only inside the drain or Round's Preparing step, Roster event flush ordering), ADR-0003 (allowlist row for `AppearanceId`, pawn net parity, relevancy re-open initialises presentation from replicated state), ADR-0004 (`FCharacterPoolTuning_Server` keys, freeze before the deal, preset selectability for the floor policy), ADR-0005 (Session's single round acknowledgement carries `PoolHash`; `TravelLoadTimeoutSec`; seamless-travel lifetime of game-instance subsystems), ADR-0006 (`GetServerSubSeed(AppearanceDeal, 0)`, `FSecretStream`, `UniformBelow`, `FisherYates`, `DealDigest`), ADR-0007 (`StealthGameplay/Core` placement, `Public/Server` vs `Public/View`, plain-C++ core), ADR-0008 (T1 headless tests, G-DECK negative controls, `CI.ShippingSymbolAudit`), ADR-0010 (`UStealthPawnFactory::SpawnSharedPawn` deferred-spawn sequence that writes `AppearanceId` before `FinishSpawning`). All must be Accepted before this ADR is Accepted |
| **Enables** | ADR-0016 (Crowd replacement and ambient entry/exit call `DrawReplacement`/`CommitCard`/`VacateCard`), ADR-0019 (informant takeover calls `TakeNpcCard` in the `PickRespawnPawn` → `TakeOverCrowdPawn` call stack), ADR-0022 (pool residency and skin-cache uniformity are inputs to the rendering budget; the skin-cache *mode* is ADR-0022's) |
| **Blocks** | Character Pool & Appearance epic (all stories); the Shared Pawn "appearance apply" story; Round's Preparing deal step (P3/P6); Crowd replacement stories; Session ack `PoolHash` field |
| **Ordering Note** | Implement the deck core and its T1 suite (AC-CP-01..12, G-DECK) first: it has no engine dependency and can start once ADR-0006's `FSecretStream` exists. The apply path waits for ADR-0010's factory; the warm-up rig waits for CPA-V1..V3 |

## Context

### Problem Statement
Pillar 1 must hold statistically, not only visually: if the number of copies of a look, the order of looks, the time a look takes to appear or the way a look is chosen correlates with player-ness or team, an observer can identify players without seeing any behaviour. The prototype shows every failure mode at once: `UStealthSettings::AppearanceMeshes` (four Meshy citizens bound to the Game Animation Sample's mannequin skeleton) and `AppearancePalette` live as `Config` properties in `DefaultGame.ini` (a tuning back door, TDC CR-13); the deck is reshuffled with `FMath::RandRange` (invertible, ADR-0006 forbids it); the fallback look path creates a `UMaterialInstanceDynamic` per pawn (a per-pawn parameter surface, art bible 8.x.6); and `ApplyAppearance` calls `TSoftObjectPtr::LoadSynchronous()` at first use (a first-draw load, CP-18). Those four citizens are also not admissible content: their arms are about 17 % shorter than the mannequin's (first playable report, known issue 3), which art bible 1.3.2 ("bone chain lengths equal Manny's; chain lengths win") rejects. CPA's GDD fixes the behaviour (CP-1..CP-19, F-1..F-9); this ADR fixes the classes, data layout, random streams, ledger, replication and residency mechanism, so that stories can be written against them.

### Constraints
- UE 5.8.4, C++20, legacy NetDriver (ADR-0001); one pawn class `AStealthPawn` (ADR-0001 §1); four runtime modules with the `Public/Server` compile guard (ADR-0007).
- Every hidden decision draws from `GetServerSubSeed` through `FSecretStream`; no engine RNG (ADR-0006 forbidden pattern `engine_rng_in_gameplay`).
- Every gameplay number is a tuning key (ADR-0004); the pool itself is **content**, versioned by `PoolHash`, not tuning.
- No CPA-owned RPC, timer, kick rule or Reason code (CP-18); residency rides Session's single acknowledgement (ADR-0005).
- Art constraints (art bible 1.3, 4.3, 8.x.4-8.x.9): one material slot per pool mesh, static `MI_Char_<Model>_<Variant>` instances of `M_Character_Master`, no MIDs, no keyed parameters, non-Nanite skeletal meshes with 4 LODs, the Manny skeleton unmodified, one shared physics asset, ≤ 40 MB pool texture memory, characters never instanced.
- Listen-server host trust is an accepted limit (CPA Replication; AQ-14); Pillar-1 evidence is gathered on remote clients.
- Team: 1-2 programmers; the deck must be unit-testable headless (ADR-0007 testable cores).

### Requirements
- One cooked pool asset of ≤ 24 rows with stable authored `AppearanceId` 0-23 (255 = `Unassigned`) (CP-1, CP-2).
- `AllocateAppearances` once per round in `Preparing`, after `OnTuningReady`, before any pawn spawns; team-blind by signature; F-1..F-4 counts, uniform valid-subset law, uniform extras, unbiased Fisher-Yates; participants take the first `P` cards in canonical `ParticipantId` order (CP-6..CP-8, CP-19).
- Three domain-separated server-private streams `Deal`, `Replace`, `Take` (reserved) from `GetServerSubSeed(AppearanceDeal, 0)`; only `DealDigest` logged (CP-9).
- Conserved ledger: `DrawReplacement` uniform over vacant cards in the spawn tick; dead participant pawns vacate like NPCs; `TakeNpcCard` moves an existing pawn's card with no draw; `ReleaseToCrowd` keeps the card live; own-card respawn moves nothing (CP-11..CP-13, F-6, invariants I-C1..I-C5).
- `AppearanceId` set before first replication, immutable, initial-only, applied by one idempotent function; no default look for any frame (CP-10, CP-18).
- All pool assets resident and warmed (all `A` rows, never only `K`) before the client's single acknowledgement; no first-draw load (CP-18).
- One look path: same mesh, MI, LOD rule, skin-cache setting, physics asset, shadow and animation settings for every pawn and viewer (CP-16).
- G-DECK suite (F-8) runs headless in milliseconds; Shipping contains no entropy-injection hook, no debug overlay and no `Warn` floor policy.

## Decision

### 1. Pool data: one hard-referenced primary data asset

`UCharacterPoolData : UPrimaryDataAsset` (`StealthGameplay/Public/View/CharacterPool/CharacterPoolData.h`; client-safe, read by apply, warm-up and `PoolHash`). Exactly one instance ships, `DA_CharacterPool`, registered with the Asset Manager as primary asset type `CharacterPool` with cook rule **AlwaysCook** (an Asset Manager settings entry; a config change outside this run's write scope, applied by the implementing story).

```cpp
UENUM() enum class EAppearanceVariantKind : uint8 { Default, GarmentSwap, Accessory };

USTRUCT() struct FAppearanceRow {
    GENERATED_BODY()
    UPROPERTY(EditAnywhere) uint8 AppearanceId = 255;          // authored, stable, 0..23; never derived (CP-1)
    UPROPERTY(EditAnywhere) uint8 ModelIndex = 0;              // k, 1-based pool-table model order (F-7)
    UPROPERTY(EditAnywhere) FName ModelKey;                    // stable model identity (stratification, F-4)
    UPROPERTY(EditAnywhere) EAppearanceVariantKind VariantKind = EAppearanceVariantKind::Default;
    UPROPERTY(EditAnywhere) TObjectPtr<USkeletalMesh> Mesh;    // HARD reference (residency)
    UPROPERTY(EditAnywhere) TObjectPtr<UMaterialInstanceConstant> Material;  // HARD; static MI, parent M_Character_Master
    UPROPERTY(VisibleAnywhere) uint8 GarmentSwatchIndex = 0;   // written by the validator from F-7, never hand-edited
    UPROPERTY(VisibleAnywhere) uint8 TrimSwatchIndex = 0;      // idem
    UPROPERTY(VisibleAnywhere) uint64 ContentDigest = 0;       // editor-computed digest of the row's hard-dependency closure
};

UCLASS() class UCharacterPoolData : public UPrimaryDataAsset {
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere) TArray<FAppearanceRow> Rows;       // stored sorted by AppearanceId
    UPROPERTY(VisibleAnywhere) uint32 PoolSchemaVersion = 1;
    const FAppearanceRow* FindRow(FAppearanceId Id) const;     // O(1) via a 256-entry index built in PostLoad
    virtual FPrimaryAssetId GetPrimaryAssetId() const override; // FPrimaryAssetId("CharacterPool", GetFName())
};
```

- **Hard references are the residency mechanism.** Loading `DA_CharacterPool` loads every mesh, material instance and texture it references. `UCharacterPoolSubsystem : UGameInstanceSubsystem` (`Public/View/`, exists on server and clients) resolves `FPrimaryAssetId("CharacterPool", "DA_CharacterPool")` through `UAssetManager::GetPrimaryAssetPath` (no config, settings or ini path: TDC CR-13; if the Asset Manager has not finished its scan at `Initialize` [U], it falls back to `LoadObject` on the fixed path `/Game/StealthGame/Characters/Pool/DA_CharacterPool`) and loads it once at `Initialize` with `RequestSyncLoad` (front-end, before any round) and holds it in a `UPROPERTY` for the session, so the pool survives every seamless travel (ADR-0005: game-instance subsystems survive travel) and is never garbage-collected mid-match. A pool row's mesh or material is never a soft pointer and is never loaded by gameplay code.
- **Tier independence (CP-5).** Tier A (mannequin kit on Manny), Tier B (Meshy models normalised to Manny) and Tier C (Body_Base + Meshy heads/garments) are all expressed as **one merged single-slot skeletal mesh per model or accessory variant**, merged offline in Blender (art bible 1.3.1, 8.7). No runtime mesh merging, modular components (leader-pose children) or Mutable is used, so the component set and draw path are identical for every appearance.
- **Pool validator** (`StealthGameEditor`, runs on asset save and in CI; AC-CP-21/22/29/33): recomputes swatch indices from F-7 and overwrites them (hand edits fail CI); checks ids unique, in 0-23, never 255; 6-8 models and 12-24 rows for a shipping pool (`PoolBelowMvpFloor` otherwise); every MI is a `UMaterialInstanceConstant` whose parent is `M_Character_Master` and whose parameter names contain no `Team`, `Role`, `IsPlayer`, `IsNPC`, `Ally`; every mesh uses the project's Manny skeleton asset, has 4 LODs, Nanite disabled (art bible 8.x.4), the shared physics asset, ≤ 4 influences, and **an identical `SkinCacheUsage` on every LOD of every pool mesh** (the value itself is ADR-0022's, CP-16 requires only uniformity); **reference-pose bone chain lengths within the art bible 1.3.2 tolerance of the Manny reference mesh** (thigh, shin, foot, spine chain, clavicle-to-upper-arm, forearm, hand, neck) — this is the automated half of T1 and is the check that rejects the prototype's Meshy citizens (arms ≈ 17 % short); for every reachable `K` (5..16) at least one valid stratified subset exists (F-4) or the pool is reported `DeckInvalid`; one `bSupportRayTracing` value across all pool meshes (E12: ray tracing forces skin-cache use); no optional mips on any pool texture (E12: `IsFullyStreamedIn` would never become true); and writes `ContentDigest` = FNV-1a-64 over the **sorted hard-dependency closure** of the row's mesh and material (textures, `M_Character_Master`, physics asset, skeleton), using each package's Asset Registry `GetPackageSavedHash()` [V E7], so a texture re-import changes `PoolHash`. A stale digest (package changed, digest not rewritten) fails CI (`CI.PoolEvidenceFreshness` also keys on it, AC-CP-33).
- **`PoolHash`** = ADR-0004's FNV-1a-64 over the canonical byte stream `PoolSchemaVersion ‖ RowCount ‖ for each row in AppearanceId order: (AppearanceId, ModelIndex, VariantKind, GarmentSwatchIndex, TrimSwatchIndex, UTF-8 Mesh object path, UTF-8 Material object path, ContentDigest)`, fixed-width little-endian, computed once per process by `UCharacterPoolSubsystem` after load. It is public (it identifies content, not a secret) and is computed identically on server, listen host and clients (CPA-V5).

### 2. The deck core (plain C++, server-only)

`FAppearanceDeckCore` (`StealthGameplay/Public/Server/CharacterPool/AppearanceDeckCore.h`; no `UObject`, no `UWorld`, ADR-0007) holds the pool view, the frozen deck parameters, the valid-subset table, the streams and the card ledger for one round.

**Construction inputs (dependency injection):** `FPoolView` (rows reduced to `{Id, ModelOrdinal, VariantOrdinal, GarmentSwatch}`, sorted by id), `FDeckParams {CopiesFloor, ActiveMin, ActiveMax}` (from `FCharacterPoolTuning_Server`: `DeckCopiesFloorCount`, `DeckActiveAppearancesMinCount`, `DeckActiveAppearancesMaxCount`), `EDeckFloorPolicy` and `uint64 RootSubSeed`.

**Valid-subset table (F-4, CP-19).** Built once per `FPoolView` (server at pool load; editor validator): for each `K` in the reachable range, enumerate every stratified subset — per model, how many variants are chosen (`n_m`, differing by at most 1 among models that still have unchosen variants, every model chosen once when `K ≥ M`) and which variants — keep those with at least `SwatchFloor = min(5, K, M)` distinct garment swatches, deduplicate, and store as a **sorted** array of `uint32` bitmasks over row ordinals. At most a few thousand masks for 8 × 3; built in well under 1 ms. The table is canonical (sorted), so a draw over it is deterministic given the stream.

**Streams (CP-9; ADR-0006 §5).** `Root = GetServerSubSeed(ESeedDomain::AppearanceDeal, 0)` (the facade fetches it; the core receives the value). Three `FSecretStream`s: `Deal = FSecretStream(Root, Tag("CPA_DEAL"))`, `Replace = FSecretStream(Root, Tag("CPA_REPL"))`, `Take = Tag("CPA_TAKE")` **reserved and unused** (Penalty CR-6: `TakeNpcCard` makes no draw). `DealDigest = SipHash(fixed public digest key, Root)` (ADR-0006 §7); the root, the streams and their counters are never logged, reflected or replicated (ADR-0006 forbidden pattern `secret_seed_exposure`).

**Deal (CP-6..CP-8, F-1..F-4).** `Deal(TConstArrayView<FParticipantId> PlayersSorted, int32 NpcCount) → TValueOrError<FDealResult, EDealError>`:
1. `D = P + N`; `K = max(1, min(A, ActiveMax, floor(D / CopiesFloor)))`; floor check `K ≥ ActiveMin` (and the subset table non-empty for `K`). `Enforce` → `EDealError::DeckInvalid`; `Warn` (non-Shipping only) → proceed with the largest feasible `K`, flag `DeckBelowFloor`.
2. Draw, in this fixed order from `Deal`: (a) subset index `UniformBelow(Table[K].Num())`; (b) extras: partial Fisher-Yates of the `K` active ids (sorted) taking the first `r = D mod K`; (c) build the deck in canonical order (active ids ascending, `q` or `q + 1` copies each) and run `FisherYates` over all `D` cards.
3. Participants, already sorted by `FParticipantId::Value`, take cards `0..P-1`; NPC cards are `P..D-1` in deck order. No later step sorts or groups by appearance.
4. Result: `ParticipantCards` (participant → card ticket; **server-only**), `NpcCards` (tickets in dealt order), `FDealSummary {K, q, r, ActiveSet, Counts, Extras, DealDigest, Flags}` for Round's server-only Event Log header (CP-6 (c)/(d)).

Ranged draws use ADR-0006 `UniformBelow` (Lemire multiply-shift with rejection) and `FisherYates`; no `%` on a draw, no float, no unordered container feeds a draw. The facade converts the caller's set into the sorted array, so `TSet` iteration order never reaches the stream.

**Ledger (CP-11..CP-13, F-6, I-C1..I-C5).** `TArray<FCard>` of length `D`, each `{AppearanceId, State ∈ {InDeck, BoundLive, NpcLive, NpcVacant}, FPawnKey Holder, FParticipantId Participant (valid only when BoundLive)}`, plus per-appearance `c_a` (fixed at the deal) and a vacant-card index set kept **sorted by card index** (so a draw over it is canonical). `FPawnKey` is an opaque `uint32` the facade issues per pawn; the core never sees a pointer. Operations (all O(1) or O(|V|)):

| Operation | Card move | Draw |
|---|---|---|
| `CommitCard(Ticket, PawnKey)` | dealt/drawn card → `BoundLive` (participant ticket) or `NpcLive` | none |
| `DrawReplacement() → Ticket` | one `NpcVacant` card chosen by `UniformBelow(|V|)` on `Replace` over the sorted vacant set → *pending* until committed | `Replace` |
| `VacateCard(PawnKey)` | `NpcLive` or `BoundLive` → `NpcVacant` (participant mapping cleared) — **same call for every victim kind** | none |
| `TakeNpcCard(Participant, PawnKey)` | that pawn's `NpcLive` card → `BoundLive` | none (Penalty CR-6) |
| `ReleaseCard(PawnKey)` | `BoundLive` → `NpcLive` (strike exile at the exile; grace expiry) | none |
| `RecoverOwnCard(Participant, NewPawnKey)` | `BoundLive` card re-pointed to the new pawn (Round R17.9 own-card respawn) | none |
| `Reset()` | all cards discarded; ledger and streams dropped | — |

A pending replacement ticket that is not committed before the caller's call stack returns is rolled back to `NpcVacant`, raises `ensure` in non-Shipping builds and reports `LedgerMismatch` to the fault sink in every build. `DrawReplacement` with `V` empty returns `Error_NoVacancy` and reports `LedgerMismatch` (CPA Edge Cases). Every operation re-asserts `sum(L_a + V_a) = D` and fixed `c_a` in non-Shipping builds (I-C1); a ledger invariant failure is reported to the fault sink in every build.

### 3. The server facade: `UAppearanceAllocator`

`UAppearanceAllocator : UWorldSubsystem` (`Public/Server/CharacterPool/`; `ShouldCreateSubsystem` true only for `NM_ListenServer`/`NM_DedicatedServer`/`NM_Standalone` worlds; never `BlueprintType`/`BlueprintCallable`, ADR-0007). One per round world (each round is its own world after seamless travel, so CP-14's re-deal is structural: the previous round's subsystem and ledger are gone). It owns the `FAppearanceDeckCore`, the `TWeakObjectPtr<AStealthPawn>` ↔ `FPawnKey` map, and the call-order guard.

- **Callers (only):** Round (deal, own-card respawn), Crowd (replacement, ambient entry/exit, NPC non-kill removal), the death-write handler (vacate, every victim kind; ADR-0018 owns where it sits), Penalty (`TakeNpcCard`, informant non-kill loss; ADR-0019), and the `ReleaseCard` callers, each pairing it with a Roster release in the same call stack: Round after its own `ReleaseToCrowd` (disconnect grace expiry, Round R17.4), and Penalty after `RequestTransition(P, Informant, StrikeLimit)`, inside which the Roster performs `ReleaseToCrowd` itself (exile: release, not retire; ADR-0005 §2, ADR-0019 §2.3). Callers are server game-thread code inside the queue drain (ADR-0002) or Round's `Preparing` step; any other caller, thread or net mode takes the misuse branch (returns an error, reports to the fault sink in every build).
- **Call-order guard (AC-CP-28):** `AllocateAppearances` succeeds only when Round's injected phase provider reports `Preparing`, `OnTuningReady` has fired for this round, no Shared Pawn of this round has been spawned, and no deal exists (`Error_AlreadyDealt`, AC-CP-08a).
- **Pairing with the Roster.** The ledger is the authority for cards; the Roster is the authority for bindings. The paired calls run in one server call stack: `Roster.BindPawn` + `TakeNpcCard` + `TakeOverCrowdPawn` (ADR-0019 owns the sequence), `Roster.ReleaseToCrowd` (called directly, or inside `RequestTransition(StrikeLimit)`) + `ReleaseCard`. At the end of each drain (non-Shipping), a sweep checks every live pawn (a pawn whose strike role is not `Dead`; `VacateCard` runs in the kill slot while the body is still present, ADR-0018) holds exactly one card, every `BoundLive` card's pawn is the Roster's binding for that participant, and every `NpcLive` card's pawn is `Unbound`; a mismatch is `LedgerMismatch`. Because the card is vacated while the body lies, a same-look replacement may emerge beside a body only if a preset violates Assassination's warning I-ASN-5w (`DeathFallSec + BodyLingerSec + BodyRemovalGraceSec < CrowdReplacementMinDelaySec`); that is accepted as a warned preset choice, not a ledger defect (ADR-0018 §6.4).
- **Floor policy.** `Enforce` for `Default` and `HostSelectable` presets in every build (ADR-0004 catalog flag); `Warn` exists only under `#if !UE_BUILD_SHIPPING` and only for non-selectable test presets. `CI.ShippingSymbolAudit` (ADR-0008) fails if `Warn`, `CharacterPool_InjectEntropy` or `CharacterPool_DebugOverlay` is linked into Shipping.
- **Test injection.** Tests construct `FAppearanceDeckCore` directly with a root sub-seed; integration tests use Session's non-Shipping `IEntropySource` override (ADR-0006). The facade has no seed setter in Shipping.

### 4. `AppearanceId` on the pawn: set once, replicate once, apply once

`AStealthPawn` (ADR-0001 §1, ADR-0010) declares:

```cpp
UPROPERTY(ReplicatedUsing = OnRep_AppearanceId) uint8 AppearanceId = 255;   // FAppearanceId::Unassigned
// GetLifetimeReplicatedProps: DOREPLIFETIME_CONDITION(AStealthPawn, AppearanceId, COND_InitialOnly);
```

- **Write path (server).** Only `UStealthPawnFactory::SpawnSharedPawn` writes it, between `SpawnActorDeferred` and `FinishSpawning` (SCL R2 step 2), through `bool ApplyAppearance(FAppearanceId)` (the name ADR-0010 §3.1 step 2 already calls; ADR-0010's sketch shows `void`, aligned at `/architecture-review`), which refuses `255`, refuses an id with no pool row and refuses a second write (returns `false`, leaves the value, logs `Error_AppearanceRewrite`; AC-CP-10) and then calls `ApplyAppearanceVisuals()` directly (the authority receives no RepNotify, E9). `TakeOverCrowdPawn` never touches it (CP-12b). The first bunch any client receives therefore already carries it (SCL R2 "replication starts on the next net tick").
- **Read path (client).** `OnRep_AppearanceId()` calls `ApplyAppearanceVisuals()`. A relevancy re-open (concealment channel close and re-open, ADR-0003 §5.8) creates a new client actor whose initial bunch carries the id again (CPA-V4). `PostNetInit` asserts `AppearanceId != 255` in non-Shipping builds (AC-CP-15a).
- **`ApplyAppearanceVisuals()` — the one look path (CP-16, CP-18).** Idempotent; depends on no `BeginPlay` state; reads only `UCharacterPoolSubsystem::GetPool()->FindRow(Id)` (resident); calls `GetMesh()->SetMaterial(0, Row.Material)` **then** `GetMesh()->SetSkeletalMeshAsset(Row.Mesh)` (material first, so the one PSO precache request is for the final pair, E11; setting the mesh initialises the AnimBP instance, E6); never calls `LoadSynchronous`, never creates a MID, never reads controller, team, binding, local-ness or viewer; returns without drawing if the row is missing (and reports `PoolNotResident` once, CPA Edge Cases). The skin-cache setting, LOD rule, physics asset, shadow flags and animation settings come from the asset and the class defaults, identical for every row and pawn.
- **Nothing drawn before resolution.** The pawn class default (and the reparented Game Animation Sample Blueprint) has **no skeletal mesh asset assigned** on its mesh component, so a pawn whose id has not resolved has nothing to draw, shadow or ray-trace. This avoids a visibility flag that the Camera eye mask (ADR-0015) and Concealment (ADR-0017) also own. Engine source shows the AnimBP instance initialises when the mesh is first set (E6); the residual risk is the reparented Blueprint's construction script, which runs in `FinishSpawning` **after** the apply and must not set a mesh (lint: no `SetSkeletalMesh*`/`SetSkinnedAsset*` node in that Blueprint or its construction script). CPA-V1 confirms end to end; if it fails, the fallback is a class-default mesh component with `bHiddenInGame` driven by a single visibility arbiter shared with ADR-0015/ADR-0017 (recorded, not chosen).
- **Re-open.** ADR-0003 §5.8's `RefreshPresentation` never sets a mesh or material: a re-opened actor's appearance is applied only by its initial `OnRep_AppearanceId` → `ApplyAppearanceVisuals()` (E10).
- **Forbidden surfaces** (CPA Replication, ADR-0003 allowlist): no appearance field on `PlayerState`, `GameState` or PlayerController; no RPC carrying an appearance; no per-connection or per-viewer condition on `AppearanceId`; no audio, VFX, footstep or UI selector keyed by `AppearanceId` (CP-16, AC-CP-20b). The allowlist row is `AStealthPawn.AppearanceId | COND_InitialOnly | audience All | carrier pawn | IVP fact AppearanceId`.

### 5. Client warm-up and the acknowledgement

`UCharacterPoolWarmup : UWorldSubsystem` (`Public/View/CharacterPool/`; created on clients and the listen host, not on a dedicated server) runs once per round world after the map is loaded, behind the loading screen:

1. Spawn one local, non-replicated `ACharacterPoolWarmupRig` holding **one `USkeletalMeshComponent` per pool row — all `A` rows, never only the active `K`** (warming only the subset would reveal it), each **created with the pawn class default's mesh component as its template** (so settings cannot drift: empty per-component `SkinCacheUsage` override array, same shadow and ray-tracing flags), then given its row's material (`SetMaterial(0, MIC)` **first**) and mesh, so PSO precache is requested for the final mesh/material pair (E11). Precache does not need the rig to be in view (E11); the rig sits in front of the local view under the loading-screen overlay only so the texture streamer treats the textures as wanted.
2. Call `SetForceMipLevelsToBeResident(WarmupHoldSec)` on every pool texture and mesh and `PrestreamTextures(WarmupHoldSec, true)` on each rig component, where `WarmupHoldSec = TravelLoadTimeoutSec + RevealPawnWaitMaxSec + 10 s` (derived from existing Session/Round keys, not a new tuning key; covers AC-CP-15c's reveal + 10 s window); after that, ordinary screen-size streaming governs every pawn identically.
3. Complete when, for every rig component, `SceneProxy != nullptr` (the proxy exists only once its PSOs have compiled, E11) and `!IsPSOPrecaching()`, `!Mesh->HasPendingInitOrStreaming()`, every texture `IsFullyStreamedIn()`, and at least two frames have rendered with all proxies present. There is no CPA timer: Session's `TravelLoadTimeoutSec` bounds the whole load (CP-18).
4. Destroy the rig and fire `OnPoolWarm`. ADR-0005's acknowledgement builder waits for `OnPoolWarm` (alongside the tuning `ClientHash` and the layout hash) and then sends `ServerRoundAck(..., PoolHash)`; a mismatch follows Session's resend-once then `ParamsMismatch` path. Rejoiners take the same path. **The listen host's local acknowledgement (ADR-0005, no RPC) also waits for its own `OnPoolWarm`.**

`r.PSOPrecache.ProxyCreationStrategy` (`ECVF_ReadOnly`; set only in `DefaultEngine.ini` and asserted at startup by `UCharacterPoolSubsystem`) must stay **1** (delay) and must never be 2: strategy 2 draws a fallback material, which is a default look (CP-18). Strategy 1 makes a pawn invisible while its PSOs compile; the warm-up exists so that this never happens after the reveal (CPA-V2 counts it). A pawn that would still be delayed after warm-up is a defect logged as `PoolNotResident`, and the round is excluded from Pillar-1 evidence (CPA Edge Cases). Because component PSO precache is off whenever `GIsEditor` (E11), CPA-V2 runs only in cooked or `-game` builds; in PIE the warm-up completes on the residency conditions alone.

### 6. Round-start sequence (where this sits in Data Flow §5)

`OnTuningReady` → Round `Preparing` step calls `AllocateAppearances(Players, NpcCount)` → Round builds **one spawn list of all `D` entries** (participant tickets and NPC tickets), shuffled by `GetServerSubSeed(SpawnOrder, LayoutAttempt)` (ADR-0006 domain 6) → for each entry in one frame: `SpawnSharedPawn(Transform, Ticket.AppearanceId, ControlKind, Participant?, Epoch)` → `CommitCard(Ticket, Pawn)` → possess. Placement uses LSC's shared emergence rule, independent of appearance and player-ness (CP-10). Pawns may replicate to a client **before** its warm-up completes (architecture.md Data Flow §5 spawns at step 6 and runs the acknowledgement barrier at step 7; ADR-0005 requests the acknowledgement only once tuning and layout are ready). They stay behind the loading screen, which lifts only at the reveal after the acknowledgement barrier, so the first **visible** draw on every client is pop-free (CPA-V2/V3 measure from the reveal).

### Architecture Diagram

```
 EDITOR / CI                                   EVERY PROCESS (server, listen host, clients)
 ┌───────────────────────────┐                 ┌───────────────────────────────────────────────┐
 │ Pool validator            │  writes swatch, │ UCharacterPoolSubsystem (GameInstance)        │
 │ F-7 swatches, chain-length│  ContentDigest  │  holds DA_CharacterPool (hard refs = resident)│
 │ check, subset feasibility,├────────────────►│  FindRow(Id), PoolHash (FNV-1a-64)            │
 │ skin-cache uniformity     │                 └───────┬───────────────────────┬───────────────┘
 └───────────────────────────┘                         │ rows                  │ rows + PoolHash
                                                       ▼                       ▼
 SERVER ONLY (Public/Server)                 CLIENT / HOST (Public/View)
 ┌────────────────────────────────────────┐  ┌────────────────────────────────────────────┐
 │ UAppearanceAllocator (World, per round)│  │ UCharacterPoolWarmup (World, per round)    │
 │  call-order guard, PawnKey map, sweep  │  │  rig of ALL A rows → PSO + mips resident   │
 │  ┌──────────────────────────────────┐  │  │  OnPoolWarm ─► Session ack (PoolHash)      │
 │  │ FAppearanceDeckCore (plain C++)  │  │  └────────────────────────────────────────────┘
 │  │  subset table · Deal · ledger    │  │
 │  │  FSecretStream CPA_DEAL/CPA_REPL │◄─┼── GetServerSubSeed(AppearanceDeal, 0)  (ADR-0006)
 │  └──────────────────────────────────┘  │
 └──┬───────────┬──────────┬──────────┬───┘
    │Deal       │Draw/Commit│Vacate    │Take/Release/RecoverOwn
  Round       Crowd      death write  Penalty / Round (paired with Roster)
    │           │
    ▼           ▼
 UStealthPawnFactory::SpawnSharedPawn ── ApplyAppearance(Id) before FinishSpawning
                                              │  AStealthPawn.AppearanceId (uint8, COND_InitialOnly)
                                              ▼
                     client OnRep_AppearanceId / server direct ─► ApplyAppearanceVisuals()
                     SetMaterial(0, Row.MIC) then SetSkeletalMeshAsset(Row.Mesh)  (no load, no MID)
```

### Key Interfaces

```cpp
// --- StealthGameplay/Public/View/CharacterPool (client-safe) ---------------------
struct FAppearanceId { uint8 Value = 255; static constexpr uint8 Unassigned = 255; };
class UCharacterPoolData;                                  // §1
class UCharacterPoolSubsystem : public UGameInstanceSubsystem {
public:
    const UCharacterPoolData* GetPool() const;             // never null after Initialize
    const FAppearanceRow* FindRow(FAppearanceId Id) const;
    uint64 GetPoolHash() const;                            // FNV-1a-64, public
};
class UCharacterPoolWarmup : public UWorldSubsystem {      // clients + listen host
public:
    bool IsPoolWarm() const;
    FSimpleMulticastDelegate OnPoolWarm;                   // ADR-0005 ack builder subscribes
};

// --- StealthGameplay/Public/Server/CharacterPool (server-only; ADR-0007 guard) -----
enum class EDealError : uint8 { AlreadyDealt, CallOrder, DeckTooSmall, DeckInvalid };
enum class ELedgerError : uint8 { NoVacancy, UnknownPawn, WrongCardState, StaleTicket, CallOrder };
enum class EDeckFloorPolicy : uint8 { Enforce /* , Warn: !UE_BUILD_SHIPPING only */ };
struct FCardTicket { uint16 CardIndex; uint32 DealEpoch; FAppearanceId Appearance; };
struct FDealSummary { uint8 K, Q, R; TArray<FAppearanceId> ActiveSet; TArray<uint8> Counts;
                      TArray<FAppearanceId> Extras; uint64 DealDigest; uint8 Flags; /* DeckBelowFloor, PoolBelowMvpFloor */ };
struct FDealResult { TArray<TPair<FParticipantId, FCardTicket>> ParticipantCards;  // server-only
                     TArray<FCardTicket> NpcCards;                                  // dealt order
                     FDealSummary Summary; };

class FAppearanceDeckCore {                                // plain C++, headless-testable
public:
    FAppearanceDeckCore(const FPoolView&, const FDeckParams&, EDeckFloorPolicy, uint64 RootSubSeed);
    TValueOrError<FDealResult, EDealError> Deal(TConstArrayView<FParticipantId> PlayersSorted, int32 NpcCount);
    TValueOrError<FCardTicket, ELedgerError> DrawReplacement();
    TOptional<ELedgerError> CommitCard(const FCardTicket&, FPawnKey);
    TOptional<ELedgerError> VacateCard(FPawnKey);
    TOptional<ELedgerError> TakeNpcCard(FParticipantId, FPawnKey);
    TOptional<ELedgerError> ReleaseCard(FPawnKey);
    TOptional<ELedgerError> RecoverOwnCard(FParticipantId, FPawnKey NewPawn);
    void Reset();
    bool CheckInvariants() const;                           // I-C1..I-C4
};

class UAppearanceAllocator : public UWorldSubsystem {      // facade; no Blueprint exposure
public:
    TValueOrError<FDealResult, EDealError> AllocateAppearances(const TSet<FParticipantId>& Players, int32 NpcCount);
    TValueOrError<FCardTicket, ELedgerError> DrawReplacement();     // same call stack as the spawn
    TOptional<ELedgerError> CommitCard(const FCardTicket&, AStealthPawn*);
    TOptional<ELedgerError> VacateCard(const AStealthPawn*);       // every victim kind; no kind parameter
    TOptional<ELedgerError> TakeNpcCard(FParticipantId, AStealthPawn*);  // informant adoption
    TOptional<ELedgerError> ReleaseCard(const AStealthPawn*);      // paired with Roster ReleaseToCrowd
    TOptional<ELedgerError> RecoverOwnCard(FParticipantId, AStealthPawn* NewPawn);  // Round R17.9
};
// static_assert(sizeof(FParticipantId) == sizeof(uint32)): the deal input carries no team/role/order (AC-CP-03).

// --- On AStealthPawn (ADR-0010 owns the class) -----------------------------------
UPROPERTY(ReplicatedUsing = OnRep_AppearanceId) uint8 AppearanceId = 255;   // COND_InitialOnly
bool ApplyAppearance(FAppearanceId);    // server, factory only, once, before FinishSpawning (ADR-0010 §3.1 step 2)
void ApplyAppearanceVisuals();          // idempotent look path; server direct, client OnRep
```

### Implementation Guidelines
1. The pool **must** be one `UCharacterPoolData` asset with **hard** references to every mesh and static material instance, held by `UCharacterPoolSubsystem` for the session; gameplay code **must never** load a pool asset (`LoadSynchronous`, `RequestAsyncLoad`) after the front end.
2. Pool characters **must** use one merged single-slot skeletal mesh per model/accessory variant and one `UMaterialInstanceConstant` per appearance; code **must never** create a `UMaterialInstanceDynamic`, set a material parameter, scale, tint or add a component per pawn (lint: `CreateDynamicMaterialInstance`/`SetScalarParameterValue`/`SetVectorParameterValue` forbidden under `Core/SharedPawn` and `Core/CharacterPool`).
3. `AppearanceId` **must** be written only by `UStealthPawnFactory` (through `ApplyAppearance(Id)`) before `FinishSpawning`, exactly once, and replicated `COND_InitialOnly` with a RepNotify; `ApplyAppearanceVisuals` **must** be the only function that sets a pawn's mesh or material, and **must not** read controller, team, binding, local-ness, `BeginPlay` state or viewer.
4. The pawn class default and its Blueprint child **must** have no skeletal mesh asset assigned (nothing drawn before resolution), the Blueprint (including its construction script) **must not** set a mesh, and the pawn and rig mesh components **must** keep an empty per-component `SkinCacheUsage` override array.
5. Every allocator draw **must** come from `FSecretStream(GetServerSubSeed(AppearanceDeal, 0), Tag("CPA_DEAL"|"CPA_REPL"))`; draws **must** use `UniformBelow`/`FisherYates` over canonically sorted inputs; the `CPA_TAKE` tag is reserved and **must not** be consumed (Penalty CR-6).
6. The deal input **must** be an opaque set of `FParticipantId` (no team, role, order or state type reachable from the signature); the facade **must** sort it before it reaches the core.
7. `VacateCard` **must** be called for every pawn death or removal with **no** kind parameter; `TakeNpcCard`, `ReleaseCard` and `RecoverOwnCard` **must** run in the same server call stack as their Roster counterpart; a `DrawReplacement` ticket **must** be committed in the same call stack as its `SpawnSharedPawn`.
8. The ledger **must** never change `D` or any `c_a` after the deal; a fresh uniform draw from the active set (instead of the vacant set) is forbidden (F-6).
9. Clients and the listen host **must** warm all `A` pool rows (never only the active `K`) before Session's acknowledgement (RPC or host-local); `r.PSOPrecache.ProxyCreationStrategy` **must** be 1 and is asserted at startup.
10. Only `DealDigest`, the active set, counts, extras and flags may be logged (server-only Event Log header); the root sub-seed, stream state and the participant-to-card map **must never** be logged, replicated or reflected to Blueprint.
11. The `Warn` floor policy, the entropy-injection hook and any `AppearanceId` debug overlay **must** be compiled out of Shipping (`CI.ShippingSymbolAudit`).
12. Tests live in `Source/StealthGameplay/Private/Tests/CharacterPool/` (ADR-0007/0008 layout; the GDD's `Source/StealthGame/...` path predates the module split) and the editor validator tests in `Source/StealthGameEditor/Private/Tests/CharacterPool/`; G-DECK seeds are a `constexpr` array in `CharacterPoolTestSeeds.h` (no file I/O), chosen once and never reseeded.

## Alternatives Considered

### Alternative 1: Keep the prototype path (`UStealthSettings` Config arrays, MID tint, `FMath::RandRange`, `LoadSynchronous`)
- **Description**: `AppearanceMeshes` (soft pointers) and `AppearancePalette` in `DefaultGame.ini`; a per-round shuffled index deck in the game mode; apply by loading the mesh at first use or creating a MID and tinting it.
- **Pros**: Already works in 10-instance PIE with four Meshy citizens; trivial to edit.
- **Cons**: `Config` properties are an ini back door (TDC CR-13, architecture "prototype promotion rule"); `FMath::RandRange` is invertible and engine-versioned (ADR-0006); the deck has one copy per look and no conservation (a kill can change counts, F-6); a MID per pawn is a per-pawn parameter surface (art bible 8.x.6); `LoadSynchronous` at first draw is a pop correlated with first use (CP-18); the four citizens fail art bible 1.3.2 chain lengths (arms ≈ 17 % short).
- **Rejection Reason**: Violates five locked rules; replaced, not migrated.

### Alternative 2: Soft references with an async load per round
- **Description**: The pool asset holds `TSoftObjectPtr`s; at `Preparing` the server and each client async-load only the active `K` rows.
- **Pros**: Lower steady memory when the pool reaches 24 rows; faster front end.
- **Cons**: Loading only `K` reveals the subset through timing; loading all `A` gives no saving; any late completion is a first-draw pop on one look; a separate residency acknowledgement would be needed (retired by CP-18). Pool texture memory is ≤ 40 MB (art bible 8.x.5), so the saving is small.
- **Rejection Reason**: Residency by hard reference is simpler and leak-free at a cost inside budget.

### Alternative 3: Runtime modular characters (leader-pose components, skeletal mesh merge or Mutable)
- **Description**: Build each appearance at run time from a body plus head, hair and garment parts.
- **Pros**: More combinations from fewer assets; natural fit for Tier C.
- **Cons**: Different component counts or merged meshes per pawn break "identical component set" (ADR-0001 §1) and multiply PSO/skin-cache work; runtime merge creates per-pawn assets and hitches; Mutable's 5.8 status is not in the engine reference [U]; art bible 1.3.6/8.x.6 require one merged single-slot mesh.
- **Rejection Reason**: Offline merge in Blender gives the same variety with a uniform runtime path.

### Alternative 4: Allocation per spawn (fresh uniform draw over the active set)
- **Description**: Every spawn, including replacements, draws a look uniformly from the `K` active appearances.
- **Pros**: Stateless; no ledger.
- **Cons**: Counts random-walk and break `DeckCopiesFloorCount` within a few dozen kills (F-6); copy counts then drift with kill patterns that correlate with where players are.
- **Rejection Reason**: Breaks conservation and T1.

### Alternative 5: `UDataTable` for the pool
- **Description**: A `FTableRowBase` row struct in a data table.
- **Pros**: Spreadsheet-style editing, CSV import.
- **Cons**: Row names are strings (ids by convention), soft references are the idiom, and residency, validator hooks and Asset Manager cook rules are more natural on a typed primary data asset; the pool has ≤ 24 rows, so table tooling buys little.
- **Rejection Reason**: Weaker typing and residency story for no gain at this size.

### Alternative 6: Replicate the deck (a GameState array of appearances) and let clients assign looks
- **Description**: Replicate the active set and counts so clients can pre-warm only `K` and validate counts.
- **Pros**: Smaller warm-up.
- **Cons**: Publishes deck composition (CPA Edge Cases "a replicated deck composition" is forbidden), invites per-participant arrays.
- **Rejection Reason**: Information leak.

## Consequences

### Positive
- Team-blindness is structural: the deal's input type cannot carry a team, the facade sorts ids, and every draw comes from a non-invertible keyed stream; the G-DECK suite and its negative controls run headless in milliseconds.
- Conservation is enforced by one ledger with one vacate call for every victim kind, so player kills and NPC kills are indistinguishable at the ledger level by construction.
- One apply function with no load and no MID removes the pop, placeholder and per-pawn-parameter tells together.
- Hard-reference residency plus an all-rows warm-up makes "no look is slower to appear" testable (CPA-V2/V3) without any CPA RPC or timer.
- The chain-length check in the validator turns art bible 1.3.2 into a CI gate, so a Meshy model with short arms cannot enter the pool.

### Negative
- All pool assets stay resident for the whole session (bounded by art bible 8.x.5: ≤ 40 MB textures for 8 models plus meshes).
- The warm-up rig lengthens client load by the time PSO compilation and mip streaming take on a cold cache (first run after a driver or build change can be seconds); it is bounded only by Session's `TravelLoadTimeoutSec`.
- The pawn has no mesh in its class default, which makes editor previews of the Blueprint empty; a preview-only mesh must live outside the class default (editor tooling).
- Pairing card moves with Roster operations is a convention enforced by tests and a debug sweep, not by a type.
- The prototype's four Meshy citizens cannot ship as-is; Tier A mannequin variants come first and the Meshy pool waits for re-generation to mannequin proportions (image-to-3D reference, first playable report).

## Risks
- **R1 — AnimBP initialisation from a null mesh (CPA-V1).** The engine path is verified (E6); the residual risks are the sample Blueprint's construction script resetting the mesh after the apply and a visible first-pose pop. If CPA-V1 fails, use the recorded fallback (class-default mesh, hidden until applied through one visibility arbiter shared with Camera and Concealment). *Mitigation*: CPA-V1 runs in the first Shared Pawn story, before any appearance story.
- **R2 — PSO precache completeness (CPA-V2).** Precache covers the vertex factories and passes the component predicts; skin-cache on/off, VSM and Lumen passes may request PSOs the rig did not trigger. *Mitigation*: the rig uses the pawn's exact component settings and renders ≥ 2 frames with all passes; CPA-V2 counts delayed proxies after reveal on a cooked build; a bundled PSO cache (`FShaderPipelineCache`) is the escalation if the count is not zero [U].
- **R3 — Warm-up cost on 3060-class hardware.** Unknown until measured. *Mitigation*: measured in the R3/R2 spikes (ADR-0013/ADR-0022); the load bound is Session's, so a slow warm-up shows as a longer load, never as a pop.
- **R4 — Skin-cache mode (R2 spike).** The project default is Exclusive; this ADR only requires uniformity across pool meshes. *Mitigation*: ADR-0022 picks the mode; the validator enforces whichever value is chosen on every LOD of every pool mesh.
- **R5 — Initial-only re-send on re-open (CPA-V4).** If a re-opened channel did not carry `AppearanceId`, a re-entering pawn would draw nothing. *Mitigation*: engine path verified (E10: a relevancy close keeps no replicator; initial-only properties are force-compared on the initial send); CPA-V4 plus AC-CP-18b confirm it end to end through ADR-0003's explicit close.
- **R6 — Ledger/Roster drift.** A missed paired call leaves a card bound to a released pawn. *Mitigation*: end-of-drain sweep (non-Shipping), `LedgerMismatch` to the fault sink in every build, AC-CP-10 randomised 500-event sequences.
- **R7 — `PoolHash` stability (CPA-V5).** Object paths or digests could differ between PIE and cooked processes. *Mitigation*: hash only authored data and editor-written digests (saved-package hashes of the dependency closure), never runtime pointers or cooked offsets.
- **R9 — Asset Manager scan timing [U].** If `UAssetManager` has not scanned when the game-instance subsystem initialises, the primary-asset lookup fails. *Mitigation*: the fixed-path `LoadObject` fallback (§1); a boot test asserts the pool loaded in a cooked build.
- **R8 — Content pipeline: Meshy proportions.** The 17 % short arms (and any future proportion drift) block Tier B. *Mitigation*: chain-length validator; Tier C fallback (Body_Base + Meshy heads/garments) is fully supported by this mechanism (CP-5, AC-CP-29).

## GDD Requirements Addressed

| GDD System | Requirement | How This ADR Addresses It |
|------------|-------------|--------------------------|
| character-pool-appearance.md | CP-1 one pool table, stable authored `AppearanceId` 0-23, 255 sentinel, static MI, no runtime MI per pawn | §1 `UCharacterPoolData` rows with hard refs to static `UMaterialInstanceConstant`; validator id rules; §4 no MID |
| character-pool-appearance.md | CP-2 size limits, MVP floor, `PoolBelowMvpFloor` | §1 validator; §2 summary flag; floor policy |
| character-pool-appearance.md | CP-3 / F-7 swatch rule computed, not chosen | §1 validator writes `GarmentSwatchIndex`/`TrimSwatchIndex` from F-7; hand edits fail CI |
| character-pool-appearance.md | CP-4 accessory variants are separate meshes | §1 one merged mesh per accessory variant; Alternative 3 |
| character-pool-appearance.md | CP-5 tier independence | §1 all tiers as merged single-slot meshes; no rule depends on tier; AC-CP-29 |
| character-pool-appearance.md | CP-6 allocator contract, team-blind signature, one-shot, call order | §2 `Deal`, §3 facade guard, `FParticipantId`-only input, `Error_AlreadyDealt`, `Error_CallOrder` |
| character-pool-appearance.md | CP-7, F-1..F-3 deck construction | §2 Deal steps 1-2 |
| character-pool-appearance.md | CP-8 unbiased Fisher-Yates, canonical participant order, no re-sort | §2 `FisherYates` + `UniformBelow` (ADR-0006), sorted ids |
| character-pool-appearance.md | CP-9 server-private root, three domain-separated keyed streams, `DealDigest` only, injection compiled out | §2 Streams (`CPA_DEAL`, `CPA_REPL`, `CPA_TAKE` reserved); §3 test injection; Guideline 10-11 |
| character-pool-appearance.md | CP-10 one spawn list, appearance before first replication, possession invisible | §4 write path in deferred spawn, `COND_InitialOnly`; §6 single shuffled spawn list; possession parity is ADR-0001/0003/0010 |
| character-pool-appearance.md | CP-11 immutable per pawn, conserved per round, own-card respawn | §4 single write; §2 ledger (`RecoverOwnCard`), invariants |
| character-pool-appearance.md | CP-12 / F-6 replacement draw uniform over vacant cards in the spawn tick; look re-roll on participant loss | §2 `DrawReplacement` on `CPA_REPL` over sorted vacant set; `VacateCard` for every victim kind; ticket committed in the spawn call stack |
| character-pool-appearance.md | CP-13 disconnect, release, no late join, informant adoption | §2 `ReleaseCard`, `TakeNpcCard` (no draw); §3 pairing with Roster |
| character-pool-appearance.md | CP-14 every round re-deals | §3 per-round world subsystem; `Reset` |
| character-pool-appearance.md | CP-15 pool admission gates | §1 validator automates T1 numeric items incl. chain lengths; visual gates stay manual (AC-CP-23/24) |
| character-pool-appearance.md | CP-16 one look for everyone | §4 one apply path, no MID, no keyed selector; validator skin-cache uniformity |
| character-pool-appearance.md | CP-17 future-tier guard | Any cosmetic is a new pool row in the deck; no per-participant apply path exists |
| character-pool-appearance.md | CP-18 residency, warm-up of all rows, no first-draw load, `PoolHash` in Session's single ack | §1 hard refs held by GI subsystem; §5 warm-up rig + `OnPoolWarm` + `PoolHash` |
| character-pool-appearance.md | CP-19 subset per round, shared, every model when `K ≥ M`, swatch floor | §2 valid-subset table, uniform pick |
| character-pool-appearance.md | F-8 G-DECK suite headless, fixed seeds, negative controls | §2 plain core; Guideline 12; ADR-0008 T1 |
| character-pool-appearance.md | F-9 bandwidth ≈ 1 B per pawn per channel open | §4 `uint8` initial-only |
| shared-character-locomotion.md | R2 step 2 set and apply `AppearanceId` in the deferred spawn | §4 write path |
| information-visibility-policy.md | `AppearanceId` audience All; look-owner link ServerOnly | §4 allowlist row; participant map server-only |
| session-travel.md | rule 11.6 `PoolHash` in the single round acknowledgement | §5 |

## Performance Implications
- **CPU (server)**: deal ≤ 0.2 ms once per round (D ≤ 110, a few hundred SipHash evaluations); replacement draw and ledger ops O(|V|) ≤ a few µs; subset table built once per session (< 1 ms). Counted inside the 0.5 ms "queue drain, resolvers, Roster" line (architecture.md GT budget).
- **CPU (client)**: `ApplyAppearance` per pawn at spawn (mesh + material set, render-state recreation) — about 60 calls in the round-start frame (`NpcCount` 50), inside the loading screen; estimate 2.4-6 ms one-off [U, measured in CPA-V1]. Steady state 0.
- **Memory**: whole pool resident for the session: ≤ 40 MB textures for 8 models (art bible 8.x.5) plus ≤ 16 skeletal meshes (8 models + accessory variants) at 12k/6k/2.5k/0.9k tris; the subset table < 64 KB.
- **Load Time**: + warm-up time per round load (PSO compile on a cold cache, mip streaming); bounded by `TravelLoadTimeoutSec`.
- **Network**: `AppearanceId` ≈ 1 B payload + handle per pawn per channel open, ≈ 100-150 B per client per round, 0 B/s steady (F-9, AC-CP-32); `PoolHash` is 8 B inside the existing acknowledgement.

## Migration Plan
The prototype's appearance code is **replaced, not migrated** (architecture.md prototype promotion rule; ADR-0007): `UStealthSettings::AppearanceMeshes`, `AppearancePalette`, `AppearanceMaterial` and `AStealthGameMode::DrawAppearance` are not carried over; `AStealthCharacter::AppearanceIndex` (`int32`, `COND_None`) becomes `AStealthPawn::AppearanceId` (`uint8`, `COND_InitialOnly`). The palette's colours become the art bible 4.3 swatch values inside the static MIs (CPA Tuning Knobs, `AppearancePalette` is pool content). The four Meshy citizens (`/Game/StealthGame/Characters/Pool/SK_Citizen02..05`) stay out of `DA_CharacterPool` until regenerated to mannequin proportions and passing the validator; the MVP pool starts as Tier A (six mannequin-derived models × 2 variants). Order: (1) deck core + G-DECK; (2) pool asset + validator + Tier A content; (3) pawn property + apply (after ADR-0010 factory, CPA-V1); (4) allocator facade + Round/Crowd wiring; (5) warm-up + Session ack field (CPA-V2/V3/V5).

## Validation Criteria
- T1 (headless): AC-CP-01, 01b, 02, 02b, 03, 04, 04b (nightly), 05, 06, 07, 08a-c, 09, 10, 11a-b, 12, 12b, 14, 21, 22 (asset automation, once the pool exists), 28, 29 pass; `StealthGame.CharacterPool.Net.LifetimePropsAllowlist` (AC-CP-15d) passes on the class default object.
- Integration (2-client PIE and cooked multi-process): AC-CP-13, 15a-e, 16, 16b, 17, 18, 18b, 19, 20a-c, 27, 30, 31; CPA-V1..V5 reports retained.
- Performance: AC-CP-23 `PT-CP-T5` on the RTX 3060-class reference PC (median frame ≤ 16.67 ms, animation ≤ 4 ms, one run with real allocator output); AC-CP-32 network record.
- CI: `CI.ShippingSymbolAudit` (no `Warn`, no injection hook, no overlay in Shipping), `CI.PoolEvidenceFreshness` (digest/manifest), lint for MID creation and engine RNG in `Core/CharacterPool` and `Core/SharedPawn`.
- Visual / review / playtest: AC-CP-24a (G-VAR), AC-CP-24b (G-TWIN), AC-CP-26 (cross-GDD Baseline and Tell Matrix review), AC-CP-27b (cosmetic and colour-aid review).
- Evidence of correctness: G-DECK accepts the allocator and rejects B1-B4; AC-CP-25 Pillar-1 chi-square over ≥ 50 logged rounds does not reject.

**Gate results (2026-10-24):**
- **TD-ADR: CONCERNS → REVISED** (autonomous run: "Revise flagged items" chosen and logged). Eight items applied: appearance setter aligned with ADR-0010's `ApplyAppearance(Id)` (+ `ApplyAppearanceVisuals()`); §6 ordering corrected (pawns may replicate before warm-up; the loading screen hides them until the reveal); `ContentDigest` over the hard-dependency closure via `GetPackageSavedHash`; pool asset resolved by `FPrimaryAssetId` with a fixed-path fallback, `GetPrimaryAssetId` override; `WarmupHoldSec` derived from existing keys; listen-host local acknowledgement waits for `OnPoolWarm`; `RefreshPresentation` never sets mesh/material; AC-CP-22/24a/24b/26/27b added to validation. Not re-run (autonomous run); `/architecture-review` is the check.
- **Engine validation (unreal-specialist lens): MINOR NOTES**, nothing blocking. Applied: E6 and E10 upgraded to [V]; E11 (PSO precache on mesh/material set, proxy deferred, off under `GIsEditor` so CPA-V2 runs cooked/`-game`), E12 (optional mips, per-component skin-cache override, ray tracing forces skin cache) added; material-before-mesh order; rig components templated from the pawn's mesh component; `ProxyCreationStrategy` read-only and asserted; Blueprint construction-script lint; validator checks `bSupportRayTracing` uniformity and no optional mips. Remaining [U]: Asset Manager scan timing at GI `Initialize` (R9), runtime cost of `ApplyAppearanceVisuals` at the round-start frame.

## Related
- Depends on: ADR-0001 (identity invariants, one look path), ADR-0002 (value types, drain), ADR-0003 (allowlist, net parity, re-open), ADR-0004 (deck keys, preset selectability), ADR-0005 (single acknowledgement), ADR-0006 (streams, domain 3 `AppearanceDeal`, tags `CPA_DEAL`/`CPA_REPL`/`CPA_TAKE`), ADR-0007 (module placement), ADR-0008 (tests, Shipping audit), ADR-0010 (factory).
- Enables: ADR-0016 (Crowd), ADR-0019 (informant takeover), ADR-0022 (rendering budgets: skin-cache mode, residency figures).
- Design: `design/gdd/character-pool-appearance.md`; `design/art/art-bible.md` 1.3.1-1.3.10, 4.3, 8.x.4-8.x.9; `production/first-playable-report.md` (known issue 3: Meshy arms ≈ 17 % short).
- **GDD sync items (not applied; GDDs are owned by other agents in this run):** (1) CPA F-8 says the allocator "uses its own fixed-algorithm PRNG (for example PCG32)" — superseded by ADR-0006: every CPA draw uses `FSecretStream` (SipHash counter mode); golden vectors replace the PCG32 example. (2) CPA Interactions/Downstream name `RetireNpcCard()` — this ADR's single call is `VacateCard(Pawn)` for every victim kind (NPC kill, ambient exit, participant death, informant loss). (3) CPA AC preamble test path `Source/StealthGame/Private/Tests/CharacterPool/` → `Source/StealthGameplay/Private/Tests/CharacterPool/` (ADR-0007). (4) ADR-0008 names a G-DECK seed *file* (`GDeckSeeds.json`); CPA's AC preamble and the coding standard's "no file I/O" rule call for a `constexpr` array in `CharacterPoolTestSeeds.h` — this ADR follows the GDD; ADR-0008 to align at `/architecture-review`. (5) architecture.md API sketch `TakeNpcCard(AStealthPawn*)` → `TakeNpcCard(FParticipantId, AStealthPawn*)` (GDD form). (6) architecture.md API sketch `FAppearanceId DrawReplacement()` → `FCardTicket DrawReplacement()` + `CommitCard` (a ticket, so no card is ever `NpcLive` without a pawn). (7) ADR-0010 §3.1 and its Key Interfaces show `void ApplyAppearance(FAppearanceId)`; this ADR returns `bool` (refuses 255, an unknown id or a second write) — align at `/architecture-review`.

## Amendments (2026-10-24 sync)
Inter-ADR amendment pass; decision unchanged, Status unchanged (Proposed).
- Ledger sweep: "every live pawn" = strike role not `Dead`; same-look replacement beside a body accepted only under an I-ASN-5w-violating preset — requested by ADR-0018 (item 3).
- GDD sync item 4 (G-DECK seed storage): resolved in this ADR's favour by technical-director ruling; ADR-0008 §9 amended. No edit to this ADR's decision.

Architecture-review fix pass (`docs/architecture/architecture-review-2026-10-25.md`); decision unchanged, Status unchanged (Proposed).
- §3 callers and pairing: `ReleaseCard` callers are Round (after its own `ReleaseToCrowd`, disconnect expiry) and Penalty (after `RequestTransition(StrikeLimit)`, inside which the Roster calls `ReleaseToCrowd`); "Round/Penalty call `ReleaseToCrowd`" wording removed — requested by ADR-0019 (item 8); architecture-review fix 2026-10-25 (Conflict 8).

User decisions (2026-10-25); decision unchanged in substance, Status unchanged (Accepted).
- user decision 2026-10-25: `NpcCount` default 50 (was ~40, user-locked default changed by the user), so 5v5 has 60 characters; the default deck is `D = 60`, `K = 12`, `q = 5`, `r = 0` (every look 5 copies); the G-DECK suite (F-8) keeps its reference configuration `N = 40` (`D = 50`, `r = 2`) so its copy-count tests stay meaningful (CPA F-8).
- Deck rule — user decision 2026-10-25: the balanced appearance deck is confirmed.
