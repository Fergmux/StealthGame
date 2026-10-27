# Story 019: Server body sequence (release, removal, possession through the fall) and Reset

> **Epic**: Assassination & Defence Reversal
> **Status**: Complete
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25f
> **Last Updated**: 2026-10-27

## Context

**GDD**: `design/gdd/assassination-defence-reversal.md` (ASN-R17, ASN-R22; Edge Cases "Bodies", "If `Reset` runs"; AC-ASN-24, AC-ASN-41)
**Requirement**: `TR-asn-007`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0018: Encounter data and outcome presentation timing (§6 body sequence), with ADR-0010 (`ReleaseBody` hand-over pass) and ADR-0002 (`BodyRelease`/`BodyRemoval` timer kinds, latch rejection)
**ADR Decision Summary**: For every victim kind alike, the body is released from its controller at `DeathServerTime + DeathFallSec` (collision off pawn channels, `UStealthPawnFactory::ReleaseBody`) and removed at `… + BodyLingerSec + BodyRemovalGraceSec` through queue timers (`IBodyRemovalObserver` then `RequestTeardown`), never by engine timers or by destroying a possessed pawn; possession stays put through the fall.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (possession and destroy paths; E9)
**Engine Notes**: Destroying a possessed pawn runs `AController::PawnPendingDestroy` (crowd controllers destroyed, PlayerControllers go `NAME_Inactive`); a plain `UnPossess` runs `APlayerController::OnUnPossess`, which calls `SetViewTarget(this)`; therefore `ReleaseBody` is the only release path and `AStealthPlayerController::OnUnPossess` keeps the view target when the unpossess comes from `ReleaseBody`, leaving Camera's `Dying → NoPawn` fade in control (ADR-0015 coordination). Controller, `Owner` and `PlayerState` are `COND_OwnerOnly`, so no non-owner sees the change. ([V] read in the 5.8.4 install: `Controller.cpp:421-438`, `Pawn.cpp:550-561`, `PlayerController.cpp:1282, 1353-1368`.)

**Control Manifest Rules (this layer)**:
- Required: Bodies are released from their controller at `DeathServerTime + DeathFallSec` and torn down after linger and grace through the queue, identically for every victim kind — source: ADR-0018
- Forbidden: Never release or remove bodies with engine timers or by destroying the possessed pawn; never re-possess a body; after the latch both items return `Rejected_Latched` (bodies lie through `Ended` and are destroyed by `Reset`/travel) — source: ADR-0018, ADR-0002
- Required: `IBodyRemovalObserver::OnBodyRemovalCommitted` is called for every body, whatever the kind or concealment state — source: ADR-0018, ADR-0017
- Guardrail: Two due-time-heap items per killed pawn

The kill slot schedules `TimerFired(BodyRelease, PawnHandle, Epoch)` and `TimerFired(BodyRemoval, PawnHandle, Epoch)`. Release turns the body's collision off pawn channels (it blocks no pawn after the fall) and commits `ReleaseBody` through the hand-over pass; removal runs the observer (Concealment clears the killed-concealed owner/team flags) then `RequestTeardown`. The victim's controller keeps the dying pawn until release, so informant respawn takeover (Penalty rule C1) and every ownership change happen after the visible fall.

---

## Acceptance Criteria

*From GDD `design/gdd/assassination-defence-reversal.md` (AC-ASN-24, AC-ASN-41) and ADR-0018 Validation Criteria `BodySequenceParity`, scoped to this story:*

- [x] **AC1** (AC-ASN-24, [Integration/BLOCKING] `StealthGame.Assassination.PossessionThroughFall`): **GIVEN** a killed player **THEN** its controller keeps the dying pawn until `DeathServerTime + DeathFallSec`, and no informant respawn takeover or ownership change happens before that time.
- [x] **AC2** (ADR-0018 `BodySequenceParity`, [Integration/BLOCKING]): **GIVEN** an NPC, a player and an informant victim **THEN** body release and removal times are identical (± 1 server frame) for the three, the observer is called once for each body, no `Spectating` state is entered, and after the latch both items return `Rejected_Latched` while the body persists until `Reset`.
- [x] **AC3** (AC-ASN-41, [Logic/BLOCKING] `StealthGame.Assassination.ResetClearsState`): **GIVEN** pending contact writes, a running cooldown, a lockout and bodies **WHEN** `Reset` runs **THEN** every stamp is `none`, every strike role is `Ready`, no dropped write fires afterwards, and no body remains.

---

## Implementation Notes

*Derived from ADR-0018 §6.1-§6.4 and GDD ASN-R17, ASN-R22:*

