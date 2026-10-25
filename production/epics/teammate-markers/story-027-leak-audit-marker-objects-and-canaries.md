# Story 027: Leak audit, no opponent marker objects or canaries

> **Epic**: Teammate Markers
> **Status**: Ready
> **Layer**: Presentation
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

The locked rule is that teammate markers exist only on teammates' clients, and the way to prove it is to attack it. This is the ship-gating half of the leak suite: in a real 2v2 match under the leak network profile, each opposing client's marker module and whole UObject graph are enumerated and must hold exactly its own teammate and nothing of the other team; a 64-bit canary planted by the server in each team-A record's server-only field must not appear anywhere in a team-B client's memory while the same scan of a team-A client finds its teammates' canaries (a positive control that proves the scan can see). A 2v2 match is deliberate: it is the smallest match in which the opposing clients' marker modules are live rather than inert. The accepted consequence that every unmarked figure is an NPC or an enemy is recorded, not treated as a defect. A failure of any criterion here blocks every build that leaves the team.

**GDD**: `design/gdd/teammate-markers.md` (TM-R1, Edge Cases audience and leak cases, Acceptance Criteria A; AC-TMK-01, AC-TMK-05, AC-TMK-28)
**Requirement**: `TR-tmk-001`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0001: Crowd identity rules (section 5, owner-only C-TeamView; host parity)
**Also governed by**: ADR-0008 (leak and parity test strategy, `LeakNetProfile`); ADR-0021
**ADR Decision Summary**: Team data reaches a client only through the owner-only C-TeamView on its own PlayerController; an opponent's controller is never relevant to you, so an opponent's client holds no entry and no feed for your team and never creates a marker: enforcement is by the absence of data.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: ADR-0001 conditional gates S1, S2 and S14 reported on 2026-10-25 (`production/qa/spikes/2026-10-25_S1-S2-S14.md`): PASS for the three pointers, so the data-absence property this story tests holds; the full pawn-parity S1 FAIL affects story 028 only. The canary plant is a test-build-only server field copied by the C-TeamView copy path if it ever leaked. A raw scan for 32-bit `PublicHandle` values is not used (chance matches and legitimate C-Public copies make it meaningless).

**Control Manifest Rules (this layer)**:
- Required: Team-audience data travels only in a `UTeamViewComponentBase` subclass on the receiving teammate's own PlayerController — source: ADR-0001
- Forbidden: Never use a team-filtered actor, a per-team replica actor or a team relevancy rule — source: ADR-0001
- Guardrail: The leak tests (AC-TMK-01 to 07) are ship-gating; leak tests run against the real server path, not a mocked view (GDD edge case)

---

## Acceptance Criteria

*From GDD `design/gdd/teammate-markers.md` (AC-TMK-01, AC-TMK-05, AC-TMK-28), scoped to this story:*

- [ ] **AC1** (AC-TMK-01, [PIE-3][Net][U] `NoOpponentMarkerObjects`): **GIVEN** a 2v2 match in `Active` with every pawn visible, one A player concealed and one A player an informant, under `LeakNetProfile` **WHEN** each B client's marker module and its whole UObject graph are enumerated **THEN** each B client's marker set equals exactly {its own B teammate}, it holds zero marker objects, set entries or `IsMarkedAlly` hits for any team-A pawn, and each A client's set equals exactly {its A teammate}.
- [ ] **AC2** (AC-TMK-05, [PIE-3][U] `OpponentMemoryAudit`): **GIVEN** the B client process after 60 s of play **WHEN** (a) a UObject-graph and reflection walk enumerates every instance of the C-TeamView entry type, the fallback-feed type and every marker-module class, and (b) in a test build the server plants a 64-bit random canary in a server-only field of each team-A record that the C-TeamView copy path would carry if it leaked, and the B process memory is scanned for those canaries **THEN** (a) finds no instance holding team-A data and (b) finds zero canaries, while the same scan of a team-A client finds its teammates' canaries (positive control; the test fails if the positive control finds none).
- [ ] **AC3** (AC-TMK-28, [PIE-3] `TwoVTwoNoExtraInfo`, revised): **GIVEN** the 2v2 test match **THEN** the AC1 and AC2 assertions pass, a retained screenshot of the opposing client's view shows no team-A marker or label, and the "reveal by elimination" note (every unmarked figure is an NPC or an enemy; expected and accepted, not a defect) exists in the test header.

