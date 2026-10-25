# Story 045: AC-5 HashStableAndSensitive: FNV-1a-64 over the F-2 byte stream

> **Epic**: Tuning Data & Config
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/tuning-data-config.md`
**Requirement**: `TR-tdc-005` (requirement text lives in `docs/architecture/tr-registry.yaml`; read fresh at review time); GDD AC-5, CR-7, F-2 (canonical hash)

**ADR Governing Implementation**: ADR-0004: Tuning data pipeline and validator (section 5, freeze, hashes, self-check)
**ADR Decision Summary**: The hash is the project's own FNV-1a-64 over the F-2 byte stream, a small function in `FTuningHasher` over bytes with a committed golden fixture. `TuningHash` covers all keys and is never replicated; `ClientHash` covers public keys only. It is not `GetTypeHash` (not stable by contract) and not `UE::HashStringFNV1a` (a string API that widens each character to 4 bytes).
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: LOW
**Engine Notes**: No post-cutoff API. [V] `UE::HashStringFNV1a` hashes strings and widens every character to 4 bytes, so it is not a byte-buffer hash (`Core/Public/Hash/Fnv.h`); this is why the function is written in-project. Little-endian encoding is explicit in the F-2 stream, so the code never depends on host endianness. No unverified [U] claim is touched.

**Control Manifest Rules (this layer)**:
- Required: The tuning hash is the project's own FNV-1a-64 over the F-2 byte stream, with the committed golden fixture (source: ADR-0004)
- Forbidden: Never hash with `GetTypeHash`, `HashStringFNV1a` or a struct memory image (source: ADR-0004)
- Required: Queue, Roster, tuning validator, seed mixer and appearance deck are plain C++ cores (`F...Core`) with no `UWorld` dependency (source: ADR-0007)
- Guardrail: Tuning costs 0 per frame; validation under 1 ms at `Preparing` (source: ADR-0004)

---

## Acceptance Criteria

*From GDD `design/gdd/tuning-data-config.md` AC-5, scoped to this story:*

- [ ] **AC-5 (CR-7, F-2)**: GIVEN the committed fixture (fixed `SchemaVersion` and one key of each type tag), WHEN hashed on every CI platform, THEN the result equals the committed golden value. AND all of the following hold: permuting the input key order gives the same hash; a one-bit change to any field changes it; the hash function maps `-0.0` and `+0.0` equally; changing a server-only key changes `TuningHash` but not `ClientHash`, and changing a public key changes both; `Default` hashes equally on all CI platforms. (`StealthGame.Tuning.HashStableAndSensitive`)

---

## Implementation Notes

*Derived from ADR-0004 section 5, Key Interfaces and GDD F-2:*

- Files: `Source/StealthFoundation/Public/Server/StealthTuningHasher.h` (guard header first), `Private/Tuning/StealthTuningHasher.cpp`, test `Private/Tests/Tuning/HashStableAndSensitiveTest.cpp`. Plain C++ core, no `UWorld`. Interface from the ADR: `class FTuningHasher { static uint64 Hash(const FStealthServerTuning&, int32 SchemaVersion, bool bPublicOnly); };`. The golden fixture has its own key set (one key per type tag), so the hasher must also accept a schema view and a value source rather than only the real schema (the ADR signature does not show it; the story 050 amendment adds an injectable schema view to `FTuningResolveInput` for the same reason). Settle the overload when writing the fixture (open).
- Byte stream (F-2): `u32le(SchemaVersion)`, then for each key sorted by byte-wise case-sensitive `memcmp` of its ASCII name: `u8(len) | ascii(name) | u8(typeTag) | canon(Effective(value))`. Canon: bool one byte (0 or 1); int32 and enum widened to int64, 8 bytes little-endian (enum is its declared value); float 4 bytes IEEE-754 little-endian with `-0.0` mapped to `+0.0` (a NaN is never hashed because F-1 removes it). Schema constants that are not preset fields (for example `BlindGuessWarnPct`) are never hashed. `TuningHash` hashes all keys, `ClientHash` only keys whose row is `bPublic`.
- Hash64 is FNV-1a-64 (offset basis 14695981039346656037, prime 1099511628211) fed byte by byte; the constants are the standard FNV-1a-64 parameters, verify them against the FNV specification when writing, they are not stated in the inputs. About 15 lines. Never `GetTypeHash`, `UE::HashStringFNV1a`, `CityHash` or a struct memory image.
- Golden value: the spec requires an independently computed golden, not generated from the implementation under test. Compute it outside the code under test (for example a throwaway script or by hand from the GDD byte layout) and commit both the fixture and the value. The GDD F-2 Example gives two encoded entries (`TeamSize` int 5 as `08 | "TeamSize" | 01 | 05 00 00 00 00 00 00 00`; `DefenceDurationSec` float 4.0 as `12 | "DefenceDurationSec" | 02 | 00 00 80 40`) that the test should also assert at the byte-stream level, before hashing.
- The golden fixture is not the `Default` preset (that changes on every retune). `Default` is checked only for equal hashes across the CI platform matrix; MSVC x64 is the only platform run today, so claim cross-platform equality only for configurations actually run (ADR-0008 section 9 wording).
- Sensitivity cases: key-order permutation gives the same hash (the sort makes the input order irrelevant); one-bit flip per field and per type tag changes the hash; `-0.0` equals `+0.0` tested on the hash function directly (F-1 never emits `-0.0`); a server-only key change moves `TuningHash` but not `ClientHash`; a public key change moves both.
- Test class `FTuningHashStableAndSensitiveTest`, name `StealthGame.Tuning.HashStableAndSensitive`, flags `EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter`, whole file in `#if WITH_DEV_AUTOMATION_TESTS`, headless `-nullrhi`. Because the golden and sensitivity cases also need to catch a broken hash, include a negative control (a hasher variant that ignores one field must be detected) using `FStealthNegativeControl` if story 010 has landed (not a listed dependency of this story).
- Prototype: none (no `Prototype:` line in the plan).

