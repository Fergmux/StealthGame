# Epic: Character Pool & Appearance

> **Layer**: Core
> **GDD**: design/gdd/character-pool-appearance.md
> **Architecture Module**: Character Pool (CPA) in `StealthGameplay/Core` (`UCharacterPoolData`, deck allocator, appearance application)
> **Status**: Ready
> **Stories**: Not yet created — run `/create-stories character-pool-appearance`
> **Engine Risk**: HIGH (highest among governing ADRs; engine 5.8, installed 5.8.4)
> **Control Manifest**: 2026-10-25e
> **Build Order**: 6 of 19 (layer order; the risk-first sequence is in the Build Order and Gates section)

## Overview

Character Pool & Appearance defines every look a character can have and the one rule that hands looks out. The pool is one cooked data asset of 6 to 8 base models with up to 3 variants each, at most 24 distinct appearances (MVP floor 6 models x 2 variants = 12, built from mannequin variants first), each addressed by a one-byte stable `AppearanceId`. The allocator `AllocateAppearances(Players, NpcCount)` is a pure server-side function Round calls once per round in `Preparing`: it builds a single balanced appearance deck (at most 12 appearances in play, at least 4 copies of each, counts differing by at most 1, model-stratified subset, unbiased Fisher-Yates), shuffles it with a server-private stream from `GetServerSubSeed(AppearanceDeal)`, and deals it to the whole crowd without ever being told who is on which team or which pawn is a player.

After the deal the deck is conserved: `DrawReplacement()` refills a killed NPC with the vacated card in the same tick, and `TakeNpcCard()` serves informant takeover so the look a killer saw never follows the victim. The epic also delivers the appearance-before-first-replication rule (the pawn class has no mesh until its id resolves), full residency and PSO precache before leaving `Preparing`, one look path for every viewer and pawn kind (no keyed material parameters), the `PoolHash` for Session's acknowledgement, and the automated G-DECK statistical indistinguishability gate. It exists to make Pillar 1 true at the level of statistics.

## Governing ADRs

| ADR | Decision Summary | Engine Risk |
|-----|------------------|-------------|
| ADR-0001: Crowd identity rules | Four audience carriers (C-Public allowlist on PlayerState, owner-only C-TeamView on PlayerController, C-Server, GameState public structs); every UE default that reveals player-ness is overridden; one pawn class, one look path, identity blindness. | HIGH |
| ADR-0006: Seeds and deterministic/secret random streams | Public sub-seeds for layout vs SipHash-2-4 keyed server-secret streams; integer PCG32 streams so layout is bit-identical across machines. | LOW |
| ADR-0008: Test and automation strategy for leak and parity tests | Headless pure-C++ cores, multi-client and multi-process leak harness, raw-bunch capture, net-emulation profiles, per-commit canaries. | MEDIUM |
| ADR-0012: Character pool data and appearance allocation | Pool as one data asset (AppearanceId 0-23), single balanced deck per round, server-private streams, appearance resolved before first replication. | HIGH |
| ADR-0022: Rendering configuration and performance budgets | Lumen/Nanite/VSM configuration and CPU/GPU frame budgets for the RTX 3060-class 1080p60 target (GPU 14.6 ms). | HIGH |

All governing ADRs are Accepted (user, 2026-10-25). Several are conditionally accepted; the outstanding gates are in Build Order and Gates below.

## GDD Requirements

7 of 7 requirements are covered by an Accepted ADR. Untraced requirements: None.

