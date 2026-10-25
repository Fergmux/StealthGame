# Story 016: Presentation view helpers and the tend-cue ring rule

> **Epic**: Objectives Chain
> **Status**: Ready
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/objectives-chain.md` (OC-6 the live-site cue exists only after Begin and `TendCueDelaySec`; OC-15; UI Requirements; AC-OC-32 third clause)
**Requirement**: `TR-oc-003` (the view's presentation inputs: stage card row, clue text, live-site ring rule)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0020: Objectives data and team-view sub-structs (Decision section 9)
**Other ADRs**: ADR-0021: UI, HUD layering and Common UI (the consumer; its HUD stories call these helpers)
**ADR Decision Summary**: A View helper (`StealthGameplay/Public/View/Features/Objectives/ObjectiveViewHelpers.h`, pure functions) exposes `IsOwnLiveSite(View, FCourtFixtureId)`, `GetStageCardRow(View, UObjectiveChainData)`, `BuildClueText(FClueTag)` and `RingIsMarigold(View, OwnHoldTarget, HoldServerStartTime, EstimatedServerNow, TendCueDelaySec)` = own live site **and** `EstimatedServerNow - HoldServerStartTime >= TendCueDelaySec` (Bone otherwise; never Marigold for an informant). The ring's own-hold target and start come from the owner's outstanding Begin and `FHoldStatus`; the server sends nothing extra for the ring.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: LOW (pure client-side functions over data, no engine API)
**Engine Notes**: None. Time comes from the client server-time estimator (`EstimatedServerNow`, ADR-0002), never a client wall clock; the helper takes it as an argument so tests can step it.

**Control Manifest Rules (this layer)**:
- Required: Card text and clue phrases are rendered client-side from cooked data; only indices and enums travel — source: ADR-0020
- Required: Client presentation reads time from `EstimatedServerNow()` only — source: ADR-0002, ADR-0020
- Forbidden: Never include the Objectives `Public/Server/` headers from client modules or Presentation — source: ADR-0020
- Guardrail: helpers are allocation-free pure functions evaluated per frame — source: ADR-0020

The ring is the one place the player learns which court is live, and only after a deliberate action: a neutral Bone ring at every court while the tend begins, turning Marigold after `TendCueDelaySec` (default 1.0 s) only at an own live site. The delay is a Pillar 1 device (a probe's earliest informed release must fall inside Crowd's decoy early-release range, I-OC3 in story 001), so the helper's threshold is the load-bearing part. The helpers are pure so the HUD and IIF presentation stories get one tested implementation and neither can accidentally read a server header or a role.

---

## Acceptance Criteria

*From GDD `design/gdd/objectives-chain.md` (AC-OC-32 third clause, OC-6, OC-15) and ADR-0020 section 9, scoped to this story:*

- [ ] **AC1** (AC-OC-32 [U] `TendCueDelay`, ring clause): **GIVEN** the defaults (`TendCueDelaySec` 1.0) **THEN** `RingIsMarigold` is false (Bone) until Begin + 1.0 s and true at Begin + 1.0 s (+- 1 frame, at 60 and at 30 fps) **only** at an own live site; it is never true at a non-live court and never true for an informant; a tend cancelled before 1.0 s resets the ring. The visual check of the ring is covered by the story 020 screenshots.
- [ ] **AC2** (ADR section 9 [U] `ViewHelpersPure`): `IsOwnLiveSite`, `GetStageCardRow` and `BuildClueText` are pure functions in `ObjectiveViewHelpers.h`; **GIVEN** a test module that links only the View and client modules **THEN** results depend only on the team view, the cooked chain asset and client tuning, the header and its dependencies include no server header (include scan; a seeded server include fails it), the same inputs give the same outputs and an empty view returns no live site and no card.

---

## Implementation Notes

*Derived from ADR-0020 Decision sections 4 and 9:*

`RingIsMarigold(const FObjectiveTeamView&, FCourtFixtureId OwnHoldTarget, double HoldServerStartTime, double EstimatedServerNow, double TendCueDelaySec, bool bIsInformant)` returns `IsOwnLiveSite(View, OwnHoldTarget) && !bIsInformant && (EstimatedServerNow - HoldServerStartTime) >= TendCueDelaySec`. The informant exclusion mirrors the predicate: informants cannot tend, and the ring is never Marigold for one. The delay comes from the client tuning face (public key `TendCueDelaySec`, story 001), read once per hold at Begin so a mid-hold tuning publish cannot change it.

`GetStageCardRow(View, Chain)` returns the card title and line row for `LiveSites[0].DefIndex` from the cooked chain asset (story 002); `BuildClueText(FClueTag)` builds "A court [phrase]" from the localised phrase table keyed by area type and feature kind, falling back to area type only (story 013). Rival steps fill from `RivalPrelimCount` only and never show a final step (HUD-R11: 3 own steps, 2 rival steps, rival steps 0 in a 1v0 dev round): the HUD story owns that layout; the helpers expose only the integers.

Tests step a fake `EstimatedServerNow` at 60 and 30 fps and assert the transition frame within +- 1 frame of Begin + 1.0 s. The include scan reuses `ObjectivesIncludeLint` machinery from story 022.

Prototype replaced: the prototype HUD (`StealthHUD.cpp`) drew markers on objectives the team may tend from replicated actor state; OC-15 forbids any compass, waypoint, map marker or world plate. The HUD epic replaces the drawing; this story supplies the rules it must call.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-020-pie2-hidden-site-oracle-and-world-signature.md`: the ring screenshots and the byte-exact owner traces.
- `story-013-clue-function-and-too-broad-log.md`: `FClueTag` and the phrase table keys.
- HUD and IIF presentation epics: drawing the ring, the stage card and the rival steps.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Public/View/Features/Objectives/ObjectiveViewHelpers.h`
- `Source/StealthGameplay/Private/Features/Objectives/ObjectiveViewHelpers.cpp`
- `Source/StealthGameplay/Private/Tests/Objectives/TendCueDelayTest.cpp` (class `FObjectivesTendCueDelayTest`, name `StealthGame.Objectives.TendCueDelay`)
- `Source/StealthGameplay/Private/Tests/Objectives/ViewHelpersPureTest.cpp` (`StealthGame.Objectives.ViewHelpersPure`)

---

## QA Test Cases

*Written by qa-lead at story creation (QL-STORY-READY: ADEQUATE; the visual check is covered by the story 020 screenshots). The developer implements against these — do not invent new test cases during implementation.*

- **AC-1** (`TendCueDelay` helper): ring colour rule
  - Given: default `TendCueDelaySec` 1.0; fake clock stepping per frame; own live site, non-live court, informant pawn
  - When: a tend Begins and time advances
  - Then: `RingIsMarigold` false (Bone) until Begin + 1.0 s, true at Begin + 1.0 s (+- 1 frame) only at an own live site; never true at a non-live court; never true for an informant
  - Edge cases: frame step at 60 and 30 fps; tend cancelled before 1.0 s resets the ring.
- **AC-2** (`ViewHelpersPure`): pure view helpers
  - Given: `ObjectiveViewHelpers.h`; a test module that links only the View and client modules
  - When: `IsOwnLiveSite`, `GetStageCardRow`, `BuildClueText` are called with a team view, the cooked chain asset and client tuning
  - Then: results depend only on those inputs; the header and its dependencies include no server header (include scan); same inputs give same outputs
  - Edge cases: a seeded server include fails the scan; empty view returns no live site and no card.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: the two test files under `Source/StealthGameplay/Private/Tests/Objectives/` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/objectives-chain/story-001-objectives-tuning-rows-and-invariants.md`, `production/epics/objectives-chain/story-002-chain-data-types-and-cacao-asset.md`, `production/epics/objectives-chain/story-010-team-view-struct-and-builder.md`, `production/epics/objectives-chain/story-013-clue-function-and-too-broad-log.md`; cross-epic: input-interaction-framework (`FHoldStatus` and the owner's outstanding-Begin start time; `story-078-ring-monotonic-clamp.md`)
- Unlocks: `production/epics/objectives-chain/story-020-pie2-hidden-site-oracle-and-world-signature.md`; cross-epic: hud epic (stage card, ring and rival steps)
