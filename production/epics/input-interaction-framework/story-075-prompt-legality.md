# Story 075: AC 66 PromptLegality

> **Epic**: Input & Interaction Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: UI
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/input-interaction-framework.md`
**Requirement**: `TR-iif-004`, `TR-iif-005`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0021: UI/HUD layering, CommonUI and Enhanced Input
**Other ADRs**: ADR-0011: Pawn command interface and input pipeline
**ADR Decision Summary**: World-anchored drawing (markers, the interact ring and the context prompt) happens in one volatile Slate leaf at HUD layer L0, projected inside its paint pass from the rendered frame's view; view-models are plain `USTRUCT`s built by a headless presenter from owner-known data sampled every frame. ADR-0011 makes prompt legality owner-local presentation only: computed from owner-known data (own state, own cooldown, own slot, `IsRunning`/`IsInTraversal`) and public geometry, never a per-press server query, and every court fixture id is accepted at ingress so the server leaks nothing either.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0021 HIGH; ADR-0011 input layer HIGH)
**Engine Notes**: ADR-0021 E3 (the Slate paint pass sees the camera of the frame being rendered) and E4 (volatile widgets are painted every frame) apply to the prompt and ring in the L0 leaf; an informant has no prompt, and an informant context change goes through the story 071 gate. No new post-cutoff API is introduced here. The capture method is the render-target comparison used for story 073.

**Control Manifest Rules (this layer)**:
- Forbidden: Never let `AllowNoEffect` vs `Reject` depend on anything but the site and legality class — source: ADR-0011
- Forbidden: Never let a reply code depend on a target pawn's identity, team, concealment or brace state — source: ADR-0011
- Required: Owner-only data (`FHoldStatus`, rejection codes, owner results, cooldown end) lives on the PlayerController carrier family — source: ADR-0011
- Required (Presentation Layer Rules): World-anchored drawing (markers, interact ring, context prompt) happens in `SStealthWorldAnchorLeaf::OnPaint` from the rendered frame's camera view; the leaf is volatile and never inside an invalidation or retainer box; widgets are dumb; text comes from string tables — source: ADR-0021
- Guardrail: Presentation timing uses only `EstimatedServerNow()`; ring and plate timelines are sampled from `now - t0` in C++ — source: ADR-0021, ADR-0002

This story is the owner-side counterpart to the hidden-site probe (story 060). The GDD requires that a context prompt "Tend" shows identically at every court fixture in reach, whether the fixture is the owner's own live site, an enemy site, a hidden final site or a non-site court, because a prompt that appeared only at live sites would confirm which fixtures are objectives. The live-site cue exists only in the ring colour: the Interact ring is neutral Bone at every court for `TendCueDelaySec` (1.0 s) after Begin, and turns Marigold only at an own live site thereafter, from Objectives' Team-audience live-site set. A bush shows "Slip in" instead, and an informant sees no prompt at all. A prompt is a hint, never a gate: the command is valid from the moment the owner is in reach, whether or not the prompt has finished fading in.

It depends on story 060 (the hidden-site probe), which pins the server-side half: all court ids are accepted and complete as `AllowNoEffect`, so the prompt cannot be hiding a server distinction. It is sized L because the check spans six prompt screenshots plus the ring captures at each court. It relates to GDD UI Requirements items 1 and 3, R5.1, and to ADR-0021 sections 1, 2 and 4 and ADR-0011 section 9 (owner prediction). HUD widget code (the L0 leaf, the prompt and ring drawing, the string table) belongs to the HUD epic; this story owns the IIF-side owner-known inputs the presenter samples and the evidence.

---

## Acceptance Criteria

*From GDD `design/gdd/input-interaction-framework.md`, scoped to this story:*

- [ ] AC 66 (UI 3, UI, P `PromptLegality`): **GIVEN** the owner in reach of (a) an own live site, (b) an enemy site, (c) a hidden final site, (d) a non-site court, and (e) a bush, and separately as an informant **WHEN** prompts are evaluated **THEN** an identical "Tend" prompt shows in (a), (b), (c) and (d) (Objectives OC-6) and "Slip in" only in (e); **AND** nothing shows to the informant; **AND** after Begin the ring stays Bone for `TendCueDelaySec` at every court and then turns Marigold only in (a). Screenshots: `production/qa/evidence/iif-prompt-{live,enemy,hidden,court,bush,informant}.png`.

---

## Implementation Notes

*Derived from ADR-0021 Decision sections 1, 2 and 4, ADR-0011 Decision sections 3.1 (step 5), 9 and Implementation Guidelines, and GDD UI Requirements items 1 and 3:*

- Prompts: the glyph of the currently bound key plus a verb (Tend, Slip in, Slip out), computed locally from public Level Socket Contract / Manifest geometry only. "Tend" shows identically at every court fixture in reach for an Active owner, live or not, own, rival or hidden final alike (Objectives OC-6; reconciliation 2026-10-24). Bush prompts use Manifest geometry. Prompts are never a per-press server query and never shown to informants. The client does not send `PrepareBegin` while its known cooldown runs, but this story's prompt is about Interact and Hide verbs.
- Ring: 48 px, fill-only, linear sweep, predicted from the local press (F4) and re-based on `FHoldStatus`, clamped at 0.98; the fill is neutral Bone at every court for the first `TendCueDelaySec` after Begin; only then Marigold at an own live site (from Objectives' Team-audience live-site set) and Bone at any other court. Presentation only. `TendCueDelaySec` is owned by Objectives (1.0, client-visible) and must be read from tuning at test time.
- Server counterpart (story 060): every court fixture id is accepted at ingress and completes as `AllowNoEffect` for a hidden or rival fixture. The owner-visible prompt must not contradict that, and must not leak it.
- Gap fixes from the qa-lead (author to confirm; the GDD AC stays the authority): (1) "identical" prompt is not defined; use the AC 49 method (the prompt widget rendered alone to a render target, byte-equal for a-d); (2) the six named screenshots do not cover the ring timeline, so add ring captures: all courts at `TendCueDelaySec` minus one frame (Bone), then after the delay (a) Marigold and (b)-(d) Bone; (3) Bone and Marigold need token values from the art bible so the check is a pixel colour test.
- Informant: no prompt is shown (the `IMC_Informant` gate is story 071; the prompt layer simply has no gameplay prompt for an informant view-model). An informant owner standing at a court shows nothing.
- Prototype disposition: the prototype `OnInteractPressed/Released` and `ServerSetInteracting` in `StealthCharacter.cpp` and the prototype `AStealthHUD` drawing code are replaced, not ported; this story is new code with no prototype counterpart. Content step: widget layout belongs to the HUD epic; before any Content step read `production/never-resave-assets.md`.
- Test seams: a 2-client PIE harness with fixtures for a live own site, an enemy site, a hidden final site, a non-site court and a bush at 100 cm from the owner; a render-target capture helper; a way to force the owner into informant state; Objectives' live-site set (cross-epic). Repeat the five placements as an informant for the sixth screenshot.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/input-interaction-framework/story-060-hidden-site-probe.md`: the server-side acceptance and `AllowNoEffect` completion of all court ids.
- `production/epics/input-interaction-framework/story-073-strike-hint-parity.md`: the strike hint widget parity.
- `production/epics/input-interaction-framework/story-076-rejection-plate.md`: rejection plates.
- `production/epics/input-interaction-framework/story-071-context-swap-on-state-and-possession.md`: the informant context swap.
- `production/epics/input-interaction-framework/story-078-ring-monotonic-clamp.md`: the ring's clamp arithmetic.
- The prompt and ring widgets, the L0 leaf and the string table: hud epic (stories not yet written).

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGame/Public/PawnCommandClient.h` and `Source/StealthGame/Private/PawnCommandClient.cpp` (owner-known prompt inputs: in-reach fixture class, local cooldown and slot; no per-press server query)
- `Source/StealthFoundation/Public/View/` (plain data the presenter reads for the prompt and ring; IIF-owned data side)
- `production/qa/evidence/iif-prompt-live.png`
- `production/qa/evidence/iif-prompt-enemy.png`
- `production/qa/evidence/iif-prompt-hidden.png`
- `production/qa/evidence/iif-prompt-court.png`
- `production/qa/evidence/iif-prompt-bush.png`
- `production/qa/evidence/iif-prompt-informant.png`
- The ring captures at `TendCueDelaySec` minus one frame and plus one frame under `production/qa/evidence/` (names chosen when the check is run, prefix `iif-ring-`)
- HUD widget code: hud epic, not this story

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

*Test cases not yet defined — run /qa-plan to generate them.*

QL-STORY-READY: GAPS
- Gap 1: "identical" prompt is not defined; use the AC 49 method (the prompt widget rendered alone to a render target, byte-equal for a-d).
- Gap 2: the six named screenshots do not cover the ring timeline. Add ring captures: all courts at `TendCueDelaySec` minus one frame (Bone), (a) after the delay (Marigold), (b)-(d) after the delay (Bone).
- Gap 3: Bone and Marigold need token values (art bible) so the check is a pixel colour test, and `TendCueDelaySec` is read from the Objectives tuning.

QA-lead manual check sketch (unconfirmed): prompt legality and tend ring cue. Setup: 2-client PIE, owner at 100 cm from (a) own live site, (b) enemy site, (c) hidden final site, (d) non-site court, (e) bush; repeat as informant. Verify: capture the prompt at each; compare the a-d widgets; start Tend at a-d and capture the ring at delay minus 1 frame and at delay plus 1 frame. Pass condition: the a-d prompts are byte-identical "Tend"; (e) shows "Slip in" only; the informant shows no prompt; the ring is Bone at both samples for b-d, and Bone then Marigold for (a); screenshots `production/qa/evidence/iif-prompt-{live,enemy,hidden,court,bush,informant}.png` plus the ring captures retained.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: UI
**Required evidence**:
- UI: a retained screenshot of each screen touched, in `production/qa/evidence/`: `production/qa/evidence/iif-prompt-live.png`, `production/qa/evidence/iif-prompt-enemy.png`, `production/qa/evidence/iif-prompt-hidden.png`, `production/qa/evidence/iif-prompt-court.png`, `production/qa/evidence/iif-prompt-bush.png`, `production/qa/evidence/iif-prompt-informant.png`

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/input-interaction-framework/story-060-hidden-site-probe.md`; cross-epic: hud (stories not yet written; the L0 leaf, prompt and ring), cross-epic: objectives-chain (stories not yet written; OC-6, live-site set, `TendCueDelaySec`), cross-epic: level-socket-contract (stories not yet written; public fixture and bush geometry)
- Unlocks: None
