# Story 021: PIE-2 end-to-end Cacao chain

> **Epic**: Objectives Chain
> **Status**: Ready
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/objectives-chain.md` (OC-16 "The Ruler's Cacao", OC-17; Edge Cases on the 1v0 dev round; AC-OC-14)
**Requirement**: `TR-oc-005` (completion gate and `DeclareObjectiveWin`), `TR-oc-002` (binding to a real layout)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0020: Objectives data and team-view sub-structs (Decision sections 3-7; Validation Criteria `EndToEndCacaoChain`)
**Other ADRs**: ADR-0002: Server resolution queue (latch, Event Log order); ADR-0008: Test automation and leak parity (T2 tier)
**ADR Decision Summary**: The full chain runs through the real systems: bind at `OnLayoutReady`, views pushed at `Active`, stage completions in the `HoldTimer(Complete)` slot unlocking the next stage per team, the final calling `DeclareObjectiveWin` in the same slot, the Round Event Log recording `ObjectiveStage` and `FinalCompleted` in key order, and the opponent's client never holding the final id.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (ADR-0020 MEDIUM; integration over Round, IIF, LSC and the carrier, so it inherits their unreported gates at close)
**Engine Notes**: CQTest PIE listen server with 2 clients under `-nullrhi` is ADR-0008 verification (1) [U]. Evidence screenshots follow `.claude/docs/run-and-observe.md`. This story is the epic's BLOCKING integration proof and the prototype behavioural reference (networked tests D and D2: full chain 1 to 2 to final to objective win; on a generated city) is re-expressed against the production path.

**Control Manifest Rules (this layer)**:
- Required: The final calls `DeclareObjectiveWin(Team, CurrentKey())` in the `HoldTimer(Complete)` slot — source: ADR-0020
- Required: Objectives Event Log entries go through the Round Event Log `Append` only — source: ADR-0020
- Forbidden: Never put the final before the team's preliminaries are complete in the view — source: ADR-0020
- Guardrail: Objectives at most 0.05 ms average on the listen host — source: ADR-0020

This is the epic's end-to-end proof: two players, one per team, play the shipped chain to a result. It is Integration BLOCKING (the story cannot close without it passing and its screenshots retained), and it is the only place the unit-tested parts are shown to compose with the real Round latch, the real IIF hold and the real carrier. The 1v0 dev-round clause pins the dev-only path (`bAllowEmptyTeamDev`) so the single-team case never publishes a rival signal and never pollutes the F5 metrics.

---

## Acceptance Criteria

*From GDD `design/gdd/objectives-chain.md` (AC-OC-14, Edge Cases 1v0 dev round), scoped to this story. QL-STORY-READY revised AC2 (enumerated evidence) and AC3 (the F5 exclusion mechanism named):*

- [ ] **AC1** (AC-OC-14 [Integration/BLOCKING, PIE-2] `StealthGame.Objectives.PIE.EndToEndCacaoChain`): **GIVEN** the graybox, 1v1, **WHEN** each player completes its two preliminaries and team 0 completes the final **THEN** `FRoundResult{ObjectiveWin, Winner 0}` (team 1 finishing its preliminaries after team 0 starts the final does not change the result; team 1 cannot complete the final).
- [ ] **AC2** (AC-OC-14, log and evidence): the Round Event Log holds exactly 4 `ObjectiveStage` and 1 `FinalCompleted` entries in key order, and team 1's client never held the final id; evidence screenshots are retained as `production/qa/evidence/oc-e2e-card-p1.png`, `oc-e2e-card-p2.png`, `oc-e2e-final-card.png` and `oc-e2e-result.png` (team 0 client at each moment, plus the team 1 client at the result), each showing the Cacao row text of the OC-16 table for that stage.
- [ ] **AC3** (GDD Edge Cases [PIE-2] `StealthGame.Objectives.PIE.OneVersusZeroDevRound`): **GIVEN** a 1v0 dev round (`bAllowEmptyTeamDev`) **THEN** the chain runs for the one team, `bRivalSignalEnabled` is forced false (no `RivalPrelimCount` is published, even if the preset sets it true) and the round is excluded from F5 metrics through a flag on the round record that the F5 calculator filters (a round without the flag counts as normal).

---

## Implementation Notes

*Derived from ADR-0020 Decision sections 3-7 and the GDD OC-16/OC-17:*

Flow for AC1: listen server with 2 clients on the graybox layout with the real `UObjectivesService`, Round, IIF and the carrier. Drive each player to its live court (the stage card and clue are team data; the test knows the assignment through the server query or the debug dump, not through a client), tend each stage to `D`, and complete the final for team 0. Real holds are used here, not only the debug path, because this is the end-to-end behaviour; the debug path may be used for a second, fast variant. The assertion is the Round result `FRoundResult{ObjectiveWin, Winner 0}`.

AC2: after the round ends, read the server Event Log (server-only) and assert exactly 4 `ObjectiveStage` (two per team) and 1 `FinalCompleted`, in key order. The team 1 client recorder asserts its replicated state never contained the final's court id or `DefIndex`. Screenshots (ring and cards are HUD work: the HUD epic provides the stage card; if it has not landed, capture the placeholder card and note it in the evidence file): `oc-e2e-card-p1.png` when the stage-1 card appears, `oc-e2e-card-p2.png` for stage 2, `oc-e2e-final-card.png` for the final card, `oc-e2e-result.png` at the result, plus the team 1 client at the result. Pass: all four files exist in `production/qa/evidence/`, card text matches the OC-16 table, team 1 never shows the final card.

AC3: `bAllowEmptyTeamDev` forces `bRivalSignalEnabled` false (GDD edge case, story 015 asserts the unit behaviour); the F5 exclusion is a round-record flag (name chosen at implementation, for example `bExcludeFromStallMetrics`) that story 017's calculator filters; a round without the flag counts as normal and a round with the flag set by the dev path is excluded.

Prototype replaced: the prototype's networked tests D and D2 and the host console commands that completed objectives; the production path replaces them (story 022 retires the prototype code).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-019-pie2-team-view-ingress-and-informant-parity.md` and `story-020-pie2-hidden-site-oracle-and-world-signature.md`: leak and oracle checks.
- `story-023-playtest-advisory-metrics.md`: pacing and exposure metrics over many rounds.
- HUD epic: drawing the stage card the screenshots capture.
- Round epic: the result presentation and round-end staging.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthTestHarness/Private/Tests/PIE/Objectives/EndToEndCacaoChainTest.cpp` (`NETWORK_TEST_CLASS(EndToEndCacaoChain, "StealthGame.Objectives.PIE")`)
- `Source/StealthTestHarness/Private/Tests/PIE/Objectives/OneVersusZeroDevRoundTest.cpp` (`NETWORK_TEST_CLASS(OneVersusZeroDevRound, "StealthGame.Objectives.PIE")`)
- `Source/StealthGameplay/Private/Features/Objectives/ObjectivesService.cpp` (1v0 handling, F5 exclusion flag)
- `production/qa/evidence/oc-e2e-card-p1.png`, `oc-e2e-card-p2.png`, `oc-e2e-final-card.png`, `oc-e2e-result.png`
- `production/qa/evidence/oc-e2e-evidence.md` (sign-off record)

---

## QA Test Cases

*Written by qa-lead at story creation (QL-STORY-READY: GAPS; the AC2 evidence list and the AC3 flag mechanism were applied above and the specs cover the revised text). The developer implements against these — do not invent new test cases during implementation.*

- **AC-1** (`PIE.EndToEndCacaoChain`): full chain to win
  - Given: graybox, 1v1 PIE, both players bound
  - When: each player completes its two preliminaries; team 0 then completes the final
  - Then: `FRoundResult{ObjectiveWin, Winner 0}`
  - Edge cases: team 1 finishing preliminaries after team 0 starts the final does not change the result; team 1 cannot complete the final.
- **AC-2** (`PIE.EndToEndCacaoChain` log and evidence): log contents and screenshots
  - Given: the same run; Event Log capture; team 1 client replicated-state recorder
  - When: the round ends
  - Then: Event Log holds 4 `ObjectiveStage` and 1 `FinalCompleted` entries in key order; team 1's client never held the final id
  - Edge cases: entry count exactly 5 objective entries; no entry out of key order.
- **AC-2 screenshots**: `production/qa/evidence/oc-e2e-{card-p1,card-p2,final-card,result}.png`
  - Setup: graybox 1v1 PIE, team 0 client as capture source, capture on each stage card appearing and on the result screen; capture team 1 client at the result
  - Verify: each card shows the Cacao row text for that stage; the result shows the `ObjectiveWin` for team 0
  - Pass condition: all four files exist in `production/qa/evidence/`; card text matches the OC-16 table; team 1 never shows the final card
- **AC-3** (`PIE.OneVersusZeroDevRound`): 1v0 dev round
  - Given: `bAllowEmptyTeamDev` true, one team only
  - When: the chain runs for that team
  - Then: the chain completes for that team; `bRivalSignalEnabled` is forced false and no `RivalPrelimCount` is published; the round is excluded from F5 metrics
  - Edge cases: `bRivalSignalEnabled` set true by preset is still overridden; flag not set means the round counts as normal.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: the two T2 PIE tests under `Source/StealthTestHarness/Private/Tests/PIE/Objectives/` — must exist and pass (BLOCKING)
- Retained screenshots: `production/qa/evidence/oc-e2e-{card-p1,card-p2,final-card,result}.png` and `production/qa/evidence/oc-e2e-evidence.md`

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/objectives-chain/story-009-final-completion-and-win-declaration.md`, `production/epics/objectives-chain/story-011-team-view-component-and-replication.md`, `production/epics/objectives-chain/story-017-stall-metrics-and-tend-posterior.md`, `production/epics/objectives-chain/story-019-pie2-team-view-ingress-and-informant-parity.md`, `production/epics/objectives-chain/story-020-pie2-hidden-site-oracle-and-world-signature.md`; cross-epic: round-match-framework (result and latch), level-socket-contract (graybox layout and `FSiteAssignment`), hud (stage card), input-interaction-framework (real holds)
- Unlocks: `production/epics/objectives-chain/story-022-prototype-retirement-and-static-scan.md`, `production/epics/objectives-chain/story-023-playtest-advisory-metrics.md`
