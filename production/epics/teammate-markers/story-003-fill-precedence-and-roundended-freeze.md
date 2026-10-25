# Story 003: Fill precedence and RoundEnded freeze

> **Epic**: Teammate Markers
> **Status**: Ready
> **Layer**: Presentation
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

A marker shows three states of the marked ally, independent of the viewer: filled (Active), a two-band stepped hatch (concealed in a bush) and outline-only (Informant). This story implements the pure precedence function and the `RoundEnded` freeze rule. Precedence is outline first, then hatch, then filled; the hatch uses one source per entry kind (for a tracked entry the client-local `IsAllyBodyHidden(Pawn)` latch alone, for a fed entry the C-TeamView `IsConcealed` flag), so the hatch appears on the same frame the body disappears. The server-timed `IsConcealed` is never ORed into a tracked entry because it can arrive before the local swap or outlive the body's reappearance. `RoundEnded` is keyed on the round phase and tolerates the Roster and GameState arriving a net frame apart.

**GDD**: `design/gdd/teammate-markers.md` (TM-R5)
**Requirement**: `TR-tmk-003`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0021: UI/HUD layering, CommonUI and Enhanced Input (section 5)
**Also governed by**: ADR-0017: Concealment per-viewer presentation (the `IsAllyBodyHidden` public read, Accepted; its conditional gates S-CON-1 and R5 affect only the real latch, story 021)
**ADR Decision Summary**: Fill: informant gives outline; concealed gives hatch from `IsAllyBodyHidden(Pawn)` for tracked entries or the entry's concealed flag for fed entries; else filled; `RoundEnded` is frozen by phase.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Pure C++; the latch is injected as a callable so the test needs no engine object. The real latch is a Foundation-safe `Public/View` facade (`IsAllyBodyHidden(const AStealthPawn*)`, ADR-0017 section 5).

**Control Manifest Rules (this layer)**:
- Required: `IsMarkedAlly` and every presentation read come only from the local marker model built from the own controller's C-TeamView — source: ADR-0021
- Forbidden: Never read a team, informant or concealment field from a pawn — source: ADR-0001, ADR-0021
- Guardrail: Opacity encodes no state (art bible 3.4); fill is shape and pattern only

---

## Acceptance Criteria

*From GDD `design/gdd/teammate-markers.md` (AC-TMK-10, TM-R5), scoped to this story:*

- [ ] **AC1** (AC-TMK-10, precedence, [U] `FillPrecedence`): **GIVEN** every combination of entry kind {Tracking, Fed}, `State` {Active, Informant, RoundEnded}, phase {Active, Ended}, `IsConcealed` {false, true} and `IsAllyBodyHidden` {false, true} **WHEN** the fill is evaluated **THEN** Informant gives outline; else hatch iff (Tracking: `IsAllyBodyHidden` alone; Fed: `IsConcealed`), so a tracked entry with `IsConcealed` true and `IsAllyBodyHidden` false is filled; else Active gives filled.
- [ ] **AC2** (AC-TMK-10, RoundEnded): **GIVEN** a `RoundEnded` entry **THEN** in phase `Ended` it keeps the last fill seen this round (filled if none was seen); outside `Ended` it keeps the last fill if it was seen `Active` or `Informant` this round (early arrival, Round R9) and otherwise produces no marker.
- [ ] **AC3** (AC-TMK-10, instant swap): **GIVEN** any fill change **THEN** it is applied on the same frame with no cross-fade and no intermediate pattern, and the marker's opacity is identical before and after.

---

## Implementation Notes

*Derived from ADR-0021 §5 and TM-R5:*

Implement `EMarkerFill ResolveFill(const FMarkerEntryView&, EEntryKind, ERoundPhase, TFunctionRef<bool(const APawn*)> IsAllyBodyHidden, FMarkerFillHistory&)` as a pure function; `FMarkerFillHistory` remembers the last fill seen `Active` or `Informant` this round, per handle, cleared at `Reset`. Informants cannot conceal (IIF R7), so outline and hatch never compete. The `RoundEnded` rule has two halves: in `Ended` the fill is frozen at the last one seen (filled for a rejoin during `Ended`); before `Ended`, a `RoundEnded` entry already seen `Active` or `Informant` keeps its last fill (Round writes `EndRoundAll` in the same server step as `Ended`, but the Roster record and the GameState are different actors and may arrive a net frame apart), and an entry never seen produces no marker.

The test enumerates all 48 combinations (2 kinds x 3 states x 2 phases x 2 x 2) against a table written in the test file; the negative edges are a Tracking entry with `IsConcealed` true and the latch false (filled) and a Fed entry with the latch true and `IsConcealed` false (not hatched).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-019-paint-markers-in-world-anchor-leaf.md` and `story-020-glyph-states-and-edge-variant-evidence.md`: drawing the three glyphs.
- `story-021-hatch-on-body-hide-frame.md`: the real latch on the swap frame.
- `story-024-ended-and-reset.md`: the multi-client `Ended` and `Reset` behaviour.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthPresentation/Private/Markers/MarkerFill.h` and `MarkerFill.cpp`
- `Source/StealthPresentation/Private/Markers/TeammateMarkerModel.cpp` (calls `ResolveFill` each frame)
- `Source/StealthPresentation/Private/Tests/Markers/MarkerFillPrecedenceTest.cpp` (name `StealthGame.Markers.FillPrecedence`, class `FMarkersFillPrecedenceTest`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC1**: Given: all 48 combinations of kind {Tracking, Fed}, State {Active, Informant, RoundEnded}, phase {Active, Ended}, `IsConcealed` and `IsAllyBodyHidden`. When: the fill is evaluated. Then: Informant gives outline; else hatch iff (Tracking: `IsAllyBodyHidden`; Fed: `IsConcealed`); else Active gives filled. Edge cases: Tracking with `IsConcealed` true and `IsAllyBodyHidden` false is filled; Fed with `IsAllyBodyHidden` true and `IsConcealed` false is not hatched.
- **AC2**: Given: a `RoundEnded` entry whose history is seen-Active, seen-Informant, seen-hatched or never seen. When: phase is Ended, then phase is Active. Then: in Ended it keeps the last fill (filled if none seen); outside Ended it keeps the last fill only if it was seen Active or Informant, else no marker. Edge cases: history from a previous round is ignored (Round R9 early arrival).
- **AC3**: Given: any fill transition. When: it occurs. Then: it applies on the same frame with no cross-fade or intermediate pattern, and alpha is identical before and after. Edge cases: changes on consecutive frames.

QL-STORY-READY: ADEQUATE

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthPresentation/Private/Tests/Markers/MarkerFillPrecedenceTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/teammate-markers/story-002-marker-model-teamview-source-and-validation.md`
- Unlocks: `production/epics/teammate-markers/story-006-lifecycle-retire-hold-takeover-direct-switch.md`, `story-019-paint-markers-in-world-anchor-leaf.md`, `story-021-hatch-on-body-hide-frame.md`, `story-024-ended-and-reset.md`
