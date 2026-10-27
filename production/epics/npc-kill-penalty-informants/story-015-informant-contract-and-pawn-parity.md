# Story 015: Informant contract, pawn parity and the owner-AC trace

> **Epic**: NPC-Kill Penalty & Informants
> **Status**: Blocked — BLOCKED: the owners of three PEN-R12 rows are not built: Traverse / Interact / Hide NotNow for the Informant legality class (input-interaction-framework story 057), no x-ray (IVP viewer class OpponentInformant, concealment AC-CON-04) and the concealed ally's marker (Teammate Markers AC-TMK-16a). Every built row passes (InformantPawnParity, InformantContract.EveryBuiltRowHolds); AC3 trace done.
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25f
> **Last Updated**: 2026-10-27

## Context

**GDD**: `design/gdd/npc-kill-penalty-informants.md`
**Requirement**: `TR-pen-007`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0019: Informant lifecycle and crowd-pawn takeover (§5, the informant restriction enforcement map), with ADR-0010 (gait clamp, possession parity) and ADR-0011 (legality class `Informant`)
**ADR Decision Summary**: Penalty adds no restriction of its own (an informant-only rule would itself be a tell). Each informant restriction is enforced by its owner — walk-only gait clamp (ADR-0010 via `IGaitLegalitySource`), `NotNow` for run/traverse/interact/brace/attack/hide (ADR-0011 legality class `Informant`), no x-ray (ADR-0001/0003/0017: viewer class `OpponentInformant ≡ Opponent`), allies' markers (C-TeamView), `CountActive` excludes informants (Roster) — and has a named test. Tell Matrix T7: an informant's body is never seen running, vaulting, climbing, interacting, bracing or hiding, and its replication, collision, capsule and avoidance equal any other Shared Pawn.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Risk is the highest of the governing ADRs: ADR-0010 (possession and CMC parity) is HIGH, ADR-0019 MEDIUM. Spikes S1 and S14 reported 2026-10-25 (`production/qa/spikes/2026-10-25_S1-S2-S14.md`); the prototype showed whole-pawn parity differences (`RemoteViewPitch16`, `CharacterInputState`, `TraversalResult`) that the production pawn must remove before AC1's dump comparison can pass. `[Provisional: spikes S1/S14]` on AC-NKP-17 is read as "re-run on production code", not Blocked, because both spikes reported.

**Control Manifest Rules (this layer)**:
- Required: informant restrictions are enforced only by the owners in ADR-0019 §5, each with a named test — source: ADR-0019
- Forbidden: never add an informant-only movement, collision, sight or region rule; never have Penalty call `BindPawn` itself — source: ADR-0019
- Guardrail: parity checks run on the production pawn class, not a test double — source: ADR-0008, ADR-0010

PEN-R12 is the informant role contract in one table, user-locked. Penalty owns no enforcement code, so this story is a *verification* story: it runs the contract end to end, dumps the pawn parity, and records the traceability matrix from each restriction to the owner's AC.

**Prototype replaced:** the prototype's informant restrictions in `AStealthCharacter` (speed cap, traversal/jump block by an `IsInformant()` check on the replicated `PlayerState`) and `FStealthRules` informant gates. The prototype reads the replicated `PlayerState` on every client, which the S14 audit flags as a leak; production enforces through the legality class and gait clamp. Nothing is promoted.

---

## Acceptance Criteria

*From GDD `design/gdd/npc-kill-penalty-informants.md` (AC-NKP-17, AC-NKP-29, AC-NKP-47), scoped to this story:*

