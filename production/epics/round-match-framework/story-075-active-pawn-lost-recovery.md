# Story 075: A pawn destroyed without a kill is replaced once with the same AppearanceId through RequestEmergence(PawnRecovery)

> **Epic**: Round & Match Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-007` (disconnect policy: blend hand-over, guarded grace timer, forfeit/void; the pawn-loss recovery of R17.9 sits under this requirement); GDD AC 54a; rule R17.9 (an `Active` participant left without a pawn), Roster I4 recovery.
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0010: Shared Pawn: CMC locomotion, spawn factory and possession parity
**ADR Decision Summary**: One `AStealthPawn` on CMC, `UStealthPawnFactory` as the only creator and possessor of pawns, kind-blind possession and hand-over pass, disconnect never destroys a pawn. This story applies the factory-only creation rule to Round's own-card respawn: Round never spawns; it asks Crowd's replacement path, which spawns one pawn of the shared class with the participant's current `AppearanceId` through `RequestEmergence(Kind = PawnRecovery)` and the factory (ADR-0002 section 8.7 (5) carries the policy and caps).
**ADR Version**: ADR-0010 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0010 knowledge risk)
**Engine Notes**: The real respawn spawns a CMC pawn inside `TG_PostPhysics` from a resolver slot: ADR-0010 verification (and ADR-0002 E4/E14) lists "spawning a Shared Pawn inside `TG_PostPhysics` behaves" as a [U] spike item, and destruction is always deferred through `RequestTeardown`. This unit test does not touch it: the spawn is a fake Crowd/Level Socket gateway call and the "destroyed pawn" is a test hook. No post-cutoff API is used. Possession parity items (S8, S14, H1, P1) are not exercised.

**Control Manifest Rules (this layer)** (pawn rules in the Core Layer Rules, section Shared Pawn, movement and possession; Round policy in the Foundation Layer Rules):
- Required: Every pawn is created by `UStealthPawnFactory::SpawnSharedPawn`: deferred spawn, `ApplyAppearance`, `BindPawn` (if a participant), `FinishSpawning`, possess, in one call stack (ADR-0010).
- Forbidden: No pawn may be destroyed inside a resolver except through `RequestTeardown`; `Reset` must never destroy a Shared Pawn (ADR-0010, ADR-0002).
- Guardrail: Layout (server, `Active`): no navmesh rebuild; runtime services at most 0.1 ms average and 0.5 ms in a respawn-pick frame, at most 1 pick per frame (ADR-0014).

**Prototype disposition**: none (new Round Core code). The prototype's `RestartPlayer` round-start path is replaced by the factory (ADR-0010) in stories 039/040, not here.

---

## Acceptance Criteria

*From GDD `design/gdd/round-match-framework.md` AC 54a, scoped to this story:*

- [ ] GIVEN an Active, Connected participant whose pawn is destroyed by a test hook without a kill, THEN in the next drain `PawnLost` is logged, the participant stays Active, exactly one new pawn with the same `AppearanceId` is bound through `RequestEmergence(PawnRecovery)` at a socket outside non-team sight, the total pawn count is restored, no evaluation runs, and `GetUnboundActiveParticipants()` is empty at the end of every later drain.

---

## Implementation Notes

*Derived from GDD R17.9 and ADR-0002 section 8.7 (5); pawn-side rules from ADR-0010 Implementation Guidelines:*

- **Detection.** The Roster raises `OnBindingLost(P, PawnDestroyed)` when a `BoundLive` binding of an `Active` participant clears without a resolved kill (KillZ, streaming, a pool recycle or a bug). Round also asserts at the end of every drain that `GetUnboundActiveParticipants()` is empty. Open: confirm against the Roster contract that `OnBindingLost` exists (GDD R17.9 lists it as a change request to the Roster).
- **Item.** On the event Round submits a `PawnLost(P)` item (closed `PawnLost` item type, payload `FPawnLostPayload { FParticipantId Participant; FPawnHandle Pawn; }`, ADR-0002 section 2a) so the work runs in a slot with a key.
- **In the slot.** Log `PawnLost(P, cause)`; cancel P's in-flight hold or encounter (queued items void at re-validation); keep P `Active` (no transition, no new Reason, no elimination evaluation); request an own-card respawn: Crowd's replacement path calls `RequestEmergence(Kind = PawnRecovery)` at an emergence socket out of every non-team viewer's line of sight (Level Socket Contract emergence rules), the pawn is created through `UStealthPawnFactory::SpawnSharedPawn` with P's current `AppearanceId`, then `BindPawn`, then possess if P is `Connected` (or AI-drive if not, story 076's case).
- **Not a takeover.** Decided rule C1 (informant respawn by taking a crowd NPC) is untouched; the card count is unchanged (Character Pool CP-11) and the total pawn count is restored. Round never reads player-ness when choosing sockets (R23 (a)).
- **Cap and retry (story 076).** `PawnLossRecoveryMaxCount` (2) per participant per round, one retry per second up to `LateJoinTakeoverTimeoutSec` when no socket is eligible; both live in story 076. This story covers the first recovery only.
- **Invariant.** After every drain `GetUnboundActiveParticipants()` is empty. The test asserts it at the end of the drain that recovers and at the end of several later drains.
- **Teardown.** The "destroyed" pawn in the test is removed by a test hook (not a kill). In production a resolver never destroys an actor inline; teardown goes through `RequestTeardown` (ADR-0002) and a pawn pending teardown is treated as gone in later slots.
- Manifest 2026-10-25f: `ClearBinding` emits no event of its own and raises only `OnBindingLost` (taking an `EventSeq`) when its R9 condition holds.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 042 (`production/epics/round-match-framework/story-042-active-gate.md`): Active entry, the state this story needs (this story's dependency).
- Story 076 (`production/epics/round-match-framework/story-076-pawn-lost-void.md`): the third loss and no-socket timeout (`Void(PawnLost)`), the pawnless Disconnected case and no recovery in `Ended` or `Reset`.
- Story 039 (`production/epics/round-match-framework/story-039-spawn-one-frame-and-gate.md`): round-start spawning.
- Story 087 (`production/epics/round-match-framework/story-087-takeover-no-pawn-property-delta.md`): the wire-level check that a pawn-loss respawn changes no identity-bearing property (Blocked on spikes S8, H1 and OQ11(a)).
- Penalty's informant re-pawn (R17.9: an `Informant` whose pawn is lost goes to Penalty), Crowd's `RequestEmergence` internals, the Level Socket Contract's socket rules and the factory (other epics).

---

## Files

*Planned files only. Tests live under the engine test roots (`tests/README.md`), not under `tests/`. `/dev-story` replaces this list with the files it actually changed.*

- `Source/StealthGameplay/Public/Round/Disconnect/PawnLossRecovery.h`
- `Source/StealthGameplay/Private/Round/Disconnect/PawnLossRecovery.cpp`
- `Source/StealthGameplay/Private/Tests/Round/ActivePawnLostRecoveryTest.cpp` (class `FRoundActivePawnLostRecoveryTest`, test name `StealthGame.Round.ActivePawnLostRecovery`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: A pawn destroyed without a kill is replaced once with the same AppearanceId through RequestEmergence(PawnRecovery).
  - Given: an Active, Connected participant with a bound pawn, and a test hook that destroys it without a kill.
  - When: the next drain runs.
  - Then: `PawnLost` is logged and the participant stays Active.
  - Then: exactly one new pawn with the same `AppearanceId` is bound through `RequestEmergence(PawnRecovery)` at a socket outside non-team sight.
  - Then: the total pawn count is restored and no evaluation runs.
  - Then: `GetUnboundActiveParticipants()` is empty at the end of every later drain.
  - Edge cases: the fake Level Socket supplies one eligible socket.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Round/ActivePawnLostRecoveryTest.cpp` (automation test `StealthGame.Round.ActivePawnLostRecovery`, class `FRoundActivePawnLostRecoveryTest`) — must exist and pass.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-003-test-seam-fake-gateways.md`, `production/epics/round-match-framework/story-042-active-gate.md`
- Unlocks: `production/epics/round-match-framework/story-076-pawn-lost-void.md`
- Cross-epic: `production/epics/crowd-npc-behaviour` (`RequestEmergence(PawnRecovery)` replacement path; stories not yet created), `production/epics/level-socket-contract` (emergence sockets out of non-team sight; stories not yet created), `production/epics/shared-character-locomotion` (factory `SpawnSharedPawn`; stories not yet created), `production/epics/participant-state-roster` (`OnBindingLost`, `GetUnboundActiveParticipants`; stories not yet created), `production/epics/character-pool-appearance` (current `AppearanceId` per participant; stories not yet created)
