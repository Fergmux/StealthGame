# Story 006: The exiled body becomes an ordinary NPC and leaves through a scheduled exit slot

> **Epic**: NPC-Kill Penalty & Informants
> **Status**: Ready
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/npc-kill-penalty-informants.md`
**Requirement**: `TR-pen-002`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0019: Informant lifecycle and crowd-pawn takeover (§2.3 steps 2-5; Crowd side in ADR-0016, card pairing in ADR-0012)
**ADR Decision Summary**: After the exile the surviving body is `Unbound` and is an NPC: its card is `NpcLive` with the same `AppearanceId`, Crowd adopts it with a restricted driver until the strike lockout ends and `bExitAtNextScheduledSlot = true` (the prompt exit takes the next scheduled city-wide exit slot instead of adding one), and `PickRespawnPawn` never returns it for the exiled participant (`ExcludePawn`), even with LSC eligibility stubbed true.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: No engine API in Penalty's code for this story; the checks run through the Roster, Crowd, Character Pool and LSC seams. The released pawn's autonomous-to-simulated role switch mid-clip is the HIGH-risk part (ADR-0010, spike H1, observed visually in story 019); here only logical state is asserted.

**Control Manifest Rules (this layer)**:
- Required: the exile runs `RequestTransition(StrikeLimit)`, `ReleaseCard`, `AdoptReleasedPawn` and `RequestHandOver(Ai)` in one call stack — source: ADR-0019
- Forbidden: never leave a surviving exiled body `Retired`; never add an informant-only region or collision rule — source: ADR-0019
- Guardrail: no actor is created or destroyed by an exile — source: ADR-0019, ADR-0010

A released exile body that stays `Retired` while walking would turn the next strike on it into a `NoContact` whiff, a Pillar-1 tell. These checks prove it is indistinguishable from an NPC (`ResolveVictimKind` returns `Npc`), that it leaves the stage like any citizen, and that the exile adds no extra exit to the city's exit rate (a statistical tell).

**Prototype replaced:** none directly. The prototype keeps the exiled player's pawn as the informant (`ExilePlayer` sets the participant to informant on the same pawn), so there is no released body in the prototype to compare with.

---

## Acceptance Criteria

*From GDD `design/gdd/npc-kill-penalty-informants.md` (AC-NKP-06, AC-NKP-24, AC-NKP-45), scoped to this story:*

- [ ] **AC1** (AC-NKP-06, [I][Dep:Roster CR-3] `StealthGame.Penalty.ExileReleasesBody`): **GIVEN** an exile **WHEN** `ResolveVictimKind` is called on the exiled body immediately after the transition returns and again 1.75 s later **THEN** both return `Npc` (never `AlreadyResolved`); the body keeps its actor and `AppearanceId`, its card is `NpcLive`, and no actor is created or destroyed (PEN-R4.2).
- [ ] **AC2** (AC-NKP-24, [I][Dep:Crowd CR-4][Dep:LSC CR-5] `StealthGame.Penalty.ExileBodyExits`): **GIVEN** an exile **WHEN** the released body's lockout ends **THEN** its next leg is an `AmbientExit` with Crowd's give-up rule; **AND** `PickRespawnPawn` never returns it for the exiled participant, even with LSC eligibility stubbed true (`ExcludePawn`) (PEN-R5).
- [ ] **AC3** (AC-NKP-45, [L][Dep:Crowd CR-4] `StealthGame.Penalty.ExileExitUsesSlot`): **GIVEN** a fixed crowd seed and 30 simulated minutes run twice, once with 10 exiles injected and once with none **WHEN** `AmbientExit` events are counted city-wide **THEN** each exile body's exit occupies a scheduled exit slot (no extra exit is created) and the total exit count of the two runs is equal (PEN-R5).

---

## Implementation Notes

*Derived from ADR-0019 Decision §2.3 and the cross-ADR rows it cites:*

Penalty's part is small: it passes the right arguments in story 005's steps 2-3 and records `ExcludePawn`. This story proves the *composition*. `ResolveVictimKind(Pawn)` is Assassination's query over the Roster binding (`Unbound` -> `Npc`); the AC1 test calls it at t = 0 and t = 1.75 s on a fake clock and also asserts the actor identity (`FPawnHandle` equal), the `AppearanceId` and the card state through the Character Pool's server query, and that the actor spawn/destroy counters (the harness's world-actor counters) did not move.

For AC2, Crowd gives the released body its next ambient leg as `AmbientExit` after `RestrictedUntilServerTime`; Crowd's `CrowdExitGiveUpSec` give-up rule applies while it is watched. Penalty passes `ExcludePawn` on every `PickRespawnPawn` call for that participant (story 012), and the test stubs LSC eligibility true for the body to prove the explicit exclude backstop works rather than relying on Crowd's `AlleyExit` eligibility reason alone.

For AC3, use the Crowd test harness's fixed crowd seed and a simulated-time runner (30 simulated minutes, not wall clock). Count `AmbientExit` events through Crowd's calibration counter (`crowd-npc-behaviour` `story-018-tell-sampler-and-calibration-counters.md`); the exile must take the next scheduled slot of CNB-R9.1's cadence. The claim to prove is rate-neutrality: identical total exit counts with and without exiles.

Remember the tuning keys are data-driven: `AttackAttemptLockoutSec` (Assassination), `CrowdExitGiveUpSec` (Crowd); no literal in the tests except boundary values.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-005-exile-sequence-in-attack-slot.md`: the call stack that produces the released body.
- `story-019-takeover-and-exile-visual-continuity.md`: no snap, re-blend or head-look jump on the released body (visual evidence).
- `story-014-one-respawn-path-and-takeover-wire-parity.md`: the exiled *participant's* new body.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Private/Features/Penalty/StealthPenaltyService.cpp` (`ExcludePawn` plumbing, lockout argument)
- `Source/StealthTestHarness/Private/Penalty/ExileReleasesBodyTest.cpp` (CQTest, `StealthGame.Penalty.PIE.ExileReleasesBody.*`)
- `Source/StealthTestHarness/Private/Penalty/ExileBodyExitsTest.cpp` (CQTest, `StealthGame.Penalty.PIE.ExileBodyExits.*`)
- `Source/StealthGameplay/Private/Tests/Penalty/PenaltyExileExitUsesSlotTest.cpp` (class `FPenaltyExileExitUsesSlotTest`, name `StealthGame.Penalty.ExileExitUsesSlot`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC1** (AC-NKP-06, `StealthGame.Penalty.PIE.ExileReleasesBody`)
  - Given: an exile through the real Roster, Character Pool and a Crowd shell.
  - When: `ResolveVictimKind(body)` is called right after the transition returns and again at + 1.75 s.
  - Then: both return `Npc`, never `AlreadyResolved`; actor and `AppearanceId` unchanged, card `NpcLive`, world actor create/destroy delta 0.
  - Edge cases: a rival kill at + 0 and at + 1.75 gives `WrongTarget`, not `RivalEliminated`; the body is never left Retired.
- **AC2** (AC-NKP-24, `StealthGame.Penalty.PIE.ExileBodyExits`)
  - Given: an exile with `RestrictedUntilServerTime` set to the lockout end.
  - When: the lockout ends; separately `PickRespawnPawn(Team, seq, ExcludePawn)` is called with eligibility stubbed true.
  - Then: the next leg is `AmbientExit` under Crowd's give-up rule in the next scheduled exit slot; the pick never returns the exiled participant's own body.
  - Edge cases: `ExcludePawn` clears at embodiment.
- **AC3** (AC-NKP-45, `StealthGame.Penalty.ExileExitUsesSlot`, integration with a headless Crowd simulation)
  - Given: a fixed crowd seed, 30 simulated minutes, run twice with 10 exiles and with 0.
  - When: city-wide `AmbientExit` events are counted.
  - Then: each exile exit occupies a scheduled slot, no extra exit is created, total exits are equal across the two runs.
  - Edge cases: include two exiles inside one slot interval; if no headless Crowd simulation exists yet the test is deferred, not weakened.

QL-STORY-READY: ADEQUATE

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: the CQTest PIE tests under `Source/StealthTestHarness/Private/Penalty/` and the unit test under `Source/StealthGameplay/Private/Tests/Penalty/` OR a documented playtest

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/npc-kill-penalty-informants/story-005-exile-sequence-in-attack-slot.md`; cross-epic: crowd-npc-behaviour (`AdoptReleasedPawn`, `AmbientExit` with the exile slot, `story-018-tell-sampler-and-calibration-counters.md`), level-socket-contract (`PickRespawnPawn` with `ExcludePawn`), character-pool-appearance (`ReleaseCard`), participant-state-roster (`ResolveVictimKind` binding state), tuning-data-config `story-019-cqtest-headless-proof.md`
- Unlocks: `production/epics/npc-kill-penalty-informants/story-019-takeover-and-exile-visual-continuity.md`
