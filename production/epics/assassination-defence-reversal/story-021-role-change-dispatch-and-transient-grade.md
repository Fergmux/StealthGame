# Story 021: `OnRoleChangePresented` dispatch and the transient grade at the contact frame

> **Epic**: Assassination & Defence Reversal
> **Status**: Ready
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/assassination-defence-reversal.md` (ASN-R22, ASN-R23; VA-ASN-4; UI-ASN-5; AC-ASN-23)
**Requirement**: `TR-asn-005`, `TR-asn-008`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0018: Encounter data and outcome presentation timing (§5.2 own-role gating)
**ADR Decision Summary**: A client-local native multicast `FOnRoleChangePresented(ERoleChangeReason, LocalTime)` on the owning client's `UEncounterViewSubsystem` is fired by `Refresh` for the locally possessed pawn exactly once per `FDeathState`, at `DeathServerTime` (or on arrival when late); HUD, Camera, input-context swaps and owner audio subscribe to it and to nothing else, and outcome-caused owner data that arrives earlier is buffered by its consumer and released at this dispatch.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (networking, input and camera interplay under the 5.8 Enhanced Input / Common UI unification)
**Engine Notes**: The subsystem is `LocalPlayer`-scoped in `Public/View`; the listen host receives no RepNotify for its own writes, so its dispatch is driven by the same direct `Refresh` call (E7) and the same gate. Input-context swaps use ADR-0011's `IMC_Informant`; the camera consumes `Dying` / `ViewRelease` (ADR-0015, Camera story 018). `ApplyTransientGrade` is Camera's hook (Camera story 019); it must not time-dilate or shake (no hit-stop, slow motion or shake, CS-R11.1).

**Control Manifest Rules (this layer)**:
- Required: Own-role presentation subscribes only to `OnRoleChangePresented`; the exile has no own death and its clock is HUD's card-entrance event, not this dispatch — source: ADR-0018
- Forbidden: Never react to the arrival of the Roster's owner state in own-role presentation; never apply a grade before the local contact frame, on a bystander's or ally's camera, or on a whiff — source: ADR-0018, GDD VA-ASN-4
- Required: The Reduce-screen-effects setting applies to the grade — source: GDD VA-ASN-4
- Guardrail: Client-local, no server cost; no change to server tick rate or any pawn's simulation

Dispatch reasons: `Assassinated`, `ReversedAgainst`, `InformantKilled` (the owner's C-TeamView own `State` decides: Informant → `Assassinated`/`ReversedAgainst` per the transition the owner part records; already Informant → `InformantKilled`). Held until the dispatch: the owner's Roster state, `FHoldStatus` cancels, `NotNow` replies and `UDefenceOwnerComponent` changes. The grade (art bible 2.7: 0.12 s in, 0.20 s hold, 0.80 s out) is applied at the local contact frame to the attacker on a kill or a reversal, to a player victim (Active or Informant) before its CS-R12 fade, and to a bound defender on a reversal; never for a whiff, a brace, bystanders or allies.

---

## Acceptance Criteria

*From GDD `design/gdd/assassination-defence-reversal.md` (AC-ASN-23) and ADR-0018 Validation Criteria `RoleChangeGate`, scoped to this story:*

- [ ] **AC1** (AC-ASN-23, [Integration/BLOCKING] `StealthGame.Assassination.TransientGradeAtContact`): **GIVEN** a kill of a player, a reversal of a player attacker by a player defender, and a whiff **THEN** `ApplyTransientGrade` is called on the attacker's and victim's (or defender's) cameras on their local contact frames and never earlier, not on any bystander's or ally's camera, and not at all for the whiff; the server tick rate and every pawn's simulation are unaffected (no time dilation).
- [ ] **AC2** (ADR-0018 `RoleChangeGate`, [Integration/BLOCKING]): **GIVEN** a struck player on a remote client and on the listen host **THEN** own HUD, Camera and input-context changes occur no earlier than the pawn's own `DeathServerTime`; **AND WHEN** `FDeathState` arrives after `DeathServerTime` **THEN** the dispatch fires on arrival, exactly once per `FDeathState`, with the reason matching the owner's transition.

---

## Implementation Notes

*Derived from ADR-0018 §5.2 and GDD ASN-R23, VA-ASN-4:*

`Refresh` fires `OnRoleChangePresented` for the locally possessed pawn when `Now ≥ DeathServerTime` (never before) and records that it has fired for this `FDeathState` (idempotent: later `Refresh` calls from the per-frame tick or a re-open do not fire again). The dispatch is the single own-role presentation clock (HUD-R6a, `PresentedOwnState`). Consumers buffer their own data: HUD removal card, Camera `Dying` start, input-context swap, owner audio, and replies to the struck pawn's own later commands (rate-limited plates, `NotNow`) are held to the same time (ASN-R23). The reversed attacker's removal card, grade, plate and fall form one sequence under art bible 7.6 rule 7 (one-sequence budget).

Grade call sites: the attacker's camera at `FStrikeResult.ContactServerTime` (story 024 delivers the plate on the same frame), the victim's at its own `DeathServerTime`, the bound defender's at the Counter write's `ContactServerTime`. NPCs have no camera. The grade means "a body met a body", so a whiff plays none.

Prototype: none to promote; the prototype's client-side elimination HUD message fires on receipt (`MessagePlayer`), the behaviour this story removes the possibility of.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-024-owner-result-matcher-and-plate-timing.md`: result plates, the brace square end, AC-ASN-49.
- Camera epic stories 018 (`dying-hold-and-view-release`) and 019 (`grade-hooks`): the camera's own dying and grade machinery.
- HUD epic: removal card and widgets; Penalty epic: the exile card clock.

