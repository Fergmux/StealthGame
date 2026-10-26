# Story 016: AC 31 (MVP clause only; the VS deferred gate ToggleStreamParity is out of scope)

> **Epic**: Input & Interaction Framework
> **Status**: Complete
> **Layer**: Core
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: 2026-10-26

## Context

**GDD**: `design/gdd/input-interaction-framework.md`
**Requirement**: `TR-iif-003`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0011: Pawn command interface and input pipeline
**ADR Decision Summary**: Bindings, the hold/toggle preference and sensitivity are client-local, never tuning; a hold/toggle translation (IIF R16) is not in MVP and, when added, would produce the identical Begin/End stream. Actions with server meaning never use Hold-style triggers, so the client never owns hold timing.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Input layer HIGH. The check enumerates registered settings in `UEnhancedInputUserSettings` (E4, replaces `UPlayerMappableInputConfig`), project settings classes and console variables; it uses no post-cutoff API beyond that. No V1-V6 item gates this story.

**Control Manifest Rules (this layer)**:
- Required: Rebinding uses `UEnhancedInputUserSettings` — source: ADR-0011
- Required: Gameplay input is bound on `AStealthPlayerController`; held commands map `Started` to Begin and both `Completed` and `Canceled` to End — source: ADR-0011
- Forbidden: Never use Hold/HoldAndRelease/Tap/Pulse/Chorded triggers for server-meaningful actions — the client would own hold timing — source: ADR-0011
- Forbidden: Never debounce releases client-side — source: ADR-0011
- Guardrail: Client-local settings (bindings, hold/toggle preference, Look sensitivity) are not tuning (TDC CR-14) — source: GDD Tuning Knobs

This story pins what the MVP does not have: no hold/toggle setting. GDD R16 describes a client-local accessibility option that would translate press/press into Begin/End for Prepare, Interact and Run, but "MVP ships hold only; the decision is an open question for Settings & Accessibility." A toggle setting that sneaked in early would be untested, would alter how holds are produced, and (per the AC's deferred VS gate) would need a stream-parity test that the MVP does not carry. This test is the MVP guard: it enumerates the settings surfaces and fails if any toggle-style setting for those actions exists.

The AC has two clauses; only the MVP clause is in this story. The Vertical Slice clause (deferred gate `ToggleStreamParity`) is listed under Out of Scope below with its GDD text. It depends on story 007 (the contexts and actions the surfaces refer to). GDD relation: R16, R4.

---

## Acceptance Criteria

*From GDD `design/gdd/input-interaction-framework.md`, scoped to this story (MVP clause only):*

- [ ] AC 31 (R16, Logic, MVP clause, `NoToggleSettingMvp`): MVP: **GIVEN** the MVP build **THEN** no toggle setting exists.

---

## Implementation Notes

*Derived from ADR-0011 Decision §9 (Rebinding bullet) and GDD R16:*

ADR-0011 §9: "Rebinding uses `UEnhancedInputUserSettings` (E4); `UPlayerMappableInputConfig` must not be used. Bindings, the hold/toggle preference and sensitivity are client-local, never tuning (TDC CR-14). A hold/toggle translation (IIF R16, not in MVP) would produce the identical Begin/End stream." GDD R16: "A client-local setting may translate press/press into Begin/End for Prepare, Interact and Run. The second press sends End, so early release stays available and stance durations are equivalent. The server stream is identical to hold mode, the setting is never replicated, and caps and cooldowns are unchanged. MVP ships hold only."

**QL-STORY-READY gap (to pin in the implementation):** "no toggle setting exists" is an absence claim with no defined search surface or name pattern. Proposed rewrite: "No `UEnhancedInputUserSettings` registered setting, `UDeveloperSettings`/`GameUserSettings` property, or console variable has a name containing both a hold/toggle term (Toggle, HoldMode, HoldToggle) and one of Prepare, Interact, Run; and no input-layer code path converts press/press to Begin/End." Test: enumerate the MVP build's settings surfaces (user settings class properties and registered settings, project settings classes, StealthGame console variables) and assert 0 matches; `IA_CameraToggle` is a binding, not a setting, so it is excluded by a name allowlist. The checker needs a seeded negative: a fake `bToggleHoldPrepare` property must fail it.

The prototype has no toggle setting. Tuning keys: none (hold/toggle preference is client-local, not tuning).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Vertical Slice (deferred gate `ToggleStreamParity`, not in the MVP plan and no story yet): "VS, deferred gate `ToggleStreamParity`: **GIVEN** scripted press/press timings in toggle mode, and the equivalent press/release timings in hold mode **WHEN** the server command log (type, payload, `ClientCommandSeq`, arrival tick) is captured for each **THEN** the two logs are identical."
- `story-012-edges-not-repeat.md`: the hold-mode Begin/End edge handling.
- `story-007-input-assets-and-trigger-lint.md`: the Hold/Tap trigger lint on assets.
- Settings & Accessibility epic (cross-epic, not yet written): the eventual hold/toggle setting.

---

## Files

*The code and test files this story changed (written by /dev-story; /story-done checks scope against it).*

- `Source/StealthGameplay/Private/Tests/Input/NoToggleSettingMvpTest.cpp`

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

*Test cases not yet defined — run /qa-plan to generate them.*

QL-STORY-READY: GAPS
- "No toggle setting exists" is an absence claim with no defined search surface or name pattern.
- QA-lead proposed rewrite (unconfirmed; the GDD AC stays the authority): "No UEnhancedInputUserSettings registered setting, UDeveloperSettings/GameUserSettings property, or console variable has a name containing both a hold/toggle term (Toggle, HoldMode, HoldToggle) and one of Prepare, Interact, Run; and no input-layer code path converts press/press to Begin/End."
- Scenario sketch from the qa file (`NoToggleSettingMvp`, AC 31 MVP clause only): enumerate the MVP build's settings surfaces (user settings class properties and registered settings, project settings classes, StealthGame console variables); 0 matches for the pattern above; `IA_CameraToggle` is not a match (a binding, excluded by name allowlist). Edge cases: the VS gate `ToggleStreamParity` is out of scope and listed as deferred; the checker seeded with a fake `bToggleHoldPrepare` property must fail.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Input/NoToggleSettingMvpTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [x] Created and passing (`StealthGame.Input.NoToggleSettingMvp`, 2026-10-26)

---

## Dependencies

- Depends on: `production/epics/input-interaction-framework/story-007-input-assets-and-trigger-lint.md`
- Unlocks: None

---

## Completion Notes
**Completed**: 2026-10-26 (IIF batch 13: command/reply types, core, hub, reply step, transport, seams, spikes V2/V3/V5, input ACs; D53 integration of the production input path)
**Criteria**: 1/1 passing (no project UDeveloperSettings/UGameUserSettings property, UEnhancedInputUserSettings property, builder property or Stealth* console variable names a hold/toggle term with Prepare, Interact or Run; seeded names fail; a second press while held is not an End)
**Deviations**: None beyond the pinned search surface.
**Test Evidence**: `Source/StealthGameplay/Private/Tests/Input/NoToggleSettingMvpTest.cpp`
Run result: N/A — an absence check
**Reviewed by**: lead-programmer focused review (sonnet; 3 medium + 6 low findings, all fixed except the documented limits) + self-review, IIF batch 13
