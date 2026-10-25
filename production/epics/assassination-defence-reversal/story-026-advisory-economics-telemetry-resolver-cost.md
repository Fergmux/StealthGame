# Story 026: Advisory checks: probe economics (F5), probe and brace telemetry, resolver cost

> **Epic**: Assassination & Defence Reversal
> **Status**: Ready
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/assassination-defence-reversal.md` (Formulas F5; Edge Cases "Degenerate strategies"; OQ-ASN-3, OQ-ASN-4; AC-ASN-32, AC-ASN-33, AC-ASN-34)
**Requirement**: `TR-asn-001`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0023: Playtest telemetry hooks (server-only `NeverReleased` telemetry section of the Round Event Log store), with ADR-0018 (Performance Implications, the 0.5 ms queue line)
**ADR Decision Summary**: Kind-blind probes at owners' existing server decision points and kind-blind detectors feed a bounded server-only telemetry section that is serialised only after the round has terminated, never through `UE_LOG`, trace or analytics channels, and never read back by the game; the encounter's per-Attack cost is microseconds inside the 0.5 ms queue line.
**ADR Version**: 2026-10-24 (ADR-0023); ADR-0018 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (project-owned code; the cost capture is an Insights/stat measurement on a Development build)
**Engine Notes**: All three checks are ADVISORY (non-blocking). F5 is a balance model, not runtime code unless TDC's I-4 adopts it, so AC-ASN-32 is a Config/Data check run in the test harness; AC-ASN-33 is a playtest data pull from the Event Log (`KillResolved`, `Reversal`, plus the telemetry section's braces and probe aggregates); AC-ASN-34 measures server game-thread time in a Development build (performance evidence standard: cooked Development standalone on 3060-class hardware for any claim beyond this advisory smoke).

**Control Manifest Rules (this layer)**:
- Required: Probe call sites are on the path every pawn takes and are never conditional on controller kind, team or participant state; telemetry is recorded server-side only — source: ADR-0023
- Forbidden: Never record telemetry client-side; never route Assassination telemetry through `UE_LOG`/`UE_LOGF`; Assassination holds no strike counter — source: ADR-0023, ADR-0018
- Guardrail: ≤ 0.1 ms total resolution for 10 Attacks in a frame with 50 pawns; ≤ 64 bytes of per-strike replicated payload to each relevant client before property headers

F5 (`q = p(1 − b_P)`, `ρ = p·b_P + (1 − p)·b_N`, `m = (1 − p)(1 − b_N)`, `π = m / (m + ρ)`) quantifies the probe's expected cost: at defaults a blind probe kills about one rival per 10.7 NPC kills and is reversed under 1 % of the time; the brace deters deliberate hunters, not guessers; the probe's real price is the strike count plus exposure. The telemetry targets (NPC kills per rival kill between 3 and 15, pair trades ≤ 25 % of reversals, reversals per match between 0.5 and 2) are for review, not gates.

---

## Acceptance Criteria

*From GDD `design/gdd/assassination-defence-reversal.md` (AC-ASN-32, AC-ASN-33, AC-ASN-34), scoped to this story:*

- [ ] **AC1** (AC-ASN-32, [Config/ADVISORY] `ProbeEconomicsF5`): **GIVEN** `TeamSize` 5, `NpcCount` 50, L 3, b_P 0.067, b_N 0.00135 **WHEN** F5 is evaluated **THEN** E[N] = 2.98 ± 0.01, E[RivalKills] = 0.276 ± 0.002, P(≥ 1 kill) = 0.232 ± 0.002, P(strike-out) = 0.976 ± 0.002 and P(reversed) = 0.024 ± 0.002.
- [ ] **AC2** (AC-ASN-33, [Integration/ADVISORY] `ProbeAndBraceTelemetry`, playtest): **GIVEN** ≥ 10 logged internal matches **THEN** the Event Log store yields per match strikes by outcome, NPC kills per rival kill, reversals, braces, and pair trades (a reversal followed within 10 s by a kill of the same defender); targets for review, not gates: NPC kills per rival kill 3 to 15 (F5 baseline 10.7 at `NpcCount` 50), pair trades ≤ 25 % of reversals (the creative director reviews above), reversals per match 0.5 to 2.
- [ ] **AC3** (AC-ASN-34, [Integration/ADVISORY] `ResolverCost`): **GIVEN** 50 pawns and 10 Attacks in one server frame **THEN** Assassination's resolution costs ≤ 0.1 ms of server game-thread time in total in a Development build, and the per-strike replicated payload to each relevant client is ≤ 64 bytes before property headers.

---

## Implementation Notes

*Derived from GDD F5, AC-ASN-33 and ADR-0023:*

F5 as a pure function in the test module (`FProbeEconomicsF5`), not shipped: the story proves the balance model against the TDC defaults and flags when `TeamSize`, `NpcCount`, `NpcKillStrikeLimit` or the braced shares change (TDC `BlindGuessSuccess` 0.249 and `ExpectedBlindKills` 0.300 ignore reversals and are about 7-9 % optimistic: the safe direction, noted for TDC I-4). Telemetry: the braces and probe aggregates come from the `NeverReleased` telemetry section (ADR-0023), the rest from the Round Event Log R19 entries; write a report generator (offline script or commandlet) that reads a finished match's exported section and emits the AC2 table; it never runs inside a round. Detectors run the same code for every pawn.

Cost capture: a Development-build stat scope `Stealth.Budget.Assassination` (Insights naming from the manifest) around the Attack resolver; run the 50-pawn, 10-Attack frame in the harness and assert the scope's total. The 64-byte figure is the sum of the attacker's `FActionState` (26 bytes) and the reply entry, per strike, excluding property headers.

**Prototype:** none; the prototype logged no telemetry.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- ADR-0023 telemetry section implementation (Round / Foundation epics).
- Making F5 runtime code (only if TDC I-4 adopts it).
- Penalty epic's counts and exile.

---

## Files

- `Source/StealthGameplay/Private/Tests/Assassination/ProbeEconomicsF5Test.cpp` (class `FAssassinationProbeEconomicsF5Test`, name `StealthGame.Assassination.ProbeEconomicsF5`)
- `Source/StealthTestHarness/Private/Assassination/ResolverCostTest.cpp` (`StealthGame.Assassination.ResolverCost`)
- `Source/StealthGameplay/Private/Features/Assassination/EncounterService.cpp` (probe call sites for strikes by outcome and braces: ADR-0023 hooks)
- `tools/qa/assassination-telemetry-report.py` (offline report over an exported telemetry section; not shipped) and the resulting `production/qa/assassination-telemetry-[date].md`

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

QL-STORY-READY: GAPS
- AC1: inputs `p` and `L` are not defined; proposed rewrite (unconfirmed): `TeamSize` 5, `NpcCount` 50, `NpcKillStrikeLimit` L = 3, p = TeamSize ÷ (TeamSize + NpcCount), b_P 0.067, b_N 0.00135 (this derivation of p reproduces the stated outputs; the author must confirm); P(strike-out) and P(reversed) are mutually exclusive terminal outcomes summing to 1.
- AC2: the report generator has no fixture before a playtest exists; proposed rewrite (unconfirmed): the generator run on a synthetic exported section with known counts emits the expected per-match table exactly; a live pull of ≥ 10 matches is then reviewed against the targets.
- AC3: the hardware, sample size and statistic are unspecified; proposed rewrite (unconfirmed): on cooked Development standalone, 3060-class, the median over 100 frames after 30 warm-up frames of the `Stealth.Budget.Assassination` scope is ≤ 0.1 ms for 10 Attacks and 50 pawns; replicated payload per strike ≤ 64 bytes (the attacker's `FActionState` is 26 bytes, so the reply entry is ≤ 38 bytes) before property headers.

- **AC-1**: Probe economics F5
  - Given: the F5 pure function `FProbeEconomicsF5` with the inputs above
  - When: F5 is evaluated
  - Then: E[N] = 2.98 ± 0.01, E[RivalKills] = 0.276 ± 0.002, P(≥ 1 kill) = 0.232 ± 0.002, P(strike-out) = 0.976 ± 0.002, P(reversed) = 0.024 ± 0.002
  - Edge cases: p = 0 and p = 1; L = 1; b_P = 0; non-default `TeamSize` or `NpcCount` flags the TDC `BlindGuessSuccess` optimism
- **AC-2**: Probe and brace telemetry
  - Given: ≥ 10 exported telemetry sections from internal matches (plus the synthetic fixture)
  - When: the offline report generator runs
  - Then: the table gives per match strikes by outcome, NPC kills per rival kill, reversals, braces and pair trades (a reversal followed within 10 s by a kill of the same defender); review targets only: 3 to 15 NPC kills per rival kill, pair trades ≤ 25 % of reversals, 0.5 to 2 reversals per match
  - Edge cases: a match with zero rival kills (division); a pair trade at exactly 10.0 s; an unterminated round is not serialised; telemetry never reaches `UE_LOG`
- **AC-3**: Resolver cost
  - Given: 50 pawns and 10 Attacks in one server frame in a Development build
  - When: the stat scope is captured and the replicated payload is summed
  - Then: total ≤ 0.1 ms (median, per the rewrite) and ≤ 64 bytes per strike
  - Edge cases: the first frame (cold caches); 10 Attacks all resolving to reversals; 10 Attacks on one victim; the result is advisory, not a gate

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration (ADVISORY): `ProbeEconomicsF5Test.cpp`, `ResolverCostTest.cpp`, and a telemetry report from ≥ 10 internal matches in `production/qa/` or a playtest document — advisory gate

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `story-007-attack-outcomes-resolved-in-slot.md`, `story-008-attacker-lockout-and-attack-lifecycle.md`, `story-010-reversal-counter-and-chained-strikes.md`, `story-015-owner-results-reply-entries-and-tally.md`; cross-epic ADR-0023 telemetry section (Round epic `story-011-event-log-content.md`, `story-016-pacing-metrics.md`); Penalty and Crowd epics for meaningful NPC-kill counts
- Unlocks: first graybox playtest review of OQ-ASN-3 and OQ-ASN-4
