# Story 083: No widget binds elapsed time, a countdown or a count in a full Active round; screenshots retained

> **Epic**: Round & Match Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: UI
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-005` (Objective and elimination wins; no draw, no clock), `TR-round-009` (`FRoundPublicState` identical bytes for every viewer; `ActiveAtServerTime` zeroed in Active; bounded late-loader reveal); GDD AC 31; rules R16 (4), R20, R23 (d).
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0002: Server resolution queue, server clock and round-end staging; ADR-0003: Replication, relevancy and the concealment cull hybrid
**ADR Decision Summary**: ADR-0002: one total-order queue per round, a due-time heap, typed staged writes, the R9 two-step round end and an RTT-compensated client `EstimatedServerNow()`; it deliberately leaves no clock source in the public struct (`ActiveAtServerTime` and `PhaseEnteredServerTime` read 0 in Active) and names no timer that can end an Active round. ADR-0003: the allowlist audit and identical bytes for every viewer, so no replicated field carries an opponent count. This story is the presentation-side proof of both: no widget binds elapsed Active time, a countdown or a count.
**ADR Version**: ADR-0002 2026-10-24; ADR-0003 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (ADR-0002 knowledge risk)
**Engine Notes**: Touches ADR-0002 section 9 only indirectly: the "simulated 2 x `TargetRoundMinutes`" sample is a server-clock jump (the fake or advanced server clock), never real time, and `EstimatedServerNow()` is the only server-time source presentation may use. No post-cutoff API is used. The scan walks UMG widget trees and their bindings (the same reflection the ADR-0007 Blueprint audit uses); ADR-0007 Verification item (3) (graph-walk API details [U]) concerns the editor-side audit and is not exercised here.

**Control Manifest Rules (this layer)** (Round-relevant rules sit in the Foundation Layer Rules):
- Required: C-TeamView carries own-team Roster data only; opponent `ActiveCount`, `InformantCount`, `AwaitingPawn` and anything derived from them are never replicated to, computed on or presented by a client before `Ended` (ADR-0005, ADR-0001).
- Forbidden: Never write a non-zero `TeamSummary` (or any per-team count) into `FRoundPublicState` before `Ended` (ADR-0002, user decision 2026-10-25).
- Guardrail: no layer guardrail targets a widget-tree scan; nearest is Tests: per-commit pipeline <= 10 min (ADR-0008). The four-sample scan and screenshot capture run in the [PIE-2] pass.

**Prototype disposition**: None (new verification of the HUD epic's widgets; the prototype HUD is not promoted).

**Where it sits**: Exit B (Core). It is anchor moment 3 of the GDD: no clock, countdown or enemy count is ever shown before `Ended`. The widgets are built by the HUD epic; this story owns the scan, the allowlist and the retained screenshots that prove the rule on a full Active round.

---

## Acceptance Criteria

*From GDD `design/gdd/round-match-framework.md` AC 31, scoped to this story:*

- [ ] GIVEN a full Active round, WHEN the widget tree is scanned at t0, at simulated 2 x `TargetRoundMinutes`, after an opposing elimination and after a disconnect, THEN no widget binds elapsed time, a countdown or a count; screenshots of each sample are retained in `production/qa/evidence/` named `round-noclock-<sample>.png`.

---

## Implementation Notes

*Derived from GDD R16 (4), R20 (Forbidden presentation) and the UI Requirements section; ADR-0002 section 9 and ADR-0007 section 3:*

- **Predicate freeze (QA requirement).** The AC does not define "binds elapsed time, a countdown or a count". Freeze a checked-in allowlist of permitted widget bindings (data file next to the test); the scan fails on any binding whose source is elapsed Active time, a countdown or a count that is not on the list. The forbidden list is the GDD's: elapsed Active time, a countdown, an enemy count, a remaining-rival icon, an elimination-progress bar (Presentation rules, Roster R6, Pillar 4). Allowed counts such as the lobby's `n / capacity` are lobby-only; they must be on the allowlist by name if a widget carries them.
- **Non-vacuity.** The scan fails if it finds zero widgets. Keep one planted-violation widget fixture outside the scanned content to prove the scan can fail.
- **Four samples.** (1) `t0` (just after Active entry); (2) simulated 2 x `TargetRoundMinutes` (default 12 min, so 24 min): jump the server clock, do not wait; (3) after an opposing elimination (`stealth.round.debugeliminate` on an opposing participant); (4) after a disconnect (force-drop a client). At each sample scan the live widget tree and capture a screenshot. The sample names are `t0`, `2xtarget`, `elimination`, `disconnect`; the files are `production/qa/evidence/round-noclock-t0.png`, `round-noclock-2xtarget.png`, `round-noclock-elimination.png` and `round-noclock-disconnect.png`.
- **What must not move.** Round adds no dwell timer, no sudden-death rule, no countdown and shows no clock (R16 (4), R23 (d)); the public struct has no clock source (`PhaseEnteredServerTime` and `ActiveAtServerTime` are 0 in Active), though a client could timestamp the phase change locally, which is why the binding rule is on presentation. "Eliminated" appears only as "eliminated for the round" after `Ended`; the words "dead", "lives", "revive", "spectator", "ghost" never appear (Roster language rule).
- **Retained means on disk.** The screenshots must still be in `production/qa/evidence/` when the story closes; a captured-and-discarded screenshot is not evidence (coding-standards, Test Evidence by Story Type).
- **Dependency on the HUD epic.** The widgets scanned live in the HUD epic (`production/epics/hud`); sequence this story after the HUD has an Active-round surface, or scan whatever widget tree exists and keep the allowlist current. Open: confirm against `design/ux/` (the GDD UX Flag asks for `/ux-design` specs for the briefing screen and end banner) which screens are in the Active-round tree.
- Manifest 2026-10-25f: opponent `ActiveCount`, `InformantCount`, `AwaitingPawn` and anything derived reach a client only as `FRoundResult.TeamSummary[2]` published in the round-end step.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 042 (`production/epics/round-match-framework/story-042-active-gate.md`): the Active-entry step this story's round runs on.
- Story 080 (`production/epics/round-match-framework/story-080-no-leak-before-ended.md`): the replicated-property audit that keeps counts off the wire.
- Story 082 (`production/epics/round-match-framework/story-082-host-parity-includes.md`): the host-HUD include rule.
- Story 054 (`production/epics/round-match-framework/story-054-team-summary-at-ended.md`): the `TeamSummary` data published at `Ended` (the HUD epic carries the display half of MVP item 9, user decision 2026-10-25).
- The widgets, the end banner and the briefing screen themselves (hud epic); post-`Ended` summary text is not scanned here.

---

## Files

*Planned files only. Tests live under the engine test roots (`tests/README.md`), not under `tests/`. `/dev-story` replaces this list with the files it actually changed.*

- `Source/StealthTestHarness/Private/Round/NoClockNoCountTest.cpp` (CQTest `NETWORK_TEST_CLASS(NoClockNoCount, "StealthGame.Round.PIE")`, full name `StealthGame.Round.PIE.NoClockNoCount.<Method>`; the GDD scenario name is `StealthGame.Round.NoClockNoCount`)
- `Source/StealthTestHarness/Private/Round/Data/NoClockWidgetBindingAllowlist.json` (checked-in allowlist of permitted widget bindings; path planned, `/dev-story` confirms)
- `production/qa/evidence/round-noclock-t0.png`, `round-noclock-2xtarget.png`, `round-noclock-elimination.png`, `round-noclock-disconnect.png` (retained screenshots)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: No widget binds elapsed time, a countdown or a count in a full Active round, with screenshots retained.
  - Setup: a [PIE-2] full Active round; the widget-tree scan and screenshot capture run at four samples: `t0`, simulated 2 x `TargetRoundMinutes`, after an opposing elimination, after a disconnect. The simulated time is a server-clock jump, not real time.
  - Verify: scan each sample's widget tree for bindings whose source is elapsed time, a countdown or a count; save a screenshot per sample to `production/qa/evidence/round-noclock-<sample>.png`.
  - Pass condition: every sample finds zero such bindings, and all four screenshots exist on disk at close. The AC does not define the predicate "binds elapsed time, a countdown or a count", so freeze a checked-in allowlist of permitted widget bindings; the scan must also fail if it finds zero widgets (non-vacuity). The AC names `<sample>` without values, so use `t0`, `2xtarget`, `elimination`, `disconnect`.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: UI
**Required evidence**:
- UI: retained screenshots of each sample in `production/qa/evidence/`: `round-noclock-t0.png`, `round-noclock-2xtarget.png`, `round-noclock-elimination.png`, `round-noclock-disconnect.png`; plus the [PIE-2] scan test `Source/StealthTestHarness/Private/Round/NoClockNoCountTest.cpp` (CQTest `NETWORK_TEST_CLASS` per the exemption in `.claude/rules/test-standards.md`).

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-042-active-gate.md`
- Unlocks: None
- Cross-epic: `production/epics/participant-state-roster` (elimination and disconnect states driving the samples; stories not yet created), `production/epics/session-travel` (travel to a full Active round; stories not yet created), `production/epics/tuning-data-config` (`TargetRoundMinutes` key, module skeleton and StealthTestHarness bootstrap; stories not yet created)
