# Story 044: Animation stays within 4 ms at the reference scene on RTX 3060-class hardware

> **Epic**: Shared Character & Locomotion
> **Status**: Blocked — BLOCKED: spike R3 (ADR-0013 and ADR-0022 gate: RTX 3060-class hardware at 1080p, 60 characters) has not reported; no R3 report exists in `production/qa/spikes/`, and all existing performance evidence is from an RTX 5090 machine, which ADR-0013 says is not acceptance evidence
> **Layer**: Core
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: [set by /dev-story when implementation begins]

## Context

**GDD**: `design/gdd/shared-character-locomotion.md` (R12 Performance contract, F8 Animation cost budget; AC-SCL-29; OQ4 answered by ADR-0013 section 5, OQ16 closed)
**Requirement**: `TR-scl-008`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0013: Animation: motion matching and significance budget (section 5, "The 4 ms definition and the performance gate"), with ADR-0022 (CPU/GPU budgets)
**ADR Decision Summary**: `C_anim` is the per-frame sum of game-thread animation time, worker-thread animation update and evaluation time (PoseSearch and chooser included) and the significance subsystem and step clocks, excluding simulated-proxy CMC and GPU skinning. It must be at most 4.0 ms with a game-thread share at most 2.0 ms, p95 over 60 s after a 10 s warm-up, on the client reference scene and on the listen host with 9 remote clients. If it fails, the equal-for-all fallbacks apply in order: further database pruning and throttles, lower caps, foot placement off below tier A, tier C and D-visible to a non-motion-matching locomotion state; never a kind-dependent setting.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Gate **R3** is the precondition (technical-artist runs, performance-analyst signs, art bible 8.x.14). Motion-matching cost per search at this database size is unknown until R3 (E9). Measured with Unreal Insights (`-trace=cpu,frame`, named scopes on the tier subsystem and step clocks). Never cite RTX 5090 numbers as acceptance evidence.

**Control Manifest Rules (this layer)**:
- Required: `C_anim` is measured per ADR-0013 section 5 (game-thread plus worker sum, p95, Insights) and reported with the game-thread share.
- Forbidden: never apply a performance fallback to some pawns or clients only, drop character shadows or adapt to who is on screen; never cite RTX 5090 numbers as acceptance evidence.
- Guardrail: `C_anim` at most 4.0 ms (GT + workers, p95), game thread at most 2.0 ms, tier A at most 8, at 60 characters on an RTX 3060-class PC at 1080p.

---

## Acceptance Criteria

*From GDD `design/gdd/shared-character-locomotion.md`, scoped to this story:*

- [ ] **AC-SCL-29** [P][R3] `AnimBudget50`: GIVEN the reference scene with 50 characters on an RTX 3060-class machine at 1080p, WHEN an Insights trace runs 60 s, THEN summed animation time ≤ 4.0 ms per frame (p95), tier A ≤ 8 characters, and CMC proxy time is reported; AND the same 60 s capture on the listen host with 9 remote clients shows summed animation time ≤ 4.0 ms and game-thread share ≤ 2.0 ms at p95 with server-side CMC time reported separately (ADR-0013 §5; if it fails, the uniform `TickPose` escalation applies, never a kind-dependent setting).

*QA clarification (create-stories 2026-10-25, not a GDD change):* the reference scene is 60 characters (owner included, `NpcCount` 50) per ADR-0013 R3 and TR-scl-008; the AC text's "50 characters" is stale wording that the GDD owner should correct (the QA lead concurs).

---

## Implementation Notes

*Derived from ADR-0013 section 5:*

- This is the R3 measurement story: build the reference scene (10 players plus 50 NPCs, walking), capture 60 s after a 10 s warm-up on a 3060-class machine at 1080p, report `C_anim` (p95), the game-thread share, tier-A count and, separately, CMC proxy time; repeat on the listen host with 9 remote clients and report server CMC time separately. Store the Insights summary under `production/qa/spikes/`.
- Also re-verify the 36 KB/s working value (story 034) and the F8 budget sum (I-SCL-11) with the measured per-tier costs `c_t`.
- To unblock: someone with RTX 3060-class hardware runs the capture (owner: technical-artist, signed by performance-analyst), the report lands in `production/qa/spikes/`, and ADR-0013's conditional gate is updated.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 039: the tiering it measures. Story 034: bandwidth.

---

## Files

- `Source/StealthGameplay/Private/Tests/Locomotion/AnimBudget50Test.cpp` (performance test with thresholds, planned; hardware-gated)
- `production/qa/spikes/` R3 report (new)

---

## QA Test Cases

*QL-STORY-READY: ADEQUATE (after clarification; BLOCKED on R3 hardware). Test `StealthGame.Locomotion.AnimBudget50` (performance test, hardware-gated).*

Test: animation budget at the reference scene (AC-SCL-29, spike R3)
  Given: the reference scene with 60 characters (owner included, `NpcCount` 50) on an RTX 3060-class machine at 1080p
  When: an Insights trace runs 60 s after a 10 s warm-up, and the same capture on the listen host with 9 remote clients
  Then: summed animation time ≤ 4.0 ms per frame (p95), tier A ≤ 8 characters, CMC proxy time reported; on the listen host summed animation time ≤ 4.0 ms and game-thread share ≤ 2.0 ms at p95, with server-side CMC time reported separately; on failure the uniform `TickPose` escalation applies, never a kind-dependent setting
  Edge cases: the AC text says 50 characters (stale); never cite RTX 5090 numbers as acceptance evidence

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration (performance): `Source/StealthGameplay/Private/Tests/Locomotion/AnimBudget50Test.cpp`, test name `StealthGame.Locomotion.AnimBudget50` — must exist and pass with the thresholds in the AC; the Insights capture summary in `production/qa/spikes/`

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/shared-character-locomotion/story-039-tier-parity-ab.md`, `story-041-body-and-anim-set-parity.md`, `story-043-footstep-parity.md`, `story-034-bandwidth-budget-59-pawns.md`; `production/epics/crowd-npc-behaviour/EPIC.md` (50 NPCs), RTX 3060-class hardware
- Unlocks: ADR-0013 and ADR-0022 conditional-gate closure; the mass-production gate (art bible 8.x.14)
