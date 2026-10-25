# Story 016: StrikeCount never leaves the server and removal audiences match an NPC kill

> **Epic**: NPC-Kill Penalty & Informants
> **Status**: Ready
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/npc-kill-penalty-informants.md`
**Requirement**: `TR-pen-006`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0001: Crowd identity rules (audience carriers; owner-only C-TeamView), with ADR-0019 §1-§2 and ADR-0008 (leak and parity suites)
**ADR Decision Summary**: ADR-0001 fixes four audience carriers (C-Public allowlist on PlayerState, owner-only C-TeamView on PlayerController, C-Server, GameState public structs) and overrides every UE default that reveals player-ness. ADR-0019 keeps `StrikeCount` ServerOnly and exposes only `TallyAfter` (in `FStrikeResult`) and the owner C-TeamView tally, both Owner-only; an exile reaches only the exiled player (`bExiled`), allies see only the Roster Team-audience state change, identical in shape to an `Assassinated` removal.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: ADR-0001 is HIGH (owner-only replication on legacy NetDriver with Iris pinned off). Spikes S1/S2/S14 reported 2026-10-25 (`production/qa/spikes/2026-10-25_S1-S2-S14.md`): the pointer trims work; open conditions are rejoin, travel, PawnRecovery and a multi-process capture under the exact `LeakNetProfile` (PktLag 100, PktLoss 1-2 %). The differential capture here is the harness' multi-client capture (ADR-0008); run it single-process first, multi-process when the harness supports it.

**Control Manifest Rules (this layer)**:
- Required: Owner-only data reaches only its owner; every outcome-revealing property class is identical across victim kinds — source: ADR-0001, ADR-0018
- Forbidden: never make Penalty state a replicated property; never let `StrikeCount` reach a client before `Ended` — source: ADR-0019
- Guardrail: the capture and diff run in CI headless (`-nullrhi`) within the test budget — source: ADR-0008

`StrikeCount` is the one number whose leak would turn every informant kill into an identification tool: if clients could see the true strike count, a kill that did not raise it would reveal "that citizen was an informant". The suite proves it never appears (reflection scan plus differential wire capture), and that an exile is visible only to its owner and, as a plain state change, to allies.

**Prototype replaced:** the prototype's replicated `NpcKills` on `AStealthPlayerState` (visible to every client through the PlayerState) and its distinct removal broadcast; both are the leak this suite exists to prevent. The prototype's counter is not promoted; its removal is story 021.

---

## Acceptance Criteria

*From GDD `design/gdd/npc-kill-penalty-informants.md` (AC-NKP-04 split into two criteria at QL-STORY-READY, AC-NKP-21), scoped to this story:*

- [ ] **AC1** (AC-NKP-04 part (a), [L] `StealthGame.Penalty.StrikeCountNeverLeaves`): **GIVEN** the reflected replicated properties and RPC signatures of every class **WHEN** they are scanned (with `StrikeCount` set to a sentinel value in a live probe and the serialized `FStrikeResult`, `FDefenceResult`, owner part and public carriers scanned for it) **THEN** no property or parameter carries `StrikeCount`, and only `TallyAfter` (in `FStrikeResult`) and the owner C-TeamView tally exist, both Owner-only (PEN-R2, CR-1).
- [ ] **AC2** (AC-NKP-04 part (b), [I] `StealthGame.Penalty.PIE.StrikeCountDifferential`, needs the wire-capture harness): **GIVEN** a differential wire capture of every client connection over two scripted 3v3 rounds with identical seeds that differ only in the attacker's pre-set `StrikeCount` (for example run A: two informant kills, k = 0, t = 2; run B: two NPC kills, k = 2, t = 2; L >= k + 2 so the identical kill sequence that follows, an NPC kill, an informant kill and a graced kill, does not exile in run B) **WHEN** the captures are diffed **THEN** no connection receives any byte that differs between the two rounds apart from normalised timestamps, until an exile actually fires (PEN-R2, CR-1).
- [ ] **AC3** (AC-NKP-21, [I][PIE-3] `StealthGame.Penalty.PIE.RemovalAudiences`): **GIVEN** an exile **WHEN** every client's received data are logged **THEN** only the exiled player's connection receives `bExiled`; allies receive only the Roster Team-audience state change, identical in shape to an `Assassinated` removal; each opponent's received property set for the released pawn has the same classes, replication conditions and update cadence as in an NPC-kill run by the same attacker, and only pawn movement differs (PEN-R15; the witness-side residual PEN-R14.2 is accepted, not tested away).

---

## Implementation Notes

*Derived from ADR-0019 Decision §1, §2 and the leak harness of ADR-0008:*

AC1(a) is a reflection test over every `UClass`/`UScriptStruct` in the three runtime modules: for each replicated `FProperty` and each RPC parameter, assert none is named, typed or derived as `StrikeCount` (check name, and that no struct field of `FStrikeResult`, `FDefenceResult`, `FTeamViewOwnerPart`, `FDeathState` or any C-Public carrier equals the strike count by value in a live probe: set k to a sentinel like 7 and scan serialized output bytes for it). AC1(b) is the differential capture: run the same scripted round twice (the same seeds), differing only in the attacker's `StrikeCount` before an identical kill sequence; capture every client connection's raw bytes (ADR-0008 capture helper, normalise timestamps) and diff; the diff must be empty until an exile fires. The `LeakNetProfile` is PktLag 100 / PktLoss 1-2 % per ADR-0001.

AC2 logs, per connection, what is received on an exile: owner connection gets `bExiled` (in the `FStrikeResult` entry) and its own state change; allies get the Roster Team-audience state change with the same shape as an `Assassinated` removal (compare the byte shape with an assassination run); opponents get the released pawn's properties with the same classes, conditions and cadence as in an NPC-kill run by the same attacker (movement differs only). Assert specific properties, not just "no diff": the owner-only `bExiled` presence is a positive control.

Do not "test away" the witness-side residual PEN-R14.2: an opponent who strikes the released body may receive `WrongTarget`; the GDD accepts it. The suite must not add an assertion that would fail on the accepted residual.

Failures that originate in carriers owned by other epics (information-visibility-policy, participant-state-roster) are routed to those epics; the reflection scan here is Penalty's guarantee that it adds no replicated type.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-003-tally-rule-and-strike-result-supply.md`: tally logic and the `SetOwnerTally` discipline.
- `story-008-informant-kill-inline-retire.md`: per-kill parity for the attacker and allies.
- information-visibility-policy (all leak-harness stories, e.g. `story-052-strike-id-oracle-parity.md`, `story-055-clock-exchange-owner-only-audit.md`): the generic carrier and allowlist audits.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Private/Tests/Penalty/PenaltyStrikeCountNeverLeavesTest.cpp` (class `FPenaltyStrikeCountNeverLeavesTest`, name `StealthGame.Penalty.StrikeCountNeverLeaves`: the reflection scan)
- `Source/StealthTestHarness/Private/Penalty/StrikeCountDifferentialTest.cpp` (CQTest, `StealthGame.Penalty.PIE.StrikeCountDifferential.*`)
- `Source/StealthTestHarness/Private/Penalty/RemovalAudiencesTest.cpp` (CQTest, `StealthGame.Penalty.PIE.RemovalAudiences.*`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC1** (AC-NKP-04 part (a), `StealthGame.Penalty.StrikeCountNeverLeaves`)
  - Given: reflection over every replicated property and RPC signature, and a live probe with `StrikeCount` set to two sentinels (7, then 200).
  - When: the scan runs and `FStrikeResult`, `FDefenceResult`, the owner part and the public carriers are serialised under each sentinel.
  - Then: no property or parameter carries `StrikeCount`; only `TallyAfter` and the owner C-TeamView tally exist, both Owner-only; the serialised bytes are identical across the two sentinels.
  - Edge cases: a byte search for one uint8 value gives false positives, hence the two-sentinel comparison; watch a seeded replicated `StrikeCount` fail the scan once.
- **AC2** (AC-NKP-04 part (b), `StealthGame.Penalty.PIE.StrikeCountDifferential.<Method>`; DEFERRED until the wire-capture harness exists)
  - Given: identical seeds; run A pre-set to k = 0, t = 2 via two informant kills, run B to k = 2, t = 2 via two NPC kills; L >= 4.
  - When: both runs play the same NPC kill, informant kill and graced kill and every client connection's capture is diffed.
  - Then: no connection receives a differing byte apart from normalised timestamps, until an exile fires.
  - Edge cases: first diff run A against run A and require a clean result (proves the harness is deterministic); watch a seeded exile run fail the diff once.
- **AC3** (AC-NKP-21, `StealthGame.Penalty.PIE.RemovalAudiences.<Method>`)
  - Given: an exile in 3v3 PIE.
  - When: every client's received data are logged.
  - Then: only the exiled player's connection gets `bExiled`; allies get only the Team-audience state change, identical in shape to an `Assassinated` removal; opponents' property sets for the released pawn have the same classes, conditions and cadence as in an NPC-kill run by the same attacker, and only movement differs.
  - Edge cases: the same attacker and seed in both runs.

QL-STORY-READY: ADEQUATE (AC2 DEFERRED until the wire-capture harness exists)

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: the CQTest PIE tests under `Source/StealthTestHarness/Private/Penalty/` and the reflection test under `Source/StealthGameplay/Private/Tests/Penalty/` OR a documented playtest

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/npc-kill-penalty-informants/story-003-tally-rule-and-strike-result-supply.md`, `story-005-exile-sequence-in-attack-slot.md`, `story-008-informant-kill-inline-retire.md`; cross-epic: information-visibility-policy (carriers and the capture harness, `story-052-strike-id-oracle-parity.md`), participant-state-roster (`story-038-owner-view-feed.md`, Team-audience state change), tuning-data-config `story-004-test-harness-module-shipping-excluded.md`, `story-019-cqtest-headless-proof.md`
- Unlocks: None
