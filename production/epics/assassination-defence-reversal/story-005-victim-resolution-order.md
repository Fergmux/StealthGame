# Story 005: Victim resolution order, ally exclusion and dead bodies (rules 1, 3 and 4)

> **Epic**: Assassination & Defence Reversal
> **Status**: Complete
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: L
> **Manifest Version**: 2026-10-25f
> **Last Updated**: 2026-10-27

## Context

**GDD**: `design/gdd/assassination-defence-reversal.md` (ASN-R2, ASN-R8, ASN-R9; Formulas F4; Edge Cases "Targets"; AC-ASN-08, AC-ASN-09, AC-ASN-17)
**Requirement**: `TR-asn-001`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0018: Encounter data and outcome presentation timing (§2.1 step 1, §4.1)
**ADR Decision Summary**: Victim resolution and the outcome are computed at the Attack's queue slot from live server state; `UEncounterService` keeps a strike-role table whose `Dead` role excludes a pawn from every candidate set, including one whose death write is still pending.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0018 overall; this story is server logic over capsule distances and Roster queries)
**Engine Notes**: No post-cutoff API. Candidate distances use server capsule axes (horizontal), never bones; ties break on the pawn spawn serial, a value every pawn has (never kind or team). The `CanViewerKnow` query is the Visibility Policy's (ADR-0003 default-deny).

**Control Manifest Rules (this layer)**:
- Required: The resolver reads only live state at its slot and has no controller, binding or kind term — source: ADR-0018, ADR-0002
- Required: A `PawnRef` is honoured only through `CanViewerKnow(Attacker, Pawn, Position)` and `PawnRefAgreementCm`; a culled or forged ref is identical to a `TargetSpot` at the same point — source: ADR-0011, ADR-0003
- Forbidden: Never let a reply, timing or presentation depend on a target's kind, team, concealment or brace state; never fall through from a named `Dead` pawn to a spot rule — source: ADR-0011, ADR-0018
- Guardrail: Candidate evaluation is O(pawns) at most a handful of capsule tests, inside the 0.5 ms queue line

The candidate set excludes the attacker, every pawn with `GetRelation(Attacker, Pawn) = Teammate`, every pawn for which `ResolveVictimKind` returns `NotTargetable`, and every pawn whose strike role is `Dead`. Then first match wins: (1) an honoured `PawnRef`, if it passes reach, else a whiff, and a `PawnRef` naming a `Dead` pawn is a terminal whiff; (2) the bush spot rule (story 006); (3) the open spot: non-concealed candidates within `r_c + OpenSpotToleranceCm` of the `WorldPoint` that pass reach, nearest wins, ties go to the lower spawn serial; (4) otherwise a whiff. This story builds rules 1, 3 and 4 and the candidate filter; rule 2 plugs in from story 006.

---

## Acceptance Criteria

*From GDD `design/gdd/assassination-defence-reversal.md` (AC-ASN-08, AC-ASN-09, AC-ASN-17), scoped to this story:*

- [x] **AC1** (AC-ASN-08, [Logic/BLOCKING] `StealthGame.Assassination.VictimResolutionOrder`): **GIVEN** a visible pawn named by an honoured `PawnRef` and a second pawn nearer the `WorldPoint` **THEN** the named pawn is the victim; **GIVEN** the same `PawnRef` culled for the attacker or 76 cm from the `WorldPoint` **THEN** the outcome equals a `TargetSpot` at that point; **GIVEN** an open spot with pawns whose axes are 109 cm and 111 cm from the point **THEN** only the first is a candidate; **GIVEN** two candidates equidistant **THEN** the lower spawn serial is the victim, in both spawn orders of kind (NPC first, player first).
- [x] **AC2** (AC-ASN-09, [Logic/BLOCKING] `StealthGame.Assassination.AlliesNeverVictims`): **GIVEN** a forged `PawnRef` naming the attacker's ally with a rival 50 cm behind it **THEN** the rival is the victim; with no rival **THEN** `NoContact`; **GIVEN** a bush holding only allies **THEN** `NoContact`; no case returns a distinct reply or timing.
- [x] **AC3** (AC-ASN-17, [Logic/BLOCKING] `StealthGame.Assassination.DeadBodyIsNoContact`): **GIVEN** a dying NPC body and a dying player body (strike role `Dead`, one with its death write still pending) **WHEN** each is struck by `PawnRef` and by `TargetSpot` with no other pawn near **THEN** all four are `NoContact`, no `OnPawnKilled` fires and no strike is charged; **AND GIVEN** a live NPC 50 cm beside each body **THEN** the `PawnRef` strikes are still `NoContact` (terminal; the neighbour is not struck), while a `TargetSpot` strike resolves exactly as if the body were absent, byte-identically for the NPC and the player body.

