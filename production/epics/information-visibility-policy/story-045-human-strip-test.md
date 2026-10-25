# Story 045: Human strip test hits stay below h_fail

> **Epic**: Information Visibility Policy
> **Status**: Blocked — BLOCKED: needs the strip harness scenes (character-pool-appearance pool and art-bible scenes) and 8 human testers; results retained in production/qa/evidence/
> **Layer**: Foundation
> **Type**: Visual/Feel
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

Every automated gate in this epic checks structure and statistics; none of them shows that a human cannot spot the players in a crowd by eye. The strip test puts 8 testers in front of 12 scenes from the strip harness and asks them to pick enemy players. If total hits stay below `h_fail` (computed per F8 from each scene's chance level), the crowd's look and movement are not an exploitable tell. It is the one human-judged Pillar 1 gate and feeds the milestone and release checks (R12).

**GDD**: `design/gdd/information-visibility-policy.md`
**Requirement**: `TR-ivp-007` (Leak catalogue with CRITICAL ship gate; per-commit canaries)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0008: Test and automation strategy for leak and parity tests (section 1, Visual and UI evidence note, section 9); ADR-0001: Crowd identity rules (Decision, section 4.6)
**ADR Decision Summary**: ADR-0008 states that visual and UI evidence stays as the coding standards define it, screenshots and sign-off retained in `production/qa/evidence/`, and is not replaced by any automated tier; pre-registered thresholds for statistical analyses are checked in before data is collected. ADR-0001 requires one pawn class and one component set so that no body differs by kind.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: ADR-0008 is MEDIUM risk (CQTest PIE network component, `EAutomationTestFlags` as an enum class and the net-trace path are newer than reliable training data). This story is human-run and rendering-dependent, so it is Visual evidence, not T2 (ADR-0008 section 1 T2 environment note: rendering-dependent checks are Visual evidence).

**Control Manifest Rules (this layer)**:
- Required: **Every body is an `AStealthPawn`**: one class, one component set, no per-instance replication or net-config difference — source: ADR-0001
- Forbidden: **Never add an NPC subclass, an NPC-only component, a lightweight NPC pawn or a Mass crowd representation** — every difference is a Pillar-1 detector — source: ADR-0001
- Forbidden: **Never feed anything except screen height and the eye-mask rendered flag into animation tiering, URO, budget allocation, mesh LOD or audio concurrency; never a gameplay fact** — source: ADR-0001

---

## Acceptance Criteria

*From GDD `design/gdd/information-visibility-policy.md`, AC-IVP-27, scoped to this story:*

- [ ] **AC-IVP-27** [Human] `StealthGame.Visibility.HumanStripTest`: GIVEN 12 scenes × 8 testers from the strip harness, WHEN testers pick enemy players, THEN total hits stay below `h_fail` computed per F8 from the scenes' chance levels (16 when every scene is 1/9); results retained in `production/qa/evidence/`. (F8)

---

## Implementation Notes

*Derived from the GDD criterion and ADR-0008 sections 1 and 9:*

- This is a human protocol, not an automated test. The deliverable is an evidence file `production/qa/evidence/human-strip-test-evidence.md` holding: the scene list with each scene's chance level, `h_fail` computed per F8 (16 when every scene is 1/9, so 15 or fewer hits passes), each tester's picks and hits, the total, the build and commit tested, and the lead sign-off. Retained screenshots of each scene go in `production/qa/evidence/` beside it.
- Pre-register `h_fail` and the scene chance levels before testers see anything (ADR-0008 section 9: pre-registered priors and thresholds are checked in before the data is collected). Testers must not be told which pawns are players.
- Session-record rule: playtests whose data feeds F8 must carry a session record that satisfies the gate in story-048 (AC-IVP-36: names a non-competing host or excludes the host team's rows); record it with this evidence.
- The strip harness scenes come from the character-pool-appearance epic (the pool) and the art-bible scenes; this story does not build them. The harness lives in `StealthTestHarness`; scenes must use real `AStealthPawn` instances so that a tell in the pawn or its animation shows up here.
- Prototype: nothing promoted. Replaces: none (new).
- Cross-epic needs: character-pool-appearance epic (appearance pool and deal), the art-bible strip scenes, shared-character-locomotion epic (`AStealthPawn`), crowd-npc-behaviour epic (NPC behaviour in the scenes), 8 human testers.
- Unblocks when: the strip harness scenes exist on production classes and 8 testers are scheduled.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- story-043: transport classifier AUC (statistical, not human)
- story-044: raw-bunch schema parity
- story-048: the playtest session-record gate (this story only produces a record that must pass it)
- character-pool-appearance epic: building the appearance pool and the strip scenes
- crowd-npc-behaviour epic: the NPC behaviour and tell matrix

---

## Files

*Planned:*

- `production/qa/evidence/human-strip-test-evidence.md` (protocol, scene chance levels, `h_fail`, per-tester results, sign-off)
- `production/qa/evidence/` retained screenshots of the 12 scenes
- strip harness scenes and any tester sheet under `Source/StealthTestHarness/` (owned by character-pool-appearance; listed here only as the dependency)

---

## QA Test Cases

- **AC-1**: AC-IVP-27 `StealthGame.Visibility.HumanStripTest` (manual, human)
  - Setup: the strip harness, 12 scenes and 8 testers.
  - Verify: testers pick enemy players in each scene; total hits are compared with `h_fail` computed per F8 from the scenes' chance levels.
  - Pass condition: total hits below `h_fail` (16 when every scene is 1/9, so 15 or fewer); results and sign-off retained in `production/qa/evidence/`.

---

## Test Evidence

**Story Type**: Visual/Feel
**Required evidence**:
- Visual/Feel: a retained screenshot in `production/qa/evidence/` + sign-off in `production/qa/evidence/human-strip-test-evidence.md`

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: character-pool-appearance epic (the strip harness scenes and appearance pool); art-bible strip scenes; shared-character-locomotion epic (`AStealthPawn`); crowd-npc-behaviour epic (NPC behaviour in the scenes); 8 human testers
- Unlocks: None