| TR-ID | Requirement | ADR Coverage |
|-------|-------------|--------------|
| TR-cpa-001 | Pool as one cooked data asset of appearance rows; 6-8 models, ≤ 24 appearances, stable `AppearanceId` | ADR-0012 ✅ |
| TR-cpa-002 | `AllocateAppearances` once per round; balanced deck, model-stratified subset, unbiased Fisher-Yates | ADR-0012 ✅ |
| TR-cpa-003 | Server-private streams from `GetServerSubSeed(AppearanceDeal)`; DRBG-class primitive for Replace/Take | ADR-0006, ADR-0012 ✅ |
| TR-cpa-004 | Appearance before first replication, immutable per pawn; deck conserved; `DrawReplacement` same tick; `TakeNpcCard` on adoption | ADR-0012 ✅ |
| TR-cpa-005 | All pool assets resident and precached before leaving `Preparing`; no first-draw load | ADR-0012, ADR-0022 ✅ |
| TR-cpa-006 | One look path for every viewer and pawn kind; no keyed material parameters | ADR-0001, ADR-0012 ✅ |
| TR-cpa-007 | Statistical indistinguishability gate G-DECK automated | ADR-0008 ✅ |

## Prototype Disposition

The code in `Source/StealthGame` is marked PROTOTYPE (`production/first-playable-report.md`). Production code follows ADR-0007 modules, and nothing is promoted until the ADR that governs it is built against.

**Replaces** the prototype pool and dealer: the character pool entries live in `StealthSettings.h`/`DefaultGame.ini`, the deck is dealt inside `StealthGameMode.cpp`, and `AStealthCharacter::SetAppearanceIndex`, `ApplyAppearance` and `OnRep_AppearanceIndex` apply it (four Meshy citizens `SK_Citizen02-05` under `/Game/StealthGame/Characters/Pool/`, `BP_CrowdCharacter`, `M_CrowdTint` fallback). The prototype's balanced-deck idea is correct and is the origin of the GDD rule, but the allocator is rewritten as a pure function with the ADR-0012 streams and conservation rules.
**Kept (asset pipeline, not code):** `tools/asset-pipeline/blender_bind_to_mannequin.py`, `assets/characters/meshy/` sources and the Blender-to-UEFN-skeleton procedure carry over to the Meshy swap. No `.uasset` is modified by this epic definition; the asset-resave hazards in `production/never-resave-assets.md` apply to every story.

## Build Order and Gates

- **Depends on (epics)**: tuning-data-config, session-travel (seeds), participant-state-roster (soft); called by round-match-framework at `Preparing` and re-pawn; consumed by shared-character-locomotion, crowd-npc-behaviour and npc-kill-penalty-informants
- **Gates and risks**: Conditional gate from ADR-0012: CPA-V1..CPA-V5 (PSO precache component, 5.8 streaming signatures, skin-cache per-mesh opt-in). ADR-0022 R1-R3 for the 3060-class memory and residency budget. Build order is risk-first, so the epic has two exit points: **Exit A**, the mannequin-variant pool and allocator (about 6 d, early); **Exit B**, the Meshy swap (about 5 d, late, credit-gated). Meshy citizen arms are 17 % shorter than the mannequin's (first-playable known issue 3); the fallback is a shared body plus Meshy heads (art bible 1.3).
- **Estimate**: about 11 bottom-up days (human-paced yardstick, `production/estimates/2026-10-25_mvp-rebaseline.md`, Method B work packages); planning figure about 6.6 days after the uniform 0.6 calibration. Low confidence: no sprint history exists, so run the 2-week calibration spike and re-baseline.
- **Acceptance criteria in the GDD**: about 47 (counted as GIVEN-form criteria in the Acceptance Criteria section).
- **MVP build order**: risk-first per `design/gdd/systems-index.md`: graybox map and mannequin-variant pool first, then camera toggle and traversal replication, then the Meshy pool swap, then the generator.

## Definition of Done

- Every story implemented, reviewed and closed with `/story-done`
- Every acceptance criterion of design/gdd/character-pool-appearance.md verified
- Logic and Integration stories have passing tests in the test root (`Source/<Module>/Private/Tests/`)
- Visual/Feel and UI stories have retained screenshots in `production/qa/evidence/`
  (a lead sign-off too for Visual/Feel)
- Stories embed Control Manifest version 2026-10-25e and every governing ADR's conditional gate has reported before dependent work is marked Done
