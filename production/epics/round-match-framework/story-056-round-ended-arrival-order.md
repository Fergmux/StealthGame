# Story 056: With Roster/GameState arrival skew a strike win changes no ally marker or HUD state before Phase = Ended

> **Epic**: Round & Match Framework
> **Status**: Complete
> **Layer**: Core
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: 2026-10-26

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-004` (Single-assignment latch and the two-step round end) and `TR-round-009` (`FRoundPublicState` identical bytes for every viewer; bounded late-loader reveal). Also `GDD AC 12d` ([PIE-2]), `R9` ("Arrival order" paragraph), Teammate Markers TM-R5 and the HUD "States and Transitions".
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0002: Server resolution queue, server clock and round-end staging; ADR-0003: Replication, relevancy and the concealment cull hybrid
**ADR Decision Summary**: ADR-0002: one total-order queue per round, typed staged writes and the R9 two-step round end; section 8.5 states the client arrival rule: the GameState and the Roster carriers are different actors, so a client may receive `RoundEnded` records a net frame before `Phase = Ended`, and consumers (Teammate Markers TM-R5, HUD states) treat a `RoundEnded` received while their phase is not yet `Ended` as no change and move only when the phase does. ADR-0003: uniform pawn net configuration, quantised server cull plus client-local hide, fixed-width serialisers and an allowlist audit of every replicated property; here it covers the replicated round struct and carriers.
**ADR Version**: ADR-0002 2026-10-24; ADR-0003 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (ADR-0002, governing; ADR-0003 is HIGH)
**Engine Notes**: ADR-0002 uses no post-cutoff API. ADR-0003's post-cutoff items (`SetNetUpdateFrequency` and the other actor net-config setters, `UNetDriver::GetNetServerMaxTickRate`, `APawn::RemoteViewPitch16`) are not touched here, and its spikes S9, S10, S12, S13 are not touched: this is a two-client PIE test of the ordering of two replicated carriers, not of relevancy, cull or CMC cadence. PIE runs in a single process, so no seamless travel is involved. Whether replicated-actor arrival order across a forced net update can be skewed deterministically by a test-only delay is an assumption of the harness (see Implementation Notes), not an ADR fact.

**Control Manifest Rules (this layer)** (the Round-relevant rules sit in the manifest's Foundation Layer Rules, Round phase flow, Replication and Forbidden; the Core Layer Rules section holds no Round-specific rule):
- Required: `FRoundPublicState` is replicated unconditionally (identical bytes for every viewer); every Round mutation forces a GameState net update, and the host calls the client handler explicitly (ADR-0002).
- Forbidden: Never end the round in one step at the latch, or through an `OnRoundEnded` subscriber chain (ADR-0002); the arrival rule exists because the two carriers do not arrive together.
- Guardrail: Identity policy: C-TeamView pushes change-only, at most 1 per frame, within the 50 KB/s per-client budget (ADR-0001). The round-end step forces one update on each of at most 10 Roster records plus the GameState (R9 round-end step budget; verified by story 092).

**Prototype disposition**: none (new code). The prototype replicates one `StealthGameState` with its win fields and has no separate Roster carrier, so it has no arrival-skew problem to preserve.

---

## Acceptance Criteria

*From GDD `design/gdd/round-match-framework.md` AC 12d, scoped to this story:*

- [x] GIVEN two clients with injected arrival skew between the GameState and the Roster records (Roster first by one net frame), WHEN a strike decides the round, THEN no ally marker disappears or changes fill and the HUD viewer state does not change before that client's `Phase = Ended` (TM-R5, HUD States).

---

## Implementation Notes

*Derived from ADR-0002 section 8.4 and 8.5, and GDD R9 "Arrival order":*

- What Round guarantees: in the round-end step the GameState (`FRoundPublicState`, phase `Ended`) and every Roster record written by `EndRoundAll()` leave in the same server frame, each with a forced net update (R9 a, b). They are different actors, so a client may still receive the `RoundEnded` records one net frame before `Phase = Ended`. Round cannot change that ordering; the rule (ADR-0002 8.5) is that the consumers treat a `RoundEnded` arriving while their phase is not yet `Ended` as no change.
- Strike scope: the case matters for a strike win because in the interval between the latch step and the death write nothing observable may change (story 053); the skewed arrival at the round-end step must not reintroduce an early change. The test therefore runs a strike-decided round (a stub or debug strike, `StrikeNetLeadSec` staging as in story 053), then samples each client every frame from the round-end step until its own `Phase = Ended` has arrived.
- Skew injection: a test-only delay on the GameState's replication to one client (so the Roster carrier, the C-TeamView part and PlayerState delta, reaches that client first by one net frame). QA's note: the hook is test-only; it lives in the harness, never in a production module (`StealthTestHarness` is never in a Target `ExtraModuleNames`, ADR-0007). Open: confirm the mechanism (for example a harness-side net-driver packet delay or a replication-graph hold on the GameState) when implemented; ADR-0002 and ADR-0003 do not specify one.
- What is sampled: per client, per frame, (1) every ally marker's presence and fill (Teammate Markers TM-R5 state), (2) the HUD viewer state, from the phase-gated view model that consumes `RoundEnded` records. If the HUD and the Teammate Markers consumers do not exist yet in this epic's timeline, assert at the viewer-state model level instead: the viewer-state function that the consumers use maps (phase, record state) to marker and HUD state, and it must return the previous state while the client's phase is not yet `Ended` (Open: confirm against the HUD and Teammate Markers epics which of the two levels is available when this story is built; QA's spec allows the model level).
- The consumer implementations of TM-R5 and the HUD states live in the Teammate Markers and HUD epics. This story carries the test and any Round-side piece that exposes the client's replicated phase for them (the `FRoundPublicState` handler and `PhaseChangeCount`, story 029); it does not implement marker or widget logic.
- Test form: multi-client PIE (2 clients) declared with `NETWORK_TEST_CLASS(RoundEndedArrivalOrder, "StealthGame.Round.PIE")`, full name `StealthGame.Round.PIE.RoundEndedArrivalOrder.<Method>` per the CQTest exemption in `.claude/rules/test-standards.md`; the GDD scenario name `StealthGame.Round.RoundEndedArrivalOrder` is the scenario.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 053 (`production/epics/round-match-framework/story-053-strike-win-staging.md`): the server-side staging of the strike win between the latch step and the death write.
- Story 052 (`production/epics/round-match-framework/story-052-ended-latch-effects.md`) and story 029 (`production/epics/round-match-framework/story-029-public-state-replication.md`): the round-end step effects and the `FRoundPublicState` replication and handler.
- Story 080 (`production/epics/round-match-framework/story-080-no-leak-before-ended.md`): replicated-property allowlist and identical bytes on 3 clients.
- Teammate Markers TM-R5 and HUD state implementation (their own epics): the consumers of the arrival rule.
- Roster record replication internals and C-TeamView (participant-state-roster epic).

---

## Files

*Planned files. Tests live under the engine test roots (`Source/<Module>/Private/Tests/`, see `tests/README.md`), not under `tests/`. `/dev-story` replaces this list with the files it actually changed.*

- `Source/StealthTestHarness/Private/Round/RoundEndedArrivalOrderTest.cpp` (CQTest `NETWORK_TEST_CLASS(RoundEndedArrivalOrder, "StealthGame.Round.PIE")`, scenario `StealthGame.Round.RoundEndedArrivalOrder`)
- `Source/StealthTestHarness/Private/Round/` test-only replication-skew hook (new; Open: name set when implemented)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: With Roster/GameState arrival skew, a strike win changes no ally marker or HUD state before Phase = Ended.
  - Given: two clients with injected skew (Roster record arrives first by one net frame).
  - When: a strike decides the round.
  - Then: no ally marker disappears or changes fill before that client's `Phase = Ended`.
  - Then: the HUD viewer state does not change before that client's `Phase = Ended`.
  - Edge cases: sample per frame on each client; if the HUD does not exist yet in this epic, assert at the viewer-state model level (Open: confirm); the skew injection hook is a test-only delay on GameState replication.

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/Round/RoundEndedArrivalOrderTest.cpp` — must exist and pass (CQTest network test, scenario `StealthGame.Round.RoundEndedArrivalOrder`, full name `StealthGame.Round.PIE.RoundEndedArrivalOrder.<Method>`)

