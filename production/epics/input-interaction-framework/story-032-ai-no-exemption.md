# Story 032: AC 12 AiNoExemption

> **Epic**: Input & Interaction Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/input-interaction-framework.md`
**Requirement**: `TR-iif-004`, `TR-iif-011`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

This story verifies GDD R17: AI controllers call the same entry points and are subject to every stage 2 step, including the per-pawn token buckets, with no exemption. An AI pawn and a human pawn each send the same sequence of Prepare Begins (each preceded by an executed End) against a bucket of B2 R1, and each path must admit exactly two (at t = 0 and t = 0.2). If AI skipped the buckets, an NPC could brace more often than a player could, which is a behavioural tell; if only ingress applied the buckets, AI (which skips stage 1) would escape them, which is why the buckets live at stage 2.

It follows story 031 (the bucket defaults and refill behaviour it reuses). The AI path here is the direct component call through the server-local mailbox with a null sink; the human path is `IngestCommand` through the stub PlayerController route with strictly increasing Seq.

**ADR Governing Implementation**: ADR-0011: Pawn command interface and input pipeline
**ADR Decision Summary**: Seven server-validated entry points with no caller identity; AI uses the same entry points under the same stage 2 rules, including the token buckets, and skips only ingress; a null reply sink is the only permitted difference.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0011: HIGH for the input layer, MEDIUM for the command path)
**Engine Notes**: No V1-V6 item gates this story. Use dt = 1/64 s where a 0.125 s multiple matters (the plan's note on rate-limit arrivals); the 0.2 s spacing here does not land exactly on a 1/64 tick, so state the tick quantisation in the test, and keep the expected admitted set (t = 0 and t = 0.2) robust to it (token 1.2 at 0.2 s is above 1.0).

**Control Manifest Rules (this layer)**:
- Required: Ingress drops are silent; stage 2 runs only at the command's queue slot and in the stated order; the per-pawn token bucket is consumed only on execution — source: ADR-0011
- Required: A resolution-voided command produces exactly one reply; a null sink (AI) discards — source: ADR-0011
- Forbidden: Never carry caller identity (`IsAI`, `IsPlayer`, controller, PlayerState, team) in an entry point, payload, event or listener — source: ADR-0011
- Forbidden: Never rate-limit `End` per pawn — source: ADR-0011
- Guardrail: Command interface: stage 2 a few microseconds per frame at the IIF F3 worst case — source: ADR-0011

---

## Acceptance Criteria

*From GDD `design/gdd/input-interaction-framework.md`, scoped to this story:*

- [ ] AC 12 (R17, Logic, U `AiNoExemption`): **GIVEN** the Prepare predicate stubbed to `Allow`, a bucket of B2 R1, and Begins sent at t = 0, 0.2, 0.4, 0.6 and 0.8 s (each preceded by an executed End) **WHEN** an AI pawn and a human pawn each send this sequence **THEN** each path admits exactly 2 (t = 0 and t = 0.2).

---

## Implementation Notes

*Derived from ADR-0011 Decision sections 3.2 and 10 and Implementation Guidelines:*

AI commands skip stage 1 and submit to the server-local mailbox with a null sink, but "AI is subject to every step including the buckets (IIF R17)". The bucket check is stage 2 step 5 for every source; there is no `IsAI` branch, only the sink differs. The refill arithmetic is F3: Prepare B2 R1 starts full (2 tokens); the Begin at t = 0 spends one (1.0 left); at t = 0.2 the bucket holds 1.2 and the Begin is admitted (0.2 left); at 0.4, 0.6 and 0.8 it holds 0.4, 0.6 and 0.8, so those Begins are refused at 2e.

**Rationale.** ADR-0011 section 10 and GDD R17: AI motion and red-herring actions must look and replicate identically, so the rate limit is part of the shared path; the buckets are a backstop against both human mashing and an AI bug flooding commands (Edge Cases "If an AI bug floods commands": the NPC simply idles).

**Test mechanics (QL-STORY-READY, ADEQUATE).** Each Begin k is sent at t_k; the preceding End is sent at t_(k-1) + 0.05 so its `DebounceExpiry` executes (+0.15) before Begin k; an End that has not executed would make the debounce merge them (AC 19) and no token would be drawn. Refused Begins consume no token (tokens at 0.4/0.6/0.8 stay below 1.0, still refused). The human path gets `NotNow` for each refusal; the AI path gets none (null sink; covered by AC 3, story 026).

**GDD references.** Core Rules R17; R5 item 2e; F3 defaults. Tuning Knobs (defaults, read from tuning data): `BucketPrepareSize` 2 (1-10), `BucketPreparePerSec` 1.0 (0.5-10), `ReleaseDebounceSec` 0.10.

**Prototype superseded.** Supersedes the prototype's AI path in `StealthNpcController.*`, which acted without any shared rate limit or command path. Not ported.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/input-interaction-framework/story-031-token-bucket-defaults.md`: the F3 refill and admit sequence for the Interact bucket
- `production/epics/input-interaction-framework/story-026-bound-unbound-same-events.md`: hub-event and reply parity across human, host and AI paths
- `production/epics/input-interaction-framework/story-044-debounce-merge.md`: the debounce merge rule that this test's End timing avoids
- `production/epics/input-interaction-framework/story-057-roster-legality-class.md`: the legality-class resolution of unbound pawns

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- `Source/StealthGameplay/Private/Core/PawnCommandCore.cpp` (no AI exemption at stage 2; no production change expected beyond story 031)
- `Source/StealthGameplay/Private/Tests/Input/AiNoExemptionTest.cpp` (class `FInputAiNoExemptionTest`, name `StealthGame.Input.AiNoExemption`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

**Logic — automated test specs (QL-STORY-READY: ADEQUATE):**

- **AC-1**: AC 12 `AiNoExemption` (`StealthGame.Input.AiNoExemption`): AI and human pawns each admit exactly 2 Prepare Begins (B2 R1) at t = 0, 0.2, 0.4, 0.6, 0.8
  - Given: Prepare predicate stub = Allow; Prepare bucket B/R from tuning (2, 1.0/s); pawn A driven by direct AI calls (null sink); pawn H driven by `IngestCommand` (stub PC path) with strictly increasing Seq; transport bucket at defaults.
  - When: for each t_k, Begin is sent at t_k; the preceding End is sent at t_(k-1) + 0.05 so its `DebounceExpiry` executes (+0.15) before Begin k.
  - Then: for A and for H, `OnActionStarted(Prepare)` fires exactly at t = 0 and 0.2 (tokens 1.2 at 0.2 admitted); Begins at 0.4/0.6/0.8 are refused at 2e; admitted-tick sequence identical for A and H.
  - Edge cases: the End must execute before the next Begin or the debounce merges them (AC 19) and no token is drawn; refused Begins consume no token (tokens 0.4/0.6/0.8, still refused); H gets `NotNow` for each refusal, A gets none (null sink, covered by AC 3).

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Input/AiNoExemptionTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on:
  - `production/epics/input-interaction-framework/story-031-token-bucket-defaults.md`
  - cross-epic: crowd-npc-behaviour (AI controller direct-call path; stories not yet written)
- Unlocks:
  - None
