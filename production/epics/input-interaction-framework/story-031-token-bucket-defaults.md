# Story 031: AC 34 TokenBucketDefaults

> **Epic**: Input & Interaction Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/input-interaction-framework.md`
**Requirement**: `TR-iif-004`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

This story verifies the per-pawn token bucket of stage 2 step 5 against the F3 formula and its defaults (GDD F3, ADR-0011 section 3.2). The bucket is consumed only when stage 2 steps 0 to 4 pass and the command actually executes, so mashing a key during a cooldown never starves the first legal press: Begins rejected at step 2d consume no token. The AC's worked example is the Interact bucket B3 R2: sixteen admissible Begins arriving every 0.125 s are admitted at exactly 0, 0.125, 0.25, 0.5, 1.0 and 1.5 s (six in total), with the boundary value of exactly 1.0 token admitted at t = 0.5.

It follows story 027 (stage 2 resolver) and is the base of story 032 (AI gets no exemption), story 086 (command cost per 50 pawns) and story 089 (knob registry conformance). The bucket primitive itself is shared with story 022's transport bucket.

**ADR Governing Implementation**: ADR-0011: Pawn command interface and input pipeline
**ADR Decision Summary**: Seven server-validated entry points with no caller identity; stage 2 ends with a per-pawn, per-type token bucket consumed only on execution; End is never rate-limited.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0011: HIGH for the input layer, MEDIUM for the command path)
**Engine Notes**: No V1-V6 item gates this story. Times are doubles (server world time, `UWorld::TimeSeconds`, ADR-0002 E6); use dt = 1/64 s so a 0.125 s multiple lands exactly on a tick (60 Hz ticks cannot hit 0.125 s).

**Control Manifest Rules (this layer)**:
- Required: Ingress drops are silent; stage 2 runs only at the command's queue slot and in the stated order; the per-pawn token bucket is consumed only on execution — source: ADR-0011
- Required: Hold ends are `ScheduleAt` queue items; the release debounce is server-side and identical for every pawn — source: ADR-0011
- Forbidden: Never rate-limit `End` per pawn or drop it at the transport bucket when it matches an outstanding Begin — source: ADR-0011
- Forbidden: Never time a hold on the IIF's own tick, `FTimerManager` or a client timestamp; never debounce releases client-side — source: ADR-0011
- Guardrail: Command interface: stage 2 a few microseconds per frame at the IIF F3 worst case (about 5 items/frame), inside the 0.5 ms queue line — source: ADR-0011

---

## Acceptance Criteria

*From GDD `design/gdd/input-interaction-framework.md`, scoped to this story:*

- [ ] AC 34 (F3, Logic, U `TokenBucketDefaults`): **GIVEN** a stub `Allow` predicate and an Interact bucket of B3 R2 **WHEN** 16 admissible Begins arrive at t = k·0.125 s **THEN** exactly 6 are admitted, at t = 0, 0.125, 0.25, 0.5, 1.0 and 1.5 s; **AND** Begins rejected at stage 2d consume no token.

---

## Implementation Notes

*Derived from ADR-0011 Decision section 3.2 step 5 and Implementation Guidelines:*

Stage 2 step 5: per-pawn, per-type token bucket (`Bucket*Size`/`PerSec`), consumed only when steps 0-4 pass and the command executes; `End` is never rate-limited, and an `End` with no matching active action is a no-op.

**F3 (GDD).** `tokens(t) = min(B, tokens(t_last) + R * (t - t_last))`; admitted if `tokens >= 1` (the boundary value 1.0 is admitted), then `tokens -= 1`; initial tokens = B. Per-pawn defaults: Interact B3 R2; Prepare B2 R1; Attack B2 R1; HideEnter/HideExit B4 R2 as one shared bucket (four buckets, sum of B = 11, sum of R = 6/s). At t = 0.5 the Interact bucket holds exactly 1.0 and that Begin is admitted. Refill is capped at B after idle.

**Rationale.** Consuming only on execution is what keeps a cooldown mash from starving the first legal press (GDD F3 example); making the buckets per pawn and applying them to AI as well as humans (R17) removes any rate-limit tell between kinds.

**Tuning keys (defaults, read from tuning data; test also at B4 R2 and B3 R1).** `BucketInteractSize` 3 (1-10), `BucketInteractPerSec` 2.0 (0.5-10), `BucketPrepareSize` 2, `BucketPreparePerSec` 1.0, `BucketAttackSize` 2, `BucketAttackPerSec` 1.0, `BucketHideSize` 4, `BucketHidePerSec` 2.0.

**Test precondition (QL-STORY-READY).** "Each preceded by an End that has already executed": the AC example sends each End the same tick as the prior Begin so it executes after the 0.10 s debounce (`ReleaseDebounceSec`) and before the next Begin. Otherwise the debounce merges them (AC 19) and no token is drawn. A predicate returning `Reject(Cooldown)` is needed for the "no token on 2d rejection" clause; the Given in the AC mentions only `Allow`.

**Prototype superseded.** New code, no prototype counterpart.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/input-interaction-framework/story-022-transport-bucket-and-abuse-log.md`: the per-connection transport bucket at ingress
- `production/epics/input-interaction-framework/story-032-ai-no-exemption.md`: AI versus human equality at the Prepare bucket (AC 12)
- `production/epics/input-interaction-framework/story-044-debounce-merge.md`: the release debounce and merge rule (AC 19)
- `production/epics/input-interaction-framework/story-089-knob-registry-conformance.md`: knob presence, ranges and suffixes in the tuning asset

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- `Source/StealthGameplay/Public/Server/Core/PawnCommandCore.h` (per-pawn token buckets at stage 2e)
- `Source/StealthGameplay/Private/Core/PawnCommandCore.cpp`
- `Source/StealthGameplay/Private/Tests/Input/TokenBucketDefaultsTest.cpp` (class `FInputTokenBucketDefaultsTest`, name `StealthGame.Input.TokenBucketDefaults`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

*Test cases not yet defined — run /qa-plan to generate them.*

QL-STORY-READY: GAPS
- The "Begins rejected at stage 2d consume no token" clause needs a Reject-returning predicate but the Given says only a stub `Allow`.
- The "each preceded by an executed End" precondition is dropped.
- Times are not binary-exact (60 Hz ticks cannot hit 0.125 s).

QA-lead proposed rewrite (unconfirmed; the GDD AC stays the authority): "Given a stub Allow predicate and Interact bucket B=BucketInteractSize (3), R=BucketInteractPerSec (2.0), at dt = 1/64 s: 16 Begins at t = k x 0.125, each preceded by an End sent the same tick as the prior Begin (executes after the 0.10 s debounce, before the next Begin) -> exactly 6 admitted at 0, 0.125, 0.25, 0.5, 1.0, 1.5. Separately, with a predicate returning Reject(Cooldown), the bucket is unchanged."

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Input/TokenBucketDefaultsTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on:
  - `production/epics/input-interaction-framework/story-027-stage-two-at-resolution.md`
  - cross-epic: tuning-data-config (`Bucket*` knobs and the knob registry; stories not yet written)
- Unlocks:
  - `production/epics/input-interaction-framework/story-032-ai-no-exemption.md`
  - `production/epics/input-interaction-framework/story-086-command-cost-per-50-pawns.md`
  - `production/epics/input-interaction-framework/story-089-knob-registry-conformance.md`
