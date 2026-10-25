# Story 050: Killing an informant produces the same attacker-side RPCs, HUD and cues as killing an NPC

> **Epic**: Participant State & Roster
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/participant-state-roster.md`
**Requirement**: `TR-roster-011`, `TR-roster-008`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0001: Crowd identity rules
**ADR Decision Summary**: ADR-0001's identity invariants make everything observable about a body identical for every pawn kind: one pawn class, one command path, and carriers that hold only their audience, with the `ServerOnly` Roster answering "NPC or player or informant?" through `ResolveVictimKind` and that answer never reaching a client directly. The informant-kill invariant (GDD R10, binding on Assassination and NPC-Kill Penalty) requires that everything the attacker perceives after killing an `Informant`-kind victim is identical to an NPC kill, so the attacker's feedback cannot become a probe.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Post-cutoff: `APawn::RemoteViewPitch16` (5.6) [V]; `net.Iris.UseIrisReplication` default 0 (legacy NetDriver, Iris pinned off) [V]. Verification required: S1, S2, S14 and the harness startup assertion that the game NetDriver runs the legacy replication system. This story logs only attacker-side RPCs, HUD widget tree and cue ids, so the S1 pointer-level result is not a direct input.

**Control Manifest Rules (this layer)**:
- Required: every body is an `AStealthPawn`: one class, one component set, no per-instance replication or net-config difference — source: ADR-0001
- Required: every leak, parity, statistical and lint gate includes a negative control the suite detects; T2/T3 tests are bound by frames or server time and assert and record the active replication system — source: ADR-0008
- Required: outcome-bearing replicated structs serialise to a fixed bit width — source: ADR-0003
- Forbidden: Never branch on `IsPlayerControlled()`, `IsBotControlled()`, controller class, `PlayerState` presence, team or Roster state in pawn, component, AnimBP or audio code for anything another client can observe — source: ADR-0001
- Forbidden: Never put team, state, connection, appearance or pawn data on a PlayerState; never give NPCs a PlayerState — source: ADR-0001
- Guardrail: C-TeamView pushes change-only, at most one per frame, within the 50 KB/s per-client budget — source: ADR-0001

---

## Acceptance Criteria

*From GDD `design/gdd/participant-state-roster.md`, scoped to this story:*

- [ ] **AC-PSR-52** [PIE-3][Dep:NPC-Kill Penalty][Dep:Assassination][UI-ev] `StealthGame.Roster.PIE.InformantKillLooksLikeNpcKill`: GIVEN an attacker who kills an informant and, as a control, an NPC, WHEN the attacker client's received RPCs, HUD widget tree and played cue ids are logged, THEN the two logs are equal apart from the victim actor id (R10 informant-kill invariant).

---

## Implementation Notes

*Derived from ADR-0001 sections 1, 2, 5 and GDD R10 "Informant-kill invariant (P1)", R9 `RetireBinding`/`ResolveVictimKind`:*

- The Roster's part is that `ResolveVictimKind` maps an `Informant`-kind victim and an `Npc` victim into one attacker-visible outcome: Informant kills do not count as strikes, the true strike count is server-only and never replicated, and the attacker's HUD shows only a tally of `WrongTarget` plates that increments identically for both victim kinds. Everything else (animation, audio, kill confirmation, strike feedback, whether a visible strike is shown) is Assassination's and NPC-Kill Penalty's to produce identically; this story proves it end to end.
- Harness test on `StealthTestHarness`: a PIE session with one attacker client; the attacker kills an informant, then (control) an NPC. Log, on the attacker client, the received RPCs, the HUD widget tree and the played cue ids. Assert the two logs are equal apart from the victim actor id after normalization.
- Normalization (provisional, see Open point): two kills in one session differ in `ClientCommandSeq`, tally (1 versus 2) and timestamps. Run each kill in a separate round from an identical starting state, or list the normalized fields explicitly (victim actor id, `ClientCommandSeq`, tally, server and client timestamps) in the test fixture.
- Stubs: Penalty, Assassination, HUD and cues are stubbed until `production/epics/npc-kill-penalty-informants/EPIC.md` and `production/epics/assassination-defence-reversal/EPIC.md` land (HUD and cue ids come from `production/epics/hud/EPIC.md`). The stubs must implement the invariant faithfully (same tally increment, same plate, same cue ids for both victim kinds), otherwise the test only exercises the stub.
- Retain a screenshot of the attacker HUD after each kill in `production/qa/evidence/` as extra [UI-ev] evidence; the screenshot is human-reviewed, never the pass condition. Gating runs use fixed net emulation (PktLag 100 ms, no variance, PktLoss 0 %) with frame-counted timeouts; negative control: a stub that deliberately changes one cue id for informant victims must be detected.
- Open point: QA flags "the two logs are equal apart from the victim actor id" as too strict; the normalization list above needs sign-off.
- Open point: the three residuals permitted by R10 (removal timing, witness `WrongTarget`, awaiting-informant countdown) are outside this criterion and must not be asserted equal.
- Prototype disposition: no prototype counterpart (the prototype's `StealthGameMode.cpp` kill resolution distinguishes informant victims directly through `IsNpc()`/`IsInformant()` checks and is replaced).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 045 (Opponents receive nothing but allowlisted movement updates after a kill): opponent-side replication after a kill.
- Story 013 (ResolveVictimKind answers the full pawn-state matrix): the query matrix itself.
- Story 020 (Killing an informant retires its binding without a state event): the Roster-side `RetireBinding` behaviour.
- Story 055 (Same-tick assassinations and strike-outs resolve in queue order): same-tick ordering with Assassination and Penalty.
- Story 047 (No opponent count reaches a client before the round ends): opponent counts.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthTestHarness/Private/Roster/StealthRosterInformantKillParityTest.cpp` (CQTest `NETWORK_TEST_CLASS(InformantKillLooksLikeNpcKill, "StealthGame.Roster.PIE")`, exempt from the `F<System><Scenario>Test` class-name rule)
- `Source/StealthTestHarness/Private/Roster/StealthRosterAttackerLog.h` and `.cpp` (attacker-side RPC, widget-tree and cue-id logger with normalization)
- `Source/StealthTestHarness/Private/Roster/StealthRosterPenaltyAssassinationStub.h` (stubs of Penalty, Assassination, HUD and cue interfaces)
- `production/qa/evidence/` attacker HUD screenshots after each kill (extra [UI-ev] evidence)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation. (At tiers where the QL-STORY-READY gate is skipped — `minimal`, or `lean`/`solo` review mode — no qa-lead specs exist, and this section reads: N/A — no qa-lead specs at this tier; implement against the Acceptance Criteria above.)*

