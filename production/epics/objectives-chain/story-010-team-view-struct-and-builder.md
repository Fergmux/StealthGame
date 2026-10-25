# Story 010: Team view struct and builder

> **Epic**: Objectives Chain
> **Status**: Ready
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/objectives-chain.md` (OC-5 a court's location enters a team's view only when live for that team, OC-17, SP-1; AC-OC-03 contents, AC-OC-29)
**Requirement**: `TR-oc-003` (`FObjectiveTeamView` rides C-TeamView; pushed at Active entry)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0020: Objectives data and team-view sub-structs (Decision sections 4 and 6)
**ADR Decision Summary**: `FObjectiveTeamView {RoundEpoch, ChainSerial, StageIndex, LiveSites[], CompletedDefMask, RivalPrelimCount}` holds exactly what IVP R3's Team rows grant: own stage, own live site ids, own completed rows, the stage card row index and role-blind clue, and the 0-2 rival count. It never holds a locked site, an opponent site, opponent progress other than `RivalPrelimCount`, anyone's hold progress, or the final before the team's preliminaries are complete. Every member of team T (Active or Informant) gets the same view. Row identity on the wire is the index; the final's completion is not written to the view.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: LOW (struct and builder are plain C++/USTRUCT; the carrier is story 011)
**Engine Notes**: E7: `uint16`/`uint8` `UPROPERTY` members and a `TArray` of `USTRUCT`s inside a replicated struct replicate on the legacy driver, with members changed within one net update arriving in one bunch [U, verified by OBJ-V2 in story 011]. Width is about 12 bytes plus 4 per live site.

**Control Manifest Rules (this layer)**:
- Required: Team-audience objective data travels only in `UObjectiveTeamViewComponent::View`, written only by `UObjectivesService` inside a queue slot, the `OnRoundActive` step or the Roster rebind handler — source: ADR-0020
- Forbidden: Never put a locked site, an opponent site, opponent progress other than `RivalPrelimCount`, anyone's hold progress, or the final before the team's preliminaries are complete in the view — source: ADR-0020
- Forbidden: Never replicate card text (`FText`) or clue strings; only indices and enums travel — source: ADR-0020
- Guardrail: about 16 bytes per view change per team member, a handful per round — source: ADR-0020

This story builds the data the HUD will read and the rules for what may be in it. It is the Policy-facing half of the epic: IVP R3 grants the Team audience exactly these facts, `SiteKnownToTeam` is presentation only (it colours the ring and fills the stage card, never gates ingress or the predicate, ruling D), and `RivalPrelimCount` is the one piece of opponent progress allowed, as an integer only (ruling B). The builder is a pure function of the core store so the contents rules are testable without a network; the multi-client dump is story 019.

---

## Acceptance Criteria

*From GDD `design/gdd/objectives-chain.md` (AC-OC-03 contents, AC-OC-29) and ADR-0020 sections 4 and 6, scoped to this story. QL-STORY-READY revised AC1 (core-level scope) and AC3 (exact width):*

- [ ] **AC1** (AC-OC-29 [U] `NoTeamViewBeforeActive`, core level): **GIVEN** a round in `Preparing` after `OnLayoutReady` **THEN** every stub per-controller view sink holds the empty default (no site id, no stage card); **WHEN** `Active` begins **THEN** each team's view is pushed in that slot, exactly once per team. (The replicated check is story 019.)
- [ ] **AC2** (AC-OC-03 [U] `TeamViewContentsCore`): **GIVEN** a round at `Active` start **THEN** team 0's built view holds only `P1[0]` as a live site and team 1's only `P1[1]`, and no view holds `Final`, `P2[*]` or any opponent site; **WHEN** team 0 completes both preliminaries **THEN** only team 0's view gains `Final`; `FRoundPublicState` carries no objective field.
- [ ] **AC3** (ADR-0020 sections 4 and 6 [U] `TeamViewWidthAndChangeOnly`): the view is written only when its value differs (an identical second write is a no-op); every member of team T, Active or Informant, gets the same view; `CompletedDefMask` never includes the final; `NetSerialize` of `FObjectiveTeamView` writes **at most 12 + 4 x LiveSiteCount bytes** (asserted for 0, 1, 2 and the maximum live-site count; a regression above that fails); `DefIndex` (not `FName`) travels; `RoundEpoch` and `ChainSerial` are set; the struct has no `FString`, `FText` or `FName` member.

---

## Implementation Notes

*Derived from ADR-0020 Decision sections 4 and 6:*

```
FObjectiveLiveSite { FCourtFixtureId Court; uint8 DefIndex = 0xFF; FClueTag Clue; }
FObjectiveTeamView { uint32 RoundEpoch; uint16 ChainSerial; EObjectiveTeamStage StageIndex {None, Stage1, Stage2, Final};
                     TArray<FObjectiveLiveSite> LiveSites; uint16 CompletedDefMask; uint8 RivalPrelimCount; }