---

## Out of Scope

*Handled by neighbouring stories, do not implement here:*

- `production/epics/tuning-data-config/story-041-schema-generator-commandlet.md`: the schema table the hasher walks.
- `production/epics/tuning-data-config/story-050-resolve-fallback-to-default.md`: `Resolve()` and freezing (this story provides the hasher it calls).
- `production/epics/tuning-data-config/story-054-validated-tuning-frozen-immutable.md`: `FValidatedTuning` immutability and the unchanged hash at `Ended`.
- `production/epics/tuning-data-config/story-070-snapshot-struct-size-budget.md` and `production/epics/tuning-data-config/story-073-gamestate-snapshot-public-only.md`: client-side recomputation of `ClientHash` from the replicated snapshot.
- `production/epics/tuning-data-config/story-083-full-hash-not-replicated.md`: `TuningHash` is not replicated.
- `production/epics/tuning-data-config/story-043-schema-fingerprint-version-bump.md`: the schema fingerprint (a different hash).

---

## Files

- `Source/StealthFoundation/Public/Server/StealthTuningHasher.h`
- `Source/StealthFoundation/Private/Tuning/StealthTuningHasher.cpp`
- `Source/StealthFoundation/Private/Tests/Tuning/HashStableAndSensitiveTest.cpp`

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these, do not invent new test cases during implementation.*

- **AC-1**: The hash equals the committed golden and is stable and sensitive as stated.
  - Given: a committed golden fixture (fixed `SchemaVersion`, one key per type tag) and an independently computed golden value (not generated from the implementation under test).
  - When: hashed; keys permuted; one bit flipped; -0.0 vs +0.0; a server-only key changed; a public key changed; Default hashed.
  - Then: equals golden; permutation equal; bit flip differs; -0.0 equals +0.0; a server-only change alters `TuningHash` not `ClientHash`; a public change alters both; Default identical on all CI platforms.
  - Edge cases: no `GetTypeHash`/`UE::HashStringFNV1a`/memory image (ADR-0004 section 5); each type tag flipped.

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Tuning/HashStableAndSensitiveTest.cpp` (`StealthGame.Tuning.HashStableAndSensitive`) must exist and pass.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-041-schema-generator-commandlet.md`
- Unlocks: `production/epics/tuning-data-config/story-050-resolve-fallback-to-default.md`
- Open verification claims from the plan Note lines: none. The plan Note ("never `GetTypeHash`, `UE::HashStringFNV1a` or a struct memory image") is a settled rule, not an open claim. Open design point: the hasher overload that takes a fixture schema.
