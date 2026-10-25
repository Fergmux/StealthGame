# Story 017: Ring labels and the label drop

> **Epic**: Informant Pings & Team Communication
> **Status**: Ready
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/informant-pings-team-comms.md` (PNG-R7 label rules, cross-review S-I1, edge case on unresolved name; AC-PNG-19)
**Requirement**: `TR-png-016`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0021: UI/HUD layering, CommonUI and Enhanced Input (§5 markers model and `OnAllyMarkerRetired`; ring label rules from the 2026-10-25b amendment)
**ADR Decision Summary**: The ring label is the sender's C-Public `DisplayName` resolved by `SenderHandle` ("Ally" if unresolved), dropped at that ally's `OnAllyMarkerRetired(PublicHandle)` (the same instant as the HUD removal line) while the ring stays to expiry; the sender's own screen keeps its own-name label. Presentation reaches data only through View interfaces from its own controller.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0021 overall; plain C++ in this story)
**Engine Notes**: No engine API. The label source is the C-Public `DisplayName` path used by Teammate Markers; no new replicated field and no `GetPlayerState` on a pawn (ADR-0007 lint rule (h)).

**Control Manifest Rules (this layer)**:
- Required: the ring label is the sender's C-Public `DisplayName` by `SenderHandle` ("Ally" if unresolved); dropped at `OnAllyMarkerRetired` while the ring stays to expiry — source: ADR-0021
- Forbidden: Never name a person with a ring: a ring marks a place, so after the retire event it has no label — source: GDD PNG-R7, art bible 7.x rule 2

---

## Acceptance Criteria

*From GDD `design/gdd/informant-pings-team-comms.md` (AC-PNG-19, PNG-R7), scoped to this story:*

- [ ] **AC1** (PNG-R7, unresolved name): **GIVEN** a ring **WHEN** its label is resolved **THEN** it is the sender's C-Public `DisplayName` by `SenderHandle`, "Ally" when unresolved (updating while the ring is showing if the name arrives), and the own name on the sender's own ring.
- [ ] **AC2** (AC-PNG-19, `[Logic]` `StealthGame.Pings.LabelDropAtRemoval`): **GIVEN** a live ring from an `Active` sender who is then eliminated **WHEN** the viewer's `OnAllyMarkerRetired` fires for that sender **THEN** from that tick the ring stays to expiry with no label, the sender's own screen keeps the own-name label, and no label is dropped without the retire event.
- [ ] **AC3** (PNG-R7): **GIVEN** the same sender later pings as an informant **THEN** that new ring carries its label again.

---

## Implementation Notes

*Derived from GDD PNG-R7 and ADR-0021 §5:*

Extend `FTeamPingRingModel` (story 016) with a label view-model field resolved each frame from an injected name lookup (the C-Public `DisplayName` accessor Teammate Markers already uses) and a `RetiredSenders` set fed by the `OnAllyMarkerRetired(PublicHandle)` delegate from the Teammate Markers model (TM-R14). The drop is per ring-sender pair: the set entry removes the label of every ring from that sender, live and future until that sender creates a new ring after the retire (an informant pinging after its removal is a new ring with a label). Order inside a tick: the retire event is processed before the model's label resolution so the label is gone from the same tick, not the next. Edge rings never carry a label (story 018).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 016: the ring list, age and replace rules.
- Story 018 and 020: edge rings (no label by construction).
- Story 019: painting the label plate (Bone text, Turquoise keyline).
- teammate-markers epic: raising `OnAllyMarkerRetired`.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthPresentation/Private/StealthPingRingModel.h` and `StealthPingRingModel.cpp` (label fields and retire handling)
- `Source/StealthPresentation/Private/Tests/Pings/PingLabelDropAtRemovalTest.cpp` (class `FPingsLabelDropAtRemovalTest`, name `StealthGame.Pings.LabelDropAtRemoval`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

**Test: AC1 label resolution**
- Given: a ring with various `SenderHandle` states.
- When: resolve the label; the name arrives late; view the sender's own ring.
- Then: C-Public `DisplayName` by `SenderHandle`; "Ally" while unresolved, updating in place when the name arrives; the own name on the own ring.
- Edge cases: an empty `DisplayName`.

**Test: AC2 label drop at retire (AC-PNG-19)**
- Given: a live ring from an `Active` sender who is then eliminated.
- When: the viewer's `OnAllyMarkerRetired` fires for that sender.
- Then: from that tick the ring stays to expiry with no label; the sender's own screen keeps the own-name label; no label drops without the retire event.
- Edge cases: elimination without a retire event keeps the label.

**Test: AC3 label returns for a later ring**
- Given: the same sender later pings as an informant.
- When: the new ring is created.
- Then: it carries its label.
- Edge cases: the old (label-dropped) ring is not relabelled.

QL-STORY-READY: ADEQUATE

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthPresentation/Private/Tests/Pings/PingLabelDropAtRemovalTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/informant-pings-team-comms/story-016-ring-model-and-age.md`; cross-epic teammate-markers epic (`OnAllyMarkerRetired`, TM-R14), participant-state-roster epic (C-Public `DisplayName`)
- Unlocks: `production/epics/informant-pings-team-comms/story-019-ring-painter-l0-leaf.md`
