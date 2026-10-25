# Story 044: Static-figure, sun-pool beacon and paired-capture tests

> **Epic**: Level Socket Contract
> **Status**: Ready
> **Layer**: Core
> **Type**: Visual/Feel
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/level-socket-contract.md` (AC-47, AC-48, AC-49; Visual human tests section)
**Requirement**: `TR-lsc-???` (no registry entry matches these human tests; traced to GDD AC-47..49, VA-1, VA-3, VA-5 and art 3.3)

**ADR Governing Implementation**: N/A — human perception tests of art-bible and LSC rules on the graybox; V-POOL-40 (ADR-0014 5.3) is a Warn in MVP and escalates to Reject only if AC-48 says so
**ADR Decision Summary**: N/A
**ADR Version**: N/A

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: Captures from the graybox renderer with debug draw off; pass thresholds are set so the one-sided 95% bound on the true rate meets the target; testers are counted as clusters and option positions randomised.

**Why this story**: Kit pieces must not read as people, court sun pools must not be beacons, and sockets/zones/courts/respawn regions must be indistinguishable from their ordinary equivalents (Pillar 1). Read first: GDD "Visual human tests" preamble, AC-47, AC-48, AC-49; V-POOL-40, V-ZONE; art 3.3, 6.8.

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: A court is marked by light and architecture only; no role-specific mesh, decal, light, prop, sound or area type (GDD R10)
- Forbidden: Debug draw in evidence captures (GDD Acceptance Criteria preamble)
- Guardrail: n/a

---

## Acceptance Criteria

*From GDD AC-47, AC-48 and AC-49 (revised after QL-STORY-READY: AC-47 wording corrected to the intent), scoped to this story:*

- [ ] GIVEN greyscale 30 m captures of every kit piece in context, with each piece getting at least 10 trials across at least 5 testers, THEN each piece passes with at most 2 "person" calls (3 or more fails) and overall "person" calls are at most 5% over at least 200 trials
- [ ] GIVEN a 40-60 m 4-choice among lit patches (one court pool, three non-court) with at least 200 trials, THEN the court is picked in at most 58 of 200 (ADVISORY while `bSunPoolBeaconIsReject` is false, BLOCKING once true)
- [ ] GIVEN paired captures (socket vs non-socket equivalent) per category (alley, start zone, court, respawn region), at least 30 pairs and at least 5 reviewers (at least 150 judgments) each, THEN at most 80 of 150 judgments are correct (rate bound at most 60%) in every category

---

## Implementation Notes

- Static-figure test (art 3.3): every kit piece in context at 30 m, greyscale; a piece fails at 3 or more "person" calls among its trials. The GDD wording ("fails at 3 or more") was read by QL as the opposite of the intent; the corrected rule above is the intended reading (flag to the GDD owner).
- Beacon test (VA-3): 4-choice among lit patches at 40-60 m (one court pool, three non-court), randomised positions; at most 58 of 200 picks of the court pool. A fail with `bSunPoolBeaconIsReject` true blocks; with false it is ADVISORY and informs flipping the flag.
- Paired-capture test (VA-1/VA-5): per category (alley, start zone, court, respawn region), at least 30 pairs and at least 5 reviewers (at least 150 judgments each); at most 80 of 150 correct (rate bound at most 60%). The stand-in reviewers use the graybox, not final art.
- Retain captures, trial sheets, randomisation seeds and the art-director sign-off in `production/qa/evidence/level-socket/`; record failures with the offending kit piece or socket for a fix and re-run.

---

## Out of Scope

- Story 043: bush tests; story 042: value band; story 032: emergence no-pop; final-art conformity (character/kit pipeline epics)

---

## Files

- `production/qa/evidence/level-socket/static-figure-*.png`, `beacon-*.png`, `paired-*-*.png`
- `production/qa/evidence/level-socket/static-figure-beacon-paired-results.md`

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE after revision, provisional TR).*

- **AC-1** static-figure test
  - Setup: greyscale 30 m captures of every kit piece, >= 10 trials per piece, >= 5 testers.
  - Verify: "person" calls per piece and overall.
  - Pass condition: each piece <= 2 calls (3 or more fails); overall <= 5% over >= 200 trials.
- **AC-2** sun-pool beacon
  - Setup: 40-60 m, 4-choice among lit patches (1 court pool, 3 non-court), >= 200 trials.
  - Verify: court picks.
  - Pass condition: <= 58 of 200; ADVISORY while `bSunPoolBeaconIsReject` is false, BLOCKING once true.
- **AC-3** paired captures
  - Setup: >= 30 pairs per category (alley, start zone, court, respawn region), >= 5 reviewers, >= 150 judgments each.
  - Verify: correct identifications.
  - Pass condition: <= 80 of 150 per category.

---

## Test Evidence

**Story Type**: Visual/Feel
**Required evidence**:
- Visual/Feel: retained captures and the results sheet in `production/qa/evidence/level-socket/` with art-director sign-off

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/level-socket-contract/story-028-graybox-map-authoring.md`, `production/epics/level-socket-contract/story-032-emergence-no-visible-pop.md`, `production/epics/level-socket-contract/story-040-sun-azimuth-and-cornice-shade-oi4.md`
- Cross-epic: `production/epics/character-pool-appearance/EPIC.md` (mannequin pool for figures)
- Unlocks: Vertical Slice gate (`/vertical-slice`)