*Test cases not yet defined — run /qa-plan to generate them.*

**Flagged gap (QL-STORY-READY: GAPS)**: "the two logs are equal apart from the victim actor id" is too strict: two kills in one session differ in ClientCommandSeq, tally (1 vs 2) and timestamps. Needs: run each kill in a separate round from identical state, or list the normalized fields.

**Provisional spec (qa-lead, pending gap resolution)**:

- **AC-1**: killing an informant looks like killing an NPC.
  - Given: PIE with stubs for Penalty, Assassination, HUD, cues, identical starting state per run.
  - When: the attacker kills an informant, then (control) an NPC.
  - Then: received RPCs, HUD widget tree and cue ids equal apart from the victim actor id (after normalization).
  - Edge cases: a screenshot retained as extra evidence.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: automated test `Source/StealthTestHarness/Private/Roster/StealthRosterInformantKillParityTest.cpp` (`StealthGame.Roster.PIE.InformantKillLooksLikeNpcKill`) OR a documented playtest; `qa.level` is full, so the automated test is required.
- Extra: a retained attacker HUD screenshot per kill in `production/qa/evidence/` (human-reviewed, never the pass condition).

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/participant-state-roster/story-045-pie-elimination-leak-diff.md`
- Cross-epic: `production/epics/npc-kill-penalty-informants/EPIC.md` and `production/epics/assassination-defence-reversal/EPIC.md` ([Dep:NPC-Kill Penalty][Dep:Assassination]); stubbed until they land
- Unlocks: None
