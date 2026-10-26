# Story 029: FRoundPublicState replicates with Unknown = 0, survives seamless travel and LastResult survives into Lobby

> **Epic**: Round & Match Framework
> **Status**: Complete
> **Layer**: Core
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25f
> **Last Updated**: 2026-10-26

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-009` (`FRoundPublicState` identical bytes for every viewer; `ActiveAtServerTime` zeroed in Active; bounded late-loader reveal), `TR-round-001` (Five-phase machine in a game-instance store surviving travel); GDD AC 57; rules R20, R21, R4.
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0003: Replication, relevancy and the concealment cull hybrid; ADR-0005: Session, seamless travel and match-scoped state stores
**ADR Decision Summary**: ADR-0003: uniform pawn net configuration for NPC and player, quantised server cull for non-allies plus client-local hide, fixed-width serialisers, and an allowlist audit of every replicated property; it owns the carrier (`FRoundPublicState` on the GameState, one All-audience struct, unconditional, identical bytes). ADR-0005: four game-instance stores are the only authoritative match state, so the recreated GameState is re-seeded from the Round store. The field-level rules (`Phase` 0 = `Unknown`, `PhaseChangeCount`, zeroed times, re-seed in `InitGameState`) are ADR-0002 section 8.10, which this story implements.
**ADR Version**: ADR-0003 2026-10-24; ADR-0005 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0003 and ADR-0005 knowledge risk)
**Engine Notes**: Touches ADR-0005 E5, E6, E16 and E19 (seamless travel recreates the GameState, PlayerController and PlayerState; `InitGameState` is the first point the GameState can be written; the host's controller arrives before world `BeginPlay`) and E7 (seamless travel is disabled in single-process PIE unless `net.AllowPIESeamlessTravel=1`; per ADR-0005 guideline 10 a PIE seamless-travel result must be labelled non-representative and travel acceptance runs multi-process standalone). It touches no ADR-0003 relevancy item (S9, S10, S12, S13 are the cull and re-open spikes and are not exercised by one replicated struct). ADR-0003 E12 (custom `NetSerialize` struct sent whole, [U], byte capture) applies only if `FRoundPublicState` is given a custom serialiser; Open: confirm against ADR-0003 section 6 whether the struct uses default property replication or `WithNetSerializer`.

**Control Manifest Rules (this layer)** (Round-relevant rules sit in the Foundation Layer Rules):
- Required: Authoritative match state lives only in the four game-instance stores; the GameState holds a mirror re-written from the Round store in `InitGameState` (ADR-0005, ADR-0002 section 8.10).
- Forbidden: Never write a non-zero `TeamSummary` (or any per-team count) into `FRoundPublicState` before `Ended` (ADR-0002, user decision 2026-10-25).
- Guardrail: Replication: relevancy override O(1) per pawn-connection pair inside the 1.5 ms replication line, pawns at a fixed 15 Hz within 36 KB/s worst case (ADR-0003); `FRoundPublicState` is a small always-relevant struct inside that line.

**Prototype disposition**: Replaces the replicated round fields in `Source/StealthGame/Public/StealthGameState.h` and `Source/StealthGame/Private/StealthGameState.cpp`; the production struct is a mirror re-seeded from the game-instance Round store.

---

## Acceptance Criteria

*From GDD `design/gdd/round-match-framework.md` AC 57, scoped to this story:*

- [ ] GIVEN a late joiner whose first replicated `Phase` equals the class default, THEN value 0 (`Unknown`) runs the handler; the host calls the handler explicitly; after seamless travel the new GameState reads `Preparing`, never a default; `PhaseChangeCount` strictly increases; `LastResult` survives into the lobby.

---

## Implementation Notes

*Derived from ADR-0002 section 8.10, ADR-0003 (carrier) and ADR-0005 section 2 and Implementation Guidelines, and GDD R20:*

- **The struct.** `FRoundPublicState` lives on `AStealthGameState`: one property, one RepNotify, replicated unconditionally (no `COND_OwnerOnly`, `COND_SkipOwner` or per-connection filter), so every viewer receives identical bytes. Fields: `Phase` (`uint8`; **value 0 is an explicit `Unknown`**; `Lobby`, `Preparing`, `Active`, `Ended`, `Reset` are 1 to 5), `RoundIndex`, `PhaseChangeCount` (`uint32`, incremented on every phase write, held in the Round phase core so it is re-seeded and never moves backward across travel), `PhaseEnteredServerTime` (stamped for `Lobby`, `Preparing`, `Ended`, `Reset` only; 0 during `Active`), `MatchScore[2]`, `bCalledOff`, `ExpectedPawnCount`, `ActiveAtServerTime` (non-zero only in `Preparing` after P7), and in `Ended` the `FRoundResult`, kept as `LastResult` through `Reset`. `RoundEpoch` is not public.
- **Late joiner and the handler.** Because the class default of `Phase` would otherwise equal the first received value and skip the RepNotify, the explicit 0 = `Unknown` ensures the handler runs for a late joiner. Clients also read the struct at begin-play. The host (a server receives no RepNotify) calls the same handler explicitly after every write. The client handler diffs the struct against its previous copy (one RepNotify can carry a phase and a score change) and uses `PhaseChangeCount` to detect a skipped phase.
- **Re-seed after seamless travel.** The GameState is recreated at every travel (ADR-0005 E5/E6) and the authoritative values live in the game-instance Round store, so Round writes the whole struct from `URoundMatchStore` in `InitGameState` (the first point the GameState exists, E16), before its first net update: a new match world starts at `Preparing`, never at the default `Unknown`; the lobby GameState is re-seeded with `Phase = Lobby`, `MatchScore` and `LastResult` from the store. `FRoundParams` is also written in `InitGameState` (ADR-0005 guideline 4b).
- **Forced net updates.** Every Round mutation forces a net update of the GameState (it otherwise sends at 10 Hz, up to 100 ms late).
- **What must never appear before `Ended`:** opponent `ActiveCount`, `InformantCount`, `AwaitingPawn` or anything derived, grace deadlines, queue keys, the Event Log, elapsed Active time, and any non-zero `TeamSummary`. The field audit and identical-bytes assertion in Active are story 080; this story owns the `Phase`/`PhaseChangeCount`/`LastResult`/travel behaviour.
- **Test shape.** A multi-client PIE test in `StealthTestHarness` (CQTest `NETWORK_TEST_CLASS`) with a late joiner and two consecutive travels. Run travel acceptance multi-process standalone where the harness allows; a single-process PIE result is non-representative (ADR-0005 guideline 10).
- Manifest 2026-10-25f: `FRoundPhaseCore` holds the monotonic `PhaseChangeCount` and is the source of the `FRoundPublicState` re-seed in `InitGameState`; the struct carries no per-team count before `Ended`.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 028 (`production/epics/round-match-framework/story-028-phase-admission-and-illegal-transitions.md`): the phase machine and its admission pushes (this story's dependency).
- Story 030 (`production/epics/round-match-framework/story-030-match-scope-survives-travel.md`): `MatchScore`, `RoundsPlayedCount`, host id, teams and archived slices persisting; `RoundEpoch` increments.
- Story 080 (`production/epics/round-match-framework/story-080-no-leak-before-ended.md`): allowlist diff and identical bytes on 3 clients in Active.
- Story 081 and 091: the late-loader reveal bound and the scheduled reveal.
- Story 082: the host-HUD include rule.
- Story 054 (`production/epics/round-match-framework/story-054-team-summary-at-ended.md`): `FRoundResult.TeamSummary` content (the HUD epic carries the display half).
- Roster internals and Session internals (seamless travel mechanics, rejoin); the HUD epic's handler presentation.

---

## Files

*Planned files only. Tests live under the engine test roots (`tests/README.md`), not under `tests/`. `/dev-story` replaces this list with the files it actually changed.*

- Source/StealthFoundation/Public/View/StealthRoundPublicState.h, Private/Round/StealthRoundPublicState.cpp (new: ERoundPhase with Unknown = 0, FRoundResult, FRoundPublicState)
- Source/StealthGameplay/Public/Server/Core/Round/StealthRoundPublicStateMirror.h, Private/Core/Round/StealthRoundPublicStateMirror.cpp (new: IRoundPublicStateSink, mirror)
- Source/StealthGame/Public/StealthGameState.h, Private/StealthGameState.cpp (RoundPublicState, RepNotify handler, explicit host call)
- Source/StealthGame/Private/StealthGameMode.cpp (InitGameState re-seed)
- Source/StealthTestHarness/Private/Tests/PIE/Round/PublicStateReplicationTest.cpp (new, CQTest PIE)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: FRoundPublicState replicates with Unknown = 0, survives seamless travel, and LastResult survives into Lobby.
  - Given: a 2-client PIE session with a late joiner.
  - When: the late joiner connects, travel completes and the round returns to Lobby.
  - Then: a late joiner whose first replicated `Phase` equals the class default reads 0 (`Unknown`) and runs the handler.
  - Then: the host calls the handler explicitly.
  - Then: after seamless travel the new GameState reads `Preparing`, never a default.
  - Then: `PhaseChangeCount` strictly increases.
  - Then: `LastResult` survives into the lobby.
  - Edge cases: two consecutive travels; a joiner arriving mid-travel.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/Round/PublicStateReplicationTest.cpp` (CQTest `NETWORK_TEST_CLASS(PublicStateReplication, "StealthGame.Round.PIE")` per the CQTest exemption in `.claude/rules/test-standards.md`) — must exist and pass.

