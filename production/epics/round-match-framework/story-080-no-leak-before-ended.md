# Story 080: Replicated-property sets match the allowlist and FRoundPublicState bytes are identical on 3 clients with zeroed times

> **Epic**: Round & Match Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25f
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-009` (`FRoundPublicState` identical bytes for every viewer; `ActiveAtServerTime` zeroed in Active; bounded late-loader reveal); GDD AC 27a; rules R20, R23 (c).
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0003: Replication, relevancy and the concealment cull hybrid
**ADR Decision Summary**: Uniform pawn net configuration for NPC and player, quantised server cull for non-allies plus client-local hide, fixed-width serialisers, and an allowlist audit of every replicated property. This story applies Decision section 8: one checked-in allowlist data file, an audit that enumerates replicated properties on class defaults and live instances and fails on any property missing from the allowlist or with a different condition, plus the identical-bytes parity run in the harness.
**ADR Version**: ADR-0003 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0003 knowledge risk)
**Engine Notes**: Touches the allowlist audit and parity harness of ADR-0003 section 8 and its Verification Required list only where a Round-owned property is replicated (`FRoundPublicState` on the GameState). The 5.5 setter-only net-config members (`SetNetUpdateFrequency`, `SetMinNetUpdateFrequency`, `SetNetCullDistanceSquared`) and `RemoteViewPitch16` are audited as pawn rows owned by Shared Character, not changed here. The cull and re-open spikes (S9, S10, S12, S13) are not exercised by this story; byte identity of fixed-width `NetSerialize` structs applies only if `FRoundPublicState` gets a custom serialiser (Open: confirm against ADR-0003 section 4 and the property-replication choice made in story 029).

**Control Manifest Rules (this layer)** (Round-relevant replication rules sit in the Foundation Layer Rules):
- Required: Every new replicated field, fact, audience, condition or replicated class ships with a Visibility Policy row and a signed-off allowlist row; the reflection audit fails the build otherwise (ADR-0001, ADR-0003).
- Forbidden: Never write a non-zero `TeamSummary` (or any per-team count) into `FRoundPublicState` before `Ended` (ADR-0002, user decision 2026-10-25).
- Guardrail: Replication: relevancy override O(1) per pawn-connection pair inside the 1.5 ms replication line; pawns at a fixed 15 Hz within 36 KB/s worst case (ADR-0003); `FRoundPublicState` is a small always-relevant struct inside that line.

**Prototype disposition**: None (new test and audit data). The prototype's replicated round fields on `AStealthGameState` are retired by story 029, not here.

**Where it sits**: Exit B (Core). It is the Pillar 1 leak guard for the replicated surface of an Active round: no opponent count, grace deadline, queue key or clock source reaches a client before `Ended`.

---

## Acceptance Criteria

*From GDD `design/gdd/round-match-framework.md` AC 27a, scoped to this story:*

- [ ] GIVEN 3 clients in Active, WHEN each client's replicated-property set is diffed against the checked-in allowlist, THEN the diff is empty, `FRoundPublicState` bytes are identical on all clients, `PhaseEnteredServerTime` and `ActiveAtServerTime` are 0; the same holds after an induced teammate disconnect and an opposing elimination.

---

## Implementation Notes

*Derived from ADR-0003 section 8 and Implementation Guidelines, ADR-0008 (allowlist path, harness) and GDD R20:*

- **Allowlist.** One checked-in JSON file, `Source/StealthFoundation/Private/Tests/Data/ReplicationAllowlist.json` (ADR-0008 fixes the path and the row schema `{class, property, condition, audience, carrier, owners[]}`). The Visibility Policy owns the file and its audience column; Shared Character authors the pawn rows; a row changes only with both owners' sign-off (IVP R11). The QA spec makes the file a deliverable of this story: the test fails if the file is missing, and this story adds the Round-owned rows (the GameState `FRoundPublicState` property, unconditional, All audience). Open: confirm the ownership split (file owner IVP, Round supplies the GameState rows) against ADR-0003 section 8.
- **The diff runs in both directions.** Enumerate `GetLifetimeReplicatedProps` through each class's rep layout on the class default object (including Blueprint replication lists) and on live instances in the test world (components, sub-objects). Fail on a property missing from the allowlist and on an allowlist row with no matching property.
- **Identical bytes.** Capture the serialised `FRoundPublicState` on each of three clients (listen-server host plus clients, or three clients per the harness shape) and compare byte for byte. `PhaseEnteredServerTime` is stamped for `Lobby`, `Preparing`, `Ended` and `Reset` only and is 0 during `Active`; `ActiveAtServerTime` is non-zero only in `Preparing` after P7 and is zeroed in the Active-entry step (R7), so both read 0 in Active.
- **Disturbances.** Repeat the three checks after an induced teammate disconnect and after an opposing elimination (use `stealth.round.debugeliminate` and a forced client drop). No Round event, banner or replicated field may change at a single elimination, disconnect or rejoin before `Ended` (R23 (c)); `TeamSummary` stays all zero, and opponent `ActiveCount`, `InformantCount`, `AwaitingPawn`, grace deadlines and queue keys never appear.
- **Test shape.** Multi-client PIE in `StealthTestHarness` with CQTest `NETWORK_TEST_CLASS(NoLeakBeforeEnded, "StealthGame.Round.PIE")`; this is the [PIE-3] test. Run under the harness `LeakNetProfile` (ADR-0008) where available so parity is checked under loss.
- Manifest 2026-10-25f: `CanViewerKnow` defaults to deny (unlisted fact, unknown viewer, client or off-thread call) and opponent counts leave the server only as `FRoundResult.TeamSummary` at `Ended`.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 029 (`production/epics/round-match-framework/story-029-public-state-replication.md`): the struct itself, `Phase` 0 = `Unknown`, re-seed after travel and `LastResult`.
- Story 042 (`production/epics/round-match-framework/story-042-active-gate.md`): the Active-entry step that zeroes `ActiveAtServerTime`.
- Story 087 (`production/epics/round-match-framework/story-087-takeover-no-pawn-property-delta.md`): per-pawn property deltas through takeover and hand-over (Blocked on spikes).
- Story 082 (`production/epics/round-match-framework/story-082-host-parity-includes.md`): the host-HUD include rule.
- The pawn-row content of the allowlist (Shared Character epic) and the Visibility Policy rows (information-visibility-policy epic).

---

## Files

*Planned files only. Tests live under the engine test roots (`tests/README.md`), not under `tests/`. `/dev-story` replaces this list with the files it actually changed.*

- `Source/StealthFoundation/Private/Tests/Data/ReplicationAllowlist.json` (shared deliverable; this story adds the Round rows)
- `Source/StealthTestHarness/Private/Round/NoLeakBeforeEndedTest.cpp` (CQTest `NETWORK_TEST_CLASS(NoLeakBeforeEnded, "StealthGame.Round.PIE")`, full name `StealthGame.Round.PIE.NoLeakBeforeEnded.<Method>`; the GDD scenario name is `StealthGame.Round.NoLeakBeforeEnded`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: Replicated-property sets match the allowlist and `FRoundPublicState` bytes are identical on 3 clients with zeroed times.
  - Given: 3 clients in Active and the checked-in replicated-property allowlist.
  - When: each client's replicated-property set is diffed against the allowlist, then an induced teammate disconnect and an opposing elimination occur.
  - Then: the diff is empty, in both directions.
  - Then: `FRoundPublicState` bytes are identical on all clients.
  - Then: `PhaseEnteredServerTime` and `ActiveAtServerTime` are 0.
  - Then: the same holds after the induced disconnect and elimination.
  - Edge cases: the allowlist file is a deliverable of this story and the diff fails if it is missing.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/Round/NoLeakBeforeEndedTest.cpp` (CQTest `NETWORK_TEST_CLASS(NoLeakBeforeEnded, "StealthGame.Round.PIE")` per the CQTest exemption in `.claude/rules/test-standards.md`) - must exist and pass.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-029-public-state-replication.md`, `production/epics/round-match-framework/story-042-active-gate.md`
- Unlocks: None
- Cross-epic: `production/epics/participant-state-roster` (C-TeamView and Roster records in the replicated sets; stories not yet created), `production/epics/shared-character-locomotion` (pawn rows of the allowlist; stories not yet created), `production/epics/tuning-data-config` (module skeleton and StealthTestHarness bootstrap; stories not yet created)
