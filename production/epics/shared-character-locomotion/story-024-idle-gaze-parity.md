# Story 024: Idle gaze layer: four idle pawns are indistinguishable (decision D14)

> **Epic**: Shared Character & Locomotion
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: [set by /dev-story when implementation begins]

## Context

**GDD**: `design/gdd/shared-character-locomotion.md` (R8.9 Idle gaze layer, decision D14 2026-10-25 resolving pause-menu PM-1; AC-SCL-49; accepted trade-offs D27)
**Requirement**: `TR-scl-015` (idle gaze layer), `TR-scl-005`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0006: Seeds and deterministic/secret random streams (domain 16 `IdleGaze`, tag `SCL_GAZE`), with ADR-0001 (one look path, no AI hand-over for an idle or menu-open player) and ADR-0016 (Crowd G1 scan is this layer)
**ADR Decision Summary**: On the server any pawn whose control rotation has not changed for more than `IdleGazeDelaySec` (3 s) gets the idle head-scan layer: sweeps of U(30, 60)° yaw every U(3, 7) s, eased at the F6 limit, drawn from a server-only counter-based stream keyed by the pawn's stable ordinal `(PawnOrdinal, SweepIndex)`, never by kind, binding or controller. It changes only the replicated `HeadLook` target, never the control rotation. There is no AI hand-over for an idle or menu-open player.
**ADR Version**: 2026-10-24 (ADR-0006; the epic's other governing ADRs carry the same date)

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Pure game logic on the server tick; the engine facts that matter are those of the look path (story 021). Note for planning: `requirements-traceability.md` marks TR-scl-015 **Partial** (stream owned by ADR-0006, layer rules sit in the HeadLook filter; see TR-scl-005). All governing ADRs are Accepted, so the story is Ready, but flag the layer-rules ownership gap to `/architecture-review` and do not invent behaviour beyond GDD R8.9.

**Control Manifest Rules (this layer)**:
- Required: one control-rotation path for every pawn; draws come from a secret server-only stream (ADR-0006), keyed by stable ordinal.
- Forbidden: never key the layer by kind, binding, controller or team; never log, replicate or reflect the stream state; no AI controller possesses a menu-open or idle player; never change control rotation from the layer.
- Guardrail: negligible per pawn per tick.

---

## Acceptance Criteria

*From GDD `design/gdd/shared-character-locomotion.md`, scoped to this story:*

- [ ] **AC-SCL-49** [Integration/BLOCKING] `IdleGazeParity` (R8.9, D14; makes pause-menu `PauseMenuNoTell` testable): GIVEN four pawns standing still with no look change, (a) a player with the Escape menu open, (b) a player idle with no menu and (c) an NPC with no Crowd `Look` command and (d) an NPC whose Crowd layer re-issues an identical `Look` every tick, at a 1/60 s server step with injected stream seeds, THEN for each, `HeadLook` is constant until `IdleGazeDelaySec` (3 s) ± 1 frame after the last control-rotation change and then begins scan sweeps of 30-60° yaw at 3-7 s intervals within `LookYawLimitDeg` and `LookMaxRateDegPerSec`; the replicated property set and update cadence of the three pawns are identical (same dirty properties, same send ticks); the pawn in (a) receives zero `Move`, zero `Look` and no held action while the menu is open; the control rotation of (a) and (b) never changes during the scan; no AI controller possesses (a) or (b); and the first `Look` input ends the scan in the same server frame. A 2 s observer-side classifier of scan statistics separates the four cases no better than chance over ≥ 200 trials (two-sided binomial p ≥ 0.05).

---

## Implementation Notes

*Derived from GDD R8.9 and ADR-0006 domain 16:*

- Add the layer to the F6 filter's input stage: track the time since control rotation last changed (one trigger for every kind: an identical re-issued `Look` counts as unchanged); after `IdleGazeDelaySec` offset the head target by successive sweeps drawn from `FSecretStream` with the `SCL_GAZE` tag and counter tuple `(PawnOrdinal, SweepIndex)` (counter-based, so one pawn's draws are independent of others). Yaw clamps to `LookYawLimitDeg`; pitch unchanged; eased at the F6 limit. The first `Look` change ends the layer in the same server frame and the head eases back through F6.
- The deadzone refinement (decision D31) is story 025; build this story with "unchanged" as exact equality first, and keep the comparison in one function story 025 changes.
- The pause-menu half: the pawn of a menu-open player receives zero `Move`, zero `Look` and every hold ended (pause-menu PM-R2, IIF); this story asserts it, the IIF epic owns the cause. There is no hand-over to AI for (a) or (b); assert no `AStealthCrowdController` possesses them.
- The classifier test uses injected stream seeds and the harness's observer capture (≥ 200 trials); it is a BLOCKING-gate test (`testing.strict.integration`).
- Crowd's G1 gaze scan is this layer with the same parameters (Crowd CNB-R6): Crowd must not re-implement it.
- **Prototype replaced**: none (no idle gaze existed); the prototype AI hand-over for idle players is not carried forward.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 025: the look-input deadzone (D31).
- Story 021: the F6 filter and `HeadLook`.
- The IIF's menu-open zeroing and Crowd's gaze catalogue (their epics).
- The T13 head-hold telemetry (Crowd/telemetry epics).

---

## Files

- `Source/StealthGameplay/Private/Core/SharedPawn/HeadLookFilter.cpp` (idle layer)
- `Source/StealthGameplay/Private/Core/SharedPawn/IdleGazeLayer.cpp` (planned)
- `Source/StealthTestHarness/Private/Locomotion/IdleGazeParityTest.cpp` (planned)

---

## QA Test Cases

*QL-STORY-READY: GAPS accepted — Test cases not yet defined — run /qa-plan to generate them.*

Open QA gaps flagged by the QA lead (not specs; to be resolved by the AC author before `/story-readiness`):
- The "2 s observer-side classifier of scan statistics" has no defined features, training data or training/test split, so a weak classifier would pass trivially.
- The null probability is unstated (chance for four classes is 0.25, but the AC cites a two-sided binomial test).
- The AC says "the three pawns" but lists four cases (a)-(d).
- Case (a) (Escape menu open) depends on the pause-menu epic.
- Suggested resolution: fix the classifier and its split, state the null probability, reconcile three versus four, and add a negative control (a deliberately biased case must be detected by the classifier).

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration (BLOCKING): `Source/StealthTestHarness/Private/Locomotion/IdleGazeParityTest.cpp` — must exist and pass; retained screenshot of an idle scan in `production/qa/evidence/` (player-observable)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/shared-character-locomotion/story-021-look-envelope-700-deg-spin.md`, `story-003-tuning-keys-registered.md`; `production/epics/input-interaction-framework/EPIC.md` (menu-open zeroing, PM-R2); ADR-0006 stream facade in the Foundation epic (`production/epics/tuning-data-config/EPIC.md` Exit A or the Foundation random-streams work)
- Unlocks: `production/epics/shared-character-locomotion/story-025-idle-gaze-look-deadzone.md`, `production/epics/crowd-npc-behaviour/EPIC.md` (G1 scan)
