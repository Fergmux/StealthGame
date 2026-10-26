# Story 022: A culled victim re-opens already falling, and late actors seek and never snap

> **Epic**: Assassination & Defence Reversal
> **Status**: Ready — Update 2026-10-26: the S10 gate is cleared — story-056 (S10 fix + re-run) reported **PASS** (`production/qa/spikes/2026-10-26_S10-rerun.md`: re-open guard with `ReopenAckTimeoutMs` 350, lost-pawn repair, warm-up and the ADR-0002 clock budget all met; S9 PASS 2026-10-25). Other dependencies are ordinary story order (see Dependencies).
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25f
> **Last Updated**: 2026-10-26

## Context

**GDD**: `design/gdd/assassination-defence-reversal.md` (ASN-R10, ASN-R21, VA-ASN-2; Edge Cases "Presentation and network"; AC-ASN-45, AC-ASN-25)
**Requirement**: `TR-asn-004`, `TR-asn-010`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0018: Encounter data and outcome presentation timing (§3.2 step 3 death-write re-open, §5.1 fall seek, Verification Required (2)), with ADR-0003 §5.8 (re-open behaviour)
**ADR Decision Summary**: A victim culled for a reason other than concealment re-opens at the death write, never in the slot, already carrying `FDeathState`; the re-created client actor initialises its presentation from the replicated state through the same idempotent `Refresh(EstimatedServerNow())` path, seeking the fall and never snapping, and an actor first received after the linger is never shown.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (networking; E10/E11: a closed channel stays closed while the override returns false, but whether the NetGUID is reused and how the AnimBP initialises on re-open are [U], owned by S10)
**Engine Notes**: In the MVP configuration no pawn has a cull distance, dormancy or `bOnlyRelevantToOwner` (SCL R13, ADR-0003 §3), so there is no real non-concealment cull; AC-ASN-45 exercises the defensive path through the test-only per-connection deny seam (`WITH_STEALTH_TEST_SEAMS`). If a pawn distance cull is ever introduced, SCL R13 and ADR-0003 must be amended first and AC-ASN-45's control revisited. Client-side registries (animation significance, audio, decals, pawn lists, Markers) must tolerate a relevancy destroy and re-create.

**Control Manifest Rules (this layer)**:
- Required: Every non-owner pawn actor is created presentation-hidden and evaluated in `PostNetInit`; clips are seeked, never restarted — source: ADR-0017
- Required: Client presentation goes through one idempotent `Refresh` called from every RepNotify, re-open and the host's direct path; a re-open blends in over ≤ 0.1 s with a blend weight that ramps over at least 2 frames — source: ADR-0018, GDD ASN-R21
- Forbidden: Never re-open a victim in the slot; never present a late actor's fall as a snap; nothing gameplay-visible distinguishes a cull from a real destroy — source: ADR-0018, ADR-0003
- Guardrail: One re-open path for every victim kind

When viewer W's channel to the victim is closed for a reason other than concealment, the record opens in the death-write tick or the next, with `FDeathState` in the initial block; W's presentation first commands `Death_Fall` seeked to `EstimatedServerNow() − DeathServerTime`, blended in over ≤ 0.1 s, identically for an NPC, a player and an informant victim. A viewer that receives the actor 0.6 s after `DeathServerTime` shows the fall seeked to 0.6 s with no pose pop; one that receives it 7.5 s after (past `DeathFallSec + BodyLingerSec`) never shows the body. On the listen-server host the fall starts on the same `ContactServerTime` schedule as on clients.

---

## Acceptance Criteria

*From GDD `design/gdd/assassination-defence-reversal.md` (AC-ASN-45, AC-ASN-25), scoped to this story:*

- [ ] **AC1** (AC-ASN-45, [Integration/BLOCKING][Net] `StealthGame.Assassination.CulledVictimOpensFalling`): **GIVEN** the defensive path of ASN-R10 / R21 (a victim whose channel to viewer W is closed for a reason other than concealment, exercised through the test-only per-connection deny seam) and W placed near the victim before the death write **WHEN** the victim is struck (an NPC, a player and an informant victim, as three runs) **THEN** the server send capture to W shows no record for the victim before the death-write tick; the record opens in the death-write tick or the next with `FDeathState` in the initial block; W's presentation log first commands `Death_Fall` seeked to `EstimatedServerNow() − DeathServerTime` (± 1 frame), blended in over ≤ 0.1 s with a blend weight that ramps over at least 2 frames, identically for the three victim kinds; CONTROL: with the seam released and no strike, W receives the victim's normal state; NEGATIVE CONTROL: a seam build that opens the record in the slot makes this test fail.
- [ ] **AC2** (AC-ASN-25, [Integration/BLOCKING][Net] `StealthGame.Assassination.LateActorSeeksNeverSnaps`): **GIVEN** a bystander client that receives the victim's actor 0.6 s after `DeathServerTime` **THEN** it shows the fall seeked to 0.6 s with no pose pop; **GIVEN** one that receives it 7.5 s after **THEN** the body is never shown; **AND** on the listen-server host the fall starts on the same `ContactServerTime` schedule as on clients.

---

## Implementation Notes

