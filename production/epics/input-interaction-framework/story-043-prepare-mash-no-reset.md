# Story 043: AC 18 PrepareMashNoReset

> **Epic**: Input & Interaction Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/input-interaction-framework.md`
**Requirement**: `TR-iif-005`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

This story guarantees that mashing Prepare can neither reset nor extend anything. While a brace is active, `Prepare(Begin)` is an idempotent no-op: no event, no reply, no token, no cap restart. While the brace is cooling, each Begin is rejected `Cooldown` by Assassination's predicate: exactly one reply per Begin, no event, and the cooldown end is unchanged. The first legal press after the cooldown must still be admitted, so a refused press must not drain the per-pawn bucket.

It follows story 042 (cooldown stamped at execution) and unlocks story 056 (nothing resets the cooldown). In ADR-0011 it is section 4 (same-type rule: `PrepareBegin` while bracing is a no-op with no reply) and section 3.2 (the per-pawn token bucket is consumed only when steps 0-4 pass and the command executes). GDD rules: R8 (Prepare) and R9 (same-type commands).

**ADR Governing Implementation**: ADR-0011: Pawn command interface and input pipeline
**ADR Decision Summary**: One action slot per pawn: `PrepareBegin` while bracing is a no-op with no reply; the token bucket is consumed only on execution; a resolution-voided command gets exactly one reply.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Command path is MEDIUM in ADR-0011's Engine Compatibility; the input-layer HIGH rating does not apply. No V1-V6 gate. Note the client-side traffic saving (the client does not send `PrepareBegin` while its known cooldown runs) is owner prediction only and is tested elsewhere; the server must validate every Begin it receives.

**Control Manifest Rules (this layer)**:
- Required: Ingress drops are silent; stage 2 runs only at the command's queue slot and in the stated order; the per-pawn token bucket is consumed only on execution — ADR-0011
- Required: A resolution-voided command produces exactly one reply, flushed at the end of the drain's resolution step in one owner-only RPC per connection — ADR-0011
- Required: Feature predicates are registered weakly from the composition root per `EActionType` — ADR-0011
- Forbidden: Never let a reply code depend on a target pawn's identity, team, concealment or brace state — ADR-0011
- Forbidden: Never carry caller identity in an entry point, payload, event or listener — ADR-0011
- Guardrail: Command interface: uplink at most 20 reliable RPCs/s per connection (transport bucket); at most one reply RPC per connection per drain — ADR-0011

---

## Acceptance Criteria

*From GDD `design/gdd/input-interaction-framework.md`, scoped to this story:*

- [ ] AC 18 (R8, Logic, U `PrepareMashNoReset`): **GIVEN** (a) an active brace and (b) a cooling brace **WHEN** Begin is sent 10 times in each state **THEN** no event fires, the cap does not restart and the cooldown end is unchanged in both; **AND** in (a) no reply is sent; **AND** in (b) each Begin gets exactly one `Cooldown` reply.

---

## Implementation Notes

*Derived from ADR-0011 Decision sections 3.2, 4 and 5:*

- **State (a), active brace.** The slot arbitration (stage 2 step 3) treats `PrepareBegin` while `Bracing` as a no-op: no event, no reply, no token drawn, `HoldTimer(CapEnd)` untouched (same `DueTime`, same `ActionEpoch`), no pending End created.
- **State (b), cooling.** The slot is free, so Assassination's registered predicate answers `Reject(Cooldown)` at step 4. A void produces exactly one reply per Begin, enqueued at drain end. Because the refusal happens at step 4 (2d), before the token bucket (step 5, 2e), no token is consumed and the first legal press after the cooldown is admitted. No cap, event or cooldown change.
- **Sequence numbers.** Use a strictly increasing `ClientCommandSeq`; 10 Begins stay within the transport bucket (`TransportBucketSize` 30), so none is dropped at ingress.
- **AI path.** Identical, except a null sink discards the replies (0 replies); the difference is decided by the sink, never by a caller check.
- **Test seam.** Assassination predicate stub with `Reject(Cooldown)` mode and a free slot; record cap `DueTime` and the stubbed cooldown end before and after. Named doubles come from story 095.
- **Tuning keys (defaults).** `BucketPrepareSize` 2, `BucketPreparePerSec` 1.0, `TransportBucketSize` 30; `DefenceDurationSec` 4.0 and `DefenceCooldownSec` 60 are Assassination's.
- **Prototype.** Replaces `OnDefencePressed/Released` and `ServerSetDefence` (the prototype let the client set the hold state); not ported.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 042 (`story-042-prepare-cooldown-stamped.md`): when the cooldown is stamped.
- Story 044 (`story-044-debounce-merge.md`): a re-press inside the End debounce window.
- Story 029 (`story-029-rejection-code-set.md`): the general code mapping and reply timing.
- Story 031 (`story-031-token-bucket-defaults.md`): token bucket refill behaviour.
- Story 056 (`story-056-assassination-cooldown-no-reset.md`): other actions that must not reset the cooldown.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- Source/StealthGameplay/Private/Core/PawnCommandCore.cpp (same-type no-op, predicate-before-bucket ordering)
- Source/StealthGameplay/Private/Tests/Input/PrepareMashNoResetTest.cpp (`FInputPrepareMashNoResetTest`, `StealthGame.Input.PrepareMashNoReset`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation. (At tiers where the QL-STORY-READY gate is skipped — `minimal`, or `lean`/`solo` review mode — no qa-lead specs exist, and this section reads: N/A — no qa-lead specs at this tier; implement against the Acceptance Criteria above.)*

**Logic story — automated test spec (`StealthGame.Input.PrepareMashNoReset`):**

- **AC-1**: Mashing Prepare cannot reset or extend
  - Given: (a) active brace; (b) cooling (Assassination predicate stub = `Reject(Cooldown)`, slot free); cap and cooldown end recorded; strictly increasing `Seq`.
  - When: 10 Prepare Begins sent in each state, within the transport bucket.
  - Then: (a) no event, no reply, cap `DueTime` and cooldown end unchanged, no token drawn; (b) no event, exactly 10 `Cooldown` replies (one per Begin, enqueued at drain end), cooldown end unchanged.
  - Edge cases: (b) refused at 2d before 2e so no token consumed and the first legal press after cooldown is admitted; no pending End is created; AI path identical with null sink (0 replies).

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Input/PrepareMashNoResetTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/input-interaction-framework/story-042-prepare-cooldown-stamped.md`; cross-epic: assassination (stories not yet written) for the real `Cooldown` predicate; cross-epic: tuning-data-config (stories not yet written) for the knobs
- Unlocks: `production/epics/input-interaction-framework/story-056-assassination-cooldown-no-reset.md`
