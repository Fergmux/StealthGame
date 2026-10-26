# Story 023: Start refuses with ParticipantNotConnected naming exactly the Dormant, Unresponsive or Connecting set

> **Epic**: Round & Match Framework
> **Status**: Complete
> **Layer**: Core
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25f
> **Last Updated**: 2026-10-26

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-001` (Five-phase machine in a game-instance store surviving travel). The nearest registered requirement; Start refusal is not a separate TR, so this mapping is approximate. Also `GDD AC 4`, `R5` check 3 (`ParticipantNotConnected`), `F3` (`Conn(p) = Connected`).
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0005: Session, seamless travel and match-scoped state stores
**ADR Decision Summary**: `AGameModeBase` with four game-instance stores (Session, Roster, Round, Tuning preset) as the only authoritative match state, seamless `ServerTravel` with `?Seed=`, single round acknowledgement, admission modes, host commands on the game instance, no host migration. This story applies section 2 and the interface in Key Interfaces: connection states (`EConnectionState`: `Connecting`, `Connected`, `Unresponsive`, `Traveling`, `Disconnected`, `Rejoining`, `Expired`, `Dormant`, ...) are Session's facts, read through `GetConnectionState(FParticipantId)`; Round decides the consequence.
**ADR Version**: ADR-0005 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: ADR-0005 post-cutoff item (5.6 `DispatchPostLogin` deprecation) is not touched. Headless check against a fake Session/Roster; the real connection-state machine (S4 reconnect, S5 listen-host loss) is Session's, and none of spikes S3, S4, S5 is touched here.

**Control Manifest Rules (this layer)** (the Round-relevant rules sit in the manifest's Foundation Layer Rules, Session and travel and Forbidden; the Core Layer Rules section holds no Round-specific rule):
- Required: Ack records, kick list, silence clocks and connection maps are keyed by `ParticipantId` (or `UNetConnection` for the live mapping) and never replicated (ADR-0005); the refusal lists `ParticipantId`s only.
- Forbidden: Never add a teammate connection field other than the one `bReconnecting` bit (no ping, time, reason or address), and never send it to a non-teammate (ADR-0001, ADR-0005); the refusal ids reach only the host sink.
- Guardrail: Tests: per-commit pipeline at most 10 min (ADR-0008); this is a headless T1 test with no world.

**Prototype disposition**: none (new Core code). The prototype has no connection-state check at start (`StartRound` in `StealthGameMode.cpp` gates on player count only).

---

## Acceptance Criteria

*From GDD `design/gdd/round-match-framework.md` AC 4, scoped to this story:*

- [ ] GIVEN one participant each `Dormant`, `Unresponsive` or `Connecting`, WHEN Start, THEN `ParticipantNotConnected` with `ParticipantIds` equal to exactly that set, and the roster count is unchanged.

---

## Implementation Notes

*Derived from GDD R5 check 3 and ADR-0005 Key Interfaces:*

- Rule (R5 check 3): `ParticipantNotConnected` (retryable; lists the ids): any rostered participant is `Unresponsive`, `Connecting` or `Dormant`. A `Dormant` participant is **never pruned automatically**: the host removes them explicitly with `stealth.round.remove <participant>` (legal while teams are unlocked) or waits for them to return, so an accidental alt-tab does not silently cost a friend their seat. The ids come back in `FStartRefusal.ParticipantIds`, shown to the host only.
- This builds on story 022's Start-validation unit: the check-3 branch collects the exact offending `FParticipantId` set by reading each rostered participant's connection state through the Session gateway (fake, story 003). The set must equal the offenders exactly (no extra ids, none missing), including for a mixed layout with all three states present at once.
- No mutation: the roster count is unchanged after the refusal (no implicit prune of the Dormant participant), the phase stays Lobby, and no admission or lock call is made.
- `Traveling`, `Disconnected`, `Rejoining` and `Expired` are not in the R5 check-3 list; Open: confirm against GDD R5 and F3 (`Conn(p) = Connected`) how those states are treated at Start, because F3 requires `Connected` while R5 names only three states. Only the three named states are asserted by this story.
- Order: this check precedes `TeamsInvalid` (story 022 asserts the pair); here the layouts are otherwise valid so the refusal is attributable to connection state alone.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 022 (`production/epics/round-match-framework/story-022-start-validity.md`): the full set of refusal codes and their check order, including NotConnected before Imbalance.
- Story 024 (`production/epics/round-match-framework/story-024-start-ordering.md`) and story 025 (`production/epics/round-match-framework/story-025-start-rollback.md`): accepted-Start ordering and rollback.
- Host `stealth.round.remove` behaviour (story 020 covers the Dormant-slot rule, story 026/027 the command registration and phase matrix).
- Session connection-state transitions and the Roster's Dormant handling: epics `session-travel` and `participant-state-roster`.

---

## Files

*Planned files. Tests live under the engine test roots (`Source/<Module>/Private/Tests/`, see `tests/README.md`), not under `tests/`. `/dev-story` replaces this list with the files it actually changed.*

- Source/StealthGameplay/Private/Core/Round/StealthRoundStart.cpp (check 3)
- Source/StealthGameplay/Private/Tests/Round/StartRefusesNotConnectedTest.cpp (new)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: Start refuses with `ParticipantNotConnected` naming exactly the Dormant, Unresponsive or Connecting set.
  - Given: Lobby with one participant each in `Dormant`, `Unresponsive` and `Connecting`.
  - When: Start is issued once per case, and once with all three.
  - Then: the refusal is `ParticipantNotConnected` and `ParticipantIds` equals exactly that set.
  - Then: the roster count is unchanged.
  - Edge cases: a mixed layout lists all of the offending ids and no others; the phase stays `Lobby`. Open: confirm against GDD R5 and F3 how `Traveling`, `Disconnected`, `Rejoining` and `Expired` are treated at Start (not asserted here).

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Round/StartRefusesNotConnectedTest.cpp` — must exist and pass (automation test `StealthGame.Round.StartRefusesNotConnected`, class `FRoundStartRefusesNotConnectedTest`)

**Status**: [x] Created and passing (`StealthGame.Round.StartRefusesNotConnected`, 2026-10-26)

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-022-start-validity.md`
- Unlocks: None

Cross-epic: `production/epics/session-travel` (`EConnectionState`, `GetConnectionState`; stories not yet created); `production/epics/participant-state-roster` (rostered participants, Dormant records; stories not yet created); `production/epics/tuning-data-config` (module skeleton and StealthTestHarness bootstrap; stories not yet created)

---

## Completion Notes
**Completed**: 2026-10-26 (D53 integration batch: PSR 061-062, RMF 018-029, SCL 002/006)
**Criteria**: 1/1 passing
**Deviations**: Traveling/Disconnected/Rejoining/Expired are not refused at Start (story open point left as the R5 list).
**Test Evidence**: `Source/StealthGameplay/Private/Tests/Round/StartRefusesNotConnectedTest.cpp`; full suite: `production/qa/evidence/integration-spine-2026-10-26/full-suite.txt` (120/120 Success)
**Run result**: N/A: host-only refusal, no visible change
**Reviewed by**: lead-programmer focused review (sonnet) + self-review, D53 batch
