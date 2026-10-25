# Story 074: With no eligible NPC the returner is admitted with a neutral view and the re-pawn is handed to Penalty with no timer of its own

> **Epic**: Round & Match Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25f
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-007` (disconnect policy: blend hand-over, guarded grace timer, forfeit/void); GDD AC 53; rules R18 (re-admission after expiry, informant path), R17.9 (Round's own timer use is limited to `Active` pawn recovery).
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0005: Session, seamless travel and match-scoped state stores
**ADR Decision Summary**: `AGameModeBase` with four game-instance stores (Session, Roster, Round, Tuning preset) as the only authoritative match state, seamless `ServerTravel` with `?Seed=`, single round acknowledgement, admission modes, host commands on the game instance, no host migration. This story applies Round's re-admission policy for an expired returner (ADR-0002 section 8.7 (3)): the informant is admitted, sees a neutral view, and the re-pawn belongs to Penalty, which owns the respawn and its retries; Round starts none of its own.
**ADR Version**: ADR-0005 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0005 knowledge risk)
**Engine Notes**: None touched. The test uses a Penalty stub and a fake Level Socket gateway; no engine API, post-cutoff API or verification item (spikes S3, S4 retained-PlayerState reconnect; ADR-0010 S14) is exercised. "Never the engine spectator state" is asserted on a fake record flag, not on a real `APlayerState`.

**Control Manifest Rules (this layer)** (Round policy rules sit in the Foundation Layer Rules):
- Required: A rejoiner adopts its retained PlayerState in `InitPlayerState`; `OnDeactivated` does not destroy the PlayerState while admission is `RejoinOnly` (ADR-0005).
- Forbidden: Never use `FTimerManager`, a feature tick or a latent action for a gameplay timer (ADR-0002); Round must not add a retry timer here.
- Guardrail: Session: silence clocks and ack bookkeeping inside the 0.5 ms line (ADR-0005).

**Prototype disposition**: none (new Round Core code).

---

## Acceptance Criteria

*From GDD `design/gdd/round-match-framework.md` AC 53, scoped to this story:*

- [ ] GIVEN no eligible NPC in the team's respawn set, THEN Round admits the returner with a neutral view, never in the spectator state, hands the re-pawn to Penalty (stub) and starts no `LateJoinTakeoverTimeoutSec` timer of its own.

---

## Implementation Notes

*Derived from GDD R18 and R17.9, and ADR-0002 section 8.7 (3):*

- **Context.** After grace expiry the participant is an `Informant`; a returner in `Active` with no live pawn ("awaiting pawn", Roster R11) is re-pawned by Penalty through the C1 informant respawn path in Penalty's PEN-R9 order: the Level Socket Contract `PickRespawnPawn` picks an unseen eligible crowd NPC in the team's respawn region, Crowd releases it, Shared Character's `TakeOverCrowdPawn` checks eligibility and calls Roster `BindPawn` on Penalty's behalf, then `TakeNpcCard` and `RecordAppearance`. Never a spawn (story 062 covers the path with an NPC available).
- **No eligible NPC.** When no NPC in the respawn set is eligible (out of every non-team viewer's line of sight; not mid-interaction, concealed or braced), Round does not retry. It admits the returner, which sees the world from a neutral view (not the engine spectator state, because `bIsSpectator` replicates to every client), and hands the re-pawn to Penalty.
- **The hand-off.** Round calls Penalty's `IInformantBodyProvider::OnInformantReadmitted(P)` (an interface Round declares and the composition root binds to Penalty, ADR-0019 section 1) inside its slot, only after the returner's Session acknowledgement, when its PlayerController is ready. In the test the stub records the call; it must be called once.
- **No Round timer.** Penalty owns the respawn and its retries (PEN-R10, Level Socket Contract R5b.5 widening). `LateJoinTakeoverTimeoutSec` and its view-only fallback do not apply to informants, so the Round-owned due-time heap holds no `LateJoinTakeoverTimeoutSec`-based item (its only use in Round is the `Active` pawn-loss retry, R17.9, story 076). Assert on the heap contents, not on Penalty's behaviour.
- **Open: confirm against GDD R18 and ADR-0019 section 1** the exact interface name and signature of `IInformantBodyProvider::OnInformantReadmitted` (named in R18; ADR-0019 is outside this story's governing ADRs).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 062 (`production/epics/round-match-framework/story-062-rejoin-after-grace.md`): the rejoin after expiry with an eligible NPC (C1 takeover; this story's dependency).
- Story 073 (`production/epics/round-match-framework/story-073-should-admit-expired-table.md`): the `ShouldAdmitExpired` table that admits the returner.
- Story 063 (`production/epics/round-match-framework/story-063-rejoin-in-ended.md`): the `Ended` rejoin.
- Penalty's respawn, retries and view-only fallback, the Level Socket Contract's `PickRespawnPawn` widening, and Shared Character's `TakeOverCrowdPawn` (other epics).

---

## Files

*Planned files only. Tests live under the engine test roots (`tests/README.md`), not under `tests/`. `/dev-story` replaces this list with the files it actually changed.*

- `Source/StealthGameplay/Public/Round/Disconnect/InformantBodyProvider.h` (consumer-owned interface declared by Round)
- `Source/StealthGameplay/Private/Round/Disconnect/ReadmissionPolicy.cpp` (shared with story 062)
- `Source/StealthGameplay/Private/Tests/Round/ReadmitRetryTest.cpp` (class `FRoundReadmitRetryTest`, test name `StealthGame.Round.ReadmitRetry`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: With no eligible NPC the returner is admitted with a neutral view and the re-pawn is handed to Penalty with no timer of its own.
  - Given: no eligible NPC in the team's respawn set, and a Penalty stub.
  - When: the participant returns.
  - Then: it is admitted with a neutral view, and never in the spectator state.
  - Then: the re-pawn is handed to the Penalty stub.
  - Then: no `LateJoinTakeoverTimeoutSec` timer of Round's own exists in the heap.
  - Edge cases: the Penalty stub is called once.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Round/ReadmitRetryTest.cpp` (automation test `StealthGame.Round.ReadmitRetry`, class `FRoundReadmitRetryTest`) — must exist and pass.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-062-rejoin-after-grace.md`
- Unlocks: None
- Cross-epic: `production/epics/level-socket-contract` (`PickRespawnPawn` and its eligibility rules; stories not yet created), `production/epics/session-travel` (returner acknowledgement; stories not yet created), `production/epics/participant-state-roster` (awaiting-pawn state; stories not yet created)