**Status**: [x] Created and passing (`StealthGame.Round.PIE.PublicStateReplication.PhaseReplicatesReseedsAndKeepsLastResult`, 2026-10-26)

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-028-phase-admission-and-illegal-transitions.md`
- Unlocks: `production/epics/round-match-framework/story-030-match-scope-survives-travel.md`, `production/epics/round-match-framework/story-080-no-leak-before-ended.md`, `production/epics/round-match-framework/story-081-late-loader-reveal-bound.md`, `production/epics/round-match-framework/story-082-host-parity-includes.md`
- Cross-epic: `production/epics/session-travel` (seamless travel, game-instance stores and `FRoundParams`; stories not yet created), `production/epics/tuning-data-config` (module skeleton and StealthTestHarness bootstrap; stories not yet created)

---

## Completion Notes
**Completed**: 2026-10-26 (D53 integration batch: PSR 061-062, RMF 018-029, SCL 002/006)
**Criteria**: 1/1 passing (late joiner Unknown->Lobby handler; host explicit handler; re-seed reads the store phase and Preparing; PhaseChangeCount strictly increases; LastResult kept into Lobby)
**Deviations**: No Session travel exists: the after-travel clause is proven by the InitGameState re-seed of a recreated GameState in-process (ADR-0005 guideline 10: not representative of a multi-process seamless travel; rerun when Session travel lands). Prototype GameState round fields are kept for the prototype HUD.
**Test Evidence**: `Source/StealthTestHarness/Private/Tests/PIE/Round/PublicStateReplicationTest.cpp`; full suite: `production/qa/evidence/integration-spine-2026-10-26/full-suite.txt` (120/120 Success)
**Run result**: OBSERVED: integration-spine-2026-10-26/02-client1-active.png (client reads replicated phase Active, changes 3)
**Reviewed by**: lead-programmer focused review (sonnet) + self-review, D53 batch
