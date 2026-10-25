# Story 054: Blind observers cannot tell a walking NPC from a walking player

> **Epic**: Shared Character & Locomotion
> **Status**: Ready
> **Layer**: Core
> **Type**: Visual/Feel
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: [set by /dev-story when implementation begins]

## Context

**GDD**: `design/gdd/shared-character-locomotion.md` (Pillar 1 evidence; AC-SCL-33)
**Requirement**: `TR-scl-001`, `TR-scl-006`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0001: Crowd identity rules (one pawn class, one command path, one look path; identity blindness)
**ADR Decision Summary**: Pillar 1 ("an enemy cannot tell a player from an NPC") is the product-level result of every parity rule in this epic; a blind-observer test is the human check that no tell survives the automated checks.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Capture clips on the standalone/PIE build with fixed camera, tier band and lighting; clips are screen captures, so they depend on rendering. Not CI-gating.

**Control Manifest Rules (this layer)**:
- Required: Visual/Feel stories keep a retained screenshot plus lead sign-off in `production/qa/evidence/`.
- Forbidden: none specific.
- Guardrail: none.

---

## Acceptance Criteria

*From GDD `design/gdd/shared-character-locomotion.md`, scoped to this story:*

- [ ] **AC-SCL-33** [PLAYTEST] `BlindObserverWalk`: GIVEN 20 clips of 30 s (10 NPC, 10 player, all walking without tells, same map, camera, tier band and lighting), each shown in a fixed-seed shuffled order to 5 testers who did not record them and have played ≥ 3 rounds, WHEN each clip is classified NPC or player, THEN correct judgements are ≤ 60 of 100 (exact binomial, two-sided p ≥ 0.05 against 50 %) and no tester exceeds 16 of 20; evidence in `production/qa/evidence/`. Not CI-gating.

---

## Implementation Notes

*Derived from the GDD:*

- Needs a playable build with crowd NPCs, the Meshy character pool and generator output (the epic's MVP build order places it after the pool swap). Record 10 NPC and 10 player clips with matching conditions (a scripted player path that holds the shared pace set and no tells), shuffle with a fixed seed, run with 5 qualified testers, and tally.
- Retain the clip list, shuffle seed, response sheets, tally, the lead sign-off and one still frame from an NPC clip and one from a player clip in `production/qa/evidence/` (recruiting five qualified testers is the main logistics risk).
- **Prototype replaced**: none; the prototype's first-playable screenshots (`production/qa/evidence/2026-10-24_*.png`) are not evidence for this criterion.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- The automated parity checks (stories 004-043). Telling-with-tells experiments (Crowd's tell calibration).

---

## Files

- `production/qa/evidence/blind-observer-walk-evidence.md` (new)
- `production/qa/evidence/` clips, sheets and stills (new)

---

## QA Test Cases

*QL-STORY-READY: ADEQUATE (playtest, not CI-gating).*

Manual check: blind observer walk (AC-SCL-33)
  Setup: 20 clips of 30 s (10 NPC, 10 player), all walking without tells, same map, camera, tier band and lighting; shuffle with a fixed seed; recruit 5 testers who did not record the clips and have played ≥ 3 rounds
  Verify: each tester classifies each clip as NPC or player; tally correct judgements overall and per tester
  Pass condition: ≤ 60 of 100 correct (exact binomial, two-sided p ≥ 0.05 against 50 %) and no tester exceeds 16 of 20; the clip list, shuffle seed, response sheets, tally, lead sign-off and one still frame from an NPC clip and one from a player clip are retained in `production/qa/evidence/`
  Edge cases: recruiting five qualified testers is a logistics risk for a small team, not a testability gap

---

## Test Evidence

**Story Type**: Visual/Feel
**Required evidence**:
- Visual/Feel: retained screenshots plus the sign-off in `production/qa/evidence/blind-observer-walk-evidence.md` (lead sign-off); not CI-gating

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/shared-character-locomotion/story-010-speed-tiers.md`, `story-021-look-envelope-700-deg-spin.md`, `story-024-idle-gaze-parity.md`, `story-043-footstep-parity.md`; `production/epics/crowd-npc-behaviour/EPIC.md`, `production/epics/character-pool-appearance/EPIC.md`, `production/epics/procedural-city-generator/EPIC.md`
- Unlocks: None
