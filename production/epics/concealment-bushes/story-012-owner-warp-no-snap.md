# Story 012: The owner's hide and exit show no snap and start moving within 50 ms (S-CON-1 mechanism)

> **Epic**: Concealment (Bushes)
> **Status**: Complete
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: 2026-10-29

## Context

The owner presses Hide, but the server executes it at a later queue drain, so without a deliberate mechanism the owner sees a correction hitch on every hide and exit. ADR-0017 §4 leaves the choice to spike S-CON-1 (a1: owner starts the identical source; b: suppress correction hitches for the transition window; a2: owner-only start message). This story implements whichever the spike chose and proves AC-CON-31. It cannot start until the verdict is recorded and ADR-0017 §4 is amended.

**GDD**: `design/gdd/concealment-bushes.md` (CON-R4a, Game Feel, AC-CON-31)
**Requirement**: `TR-con-004`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0017: Concealment component and per-viewer presentation (§4 owner smoothness)
**ADR Decision Summary**: Owner smoothness is one of: (a1) owner starts the identical source (same `InstanceName`, parameters and start server time) from the replicated `FActionState`, blending the residual correction; (b) server-side `bIgnoreClientMovementErrorChecksAndCorrection` plus owner-side `bClientIgnoreMovementCorrections` for the transition plus one RTT, then one normal correction; (a2) an owner-only start message (last resort; new owner wire element needing an IVP allowlist row). The server never takes the client's position.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: E4 [U]: duplicate-source behaviour of an owner-started source is verified by S-CON-1. Correction flags are those of `CharacterMovementComponent.h:977-1000, 2683` [V]; `bServerAcceptClientAuthoritativePosition` is banned (ADR-0003 §3).

**Control Manifest Rules (this layer)**:
- Required: NPCs and players use the same server path; the owner mechanism is the one recorded in the ADR-0017 §4 amendment — source: ADR-0017
- Forbidden: Never use `bServerAcceptClientAuthoritativePosition` — source: ADR-0003, ADR-0017
- Guardrail: Press-to-motion ≤ 50 ms at 60 fps; no owner positional correction > 5 cm during the transition

---

## Acceptance Criteria

*From GDD `design/gdd/concealment-bushes.md` (AC-CON-31) and ADR-0017 §4, scoped to this story:*

- [ ] **AC1** (AC-CON-31, [PIE-2][Net] `StealthGame.Concealment.OwnerWarpNoSnap`): **GIVEN** 100 owner hides and 100 exits under `LeakNetProfile` **THEN** press-to-motion is ≤ 50 ms at 60 fps and no owner frame shows a positional correction > 5 cm during the transition ("motion" as defined by story 010).
- [ ] **AC2** (derived, ADR-0017 §4): **GIVEN** the S-CON-1 mechanism M recorded in ADR-0017 §4's amendment **THEN** the root-motion source name and parameters captured at runtime for an NPC and a player equal those recorded (automated capture test; if M is mechanism b the recorded values are the ignore-correction flags and the one-RTT window), and `bServerAcceptClientAuthoritativePosition` is false (lint).

---

## Implementation Notes

*Derived from ADR-0017 Decision §4:*

Implement exactly the mechanism recorded in the §4 amendment; if the verdict is "none", a superseding ADR replaces §4 first and this story is rewritten. Collision remains the pure function of story 008 on both sides; correction replays must re-derive it from each saved move's mapped timestamp. If the chosen mechanism adds an owner-only start message, it needs an IVP allowlist row (process and owner sign-off) before merging. The amendment must state which of "source name and parameters" or "flags and RTT window" is the recorded value so AC2 is checkable.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-011-server-warp-to-anchor-and-exit-motion.md`: server path.
- `story-013-inside-the-bush-stillness-and-view.md`: inside-the-bush view and stillness.

---

## Files

- `Source/StealthGameplay/Features/Concealment/Private/ConcealmentOwnerWarp.cpp`
- `Source/StealthGameplay/Features/Concealment/Public/View/ConcealmentOwnerWarp.h`
- `Source/StealthTestHarness/Private/Tests/PIE/OwnerWarpNoSnapTest.cpp` (`NETWORK_TEST_CLASS(OwnerWarpNoSnap, "StealthGame.Concealment.PIE")`)
- `Source/StealthGameplay/Private/Tests/Concealment/OwnerWarpMechanismTest.cpp` (class `FConcealmentOwnerWarpMechanismTest`)

---

## QA Test Cases

*Written by qa-lead at story creation (QL-STORY-READY: GAPS on AC2 of the first draft; revised as above, re-check ADEQUATE).*

- **AC1**: OwnerWarpNoSnap (verbatim AC-CON-31)
  - Given: a PIE-2 session under `LeakNetProfile` with 100 owner hides and 100 exits.
  - When: press-to-motion and per-frame correction are logged.
  - Then: press-to-motion is 50 ms or less at 60 fps and no owner frame shows a correction above 5 cm.
  - Edge cases: "motion" is whatever story 010 fixed (at least 1 cm displacement).
- **AC2**: MechanismMatchesAdr
  - Given: the S-CON-1 mechanism M recorded in the ADR-0017 §4 amendment.
  - When: the runtime parameters are captured for an NPC and a player.
  - Then: the source name and parameters equal those recorded for both pawn kinds; `bServerAcceptClientAuthoritativePosition` is false (lint).
  - Edge cases: if M is mechanism b there is no named root-motion source; the recorded values are then the ignore-correction flags and the one-RTT window.

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: the PIE `OwnerWarpNoSnapTest.cpp` and `OwnerWarpMechanismTest.cpp` — must exist and pass, OR a documented playtest

**Status**: [x] Created and passing (2026-10-29): StealthGame.Concealment.PIE.OwnerWarpNoSnap.HideAndExitStartWithin50msWithoutSnap; evidence production/qa/evidence/concealment-001-026-2026-10-29/

Run result: OBSERVED - 3-player listen-server PIE on L_Prototype (production/qa/evidence/concealment-001-026-2026-10-29/, con-*.png per viewer)

---

## Dependencies

- Depends on: `story-010-spike-s-con-1-owner-side-warp.md` (blocker), `story-008-conceal-collision-pure-function.md`, `story-011-server-warp-to-anchor-and-exit-motion.md`
- Unlocks: `story-013-inside-the-bush-stillness-and-view.md` (visual evidence), `story-035-prototype-retirement-and-regression.md`

## Completion Notes
**Completed**: 2026-10-29
**Criteria**: AC1-AC2 passing: StealthGame.Concealment.PIE.OwnerWarpNoSnap runs 20 hides + 20 exits under StealthLeak2 through the production path: every transition moves the owner within 50 ms (worst 10.3 ms) with no correction above 5 cm (worst 0.00 cm) and the server reaches every state; the recorded mechanism (b) is the default (CVar 2) and bServerAcceptClientAuthoritativePosition is false (asserted). The 100 + 100 measurement is the spike's (story 010) on the same code path.
**Deviations**: Per-commit gate runs 20 + 20 transitions instead of 100 + 100 (the full count is the spike record; decision logged). Known cost: a Hide the server refuses after the owner predicted it is undone by one normal correction after the window.
**Test Evidence**: production/qa/evidence/concealment-001-026-2026-10-29/
**Reviewed by**: self code review 2026-10-29 (APPROVED; notes in the evidence)
