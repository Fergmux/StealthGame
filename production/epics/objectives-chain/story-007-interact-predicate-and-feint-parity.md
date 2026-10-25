# Story 007: Interact predicate and feint/decoy parity

> **Epic**: Objectives Chain
> **Status**: Ready
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/objectives-chain.md` (OC-6, OC-7, OC-9, OC-11; AC-OC-04, AC-OC-30, AC-OC-31)
**Requirement**: `TR-oc-004` (tend predicate reads only the site, public occupancy and legality class)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0020: Objectives data and team-view sub-structs (Decision section 5.3)
**Other ADRs**: ADR-0011: Pawn command interface and input pipeline (`ICommandPredicate`, `Allow | AllowNoEffect | Reject`)
**ADR Decision Summary**: `CanBegin` for `Interact` is side-effect-free and reads only the court, its claim count and the pawn's legality class: `Informant` or `RoundEnded` gives `Reject(NotNow)`; a full fixture (`ClaimCount >= FixtureInteractSideCount`) gives `Reject(NotNow)`, identically at every court for every pawn; otherwise `Allow`, at every court whatever its role or state. `AllowNoEffect` is never returned at Begin: whether a tend counts is decided only at the completion slot.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: LOW (plain C++ predicate, no engine API)
**Engine Notes**: None. The static check is a source scan of the predicate translation unit, which the story's test performs (a grep or include scan), not an engine feature.

**Control Manifest Rules (this layer)**:
- Required: The Interact predicate reads only the court, its claim count and the pawn's legality class, and returns `Reject(NotNow)` or `Allow` — source: ADR-0020
- Forbidden: Never return `AllowNoEffect` at Begin from the Interact predicate — source: ADR-0020
- Forbidden: Never branch on player vs NPC in Objectives except the gate's server-only queries and the bound-only `TendCompleted` entry — source: ADR-0020
- Guardrail: Predicate O(1), inside the 0.5 ms queue line — source: ADR-0020, ADR-0011

The hidden-site oracle guard lives here. The prompt, the pose, the timer and the reply must be identical at every court, because any difference at Begin would tell a player which court is live (SP-1, OC-11). So the predicate must not know the role of any court, which is why it takes no team, no binding and no site state. A human feint (tend a court on purpose, to draw or test watchers) and an NPC decoy tend must be indistinguishable end to end (OC-9, Crowd CNB-R4): an objective interaction only NPCs could perform would prove NPC-ness. This story also fixes the Objectives side of AC-OC-31: a decoy shares a court with a tending human (it takes a free stand point) and is refused only when all four are claimed.

---

## Acceptance Criteria

*From GDD `design/gdd/objectives-chain.md` (AC-OC-04, AC-OC-30, AC-OC-31), scoped to this story:*

- [ ] **AC1** (AC-OC-04 [U] `PredicateSiteAndClassOnly`): **GIVEN** an Active-class pawn (human-bound, unbound NPC) and an Informant pawn **WHEN** `CanBegin(Interact)` is called on a live, locked, completed, opponent and hidden-final court with a free stand point **THEN** the Active pawns get `Allow` at all five and the Informant gets `Reject(NotNow)` at all five; **WHEN** all 4 stand points of the court are occupied **THEN** every pawn gets `Reject(NotNow)` at every court kind; the predicate's inputs are only (court, its public stand-point occupancy, legality class) (static check: the predicate translation unit calls no Roster team or binding query, no `FSiteAssignment` accessor and no `FObjectivesCore` site-state accessor); it never returns `AllowNoEffect`.
- [ ] **AC2** (AC-OC-30 [U] `FeintMatchesDecoy`): **GIVEN** an NPC decoy tend and a human feint at the same non-live court **THEN** both pass the same `CanBegin`, run the same IIF path, emit identical `OnActionCompleted` events and identical replication to non-owner clients (recording stub driver), and neither changes state.
- [ ] **AC3** (AC-OC-31 [U] `DecoyIncludesTendedCourts`, Objectives side): **GIVEN** a court with one stand point taken by a tending human **THEN** a decoy's `CanBegin` is `Allow` and its Begin takes a free stand point; **WHEN** all 4 points are taken **THEN** the decoy's Begin is `Reject(NotNow)`. (The 2,000-trial uniform court-choice chi-square belongs to the Crowd epic and consumes this predicate; it is not duplicated here.)

---

## Implementation Notes

*Derived from ADR-0020 Decision section 5.3 and GDD OC-6/OC-7:*

`UObjectivesService::CanBegin(const FCommandContext&)` (the `ICommandPredicate` for `EActionType::Interact`) implements exactly three steps: (1) `Roster.GetLegalityClass(Pawn)` in {`Informant`, `RoundEnded`} gives `Reject(NotNow)`; (2) `ClaimCount(Court) >= FixtureInteractSideCount` gives `Reject(NotNow)`; (3) otherwise `Allow`. `Reject(NotNow)` with the legality class is what also yields the IIF informant plate ("Informants can only observe") with no prompt shown. The claim count is the one story 006 maintains; it is public occupancy, what every observer can see, so the reply is no oracle. A body merely standing at a face is not counted (AC3 of story 006).

The predicate lives in its own translation unit so the static check is simple: the test greps the file (and its includes) for Roster team and binding queries (`GetTeam`, binding lookups), `FSiteAssignment` accessors and the core's site-state accessors, and fails on any hit; a seeded violation must fail the check. `GetLegalityClass` is the one Roster call allowed (Roster story 031 supplies it).

For AC2, build a recording stub replication driver for non-owner clients and normalise pawn ids and timestamps out before comparing; the NPC decoy is an unbound pawn issuing `Interact` through the same command interface (Crowd sinks), the feint a human-bound pawn. Both must reach the same `CanBegin` result and the same `OnActionCompleted` payloads; the IIF's `AllowNoEffect` outcome at completion changes nothing and is the same pose, timer and events (IIF story 028 owns the IIF-side parity, this story owns the Objectives side).

Prototype replaced: the prototype let the server decide interactability from the team's stage at Begin time (`HandleInteractRequest` checking `FStealthRules::CanCompleteObjective` before starting the hold), which leaks the live site by refusing the hold at every other court; replaced, not ported.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-006-fixture-claims-and-interact-target-owner.md`: the claim table and owner callbacks.
- `story-008-completion-gate-and-stage-effects.md`: whether a tend counts.
- Crowd epic: uniform decoy court choice and the 2,000-trial chi-square.
- `story-019-pie2-team-view-ingress-and-informant-parity.md` and `story-020-pie2-hidden-site-oracle-and-world-signature.md`: the multi-client replication and byte-exact reply checks.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Private/Features/Objectives/ObjectivesInteractPredicate.cpp` (the predicate translation unit)
- `Source/StealthGameplay/Private/Features/Objectives/ObjectivesService.cpp` (`ICommandPredicate` registration glue)
- `Source/StealthGameplay/Private/Tests/Objectives/PredicateSiteAndClassOnlyTest.cpp` (class `FObjectivesPredicateSiteAndClassOnlyTest`, name `StealthGame.Objectives.PredicateSiteAndClassOnly`)
- `Source/StealthGameplay/Private/Tests/Objectives/FeintMatchesDecoyTest.cpp` (`StealthGame.Objectives.FeintMatchesDecoy`)
- `Source/StealthGameplay/Private/Tests/Objectives/DecoyIncludesTendedCourtsTest.cpp` (`StealthGame.Objectives.DecoyIncludesTendedCourts`)

---

## QA Test Cases

*Written by qa-lead at story creation (QL-STORY-READY: ADEQUATE; a recording stub replication driver is needed for AC-2). The developer implements against these — do not invent new test cases during implementation.*

- **AC-1** (`PredicateSiteAndClassOnly`): predicate matrix
  - Given: pawns human-bound Active, unbound NPC Active, Informant; five courts (live, locked, completed, opponent, hidden-final) each with a free stand point; stub occupancy provider
  - When: `CanBegin(Interact)` for each pawn x court; then all 4 stand points occupied
  - Then: Active pawns Allow at all five; Informant `Reject(NotNow)` at all five; with 4 occupied every pawn `Reject(NotNow)` at every court kind; never `AllowNoEffect`; static check: predicate TU calls no Roster team or binding query, no `FSiteAssignment` accessor, no `FObjectivesCore` site-state accessor
  - Edge cases: 3 of 4 occupied still Allow; occupancy changes between calls re-evaluate; the static check fails on a seeded violation.
- **AC-2** (`FeintMatchesDecoy`): feint and decoy indistinguishable
  - Given: non-live court; NPC decoy and human feint; recording stub replication driver for non-owner clients
  - When: both run `CanBegin` and the IIF path to completion
  - Then: same `CanBegin` result; same `OnActionCompleted` payloads; identical recorded replication to non-owner clients; no state change either way
  - Edge cases: pawn ids are normalised out before comparison; timing class identical.
- **AC-3** (`DecoyIncludesTendedCourts`): decoy shares tended courts
  - Given: a court with 1 of 4 stand points taken by a tending human
  - When: a decoy calls `CanBegin` and Begin; then all 4 points are taken
  - Then: Allow and the decoy takes a free point; with 4 taken the decoy's Begin is `Reject(NotNow)`
  - Edge cases: the 2,000-trial chi-square belongs to the Crowd epic; here only predicate and occupancy are asserted.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: the three test files under `Source/StealthGameplay/Private/Tests/Objectives/` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/objectives-chain/story-006-fixture-claims-and-interact-target-owner.md`; cross-epic: `production/epics/participant-state-roster/story-031-legality-class-and-adoption.md` (`GetLegalityClass`), `production/epics/input-interaction-framework/story-003-action-hub-and-predicate-registry.md`, `production/epics/input-interaction-framework/story-028-allow-no-effect-parity.md`
- Unlocks: `production/epics/objectives-chain/story-008-completion-gate-and-stage-effects.md`, `production/epics/objectives-chain/story-019-pie2-team-view-ingress-and-informant-parity.md`, `production/epics/objectives-chain/story-020-pie2-hidden-site-oracle-and-world-signature.md`