*(Revised after QL-STORY-READY: AC3 restates a check instead of "recorded as accepted" alone.)*

---

## Implementation Notes

*Derived from AC-TMK-01, AC-TMK-05, AC-TMK-28:*

Use the leak harness from ADR-0008 and the net-emulation profile `LeakNetProfile` (PktLag 100 ms, 2 % loss; tuning-data-config story 017). The 2v2 scene: team A has one concealed player and one informant, team B has two players; enumerate on each B client the marker model, the marker set entries, and `IsMarkedAlly` over every pawn that client holds. The canary scan is a test-build-only facility: plant per-record 64-bit random values server-side in a field the C-TeamView copy path would carry if it leaked, then scan B's process memory; the positive-control scan on an A client must find its teammates' canaries or the test fails (so a broken scanner cannot pass). Retain the opposing client's screenshot (`tmk-leak-opponent-*.png`) as the AC3 evidence.

Record in the test header: reveal-by-elimination in a 2v2 is the expected, accepted consequence of having allies (GDD design risk "markers in 2v2 may reveal enemies by elimination", OQ8).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-028-leak-audit-reflection-and-packet-capture.md`: the reflection dump and the packet audit.
- `story-017-include-grep-ban-no-replicated-fields.md`: the static bans.
- `information-visibility-policy/story-021-team-view-never-to-opponents.md` and `story-033-teammate-marker-concealed-ally.md`: the Policy-side proofs.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthTestHarness/Private/Markers/NoOpponentMarkerObjectsTest.cpp` (CQTest, `StealthGame.Markers.PIE.NoOpponentMarkerObjects.*`)
- `Source/StealthTestHarness/Private/Markers/OpponentMemoryAuditTest.cpp` (CQTest, `StealthGame.Markers.PIE.OpponentMemoryAudit.*`)
- `production/qa/evidence/tmk-leak-opponent-*.png`

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC1**: Given: a 2v2 match in `Active`, every pawn visible, one A player concealed, one A informant, under `LeakNetProfile`. When: each B client's marker module and UObject graph are enumerated. Then: each B set = {its own B teammate}, with 0 marker objects, set entries or `IsMarkedAlly` hits for any team-A pawn; each A set = {its A teammate}. Edge cases: the concealed and informant A pawns are explicitly checked.
- **AC2**: Given: the B client after 60 s of play, with a server-planted 64-bit canary in each team-A record. When: a UObject-graph walk and memory scan run. Then: no instance holds team-A data and 0 canaries are found; the same scan of a team-A client finds its teammates' canaries (positive control). Edge cases: the positive control must fail the test if it finds none.
- **AC3**: Given: the 2v2 match. When: AC1 and AC2 run and the opposing client is captured. Then: both pass, the screenshot shows no team-A marker or label, and the reveal-by-elimination note is in the test header. Edge cases: none.

QL-STORY-READY: GAPS, revised in place per the agent's proposal (AC3 rewritten as a check); not re-run through the gate.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: the CQTest files above under `Source/StealthTestHarness/Private/Markers/` (BLOCKING, ship-gating) with the retained opposing-client screenshot

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/teammate-markers/story-016-presenter-local-controller-only.md`, `story-017-include-grep-ban-no-replicated-fields.md`; cross-epic `production/epics/tuning-data-config/story-017-net-emulation-profiles.md`, `story-019-cqtest-headless-proof.md`, `production/epics/information-visibility-policy/story-021-team-view-never-to-opponents.md`
- Unlocks: `production/epics/teammate-markers/story-028-leak-audit-reflection-and-packet-capture.md` (shared harness)
