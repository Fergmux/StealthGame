# Story 053: Pings, reconnect state and the round summary reach only their allowed audience

> **Epic**: Information Visibility Policy
> **Status**: Blocked — BLOCKED: needs UTeamPingViewComponent (informant-pings-team-comms), bReconnecting/ReconnectEndCause (participant-state-roster) and FRoundResult.TeamSummary (round-match-framework)
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

The 2026-10-25 scope additions put three new things on the wire: team pings (`UTeamPingViewComponent`), a teammate-reconnecting bit with its end cause (`bReconnecting`, `ReconnectEndCause`), and a per-team round summary (`FRoundResult.TeamSummary`). Each has a strict audience: the first two only to the sender's or disconnected player's teammates through their own PlayerControllers, the summary to nobody until the round-end step. This story checks all three against the schemas and against a real 2v2 PIE round, so that a new feature cannot quietly open an opponent-visible channel.

**GDD**: `design/gdd/information-visibility-policy.md`
**Requirement**: `TR-ivp-002` (One carrier kind per audience)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0001: Crowd identity rules (section 5, the carrier model); ADR-0002: Resolution queue, round-end staging (round-end step, `TeamSummary`); ADR-0008: Test and automation strategy for leak and parity tests (sections 1 T1/T2, 3)
**ADR Decision Summary**: Team-audience data rides only on C-TeamView parts on the receiving teammate's own PlayerController (`UTeamPingViewComponent` is one such part; `bReconnecting` is a Team-audience bit on the Roster teammate entry); public structs carry no per-team value, and `FRoundResult.TeamSummary[2]` is computed in the latch step and published only in the round-end step (all zero before).
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: The PlayerController is relevant only to its owner (ADR-0001 E6) [V], so each C-TeamView part reaches exactly one connection; registered sub-object list (E11) [V]. T2 uses CQTest `FPIENetworkComponent` (ADR-0008 Verification 1, [U] headless). No spike owns an item here.

**Control Manifest Rules (this layer)**:
- Required: **`UTeamPingViewComponent` is a C-TeamView part written only by the Informant Pings server handler, cleared at `OnRosterReset`, travel and Round's round-end step** — source: ADR-0001, ADR-0002
- Forbidden: **Never add a teammate connection field other than the one `bReconnecting` bit (no ping, time, reason or address), and never send it to a non-teammate** — source: ADR-0001, ADR-0005 (user decision 2026-10-25)
- Forbidden: **Never write a non-zero `TeamSummary` (or any per-team count) into `FRoundPublicState` before `Ended`** — source: ADR-0002 (user decision 2026-10-25)
- Required: **`FRoundResult.TeamSummary[2]` is computed in the latch step and published only in the round-end step (all zero before)** — source: ADR-0002

---

## Acceptance Criteria

*From GDD `design/gdd/information-visibility-policy.md`, AC-IVP-46, scoped to this story:*

- [ ] **AC-IVP-46** [Unit][PIE-2] `StealthGame.Visibility.ScopeAdditions2026_10_25`: GIVEN the C-TeamView schema, the `FRoundPublicState` schema and a 2v2 PIE round with pings, a teammate disconnect inside the grace and an objective win, WHEN the R11 audit and the per-connection replicated-property logs are checked, THEN `UTeamPingViewComponent`, `bReconnecting` and `ReconnectEndCause` appear only on PlayerControllers and only in the sender's or disconnected player's teammates' logs (never an opponent's), and `FRoundResult.TeamSummary` is absent or zero in every `FRoundPublicState` received before `Ended` and present only from the round-end step.

---

## Implementation Notes

*Derived from ADR-0001 section 5, ADR-0002 (round-end step) and ADR-0008 sections 1, 3:*

