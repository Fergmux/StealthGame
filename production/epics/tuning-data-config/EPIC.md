# Epic: Tuning Data & Config

> **Layer**: Foundation
> **GDD**: design/gdd/tuning-data-config.md
> **Architecture Module**: Tuning (TDC) in `StealthFoundation`, plus the cross-cutting bootstrap of the module skeleton, test harness and CI
> **Status**: Ready
> **Stories**: 100 stories (99 Ready, 1 Blocked); see the Stories table below
> **Engine Risk**: MEDIUM (highest among governing ADRs; engine 5.8, installed 5.8.4)
> **Control Manifest**: 2026-10-25e
> **Build Order**: 1 of 19 (layer order; the risk-first sequence is in the Build Order and Gates section)

## Overview

Tuning Data & Config is the Foundation data convention for the whole game: every gameplay number is a key in one cooked data-asset class of per-owner sub-structs, validated against a hard range by a reflection-driven validator, frozen server-side when a round enters `Preparing`, stamped with a `TuningHash`, and delivered to clients only as a public-key snapshot (`ClientHash`) identical for every viewer. The epic delivers the schema (C++ rows for type, default, hard range, audience and latch, with a compiled schema table generated from the metadata so cooked builds do not lose range data), the resolve ladder with its fallback rungs and `ValidationReport`, the cross-field invariants that reject or warn and never clamp, the world-scoped accessors (`IStealthClientTuning` on the View side, `UStealthTuningService` on the Server side, so a server-only key cannot even be named in client C++), PIE-only hot reload, and the registration of the eight canonical keys (`TeamSize`, `NpcCount` default 50, `AssassinationRangeCm`, `DefenceDurationSec`, `DefenceCooldownSec`, `NpcKillStrikeLimit`, `ObjectiveInteractSec`, `TargetRoundMinutes`).

Because it is first in build order it also hosts the cross-cutting bootstrap that has no GDD of its own and that every later epic needs: the ADR-0007 module skeleton (`StealthFoundation`, `StealthGameplay`, `StealthPresentation`, `StealthGame`, `StealthGameEditor`, and the Shipping-excluded `StealthTestHarness`) with the Presentation compile guard and include-lint; the ADR-0008 headless test and multi-client leak-parity harness with `-nullrhi` CI; and the ADR-0009 version-control and binary-asset workflow. This was logged as a planning decision (create-epics decision log) rather than adding a twentieth epic; the PR-EPIC producer review recommended splitting it, so the epic defines **two exit points**: Exit A, bootstrap (skeleton, harness, CI, VCS), which unblocks every other epic; Exit B, the TDC pipeline and registry. The bottom-up estimate for the bundle is about 12 days (about 7 planning days after the 0.6 calibration), the largest single Foundation package.

## Governing ADRs

| ADR | Decision Summary | Engine Risk |
|-----|------------------|-------------|
| ADR-0004: Tuning data pipeline and validator | One cooked data-asset class of per-owner sub-structs, C++ schema rows, server-only resolve/validate/freeze with TuningHash and ClientHash, no config/CVar back doors. | LOW |
| ADR-0007: Code module layout, composition root and include boundaries | Four runtime modules (StealthFoundation, StealthGameplay, StealthPresentation, StealthGame) plus Editor and StealthTestHarness; compile guard, facade split, consumer-owned interfaces. | MEDIUM |
| ADR-0008: Test and automation strategy for leak and parity tests | Headless pure-C++ cores, multi-client and multi-process leak harness, raw-bunch capture, net-emulation profiles, per-commit canaries. | MEDIUM |
| ADR-0009: Version control and binary asset workflow | Git with LFS for binary assets, single-writer/lock discipline for .uasset files. | LOW |

All governing ADRs are Accepted (user, 2026-10-25). Several are conditionally accepted; the outstanding gates are in Build Order and Gates below.

## GDD Requirements

11 of 11 requirements are covered by an Accepted ADR. Untraced requirements: None.

