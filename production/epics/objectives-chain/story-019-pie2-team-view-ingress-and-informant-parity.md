# Story 019: PIE-2 team view contents, ingress and informant parity

> **Epic**: Objectives Chain
> **Status**: Ready
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/objectives-chain.md` (OC-5, OC-6, OC-12 audience, OC-17; AC-OC-03, AC-OC-26, AC-OC-33)
**Requirement**: `TR-oc-003` (team view reaches only its team), `TR-oc-004` (ingress accepts any court; the predicate decides)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0020: Objectives data and team-view sub-structs (Decision sections 4, 5.3 and 6; Validation Criteria PIE-2/net harness)
**Other ADRs**: ADR-0008: Test automation and leak parity (T2 in-process multi-client CQTest tier); ADR-0001: the C-TeamView carrier
**ADR Decision Summary**: The team view is the only objective data any client receives; ingress accepts any court fixture id (public geometry) and the predicate and completion gate make every non-live court resolve identically; every member of team T, Active or Informant, gets the same view and a client of the other team gets none of it.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (ADR-0020 MEDIUM; ADR-0008 verification: CQTest PIE network tests under `-nullrhi` with a listen server and several clients is [U], `PIENetworkComponent.cpp:121-127`)
**Engine Notes**: T2 tests are CQTest `NETWORK_TEST_CLASS(<Scenario>, "StealthGame.Objectives.PIE")` (full name `StealthGame.Objectives.PIE.<Scenario>.<Method>`), living in `StealthTestHarness`. Single-process PIE cannot represent seamless travel (ADR-0005), so any rejoin check here uses the in-process rejoin path and the travel case stays in the Session harness.

**Control Manifest Rules (this layer)**:
- Required: Team-audience objective data travels only in `UObjectiveTeamViewComponent::View` — source: ADR-0020
- Required: Every member of team T, Active or Informant, receives the same view — source: ADR-0020, IVP R3
- Forbidden: Never put objective data on the GameState, `FRoundPublicState`, a PlayerState, a pawn, a fixture or an RPC — source: ADR-0020
- Guardrail: nothing objective-related to non-team connections — source: ADR-0020

This is the whole-state proof that nothing leaks. Story 010 shows the builder never puts a forbidden fact in a view, and story 011 shows the carrier reaches only the owner; this story dumps what each real client actually holds and checks it against the design (IVP AC-IVP-19 and AC-IVP-41 share fixtures with it). It also proves the other half of the hidden-site guard at the transport: a modified client may send `Interact(Begin)` with any court id, and a court that is not live for it must reach the queue and finish with no effect indistinguishably from its own live site, while an id that is not a court at all is dropped at ingress as an unknown id with no reply (IIF R5).

---

## Acceptance Criteria

*From GDD `design/gdd/objectives-chain.md` (AC-OC-03 PIE, AC-OC-26, AC-OC-33), scoped to this story:*

- [ ] **AC1** (AC-OC-03 [PIE-2] `StealthGame.Objectives.PIE.TeamViewContents`): **GIVEN** a round at `Active` start **WHEN** each client's replicated data is dumped **THEN** team 0 clients hold only `P1[0]` as a live site, team 1 clients only `P1[1]`, no client holds `Final`, `P2[*]` or any opponent site, and `FRoundPublicState` holds no objective field; **WHEN** team 0 completes both preliminaries **THEN** only team 0 clients gain `Final` (a late-joining client sees the same set).
- [ ] **AC2** (AC-OC-26 [PIE-2] `StealthGame.Objectives.PIE.IngressAcceptsAnyCourt`): **GIVEN** a human `Interact(Begin)` with a court id that is not live for its team (opponent, locked or completed court) **THEN** the command reaches the queue (not dropped at ingress) and completes with no effect, with acceptance, reply code, hold duration and timing class identical to an own live site (sharing the AC-IVP-41 fixture); **GIVEN** an id that is not a court fixture (including a malformed value) **THEN** it is dropped at ingress as an unknown id (IIF R5, no reply).
- [ ] **AC3** (AC-OC-33 [PIE-2] `StealthGame.Objectives.PIE.InformantsShareTeamView`): **GIVEN** a team with one Active member and one Informant **WHEN** a release raises that team's `RivalPrelimCount` **THEN** both clients receive the same count, stage card and live-site set in the same update and a client of the other team receives none of them; **GIVEN** the informant rejoins inside the grace **THEN** its team view equals the teammate's (Roster `OnViewRebound`).

---

## Implementation Notes

*Derived from ADR-0020 Decision sections 4-6 and ADR-0008 section 1 (T2):*

Harness: `AsListenServer().WithClients(2)` with `AStealthGameMode` (the possession-canary pattern, IVP story 019); a tool dumps each client's replicated state (properties, component state, `FRoundPublicState`) to a normalised snapshot. The test drives completions through the debug path (`DebugCompleteStage`, story 008) so no hold or movement is needed, except in AC2 where a real client sends the Begin. The fixture for AC2 is shared with AC-IVP-41 (IVP epic): verify at story start that it exists; if it does not, build it here and hand it to IVP.

AC2 compares, for an own live court, an opponent court, a locked court, a completed court and a non-fixture id, the observable results: acceptance, reply code, hold duration and timing class must be identical for the five court cases; the non-fixture id is dropped at ingress with no reply. The ingress filter accepts any court fixture id because public geometry names a court and nothing about its role (OC-6, IVP R7.4a); the team's live-site set is presentation only and never gates ingress or the predicate. The Objectives side of "identical" is story 007's predicate plus story 008's gate; the IIF side is IIF story 060 (the hidden-site probe).

AC3's release is driven by a test hook that schedules a rival release deterministically (the queue test seam from story 015's tests); the informant's rejoin uses the in-process rejoin path with the grace window. Completion by an Informant is impossible (the predicate rejects) and is not tested here.

Prototype replaced: the networked prototype tests D and D2 (full chain on a generated city) drove replicated actors; these tests replace their leak surface (the prototype replicated every site's `bCompleted` to everyone).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-011-team-view-component-and-replication.md`: the component, push model, allowlist and OBJ-V1/OBJ-V2.
- `story-020-pie2-hidden-site-oracle-and-world-signature.md`: the byte-exact owner reply and no-world-signature checks.
- IVP epic: `story-021-team-view-never-to-opponents.md` (the policy-level parity) and the AC-IVP-41 fixture.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthTestHarness/Private/Tests/PIE/Objectives/TeamViewContentsTest.cpp` (`NETWORK_TEST_CLASS(TeamViewContents, "StealthGame.Objectives.PIE")`)
- `Source/StealthTestHarness/Private/Tests/PIE/Objectives/IngressAcceptsAnyCourtTest.cpp` (`NETWORK_TEST_CLASS(IngressAcceptsAnyCourt, "StealthGame.Objectives.PIE")`)
- `Source/StealthTestHarness/Private/Tests/PIE/Objectives/InformantsShareTeamViewTest.cpp` (`NETWORK_TEST_CLASS(InformantsShareTeamView, "StealthGame.Objectives.PIE")`)
- `Source/StealthTestHarness/Private/Tests/PIE/Objectives/ObjectivesPieFixture.h` (shared fixture)

---

## QA Test Cases

*Written by qa-lead at story creation (QL-STORY-READY: ADEQUATE; check at story start that the AC-IVP-41 fixture exists). The developer implements against these — do not invent new test cases during implementation.*

- **AC-1** (`PIE.TeamViewContents`): replicated contents per client
  - Given: two-client PIE, 1v1, Active start; tool that dumps each client's replicated state
  - When: the dump is taken; then team 0 completes both preliminaries
  - Then: team 0 client holds only P1[0] live, team 1 client only P1[1]; no client holds Final, P2[*] or an opponent site; `FRoundPublicState` has no objective field; after both preliminaries only team 0 gains Final
  - Edge cases: late-joining client sees the same set; completion by an Informant is impossible and not tested.
- **AC-2** (`PIE.IngressAcceptsAnyCourt`): ingress accepts non-live court ids
  - Given: PIE client sending `Interact(Begin)`; fixture shared with AC-IVP-41 (verify it exists at story start); own live court, opponent court, non-fixture id
  - When: each Begin is sent
  - Then: non-live court id reaches the queue and completes with no effect, with acceptance, reply code, hold duration and timing class identical to the own live site; a non-fixture id is dropped at ingress as unknown id (IIF R5, no reply)
  - Edge cases: locked and completed court ids behave like the opponent id; malformed id value.
- **AC-3** (`PIE.InformantsShareTeamView`): informant and active member share a view
  - Given: team with one Active member and one Informant; opposing client; rival count release driven by test hook
  - When: a release raises `RivalPrelimCount`; then the informant rejoins inside the grace
  - Then: both teammates receive the same count, stage card and live-site set in the same update; the other team's client receives none; the rejoiner's view equals the teammate's
  - Edge cases: rejoin outside the grace; release in the same frame as the rejoin.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: the three T2 PIE tests under `Source/StealthTestHarness/Private/Tests/PIE/Objectives/` — must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/objectives-chain/story-007-interact-predicate-and-feint-parity.md`, `production/epics/objectives-chain/story-011-team-view-component-and-replication.md`, `production/epics/objectives-chain/story-012-rejoin-rebind-and-reset.md`, `production/epics/objectives-chain/story-015-rival-signal-release-scheduling.md`; cross-epic: `production/epics/input-interaction-framework/story-020-ingress-clamp-and-drop.md`, `production/epics/input-interaction-framework/story-060-hidden-site-probe.md`, `production/epics/information-visibility-policy/story-051-court-ingress-parity.md`; `production/epics/tuning-data-config/story-019-cqtest-headless-proof.md`
- Unlocks: `production/epics/objectives-chain/story-021-pie2-end-to-end-cacao-chain.md`
