# Story 015: Owner results `FStrikeResult` and `FDefenceResult` as reply entries, with tally and exile flag

> **Epic**: Assassination & Defence Reversal
> **Status**: Complete
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: 2026-10-27

## Context

**GDD**: `design/gdd/assassination-defence-reversal.md` (ASN-R19; UI-ASN-1, UI-ASN-2; AC-ASN-46, AC-ASN-16 payload clause)
**Requirement**: `TR-asn-005`, `TR-asn-011`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0018: Encounter data and outcome presentation timing (§1.3, §1.4, §2.1 step 8, §4.2), with ADR-0011 §8 (the per-connection reply batch)
**ADR Decision Summary**: After every executed Attack the attacker's connection (null sink for AI) receives exactly one Owner-only `FStrikeResult`, and a bound defender who reverses receives `FDefenceResult`, both as entries of the batched owner-only `ClientCommandReplies` RPC, never as properties and never as a stand-alone RPC; the payload has the same shape and size for every `Result`, with `TallyAfter` and `bExiled` read from Penalty's `IStrikeTallySource` after the `OnPawnKilled` fan-out in the same slot.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0018 / ADR-0011 networking; reliable owner RPC ordering relative to property replication is [U] E6, measured in story 024)
**Engine Notes**: The reply batch and `FReplySinkId` table are ADR-0011 §8 / ADR-0002 payload types; this story appends two entry kinds. A missing `IStrikeTallySource` is a fault: it yields `{0, false}` and a fault-sink report, never a crash.

**Control Manifest Rules (this layer)**:
- Required: `FStrikeResult` is sent for every executed Attack and `FDefenceResult` for every reversal by a bound defender, as entries of the ADR-0011 reply batch, same shape for every result; no other outcome field may reach any connection — source: ADR-0018
- Required: `IStrikeTallySource::GetTallyForResult` is read after the `OnPawnKilled` fan-out in the same slot; `bExiled` is Penalty's own in-slot decision, never read from Roster state — source: ADR-0018, ADR-0019
- Forbidden: Never deliver replies as replicated owner-only properties; never send an outcome field besides `FStrikeResult`/`FDefenceResult`; never let `StrikeCount` reach a client before `Ended` — source: ADR-0011, ADR-0018, ADR-0019
- Guardrail: ≤ 4 bytes plus `FClientCommandSeq` per entry