---

## Files

- `Source/StealthGameplay/Public/View/Features/Assassination/EncounterViewSubsystem.h` (`UEncounterViewSubsystem`, `FOnRoleChangePresented`, `ERoleChangeReason`)
- `Source/StealthGameplay/Private/Features/Assassination/EncounterViewSubsystem.cpp`
- `Source/StealthGameplay/Private/Features/Assassination/EncounterPresentation.cpp` (dispatch call and the per-`FDeathState` fired latch)
- `Source/StealthTestHarness/Private/Assassination/TransientGradeAtContactTest.cpp` (`StealthGame.Assassination.PIE.TransientGradeAtContact`)
- `Source/StealthTestHarness/Private/Assassination/RoleChangeGateTest.cpp` (`StealthGame.Assassination.PIE.RoleChangeGate`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

QL-STORY-READY: GAPS
- AC1: "local contact frames" has no tolerance, the fixture lacks bystander and ally clients, and "no time dilation" has no check; proposed rewrite (unconfirmed): `ApplyTransientGrade` is called on exactly the attacker's, victim's (or bound defender's) camera at its local `ContactServerTime` / `DeathServerTime` ± 1 frame, never earlier; on zero bystander or ally cameras and zero times for a whiff; global time dilation stays 1.0, server tick rate is unchanged, and pawn transforms match a no-grade run.
- AC2: "arrives after `DeathServerTime`" has no delay and the three reasons are not enumerated; proposed rewrite (unconfirmed): `FDeathState` arriving 300 ms after `DeathServerTime` dispatches once on arrival; reason is `Assassinated`, `ReversedAgainst` or `InformantKilled` per the owner's transition, one run each.
- The manifest requires the Reduce-screen-effects setting to apply to the grade but no AC covers it; proposed AC3 (unconfirmed): GIVEN Reduce-screen-effects on THEN the grade is suppressed or reduced as the art bible 2.7 accessibility clause states (the author must confirm which behaviour).

- **AC-1**: Transient grade at contact
  - Given: a PIE session with attacker, victim, defender, one bystander and one ally on separate clients, a spy on `ApplyTransientGrade`; kill, reversal and whiff scenarios
  - When: each strike resolves
  - Then: grade calls land only on participants' cameras at their local contact frame (± 1 frame) and never before; none on the bystander or ally, none for the whiff, no time dilation or tick change
  - Edge cases: NPC victim (no camera); informant victim; reversal with an NPC defender; high RTT where the result arrives after contact; host versus remote client
- **AC-2**: Role-change gate
  - Given: a struck player on a remote client and on the listen host, spies on HUD, Camera and input-context consumers; owner data arriving early in one run, `FDeathState` arriving 300 ms late in another
  - When: Roster owner state, `FHoldStatus` cancels and `FDeathState` arrive in each order
  - Then: no consumer changes before `DeathServerTime`; the dispatch fires exactly once per `FDeathState`, on arrival if late, with the matching reason
  - Edge cases: repeated `Refresh` from the tick or a re-open (no second fire); a new `FDeathState` after respawn fires again; host direct-`Refresh` path; the exile has no dispatch

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: `TransientGradeAtContactTest.cpp` and `RoleChangeGateTest.cpp` (PIE, remote client and listen host) — must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `story-013-death-write-staging-and-after-latch.md`, `story-018-strike-clip-variants-warp-and-contact-scheduling.md`; cross-epic `production/epics/camera-system/story-018-dying-hold-and-view-release.md`, `story-019-grade-hooks.md`; `production/epics/input-interaction-framework/story-071-context-swap-on-state-and-possession.md`
- Unlocks: `story-017-no-warning-and-no-observer-cue.md`, `story-024-owner-result-matcher-and-plate-timing.md`
