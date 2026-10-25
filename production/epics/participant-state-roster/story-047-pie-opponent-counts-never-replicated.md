# Story 047: No opponent count reaches a client before the round ends

> **Epic**: Participant State & Roster
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/participant-state-roster.md`
**Requirement**: `TR-roster-008`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0001: Crowd identity rules
**ADR Decision Summary**: ADR-0001 gives each audience exactly one carrier and a carrier holds only its audience: `All` facts ride the pawn, C-Public on the PlayerState and GameState public structs; `Team` and `Owner` facts ride C-TeamView components on the receiving player's own PlayerController; `ServerOnly` facts (the Roster store, including every opponent count) are never a replicated property. Public structs carry no per-participant and no per-team value, which is how the "opponent counts never leave the server before `Ended`" rule is enforced by the absence of data rather than by a client-side filter.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Post-cutoff: `APawn::RemoteViewPitch16` (5.6) [V]; `net.Iris.UseIrisReplication` default 0 (legacy NetDriver, Iris pinned off) [V]. Verification required: S1, S2 and S14; harness startup assertion that the game NetDriver runs the legacy replication system. S2 reported PASS 2026-10-25 within its testable scope (legacy driver, property-level logging, one-process PIE).

**Control Manifest Rules (this layer)**:
- Required: Team-audience data travels only in a `UTeamViewComponentBase` subclass on the receiving teammate's own PlayerController; owner-audience data only on the owner's PlayerController — source: ADR-0001
- Required: every new replicated field, fact, audience, condition or replicated class ships with a Visibility Policy row and a signed-off allowlist row; the reflection audit fails the build otherwise — source: ADR-0001, ADR-0003
- Forbidden: Never write a non-zero `TeamSummary` (or any per-team count) into `FRoundPublicState` before `Ended` — source: ADR-0002 (user decision 2026-10-25)
- Forbidden: Never use a team-filtered actor, a per-team replica actor or a team relevancy rule — source: ADR-0001
- Guardrail: C-TeamView pushes change-only, at most one per frame, within the 50 KB/s per-client budget — source: ADR-0001

---

## Acceptance Criteria

*From GDD `design/gdd/participant-state-roster.md`, scoped to this story:*

- [ ] **AC-PSR-41** [PIE-3][S2] `StealthGame.Roster.PIE.OpponentCountsNeverReplicated`: GIVEN 3 clients plus the host in a 2v2 under emulation, with an instrumented replication log, WHEN opponents are eliminated one by one until one Active opponent remains, THEN:
  - before `Ended`, no client receives any property or RPC whose value changes with the opponent `ActiveCount`, `InformantCount` or `AwaitingPawn`;
  - no HUD widget shows an opponent count, which is checked by a [UI-ev] screenshot at "one opponent left";
  - after `Ended`, the post-round reveal may show the counts (R6 opponent-count rule).

---

## Implementation Notes

*Derived from ADR-0001 sections 4, 5 and Implementation Guidelines, and GDD R6 "Opponent counts never leave the server before `Ended`" (binding rule, Pillar 4 and Pillar 1):*

- The binding rule covers opponent `ActiveCount`, `InformantCount`, `AwaitingPawn` and anything derived from them ("1 enemy left", a remaining-rival icon, an elimination-progress bar). None may be replicated to, computed on, or presented by a client before Round phase `Ended`. Only the owner's own-team counts, derived from C-TeamView, may be shown. The Roster's count queries (R10, F1-F3) are ServerOnly and return 0 on a client (Story 004).
- Build this as a harness test on `StealthTestHarness` using the S2 method (3-client PIE, instrumented replication log of every `(actor class, property)` update and every RPC each client receives, legacy driver). Fixed net emulation per the GDD tags (PktLag 100 ms, no variance, PktLoss 0 %); waits are conditions with frame or millisecond timeouts; a separate non-gating [Soak] run repeats at PktLoss 2 %.
- Oracle (provisional, see Open point): a differential replication log. Run the same scripted 2v2 session twice, a control run and a run that differs only in the number of opponents eliminated (opponent `ActiveCount`/`InformantCount`/`AwaitingPawn` differ), and assert that the received logs on each non-ending client contain no property or RPC whose value differs between the two before `Ended`. After `Ended` the reveal (`FRoundResult.TeamSummary`, `EliminatedCount`) may differ.
- Add a widget-tree scan for opponent-count text on the HUD at the stage "one opponent left". The screenshot at that stage is retained in `production/qa/evidence/` as extra human-reviewed [UI-ev] evidence only; it is never the pass condition.
- Stage handling: eliminate opponents one by one until one Active opponent remains (stage "one opponent left"), then let the round end. Elimination uses the Roster `RequestTransition` path (Story 014) driven by a scripted Assassination/Penalty stand-in.
- Negative control (ADR-0008): include a run with a deliberately injected replicated opponent-count field (harness-only, `WITH_STEALTH_TEST_SEAMS`) to show the differential detects it.
- Open point: the QA report flags the HUD clause as having no automated pass condition and "no property or RPC whose value changes with the opponent count" as lacking an oracle. The differential log and widget scan above are the provisional oracle; the HUD widget tree format is owned by the HUD epic (`production/epics/hud/EPIC.md`) and the plan carries no [Dep:HUD] tag, so use a stub tree scan until it lands.
- Open point: phase `Ended` is published by Round (`production/epics/round-match-framework/EPIC.md`); stub the phase transition through the harness until that epic lands.
- Prototype disposition: no prototype counterpart (the prototype replicated team and life state to all clients through `Source/StealthGame/Public/StealthPlayerState.h`; the replacement is Story 040).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 045 (Opponents receive nothing but allowlisted movement updates after a kill): per-kill allowlist diff for player, NPC and informant victims.
- Story 037 (Maximal C-TeamView net-serializes in at most 96 bytes): the owner-view and team-view structure.
- Story 038 (SetOwnerTally and SetOwnerRespawnDue feed only the owner view): Owner-only tally feed.
- Story 044 (Public player list is identical before and after eliminations and disconnects): public list contents.
- Story 050 (Killing an informant produces the same attacker-side RPCs, HUD and cues as killing an NPC): attacker-side kill feedback parity.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthTestHarness/Private/Roster/StealthRosterOpponentCountsTest.cpp` (CQTest `NETWORK_TEST_CLASS(OpponentCountsNeverReplicated, "StealthGame.Roster.PIE")`, exempt from the `F<System><Scenario>Test` class-name rule)
- `Source/StealthTestHarness/Private/Roster/StealthRosterReplicationLogDiff.h` and `.cpp` (differential replication log helper; shared with Stories 045 and 050 if they land first, otherwise created here)
- `production/qa/evidence/` screenshot at "one opponent left" (extra [UI-ev] evidence)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation. (At tiers where the QL-STORY-READY gate is skipped — `minimal`, or `lean`/`solo` review mode — no qa-lead specs exist, and this section reads: N/A — no qa-lead specs at this tier; implement against the Acceptance Criteria above.)*

