# Story 033: Blind strikes at any bush look identical to the attacker and a killed-concealed body is never drawn on a non-owner client

> **Epic**: Concealment (Bushes)
> **Status**: Ready — Update 2026-10-29: the assassination-defence-reversal strike resolver is built (asn epic: AttackResolverCore / VictimResolverCore with the BushSpotPick draw over Concealment's IBushSpotQuery); the S10 gate was cleared 2026-10-26 (story-056 PASS).
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25f
> **Last Updated**: 2026-10-26

## Context

**GDD**: `design/gdd/concealment-bushes.md` (CON-R9, CON-R12, AC-CON-15, AC-CON-26, AC-CON-25 PIE form)
**Requirement**: `TR-con-007`, `TR-con-011`, `TR-con-012`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0017: Concealment component and per-viewer presentation (§7 server queries, §8 B), with ADR-0018 (outcome slot, death write, spot-pick consumer)
**ADR Decision Summary**: A blind strike at a bush resolves identically in content and timing whether the bush is empty, ally-only, holds an out-of-reach occupant, a concealed NPC or a concealed rival, until Assassination's Owner-only confirmation. A concealed kill re-opens the victim only at the death write carrying `bDiedConcealed = true`, and the body stays presentation-hidden for every viewer but its owner through fall, linger and removal. The wire to a bystander stays outcome-blind until then (AC-ASN-20).
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: The parity check compares packet traces on the attacker client within ±1 server tick; the death write is at `ContactServerTime − StrikeNetLeadSec`. The un-cull and re-open depend on S9/S10.

**Control Manifest Rules (this layer)**:
- Required: A concealed kill un-culls at the death write with `bDiedConcealed = true`; the spot pick uses the secret stream — source: ADR-0003, ADR-0017
- Forbidden: Never re-open a killed-concealed pawn before the death write, and never draw its body on a non-owner client — source: ADR-0017
- Guardrail: Wire outcome-blind until the death write

---

## Acceptance Criteria

*From GDD `design/gdd/concealment-bushes.md` (AC-CON-15, AC-CON-26, AC-CON-25), scoped to this story:*

- [ ] **AC1** (AC-CON-15, [PIE-2] `StealthGame.Concealment.BlindStrikeParity`): **GIVEN** strikes at an empty bush, an ally-only bush, a bush whose occupant is out of reach, a bush with a concealed NPC and one with a concealed rival **THEN** the attacker client's packet traces (replies, replicated properties, timings within ±1 tick) are identical until Assassination's Owner-only confirmation; the NPC case is an NPC kill and the killed body stays hidden through its removal on every non-owner client.
- [ ] **AC2** (AC-CON-26, [PIE-2][Net] `StealthGame.Concealment.KillWhileConcealedTiming`): **GIVEN** a concealed rival killed by a bush strike **THEN** the victim leaves the occupancy record in the execution slot; opponent connections receive no bunch for the victim before `ContactServerTime − StrikeNetLeadSec` (±1 tick); at the death write the re-opened actor carries `bDiedConcealed = true`; the body is never rendered on any non-owner client (allies included) through fall, linger and removal; the same run with an NPC victim produces the same replicated fields and values.
- [ ] **AC3** (AC-CON-25 PIE form): **GIVEN** a `TargetPawn` strike at +0.3 s of `BushEnter` **THEN** the fall is visible and the pending swap is cancelled on every client.

---

## Implementation Notes

*Derived from ADR-0017 §7, §8 B and the GDD edge cases:*

These are CQTest PIE tests using the real strike resolver, the cull (story 025) and the latch (story 020). The five AC1 cases run with a fixed attacker command seq and stream seed so traces are comparable; the comparison point is the attacker client's replies, replicated properties and timings. For AC2 the NPC-victim run must produce the same replicated fields and values as the player-victim run (kind-blindness). AC3 is the two-client form of story 018. The d = 2.34/2.36 m and |Δz| reach-boundary assertions belong to Assassination's F1 reach story (see story 016's coverage note) and are not repeated here.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-016-spot-pick-secret-stream.md`, `story-017-kill-while-concealed-server-rules.md`, `story-018-no-immunity-while-entering.md`: the unit-level server rules.
- IVP `story-028-blind-spot-attack-on-culled.md`: the Policy-side blind-strike check.
- assassination-defence-reversal epic: the resolver itself.

---

## Files

- `Source/StealthTestHarness/Private/Tests/PIE/BlindStrikeParityTest.cpp` (`NETWORK_TEST_CLASS(BlindStrikeParity, "StealthGame.Concealment.PIE")`)
- `Source/StealthTestHarness/Private/Tests/PIE/KillWhileConcealedTimingTest.cpp` (`NETWORK_TEST_CLASS(KillWhileConcealedTiming, "StealthGame.Concealment.PIE")`)
- `Source/StealthTestHarness/Private/Tests/PIE/NoImmunityWhileEnteringPieTest.cpp`

---

## QA Test Cases

*Written by qa-lead at story creation (QL-STORY-READY: ADEQUATE; deferred until the assassination resolver exists).*

- **AC1**: BlindStrikeParity
  - Given: strikes at an empty bush, an ally-only bush, an out-of-reach occupant, a concealed NPC, and a concealed rival.
  - When: the attacker client's packet traces are compared.
  - Then: replies, replicated properties and timings (±1 tick) are identical until Assassination's Owner-only confirmation; the NPC case is an NPC kill, and the body stays hidden through removal on every non-owner client.
- **AC2**: KillWhileConcealedTiming
  - Given: a Concealed rival killed by a bush strike.
  - When: the connections are observed.
  - Then: the victim leaves the record in the execution slot; no opponent bunch arrives before ContactServerTime minus StrikeNetLeadSec (±1 tick); the death write carries bDiedConcealed = true; the body is never rendered on any non-owner client through fall, linger and removal; an NPC victim gives the same replicated fields.
- **AC3**: SwapCancelVisible
  - Given: a TargetPawn strike at +0.3 s of BushEnter.
  - When: all clients are observed.
  - Then: the fall is visible and the pending swap is cancelled on every client.

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: the PIE tests above — must exist and pass, OR a documented playtest

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `story-014-attack-breaks-concealment.md`, `story-015-spot-zone-and-occupants.md`, `story-016-spot-pick-secret-stream.md`, `story-017-kill-while-concealed-server-rules.md`, `story-018-no-immunity-while-entering.md`, `story-020-fresh-actor-latch-and-ally-hidden-read.md`, `story-024-is-concealed-from-truth-table.md`, `story-025-cull-push-wiring.md`; IVP S9, S10 spike stories (blockers) and `story-028-blind-spot-attack-on-culled.md`; assassination-defence-reversal epic
- Unlocks: `story-035-prototype-retirement-and-regression.md`
