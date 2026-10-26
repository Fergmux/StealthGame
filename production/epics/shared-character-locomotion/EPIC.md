# Epic: Shared Character & Locomotion

> **Layer**: Core
> **GDD**: design/gdd/shared-character-locomotion.md
> **Architecture Module**: Shared Pawn (SCL) and Animation & significance (SCL R10-R12) in `StealthGameplay/Core` (`AStealthPawn : ACharacter`, `UStealthPawnFactory`, AnimBP, tier function)
> **Status**: Ready
> **Stories**: 56 stories written; 22 Complete (001, 002, 003, 004, 005, 006, 007, 009-018, 045, 046, 047), 056 In Progress, 044 Blocked (spike R3), the rest Ready; see the Stories table below
> **Engine Risk**: HIGH (highest among governing ADRs; engine 5.8, installed 5.8.4)
> **Control Manifest**: 2026-10-25e
> **Build Order**: 9 of 19 (layer order; the risk-first sequence is in the Build Order and Gates section)

## Overview

Shared Character & Locomotion owns the Shared Pawn: the one `ACharacter`-derived class every body in the city uses, whether a human or an NPC drives it. The epic delivers (1) the pawn and its lifecycle: one class, the `UStealthPawnFactory` world subsystem (deferred spawn, `AppearanceId` set, optional `BindPawn`, possession), `TakeOverCrowdPawn` for informant respawn, a `PawnLeavingGame` override so a disconnect never destroys a pawn, possession by `PlayerController` or `AIController` with possession-invariant net configuration and the stop-and-blend hand-over; (2) movement execution on CharacterMovementComponent (not Mover) for every pawn: Walk at crowd pace, Run as a tell, the MVP keyboard walk-slower pace quantisation (R5.1), baseline acceleration, braking and turn shaping, the traversal set (vault 0.8 m, climb/mantle 1.6 m, provisional pending the traversal spike) and restrictions by participant state and action; (3) one look path: a single clamped, rate-limited head-look state computed on the server for every pawn (`RemoteViewPitch16` written from `Look`); (4) animation: one AnimBP, the motion-matching database and chooser set from the Game Animation Sample, `ActionPose`-driven states, and significance tiers by screen height only with the project's own external tick-rate control (ADR-0013); (5) net parity (15 Hz fixed, priority 3.0, `DORM_Never`, no cull distance, no pawn multicast), smoothing parity, footstep parity from one step clock, and the published server state `IsRunning`/`IsInTraversal`/`CommandedDisplacementCm`.

Budgets: at most 4 ms animation at 60 characters and at most 36 KB/s pawn replication per client at `NpcCount` 50.

## Governing ADRs

| ADR | Decision Summary | Engine Risk |
|-----|------------------|-------------|
| ADR-0001: Crowd identity rules | Four audience carriers (C-Public allowlist on PlayerState, owner-only C-TeamView on PlayerController, C-Server, GameState public structs); every UE default that reveals player-ness is overridden; one pawn class, one look path, identity blindness. | HIGH |
| ADR-0003: Replication, relevancy and the concealment cull hybrid | Uniform pawn net configuration for NPC and player, quantised server cull for non-allies plus a client-local hide, allowlist audit of every replicated property. | HIGH |
| ADR-0004: Tuning data pipeline and validator | One cooked data-asset class of per-owner sub-structs, C++ schema rows, server-only resolve/validate/freeze with TuningHash and ClientHash, no config/CVar back doors. | LOW |
| ADR-0010: Shared Pawn: CMC locomotion, spawn factory and possession parity | One AStealthPawn class on CMC, factory-only spawn, disconnect never destroys a pawn, stop-and-blend hand-over, possession-invariant net config. | HIGH |
| ADR-0013: Animation: motion matching and significance budget | One AnimBP and PoseSearch set, tiers by screen height only, project-owned external tick-rate control (no URO, budget allocator or SignificanceManager). | HIGH |
| ADR-0022: Rendering configuration and performance budgets | Lumen/Nanite/VSM configuration and CPU/GPU frame budgets for the RTX 3060-class 1080p60 target (GPU 14.6 ms). | HIGH |

All governing ADRs are Accepted (user, 2026-10-25). Several are conditionally accepted; the outstanding gates are in Build Order and Gates below.

