# Story 011: NPCs brace through the same path, NPCs never strike, and a brace is planted

> **Epic**: Assassination & Defence Reversal
> **Status**: Ready
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/assassination-defence-reversal.md` (ASN-R4, ASN-R7; Crowd Tell Matrix T4, T6; AC-ASN-15, AC-ASN-26, AC-ASN-27)
**Requirement**: `TR-asn-002`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0011: Pawn command interface and input pipeline (one command path for human and AI), with ADR-0018 §3.4 and ADR-0013 (one AnimBP)
**ADR Decision Summary**: Every body in the city is driven through the same seven server-validated commands, so nothing downstream of the input boundary can tell a human from an NPC; the crowd's restricted sink exposes only `Move` and `Look` plus the brace through `Prepare`, and has no `Attack` method.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0011 input layer; Crowd StateTree is HIGH in ADR-0016)
**Engine Notes**: The 10-simulated-minute crowd run (AC-ASN-27) uses a fixed timestep and fixed crowd seed on the reference scene, and counts `Attack` commands at the pawn command component's ingress for unbound pawns; the reflection check inspects the Crowd command emitter's sink interface. StateTree specifics are Crowd's (ADR-0016) and are not exercised here.

**Control Manifest Rules (this layer)**:
- Required: Crowd code reaches a pawn only through `FCrowdCommandSink`/`FCrowdMotionSink`, which call only `UPawnCommandComponent` entry points — source: ADR-0016
- Forbidden: Never give the NPC sink an `Attack` method (the restricted sink exposes only `Move` and `Look`); never give the resolver a controller, binding or kind term — source: ADR-0016, ADR-0018
- Required: The brace is planted: no translation and no yaw while `Bracing`, only head-look moves — source: GDD ASN-R4, SCL
- Guardrail: No extra per-pawn cost for NPC braces; same cooldown stamp component as players

Crowd issues real `Prepare` commands (Tell Matrix T4: flinch and startle braces); they enter through the IIF `Prepare` path with the same 60 s cooldown and planted stance, and a rival's strike on a braced NPC is a reversal exactly as on a braced player (same transition, same `OnPawnKilled` fields with `Killer` = the NPC pawn, same presentation and timings; only the null reply sink means no `FDefenceResult`; nothing is charged to anyone). NPCs never `Attack` (T6, rate 0): the strike is the intended confession, and this story proves the crowd has no path to it.

---

## Acceptance Criteria

*From GDD `design/gdd/assassination-defence-reversal.md` (AC-ASN-15, AC-ASN-26, AC-ASN-27), scoped to this story:*

- [ ] **AC1** (AC-ASN-26, [Integration/BLOCKING] `StealthGame.Assassination.NpcBraceSamePath`, Crowd T4): **GIVEN** an NPC brace issued by Crowd **THEN** it enters through the IIF `Prepare` path with the same 60 s cooldown and planted stance, and a rival's strike on it is a reversal exactly as on a braced player (same transition, same `OnPawnKilled` fields with `Killer` = the NPC pawn, same presentation and timings; only the null sink means no `FDefenceResult`; nothing is charged to anyone).
- [ ] **AC2** (AC-ASN-27, [Integration/BLOCKING] `StealthGame.Assassination.NpcsNeverStrike`, Crowd T6): **GIVEN** the reference crowd scene with 50 NPCs, a fixed crowd seed and 10 simulated minutes at a fixed timestep **THEN** 0 `Attack` commands are issued by any AI controller driving an unbound pawn; **AND** (Logic) the Crowd command emitter's command set contains no `Attack` path (reflection check).
- [ ] **AC3** (AC-ASN-15, [Integration/BLOCKING] `StealthGame.Assassination.PlantedBrace`): **GIVEN** a bracing pawn whose owner holds Move and sweeps Look for 4 s **THEN** server displacement is 0 cm and body yaw changes by 0°, while head-look follows input.

---

## Implementation Notes

*Derived from ADR-0011, ADR-0016 and GDD ASN-R4, ASN-R7:*

The resolver (stories 005, 007, 009, 010) has no controller, binding or kind term, so AC1 is largely a parity assertion over the same code: run the AC-ASN-03 scenario with an NPC defender and diff the resolution trace against the player run (transition, `OnPawnKilled` arguments, event log rows, presentation write sequence); the only allowed differences are the absent `FDefenceResult` (null sink) and Roster rows. The NPC cooldown honours the same `LastPrepareActivation` component (story 003); `ReleaseToCrowd` keeps the stamp.

AC3's planted brace is SCL's rule (`BracePlanted`, SCL story 015, AC-SCL-11) observed from the Assassination side: assert on the server capsule and body yaw, with head-look (`HeadLook`) still following Look within the clamp. Brace presentation is `Brace_In` (0.25 s) / `Brace_Hold` / `Brace_Out` (0.40 s), one asset set for NPC and player pawns.

The Crowd side (the herring that issues braces, story 021 in the Crowd epic; the command sink and its `Attack`-free interface, story 005 and 013 there) is built by the Crowd epic; this story supplies the acceptance test across both and the reflection check.

**Prototype replaced:** the prototype's brace is the sample's crouch (`SetDefenceStance`, known issue 6) and NPCs never brace; `StartDefence`/`StopDefence`/`SetDefenceStance` and the crouch tie-in are removed by story 027.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Crowd epic stories 021 (`brace-and-startle-herring`) and 005 (`command-sinks-epoch-and-halt-filter`): the NPC brace emitter and the sink.
- SCL story 015: the planted-brace movement rule itself.
- `story-010-reversal-counter-and-chained-strikes.md`: the reversal logic that the NPC case reuses.

---

## Files

- `Source/StealthTestHarness/Private/Assassination/NpcBraceSamePathTest.cpp` (CQTest PIE, `StealthGame.Assassination.PIE.NpcBraceSamePath`)
- `Source/StealthTestHarness/Private/Assassination/NpcsNeverStrikeTest.cpp` (`StealthGame.Assassination.NpcsNeverStrike`, plus the reflection check)
- `Source/StealthTestHarness/Private/Assassination/PlantedBraceTest.cpp` (`StealthGame.Assassination.PIE.PlantedBrace`)
- `Source/StealthGameplay/Private/Features/Assassination/EncounterService.cpp` (no code expected beyond what 003/007/010 add; list any parity fix here)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

QL-STORY-READY: GAPS
- AC1: "exactly as on a braced player" and "same timings" have no measurable parity definition or tolerance; proposed rewrite (unconfirmed): run the AC-ASN-03 scenario with an NPC defender and a player defender and diff the resolution trace (transition, `OnPawnKilled` arguments, event-log rows, presentation write sequence with `ServerTime` ± 1 tick); the only permitted differences are the absent `FDefenceResult` and Roster rows; the NPC cooldown end = `LastPrepareActivation` + 60 s exactly.
- AC2: the fixed timestep and scene id are not stated, and a zero count can pass vacuously; proposed rewrite (unconfirmed): state the fixed timestep (for example 1/60 s) and assert the run logs N > 0 other commands (`Move`/`Prepare`) from the same controllers, so 0 `Attack` is not an idle run.
- AC3: 0 cm and 0° have no tolerance, and the Look sweep and brace window are unspecified; proposed rewrite (unconfirmed): server capsule displacement ≤ 0.1 cm and body yaw delta ≤ 0.1° over 4 s, Look swept ±90° at a stated rate, head-look yaw following within its clamp; the brace window must cover 4 s + `Brace_In` 0.25 s, or the test re-braces and says so.

- **AC-1**: NPC brace same path
  - Given: a Crowd-issued NPC `Prepare` (T4), and the same scenario run with a player defender
  - When: a rival strikes the braced NPC
  - Then: the NPC `Prepare` enters through the IIF path with a 60 s cooldown and planted stance, the reversal trace equals the player trace except for the absent `FDefenceResult`, and nothing is charged to anyone
  - Edge cases: `ReleaseToCrowd` keeps the stamp; a second NPC brace during cooldown is rejected
- **AC-2**: NPCs never strike
  - Given: 50 NPCs, the fixed seed, 10 simulated minutes at a fixed timestep, unbound pawns
  - When: the run completes, and the Crowd emitter's sink is reflected
  - Then: 0 `Attack` commands at command-component ingress for unbound pawns, and the reflection finds no `Attack` method
  - Edge cases: non-vacuous run (> 0 other commands); a bound pawn's `Attack` is not counted
- **AC-3**: Planted brace
  - Given: a bracing pawn whose owner holds Move and sweeps Look for 4 s
  - When: 4 s elapse on the server
  - Then: displacement ≤ 0.1 cm, body yaw change ≤ 0.1°, head-look follows input
  - Edge cases: a Look sweep past the head clamp; an NPC pawn bracing; Move direction reversed mid-hold

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: the three test files above (PIE, `LeakNetProfile` where networked) — must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `story-003-defence-state-component-and-cooldown.md`, `story-010-reversal-counter-and-chained-strikes.md`; cross-epic `production/epics/shared-character-locomotion/story-015-brace-planted.md`; `production/epics/crowd-npc-behaviour/story-005-command-sinks-epoch-and-halt-filter.md`, `story-013-identity-and-command-path-gate.md`, `story-021-brace-and-startle-herring.md`
- Unlocks: `story-025-reversal-readability-and-brace-tell-playtest.md`