Release resolver: (a) collision off pawn channels (server), (b) `UStealthPawnFactory::ReleaseBody(Pawn)` committed in the slot and executed apply-only by the hand-over pass: crowd controller destroyed, PlayerController left pawnless in its normal playing state, never `Spectating`. Removal resolver: `IBodyRemovalObserver::OnBodyRemovalCommitted(Body)`, then `RequestTeardown(Pawn)`; a missing observer is a fault-sink report. `ETimerKind` gains `BodyRelease` and `BodyRemoval` (ADR-0002 amendment applied). `FTimerPayload.Subject` = the pawn handle. Clients have already hidden the body by VA-ASN-2 (story 020); Roster `ClearBinding` runs at `EndPlay`.

After the latch the server keeps a body's pawn collision on while clients have turned it off: accepted divergence, because the round-end step freezes every pawn's movement (ADR-0010 §4.5) so no human moves into a body during `Ended`. Same-look replacement beside a body is bounded by I-ASN-5w (story 001) and the replacement emerging out of every witness's sight (LSC R5). `Reset` clears every defence component, strike roles, pending staged writes (by epoch), body timers and bodies (they are removed with their pawns), consistent with Round's reset step (Round story 058).

**Prototype replaced:** the prototype's `BodyLifetime` timer and `Destroy` of the possessed ragdoll body in `StealthCharacter.cpp` (`KillCharacter` → delayed destroy) are replaced; retired by story 027.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-020-client-fall-linger-and-occlusion-removal.md`: client-side fall, linger and removal presentation.
- Penalty epic: the informant takeover after release, `T_due` floor.
- SCL / ADR-0010: the factory's `ReleaseBody` and hand-over pass mechanics (this story calls it).

---

## Files