*Derived from ADR-0018 §3.2, §5.1 and ADR-0003 §5.8:*

Server side: the death-write's Visibility Policy hook (story 013 step 3) re-opens the pawn for the denied connection; no property on the victim changes before the death-write tick (story 013 AC3). Client side: `PostNetInit` evaluates `Refresh`, reading the sentinel defaults (`DeathServerTime = −1`, `Seq = 1`) to tell "no death yet" from a death; the seek uses `EstimatedServerNow()`; cross-fade ≤ 0.1 s (blend weight ramp at least 2 frames, no single-frame jump); past `DeathServerTime + DeathFallSec + BodyLingerSec` the actor is never shown (story 020's evaluator). Verification Required (2) is this story's net run: an actor first opened mid-fall joins at the right frame, one opened after the linger is never shown (ADR-0018 `ReopenFastForward`).

Use S10's reported facts (NetGUID reuse or not, AnimBP initialisation, destroy tolerance of client registries, the cull-then-un-cull race) to fix the harness's expectations; if S10 reports that the AnimBP pops on first frame, the seek and blend must be adapted here and ADR-0003 §5.8 / ADR-0018 amended by a superseding ADR if the contract changes.

**Prototype:** none (prototype has no relevancy cull of pawns and no fall seek).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-013-death-write-staging-and-after-latch.md`: the staged write and hook call.
- `story-023-concealed-body-stays-hidden.md`: killed-concealed victims (`bDiedConcealed`).
- IVP epic stories 004, 005, 028, 029: S9 cull close, S10 spike, blind attack on culled, un-cull lead bound.

---

## Files

- `Source/StealthGameplay/Private/Features/Assassination/EncounterPresentation.cpp` (`PostNetInit` evaluation, seek and blend; shared with stories 018 and 020)
- `Source/StealthTestHarness/Private/Assassination/CulledVictimOpensFallingTest.cpp` (`StealthGame.Assassination.PIE.CulledVictimOpensFalling`, with the per-connection deny seam)
- `Source/StealthTestHarness/Private/Assassination/LateActorSeeksNeverSnapsTest.cpp` (`StealthGame.Assassination.PIE.LateActorSeeksNeverSnaps`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

QL-STORY-READY: GAPS (blocked on S10; the spec is conditional on S10's reported facts)
- AC2: "no pose pop" and "same schedule" are not measurable; proposed rewrite (unconfirmed): a client receiving the actor 0.6 s after `DeathServerTime` shows `Death_Fall` seeked to 0.6 s ± 1 frame; the maximum per-frame root-relative bone-position delta over the first 10 frames is ≤ the clip's own maximum per-frame delta plus the blend allowance (numeric bound set once S10 reports AnimBP initialisation); a client receiving it 7.5 s after (more than 1.2 + 6.0 s) never has a visible primitive; the host's fall starts at `ContactServerTime` ± 1 frame, the same as a client's.
- AC1: adequate; it has numbers, a control and a negative control; state in the test header that the harness expectations depend on S10 (NetGUID reuse, AnimBP initialisation).

- **AC-1**: Culled victim opens falling
  - Given: the per-connection deny seam holds the victim's channel closed to viewer W (non-concealment), W placed near the victim; three runs (NPC, player, informant), a control with no strike, and a negative-control build that opens the record in the slot
  - When: the victim is struck
  - Then: the server capture to W has no record before the death-write tick; the record opens in the death-write tick or the next with `FDeathState` in the initial block; W's log first commands `Death_Fall` seeked to `EstimatedServerNow() − DeathServerTime` (± 1 frame), blended in over ≤ 0.1 s with a ramp of ≥ 2 frames, identically for the three kinds; the control shows normal state; the negative control fails the test
  - Edge cases: cull-then-un-cull race (S10); two viewers denied; the death-write tick coinciding with a travel; `PostNetInit` reading the sentinel defaults (`DeathServerTime = −1`)
- **AC-2**: Late actor seeks, never snaps
  - Given: a bystander client whose actor creation is delayed to 0.6 s and, in a second run, 7.5 s after `DeathServerTime`; the listen host
  - When: the actor is first received
  - Then: the 0.6 s case shows the fall seeked to 0.6 s with no pose pop (bound above); the 7.5 s case never shows the body; the host's schedule matches the clients'
  - Edge cases: arrival exactly at 1.2 s and at 7.2 s (boundaries); arrival during the removal grace; a re-open after the client registry destroys and re-creates the actor

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: `CulledVictimOpensFallingTest.cpp` and `LateActorSeeksNeverSnapsTest.cpp` (`LeakNetProfile`, `WITH_STEALTH_TEST_SEAMS`) — must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `story-013-death-write-staging-and-after-latch.md`, `story-018-strike-clip-variants-warp-and-contact-scheduling.md`, `story-020-client-fall-linger-and-occlusion-removal.md`; cross-epic (BLOCKING) `production/epics/information-visibility-policy/story-004-spike-s9-cull-latency-and-explicit-channel-close.md`, `story-005-spike-s10-reopen-behaviour-and-netguid.md`; `story-028-blind-spot-attack-on-culled.md`
- Unlocks: `story-023-concealed-body-stays-hidden.md`