---

## Implementation Notes

*Derived from ADR-0018 §2.1, §4.1 and GDD ASN-R8, ASN-R9, F4:*

Order inside the resolver: (a) build candidates; (b) rule 1: a `PawnRef` is honoured iff `CanViewerKnow(Attacker, Pawn, Position)` and the pawn lies within `PawnRefAgreementCm` of the `WorldPoint` (IIF R10; IIF stories 061 to 063 own the gate); an honoured ref returns that pawn if it passes F1 reach (story 002) else a whiff; a ref naming a `Dead` pawn is a terminal whiff; (c) rule 2 via story 006; (d) rule 3: non-concealed candidates only (a concealed pawn is a candidate only through rule 2), horizontal capsule-axis distance `≤ r_c + OpenSpotToleranceCm` (default 75 + 35 = 110 cm, so 109 qualifies and 111 does not), reach filter, nearest wins, exact ties to the lower pawn spawn serial; (e) otherwise a whiff.

A forged ally `PawnRef` falls through to the spot rules with allies excluded (ASN-R9); the reply path and timing are identical, so no reply differs. The strike role table lives in `UEncounterService`; a role becomes `Dead` in the kill slot (story 007), before the death write lands, which is what makes AC3's "pending death write" case terminal. The dead-body `TargetSpot` result must be byte-identical for an NPC and a player body: assert the full result struct equality in the test, not only the enum.

