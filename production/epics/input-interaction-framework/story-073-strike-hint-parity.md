# Story 073: AC 49 StrikeHintParity

> **Epic**: Input & Interaction Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: UI
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/input-interaction-framework.md`
**Requirement**: `TR-iif-010`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0021: UI/HUD layering, CommonUI and Enhanced Input
**Other ADRs**: ADR-0011: Pawn command interface and input pipeline
**ADR Decision Summary**: HUD layers are UMG widgets added per local player at fixed ZOrders, all hit-test invisible below the menus. The context prompt, interact ring and markers draw in one volatile Slate leaf at L0 from the rendered frame's view; view-models are plain `USTRUCT`s built by a headless presenter that samples its inputs every frame by value, and widgets are dumb. ADR-0011 fixes that the strike hint's data (the aim ray, the eye-mask flag, `IsMarkedAlly`) comes only from Camera's `GetAimRay()` and the owner's local models, with no lock-on or target-dependent presentation.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0021 HIGH; ADR-0011 input layer HIGH)
**Engine Notes**: ADR-0021 E3 (the Slate paint pass sees the rendered frame's camera view) and E4 (a volatile widget is laid out and painted every frame) apply to anything drawn in the world-anchored leaf; the strike hint is screen-fixed (lower-centre) per GDD UI Requirements item 4, so it lives in a persistent layer, not the world-anchored leaf. Rendering the hint widget alone to a render target (fixed frame, fixed animation time, same resolution, no world-space anchor) is a standard UMG capability; the byte-for-byte comparison is a harness step, not an engine API.

**Control Manifest Rules (this layer)**:
- Required: The Attack payload comes only from Camera's `GetAimRay()`; `PawnRef` is honoured only through `CanViewerKnow` and `PawnRefAgreementCm` — source: ADR-0011, ADR-0015
- Forbidden: Never add lock-on, snapping, magnetism or aim assist — source: ADR-0011, ADR-0015
- Required (Presentation Layer Rules): Widgets are dumb (no UMG property bindings, no Blueprint logic beyond layout and style); `FHudViewModel::Dump()` is deterministic; player-facing text comes from string tables — source: ADR-0021
- Forbidden: Never let a reply code depend on a target pawn's identity, team, concealment or brace state — source: ADR-0011

This story is the pixel-parity check for the strike hint, the only on-screen affordance that reacts to what the aim ray is pointing at. The hint, "[bound glyph] Strike" on an opaque Bone plate at lower-centre, appears whenever the ray hits any rendered, non-ally pawn in reach, NPC or player alike, and nothing appears for an empty spot or a marked ally. If the hint differed in any pixel between an NPC, an enemy player and an enemy informant who share one `AppearanceId`, the owner could read the target's nature from the hint itself; so the criterion requires the three images to be byte-identical when the hint widget is rendered alone, and requires that no hint widget is constructed at all for a spot or an ally.

It depends on story 064 (the client's ally rule and no-aim-assist behaviour), which fixes the inputs the hint uses (`IsMarkedAlly`, the eye-mask flag). It relates to GDD R10 and UI Requirements item 4 and to ADR-0021 sections 1 and 4. HUD widget code (the strike hint widget, its view-model and its string) belongs to the HUD epic; this story owns the IIF-side input data (the aim-ray hit class and the ally exclusion that feed the presenter) and the five evidence captures.

---

## Acceptance Criteria

*From GDD `design/gdd/input-interaction-framework.md`, scoped to this story:*

- [ ] AC 49 (UI, P `StrikeHintParity`): **GIVEN** an NPC, an enemy player and an enemy informant with the same `AppearanceId`, each in reach **WHEN** the ray hits each **THEN** the hint widget, rendered alone to a render target, is pixel-identical in all three; **AND** no hint appears for an empty spot or a marked ally. Screenshots: `production/qa/evidence/strike-hint-parity-{npc,player,informant,spot,ally}.png`.

---

## Implementation Notes

*Derived from ADR-0021 Decision sections 1 and 4 and Implementation Guidelines, ADR-0011 Decision section 9 (Attack payload), and GDD UI Requirements item 4:*

- Hint inputs: the hint appears whenever Camera's aim ray (`GetAimRay()`) hits any rendered pawn (eye-mask alpha above 0, never the engine's was-recently-rendered test) that is non-ally and within reach, NPC or player alike; nothing appears for a spot. Allies are excluded via `IsMarkedAlly` from the local marker model (ADR-0021: served only from the owner's own C-TeamView, never sent to the server). Concealed pawns are not rendered, so the hint cannot act as x-ray. No highlight, outline, colour or sound differs by target class.
- The view-model is a plain `USTRUCT` built by `FHudPresenterCore` and dumped deterministically (`FHudViewModel::Dump()`); the widget receives only values to draw, applies a value only when it differs from the last applied, and is created once at HUD init and toggled by visibility, so the parity capture compares the same widget tree for every target class (ADR-0021 Implementation Guidelines). Strings come from `ST_Hud`. The presenter must receive no target-class input (no NPC/player/informant flag): that absence is what makes the pixel parity true by construction, and the screenshots prove it.
- Comparison method (qa-lead): the hint widget is rendered alone to a render target with a fixed frame and animation time, no world-space anchor and the same resolution; the three hint captures are compared byte for byte (zero differing pixels); a spot capture and an ally capture show no hint, and no hint widget is constructed for them. Setup: PIE with an NPC, an enemy player and an enemy informant sharing one `AppearanceId`, each in reach, plus an empty spot and a marked ally on the ray.
- Fallback noted by the GDD (not built here): if playtests show aiming fails, a static 4 px Bone stepped square at centre that never changes state (art bible 7.1 amendment, UX decision). The art bible bans crosshairs; do not add one.
- Prototype disposition: the prototype's `AStealthHUD` drawing code and its `FindAssassinationTarget` target logic are replaced, not ported (ADR-0021; ADR-0011 Migration Plan); this story is new code with no prototype counterpart. Content step: any widget Blueprint layout belongs to the HUD epic; before any Content step read `production/never-resave-assets.md`.
- Test seams: a render-target capture helper in the CQTest harness, an `IAllyMarkerQuery` stub, a fixture with three pawns sharing one `AppearanceId`, Camera's aim ray (cross-epic, camera-system).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/input-interaction-framework/story-064-no-ally-attack-no-aim-assist.md`: the client Attack payload rule and no aim assist.
- `production/epics/input-interaction-framework/story-075-prompt-legality.md`: context prompts and the interact ring colour.
- `production/epics/input-interaction-framework/story-076-rejection-plate.md`: rejection plates.
- The strike hint widget, its view-model builder and string: hud epic (stories not yet written).

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGame/Public/PawnCommandClient.h` and `Source/StealthGame/Private/PawnCommandClient.cpp` (the aim-ray hit data and ally exclusion that feed the presenter; shared with story 064)
- `Source/StealthFoundation/Public/View/` (plain data the presenter reads for the hint; IIF-owned data side)
- `production/qa/evidence/strike-hint-parity-npc.png`
- `production/qa/evidence/strike-hint-parity-player.png`
- `production/qa/evidence/strike-hint-parity-informant.png`
- `production/qa/evidence/strike-hint-parity-spot.png`
- `production/qa/evidence/strike-hint-parity-ally.png`
- HUD widget code: hud epic, not this story

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: AC 49 StrikeHintParity — strike hint pixel parity
  - Setup: PIE with an NPC, an enemy player and an enemy informant sharing one `AppearanceId`, each in reach; also an empty spot and a marked ally on the ray; the hint widget rendered alone to a render target with a fixed frame and animation time (no world-space anchor, same resolution).
  - Verify: aim the ray at each of the five; capture the render target; compare the three hint images byte for byte; confirm no hint widget is constructed for the spot and the ally.
  - Pass condition: the npc, player and informant images are pixel-identical (zero differing pixels); the spot and ally captures show no hint; files `production/qa/evidence/strike-hint-parity-{npc,player,informant,spot,ally}.png` retained.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: UI
**Required evidence**:
- UI: a retained screenshot of each screen touched, in `production/qa/evidence/`: `production/qa/evidence/strike-hint-parity-npc.png`, `production/qa/evidence/strike-hint-parity-player.png`, `production/qa/evidence/strike-hint-parity-informant.png`, `production/qa/evidence/strike-hint-parity-spot.png`, `production/qa/evidence/strike-hint-parity-ally.png`

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/input-interaction-framework/story-064-no-ally-attack-no-aim-assist.md`; cross-epic: hud (stories not yet written; the hint widget and presenter), cross-epic: camera-system (stories not yet written; `GetAimRay`, eye-mask flag), cross-epic: teammate-markers (stories not yet written; `IsMarkedAlly`)
- Unlocks: None
