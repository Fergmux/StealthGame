# Story 035: A Shipping listen-server build exposes no server data through commands or overlays and writes the Event Log only at Ended or Reset

> **Epic**: Information Visibility Policy
> **Status**: Blocked — BLOCKED: needs the Event Log store and the Ended/Reset disk writer (round-match-framework) and debug-command guards; a Shipping listen-server build.
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

The listen host runs the authoritative server in the same process as its own client, so any debug command, overlay or log that reads server data is a direct view of every hidden fact (who is a player, who is concealed, the Event Log). The policy accepts that a determined host can read process memory (L21, recorded as accepted and not a pass condition), but a Shipping build must not offer the data through the console, an exec command, an overlay, a demo recording or a premature Event Log write. This story proves the Shipping listen-server surface is closed: the command registry equals a checked-in allowlist, demo recording cannot start, and the Event Log is written to disk only at `Ended` or `Reset`.

**GDD**: `design/gdd/information-visibility-policy.md`
**Requirement**: `TR-ivp-007` (Leak catalogue with CRITICAL ship gate; per-commit canaries)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0007: Code modules and include boundaries (section 6, logging policy; debug-command rules); ADR-0002: Resolution queue and round-end staging (Event Log store and the `Ended`/`Reset` writer)
**ADR Decision Summary**: Event Log content, `DealDigest`, server sub-seeds, bindings and mailbox order never reach a log, message, ensure/check text or trace; debug commands such as `stealth.round.dumplog` are `#if !UE_BUILD_SHIPPING` and refuse unless the phase is `Ended`/`Reset`; the Event Log is exported to disk only by the store's writer at `Ended` or `Reset`.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: Console command registration (`IConsoleManager`, `FAutoConsoleCommand`, exec handlers) and demo recording (`DemoRecDriver`, `StartRecordingReplay`) are pre-cutoff engine APIs; whether the Shipping listen build still exposes console or exec paths through the project's own command registry is verified here against the registry and a runtime attempt, not assumed. Needs a cooked Shipping listen-server build (a deferred, nightly or milestone check).

**Control Manifest Rules (this layer)**:
- Required: **Debug commands such as `stealth.round.dumplog` are `#if !UE_BUILD_SHIPPING` and refuse unless the phase is `Ended`/`Reset`** — source: ADR-0007
- Required: **Hot-reload, `?Tuning=` and console-command code sits inside `WITH_EDITOR` / `!UE_BUILD_SHIPPING` blocks (also `!UE_BUILD_TEST` for `stealth.tuning.preset`)** — source: ADR-0004
- Forbidden: **Never route queue Event Log entries through `UE_LOG`/`UE_LOGF`, the output log or trace channels** — source: ADR-0002
- Forbidden: **Never pass Event Log content, `DealDigest`, server sub-seeds, bindings or mailbox order to `UE_LOG`/`UE_LOGF`/`ensureMsgf`/`checkf`/`ClientMessage`/`TRACE_*`/`CSV_*`/`SCOPE_CYCLE_COUNTER` names** — source: ADR-0007

---

## Acceptance Criteria

*From GDD `design/gdd/information-visibility-policy.md` (AC-IVP-18), scoped to this story:*

- [ ] **AC-IVP-18** [Int][Doc] `StealthGame.Visibility.ListenHostHardening`: GIVEN a Shipping listen-server build, WHEN symbols, console commands and file writes are inspected during a round, THEN no debug command or overlay reads server data, demo recording cannot start, and the Event Log is written to disk only at `Ended` or `Reset`. The host memory caveat (L21) is recorded as accepted, not a pass condition. (R10) Clarification (story-level, QL-STORY-READY): "no debug command or overlay reads server data" means the Shipping listen-server console/exec command registry equals a checked-in allowlist, and a file-write trace during a round shows Event Log writes only at `Ended` or `Reset`.

---

## Implementation Notes

*Derived from ADR-0007 section 6 and Implementation Guidelines, ADR-0002 sections 8.4 and the Event Log store, ADR-0008 sections 1, 3, 7 and 8:*

