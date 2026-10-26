# Story 023: A body killed while concealed stays hidden from every non-owner client

> **Epic**: Assassination & Defence Reversal
> **Status**: Blocked — BLOCKED: Concealment's client latch and `bDiedConcealed` consumer must be implemented (concealment-bushes story-020 and its latch stories); S9 and S10 have reported. Update 2026-10-26: the S10 gate is cleared — story-056 (S10 fix + re-run) reported **PASS** (`production/qa/spikes/2026-10-26_S10-rerun.md`: re-open guard with `ReopenAckTimeoutMs` 350, lost-pawn repair, warm-up and the ADR-0002 clock budget all met; S9 PASS 2026-10-25).
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25f
> **Last Updated**: 2026-10-26

## Context

**GDD**: `design/gdd/assassination-defence-reversal.md` (ASN-R10, ASN-R17, VA-ASN-6; Concealment CON-R6b, CON-R9; AC-ASN-44)
**Requirement**: `TR-asn-007`, `TR-asn-004`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0018: Encounter data and outcome presentation timing (§1.2 `bDiedConcealed`, §3.2 step 3), with ADR-0017 (§8 B killed-concealed re-open; client latch `IsAllyBodyHidden`) and ADR-0003 §5.7
**ADR Decision Summary**: A victim killed while `Concealed`/`CullPending` stays culled (channel closed on every non-allied connection) until the death write, where it re-opens to every viewer already falling with `FDeathState.bDiedConcealed = true` written in the same step; every non-owner client then keeps it presentation-hidden through fall, linger and removal, and the initial block is identical for a player and an NPC victim.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (networking and render flags; hidden-primitive flags and re-open race are S10/R5 [U])
**Engine Notes**: Hiding applies Concealment VA-4's flag set to every primitive of the pawn and its attachments; `bCastHiddenShadow`, `bAffectIndirectLightingWhileHidden`, `bRayTracingFarField` are class-default false and never toggled; hiding uses the presentation latch, never `SetActorHiddenInGame` (`bHidden` replicates and feeds relevancy). The server send capture must show no actor-channel record, no owned or attached actor record and no NetGUID export of the pawn in any other actor's data (`PlayerState`, `Instigator`) before the death-write tick (zero tolerance). The frame capture from channel open is taken in a rendering session (the Integration assertions run headless).

**Control Manifest Rules (this layer)**:
- Required: A killed-concealed pawn un-culls at the death write together with `FDeathState.bDiedConcealed = true`; Owner/Team `bIsConcealed` clears at the server body removal or the participant's rebind, whichever comes first — source: ADR-0003, ADR-0017
- Forbidden: Never re-open a killed-concealed pawn before the death write, and never draw its body on a non-owner client — source: ADR-0017
- Required: `FDeathState.bDiedConcealed` is written in the same write for every victim kind (identical wire shape for the player and the NPC victim) — source: ADR-0018
- Guardrail: Initial-block property-handle list, order, bit length and send-tick offset identical for player and NPC victims

Killed-concealed means the victim only leaves Concealment's server-only occupancy record at the slot (so a second strike in the same tick draws from the survivors); everything visible happens at the death write. Allies are included in the hide: the body is shown to nobody but its owner.

---

## Acceptance Criteria

*From GDD `design/gdd/assassination-defence-reversal.md` (AC-ASN-44, ASN-R10), scoped to this story:*

- [ ] **AC1** (AC-ASN-44 wire, [Integration/BLOCKING][Net] `StealthGame.Assassination.ConcealedBodyStaysHidden`, ASN-R10, R17, CON-R9): **GIVEN** a rival in the `Culled` state (its channel closed on every non-allied connection at the slot, asserted) and, separately, a concealed NPC in `Culled`, killed by a bush strike, under no loss and a fixed zero-variance packet delay **THEN** the server send capture shows, for every connection that is not an ally of the victim (for the NPC: every connection), no actor-channel or replicated-object record for the pawn, for any actor owned by or attached to it, and no NetGUID export of the pawn inside any other actor's data, with send tick < the death-write tick (`ContactServerTime − StrikeNetLeadSec`, zero tolerance); the record opens in the death-write tick or the next with `FDeathState.bDiedConcealed = true` and the fall already commanded in the initial block; the initial block's property-handle list and order, bit length and send-tick offset are identical for the player and the NPC victim.
- [ ] **AC2** (AC-ASN-44 client latch, [Integration/BLOCKING][Net]): **GIVEN** the same two kills **THEN** on every non-owner client, allied clients included, the presentation-hidden latch (`IsAllyBodyHidden`) is true at `PostNetInit`, at `BeginPlay` and on every client frame through fall, linger and removal, and every primitive component of the pawn and its attachments reports hidden (never cleared by the death pose, CON-R6b rule 3); this holds again under the `StealthLeak2` profile with a seeded loss stream.
- [ ] **AC3** (AC-ASN-44 negative controls and evidence, [Integration/BLOCKING] + [Visual/BLOCKING] `asn-44-*.png`): **GIVEN** a seam build that un-culls in the slot and a seam build that omits `bDiedConcealed` **THEN** AC1/AC2 fail for each; a frame capture from channel open is retained as `production/qa/evidence/asn-44-*.png` from a rendering session.

---

## Implementation Notes

