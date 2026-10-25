# Story 028: Leak audit, reflection dump and packet capture

> **Epic**: Teammate Markers
> **Status**: Blocked — BLOCKED: ADR-0001 conditional gate: spike S1 reported FAIL for full pawn parity (two non-pointer differentiators; `production/qa/spikes/2026-10-25_S1-S2-S14.md`). AC-TMK-02(b) and AC-TMK-04 cannot pass until those differentiators are resolved under the Roster and Policy stories (`production/epics/participant-state-roster/story-049-pie-pawn-replication-parity.md` is Blocked on the same gate).
> **Layer**: Presentation
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

This is the wire half of the leak suite. A reflection dump of everything a team-B client received (every replicated property of every PlayerController, PlayerState, pawn and component, and the GameState) must hold no C-TeamView entry, `PawnRef`, `IsConcealed` flag or feed entry with a team-A handle and no PlayerController other than its own, and no pawn property may partition team-A pawns, team-B pawns and NPCs into separate value groups (except the allowlisted per-instance facts, whose distributions the IVP tests check instead). A 60 s server-side capture of every bunch sent to a B connection, run once with a dedicated-style client B and once with the listen host on team B and a remote team-A client, must contain no field that carries a team-A `PublicHandle` outside C-Public itself or links a team-A pawn's NetGUID to a handle, PlayerState or team. These are ship-gating once unblocked.

**GDD**: `design/gdd/teammate-markers.md` (TM-R1, Acceptance Criteria A; AC-TMK-02, AC-TMK-04)
**Requirement**: `TR-tmk-001`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0001: Crowd identity rules
**Also governed by**: ADR-0008 (raw-bunch capture, net-emulation profiles, leak schema parity); ADR-0003 (relevancy)
**ADR Decision Summary**: Every UE default that reveals player-ness is overridden; the pawn-to-participant link is owner-only; team data travels only in the owner-only C-TeamView; raw-bunch parity tests prove non-owners cannot tell player pawns from NPC pawns.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: The S1 report shows the three pointers (`PlayerState`, `Controller`, `Owner`) absent from non-owner changelists but two non-pointer differentiators remaining; whether `APawn::PlayerState`-style references are replicated by default in 5.8.4 is to be verified by this capture ([verify]). Capture hook: the server-side send hook of ADR-0008, decoded by property.

**Control Manifest Rules (this layer)**:
- Required: Every replicated property is on the audited allowlist; team-audience data only in the owner-only C-TeamView — source: ADR-0001, ADR-0003
- Forbidden: Never replicate a team, informant, concealment or marker field on a pawn — source: ADR-0001
- Guardrail: Leak tests run under `LeakNetProfile` (PktLag 100 ms, 2 % loss)

---

## Acceptance Criteria

*From GDD `design/gdd/teammate-markers.md` (AC-TMK-02, AC-TMK-04), scoped to this story:*

- [ ] **AC1** (AC-TMK-02, [PIE-3][Net][U] `NoTeamDataOnOpponentClient`, revised): **GIVEN** the 2v2 match **WHEN** a reflection dump of the B client's received replicated state is taken **THEN** (a) it holds no C-TeamView entry, `PawnRef`, `IsConcealed` flag or fallback feed entry with a team-A `PublicHandle` and no PlayerController other than its own, and (b) for every non-allowlisted pawn property the observed values (exact for discrete values, the [min, max] interval for continuous values) occur in at least two of the three groups (team-A pawns, team-B pawns, NPCs), with the allowlist stated in the test.
- [ ] **AC2** (AC-TMK-04, [PIE-3][Net] `OpponentPacketAudit`): **GIVEN** a 60 s capture of every bunch the server sends to a B connection, run once with a dedicated-style client B and once with the listen host on team B and a remote team-A client **WHEN** it is searched for any field in any channel that carries a team-A `PublicHandle` outside C-Public `PublicHandle` itself, or that links a team-A pawn's NetGUID to a handle, PlayerState or team (including `APawn::PlayerState`-style references) **THEN** there are zero matches (art bible 7.7 test 2, network half).

*(Revised after QL-STORY-READY: "value set" is defined for continuous values. The agent notes that the S1 FAIL makes AC1(b) fail by design until the dependency is resolved; that is why the story is Blocked.)*

---

## Implementation Notes

*Derived from AC-TMK-02, AC-TMK-04:*

Reuse the story 027 harness. The reflection dump enumerates, per class and property name, every replicated property of every PlayerController, PlayerState, pawn and component, and the GameState, as received by the B client; the allowlist of per-instance `All` facts is the IVP R3 list (transform, velocity, `AppearanceId`, `FActionState`, `FDeathState`), whose distributions are checked by AC-IVP-01 and AC-IVP-02 instead. For continuous properties compare the [min, max] interval of each group; "partitions" means an interval or value set that occurs in exactly one group.

Packet audit: decode the 60 s send-hook capture by property; search for team-A handles outside the C-Public copy and for NetGUID-to-handle, PlayerState or team links, including the `APawn::PlayerState` default. Run twice: dedicated-style client B, and the listen host on team B with a remote team-A client. A failure here is not fixed in this story; it files against the owning Roster, Policy or Pawn story.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-027-leak-audit-marker-objects-and-canaries.md`: marker objects, canaries, the 2v2 consequence.
- `information-visibility-policy/story-044-raw-bunch-schema-parity.md`: raw-bunch parity at the Policy level.
- `participant-state-roster/story-049-pie-pawn-replication-parity.md`: the pawn parity fix this depends on.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthTestHarness/Private/Markers/NoTeamDataOnOpponentClientTest.cpp` (CQTest, `StealthGame.Markers.PIE.NoTeamDataOnOpponentClient.*`)
- `Source/StealthTestHarness/Private/Markers/OpponentPacketAuditTest.cpp` (CQTest, `StealthGame.Markers.PIE.OpponentPacketAudit.*`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC1 (a)**: Given: a 2v2 match; a reflection dump of the B client's received replicated state. When: searched. Then: no C-TeamView entry, `PawnRef`, `IsConcealed` flag or fallback feed entry with a team-A `PublicHandle`, and no PlayerController other than its own. Edge cases: PlayerState and GameState included in the dump.
- **AC1 (b)**: Given: the same dump. When: each non-allowlisted pawn property's observed values are grouped by team-A pawns, team-B pawns and NPCs. Then: values (exact for discrete, [min, max] for continuous) occur in at least two groups for every such property. Edge cases: the allowlist is stated in the test.
- **AC2**: Given: a 60 s server send-hook capture to a B connection, once with a dedicated-style B and once with the listen host on team B plus a remote team-A client. When: searched. Then: 0 fields carry a team-A `PublicHandle` outside C-Public `PublicHandle`, and 0 link a team-A pawn's NetGUID to a handle, PlayerState or team. Edge cases: include `APawn::PlayerState`-style references (verify the 5.8.4 default). (Needs a real connection.)

QL-STORY-READY: GAPS, revised in place per the agent's proposal (AC1(b) "value set" defined); not re-run through the gate.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: the CQTest files above (BLOCKING, ship-gating) with the capture decode logs in `production/qa/logs/`

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/teammate-markers/story-027-leak-audit-marker-objects-and-canaries.md`; cross-epic `production/epics/information-visibility-policy/story-001-spike-s1-multiprocess-raw-bunch-capture.md`, `story-044-raw-bunch-schema-parity.md`, `production/epics/participant-state-roster/story-049-pie-pawn-replication-parity.md`
- Unlocks: None
