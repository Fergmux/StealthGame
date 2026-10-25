# Story 045: PIE proof of rebuild without replay across disconnect, restart and RoundEpoch change

> **Epic**: HUD
> **Status**: Blocked — BLOCKED: spikes S2 (IVP story 002) and S14 (IVP story 003) have not reported (rejoin, seamless travel and restart behaviour)
> **Layer**: Presentation
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/hud.md`
**Requirement**: `TR-hud-005`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0001: Crowd identity rules (rejoin and carrier behaviour; conditional on spikes S2 and S14), with ADR-0021: UI/HUD layering and Common UI (HUD-R17 rebuild, tally cache) and ADR-0008: Test automation, leak and parity (the PIE harness)
**ADR Decision Summary**: The HUD rebuilds by re-sampling its carriers by value, so a rejoin shows the carriers' current values with no replay; the tally display cache keyed by `RoundEpoch` lives in `UStealthHudLocalStore`, a `ULocalPlayerSubsystem` that outlives the `PlayerController`, and only bridges until the C-TeamView owner re-push lands. ADR-0001 is Accepted conditional on spikes S2 and S14, which decide the rejoin, seamless-travel and restart behaviour this proof rests on.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: HIGH networking-domain story (ADR-0018 note: MotionWarping is Beta in 5.8.4 and the networking domain is HIGH risk). The rejoin, seamless-travel and "simulated client restart" paths depend on `PlayerState` trimming and possession behaviour that spikes S2 and S14 have not confirmed on 5.8.4, which is why the story is Blocked. The simulated restart must also recreate `UStealthHudLocalStore` state as a real restart would (a `ULocalPlayerSubsystem` lives for the local player, so a process restart loses it and a controller replacement does not); the harness hook chosen for "restart" must be recorded in the evidence. Tests are CQTest `NETWORK_TEST_CLASS` with the 60 Hz fixed-step determinism rule. `UE_LOGF` is preferred over `UE_LOG` in 5.8.

**Control Manifest Rules (this layer)**:
- Required: Presentation builds view-models by sampling its carriers every frame by value (the listen host runs the same path) — source: ADR-0021
- Forbidden: Never depend on a RepNotify having fired; never drive widgets event-style from RepNotify/RPC handlers — the host gets no RepNotify for its own writes — source: ADR-0021
- Guardrail: HUD ≤ 0.3 ms CPU (AC-HUD-22), ≤ 0.5 ms with markers; UI GPU ≤ 0.5 ms at 1080p on the reference GPU — source: ADR-0021

This story is the network proof of story 044's rebuild rules, run on real clients: a disconnect mid-plate with a rejoin inside the grace, a simulated client restart, and a `RoundEpoch` change during a removal card.

---

## Acceptance Criteria

*From GDD `design/gdd/hud.md` AC-HUD-19 and UX spec `design/ux/hud.md` UXA-HUD-31, scoped to this story:*

- [ ] **AC1** (AC-HUD-19, `StealthGame.HUD.PIE.RebuildNoReplay`, verbatim): **GIVEN** a client that disconnects mid-plate and rejoins inside the grace, and separately a `RoundEpoch` change during a removal card, **WHEN** the HUD rebuilds, **THEN** no plate, card, feed line or cue is replayed, persistent elements equal the carriers' current values, the tally equals the server's C-TeamView owner re-push (also after a simulated client restart) and never shows a lower value while the cache bridges, and 0 after the `RoundEpoch` change, and a first strike after the rejoin (whose `ClientCommandSeq` may restart) shows its plate.
- [ ] **AC2** (UXA-HUD-31, `StealthGame.HUD.PIE.RebuildNoReplay`): **GIVEN** the same rejoin scenario **WHEN** the dumps are read **THEN** after a disconnect and a rejoin inside the grace, no plate, card, feed line, ping ring or cue is replayed, the tally equals the server's value, and a first strike afterwards shows its plate.

---

## Implementation Notes

*Derived from ADR-0008 (PIE harness), ADR-0021 §4 and `design/gdd/hud.md` HUD-R17, HUD-R7, AC-HUD-19:*

**What to build.** One CQTest PIE scenario file with `NETWORK_TEST_CLASS(RebuildNoReplay, "StealthGame.HUD.PIE")`, full name `StealthGame.HUD.PIE.RebuildNoReplay.<Method>`. The 2v2 fixture, dump capture, the drop and rejoin hooks and the cue-event capture come from IVP story 001, story 003 of this epic and `production/epics/session-travel/story-077-silent-drop-and-rejoin-presentation.md`. No new HUD production code is expected; a failing case is fixed in story 044 (or the state machine it resets), not here.

**Scenarios.**
1. Rejoin inside the grace: a team-0 client disconnects while one of its own result plates is showing, rejoins inside the grace. Read the dump of the rebuilt HUD: no plate, no card, no feed line, no ping ring and no cue event is replayed (assert the cue-event list for the rebuild frames is empty), persistent elements equal the carriers' current values (brace, tally, objectives cluster, latency icon), and `PresentedOwnState` matches the current own state with no card.
2. Tally: the tally after the rejoin equals the server's C-TeamView owner re-push; assert it never reads lower than the last shown value while the cache bridges (sample every frame from the reconnect to the re-push landing). Repeat after a simulated client restart (the harness recreates the local player state so the controller handles and the `ClientCommandSeq` restart); the tally must still equal the server's re-push, bridged by the cache when it survives and by the re-push alone when the process is restarted.
3. `RoundEpoch` change during a removal card: change the epoch while a card is showing; assert the card, plates and feed lines are discarded with no exit animation, and the tally reads 0.
4. First strike after the rejoin: the rejoiner's `ClientCommandSeq` may restart; send a strike and assert its plate shows (not suppressed as a duplicate or as older than `seq_lastShown`, both cleared on reconnect).

**Determinism.** Fixed step 60 Hz, identical seeds, dumps keyed by tick index with wall-clock fields excluded. "No replay" is a negative assertion over every dump from the rejoin to the end of the case, not only the first frame, plus a control comparison for the rebuilt persistent elements.

**Blocking.** Rejoin, seamless travel and restart behaviour are what spikes S2 and S14 (IVP stories 002 and 003) decide; session-travel story 077 supplies the presentation hooks. Do not start until both spikes report. The test file may be drafted earlier but cases must not be recorded as passing on an unconfirmed rejoin model.

**Does not touch.** The rebuild logic (story 044), the tally logic and its cache (story 020), the reconnect feed lines (stories 037, 038).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/hud/story-044-hud-reset-and-rebuild-logic.md`: the rebuild logic under test.
- `production/epics/hud/story-020-hud-wrong-target-tally-logic.md`: the tally gate and display cache.
- `production/epics/hud/story-038-hud-reconnect-lines-pie.md`: the ally-side reconnect lines and their control comparison.
- `production/epics/session-travel/story-077-silent-drop-and-rejoin-presentation.md`: the Session presentation of a silent drop and rejoin.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthTestHarness/Private/HUD/RebuildNoReplayTest.cpp` (CQTest, `NETWORK_TEST_CLASS(RebuildNoReplay, "StealthGame.HUD.PIE")`, `StealthGame.HUD.PIE.RebuildNoReplay.*`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: AC-HUD-19 `RebuildNoReplay`
  - Given: a client disconnects mid-plate and rejoins inside the grace, and separately a `RoundEpoch` change during a card.
  - When: the HUD rebuilds.
  - Then: no plate, card, feed line or cue replays. Persistent elements equal the carriers. The tally equals the server's owner re-push (also after a simulated client restart), never shows lower while the cache bridges, and is 0 after the epoch change. A first strike after the rejoin (`ClientCommandSeq` may restart) shows its plate.
  - Edge cases: blocked on IVP stories 002 and 003 and session-travel story 077. DEFERRED until they report.
- **AC-2**: UXA-HUD-31
  - Given: the same rejoin scenario.
  - When: dumps are read.
  - Then: no plate, card, feed line, ping ring or cue replays, the tally equals the server's value, and a first strike afterwards shows its plate.
  - Edge cases: none.

DEFERRED: both criteria depend on IVP stories 002 and 003 (spikes S2 and S14) and session-travel story 077 reporting.

QL-STORY-READY: ADEQUATE

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/HUD/RebuildNoReplayTest.cpp`, test `StealthGame.HUD.PIE.RebuildNoReplay` OR a documented playtest. Gate level: BLOCKING (the Integration default).

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/hud/story-044-hud-reset-and-rebuild-logic.md`, `production/epics/hud/story-003-hud-input-sampling-and-host-parity-path.md`, `production/epics/hud/story-020-hud-wrong-target-tally-logic.md`; `production/epics/information-visibility-policy/story-002-spike-s2-seamless-travel-and-rejoin-playerstate-trims.md`, `story-003-spike-s14-s1-rejoin-and-pawnrecovery-possession.md`; `production/epics/session-travel/story-077-silent-drop-and-rejoin-presentation.md`
- Unlocks: `production/epics/hud/story-061-hud-mvp-exit-5v5-presentation-verification.md`