- Two layers, matching the criterion's [Unit] and [PIE-2] tags. **T1 schema check** `StealthGame.Visibility.ScopeAdditions2026_10_25` (class `FVisibilityScopeAdditions2026_10_25Test`) in `Source/StealthFoundation/Private/Tests/Visibility/ScopeAdditions2026_10_25Test.cpp`: reflect over the C-TeamView parts and the `FRoundPublicState` schema; `UTeamPingViewComponent`, `bReconnecting` and `ReconnectEndCause` exist only on PlayerController-owned parts, and `FRoundPublicState` carries no per-team field. **T2 run** `NETWORK_TEST_CLASS(ScopeAdditions2026_10_25, "StealthGame.Visibility.PIE")` in `Source/StealthTestHarness/Private/Tests/PIE/`: a 2v2 round with pings from both teams, a teammate disconnect inside the grace period (a required event) and an objective win; read each connection's replicated-property log and the received-RPC list.
- Assertions: on every opponent connection (Active or Informant) none of the three appears; on the sender's or disconnected player's teammates they appear only as allowed; every `FRoundPublicState` received before `Ended` (not just the last) shows `TeamSummary` absent or zero, and a non-zero `TeamSummary` appears only from the round-end step. The R11 audit machinery is story-015's; reuse it.
- Negative control (added by the story, ADR-0008 section 3): a harness-only variant that injects an opponent receipt of `bReconnecting` (for example the part written to an opponent controller) must be detected; the suite passes only if the control is detected. The control lives only in `StealthTestHarness`, never Shipping. The criterion text stays the GDD's.
- Bound steps by server time and frames; wall-clock only as a failure ceiling. Assert and record the legacy replication system.
- Prototype: not applicable. Replaces: none (new).
- Cross-epic needs: informant-pings-team-comms epic (`UTeamPingViewComponent` and the ping handler), participant-state-roster epic (`bReconnecting`, `ReconnectEndCause`, the C-TeamView Roster part), round-match-framework epic (`FRoundResult.TeamSummary` and the round-end step), session-travel epic (grace-period disconnect), objectives-chain epic (an objective win), tuning-data-config Exit A (T2 harness).
- Unblocks when: those three features exist on production classes.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- story-021: team view never to opponents through elimination, takeover, disconnect and rejoin (the general C-TeamView audience check)
- story-022: public structs global-only (the general `FRoundPublicState` and snapshot check)
- story-036: post-round latch release (`PostRound` class change at round end)
- story-015: the replicated-property audit machinery
- informant-pings-team-comms, participant-state-roster and round-match-framework epics: the features themselves

---

## Files

*Planned (module folders are created by tuning-data-config Exit A and do not exist yet):*

- `Source/StealthFoundation/Private/Tests/Visibility/ScopeAdditions2026_10_25Test.cpp` (T1 schema check)
- `Source/StealthTestHarness/Private/Tests/PIE/ScopeAdditions2026_10_25Test.cpp` (T2 2v2 PIE round, including the opponent-receipt negative control)

---

## QA Test Cases

- **AC-1**: AC-IVP-46 `StealthGame.Visibility.ScopeAdditions2026_10_25` and `StealthGame.Visibility.PIE.ScopeAdditions2026_10_25`
  - Given: the C-TeamView and `FRoundPublicState` schemas, and a 2v2 PIE round with pings, a teammate disconnect inside the grace period and an objective win.
  - When: the R11 audit and the per-connection replicated-property logs are checked.
  - Then: `UTeamPingViewComponent`, `bReconnecting` and `ReconnectEndCause` appear only on PlayerControllers, and only in the logs of the sender's or disconnected player's teammates, never an opponent's; `FRoundResult.TeamSummary` is absent or zero in every `FRoundPublicState` received before `Ended` and present only from the round-end step.
  - Edge cases: check every `FRoundPublicState` received before `Ended`, not just the last; the grace-period disconnect is a required event; negative control (added): an injected opponent receipt of `bReconnecting` must be detected.

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: T2 test under `Source/StealthTestHarness/Private/Tests/PIE/` (with the T1 schema check), OR a documented playtest/spike report

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/information-visibility-policy/story-011-matrix-per-fact.md`, `production/epics/information-visibility-policy/story-015-replicated-property-audit.md`; informant-pings-team-comms epic (`UTeamPingViewComponent`); participant-state-roster epic (`bReconnecting`, `ReconnectEndCause`); round-match-framework epic (`FRoundResult.TeamSummary`, round-end step); tuning-data-config Exit A (T2 harness)
- Unlocks: None
