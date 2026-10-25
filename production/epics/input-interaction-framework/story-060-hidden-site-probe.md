# Story 060: AC 71 HiddenSiteProbe

> **Epic**: Input & Interaction Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/input-interaction-framework.md`
**Requirement**: `TR-iif-004`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0011: Pawn command interface and input pipeline
**ADR Decision Summary**: Seven server-validated entry points with no caller identity, two-stage validation (ingress, then the command check at the queue slot) with token buckets, Enhanced Input edges and batched owner replies. At ingress an `InteractBegin` id must be a public Interact target, and every court fixture id is accepted whatever its role or the sender's knowledge, so a probing client learns nothing about which court is live or hidden.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0011 input layer HIGH, command path MEDIUM)
**Engine Notes**: No post-cutoff input API is involved; this is server ingress and queue-slot behaviour. The test runs under the ADR-0008 net-emulation profile (N form: PktLag 50 ms per direction, 1 % loss) but its assertions are on server-side logs only, because client arrival times are noisy under loss and retransmits.

**Control Manifest Rules (this layer)**:
- Required: Ingress drops are silent; stage 2 runs only at the command's queue slot and in the stated order; the per-pawn token bucket is consumed only on execution — source: ADR-0011
- Required: A resolution-voided command produces exactly one reply, flushed at the end of the drain's resolution step in one owner-only RPC per connection — source: ADR-0011
- Forbidden: Never let `AllowNoEffect` vs `Reject` depend on anything but the site and legality class — source: ADR-0011
- Forbidden: Never let a reply code depend on a target pawn's identity, team, concealment or brace state — source: ADR-0011
- Guardrail: uplink at most 20 reliable RPCs per second per connection (transport bucket); at most one reply RPC per connection per drain — source: ADR-0011

This story is the hidden-site probe test. A modified client can send `Interact(Begin)` with any `FCourtFixtureId` from 100 cm away. If the server treated a hidden final site, a non-site court fixture and a rival live site differently from each other (a drop, a different reply, a different timing class, a different completion), the probing client would learn which court fixture is the hidden final site. The reconciled rule (2026-10-24: every court is accepted at ingress, IVP R7.4a, AC-IVP-41) is that all three are accepted, run the same hold, emit the same events and replies with the same timing class, and complete as `AllowNoEffect`; only an id that names no public Interact target at all (a nonexistent id) is dropped, silently, at ingress.

The story sits after story 055 (objectives completion), which proves the completion path (`Allow` advances Objectives, a later End is a no-op), and builds on the ingress rules of stories 020 and 055. It unlocks story 075 (prompt legality), which tests the owner-side counterpart: the identical "Tend" prompt and ring at every court. It relates to GDD R5.1 (ingress) and R5d (`AllowNoEffect`), and to ADR-0011 sections 3.1 step 5 and 6.

---

## Acceptance Criteria

*From GDD `design/gdd/input-interaction-framework.md`, scoped to this story:*

- [ ] AC 71 (R5.1, Integration, N `HiddenSiteProbe`): **GIVEN** a modified client sending `Interact(Begin)` from 100 cm with (a) a hidden final site's id, (b) a non-site court's id and (c) a rival live site's id, 50 times each, plus (d) a nonexistent id **WHEN** server-side handling is logged **THEN** (a), (b) and (c) are accepted at ingress, run the same hold, emit the same events and replies with the same timing class, and complete as `AllowNoEffect`; **AND** (d) is dropped silently at ingress; **AND** no client-observable message differs among (a)-(c) (reconciliation 2026-10-24: every court is accepted at ingress, IVP R7.4a, AC-IVP-41).

---

## Implementation Notes

*Derived from ADR-0011 Decision sections 3.1 (step 5), 5 and 6, and the Implementation Guidelines:*

- Ingress step 5: `InteractBegin` — the id must be a public Interact target; every court fixture id is accepted, whatever its role or the sender's knowledge (IIF R5.1, Objectives OC-6, IVP R7.4a ruling D). A nonexistent id is a silent drop counted for the server-only abuse log; no reply (ADR-0011 section 8, ingress tier).
- `AllowNoEffect` runs exactly like `Allow` (same pose, same timer, same events); only its completion changes nothing. The choice between `AllowNoEffect` and `Reject` may depend only on the site and the legality class, so the Objectives predicate for a hidden or rival fixture must return `AllowNoEffect`, never `Reject`. If it returns `Reject` the probe has a side channel; fix it in Objectives, but this test reports it.
- Identical timing class: the hold duration `D` (`ObjectiveInteractSec`, 3.0) is read once at execution from the Interact target owner (`IInteractTargetOwner::GetInteractHoldSec()`, which takes no court parameter, so `D` cannot differ by court). The `FHoldStatus` enqueue tick and completion tick offsets are identical across (a)-(c).
- Pacing (qa-lead): 50 holds per case collide with the per-pawn Interact bucket (`BucketInteractSize` 3, `BucketInteractPerSec` 2.0) and the 1 % loss profile makes client timing noisy. Either pace each Begin after the previous hold has completed (about 3 s apart) or raise the bucket in test config. Compare server logs, not client arrival times; retransmits are excluded that way. Do not weaken the transport bucket (`TransportBucketSize` 30, `TransportBucketPerSec` 20) for the production defaults.
- Prototype disposition: the prototype `ServerSetInteracting` lets the client choose the hold state and names no court id; it is replaced, not ported. This test has no prototype counterpart.
- Test seams missing today and named by the qa-lead: a "modified-client harness hook" that sends a raw `ServerPawnCommand(FNetPawnCommand)` with an arbitrary `TargetId` (a test-only send path on `AStealthPlayerController`, story 094's transport plus story 095's server command log). Without it this test cannot be written; do not add a production RPC for it. Objectives and Information Visibility Policy stubs or the real systems are needed for the hidden/rival/non-site classification; the qa-lead marks the story DEFERRED until Objectives and IVP exist, which per the plan is a cross-epic dependency and does not make it Blocked.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/input-interaction-framework/story-020-ingress-clamp-and-drop.md`: the nonexistent-id drop rule in isolation (a Logic test of ingress).
- `production/epics/input-interaction-framework/story-055-objectives-completion.md`: `Allow` completion and Objectives advancing exactly once.
- `production/epics/input-interaction-framework/story-028-allow-no-effect-parity.md`: `Allow` versus `AllowNoEffect` event parity at Logic level.
- `production/epics/input-interaction-framework/story-075-prompt-legality.md`: the owner-side prompt and ring colour parity.
- `production/epics/input-interaction-framework/story-083-rejection-timing-identity-blind.md`: rejection reply timing.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Private/Core/PawnCommandCore.cpp` (ingress step 5 and the Interact path, changed only if the probe exposes a difference)
- `Source/StealthTestHarness/Private/HiddenSiteProbePieTest.cpp` (CQTest `NETWORK_TEST_CLASS(HiddenSiteProbe, "StealthGame.Input.PIE")`, full name `StealthGame.Input.PIE.HiddenSiteProbe.<Method>`; net emulation PktLag 50 ms per direction, 1 % loss; assertions on server logs only)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

*Test cases not yet defined — run /qa-plan to generate them.*

QL-STORY-READY: GAPS
- Gap 1: "same timing class" and "no client-observable message differs" are unmeasurable as written.
- Gap 2: 50 holds per case collide with the Interact bucket (B3, 2/s), and the 1 % loss profile makes client-side timing noisy.

QA-lead proposed rewrite (unconfirmed; the GDD AC stays the authority): "Timing class = the server-side log of (command arrival tick, FHoldStatus enqueue tick, hold duration D +/- dt, completion event tick offset) identical across (a)-(c); client-observable = the captured sequence of owner-only reliable RPC payloads (entry kind, code, size) identical modulo site id and seq; holds are paced so each Begin follows the previous completed hold (about 3 s apart) or the test raises the bucket in test config."

QA-lead test sketch (unconfirmed): `StealthGame.Input.PIE.HiddenSiteProbe.<Method>` (net emulation PktLag 50 ms/direction, 1 % loss; assertions on server logs only). Given a modified-client harness hook sending raw `Interact(Begin)` with arbitrary TargetId from 100 cm for (a) hidden final id, (b) non-site court id, (c) rival live site id, (d) nonexistent id, 50 each; Then (a)-(c) accepted at ingress with identical hold D, identical events and replies, completing as `AllowNoEffect` with no effect on objective state, and (d) dropped at ingress with zero replies and counted in the abuse log; captured owner-RPC sequences for (a)-(c) identical. Edge cases: DEFERRED until Objectives and IVP exist; retransmits are excluded by comparing server logs, not client arrival times.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/HiddenSiteProbePieTest.cpp` (CQTest `StealthGame.Input.PIE.HiddenSiteProbe`) OR a playtest doc

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/input-interaction-framework/story-055-objectives-completion.md`; cross-epic: objectives-chain (stories not yet written; court fixture predicates and OC-6), cross-epic: information-visibility-policy (stories not yet written; IVP R7.4a, AC-IVP-41)
- Unlocks: `production/epics/input-interaction-framework/story-075-prompt-legality.md`
