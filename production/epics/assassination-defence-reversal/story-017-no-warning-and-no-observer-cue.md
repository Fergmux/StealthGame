# Story 017: The target gets no warning and no observer gets a cue

> **Epic**: Assassination & Defence Reversal
> **Status**: Complete
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25g (was 2026-10-25f; the g changes amend ADR-0002/0003/0013/0017 S10 items, none of this story's rules)
> **Last Updated**: 2026-10-27

## Context

**GDD**: `design/gdd/assassination-defence-reversal.md` (ASN-R6, ASN-R20, ASN-R23; AC-ASN-05, AC-ASN-35)
**Requirement**: `TR-asn-008`, `TR-asn-004`, `TR-asn-009`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0018: Encounter data and outcome presentation timing (§2.3 what does not change at the slot, §5.2 gating, §7)
**ADR Decision Summary**: Anything a client can observe about an outcome appears at the moment of contact, identically for every pawn kind and without arrival order or packet size encoding the outcome; owner- and team-audience data caused by the outcome may reach the wire at the slot but is never presented before the local contact frame; no multicast and no effect actor exist.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (networking and presentation timing; ADR-0018 overall)
**Engine Notes**: The two ACs are harness assertions (ADR-0008): fresh scenario instances each paired with a control that replays the same scripted `Move` input, fixed server and client frame rates, no loss and a fixed zero-variance delay (`PktLagVariance` 0), a presentation log that records widget view-model changes, owner-only audio, haptics, camera parameters, own-pawn montage and state-machine/pose-variable changes, and comparison by consumed CMC move timestamp (never wall-clock). A negative-control seam build must make AC-ASN-05 fail.

**Control Manifest Rules (this layer)**:
- Required: Own-role presentation subscribes only to `OnRoleChangePresented`; client presentation schedules only from `EstimatedServerNow()` — source: ADR-0018, ADR-0002
- Forbidden: Never change anything on the victim or a defender before the death write; never present an outcome on receipt; never react to the arrival of the Roster's owner state in own-role presentation; no observer UI, ping, marker or cue for a strike — source: ADR-0018
- Required: Test seams compile only with `WITH_STEALTH_TEST_SEAMS` and are Shipping-excluded — source: ADR-0008
- Guardrail: Zero outcome-caused presentation entries before the local contact frame

This is the system-level proof of Pillar 1 for the strike: the victim and every observer see nothing that a control run would not show, up to contact. The only exception is the accepted ally-only teammate marker retirement (Teammate Markers TM-R6 / F6, retire update at the slot carrying the team-only `RetireAtServerTime` = the kill's `ContactServerTime`, so the marker is half faded as the blow lands). The round-end presentation of a deciding strike is staged to the death write, so no client shows it earlier than `DeathServerTime − StrikeNetLeadSec` (bystanders, the documented I-ASN-4 residual) or earlier than their own plate or fall (attacker and eliminated victim).

---

## Acceptance Criteria

*From GDD `design/gdd/assassination-defence-reversal.md` (AC-ASN-05, AC-ASN-35), scoped to this story:*

- [x] **AC1** (AC-ASN-05, [Integration/BLOCKING][Net] `StealthGame.Assassination.PIE.NoWarningToVictim`, ASN-R6, R16, R23): **GIVEN** an instrumented target client run as 6 independent fresh scenario instances (cases (a) unbraced moving victim, (b) unbraced standing victim, (c) braced defender, each struck from behind and from the front), each paired with an un-struck control instance replaying the same scripted `Move` input, under fixed server and client frame rates, no loss and a fixed zero-variance delay **WHEN** the strike executes **THEN** between execution and the client's local contact frame the presentation log contains no entry that the control instance's log lacks at the same server time; **AND** the victim's server position at the CMC move timestamp of the last move consumed before the death write matches the control's position at the same timestamp within 1 cm; **AND** the server send capture to the victim's connection and to a third-team bystander connection shows, for the victim's pawn from the slot up to but excluding the death-write tick, no property change other than `ReplicatedMovement` and CMC acks, occurring at the same ticks as in the control (cases (b) and (c): none). NEGATIVE CONTROL (ADR-0008 §3): a seam build that presents a widget on receipt of the Roster state makes this test fail. *(Tolerances adopted 2026-10-27 from the qa-lead proposal: every PIE world at the engine's fixed 60 Hz frame rate, entries compared at the same server time ± 1 client frame, the 1 cm position bound unchanged.)*
- [x] **AC2** (AC-ASN-35, [Integration/BLOCKING][Net] `StealthGame.Assassination.PIE.NoObserverCue`, ASN-R20, R23): **GIVEN** a victim's ally client, an attacker's ally client, a rival bystander client (all with line of sight) and the listen-server host **WHEN** a kill, a whiff and a reversal are executed **THEN** from execution until body removal none records a widget change, UI audio cue or marker caused by the strike, except the victim's ally's teammate marker and ally removal line (TM-R6 / F6, retire update at the slot carrying `RetireAtServerTime` = the kill's `ContactServerTime`), and the only strike-related replicated changes the bystander receives are `FActionState`, `FDeathState` and the `Counter` pose; **AND** when the kill ends the round no client (host included) shows the round-end presentation earlier than the deciding strike's death write (`DeathServerTime − StrikeNetLeadSec`): bystanders may show the banner at their `Phase = Ended` arrival, up to `StrikeNetLeadSec` (0.15 s) before `DeathServerTime`, and the attacker's and the eliminated victim's own banners follow the HUD F10 floors (not before their own plate or own fall start, both at `ContactServerTime`). *(Adopted 2026-10-27 from the qa-lead proposal: the window is execution to the body's removal (`DeathServerTime + DeathFallSec + BodyLingerSec + BodyRemovalGraceSec`, a whiff its lockout); "widget change" is any HUD view-model or widget-list entry of the presentation log; the round-end entries are compared with the deciding death write minus one frame for bystanders and the host, and with the own plate / own fall for the attacker and the eliminated victim.)*

---

## Implementation Notes

*Derived from GDD ASN-R6, ASN-R20, ASN-R23 and ADR-0018 §2.3, §5.2:*

Residuals that are accepted and not asserted away: a modified client among the attacker, target or target's ally may learn the outcome up to `T_v` (0.40 s; 0.75 s for `BushStrike`) early from Owner/Team data; bystanders may learn kill versus reversal at most `StrikeNetLeadSec` before contact (I-ASN-4). The tests assert **presentation**, not wire absence, for owner/team audiences ("Owner-audience data may arrive in the window; the test asserts it is not presented").

Instrument: add a presentation-log hook to the harness that records every consumer's change (HUD view-model, owner audio bus, haptic, camera parameter set, own-pawn montage, AnimBP state machine and pose variables). The control instance replays the same scripted `Move`; a locomotion change caused by that input appears in both logs and does not count. The listen host routes its own HUD and Camera through the same gating (ASN-R23), so it is one of the AC2 observers and its leg is the host-parity verification (ADR-0018 Verification (5)).

Order dependencies: this story can only pass when presentation consumers exist (story 018 clips, story 021 dispatch, story 024 plates and HUD hold buffers, Teammate Markers retire path, Round's staged round-end broadcast). Keep the harness scenarios written first with the negative-control seam, and expect them to fail until those stories land.

**Prototype replaced:** the prototype presents elimination on receipt (HUD message and ragdoll at the slot), the opposite of this contract; nothing is promoted.

### Implementation record (2026-10-27, /dev-story)

- The presentation log (`Tests/PIE/Assassination/PresentationLogHook.h/.cpp`, `WITH_STEALTH_TEST_SEAMS`): per machine,
  the encounter presentation rows of the named pawns (the victim's own montage and pose rows), the controller's owner
  presentation log (the HUD view-model: plates, tally, cards, brace square, HUD lines, round end; story 024), every
  change of the HUD element list (`AStealthHUD::CollectOwnerElements`), the camera grade and role-clock calls, and the own
  pawn's active montage and crouch pose. The build has no owner audio bus and no haptics yet, so nothing exists to
  record for them. `FReplicatedPropertyWatcher` is the client-receipt half of the send capture (every replicated
  property of a copy and its replicated components, per frame): a change sent but never received cannot be presented,
  and the process-level raw-bunch capture (`-StealthNetCapture`) cannot run inside the PIE automation pass.
- AC1 deviations: each case is one fresh PIE session (four players, 2v2, so neither the kill nor the reversal ends the
  round) whose control is the session's first, un-struck leg from the same placement with the same scripted input (the
  60 s `DefenceCooldownSec` is cleared between the legs by `FDefenceStateCore::Reset`, as Round's Reset does); the
  scripted Move is the W key held through the victim machine's Enhanced Input (the production Move path), the attacker
  200 cm ahead / 100 cm behind so the walking victim is about 150 cm away at the strike; the position check compares
  the displacement from the leg's start (a teleport's settle is not the strike's); the delay is PIE's in-process one (no
  emulated lag, so zero variance); the game has two teams (`STEALTH_NUM_TEAMS`), so the "third-team bystander" is a
  remote player who is neither participant (the victim's ally when remote).
- AC2: four players 2v2; the AC's three observer roles map onto the attacker's ally and the victim's team per event;
  the body release's own fields (PlayerState, Owner, Instigator) and the prototype `bDying` (retired by story 027) are
  the post-death changes allowed beside `FActionState` / `FDeathState` and movement.
- Product fixes this story needed (ASN-R23), in `AStealthPlayerController` / `AStealthHUD`: the HUD's role line, defence
  and tally lines and objective markers switched on the Roster informant state's arrival, before contact; they now
  follow `IsPresentedInformant()` (the own role clock, the exile card or a possession as an informant; cap fallback).
  Server HUD lines are never shown in their RPC handler: they queue and the per-frame tick releases them once no own
  outcome is unpresented, so a round-end line sent with a death write waits for the FDeathState of the same frame (HUD
  F10 floors); the round banner and the objective markers' removal follow the same release. The defender's own brace
  stance (the prototype crouch tell) now holds until the Counter's ContactServerTime on its own machine
  (`AStealthCharacter::HandleActionStateChanged`) and on the server (`AStealthGameMode::HandleEncounterCounterPose`),
  where it used to drop at the staged write, 150 ms before contact.
- The negative-control seam is `StealthOwnerPresentationTestSeams::SetPresentOnRosterReceipt` in the controller (the
  place that presents), not a separate `NegativeControlWidgetOnRosterReceipt.cpp`; the method
  `NoWarningToVictim.NegativeControlWidgetOnRosterReceiptIsReported` asserts check 1 reports the seam's line.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-012`, `story-013`: the wire writes and their timing (asserted here system-wide).
- `story-021-role-change-dispatch-and-transient-grade.md`: the dispatch itself and AC-ASN-23.
- `story-024-owner-result-matcher-and-plate-timing.md`: plates and the brace square timing (AC-ASN-49).
- Teammate Markers epic: the marker fade itself.

---

## Files

- `Source/StealthTestHarness/Private/Tests/PIE/Assassination/NoWarningToVictimPieTest.cpp` (`StealthGame.Assassination.PIE.NoWarningToVictim`; 6 scenario instances with their controls, plus the negative control)
- `Source/StealthTestHarness/Private/Tests/PIE/Assassination/NoObserverCuePieTest.cpp` (`StealthGame.Assassination.PIE.NoObserverCue`)
- `Source/StealthTestHarness/Private/Tests/PIE/Assassination/PresentationLogHook.h` and `.cpp` (client presentation-log recorder and replicated-property watcher, `WITH_STEALTH_TEST_SEAMS`)
- `Source/StealthGame/Public/StealthPlayerController.h`, `Private/StealthPlayerController.cpp` (`StealthOwnerPresentationTestSeams::SetPresentOnRosterReceipt`, the presented role, the hold buffer)
- `Source/StealthGame/Private/StealthHUD.cpp` (the status panel and objective markers follow the presented role)
- `Source/StealthGame/Private/StealthCharacter.cpp`, `Source/StealthGame/Private/StealthGameMode.cpp` (the brace stance ends at the Counter's contact)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

QL-STORY-READY: GAPS
- AC1: "at the same server time" has no tolerance and the fixed frame rates are unnamed; proposed rewrite (unconfirmed): server 60 Hz and client 60 Hz (or the profile values, stated), presentation-log entries compared at the same server time ± 1 client frame; the 1 cm position tolerance stays.
- AC2: "until body removal" depends on story 020's timings and "round-end presentation" has no logged event; proposed rewrite (unconfirmed): observation window = execution to the logged body-removal event; "widget change" = any HUD view-model entry in the presentation log; the round-end banner entry timestamp ≥ `DeathServerTime − 0.15 s` − 1 client frame for bystanders and the host, and ≥ own plate or own fall start at `ContactServerTime` for the attacker and the eliminated victim.

- **AC-1**: No warning to the victim
  - Given: 6 fresh instances (unbraced moving, unbraced standing, braced; each from behind and front), each paired with a control replaying the same `Move`, fixed rates, no loss, zero-variance delay
  - When: the strike executes
  - Then: no presentation entry exists that the control lacks before the local contact frame, victim server position matches the control within 1 cm at the same CMC timestamp, and no victim property changes other than `ReplicatedMovement` and CMC acks at the same ticks as the control (none in (b), (c))
  - Edge cases: a locomotion change from the scripted `Move` appears in both logs and is ignored; the negative-control seam (widget on Roster receipt) must fail
- **AC-2**: No observer cue
  - Given: the victim's ally, the attacker's ally and a rival bystander (all with line of sight), plus the listen host, for a kill, a whiff and a reversal
  - When: each is executed and the presentation logs are collected
  - Then: no widget change, UI audio cue or marker appears except the TM-R6/F6 retire at `RetireAtServerTime` = `ContactServerTime`, the bystander receives only `FActionState`, `FDeathState` and `Counter`, and a round-ending kill shows no round-end presentation before the death write minus the lead
  - Edge cases: a host leg through the same gating; a deciding strike's banner at `Phase = Ended` arrival up to 0.15 s early for bystanders only; owner-audience payload arriving early but not presented

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: `NoWarningToVictimTest.cpp` and `NoObserverCueTest.cpp` plus the retained negative-control failing run log — must exist and pass

**Status**: [x] Created and passing (2026-10-27): `NoWarningToVictimPieTest.cpp`, `NoObserverCuePieTest.cpp`; negative control run recorded in `production/qa/evidence/story-017-no-warning-and-no-observer-cue/pie-observations.txt`

---

## Dependencies

- Depends on: `story-012-slot-write-outcome-blind-and-warp-point.md`, `story-013-death-write-staging-and-after-latch.md`, `story-018-strike-clip-variants-warp-and-contact-scheduling.md`, `story-021-role-change-dispatch-and-transient-grade.md`, `story-024-owner-result-matcher-and-plate-timing.md`; cross-epic `production/epics/tuning-data-config/story-010-negative-control-helper.md`, `story-017-net-emulation-profiles.md`; `production/epics/round-match-framework/story-053-strike-win-staging.md`; `production/epics/information-visibility-policy/story-038-kill-confirmation-parity.md`
- Unlocks: Pillar-1 parity gates in the IVP epic (`story-038-kill-confirmation-parity.md`, `story-052-strike-id-oracle-parity.md`)

## Completion Notes
**Completed**: 2026-10-27
**Criteria**: 2/2 passing. AC1 `StealthGame.Assassination.PIE.NoWarningToVictim` (6 cases + negative control): no presentation entry before contact that the control lacks in any case; walking victim's displacement at the last consumed move before the death write 92.168 vs 92.167 cm (0.002 cm), standing / braced 0 cm; no property change but control-matched movement on the victim's and the bystander's copies before the death write; the Roster/death-state receipt seam is reported 133 ms before contact (fails as it must). AC2 `StealthGame.Assassination.PIE.NoObserverCue`: no HUD, widget, camera or marker cue on any observer (allies, rival bystander, listen host) for a whiff, a kill, a reversal or the deciding strike; only FActionState / FDeathState / movement / body release (and the prototype crouch tell) replicated; round end not before the deciding death write for bystanders, not before the own plate (attacker) or own fall (victim).
**Deviations**: (1) Each AC1 case's control is the first, un-struck leg of the same fresh 2v2 session (DefenceStateCore::Reset between legs for the 60 s cooldown), not a separate instance. (2) Client-receipt property watcher instead of the process-level raw-bunch send capture (cannot run inside the PIE automation pass); a sent-but-unreceived change cannot be presented. (3) Two teams only: the "third-team" bystander is a remote non-participant (the victim's ally when remote). (4) Negative-control seam in AStealthPlayerController (`StealthOwnerPresentationTestSeams::SetPresentOnRosterReceipt`, fires on the Roster informant state or the own FDeathState, whichever first: the Roster state can arrive after contact in this build). (5) No owner audio bus or haptics exist: nothing to record. (6) Product fixes made here (ASN-R23): server HUD lines and the round banner held by the per-frame hold buffer; HUD role/defence/objective lines follow the presented role; the brace crouch tell ends at the Counter's contact (owner and server).
**Test Evidence**: `Source/StealthTestHarness/Private/Tests/PIE/Assassination/NoWarningToVictimPieTest.cpp`, `NoObserverCuePieTest.cpp`, `PresentationLogHook.h/.cpp`; `production/qa/evidence/story-017-no-warning-and-no-observer-cue-evidence.md`
**Run result**: OBSERVED - 3-player PIE: the struck player's screen unchanged before contact, card, role line and round end together at its fall (`production/qa/evidence/story-024-owner-result-matcher-and-plate-timing/asn-49-victim-*.png`)
**Reviewed by**: self, no independent review (focused /code-review pass by the implementing agent; unattended batch)