*Derived from ADR-0018 §3.2 step 3, ADR-0017 §8 B, ADR-0003 §5.7 and GDD ASN-R10, VA-ASN-6:*

Slot: the victim exits Concealment's occupancy record (story 007 step 5) but stays server-culled. Death write (story 013): `FDeathState` with `bDiedConcealed = true` in the same write as the re-open, so the initial bunch carries the death state and the client latch can hide before the first rendered frame. Body removal clears the Owner/Team `bIsConcealed` flag (`IBodyRemovalObserver`, story 019). Every non-owner pawn actor is created presentation-hidden and the latch does not clear while a living teammate entry's `PawnRef` is unresolved (ADR-0017).

Test rig: S9's close instrumentation (IVP story 004) and the concealed-victim construction from the Concealment epic. The two negative-control seams (un-cull in the slot; omit `bDiedConcealed`) live under `WITH_STEALTH_TEST_SEAMS`. Keep the NPC and player runs byte-structure-identical (CON-R9 route, user decision 2026-10-25).

**Prototype:** the prototype's `bConcealed` replicated to every client (known issue 2 in `production/first-playable-report.md`), the opposite of this contract; nothing is promoted.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-022-culled-victim-opens-falling-and-late-actors.md`: non-concealment culls.
- `story-013-death-write-staging-and-after-latch.md`: the write and hook mechanism.
- Concealment epic: occupancy, the client latch and `bDiedConcealed` consumption.

---

## Files

- `Source/StealthTestHarness/Private/Assassination/ConcealedBodyStaysHiddenTest.cpp` (`StealthGame.Assassination.PIE.ConcealedBodyStaysHidden`; two negative-control seams)
- `Source/StealthGameplay/Private/Features/Assassination/EncounterService.cpp` (slot step 5 hand-off and death-write `bDiedConcealed`; shared with stories 007 and 013)
- `production/qa/evidence/asn-44-*.png`

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

QL-STORY-READY: GAPS (blocked on S9, S10 and the Concealment epic)
- AC1: "a fixed zero-variance packet delay" has no value or profile name; proposed rewrite (unconfirmed): under the 0 % loss, constant-delay profile (profile name and delay D ms in the test header; zero tolerance on the send-tick assertions).
- AC2: "every client frame" and "a seeded loss stream" are unbounded; proposed rewrite (unconfirmed): true on every client frame from `PostNetInit` through removal completion (frame count recorded), under `StealthLeak2` with seed S and loss rate L fixed in the test header.
- AC3: the visual capture has no pass condition; proposed rewrite (unconfirmed): the retained `asn-44-*.png` sequence from channel open through the first 10 frames of the fall shows the body in no frame on any non-owner client, with a lead sign-off line; the owner is exempt from the hide.

- **AC-1**: Wire contract
  - Given: a Culled rival (channel closed on every non-allied connection, asserted) and, separately, a Culled concealed NPC; a bush strike; no loss
  - When: the strike resolves and the death write commits
  - Then: no record, owned or attached actor record, or NetGUID export of the pawn exists on any non-ally connection (all connections for the NPC) before the death-write tick (`ContactServerTime − StrikeNetLeadSec`, zero tolerance); the record opens in the death-write tick or the next with `bDiedConcealed = true` and the fall in the initial block; property-handle list and order, bit length and send-tick offset are identical for the player and NPC runs
  - Edge cases: a second strike in the same tick draws from the survivors; the killer's team connection; a viewer joining mid-sequence; attachments owned by the pawn
- **AC-2**: Client latch
  - Given: the same two kills with an allied client, a non-allied client and the owner; a second pass under `StealthLeak2`
  - When: the clients process creation, `BeginPlay` and every frame through fall, linger and removal
  - Then: `IsAllyBodyHidden` is true at `PostNetInit`, `BeginPlay` and every frame on non-owner clients; every primitive of the pawn and its attachments reports hidden and is never cleared by the death pose
  - Edge cases: an unresolved teammate `PawnRef` (the latch must not clear); loss that delays the initial bunch; the owner still sees its own body
- **AC-3**: Negative controls and evidence
  - Given: seam builds that (a) un-cull in the slot and (b) omit `bDiedConcealed`
  - When: AC1 and AC2 run against each
  - Then: each seam build fails AC1 or AC2 as expected, and the rendering-session capture is retained
  - Edge cases: a seam build that passes by accident means the assertion is too loose; a capture taken headless is not valid evidence

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: `ConcealedBodyStaysHiddenTest.cpp` — must exist and pass
- Visual: retained capture `production/qa/evidence/asn-44-*.png` (AC-ASN-44's separate `[Visual]` evidence)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `story-006-bush-spot-rule-and-victim-pick.md`, `story-013-death-write-staging-and-after-latch.md`, `story-019-server-body-sequence-and-reset.md`, `story-022-culled-victim-opens-falling-and-late-actors.md`; cross-epic (BLOCKING) `production/epics/information-visibility-policy/story-004-spike-s9-cull-latency-and-explicit-channel-close.md`, `story-005-spike-s10-reopen-behaviour-and-netguid.md`; Concealment epic `production/epics/concealment-bushes/EPIC.md` (no stories yet)
- Unlocks: Concealment epic's kill-concealed acceptance; IVP `story-029-uncull-lead-bound.md`
