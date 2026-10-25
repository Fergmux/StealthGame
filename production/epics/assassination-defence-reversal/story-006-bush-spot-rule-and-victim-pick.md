# Story 006: Bush spot rule is terminal, with an unbiased victim pick from the secret stream

> **Epic**: Assassination & Defence Reversal
> **Status**: Ready
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/assassination-defence-reversal.md` (ASN-R8 rule 2; Edge Cases "If the target is concealed"; AC-ASN-12, AC-ASN-39)
**Requirement**: `TR-asn-001`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0018: Encounter data and outcome presentation timing (§2.1 step 1; confirmed as consumer of `BushSpotPick`), with ADR-0017 (§7 spot occupants and the stream) and ADR-0006 (seed domain 14)
**ADR Decision Summary**: For a bush spot Assassination filters Concealment's occupants by F1 reach, then draws the victim from the `BushSpotPick` secret stream (`ESeedDomain` 14, tag `CON_SPOT`), never `H(...) mod n` and never `RoundSeed`; the rule is terminal, so an empty or ally-only bush is a whiff.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0018 overall; the draw itself is project code over ADR-0006's integer generator)
**Engine Notes**: No engine RNG: `UniformBelow(n)` is ADR-0006's unbiased ranged draw (no modulo bias), keyed by `(ServerTickIndex, AttackerCommandSeq, BushId)`. The test injects the stream seed so the distribution check is deterministic. Concealment's `GetSpotOccupants(BushId, Attacker)` and the bush footprint are consumed through the consumer-owned interface, not Concealment types.

**Control Manifest Rules (this layer)**:
- Required: The spot pick uses the secret `BushSpotPick` stream with an unbiased ranged draw — source: ADR-0017, ADR-0006
- Forbidden: Never use `RoundSeed`, a public seed or `mod` for the bush spot pick; never use `FMath::Rand*` or `FRandomStream` — source: ADR-0017, ADR-0006
- Forbidden: Never let Hide legality, replies or timing read occupancy; a blind strike shows no targeting UI — source: ADR-0017
- Guardrail: Pick is O(occupants) per strike

Rule 2 applies when the `WorldPoint` lies in a bush's spot zone: `GetSpotOccupants(BushId, Attacker)` is filtered by the full F1 reach test at each candidate's server position at the slot, then one survivor is picked with `UniformBelow(|Reachable|)`. The rule is terminal: if no occupant qualifies, the strike is a whiff and never falls through to rule 3, so a blind strike is a commitment to a bush. Every victim in the bush is a concealed pawn, so a visible NPC standing just outside the footprint is not struck.

---

## Acceptance Criteria

*From GDD `design/gdd/assassination-defence-reversal.md` (AC-ASN-12, AC-ASN-39), scoped to this story:*

- [ ] **AC1** (AC-ASN-12, [Integration/BLOCKING] `StealthGame.Assassination.BlindBushStrikeIsACommitment`): **GIVEN** a bush concealing one rival **WHEN** the attacker strikes 10 `WorldPoint`s spread across the bush's spot zone **THEN** every strike resolves the same candidate set and kills the rival (`RivalEliminated`); **GIVEN** an empty bush with a visible NPC standing just outside the footprint within the zone **WHEN** struck by `TargetSpot` inside the zone **THEN** `NoContact` (rule 2 is terminal).
- [ ] **AC2** (AC-ASN-39, [Logic/BLOCKING] `StealthGame.Assassination.BushVictimHash`): **GIVEN** a bush with two non-ally occupants and 1,000 strikes whose picks are drawn from `BushSpotPick` with a fixed injected stream seed **THEN** exactly one victim is picked per strike, only after the F1 reach filter, and each occupant is picked 50 ± 5 % (`UniformBelow` draws from Concealment's server-secret `BushSpotPick` stream per F4, never `RoundSeed`; shared with AC-CON-14).

---

## Implementation Notes

*Derived from ADR-0018 §2.1 and ADR-0017 §7, GDD ASN-R8:*

The test name `BushVictimHash` is a stable identifier from the GDD; the pick is the `BushSpotPick` stream, not a hash (the former `SpotSalt` hash is replaced). Stream: `FSecretStream(GetServerSubSeed(BushSpotPick, LayoutAttempt), "CON_SPOT")`, ADR-0006 domain 14, keyed by `(ServerTickIndex, AttackerCommandSeq, BushId)`; the stream and the occupancy live server-only and are Concealment's; Assassination consumes one `UniformBelow(|Reachable|)` draw per resolving bush strike and must not draw when `|Reachable| ≤ 1` differently from when it is larger (always draw once, so the stream position never reveals the occupancy count; ask Concealment's author to confirm the draw count rule).

The honoured spot zone is Concealment's (`BushSpotToleranceCm`, default equal to `OpenSpotToleranceCm`); a concealed occupant whose server position fails F1 reach is filtered out before the draw; an `Exiting` pawn that is already un-culled remains a zone candidate. The occupant removal (occupancy exit in the slot) is story 007's in-slot effect; the death-write un-cull of a killed-concealed victim is stories 013 and 023.

The `BushStrike` clip variant, its `T_v` of 0.75 s and `BushPreface` bit are handled by stories 002 (reach), 012 (write) and 018 (presentation). From a bush the attacker's own concealment breaks at execution through `OnActionStarted(Attack)` (Concealment).

**Prototype acceptance reference:** networked prototype test F (a blind strike kills a hidden enemy) is the behavioural reference; the prototype's `BlindStrikeRadius` sphere test is not.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-005-victim-resolution-order.md`: rules 1, 3 and 4 and the candidate filter.
- `story-012-slot-write-outcome-blind-and-warp-point.md`: bush `WarpPoint` clamped to the footprint edge (AC-ASN-47).
- `story-023-concealed-body-stays-hidden.md`: the killed-concealed re-open and `bDiedConcealed`.
- `story-024-owner-result-matcher-and-plate-timing.md`: AC-ASN-40, no bush targeting UI.
- Concealment epic (stories not yet created): `GetSpotOccupants`, spot zones, the `BushSpotPick` stream owner.

