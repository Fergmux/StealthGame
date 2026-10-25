# Story 021: AC 7 IngressNeverDropsMatchedEnd

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

This story adds the End-handling rules to stage 1 ingress (ADR-0011 Decision section 3.1 step 3, GDD R5.1 and F3). Held commands (Interact, Prepare) send `End` on every release, and a lost or dropped End would extend a hold on the server, so ingress must never drop an End that matches an outstanding Begin on that connection, even when the per-connection transport bucket is empty. Conversely a client could flood the reliable channel with Ends that match nothing, so Ends with no outstanding Begin are limited to `UnmatchedEndPerSec`.

It follows story 020 because it extends the `IngestCommand` function and its drop counter. Ingress must therefore track, per connection, which Begins are outstanding (by kind), which is the server-side mirror of the `UPawnCommandClient` outstanding-Begin tracking built in story 094 and used by the flush stories (012, 013).

**ADR Governing Implementation**: ADR-0011: Pawn command interface and input pipeline
**ADR Decision Summary**: Seven server-validated entry points with no caller identity; two-stage validation, where ingress applies a per-connection transport bucket to Begin and instant commands but never to a matching End; per-pawn buckets are consumed only on execution; one batched owner reply per drain.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0011: HIGH for the input layer, MEDIUM for the command path)
**Engine Notes**: No V1-V6 item gates this story. The reliable `ServerPawnCommand` RPC is why End must never be dropped and why the transport bucket bounds the reliable buffer (ADR-0011 section 2). V1 (story 008) concerns the client's exactly-one-End behaviour; this story is the server-side counterpart and tests the server only.

**Control Manifest Rules (this layer)**:
- Required: Ingress drops are silent; stage 2 runs only at the command's queue slot and in the stated order; the per-pawn token bucket is consumed only on execution — source: ADR-0011
- Required: Ends are idempotent per Begin; `UPawnCommandClient` sends End for every outstanding Begin before any context swap, menu open, focus loss, possession change or `ClientRestart` — source: ADR-0011
- Forbidden: Never rate-limit `End` per pawn or drop it at the transport bucket when it matches an outstanding Begin — source: ADR-0011
- Forbidden: Never validate everything at ingress (single stage) — source: ADR-0011
- Guardrail: Command interface: uplink at most 20 reliable RPCs/s per connection (transport bucket) — source: ADR-0011

---

## Acceptance Criteria

*From GDD `design/gdd/input-interaction-framework.md`, scoped to this story:*

- [ ] AC 7 (R5.1/F3, Logic, U `IngressNeverDropsMatchedEnd`): **GIVEN** an empty transport bucket and one outstanding Begin **WHEN** its End arrives **THEN** it is processed; **AND WHEN** 40 unmatched Ends arrive in 1 s **THEN** at most `UnmatchedEndPerSec` (30) reach the mailbox.

---

## Implementation Notes

*Derived from ADR-0011 Decision section 3.1 step 3 and Implementation Guidelines:*

Stage 1 step 3: per-connection transport bucket (`TransportBucketSize`/`PerSec`) for `Begin` and instant commands; an `End` matching an outstanding Begin is **never** dropped; unmatched Ends above `UnmatchedEndPerSec` are dropped (silent, counted for the abuse log). Stage 2 step 5 repeats the rule per pawn: `End` is never rate-limited, and an `End` with no matching active action is a no-op (that part is story 027 and the hold stories).

**Rationale.** The Begin/End asymmetry exists because the server owns every hold timer and the End is the only signal that shortens a hold; dropping one would silently lengthen a tell for the owner. The unmatched-End limiter exists only to stop a modified client flooding reliable Ends (ADR-0011 Consequences, transport bucket).

**GDD references.** R5.1 last bullet; F3 (Token bucket) defaults and the abuse-log note; Edge Cases "Input-layer" (a legitimate client cannot reach the limits). Tuning Knobs (defaults): `UnmatchedEndPerSec` 30 (hard 10-60, safe 20-40), `TransportBucketSize` 30, `TransportBucketPerSec` 20. Read from tuning data at test time (re-run the test at 20).

**Open algorithm decision (QL-STORY-READY).** The GDD does not define the unmatched-End limiter's algorithm. With a bucket of B = R = 30, 40 Ends spread evenly over 1 s all pass (net -0.25 token per arrival), so "at most 30" holds only if all 40 arrive together. The qa-lead proposes: the limiter is a token bucket with B = R = `UnmatchedEndPerSec`, initial full; 40 unmatched Ends arriving at the same server time give exactly 30 passes; after 1.0 s idle another 30 may pass. The author must confirm this algorithm (or a fixed 1 s window) with the GDD owner before `/dev-story`; whichever is chosen must be written into the test's arrival-time assumption (same server time, dt = 1/64 s).

**Prototype superseded.** New code, no prototype counterpart (the prototype `ServerSetInteracting(bool)` carried the hold state itself and had no Begin/End matching).

**Edge cases the test should cover (from QA review):** an End of a different kind than the outstanding Begin is unmatched; a second End for the same Begin is unmatched; a matched End does not consume an unmatched token.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/input-interaction-framework/story-020-ingress-clamp-and-drop.md`: the ingress function, clamp, self-ref strip and Interact id gate
- `production/epics/input-interaction-framework/story-022-transport-bucket-and-abuse-log.md`: the transport bucket for Begins and the abuse-log entry rule
- `production/epics/input-interaction-framework/story-013-flush-sends-end.md`: the client side that sends an End per outstanding Begin (AC 5)
- `production/epics/input-interaction-framework/story-044-debounce-merge.md`: the server-side release debounce at stage 2

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- `Source/StealthGameplay/Public/Server/Core/CommandIngress.h` (extends the file created by story 020)
- `Source/StealthGameplay/Private/Core/CommandIngress.cpp`
- `Source/StealthGameplay/Private/Tests/Input/IngressNeverDropsMatchedEndTest.cpp` (class `FInputIngressNeverDropsMatchedEndTest`, name `StealthGame.Input.IngressNeverDropsMatchedEnd`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

*Test cases not yet defined — run /qa-plan to generate them.*

QL-STORY-READY: GAPS
- The unmatched-End limiter's algorithm is undefined (bucket burst or fixed 1-s window?). With a bucket of B = R = 30, 40 Ends spread evenly over 1 s all pass (net -0.25 token per arrival), so "at most 30" holds only if all 40 arrive together.

QA-lead proposed rewrite (unconfirmed; the GDD AC stays the authority): "UnmatchedEnd limiter is a token bucket, B = R = UnmatchedEndPerSec, initial full. 40 unmatched Ends arriving at the same server time: exactly 30 pass. After 1.0 s idle another 30 may pass."

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Input/IngressNeverDropsMatchedEndTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on:
  - `production/epics/input-interaction-framework/story-020-ingress-clamp-and-drop.md`
  - cross-epic: tuning-data-config (knob registry; stories not yet written)
- Unlocks:
  - None
