# Story 036: PIE proof that opponents never receive team lines and that no line is created after Ended

> **Epic**: HUD
> **Status**: Ready
> **Layer**: Presentation
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/hud.md` · UX spec `design/ux/hud.md`
**Requirement**: `TR-hud-001`, `TR-hud-002`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0001: Crowd identity rules (with ADR-0008 for the multi-client harness)
**ADR Decision Summary**: ADR-0001 gives a team's removal data only to that team (the Team-audience C-TeamView on the owning controller) so opponents have no carrier from which a line, card, count or cue could be built. ADR-0008 provides the multi-client and multi-process PIE harness that asserts what each client's HUD holds, compared by deterministic view-model dumps.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Runs as a 2v2 PIE at a fixed 60 Hz step, identical seeds, dumps keyed by tick index. AC-HUD-30 stages an objective win in the same drain as a rival strike so the State change and the retire update reach the viewer after `Phase = Ended`; this needs a drain-ordering hook in the harness (non-Shipping) and the teammate-markers behaviour at `Ended` (AC-TMK-34: the marker is `Absent` on the tick `Ended` arrives). The multi-process harness comes from `production/epics/information-visibility-policy/story-001-spike-s1-multiprocess-raw-bunch-capture.md` (ADR-0008). ADR-0001 is HIGH-risk on replication. `UE_LOGF` is preferred over `UE_LOG` in 5.8.

**Control Manifest Rules (this layer)**:
- Required: Presentation reads only its own controller's carriers, the GameState public structs and client-local queries — source: ADR-0001, ADR-0021
- Forbidden: Never iterate actors, read a pawn's `PlayerState`/`Controller`/`Owner`, call `IsPlayerControlled`/`IsBotControlled`/`CanViewerKnow`, include a `Public/Server/` header, or write anything replicated from Presentation — source: ADR-0021, ADR-0001
- Guardrail: HUD ≤ 0.3 ms CPU (AC-HUD-22), ≤ 0.5 ms with markers; UI GPU ≤ 0.5 ms at 1080p on the reference GPU — source: ADR-0021

Story 035 builds the feed queue and its rules as logic. This story proves them end to end on real clients: an opponent's removal reaches no opponent, an ally's removal gives exactly one identical line, and nothing is created once the round has ended.

---

## Acceptance Criteria

*From GDD `design/gdd/hud.md` AC-HUD-06 and AC-HUD-30 (verbatim) and UX spec UXA-HUD-15:*

- [ ] **AC1** (AC-HUD-06, `StealthGame.HUD.PIE.OpponentNoTeamLines`): **GIVEN** 2v2 PIE, **WHEN** a team-1 player is removed by each of strike, reversal and exile, **THEN** team-0 clients' dumps contain no feed line, card, count or cue for it, and the team-1 ally's dump contains exactly one line "{name} — eliminated for the round", identical text for the three reasons; at `Ended` (`EndRoundAll`) no client gains a line or card.
- [ ] **AC2** (AC-HUD-30, `StealthGame.HUD.PIE.RemovalAfterRoundEnd`): **GIVEN** a 2v2 PIE where a team-0 ally is struck by a rival strike whose State change and retire update reach the viewer after `Phase = Ended` (an objective win staged in the same drain), and a second case where a removal line is queued in F8's pending queue when `Ended` arrives, **WHEN** the viewer's dumps are inspected, **THEN** no feed line is created for that ally after the `Ended` tick, the pending queue is empty at that tick, lines already showing fade with the feed cluster over 600 ms, the banner's summary counts the ally as eliminated, and the ally's marker is `Absent` on the tick `Ended` arrives.
- [ ] **AC3** (UXA-HUD-15; retained screenshot `hud-feed-line-1080p.png`): **GIVEN** four ally removals in one update **WHEN** the feed is observed **THEN** three lines show at once and the fourth after at least 2.0 s (evicting the oldest); nothing is shown to a player of the other team; an ally feed line is captured at 1080p.

---

## Implementation Notes

*Derived from ADR-0001, ADR-0008, GDD HUD-R10, F8 and the round-end rule (cross-review S-W2), and UX element E06:*

**What to build.** Two CQTest PIE scenario files in `StealthTestHarness` (`NETWORK_TEST_CLASS(OpponentNoTeamLines, "StealthGame.HUD.PIE")`, `NETWORK_TEST_CLASS(RemovalAfterRoundEnd, "StealthGame.HUD.PIE")`) and one captured screenshot for AC3. No production code is written here; fix a defect in story 035's queue or the owning carrier story and re-run.

**OpponentNoTeamLines.** 2v2 PIE: team 0 (viewer clients V0, V1) and team 1 (P, Q). Remove P in turn by strike, reversal and exile (three runs or one run with respawn handling, whichever the harness supports). In every case: team-0 client dumps contain no feed line, no card, no count and no cue for P (compare to a control without the removal); Q's dump contains exactly one line "{name} — eliminated for the round" with identical text for all three causes; at `Ended` (`EndRoundAll`) no client gains a line or card (`RoundEnded` is not a removal).

**RemovalAfterRoundEnd.** Case 1: a team-0 ally is struck by a rival strike whose State change and retire update reach the viewer after `Phase = Ended` (stage an objective win in the same drain with the harness's drain-ordering hook). Assert: no feed line for that ally after the `Ended` tick; F8's pending queue is empty at that tick; showing lines fade with the feed cluster over 600 ms; the banner's `TeamSummary` counts the ally as eliminated (Round R9 (a)); and the ally's marker is `Absent` on the tick `Ended` arrives (AC-TMK-34). Case 2: a removal line is queued in the pending queue when `Ended` arrives (fill the feed with a burst larger than `HudFeedLineCount` first) and assert it is discarded.

**UX AC3.** Four ally removals in one update show three lines at once and the fourth after at least 2.0 s (evicting the oldest); a marker retiring without a state change creates no line; capture a feed line at 1080p as `hud-feed-line-1080p.png`. The widget (E06: 30 px high line plates, 5 px gap, right-aligned in the 420 px footprint, upward triangle 16 px Cochineal Bright, body 20 px Bone) is built by the layout stories; this story only captures and signs off.

**Determinism rule.** Fixed 60 Hz, identical seeds, dumps keyed by tick index, wall-clock fields excluded, TAA off for the screenshot.

**Traps.** The retire update and the State can arrive in either order relative to `Ended`; the staged same-drain case exercises the hardest ordering. The host's HUD must behave identically (host parity); include the listen host as a team-0 viewer where the harness allows. Do not assert on server stores: compare client dumps only (ADR-0008).

**Dependencies not in a story yet.** The teammate-markers epic (marker `Absent` on `Ended`, AC-TMK-34) has no stories; if its behaviour is missing when this story is picked up, mark AC2's marker clause as blocked in the evidence file rather than weakening the assertion.

**Prototype.** The prototype's message plumbing broadcast text to a team by iterating controllers on the server; none of it is promoted (story 059 removes it).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/hud/story-035-hud-team-feed-queue-f8-and-removal-lines.md`: the queue and line logic proved here end to end.
- `production/epics/hud/story-037-hud-reconnect-lines-logic.md` and `production/epics/hud/story-038-hud-reconnect-lines-pie.md`: reconnecting and back lines.
- `production/epics/hud/story-043-hud-round-summary-pie.md`: PIE proof of the round summary lines.
- `production/epics/hud/story-045-hud-rebuild-no-replay-pie.md`: rebuild replays no line.
- `production/epics/hud/story-003-hud-input-sampling-and-host-parity-path.md`: the sampling and host-parity path this test relies on.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthTestHarness/Private/HUD/OpponentNoTeamLinesTest.cpp` (CQTest, `StealthGame.HUD.PIE.OpponentNoTeamLines.*`)
- `Source/StealthTestHarness/Private/HUD/RemovalAfterRoundEndTest.cpp` (CQTest, `StealthGame.HUD.PIE.RemovalAfterRoundEnd.*`)
- `Source/StealthTestHarness/Private/HUD/HudPieFixtures.h` and `.cpp` (shared 2v2 fixture and drain-ordering hook; created by an earlier HUD PIE story if present, otherwise here)
- `production/qa/evidence/hud-feed-line-1080p.png`, `production/qa/evidence/hud-team-feed-opponent-and-round-end-pie-evidence.md` (retained evidence, not code)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: AC-HUD-06 `OpponentNoTeamLines`
  - Given: 2v2 PIE, a team-1 player removed by strike, reversal and exile in turn.
  - When: dumps are read.
  - Then: team-0 dumps contain no line, card, count or cue for it. The team-1 ally dump contains exactly one "{name} — eliminated for the round", identical for all three. At `Ended` no client gains a line or card.
  - Edge cases: none.
- **AC-2**: AC-HUD-30 `RemovalAfterRoundEnd`
  - Given: a team-0 ally struck by a rival strike whose State and retire reach the viewer after `Ended` (objective win staged in the same drain), and a second case with a line in the pending queue at `Ended`.
  - When: dumps are read.
  - Then: no line for that ally after the `Ended` tick. The pending queue is empty at that tick. Showing lines fade over 600 ms. The summary counts the ally eliminated. The marker is `Absent` on the `Ended` tick.
  - Edge cases: depends on the teammate-markers marker behaviour (AC-TMK-34).
- **AC-3**: UXA-HUD-15 feed line (Manual check)
  - Setup: four ally removals in one update. Capture an ally feed line at 1080p.
  - Verify: three lines at once, the fourth after at least 2.0 s evicting the oldest. Nothing appears for the other team.
  - Pass condition: `hud-feed-line-1080p.png` retained and matching.

QL-STORY-READY: ADEQUATE

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/HUD/OpponentNoTeamLinesTest.cpp` (`StealthGame.HUD.PIE.OpponentNoTeamLines`) and `Source/StealthTestHarness/Private/HUD/RemovalAfterRoundEndTest.cpp` (`StealthGame.HUD.PIE.RemovalAfterRoundEnd`) — must exist and pass; plus retained screenshot `production/qa/evidence/hud-feed-line-1080p.png` with sign-off in `production/qa/evidence/hud-team-feed-opponent-and-round-end-pie-evidence.md` (gate BLOCKING)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/hud/story-035-hud-team-feed-queue-f8-and-removal-lines.md`, `production/epics/hud/story-003-hud-input-sampling-and-host-parity-path.md`; teammate-markers epic (`production/epics/teammate-markers/EPIC.md`; no stories yet): the marker is `Absent` on the tick `Ended` arrives, AC-TMK-34
- Unlocks: `production/epics/hud/story-038-hud-reconnect-lines-pie.md`, `production/epics/hud/story-060-hud-run-and-observe-2v2-evidence.md`, `production/epics/hud/story-061-hud-mvp-exit-5v5-presentation-verification.md`
