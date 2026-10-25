# Story 056: AC 43 AssassinationCooldownNoReset

> **Epic**: Input & Interaction Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/input-interaction-framework.md`
**Requirement**: `TR-iif-006`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

This story is the cross-system guard that nothing the IIF does can reset or move the brace cooldown that Assassination stamped at execution. With a defender cooling (cooldown end `T_c`, stamp count 1), re-pressing Prepare (a `Cooldown` reply), beginning and completing an Interact, and toggling the camera (client-local, no server effect) each leave `cooldown_end` equal to `T_c` and the stamp count at 1. A Prepare mash during the cooldown draws no token and the next Begin is admitted exactly at `T_c`, not before.

It depends on story 043 (Prepare mash cannot reset anything), the in-slot half of the same guarantee. The IIF's part is that none of its paths stamps, extends or clears the cooldown: the cooldown is Assassination's data, stamped by its hub subscriber on `OnActionStarted(Prepare)`, and the owner's copy lives on the PlayerController carrier (ADR-0011 section 7, never on the pawn). GDD rules: R8 (Prepare: mashing can neither reset nor extend anything), R14 (camera toggle is local) and R6 (rejections change no cooldown).

**ADR Governing Implementation**: ADR-0011: Pawn command interface and input pipeline
**ADR Decision Summary**: Prepare's cooldown is Assassination's, stamped at execution; a rejected command produces no event, no replicated change and no cooldown change, and the owner's cooldown end lives with the owner-only data on the PlayerController.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Command path is MEDIUM in ADR-0011's Engine Compatibility; input-layer HIGH does not apply. No V1-V6 gate. The camera toggle is bound in `IMC_Common` and handled client-locally by Camera (never a command, never reaches `ServerPawnCommand`), so the test asserts zero RPCs rather than driving Enhanced Input. U-form automation test with stubs.

**Control Manifest Rules (this layer)**:
- Required: Owner-only data (`FHoldStatus`, rejection codes, owner results, cooldown end) lives on the PlayerController carrier family — ADR-0011
- Required: Ingress drops are silent; stage 2 runs only at the command's queue slot; the per-pawn token bucket is consumed only on execution — ADR-0011
- Required: A resolution-voided command produces exactly one reply, flushed at the end of the drain's resolution step — ADR-0011
- Forbidden: Never put owner-only data on the pawn — ADR-0011
- Forbidden: Never let a reply code depend on a target pawn's identity, team, concealment or brace state — ADR-0011
- Forbidden: Never carry caller identity in an entry point, payload, event or listener — ADR-0011
- Guardrail: Command interface: uplink at most 20 reliable RPCs/s per connection (transport bucket) — ADR-0011

---

## Acceptance Criteria

*From GDD `design/gdd/input-interaction-framework.md`, scoped to this story:*

- [ ] AC 43 (Integration, U `AssassinationCooldownNoReset`): **GIVEN** a cooling defender **WHEN** Prepare is re-pressed, Interact is used or the camera is toggled **THEN** the cooldown end time is unchanged.

---

## Implementation Notes

*Derived from ADR-0011 Decision sections 5, 7 and 8 and GDD R8, R14:*

- **What to assert.** After each stimulus, in turn: re-pressed Prepare (one `Cooldown` reply per Begin), an Interact begun and completed, a camera toggle press (zero RPCs and zero server stimulus, AC 29): the Assassination stub's `cooldown_end` equals `T_c` and its stamp count is 1. The IIF has no cooldown field to disturb; the test guards against a future IIF path that would stamp or extend.
- **Mash draws no token.** A Prepare Begin refused at stage 2 step 4 (2d, Assassination's `Cooldown` predicate) consumes no token (step 5 is reached only when 0-4 pass), so the first legal Begin after `T_c` is admitted; the next Begin is accepted at exactly `T_c`, not before.
- **No client trust.** The client does not send `PrepareBegin` while its owner-known cooldown runs (a traffic saving only); the server still validates every Begin it receives.
- **Test seam.** An Assassination stub with `cooldown_end = T_c` and a stamp counter subscribed to `OnActionStarted(Prepare)`; an RPC-send spy for the camera-toggle assertion (the outgoing-RPC spy is a story 095 seam). Named doubles from story 095.
- **Tuning keys.** `DefenceCooldownSec` 60 (Assassination-owned), `BucketPrepareSize` 2, `CameraToggleLockoutSec` 0.30.
- **Prototype.** Replaces the prototype's cooldown kept on the PlayerState with `OnDefencePressed/Released` / `ServerSetDefence` (ADR-0011 migration: replaced, not ported).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 043 (`story-043-prepare-mash-no-reset.md`): the active-brace and cooling Prepare mash cases in detail.
- Story 042 (`story-042-prepare-cooldown-stamped.md`): when the stamp happens.
- Story 014 (`story-014-camera-toggle-local.md`): the camera toggle lockout and zero-RPC assertion at the input layer.
- Assassination epic (cross-epic): the real cooldown stamp, predicate and owner data.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- Source/StealthGameplay/Private/Core/PawnCommandCore.cpp (no cooldown writes; guard only)
- Source/StealthGameplay/Private/Tests/Input/AssassinationCooldownNoResetTest.cpp (`FInputAssassinationCooldownNoResetTest`, `StealthGame.Input.AssassinationCooldownNoReset`; U-form automation test)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation. (At tiers where the QL-STORY-READY gate is skipped — `minimal`, or `lean`/`solo` review mode — no qa-lead specs exist, and this section reads: N/A — no qa-lead specs at this tier; implement against the Acceptance Criteria above.)*

**Logic / Integration story — automated test spec (`StealthGame.Input.AssassinationCooldownNoReset`):**

- **AC-1**: Nothing resets the cooldown
  - Given: defender cooling, Assassination stub `cooldown_end = T_c`, stamp count = 1.
  - When: Prepare re-pressed (`Cooldown` reply), Interact begun and completed, camera toggle pressed (client-local, no server effect).
  - Then: `cooldown_end = T_c` and stamp count = 1 after each.
  - Edge cases: camera toggle has no server stimulus, assert zero RPC (AC 29); mash during cooldown draws no token; ends exactly at `T_c` (not before) for the next Begin.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthGameplay/Private/Tests/Input/AssassinationCooldownNoResetTest.cpp` (U-form automation test `StealthGame.Input.AssassinationCooldownNoReset`) OR a playtest doc

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/input-interaction-framework/story-043-prepare-mash-no-reset.md`; cross-epic: assassination (stories not yet written) for the real cooldown stamp and predicate; cross-epic: round-match-framework (stories not yet written) for the resolution queue
- Unlocks: None