| TR-ID | Requirement | ADR Coverage |
|-------|-------------|--------------|
| TR-tdc-001 | Every gameplay number is a key in one cooked data-asset class of per-owner sub-structs; presets listed by a catalog | ADR-0004 ✅ |
| TR-tdc-002 | C++ schema row per key (type, default, hard range, audience, latch); editor metadata is a mirror; schema defaults pinned to GDD defaults by test | ADR-0004, ADR-0008 ✅ |
| TR-tdc-003 | Server-only resolve/validate/freeze at `Preparing` with the CR-6 ladder and fallback rungs (`FallbackLevel`, `ValidationReport`) | ADR-0004 ✅ |
| TR-tdc-004 | Cross-field invariants reject or warn, never clamp; safe ranges linted against all invariants at other keys' defaults | ADR-0004 ✅ |
| TR-tdc-005 | Immutable `ValidatedTuning` per round; `TuningHash` (server-only) and `ClientHash` (public keys) | ADR-0004 ✅ |
| TR-tdc-006 | ServerOnly/All audience split; client snapshot replicated; client face typed to the public struct (compile-time guarantee) | ADR-0004, ADR-0007 ✅ |
| TR-tdc-007 | World-scoped accessor correct in multi-client PIE; no per-pawn/team/viewer parameter | ADR-0004 ✅ |
| TR-tdc-008 | Latch classes Round/Action/Use honoured by every consumer | ADR-0004 ✅ |
| TR-tdc-009 | No back doors: no `Config` properties, no ini/CVar overrides in Shipping; Shipping always resolves `Default` | ADR-0004 ✅ |
| TR-tdc-010 | PIE-only hot-reload re-validation, debounced, routed to the PIE server world | ADR-0004 ✅ |
| TR-tdc-011 | Client tuning check rides Session's single acknowledgement (no TDC RPC/timer) | ADR-0004 ✅ |

## Prototype Disposition

The code in `Source/StealthGame` is marked PROTOTYPE (`production/first-playable-report.md`). Production code follows ADR-0007 modules, and nothing is promoted until the ADR that governs it is built against.

**Replaces** `Source/StealthGame/Public/StealthSettings.h` (`UStealthSettings`: `Config` properties in `DefaultGame.ini` section `[/Script/StealthGame.StealthSettings]` holding all tuning values and the character pool). This violates TDC CR-13 (no `Config` back doors) and is **replaced, not migrated** (architecture.md, Prototype promotion rule). The prototype values are only a source for the schema defaults, which are pinned to the GDD defaults by test.
**Keeps** the editor module `Source/StealthGameEditor` (the MCP toolsets `StealthPlayToolset` and `StealthAssetToolset`) as the seed of the new `StealthGameEditor` module. The prototype's single runtime module `StealthGame` is split into the ADR-0007 layout; nothing is promoted until the ADR that governs it is built against. The 12 prototype tests (`StealthGame.Rules.*`, `StealthGame.EditorTools.*`, in `Source/StealthGame/Private/Tests/`) stay green during the transition and new tests land in `Source/<Module>/Private/Tests/`.

## Build Order and Gates

- **Depends on (epics)**: none (first epic; every other epic consumes its schema)
- **Gates and risks**: No outstanding spike: ADR-0004, ADR-0007 and ADR-0008 are unconditional; ADR-0009's repository init and first commit still need an explicit user instruction.
- **Estimate**: about 12 bottom-up days (human-paced yardstick, `production/estimates/2026-10-25_mvp-rebaseline.md`, Method B work packages); planning figure about 7.2 days after the uniform 0.6 calibration. Low confidence: no sprint history exists, so run the 2-week calibration spike and re-baseline.
- **Acceptance criteria in the GDD**: about 43 (counted as GIVEN-form criteria in the Acceptance Criteria section).
- **MVP build order**: risk-first per `design/gdd/systems-index.md`: graybox map and mannequin-variant pool first, then camera toggle and traversal replication, then the Meshy pool swap, then the generator.

## Definition of Done