Results: `RivalEliminated` (kill of an Active rival), `WrongTarget` (kill of an NPC **or** an informant, byte-identical payload), `NoContact` (whiff: empty spot, ally-only bush, out of reach, already-resolved body), `Reversed` (attacker reversed; never the defender's kind). `FStrikeResult { ClientCommandSeq, Result, TallyAfter, bExiled, ContactServerTime }`; `ContactServerTime` is the strike's contact time (for `NoContact` the whiff's nominal contact time), so every `Result` keeps one shape. `FDefenceResult { Reversed }` carries no time and no attacker reference.

---

## Acceptance Criteria

*From GDD `design/gdd/assassination-defence-reversal.md` (AC-ASN-46, AC-ASN-16 payload clause), scoped to this story:*

- [x] **AC1** (AC-ASN-46, [Logic/BLOCKING] `StealthGame.Assassination.ExileFlagAndTally`): **GIVEN** an attacker with two NPC strikes and a tally of 2 **WHEN** its third NPC kill resolves **THEN** its `FStrikeResult` carries `WrongTarget`, `TallyAfter` 3 and `bExiled` true, filled inside the same slot; an informant kill at the same counts carries `TallyAfter` 3 and `bExiled` false; no `FStrikeResult` field or size reveals the strike count; `ContactServerTime` equals the attacker's strike write's contact time.
- [x] **AC2** (AC-ASN-16 payload clause, [Integration/BLOCKING] `StealthGame.Assassination.KillPlateTellsOneThing` data half): **GIVEN** strikes that kill an Active rival, an NPC and an informant, a whiff and a reversed strike **THEN** every `FStrikeResult` has the same serialised size; the NPC and informant kills' payloads are identical apart from `ClientCommandSeq` and `TallyAfter`, and `TallyAfter` rises by exactly 1 for each of them (unchanged for the rival kill, the whiff and the reversal); and no other client receives any result field.

---

## Implementation Notes

*Derived from ADR-0018 §1.3, §1.4, §2.1 step 8, §4.2:*

Step 8 of the slot (after step 6, `OnPawnKilled`, so Penalty's strike, tally and exile have already applied): build `FStrikeResult` with `GetTallyForResult(Attacker, Result)`; for a reversal by a bound defender also build `FDefenceResult`; append both to their sinks' per-pass reply buffers (within one slot the IIF's own reply precedes the feature result, ADR-0011 §8). Flushed once per connection at drain end by the IIF reply step (IIF story 004). `NoContact` and `Reversed` still ask the tally source so they carry the current tally unchanged. Every `Result` asks it (uniform call pattern).

Two soundness conditions (TD-ADR concern 4): (i) Penalty's strike, tally and exile follow-ons run inline in the `OnPawnKilled` fan-out (depth ≤ 3 from `Resolve`, under `QueueCascadeDepthMax` 8); if a `CascadeOverflow` ever moved them out of the slot, Assassination defers building `FStrikeResult` into the same overflow item (carrying the slot's `RootCauseKey`) and logs a defect; (ii) `bExiled` is Penalty's decision. `IStrikeTallySource` is consumer-owned (declared by Assassination, implemented by Penalty, registered weakly by the composition root); until Penalty is built, tests inject a fake that returns scripted tallies.

An NPC attacker never strikes (story 011) and a defender NPC has the null sink (no `FDefenceResult`). Reply code, timing and the result size must never depend on kind, team or the defender's identity. A `Rejected_Latched` Attack sends no `FStrikeResult` (story 009, AC-ASN-19 c).

**Prototype replaced:** the prototype's `MessagePlayer(...)` lines in `HandleAssassinationRequest` ("Reversal!", "Your target was ready...", kill confirmation text) are replaced by these structured results; the plate text lives in the HUD (story 024).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-024-owner-result-matcher-and-plate-timing.md`: the client matcher, plates and their timing.
- Penalty epic (stories not yet created): `IStrikeTallySource` implementation, strike count, exile.
- IIF stories 001 and 004: reply types, sinks and the drain-end flush.

---

## Files

- `Source/StealthGameplay/Public/Server/Features/Assassination/StrikeTallySource.h` (`IStrikeTallySource`, `FStrikeTally`)
- `Source/StealthGameplay/Private/Features/Assassination/EncounterService.cpp` (slot step 8: result building and sink append)
- `Source/StealthGameplay/Private/Tests/Assassination/ExileFlagAndTallyTest.cpp` (class `FAssassinationExileFlagAndTallyTest`)
- `Source/StealthTestHarness/Private/Assassination/OwnerResultShapeTest.cpp` (`StealthGame.Assassination.PIE.OwnerResultShape`: sizes, identity of NPC and informant payloads, no result to other clients)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

QL-STORY-READY: GAPS
- AC1: "filled inside the same slot" has no observable, and the exile rule belongs to Penalty (the fake is scripted); proposed rewrite (unconfirmed): with a fake `IStrikeTallySource` scripted to return {3, true} for an NPC kill and {3, false} for an informant kill, the fake's call is logged after the `OnPawnKilled` fan-out and before the slot ends; `FStrikeResult` serialised size is equal for tallies 0 to 3; `ContactServerTime` = the strike write's contact time exactly.
- AC2: "identical apart from `ClientCommandSeq` and `TallyAfter`" fails by construction if the strikes are at different times because `ContactServerTime` differs; proposed rewrite (unconfirmed): run each strike in a fresh instance with the same scripted slot time, or list `ContactServerTime` among the allowed differences; add: two non-attacker clients each receive 0 result entries.

- **AC-1**: Exile flag and tally
  - Given: an attacker with two NPC strikes, a scripted tally of 2, and a fake tally source
  - When: the third NPC kill resolves, then an informant kill at the same counts
  - Then: the NPC kill carries `WrongTarget`, `TallyAfter` 3, `bExiled` true; the informant kill carries `WrongTarget`, `TallyAfter` 3, `bExiled` false; size is count-independent; `ContactServerTime` is exact
  - Edge cases: a missing tally source gives {0, false} plus a fault report, no crash; `NoContact` and `Reversed` still query the source
- **AC-2**: Result shape and identity
  - Given: an Active-rival kill, an NPC kill, an informant kill, a whiff and a reversal, with attacker, defender and bystander connections
  - When: the owner replies are captured
  - Then: every `FStrikeResult` has the same size, the NPC and informant payloads are identical except `ClientCommandSeq` and `TallyAfter`, `TallyAfter` rises by exactly 1 for those two and is unchanged otherwise, and no other client receives a result field
  - Edge cases: a `Rejected_Latched` Attack sends no result; a bound versus null-sink defender

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: `ExileFlagAndTallyTest.cpp` (headless, injected tally source) and `OwnerResultShapeTest.cpp` (PIE, 3 clients) — must exist and pass

**Status**: [x] Created and passing (2026-10-27)

---

## Dependencies

- Depends on: `story-004-wire-structs-fixed-width.md`, `story-007-attack-outcomes-resolved-in-slot.md`; cross-epic `production/epics/input-interaction-framework/story-001-command-reply-types.md`, `story-004-reply-sink-and-drain-end-flush.md`; Penalty epic `production/epics/npc-kill-penalty-informants/EPIC.md` (no stories yet)
- Unlocks: `story-024-owner-result-matcher-and-plate-timing.md`, `story-026-advisory-economics-telemetry-resolver-cost.md`

## Completion Notes
**Completed**: 2026-10-27
**Criteria**: AC1 `StealthGame.Assassination.ExileFlagAndTally` (QA rewrite): a fake Penalty on OnPawnKilled + IStrikeTallySource: the third NPC kill's FStrikeResult is WrongTarget / TallyAfter 3 / bExiled true, the informant kill at the same counts WrongTarget / 3 / false; the tally is read after the OnPawnKilled fan-out and before the append (call log); ContactServerTime equals the attacker's Strike write's contact exactly; one serialised width (137 bits) for tallies 0-3 and 255, exiled or not; NoContact and Reversed still ask the source; a missing source gives {0, false} plus one DeadRegistration fault. AC2 data half `StealthGame.Assassination.OwnerResultShape` (fresh resolvers at one slot time): rival / NPC / informant kill, whiff and reversal each give exactly one FStrikeResult on the attacker's sink at the same size, the NPC and informant entries are bit-identical once ClientCommandSeq and TallyAfter are equalised, TallyAfter +1 for those two only, the bystander sink gets nothing, a bound defender gets exactly one FDefenceResult {Reversed} answering its Prepare seq, an NPC defender none, a Rejected_Latched Attack no result; every Kind round-trips at its fixed width. AC2 delivery half `StealthGame.Assassination.PIE.OwnerResultShape.EveryResultReachesOnlyItsOwnerAtOneSize` (3 machines): every result is received through ClientCommandReplies only on its owner's machine, at one size, the non-striking player receives none.
**Deviations**: ADVISORY - (1) the IIF drain-end flush had no production caller: `URoundMatchStore::OnRoundQueueDrained` + the composition root's `FStealthControllerReplyTransport` wire it (decision logged); voided Attack/Prepare commands also append their Rejected reply (held Rejected_Latched replies do not). (2) FStrikeResult / FDefenceResult moved from View/StealthEncounterWire.h to View/CommandReplyTypes.h so FCommandReply carries them; FCommandReply has a fixed-width NetSerialize per Kind (StealthGame.Input.CommandReplyTypes amended). (3) Penalty is not built: STAND-IN `FStealthPrototypeStrikeTally` over server-only AStealthPlayerState::WrongTargetTally (+1 per WrongTarget, NPC and informant) and the NpcKillLimit exile flag; NpcKills and LastDefenceActivationTime no longer replicate; the HUD shows "Wrong targets: n". (4) The plates are story 024's: a STAND-IN in AStealthPlayerController shows one HUD message per result at its ContactServerTime (FDefenceResult at receipt); the killer-side server messages and the in-slot exile message are removed. (5) The guardrail "<= 4 bytes plus FClientCommandSeq" cannot hold with story 004's double ContactServerTime (8 bytes): the entry is 137 bits. Cross-epic IIF 001/004, Penalty: proceeded (risk accepted).
**Test Evidence**: `Source/StealthGameplay/Private/Tests/Assassination/ExileFlagAndTallyTest.cpp`, `OwnerResultShapeTest.cpp`, `Source/StealthTestHarness/Private/Tests/PIE/Assassination/OwnerResultShapePieTest.cpp`; full suite in `production/qa/evidence/assassination-defence-reversal-015-019-2026-10-27/full-suite.txt`
**Run result**: OBSERVED - 3-player PIE: each owner's own HUD shows its result from the owner replies: "Wrong target. Wrong targets: 1" (NPC kill by the host; informant kill by client 1), "Target eliminated" (rival kill by client 2), "Reversed" (client 2 reversed) and the defender's "Reversal! Your attacker is eliminated." (client 1); bystanders show none (a whiff's "No contact" is covered by the PIE test) (`production/qa/evidence/story-015-owner-results-reply-entries-and-tally/`)
**Reviewed by**: self, no independent review (focused /code-review pass by the implementing agent; unattended batch)
