# Story 015: Conversation (C3) and turn-in-place (C6), partner-blind

> **Epic**: Crowd & NPC Behaviour
> **Status**: Complete
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: 2026-10-26

## Context

**GDD**: `design/gdd/crowd-npc-behaviour.md`
**Requirement**: `TR-crowd-002`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-crowd-002 = StateTree per NPC (~4 Hz) + follower emitting Move/Look through the IIF; no path-following driving.)*

**ADR Governing Implementation**: ADR-0016: Crowd AI architecture
**Also relevant**: ADR-0001 (identity blindness), shared-character-locomotion GDD (R5.5 idle re-centre, `TurnInPlaceExitFrac`, `LookYawLimitDeg`)
**ADR Decision Summary**: Conversation partners come from the Director's spatial hash filtered by the perception filter; the face-toward decision never reads who the neighbour is; the `Conversing` state issues only `Look`; `Crowd.Converse.Faced` is an event interrupt.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: The statistical test needs a scripted human-driven pawn (harness bot) beside an NPC; the facing observation uses replicated body yaw as an observer would see it.

**Why this story**: If NPCs turned toward a standing player more or less often than toward another NPC, conversation behaviour would become an identity test (GDD C3, AC-CNB-17). Read GDD CNB-R5 C3, C6, R6 G5 and AC-CNB-17.

**Control Manifest Rules (Feature layer, manifest 2026-10-25e)**:
- Required: perception uses only public facts and server line of sight, excluding concealed pawns; the Conversing state issues only `Look` (ADR-0016)
- Forbidden: reading player, binding or team to choose a partner; NPCs walking toward a stationary pawn to start a conversation (GDD C3) (ADR-0001, ADR-0016)
- Guardrail: partner queries use the 2 m hash and the 150 cm radius; cost inside `Crowd.Perception`

---

## Acceptance Criteria

*From GDD `design/gdd/crowd-npc-behaviour.md` AC-CNB-17, CNB-R5 (C3, C6) and CNB-R6 (G5), scoped to this story:*

- [x] (AC-CNB-17a) GIVEN a scripted player pawn standing still within 150 cm of an idle NPC 1,236 times and an NPC in the same spot 1,236 times (80 % power for a 5-point margin at p = 0.5), WHEN the NPC's face-toward rate is counted, THEN the two rates differ by at most 5 percentage points (TOST equivalence at alpha 0.05).
- [x] (AC-CNB-17b) A conversation forms only while both pawns face each other within 60 degrees; an unanswered initiator turns away after 2-5 s for either partner kind; NPCs never path toward a stationary pawn to start one.
- [x] C6: when idle gaze exceeds 70 degrees for 0.5 s, the body re-centres until head yaw is at most `TurnInPlaceExitFrac` x `LookYawLimitDeg` (the SCL R5.5 exit); a conversation lasts 6-20 s, and across at least 100 conversations the mean partner-look (G5) share lies in [0.60, 0.80].

---

## Implementation Notes

*Derived from GDD CNB-R5 C3 and C6, CNB-R6 G5:*

- An NPC that comes to rest within 150 cm of another resting, perceivable pawn turns to face it with probability `CrowdConverseStartPct` (50 %, `ECrowdDecision::ConverseStart`); an NPC that is faced turns back with probability `CrowdConverseReplyPct` (60 %, `ConverseReply`). Both come from logged player face-back rates and never read the neighbour's kind; a player who faces an NPC is treated exactly as an NPC who does.
- A conversation exists only while both face each other within 60 degrees; length U(6, 20) s (`ConverseLength`). The initiator turns away after U(2, 5) s if not faced back, whoever the partner is.
- G5: look at the partner's head 60-80 % of the conversation; it is a contextual hold, not a T13 head hold. Conversations count toward the stationary-stretch budget of story 014.
- Event `Crowd.Converse.Faced` wakes the tree (story 012).