- [ ] **AC1** (AC-NKP-17, [L][Dep:Crowd][Dep:SCL][Provisional: spikes S1/S14] `StealthGame.Penalty.InformantPawnParity`, Tell Matrix T7, Crowd AC-CNB-30): **GIVEN** an informant pawn and an NPC pawn **WHEN** their replication settings, collision profiles, capsule and avoidance settings are dumped **THEN** they are equal (SCL AC-SCL-16); **AND** the informant's `Gait=Run` is clamped to Walk while `Traverse`, `Interact`, `Prepare`, `Attack` and `Hide` are rejected `NotNow` (IIF R7) (PEN-R13).
- [ ] **AC2** (AC-NKP-29, [I][PIE-3] `StealthGame.Penalty.PIE.InformantContract`): **GIVEN** an embodied informant **WHEN** each PEN-R12 row is exercised (walk, run clamp, each rejected command, a concealed opponent in view, a concealed ally in view, the elimination count, being killed, the respawn) **THEN** every row holds (PEN-R12; the owner-AC trace is AC3).
- [x] **AC3** (AC-NKP-47, [Review/ADVISORY] informant contract trace): **GIVEN** PEN-R12 **WHEN** the GDDs named under "Enforced by" are reviewed **THEN** each row maps to an acceptance criterion in its owner's GDD, listed as (row, GDD, AC id) with no missing entry (PEN-R12).

---

## Implementation Notes

*Derived from ADR-0019 Decision §5 and PEN-R12/R13:*

