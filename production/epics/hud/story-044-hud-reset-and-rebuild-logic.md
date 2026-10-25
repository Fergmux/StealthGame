# Story 044: HUD-R17 reset and rebuild: discard transients, rebuild persistent elements, no replay

> **Epic**: HUD
> **Status**: Ready
> **Layer**: Presentation
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/hud.md`
**Requirement**: `TR-hud-005`, `TR-hud-004`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0021: UI/HUD layering and Common UI (presenter core, HUD-R17 rebuild)
**ADR Decision Summary**: `FHudPresenterCore` owns every presentation state machine, HUD-R17 rebuild among them, and builds the view-model from a sampled `FHudInputs` snapshot every frame by value, so a rebuild is a re-sample, not an event replay; client-local stores that must outlive the controller (the tally display cache keyed by `RoundEpoch`, the third-person-hint profile flag) live in `UStealthHudLocalStore`, a `ULocalPlayerSubsystem`, never in a replicated value. Timelines are sampled from `now − t0`.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Engine surface: none (plain C++ core, no UWorld/UObject); the module still builds against 5.8.4 / C++20. The triggers (`RoundEpoch` change, travel, reconnect, `OnRosterReset`) arrive as plain fields or flags in `FHudInputs` sampled by story 003; this story never subscribes to an engine delegate. The local store is exercised through its plain interface with a fake in the test. Log with `UE_LOGF`.

**Control Manifest Rules (this layer)**:
- Required: Presentation builds view-models by sampling its carriers every frame by value (the listen host runs the same path) — source: ADR-0021
- Forbidden: Never depend on a RepNotify having fired; never drive widgets event-style from RepNotify/RPC handlers — the host gets no RepNotify for its own writes — source: ADR-0021
- Guardrail: HUD ≤ 0.3 ms CPU (AC-HUD-22), ≤ 0.5 ms with markers; UI GPU ≤ 0.5 ms at 1080p on the reference GPU — source: ADR-0021

This story implements HUD-R17 once, in the presenter core, so that every transient state machine built in stories 015, 020, 024, 035 and 037 shares one reset path. It proves the rules with fake carriers; the network proof is story 045.

---

## Acceptance Criteria

*From GDD `design/gdd/hud.md` HUD-R17, scoped to this story. AC1 includes the per-trigger resolution of the HUD-R17 / F2 tension (decision logged):*

- [ ] **AC1** (HUD-R17, `StealthGame.HUD.RebuildNoReplayCore` reset per trigger): **GIVEN** a `RoundEpoch` change, `OnRosterReset`, travel or reconnect **WHEN** the trigger fires **THEN** every showing plate, card and feed line, every pending feed line and every pending tally re-push hold is discarded (the `RoundEpoch`-keyed tally display cache of story 020 is retained), persistent elements are rebuilt from the carriers' current values with no animation and no cue, and `PresentedOwnState` is rebuilt from the current own `State` at once (no card). Per trigger (resolution of the HUD-R17 / F2 tension, decision logged): on `RoundEpoch` change, travel and reconnect every pending unshown result is also dropped (new controller or seq space); on a same-controller rebuild (`OnRosterReset`) an unshown pending result stays subject to `HudReplayWindowSec` (story 015).
- [ ] **AC2** (HUD-R17, `StealthGame.HUD.RebuildNoReplayCore` seq state and ping entries): **GIVEN** a duplicate-seq ring, `seq_lastShown`, an RPC-only result whose show time passed during a disconnect, and ping entries present at a rebuild **WHEN** reconnect, travel and `RoundEpoch` change occur in turn **THEN** the duplicate-seq ring and `seq_lastShown` are cleared on reconnect, travel and `RoundEpoch` change (a new controller may restart `ClientCommandSeq`); an RPC-only result whose show time passed during a disconnect is not replayed; ping entries present at a rebuild are marked shown and never drawn.
- [ ] **AC3** (HUD-R17 and HUD-R14, `StealthGame.HUD.RebuildNoReplayCore` rejoin and called-off): **GIVEN** a rejoiner after the grace (now an informant) and `Phase = Reset` with `bCalledOff` **WHEN** the HUD processes each **THEN** a rejoiner after the grace enters `InformantAwaiting` or `InformantBodied` with no card; `Phase = Reset` with `bCalledOff` clears every transient at once.

---

## Implementation Notes

*Derived from ADR-0021 Decision §4 and `design/gdd/hud.md` HUD-R17, HUD-R7, F2:*

**What to build.** A single `Rebuild(ERebuildTrigger)` path in `FHudPresenterCore` (`Source/StealthPresentation/Private/Hud/`), called from `Step` when `FHudInputs` reports a trigger, and an `ERebuildTrigger` enum `{ RoundEpochChange, OnRosterReset, Travel, Reconnect }`. Triggers are detected by value from `FHudInputs`: a `RoundEpoch` that differs from the last sampled one (C-TeamView), an `OnRosterReset` serial incremented by the roster client view, a travel-start flag, and a rebind of the own controller handles (`Bind`, which also covers a new `PlayerController`). Do not detect any of them from a delegate or RepNotify (the host gets none for its own writes).

**What every trigger does (HUD-R17).**
1. Discard every pending or showing transient: result and defence plates (story 015/022 state), the removal card (story 024), feed lines showing and pending (story 035, which includes reconnect lines from story 037, and calls the reconnect detector's `Reset()`), and every pending tally re-push hold (story 020). Discard with no exit animation (a `bCalledOff` or `RoundEpoch` change during a plate, card or feed line clears everything at once).
2. Retain the `RoundEpoch`-keyed tally display cache (the client-local copy in `UStealthHudLocalStore`, HUD-R7): it bridges a rebuild until the owner re-push lands and never overrides a re-push that passes the story 020 gate. A new `RoundEpoch` still shows 0 (story 020 owns that reset), so retention here is for the other triggers only.
3. Rebuild persistent elements (brace, tally, objectives cluster, latency icon, brackets) from the carriers' current values: no animation, no tick, no cue event in the dump for the rebuild frame.
4. Rebuild `PresentedOwnState` from the current own `State` at once, with no removal card. A rejoiner after the grace is, by then, an informant: it enters `InformantAwaiting` or `InformantBodied` (story 014/034) with the informant presentation and no card (GDD Open Question 6, edge case "If the owner rejoins after the grace").

**Per-trigger pending-result rule (decision logged).** HUD-R17 says to discard every pending element, while F2 says an RPC-only result may be replayed inside `HudReplayWindowSec` after a rebuild. The resolution: on `RoundEpoch` change, travel and reconnect the owner may be on a new `PlayerController` with a restarted `ClientCommandSeq`, so a pending unshown result cannot be matched to the new seq space and is dropped. On `OnRosterReset` the controller is the same, so an unshown pending result stays and remains subject to `HudReplayWindowSec` (default 1.0 s, hard range 0.5-3.0, read through `GetClientTuning()`; story 015 owns the replay-window rule itself). Live play never drops a late result; the window applies to rebuilds only. An RPC-only result whose show time passed during a disconnect is not replayed after a reconnect.

**Seq state.** The duplicate-seq ring (8 entries) and `seq_lastShown` of story 015 are cleared on reconnect, travel and `RoundEpoch` change. They are not cleared on `OnRosterReset`, where the controller and its seq space persist (consistent with the rule above). A first strike after the rejoin, whose `ClientCommandSeq` may restart, must therefore show its plate: after clearing, a result with seq 1 is not treated as a duplicate or as older than `seq_lastShown`.

**Ping entries.** At a rebuild, set `LastShownSeq := PingSeq` for every present ping entry (manifest rule: entries present at a rebuild are marked shown and never drawn; entries accepted afterwards show normally). The ring model itself is story 049; expose the hook it calls and assert in this story, with a fake ring model, that the rebuild marks every present entry shown and nothing is drawn.

**`bCalledOff`.** `Phase = Reset` with `bCalledOff` true clears every transient at once; the called-off line is then shown by the viewer state machine (story 014/042). It is the same discard path with no rebuild animation.

**Traps.** (1) Do not rebuild by replaying the carriers' history: sample current values only. (2) Do not clear the tally cache on reconnect, travel or `OnRosterReset`; it is keyed by `RoundEpoch`, so only an epoch change resets it. (3) The rebuild frame must not emit the cue events of the persistent elements it redraws. (4) Do not create a feed line for an ally whose reconnecting flag is already true at the rebuild (story 037's detector primes without a line). (5) Never read the raw own `State` for presentation after the rebuild: `PresentedOwnState` is rebuilt from it once, at once, and then follows HUD-R6a again.

**Does not touch.** The individual state machines (they expose `Discard()`/`Reset()` here), the network proof (story 045), Session's L4 overlays. Prototype: none of `Source/StealthGame/Private/StealthHUD.cpp` is promoted; the prototype has no rebuild logic.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/hud/story-045-hud-rebuild-no-replay-pie.md`: the PIE proof across disconnect, rejoin, restart and `RoundEpoch` change.
- `production/epics/hud/story-015-hud-result-show-time-f2.md`: F2 matching, the duplicate-seq ring and `HudReplayWindowSec` semantics.
- `production/epics/hud/story-020-hud-wrong-target-tally-logic.md`: the tally display cache and its `RoundEpoch` reset.
- `production/epics/hud/story-024-hud-removal-card-timing-f4-logic.md` and `story-035-hud-team-feed-queue-f8-and-removal-lines.md`: the card and feed state that this story discards.
- `production/epics/hud/story-037-hud-reconnect-lines-logic.md`: the reconnect edge detector this story resets.
- `production/epics/hud/story-049-hud-team-ping-rings-l0.md`: the ping ring model that marks entries shown.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthPresentation/Private/Hud/HudRebuild.h` (`ERebuildTrigger`, rebuild trigger detection)
- `Source/StealthPresentation/Private/Hud/HudRebuild.cpp`
- `Source/StealthPresentation/Private/Hud/HudPresenterCore.cpp` (`Rebuild` call from `Step`)
- `Source/StealthPresentation/Private/Tests/HUD/RebuildNoReplayCoreTest.cpp` (class `FHudRebuildNoReplayCoreTest`, name `StealthGame.HUD.RebuildNoReplayCore`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: Reset and rebuild per trigger
  - Given: a showing plate, card and feed line, a pending feed line, a pending tally re-push hold, an unshown pending result, and the tally display cache. Triggers: `RoundEpoch` change, travel, reconnect and `OnRosterReset`.
  - When: each trigger fires.
  - Then: every showing plate, card and feed line, every pending feed line and every pending tally re-push hold is discarded, and the `RoundEpoch`-keyed tally cache is retained. Persistent elements are rebuilt from the carriers' current values with no animation and no cue. `PresentedOwnState` is rebuilt from the current own `State` at once (no card). On `RoundEpoch` change, travel and reconnect every pending unshown result is also dropped. On `OnRosterReset` an unshown pending result stays subject to `HudReplayWindowSec` (story 015).
  - Edge cases: a new `RoundEpoch` still resets the cache to 0 (story 020).
- **AC-2**: Seq state and ping entries
  - Given: a duplicate-seq ring, `seq_lastShown`, an RPC-only result whose show time passed during a disconnect, and ping entries present at a rebuild.
  - When: reconnect, travel and `RoundEpoch` change occur in turn.
  - Then: the ring and `seq_lastShown` are cleared on each. The RPC-only result is not replayed. Ping entries are marked shown and never drawn.
  - Edge cases: a new controller may restart `ClientCommandSeq`.
- **AC-3**: Rejoin and called-off
  - Given: a rejoiner after the grace (now an informant), and `Phase = Reset` with `bCalledOff`.
  - When: the HUD processes each.
  - Then: the rejoiner enters `InformantAwaiting` or `InformantBodied` with no card. `bCalledOff` clears every transient at once.
  - Edge cases: none.

QL-STORY-READY: ADEQUATE (qa-lead, 2026-10-25; criteria revised after the first pass and re-judged)

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthPresentation/Private/Tests/HUD/RebuildNoReplayCoreTest.cpp`, test `StealthGame.HUD.RebuildNoReplayCore` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later). Gate level: BLOCKING (the Logic default).

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/hud/story-014-hud-presented-own-state-and-viewer-state-machine.md`, `production/epics/hud/story-015-hud-result-show-time-f2.md`, `production/epics/hud/story-020-hud-wrong-target-tally-logic.md`, `production/epics/hud/story-035-hud-team-feed-queue-f8-and-removal-lines.md`, `production/epics/hud/story-037-hud-reconnect-lines-logic.md`
- Unlocks: `production/epics/hud/story-045-hud-rebuild-no-replay-pie.md`, `production/epics/hud/story-049-hud-team-ping-rings-l0.md`