**Prototype replaced:** the prototype's target selection (`FindAssassinationTarget`, the cone and `BlindStrikeRadius` in `AStealthGameMode::HandleAssassinationRequest`) is replaced; `FStealthRules::ResolveAssassination`'s `Teammate → Rejected` row becomes `NoContact` here (no target-dependent rejection, story 008).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-006-bush-spot-rule-and-victim-pick.md`: rule 2, the terminal bush rule and the `BushSpotPick` draw.
- `story-007-attack-outcomes-resolved-in-slot.md`: turning a victim into a kill, reversal or whiff and the in-slot effects.
- IIF stories 061 to 063: the `PawnRef` gate and stale-ref fallback.
- Roster stories 013 and 032: `ResolveVictimKind` and `GetRelation` themselves.

---

## Files

- `Source/StealthGameplay/Private/Features/Assassination/VictimResolverCore.h` and `.cpp` (`FVictimResolverCore`, plain C++, injected roster and visibility facades)
- `Source/StealthGameplay/Public/Server/Features/Assassination/EncounterService.h` (strike-role table, `IsDead`)
- `Source/StealthGameplay/Private/Features/Assassination/EncounterService.cpp`
- `Source/StealthGameplay/Private/Tests/Assassination/VictimResolutionOrderTest.cpp` (class `FAssassinationVictimResolutionOrderTest`)
- `Source/StealthGameplay/Private/Tests/Assassination/AlliesNeverVictimsTest.cpp` (class `FAssassinationAlliesNeverVictimsTest`)
- `Source/StealthGameplay/Private/Tests/Assassination/DeadBodyIsNoContactTest.cpp` (class `FAssassinationDeadBodyIsNoContactTest`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

QL-STORY-READY: GAPS
- AC1: positions are not given, so reach could fail for another reason, and "is a candidate" is internal state; proposed rewrite (unconfirmed): attacker at origin facing +X, all pawns inside F1 reach; named pawn at 150 cm and a second pawn 20 cm from the `WorldPoint`; a culled ref, or a ref 76 cm from the point (`PawnRefAgreementCm` 75), yields a result struct equal to the `TargetSpot` strike at the same point; pawns 109 cm and 111 cm from the point: the 109 cm pawn is the victim and the 111 cm pawn is never struck; equal distances: lower spawn serial wins in both spawn orders.
- AC2: "behind it" has no frame and "no distinct reply or timing" has no comparator; proposed rewrite (unconfirmed): the rival is 50 cm further along the attacker-to-ally line; the full reply struct and reply server tick are byte-identical across the ally-ref, no-rival and allies-only-bush cases.
- AC3: "no strike is charged" needs an observable and the `TargetSpot` result is unnamed; proposed rewrite (unconfirmed): a Penalty stub spy records zero strike calls and zero `OnPawnKilled`; with a live NPC 50 cm beside the body the `TargetSpot` result struct equals that of the same strike with the body removed, byte-identically for NPC and player body.

- **AC-1**: Resolution order (rules 1, 3, 4)
  - Given: attacker at origin facing +X; pawns in reach; `OpenSpotToleranceCm` 75, `PawnRefAgreementCm` 75, r_c 35
  - When: strikes use an honoured ref, a culled ref, a ref 76 cm from the point, open-spot pawns at 109 and 111 cm, and an equidistant pair
  - Then: the named pawn wins over a nearer one; a culled or 76 cm ref equals the `TargetSpot` result; only the 109 cm pawn is struck; ties go to the lower serial in both orders
  - Edge cases: 110 cm inclusive boundary; a ref at exactly 75 cm is honoured; a ref failing reach is a whiff, not a fall-through
- **AC-2**: Allies are never victims
  - Given: a forged `PawnRef` naming an ally with a rival 50 cm beyond it; separate no-rival and allies-only-bush cases
  - When: each is resolved
  - Then: the rival is the victim; the others return `NoContact`; reply struct and reply tick are identical in every case
  - Edge cases: a team-0 and a team-1 ally; the forged ref is indistinguishable from a spot strike
- **AC-3**: Dead bodies are `NoContact`
  - Given: a dying NPC and a dying player (role `Dead`, one with a pending death write), alone and with a live NPC 50 cm beside
  - When: each is struck by `PawnRef` and by `TargetSpot`
  - Then: the four lone strikes are `NoContact` with zero `OnPawnKilled` and zero strike charges; with a neighbour the `PawnRef` strikes stay `NoContact` and the neighbour is untouched; `TargetSpot` equals the body-removed run, byte-identically for NPC and player
  - Edge cases: full struct equality, not enum only; the pending-death-write case is terminal

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: the three test files above — must exist and pass

**Status**: [x] Created and passing (2026-10-27)

---

## Dependencies

- Depends on: `production/epics/assassination-defence-reversal/story-002-reach-f1-pure-function.md`; cross-epic `production/epics/participant-state-roster/story-013-resolve-victim-kind-matrix.md`, `story-032-get-relation-matrix.md`; `production/epics/input-interaction-framework/story-061-pawnref-same-as-spot.md`, `story-063-pawnref-gate.md`; `production/epics/information-visibility-policy/story-010-query-default-deny.md`
- Unlocks: `story-006-bush-spot-rule-and-victim-pick.md`, `story-007-attack-outcomes-resolved-in-slot.md`

## Completion Notes
**Completed**: 2026-10-27
**Criteria**: 3/3 passing (AC1 honoured ref beats a nearer pawn; a culled ref and a ref 76 cm away equal the TargetSpot result as full FVictimResolution structs; 75 cm honoured; 109 cm a candidate, 111 never, 110 inclusive; ties to the lower spawn serial with NPC first and with player first; an honoured ref out of reach (dz 100) is a whiff, never a fall-through. AC2 a forged ally ref falls through: the rival 50 cm beyond the ally is killed; no rival NoContact; allies-only bush NoContact; replies identical (Executed, same tick) for team 0 and team 1; the forged ref resolves equal to the spot strike. AC3 an NPC body (death write pending) and a player body (also Roster AlreadyResolved) struck by PawnRef and TargetSpot: NoContact, no OnPawnKilled, the Penalty stub's strike count unchanged; with a live NPC 50 cm beside, the PawnRef stays NoContact and the TargetSpot result equals the body-removed probe and is identical for NPC and player bodies) - `StealthGame.Assassination.VictimResolutionOrder`, `.AlliesNeverVictims`, `.DeadBodyIsNoContact`
**Deviations**: ADVISORY - files live in `Features/Assassination/{Public/Server,Private}` (the ADR-0007 feature folder, as asn-002/003), not the story's `Private/Features/Assassination`. Victim identity is an opaque `APawn*` with facts from `IEncounterPawnQuery`; the production spawn serial is a stand-in (UObject unique id) until SCL/ADR-0010 publishes one. `PawnRefAgreementCm` is measured horizontally on capsule axes; the Dead-ref terminal rule applies once a ref is honoured (an unhonoured ref always falls through, so it equals a TargetSpot). Cross-epic Roster 013/032, IIF 061/063 and IVP 010 not Complete: proceeded with consumer-owned seams (`IEncounterRosterGateway`, `IEncounterVisibilityQuery`; a null visibility query is default-deny; the world stand-in answers knowable unless concealed). Decisions logged.
**Test Evidence**: `VictimResolutionOrderTest.cpp`, `AlliesNeverVictimsTest.cpp`, `DeadBodyIsNoContactTest.cpp` under `Source/StealthGameplay/Private/Tests/Assassination/` (shared rigs `EncounterTestRig.h`, `EncounterRoundRig.h`); run in `production/qa/evidence/assassination-defence-reversal-005-011-2026-10-27/full-suite.txt`
**Run result**: N/A - server resolver logic, not yet registered in play (the composition-root wiring and the prototype-bridge removal are stories asn-012/013), so nothing player-visible changes in this story
**Reviewed by**: self, no independent review (focused /code-review pass by the implementing agent; unattended batch)
