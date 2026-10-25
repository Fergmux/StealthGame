# Story 044: The public player list is identical before and after eliminations and disconnects

> **Epic**: Participant State & Roster
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/participant-state-roster.md`
**Requirement**: `TR-roster-007`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0001: Crowd identity rules
**ADR Decision Summary**: ADR-0001 freezes the in-round player array: no PlayerState is created, destroyed, deactivated or renamed between `Preparing` and the next travel, a rejoiner is re-associated with its retained PlayerState, and the array reveals only the human count, which is public by design. `AStealthPlayerState` carries a closed public allowlist (`PublicHandle`, `PlayerNamePrivate`) so that no elimination or disconnect can change a byte a client sees. Team membership of opponents is derivable ("every public name not in my team view") but must never be presented as a grouping.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: ADR-0001 post-cutoff items: `APawn::RemoteViewPitch16` (5.6) and the `net.Iris.UseIrisReplication` default of 0 (legacy driver; the harness asserts the active replication system at startup). Verification items S1, S2, S14 (S2 and S14 PASS 2026-10-25; S1 PASS for the three pointers only). This story is a CQTest network test (`FPIENetworkComponent`, ADR-0008) in the `StealthTestHarness` module; rejoin and seamless travel are not exercised, so it does not depend on S3 or S4.

**Control Manifest Rules (this layer)**:
- Required: Every Integration story ships a T2 or T3 test or a documented playtest; T2/T3 code lives in `StealthTestHarness` — source: ADR-0008
- Required: T2/T3 tests are bound by frames or server time, and assert and record the active replication system — source: ADR-0008
- Required: Test names for T2: `NETWORK_TEST_CLASS(<Scenario>, "StealthGame.<System>.PIE")`; T2 tests set `WithGameMode(AStealthGameMode)` and load their fixture map — source: ADR-0008
- Forbidden: Never change the in-round player array; never enter the engine spectator state — source: ADR-0001
- Forbidden: Never put team, state, connection, appearance or pawn data on a PlayerState — source: ADR-0001
- Forbidden: Never use wall-clock time as a test pass/fail predicate (only as a failure ceiling); never prove multi-process criteria in in-process PIE only — source: ADR-0008
- Guardrail: Tests: per-commit pipeline at most 10 min; nightly cook and T3 estimated 1-2 h — source: ADR-0008

This story delivers the PIE proof that a team-1 client's public player list is byte-for-byte the same before and after an elimination and before and after a team-0 disconnect, and that it contains only name text in an order that encodes nothing, with no team, state, colour or count property. It exists because the list is the one UI element every client always sees, and any change in it would be a remote elimination or disconnect detector (Pillar 1). It realises GDD R6 ("the in-round name list is frozen"; "the public list must not present team as a grouping") and the Pillar-1 leak cases.

---

## Acceptance Criteria

*From GDD `design/gdd/participant-state-roster.md`, scoped to this story:*

- [ ] **AC-PSR-37** [PIE-3][UI-ev] `StealthGame.Roster.PIE.PublicListRevealsNothing`: GIVEN 3 clients plus the host, WHEN a team-1 client dumps the player-list widget tree mid-round, before and after an elimination and before and after a team-0 disconnect, THEN:
  - all four dumps are identical;
  - they contain only name text widgets in invariant-culture ordinal order;
  - they have no team, state, colour or count property, and the entry count is unchanged.

  Screenshots are kept as evidence (Player Fantasy anchor 1; R6).

---

## Implementation Notes

*Derived from ADR-0001 sections 4.3-4.4, ADR-0008 (T2 rules) and GDD R6, Edge Cases (Pillar 1 leak cases):*

- **Test shape.** CQTest network test `NETWORK_TEST_CLASS(PublicListRevealsNothing, "StealthGame.Roster.PIE")` in `Source/StealthTestHarness/Private/Roster/StealthRosterPublicListRevealsNothingTest.cpp` (full name `StealthGame.Roster.PIE.PublicListRevealsNothing.<Method>`; CQTest classes are exempt from the `F<System><Scenario>Test` rule). PIE listen server with 3 clients plus the host, `WithGameMode(AStealthGameMode)`, fixture map, fixed net emulation for the gating run (PktLag 100 ms, no variance, PktLoss 0 %). Every wait is a condition with a timeout counted in server frames or integer milliseconds; hitting the timeout fails the test. A separate non-gating Soak run (PktLoss 2 %, variance +-20 ms) is advisory.
- **Dump format.** The qa-lead gap says the list widget and its dump format are not defined in this epic. Define the dump as an ordered list of `(widget class, text)` entries plus a checked-in whitelist of readable widget properties (embed as a constant, for example `StealthRosterPublicListWhitelist.h`). "No team, state, colour or count property" is then checked as: every dumped widget exposes only the whitelisted properties (text), and none matches a deny-list. The team-1 client does the dump.
- **Stub widget until the HUD lands.** The real player-list widget belongs to the HUD. Build a test-only stub list in the harness that fills from the client's `GameState->PlayerArray` display names (PlayerNamePrivate) sorted by a locale-independent ordinal comparison (never `FText` culture sorting), so the test runs before the HUD exists. When the HUD list widget lands, point the same dump at it; the assertions do not change. Owning epic: the HUD & Event Feedback epic (design source `design/gdd/hud.md`; epic slug not named in the plan, see Open point).
- **Scenario.** Four dumps from the team-1 client mid-round: (1) before an elimination, (2) after it, (3) before a team-0 disconnect (taken after the elimination settles), and (4) after that team-0 client disconnects. Per the AC they must all be identical, including the entry count (the disconnected player's name stays; the in-round list is frozen).
- **Server-side drivers.** Drive the elimination through the Roster server calls (`RequestTransition(Informant, Assassinated)` plus the binding retire), since the Assassination epic is not a declared dependency; the disconnect is a real PIE client disconnect. These produce the events whose absence from the PlayerState is being tested.
- **Depends on story 040.** The list is only invariant because `AStealthPlayerState` replicates the closed allowlist; story 040 must have rewritten the class first.
- **Evidence.** Retain screenshots of the list at the four points in `production/qa/evidence/` (suggested prefix `participant-state-roster-044-`), captured per `.claude/docs/run-and-observe.md`. They are human-reviewed evidence, never the pass condition.
- **Open point:** the disconnect case needs the PlayerState to survive the controller's destruction in-round (`AStealthPlayerState::OnDeactivated` must not destroy it, ADR-0005 guideline 4) and `ShouldBroadCastWelcomeMessage` to return false (guideline 4a). Those are Session & Travel work and are not listed as dependencies of this story in the plan; confirm they exist before this test can pass, or the test will fail for a reason outside this epic.
- **Open point:** the plan carries no `[Dep:HUD]` tag although the real list widget is HUD-owned; the qa-lead asks that the owning epic be named. The stub avoids a blocker; name the HUD epic when it exists.
- **Prototype disposition:** no direct prototype counterpart. The prototype's player-facing team data lives on the replicated PlayerState (`Source/StealthGame/Public/StealthPlayerState.h`, replaced by story 040), which is what this test would catch if reintroduced.
- **Header naming (ADR-0007).** Stealth prefix, project-unique: test-only headers `StealthRosterPublicListWhitelist.h` and `StealthRosterPublicListStub.h` under the harness `Private/Roster/`.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 040 (PlayerState replicates only the allowlist and engine defaults are disabled or constant): owns the PlayerState class and its reflection audit; this story is the PIE proof on the player list.
- Story 045 (Opponents receive nothing but allowlisted movement updates after a kill of a player, NPC or informant): property-level leak diff for a kill.
- Story 046 (Non-owned pawns carry null PlayerState, Controller and Owner for players and NPCs alike): pawn pointer dump.
- Story 048 (The listen host's presentation model equals a client's and presentation code never includes the roster query header): host view parity.
- The HUD player-list widget itself (HUD epic): not built here.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- `Source/StealthTestHarness/Private/Roster/StealthRosterPublicListRevealsNothingTest.cpp` (CQTest network test, full name `StealthGame.Roster.PIE.PublicListRevealsNothing`)
- `Source/StealthTestHarness/Private/Roster/StealthRosterPublicListWhitelist.h` (checked-in property whitelist)
- `Source/StealthTestHarness/Private/Roster/StealthRosterPublicListStub.h` (stub list widget and dump helper, until the HUD list exists)
- `production/qa/evidence/` (retained screenshots of the four dumps)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation. (At tiers where the QL-STORY-READY gate is skipped — `minimal`, or `lean`/`solo` review mode — no qa-lead specs exist, and this section reads: N/A — no qa-lead specs at this tier; implement against the Acceptance Criteria above.)*

*Test cases not yet defined — run /qa-plan to generate them.*

**Flagged gap (QL-STORY-READY: GAPS):** the player-list widget and its dump format are not defined in this epic (they belong to HUD/UI and the story has no [Dep:] for them). "No count property" is unmeasurable without a property whitelist. Needs the dump defined as an ordered list of (class, text) plus a checked-in property list, and the owning epic named.

**Provisional spec (qa-lead, pending gap resolution):**

- **AC-1**: the public player list is identical before and after eliminations and disconnects.
  - Given: PIE with 3 clients and the host, a team-1 client, a stub widget until HUD lands.
  - When: four dumps: before and after an elimination, before and after a team-0 disconnect.
  - Then: all four identical, contain only name text widgets in invariant-culture ordinal order, no team, state, colour or count property. Entry count unchanged.
  - Edge cases: screenshots retained as extra evidence only.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/Roster/StealthRosterPublicListRevealsNothingTest.cpp` — automated test must exist and pass (OR a documented playtest; `qa.level` is full, so the automated test is required).
- Extra evidence (explicit, from the plan): retained screenshots of the player list at the four dump points in `production/qa/evidence/`. Human-reviewed; never the pass condition.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/participant-state-roster/story-040-player-state-replicated-allowlist.md`
- Unlocks: None