*Test cases not yet defined — run /qa-plan to generate them.*

**Flagged gap (QL-STORY-READY: GAPS)**: "no property or RPC whose value changes with the opponent count" has no oracle. The HUD clause has no automated pass condition (a screenshot is never a pass condition). Needs a differential replication log between two runs that differ only in opponent counts, plus a widget-tree scan for count text.

**Provisional spec (qa-lead, pending gap resolution)**:

- **AC-1**: no opponent count reaches a client before the round ends.
  - Given: PIE 2v2 under emulation, instrumented replication log, stage "one opponent left".
  - When: opponents eliminated one by one.
  - Then: before Ended the received logs show no property or RPC varying with opponent counts. After Ended the reveal may show counts.
  - Edge cases: the "one opponent left" screenshot is retained as extra evidence.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: automated test `Source/StealthTestHarness/Private/Roster/StealthRosterOpponentCountsTest.cpp` (`StealthGame.Roster.PIE.OpponentCountsNeverReplicated`) OR a documented playtest; `qa.level` is full, so the automated test is required.
- Extra: a retained screenshot at "one opponent left" in `production/qa/evidence/` (human-reviewed, never the pass condition).

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/participant-state-roster/story-045-pie-elimination-leak-diff.md`, `production/epics/participant-state-roster/story-037-team-view-serialized-size.md`
- Cross-epic: `production/epics/round-match-framework/EPIC.md` (phase `Ended`) and `production/epics/hud/EPIC.md` (widget tree scan); stubbed until they land (see Open points)
- Unlocks: None
