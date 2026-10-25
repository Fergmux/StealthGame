# Epic: Tuning Data & Config

> **Layer**: Foundation
> **GDD**: design/gdd/tuning-data-config.md
> **Architecture Module**: Tuning (TDC) in `StealthFoundation`, plus the cross-cutting bootstrap of the module skeleton, test harness and CI
> **Status**: Ready
> **Stories**: Not yet created — run `/create-stories tuning-data-config`
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
