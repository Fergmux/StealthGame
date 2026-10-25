# Story 084: Host Start through travel reaches Active with one bound pawn per participant (one drain stamps ActivePhaseStart, RoundsPlayedCount 1, queue at Seq 1)

> **Epic**: Round & Match Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25f
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-001` (Five-phase machine in a game-instance store surviving travel), `TR-round-006` (Preparing readiness sequence, spawn gate and watchdog); GDD AC 1a (revised: without the 50 ms clause, which moves to story 091); rules R5, R6, R7, R22.
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0005: Session, seamless travel and match-scoped state stores; ADR-0002: Server resolution queue, server clock and round-end staging
**ADR Decision Summary**: ADR-0005: `AGameModeBase` with four game-instance stores (Session, Roster, Round, Tuning preset) as the only authoritative match state, seamless `ServerTravel` with `?Seed=`, a single round acknowledgement, admission modes, host commands on the game instance, no host migration. ADR-0002: one total-order queue per round keyed (`ServerTickIndex`, `Seq`), drained once per frame in `TG_PostPhysics`; the queue opens at `Active` entry with `Seq` starting at 1 while `ServerTickIndex` continues. This story is the first end-to-end run: Start through travel to Active.
**ADR Version**: ADR-0005 2026-10-24; ADR-0002 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0005 knowledge risk)
**Engine Notes**: Touches ADR-0005 spike S3 (seamless and hard travel: PlayerController/PlayerState replacement, re-bind, game-instance stores intact) and ADR-0005 guideline 10: seamless travel is disabled in single-process PIE unless `net.AllowPIESeamlessTravel=1`, so a PIE result is labelled non-representative and travel acceptance is observed multi-process standalone (a T3 Gauntlet node `StealthGame.Round.MP.PIELifecycleStart` if the harness provides one; Open: confirm against ADR-0008). Touches ADR-0002 Verification items (1) spawning Shared Pawns (CMC) inside `TG_PostPhysics` from a resolver, and (3) the drain running once per server frame on the listen host (host frame time <= 1/60 s). No post-cutoff API is called directly.

**Control Manifest Rules (this layer)** (Round-relevant rules sit in the Foundation Layer Rules):
- Required: Host commands are registered on the game instance with the net-mode/host check (ADR-0005).
- Forbidden: Never use a single-process T2 PIE run as seamless-travel evidence - PIE hard-travels unless `net.AllowPIESeamlessTravel=1` (ADR-0008, Technical Setup 2026-10-25b).
- Guardrail: Session: travel load bounded by `TravelLoadTimeoutSec` (60 s); silence clocks and ack bookkeeping inside the 0.5 ms line (ADR-0005).

**Prototype disposition**: This story is the acceptance run for the code that REPLACES `StealthGameMode.cpp`/`.h`: `StartRound`, `TryScheduleRoundStart`, `CleanupRoundActors` and the dev console command `StealthStartRound`, which the Core stories retire. This story itself edits no prototype file.

**Where it sits**: Exit B (Core), the MVP test (lifecycle, part 1 of 3). Story 085 continues from this Active round and story 086 runs a second round. The 50 ms loading-screen clause of GDD AC 1a is carried by story 091 (Blocked on the OQ11(b) clock-sync spike), not here.

---

## Acceptance Criteria

*From GDD `design/gdd/round-match-framework.md` AC 1a (revised in `rmf-ac-overrides.md`: minus the 50 ms clause), scoped to this story:*

- [ ] GIVEN a listen-server host (registered first, so team 0 by R3) and one client (team 1) in Lobby with `TeamSize` 1 and `bAllowEmptyTeamDev` false, under 100 ms RTT net emulation, WHEN the host runs `stealth.round.start` and travel completes, THEN on the server one drain stamps `ActivePhaseStart`, sets `RoundsPlayedCount` = 1 and opens the queue at `Seq` 1, and each participant has exactly one bound pawn.

---

## Implementation Notes

*Derived from ADR-0005 sections 1-4 and Implementation Guidelines, ADR-0002 sections 5 and 8, and GDD R5-R7, R22:*

- **What the test drives.** A [PIE-2] run: listen-server host plus one client in Lobby, `TeamSize` 1, `bAllowEmptyTeamDev` false, 100 ms RTT net emulation (a fixture constant in the test constant files; story 091 reuses the same session setup). The host runs `stealth.round.start` (console, on the game instance; net-mode and host check per R22). Travel completes through Session's seamless `ServerTravel` with the `?Seed=` URL option.
- **Server-side assertions (one drain).** At the first drain whose server time is `>= ActiveAtServerTime` Round performs R7 in one step: `ActiveAtServerTime` set to 0, `ActivePhaseStart` stamped, `RoundsPlayedCount` incremented (so `RoundIndex` is now 1), the queue opened with `Seq` starting at 1 while `ServerTickIndex` continues, `OnRoundActive` raised. Read these through the harness's server-side accessors in the same drain, not after later frames.
- **One bound pawn each.** After Active entry each participant has exactly one bound pawn: the host's and the client's, each created through `UStealthPawnFactory::SpawnSharedPawn` (deferred spawn, `ApplyAppearance`, `BindPawn`, `FinishSpawning`, possess), plus the NPC pawns of the `D = players + NpcCount` list (the assertion is on participants; NPC count is not asserted here). Before `Active` `GetUnboundActiveParticipants()` must be empty (R7; non-empty aborts `BindFailed`, story 041).
- **Ordering.** The accepted Start calls Session and Roster in the fixed order of story 024 and enters `Preparing` only after travel acceptance (decided rule C2); this story observes that order end to end, it does not re-test the call-order spy.
- **Travel evidence.** Per ADR-0005 guideline 10 a single-process PIE run that hard-travels is non-representative for travel mechanics. Run with `net.AllowPIESeamlessTravel=1` where the harness allows, and label the result; seamless-travel acceptance proper comes from the multi-process node. Open: confirm against ADR-0008 which tier (T2 PIE with the CVar, or T3 Gauntlet) this scenario is registered under.
- **Test shape.** CQTest `NETWORK_TEST_CLASS(PIELifecycleStart, "StealthGame.Round.PIE")` in `StealthTestHarness`, two clients (host and client), named per the CQTest exemption in `.claude/rules/test-standards.md`.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 091 (`production/epics/round-match-framework/story-091-scheduled-reveal.md`): the loading-screen drop within 50 ms of `ActiveAtServerTime` (AC 1a's clause, Blocked on the OQ11(b) spike).
- Story 085 (`production/epics/round-match-framework/story-085-pie-lifecycle-objective-and-reset.md`): `debugwin 0`, the end state and `reset`.
- Story 086 (`production/epics/round-match-framework/story-086-pie-lifecycle-elimination.md`): the second round and `debugeliminate`.
- Story 030 (`production/epics/round-match-framework/story-030-match-scope-survives-travel.md`) and story 042 (`production/epics/round-match-framework/story-042-active-gate.md`): the unit-level behaviour this run composes (store survival across travel; the Active gate).
- GDD AC 2 (`PIELifecycleReal`) is [DEFERRED] to the Objectives and Assassination epics and has no story.

---

## Files

*Planned files only. Tests live under the engine test roots (`tests/README.md`), not under `tests/`. `/dev-story` replaces this list with the files it actually changed.*

- `Source/StealthTestHarness/Private/Round/PIELifecycleStartTest.cpp` (CQTest `NETWORK_TEST_CLASS(PIELifecycleStart, "StealthGame.Round.PIE")`, full name `StealthGame.Round.PIE.PIELifecycleStart.<Method>`; the GDD scenario name is `StealthGame.Round.PIELifecycleStart`)
- `Source/StealthGame/Private/StealthGameMode.cpp`, `Source/StealthGame/Public/StealthGameMode.h` (prototype round logic: retired/replaced by the Core stories this run exercises; not edited by this story)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: One drain stamps `ActivePhaseStart`, sets `RoundsPlayedCount` to 1 and opens the queue at `Seq` 1, and each participant has one bound pawn.
  - Given: a listen-server host (team 0) and one client (team 1) in Lobby with `TeamSize` 1 and `bAllowEmptyTeamDev` false.
  - When: the host runs `stealth.round.start` and travel completes.
  - Then: on the server, one drain stamps `ActivePhaseStart`, sets `RoundsPlayedCount` = 1 and opens the queue at `Seq` 1.
  - Then: each participant has exactly one bound pawn.
  - Edge cases: the 50 ms loading-screen clause is not asserted here (moved to story 091); the net emulation stays at the 100 ms RTT fixture so that story 091 reuses the same session setup.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/Round/PIELifecycleStartTest.cpp` (CQTest `NETWORK_TEST_CLASS(PIELifecycleStart, "StealthGame.Round.PIE")` per the CQTest exemption in `.claude/rules/test-standards.md`) - must exist and pass.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-030-match-scope-survives-travel.md`, `production/epics/round-match-framework/story-042-active-gate.md`
- Unlocks: `production/epics/round-match-framework/story-085-pie-lifecycle-objective-and-reset.md`, `production/epics/round-match-framework/story-091-scheduled-reveal.md`
- Cross-epic: `production/epics/session-travel` (seamless travel, `FRoundParams`, single acknowledgement; stories not yet created), `production/epics/participant-state-roster` (registration, `BindPawn`, `ResetAll`; stories not yet created), `production/epics/tuning-data-config` (module skeleton, StealthTestHarness bootstrap, `TeamSize` and `bAllowEmptyTeamDev`; stories not yet created), `production/epics/level-socket-contract` (placement for the one-frame spawn; stories not yet created), `production/epics/character-pool-appearance` (appearance allocation; stories not yet created), `production/epics/shared-character-locomotion` (`SpawnSharedPawn`; stories not yet created), `production/epics/crowd-npc-behaviour` (NPC spawn and Crowd start; stories not yet created)
