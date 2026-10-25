# Story 089: AC-22 NoShippingBackDoor (CI script)

> **Epic**: Tuning Data & Config
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/tuning-data-config.md`
**Requirement**: `TR-tdc-009`; GDD AC-22 (CR-2, CR-13)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0004: Tuning data pipeline and validator
**ADR Decision Summary**: Shipping always resolves `Default`; the preset option parser, the preset and dump console commands and the hot-reload hook are compiled out and each excluded block carries a sentinel literal. The sentinel scan has a Development-binary positive control (story 088) so the Shipping result is not vacuous.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: LOW
**Engine Notes**: No post-cutoff API for this story. Two claims are open. (a) A Development client joining a Shipping listen host is [U]: the inputs do not confirm that the two build configurations are network-compatible on one machine. (b) Gauntlet launching a cooked host and a client on one machine is [U] (ADR-0008 Verification 3, story 020). The GDD wording says the Shipping build "logs `PresetId` `Default` in its server log". Shipping compiles logging out by default, so the observation channel is instead the replicated snapshot field read by a Development client; the GDD wording needs a change request to its owner (a request only; this story does not edit the GDD). This story needs a packaged Shipping build and is deferred until one exists.

**Control Manifest Rules (this layer)**:
- Forbidden: Never read a gameplay value from `UDeveloperSettings`, ini, `Saved/Config`, command line (Shipping) or a console variable — a live back door (TDC CR-13) — source: ADR-0004
- Required: Hot-reload, `?Tuning=` and console-command code sits inside `WITH_EDITOR` / `!UE_BUILD_SHIPPING` blocks (also `!UE_BUILD_TEST` for `stealth.tuning.preset`) — source: ADR-0004
- Forbidden: Never prove multi-process criteria in in-process PIE only — source: ADR-0008

---

## Acceptance Criteria

*From GDD `design/gdd/tuning-data-config.md` (AC-22), as amended (amendment 089 REVISED):*

- [ ] **AC-22 (CR-2, CR-13)**: the packaged Shipping binary scanned for the hot-reload, preset-option and console-command sentinels (ASCII and UTF-16LE) contains none, and a Shipping listen host launched with `?Tuning=Playtest` (Playtest HostSelectable) is observed by a Development client joining it to replicate snapshot PresetId `Default` (Shipping compiles logging out by default, so the GDD's "server log" wording is satisfied through the public snapshot field; wording change request to the TDC GDD owner). (`StealthGame.Tuning.NoShippingBackDoor`, CI script)

---

## Implementation Notes

*Derived from ADR-0004 section 9 and ADR-0008 sections 1 (T3) and 8, and amendment G2:*

- Scan: `tools/ci/shipping-backdoor-scan.ps1` (the plan's `TOOLS/shipping-backdoor-scan.ps1`) reuses the scan from story 088 over the packaged Shipping binary and emits one observation per sentinel (ASCII and UTF-16LE). Any sentinel found is a failure observation. The gate script decides pass/fail, never the scan. No binary supplied means NOT ASSESSED (nonzero), never a pass. The positive control is story 088's Development scan: run it in the same job so a stripped sentinel cannot make this scan pass.
- Join observation: `Build/Scripts/StealthShippingPresetNode.cs` is a Gauntlet node (T3, ADR-0008 s1) that launches a packaged Shipping listen host with `?Tuning=Playtest` (Playtest is HostSelectable in the catalog, so only the Shipping guard can force `Default`) and a Development client that joins it. The Development client runs a harness scenario that reads the replicated `FStealthTuningSnapshot.PresetId` and writes result JSON via `-StealthHarnessOut`. The node asserts `PresetId == Default`. A client that fails to join is not a pass. The Shipping host has no harness (the harness module is excluded from Shipping and Test), which is why the observation is client-side. The plan lists no scenario file for this client read; reuse the story 020 scenario mechanism (the name is open).
- Replication system: the client harness asserts and records the legacy driver, as for every T2/T3 run (ADR-0008 s5).
- Build/ must not be ignored by `.gitignore` (story 024).
- G2 for this CI-script story: (a) a selftest under `tools/ci/selftest/` (a clean fixture binary passes; a fixture binary containing a sentinel is detected; empty input is NOT ASSESSED); (b) an automation wrapper `StealthGame.Build.NoShippingBackDoorScan` (name chosen here; the plan gives none) in `Source/StealthFoundation/Private/Tests/Build/NoShippingBackDoorScanTest.cpp` that runs the script on those fixtures through `FPlatformProcess` and asserts exit code and observations. The GDD criterion name `StealthGame.Tuning.NoShippingBackDoor` is the CI script's label (GDD: "AC-22 ... are CI scripts, not automation tests").
- The nightly job (story 023) runs this after a Shipping package step; that package step is part of the nightly work, not this story.
- Prototype: none.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/tuning-data-config/story-088-backdoor-code-sentinels.md`: places the sentinels and runs the Development positive-control scan.
- `production/epics/tuning-data-config/story-087-hot-reload-absent-packaged.md`: Game-target (WITH_EDITOR 0) scan of the hot-reload sentinel.
- `production/epics/tuning-data-config/story-023-nightly-workflow.md`: the nightly job that runs this script and the Shipping package.
- `production/epics/tuning-data-config/story-020-gauntlet-multiprocess-smoke.md`: the Gauntlet host-plus-client machinery this node builds on.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- tools/ci/shipping-backdoor-scan.ps1
- Build/Scripts/StealthShippingPresetNode.cs
- tools/ci/selftest/shipping-backdoor-scan-selftest.ps1 (plus fixtures)
- Source/StealthFoundation/Private/Tests/Build/NoShippingBackDoorScanTest.cpp
- Source/StealthTestHarness/Private/Scenarios/ (client snapshot-read scenario; file name not in the plan, open)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: AC-22 Shipping scan and join observation
  - Given: a packaged Shipping binary; the sentinel positive control (story 088); a Shipping listen host launched with `?Tuning=Playtest` (Playtest HostSelectable); a Development client.
  - When: the binary is scanned (ASCII and UTF-16LE); the client joins the host.
  - Then: no hot-reload, preset-option or console-command sentinel is found; the joined client's replicated snapshot `PresetId` is `Default`.
  - Edge cases: no binary means NOT ASSESSED; a client that fails to join is not a pass; wording change request to the GDD owner.

---

## Test Evidence

*Governed by `qa.level`: tests are required (qa.level is full).*

**Story Type**: Integration
**Required evidence**:
- Integration: the Gauntlet node `Build/Scripts/StealthShippingPresetNode.cs` run result (documented T3 run) plus the scan observations from a packaged Shipping build. DEFERRED until a packaged Shipping build exists.
- Script story (G2): the selftest under `tools/ci/selftest/` and the wrapper `StealthGame.Build.NoShippingBackDoorScan` (must exist and pass).

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-088-backdoor-code-sentinels.md`, `production/epics/tuning-data-config/story-023-nightly-workflow.md`
- Unlocks: None
- Open verification claims: Development client joining a Shipping host is [U]; Gauntlet one-machine topology is [U] (ADR-0008 Verification 3); the GDD "server log" wording needs a change request to the TDC GDD owner.
