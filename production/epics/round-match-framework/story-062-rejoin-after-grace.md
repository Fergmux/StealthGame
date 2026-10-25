# Story 062: A rejoin after grace expiry makes the participant an Informant re-pawned by the C1 takeover

> **Epic**: Round & Match Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-007` (Disconnect policy: blend hand-over, guarded grace timer, forfeit/void). Also `GDD AC 23b`, `R18` (second bullet, `Informant` in `Active` with no live pawn), decided rule C1 (informant respawn path), `R17.4`.
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0005: Session, seamless travel and match-scoped state stores; ADR-0010: Shared Pawn: CMC locomotion, spawn factory and possession parity
**ADR Decision Summary**: ADR-0005: `AGameModeBase` with four game-instance stores (Session, Roster, Round, Tuning preset) as the only authoritative match state, seamless `ServerTravel` with `?Seed=`, single round acknowledgement, admission modes, host commands on the game instance, no host migration; section 5: the rejoin passes Session's acknowledgement, and for a returning informant Round calls Penalty's `IInformantBodyProvider::OnInformantReadmitted` only after that acknowledgement. ADR-0010: one `AStealthPawn` on CMC, `UStealthPawnFactory` as the only creator/possessor, `TakeOverCrowdPawn` checks eligibility and calls `BindPawn` itself (never the caller), and no card move is committed unless it returned `Applied`. This story applies the Round side: it admits the returner as an `Informant` with no live pawn and starts the C1 path by `OnInformantReadmitted(P)`.
**ADR Version**: ADR-0005 2026-10-24; ADR-0010 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: ADR-0005's post-cutoff item (5.6 `DispatchPostLogin` deprecation) is not touched; its spike S4 (retained PlayerState adoption for a returning key) is not touched because the rejoin and acknowledgement are fake Session events. ADR-0010's post-cutoff items (`UMotionWarpingComponent::GetOwnerAdapter()`, MotionWarping Beta in 5.8.4) are not touched; its spikes (traversal, S8, S14, H1, P1) are not touched because the takeover chain runs against fake Crowd, Level Socket, Roster and Character Pool gateways. The real `TakeOverCrowdPawn` and its possession behaviour belong to the shared-character-locomotion epic and its spikes.

**Control Manifest Rules (this layer)** (the Round-relevant required and forbidden rules are from the Core Layer Rules, Shared Pawn and possession; the guardrail is from the Foundation Layer Rules):
- Required: `TakeOverCrowdPawn` checks eligibility and calls `BindPawn` itself; no card move is committed unless it returned `Applied` (ADR-0010).
- Forbidden: Never call `BindPawn` from a takeover caller (ADR-0010); Round and Penalty never call it for the takeover.
- Guardrail: Queue drain + staging + teardown + Roster flush: at most 0.5 ms listen-host game thread (ADR-0002, ADR-0022).

**Prototype disposition**: none (no prototype equivalent: the prototype has no informant respawn and no rejoin).

---

## Acceptance Criteria

*From GDD `design/gdd/round-match-framework.md` AC 23b, scoped to this story:*

- [ ] GIVEN a rejoin after grace expiry within the window during Active, THEN the participant is an Informant and is re-pawned by the C1 takeover (fake Crowd: one NPC deregistered, `BindPawn`, `RecordAppearance` with that pawn's card, pawn count unchanged).

---

## Implementation Notes

*Derived from GDD R18, R17.4 and ADR-0005 section 5:*

- State at the rejoin (R17.4, R18): the grace expired, `RequestTransition(P, Informant, Disconnect, ...)` was `Applied`, and the pawn was released to the crowd (`ReleaseToCrowd`, the pawn is an NPC with the same look). `ShouldAdmitExpired(P)` is true for an `InRound` record in `Active` (story 073), so Session admits the returner within the rejoin window. The Roster state stays `Informant` on return ("reconnecting never changes state", R18); the returner is admitted with no live pawn ("awaiting pawn", Roster R11) and sees a neutral view, not the engine spectator state.
- Round's action (R18): inside its slot, only after the returner's Session acknowledgement (when its PlayerController is ready), Round calls `IInformantBodyProvider::OnInformantReadmitted(P)`, an interface Round declares and the composition root binds to Penalty (ADR-0019 section 1). Penalty owns the respawn and its retries (PEN-R10, LSC R5b.5 widening); `LateJoinTakeoverTimeoutSec` and its view-only fallback do not apply to informants.
- C1 chain (Penalty's PEN-R9 order, ADR-0019 section 4; the fake chain this test runs): the Level Socket Contract `PickRespawnPawn` (R5b) picks an unseen eligible crowd NPC in the team's respawn region (out of line of sight of all non-team viewers; NPCs mid-interaction, concealed or braced are not eligible); Crowd releases it; Shared Character's `TakeOverCrowdPawn` (R2a) checks eligibility and calls Roster `BindPawn` on Penalty's behalf; only if it returned `Applied`, the taken pawn's existing card is moved by `TakeNpcCard(ParticipantId, Pawn)` (a card move that leaves `AppearanceId` untouched, Character Pool CP-12b), then `RecordAppearance`; never a spawn. In the AC's fake Crowd terms: one NPC deregistered, `BindPawn`, `RecordAppearance` with that pawn's card, and the pawn count unchanged (the returner's old pawn was released to the crowd, so taking one back leaves the total pawn count and the NPC count unchanged from before the departure, and no extra I-2 check is needed).
- What this story asserts versus fakes: assert (1) the participant's Roster state is `Informant` after the rejoin, (2) `OnInformantReadmitted(P)` is called exactly once, after the acknowledgement and inside Round's slot, (3) the fake Crowd and pool record, in order, one NPC deregistered, one `BindPawn` (not called by Round or by the test's Penalty stand-in; it is called by the faked `TakeOverCrowdPawn`), then one `RecordAppearance` with the taken pawn's card, (4) the total pawn count and the NPC-plus-bound count are unchanged. The Penalty stand-in is a fake `IInformantBodyProvider` that executes the PEN-R9 order against the fake gateways (story 003). Open: confirm against ADR-0019 section 4 and the Penalty GDD (PEN-R9, PEN-R10) the exact ordering and names the fake must follow; the real chain is Penalty's, Level Socket's and Shared Character's to test.
- No eligible NPC in the respawn set is story 074 (neutral view, re-pawn handed to Penalty, no Round timer).
- Where it lives: the rejoin handler in the Round Core disconnect policy (planned `Source/StealthGameplay/Private/Round/Disconnect/`), and the `IInformantBodyProvider` declaration in a Round-owned header under `Source/StealthGameplay/Public/Round/` (a consumer-owned interface wired by the composition root, ADR-0007).
- Manifest 2026-10-25f: a takeover keeps the `COND_InitialOnly` `AppearanceId` already on the client (no re-send, no `OnRep`); it is re-sent only after a relevancy re-open.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 060 (`production/epics/round-match-framework/story-060-grace-timeline.md`): the grace timeline and the conversion that precedes this rejoin.
- Story 061 (`production/epics/round-match-framework/story-061-rejoin-inside-grace.md`): a rejoin inside grace (the Active participant keeps the pawn).
- Story 073 (`production/epics/round-match-framework/story-073-should-admit-expired-table.md`): the `ShouldAdmitExpired` table.
- Story 074 (`production/epics/round-match-framework/story-074-readmit-retry.md`): no eligible NPC, retry handed to Penalty.
- Penalty's respawn scheduling and retries, Level Socket Contract `PickRespawnPawn`, Shared Character `TakeOverCrowdPawn`, Character Pool `TakeNpcCard` and `RecordAppearance`, Crowd release (their epics: the Core stories here use fakes).
- Session's acknowledgement and PlayerState adoption (session-travel epic).

---

## Files

*Planned files. Tests live under the engine test roots (`Source/<Module>/Private/Tests/`, see `tests/README.md`), not under `tests/`. `/dev-story` replaces this list with the files it actually changed.*

- `Source/StealthGameplay/Private/Tests/Round/RejoinAfterGraceTest.cpp` (class `FRoundRejoinAfterGraceTest`, test name `StealthGame.Round.RejoinAfterGrace`)
- `Source/StealthGameplay/Public/Round/` `IInformantBodyProvider` declaration (new; Open: header name set when implemented)
- `Source/StealthGameplay/Private/Round/Disconnect/` informant re-admission handler (new)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: A rejoin after grace expiry makes the participant an Informant re-pawned by the C1 takeover.
  - Given: a participant whose grace expired, with a fake Crowd holding an eligible NPC.
  - When: it rejoins within the window during Active.
  - Then: the participant is an Informant.
  - Then: the fake Crowd records one NPC deregistered, `BindPawn`, and `RecordAppearance` with that pawn's card.
  - Then: the pawn count is unchanged.
  - Edge cases: the no-eligible-NPC case is story 074.

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Round/RejoinAfterGraceTest.cpp` — must exist and pass (automation test `StealthGame.Round.RejoinAfterGrace`, class `FRoundRejoinAfterGraceTest`)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-060-grace-timeline.md`
- Unlocks: `production/epics/round-match-framework/story-074-readmit-retry.md`

Cross-epic: `production/epics/session-travel` (rejoin admission, acknowledgement; stories not yet created); `production/epics/participant-state-roster` (`Informant` state, `BindPawn`; stories not yet created); `production/epics/level-socket-contract` (`PickRespawnPawn`, faked here; stories not yet created); `production/epics/character-pool-appearance` (`TakeNpcCard`, `RecordAppearance`, faked here; stories not yet created); `production/epics/shared-character-locomotion` (`TakeOverCrowdPawn`, faked here; stories not yet created); `production/epics/crowd-npc-behaviour` (release of the taken NPC, faked here; stories not yet created); `production/epics/tuning-data-config` (module skeleton and StealthTestHarness bootstrap; stories not yet created)
