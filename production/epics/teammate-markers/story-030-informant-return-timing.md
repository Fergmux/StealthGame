# Story 030: Informant return timing

> **Epic**: Teammate Markers
> **Status**: Blocked — BLOCKED: ADR-0003 spikes S9 and S10 have not reported (the marker leaves on the retiring C-TeamView update and the takeover pawn arrives through a re-opened channel), and ADR-0019's amendment requests to owning ADRs are outstanding (conditional acceptance); the npc-kill-penalty-informants epic has no stories yet. Update 2026-10-25: spike S10 reported FAIL (production/qa/spikes/2026-10-25_S10.md) — the in-flight cull/un-cull race can leave a client without the pawn (~1-2 % under loss), the re-created pawn renders one un-evaluated frame, and presentation times need ADR-0002's clock estimator; a superseding ADR to ADR-0003 must decide the fixes before this story can proceed.
> **Layer**: Presentation
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

The third anchor moment of the Player Fantasy: your teammate is struck down, their marker leaves as the blow lands, and a little later an outline pentagon appears over a stranger near your spawn: they are back, as a spy in the crowd. This story measures it on a real network. The marker's exit is keyed on the team-only `RetireAtServerTime` (the kill's contact time), so at defaults an open kill gives a marker half faded at the contact frame and gone 0.15 s after it; the informant takeover then binds an unseen crowd pawn and the marker fades in outline-only on it, never on the corpse. Opponents see no marker before, during or after. The runs repeat 20 times under the leak network profile because the "leaves as the blow lands" target is a statistical, not a single-run, property.

**GDD**: `design/gdd/teammate-markers.md` (TM-R6 steps 1-4, TM-R7, F6, Game Feel; AC-TMK-13)
**Requirement**: `TR-tmk-007` (and `TR-tmk-002` for the null semantics at takeover)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0021: UI/HUD layering, CommonUI and Enhanced Input (section 5)
**Also governed by**: ADR-0019: Informant lifecycle and takeover (Accepted, conditional); ADR-0002 (`EstimatedServerNow()`); ADR-0003
**ADR Decision Summary**: Presentation timing uses `EstimatedServerNow()`; the retiring update carries the team-only `RetireAtServerTime` and the takeover's new `PawnRef` arrives through the same C-TeamView entry.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: "Locally rendered contact frame" is the frame on which the viewer's client renders the strike's contact; the test marks it from the victim pawn's `FActionState` contact time on `EstimatedServerNow()` and a captured frame index. `LeakNetProfile` is PktLag 100 ms, 2 % loss.

**Control Manifest Rules (this layer)**:
- Required: Presentation timing uses only `EstimatedServerNow()`; timelines are sampled from `now - t0` in C++ — source: ADR-0021, ADR-0002
- Forbidden: Never show an informant marker over a pawn the viewer's client cannot resolve; never render a marker on a corpse past its hold — source: GDD TM-R6, TM-R7
- Guardrail: Gone no later than 0.35 s after the contact frame in every run; at least partly visible at contact in at least 18 of 20 runs

---

## Acceptance Criteria

*From GDD `design/gdd/teammate-markers.md` (AC-TMK-13, F6 examples), scoped to this story:*

- [ ] **AC1** (AC-TMK-13, [PIE-2][Net] `InformantReturn`): **GIVEN** an ally killed in the open, under `LeakNetProfile`, repeated 20 times **WHEN** the informant takeover binds a crowd pawn **THEN** in every run the viewer's marker is fully gone no later than 0.35 s after the locally rendered contact frame, is still at least partly visible at the contact frame in at least 18 of 20 runs (the "leaves as the blow lands" target; up to 2 runs may fall short), and never reappears on the corpse.
- [ ] **AC2** (AC-TMK-13): **GIVEN** the same runs **THEN** the marker fades in outline-only on the adopted pawn, and an opponent client sees no marker before, during or after, across all 20 runs.
- [ ] **AC3** (F6 examples): **GIVEN** a bush-strike kill and an exile release **THEN** the exit starts at +0.60 s and +0.25 s respectively after `t_r`, and the bush-strike marker is half faded at contact.

---

## Implementation Notes

*Derived from F6 and AC-TMK-13:*

Script 20 open kills of an ally with a viewer ally, an informant respawn on the ally's team and an opponent client observing. Per run record: contact frame (from the strike's contact time mapped to the viewer's rendered frame), the marker alpha per frame, the first frame at which alpha is 0, whether the marker was drawn over the corpse after its hold, the new marker's pawn and fill, and the opponent's marker count. Pass: max gone-after-contact <= 0.35 s over 20 runs, partly visible at contact in >= 18, never on the corpse, outline-only on the new pawn, opponent count always 0. For AC3 reuse the F6 worked examples (bush strike `t_R` about `t_r` + 0.75 gives exit at 0.60 s; release at defaults gives exit at 0.25 s).

If runs miss the 0.35 s bound, the cause is usually the clock mapping or the replication of `RetireAtServerTime`, not the marker module; check roster story 021 and the clock first and file against the owner.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-007-retire-contact-alignment.md`: the timing logic.
- `story-029-feed-handoff-and-null-ref-on-real-connection.md`: the null-reference semantics.
- the npc-kill-penalty-informants epic: the informant respawn and takeover itself.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthTestHarness/Private/Markers/InformantReturnTest.cpp` (CQTest, `StealthGame.Markers.PIE.InformantReturn.*`)
- `production/qa/logs/` (per-run timing table, 20 rows)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC1**: Given: an ally killed in the open under `LeakNetProfile`, 20 runs. When: the informant takeover binds a crowd pawn. Then: every run has the marker fully gone within 0.35 s of the locally rendered contact frame; at least 18 of 20 runs show it still partly visible at contact; it never reappears on the corpse. Edge cases: the 2 allowed runs below the "partly visible" bar.
- **AC2**: Given: the same runs. When: the pawn is adopted. Then: the marker fades in outline-only on the adopted pawn, and an opponent client shows no marker before, during or after. Edge cases: the opponent's capture covers all 20 runs.
- **AC3**: Given: a bush-strike kill and an exile release. When: each retirement runs. Then: the exit starts at +0.60 s and +0.25 s respectively, and the kill's marker is half faded at contact. Edge cases: `RetireAtServerTime` = 0 as in story 007.

QL-STORY-READY: ADEQUATE

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: the CQTest `InformantReturnTest.cpp` with the per-run timing table OR a documented playtest

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/teammate-markers/story-006-lifecycle-retire-hold-takeover-direct-switch.md`, `story-007-retire-contact-alignment.md`, `story-019-paint-markers-in-world-anchor-leaf.md`; cross-epic `production/epics/information-visibility-policy/story-004-spike-s9-cull-latency-and-explicit-channel-close.md`, `story-005-spike-s10-reopen-behaviour-and-netguid.md`, `production/epics/participant-state-roster/story-021-retire-at-server-time-source.md`, and the npc-kill-penalty-informants and assassination-defence-reversal epics (stories not yet created)
- Unlocks: None