## GDD Requirements

14 of 14 requirements are covered by an Accepted ADR. Untraced requirements: None.

| TR-ID | Requirement | ADR Coverage |
|-------|-------------|--------------|
| TR-scl-001 | One Shared Pawn class; one spawn factory; takeover function | ADR-0010 ✅ |
| TR-scl-002 | Disconnect never destroys a pawn (`PawnLeavingGame` override, same-frame AI possession) | ADR-0010 ✅ |
| TR-scl-003 | Possession parity and stop-and-blend hand-over; `ForceNetUpdate` deferral | ADR-0003, ADR-0010 ✅ |
| TR-scl-004 | CMC locomotion, speed tiers, state restrictions, replicated traversal set (spike) | ADR-0010 ✅ |
| TR-scl-005 | One look path; `RemoteViewPitch16` written from `Look` for every pawn | ADR-0001, ADR-0003 ✅ |
| TR-scl-006 | One AnimBP, motion-matching database, chooser and pose set; `ActionPose`-driven states | ADR-0013 ✅ |
| TR-scl-007 | Significance-only tiers by screen height; caps; client-local tie key; server always-ticks pose | ADR-0013 ✅ |
| TR-scl-008 | ≤ 4 ms animation at 60 characters; ≤ 36 KB/s pawn replication per client (working values, `NpcCount` 50) | ADR-0013, ADR-0022 ✅ |
| TR-scl-009 | Net parity config (15 Hz fixed, priority 3.0, `DORM_Never`, no cull distance, no pawn multicast) | ADR-0003 ✅ |
| TR-scl-010 | Smoothing parity and input-starvation rule | ADR-0003, ADR-0010 ✅ |
| TR-scl-011 | Footstep path parity from one step clock | ADR-0013 ✅ |
| TR-scl-012 | Published server state (`IsRunning`, `IsInTraversal`, commanded displacement) | ADR-0010 ✅ |
| TR-scl-013 | One capsule, collision profile and physics asset | ADR-0010 ✅ |
| TR-scl-014 | Engine CMC net settings pinned and boot-tested against SCL keys (I-SCL-15) | ADR-0010, ADR-0004 ✅ |

## Prototype Disposition

The code in `Source/StealthGame` is marked PROTOTYPE (`production/first-playable-report.md`). Production code follows ADR-0007 modules, and nothing is promoted until the ADR that governs it is built against.

**Replaces / promotes with change:** `Source/StealthGame/Private/StealthCharacter.cpp` (675 lines) and `Public/StealthCharacter.h` (the prototype `AStealthCharacter`: movement, appearance, death presentation and the per-feature RPCs all in one class), and `StealthMovementComponent.*`. The sample's `CBP_SandboxCharacter` is reparented onto `AStealthCharacter` (the sample's internal casts require it; pre-change snapshot in `_snapshots/2026-10-24_sample-blueprints-pre-resave/`). The production class `AStealthPawn` per ADR-0010 must keep that Blueprint reparent working, which is a Content step and carries the never-resave hazards in `production/never-resave-assets.md`; no `.uasset` is touched by this epic definition. The informant speed cap and traversal block exist in prototype code but were not exercised in play (known issue 4). Sample motion matching and CMC traversal replication are in the prototype and unverified under latency.

## Build Order and Gates

