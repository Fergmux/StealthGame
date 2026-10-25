# Story 083: AC 47 RejectionTimingIdentityBlind

> **Epic**: Input & Interaction Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/input-interaction-framework.md`
**Requirement**: `TR-iif-007`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0011: Pawn command interface and input pipeline
**ADR Decision Summary**: ADR-0011 section 8 makes the resolution tier of replies exactly-one-per-voided-command, with a code computed only from facts the owner already knows and never from a target pawn's identity, team, concealment or brace state; every reply (rejection or `FHoldStatus`) is appended to its sink's per-pass buffer and flushed once at the end of the pass that produced it, so arrival time never encodes the failure point.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: No post-cutoff API. The measurement is taken on the server's reply-send log (not on the client), so it needs the server-side command/reply log seam from story 095 and depends on ADR-0002's drain-end flush structure. Engine fact E10 (server RPC bodies run before the tick groups) places the command in its mailbox before the same frame's drain.

**Control Manifest Rules (this layer)**:
- Required: A resolution-voided command produces exactly one reply, flushed at the end of the drain's resolution step in one owner-only RPC per connection; `Rejected_Latched` replies `NotNow` through the held-reply buffer — source: ADR-0011
- Forbidden: Never let a reply code depend on a target pawn's identity, team, concealment or brace state; never deliver replies as replicated owner-only properties — source: ADR-0011
- Forbidden: Never validate everything at ingress (single stage) — breaks source parity and the total order — source: ADR-0011

---

## Acceptance Criteria

*From GDD `design/gdd/input-interaction-framework.md`, scoped to this story:*

- [ ] AC 47 (Integration, U `RejectionTimingIdentityBlind`): **GIVEN** rejected Attack and Interact commands that differ only in the nearby pawn (NPC, player, concealed, teammate), 50 samples each **WHEN** the server-side reply-send log is inspected **THEN** in 100 % of samples the code is identical and the reply is enqueued in the end-of-resolution step of the same drain as the command; **AND** the same holds for `FHoldStatus` confirmations of accepted commands.

---

## Implementation Notes

*Derived from ADR-0011 Decision section 8 and Implementation Guidelines; GDD R6 ("Rejections reveal nothing about anyone else"):*

**What this story builds.** The identity-blind timing test for the reply tier. For each of four nearby-pawn variants (NPC, player, concealed pawn, teammate) it produces a rejected Attack and a rejected Interact that differ in nothing but that pawn, 50 samples per variant, then inspects the server reply-send log. In every sample the reply code is identical across variants and the reply is enqueued in the end-of-resolution step of the same drain as the command; the same must hold for the `FHoldStatus` confirmation of an accepted command. This is the timing side-channel guard of GDD R6 and ADR-0011 section 8: if a reply could be enqueued inside a slot, or in a later drain, for some variants only, arrival time would encode the failure point and so the nearby pawn's kind.

**Interpretation notes (qa-lead).** A rejected Attack must fail on attacker-side legality (for example `Busy`, or a concealed `Hide` transition); an Attack is never rejected by the target (GDD R6, story 030). A reply enqueued inside a slot, or in a later drain, fails the test. A null-sink (AI) variant produces no reply and is not compared.

**Where it sits.** The reply code set (story 029) and the drain-end flush (story 004) define the behaviour; this Integration story asserts it as an invariant over variants, using the server reply-send log from story 095 (the log, the RPC-send hook and the stub doubles are owned there; plan note: stories 013, 015, 018, 067 and 083 also depend on 095).

**Prototype.** New code, no prototype counterpart (the prototype has no reply tier; `ServerAssassinate` returned results by pawn-visible state).

**Tuning keys (defaults).** None are asserted. The test depends on `BucketAttackSize`/`PerSec` and `BucketInteractSize`/`PerSec` only through the need to avoid bucket refusals unless intended (each sample is built to fail attacker-side legality).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/input-interaction-framework/story-029-rejection-code-set.md`: which failure maps to which code.
- `production/epics/input-interaction-framework/story-030-attack-never-rejected-by-target.md`: an Attack is never rejected by its target.
- `production/epics/input-interaction-framework/story-004-reply-sink-and-drain-end-flush.md`: the flush mechanism itself.
- `production/epics/input-interaction-framework/story-095-command-test-seams.md`: the server command/reply log and stub doubles this test uses.
- `production/epics/input-interaction-framework/story-060-hidden-site-probe.md`: identity-blindness of Interact ingress for hidden, rival and non-site fixtures.

---

## Files

- `Source/StealthGameplay/Private/Tests/Input/RejectionTimingIdentityBlindTest.cpp` — name `StealthGame.Input.RejectionTimingIdentityBlind` (class `FInputRejectionTimingIdentityBlindTest`; U form (automation test, no PIE), using the story 095 reply-send log and stub doubles; planned)
- `Source/StealthGameplay/Private/Core/` — reply-send logging hook exposed for the test harness (shared with story 095; planned)

---

## QA Test Cases

**Automated test specs:**

- **AC-1**: Reply code and enqueue point are identical across nearby-pawn variants (`StealthGame.Input.RejectionTimingIdentityBlind`).
  - Given: a rejected Attack and a rejected Interact, varying only the nearby pawn (NPC, player, concealed, teammate), 50 samples per variant, the server reply-send log.
  - When: commands resolve.
  - Then: the reply code is identical across variants in 100 percent of samples; each reply is enqueued in the end-of-resolution step of the command's own drain.
  - Given: accepted commands with the same variants.
  - Then: the `FHoldStatus` confirmation obeys the same rule.
  - Edge cases: a reply enqueued inside a slot or in a later drain fails; the null-sink (AI) variant produces no reply and is not compared. Note: a rejected Attack must fail attacker-side legality (for example `Busy`, or a concealed `Hide` transition); an Attack is never rejected by the target.

*QL-STORY-READY: ADEQUATE (qa-lead). Systemic gap recorded by the qa-lead: the reply-send log seam is owned by story 095.*

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthGameplay/Private/Tests/Input/RejectionTimingIdentityBlindTest.cpp` (`StealthGame.Input.RejectionTimingIdentityBlind`) must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/input-interaction-framework/story-029-rejection-code-set.md`, `production/epics/input-interaction-framework/story-095-command-test-seams.md`; cross-epic: round-match-framework (stories not yet written; resolution queue and drain); cross-epic: assassination-defence-reversal (stories not yet written; Attack and Prepare predicates); cross-epic: objectives-chain (stories not yet written; Interact predicate); cross-epic: concealment-bushes (stories not yet written; Hide transition predicate)
- Unlocks: None