- `Source/StealthGameplay/Public/Server/Features/Assassination/BodyRemovalObserver.h` (`IBodyRemovalObserver`)
- `Source/StealthGameplay/Private/Features/Assassination/EncounterService.cpp` (body timers' resolvers, `Reset`)
- `Source/StealthGameplay/Private/Tests/Assassination/ResetClearsStateTest.cpp` (class `FAssassinationResetClearsStateTest`)
- `Source/StealthTestHarness/Private/Assassination/PossessionThroughFallTest.cpp` (`StealthGame.Assassination.PIE.PossessionThroughFall`)
- `Source/StealthTestHarness/Private/Assassination/BodySequenceParityTest.cpp` (`StealthGame.Assassination.PIE.BodySequenceParity`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

QL-STORY-READY: GAPS
- AC1: "no informant respawn takeover or ownership change" has no observable and no tolerance is stated on the release time; proposed rewrite (unconfirmed): for a killed player (and separately a killed NPC and informant) `GetPawn()` still returns the dying pawn and no `Possess`, `UnPossess` or Penalty takeover call is recorded by a spy on the hand-over pass at any server tick before `DeathServerTime + DeathFallSec` (1.2 s); `ReleaseBody` is committed at that time ± 1 server frame.
- AC2: the latch half has no fixture ordering; proposed rewrite (unconfirmed): in a separate run the `Ended` latch is committed before the BodyRelease and BodyRemoval due times; then both timer items return `Rejected_Latched`, the observer is not called, collision stays on, and the body persists until `Reset`; in the non-latched run the observer is called exactly once per body; "no `Spectating` state" reads: the PlayerController's state name is never `Spectating` from kill through release, sampled every server frame.
- AC3: "every stamp", "every strike role" and "no dropped write fires afterwards" are unbounded; proposed rewrite (unconfirmed): `Reset` clears the stamp fields of all N pawns to `none` (enumerate them), all strike roles to `Ready`, and the body count to 0; advancing the queue clock past the latest pending epoch's due time plus 1 s produces zero applied writes and zero `TimerFired` resolver calls.

- **AC-1**: Possession through the fall
  - Given: a player victim, a spy on the hand-over pass and Penalty takeover seam, `DeathFallSec` 1.2 s
  - When: the kill is committed and the server steps frame by frame past `DeathServerTime + DeathFallSec`
  - Then: the controller holds the pawn until the release time (± 1 server frame), and no takeover or ownership change occurs earlier
  - Edge cases: a kill on the last tick before a travel or latch; a second strike on the same victim in the same slot; the informant victim has no early takeover; the host's own pawn
- **AC-2**: Body sequence parity and latch rejection
  - Given: three runs (NPC, player, informant) with identical kill geometry, plus a run where the latch is committed before the due times
  - When: the server runs through release and removal, then the latch is applied
  - Then: release and removal times match across the three kinds (± 1 server frame); the observer is called once per body; no `Spectating` state; after the latch both items return `Rejected_Latched` and the body persists until `Reset`
  - Edge cases: missing observer (fault-sink report); two victims in the same slot; the latch landing exactly on a due frame; removal time is `DeathFallSec + BodyLingerSec + BodyRemovalGraceSec`
- **AC-3**: Reset clears state
  - Given: a fixture with pending staged contact writes, a running cooldown, a lockout and N ≥ 2 bodies, built through factory functions
  - When: `Reset` runs
  - Then: all stamps are `none`, all strike roles are `Ready`, no later write or timer fires, and no body remains
  - Edge cases: `Reset` during a pending epoch's due frame; a stale-epoch write arriving after `Reset`; `Reset` with zero bodies; `Reset` twice

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: the three test files above — must exist and pass

**Status**: [x] Created and passing (2026-10-27)

---

## Dependencies

- Depends on: `story-003-defence-state-component-and-cooldown.md`, `story-007-attack-outcomes-resolved-in-slot.md`, `story-013-death-write-staging-and-after-latch.md`; cross-epic `production/epics/round-match-framework/story-058-reset-clears.md`, `story-052-ended-latch-effects.md`; ADR-0010 `UStealthPawnFactory::ReleaseBody` (SCL/Roster pawn factory stories); `production/epics/camera-system/story-018-dying-hold-and-view-release.md`
- Unlocks: `story-020-client-fall-linger-and-occlusion-removal.md`, Penalty epic informant takeover after release

## Completion Notes
**Completed**: 2026-10-27
**Criteria**: AC1 `StealthGame.Assassination.PIE.PossessionThroughFall.KilledBodiesKeepTheirControllerUntilReleaseAlike` (3 machines, sampled every server frame): an AI-driven NPC, a reversed player and an informant each keep their controller until DeathServerTime + 1.2 s and are released within one frame after it (no earlier change or takeover), the PlayerController never enters Spectating, the informant respawns only after the release (a new pawn). AC2: the same PIE run's release / removal times match across the three kinds within one frame and every body is removed at +9.2 s; `StealthGame.Assassination.BodySequenceParity` (real round queue + Round core): NPC, player and informant victims released at +1.2 s and removed at +9.2 s (+ one frame) through two TimerFired items, the observer once per body before the teardown; a round-deciding kill (latch in its own slot, before both due times) gets both items Rejected_Latched, no release, no observer, no teardown until Reset removes it; a missing observer is a DeadRegistration fault and the body is still removed. AC3 `StealthGame.Assassination.ResetClearsState`: two NPC bodies plus a reversed attacker's body (pending death writes and a CounterPose), a running cooldown (owner value), two lockouts: after Reset every stamp and owner cooldown is none, every role Ready, every body torn down once, the body generation moved on; past the latest due time + 1 s the stale writes apply nothing and all six body items, and the stale CapEnds, execute nothing; Reset twice and with zero bodies change nothing more.
**Deviations**: ADVISORY - (1) ADR-0010's ReleaseBody and hand-over pass are not built: minimal `UStealthPawnFactory::ReleaseBody` / `RequestTeardown` (control epoch bump, UnPossess, crowd controller destroyed; never Spectating), committed directly in the release item's slot; `AStealthPlayerController::OnUnPossess` keeps the view target during it. (2) Concealment is not built: STAND-IN no-op `FStealthPrototypeBodyRemovalObserver`. (3) The prototype ragdoll stays the fall presentation STAND-IN (`AStealthCharacter::BeginDeathFallPresentation` at DeathServerTime, no unpossess, no BodyLifetime, server capsule kept until release) until stories 018/020; the informant respawn is scheduled from the release (InformantRespawnDelay - DeathFallSec later) and clears the released body's binding. (4) The encounter service's ResetRound moved from Round's Active entry to Round's Reset phase. (5) AC2's "PIE.BodySequenceParity" is covered by the headless real-queue test plus the PIE.PossessionThroughFall session (one PIE session for both possession ACs; decision logged). Cross-epic Round 052/058, SCL ReleaseBody, Camera 018, Penalty takeover: proceeded.
**Test Evidence**: `Source/StealthGameplay/Private/Tests/Assassination/ResetClearsStateTest.cpp`, `BodySequenceParityTest.cpp`, `Source/StealthTestHarness/Private/Tests/PIE/Assassination/PossessionThroughFallPieTest.cpp`; full suite in `production/qa/evidence/assassination-defence-reversal-015-019-2026-10-27/full-suite.txt`
**Run result**: OBSERVED - 3-player PIE: bodies fall at contact, lie, and are removed about 9 s later; a killed informant respawns at its team spawn after the fall (`production/qa/evidence/story-019-server-body-sequence-and-reset/`)
**Reviewed by**: self, no independent review (focused /code-review pass by the implementing agent; unattended batch)
