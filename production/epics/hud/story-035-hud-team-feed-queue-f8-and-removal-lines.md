# Story 035: F8 feed queue and the HUD-R10 ally removal lines

> **Epic**: HUD
> **Status**: Ready
> **Layer**: Presentation
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/hud.md`
**Requirement**: `TR-hud-001`, `TR-hud-004`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0001: Crowd identity rules (with ADR-0021 for the presenter core and feed)
**ADR Decision Summary**: ADR-0001 defines four audience carriers: an ally's removal is Team-audience data on the viewer's own C-TeamView, and opponents have no carrier at all, so the HUD can show a removal line to allies only by reading that carrier. ADR-0021 puts F8 (the team feed queue) in `FHudPresenterCore`, consuming the markers presenter's client-local `OnAllyMarkerRetired(PublicHandle)` delegate (TM-R14) in the same module.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Engine surface: none (plain C++ core, no UWorld/UObject); the module still builds against 5.8.4 / C++20. `OnAllyMarkerRetired` is a native client-local delegate on the markers presenter (ADR-0021 §5), never replicated and never called by server code; the core sees it only as an input event, and the teammate-markers epic has no stories yet, so tests use a fake source. Names resolve from C-Public `DisplayName` by `PublicHandle`, never from the wire. ADR-0001 is HIGH-risk on replication.

**Control Manifest Rules (this layer)**:
- Required: Presentation reads only its own controller's carriers, the GameState public structs and client-local queries — source: ADR-0001, ADR-0021
- Forbidden: Never depend on a RepNotify having fired; never drive widgets event-style from RepNotify/RPC handlers — the host gets no RepNotify for its own writes — source: ADR-0021
- Guardrail: HUD ≤ 0.3 ms CPU (AC-HUD-22), ≤ 0.5 ms with markers; UI GPU ≤ 0.5 ms at 1080p on the reference GPU — source: ADR-0021

Allies are entitled to know a teammate was removed (IVP R3); opponents receive nothing. This story builds the queue that turns ally removals into at most `HudFeedLineCount` lines with a dwell floor, and the rules that decide when a removal line exists at all, including the round-end rule that stops late lines after `Ended`.

---

## Acceptance Criteria

*From GDD `design/gdd/hud.md` AC-HUD-16 (verbatim), HUD-R10 and the round-end rule (cross-review S-W2), scoped to this story:*

- [ ] **AC1** (AC-HUD-16, F8; `StealthGame.HUD.FeedQueue`): four ally removals in one update at t = 50.0 → lines A, B, C at 50.0; D at 52.0 evicting A; B and C gone at 56.0; D gone at 58.0; with `HudFeedLineCount` 1: A 50.0, B 52.0, C 54.0, D 56.0, D gone at 62.0; a marker retire without a State change creates no line; a line waits for `OnAllyMarkerRetired` when it fires after the State.
- [ ] **AC2** (HUD-R10; `StealthGame.HUD.FeedQueue` line cases): **GIVEN** a removal by strike, reversal, exile and forfeit, `OnAllyMarkerRetired` before and after the State, and a `RoundEnded` written by `EndRoundAll` **WHEN** lines are created **THEN** one line "{name} — eliminated for the round" is created, identical for all four causes, at the later of the State arrival and the ally's `OnAllyMarkerRetired`; `RoundEnded` written by `EndRoundAll` never creates a line or a card.
- [ ] **AC3** (HUD-R10 round-end rule, cross-review S-W2; `StealthGame.HUD.FeedQueue` round-end case): **GIVEN** queued and showing lines when `Phase = Ended` arrives **WHEN** the banner enters **THEN** no line of any kind is created, the pending queue is discarded (empty at that tick), and lines already showing fade with the feed cluster over 600 ms.

---

## Implementation Notes

*Derived from ADR-0021 §4 and §5, GDD F8, HUD-R10, HUD-R14 and UX element E06:*

**What to build.** The F8 queue and the HUD-R10 line rules inside `FHudPresenterCore` (`Source/StealthPresentation/Private/Hud/`), as a separate small class `FHudFeedQueue` the core owns. The widget (E06) is a later story; this story produces the view-model's feed sub-struct (line texts as string-table keys plus the resolved name, enum line type, entrance and exit alphas, ages).

**F8 (formula).** Each line has `t_in` (F3 eased entrance, 400 ms ease-out with an 8 px settle) and lives `HudFeedLineLifeSec` (6.0 s default, x `HudReadHoldMult`); at most `HudFeedLineCount` (3 default) lines show; a new line, when full, evicts the oldest line only if that line has shown for at least `HudFeedMinDwellSec` (2.0 s), otherwise it waits in a pending queue of at most 4 (the most a team of 5 can produce at once) until that holds. All three keys come from story 001's tuning data; nothing hardcoded. Expected burst timing: `b = 4`, `c = 3` makes the last line wait 2 s; worst case `c = 1`, dwell 4.0 gives 12 s. Exit 600 ms.

**HUD-R10 rule.** When a teammate's C-TeamView `State` changes `Active → Informant`, allies (Active and Informant viewers) get one line "{Ally name} — eliminated for the round" (`Hud.Feed.Removed`), identical text for every reason (strike, reversal, exile, forfeit). The line is created at the later of the State's arrival and that ally's `OnAllyMarkerRetired(PublicHandle)` (TM-R14, which fires exactly once per retirement, also for an undrawn marker, and immediately if no marker existed). A marker retirement alone (for example a release to the crowd on disconnect) never creates a line. The name comes from C-Public `DisplayName` by `PublicHandle`; an unresolved name reads "Ally" (`Hud.Ally.Unresolved`). Opponents receive nothing (no carrier exists, so there is nothing to suppress). `RoundEnded` states written to every record by Round's `EndRoundAll` at `Ended` (Round R9) are never removals: no line and no card.

**Round-end rule (S-W2).** Once `Phase = Ended` arrives (the round-end step), the HUD creates no feed line of any kind and discards every line still waiting: a removal line pending on `OnAllyMarkerRetired`, every entry in F8's pending queue, and any line due to a State or flag change that arrives after the phase. `FRoundResult.TeamSummary` already counts every ally eliminated at that step, so the banner's summary is the record. Lines already showing fade with the feed cluster over 600 ms when the banner enters.

**Reconnect lines are not here.** The "reconnecting" and "back" lines (HUD-R10a, driven by the teammate `bReconnecting` bit and `ReconnectEndCause`) share this queue but are story 037's logic; build the queue so a line carries a type enum and story 037 only adds producers. A rebuild replays no line (HUD-R17, story 044).

**Tests.** `Source/StealthPresentation/Private/Tests/HUD/FeedQueueTest.cpp`, class `FHudFeedQueueTest`, name `StealthGame.HUD.FeedQueue`. Use a fake clock and a fake `OnAllyMarkerRetired` source (the teammate-markers epic has no stories yet, so the fake must implement the TM-R14 contract: fires once per retirement, immediately if no marker existed). Cover the `HudFeedLineCount` 1 variant, a retire without a State change, a retire that fires after the State, all four removal causes producing identical text, and `Ended` with both queued and showing lines.

**Traps.** Never drive a line from a RepNotify: the core samples carriers every frame and detects the State edge by value, so the listen host works identically. A State edge seen only on a rebuild (reconnect, travel, late join) creates no line. The em dash in the line text is the approved character of the UX string table, not a hyphen.

**Prototype.** The prototype HUD's message plumbing (`MessageTeam`, `MessageAll`) writes text lines with no queue, dwell or round-end rule; it is retired by story 059 and not promoted.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/hud/story-036-hud-team-feed-opponent-and-round-end-pie.md`: PIE proof that opponents get no lines and that none are created after `Ended`.
- `production/epics/hud/story-037-hud-reconnect-lines-logic.md`: reconnecting and back lines (HUD-R10a).
- `production/epics/hud/story-044-hud-reset-and-rebuild-logic.md`: rebuild replays nothing, reset clears the queue.
- `production/epics/hud/story-040-hud-end-banner-f10-and-round-summary-logic.md`: the banner whose entrance triggers the 600 ms feed fade.
- `production/epics/hud/story-005-hud-string-table-and-language-lint.md`: the `Hud.Feed.Removed` string and its lint.
- The feed line widget (UX E06) is delivered with the layout stories; this story ends at the view model.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthPresentation/Private/Hud/HudFeedQueue.h` and `.cpp` (`FHudFeedQueue`, F8 and the HUD-R10 line rules)
- `Source/StealthPresentation/Private/Hud/HudPresenterCore.cpp` (wire the queue into `Step`)
- `Source/StealthPresentation/Private/Hud/HudInputs.h` (ally State edges, `OnAllyMarkerRetired` events, `Phase`, if not present)
- `Source/StealthPresentation/Private/Tests/HUD/FeedQueueTest.cpp` (class `FHudFeedQueueTest`, name `StealthGame.HUD.FeedQueue`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: `FeedQueue`
  - Given: four ally removals in one update at t=50.0 (`HudFeedLineCount` 3).
  - When: time steps.
  - Then: A, B, C at 50.0. D at 52.0, evicting A. B and C gone at 56.0, D at 58.0. With count 1: A 50.0, B 52.0, C 54.0, D 56.0, D gone at 62.0. A marker retire without a State change makes no line. A line waits for a late `OnAllyMarkerRetired`.
  - Edge cases: use a fake `OnAllyMarkerRetired` source (the teammate-markers epic has no stories).
- **AC-2**: HUD-R10 lines
  - Given: a removal by strike, reversal, exile and forfeit. `OnAllyMarkerRetired` before and after the State. A `RoundEnded` from `EndRoundAll`.
  - When: lines are created.
  - Then: one line "{name} — eliminated for the round", identical for all four causes, created at the later of State arrival and marker retire. `EndRoundAll` creates no line or card.
  - Edge cases: none.
- **AC-3**: round-end rule
  - Given: queued and showing lines when `Phase = Ended` arrives.
  - When: the banner enters.
  - Then: no new line, the pending queue is empty, and showing lines fade with the feed cluster over 600 ms.
  - Edge cases: none.

QL-STORY-READY: ADEQUATE

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthPresentation/Private/Tests/HUD/FeedQueueTest.cpp` (`StealthGame.HUD.FeedQueue`) — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`; gate BLOCKING)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/hud/story-002-hud-view-model-and-presenter-core-shell.md`, `production/epics/hud/story-005-hud-string-table-and-language-lint.md`, `production/epics/hud/story-014-hud-presented-own-state-and-viewer-state-machine.md`; teammate-markers epic (`production/epics/teammate-markers/EPIC.md`; no stories yet): the client-local `OnAllyMarkerRetired(PublicHandle)` delegate, TM-R14; `production/epics/participant-state-roster/story-021-retire-at-server-time-source.md`
- Unlocks: `production/epics/hud/story-036-hud-team-feed-opponent-and-round-end-pie.md`, `production/epics/hud/story-037-hud-reconnect-lines-logic.md`, `production/epics/hud/story-044-hud-reset-and-rebuild-logic.md`, `production/epics/hud/story-046-hud-host-parity-pie.md`, `production/epics/hud/story-049-hud-team-ping-rings-l0.md`, `production/epics/hud/story-053-hud-no-clock-no-counts-pie.md`, `production/epics/hud/story-058-hud-performance-capture.md`, `production/epics/hud/story-059-hud-retire-prototype-canvas-and-message-plumbing.md`
