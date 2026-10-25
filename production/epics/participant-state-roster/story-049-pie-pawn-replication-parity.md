# Story 049: Player and NPC pawns replicate identically and bind, transition or possession changes no setting

> **Epic**: Participant State & Roster
> **Status**: Blocked — BLOCKED: spike S1 reported FAIL for full pawn parity (`RemoteViewPitch16` and the sample AnimBP `IsLocallyControlled` branch still differ player vs NPC, `production/qa/spikes/2026-10-25_S1-S2-S14.md`); the fixes live in ADR-0003/ADR-0013 work (shared-character-locomotion and camera-system epics). Unblocks when those land.
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/participant-state-roster.md`
**Requirement**: `TR-roster-011`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0001: Crowd identity rules
**Also governed by**: ADR-0010: Shared Pawn: CMC locomotion, spawn factory and possession parity; ADR-0003: Replication, relevancy and the concealment cull hybrid
**ADR Decision Summary**: ADR-0001 requires one pawn class (`AStealthPawn`) with one component set and no per-instance replication or net-config difference: nothing on the pawn may branch on controller kind, and `RemoteViewPitch16` is written in `PreReplication` from the `HeadLook` source for every pawn, with or without a controller. ADR-0010 makes binding, transition and possession leave the net config, movement mode, velocity and `HeadLook` unchanged (a kind-blind hand-over pass and `FScopedPossessionParity`), and ADR-0003 owns the fixed net config (setters, cadence, no dormancy) and the pitch mechanism.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Post-cutoff: `APawn::RemoteViewPitch16` (5.6; 8-bit `RemoteViewPitch` deprecated, `COND_ReplayOnly`) [V]; net-config members are setter-only since 5.5 (`SetNetUpdateFrequency`, `SetMinNetUpdateFrequency`, `SetNetCullDistanceSquared`); `net.Iris.UseIrisReplication` default 0 (legacy driver). Verification required: S1 raw-bunch parity including `RemoteViewPitch16` and the AnimBP `IsLocallyControlled` branch (S1 reported FAIL for full parity on 2026-10-25), S14, S8 and the ADR-0010 H1 hand-over spike for the possession-invariance part.

**Control Manifest Rules (this layer)**:
- Required: every body is an `AStealthPawn`: one class, one component set, no per-instance replication or net-config difference — source: ADR-0001
- Required: pawn net config is set only from tuning values, through the 5.5+ setters, identically for every pawn; `PreReplication` writes `RemoteViewPitch16` from `HeadLook` for every pawn, after `Super` — source: ADR-0003
- Required: every leak, parity, statistical and lint gate includes a negative control the suite detects — source: ADR-0008
- Forbidden: Never give a pawn dormancy, a cull distance, `bOnlyRelevantToOwner`, `bNetUseOwnerRelevancy`, a per-instance priority or team relevancy; never change net config per instance, kind or on possession — source: ADR-0003
- Forbidden: Never branch on `IsPlayerControlled()`, `IsBotControlled()`, controller class, `PlayerState` presence, team or Roster state in pawn, component, AnimBP or audio code for anything another client can observe — source: ADR-0001
- Guardrail: pawns at a fixed 15 Hz within 36 KB/s worst case (60 pawns) — source: ADR-0003

---

## Acceptance Criteria

*From GDD `design/gdd/participant-state-roster.md`, scoped to this story:*

- [ ] **AC-PSR-45** [PIE-3][S1][Dep:Shared Character] `StealthGame.Roster.PIE.PawnReplicationParity`: GIVEN 2 player pawns and 2 NPC pawns in view of an opponent client for 60 s of scripted identical motion and look, WHEN the client logs each pawn's received properties, subobjects, multicast RPCs, updates per second, bytes per second, dormancy transitions and `RemoteViewPitch` samples, THEN:
  - the player and NPC pawns show identical property, subobject and RPC sets;
  - their update rates and byte rates are within 10 % of each other;
  - none of them goes dormant;
  - their pitch distributions are drawn from the same look path.

  Binding, transition or possession changes none of the replication settings (R6 parity rule).

---

## Implementation Notes

*Derived from ADR-0001 sections 1, 2, 4.2 and Implementation Guidelines, ADR-0010 section 4, and GDD R6 "Pawn replication parity (requirement on Shared Character & Locomotion, binding)":*

- **Blocker, stated precisely:** S1 passed for the three pointers (`PlayerState`, `Controller`, `Owner`, Story 046) but failed for full pawn parity. Two non-pointer differentiators remain: `RemoteViewPitch16` differs between player and NPC pawns (measured) and must be driven from `Look` for both (ADR-0003 mechanism; GDD R6 requires AI pitch from the same `Look(Rotation)` path with a crowd-plausible distribution), and the sample AnimBP branches on `IsLocallyControlled`, so on the listen host NPCs and remote players animate differently (ADR-0013 section 1 removes the branch for every pawn). The sample's `CharacterInputState` and `TraversalResult` replicated properties must also join the parity allowlist audit (ADR-0001 amendments 2026-10-25). **Unblocks when** the shared-character-locomotion epic (`production/epics/shared-character-locomotion/EPIC.md`) and the camera-system epic (`production/epics/camera-system/EPIC.md`) land those fixes and S1 is re-run to PASS for full parity. Do not start the pitch clause before then.
- Parity scope (R6): `NetUpdateFrequency` and adaptive update frequency, `NetPriority`, `NetCullDistanceSquared`, dormancy (no pawn class may use it differently for NPCs), replicated components and subobjects, multicast RPCs (the same cue fires for a player action and the identical AI action), and `RemoteViewPitch`. Values live in config, not code. Binding, transition and possession never change any of them (ADR-0010 section 4.1: possession changes only the controller pointer, `Owner`, `PlayerState`, control rotation, owner-only camera/input setup and the control epoch).
- Harness test on `StealthTestHarness`: 2 player pawns and 2 NPC pawns in view of an opponent client for 60 s of scripted identical motion and look; the client logs each pawn's received properties, subobjects, multicast RPCs, updates per second, bytes per second, dormancy transitions and `RemoteViewPitch` samples. Assert identical property, subobject and RPC sets, update and byte rates within 10 %, no dormancy. Add a before/after snapshot of each pawn's replication settings around a bind, a transition and a possession change, asserting no setting changed (the QA report notes the AC names no comparison).
- Pitch clause (provisional, see Open point): compare the two pitch sample sets with a two-sample test and a stated threshold; held until the rewrite lands. Fixed net emulation (PktLag 100 ms, no variance, PktLoss 0 %), frame-counted timeouts, assert and record the legacy replication system; negative control with a deliberately mismatched pawn.
- [Dep:Shared Character]: the `AStealthPawn` replication configuration (`production/epics/shared-character-locomotion/EPIC.md`) is stubbed with the S1 spike configuration until it lands; the stub proves little, so this story stays Blocked.
- Open point: QA reports the pitch clause as not measurable (no statistical test or threshold) and "Binding, transition or possession changes none of the replication settings" as naming no comparison. A threshold and the before/after snapshot are needed before the criterion is testable.
- Prototype disposition: no prototype counterpart (the prototype's `AStealthCharacter` and sample AnimBP are the source of the measured differences and are replaced, not ported; `Source/StealthGame/Private/Spikes/StealthIdentitySpike.*` is reference only).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 046 (Non-owned pawns carry null PlayerState, Controller and Owner): the three engine pointers (S1 PASS).
- Story 043 (Player pawns are bound before BeginPlay and a failed bind destroys the pawn): bind-before-visible spawn path.
- Story 045 (Opponents receive nothing but allowlisted movement updates after a kill): kill-time replicated-update diff.
- Story 056 (A disconnect hand-over never decelerates faster than the crowd stop decel and never destroys the pawn): hand-over blend.
- Story 040 (PlayerState replicates only the allowlist): PlayerState property treatment.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthTestHarness/Private/Roster/StealthRosterPawnReplicationParityTest.cpp` (CQTest `NETWORK_TEST_CLASS(PawnReplicationParity, "StealthGame.Roster.PIE")`, exempt from the `F<System><Scenario>Test` class-name rule)
- `Source/StealthTestHarness/Private/Roster/StealthRosterParityLog.h` and `.cpp` (per-pawn received-property, rate and pitch logger)
- The pawn-side fixes (`AStealthPawn::PreReplication` pitch, AnimBP branch removal) are owned by the shared-character-locomotion and camera-system epics and are not files of this story.

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation. (At tiers where the QL-STORY-READY gate is skipped — `minimal`, or `lean`/`solo` review mode — no qa-lead specs exist, and this section reads: N/A — no qa-lead specs at this tier; implement against the Acceptance Criteria above.)*

