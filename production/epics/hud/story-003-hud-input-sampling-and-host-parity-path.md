# Story 003: UStealthHudPresenter sampling carriers every frame by value, identically for host and client

> **Epic**: HUD
> **Status**: Ready
> **Layer**: Presentation
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/hud.md`
**Requirement**: `TR-hud-001`, `TR-hud-004`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0021: UI/HUD layering, CommonUI and Enhanced Input §4 and §9 (with ADR-0001 carriers)
**ADR Decision Summary**: `UStealthHudPresenter` (Presentation, owned by the bootstrap) builds `FHudInputs` every frame from its handles: the own controller's C-TeamView parts, the owner reply sink, `FRoundPublicState` and the client tuning face (through `FStealthClientViewHandles`), Camera's `GetViewState()`, the queued `OnRoleChangePresented`, the own pawn's `FActionState`/`FDeathState` for the pawn handle captured at the press, own RTT from the local net connection, and local settings. Change detection is by value; no presentation depends on a RepNotify having fired, so the listen host and a remote client run the identical path.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: ADR-0001 and ADR-0021 §9 define the read set; the listen-server host gets no RepNotify for its own writes, which is why sampling replaces events (ADR-0021 Forbidden: never depend on a RepNotify having fired). Own RTT is read from the local net connection, never from the `PlayerState` ping byte (ADR-0001 removes that leak surface). `EstimatedServerNow()` comes from ADR-0002 §9 (owner-only ping/pong clock exchange; audited by IVP story 055). ADR-0018 networking domain is HIGH: `FActionState`/`FDeathState` are outcome-blind and written at the queue slot, and the HUD reads them only for the pawn handle captured at the press. `UE_LOGF` is preferred over `UE_LOG` in 5.8. ADR-0021 UI-V1..V4 do not apply here; this story adds no layer or projection.

**Control Manifest Rules (this layer)**:
- Required: Presentation builds view-models by sampling its carriers every frame by value (the listen host runs the same path) — source: ADR-0021
- Forbidden: Never depend on a RepNotify having fired; never drive widgets event-style from RepNotify/RPC handlers — the host gets no RepNotify for its own writes — source: ADR-0021
- Guardrail: HUD ≤ 0.3 ms CPU (AC-HUD-22), ≤ 0.5 ms with markers; UI GPU ≤ 0.5 ms at 1080p on the reference GPU — source: ADR-0021

This is the live input side of the presenter: it turns the engine's carriers into the plain `FHudInputs` struct that story 002's headless core consumes, and proves host and client build equal inputs from equal carriers.

---

## Acceptance Criteria

*From GDD `design/gdd/hud.md` HUD-R1 and HUD-R17 (late bind), ADR-0021 §4 and §9 (derived items marked), scoped to this story:*

- [ ] **AC1** (derived, HUD-R1 and ADR-0021 §4, `StealthGame.HUD.PIE.InputSamplingParity`): **GIVEN** a client and the listen-server host in the same round **WHEN** the presenter builds `FHudInputs` **THEN** it reads only the own controller's C-TeamView parts (roster, objectives, Penalty owner part), the owner reply sink, `FRoundPublicState` and the client tuning face through `FStealthClientViewHandles`, Camera `GetViewState()`, `OnRoleChangePresented` (queued), the own pawn's `FActionState`/`FDeathState` for the pawn handle captured at the press, own RTT from the local net connection (never the `PlayerState` ping byte) and local settings.
- [ ] **AC2** (derived, HUD-R1 and AC-HUD-20 path, `StealthGame.HUD.PIE.InputSamplingParity`): **GIVEN** a carrier written without a RepNotify firing (the host's own writes) **WHEN** the next frame samples **THEN** the HUD input still changes on the next frame (sampling, not events), and the host and client view-model inputs are equal for equal carriers.
- [ ] **AC3** (derived, ADR-0021 §4 time and F2 cap, `StealthGame.HUD.PIE.InputSamplingParity`): **GIVEN** `IsServerClockSynced()` is false, and separately a missing handle (late bind) **WHEN** a result arrives and `Step` runs **THEN** persistent clusters render and timed plates are deferred per F2's cap, and a missing handle collapses the HUD without error and binds on the next frame the handle exists.

---

## Implementation Notes

*Derived from ADR-0021 §4 "Sampling, not events", §9 and ADR-0007 (client-side wiring lives in `StealthPresentation`):*

`UStealthHudPresenter` is created by `UStealthPresentationBootstrap` (owned by `AStealthHUD`; the bootstrap and `AStealthHUD` shell are created by story 006, so this story ships the presenter and its test double and is wired by 006). `Bind(const FStealthClientViewHandles&)` stores weak handles; sampling runs once per frame from a per-frame tick (for example a `FTickableGameObject` owned by the bootstrap; never a RepNotify or RPC handler) builds one `FHudInputs` per frame and calls `FHudPresenterCore::Step` (story 002) with `ServerNowEst = EstimatedServerNow(World)` and `LocalNow`. Sampling reads exactly:

1. the own controller's C-TeamView parts found with `FindComponentByClass` on the local controller: `URosterTeamViewComponent`, `UObjectiveTeamViewComponent`, Penalty's owner part (`production/epics/participant-state-roster/story-042-team-view-pawn-ref-ordering.md` fixes the roster ordering of `PawnRef`);
2. the owner reply sink (`ClientCommandReplies` entries consumed in order; `production/epics/input-interaction-framework/story-004-reply-sink-and-drain-end-flush.md`), including Assassination's owner results (`production/epics/assassination-defence-reversal/story-015-owner-results-reply-entries-and-tally.md`);
3. `FRoundPublicState` (`production/epics/round-match-framework/story-029-public-state-replication.md`) and the client tuning face through `FStealthClientViewHandles`;
4. Camera's `GetViewState()` (`production/epics/camera-system/story-017-view-state-neutral-view-fade.md`);
5. Assassination's client-local `OnRoleChangePresented`, queued;
6. the own pawn's `FActionState` / `FDeathState` for the pawn handle captured when the owner's command was sent (F2) and held in the presenter, so after an exile the HUD still reads the old pawn;
7. own RTT from the local net connection (host RTT is 0) and local settings.

Change detection is by value: `FHudInputs` is rebuilt every frame and compared by field; widgets never see a RepNotify. The host path is the same code as the client path (HUD-R1, AC-HUD-20): there is no `HasAuthority()` branch in the presenter.

`FStealthClientViewHandles` (ADR-0007 amendment) has no producing story yet in other epics. If it is absent when this story starts, declare it as ADR-0007 specifies in `Source/StealthGameplay/Public/View/Core/StealthClientViewHandles.h` and register the producer gap with the IIF epic; the test uses fake handles because no producer exists. Never reach a handle by world or component search outside the local controller (ADR-0007 Forbidden).

Timing rule: until `IsServerClockSynced()` the presenter feeds `ServerNowEst` as not-synced; persistent clusters render, and the core defers timed plates to `t_arrival + HudResultPlateMaxHoldBackSec` (F2 cap; the F2 logic itself is story 015). A missing handle makes `Step` produce a collapsed view-model with no error and no log; the next frame the handle exists, it binds.

Prototype disposition: replaces the prototype's pull from `AStealthCharacter` / `AStealthPlayerState` team fields in `Source/StealthGame/Private/StealthHUD.cpp`. That code reads a team field replicated on every pawn; it is never promoted.

Traps: two carrier writes in one frame, and a write landing in the same frame as sampling, must both resolve to the last value; the pawn handle captured at the press must not be re-resolved from the current pawn; the `PlayerState` ping byte must never be read. Decisions D20 (world-anchored elements are outside the centre-empty rule) and D26 do not change this story.

Out of this story: AC-HUD-20 full parity over a whole round (story 046), any state machine, any widget, `UStealthHudLocalStore`.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/hud/story-002-hud-view-model-and-presenter-core-shell.md`: `FHudInputs`, `FHudPresenterCore` and `FHudViewModel` themselves (this story depends on them).
- `production/epics/hud/story-006-hud-layer-stack-root-and-input-isolation.md`: `AStealthHUD`, the bootstrap and the layer widgets that consume the view-model.
- `production/epics/hud/story-046-hud-host-parity-pie.md`: AC-HUD-20 whole-round parity.
- `production/epics/hud/story-015-hud-result-show-time-f2.md`: the F2 match, show-time and drop rules that use the cap.
- `production/epics/hud/story-014-hud-presented-own-state-and-viewer-state-machine.md`: `PresentedOwnState` and the viewer state table.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthPresentation/Private/Hud/StealthHudPresenter.h`
- `Source/StealthPresentation/Private/Hud/StealthHudPresenter.cpp`
- `Source/StealthPresentation/Private/Hud/HudInputsBuilder.h` (the by-value carrier read, one function per carrier)
- `Source/StealthPresentation/Private/Hud/HudInputsBuilder.cpp`
- `Source/StealthGameplay/Public/View/Core/StealthClientViewHandles.h` (only if no producing story has created it; ADR-0007 amendment)
- `Source/StealthPresentation/Private/Hud/HudInputs.h` (edit: the fields story 002 documented)
- `Source/StealthTestHarness/Private/HUD/InputSamplingParityTest.cpp` (CQTest, `NETWORK_TEST_CLASS(InputSamplingParity, "StealthGame.HUD.PIE")`, full name `StealthGame.HUD.PIE.InputSamplingParity.*`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: carrier read set
  - Given: a client and the listen host in one round, with a carrier-access spy and an RTT-source spy.
  - When: the presenter builds `FHudInputs`.
  - Then: the spy read set equals the allowed list in the AC (own C-TeamView parts, owner reply sink, `FRoundPublicState`, client tuning face, Camera `GetViewState`, queued `OnRoleChangePresented`, own `FActionState`/`FDeathState` for the press-captured pawn handle, local-connection RTT, local settings). There is no `PlayerState` ping read.
  - Edge cases: the pawn handle is captured at the press, so after an exile it still reads the old pawn. Host RTT is 0. `FStealthClientViewHandles` has no producer story, so use fake handles.
- **AC-2**: sampling, not events
  - Given: the host writes a carrier without a RepNotify.
  - When: the next frame samples.
  - Then: `FHudInputs` changes that frame, and host and client `FHudInputs` are field-equal for equal carriers.
  - Edge cases: a write landing in the same frame as sampling. Two writes in one frame.
- **AC-3**: unsynced clock and late bind
  - Given: `IsServerClockSynced()` is false, and separately a missing handle.
  - When: a result arrives and `Step` runs.
  - Then: persistent clusters render. The plate shows no later than `t_arrival + HudResultPlateMaxHoldBackSec`. A missing handle collapses the HUD with no error or log, and it binds on the next frame the handle exists.
  - Edge cases: the clock syncs while a plate is pending.

QL-STORY-READY: ADEQUATE

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/HUD/InputSamplingParityTest.cpp` (`StealthGame.HUD.PIE.InputSamplingParity`, CQTest PIE) OR a documented playtest
- Gate: BLOCKING (Integration default).

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/hud/story-002-hud-view-model-and-presenter-core-shell.md`; `production/epics/participant-state-roster/story-042-team-view-pawn-ref-ordering.md`; `production/epics/round-match-framework/story-029-public-state-replication.md`; `production/epics/input-interaction-framework/story-004-reply-sink-and-drain-end-flush.md`; `production/epics/assassination-defence-reversal/story-015-owner-results-reply-entries-and-tally.md`; `production/epics/camera-system/story-017-view-state-neutral-view-fade.md`; ADR-0002 `EstimatedServerNow()` (audited by `production/epics/information-visibility-policy/story-055-clock-exchange-owner-only-audit.md`); the PIE harness of `production/epics/information-visibility-policy/story-001-spike-s1-multiprocess-raw-bunch-capture.md`
- Unlocks: `production/epics/hud/story-006-hud-layer-stack-root-and-input-isolation.md`, `production/epics/hud/story-022-hud-result-and-defence-plate-flow.md`, `production/epics/hud/story-036-hud-team-feed-opponent-and-round-end-pie.md`, `production/epics/hud/story-038-hud-reconnect-lines-pie.md`, `production/epics/hud/story-045-hud-rebuild-no-replay-pie.md`, `production/epics/hud/story-046-hud-host-parity-pie.md`, `production/epics/hud/story-047-hud-latency-icon.md`, `production/epics/hud/story-050-hud-ping-refusal-owner-only-pie.md`, `production/epics/hud/story-059-hud-retire-prototype-canvas-and-message-plumbing.md`
