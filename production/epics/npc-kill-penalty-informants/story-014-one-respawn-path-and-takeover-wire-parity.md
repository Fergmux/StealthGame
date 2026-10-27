# Story 014: One respawn path for every cause, and the takeover is invisible on the wire

> **Epic**: NPC-Kill Penalty & Informants
> **Status**: Blocked — BLOCKED (AC2 only): the content and cadence clauses of AC-NKP-37 wait on information-visibility-policy story 018 (owner-only PlayerState / Controller / Owner on AStealthPawn; PIE measured `Actor.PlayerState` changing on the taken pawn for a non-team client), shared-character-locomotion's pawn differentiators (RemoteViewPitch16 from HeadLook, CharacterInputState, TraversalResult) and the ADR-0008 section 6 multi-process raw-bunch capture (cadence). AC1 and the AC2 clause "no actor channel opens or closes" pass.
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25f
> **Last Updated**: 2026-10-27

## Context

**GDD**: `design/gdd/npc-kill-penalty-informants.md`
**Requirement**: `TR-pen-004`, `TR-pen-005`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0019: Informant lifecycle and crowd-pawn takeover (§4, Verification Required 1), with ADR-0001: Crowd identity rules (identity invariants) and ADR-0010 (possession parity)
**ADR Decision Summary**: No participant pawn is ever spawned mid-round: every informant body, whatever the cause, is a pre-existing unseen crowd pawn inside the team's respawn region (or its widened region), taken over through the one commit path. No actor, component or NetGUID is created, and the pawn's `AppearanceId`, velocity, `HeadLook` and net config do not change, so the takeover is invisible to every non-owner (IVP R9c, R14). Teammates see the outline marker move through C-TeamView's `PawnRef`.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Risk is the highest of the governing ADRs: ADR-0001 is HIGH (owner-only `PlayerState`/`Controller`/`Owner`, legacy NetDriver, Iris pinned off) and ADR-0010's possession parity is HIGH. Spikes S1, S2, S14 reported 2026-10-25 (`production/qa/spikes/2026-10-25_S1-S2-S14.md`): the three pointers stay hidden across takeover, AI hand-over and kill (PASS); whole-pawn parity FAILED in the prototype on `RemoteViewPitch16`, `CharacterInputState` and `TraversalResult` (non-pointer differentiators the production pawn must close); open conditions: rejoin, travel, PawnRecovery and a multi-process capture under the exact `LeakNetProfile`. AC2 (`[Provisional: spikes S1/S14]` in the GDD) is therefore Ready, to be re-run on the production pawn with those conditions. ADR-0019 Verification Required (1) and (3) are settled by this story's tests; (2) is ADR-0010 H1.

**Control Manifest Rules (this layer)**:
- Required: a respawn only takes over a pawn returned by `PickRespawnPawn`, the only random draw of the sequence — source: ADR-0019
- Forbidden: never spawn a pawn for a respawn, re-possess a dead body, or keep the participant's old look — source: ADR-0019
- Guardrail: Penalty O(1) inside the queue line; pick cost within ADR-0014's budget with 10 connections and 40 NPCs — source: ADR-0019

This is the PIE proof of the epic's central promise: "no client can tell that a citizen has just become a person". It covers every cause (assassinated, reversed against, strike limit, informant killed, returning informant, body lost to KillZ) and the wire-level parity of the taken pawn against a control NPC.

**Prototype replaced:** `AStealthGameMode::RespawnInformant` (prototype takeover without unseen test or card conservation). Prototype test B (informant killed and respawned) is the behavioural reference; the production PIE test here is its stronger successor, and the prototype's S14 spike runs (`production/qa/evidence/2026-10-25_S14_*`) are the reference for what the wire looks like today.

---

## Acceptance Criteria

*From GDD `design/gdd/npc-kill-penalty-informants.md` (AC-NKP-09, AC-NKP-37), scoped to this story:*

- [x] **AC1** (AC-NKP-09, [I][PIE-3] `StealthGame.Penalty.PIE.OneRespawnPath`, parameterised over Assassinated, ReversedAgainst, StrikeLimit, informant killed, returning informant, body lost to KillZ): **GIVEN** the cause **WHEN** the informant's body is provided **THEN** Penalty's path spawns 0 pawns (Crowd's replacement emergence for a dead body is excluded from the count and is Crowd's AC), the body is a pre-existing crowd pawn inside the team's `RespawnRegion` (or its widened region), no non-team pawn had line of sight to it at the commit tick, and, over the whole respawn, exactly one card move occurred (after the takeover returned `Applied`) and exactly one LSC `RespawnPick` draw occurred per commit attempt (retries after a failed release or takeover draw again but move no card) (PEN-R7, R9; order is story 012).
- [ ] **AC2** (AC-NKP-37, [I][PIE-3][Provisional: spikes S1/S14] `StealthGame.Penalty.PIE.TakeoverWireParity`): **GIVEN** an informant takeover of a pawn relevant to a non-team client (unseen at commit) and a control NPC pawn that is not taken over **WHEN** that client's received property bunches for both pawns are diffed across the takeover **THEN** they are equal in shape and content apart from movement, the taken pawn's bunch cadence over the 5 s before and after the takeover matches the control pawn's within ± 10 %, and no actor channel opens or closes (PEN-R9, IVP R9c, R14).

