# Story 022: AC 59 TransportBucketAndAbuseLog

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

This story implements the numeric rate-limiting rules of F3 at the two places they apply: the per-connection transport bucket at ingress (B30 R20, Begin and instant commands only) and the server-only abuse log that records a connection which drops too many commands in a window (`AbuseLogDropCount` 20 within `AbuseLogWindowSec` 10 s). AC 59 also asserts that `HideEnter` and `HideExit` draw on one shared per-pawn bucket (B4 R2), which is a stage 2e property rather than an ingress one.

It follows story 020 (the ingress function and drop counter) and is a prerequisite of story 024 (host parity, which compares the host and remote paths against the 35-simultaneous-Begins case). The transport bucket is sized so that ingress never drops a press the per-pawn buckets would admit (transport B30 R20 against twice the per-pawn sums of 22 and 12), so the bucket here is purely an abuse guard, and an honest client cannot reach the abuse log (GDD F3).

**ADR Governing Implementation**: ADR-0011: Pawn command interface and input pipeline
**ADR Decision Summary**: Seven server-validated entry points with no caller identity; two-stage validation with a per-connection transport bucket at ingress and per-pawn per-type buckets consumed only on execution at the command's queue slot; silent ingress drops counted for a server-only abuse log.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0011: HIGH for the input layer, MEDIUM for the command path)
**Engine Notes**: No V1-V6 item gates this story. The abuse log is server-only data (never replicated); logging uses `UE_LOGF` per ADR-0007/VERSION.md guidance, not a gameplay channel. The reliable-RPC buffer is what the transport bucket protects (ADR-0011 section 2: "Reliable, because a lost End would extend a hold; the transport bucket (3.1) bounds the reliable buffer").

**Control Manifest Rules (this layer)**:
- Required: Ingress drops are silent; stage 2 runs only at the command's queue slot and in the stated order; the per-pawn token bucket is consumed only on execution — source: ADR-0011
- Required: Hold ends are `ScheduleAt` queue items; the release debounce is server-side and identical for every pawn — source: ADR-0011
- Forbidden: Never rate-limit `End` per pawn or drop it at the transport bucket when it matches an outstanding Begin — source: ADR-0011
- Forbidden: Never special-case `NM_ListenServer` in input ingress — source: ADR-0011
- Guardrail: Command interface: uplink at most 20 reliable RPCs/s per connection (transport bucket); stage 2 a few microseconds per frame at the IIF F3 worst case (about 5 items/frame) — source: ADR-0011

---

## Acceptance Criteria

*From GDD `design/gdd/input-interaction-framework.md`, scoped to this story:*

- [ ] AC 59 (F3, Logic, U `TransportBucketAndAbuseLog`): **GIVEN** the defaults **WHEN** a connection sends 35 Begins in 0.1 s **THEN** 30 pass ingress and 5 are dropped; **AND** HideEnter and HideExit draw on one shared per-pawn bucket; **AND** 20 ingress drops within 10 s write exactly one abuse-log entry, while 19 write none.

---

## Implementation Notes

*Derived from ADR-0011 Decision sections 3.1 and 3.2 and Implementation Guidelines:*

Ingress step 3: per-connection transport bucket (`TransportBucketSize`/`PerSec`) for Begin and instant commands; matched Ends are never dropped (story 021). Every ingress drop is counted for the server-only abuse log, which writes an entry when `AbuseLogDropCount` drops fall within `AbuseLogWindowSec`. Stage 2 step 5: per-pawn, per-type token bucket (`Bucket*Size`/`PerSec`), consumed only when steps 0-4 pass and the command executes.

**F3 formula (GDD).** `tokens(t) = min(B, tokens(t_last) + R * (t - t_last))`; a command is admitted if `tokens >= 1` (the boundary value 1.0 is admitted) and then `tokens -= 1`; initial tokens = B. Defaults: per pawn Interact B3 R2, Prepare B2 R1, Attack B2 R1, HideEnter/HideExit B4 R2 as one shared bucket; per connection B30 R20. Reuse the single bucket primitive in `FPawnCommandCore` (story 002) for both uses.