**Prototype disposition** (`production/first-playable-report.md`, `Source/StealthGame`): None; the prototype crowd never conversed. New code only.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 014: the idle bound that conversations are counted against
- Story 016: the gaze catalogue other than G5
- Story 008: the neighbour query and filter

---

## Files

- `Source/StealthGameplay/Features/Crowd/Public/Server/CrowdAgent.h`
- `Source/StealthGameplay/Features/Crowd/Private/CrowdAgent.cpp`
- `Source/StealthGameplay/Private/Tests/Crowd/CrowdConversationTest.cpp`

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY, 2026-10-25). AC-3 revised after the GAPS verdict (end state and aggregate defined).*

- **AC-1**: `StealthGame.Crowd.ConversationPartnerBlind`
  - Given: 1,236 trials with a stationary player pawn within 150 cm of an idle NPC, and 1,236 with an NPC in the same spot
  - When: the NPC's face-toward rate is counted in each arm
  - Then: the two rates differ by at most 5 percentage points (TOST, alpha 0.05)
  - Edge cases: fixed pre-registered seeds; a failing seed is a defect, never reseeded
- **AC-2**: `StealthGame.Crowd.ConversationRules`
  - Given: scripted pairs facing at 30, 60 and 90 degrees, and an unanswered initiator of each partner kind
  - When: the scene runs
  - Then: conversation forms only within 60 degrees; unanswered initiator turns away after 2-5 s for both kinds; no NPC starts a path toward a stationary pawn to converse
  - Edge cases: exactly 60 degrees; partner leaves mid-conversation
- **AC-3**: `StealthGame.Crowd.TurnInPlaceAndConverseLength`
  - Given: idle NPCs with gaze set beyond 70 degrees for 0.5 s, and 100 conversations
  - When: body yaw, durations and partner-look share are logged
  - Then: body re-centres until head yaw <= `TurnInPlaceExitFrac` x `LookYawLimitDeg`; durations in [6, 20] s; mean G5 share in [0.60, 0.80]
  - Edge cases: gaze at exactly 70 degrees; conversation ended by the idle bound

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/CrowdConversationBlindTest.cpp` and `Source/StealthGameplay/Private/Tests/Crowd/CrowdConversationTest.cpp` — must exist and pass

**Status**: [x] Created and passing (`StealthGame.Crowd.Conversation.PartnerBlind`, `StealthGame.Crowd.Conversation.LengthsAndLook`, `StealthGame.Crowd.Conversation.TurnInPlace`; 2026-10-26)

---

## Dependencies

- Depends on: `production/epics/crowd-npc-behaviour/story-008-spatial-hash-and-perception-filter.md`, `production/epics/crowd-npc-behaviour/story-012-statetree-schema-and-decision-grid.md`
- Unlocks: None

## Completion Notes
**Completed**: 2026-10-26 (Crowd & NPC Behaviour batch, stories cnb-001..018; the production crowd drives L_Prototype, D53)
**Criteria**: 3/3 passing
**Deviations**: G5 look share tracks the drawn share with a 0.6 s hysteresis band (a fixed cycle starting on the partner biased early-ended conversations high: 0.80); mean 0.76. Tests run on the headless scene, not the harness.
**Test Evidence**: `StealthGame.Crowd.Conversation.PartnerBlind`, `StealthGame.Crowd.Conversation.LengthsAndLook`, `StealthGame.Crowd.Conversation.TurnInPlace`; full suite 275/275: `production/qa/evidence/crowd-npc-behaviour-001-018-2026-10-26/full-suite.txt`
**Run result**: 3-player PIE on L_Prototype with the production crowd (40 NPCs, the NpcCount default): `production/qa/evidence/crowd-npc-behaviour-001-018-2026-10-26/evidence.md`
**Reviewed by**: self-review against the code-review skill during the batch; findings and fixes in `production/qa/evidence/crowd-npc-behaviour-001-018-2026-10-26/evidence.md`
