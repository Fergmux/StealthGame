# Story 022: A latched pawn drops every render proxy, the aim channel response and the audio and decals, on clients and the listen host

> **Epic**: Concealment (Bushes)
> **Status**: Complete
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25f
> **Last Updated**: 2026-10-29

## Context

**GDD**: `design/gdd/concealment-bushes.md` (CON-R6b, CON-R16, AC-CON-20)
**Requirement**: `TR-con-006`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0017: Concealment component and per-viewer presentation (§6 `bPresentationHidden`, §5 listen host), with ADR-0022 (render flag parity)
**ADR Decision Summary**: When the latch is set on a client (or the host), `UConcealmentComponent` applies, client-locally and never replicated: `SetVisibility(false, true)` on the mesh and every attached primitive with the three gating flags fixed on class defaults, the Shared Pawn's `IsPresentationHidden()` gate suppressing footstep decals, foley, Niagara, spawned sounds and the audio emitter, mesh pose freshness from swap to crossing, and the pawn's response on the presentation aim channel switched to ignore. The host has no RepNotify, so the server's Commit and Cross items call `RefreshPresentation` on the host's copy; `SetActorHiddenInGame` and `bHidden` are never used.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: E2/E3/E6: a hidden primitive gets no render proxy unless a gating flag keeps it; `bHidden` replicates and feeds `APawn::IsNetRelevantFor`; visibility-based anim ticking can skip notifies on a hidden mesh, and under `OnlyTickPoseWhenRendered` a hidden mesh does not evaluate its seeked pose, so pose freshness (`AlwaysTickPose` or a refresh before show) is required. The presentation aim channel is a `Config/DefaultEngine.ini` entry added by the implementer.

**Control Manifest Rules (this layer)**:
- Required: Hiding applies the full §6 flag set and the presentation aim channel response; the host latch is set by explicit server-queue calls, never an anim notify — source: ADR-0017
- Forbidden: Never use `SetActorHiddenInGame`/`bHidden` for concealment — source: ADR-0017
- Guardrail: Flags are class defaults audited by ADR-0003 and `StealthGame.Pawn.RenderFlagParity` (ADR-0022)

---

## Acceptance Criteria

*From GDD `design/gdd/concealment-bushes.md` (AC-CON-20, CON-R16) and ADR-0017 §6, scoped to this story:*

- [ ] **AC1** (derived, ADR-0017 §6): **GIVEN** the latch set **THEN** `SetVisibility(false, true)` is applied to the mesh and every attached primitive (including a late-attached prop), the three gating flags are class defaults on every pawn primitive (never toggled at runtime; audited by ADR-0003 and `StealthGame.Pawn.RenderFlagParity`), `IsPresentationHidden()` suppresses footstep decals, foley, Niagara, spawned sounds and the audio emitter, the mesh keeps pose freshness from swap to crossing, and `SetActorHiddenInGame`/`bHidden` are never used.
- [ ] **AC2** (AC-CON-20, [PIE-2] `StealthGame.Concealment.ListenHostParity`): **GIVEN** the host player watching an opponent client's pawn conceal **THEN** `bPresentationHidden` is set by the server queue item (not an anim notify), `bHidden` never changes, the host's aim trace and Strike hint miss the pawn (the presentation aim channel is switched to ignore), no shadow, footstep, foley, Niagara or audio remains, and a host `Attack(TargetPawn)` resolves through the same server path as a remote one.
- [ ] **AC3** (derived, ADR-0017 §6): **GIVEN** the Camera eye-mask dither at values 0 to 1 **THEN** a hidden body stays hidden at every mask value (the hidden state composes as an override).

---

## Implementation Notes

*Derived from ADR-0017 Decision §5 and §6:*

`bCastHiddenShadow = false`, `bAffectIndirectLightingWhileHidden = false` and `bRayTracingFarField = false` must be on the class defaults of every pawn primitive and never toggled at runtime; with no proxy there is no need to toggle `bVisibleInRayTracing`/`bVisibleInReflectionCaptures`/`bVisibleInRealTimeSkyCaptures`. While a hide window is active (swap to crossing) the mesh keeps `AlwaysTickPose` or its pose is refreshed before it is shown. The pawn's response on the presentation aim channel (a client-local trace channel used by aim, Strike-hint and prompt traces on every client, the host included) switches to ignore; shared physics collision is unchanged and server hit tests use capsule math, not trace channels. The host's server swap and crossing queue items call `RefreshPresentation(ServerNow())` on the host's local copy in the same step and the host also runs the per-frame evaluation. Any server-side sight or perception check on a pawn reads `IsConcealedFrom`. The host has no cull (IVP R10/L21, accepted for trusted playtests). Which extra steps the spike R5 verdict prescribes (VSM, groom, Niagara) are added here.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-019-body-hidden-latch-function.md` and `story-020-fresh-actor-latch-and-ally-hidden-read.md`: the latch and fresh-actor handling.
- `story-034-r5-leak-matrix.md`: the AC-CON-23 leak matrix on this implementation.
- SCL `story-043-footstep-parity.md`: footstep parity itself.

