# Story 033: F7 returning countdown text and its Notice-slot rules

> **Epic**: HUD
> **Status**: Ready
> **Layer**: Presentation
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/hud.md`
**Requirement**: `TR-hud-004`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0021: UI/HUD layering, CommonUI and Enhanced Input
**ADR Decision Summary**: `FHudPresenterCore` owns F7 (the returning countdown) among its presentation state machines, computed from `ServerNowEst` and the Owner-only `T_due` carried by the C-TeamView owner part. All timed elements use `EstimatedServerNow` and its one slewed offset; text is formatted locally from `ST_Hud` and never travels on the wire.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Engine surface: none (plain C++ core, no UWorld/UObject); the module still builds against 5.8.4 / C++20. `T_due` is an Owner-only field of the C-TeamView owner part (user decision 2026-10-25; Roster R6, ADR-0001 §6, ADR-0019 §6), read through the presenter's carrier sampling; the core receives it as a double in `FHudInputs`. The countdown is first visible when Camera's fade completes for a killed informant (`DeathServerTime + DeathFallSec + NoPawnFadeSec`, Camera's `GetViewState()` returning to the neutral view); the core receives the fade-complete fact as an input and never drives Camera.

**Control Manifest Rules (this layer)**:
- Required: Presentation timing uses only `EstimatedServerNow()`; plate and card timelines are sampled from `now − t0` in C++ — source: ADR-0021, ADR-0002
- Forbidden: Never carry text on the wire — source: ADR-0021
- Guardrail: HUD ≤ 0.3 ms CPU (AC-HUD-22), ≤ 0.5 ms with markers; UI GPU ≤ 0.5 ms at 1080p on the reference GPU — source: ADR-0021

An informant awaiting a body sees one thing in the Notice slot: how many whole seconds until it returns. This story is the arithmetic and the visibility rules; the plate widget is the Notice-slot plate of story 032's family and the surrounding informant view is story 034.

---

## Acceptance Criteria

*From GDD `design/gdd/hud.md` AC-HUD-14 (F7 half) and HUD-R15 / UX element E11, scoped to this story:*

- [ ] **AC1** (AC-HUD-14 second half, F7; `StealthGame.HUD.CooldownAndCountdown` countdown cases): **GIVEN** `T_due` 230.0 **WHEN** now = 216.4, 229.1, and 230.3 with no pawn bound **THEN** the text is "Returning in 14", "Returning in 1" and "Returning" respectively.
- [ ] **AC2** (HUD-R15, UX E11; `StealthGame.HUD.CooldownAndCountdown` slot-rule cases): **GIVEN** states `InformantAwaiting` and `InformantBodied`, a card still up, a killed informant before fade completion and a non-owner carrier fixture **WHEN** `Step` runs **THEN** the countdown shows only in the Notice slot on the neutral view in `InformantAwaiting`, in whole-second steps, with no count-up and no pulse at "Returning"; it never appears before the card has collapsed (or Camera's fade for a killed informant) and is never shown to any other connection (a non-owner fixture produces no countdown).

---

## Implementation Notes

*Derived from ADR-0021 §4, GDD F7, HUD-R13, HUD-R15 and UX element E11:*

**What to build.** F7 inside `FHudPresenterCore` (`Source/StealthPresentation/Private/Hud/`): `s = ceil(T_due - ServerNowEst)`; text "Returning in " + s while `s >= 1` (`Hud.Countdown` in `ST_Hud`); "Returning" (`Hud.Countdown.Zero`) while `s <= 0` and no pawn is bound. At `s <= 0` with a pawn bound the countdown shows nothing (the takeover has landed and the view leaves `InformantAwaiting`). The view model carries the integer `s` (or the zero form) plus a visibility flag; the widget formats via the string table. `T_due` is Owner-only data: the countdown is live, resumes correctly after a reclaim or rejoin, and exists only on the owner's own client.

**Slot rules (HUD-R13, HUD-R15, E11).** Notice slot, highest priority there (nothing else can use the slot in `InformantAwaiting`; story 031 arbitrates). Visible only in `InformantAwaiting`, on the neutral view, after the removal card has collapsed, or for a killed informant (no card) after Camera's fade completes. In `InformantBodied` the countdown is absent. Whole-second steps only: the text changes when `ceil` changes, no animation, count-up, "searching" wording, pulse or colour change near zero (Penalty PEN-R14.3, UI-NKP-4, CR-12 c).

**Output range worth asserting.** First visible about `InformantRespawnDelaySec` - 6 s (about 9 at the default 15) after a card, about `InformantRespawnDelaySec` - 2 after a killed informant's fall; at `InformantRespawnDelaySec` <= 6 only "Returning" is seen after a card. These are consequences of F7 plus the card timing, not extra rules; a unit check of the first value is optional.

**Tests.** `Source/StealthPresentation/Private/Tests/HUD/CooldownAndCountdownTest.cpp`, class `FHudCooldownAndCountdownTest`, name `StealthGame.HUD.CooldownAndCountdown`. This test name is shared with story 018 (the F5 cooldown cases of AC-HUD-14); this story adds the countdown cases to the same test class, so if story 018 has landed extend the file rather than creating another. Use a fake clock and fake carriers; the non-owner fixture injects a carrier with no owner part.

**Traps.** Never show the countdown to any other connection (no carrier exists for it; the fixture proves the core outputs nothing). The text at 0 is static "Returning" and must not gain an ellipsis. Do not derive any clock from the countdown: no round clock exists and the HUD shows no other countdown (HUD-R14 Never list; the no-clock PIE proof is story 053).

**Prototype.** The prototype HUD has no countdown; nothing is promoted.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/hud/story-031-hud-notice-slot-priority-and-rate-limits.md`: Notice-slot arbitration.
- `production/epics/hud/story-032-hud-rejection-informant-and-hint-widgets.md`: the Notice-slot plate widgets (the countdown plate reuses the same dumb plate family).
- `production/epics/hud/story-034-hud-informant-presentation.md`: the informant view contract and the countdown in the PIE view dump.
- `production/epics/hud/story-018-hud-brace-indicator-state-logic.md`: the F5 cooldown cases of `StealthGame.HUD.CooldownAndCountdown`.
- `production/epics/hud/story-026-hud-removal-card-pie-timing.md`: PIE proof that the countdown appears at Camera's fade completion for a killed informant.
- `production/epics/hud/story-053-hud-no-clock-no-counts-pie.md`: the no-clock, no-counts proof.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthPresentation/Private/Hud/HudCountdown.h` and `.cpp` (F7 text and visibility, used by `FHudPresenterCore`)
- `Source/StealthPresentation/Private/Hud/HudPresenterCore.cpp` (wire F7 into `Step`)
- `Source/StealthPresentation/Private/Hud/HudInputs.h` (`T_due`, fade-complete fact, pawn-bound flag, if not present)
- `Source/StealthPresentation/Private/Tests/HUD/CooldownAndCountdownTest.cpp` (class `FHudCooldownAndCountdownTest`, name `StealthGame.HUD.CooldownAndCountdown`; shared with story 018)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: `CooldownAndCountdown` (countdown cases)
  - Given: `T_due` 230.0.
  - When: now = 216.4, 229.1, and 230.3 with no pawn.
  - Then: "Returning in 14", "Returning in 1", "Returning".
  - Edge cases: none.
- **AC-2**: slot rules
  - Given: states InformantAwaiting and InformantBodied, a card still up, a killed informant before fade completion, a non-owner carrier fixture.
  - When: `Step` runs.
  - Then: the countdown shows only in the Notice slot on the neutral view in InformantAwaiting, in whole-second steps. No count-up and no pulse at "Returning". Not before the card collapses (or Camera's fade for a killed informant). A non-owner fixture produces no countdown.
  - Edge cases: `s` <= 0 with a pawn bound shows nothing.

QL-STORY-READY: ADEQUATE

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthPresentation/Private/Tests/HUD/CooldownAndCountdownTest.cpp` (`StealthGame.HUD.CooldownAndCountdown`, countdown cases) — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`; gate BLOCKING)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/hud/story-002-hud-view-model-and-presenter-core-shell.md`, `production/epics/hud/story-014-hud-presented-own-state-and-viewer-state-machine.md`, `production/epics/hud/story-031-hud-notice-slot-priority-and-rate-limits.md`; `production/epics/npc-kill-penalty-informants/story-011-t-due-owner-carrier-adapter.md`
- Unlocks: `production/epics/hud/story-034-hud-informant-presentation.md`
