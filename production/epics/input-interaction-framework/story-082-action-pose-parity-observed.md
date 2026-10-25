# Story 082: AC 46 ActionPoseParityObserved

> **Epic**: Input & Interaction Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/input-interaction-framework.md`
**Requirement**: `TR-iif-009`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0003: Replication, relevancy and the concealment cull hybrid
**Other ADRs**: ADR-0011: Pawn command interface and input pipeline
**ADR Decision Summary**: ADR-0003 gives every pawn, NPC or player, one net configuration (fixed 15 Hz update, no dormancy, no cull distance, `SimulatedProxy` for non-owners), forwards `ForceNetUpdate()` only inside a scoped `FStealthForcedNetUpdate(ActionStateWrite)` capped at one per pawn per frame, and serialises `FActionState` at a fixed bit width. ADR-0011 section 7 makes `UPawnCommandComponent::WriteActionState` the only writer of `FActionState`, identically for NPC and player.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: ADR-0003 (networking 5.4-5.8; the legacy NetDriver is used, per ADR-0001/ADR-0003) engine facts E8 and E16 (the `ForceNetUpdate()` override), E7/E15/E17 (net config and listen-host tick: the host frame time, not `NetServerMaxTickRate`, bounds `U_eff`). Verification items that condition this measurement: S13 (CMC cadence and correction parity for PC- versus AI-possessed pawns seen by a third client) and the fixed-width serialiser byte identity.

**Control Manifest Rules (this layer)**:
- Required: Every pawn carries exactly one non-replicated `UPawnCommandComponent`; only `UPawnCommandComponent::WriteActionState` writes `FActionState` — source: ADR-0011
- Required: Outcome-bearing replicated structs serialise to a fixed bit width with no value-dependent size — source: ADR-0003
- Required: `ForceNetUpdate()` on a pawn forwards only inside `FStealthForcedNetUpdate` with an allowed reason, at most once per pawn per frame — source: ADR-0003
- Guardrail: Replication: pawns at a fixed 15 Hz within the 36 KB/s worst-case line (60 pawns) — source: ADR-0003

---

## Acceptance Criteria

*From GDD `design/gdd/input-interaction-framework.md`, scoped to this story:*

- [ ] AC 46 (Integration, N `ActionPoseParityObserved`): **GIVEN** an NPC and a player each bracing 1.2 s **WHEN** an opponent client logs the Brace **THEN** the onset-to-offset durations differ by ≤ U + 1 frame; **AND** net update cadence and property size are identical.

---

## Implementation Notes

*Derived from ADR-0003 Decision sections 1, 2 and 4 and ADR-0011 Decision section 7; GDD F2 (U) and the Pillar 1 parity rule:*

**What this story builds.** The observed half of the ActionPose parity guarantee: an opponent client that logs a Brace on an NPC (driven through the null-sink AI path) and on a player (driven through the `ServerPawnCommand` RPC path), each held 1.2 s, sees the same onset-to-offset duration within `U + 1 frame` and the same net update cadence and serialised size. `U` is defined in F2 and F5 as the replication delay under forced net update, 0 to `dt_server` (qa-lead note). Story 069 proves the writer side (one forced net update per `ActionPose` write on both pawn kinds); this story proves what an observer can actually measure, which is the Pillar 1 question: nothing about a Brace tells a human from an NPC.

**Why N-form.** The measurement must hold with latency and loss on the link: net-emulation profile PktLag 50 ms per direction with 1 percent loss (the ADR-0008 default; the AC states no other). Jitter and resends are exactly the conditions under which a differing path could leak.

**Test notes (qa-lead).** NPC brace driven through the null-sink path, player brace through the RPC path, each held 1.2 s; the opponent client logs ActionPose onset and offset times. Assert `|duration_NPC - duration_player| <= U + 1 frame` with `U = dt_server` under forced update. Assert, from the opponent client's net stats, that update cadence and serialised property size are identical for both pawns. Repeat 20 times; test a tap (0.10 s, the debounce minimum brace) as well as the 1.2 s hold.

**Prototype.** Supersedes the prototype's `ServerSetDefence` / `OnDefencePressed/Released` brace path in `StealthCharacter.cpp` (the client chose the hold state with no queue slot); new code for the observer measurement. Final removal is story 093.

**Tuning keys (defaults).** `ReleaseDebounceSec` 0.10 (sets the tap duration); `PawnNetUpdateHz` 15 (SCL-owned, ADR-0003 section 1); `DefenceDurationSec` 4.0 (Assassination-owned; the 1.2 s hold is released by `End`, not by the cap).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/input-interaction-framework/story-069-force-net-update-on-write.md`: the writer-side spy (one forced net update per write, no dormancy).
- `production/epics/input-interaction-framework/story-084-pawn-replicated-value-parity.md`: replicated property name sets and per-property sizes for a bound versus an NPC pawn.
- `production/epics/input-interaction-framework/story-085-look-parity-observed.md`: the same observer measurement for Look.
- `production/epics/input-interaction-framework/story-087-action-pose-bandwidth.md`: ActionPose bandwidth at 50 pawns.
- `production/epics/input-interaction-framework/story-045-brace-window-bounds.md`: the server-side brace window bounds (F2).

---

## Files

- `Source/StealthTestHarness/Private/ActionPoseParityObservedTest.cpp` — CQTest `NETWORK_TEST_CLASS(ActionPoseParityObserved, "StealthGame.Input.PIE")`, full name `StealthGame.Input.PIE.ActionPoseParityObserved.<Method>`; net-emulation profile N (planned)

---

## QA Test Cases

**Automated test specs:**

- **AC-1**: NPC and player brace durations match within `U + 1 frame` as seen by an opponent client (`StealthGame.Input.PIE.ActionPoseParityObserved`).
  - Given: N profile (PktLag 50 ms per direction, 1 percent loss); an NPC brace driven through the null-sink path and a player brace through the RPC path, each held 1.2 s; the opponent client logs ActionPose onset and offset times. (`U` is defined in F2: replication delay under forced net update, 0 to `dt_server`.)
  - When: both complete.
  - Then: `|duration_NPC - duration_player| <= U + 1 frame` (`U = dt_server` under forced update).
  - Given: the opponent client's net stats.
  - Then: net update cadence and serialised property size are identical for both pawns.
  - Edge cases: repeat 20 times; a tap (0.10 s) as well as 1.2 s.

*QL-STORY-READY: ADEQUATE (qa-lead).*

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/ActionPoseParityObservedTest.cpp` (CQTest `StealthGame.Input.PIE.ActionPoseParityObserved`) must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/input-interaction-framework/story-069-force-net-update-on-write.md`; cross-epic: assassination-defence-reversal (stories not yet written; Prepare predicate); cross-epic: crowd-npc-behaviour (stories not yet written; NPC brace through the null-sink path); cross-epic: information-visibility-policy (stories not yet written; replication allowlist and parity tests)
- Unlocks: None
