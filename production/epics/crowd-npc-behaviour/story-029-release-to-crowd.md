# Story 029: ReleaseToCrowd: invisible adoption, strike lockout and the exile exit slot

> **Epic**: Crowd & NPC Behaviour
> **Status**: Ready
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/crowd-npc-behaviour.md`
**Requirement**: `TR-crowd-007`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-crowd-007 = Participant-bound AI driving and takeover eligibility supply.)*

**ADR Governing Implementation**: ADR-0016: Crowd AI architecture
**Also relevant**: ADR-0019 (Penalty: exile releases the pawn in the Roster's own call stack, `AdoptReleasedPawn(Pawn, {})` reverses `TryReleaseNpc`), ADR-0010
**ADR Decision Summary**: `AdoptReleasedPawn(Pawn, FCrowdAdoptParams{RestrictedUntilServerTime, bExitAtNextScheduledSlot})` joins `ST_CrowdFull` at the pawn's next leg boundary (nothing visible changes at the release instant), or first finishes its strike lockout under `ST_CrowdRestricted` and then takes the next scheduled city-wide exit slot, reporting `AlleyExit` until absorbed. A concealed released pawn's remaining stay is drawn from the NPC stay distribution conditioned on the time already concealed (uncapped). `AdoptReleasedPawn(Pawn, {})` on a pawn the Director released by `TryReleaseNpc` and that was never bound exactly reverses it.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: Possession is unchanged by this story (ADR-0010's pass); the observer-visible requirement is continuity of velocity, heading and `ActionPose` across the release frame and the next ten frames.

**Why this story**: A pawn released to the crowd (grace expiry, informant disconnect, third NPC kill exile) must not visibly "become" an NPC at the release instant (T10 handoff rule), and an exiled body must leave through the same ambient exits as any citizen. Read GDD CNB-R10 items 4 and 5 and AC-CNB-24.

**Control Manifest Rules (Feature layer, manifest 2026-10-25e)**:
- Required: the driver kind comes from the `ICrowdControl` call (no Roster read); exile exit takes the next scheduled exit slot through the ambient schedule (ADR-0016); a cancelled schedule item resolves `Voided(Revalidation)`
- Forbidden: `Possess`/`UnPossess` from Crowd code; letting a participant re-adopt its own released body (`AlleyExit` eligibility) (ADR-0016, ADR-0019)
- Guardrail: adoption is O(1); no per-frame work beyond the ordinary tree

---

## Acceptance Criteria

*From GDD `design/gdd/crowd-npc-behaviour.md` AC-CNB-24 and CNB-R10 item 4, scoped to this story:*

- [ ] (AC-CNB-24) GIVEN a disconnected player's pawn at grace expiry, WHEN `ReleaseToCrowd` runs, THEN on an observer client in the release frame and the next 10 frames |dv| per frame never exceeds the SCL walk acceleration x dt, the heading rate never exceeds the SCL turn rate, `ActionPose` is unchanged, and the pawn's next behaviour starts at a leg boundary.
- [ ] With `RestrictedUntilServerTime` and `bExitAtNextScheduledSlot` the pawn finishes its strike lockout under `ST_CrowdRestricted`, then takes the next scheduled city-wide exit slot, reporting `AlleyExit` until absorbed.
- [ ] A concealed released pawn's remaining stay is drawn from the NPC stay distribution conditioned on the time already concealed (uncapped); `AdoptReleasedPawn(Pawn, {})` after `TryReleaseNpc` exactly reverses it (B decremented, sink unlatched, tree restarted from its current activity).

---

## Implementation Notes

*Derived from ADR-0016 sections 7 and 8 and GDD CNB-R10.4:*

- Strike removal (Penalty PEN-R4.2, PEN-R5): the pawn is released in the exile's own Roster call; it finishes its strike lockout (`AttackAttemptLockoutSec`) under the restricted driver, then takes the exit slot; the participant adopts a different pawn at `T_due` (Penalty PEN-R9).
- The exile takes the next scheduled exit slot (an ambient schedule item from story 026); the slot's original NPC simply stays. The Director tells a `TryReleaseNpc` reversal from a `ReleaseToCrowd` adoption by its own released-for-takeover set, never by a Roster read.
- Concealed released pawn: the stay already spent is a conditioning variable for the log-normal draw (`ECrowdDecision::BushStay`, retry-extended tuples if rejection sampling is used).
- `ReleaseControl` on an already-released pawn is a no-op (idempotence, ADR-0019 amendment).

**Prototype disposition** (`production/first-playable-report.md`, `Source/StealthGame`): None; the prototype exiles by removing the pawn (3rd NPC kill) with no crowd adoption. Not ported.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 028: bound driving and hand-back
- Story 030: `TryReleaseNpc` and informant takeover eligibility
- The Penalty epoch (`production/epics/npc-kill-penalty-informants/EPIC.md`): the exile sequence and `T_due`

---

## Files

- `Source/StealthGameplay/Features/Crowd/Private/CrowdAdoptReleased.cpp`
- `Source/StealthGameplay/Private/Tests/Crowd/CrowdReleaseToCrowdTest.cpp`
- `Source/StealthTestHarness/Private/CrowdReleaseNoChangeMPTest.cpp`

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY, 2026-10-25).*

- **AC-1**: `StealthGame.Crowd.ReleaseToCrowdNoChange`
  - Given: a disconnected player's pawn at grace expiry, and an observer client
  - When: `ReleaseToCrowd` runs
  - Then: in the release frame and the next 10 frames |dv| never exceeds SCL walk acceleration x dt; heading rate never exceeds the SCL turn rate; `ActionPose` unchanged; the next behaviour starts at a leg boundary
  - Edge cases: release at maximum speed
- **AC-2**: `StealthGame.Crowd.ExileExitSlot`
  - Given: `RestrictedUntilServerTime` set and `bExitAtNextScheduledSlot` true
  - When: the lockout ends
  - Then: the pawn finishes the lockout under `ST_CrowdRestricted`, takes the next scheduled city-wide exit slot, and reports `AlleyExit` until absorbed
  - Edge cases: the slot falls inside the lockout
- **AC-3**: `StealthGame.Crowd.AdoptConcealedAndReverse`
  - Given: a concealed released pawn with time already concealed t, and a `TryReleaseNpc` then `AdoptReleasedPawn(Pawn, {})` pair
  - When: the remaining stay is drawn and the adopt call runs
  - Then: remaining stay follows the uncapped NPC stay distribution conditioned on t; adopting after release restores the prior state exactly (B decremented, sink unlatched, tree restarted)
  - Edge cases: t beyond the old cap

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthGameplay/Private/Tests/Crowd/CrowdReleaseToCrowdTest.cpp` plus `Source/StealthTestHarness/Private/CrowdReleaseNoChangeMPTest.cpp` (observer-client check) — must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/crowd-npc-behaviour/story-026-ambient-schedule-and-emergence.md`, `production/epics/crowd-npc-behaviour/story-028-bound-pawn-restricted-driver.md`
- Unlocks: None
