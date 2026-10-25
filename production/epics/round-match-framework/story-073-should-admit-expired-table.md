# Story 073: ShouldAdmitExpired is true exactly for InRound records in Preparing, Active and Ended

> **Epic**: Round & Match Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25f
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-007` (disconnect policy: blend hand-over, guarded grace timer, forfeit/void); GDD AC 52; rule R18 (re-admission after expiry, `ShouldAdmitExpired`).
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0005: Session, seamless travel and match-scoped state stores
**ADR Decision Summary**: `AGameModeBase` with four game-instance stores (Session, Roster, Round, Tuning preset) as the only authoritative match state, seamless `ServerTravel` with `?Seed=`, single round acknowledgement, admission modes, host commands on the game instance, no host migration. This story implements Round's side of Session's policy hook: Session exposes `SetExpiredAdmissionPolicy(TWeakInterfacePtr<IExpiredAdmissionPolicy>)` and asks `ShouldAdmitExpired(P)`; Round answers `P.InRound AND Phase in {Preparing, Active, Ended}` (ADR-0002 section 8.7 (3)).
**ADR Version**: ADR-0005 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0005 knowledge risk)
**Engine Notes**: Touches only `TWeakInterfacePtr` registration of a consumer-owned interface, which requires a `UINTERFACE` (ADR-0002 E7, re-verified in 5.8.4 source); the table itself is a pure function and needs no engine API. No post-cutoff API or verification item (spikes S3, S4, S5) is exercised.

**Control Manifest Rules (this layer)** (Round policy rules sit in the Foundation Layer Rules, section Session and travel):
- Required: Authoritative match state lives only in the four game-instance stores, keyed by `ParticipantId`, never by controller or PlayerState pointer (ADR-0005); the answer reads the phase and the Roster record, and keeps no state of its own.
- Forbidden: Never derive from `AGameMode`; never use `AddInactivePlayer`/`FindInactivePlayer` or address-keyed reclaim (ADR-0005).
- Guardrail: Session: silence clocks and ack bookkeeping inside the 0.5 ms line (ADR-0005); the policy query is O(1).

**Prototype disposition**: none (new Round Core code).

---

## Acceptance Criteria

*From GDD `design/gdd/round-match-framework.md` AC 52, scoped to this story:*

- [ ] GIVEN the phase x state table, THEN `ShouldAdmitExpired` is true exactly for `InRound` records in `Preparing`, `Active` and `Ended`, and false for non-`InRound` records and in `Lobby` and `Reset`.

---

## Implementation Notes

*Derived from GDD R18 and ADR-0002 section 8.7 (3):*

- **Rule.** `ShouldAdmitExpired(P) = P.InRound AND Phase in {Preparing, Active, Ended}`. It is false only for a record that was not `InRound` at `ResetAll` (Dormant at the start; it returns through normal admission in `Lobby`) and in `Lobby` and `Reset` (where Session's own admission modes apply). The answer never depends on the participant's lifecycle state: the role on return follows the Roster state (`Active` repossesses, `Informant` is re-pawned through Penalty, `Ended` is view-only).
- **Shape.** Implement a consumer-owned `UINTERFACE` `IExpiredAdmissionPolicy` (declared by the consumer, Session, per ADR-0007; implemented by a Round facade) with one method `ShouldAdmitExpired(FParticipantId)`. The composition root registers it through `USessionService::SetExpiredAdmissionPolicy`. A plain-C++ core holds the pure table function over `(Phase, bInRound)` so the test needs no `UObject`.
- **Table-driven test.** Five phases (`Lobby`, `Preparing`, `Active`, `Ended`, `Reset`) crossed with the record-state list from the Roster contract (at least `InRound` and not `InRound`). Open: confirm against the Roster contract (participant-state-roster epic, R11/R6) the exact list of record states for the non-`InRound` side; the AC fixes only the `InRound` boolean, so the table should use that flag and may add a row per Roster state without changing the expectation.
- **`Unknown` phase.** The replicated `Phase` enum has an explicit `Unknown` (0) for clients only; the server-side policy sees the five real phases. Do not add a sixth row.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 028 (`production/epics/round-match-framework/story-028-phase-admission-and-illegal-transitions.md`): the phase machine and admission modes (this story's dependency).
- Story 062 (`production/epics/round-match-framework/story-062-rejoin-after-grace.md`): what happens to an expired returner in `Active` (informant re-pawn by the C1 takeover).
- Story 063 (`production/epics/round-match-framework/story-063-rejoin-in-ended.md`): the `Ended` rejoin (admitted, no pawn, no transition).
- Story 074 (`production/epics/round-match-framework/story-074-readmit-retry.md`): re-admission with no eligible NPC.
- Session's admission modes, `PreLogin` evaluation and rejoin-token handling (session-travel epic); Roster's `InRound` flag source.

---

## Files

*Planned files only. Tests live under the engine test roots (`tests/README.md`), not under `tests/`. `/dev-story` replaces this list with the files it actually changed.*

- `Source/StealthGameplay/Public/Round/Phase/ExpiredAdmissionPolicy.h`
- `Source/StealthGameplay/Private/Round/Phase/ExpiredAdmissionPolicy.cpp`
- `Source/StealthGameplay/Private/Tests/Round/ShouldAdmitExpiredTableTest.cpp` (class `FRoundShouldAdmitExpiredTableTest`, test name `StealthGame.Round.ShouldAdmitExpiredTable`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: ShouldAdmitExpired is true exactly for InRound records in Preparing, Active and Ended.
  - Given: the phase x record-state table.
  - When: `ShouldAdmitExpired` is evaluated for each cell.
  - Then: it is true exactly for `InRound` records in `Preparing`, `Active` and `Ended`.
  - Then: it is false for non-`InRound` records, and in `Lobby` and `Reset`.
  - Edge cases: table-driven over all five phases; the record-state list comes from the Roster contract (Open: confirm against the Roster contract).

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Round/ShouldAdmitExpiredTableTest.cpp` (automation test `StealthGame.Round.ShouldAdmitExpiredTable`, class `FRoundShouldAdmitExpiredTableTest`) — must exist and pass.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-028-phase-admission-and-illegal-transitions.md`
- Unlocks: None
- Cross-epic: `production/epics/session-travel` (`IExpiredAdmissionPolicy` hook and `SetExpiredAdmissionPolicy`; stories not yet created), `production/epics/participant-state-roster` (`InRound` flag; stories not yet created)
