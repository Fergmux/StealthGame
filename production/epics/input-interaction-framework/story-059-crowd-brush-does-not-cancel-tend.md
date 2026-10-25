# Story 059: AC 38 CrowdBrushDoesNotCancelTend

> **Epic**: Input & Interaction Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/input-interaction-framework.md`
**Requirement**: `TR-iif-005`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0011: Pawn command interface and input pipeline
**ADR Decision Summary**: Seven server-validated entry points with no caller identity, two-stage validation with token buckets, Enhanced Input edges and batched owner replies. For holds, the server times every end as a queue item from server time, and the Interact condition sweep only reads state: commanded displacement beyond `InteractMoveCancelDistCm` or leaving the cancel range schedules a `HoldTimer(ConditionCancel)`, while external pushes never count.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0011 input layer HIGH, command path MEDIUM)
**Engine Notes**: No post-cutoff input API is involved. The relevant engine fact is the CMC split in ADR-0011 Decision 5 and ADR-0010 section 8: `ISharedPawnStateView::CommandedDisplacementCm` accumulates displacement only while the pawn's own `Move` input is non-zero, so a capsule pushed by a crowd NPC (displacement under zero input, depenetration, bases) is never "commanded". Whether a multi-pawn physics replay is deterministic enough for a 20-of-20 pass is an open test-design question (see QA gaps), not a verification item V1-V6.

**Control Manifest Rules (this layer)**:
- Required: Hold ends are `ScheduleAt` queue items; the release debounce is server-side and identical for every pawn — source: ADR-0011
- Forbidden: Never let the condition sweep cancel a hold itself (it schedules `HoldTimer(ConditionCancel)` items) — source: ADR-0011
- Forbidden: Never time a hold on the IIF's own tick, `FTimerManager` or a client timestamp; never debounce releases client-side — source: ADR-0011
- Forbidden: Never replicate `IsRunning`, `IsInTraversal` or `CommandedDisplacementCm` (owner-local copies for prompts only) — source: ADR-0010 (pinned for the pawn state this story reads)

This story is the crowd-contact regression for the Interact (Tend) hold. In a crowded city NPCs routinely brush past a pawn that is tending a site; the pillar-1 requirement is that nothing about an NPC's contact can cancel the hold, because a contact-driven cancel would be a tell that distinguishes a tender who stood still from one who did not, and because it would let the crowd grief a tender. The only causes that may end the hold are `End` (Released), commanded movement (Moved) and leaving the 240 cm cancel range (OutOfRange). The IIF already implements these in stories 039 (the full cancel matrix) and 040 (the hysteresis ranges); this story proves it in a real two-client PIE with recorded NPC path fixtures, so it is the first place the IIF and the Crowd and Locomotion epics meet.

Because it needs Crowd NPC paths and the Locomotion capsule behaviour, the qa-lead marks it deferred until those epics exist; per the plan that does not make the story Blocked. It relates to GDD R8 (Interact cancel causes, external pushes never count) and F6 (reach and cancel hysteresis), and to ADR-0011 Decision 5 (the read-only condition sweep).

---

## Acceptance Criteria

*From GDD `design/gdd/input-interaction-framework.md`, scoped to this story:*

- [ ] AC 38 (R8/F6, Integration, P `CrowdBrushDoesNotCancelTend`): **GIVEN** a pawn tending a site, and a recorded set of 20 fixed NPC paths passing within touching distance **WHEN** the paths are replayed **THEN** the hold is never cancelled by contact. Only End, commanded movement, or leaving the 240 cm cancel range cancels it.

---

## Implementation Notes

*Derived from ADR-0011 Implementation Guidelines and Decision section 5 (Interact):*

- The distance and displacement re-checks run in the IIF condition sweep, a tick function in `TG_PostPhysics` ordered immediately before ADR-0002's drain. It only reads state; for a failed condition it schedules `HoldTimer(ConditionCancel, ActionEpoch, Reason)` with `DueTime = ServerNow()`. Its slot re-checks the `ActionEpoch` and the condition, then cancels with `Moved` or `OutOfRange`. This story must not add any other cancel path. If a replay cancels the hold with anything other than those two reasons, or without `End`, the defect is in the sweep or in `CommandedDisplacementCm`, not in the test.
- `CommandedDisplacementCm` counts displacement only while the pawn's own `Move` input is non-zero; external pushes never count (GDD R8, ADR-0010 section 6 "Commanded vs external displacement"). The tender in the test issues no `Move`.
- Cancel range = `InteractRangeCm + CommandRangeSlackCm + InteractCancelHysteresisCm` = 150 + 40 + 50 = 240 cm at defaults (GDD F6). Read the keys from tuning at test time. Relevant defaults: `InteractRangeCm` 150, `InteractCancelHysteresisCm` 50, `CommandRangeSlackCm` 40, `InteractMoveCancelDistCm` 15, `InteractMoveGraceSec` 0.25, `ObjectiveInteractSec` 3.0 (Objectives-owned).
- Look, camera toggle, an approaching pawn and being targeted by an Attack whiff do not cancel (GDD R8; story 039 owns that matrix). This story adds only physical NPC contact.
- QA-lead observation (the GDD AC stays the authority): "never cancelled by contact" conflicts with the final sentence if a push exceeds 240 cm, since the range cancel is then legitimate. The fixtures must therefore bound the tender's displacement (the qa-lead proposes at most 50 cm from a 100 cm start) and be deterministic: fixed 60 Hz timestep, seeded physics off, committed under the tests data folder.
- Prototype disposition: new code, no prototype counterpart for the test. The prototype's `StealthNpcController` path-following (AI movement not going through the player path) is superseded by Crowd's follower that calls `Move` (ADR-0016), so the fixtures replay NPC `Move` commands through the AI path, not path-following.
- Test seams: the 20 path fixtures as data assets, the multi-client CQTest harness (ADR-0008) and the server command log from story 095. State the net-emulation profile as none (P form).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/input-interaction-framework/story-039-interact-cancel-matrix.md`: every Interact cancel cause and reason, including the non-cancel clauses.
- `production/epics/input-interaction-framework/story-040-interact-hysteresis.md`: the accept range, cancel range and angle thresholds.
- `production/epics/input-interaction-framework/story-041-interact-grace-on-arrival.md`: the post-Begin movement grace.
- `production/epics/input-interaction-framework/story-060-hidden-site-probe.md`: court fixture ids accepted at ingress.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Private/Core/PawnCommandCore.cpp` (condition sweep, changed only if the replay exposes a defect)
- `Source/StealthTestHarness/Private/CrowdBrushDoesNotCancelTendPieTest.cpp` (CQTest `NETWORK_TEST_CLASS(CrowdBrushDoesNotCancelTend, "StealthGame.Input.PIE")`, full name `StealthGame.Input.PIE.CrowdBrushDoesNotCancelTend.<Method>`)
- The 20 NPC path fixtures (data assets) under the tests data folder chosen by the harness

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

*Test cases not yet defined — run /qa-plan to generate them.*

QL-STORY-READY: GAPS
- Gap 1: "touching distance" and "recorded set of 20 fixed NPC paths" are unquantified.
- Gap 2: if a push exceeds 240 cm the range cancel is legitimate, so "never cancelled by contact" contradicts the final sentence.
- Gap 3: physics-driven PIE replays are not deterministic as stated.
- Note: depends on Crowd and Locomotion (cross-epic); DEFERRED until those exist, not BLOCKED.

QA-lead proposed rewrite (unconfirmed; the GDD AC stays the authority): "GIVEN a pawn tending a site at 100 cm from its interact point for D = 3.0 s and 20 recorded NPC path fixtures (data assets; each passes within 0 cm capsule clearance, i.e. capsule overlap, and displaces the tender at most 50 cm) replayed as Move commands through the AI path at a fixed 60 Hz timestep THEN in 20 of 20 replays the hold completes (OnActionCompleted once, OnActionCancelled count 0)."

QA-lead test sketch (unconfirmed): `StealthGame.Input.PIE.CrowdBrushDoesNotCancelTend.<Method>`; 2-client PIE, fixed timestep, tender on site, no net emulation (profile none); each of the 20 paths replays during one hold; per replay the hold completes with zero Moved/OutOfRange cancels and the tender displacement margin is recorded. Edge cases: a control run with commanded Move above 15 cm cancels `Moved`; End cancels `Released`; fixtures must be deterministic (seeded physics off).

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/CrowdBrushDoesNotCancelTendPieTest.cpp` (CQTest `StealthGame.Input.PIE.CrowdBrushDoesNotCancelTend`) OR a playtest doc

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/input-interaction-framework/story-040-interact-hysteresis.md`; cross-epic: crowd-npc-behaviour (stories not yet written; NPC `Move` paths), cross-epic: shared-character-locomotion (stories not yet written; published `CommandedDisplacementCm` and capsule behaviour), cross-epic: objectives-chain (stories not yet written; the Tend site and `D`)
- Unlocks: None
