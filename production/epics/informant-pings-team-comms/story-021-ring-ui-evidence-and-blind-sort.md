# Story 021: Ring UI evidence and blind-sort pre-screen

> **Epic**: Informant Pings & Team Communication
> **Status**: Blocked — BLOCKED: ADR-0021 verification items UI-V1, UI-V2, UI-V3 and UI-V4 have not reported (gates named in this epic's Build Order and Gates), and stories 019 and 020 (the painters) are Blocked on them. Unblocks when the UI-V reports exist and 019 and 020 are done.
> **Layer**: Feature
> **Type**: UI
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/informant-pings-team-comms.md` (AC-PNG-10; Accessibility paragraph of Visual/Audio Requirements; PNG-UI-1 to PNG-UI-3)
**Requirement**: `TR-png-014` (also `TR-png-015` and `TR-png-016`)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0021: UI/HUD layering, CommonUI and Enhanced Input (Validation Criteria and the PIE test list: `LayerOrder`, `PromptAndRing`)
**ADR Decision Summary**: The HUD ring presenter is validated by run-and-observe evidence in PIE against ADR-0021's verification items UI-V1 (zero-lag projection), UI-V2 (menu edge safety), UI-V3 (layer order) and UI-V4 (DPI); a parse check is not a run, so every UI story closes only with retained screenshots.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0021 overall)
**Engine Notes**: Procedure per engine is `.claude/docs/run-and-observe.md`. Capture through the project's play toolset or the editor screenshot path; the greyscale conversion method (for example a fixed luma formula) must be recorded so the sort is repeatable.

**Control Manifest Rules (this layer)**:
- Required: a UI story closes with a retained screenshot of each screen touched in `production/qa/evidence/`; an unretained capture is an assertion, not evidence — source: `.claude/docs/coding-standards.md`
- Forbidden: Never treat a successful compile or parse check as the run-and-observe step — source: `.claude/docs/coding-standards.md`

---

## Acceptance Criteria

*From GDD `design/gdd/informant-pings-team-comms.md` (AC-PNG-10 with ruling D36; AC4 added at QL-STORY-READY), scoped to this story:*

- [ ] **AC1** (AC-PNG-10 a): **GIVEN** a 2v2 PIE round in which an informant pings a market cluster **WHEN** captured **THEN** retained screenshots in `production/qa/evidence/` show, on an ally's screen, the on-screen ring with its name label at each of the three stroke weights and an edge ring for a ping behind the camera (no label), and the same moments on an opponent's screen showing nothing.
- [ ] **AC2** (AC-PNG-10 b, ruling D36, ADVISORY): **GIVEN** greyscale captures of a ring, a teammate marker and an interact ring **WHEN** sorted blind by three fresh-context reviewer agents with no access to the UI source or spec **THEN** all three sort 3 of 3 correctly and the result is recorded in the evidence note as an agent pre-screen; the criterion is ADVISORY until three human reviewers who did not author the UI repeat it at the first playtest, then BLOCKING again.
- [ ] **AC3** (derived, accessibility paragraph): **GIVEN** the evidence note **THEN** it states the game was launched and observed (not a parse check), cites the UI-V1..UI-V4 reports, and records that the ring is told from the marker by shape and from the interact ring by size and placement (not by colour alone).
- [ ] **AC4** (derived, `.claude/docs/coding-standards.md`: one screenshot per UI screen touched): **GIVEN** a concealed press (story 011) **WHEN** captured **THEN** a retained screenshot shows the HUD's "Not now" plate on the owner's screen and none on an ally's or an opponent's.

---

## Implementation Notes

*Derived from GDD AC-PNG-10, the D36 ruling in `production/design-closeout/2026-10-25_rereview-followups.md` and the run-and-observe procedure:*

The evidence note is `production/qa/evidence/informant-pings-ring-evidence.md`; captures are named `pings-<moment>-<viewer>.png`. Blind sort protocol (D36): produce the greyscale set with a recorded conversion; spawn three fresh-context sonnet agents (never fable) that receive only the images and the three class names (ring, teammate marker, interact ring) with no access to the repository, the GDD or the UI source; each returns a label per image; record the three answers verbatim. D36 makes the criterion ADVISORY until human reviewers run it at the first playtest, when it returns to BLOCKING (milestone M2 checklist item): leave a dated line in the note that says so and which humans have not yet run it. The screenshots themselves are not advisory: the project standard requires them retained at every `qa.level`.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Stories 019 and 020: the painters and their per-story captures.
- Story 011: the refusal behaviour (this story only captures the plate it shows).
- hud epic: the plate's own evidence for other rejection reasons.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `production/qa/evidence/informant-pings-ring-evidence.md` (evidence note with the sign-off)
- `production/qa/evidence/pings-*.png` (captures named in the note)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

**Manual check: AC1 retained ring screenshots (AC-PNG-10 a)**
- Setup: 2v2 PIE; an informant pings a market cluster; capture an ally's screen at ages ~1 s, ~4 s and ~7 s (strokes 6, 4, 3) plus a ping behind the camera; capture the same moments on an opponent's screen.
- Verify: a labelled on-screen ring at each stroke weight; an unlabelled edge ring for the behind-camera ping; the opponent screen shows no ring, plate or tick.
- Pass condition: all files exist in `production/qa/evidence/` and are named with the story and moment.

**Manual check: AC2 greyscale blind sort (AC-PNG-10 b, D36; ADVISORY)**
- Setup: convert the ring, teammate marker and interact ring captures to greyscale (record the conversion method); three fresh-context agents get the images only, with no UI source or spec.
- Verify: each agent labels each image.
- Pass condition: all three sort 3 of 3 correctly and the result is recorded in the evidence note as an agent pre-screen; ADVISORY until three human non-authors repeat it at the first playtest, then BLOCKING.

**Manual check: AC3 evidence note**
- Setup: write the note after the capture session.
- Verify: states the game was launched and observed (not a parse check); cites the UI-V1..UI-V4 reports; says ring-vs-marker is told by shape and ring-vs-interact by size and placement, not colour alone.
- Pass condition: all three statements present, with the cited reports existing on disk.

**Manual check: AC4 "Not now" plate**
- Setup: an Active player inside a bush presses Ping (Concealed state); an ally and an opponent observe.
- Verify: the owner's screen shows the "Not now" plate once per `RejectPlateCooldownSec` (3.0 s, read from the schema); no plate on the ally's or opponent's screen.
- Pass condition: three retained captures (owner, ally, opponent) exist and match.

QL-STORY-READY: ADEQUATE for AC1-AC3 (AC3 cannot pass until the UI-V1..UI-V4 reports exist, which the Blocked status states); AC4 added from the qa-lead's cross-story note.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: UI
**Required evidence**:
- UI: retained screenshots of each screen touched (on-screen ring at three stroke weights, edge ring, opponent view, "Not now" plate) in `production/qa/evidence/`, with the evidence note

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/informant-pings-team-comms/story-019-ring-painter-l0-leaf.md`, `story-020-edge-ring-painter-and-ui-scale.md`, `story-011-concealed-press-refusal.md`; cross-epic ADR-0021 UI-V1..UI-V4 reports
- Unlocks: None