- Every story implemented, reviewed and closed with `/story-done`
- Every acceptance criterion of design/gdd/tuning-data-config.md verified
- Logic and Integration stories have passing tests in the test root (`Source/<Module>/Private/Tests/`)
- Visual/Feel and UI stories have retained screenshots in `production/qa/evidence/`
  (a lead sign-off too for Visual/Feel)
- Stories embed Control Manifest version 2026-10-25e and every governing ADR's conditional gate has reported before dependent work is marked Done

## Stories

Stories 001-028 are Exit A (bootstrap: module skeleton, test harness, CI, VCS), 029-037 close TD-REG-1 and the AC-3b doc lint, 038-100 are Exit B (the TDC pipeline). Numbers 097-100 were added after the QL-STORY-READY review; build order follows each story's `Depends on` list, not the number (099 and 100 before 077/079/093, 097 after 050, 098 after 097). Stories 019, 020, 064, 071 and 085 are verification stories for [U] engine claims and close PASS or with a recorded amendment request (never by skipping).

| # | Story | Type | Status | ADR |
|---|-------|------|--------|-----|
| 001 | Foundation module skeleton and server-only guard header | Integration | Ready | ADR-0007 |
| 002 | Gameplay and Presentation module skeletons with the presentation guard define | Integration | Ready | ADR-0007 |
| 003 | Split the prototype module into the ADR-0007 composition root | Integration | Ready | ADR-0007 |
| 004 | StealthTestHarness module excluded from Shipping and Test | Integration | Ready | ADR-0007 |
| 005 | Compile-probe runner with the ADR-0007 guard probe and its positive control | Integration | Ready | ADR-0008 |
| 006 | Shipping-symbol check: no harness module and no test-seam symbols in Shipping | Integration | Ready | ADR-0008 |
| 007 | IStealthClock and an injectable fake clock | Logic | Ready | ADR-0008 |
| 008 | IStealthFaultSink and the WITH_STEALTH_TEST_SEAMS define | Logic | Ready | ADR-0008 |
| 009 | Log-capture hook asserting warnings by key | Logic | Ready | ADR-0008 |
| 010 | Negative-control helper: the suite passes only if the inner check fails | Logic | Ready | ADR-0008 |
| 011 | T0 lint framework: observation emitter and gate script | Logic | Ready | ADR-0008 |
| 012 | Include lint rules (a)-(c) | Logic | Ready | ADR-0007 |
| 013 | Include lint rules (d)-(f) | Logic | Ready | ADR-0007 |
| 014 | Include lint rules (g)-(h) | Logic | Ready | ADR-0007 |
| 015 | Include lint rules (i)-(j) | Logic | Ready | ADR-0007 |
| 016 | Seeded violation per lint rule watched to fail | Logic | Ready | ADR-0008 |
| 017 | Net emulation profiles StealthLeak1, StealthLeak2, StealthClean | Config/Data | Ready | ADR-0008 |
| 018 | Harness replication-system assertion and result JSON | Logic | Ready | ADR-0008 |
| 019 | CQTest listen server with three clients runs headless under -nullrhi | Integration | Ready | ADR-0008 |
| 020 | Gauntlet host plus client smoke scenario with legacy-driver assertion | Integration | Ready | ADR-0008 |
| 021 | Per-commit gate script | Integration | Ready | ADR-0008 |
| 022 | CI workflow runs the T0 lint and the same gate | Config/Data | Ready | ADR-0008 |
| 023 | Nightly workflow: compile probes, T3 suites, Shipping-symbol check | Integration | Ready | ADR-0008 |
| 024 | VCS conformance check for ignore and attribute rules | Config/Data | Ready | ADR-0009 |
| 025 | Tracked pre-commit hook framework running the T0 lint | Logic | Ready | ADR-0009 |
| 026 | Pre-commit check rejects changes to never-resave assets | Logic | Ready | ADR-0009 |
| 027 | Pre-commit check compares changed assets with git lfs locks | Logic | Ready | ADR-0009 |
| 028 | commit-msg hook enforces Conventional Commits with a story or ADR reference | Logic | Ready | ADR-0009 |
| 029 | Parser for the Tuning Knobs tables of every GDD | Logic | Ready | ADR-0004 |
| 030 | Mirror the missing owner keys into entities.yaml (TD-REG-1 first task) | Config/Data | Ready | ADR-0004 |
| 031 | Doc lint: every Tuning Knobs table has the CR-15 columns | Logic | Ready | ADR-0004 |
| 032 | Doc lint: every key appears as owned in exactly one GDD | Logic | Ready | ADR-0004 |
| 033 | Doc lint: every owned key matches an entities.yaml constant | Logic | Ready | ADR-0004 |
| 034 | Doc lint: every owned key matches a schema row, shrinking pending-owner baseline | Logic | Ready | ADR-0004 |
| 035 | Doc lint negative control: a fixture GDD with a duplicated key fails | Logic | Ready | ADR-0008 |
| 036 | Doc lint: invariant-read keys must say "bounded by <Id>" | Logic | Ready | ADR-0004 |
| 037 | Close TD-REG-1 in the tech-debt register | Config/Data | Ready | ADR-0004 |
| 038 | Schema row struct, type tags, latch enum and schema constants | Logic | Ready | ADR-0004 |
| 039 | Preset data-asset classes with no Config back door | Logic | Ready | ADR-0004 |
| 040 | The eight canonical keys and the client/server tuning faces | Logic | Ready | ADR-0004 |
| 041 | StealthTuningSchemaGen commandlet writes the generated schema table | Logic | Ready | ADR-0004 |
| 042 | Bootstrap regeneration flow for the schema table | Integration | Ready | ADR-0004 |
| 043 | Schema fingerprint and the SchemaVersionBumped test | Logic | Ready | ADR-0004 |
| 044 | AC-3a SchemaComplete | Logic | Ready | ADR-0004 |
| 045 | AC-5 HashStableAndSensitive | Logic | Ready | ADR-0004 |
| 046 | AC-4 ValidateClampAndNan | Logic | Ready | ADR-0004 |
| 047 | AC-18 DerivedIndicators | Logic | Ready | ADR-0004 |
| 048 | AC-17 InvariantBoundaries | Logic | Ready | ADR-0004 |
| 049 | AC-26 SafeRangeWarnOnly | Logic | Ready | ADR-0004 |
| 050 | AC-6a FallbackToDefault: Resolve() and the preset-source seam | Logic | Ready | ADR-0004 |
| 051 | AC-6b FallbackToSchema | Logic | Ready | ADR-0004 |
| 052 | Round and Session stubs for TDC tests | Logic | Ready | ADR-0008 |
| 053 | AC-6c FallbackFailed | Logic | Ready | ADR-0004 |
| 054 | AC-32 FrozenImmutable | Logic | Ready | ADR-0004 |
| 055 | AC-30 mechanism: owner-invariant rows | Logic | Ready | ADR-0004 |
| 056 | Default, Playtest and Solo preset assets and the catalog | Config/Data | Ready | ADR-0004 |
| 057 | Asset Manager primary types with AlwaysCook | Config/Data | Ready | ADR-0004 |
| 058 | AC-1 DefaultPreset: the game-instance preset store | Integration | Ready | ADR-0004 |
| 059 | AC-2a PresetNotSelectable | Logic | Ready | ADR-0004 |
| 060 | AC-2b PresetCaseInsensitive | Logic | Ready | ADR-0004 |
| 061 | AC-2c PresetMalformed | Logic | Ready | ADR-0004 |
| 062 | AC-2d ClientOptionIgnored | Integration | Ready | ADR-0004 |
| 063 | AC-2e PresetSurvivesTravel (multi-process) | Integration | Ready | ADR-0004 |
| 064 | AC-27 PresetsCooked (cooked Dev build) | Integration | Ready | ADR-0004 |
| 065 | AC-29 CatalogClean (CI script) | Logic | Ready | ADR-0004 |
| 066 | UStealthTuningService and AC-21 PlayerNpcParity | Logic | Ready | ADR-0004 |
| 067 | World-scoped accessor reads its own world in multi-client PIE | Integration | Ready | ADR-0004 |
| 068 | AC-28 ServerKeyNotOnClient | Logic | Ready | ADR-0004 |
| 069 | AC-8 static half NoClientAssetReference | Logic | Ready | ADR-0004 |
| 070 | AC-19 SnapshotSizeBudget | Logic | Ready | ADR-0004 |
| 071 | Spike T-1: atomic snapshot delivery under loss and for a rejoiner | Integration | Ready | ADR-0004 |
| 072 | Snapshot replication hold-back and arrival-order test hook | Logic | Ready | ADR-0008 |
| 073 | AC-7 SnapshotPublicOnly | Integration | Ready | ADR-0004 |
| 074 | AC-10 ClientHashHandoff | Integration | Ready | ADR-0004 |
| 075 | AC-11 StaleEpochIgnored | Integration | Ready | ADR-0004 |
| 076 | AC-9 NoPawnBeforeReady | Integration | Ready | ADR-0004 |
| 077 | AC-12a HostPath | Integration | Ready | ADR-0004 |
| 078 | AC-12b RemoteLoopbackHash (multi-process) | Integration | Ready | ADR-0004 |
| 079 | AC-12c SelfCheckFault | Logic | Ready | ADR-0004 |
| 080 | AC-13 RejoinSnapshot (multi-process) | Integration | Ready | ADR-0004 |
| 081 | AC-23 TravelEpoch (multi-process) | Integration | Ready | ADR-0004 |
| 082 | AC-8 LocalTamperIgnored (multi-process) | Integration | Ready | ADR-0004 |
| 083 | AC-16 FullHashNotReplicated (provisional on Round) | Integration | Ready | ADR-0004 |
| 084 | AC-20 RoundLogHeader (provisional on Round) | Logic | Ready | ADR-0004 |
| 085 | AC-14a HotReloadPie | Integration | Ready | ADR-0004 |
| 086 | AC-14b HotReloadRejected | Integration | Ready | ADR-0004 |
| 087 | AC-14c HotReloadAbsentPackaged | Logic | Ready | ADR-0004 |
| 088 | Back-door code behind guards with sentinel literals (AC-22 positive control) | Integration | Ready | ADR-0004 |
| 089 | AC-22 NoShippingBackDoor (CI script) | Integration | Ready | ADR-0004 |
| 090 | AC-31 PresetChangeNextRound | Integration | Ready | ADR-0004 |
| 091 | AC-15 NoVariantKeys | Logic | Ready | ADR-0004 |
| 092 | AC-25 NoHardcodedGameplay (CI literal scan) | Logic | Ready | ADR-0004 |
| 093 | AC-24 LocalSettingsNotTuning | Integration | Ready | ADR-0004 |
| 094 | SafeRangeLint with a shrink-only baseline | Logic | Ready | ADR-0004 |
| 095 | Retire the eight canonical Config fields of UStealthSettings | Integration | Ready | ADR-0007 |
| 096 | AC-30 closure: every owner invariant over the full schema | Logic | Blocked | ADR-0004 |
| 097 | Invariants reject the whole preset (AC-17 closing clauses) | Logic | Ready | ADR-0004 |
| 098 | Derived indicators, preset scope (AC-18 scope clauses) | Logic | Ready | ADR-0004 |
| 099 | Self-check forcing hook and disconnect recorder (test seams) | Logic | Ready | ADR-0008 |
| 100 | Reliable-RPC counter and replicated-property CRC helper (test seams) | Logic | Ready | ADR-0008 |

Story 096 is Blocked on other epics (all 18 owners must register their schema sub-structs and invariant rows), not on an ADR or spike; it unblocks when story 034's pending-owner baseline is empty. The file names are `story-NNN-<slug>.md` in this folder.
