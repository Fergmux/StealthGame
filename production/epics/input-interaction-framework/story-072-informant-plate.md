# Story 072: AC 28 InformantPlate

> **Epic**: Input & Interaction Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: UI
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/input-interaction-framework.md`
**Requirement**: `TR-iif-011`, `TR-iif-003`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0011: Pawn command interface and input pipeline
**Other ADRs**: ADR-0021: UI/HUD layering, CommonUI and Enhanced Input
**ADR Decision Summary**: Seven server-validated entry points with no caller identity, two-stage validation, Enhanced Input edges and batched owner replies. `IMC_Informant` maps no gameplay actions: presses of the former Active keys are caught locally and show the owner-only "Informants can only observe" plate, rate-limited by `InformantPlateCooldownSec`; nothing is sent to the server. ADR-0021 places plates in HUD layer L1 (ZOrder 30), hit-test invisible, driven by a plain-C++ presenter view-model.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0011 input layer HIGH; ADR-0021 HIGH)
**Engine Notes**: The caught presses are the informant context's Enhanced Input bindings: `IMC_Informant` maps the former Active keys (Q, E, LMB, F) to a local-only handler, so no `ServerPawnCommand` is sent. V1 (story 008) covers press-edge behaviour on this project's contexts but does not gate this story, whose observable is the absence of an RPC. ADR-0021 E1: widgets are added with `AddToPlayerScreen(ZOrder)`, never `AddToViewport`, and layers below ZOrder 100 are `HitTestInvisible` and never `UCommonActivatableWidget`s.

**Control Manifest Rules (this layer)**:
- Required: Gameplay input is bound on `AStealthPlayerController`; held commands map `Started` to Begin and both `Completed` and `Canceled` to End; server-meaningful actions are Boolean — source: ADR-0011
- Forbidden: Never use Hold/HoldAndRelease/Tap/Pulse/Chorded triggers for server-meaningful actions; never make a gameplay command a Common UI action — source: ADR-0011
- Required (Presentation Layer Rules): Presentation timing uses only `EstimatedServerNow()`; plate and card timelines are sampled from `now - t0` in C++; widgets are dumb; player-facing text comes from string tables — source: ADR-0021, ADR-0002
- Guardrail: uplink at most 20 reliable RPCs per second per connection; honest play about 1-2 per second — source: ADR-0011 (the informant presses must add none)

This story verifies the owner-side behaviour of an Informant who mashes the former gameplay keys. An Informant has been removed from active play and can only observe; the server would reject every command with `NotNow`, but a rejected command is itself traffic and a reply that could differ from an Active player's. So the design catches the presses locally: no RPC leaves the client, and the owner sees one plain plate, "Informants can only observe", at most once per `InformantPlateCooldownSec` (3.0 s), with no icon and no sound. The check is a screenshot of the plate while it is visible plus three counters (server-RPC count, plate-show count, audio event count).

It depends on story 007 (the IA_* assets and the three contexts exist, including `IMC_Informant`). It relates to GDD R13 (`IMC_Informant`) and UI Requirements item 6 (plate timing 400 ms in, 2 s hold, 600 ms out, lower-centre, suppressed until the removal card has collapsed), and to ADR-0011 section 9 and ADR-0021 sections 1 and 8. HUD widget code (the plate layer and the L1 persistent layer) belongs to the HUD epic; this story owns the input side and the data that triggers the plate.

---

## Acceptance Criteria

*From GDD `design/gdd/input-interaction-framework.md`, scoped to this story:*

- [ ] AC 28 (R13, UI, P `InformantPlate`): **GIVEN** an Informant **WHEN** Q, E, LMB or F is pressed 3 times in 2 s **THEN** no server RPC is sent, the plate shows once and no sound plays. Screenshot: `production/qa/evidence/iif-informant-plate.png`.

---

## Implementation Notes

*Derived from ADR-0011 Decision section 9 (Contexts) and ADR-0021 Decision sections 1, 4 and 8; GDD UI Requirements item 6:*

- `IMC_Informant` has no gameplay actions: presses of the former Active keys are caught locally and show an owner-only plate, "Informants can only observe", rate-limited by `InformantPlateCooldownSec`; nothing is sent. The plate is shown only from presses caught under `IMC_Informant`, so it never appears before the removal card has collapsed (the context follows the presented own role, ADR-0021 section 8, story 071).
- Plate timeline per the GDD: 20 px Bone text on an Obsidian plate at lower-centre (about 70 % height; exact anchor per HUD), 400 ms in, 2 s hold, 600 ms out, at most once per 3 s, no icon and no sound. The timeline is sampled from `now - t0` in C++ by the HUD presenter, never from a widget animation asset; text comes from the `ST_Hud` string table. HUD owns the widget and the view-model; the IIF side is the local catch of the press (a handler on the informant context in `UPawnCommandClient`) raising a "plate requested" fact into the presenter's inputs, with the cooldown tracked by value.
- Interpretation to confirm (qa-lead): "pressed 3 times in 2 s" is read as each of Q, E, LMB and F pressed 3 times within the same 2 s window (12 presses), with the plate cooldown shared (3.0 s is longer than the 2 s window), so exactly one plate shows. A further press after `InformantPlateCooldownSec` shows the plate again (the control).
- Tuning key read at test time: `InformantPlateCooldownSec` default 3.0 (hard 1-10, safe 2-5; audience All, latch Use). Do not hard-code it.
- Prototype disposition: the prototype's `AStealthHUD` drawing code is replaced, not ported (ADR-0021), and the prototype has no informant handling for input; this is new code with no prototype counterpart. Content step: `IMC_Informant` is authored in story 007 under `Content/StealthGame/Input`. Before any Content step read `production/never-resave-assets.md` (never-resave hazards on the sample's assets); do not resave or edit the sample's own input assets.
- Test seams (UI manual check): an RPC counter on `ServerPawnCommand` (story 095's outgoing-RPC hook), a plate-show counter and an audio event spy in the HUD presenter or the view-model dump.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/input-interaction-framework/story-071-context-swap-on-state-and-possession.md`: when and how `IMC_Informant` becomes active (the `UseInformantContext` gate) and the flush on swap.
- `production/epics/input-interaction-framework/story-076-rejection-plate.md`: the "Not now" rejection plate for rejected commands.
- `production/epics/input-interaction-framework/story-007-input-assets-and-trigger-lint.md`: authoring `IMC_Informant` and the trigger lint.
- The HUD plate layer, view-model and string table: hud epic (stories not yet written).

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGame/Public/PawnCommandClient.h` and `Source/StealthGame/Private/PawnCommandClient.cpp` (the local catch of former Active keys under `IMC_Informant`; plate-requested fact and cooldown by value)
- `Source/StealthFoundation/Public/View/` (the plain data struct the presenter reads for the informant-plate request, IIF-owned data side)
- `production/qa/evidence/iif-informant-plate.png` (retained screenshot)
- HUD widget and presenter code: hud epic, not this story

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: AC 28 InformantPlate — informant presses gameplay keys, no RPC, plate once, no sound
  - Setup: 2-client PIE, one client Informant (`IMC_Informant`); an RPC counter on `ServerPawnCommand`, a plate-show counter and an audio event spy.
  - Verify: press Q, E, LMB and F 3 times each within 2 s; read the three counters; capture the frame while the plate is visible.
  - Pass condition: RPC count 0; plate-show count 1; audio event count 0; screenshot `production/qa/evidence/iif-informant-plate.png` retained and shows the plate; a further press after `InformantPlateCooldownSec` shows the plate again (control).
  - Interpretation to confirm (qa-lead): "pressed 3 times in 2 s" is read as each of Q, E, LMB and F pressed 3 times within the same 2 s (12 presses); the plate cooldown is shared (`InformantPlateCooldownSec` 3.0 is above the 2 s window).

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: UI
**Required evidence**:
- UI: a retained screenshot of each screen touched, in `production/qa/evidence/`: `production/qa/evidence/iif-informant-plate.png`

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/input-interaction-framework/story-007-input-assets-and-trigger-lint.md`; cross-epic: hud (stories not yet written; the plate layer, presenter and string table)
- Unlocks: None
