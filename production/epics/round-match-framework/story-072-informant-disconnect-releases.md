# Story 072: An Informant disconnect calls ReleaseToCrowd once with no evaluation, and later kills resolve Npc

> **Epic**: Round & Match Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25f
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-007` (disconnect policy: blend hand-over, guarded grace timer, forfeit/void); GDD AC 51; rule R17.5 (an Informant disconnects), R15 (Informants never count), R17.9.
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0005: Session, seamless travel and match-scoped state stores
**ADR Decision Summary**: `AGameModeBase` with four game-instance stores (Session, Roster, Round, Tuning preset) as the only authoritative match state, seamless `ServerTravel` with `?Seed=`, single round acknowledgement, admission modes, host commands on the game instance, no host migration. This story applies Session's `OnParticipantDisconnected` fact and Round's policy in ADR-0002 section 8.7 (2): an `Informant` that disconnects has no grace; Round calls `ReleaseToCrowd` on its pawn at once and the state stays `Informant`.
**ADR Version**: ADR-0005 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0005 knowledge risk)
**Engine Notes**: None touched. The test uses fake Roster and Crowd gateways; no engine API, post-cutoff API or verification item (spikes S3, S4, S5; ADR-0010 S8 `PawnLeavingGame`) is exercised.

**Control Manifest Rules (this layer)** (Round policy rules sit in the Foundation Layer Rules, section Round phase flow, Event Log and public round struct):
- Required: An `Informant` that disconnects is released to the crowd at once (ADR-0002, Technical Setup 2026-10-25b).
- Forbidden: Never mutate gameplay state outside the drain; never resolve inline in an RPC body or AI source (ADR-0002). The release runs in a queue slot, not in Session's delegate.
- Guardrail: Session: silence clocks and ack bookkeeping inside the 0.5 ms line (ADR-0005).

**Prototype disposition**: none (new Round Core code).

---

## Acceptance Criteria

*From GDD `design/gdd/round-match-framework.md` AC 51, scoped to this story:*

- [ ] GIVEN an Informant disconnects, THEN `ReleaseToCrowd` once, state stays `Informant`, no evaluation, and a kill on the released pawn resolves `Npc`.

---

## Implementation Notes

*Derived from GDD R17.5 and ADR-0002 section 8.7 (2):*

- **Entry.** `OnParticipantDisconnected` for a participant whose Roster state is `Informant` (in `Active`): Round calls `ReleaseToCrowd(pawn)` on the informant's bound pawn at once, from a queue slot (the `ConnectionTransition` item), with no grace timer and no state transition. There is nothing to wait for: an informant has no grace value.
- **State and evaluation.** The state stays `Informant` (Roster is untouched), so no `OnRosterStateChanged` event exists to evaluate; R15 never sees an event. Informants never count toward a team's Active count.
- **Kill on the released pawn.** After the release the pawn is an NPC with the same look and no participant binding, so a later kill resolves with `VictimKind = Npc` (a strike counts against the killer under Penalty's rules, which are not tested here). In this story the kill is a stub victim-classification call against the fake Roster (binding cleared by the release); Open: confirm against the Roster contract which query supplies `VictimKind` and what it returns for an unbound released pawn.
- **Once only.** A second disconnect event for the same participant (for example a duplicate delivery or a connection that flaps) must not call `ReleaseToCrowd` again; `ReleaseControl` on an already-released pawn is a no-op in Crowd (ADR-0016), but Round must not rely on that and tracks the released pawn.
- **Out of the later return.** The informant's return, inside or after the window, is R18's informant respawn path (Penalty) and is not part of this story.
- **Unresponsive informants.** An `Unresponsive` informant with a bound pawn is driven by Crowd through the same blend path and handed back on `OnParticipantResponsive` (R17.1, story 090, Blocked); that is a different event from `Disconnected` and is not tested here.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 060 (`production/epics/round-match-framework/story-060-grace-timeline.md`): the Active participant's grace timeline (this story's dependency).
- Story 062 (`production/epics/round-match-framework/story-062-rejoin-after-grace.md`): a returner who became an Informant after expiry is re-pawned by the C1 takeover.
- Story 074 (`production/epics/round-match-framework/story-074-readmit-retry.md`): re-admission with no eligible NPC.
- Story 048 (`production/epics/round-match-framework/story-048-informants-do-not-count.md`): informant kills and re-informs trigger no evaluation.
- Penalty's informant respawn, strike counting and Crowd's `ReleaseToCrowd` internals (other epics).

---

## Files

*Planned files only. Tests live under the engine test roots (`tests/README.md`), not under `tests/`. `/dev-story` replaces this list with the files it actually changed.*

- `Source/StealthGameplay/Public/Round/Disconnect/InformantDisconnectPolicy.h`
- `Source/StealthGameplay/Private/Round/Disconnect/InformantDisconnectPolicy.cpp`
- `Source/StealthGameplay/Private/Tests/Round/InformantDisconnectReleasesTest.cpp` (class `FRoundInformantDisconnectReleasesTest`, test name `StealthGame.Round.InformantDisconnectReleases`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: An Informant disconnect calls ReleaseToCrowd once with no evaluation, and later kills resolve Npc.
  - Given: an Informant participant.
  - When: it disconnects, then a kill lands on the released pawn.
  - Then: `ReleaseToCrowd` is called once.
  - Then: the state stays `Informant` and no evaluation runs.
  - Then: the kill resolves `Npc`.
  - Edge cases: a second disconnect event does not call `ReleaseToCrowd` again.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Round/InformantDisconnectReleasesTest.cpp` (automation test `StealthGame.Round.InformantDisconnectReleases`, class `FRoundInformantDisconnectReleasesTest`) — must exist and pass.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-060-grace-timeline.md`
- Unlocks: None
- Cross-epic: `production/epics/crowd-npc-behaviour` (`ReleaseToCrowd`; stories not yet created), `production/epics/participant-state-roster` (state and victim classification; stories not yet created), `production/epics/session-travel` (`OnParticipantDisconnected`; stories not yet created)
