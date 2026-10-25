# Story 011: Avoidance as Move offsets: yield rate, obstacles and the blocked ladder

> **Epic**: Crowd & NPC Behaviour
> **Status**: Ready
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/crowd-npc-behaviour.md`
**Requirement**: `TR-crowd-002`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-crowd-002 = StateTree per NPC (~4 Hz) + follower emitting Move/Look through the IIF; no path-following driving.)*

**ADR Governing Implementation**: ADR-0016: Crowd AI architecture
**Also relevant**: shared-character-locomotion GDD (R16 obstacles, OQ10 yield range)
**ADR Decision Summary**: Avoidance is expressed as a lateral `Move` offset read from the Director's 2 m spatial hash (excluding pawns the perception filter marks not-an-obstacle); the yield decision is drawn once when a predicted approach is first detected (`ECrowdDecision::Yield`) with a 0.2-0.6 s jitter as a sampled timer. No pawn is ever pushed; `bUseRVOAvoidance` is false for every pawn.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: RVO rewrites the requested velocity outside the input path (E8), which is why it stays off for every pawn class; avoidance is plain follower math. No post-cutoff API.

**Why this story**: Yielding is a classic human-versus-bot tell (a bot that never yields, or always yields, is identified at once). The GDD sets the yield share at 70 % for both partner kinds and requires the decision to be kind-blind. Read GDD CNB-R3 item 3 and AC-CNB-32.

**Control Manifest Rules (Feature layer, manifest 2026-10-25e)**:
- Required: avoidance is expressed as `Move` offsets (`bUseRVOAvoidance` false for every pawn); perception excludes pawns `ICrowdPerceptionFilter` marks not an obstacle, identically for every kind (ADR-0016)
- Forbidden: RVO, Detour Crowd, `RequestDirectMove`, any push of another pawn (ADR-0016)
- Guardrail: yield and avoidance work inside `Crowd.Follower`; reaction timers on the per-frame due-time list, not decision ticks

---

## Acceptance Criteria

*From GDD CNB-R3 item 3 and AC-CNB-32, scoped to this story:*

- [ ] (AC-CNB-32) GIVEN 2,000 scripted predicted approaches under 100 cm between an NPC and a walking pawn (half player-driven, half NPC), WHEN side-steps are counted, THEN the NPC yield share is `CrowdYieldPct` +-3 points for both partner kinds, the decision is taken at first detection, and no pawn is ever pushed.
- [ ] Avoidance is a lateral `Move` offset of up to 50 cm (side from relative position, keep-right tie-break, a per-pawn coin only on an exact head-on); `bUseRVOAvoidance` is false on the pawn class defaults (audit).
- [ ] Planted and tending pawns are static 50 cm obstacles plus margin; concealed and bush-transitioning pawns are ignored; blocked for more than 0.5 s widens the offset to 100 cm, blocked for 3 s repaths once, and after two failed repaths the NPC abandons the leg and draws a new destination; the 0.2-0.6 s jitter runs as a due-time entry.

---

## Implementation Notes

*Derived from GDD CNB-R3 item 3 (revised to the exact ladder) and ADR-0016 section 4 item 2:*

- Yield probability `CrowdYieldPct` (70 %, so non-yield is 30 %); the decision is taken once at first detection of a predicted closest approach below 100 cm, typically 3-5 m out, and is not re-rolled. The draw is kind-blind: the partner's kind is not an input.
- Progress monitor: less than 50 cm of net progress in 5 s counts as blocked. Blocked over 0.5 s widens to 100 cm; blocked 3 s repaths (through story 007's budget); after two failed repaths, or a failed or partial path query, abandon the leg. No runtime nav carving.
- The jitter is a due-time entry on the controller list (story 012 owns the list; until then a stub), never a decision-tick count or `FTimerManager`.
- Test fixture for AC-1 uses a scripted second pawn that walks a fixed approach vector; the player-driven half uses the harness bot's Enhanced Input path so both kinds present the same public facts.

**Prototype disposition** (`production/first-playable-report.md`, `Source/StealthGame`): None; the prototype used the engine's default CMC avoidance and path-following. Nothing is promoted.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 008: the hash and filter that supply obstacles
- Story 009: the base follower
- Story 012: the due-time list implementation
- Story 013: the 10-minute "no pawn pushed" soak inside the command-path gate

---

## Files

- `Source/StealthGameplay/Features/Crowd/Private/CrowdAvoidance.cpp`
- `Source/StealthGameplay/Features/Crowd/Public/Server/CrowdAvoidance.h`
- `Source/StealthGameplay/Private/Tests/Crowd/CrowdYieldTest.cpp`

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY, 2026-10-25). AC-3 revised after the GAPS verdict (the widen and repath ladder now carries the GDD values).*

- **AC-1**: `StealthGame.Crowd.YieldRate`
  - Given: 2,000 scripted predicted approaches under 100 cm (half player-driven, half NPC)
  - When: side-steps are counted
  - Then: NPC yield share is `CrowdYieldPct` +-3 points for both partner kinds; decision at first detection; no pawn pushed
  - Edge cases: exact head-on (per-pawn coin); approach detected at 3 m and at 5 m
- **AC-2**: `StealthGame.Crowd.AvoidanceAsMove`
  - Given: a crowd pawn class default object and a scripted approach
  - When: the offset and defaults are inspected
  - Then: lateral offset never exceeds 50 cm; keep-right tie-break on ties; `bUseRVOAvoidance` false
  - Edge cases: coin used only on an exact head-on
- **AC-3**: `StealthGame.Crowd.BlockedLadder`
  - Given: a pawn blocked by a static obstacle
  - When: blocked time passes 0.5 s, 3 s, and two repaths fail
  - Then: offset widens to 100 cm at 0.5 s; one repath at 3 s; after two failed repaths the leg is abandoned and a new destination drawn; `Crowd.Path.Blocked` raised once; jitter in [0.2, 0.6] s as a due-time entry
  - Edge cases: progress under 50 cm in 5 s counts as blocked; obstacle that moves away at 2.9 s

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Crowd/CrowdYieldTest.cpp` — must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/crowd-npc-behaviour/story-008-spatial-hash-and-perception-filter.md`, `production/epics/crowd-npc-behaviour/story-009-follower-shaping-pace-and-stops.md`
- Unlocks: `production/epics/crowd-npc-behaviour/story-013-identity-and-command-path-gate.md`
