# Story 056: A disconnect hand-over never decelerates faster than the crowd stop decel and never destroys the pawn

> **Epic**: Participant State & Roster
> **Status**: Blocked — BLOCKED: spike S8 (`PawnLeavingGame` override hand-over) has not reported. Unblocks when S8 reports PASS (pawn survives controller destruction and a crowd AI controller possesses it in the same server frame on a real connection close).
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/participant-state-roster.md`
**Requirement**: `TR-roster-004`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0010: Shared Pawn: CMC locomotion, spawn factory and possession parity
**Also governed by**: ADR-0005: Session, seamless travel and match-scoped state stores
**ADR Decision Summary**: ADR-0010 makes disconnect never destroy a pawn: `AStealthPlayerController::PawnLeavingGame()` is overridden so that, outside world teardown and seamless travel, it ends the outgoing controller's holds as releases, unpossesses through the `FScopedPossessionParity` wrapper and, in the same server frame, spawns an `AStealthCrowdController` that possesses the pawn and applies the cached last-command hold. Possession parity keeps velocity, movement mode and `HeadLook` unchanged, so a running pawn slows through the baseline run-to-walk deceleration rather than stopping dead. ADR-0005 keeps the binding untouched by connection events (the pawn stays `BoundLive`) and hands the pawn to Crowd through Round, never through Session.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Post-cutoff: `UMotionWarpingComponent::GetOwnerAdapter()` (5.8 replacement of the removed `GetCharacterOwner`); MotionWarping is Beta in 5.8.4 [V]; `UE_LOGF` for diagnostics. Verification required: **S8** (the `PawnLeavingGame` override keeps the pawn and a crowd AI controller possesses it in the same server frame on a real connection close, including whether spawning a controller is safe while the connection is being cleaned up, ADR-0010 E3), S14 (owner-only pointers survive every possession path), H1 (AI to player hand-over gap), P1 and ST1. S8 has not reported.

**Control Manifest Rules (this layer)**:
- Required: `PawnLeavingGame` never destroys the pawn; it hands it to an `AStealthCrowdController` in the same frame without issuing a command or changing the binding — source: ADR-0010
- Required: `Possess`/`UnPossess` on a Shared Pawn are called only by the factory (spawn, hand-over pass incl. `ReleaseBody`, takeover) and `AStealthPlayerController::PawnLeavingGame`, every call through `FScopedPossessionParity` — source: ADR-0010
- Required: authoritative match state lives only in the four game-instance stores, keyed by `ParticipantId`; the binding is untouched by a connection event — source: ADR-0005
- Forbidden: Never destroy a pawn inside a resolver except through `RequestTeardown`; `Reset` must never destroy a Shared Pawn — source: ADR-0010, ADR-0002
- Forbidden: Never let Session spawn, free, hide or possess a pawn, or call `RequestTransition` — source: ADR-0005
- Guardrail: listen-host frame time at most 1/60 s (p99 under the profiled load); the test runs at a fixed 1/60 s step — source: ADR-0002, ADR-0003

---

## Acceptance Criteria

*From GDD `design/gdd/participant-state-roster.md`, scoped to this story:*

- [ ] **AC-PSR-43** [Int][Dep:Crowd][Dep:Shared Character][S8][UI-ev] `StealthGame.Roster.DisconnectTakeoverBlend`: GIVEN an Active player sprinting mid-interaction, at a fixed step of 1/60 s, WHEN their connection drops, THEN:
  - the AI takeover never decelerates faster than `CrowdStopDecel` (`max|dv/dt| <= CrowdStopDecel`);
  - the active montage after the cancel is the ordinary interaction-cancel asset;
  - aim and head pitch change no faster than the crowd gaze catalogue's maximum turn rate;
  - the pawn is never destroyed.

  It needs a retained screenshot sequence and lead sign-off (R9 hand-over hook, R11 takeover rule).

---

## Implementation Notes

*Derived from ADR-0010 sections 4.2, 4.3, 5 and Implementation Guidelines, and GDD R9 ("Disconnect hand-over hook"), R11 ("Hand-over"), Edge Cases ("The takeover itself must not be a tell"):*

- **Blocker, stated precisely:** spike S8 (OQ0) has not reported. It must show that overriding `APlayerController::PawnLeavingGame` so the pawn survives controller destruction and a crowd AI controller possesses it in the same server frame works on a real connection close, with no visible pointer or pitch discontinuity on non-owners (S1 must inspect raw bunches, not only resolved pointers). **Unblocks when** S8 reports PASS; the hand-over mechanism itself lives in `production/epics/shared-character-locomotion/EPIC.md` (R2b, ADR-0010 section 5) and the takeover behaviour in `production/epics/crowd-npc-behaviour/EPIC.md`. Do not implement or close this story before then.
- The takeover must not be a tell (R11, a requirement on Crowd and Shared Character): the AI controller takes over by blend with no dead stop; a running pawn decelerates through the baseline gait, an interaction is cancelled with its ordinary cancel animation, and the pawn then picks up a crowd behaviour. A participant-bound AI issues only `Move` and `Look` commands. There is no single-frame snap from player movement to crowd baseline.
- Mechanism under test (ADR-0010): `PawnLeavingGame` ends the outgoing controller's holds as releases, unpossesses through `FScopedPossessionParity` (which captures and verifies velocity, movement mode, `HeadLook` world rotation and last commanded input), spawns the crowd controller in the same frame and applies the last-command hold from the cached input for `HandoverHoldSec`. The control epoch is not bumped in `PawnLeavingGame`; Round's `ConnectionTransition` slot bumps it when it decides the drive (`ICrowdControl::DriveBoundPawn`).
- Test shape (provisional, see Open points): a harness test at a fixed 1/60 s step with an Active player sprinting mid-interaction; the connection drops (a real connection close under S8). Record per-frame speed and compute `max|dv/dt|`, assert at most `CrowdStopDecel`; record aim and head pitch and assert the rate is at most the crowd gaze catalogue's maximum turn rate; assert the pawn is never destroyed during and after the drop; the binding stays `BoundLive` (R9). The montage assertion is held until the interaction-cancel asset is named.
- Constants: `CrowdStopDecel` and the gaze catalogue maximum turn rate are owned by Crowd (provisional); stub them as test constants until `production/epics/crowd-npc-behaviour/EPIC.md` finalises them.
- Evidence: a retained screenshot sequence of the hand-over in `production/qa/evidence/` and lead sign-off in `production/qa/evidence/disconnect-takeover-blend-evidence.md` are required extra evidence.
- Open point: the QA report finds "the ordinary interaction-cancel asset" has no named asset and "mid-interaction" names no interaction. Name both (asset and interaction) before the montage clause is testable; the crowd gaze catalogue maximum and `CrowdStopDecel` are provisional Crowd values.
- Prototype disposition: replaces the prototype's disconnect handling, where a leaving player's pawn was destroyed by the engine default (`APlayerController::PawnLeavingGame`); the prototype `AStealthCharacter`/`AStealthNpcController` possession code is replaced, not ported (ADR-0010 prototype note).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 030 (An informant's released pawn becomes an NPC and a reclaim adopts a different pawn): the Roster-side informant release and adoption.
- Story 011 (Tombstones respect generations and ReleaseToCrowd needs a disconnected participant): `ReleaseToCrowd` rules.
- Story 054 (Disconnect grace expires on the injected clock and stale-connection timers are rejected): grace expiry timing.
- Story 059 (A client rejoining within grace reclaims its record, pawn and informant status): rejoin repossession.
- Story 049 (Player and NPC pawns replicate identically): replication parity.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthTestHarness/Private/Roster/StealthRosterDisconnectTakeoverBlendTest.cpp` (Integration harness test, test name `StealthGame.Roster.DisconnectTakeoverBlend`)
- `Source/StealthTestHarness/Private/Roster/StealthRosterTakeoverProbe.h` (per-frame speed, aim and pitch probe at a fixed 1/60 s step; stub Crowd constants)
- `production/qa/evidence/` screenshot sequence and `production/qa/evidence/disconnect-takeover-blend-evidence.md` (lead sign-off)
- The `PawnLeavingGame` override and crowd controller are owned by the shared-character-locomotion and crowd-npc-behaviour epics and are not files of this story.

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation. (At tiers where the QL-STORY-READY gate is skipped — `minimal`, or `lean`/`solo` review mode — no qa-lead specs exist, and this section reads: N/A — no qa-lead specs at this tier; implement against the Acceptance Criteria above.)*

