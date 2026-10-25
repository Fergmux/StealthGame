# Story 090: AC 50 TellMatrixRows

> **Epic**: Input & Interaction Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/input-interaction-framework.md`
**Requirement**: `TR-iif-???` — no entry in `docs/architecture/tr-registry.yaml` matches. This is a design-document review criterion (this GDD against Crowd & NPC Behaviour's Baseline and Tell Matrix), not a runtime requirement; none of TR-iif-001 to TR-iif-013 states it.
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0011: Pawn command interface and input pipeline
**ADR Decision Summary**: ADR-0011 section 10 has the crowd controller call the same entry points through the server-local mailbox with a null sink, and states that NPCs never issue `Attack` as a Crowd behaviour constraint (not enforced by a kind check in the IIF); what NPCs do as red herrings (brace, tend, bush) and with what visible deviation is Crowd's Baseline and Tell Matrix.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: No engine API and no code: a document review. Risk is inherited from ADR-0011; no V-item gates this story.

**Control Manifest Rules (this layer)**:
- Required: AI commands use the same entry points under the same rules, subject to every stage-2 step including the token buckets; no exemption — source: ADR-0011
- Forbidden: Never carry caller identity (`IsAI`, `IsPlayer`, controller, PlayerState, team) in an entry point, payload, event or listener — source: ADR-0011

---

## Acceptance Criteria

*From GDD `design/gdd/input-interaction-framework.md`, scoped to this story:*

- [ ] AC 50 (Review, deferred gate `TellMatrixRows`): **GIVEN** Crowd & NPC Behaviour's Baseline and Tell Matrix **WHEN** this GDD is reviewed against it **THEN** rows exist for brace, objective interaction, bush enter/leave and assassination, each stating the visible deviation and whether NPCs use it as a red herring. This cannot be checked until the Crowd GDD exists.

---

## Implementation Notes

*Derived from GDD AC 50, the Crowd GDD `design/gdd/crowd-npc-behaviour.md` (CNB-R16 "Baseline and Tell Matrix", line 114) and ADR-0011 Decision section 10:*

**What this story is.** A document review, not code. The deferred gate of AC 50 was waiting for the Crowd GDD; the Crowd GDD now exists (plan note), so the gate can be run. Type stays Integration per the plan, with this note: the evidence is a review record at `production/qa/evidence/iif-tell-matrix-review.md`, because the qa-lead flagged that no evidence class of the Testing Standards table fits a document review (Integration requires an integration test or playtest; the nearest class, Config/Data, is advisory and needs a smoke check). The qa-lead proposes either reclassifying (Config/Data, advisory, evidence = signed review record in `production/qa/`) or adding an explicit rule; this story follows the plan and records the verdict in the review file, signed by the game-designer with the date.

**Review checklist (qa-lead).** Read CNB-R16 in `design/gdd/crowd-npc-behaviour.md` and verify: (1) a row for brace (T4); (2) a row for objective interaction (T3); (3) a row for bush enter/leave (T5); (4) a row for assassination (T6); (5) each row has a non-empty Visible deviation cell; (6) each row's "NPCs do it?" cell says Yes or No explicitly (T6 No, the others Yes); (7) rows point to an owning AC (an IIF-owned row cites IIF). All seven true is a pass; any missing row or empty cell fails and goes back to Crowd's author. The IIF GDD's own side of the contract is its Dependencies note (Crowd & NPC Behaviour: the Tell Matrix must cover NPC brace, tend and bush use and AI brace-duration distributions) and ADR-0011 section 10 (NPCs never issue `Attack`; a participant-bound AI drive issues only `Move` and `Look`).

**Where the record goes.** `production/qa/evidence/iif-tell-matrix-review.md`: result per checklist item, the CNB-R16 line numbers read, reviewer, date. If a row is missing the story fails; do not edit the Crowd GDD from this story (cross-domain write).

**Prototype.** New code, no prototype counterpart (no code is written).

**Tuning keys (defaults).** None.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/input-interaction-framework/story-032-ai-no-exemption.md`: AI is subject to the same token buckets as humans.
- `production/epics/input-interaction-framework/story-082-action-pose-parity-observed.md`: observed ActionPose parity of NPC and player brace.
- `production/epics/input-interaction-framework/story-030-attack-never-rejected-by-target.md`: Attack is never rejected by the target.
- Editing `design/gdd/crowd-npc-behaviour.md`: a deficiency found goes back to the Crowd author.

---

## Files

- `production/qa/evidence/iif-tell-matrix-review.md` — the signed review record (the only file this story produces; no source files)

---

## QA Test Cases

*Test cases not yet defined — run /qa-plan to generate them.*

*QL-STORY-READY: GAPS (qa-lead):*
- Gap: the story type is Integration, which requires an integration test or playtest; a document review is neither. Reclassify (proposal: Config/Data, advisory, evidence = signed review record in `production/qa/`) or add an explicit rule. Also state where the verdict is recorded and who signs (game-designer).

*Draft checklist from the qa-lead (unconfirmed), Manual check "Tell Matrix rows" (review of `design/gdd/crowd-npc-behaviour.md` CNB-R16):* Setup: the Crowd GDD exists; read the CNB-R16 table. Verify checklist: (1) a row for brace (T4); (2) a row for objective interaction (T3); (3) a row for bush enter/leave (T5); (4) a row for assassination (T6); (5) each row has a non-empty Visible deviation cell; (6) each row's "NPCs do it?" cell says Yes or No explicitly (T6 No, others Yes); (7) rows point to an owning AC (an IIF-owned row cites IIF). Pass condition: all 7 true; any missing row or empty cell fails and goes back to Crowd's author; record the result with date.

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: a documented review record at `production/qa/evidence/iif-tell-matrix-review.md` (no automated test applies; the qa-lead flagged that no evidence class fits a document review)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: None (cross-epic: crowd-npc-behaviour GDD `design/gdd/crowd-npc-behaviour.md` exists; its epic stories are not yet written)
- Unlocks: None
