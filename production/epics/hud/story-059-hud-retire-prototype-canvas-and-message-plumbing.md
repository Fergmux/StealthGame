# Story 059: Retire the prototype AStealthHUD canvas and the message plumbing

> **Epic**: HUD
> **Status**: Ready
> **Layer**: Presentation
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/hud.md`
**Requirement**: `TR-hud-001`, `TR-hud-003`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0021: UI/HUD layering, CommonUI and Enhanced Input (forbidden: porting the prototype HUD)
**ADR Decision Summary**: `AddToViewport` and the `AHUD` canvas are banned for game UI (a lint): the canvas draws with the scene viewport, under every UMG layer, so it cannot be ordered against the HUD fade layer; `AStealthHUD` survives only as the owner of the bootstrap in `StealthPresentation`. The prototype `AStealthHUD` drawing code is replaced, not ported, and no text may travel on the wire. Presentation never reads a non-local pawn's `PlayerState` or iterates actors.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: The removal touches `StealthGame` (the game module that currently owns `AStealthHUD`) and must leave `HUDClass` pointing at the production bootstrap in `StealthPresentation` (ADR-0007: `StealthGame` constructs no presentation objects beyond setting `HUDClass`). The canvas drew with the scene viewport under UMG (the reason for ADR-0021 §1, engine fact E1/E2: UMG is composited after the scene). ADR-0018 notes MotionWarping is Beta in 5.8.4 and the networking domain is HIGH risk, which matters because the first-playable networked tests exercise strikes and replication on a real listen server. `UE_LOGF` is preferred over `UE_LOG` in 5.8.

**Control Manifest Rules (this layer)**:
- Required: `AStealthHUD` and all client-side wiring live in `StealthPresentation` — source: ADR-0007
- Forbidden: Never port the prototype `AStealthHUD` drawing code — source: ADR-0021
- Guardrail: HUD: ≤ 0.3 ms CPU (AC-HUD-22), ≤ 0.5 ms with markers; UI GPU ≤ 0.5 ms at 1080p on the reference GPU (inside ADR-0022's frame budget) — source: ADR-0021

This is the last step of the HUD replacement: once the production HUD renders every cluster, the 321-line prototype (`Source/StealthGame/Private/StealthHUD.cpp` at 281 lines and `Public/StealthHUD.h` at 40) and the message plumbing that fed it are deleted, and the first-playable tests prove nothing regressed. Nothing is promoted; the prototype leaks are never carried over.

---

## Acceptance Criteria

*From GDD `design/gdd/hud.md` (HUD-R1, HUD-R2, AC-HUD-11 family), the epic's prototype disposition and ADR-0021 forbidden approaches, scoped to this story:*

- [ ] **AC1** (derived, EPIC prototype disposition, `StealthGame.*` first-playable tests and mapped `StealthGame.HUD.*` tests): **GIVEN** the production HUD renders every cluster **WHEN** the prototype `AStealthHUD` canvas drawing (`StealthHUD.cpp` / `StealthHUD.h`) and `MessagePlayer`, `MessageController`, `MessageTeam`, `MessageAll` in `StealthPlayerController.*` / `StealthGameMode.cpp` are removed **THEN** the project builds, `HUDClass` points at the production HUD and the first-playable networked tests (names in `production/first-playable-report.md`) still pass unchanged, or the story lists a one-to-one mapping from each to a named `StealthGame.HUD.*` test that passes.
- [ ] **AC2** (derived, ADR-0021 forbidden approaches, HUD-R1, `StealthGame.HUD.IncludeCheck` and the lint named below): **GIVEN** the HUD module sources and the whole tree **WHEN** the named lint and the story 004 include check run **THEN** no HUD-module file reads `AStealthPlayerState::Team` (or any `PlayerState` field) of a non-local pawn (the prototype leak in `StealthHUD.cpp`; the story names the lint), and the include check (story 004) passes on the whole tree.

---

## Implementation Notes

*Derived from the HUD EPIC prototype disposition and ADR-0021 Decision §1, §9:*

**What is removed (and where it lives today).**
- `Source/StealthGame/Private/StealthHUD.cpp` (281 lines) and `Source/StealthGame/Public/StealthHUD.h` (40 lines): `AStealthHUD : AHUD` with canvas drawing (`DrawStatusPanel`, `DrawTeammateMarkers`, `DrawObjectiveMarkers`, `DrawPings`, `DrawMessages`, `DrawRoundBanner`, `DrawInteractBar`, `DrawControls`, `ProjectToScreen`, `DrawShadowedText`).
- The message plumbing: `AStealthGameMode::MessagePlayer`, `MessageController`, `MessageTeam`, `MessageAll` (declared and defined in `StealthGameMode.*`, called throughout round, strike, penalty, stage and join paths) and the client side in `StealthPlayerController.*`: `ClientStealthMessage`, `FStealthHudMessage`, `Messages`, `GetMessages()`. These carry free text (FText) on the wire, which ADR-0021 forbids ("Never carry text on the wire"); the production HUD derives every string locally from `ST_Hud`.
- `HUDClass = AStealthHUD::StaticClass()` in `StealthGameMode.cpp` is re-pointed at the production HUD class from `StealthPresentation` (story 006's bootstrap) in the same change.

**Coverage map for what the prototype showed.** The prototype's kill confirmation is covered by story 022 (result and defence plates), civilian count by story 018 (brace indicator state logic; the tally is story 020/021), defence state by story 018 and 019, stage progress by story 039/041, enemy-stage alert by the objectives cluster (the rival steps; no text), round banner by story 040/042, teammate markers by the teammate-markers epic (`production/epics/teammate-markers/story-034-retire-prototype-marker-drawing.md` retires the marker drawing in parallel) and pings by the informant-pings epic (`story-027-retire-prototype-ping.md`). Verify each is live before deleting its prototype counterpart; none of the prototype's behaviours is a requirement on its own.

**The prototype leaks that must not survive.** `StealthHUD.cpp` reads the local `AStealthPlayerState::Team` and compares it with `Character->GetTeam()` of every character in the world (the teammate marker loop, around line 174), and reads `GetTeamStage` for both teams (the enemy-stage alert), i.e. it reads a team field replicated on every pawn and the opponent's progress. None of that may be ported: the production HUD reads only its own controller's C-TeamView, the owner reply sink, `FRoundPublicState` and the client tuning face (ADR-0021 §9).

**The named lint (AC2).** Add `StealthGame.HUD.NoRemotePlayerStateRead` to the story 004 lint suite: a grep/AST-light check over `Source/StealthPresentation/` that fails on any use of `AStealthPlayerState` (or `APlayerState`) members other than through the local-controller accessor allowlist file, on `->Team`/`GetTeam()` reads of a non-local actor, and on `GetPlayerState`, `PlayerState`, `GetController` or `Owner` access on a pawn other than the local one; confirm story 004's `StealthGame.HUD.IncludeCheck` passes over the whole tree (not only the HUD module) after the deletion.

**First-playable tests.** `production/first-playable-report.md` lists the automated `StealthGame.Rules.*` (6) and `StealthGame.EditorTools.*` (6) tests and the networked listen-server play tests A to F (3rd civilian kill exiles, enemy-informant respawn, defence reversal, objective win, per-viewer bush concealment, blind strike) driven through the real server rule paths by the `StealthBot` test driver, plus procgen seed parity and the HUD observations. Run them before and after the deletion; every test in the report must appear either as unchanged-and-passing or in a one-to-one mapping to a named `StealthGame.HUD.*` test that passes (a test that asserted on a prototype message string is mapped to the corresponding view-model dump assertion). Record the table in the story's closing note.

**Traps.** Several server-side rule paths call `MessagePlayer`/`MessageTeam`; deleting them means the rule code changes in `StealthGameMode.cpp` (delete the calls, do not stub them to no-ops that still compile text). Do not delete `StealthCharacter` members (`IsTeammateOfLocalViewer()`) that another caller still uses (teammate-markers story 034 decides that). Do not remove the `StealthBot` test driver's use of rule paths. Remove includes of `StealthHUD.h` from the build (`HUDClass`, any `Config` reference). Keep the change one commit-sized unit so a revert is clean.

**Decisions that apply.** D11 and D29 (the wording the HUD shows now replaces the prototype's strings, e.g. "Target eliminated: an enemy agent."). Out of this story: building any cluster (stories 018 to 047), the markers and pings retirements in their own epics.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/hud/story-003-hud-input-sampling-and-host-parity-path.md`, `story-006-hud-layer-stack-root-and-input-isolation.md`: the production bootstrap and layers that replace the canvas.
- `production/epics/hud/story-022-hud-result-and-defence-plate-flow.md`, `story-035-hud-team-feed-queue-f8-and-removal-lines.md`, `story-040-hud-end-banner-f10-and-round-summary-logic.md`: the clusters that cover the prototype's messages and banner.
- `production/epics/hud/story-004-hud-include-boundary-and-forbidden-api-lint.md`: the include check and lint framework this story extends.
- `production/epics/camera-system/story-025-retire-prototype-camera.md`: the parallel prototype camera retirement.
- `production/epics/teammate-markers/story-034-retire-prototype-marker-drawing.md` and `production/epics/informant-pings-team-comms/story-027-retire-prototype-ping.md`: marker and ping removal.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGame/Private/StealthHUD.cpp` and `Source/StealthGame/Public/StealthHUD.h` (deleted)
- `Source/StealthGame/Private/StealthGameMode.cpp` and `Public/StealthGameMode.h` (remove `MessagePlayer`, `MessageController`, `MessageTeam`, `MessageAll` and their call sites; `HUDClass` set to the production HUD)
- `Source/StealthGame/Private/StealthPlayerController.cpp` and `Public/StealthPlayerController.h` (remove `ClientStealthMessage`, `FStealthHudMessage`, `Messages`, `GetMessages`)
- `Source/StealthPresentation/Private/Tests/HUD/NoRemotePlayerStateReadLintTest.cpp` (class `FHudNoRemotePlayerStateReadLintTest`, name `StealthGame.HUD.NoRemotePlayerStateRead`; extends story 004's lint suite)
- `Source/StealthTestHarness/Private/HUD/PrototypeRetirementTest.cpp` (CQTest, `NETWORK_TEST_CLASS(PrototypeRetirement, "StealthGame.HUD.PIE")`; holds the one-to-one mappings of any first-playable test that cannot stay unchanged)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: prototype removal builds and tests hold
  - Given: the production HUD renders every cluster, and the prototype `AStealthHUD` canvas drawing and `MessagePlayer`, `MessageController`, `MessageTeam`, `MessageAll` are removed.
  - When: the project builds and the first-playable networked tests (names in `production/first-playable-report.md`) run.
  - Then: the project builds, `HUDClass` points at the production HUD, and each first-playable networked test passes unchanged, or the story lists a one-to-one mapping from it to a named `StealthGame.HUD.*` test that passes.
  - Edge cases: every test in the report must appear either as unchanged-and-passing or in the mapping.
- **AC-2**: prototype leak removed
  - Given: the HUD module sources and the whole tree.
  - When: the named lint and the story 004 include check run.
  - Then: no HUD-module file reads `AStealthPlayerState::Team` (or any `PlayerState` field) of a non-local pawn, and the include check passes on the whole tree.
  - Edge cases: the story names the lint (`StealthGame.HUD.NoRemotePlayerStateRead`). The leak was in the prototype `StealthHUD.cpp`.

QL-STORY-READY: ADEQUATE (qa-lead, 2026-10-25; criteria revised after the first pass and re-judged)

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: the lint test `Source/StealthPresentation/Private/Tests/HUD/NoRemotePlayerStateReadLintTest.cpp` (`StealthGame.HUD.NoRemotePlayerStateRead`), the unchanged first-playable tests plus `Source/StealthTestHarness/Private/HUD/PrototypeRetirementTest.cpp` for any mapped ones, and a green build log
- Gate: BLOCKING

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/hud/story-003-hud-input-sampling-and-host-parity-path.md`, `production/epics/hud/story-006-hud-layer-stack-root-and-input-isolation.md`, `production/epics/hud/story-022-hud-result-and-defence-plate-flow.md`, `production/epics/hud/story-035-hud-team-feed-queue-f8-and-removal-lines.md`, `production/epics/hud/story-040-hud-end-banner-f10-and-round-summary-logic.md`; camera-system `production/epics/camera-system/story-025-retire-prototype-camera.md` (parallel retirement)
- Unlocks: None within this epic (see the EPIC Definition of Done)
