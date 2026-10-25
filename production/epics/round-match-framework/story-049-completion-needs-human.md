# Story 049: A final completion counts only from a human, Connected, Active completer; other cases are Voided with distinct codes

> **Epic**: Round & Match Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-005` (Objective and elimination wins; no draw, no clock); GDD AC 20; rules R13 (the `ObjectiveCompletion` row and the `HoldTimer` row), R15 (`ObjectiveWin`), R17 (participant-bound AI constraint), F2 (`ObjectiveWin(t, i)` and `Valid(i)`), Edge Cases ("If a hold completes for a participant who disconnected in the same tick").
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0002: Server resolution queue, server clock and round-end staging
**ADR Decision Summary**: One total-order queue per round keyed (ServerTickIndex, Seq), per-source mailboxes, due-time heap, drained once per frame by a custom `TG_PostPhysics` tick function, typed staged writes, single-assignment latch storage with Round's rule injected, the R9 two-step round end, and the RTT-compensated client `EstimatedServerNow()`. This story applies section 5: resolvers re-validate against live state at the slot, and a re-validation failure is `Voided` with an owner-specific code recorded in the log entry (`EVoidReason::Revalidation`).
**ADR Version**: ADR-0002 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (ADR-0002 knowledge risk)
**Engine Notes**: None touched. The validity check runs on the fake Roster and a fake pawn-control view with stub resolvers; no tick function, `UWorld` or post-cutoff API is exercised, so ADR-0002 verification items (1) to (3) do not apply.

**Control Manifest Rules (this layer)** (the Round-relevant rules sit in the Foundation Layer Rules, Resolution queue and Forbidden Approaches):
- Required: resolvers re-validate against live state at the slot and run follow-ons inline; a deferred outcome is a new item with its own key and a logged `CauseKey` (ADR-0002).
- Forbidden: Never give a participant-bound AI-driven pawn `Interact`, `Prepare`, `Attack` or `Hide`; it would complete objectives or change concealment for an absent player (ADR-0002, Technical Setup 2026-10-25b).
- Guardrail: queue drain, staging, teardown and Roster flush stay at or below 0.5 ms of listen-host game thread (ADR-0002, ADR-0022); the validity check is a handful of comparisons per completion.

**Prototype disposition**: Replaces the completer check in `AStealthGameMode::OnObjectiveCompleted(AStealthObjective*, AStealthCharacter* Completer)` in `Source/StealthGame/Private/StealthGameMode.cpp` (called from `StealthObjective.cpp`), which accepts any character that reaches the objective. No prototype equivalent of human, Connected or Active validity.

---

## Acceptance Criteria

*From GDD `design/gdd/round-match-framework.md` AC 20, scoped to this story:*

- [ ] GIVEN a final completion by a human, Connected, Active completer, THEN `ObjectiveWin`; GIVEN the completer AI-driven, Disconnected, Informant, or disconnected in the same tick, THEN `Voided(FinalCompletion, code)` with a distinct code per case and no outcome.

---

## Implementation Notes

*Derived from GDD R13, R15, F2 and ADR-0002 section 5:*

- **The validity predicate (F2).** `ObjectiveWin(t, i) = Phase = Active AND NOT L AND i.Type = FinalCompletion AND Valid(i) AND Team(i.Completer) = t`, where `Valid(i)` is: completer `Active`, `Connected`, human-controlled, site live and not yet completed. R15 words it the same way: Objectives' resolver, after re-validating, calls `Round.DeclareObjectiveWin(team, Key)`; the completer must be `Active`, Connected and human-controlled (never an AI-driven bound pawn, R17). Implement the Round side of `Valid(i)` as a function returning a closed result code, so `DeclareObjectiveWin` and the resolver share one definition.
- **Human-controlled (R13).** The `HoldTimer(Complete)` row defines it: the pawn's control epoch is unchanged since the hold's `Begin` and no hand-over is pending (ADR-0020 section 5.4 (c), ADR-0010 section 4.4). The fake pawn-control view in this test supplies the epoch and pending-hand-over flag; it does not read a controller or PlayerState (ADR-0010: no kind-reading in movement; Roster binding queries only).
- **Five cases.** (1) human, `Connected`, `Active`: `ObjectiveWin`, one `WinLatched`. (2) AI-driven completer (a bound AI pawn, grace or hand-over): `Voided(FinalCompletion, code)`. (3) `Disconnected` completer: `Voided`. (4) `Informant` completer: `Voided`. (5) a completer that disconnected in the same tick: `Voided`. Each void code is distinct; none latches, so `TryTerminate` is not called and the round stays `Active`. A void logs one `Voided(type, reason)` entry (ADR-0002 section 8.9), subject to the one-per-source-per-second limit of story 015.
- **Codes live in the constants file (story 005).** Do not inline code names in the test. Open: confirm the four code names against GDD R13/R15 and the Voided entry definition (ADR-0002 section 8.9); the AC requires only "a distinct code per case" and names none.
- **Open: confirm against GDD R15 and Edge Cases** whether, in the "disconnected in the same tick" case, the disconnect's key must precede the completion's. The AC does not say. The Edge Cases text says `Valid(i)` requires `Connected` and human control, so the completion is voided, and the IIF has already turned the disconnect into a release. The test fixes one stated order (disconnect first) and does not assert the reverse order until the GDD answers.
- **Relation to story 044.** The preliminary (stage) completion rule is there; this story covers only the final completion's validity. The real Objectives resolver is a different epic's (AC 2 is deferred); here a stub resolver calls the shared validity function and `DeclareObjectiveWin`.
- **Where the code goes.** The validity function and result enum join `Source/StealthGameplay/Public/Round/Win/RoundWinPredicates.h` (created by story 044) and `Private/Round/Win/RoundWinPredicates.cpp`; `DeclareObjectiveWin` is on the latch core of story 043. Planned names; `/dev-story` replaces them.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 044 (`production/epics/round-match-framework/story-044-elimination-truth-table.md`): the elimination truth table and the stage-completion rule.
- Story 043 (`production/epics/round-match-framework/story-043-no-draw-property.md`): the latch and outcome uniqueness.
- Story 066 (`production/epics/round-match-framework/story-066-forfeit-vs-completion.md`): a forfeit item and a final completion in one tick.
- Story 067: the bound AI pawn's `Interact`, `Prepare`, `Attack` and `Hide` rejection (`BoundAiForbidden`) at the command interface.
- Story 060: the grace and hand-over timelines that make a completer AI-driven or `Disconnected`.
- Story 015: the Voided entry rate limit.
- Objectives internals (site live, not yet completed, stage counts) and IIF internals (hold completion item) belong to other epics.

---

## Files

*Planned files only. Tests live under the engine test roots (`tests/README.md`), not under `tests/`. `/dev-story` replaces this list with the files it actually changed.*

- `Source/StealthGameplay/Public/Round/Win/RoundWinPredicates.h` (completion validity function and code enum; the file is created by story 044)
- `Source/StealthGameplay/Private/Round/Win/RoundWinPredicates.cpp`
- `Source/StealthGameplay/Private/Tests/Round/CompletionNeedsHumanTest.cpp` (class `FRoundCompletionNeedsHumanTest`, test name `StealthGame.Round.CompletionNeedsHuman`)
- `Source/StealthGame/Private/StealthGameMode.cpp` (`OnObjectiveCompleted`: retired/replaced by this story and its neighbours)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: A final completion counts only from a human, Connected, Active completer, and other cases are `Voided` with distinct codes.
  - Given: a final completion by a human, Connected, Active completer; then by an AI-driven completer, a Disconnected one, an Informant, and one disconnected in the same tick.
  - When: each completion is drained.
  - Then: the first gives `ObjectiveWin`.
  - Then: each other case gives `Voided(FinalCompletion, code)`, a distinct code per case, and no outcome.
  - Edge cases: the disconnected-in-same-tick case depends on key order (the AC does not say whether the disconnect's key must precede). Open: confirm against GDD R15 and Edge Cases (see Implementation Notes); code names come from the constants file.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Round/CompletionNeedsHumanTest.cpp` (automation test `StealthGame.Round.CompletionNeedsHuman`, class `FRoundCompletionNeedsHumanTest`) — must exist and pass.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-044-elimination-truth-table.md`
- Unlocks: `production/epics/round-match-framework/story-060-grace-timeline.md`, `production/epics/round-match-framework/story-066-forfeit-vs-completion.md`
- Cross-epic: `production/epics/participant-state-roster` (`State` and `Connection` of the completer, via the fake Roster gateway; stories not yet created), `production/epics/input-interaction-framework` (the `HoldTimer(Complete)` item and control-epoch rule that the validity check reads; stories not yet created), `production/epics/tuning-data-config` (module skeleton and StealthTestHarness bootstrap; stories not yet created)
