# Story 052: A strike's target id gives no oracle, every id class resolves like a spot attack

> **Epic**: Information Visibility Policy
> **Status**: Blocked — BLOCKED: [Dep:Assassination] needs the assassination-defence-reversal Attack resolver; S15 unreported
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

An `Attack(TargetPawn)` command carries a target id. If the server accepted, rejected, replied to, rate-limited or timed that command differently depending on whether the id is visible, concealed-and-culled, stale or random, a modified client could use it to probe which pawns are hiding (leak row L33). The cure (R7.4) is one resolver: a culled id resolves against capsules at the supplied world point, exactly like `Attack(TargetSpot)`. This story proves the equivalence on all four id classes with enough samples to make the F7 bounds meaningful.

**GDD**: `design/gdd/information-visibility-policy.md`
**Requirement**: `TR-ivp-001` (`CanViewerKnow` server query, default deny, seven viewer classes)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0003: Replication, relevancy and the concealment cull hybrid (section 5, the concealment cull; R7.4); ADR-0008: Test and automation strategy for leak and parity tests (sections 1 T3/T4, 5)
**ADR Decision Summary**: A culled pawn exists for the attacker's connection only through the Policy's cull record, and a strike never re-opens a culled pawn in its outcome slot; a killed-concealed victim re-opens only at the death write with `bDiedConcealed = true`. ADR-0008 runs multi-process (T3) parity captures and offline statistical analysis (T4) with fixed seeds and negative controls.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: ADR-0003 E5 (`FindActorChannelRef` plus `UActorChannel::Close`) [V]; E10 (a closed channel stays closed while the override returns false) [V], in-flight cull/un-cull race [U, S10]; the culled-id behaviour depends on S9 (story-004) and S10 (story-005). S15 (the blind-attack oracle, owned by assassination-defence-reversal) is unreported.

**Control Manifest Rules (this layer)**:
- Forbidden: **Never re-open a culled pawn in a strike's outcome slot; a killed-concealed victim re-opens only at the death write, with `bDiedConcealed = true`** — source: ADR-0003
- Required: **`AStealthPawn::IsNetRelevantFor` does not call `Super`**; its only inputs are the Policy's cull record and the viewer's relation — source: ADR-0003
- Forbidden: **Never order commands by client-supplied or estimated send time (lag compensation)** — spoofable timing channel, contradicts Round R12 — source: ADR-0002
- Required: **Every leak, parity, statistical and lint gate includes a negative control the suite detects** — source: ADR-0008

---

## Acceptance Criteria

*From GDD `design/gdd/information-visibility-policy.md`, AC-IVP-39, scoped to this story:*

- [ ] **AC-IVP-39** [Int][Net][Dep:Assassination] `StealthGame.Visibility.StrikeIdOracleParity`: GIVEN `Attack(TargetPawn)` commands whose id is visible, concealed-and-culled, stale or random, each paired with `Attack(TargetSpot)` at the same `WorldPoint`, WHEN each resolves, THEN each pair has the same outcome class, reply code, rate-limit budget consumption and timing class (F7 bounds over ≥ 1236 samples per id class). (R7.4, L33)

---

## Implementation Notes

*Derived from ADR-0003 section 5, ADR-0008 sections 1, 3, 5 and the GDD R7.4 / R12 row L33:*

- Test shape (QA: "T3 or T2 statistical"): primary `StealthGame.Visibility.MP.StrikeIdOracleParity` (T3, `Source/StealthTestHarness/Private/Scenarios/` plus a Gauntlet node in `Build/Scripts/`), with the F7 comparison analysed by a script in `tools/ci/stats/` (observations out, gate script decides). A T2 `NETWORK_TEST_CLASS(StrikeIdOracleParity, "StealthGame.Visibility.PIE")` variant is acceptable for the outcome-class, reply-code and budget parts if the timing statistics are kept in T3; decide at implementation and record it. Run under `StealthLeak1` and assert the legacy replication system.
- Four id classes are all required: visible, concealed-and-culled, stale, random. For each, issue `Attack(TargetPawn)` with the id and, paired, `Attack(TargetSpot)` at the same `WorldPoint`; compare outcome class, reply code, rate-limit budget consumption and timing class, with F7 bounds over at least 1236 samples per id class (F7 is defined in the GDD Formulas; do not loosen it). Pin the command streams and seeds in a checked-in file before the run (ADR-0008 section 9).
- A culled id must resolve as the spot attack would: against capsules on the server. The cull itself (story-024) and the re-open rules (stories 005, 028) are not re-implemented here; the harness drives them.
- Spike gates: story-004 (S9) and story-005 (S10) must have reported; S15 (the blind-attack oracle owned by assassination-defence-reversal) must have reported.
- Negative control: the GDD criterion and the QA spec state none. Since ADR-0008 section 3 requires one for every parity gate, add a harness-only variant (for example a resolver that rejects a culled id) only if the story author and qa-lead agree during implementation; do not treat its absence as a pass condition.
- Prototype: not applicable. Replaces: none (new).
- Cross-epic needs: assassination-defence-reversal epic (`Attack(TargetSpot/TargetPawn)` resolver and reply codes), input-interaction-framework epic (rejection replies, rate limit), concealment-bushes epic (concealed-and-culled victims), tuning-data-config Exit A (T3 harness).
- Unblocks when: stories 004 and 005 and S15 report, and the Attack resolver exists.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- story-028: blind-spot attack on a culled victim (kill, re-open at the death write, screenshot)
- story-042: timing parity of rejection replies, `ActionPose` delay and kill-confirmation delay
- story-051: court ingress parity
- story-024: the cull itself
- assassination-defence-reversal and input-interaction-framework epics: the resolver and ingress behaviour

---

## Files

*Planned (module folders are created by tuning-data-config Exit A and do not exist yet):*

- `Source/StealthTestHarness/Private/Scenarios/` (T3 scenario `StealthGame.Visibility.MP.StrikeIdOracleParity`)
- `Build/Scripts/` (Gauntlet node)
- `tools/ci/stats/` (F7 comparison script, pinned command streams and seeds)

---

## QA Test Cases

- **AC-1**: AC-IVP-39 `StealthGame.Visibility.MP.StrikeIdOracleParity` (T3, or T2 for the non-timing parts; statistical)
  - Given: `Attack(TargetPawn)` commands whose id is visible, concealed-and-culled, stale or random, each paired with `Attack(TargetSpot)` at the same `WorldPoint`.
  - When: each resolves.
  - Then: each pair has the same outcome class, reply code, rate-limit budget consumption and timing class; F7 bounds hold over at least 1236 samples per id class.
  - Edge cases: all four id classes are required.

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: T3 test under `Source/StealthTestHarness/Private/Scenarios/` (or a T2 test under `Source/StealthTestHarness/Private/Tests/PIE/`) with its result record, OR a documented playtest/spike report

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/information-visibility-policy/story-004-spike-s9-cull-latency-and-explicit-channel-close.md`, `production/epics/information-visibility-policy/story-005-spike-s10-reopen-behaviour-and-netguid.md`; assassination-defence-reversal epic (Attack resolver; spike S15); input-interaction-framework epic (rejection replies, rate limit); tuning-data-config Exit A (T3 harness)
- Unlocks: None