- **Depends on (epics)**: input-interaction-framework, participant-state-roster, character-pool-appearance, tuning-data-config, level-socket-contract (soft), camera-system (eye-mask flag, soft); hands over to crowd-npc-behaviour through `ICrowdControl`
- **Gates and risks**: Conditional gates: ADR-0010 (traversal under net emulation at 100 ms / 1 % loss, S8, S14, H1, P1) and ADR-0013 (R3 on RTX 3060-class hardware, ANIM-V1..ANIM-V6). Systems-index High-Risk system: traversal replication under loss is unproven, 50 motion-matched characters vs the 4 ms animation budget, and net-update cadence can become a tell. The producer review judged this epic overscoped and recommended splitting traversal out. Decision: keep one epic, with **two exit points**: **Exit A**, locomotion, look path, animation, net parity and the factory/possession lifecycle; **Exit B**, traversal replication, which is **gated on the net-emulation spike** so a failed spike cannot hold up Exit A. Largest Core package (about 16 d bottom-up). This epic owns the cross-cutting **net-emulation pass** (about 4 d: 100 ms / 1 % loss profile over traversal, look path and pawn replication) as an exit criterion.
- **Estimate**: about 16 bottom-up days (human-paced yardstick, `production/estimates/2026-10-25_mvp-rebaseline.md`, Method B work packages); planning figure about 9.6 days after the uniform 0.6 calibration. Low confidence: no sprint history exists, so run the 2-week calibration spike and re-baseline.
- **Acceptance criteria in the GDD**: about 51 (counted as GIVEN-form criteria in the Acceptance Criteria section).
- **MVP build order**: risk-first per `design/gdd/systems-index.md`: graybox map and mannequin-variant pool first, then camera toggle and traversal replication, then the Meshy pool swap, then the generator.

## Definition of Done

- Every story implemented, reviewed and closed with `/story-done`
- Every acceptance criterion of design/gdd/shared-character-locomotion.md verified
- Logic and Integration stories have passing tests in the test root (`Source/<Module>/Private/Tests/`)
- Visual/Feel and UI stories have retained screenshots in `production/qa/evidence/`
  (a lead sign-off too for Visual/Feel)
- Stories embed Control Manifest version 2026-10-25e and every governing ADR's conditional gate has reported before dependent work is marked Done

## Stories

Exit A (locomotion, look path, animation, net parity, factory and possession lifecycle): stories 001-044 except the Blocked ones marked below. Exit B (traversal replication): stories 045-049 and 053, 055, gated on the net-emulation spike (story 045). Starvation stories 050-051 are also gated on that spike. Story 044 is gated on spike R3 (RTX 3060-class hardware).

