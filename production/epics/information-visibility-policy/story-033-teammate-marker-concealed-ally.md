# Story 033: The teammate marker tracks a concealed or unresolved ally and opponents create no marker

> **Epic**: Information Visibility Policy
> **Status**: Blocked — BLOCKED: [Dep:Teammate Markers] needs teammate-markers marker widgets and the concealment cull (story-024) (S9 reported PASS 2026-10-25, production/qa/spikes/2026-10-25_S9.md).
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

A culled ally has no actor on the viewer's client, yet the viewer must still see where the ally is, hatched when concealed. The marker therefore reads the local controller's C-TeamView (`PawnRef`, `IsConcealed`) and, when `PawnRef` does not resolve, the fallback position feed built in story-032. This story proves the three cases end to end (ally across the map, the same ally `Culled`, and the same ally with `PawnRef` forced unresolved), for an Active and then an Informant viewer, and that opponent clients create no marker widget because they hold no data at all. It protects Pillar 1 by making the absence of data, not a client-side filter, the enforcement mechanism. The criterion is provisional on spike S9.

**GDD**: `design/gdd/information-visibility-policy.md`
**Requirement**: `TR-ivp-009` (Teammate fallback position feed on C-TeamView)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0003: Replication, relevancy and the concealment cull hybrid (sections 5.4 and 7); ADR-0001: Crowd identity rules (section 5, "Teammate markers only on teammates' clients", host parity, section 4.6)
**ADR Decision Summary**: Markers are built from the local controller's C-TeamView entries and the fallback feed only; an opponent's controller is never relevant to you, so an opponent's client holds no entry and no feed for your team and never creates a marker. The feed carries the teammate's feet quantised to a 10 cm grid, added in the same frame the channel goes missing.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: E5/E10 (explicit close and a channel that stays closed) [V]; `UChannel::OpenAcked` [U] verified in story-032; a PlayerController is relevant only to its owner (ADR-0001 E6) [V], which is what makes the opponent case a data-absence property. Provisional on S9 (story-004).

**Control Manifest Rules (this layer)**:
- Required: **Team-audience data travels only in a `UTeamViewComponentBase` subclass on the receiving teammate's own PlayerController; owner-audience data only on the owner's PlayerController** — source: ADR-0001
- Required: **The teammate fallback feed is added in the same frame a teammate's channel goes missing, to teammate connections only; every entry carries a `uint8 Seq` advanced on every feed tick** — source: ADR-0003
- Forbidden: **Never use a team-filtered actor, a per-team replica actor or a team relevancy rule** — source: ADR-0001
- Forbidden: **Never branch on `IsPlayerControlled()`, `IsBotControlled()`, controller class, `PlayerState` presence, team or Roster state** in pawn, component, AnimBP or audio code for anything another client can observe — source: ADR-0001

---

## Acceptance Criteria

*From GDD `design/gdd/information-visibility-policy.md` (AC-IVP-15), scoped to this story:*

- [ ] **AC-IVP-15** [PIE-3][Dep:Teammate Markers] `StealthGame.Visibility.TeammateMarkerConcealedAlly`: GIVEN an ally across the map, the same ally `Culled`, and the same ally with `PawnRef` forced unresolved, WHEN the viewer (Active, then Informant) reads markers, THEN the marker tracks the pawn in the first two cases (hatched when concealed) and the fallback position within one grid cell in the third; opponent clients create no marker widget. (R8, R9a) Clarification (story-level, QL-STORY-READY): "tracks" means the marker world position equals the resolved pawn's feet position at each marker update (<= 1 cm) in the first two cases and is within one 10 cm grid cell in the third; "hatched" means the marker view-model's `IsConcealed` flag is true (asserted), plus a retained screenshot.

---

## Implementation Notes

*Derived from ADR-0003 sections 5.4 and 7, ADR-0001 sections 4.6 and 5, ADR-0008 sections 1-3:*

