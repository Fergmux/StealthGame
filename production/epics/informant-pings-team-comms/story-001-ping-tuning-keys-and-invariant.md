# Story 001: Ping tuning keys and invariant I-PNG-1

> **Epic**: Informant Pings & Team Communication
> **Status**: Ready
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/informant-pings-team-comms.md` (Tuning Knobs; Formulas F1-F3 for the variables)
**Requirement**: `TR-png-022`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0004: Tuning data pipeline and validator
**ADR Decision Summary**: One cooked data-asset class of per-owner sub-structs split by audience (`_Public` and `_Server` structs), declared once in `UPROPERTY` metadata and compiled into a generated schema table; a plain-C++ validator resolves, validates and freezes the values server-side (TuningHash and ClientHash), invariants reject or warn and never repair by clamping a dependent key, and there is no config, CVar or command-line back door.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: LOW (ADR-0004; pure C++ over data assets)
**Engine Notes**: No post-cutoff API is used by this story. Never mark a tuning type `Config`, `DefaultConfig`, `GlobalUserConfig` or `PerObjectConfig`, and never read a Ping value from `UDeveloperSettings`, ini or a console variable.

**Control Manifest Rules (this layer)**:
- Required: tuning values reach code only through the frozen snapshot (`GetServerTuning()` on the server, `IStealthClientTuning` on a client); the five keys register under owner `InformantPings` with the TDC CR-15 columns — source: ADR-0004
- Forbidden: Never repair a failed invariant by clamping a dependent key — source: ADR-0004
- Guardrail: schema rows are C++ rows matched to the `UPROPERTY` declarations by the doc-lint and schema-row-match stories of the tuning-data-config epic

---

## Acceptance Criteria

*From GDD `design/gdd/informant-pings-team-comms.md` (Tuning Knobs, I-PNG-1, CR-15.8), scoped to this story:*

- [ ] **AC1** (TR-png-022): **GIVEN** the schema table **WHEN** inspected **THEN** owner `InformantPings` holds exactly `PingLifetimeSec` (float, 8.0, hard 4.0-12.0, safe 6.0-10.0, Client, latch Use), `PingBucketSize` (int32, 2, hard 1-5, safe 2-3, All, Round), `PingBucketPerSec` (float, 0.5, hard 0.1-2.0, safe 0.33-0.5, All, Round), `PingRangeCm` (float, 6000, hard 2000-15000, safe 4000-8000, All, Round) and `PingRangeSlackCm` (float, 150, hard 50-400, safe 100-200, ServerOnly, Round); a value outside a hard range is rejected by the validator and a value outside a safe range only warns.
- [ ] **AC2** (I-PNG-1): **GIVEN** `PingBucketPerSec` 0.6 and `PingLifetimeSec` 8 **WHEN** validated **THEN** the warning I-PNG-1 is reported and no dependent key is clamped; **GIVEN** 0.5 with 8, or 0.6 with `PingLifetimeSec` 5, **THEN** no I-PNG-1 warning appears.
- [ ] **AC3** (derived, TDC CR-15.8 and the GDD constants rule): **GIVEN** the schema **WHEN** scanned **THEN** no Ping key varies by role, team, kind or outcome, none of the constants (`PingSkewTolSec` 0.1 s, ring radius, 60 degree gap, stroke weights, one slot per sender) is a key, and `PingRangeSlackCm` is absent from the client snapshot type (it lives in the `_Server` sub-struct).

---

## Implementation Notes

*Derived from ADR-0004 Decision §1, §2, §3, §8 and the GDD Tuning Knobs table:*

Register owner `InformantPings` with two sub-structs: the `_Public` struct carries `PingLifetimeSec` (Client audience), `PingBucketSize`, `PingBucketPerSec`, `PingRangeCm` (All audience, so the client's strict bucket mirror and aim range read the same value as the server); the `_Server` struct carries `PingRangeSlackCm` only. The latch for the four Round-latched keys is `Round` (read at round start), `PingLifetimeSec` is `Use` (read when a ring is created). I-PNG-1 is a cross-field **warning** (`PingBucketPerSec <= 0.5` whenever `PingLifetimeSec >= 6`) registered with the owner-invariant mechanism; it bounds the cadence of one sender's rings and does not prevent manual shadowing (OQ7 tracks that, story 025). `PingSkewTolSec` stays a code constant in the Pings core (story 004), never a key.

Keys with the safe ranges above must pass the CI safe-range lint (ADR-0004 §4). The Pings epic reads values only through the frozen snapshot; no story in this epic reads a literal tuning number.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 004: the token-bucket maths that reads `PingBucketSize`, `PingBucketPerSec` and the constant `PingSkewTolSec`.
- Story 005: the range check that reads `PingRangeCm` and `PingRangeSlackCm`.
- Story 016: the ring age and stroke that read `PingLifetimeSec`.
- tuning-data-config epic: the schema generator, validator, freeze and snapshot replication themselves.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthFoundation/Public/View/StealthTuningInformantPings.h` (the `_Public` sub-struct)
- `Source/StealthFoundation/Public/Server/StealthTuningInformantPingsServer.h` (the `_Server` sub-struct; guard header first)
- `Source/StealthFoundation/Private/Tuning/InformantPingsInvariants.cpp` (I-PNG-1)
- `Source/StealthFoundation/Private/Tests/Tuning/InformantPingsTuningTest.cpp` (class `FInformantPingsTuningTest`, name `StealthGame.Pings.TuningKeys`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

**Test: AC1 five keys with exact metadata; hard range rejects, safe range only warns**
- Given: the tuning schema, owner `InformantPings`.
- When: enumerate the keys and validate each at, just inside and just outside its hard and safe bounds.
- Then: exactly the five listed keys with the listed type, default, ranges, audience and latch; outside hard is rejected; inside hard but outside safe is accepted with a warning.
- Edge cases: exact bounds (4.0, 12.0, 6.0, 10.0) accepted with no warning; `PingBucketSize` 2.5 rejected as non-int; `PingRangeSlackCm` is ServerOnly.

**Test: AC2 I-PNG-1 cross-field warning**
- Given: the validator.
- When: validate (0.6, 8), (0.5, 8) and (0.6, 5) as (`PingBucketPerSec`, `PingLifetimeSec`).
- Then: only (0.6, 8) reports I-PNG-1; no value is clamped (read back unchanged).
- Edge cases: (0.5, 6) no warning; (0.6, 6) warns; (0.6, 5.99) no warning.

**Test: AC3 no per-role/team/kind keys; constants not keys; slack absent from the client snapshot**
- Given: the schema and the client snapshot struct.
- When: reflect all Ping keys and snapshot fields.
- Then: no key name or scope varies by role, team, kind or outcome; none of `PingSkewTolSec`, ring radius, 60 degree gap, stroke weights or slot count is a key; `PingRangeSlackCm` is not a snapshot property.
- Edge cases: adding a key named like a constant fails the test (negative control).

QL-STORY-READY: ADEQUATE

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Tuning/InformantPingsTuningTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-038-schema-row-types.md`, `story-039-preset-asset-classes.md`, `story-055-owner-invariant-mechanism.md` (cross-epic)
- Unlocks: `production/epics/informant-pings-team-comms/story-004-ping-token-bucket-core.md`, `story-005-server-validation-core.md`, `story-016-ring-model-and-age.md`