**Status**: [x] Created and passing (`StealthGame.Round.RoundEndedArrivalOrder`, 2026-10-26)

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-053-strike-win-staging.md`
- Unlocks: None

Cross-epic: `production/epics/participant-state-roster` (`EndRoundAll`, Roster records and C-TeamView carriers; stories not yet created); `production/epics/tuning-data-config` (module skeleton and StealthTestHarness bootstrap; stories not yet created)

## Completion Notes
**Completed**: 2026-10-26 (RMF round-end and disconnect batch: stories 048-065, two-step round end, TeamSummary, grace/forfeit/void)
**Criteria**: 1/1 passing at the model level (strike-decided round; client A receives RoundEnded one net frame before Ended: no marker/viewer change before its Ended)
**Deviations**: Model-level [U] test instead of the 2-client PIE NETWORK_TEST_CLASS with a replication-skew hook (no Teammate Markers or HUD consumer exists; the story's QA spec allows the model level); the rule is StealthRoundArrival::ResolveRecordState in Foundation View
**Test Evidence**: `Source/StealthGameplay/Private/Tests/Round/RoundEndedArrivalOrderTest.cpp`; full suite: `production/qa/evidence/rmf-round-end-disconnects-2026-10-26/full-suite.txt`
**Run result**: N/A - model level; no consumer exists yet
**Reviewed by**: lead-programmer focused review (sonnet) + self-review, RMF round-end batch; findings and fixes in production/qa/evidence/rmf-round-end-disconnects-2026-10-26/evidence.md