AC1 is a Logic-style dump test that runs against the production `AStealthPawn` class with two instances (one bound to a participant in `Informant` state, one NPC) and compares a fixed field list: replication settings (net update frequency, priority, relevancy flags, `bAlwaysRelevant`, dormancy), collision profile name and responses, capsule radius/half-height, `bUseRVOAvoidance`/avoidance group settings and the `UCharacterMovementComponent` movement settings that affect observable behaviour. The expected-name list is checked in as a fixture (hand-derived from SCL R3, R13). It extends shared-character-locomotion `story-014-informant-restrictions.md` and `story-052-tell-matrix-rows.md` (those own the enforcement ACs; this story only calls them from Penalty's viewpoint and the clamp/`NotNow` assertions reuse their helpers). The `Gait=Run` clamp is asserted by sending the command through the real ingress and reading the effective gait; the five `NotNow` rejections go through `input-interaction-framework` `story-057-roster-legality-class.md`'s legality class.

AC2 is the PIE integration of the rows that need a real world: an informant walks (140 cm/s baseline), is clamped when asked to run, has each restricted command rejected, cannot see a concealed opponent (viewer class `OpponentInformant ≡ Opponent`), sees a concealed ally's marker, is excluded from `CountActive`, is killed (no strike for the killer, story 008) and respawns (story 012). Reuse the harness; write only the orchestration and assertions that are Penalty's (the informant lifecycle). 3v3 PIE with a fixed crowd seed.

AC3 is a review artifact: a table (row, owner GDD, owner AC id) written into the story's evidence file `production/qa/evidence/penalty-informant-contract-trace.md`, produced by reading the owners' GDD acceptance criteria. Advisory, never blocking, but a missing entry is a finding to report to the owning epic.

No Penalty production code is expected: any defect found is a defect in the owning module and is routed to its epic. Penalty must not patch it with an informant-only rule.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- shared-character-locomotion `story-014-informant-restrictions.md`: the gait clamp and restriction enforcement itself.
- input-interaction-framework `story-057-roster-legality-class.md`: the `Informant` legality class.
- `story-014-one-respawn-path-and-takeover-wire-parity.md`: takeover wire parity.
- information-visibility-policy and concealment-bushes: the no-x-ray viewer class and the cull.

---

## Files

*Files changed by /dev-story (2026-10-27).*

- `Source/StealthGameplay/Private/Tests/Penalty/PenaltyInformantPawnParityTest.cpp` (class `FPenaltyInformantPawnParityTest`, name `StealthGame.Penalty.InformantPawnParity`)
- `Source/StealthTestHarness/Private/Tests/PIE/Penalty/InformantContractPieTest.cpp` (CQTest `StealthGame.Penalty.PIE.InformantContract.EveryBuiltRowHolds`)
- `production/qa/evidence/penalty-informant-contract-trace.md` (AC3 trace table)

No Penalty production code (as planned). Rows whose owners are not built are NOT exercised and keep the story Blocked after the build: Traverse, Interact and Hide `NotNow` (input-interaction-framework story 057, Ready), a concealed opponent not visible and a concealed ally's marker visible (information-visibility-policy viewer class `OpponentInformant`, Teammate Markers, concealment-bushes AC-CON-04). The run clamp's legality source in PIE is still the prototype PlayerState adapter (iif-071).

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC1** (AC-NKP-17, `StealthGame.Penalty.InformantPawnParity`; Provisional S1/S14)
  - Given: an informant pawn and an NPC pawn of the production `AStealthPawn` class, plus a taken-over pawn against a control pawn.
  - When: replication settings, collision profiles, capsule and avoidance settings are dumped, then commands are sent through the real ingress.
  - Then: the dumps equal the checked-in expected-name fixture (SCL AC-SCL-16); `Gait = Run` becomes Walk; `Traverse`, `Interact`, `Prepare`, `Attack` and `Hide` are rejected `NotNow`.
  - Edge cases: retagged Integration (real pawns and legality class).
- **AC2** (AC-NKP-29, `StealthGame.Penalty.PIE.InformantContract`)
  - Given: an embodied informant in 3v3 PIE.
  - When: each PEN-R12 row is exercised.
  - Then: walk, look and camera toggle are accepted; run is clamped and the six commands rejected `NotNow`; a concealed opponent is not visible (as for an Active opponent); a concealed ally is visible; `CountActive` excludes the informant; a kill gives `VictimKind = Informant`, tally + 1 and no strike; the respawn is a takeover near the team spawn and the informant never returns to Active.
  - Edge cases: the pings row belongs to the Pings ACs.
- **AC3** (AC-NKP-47, informant contract trace, advisory review)
  - Given: the nine PEN-R12 rows.
  - When: the "Enforced by" GDDs are reviewed.
  - Then: `production/qa/evidence/penalty-informant-contract-trace.md` lists (row, GDD, AC id) with no blank entry and every id exists in the owner's GDD.
  - Edge cases: a missing entry is reported to the owning epic, never patched in Penalty.

QL-STORY-READY: ADEQUATE

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: the CQTest PIE test under `Source/StealthTestHarness/Private/Penalty/`, the unit test under `Source/StealthGameplay/Private/Tests/Penalty/` and the trace table in `production/qa/evidence/` OR a documented playtest

**Status**: created and passing 2026-10-27: `StealthGame.Penalty.InformantPawnParity` (dump parity, Run clamp, Attack and Prepare NotNow) and `StealthGame.Penalty.PIE.InformantContract.EveryBuiltRowHolds` (walk, run clamp, Attack and Prepare NotNow, CountActive, killed, respawn); AC3 trace `production/qa/evidence/penalty-informant-contract-trace.md`. AC1 and AC2 stay open for the rows named in Status. Evidence: `production/qa/evidence/npc-kill-penalty-informants-013-015-2026-10-27/evidence.md`.

Run result: OBSERVED — 3-player PIE: the informant walks at 140 cm/s, its Run is clamped (140 vs 450 for an Active player), its Attack and Prepare are answered NotNow, a killed informant gives the killer a WrongTarget with tally + 1 and respawns still an informant (PIE test log, and screenshots 03-05 of the evidence folder).

---

## Dependencies

- Depends on: `production/epics/npc-kill-penalty-informants/story-012-respawn-commit-sequence.md`, `story-008-informant-kill-inline-retire.md`; cross-epic: shared-character-locomotion `story-014-informant-restrictions.md`, `story-052-tell-matrix-rows.md`; input-interaction-framework `story-057-roster-legality-class.md`, `story-090-tell-matrix-rows.md`; crowd-npc-behaviour (AC-CNB-30); information-visibility-policy `story-041-tell-matrix-bush-row.md`
- Unlocks: None