*Test cases not yet defined — run /qa-plan to generate them.*

**Flagged gap (QL-STORY-READY: GAPS; also BLOCKED, separate from testability)**: "pitch distributions are drawn from the same look path" is not measurable (no statistical test or threshold). "Binding, transition or possession changes none of the replication settings" names no comparison. Needs a two-sample test with a threshold for pitch and a before/after settings snapshot.

**Provisional spec (qa-lead, pending gap resolution)**:

- **AC-1**: player and NPC pawns replicate identically.
  - Given: 2 player and 2 NPC pawns in view for 60 s of identical scripted motion and look.
  - When: the client logs properties, subobjects, multicast RPCs, rates, byte rates, dormancy, pitch samples.
  - Then: property, subobject and RPC sets identical. Update and byte rates within 10 %. None goes dormant.
  - Edge cases: the pitch clause is held until the rewrite. Unblocks with the shared-character-locomotion and camera-system work.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: automated test `Source/StealthTestHarness/Private/Roster/StealthRosterPawnReplicationParityTest.cpp` (`StealthGame.Roster.PIE.PawnReplicationParity`) OR a documented playtest; `qa.level` is full, so the automated test is required.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/participant-state-roster/story-043-player-pawn-bound-before-visible.md`
- Cross-epic: `production/epics/shared-character-locomotion/EPIC.md` and `production/epics/camera-system/EPIC.md` (pitch and AnimBP parity fixes; the unblocking condition)
- Unlocks: None