- Evidence has three parts, all on a cooked Shipping listen-server build: (1) the console and exec command registry (enumerated at runtime and by symbol inspection) equals a checked-in command allowlist; the allowlist is a data file whose location this story chooses and records; (2) a demo-recording attempt fails (`StartRecordingReplay`/the demo net driver is not available); (3) a file-write trace during a full round shows Event Log writes only at `Ended` or `Reset`, through the Event Log store's writer (`.tmp` file renamed on completion for the telemetry section is ADR-0023's rule and is separate).
- Include the case of a command added to the registry but missing from the allowlist: the check must fail (this is the negative control, ADR-0008 section 3). A harness-only variant of the registry that adds one extra command is used to prove the comparison detects it; the control type lives in the harness only and is never compiled into Shipping.
- The host memory caveat (L21) is accepted and recorded as such; it is not a pass condition and not tested. The residual leak rows (L14, L21) appear in the release checklist as "not shippable beyond trusted playtests" (story-047).
- Debug commands are compiled out of Shipping (`#if !UE_BUILD_SHIPPING`), and `stealth.round.dumplog` additionally refuses unless the phase is `Ended` or `Reset` in non-Shipping builds. Overlay code that reads server stores is likewise `!UE_BUILD_SHIPPING`. The Shipping-symbol check in the ADR-0008 nightly job (no `StealthTestHarness` or `WITH_STEALTH_TEST_SEAMS` symbols) is reused here for the symbol inspection.
- This story is a deferred, build-level check: it runs in the nightly or milestone gate against a Shipping listen-server build (ADR-0008 section 8), not per commit. Record the result and the file-write trace under `production/qa/evidence/`.
- Prototype: Replaces: nothing. Promotes: nothing. The prototype's debug exec commands (if any) in `Source/StealthGame` are not promoted and must not appear in a Shipping build.
- Cross-epic: needs round-match-framework's Event Log store and `Ended`/`Reset` disk writer; the host and debug-command guards of session-travel (host commands registered on the game instance with the net-mode/host check, ADR-0005); a cookable Shipping listen-server target from tuning-data-config Exit A.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- story-047: the leak catalogue review and release-checklist rows (L14, L21 as accepted)
- story-049: the external-build critical gate
- story-034: presentation include and banned-symbol lint
- round-match-framework epic: the Event Log store and writer; session-travel epic: host commands

---

## Files

*The code and test files this story creates or changes (planned):*

- `Source/StealthTestHarness/Private/Scenarios/ListenHostHardeningScenario.cpp` (T3 scenario, `StealthGame.Visibility.MP.ListenHostHardening`, run against a Shipping listen-server build) and its Gauntlet node in `Build/Scripts/`
- the checked-in console/exec command allowlist (location chosen by this story, under `tools/ci/` or `Source/StealthGame/Config/`)
- `production/qa/evidence/listen-host-hardening-evidence.md` (registry comparison, demo-recording attempt, file-write trace)

*Note: the `StealthTestHarness` module folder is created by tuning-data-config Exit A (it does not exist yet).*

---

## QA Test Cases

- **AC-1**: Listen host hardening (AC-IVP-18)
  - Given: a Shipping listen-server build and a checked-in command allowlist.
  - When: symbols, console commands and file writes are inspected during a round.
  - Then: the console and exec command registry equals the allowlist, so no debug command or overlay reads server data; demo recording cannot start (an attempt fails); a file-write trace shows Event Log writes only at `Ended` or `Reset`.
  - Edge cases: the host memory caveat (L21) is accepted, not a pass condition; include a command added to the registry but missing from the allowlist.

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: a T3 scenario under `Source/StealthTestHarness/` OR a documented playtest or spike report (here: `production/qa/evidence/listen-host-hardening-evidence.md` from a Shipping listen-server build)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: round-match-framework epic (Event Log store and `Ended`/`Reset` disk writer); session-travel epic (host and debug-command guards); tuning-data-config Exit A (a Shipping listen-server target and the harness)
- Unlocks: None