---

## Implementation Notes

*Derived from ADR-0019 Decision §4, Verification Required and the harness rules of ADR-0008:*

Both tests are CQTest PIE tests in `StealthTestHarness` (`StealthGame.Penalty.PIE.<Scenario>.<Method>`, the test-standards CQTest exemption), 3v3 with a fixed crowd seed (PIE-3). Penalty's production code is written by stories 009-013; this story adds the tests and fixes any defect they expose in Penalty's call order or arguments.

AC1: for each cause, drive the cause through the real server path (a scripted assassination, a reversal, an exile through three NPC kills with the test preset, an informant kill, a disconnect and return, a forced KillZ) and record: the world's actor-spawn counter for Penalty's path (Crowd's replacement emergence is excluded by tagging the spawn instigator), the identity of the new body (it existed before `T_due`), its position inside the team's `RespawnRegion` or widened region (query LSC's region predicate), the unseen test at the commit tick (LSC's own sight query at that tick, recorded), the Character Pool's card-move counter (exactly one, ordered after the takeover result) and LSC's `RespawnPick` draw counter (one per commit attempt). A failed-release variant (inject one `TryReleaseNpc` failure) asserts the extra draw but no extra card move.

AC2: capture the non-team client's received property bunches for the taken pawn and a control NPC over the takeover with the ADR-0008 raw-bunch capture helpers (`LeakNetProfile`: PktLag 100 ms, PktLoss 1-2 %); diff shape and content apart from movement; compare bunch cadence over the 5 s before and after (± 10 %); assert no actor channel open or close. The S1 spike's analysers (`production/qa/spikes/2026-10-25_tools/`) are a starting point (`stealth.Spike.RepMap` readback and the per-parent diff). Failures found here that originate in the pawn (the three non-pointer differentiators in the spike report) are fixed in shared-character-locomotion's stories, not here; record them against that epic.

Note on `[Provisional]`: GDD AC-NKP-37 was tagged provisional against S1/S14; both reported, so the story is Ready. The AC is still re-baselined when the production pawn replaces the prototype and when the multi-process capture is available.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-012-respawn-commit-sequence.md`: the order and failure handling of the commit (Logic).
- `story-019-takeover-and-exile-visual-continuity.md`: the takeover's *visual* behaviour (no pop, braking, head-look).
- shared-character-locomotion `story-028-informant-respawn-takes-over-unseen-crowd-pawn.md`: the factory's takeover function and its own ACs.

---

## Files

*Files changed by /dev-story (2026-10-27). The PIE tests live under the harness test root (`Source/StealthTestHarness/Private/Tests/PIE/Penalty/`, not the planned `Private/Penalty/`).*

- `Source/StealthGame/Public/Server/StealthPenaltyHarnessSeams.h` (new: the harness-only crowd-less respawn switch, the respawn probe, release fault injection; WITH_STEALTH_TEST_SEAMS only)
- `Source/StealthGame/Private/Composition/StealthPenaltyWiring.cpp`, `StealthPenaltyWiring.h` (the team-start fallback gated to the harness seam, off by default and compiled out of Shipping; probe records; seam definitions)
- `Source/StealthGame/Public/StealthGameMode.h`, `Source/StealthGame/Private/StealthGameMode.cpp` (`SpawnInformantFallbackPawn` renamed `SpawnHarnessCrowdlessRespawnPawn`, null unless the seam is on)
- `Source/StealthTestHarness/Private/Tests/PIE/Assassination/{OwnerResultShape,PossessionThroughFall,KillPlateTellsOneThing,CulledVictimOpensFalling,OneBodySequence,StrikeClipParity}PieTest.cpp` (the 0-NPC rounds switch the harness-only respawn on)
- `Source/StealthTestHarness/Private/Tests/PIE/Penalty/PenaltyPieSupport.h` (new: shared helpers)
- `Source/StealthTestHarness/Private/Tests/PIE/Penalty/OneRespawnPathPieTest.cpp` (CQTest `StealthGame.Penalty.PIE.OneRespawnPath.{Assassinated,ReversedAgainst,StrikeLimit,InformantKilled,ReturningInformant,BodyLostToKillZ,FailedReleaseRedrawsWithoutCardMove}`)
- `Source/StealthTestHarness/Private/Tests/PIE/Penalty/TakeoverWireParityPieTest.cpp` (CQTest `StealthGame.Penalty.PIE.TakeoverWireParity.NoChannelOpenOrCloseAndSameNetGuid`)

Readiness / implementation notes (2026-10-27):
- Stand-ins in AC1, logged: ReturningInformant drops and returns through `URosterStore::NotifyConnectionChanged` (the composition root's logout and reclaim projection; Round R18 then readmits through its own queue item) because Session's PIE rejoin reclaim (session-travel stories 026-027) is not built; BodyLostToKillZ reports the loss through `UPenaltyService::OnInformantPawnLost`, the call Round R17.9 makes (round-match-framework rmf-072 not built). The pick, pre-check and takeover are the documented stand-ins (LSC 033, SCL 028); the card is the Character Pool stand-in, counted by the probe.
- AC2 is BLOCKED for its content and cadence clauses: with engine-default replication a player-possessed pawn sends `PlayerState`, `Controller` and `Owner` to non-owners (spike S1 baseline R0); the owner-only reset on `AStealthPawn` is information-visibility-policy story 018 (Blocked), and `RemoteViewPitch16` / `CharacterInputState` / `TraversalResult` are shared-character-locomotion's pawn differentiators; the bunch cadence needs the ADR-0008 section 6 raw-bunch capture (multi-process). The clause that holds today (no actor channel opens or closes, same actor, same NetGUID) is asserted; the content diff is logged as evidence for those owners.

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC1** (AC-NKP-09, `StealthGame.Penalty.PIE.OneRespawnPath.<Cause>`, parameterised over six causes)
  - Given: 3v3 PIE with a fixed crowd seed.
  - When: the informant's body is provided.
  - Then: spawns by Penalty's path are 0 (counted with a hook on the factory and Character Pool that excludes Crowd's replacement emergence); the body is a pre-existing crowd pawn inside `RespawnRegion` (or the widened region); no pawn bound to a participant of the other team (Active or informant) had line of sight at the commit tick; exactly one card move occurred, after `Applied`; exactly one `RespawnPick` draw per commit attempt.
  - Edge cases: "non-team pawn" is read as "pawn of another team's participant" (LSC R5b.2: NPCs are not viewers); a failed release or takeover redraws with no card move; a KillZ loss uses D = 0.
- **AC2** (AC-NKP-37, `StealthGame.Penalty.PIE.TakeoverWireParity`; needs the raw-bunch capture harness, Provisional on S1/S14: write now, re-baseline thresholds on production code)
  - Given: an unseen takeover plus a control NPC pawn, `LeakNetProfile`.
  - When: the non-team client's received bunches for both pawns are diffed across the takeover.
  - Then: equal apart from movement; bunch cadence over +/- 5 s within +/- 10 %; no actor channel opens or closes.
  - Edge cases: if the harness is MP-only, name the test `StealthGame.Penalty.MP.TakeoverWireParity` (test-standards); pawn-originated differences (`RemoteViewPitch16`, `CharacterInputState`, `TraversalResult`) are routed to shared-character-locomotion.

QL-STORY-READY: ADEQUATE

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: the two CQTest PIE tests under `Source/StealthTestHarness/Private/Penalty/` OR a documented playtest

**Status**: AC1 created and passing 2026-10-27: `StealthGame.Penalty.PIE.OneRespawnPath.*` (7 methods). AC2 partial: `StealthGame.Penalty.PIE.TakeoverWireParity.NoChannelOpenOrCloseAndSameNetGuid` passes (same actor, channel and NetGUID on the non-team observer); content and cadence BLOCKED (see Status). Evidence: `production/qa/evidence/npc-kill-penalty-informants-013-015-2026-10-27/evidence.md`.

Run result: OBSERVED — 3-player PIE with a crowd: the 3rd NPC kill exiles and the informant takes over an unseen crowd pawn in its team's region 15 s later; a killed informant respawns the same way; no pawn spawned (screenshots 01-05 in the evidence folder).

---

## Dependencies

- Depends on: `production/epics/npc-kill-penalty-informants/story-012-respawn-commit-sequence.md`, `story-013-starvation-retries-and-widening.md`, `story-010-absent-path-readmission-and-lost-body.md`, `story-005-exile-sequence-in-attack-slot.md`; cross-epic: shared-character-locomotion `story-028-informant-respawn-takes-over-unseen-crowd-pawn.md`, level-socket-contract `story-033-respawn-pick-eligibility-unseen-draw.md`, crowd-npc-behaviour `story-030-takeover-eligibility-and-atomic-release.md`, information-visibility-policy `story-037-takeover-skips-concealed.md`, tuning-data-config `story-019-cqtest-headless-proof.md`
- Unlocks: `production/epics/npc-kill-penalty-informants/story-019-takeover-and-exile-visual-continuity.md`, `story-021-retire-prototype-penalty-code.md`