| # | Story | Type | Status | ADR |
|---|-------|------|--------|-----|
| 001 | One Shared Pawn class with an identical default subobject set | Logic | Complete | ADR-0010 |
| 002 | Body constants published: step height, floor angle, capsule, kerb and block heights | Logic | Complete | ADR-0010 |
| 003 | Every SCL tuning key registered with its audience and latch, invariants enforced | Logic | Complete | ADR-0004 |
| 004 | Class parity scan: no observable path branches on controller kind, with a negative control | Logic | Complete | ADR-0001 |
| 005 | Remove the sample AnimBP's IsLocallyControlled branch for every pawn | Logic | Complete | ADR-0013 |
| 006 | Every Shared Pawn comes from one spawn site | Integration | Complete | ADR-0010 |
| 007 | A rejected bind destroys the pawn before FinishSpawning | Integration | Complete | ADR-0010 |
| 008 | NPC and player pawns have identical first bunches | Integration | Ready | ADR-0003 |
| 009 | Stop floor: every stop to rest lasts at least StopMinSec (D2) | Logic | Complete | ADR-0010 |
| 010 | Speed tiers, pace quantisation and effective-gait clamps | Integration | Complete | ADR-0010 |
| 011 | Release-to-stop baseline, identical for AI and for owner prediction | Integration | Complete | ADR-0010 |
| 012 | Turn shaping, latched reversal and pivot | Integration | Complete | ADR-0010 |
| 013 | IsRunning with hysteresis | Logic | Complete | ADR-0010 |
| 014 | Informants walk only, cannot traverse, and SCL refuses the traversal mode | Logic | Complete | ADR-0010 |
| 015 | Planted brace: step-into-stance entry, then no translation and no yaw | Integration | Complete | ADR-0010 |
| 016 | Commanded displacement is separate from external displacement | Integration | Complete | ADR-0010 |
| 017 | Idle turn-in-place after a held look beyond the yaw limit | Integration | Complete | ADR-0010 |
| 018 | AI path following enters only through Move | Integration | Complete | ADR-0010 |
| 019 | NPC pace distribution matches the quantised player pace set | Integration | Ready | ADR-0010 |
| 020 | Contact brush reaction is the same for every pair of pawns | Integration | Ready | ADR-0013 |
| 021 | One look path: HeadLook stays inside the envelope under a 700 deg/s spin | Integration | Ready | ADR-0001 |
| 022 | RemoteViewPitch16 written from HeadLook for every pawn, clamped | Integration | Ready | ADR-0003 |
| 023 | Observers render a continuous eased head, never 9 degree steps | Integration | Ready | ADR-0003 |
| 024 | Idle gaze layer: four idle pawns are indistinguishable (D14) | Integration | Ready | ADR-0006 |
| 025 | Idle gaze look-input deadzone (D31) | Logic | Ready | ADR-0006 |
| 026 | A disconnect never destroys a pawn; a crowd AI possesses it in the same frame | Integration | Ready | ADR-0010 |
| 027 | Stop-and-blend hand-over between player and AI | Integration | Ready | ADR-0010 |
| 028 | Informant respawn takes over an unseen crowd pawn | Integration | Ready | ADR-0010 |
| 029 | NPC and player pawns have the same Owner, Controller and PlayerState shape on the wire | Integration | Ready | ADR-0001 |
| 030 | Pawn replication settings are equal for NPC and player and pinned to the parity values | Integration | Ready | ADR-0003 |
| 031 | No dormancy, owner-only relevancy or class priority on any pawn | Integration | Ready | ADR-0003 |
| 032 | The pawn's replicated set equals the allowlist, with no multicast RPC | Logic | Ready | ADR-0003 |
| 033 | ActionPose writes reach observers on the same schedule for NPC and player pawns | Integration | Ready | ADR-0003 |
| 034 | Pawn replication stays within 36 KB/s per client with 59 pawns walking | Integration | Ready | ADR-0022 |
| 035 | Observers never see a snap, and smoothing is the same for NPC and player pawns | Integration | Ready | ADR-0003 |
| 036 | The listen host sees a remote player's pawn as smoothly as an NPC | Integration | Ready | ADR-0003 |
| 037 | Animation significance tier formula with hysteresis and hold | Logic | Ready | ADR-0013 |
| 038 | Tier caps and a client-local tie key that cannot order players apart from NPCs | Logic | Ready | ADR-0013 |
| 039 | Tier application: an AI pawn and a player pawn at equal depth are tiered identically | Integration | Ready | ADR-0013 |
| 040 | No rendered pawn is ever frozen or unposed | Integration | Ready | ADR-0013 |
| 041 | Identical body and animation set; no sprint, crouch or jump assets; time-based search | Logic | Ready | ADR-0013 |
| 042 | Toggling first and third person changes nothing an observer can see | Integration | Ready | ADR-0001 |
| 043 | Footsteps come from one step clock, identical for every pawn | Integration | Ready | ADR-0013 |
| 044 | Animation stays within 4 ms at the reference scene on RTX 3060-class hardware | Integration | Blocked (spike R3) | ADR-0013 |
| 045 | Net-emulation and traversal spike report (unblocks Exit B and starvation) | Integration | Complete | ADR-0010 |
| 046 | Kit coping heights match the pawn's traversal capability heights | Logic | Complete | ADR-0010 |
| 047 | Traversal eligibility matrix (F5) from body facing | Logic | Complete | ADR-0010 |
| 048 | An NPC vault and a player vault look identical to an observer | Integration | Ready | ADR-0010 |
| 049 | Traversal converges on every client under 100 ms lag and 1 % loss | Integration | Ready | ADR-0010 |
| 050 | A 2 s loss burst brakes the pawn by the baseline, with no dead stop and no snap | Integration | Ready | ADR-0010 |
| 051 | Recovery from a 2.5 s burst never produces a catch-up sprint | Integration | Ready | ADR-0010 |
| 052 | Crowd's Tell Matrix has an SCL-owned row for every SCL tell | Logic | Ready | ADR-0001 |
| 053 | NPCs actually run, vault and climb in the Crowd reference scene | Integration | Ready | ADR-0001 |
| 054 | Blind observers cannot tell a walking NPC from a walking player | Visual/Feel | Ready | ADR-0001 |
| 055 | Net-emulation pass over traversal, the look path and pawn replication (exit criterion) | Integration | Ready | ADR-0010 |
| 056 | Animation sourcing spike: strike, victim fall and brace from sample edits versus Meshy motion (D54) | Integration | In Progress (route-B half done; UE half deferred) | ADR-0013 |
