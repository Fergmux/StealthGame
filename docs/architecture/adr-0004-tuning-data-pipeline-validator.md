# ADR-0004: Tuning data pipeline and validator

## Status
Accepted — 2026-10-25, by the user (blanket acceptance of ADR-0001..0023, recorded by the orchestrator). **Conditional:** every spike or verification item named in the earlier status text below must still report before implementation depends on it; a failed spike is handled by a superseding ADR, not by editing this one.

*Earlier status text (kept for its gating conditions):* Proposed (awaiting user acceptance)

## Date
2026-10-24

## Last Verified
2026-10-24 (against UE 5.8.4 headers at `C:/Program Files/Epic Games/UE_5.8/Engine/Source` and `design/gdd/tuning-data-config.md` as reconciled 2026-10-24)

## Decision Makers
Design pipeline (autonomous run, `/architecture-decision --review full`); technical-director (TD-ADR) and Unreal engine-specialist validation folded into one review; user acceptance pending.

**Review record (2026-10-24):** TD-ADR **CONCERNS** (8 items: mixed reject/warn invariant rows and preset-scoped rows, generated-table bootstrap deadlock and `offsetof`, unplaced schema constants, hot-reload filter direction, snapshot change detection, naming/log/test-path alignment with ADR-0007, missing test loader seam, editor-only metadata tests) → **all revised in place** (autonomous run, recommended option). Engine validation **MINOR NOTES** (8 notes; `GetPrimaryAssetId` override, undo event path, partial bunches, `WithIdenticalViaEquality`, UHT exporter option, AC-22 positive control, `UE_LOGF` `%ls`) → folded into Decision, Risks and Alternative 7. Gates not re-run on the revised text; `/architecture-review` in a fresh session is the next independent check.

## Summary
Every gameplay number in Smoking Mirror must be one designer-editable, server-validated, frozen and hashed value that clients receive but never supply (Tuning Data & Config, TDC). This ADR fixes the implementation: presets are `UPrimaryDataAsset` instances of one class built from per-owner `_Public`/`_Server` sub-structs; the schema is declared once in UPROPERTY metadata and compiled into a generated, checked-in C++ schema table that cooked builds keep; a plain-C++ validator runs the CR-6 ladder (default for non-finite, clamp to hard range, then cross-field invariants that **reject or warn and never clamp or repair**); a CI **safe-range lint** proves every owner safe range satisfies every invariant with the other keys at their defaults; the frozen set is hashed with an in-project FNV-1a-64 over the F-2 byte stream; the public half replicates as one atomic GameState struct; and Shipping always resolves `Default`.

## Engine Compatibility

| Field | Value |
|-------|-------|
| **Engine** | Unreal Engine 5.8 (installed 5.8.4) |
| **Domain** | Core (data assets, reflection, cooking); Networking for the snapshot only |
| **Layer** | Foundation |
| **Knowledge Risk** | LOW for data assets, reflection, subsystems and asset-manager cook rules (pre-cutoff APIs, re-read in 5.8.4 headers); MEDIUM for the snapshot's atomic replication (networking is HIGH-risk in 5.8, but this ADR uses only the legacy driver's long-standing custom `NetSerialize` path) |
| **References Consulted** | `docs/engine-reference/unreal/VERSION.md`, `breaking-changes.md`, `deprecated-apis.md`, `current-best-practices.md`, `modules/networking.md` (all silent on data assets, metadata stripping, cook rules and asset-edit delegates); 5.8.4 headers: `Core/Public/Misc/CoreMiscDefines.h:31`, `CoreUObject/Public/UObject/Field.h:946`, `Engine/Classes/Engine/AssetManagerTypes.h:28-53`, `CoreUObject/Public/UObject/UObjectGlobals.h:3293-3294`, `CoreUObject/Public/UObject/UnrealType.h:6957-6961`, `Core/Public/Hash/Fnv.h`, `CoreUObject/Public/UObject/Class.h:1221` / `:5670`, `Engine/Public/Subsystems/GameInstanceSubsystem.h:16`, `WorldSubsystem.h:16` |
| **Post-Cutoff APIs Used** | None that changed after 5.3. `UE_LOGF` (5.8, preferred over `UE_LOG` [D]) for validator warnings. |
| **Verification Required** | (1) a ~2 KB `NetSerialize` struct on `AGameStateBase` replicates atomically and re-sends on change under the legacy driver, including to a rejoiner (spike T-1, AC-7/AC-12b/AC-19); (2) presets referenced only by the catalog are cooked with the asset-manager `AlwaysCook` rule (AC-27); (3) `OnObjectPropertyChanged` fires with `ValueSet` once per committed edit on a data asset open in a details panel during PIE (AC-14b). |

**Engine facts used (tagged as in architecture.md):**

| Fact | Tag | Evidence |
|---|---|---|
| UPROPERTY metadata exists only with editor-only data: `#define WITH_METADATA WITH_EDITORONLY_DATA`; `FField::GetMetaData` is the reader | [V] | `CoreMiscDefines.h:31`, `Field.h:946` |
| `EPrimaryAssetCookRule::AlwaysCook` ("always be cooked, in both production and development") and `DevelopmentAlwaysProductionNeverCook` exist | [V] | `AssetManagerTypes.h:28-53` |
| `FCoreUObjectDelegates::OnObjectPropertyChanged(UObject*, FPropertyChangedEvent&)`; `EPropertyChangeType::Interactive` (slider drag, "followed by a ValueSet") and `ValueSet` | [V] | `UObjectGlobals.h:3293`, `UnrealType.h:6957-6961` |
| `UE::HashStringFNV1a` hashes **strings**, widening every character to 4 bytes; it is not a byte-buffer hash | [V] | `Hash/Fnv.h` |
| `TStructOpsTypeTraits<>::WithNetSerializer` drives custom struct serialisation | [V] | `Class.h:1221`, `:5670` |
| A struct with a custom `NetSerialize` is compared and sent as one unit by the legacy property replicator | [U] | general UE knowledge; spike T-1 |
| Soft references held by an always-cooked asset are followed by the cooker | [U] | covered by registering presets themselves as `AlwaysCook`, so not relied on |
| Iris needs its own `NetSerializer` for a custom-serialised struct | [U] | out of scope while ADR-0001 keeps the legacy driver; ADR-0024 |

## ADR Dependencies