---

## Files

- `Source/StealthGameplay/Private/Features/Assassination/VictimResolverCore.cpp` (rule 2 branch; shared with story 005)
- `Source/StealthGameplay/Public/Server/Features/Assassination/BushSpotQuery.h` (consumer-owned `IBushSpotQuery`, implemented by Concealment, registered weakly by the composition root)
- `Source/StealthGameplay/Private/Tests/Assassination/BlindBushStrikeIsACommitmentTest.cpp` (class `FAssassinationBlindBushStrikeIsACommitmentTest`)
- `Source/StealthGameplay/Private/Tests/Assassination/BushVictimHashTest.cpp` (class `FAssassinationBushVictimHashTest`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

QL-STORY-READY: GAPS
- AC1: "10 WorldPoints spread across the zone" and "just outside the footprint" are unquantified, and nothing resets the lockout between strikes; proposed rewrite (unconfirmed): 10 fixed WorldPoints enumerated in the fixture (zone edge and centre included), each in a fresh round or after the 1.75 s lockout, and an NPC 10 cm outside the footprint inside the zone; every strike yields the same candidate list and `RivalEliminated`; the NPC-outside case yields `NoContact` and the NPC is not struck.
- AC2: "50 ± 5 %" is not absolute or relative, "only after the reach filter" is unproven with two occupants, and the one-draw-per-strike rule has no AC; proposed rewrite (unconfirmed): fixed stream seed `BushPickSeed`; across 1,000 strikes each occupant is picked 450-550 times; a run with a third occupant failing F1 reach shows that occupant never picked and the stream position equal to the two-occupant run after the same number of strikes (one draw per strike regardless of `|Reachable|`).

- **AC-1**: Blind bush strike is a commitment
  - Given: a stub-Concealment harness, a bush with one concealed rival, 10 enumerated WorldPoints, and an empty bush with a visible NPC 10 cm outside the footprint
  - When: each point is struck in turn (fresh round or lockout elapsed)
  - Then: all 10 resolve the same single candidate and `RivalEliminated`; the empty-bush strike is `NoContact` and the NPC is untouched
  - Edge cases: points on the zone boundary; rule 2 never falls through to rule 3; an `Exiting` occupant already un-culled is still a candidate
- **AC-2**: Unbiased victim pick
  - Given: a bush with 2 reachable non-ally occupants (and a variant with a third failing reach), injected `BushPickSeed`
  - When: 1,000 strikes each draw `UniformBelow(|Reachable|)`
  - Then: exactly one victim per strike; each occupant picked 450-550 times; the unreachable occupant is never picked
  - Edge cases: one draw per strike even when `|Reachable|` is 1; no `RoundSeed`, `mod` or `FMath::Rand*` (stream spy)

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: the two test files above (AC1 in a 2-client PIE or a stub-Concealment integration harness; AC2 headless with the injected stream) — must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/assassination-defence-reversal/story-002-reach-f1-pure-function.md`, `story-005-victim-resolution-order.md`; cross-epic Concealment epic `production/epics/concealment-bushes/EPIC.md` (no stories yet: `GetSpotOccupants`, `BushSpotPick`); ADR-0006 seed streams (Foundation)
- Unlocks: `story-007-attack-outcomes-resolved-in-slot.md`, `story-012-slot-write-outcome-blind-and-warp-point.md`, `story-023-concealed-body-stays-hidden.md`