- Three cases, each repeated for an Active viewer and then an Informant viewer (informant allies are allies, the class is `TeammateInformant`/`Teammate` for the viewer's own teammates):
  1. Ally visible across the map: `PawnRef` resolves, the marker's world position equals the resolved pawn's feet position at every marker update (<= 1 cm).
  2. The same ally `Culled`: the actor is gone for the non-ally clients but stays relevant to the viewer (allies are exempt from the cull, section 5.4), `PawnRef` resolves and the marker view-model's `IsConcealed` flag is true (asserted in the test, and a screenshot of the hatched marker retained under `production/qa/evidence/`).
  3. The same ally with `PawnRef` forced unresolved (the ally is held stationary for this case): the marker uses the feed entry. Compare against the server pawn's feet position sampled at the tick whose `Seq` the client last received, quantised to the 10 cm grid, not against the live position; the marker is within one 10 cm grid cell of that reference.
- Opponent clients create no marker widget and hold no C-TeamView entry or feed for the other team. Assert on the opponent connection's replicated data and on the marker view-model list, not on a hidden widget.
- Negative control (QA, ADR-0008 section 3): a harness-only variant in which an opponent client receives a marker entry must be detected by the suite; the suite passes only if the control fires. The control type lives only in `StealthTestHarness`.
- The marker widgets and their view-model (`IsConcealed`, world position) are built by the teammate-markers epic; presentation code lives in `StealthPresentation` and reads only its own controller's carriers (ADR-0001 4.6, include lint rule h). This story consumes them; if the view-model is not yet available the story stays blocked.
- Prototype: Replaces: `AStealthCharacter::IsTeammateOfLocalViewer()` (reads the local PlayerController's PlayerState team) and `AStealthHUD::DrawTeammateMarkers` world iteration (`Source/StealthGame/Private/StealthHUD.cpp:171-174`, `TActorIterator<AStealthCharacter>` filtered by `IsNpc()`/`GetTeam()`). Promotes: nothing; the replacements are built by teammate-markers and verified here.
- T2 test: `NETWORK_TEST_CLASS(TeammateMarkerConcealedAlly, "StealthGame.Visibility.PIE")` with `WithGameMode(AStealthGameMode)`, fixture map, at least 3 clients (viewer, ally, opponent) so the opponent exists; bounded by server time and frames; the active replication system is asserted and recorded. Rendering-dependent checks (the hatched look) are evidence screenshots, not T2 assertions.
- Spike gate: story-004 (S9) must have reported, and story-024 (the cull) and story-032 (the feed) must be built.
- Cross-epic: needs teammate-markers' marker widgets and view-model, participant-state-roster's C-TeamView `PawnRef`/`IsConcealed` pushes (`SetTeammateConcealed` from Concealment), and concealment-bushes calling `SetCullState` (or the story-024 harness caller).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- story-024: the cull whose `Culled` state this story observes
- story-032: the fallback feed component and its budget
- story-021: team view never reaching opponents across takeover, disconnect and rejoin
- story-004: spike S9
- teammate-markers epic: marker widgets, staleness rule and the hatched presentation

---

## Files

*The code and test files this story creates or changes (planned):*

- `Source/StealthTestHarness/Private/Tests/PIE/TeammateMarkerConcealedAllyTest.cpp` (T2, `NETWORK_TEST_CLASS(TeammateMarkerConcealedAlly, "StealthGame.Visibility.PIE")`, with the harness-only opponent-receives-entry control)
- `production/qa/evidence/teammate-marker-concealed-ally-evidence.md` and the retained screenshot of the hatched marker in `production/qa/evidence/`

*Note: the `StealthTestHarness` module folder is created by tuning-data-config Exit A (it does not exist yet).*

---

## QA Test Cases

- **AC-1**: Teammate marker on a concealed ally (AC-IVP-15)
  - Given: an ally across the map, the same ally `Culled`, and the same ally with `PawnRef` forced unresolved (held stationary for this third case); the viewer is Active, then Informant.
  - When: the viewer reads markers.
  - Then: in the first two cases the marker's world position equals the resolved pawn's feet position at each marker update (<= 1 cm), and the marker view-model's `IsConcealed` is true in the `Culled` case (asserted, with a retained screenshot of the hatched marker); in the third case the marker is within one 10 cm grid cell of the server pawn feet position sampled at the tick whose `Seq` the client last received (quantised to the 10 cm grid); opponent clients create no marker widget.
  - Edge cases: repeat all three cases for the Informant viewer; compare the third case against the `Seq`-sampled reference, not the live position; the screenshot is evidence under `production/qa/evidence/`; negative control (added): an opponent client that receives a marker entry must be detected.

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/Tests/PIE/TeammateMarkerConcealedAllyTest.cpp` (T2 test, must exist and pass) OR a documented playtest or spike report, plus the retained hatched-marker screenshot in `production/qa/evidence/`

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/information-visibility-policy/story-024-concealment-cull-audience.md`, `production/epics/information-visibility-policy/story-032-team-fallback-feed-budget.md`; teammate-markers epic (marker widgets and view-model with `IsConcealed`); participant-state-roster epic (C-TeamView `PawnRef`/`IsConcealed` pushes)
- Unlocks: None