| Field | Value |
|-------|-------|
| **Depends On** | ADR-0001 (net-driver choice: legacy recommended, which fixes the snapshot's serializer path) |
| **Enables** | ADR-0005 (Session's single acknowledgement carries `ClientHash`; join gate reads `TuningSchemaVersion`; travel carries `?Preset=`), ADR-0008 (schema, lint and hash tests), and every system ADR that reads a key (ADR-0010 to ADR-0021) |
| **Blocks** | Foundation epic "Tuning Data & Config" and every story that reads a tuning value; prototype promotion of any gameplay number out of `UStealthSettings` |
| **Ordering Note** | ADR-0007 (module layout) places the two faces defined here (`FStealthClientTuning` in `Public/View/`, `FStealthServerTuning` and `UStealthTuningStore` in `Public/Server/`); ADR-0007 lists this ADR as an input, so this ADR deliberately does **not** list ADR-0007 under Depends On (no mutual edge). Accept ADR-0004 and ADR-0007 together. ADR-0003 adds the snapshot property to the replicated-property allowlist (audience All, carrier GameState). **Conditional, not a dependency** (architecture-review fix 2026-10-25): only if Iris is ever adopted would the snapshot need an Iris `NetSerializer`; that work belongs to the deferred Iris-migration ADR (ADR-0024, not written), which would amend this ADR. It is not an acceptance prerequisite. |

## Context

### Problem Statement
TDC (Foundation, systems-index #1) is the convention every other GDD registers into: about 400 keys across 18 owner tables, 5 core invariants (I-1 to I-5) and about 60 owner invariants, two audiences, four latch classes, a fallback ladder, two hashes and a PIE-only hot-reload. The GDD leaves seven implementation choices to the architecture (TDC OQ-2): hash algorithm, schema table generation and the source of the hard-range table, public/server struct split with atomic replication, one root asset versus several, catalog cooking, the accessor and preset-store subsystems, and PIE hot-reload routing. The 2026-10-24 cross-review adds one more: several owner **safe** ranges currently produce presets that another owner's invariant rejects (C-W2, C-W17, C-W18, C-W19, C-W22), so the pipeline needs a lint that catches this class of error mechanically. Every feature ADR and the first Foundation stories read tuning, so this must be decided before coding starts (architecture.md, Required ADRs, Must).

### Constraints
- **Metadata is editor-only [V].** `WITH_METADATA` equals `WITH_EDITORONLY_DATA`, so a cooked build cannot read `ClampMin`/`ClampMax` or any custom meta key. The runtime validator therefore needs ranges compiled into code (TDC CR-3: "the schema owns hard ranges, in code").
- **Allowed field types:** `bool`, `int32`, `float`, enums with declared integer values; no arrays, maps, vectors, doubles (CR-3).
- **No back doors (CR-13):** no `Config` properties or class flags on any tuning type; no ini, `Saved/Config`, command-line or cvar override in Shipping. The prototype `UStealthSettings` (`UCLASS(Config = Game, DefaultConfig)`, `UDeveloperSettings`) violates this and is replaced, not migrated (architecture.md prototype promotion rule).
- **Global and uniform (CR-5, Pillar 1):** no accessor takes a pawn, controller, team or viewer.
- **Kind-blind and default-deny (architecture principles 1, 3):** only the public struct replicates; the replicated snapshot is on the GameState, never on a PlayerState.
- **Testable cores (LP concern 6):** validator, hasher and schema run headless without a `UWorld`.
- **Team:** 1-2 programmers; ~400 keys must not be declared twice by hand.
- **Listen server (MVP):** the host reads through the same client accessor path it would as a remote client (CR-10, Session rule 3).

### Requirements
- R1: one data-asset class of per-owner sub-structs; presets `Default` (+ optional `Playtest`, `Solo`) listed by a catalog with `HostSelectable` flags; every catalog preset cooked (CR-2, AC-27).
- R2: one schema row per key — key, type, default, hard min/max, safe min/max, audience, latch, owner — declared once and available in cooked builds; defaults pinned to GDD defaults by test (CR-3, AC-3a).
- R3: CR-6 ladder exactly: non-finite/undeclared enum → default; out of hard range → clamp + warning; invariants on the clamped set → **reject the whole preset or warn; never clamp, never repair**; fallback requested → `Default` → schema defaults → `Failed`, with `FallbackLevel` and a `ValidationReport`.
- R4: safe-range lint: every owner safe range must satisfy all invariants with the other keys at their defaults (cross-review note for `/create-architecture`).
- R5: freeze into an immutable `ValidatedTuning`; `TuningHash` (all keys, server-only) and `ClientHash` (public keys) by F-2; server self-check (CR-7, CR-9).
- R6: public snapshot replicated atomically on the GameState with `SchemaVersion`, `PresetId`, `RoundIndex`, `Revision`, `bValid`; ≤ 2,048 bytes (CR-8, F-4).
- R7: typed client face: a server-only key cannot be named in client C++ (CR-10, AC-28).
- R8: latch classes Round / Action / Use / Session honoured (CR-11).
- R9: Shipping resolves `Default`; `?Tuning=` and console commands compiled out (CR-2, CR-13, AC-22).
- R10: PIE-only hot-reload on committed edits, debounced, routed to the PIE server world (CR-12).
- R11: validation, hashing and freeze inside the Preparing budget; no per-frame cost.

## Decision

Build TDC as **(1) a generated schema table, (2) a plain-C++ pipeline core, (3) a game-instance store and a world-scoped accessor, (4) one atomic replicated snapshot, and (5) editor/CI tools**, as follows.

### 1. Asset model: one preset class, per-owner sub-structs split by audience

- `UStealthTuningData : UPrimaryDataAsset` is the only preset class. It contains one `UPROPERTY` per owner system, each a pair of structs: `FRoundTuning_Public` / `FRoundTuning_Server`, `FCrowdTuning_Public` / `FCrowdTuning_Server`, and so on for the 18 owners (an owner with no public key has only `_Server`). The field name **is** the canonical key (CR-2); key names stay globally unique (CR-15.2), so the owner prefix is not part of the key.
- Two aggregate value types are generated from the same sub-structs:
  - `FStealthClientTuning` (all `_Public` sub-structs) — the **client face**, in `Public/View/` (ADR-0007);
  - `FStealthServerTuning` (all `_Public` plus all `_Server` sub-structs) — the **server face**, in `Public/Server/`.
  The client face is a distinct type with no server field, so `GetClientTuning()->CrowdIdleMinSec` does not compile (AC-28). Audience is structural: membership in a `_Public` struct is the audience; no audience flag can disagree with replication (CR-8, AC-7).
- Field types are restricted to `bool`, `int32`, `float` and `UENUM(): uint8` enums with explicit values (CR-3). A range pair is two scalar keys plus I-5.
- No tuning class or struct carries `Config`, `DefaultConfig`, `GlobalUserConfig` or `PerObjectConfig` (CR-13; AC-3a reflection check).
- The catalog is `UStealthTuningCatalog : UPrimaryDataAsset`: an array of `{ FName PresetId; TSoftObjectPtr<UStealthTuningData> Preset; bool bHostSelectable; bool bTestAllowsStrikeLimitOne; }` with `Default` mandatory (CR-2). `bTestAllowsStrikeLimitOne` (user decision 2026-10-25, Penalty OQ-NKP-11) lowers `NpcKillStrikeLimit`'s hard minimum from 2 to 1 for that preset in F-1 only; the catalog check rejects it on `Default` and on any `HostSelectable` entry, and it is compiled out of Shipping. Shipping always resolves `Default` (confirmed by the user 2026-10-25, TDC OQ-6 closed).
- **One root asset, not several per-domain assets.** A preset is one package: one load, one validation unit, one hash, and the cross-owner invariants see one value set. Per-domain assets were rejected (Alternative 4).
- **Session-latched keys** (`FSessionTuning_Server`, CR-11) live in the same preset class but are validated once at session open and copied from the session-frozen values into every round's set, so a between-round preset change never alters them.

### 2. Schema: declared once in UPROPERTY metadata, compiled into a generated table

Each key is declared once, on its UPROPERTY:

```cpp
// Crowd sub-struct (ServerOnly because it is in _Server)
UPROPERTY(EditAnywhere, Category = "Crowd",
    meta = (ClampMin = "2.0", ClampMax = "60.0",      // hard range (also clamps the details panel)
            SafeMin = "4.0", SafeMax = "20.0",         // safe range (warn only)
            Latch = "Round", Owner = "Crowd", Unit = "Sec"))
float CrowdIdleMinSec = 6.0f;                          // C++ initialiser = schema default
```

- The **source of truth for the schema is the UPROPERTY declaration** (default = C++ initialiser; hard range = `ClampMin`/`ClampMax`; safe range = `SafeMin`/`SafeMax`; latch, owner, unit = custom meta; audience = which sub-struct; type = the property type).
- Because metadata is stripped from cooked builds [V], an editor-only generator (`StealthGameEditor`, commandlet `StealthTuningSchemaGen`, also run by a CI check) reflects those properties and writes **`StealthTuningSchema.generated.inl`**, a checked-in table of `constexpr` rows: `{ KeyName, TypeTag, Accessor, Default, HardMin, HardMax, SafeMin, SafeMax, Audience, Latch, Owner, EnumValues }`. The runtime validator reads only that table. No metadata is read at runtime.
- **Value access is by generated accessor, never `offsetof`** (USTRUCTs are not standard-layout; TD-ADR concern 2): each row holds a pair of generated non-capturing functions (`get`/`set` through pointers-to-member, e.g. `&FStealthServerTuning::Crowd_Server` then `&FCrowdTuning_Server::CrowdIdleMinSec`), so a renamed field is a compile error, not silent memory corruption.
- **Bootstrap guard against a regeneration deadlock** (TD-ADR concern 2): if a row names a field that was renamed or removed, `StealthFoundation` stops compiling and the editor that hosts the commandlet cannot build. The build therefore honours `STEALTH_TUNING_SCHEMA_BOOTSTRAP=1` (set by the regeneration script only), which compiles an **empty** table; the editor builds, the commandlet regenerates the file, the script rebuilds without the define. `SchemaComplete` fails on an empty table, so a bootstrap build can never ship or pass CI.
- **Schema constants** that invariants read but that are not preset fields (TD-ADR concern 3) live in `Public/Server/StealthTuningConstants.h` as `constexpr` values, never hashed (F-2) and never replicated: `BlindGuessWarnPct` (50, I-4), the bush dome inner radius (90 cm, I-CS-4), the SCL capsule radius `r_c` (I-ASN-1), the IVP `LeakNetProfile` constants and `T_uncull` inputs (I-CON-a..e), `GraceMarginSec` (30, Round F4). Each carries the GDD reference; changing one is a code change plus a GDD change, like a hard range. AC-18's "no preset field named `BlindGuessWarnPct`" is a reflection check against the preset class.
- The generator also writes a **schema fingerprint** (a hash over names, types, audiences, latches and enum values). `SchemaVersion` is a hand-maintained constant beside it; the test `StealthGame.Tuning.SchemaVersionBumped` fails when the fingerprint changes without a `SchemaVersion` bump (F-2's bump rule made mechanical).
- **Test placement** (TD-ADR concern 8, ADR-0007 folder contract): tests that read metadata (`SchemaComplete`, `SafeRangeLint`, the generator's own specs) live in `StealthGameEditor/Private/Tests/`; tests of the runtime cores (validator, hasher, invariants, fallback ladder, snapshot serialisation) live in `StealthFoundation/Private/Tests/` and run headless. The GDD's older path `Source/StealthGame/Private/Tests/` predates the module split (GDD sync note below).
- **Drift guard:** `StealthGame.Tuning.SchemaComplete` (AC-3a) regenerates in memory and fails if the checked-in table differs from reflection, so the generated file cannot go stale. It also checks: every row has a default inside its hard range, a safe range inside its hard range, unique names with a CR-15 suffix (`b` prefix for bools, `Mode` for enums), no `Config` flags, only allowed types, and the AC-15 name allowlist.
- Widening a hard range is a code change plus a GDD change (CR-3): the metadata edit regenerates the table, and the GDD lint (AC-3b) compares the owner GDD's Tuning Knobs row against the schema row.

### 3. Validator (plain C++, `FTuningPipelineCore`)

- **Input** (`FTuningResolveInput`): the requested preset name; an `ITuningPresetSource` (production: the store's preloaded catalog; tests: an injectable source that can return a missing or corrupted preset, AC-6b/AC-6c — TD-ADR concern 7); the schema table; the session-frozen keys; and, per rung, a value copy of the preset (`FStealthServerTuning`, copied out of the loaded `UStealthTuningData`) with its `bIsPublic` flag (`Default` or `HostSelectable`) and `PresetId`.
- **F-1, per key (CR-6 a, b):** non-finite float (tested **before** clamping) or undeclared enum value → schema default + warning; otherwise clamp to `[HardMin, HardMax]`, warning naming key, authored and effective value; exactly on a bound → no warning; bools unchanged. Safe-range excursions → warning only (AC-26).
- **Invariants (CR-6 c):** one table, `TuningInvariants.cpp` in the Foundation module, rows `{ Id, Owner, Check }`:
  - `Id` equals the GDD ID (`I-1`, `I-ASN-4`, `W-NKP-3`, …), so the report names exactly the row (AC-30);
  - `Check` is a pure function `ETuningCheck (const FStealthServerTuning&, const FTuningDerived&, const FTuningCheckContext&)` returning `Pass`, `Warn` or `Reject`, evaluated **on the clamped set**. `FTuningDerived` holds the F-3 indicators computed in double precision; `FTuningCheckContext` holds `bIsPublic` and `PresetId`. The return value (not a fixed severity column) lets one row express the GDD's mixed rows (TD-ADR concern 1): I-OC2 rejects below 100 s and warns below 163 s; I-HUD-4 has a reject clause and a warn clause (the worst result wins, and the report names the clause); I-3, I-4 and I-NKP-1 reject only when `bIsPublic` and warn otherwise; W-RMF-4 does not apply when `PresetId == Solo`;
  - **offline** rows (O-CNB-6, O-CNB-7, I-SCL-10/11/14, the LSC layout predicate) are **not** in this table; their owners' CI or load-time checks enforce them.
- **Hard rule (CR-6, I-OC2 ruling): an invariant never writes a value.** `Check` receives a `const` set; the only code that changes a value is F-1, and only to the key's own hard range. A failing `Reject` row rejects the whole preset; a `Warn` row logs and keeps it. An owner text that says "clamp" (Camera I-CS-1, Objectives I-OC2) is implemented as reject or warn and the owner GDD is asked to match (C-W17).
- **Fallback ladder:** requested → `Default` → schema defaults (the C++ initialisers, i.e. the generated table's defaults) → `Failed`; requesting `Default` skips the repeated rung (`FallbackLevel` 2, `PresetId` `SchemaDefaults`). Output: `FTuningResolveResult { FallbackLevel, PresetId, FValidationReport (clamps, defaults applied, invariant failures by Id, warnings), TSharedPtr<const FValidatedTuning> }`.
- **Warnings** go to the module category `LogStealthFoundation` (ADR-0007 policy: one category per module) via `UE_LOGF` (format literals `"..."`, `%ls` for `TCHAR*` [V `LogMacros.h:291`]) and into the `ValidationReport`, which Round copies into the Event Log header (CR-7). Tuning values are not secret (every preset ships), so logging them is allowed; the Event Log itself still never goes through logs (Round R19, ADR-0007).

### 4. Safe-range lint (CI and editor, never at runtime)

`StealthGame.Tuning.SafeRangeLint` (editor automation test in `StealthGameEditor/Private/Tests/`, because it reads `SafeMin`/`SafeMax` metadata, which exists only under `WITH_EDITORONLY_DATA` — TD-ADR concern 8; also run as a CI step) answers: *does every value a designer is told is safe produce an accepted preset?*

- **Base point:** the schema defaults (which must themselves pass every invariant with zero warnings, AC-30).
- **Probe set per key k:** `SafeMin(k)`, `SafeMax(k)`, the midpoint, and every declared value for an enum or bool; for an `int32` safe range of 16 or fewer values, every value.
- **Evaluation:** for each probe, set only k, run F-1 and every non-offline invariant, with `bIsPublic = true` (the strictest scope). Report each `(key, probe value, invariant Id, severity)` that fails.
- **Also reported:** safe range outside hard range; default outside safe range; a key read by an invariant whose owner GDD row does not say "bounded by <Id>" (the C-W18 pattern, from the AC-3b doc lint).
- **Single-key probing only.** Joint corners (two keys at safe extremes together) are not linted; the TDC design does not promise them, and a full joint sweep over ~400 keys is not useful. The catalog check (AC-29) covers the presets that actually ship.
- **The lint reports, it never fixes** (`.claude/scripts` observation rule; AQ-12). Gate: CI fails on any finding **not** listed in the committed baseline `Source/StealthFoundation/Private/Tests/Data/TuningLintBaseline.json`; each baseline entry must cite the pending GDD change request or OQ that will remove it (today: C-W2, C-W17, C-W18, C-W19, C-W22 where still open; C-W14 duplicate latency key, C-W21 renames and OQ-12 `NpcKillStrikeLimit` minimum were resolved by the user 2026-10-25 and must not appear in the baseline). The baseline may only shrink; adding to it needs a reviewer.

### 5. Freeze, hashes, self-check

- `FValidatedTuning` is immutable after construction (`const` members exposed through `const&` only); held as `TSharedRef<const FValidatedTuning>`; no mutator exists (AC-30 FrozenImmutable).
- **Hash: an in-project FNV-1a-64 over the F-2 byte stream** (`u32le(SchemaVersion)`, keys sorted by byte-wise ASCII compare, `u8 len | ascii | u8 typeTag | canon(value)`; ints and enums widened to `int64` LE; floats as IEEE-754 LE with `-0.0` → `+0.0`). Implemented as a ~15-line function in `FTuningHasher` over bytes, with a committed golden fixture (AC-5). Not `GetTypeHash` (not stable by contract), not `UE::HashStringFNV1a` (string API that widens each character to 4 bytes [V]), not CityHash (stable but its exact output is an engine-version detail we do not need to depend on).
- `TuningHash` covers all keys and is never replicated; `ClientHash` covers public keys only (CR-7).
- **Self-check (CR-9):** after freezing, the server serialises the `FStealthTuningSnapshot` it is about to replicate through the same `NetSerialize` into an `FNetBitWriter`, reads it back into a fresh struct, recomputes `ClientHash` and compares; a difference → `TuningSnapshotFault`, pipeline `Failed`, Round returns to Lobby, no client disconnected (AC-12c).

### 6. Runtime objects and lifetime

```
UStealthTuningStore : UGameInstanceSubsystem          (Public/Server; survives seamless and hard travel)
  ├─ PresetName (set once at session open from ?Tuning=, non-Shipping; or stealth.tuning.preset)
  ├─ Catalog + preset objects (async-loaded at session open via UAssetManager; strong refs held)
  ├─ Session-frozen keys (validated once at session open)
  ├─ FTuningPipelineCore  (Idle → Resolving → Validating → Frozen / Failed; plain C++)
  └─ Current TSharedPtr<const FValidatedTuning> + FTuningResolveResult

UStealthTuningService : UWorldSubsystem, IStealthClientTuning   (one per world → correct in multi-client PIE)
  ├─ server: BeginResolve(), GetServerTuning(), OnTuningReady, OnTuningChanged (PIE)
  │          writes AStealthGameState::TuningSnapshot after the freeze + self-check
  └─ client (and listen host): GetClientTuning() → const FStealthClientTuning* from the GameState
             snapshot if bValid && RoundIndex == current epoch, else nullptr (= invalid)
```

- The **pipeline state lives in the game-instance store** (the GDD's "game-instance-scoped TDC subsystem"), because `ServerTravel` recreates the GameMode and world subsystems; the **accessor is world-scoped** (CR-10), so each PIE world reads its own GameInstance and GameState.
- Round calls `BeginResolve()` on entering `Preparing` (TDC never calls Round); the result is announced by `OnTuningReady(TuningHash)` or a `Failed` result with reason. `Reset` returns the pipeline to `Idle`.
- Loading: the catalog and every preset are registered with the Asset Manager as primary asset types `StealthTuningCatalog` and `StealthTuningPreset`, **cook rule `AlwaysCook`** [V enum]. Both classes **override `GetPrimaryAssetId()`** to return those type names: a `UPrimaryDataAsset` otherwise reports its class name as its asset type [V `DataAsset.cpp:119-120`, engine review], and the Asset Manager rules would not match. The store async-loads them at session open (`UAssetManager::LoadPrimaryAssetsWithType`) and keeps strong references; `BeginResolve` then validates already-loaded objects and falls back to a synchronous load only if the async load has not finished (logged). `?Tuning=`/`ResolveRoundPreset` matching is case-insensitive against catalog names; anything with padding, path or separator characters resolves `Default` with a warning (AC-2c).
- Preset store precedence: the store wins over a travel-URL `?Preset=` (CR-4; warning on disagreement). Per-client login options are never read.

### 7. Snapshot replication (public face)

- `FStealthTuningSnapshot` (USTRUCT, `Public/View/`): `FStealthClientTuning Values; int32 SchemaVersion; FName PresetId (≤ 16 chars); int32 RoundIndex; int32 Revision; bool bValid`, **every member a `UPROPERTY`**. `TStructOpsTypeTraits` declares `WithNetSerializer = true` **and `WithIdenticalViaEquality = true`** with an `operator==` over every member (TD-ADR concern 5: the legacy change check calls `Identical`, which without native support compares only `UPROPERTY` members [V `Class.cpp:3672-3692`], so a bare `Revision` would never trigger a resend). `NetSerialize` writes the header then every public field in **schema-table order** at full precision (floats as 32-bit, ints as 32-bit, enums as their declared `uint8`); a custom-serialised struct is sent as one unit, not field by field [V `RepLayout.cpp:5720, 5858-5860`]. At ~2 KB it exceeds one bunch and travels as partial bunches (64 KB cap [V `DataChannel.cpp:105`]); losing one partial drops that update and relies on the legacy resend [U], which spike T-1 measures under `LeakNetProfile` loss. `FName` serialises without a package map [V `CoreNet.cpp:430-438`], so the self-check's `FNetBitWriter(nullptr, …)` round trip is valid.
- Replicated on `AStealthGameState` as `UPROPERTY(ReplicatedUsing = OnRep_TuningSnapshot)`, condition `COND_None` (not initial-only: hot-reload revisions and rejoiners need it). On the GameState, never on a PlayerState (AC-7). `TuningHash` is never a replicated field (AC-16).
- The client's `OnRep_TuningSnapshot` (and the listen host's direct call after the server write, since the host gets no RepNotify) recomputes `ClientHash` from the received fields and hands it to Session's acknowledgement path (ADR-0005; Session rule 11.6, CR-9). TDC has no RPC, timer or kick rule.
- New GameState after seamless travel: `bValid` is false until the freeze; old-epoch values are invalid from travel start (AC-23).
- Size: ≤ 2,048 bytes for ~150 public keys (F-4); AC-19 measures the serialised size through the same `NetSerialize`.
- ADR-0003 registers the property on the allowlist (audience All, carrier GameState). If the project ever moves to Iris (ADR-0024), the struct needs an Iris `NetSerializer`; that is out of scope here.

### 8. Latch classes (CR-11)

- `Round`: frozen at `Preparing`; read freely from `FValidatedTuning` for the round.
- `Action`: the consumer **must** copy the value into its action record when the action starts (server and owning client) and never re-read it for that action. Enforced by code review, the latch column in the schema row (exposed as `ETuningLatch` for tooling), and AC-14a.
- `Use`: read at point of use.
- `Session`: validated at session open; carried unchanged into each round's set; invariants mixing session and round keys are evaluated at each `Preparing` against the session values.

### 9. Shipping, non-Shipping and PIE

- `#if UE_BUILD_SHIPPING`: `ResolveRoundPreset` returns `Default` unconditionally; `?Tuning=` parsing, `stealth.tuning.preset` and `stealth.tuning.dump` are not compiled. `stealth.tuning.preset` is also excluded from Test (`UE_BUILD_TEST`). Each excluded block contains a unique sentinel literal for the AC-22 binary scan (ASCII and UTF-16LE). The scan has a **positive control**: the same scan must find every sentinel in the Development binary, so a sentinel the linker stripped cannot make the Shipping scan pass vacuously (engine review note 6). Sentinels are referenced (e.g. logged once at registration) so they are not dead-stripped.
- `HostSelectable` is honoured only in Development and Test.
- **PIE hot-reload (`#if WITH_EDITOR` only):** the store subscribes to `FCoreUObjectDelegates::OnObjectPropertyChanged` [V, editor-only delegate], and **accepts only** events on the preset in use whose `ChangeType` is `EPropertyChangeType::ValueSet` [V] with a non-null property (TD-ADR concern 4: rejecting only `Interactive` would still let undo through, which arrives via `PostEditUndo` → `PostEditChange()` with an `Unspecified` event and null property [V `Obj.cpp:885-889`], and CR-12 excludes undo and reimport; slider drags are `Interactive` [V] and followed by one `ValueSet`), debounces 0.25 s on the editor ticker, and routes the re-validation to the store of the **PIE server world's** game instance (the PIE world context whose net mode is not `NM_Client`). The live-set rule of CR-12 applies (frozen `Round` values + edited `Action`/`Use` values; `Round` edits are pending until `Reset`); a passing changed set publishes with `Revision + 1`, a failing one is rejected and the previous set stays live; the round is flagged `HotReloaded`.

### Architecture Diagram

```
 EDITOR / CI                                  RUNTIME (server)                               RUNTIME (client / host)
 ───────────                                  ────────────────                               ───────────────────────
 UPROPERTY decls (metadata)                   UStealthTuningStore (GameInstance)
   │ StealthTuningSchemaGen                     PresetName ◄─ ?Tuning= (non-Shipping)
   ▼                                            Catalog + presets (AlwaysCook, preloaded)
 StealthTuningSchema.generated.inl ──────────►  FTuningPipelineCore
   (constexpr rows, fingerprint)                  Resolve → F-1 clamp → invariants ──► reject → fall one rung
   │                                              (TuningInvariants.cpp, never repair)     (Requested→Default→Schema→Failed)
 SchemaComplete / SchemaVersionBumped             │ pass
 SafeRangeLint (+ baseline)                       ▼
 CatalogClean (AC-29)                         FValidatedTuning (immutable) ── TuningHash (server only, Event Log header)
 KnobRegistryLint (GDD ↔ schema)                  │ ClientHash + self-check (NetSerialize round trip)
                                                  ▼
                                    UStealthTuningService (World) ── GetServerTuning() ──► server systems
                                                  │ write
                                                  ▼
                                    AStealthGameState.TuningSnapshot ══ replicate (atomic NetSerialize) ══►  OnRep → GetClientTuning()
                                                                                                             recompute ClientHash
                                                                                                             → Session ack (ADR-0005)
```

### Key Interfaces

```cpp
// --- Public/View (client face; ADR-0007 places it) ------------------------------
USTRUCT() struct FStealthClientTuning { /* generated: all *_Public sub-structs */ };
USTRUCT() struct FStealthTuningSnapshot {                       // WithNetSerializer + WithIdenticalViaEquality
    UPROPERTY() FStealthClientTuning Values; UPROPERTY() int32 SchemaVersion; UPROPERTY() FName PresetId;
    UPROPERTY() int32 RoundIndex; UPROPERTY() int32 Revision; UPROPERTY() bool bValid;
    bool NetSerialize(FArchive&, UPackageMap*, bool& bOutSuccess);
    bool operator==(const FStealthTuningSnapshot&) const; };
class IStealthClientTuning {
public:
    /** Public keys for the current epoch, or nullptr (invalid) before the snapshot is valid. Never a local default. */
    virtual const FStealthClientTuning* GetClientTuning() const = 0;
    virtual uint64 GetClientHash() const = 0;                   // recomputed from received fields
};

// --- Public/Server (server face) ------------------------------------------------
USTRUCT() struct FStealthServerTuning { /* generated: all *_Public and *_Server sub-structs */ };
enum class ETuningLatch : uint8 { Round, Action, Use, Session };
enum class ETuningFallback : uint8 { Requested = 0, Default = 1, SchemaDefaults = 2 };
struct FValidatedTuning {                                       // immutable
    const FStealthServerTuning& Values() const; uint64 TuningHash() const; uint64 ClientHash() const;
    int32 SchemaVersion() const; FName PresetId() const; int32 Revision() const;
    ETuningFallback FallbackLevel() const; const FValidationReport& Report() const; };
class UStealthTuningService : public UWorldSubsystem, public IStealthClientTuning {
public:
    void BeginResolve();                                         // Round only, on entering Preparing; server + game thread
    const FStealthServerTuning* GetServerTuning() const;         // nullptr before Frozen; no pawn/team/viewer parameter (CR-5)
    TSharedPtr<const FValidatedTuning> GetValidatedTuning() const;
    FOnTuningReady OnTuningReady;                                // (uint64 TuningHash)
    FOnTuningFailed OnTuningFailed;                              // (ETuningFailReason: LoadFailed | SnapshotFault)
#if WITH_EDITOR
    FOnTuningChanged OnTuningChanged;                            // (int32 Revision), PIE only
#endif
};
class UStealthTuningStore : public UGameInstanceSubsystem {      // preset store + pipeline across travel
public:
    FName GetStoredPresetName() const;                           // for Session's ?Preset= travel option
    int32 GetSchemaVersion() const;                              // = TuningSchemaVersion for Session's join gate
    uint64 GetServerClientHash(int32 RoundIndex, int32 Revision) const;  // Session's ack comparison
};

// --- Plain C++ cores (no UWorld; headless tests) --------------------------------
struct FTuningSchemaRow { const ANSICHAR* Key; uint8 TypeTag;
                          double (*Get)(const FStealthServerTuning&); void (*Set)(FStealthServerTuning&, double); // generated, member pointers
                          double Default, HardMin, HardMax, SafeMin, SafeMax;
                          bool bPublic; ETuningLatch Latch; const ANSICHAR* Owner; };
TConstArrayView<FTuningSchemaRow> GetTuningSchema();             // generated table (empty under STEALTH_TUNING_SCHEMA_BOOTSTRAP)
enum class ETuningCheck : uint8 { Pass, Warn, Reject };
struct FTuningCheckContext { bool bIsPublic; FName PresetId; };
struct FTuningInvariant { const ANSICHAR* Id; const ANSICHAR* Owner;
                          ETuningCheck (*Check)(const FStealthServerTuning&, const FTuningDerived&, const FTuningCheckContext&); };
TConstArrayView<FTuningInvariant> GetTuningInvariants();         // non-offline rows only
class ITuningPresetSource {                                      // injectable (AC-6b/6c test seam)
public: virtual TOptional<FStealthServerTuning> Load(FName PresetId, bool& bOutIsPublic) const = 0; };
struct FTuningResolveInput { FName RequestedPreset; const ITuningPresetSource* Source;
                             const FStealthServerTuning* SessionFrozen; /* Session-latched keys */ };
class FTuningPipelineCore { public: FTuningResolveResult Resolve(const FTuningResolveInput&) const; };
class FTuningHasher { public: static uint64 Hash(const FStealthServerTuning&, int32 SchemaVersion, bool bPublicOnly); };
```

### Implementation Guidelines
1. Every gameplay number **must** be a field of a `_Public` or `_Server` tuning sub-struct; a hardcoded gameplay literal is a defect (CR-1, AC-25 literal scan with a reasoned allowlist).
2. Tuning types **must never** carry `Config`/`DefaultConfig`/`GlobalUserConfig`/`PerObjectConfig`, and no gameplay value may be read from `UDeveloperSettings`, ini, `Saved/Config`, command line (Shipping) or a console variable.
3. Server code **must** read only `GetServerTuning()` / `FValidatedTuning`, never the raw `UStealthTuningData` asset. Client and presentation code **must** read only `GetClientTuning()` and **must never** include a `Public/Server/` tuning header or reference `UStealthTuningData` (AC-8 static check, ADR-0007 guard).
4. No accessor or helper **may** take a pawn, controller, team, viewer or participant parameter (CR-5, AC-21).
5. Invariant `Check` functions **must** be pure and take `const` input; **no code other than F-1 may change a tuning value**, and F-1 only to the key's own hard range. An owner request to "clamp" for an invariant is implemented as reject or warn.
6. Every reject/warn invariant **must** carry its GDD ID; adding a cross-field constraint **must** go through a TDC change request (CR-15.6) and a row in `TuningInvariants.cpp`, never a check inside the owner's system. **Queued rows requested by other ADRs (2026-10-24; warn severity, no new key; they enter `TuningInvariants.cpp` only once the TDC GDD lists them):** (a) `DeathFallSec + BodyLingerSec + BodyRemovalGraceSec < InformantRespawnDelaySec` (ADR-0017 §3, body removal before informant respawn); (b) `AttackAttemptLockoutSec > StrikeContactSec + BushStrikeEmergeSec`, strict (ADR-0018 item 5; tightens the existing I-ASN-6 `≥ StrikeContactSec + 1.1`).
7. `Action`-latched values **must** be captured into the action record at start on server and owning client; never re-read mid-action.
8. The schema table **must** be regenerated (commandlet) and committed with every tuning field change, and `SchemaVersion` bumped when the fingerprint changes; CI fails otherwise.
9. A safe-range lint finding **must not** be silenced by editing the baseline without citing a GDD change request or OQ; the lint never edits a value or a GDD.
10. Hot-reload, `?Tuning=` and console-command code **must** sit inside `WITH_EDITOR` / `!UE_BUILD_SHIPPING` (and `!UE_BUILD_TEST` for `stealth.tuning.preset`) blocks that contain their sentinel literal.
11. The hash **must** be the project's own FNV-1a-64 over the F-2 byte stream with the committed golden fixture; never `GetTypeHash`, `HashStringFNV1a` or a struct memory image.
12. Server-side tuning-ready bookkeeping **must** be keyed by `ParticipantId`, never replicated, and cleared at every epoch (CR-9; owned with ADR-0005).

## Alternatives Considered

### Alternative 1: Hand-written C++ schema rows beside the UPROPERTYs (X-macro or table)
- **Description**: Declare each key twice: the UPROPERTY (for the editor) and a hand-written `constexpr` row (or X-macro list) holding ranges, latch and audience; a test checks they agree.
- **Pros**: No code generator; the runtime table is plainly readable; matches the GDD's literal wording ("a C++ schema row per key").
- **Cons**: ~400 keys declared twice by 1-2 programmers; UHT cannot parse macro-generated UPROPERTYs, so an X-macro cannot produce both halves; drift caught only by the test.
- **Rejection Reason**: LP-FEASIBILITY concern 7 asked for "the schema declared once"; the generated table gives the same cooked-build guarantee with one declaration. Kept as the **fallback** if the generator proves costly: the generated file format is the hand-written format, so switching is a process change, not a design change.

### Alternative 2: Read UPROPERTY metadata at runtime
- **Description**: The validator reads `ClampMin`/`ClampMax` and custom meta through `FField::GetMetaData` at run time.
- **Pros**: Simplest; one declaration, no generated file.
- **Cons**: `WITH_METADATA` is `WITH_EDITORONLY_DATA` [V]: cooked Development and Shipping builds have no metadata, so the hard-range clamp would vanish exactly where it matters.
- **Rejection Reason**: Fails CR-3 in every cooked build.

### Alternative 3: Keep `UDeveloperSettings` with `Config` properties (the prototype)
- **Description**: Promote `UStealthSettings` (`Config = Game`, `DefaultGame.ini`).
- **Pros**: Already works in the prototype; editable in Project Settings.
- **Cons**: A live back door (ini and `Saved/Config` override on any machine, CR-13); no presets, no hash, no audience split, no invariants; a client reads its own local values.
- **Rejection Reason**: Violates CR-13, CR-4 and Pillar 1. Prototype is evidence only.

### Alternative 4: `UDataTable` rows or one data asset per owner domain
- **Description**: Each key a DataTable row (name, value), or one `UPrimaryDataAsset` per owner system, with the preset as a bundle of assets.
- **Pros**: Owners edit their own asset; less merge contention on one binary asset; DataTables export to CSV.
- **Cons**: DataTable rows are untyped values (no compile-time client face, AC-28 impossible); per-domain assets turn one preset into N loads, N partial-failure cases and a cross-asset invariant evaluation; the hash and fallback ladder become multi-asset.
- **Rejection Reason**: The typed client face and "one preset = one validation unit" outweigh merge contention, which the VCS lock policy (ADR-0009) handles for one asset.

### Alternative 5: Invariants that repair (clamp the dependent key) instead of rejecting
- **Description**: When I-1 or an owner invariant fails, move one key until it passes (several owners proposed this: Camera I-CS-1 "runtime clamps the blend", Objectives I-OC2 "clamp Max up").
- **Pros**: A designer's slightly-off preset still runs.
- **Cons**: Silent repair hides designer intent, makes the frozen set differ from the authored asset with no single place to see why, and choosing which key to move is itself a design decision the validator cannot make.
- **Rejection Reason**: Locked by TDC CR-6 and the reconciliation ruling; architecture principle 4 ("cross-field invariants reject or warn and never repair").

### Alternative 6: Deliver the snapshot by reliable client RPC instead of a replicated property
- **Description**: The server sends the public set to each client with a reliable RPC after travel.
- **Pros**: Atomic by construction; explicit ordering.
- **Cons**: Per-connection sends must be re-issued for rejoiners and hot-reload revisions; a per-client send timing could encode readiness; it duplicates Session's acknowledgement machinery.
- **Rejection Reason**: CR-8 chose property replication with RepNotify (identical for every viewer, rejoin-safe); atomicity comes from `NetSerialize` (spike T-1 confirms).

### Alternative 7: UHT exporter plugin generating the schema table at build time
- **Description**: A UnrealHeaderTool exporter (C# `UhtExporter`, precedent `ScriptGeneratorUbtPlugin` [V engine review]) emits the schema table during the build from the parsed UPROPERTY metadata, so nothing generated is committed.
- **Pros**: No checked-in generated file, no regeneration step, no bootstrap deadlock.
- **Cons**: UHT parses declarations, not C++ initialisers, so defaults would have to come from a default-constructed struct at runtime (a second path); a C# build plugin is more tooling than 1-2 programmers want to own; failures surface as opaque build errors.
- **Rejection Reason**: Not rejected outright: it is the **upgrade path** if the commandlet + bootstrap workflow proves error-prone. The generated table format is identical, so switching changes tooling only.

## Consequences

### Positive
- One declaration per key; ranges survive cooking; the client face cannot express a server key at compile time.
- The CR-6 rule is structural: invariants receive `const` data, so "never repair" cannot be broken by accident.
- The safe-range lint turns a class of cross-GDD review findings (C-W2, C-W17, C-W18, C-W19, C-W22) into a mechanical CI check, and its baseline makes the pending user decisions (AQ-12) visible instead of silently fixed.
- Validator, hasher and schema run headless; every TDC AC except the multi-process ones is a fast editor test.
- The prototype's back door is removed by construction, and Shipping cannot select a preset.

### Negative
- A code generator and a checked-in generated file: one more editor tool and a regenerate-and-commit step on every tuning field change (mitigated by the drift test and the fallback to Alternative 1).
- One large binary preset asset per preset: merge contention between owners (VCS lock policy, ADR-0009).
- The invariant table is a single Foundation file that every owner's change request touches.
- A replicated struct of ~2 KB must be re-sent whole on every PIE revision (editor only) and once per round; negligible bandwidth but not delta-compressed.
- Iris would need extra serializer work (deferred to ADR-0024).

## Risks
- **R-1 Snapshot delivery under loss** [U]: the struct is applied as one unit [V], but at ~2 KB it travels as unreliable partial bunches, and a lost partial drops the whole update until the legacy resend; a rejoiner's first `OnRep` is untested. *Mitigation:* spike T-1 in the multi-process harness (ADR-0008) before the Foundation tuning epic closes; fallback is Alternative 6 (reliable RPC + replicated `RoundIndex`/`Revision` header) behind the same client face.
- **R-2 Cook misses a preset** [U for soft-ref following]. *Mitigation:* presets themselves registered `AlwaysCook` [V enum]; AC-27 in a cooked Dev build.
- **R-3 Generator/reflection drift** (e.g., a new owner sub-struct not picked up) or a **regeneration deadlock** after a field rename. *Mitigation:* `SchemaComplete` regenerates in memory and diffs; `SchemaVersionBumped` fingerprint test; member-pointer accessors make a stale row a compile error; `STEALTH_TUNING_SCHEMA_BOOTSTRAP` builds an empty table so the commandlet can run; Alternative 7 as the upgrade path.
- **R-8 Hot-reload over- or under-triggers** (undo, reimport, multi-property paste). *Mitigation:* accept only `ValueSet` with a non-null property on the preset in use; AC-14b counts revisions for slider drags and undo.
- **R-4 Hot-reload routes to the wrong PIE world** in multi-client PIE. *Mitigation:* route by world context net mode; AC-14a with 3 PIE worlds; advisory-only re-check (CR-12).
- **R-5 Lint baseline becomes a dumping ground.** *Mitigation:* each entry must cite an open GDD CR/OQ; CI fails if a cited item is closed in the GDD while the entry remains (doc lint).
- **R-6 Float canonicalisation differences across compilers** (denormals, `-0.0`). *Mitigation:* F-1 never emits `-0.0` (all float hard minima > 0) and the hasher maps it anyway; golden fixture across the CI platform matrix (AC-5).
- **R-7 Preparing hitch from synchronous load** if async preload has not finished. *Mitigation:* preload at session open; synchronous fallback logged; preset is a small asset.

## GDD Requirements Addressed

| GDD System | Requirement | How This ADR Addresses It |
|------------|-------------|--------------------------|
| tuning-data-config.md | CR-1 everything gameplay-relevant is a key | §1 sub-structs; guideline 1; AC-25 literal scan |
| tuning-data-config.md | CR-2 one data-asset class, presets, catalog with `HostSelectable`, all presets cooked, Shipping resolves `Default` | §1, §6 (Asset Manager `AlwaysCook`), §9 |
| tuning-data-config.md | CR-3 C++ schema owns hard ranges; metadata a mirror; allowed types; range pairs | §2 generated table from metadata, drift test; types restricted |
| tuning-data-config.md | CR-4 server authority; game-instance preset store; store wins over URL; client options ignored | §6 `UStealthTuningStore` |
| tuning-data-config.md | CR-5 global and uniform; no per-participant accessor | Key Interfaces (no parameters); guideline 4; AC-15, AC-21 |
| tuning-data-config.md | CR-6 validation ladder; invariants reject whole preset, never repair; fallback rungs | §3 |
| tuning-data-config.md | CR-7 freeze, `TuningHash`/`ClientHash`, not replicated, Event Log header | §5; §3 report to Round |
| tuning-data-config.md | CR-8 audience split structural; GameState snapshot; atomic; full precision; invalid until `bValid` | §1 faces, §7 |
| tuning-data-config.md | CR-9 one acknowledgement owned by Session; server self-check | §5 self-check, §7 hand-off to ADR-0005 |
| tuning-data-config.md | CR-10 world-scoped accessor with two faces; typed client face | §6, §1, Key Interfaces |
| tuning-data-config.md | CR-11 latch classes Round/Action/Use/Session | §8 |
| tuning-data-config.md | CR-12 PIE-only hot-reload, committed edits, debounced, PIE server world | §9 |
| tuning-data-config.md | CR-13 no back doors; sentinel literals | §1, §9, guideline 2, 10 |
| tuning-data-config.md | CR-14 client-local settings are not tuning | guideline 2 (settings live outside the tuning types; ADR-0021 owns settings) |
| tuning-data-config.md | CR-15 knob registry convention, AC-3a/AC-3b | §2 schema test, doc lint compares GDD rows with schema rows |
| tuning-data-config.md | F-1 validate; F-2 canonical hash; F-3 derived indicators; F-4 snapshot ≤ 2,048 B | §3, §5, `FTuningDerived`, §7 |
| tuning-data-config.md | I-1 to I-5 and owner invariants (reject/warn table, offline rows excluded) | §3 invariant table |
| gdd-cross-review-2026-10-24.md | Note for `/create-architecture`: safe ranges must satisfy all invariants at defaults of other keys (C-W2, C-W17, C-W18, C-W19, C-W22) | §4 safe-range lint |
| architecture.md AQ-12 | Pending tuning-schema items reported, not fixed | §4 baseline (C-W14, C-W21, OQ-12 resolved 2026-10-25 and removed from the baseline) |
| All 18 owner GDDs | Tuning Knobs tables (~400 keys) | §1-§2 schema rows; AC-3b lint |

## Performance Implications
- **CPU**: zero per frame. Validation at `Preparing`: ~400 keys × F-1 + ~60 invariants + 2 hashes over ~4 KB, well under 1 ms (estimate; AC timing captured in the Preparing budget). Accessor reads are a pointer dereference.
- **Memory**: one loaded preset per catalog entry (a few KB each) + one `FValidatedTuning` + the snapshot; < 100 KB total.
- **Load Time**: presets preloaded asynchronously at session open; no load on the Preparing critical path in the normal case.
- **Network**: ≤ 2,048 bytes per client per round (and per PIE revision); 8 bytes of `ClientHash` in Session's ack; nothing per tick (F-4: 4 % of one second of the 50 KB/s budget, once).

## Migration Plan
1. The prototype `UStealthSettings` and its `DefaultGame.ini` section are **replaced, not migrated** (architecture.md promotion rule). Each of its gameplay fields maps to an owner key listed in TDC Cross-References (OQ-8); owners register them through CR-15.
2. Removing the prototype's `Config` gameplay fields is a gate before any external playtest (CR-13).
3. Order: (a) Foundation module with schema generator, pipeline core, hasher and their unit tests (headless); (b) `Default` preset asset authored from the GDD Default columns, `CatalogClean` and `SafeRangeLint` green with baseline; (c) store + service + GameState snapshot; spike T-1; (d) Session ack integration (ADR-0005); (e) PIE hot-reload.
4. Project config changes this ADR needs (Asset Manager primary asset types with `AlwaysCook` in `DefaultGame.ini`) are applied by the implementing story, not by this design run (Config/ is out of this run's write scope).

## Validation Criteria
- TDC AC-1 to AC-31 pass at their stated environments (editor, Dev build, multi-process harness per ADR-0008).
- New for this ADR: `StealthGame.Tuning.SchemaVersionBumped` (fingerprint vs `SchemaVersion`), `StealthGame.Tuning.SafeRangeLint` (no finding outside the cited baseline), `StealthGame.Tuning.SnapshotAtomic` (spike T-1: a held-back snapshot delivered after a forced partial-update scenario is applied whole; a rejoiner receives the current revision).
- The validator suite runs headless with `-nullrhi` in < 10 s.
- Zero Shipping sentinels found by the binary scan (AC-22).

## Related
- ADR-0001 (crowd identity; net-driver choice) — depends on.
- ADR-0003 (replication; allowlist entry for `TuningSnapshot`).
- ADR-0005 (session acknowledgement carries `ClientHash`; join gate `TuningSchemaVersion`; `?Preset=`; game-instance stores).
- ADR-0007 (module layout; places the two faces; compile guard) — accept together.
- ADR-0008 (test harness: multi-process runs for AC-2e, AC-8, AC-12b, AC-13, AC-23, spike T-1).
- ADR-0009 (VCS lock policy for preset assets).
- ADR-0024 (Iris serializer if the driver changes).
- **GDD sync (2026-10-24):** `tuning-data-config.md` Acceptance Criteria preamble names the test root `Source/StealthGame/Private/Tests/`; under ADR-0007's module layout the runtime-core tests live in `Source/StealthFoundation/Private/Tests/` and metadata tests in `Source/StealthGameEditor/Private/Tests/` (test names `StealthGame.Tuning.*` unchanged). ADR-0007's facade example names the client struct `FStealthPublicTuning`; the final name is `FStealthClientTuning` (this ADR, matching architecture.md API Boundaries). Neither document was edited by this run.
- Design: `design/gdd/tuning-data-config.md` (CR-1..CR-15, F-1..F-4, invariants, AC-1..AC-31, OQ-2 answered here, OQ-1/OQ-6/OQ-12 closed by the user 2026-10-25, OQ-3 still open); `design/gdd/gdd-cross-review-2026-10-24.md` (C-W2, C-W14, C-W17-C-W22); `docs/architecture/architecture.md` (Foundation Tuning row, API Boundaries, AQ-12).

## Amendments (2026-10-24 sync)
Inter-ADR amendment pass; decision unchanged, Status unchanged (Proposed).
- Guideline 6: cross-field warning `DeathFallSec + BodyLingerSec + BodyRemovalGraceSec < InformantRespawnDelaySec` queued (enters through a TDC change request per CR-15.6) — requested by ADR-0017.
- Guideline 6: cross-field warning `AttackAttemptLockoutSec > StrikeContactSec + BushStrikeEmergeSec` queued as a tightening of I-ASN-6 — requested by ADR-0018 (item 5).
- The rows themselves are GDD (TDC) content; this ADR's mechanism needs no change. The TDC GDD edits are left to its owner (not in this pass's scope).

Architecture-review fix pass (`docs/architecture/architecture-review-2026-10-25.md`); decision unchanged, Status unchanged (Proposed).
- ADR Dependencies: the edge to the deferred, unwritten ADR-0024 is reworded as a conditional note in the Ordering Note (applies only if Iris is adopted); Depends On keeps ADR-0001 only — architecture-review fix 2026-10-25 (dependency order).
- Naming (Conflict 7): ADR-0017's `FConcealClientTuning` removed in ADR-0017; its pure functions take this ADR's client face `FStealthClientTuning` and read `FConcealmentTuning_Public` / `FAssassinationTuning_Public`; no change to this ADR — architecture-review fix 2026-10-25 (Conflict 7).

User decisions (2026-10-25); decision unchanged in substance, Status unchanged (Accepted).
- §1 catalog, F-1 — user decision 2026-10-25: `NpcKillStrikeLimit` hard range 2-5; catalog flag `bTestAllowsStrikeLimitOne` allows 1 only in non-selectable test presets (Penalty OQ-NKP-11, TDC OQ-12).
- §1, §9 — user decision 2026-10-25: Shipping uses `Default` only; host-selectable presets never ship (TDC OQ-6).
- Scope — user decision 2026-10-25: Tuning Data stays a GDD; this ADR covers the implementation (TDC OQ-1 / PR-SCOPE 6 declined).
- Schema rows — user decision 2026-10-25: TDC CR-10 renames (`bConcealServerCull`, `EyeMaskHideFailCount`, `StickDeadZoneFrac`/`StickOuterZoneFrac`, `LayoutSliceWorkUnitsLimit`, `RespawnPoolMinExpectedCount`); `LatencyWarnRttSec` retired (HUD's `HudLatencyWarnRttSec` stays); new keys `LayoutExactRefineMaxCount` (LSC), `CrowdMotionProfileAsset`, `CrowdFullStateTreeAsset`, `CrowdRestrictedStateTreeAsset` (Crowd `_Server` sub-struct, soft references; suffix `Asset` added to the CR-15.2 list); `NpcCount` default 50. The lint baseline drops C-W14, C-W21 and OQ-12.

User scope decisions (2026-10-25: "accept all recommendations"); decision unchanged, Status unchanged (Accepted).
- Key registry — user decision 2026-10-25: new owner `InformantPings` with five keys (`PingLifetimeSec` Client/Use; `PingBucketSize`, `PingBucketPerSec`, `PingRangeCm` All/Round; `PingRangeSlackCm` ServerOnly/Round) and the owner invariant I-PNG-1 (warn: `PingBucketPerSec <= 0.5` when `PingLifetimeSec >= 6`), registered in TDC. Crowd's `CrowdFullPacePct` default changes 100 → 55 (the keyboard walk-slower modifier now exists, IIF R15a). No schema change.

Closeout 2026-10-25 (re-review follow-ups, `production/design-closeout/2026-10-25_rereview-followups.md`); decision unchanged, Status unchanged (Accepted).
- TDC re-review follow-ups (the registry and the TDC GDD are owned by other agents; this ADR's schema and pipeline are unchanged): (D2) new key `StopMinSec` (0.5 s, safe 0.4-0.7, hard 0.2-1.0), owner SCL; (D6) new key `CameraMaskOffHysteresisCm` (10, mask off at 20 cm, back on at 30 cm; amended 2026-10-25 by the cross-review fix C-W25 to off below 25 cm, on above 35 cm); (D8) hard minimum 1.25 s for `HudResultPlateMaxHoldBackSec` and `HudOwnRemovalHoldCapSec`; (D12) validator warning W-NKP-4 is kept as a defensive guard and is documented "unreachable in hard ranges". The validator's range, cross-field and reject-vs-warn semantics are unchanged (registry I-4 reads "rejects", not "warns").
- **Deferred (closeout D13): the Shipping cook excluding non-Default presets** is not decided here; it becomes an ADR-0004 amendment later (pre-ship), and the TDC open question carries it. Nothing in this ADR changes now.