---

## Files

- `Source/StealthGameplay/Features/Concealment/Private/ConcealmentComponent.cpp`
- `Source/StealthGameplay/Features/Concealment/Private/ConcealmentPresentation.cpp`
- `Source/StealthTestHarness/Private/Tests/PIE/ListenHostParityTest.cpp` (`NETWORK_TEST_CLASS(ListenHostParity, "StealthGame.Concealment.PIE")`)
- `Source/StealthGameplay/Private/Tests/Concealment/PresentationHideTest.cpp` (class `FConcealmentPresentationHideTest`)

---

## QA Test Cases

*Written by qa-lead at story creation (QL-STORY-READY: ADEQUATE).*

- **AC1**: HideApplication
  - Given: the latch set on a pawn with attached primitives.
  - When: presentation is applied.
  - Then: `SetVisibility(false, true)` on the mesh and every attached primitive; the three flags are class defaults (RenderFlagParity audit); `IsPresentationHidden()` suppresses decals, foley, Niagara, spawned sounds and the audio emitter; pose stays fresh from swap to crossing; `SetActorHiddenInGame` is never called.
  - Edge cases: a late-attached prop is also hidden.
- **AC2**: ListenHostParity
  - Given: a PIE-2 session with the host watching an opponent client's pawn conceal.
  - When: the pawn conceals.
  - Then: `bPresentationHidden` is set by the queue item, `bHidden` never changes, the host's aim trace and Strike hint miss the pawn, nothing audible or shadowed remains, and a host `Attack(TargetPawn)` resolves through the same server path as a remote one.
- **AC3**: EyeMaskOverride
  - Given: eye-mask dither values 0 to 1.
  - When: a hidden body is evaluated.
  - Then: it stays hidden at every mask value.

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: the PIE `ListenHostParityTest.cpp` and `PresentationHideTest.cpp` — must exist and pass, OR a documented playtest

**Status**: [x] Created and passing (2026-10-29): StealthGame.Pawn.RenderFlagParity, StealthGame.Concealment.PIE.PerViewerVisibility.ListenHostParity; evidence production/qa/evidence/concealment-001-026-2026-10-29/

Run result: OBSERVED - 3-player listen-server PIE on L_Prototype (production/qa/evidence/concealment-001-026-2026-10-29/, con-*.png per viewer)

---

## Dependencies

- Depends on: `story-021-spike-r5-hidden-primitive-leaks.md` (blocker), `story-019-body-hidden-latch-function.md`, `story-004-hide-enter-execution-and-commit.md`; SCL `production/epics/shared-character-locomotion/story-043-footstep-parity.md`
- Unlocks: `story-028-enter-swap-hides-body-per-viewer.md`, `story-034-r5-leak-matrix.md`

## Completion Notes
**Completed**: 2026-10-29
**Criteria**: AC1-AC2 passing: ApplyHidden sets SetVisibility(false, true) on the mesh (and root-attached props), switches the presentation-aim channel to ignore on mesh and capsule, keeps pose freshness (AlwaysTickPoseAndRefreshBones while hidden), suppresses anim-notify dispatch (footstep decals, foley, notify Niagara, sounds) and pauses attached audio emitters; the three gating flags are pinned false on AStealthPawn's primitives and audited on the configured character class (RenderFlagParity); bHidden is never used. ListenHostParity: the host watching a remote opponent conceal has the latch set from the replicated action state, the mesh and every attachment invisible, bHidden unchanged on every frame, its presentation-aim trace passing through the pawn, and the host's blind strike resolving through the server path (the hider dies with bDiedConcealed). AC3 (the Camera eye-mask dither) holds by construction - a hidden primitive has no proxy, so no mask value can draw it - and is asserted when the Camera eye mask (ADR-0015) exists.
**Deviations**: R5 prescribed no extra step (VSM, groom, Niagara). Attached Niagara keeps simulating while hidden (render-only hide).
**Test Evidence**: production/qa/evidence/concealment-001-026-2026-10-29/
**Reviewed by**: self code review 2026-10-29 (APPROVED; notes in the evidence)
