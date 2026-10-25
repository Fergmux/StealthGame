# Story 013: Opponent sees nothing

> **Epic**: Informant Pings & Team Communication
> **Status**: Ready
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/informant-pings-team-comms.md` (PNG-R6, PNG-R8, PNG-R9; AC-PNG-01)
**Requirement**: `TR-png-012` (also `TR-png-018`)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0001: Crowd identity rules (§5 carrier model: team data is absent from opponents), with ADR-0008 (leak and parity test strategy)
**ADR Decision Summary**: An opponent's controller is never relevant to this team's clients, so opponents hold no entry, slot, widget, RPC or sound for a team's ping: the team-only rule is enforced by the absence of data. ADR-0008 proves it with headless cores, a multi-client and multi-process leak harness, raw-bunch capture and net-emulation profiles, and every leak test must be watched failing against an injected leak.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0001 networking; the legacy NetDriver is asserted by the harness)
**Engine Notes**: The harness startup assertion that the game NetDriver runs the legacy replication system must hold. ADR-0001 spike S1 (raw-bunch parity) is run by `production/epics/information-visibility-policy/story-001-spike-s1-multiprocess-raw-bunch-capture.md`; this test uses that capture tooling and the replicated-property logs, and must not be marked passed against a harness whose capture path has not itself been validated by S1.

**Control Manifest Rules (this layer)**:
- Required: Pings leak and parity rows T2 `StealthGame.Pings.PIE.OpponentSeesNothing` (run with a team-0 and a team-1 listen host), each watched failing against its injected-leak negative control — source: ADR-0008
- Required: opponent machines hold no ping part, entry, RPC, widget or sound, the host included when it is an opponent — source: ADR-0001, ADR-0003
- Forbidden: Never write a ping to `FRoundPublicState`, C-Public or any `All`-audience carrier; never spawn a world sound or effect for a ping — source: ADR-0001, ADR-0003

---

## Acceptance Criteria

*From GDD `design/gdd/informant-pings-team-comms.md` (AC-PNG-01, PNG-R8, PNG-R9), scoped to this story:*

- [ ] **AC1** (AC-PNG-01, `[Integration]` `StealthGame.Pings.PIE.OpponentSeesNothing`): **GIVEN** a 2v2 PIE with a team-0 informant and a team-0 Active player each pinging 3 times, run once with a team-0 player as listen host and once with a team-1 player as listen host **WHEN** team-1 clients' replicated-property logs, received RPC lists, HUD view-model dumps and audio-event logs (and in the second run the team-1 host's own HUD and audio dumps) are compared with a control run without pings **THEN** they are identical on every tick (no ping part, entry, widget, sound or RPC on a team-1 machine) and both team-0 allies' dumps show each ring.
- [ ] **AC2** (TR-png-018, PNG-R8): **GIVEN** the same dumps **WHEN** scanned **THEN** no machine shows a ping feed line, plate, card or counter, and nothing is written to `FRoundPublicState`, C-Public or any `All`-audience carrier.
- [ ] **AC3** (ADR-0008 negative controls): **GIVEN** an injected leak (the ping part replicated to an opponent controller; a world sound spawned on ping) **WHEN** the test runs **THEN** it fails, watched failing before the fix is trusted.

---

## Implementation Notes

*Derived from ADR-0008 §2 and the Pings amendment rows in the control manifest:*

Use the multi-client PIE harness with `NETWORK_TEST_CLASS(OpponentSeesNothing, "StealthGame.Pings.PIE")`, one listen host per run, with per-tick dumps compared by tick index (fixed 60 Hz step, identical seeds). The HUD view-model dump and audio-event log hooks come from the HUD epic's parity tests (`StealthGame.HUD.PIE.*`); until those exist, dump the Pings-visible surface only (the controller's component list, the ring model and the cue log). The control run uses the same seed and script with no `IA_Ping` presses. The informant in the scenario is a real informant with a body (post-takeover) so both roles are exercised. Negative controls live behind `WITH_STEALTH_TEST_SEAMS` in `StealthTestHarness` only and are never compiled into Shipping (ADR-0008).

AC-PNG-02 (sender pawn parity) is story 014; the ring-per-sender semantics are story 015.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 014: the sender pawn's replicated-state parity and the nightly raw-bunch row.
- Story 003: the allowlist audit rows.
- IVP epic stories 021 and 053: the cross-part audience inspections (this test is the Pings-specific run).
- hud epic: the HUD view-model dump and audio-event log hooks.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthTestHarness/Private/Pings/OpponentSeesNothingTest.cpp` (CQTest PIE, `NETWORK_TEST_CLASS(OpponentSeesNothing, "StealthGame.Pings.PIE")`)
- `Source/StealthTestHarness/Private/Pings/PingLeakNegativeControls.cpp` (behind `WITH_STEALTH_TEST_SEAMS`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

**Test: AC1 opponent sees nothing (AC-PNG-01)**
- Given: a 2v2 PIE; the team-0 informant and Active player ping 3 times each; run once with a team-0 host and once with a team-1 host; plus a no-ping control run.
- When: compare the team-1 clients' replicated-property logs, RPC lists, HUD view-model dumps and audio-event logs (and the team-1 host's own HUD and audio dumps) against the control.
- Then: identical on every tick; both team-0 allies' dumps show each ring.
- Edge cases: the team-1 host case is the one most likely to leak.

**Test: AC2 no feed anywhere**
- Given: the same dumps.
- When: scan.
- Then: no feed line, plate, card or counter on any machine; nothing written to `FRoundPublicState`, C-Public or any All-audience carrier.
- Edge cases: the refusal plate is out of scope (no refused press in these runs).

**Test: AC3 injected-leak negative controls**
- Given: an injected leak (the part replicated to an opponent controller; a world sound spawned on ping).
- When: the test runs.
- Then: it fails for each leak, and each failure is watched before the clean pass is trusted.
- Edge cases: the leak toggles are test-only and compiled out of Shipping.

QL-STORY-READY: ADEQUATE

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/Pings/OpponentSeesNothingTest.cpp` OR a documented playtest

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/informant-pings-team-comms/story-003-carrier-registration-and-team-only-audit.md`, `story-006-server-team-ping-handler-and-host-parity.md`, `story-007-ping-lifecycle-clears-and-reset.md`, `story-011-concealed-press-refusal.md`; cross-epic `production/epics/information-visibility-policy/story-001-spike-s1-multiprocess-raw-bunch-capture.md`
- Unlocks: None
