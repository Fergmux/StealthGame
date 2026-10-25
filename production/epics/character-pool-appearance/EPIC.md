# Epic: Character Pool & Appearance

> **Layer**: Core
> **GDD**: design/gdd/character-pool-appearance.md
> **Architecture Module**: Character Pool (CPA) in `StealthGameplay/Core` (`UCharacterPoolData`, deck allocator, appearance application)
> **Status**: Ready
> **Stories**: 56 stories written 2026-10-25 (46 Ready, 10 Blocked); see the Stories table below
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

## Stories

Order: the Art Director's first visual stories (OI-1 and OI-10 test character on the UEFN mannequin reference, 165.6 cm) come first and gate any Meshy mass production; prototype Meshy citizens `SK_Citizen02-05` are throwaway (arms about 17 % short, extra root bone) and are retired in story 052. Exit A (Tier A mannequin-variant pool and allocator) does not wait on a Meshy credit; Exit B (stories 053, 054) is credit-gated and Blocked.

| # | Story | Type | Status | ADR |
|---|-------|------|--------|-----|
| 001 | OI-1 closed: the UEFN mannequin measurements become the art-bible reference | Config/Data | Ready | ADR-0012 |
| 002 | Body_Base built in Blender to the UEFN reference (OI-10) | Visual/Feel | Ready | ADR-0012 |
| 003 | One Meshy test character generated and normalised to the UEFN skeleton | Visual/Feel | Blocked (OI-1 and OI-10 open; first Meshy credit) | ADR-0012 |
| 004 | Test character passes T1 to T4 and is signed off (pipeline proof) | Visual/Feel | Ready | ADR-0012 |
| 005 | Pool row schema, UCharacterPoolData and O(1) id lookup | Logic | Ready | ADR-0012 |
| 006 | Swatch assignment is a rule (F-7), checked and rewritten by the validator | Logic | Ready | ADR-0012 |
| 007 | Pool shape and material rules validator | Logic | Ready | ADR-0012 |
| 008 | Pool mesh conformance validator (skeleton, arm geometry, LODs, uniform skin cache) | Logic | Ready | ADR-0012 |
| 009 | Deck tuning keys registered with TDC and invariant I-3 | Logic | Ready | ADR-0012 |
| 010 | Deck sizing (D, K, q, r) and the floor policy | Logic | Ready | ADR-0012 |
| 011 | Valid-subset table and the model-stratified draw (F-4) | Logic | Ready | ADR-0012 |
| 012 | Deal, shuffle, canonical order and the three domain-separated streams | Logic | Ready | ADR-0012 |
| 013 | UAppearanceAllocator facade: opaque input, one-shot, call-order guard | Logic | Ready | ADR-0012 |
| 014 | G-DECK statistical indistinguishability suite on the real allocator | Logic | Ready | ADR-0008 |
| 015 | G-DECK rejects the biased allocators B1 to B4 and is calibrated nightly | Logic | Ready | ADR-0008 |
| 016 | Art-bible T6 allocator invariants simulated over 10,000 matches (OI-11) | Logic | Ready | ADR-0012 |
| 017 | No cross-round memory; Reset discards everything | Logic | Ready | ADR-0012 |
| 018 | Card ledger core: CommitCard, VacateCard and the conservation invariants | Logic | Ready | ADR-0012 |
| 019 | DrawReplacement is uniform over vacant cards and ordering-safe | Logic | Ready | ADR-0012 |
| 020 | TakeNpcCard: informant adoption of an unobserved crowd pawn | Logic | Ready | ADR-0012 |
| 021 | ReleaseCard (strike removal ordering) and RecoverOwnCard (own-card respawn) | Logic | Ready | ADR-0012 |
| 022 | Ledger conservation under the full random event mix | Logic | Ready | ADR-0012 |
| 023 | Disconnect, rejoin, no late join and host loss keep the ledger conserved | Integration | Ready | ADR-0012 |
| 024 | Tier C fixture proves the allocator and validators are tier-independent | Logic | Ready | ADR-0012 |
| 025 | UCharacterPoolSubsystem holds the pool resident for the session | Integration | Ready | ADR-0012 |
| 026 | Spike CPA-V1: null-mesh pawn takes its look from the initial OnRep | Integration | Ready | ADR-0012 |
| 027 | AppearanceId on the pawn: write once, replicate once, apply once | Integration | Blocked (CPA-V1) | ADR-0012 |
| 028 | PoolHash and ContentDigest | Logic | Ready | ADR-0012 |
| 029 | Spike CPA-V5: PoolHash identical across cooked client, cooked host and PIE | Integration | Ready | ADR-0012 |
| 030 | Spike CPA-V4: initial-only re-sent on relevancy re-entry, retained across takeover | Integration | Ready | ADR-0012 |
| 031 | Spike CPA-V2/V3: PSO precache and residency after warm-up in a cooked client | Integration | Ready | ADR-0012 |
| 032 | UCharacterPoolWarmup: warm every pool row before the acknowledgement | Integration | Blocked (CPA-V2/V3) | ADR-0012 |
| 033 | PoolHash rides in Session's single round acknowledgement, after warm-up | Integration | Blocked (CPA-V5) | ADR-0012 |
| 034 | Appearance resolved before first replication; no default-look frame | Integration | Blocked (CPA-V1) | ADR-0012 |
| 035 | No pool-asset load on reveal; a still-delayed pawn flags the round | Integration | Blocked (CPA-V2/V3) | ADR-0012 |
| 036 | Relevancy re-entry restores the id; takeover keeps it; the host sees what clients see | Integration | Blocked (CPA-V4) | ADR-0012 |
| 037 | Player and NPC pawns are indistinguishable to a remote client | Integration | Ready | ADR-0001 |
| 038 | View-pitch and cadence parity; spawn and export order uncorrelated with player-ness | Integration | Ready | ADR-0001 |
| 039 | The owner's hidden first-person body has no effect on what others see | Integration | Ready | ADR-0012 |
| 040 | One presentation for every pawn class; no appearance-keyed audio or variation | Integration | Ready | ADR-0012 |
| 041 | Visibility rows for AppearanceId and a log with no seed | Integration | Ready | ADR-0001 |
| 042 | Shipping symbol audit and the character-pool lint set | Integration | Ready | ADR-0008 |
| 043 | Pool evidence goes stale when pool membership changes | Logic | Ready | ADR-0012 |
| 044 | M_Character_Master and the static material instances | Visual/Feel | Ready | ADR-0012 |
| 045 | Tier A: six mannequin-variant models on the UEFN skeleton | Visual/Feel | Ready | ADR-0012 |
| 046 | DA_CharacterPool authored with 12 rows and validated on real assets | Config/Data | Ready | ADR-0012 |
| 047 | Tier A pool passes T1 to T4 (whole-pool gate) | Visual/Feel | Ready | ADR-0012 |
| 048 | G-TWIN: four copies of one look read as an ordinary crowd | Visual/Feel | Ready | ADR-0012 |
| 049 | G-VAR: the active models still differ at 8 m (paired-trial playtest) | Visual/Feel | Ready | ADR-0012 |
| 050 | T5 crowd performance on the 3060-class reference PC | Integration | Blocked (ADR-0022 R1-R3) | ADR-0022 |
| 051 | Network cost of AppearanceId recorded in a 10-client run | Integration | Ready | ADR-0012 |
| 052 | Retire the prototype pool, dealer and per-pawn tint | Integration | Ready | ADR-0012 |
| 053 | First six Meshy bodies pass or fail the Body_Base conform test (Tier B versus Tier C) | Visual/Feel | Blocked (test character, mass-production gate) | ADR-0012 |
| 054 | Pool swap to Tier B or Tier C content (Exit B) | Visual/Feel | Blocked (story 053 verdict, mass-production gate) | ADR-0012 |
| 055 | Cross-GDD tell-matrix review and the cosmetic/colour-aid checklist | Config/Data | Ready | ADR-0012 |
| 056 | Pillar-1 playtest: participant counts per look fit the deck expectation | Integration | Ready | ADR-0008 |

## Definition of Done

- Every story implemented, reviewed and closed with `/story-done`
- Every acceptance criterion of design/gdd/character-pool-appearance.md verified
- Logic and Integration stories have passing tests in the test root (`Source/<Module>/Private/Tests/`)
- Visual/Feel and UI stories have retained screenshots in `production/qa/evidence/`
  (a lead sign-off too for Visual/Feel)
- Stories embed Control Manifest version 2026-10-25e and every governing ADR's conditional gate has reported before dependent work is marked Done
