# Story 020: F6 tally display and the HUD-R7 hold, re-push gate and client-local cache

> **Epic**: HUD
> **Status**: Ready
> **Layer**: Presentation
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/hud.md`
**Requirement**: `TR-hud-004`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0018: Encounter data and outcome presentation timing (§5.4: HUD applies `TallyAfter` at the plate's `t_show`), with ADR-0021 (§4 `UStealthHudLocalStore`, the client-local store that outlives the controller)
**ADR Decision Summary**: The wrong-target tally is applied from `TallyAfter` at the show time of the plate that carries the increment, and no other path presents it. Client-local stores that outlive the PlayerController (the HUD-R7 tally display cache keyed by `RoundEpoch`, bridging until the C-TeamView owner re-push lands) live in a `ULocalPlayerSubsystem`, `UStealthHudLocalStore`, never in a replicated value.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: The F6 logic is a plain C++ core with no engine surface; the module still builds against 5.8.4 / C++20. The one engine type is `UStealthHudLocalStore : ULocalPlayerSubsystem`, a standard LocalPlayer-scoped subsystem (it outlives the PlayerController across reconnect and seamless travel but not a client process restart); the core reaches it through a plain interface so tests need no `ULocalPlayer`. ADR-0018's HIGH networking domain covers the owner re-push fields `{WrongTargetTally, LastResultSeq}` on the C-TeamView owner part (approved by the user 2026-10-25, Penalty CR-19), which arrive as owner data sampled by value. Use `UE_LOGF` for diagnostics.

**Control Manifest Rules (this layer)**:
- Required: Presentation reads only its own controller's carriers, the GameState public structs and client-local queries — source: ADR-0001, ADR-0021
- Forbidden: Never iterate actors, read a pawn's `PlayerState`/`Controller`/`Owner`, call `IsPlayerControlled`/`IsBotControlled`/`CanViewerKnow`, include a `Public/Server/` header, or write anything replicated from Presentation — source: ADR-0021, ADR-0001
- Guardrail: HUD ≤ 0.3 ms CPU (AC-HUD-22), ≤ 0.5 ms with markers; UI GPU ≤ 0.5 ms at 1080p on the reference GPU — source: ADR-0021

The tally is the count of wrong-target kills against the strike limit, and its timing carries the same outcome-hiding duty as everything else: it moves only with its plate (HUD-R7), never before, so a replicated counter update arriving early cannot reveal a strike's outcome. This story builds the F6 display rules and the three paths by which the displayed value can change (a plate's `TallyAfter` at its show time, the owner re-push when no plate is pending, and the client-local cache across a rebuild), with the gating that stops any of them lowering the value or applying early.

---

## Acceptance Criteria

*From GDD `design/gdd/hud.md` (AC-HUD-10, HUD-R7, F6), scoped to this story:*

- [ ] **AC1** (AC-HUD-10, `StealthGame.HUD.TallyDisplay`): **GIVEN** for `L = 3` and `n = 0..5` **THEN** filled = 0,1,2,3,3,3 and the triangle shows for n ≥ 2; the line reads "Wrong targets: n" and never contains "/3" or the limit; `L = 1`: triangle at n = 0; `L = 5`: triangle from n = 4; `n = 255`: L squares, triangle, "Wrong targets: 255"; `PresentedOwnState = Informant` at n = 3: 3 squares, no triangle, numeral frozen.
- [ ] **AC2** (derived, HUD-R7, `StealthGame.HUD.TallyRepushGate`): **GIVEN** an owner tally re-push arriving before its plate **THEN** the displayed value does not change; it is used directly only when no plate is pending and only if `LastResultSeq` >= the last plate's seq; it never lowers the displayed tally within a round.
- [ ] **AC3** (derived, HUD-R7 and ADR-0021 §4, `StealthGame.HUD.TallyRepushGate`): **GIVEN** the display cache keyed by `RoundEpoch` **THEN** it lives in `UStealthHudLocalStore` (a `ULocalPlayerSubsystem`), bridges a rebuild until the re-push lands, never overrides a passing re-push, and a new `RoundEpoch` shows 0.

---

## Implementation Notes

*Derived from GDD HUD-R7, F6, D4, ADR-0018 §5.4 and ADR-0021 §4:*

**What to build.** (1) `FHudTallyModel` in `Source/StealthPresentation/Private/Hud/`, plain C++, stepped from `FHudPresenterCore::Step`, writing `FHudTallyView` (`Filled`, `SquareCount` = L, `bTriangle`, `Numeral` as the formatted `Hud.Tally.Line` text "Wrong targets: {n}", `FillStepIndex` for the widget's 300 ms stepped fill from story 016's timeline, `bFrozen`). (2) `UStealthHudLocalStore` in `Source/StealthPresentation/Private/Hud/StealthHudLocalStore.h/.cpp`, a `ULocalPlayerSubsystem`, with `GetTallyCache(RoundEpoch)` and `SetTallyCache(RoundEpoch, n)`; the model talks to it through a small `IHudLocalStore` interface so the logic test uses a fake. ADR-0021 §4 also assigns the third-person-hint profile flag to this store (used by a later story); this story adds only the tally cache.

**Formula (F6).** `filled = min(n, L)`; `triangle = (n ≥ L − 1) ∧ PresentedOwnState = Active` (frozen informant tallies show no triangle, ruling D4); line "Wrong targets: " + n; `n` = `TallyAfter` of the latest shown plate (HUD-R7), else the owner re-push when no plate is pending. `L` is `NpcKillStrikeLimit` from the public tuning snapshot (2-5, default 3; 1 only in a test preset with `bTestAllowsStrikeLimitOne`), latched, so read it from `GetClientTuning()` at use, not at construction. The limit is never written as a number: the squares encode it, and the numeral string must never contain "/3" or L (the language lint of story 005 covers the string table; this story's test asserts the formatted line). The triangle is kept for `n > L`; nothing escalates beyond it. `n` is a `uint8` (0-255): n = 255 gives L squares, a triangle and "Wrong targets: 255" (saturated, no wrap).

**The moves, in priority.**
1. A plate's `TallyAfter`: applied at the plate's show time `t_show` from story 015 (the plate's `FStrikeResult.TallyAfter`), not on receipt and not at the Pending to Scheduled transition. This is the only source that raises the displayed value live.
2. The owner re-push `{WrongTargetTally, LastResultSeq}` on the C-TeamView owner part: held while any result plate is pending (the HUD keeps the last shown value), used directly only when no result plate is pending (a rebuild after reconnect, travel or a late join) and only if `LastResultSeq` is at least the sequence of the last plate shown (equal passes: the boundary case is in the test). It never lowers the displayed tally within a round (Penalty CR-12 b): `displayed = max(displayed, accepted)` within one `RoundEpoch`.
3. The client-local cache in `UStealthHudLocalStore`, keyed by `RoundEpoch`: written whenever the displayed value changes; read on a rebuild to bridge the gap until the re-push lands; it never overrides a re-push that passes the gate above (the re-push is the authority when it is accepted); a new `RoundEpoch` shows 0 and a cache entry with another epoch is ignored and replaced. The store is lost on a client process restart, so the re-push alone then applies; test this case as "store empty, re-push applies".

**Informant freeze (D4).** While `PresentedOwnState = Informant` the numeral and squares are frozen at their last shown value and stay visible as a record, the triangle is removed, and no animation runs. After an exile at n = 3 the informant sees "Wrong targets: 3", 3 squares, no triangle. Because the exile card and the flip are held to the card time (story 024), the freeze begins at the flip, not on receipt of the raw `State`. The tally is shown to informants as a frozen record only: no later plate or re-push changes it after the flip.

**Why held values.** A tally re-push that arrived before its plate and was shown early would reveal that the strike was a wrong target before the contact frame, which is the leak HUD-R3 and AC-HUD-01 exist to prevent. The test must drive the re-push ahead of the plate by a full second and assert the dump is unchanged until `t_show`.

**Decisions that apply.** D4 (no triangle for informants), D9 (a late plate shows on processing; the tally then moves with it), D11 wording (exile reserved for the strike-limit removal; the line text is "Wrong targets: {n}" only).

**Prototype.** The prototype tracked the strike count in the canvas HUD from replicated pawn data (`Source/StealthGame/Private/StealthHUD.cpp`); that read of a team field replicated on every pawn is a leak and is never promoted.

**Out of this story:** the widget (story 021), the plate timing (015), applying a plate at all (story 022 wires result plates; this story consumes `t_show` and `TallyAfter` from the result timer's output), the PIE parity proof (story 027).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/hud/story-021-hud-wrong-target-tally-widget.md`: squares, numeral and triangle rendering.
- `production/epics/hud/story-015-hud-result-show-time-f2.md`: `t_show` itself.
- `production/epics/hud/story-022-hud-result-and-defence-plate-flow.md`: the result plate that carries `TallyAfter`.
- `production/epics/hud/story-027-hud-tally-moves-with-plate-and-informant-parity-pie.md`: the multi-client PIE proof that the tally moves with the plate and that informants see parity.
- `production/epics/hud/story-044-hud-reset-and-rebuild-logic.md`: the rebuild procedure that calls into the cache path.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthPresentation/Private/Hud/HudTallyModel.h`
- `Source/StealthPresentation/Private/Hud/HudTallyModel.cpp`
- `Source/StealthPresentation/Private/Hud/StealthHudLocalStore.h`
- `Source/StealthPresentation/Private/Hud/StealthHudLocalStore.cpp`
- `Source/StealthPresentation/Private/Hud/HudPresenterCore.cpp` (step the model; add `FHudTallyView` to the view model and `Dump()`)
- `Source/StealthPresentation/Private/Tests/HUD/TallyDisplayTest.cpp` (class `FHudTallyDisplayTest`, name `StealthGame.HUD.TallyDisplay`)
- `Source/StealthPresentation/Private/Tests/HUD/TallyRepushGateTest.cpp` (class `FHudTallyRepushGateTest`, name `StealthGame.HUD.TallyRepushGate`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: `TallyDisplay`
  - Given: L=3, n=0..5, plus the L=1, L=5, n=255 and informant cases.
  - When: F6 is evaluated.
  - Then: filled = 0,1,2,3,3,3 and the triangle shows for n >= 2. The line is "Wrong targets: n" and never contains "/3" or the limit. L=1 shows a triangle at n=0. L=5 shows a triangle from n=4. n=255 gives L squares, a triangle and "Wrong targets: 255". Informant at n=3 gives 3 squares, no triangle, numeral frozen.
  - Edge cases: the triangle also requires `PresentedOwnState` = Active.
- **AC-2**: `TallyRepushGate`
  - Given: an owner re-push arrives before its plate, then with no plate pending.
  - When: displayed `n` is read.
  - Then: displayed `n` is unchanged while a plate is pending. The re-push is used directly only if no plate is pending and `LastResultSeq` >= the last plate's seq. It never lowers the displayed tally in a round.
  - Edge cases: `LastResultSeq` equal to the last seq.
- **AC-3**: Local store
  - Given: a rebuild, then a re-push, then a new `RoundEpoch`.
  - When: displayed `n` is read.
  - Then: `UStealthHudLocalStore` (keyed by `RoundEpoch`) bridges the rebuild. A passing re-push is never overridden. A new epoch shows 0.
  - Edge cases: the store is lost on client restart, so the re-push alone applies.

QL-STORY-READY: ADEQUATE

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthPresentation/Private/Tests/HUD/TallyDisplayTest.cpp` (`StealthGame.HUD.TallyDisplay`) and `Source/StealthPresentation/Private/Tests/HUD/TallyRepushGateTest.cpp` (`StealthGame.HUD.TallyRepushGate`) — must exist and pass. Gate level BLOCKING.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/hud/story-002-hud-view-model-and-presenter-core-shell.md`, `production/epics/hud/story-014-hud-presented-own-state-and-viewer-state-machine.md`; cross-epic: npc-kill-penalty-informants `production/epics/npc-kill-penalty-informants/story-003-tally-rule-and-strike-result-supply.md`; participant-state-roster `production/epics/participant-state-roster/story-041-team-view-repush-on-controller-swap.md` (the owner re-push)
- Unlocks: `production/epics/hud/story-021-hud-wrong-target-tally-widget.md`, `production/epics/hud/story-027-hud-tally-moves-with-plate-and-informant-parity-pie.md`, `production/epics/hud/story-044-hud-reset-and-rebuild-logic.md`, `production/epics/hud/story-045-hud-rebuild-no-replay-pie.md`