**Rationale.** The cross-field rule (transport size at least twice the sum of per-pawn sizes, 22; transport rate at least twice the sum of per-pawn rates, 12) is why ingress never drops a press stage 2 would admit; it is a TDC change request, not enforced here. Sharing one Hide bucket for Enter and Exit stops bush flicker from doubling the allowance.

**GDD references.** F3 (Token bucket) defaults and abuse-log paragraph; R5.1; Edge Cases "If a player mashes keys". Tuning Knobs (defaults, all read from tuning data at test time): `TransportBucketSize` 30, `TransportBucketPerSec` 20, `BucketHideSize` 4, `BucketHidePerSec` 2.0, `AbuseLogDropCount` 20, `AbuseLogWindowSec` 10.

**Arrival-time assumption (QL-STORY-READY).** "35 Begins in 0.1 s -> 30 pass" only holds if all 35 arrive at the same server instant (B30 R20 refills 2 tokens in 0.1 s, so spread arrivals pass 32). State in the test that the 35 arrive at the same server time and use dt = 1/64 s. Counters start at 0 per sub-case. "Exactly one" abuse-log entry semantics (40 drops in one window still write one entry) is for the author to confirm. 20 drops spread over 10.1 s write 0 entries.

**Sequencing flag.** The shared Hide bucket clause exercises stage 2 steps 2d-2e (story 027), which the plan does not list as a dependency of this story. If story 027 is not merged, drive the shared bucket primitive directly through the stub predicate and a free-slot stub; re-check at `/dev-story`.

**Seam gap.** The abuse-log entry sink has no named test observable in the ADR; the test needs a readable abuse-log writer or counter in the server-only ingress state.

**Prototype superseded.** New code, no prototype counterpart (the prototype had no rate limiting on its Server RPCs).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/input-interaction-framework/story-020-ingress-clamp-and-drop.md`: ingress function, clamp, strip, id gate and the drop counter itself
- `production/epics/input-interaction-framework/story-021-ingress-never-drops-matched-end.md`: matched-End guarantee and the unmatched-End limiter
- `production/epics/input-interaction-framework/story-031-token-bucket-defaults.md`: the per-pawn Interact bucket sequence (B3 R2) at stage 2e
- `production/epics/input-interaction-framework/story-024-host-ingress-parity.md`: host-local versus remote parity for the same inputs

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- `Source/StealthGameplay/Public/Server/Core/CommandIngress.h` (transport bucket and abuse-log state)
- `Source/StealthGameplay/Private/Core/CommandIngress.cpp`
- `Source/StealthGameplay/Private/Tests/Input/TransportBucketAndAbuseLogTest.cpp` (class `FInputTransportBucketAndAbuseLogTest`, name `StealthGame.Input.TransportBucketAndAbuseLog`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

*Test cases not yet defined — run /qa-plan to generate them.*

QL-STORY-READY: GAPS
- "35 Begins in 0.1 s -> 30 pass" only holds if all 35 arrive at the same instant (B30 R20 refills 2 tokens in 0.1 s; spread arrivals pass 32).
- The Hide-shared-bucket clause tests the per-pawn bucket (stage 2e), not the transport bucket.
- The abuse-log counter must start clean.

QA-lead proposed rewrite (unconfirmed; the GDD AC stays the authority): "35 Begins arriving at the same server time: 30 pass, 5 dropped. HideEnter and HideExit alternately drain one per-pawn bucket of BucketHideSize. Counters start at 0 per sub-case."

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Input/TransportBucketAndAbuseLogTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on:
  - `production/epics/input-interaction-framework/story-020-ingress-clamp-and-drop.md`
  - cross-epic: tuning-data-config (knob registry; stories not yet written)
- Unlocks:
  - `production/epics/input-interaction-framework/story-024-host-ingress-parity.md`