*Test cases not yet defined — run /qa-plan to generate them.*

**Flagged gap (QL-STORY-READY: GAPS; also BLOCKED by S8)**: "the ordinary interaction-cancel asset" has no named asset. The crowd gaze catalogue's maximum turn rate and CrowdStopDecel come from the Crowd epic (provisional). "Mid-interaction" names no interaction. Needs: name the asset and interaction; stub the Crowd constants.

**Provisional spec (qa-lead, pending gap resolution)**:

- **AC-1**: a hand-over never decelerates faster than the crowd stop decel and never destroys the pawn.
  - Given: an Active player sprinting mid-interaction at a fixed 1/60 s step, stubbed Crowd values.
  - When: the connection drops.
  - Then: max|dv/dt| at most CrowdStopDecel, aim and pitch turn no faster than the stubbed maximum, the pawn never destroyed. The montage assertion is held until the asset is named.
  - Edge cases: a retained screenshot sequence and lead sign-off are required extra evidence.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: automated test `Source/StealthTestHarness/Private/Roster/StealthRosterDisconnectTakeoverBlendTest.cpp` (`StealthGame.Roster.DisconnectTakeoverBlend`) OR a documented playtest; `qa.level` is full, so the automated test is required.
- Extra (required): a retained screenshot sequence in `production/qa/evidence/` plus lead sign-off in `production/qa/evidence/disconnect-takeover-blend-evidence.md`.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/participant-state-roster/story-030-informant-disconnect-release-reclaim.md`
- Cross-epic: `production/epics/crowd-npc-behaviour/EPIC.md` ([Dep:Crowd]) and `production/epics/shared-character-locomotion/EPIC.md` ([Dep:Shared Character]); spike S8 must report before this story is unblocked
- Unlocks: None