```

Put them in `StealthGameplay/Public/View/Features/Objectives/` (client-safe). Field-to-fact map, each an encoding of a fact IVP R3 already grants (no new fact, no new audience): `StageIndex` and `CompletedDefMask` are own-team `ObjectiveProgress`; `LiveSites[].Court` is `SiteKnownToTeam` (ring colour, card and credit display only); `LiveSites[].DefIndex` selects the card row in the cooked chain asset (card text is local data); `LiveSites[].Clue` is a role-blind pure function of public geometry, sent so clients need no clue derivation and the graybox works; `RivalPrelimCount` is the OC-12 integer. `RoundEpoch` and `ChainSerial` are bookkeeping (a stale-view guard, and which cooked chain the indices refer to). `FClueTag` and the clue content are story 013.

The builder `FObjectivesCore::BuildTeamView(Team)` is pure: from the store it emits `LiveSites` for the team's current stage only (at most `ObjectivesPerStageCount`, MVP 1), the team's own completed prelim rows in `CompletedDefMask` (never the final), `StageIndex` and the current `RivalPrelimCount`. The final becomes a live site only after the team's own preliminaries are complete (SP-1); the view carries no completion of the final (the round-end step presents it). Writes are change-only: compare against the last written value per team and skip an identical write (so `RivalPrelimCount` rises and stage changes are the only traffic). Writing itself, to each controller's component, is story 011.

For AC3's width check, `NetSerialize` of the struct (or the default property serialisation measured on an `FNetBitWriter`, per ADR-0018's fixed-width lesson: the base archive writes 32 bits for `SerializeInt`, so use `SerializeBits` if a custom serializer is used) must not exceed the budget; if the default serialisation is looser than 12 + 4n bytes, tighten the struct, not the budget (the budget is ADR-0020 section 4's "about 12 bytes plus 4 per live site").

Prototype replaced: the prototype replicated per-site actors with `bCompleted`/`InteractorCount` to every client and HUD code read them directly (`StealthHUD.cpp` marks objectives from the replicated actors); replaced, not ported, and the HUD side is the HUD epic.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-011-team-view-component-and-replication.md`: the `UObjectiveTeamViewComponent`, push model, owner-only delivery and allowlist row.
- `story-012-rejoin-rebind-and-reset.md`: the rebind-event write and the reset clear.
- `story-013-clue-function-and-too-broad-log.md`: `FClueTag` and clue content.
- `story-019-pie2-team-view-ingress-and-informant-parity.md`: the multi-client replicated dump.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Public/View/Features/Objectives/ObjectiveTeamView.h` (`EObjectiveTeamStage`, `FObjectiveLiveSite`, `FObjectiveTeamView`)
- `Source/StealthGameplay/Private/Features/Objectives/ObjectiveTeamView.cpp`
- `Source/StealthGameplay/Private/Features/Objectives/ObjectivesCore.cpp` (`BuildTeamView`, change-only write seam)
- `Source/StealthGameplay/Private/Tests/Objectives/NoTeamViewBeforeActiveTest.cpp` (class `FObjectivesNoTeamViewBeforeActiveTest`, name `StealthGame.Objectives.NoTeamViewBeforeActive`)
- `Source/StealthGameplay/Private/Tests/Objectives/TeamViewContentsCoreTest.cpp` (`StealthGame.Objectives.TeamViewContentsCore`)
- `Source/StealthGameplay/Private/Tests/Objectives/TeamViewWidthAndChangeOnlyTest.cpp` (`StealthGame.Objectives.TeamViewWidthAndChangeOnly`)

---

## QA Test Cases

*Written by qa-lead at story creation (QL-STORY-READY: GAPS; the AC1 scope and the AC3 exact-width rewrites were applied above and the specs cover the revised text). The developer implements against these — do not invent new test cases during implementation.*

- **AC-1** (`NoTeamViewBeforeActive`): no views before Active
  - Given: stub view sink per controller; `FObjectivesCore` after OnLayoutReady (Preparing)
  - When: Preparing ends and Active begins
  - Then: during Preparing every sink holds the empty default (no site id, no card); at Active each team's view is pushed in that slot
  - Edge cases: rejoin during Preparing still empty; Active entry pushes exactly once per team.
- **AC-2** (`TeamViewContentsCore`): view contents at start and after unlock
  - Given: MVP chain bound; Active start
  - When: team views are built; then team 0 completes both preliminaries
  - Then: team 0 view live set = {P1[0]}, team 1 = {P1[1]}; no view holds Final, P2[*] or any opponent site; after both preliminaries only team 0 view gains Final
  - Edge cases: team 1 view unchanged by team 0 progress; P1 completed removes it from the live set.
- **AC-3** (`TeamViewWidthAndChangeOnly`): write rules and width
  - Given: builder; stub sinks for two Active members and one Informant of team T; byte-count harness
  - When: the same view is written twice, then a changed view; `NetSerialize` is measured
  - Then: write only when the value differs (second identical write is a no-op); every member of team T gets the same view; `CompletedDefMask` never includes the final; size at most 12 + 4n bytes; `DefIndex` (not `FName`) travels; `RoundEpoch` and `ChainSerial` set; no `FString`/`FText`/`FName` member
  - Edge cases: n = 0, 1, 2, max; mask bit for the final stays 0 after the win.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: the three test files under `Source/StealthGameplay/Private/Tests/Objectives/` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/objectives-chain/story-002-chain-data-types-and-cacao-asset.md`, `production/epics/objectives-chain/story-004-core-site-state-and-progress.md`, `production/epics/objectives-chain/story-013-clue-function-and-too-broad-log.md` (declares `FClueTag`, which `FObjectiveLiveSite` carries); cross-epic: `production/epics/participant-state-roster/story-037-team-view-serialized-size.md` (the C-TeamView budget this width sits inside); `production/epics/information-visibility-policy/story-013-allowlist-content.md`
- Unlocks: `production/epics/objectives-chain/story-011-team-view-component-and-replication.md`, `production/epics/objectives-chain/story-012-rejoin-rebind-and-reset.md`, `production/epics/objectives-chain/story-016-presentation-view-helpers-and-ring-rule.md`, `production/epics/objectives-chain/story-019-pie2-team-view-ingress-and-informant-parity.md`
