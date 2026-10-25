# Story 082: AC-8 LocalTamperIgnored (multi-process)

> **Epic**: Tuning Data & Config
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/tuning-data-config.md`
**Requirement**: `TR-tdc-009` (text lives in `docs/architecture/tr-registry.yaml`); GDD AC-8 (CR-4, CR-10, CR-13)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0004: Tuning data pipeline and validator (Implementation Guidelines 2 and 3); the multi-process tier is ADR-0008 section 1 (T3)
**ADR Decision Summary**: Tuning types never carry `Config`/`DefaultConfig`/`GlobalUserConfig`/`PerObjectConfig`, no gameplay value is read from ini, `Saved/Config`, the command line (Shipping) or a console variable, and client code reads only `GetClientTuning()` from the replicated snapshot and never the raw asset. A client that modifies its local asset, ini or console variables therefore gains no effect and creates no tell.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: Single-process PIE shares one asset object between clients, so "the client never reads its asset" cannot be proven there; AC-8 uses separate processes (GDD Edge Cases), hence T3. Needs the Gauntlet topology of story 020 (ADR-0008 Verification 3 [U]) and the snapshot property of story 073 (ADR-0004 Verification Required (1) [U], story 071). The `set` console command acts on a property of a live object by path; whether the harness drives it through the client console command path is not stated in the inputs. No post-cutoff engine API is used by the tamper itself.

**Control Manifest Rules (this layer)**:
- Forbidden: never mark tuning types `Config`/`DefaultConfig`/`GlobalUserConfig`/`PerObjectConfig`; never read a gameplay value from `UDeveloperSettings`, ini, `Saved/Config`, command line (Shipping) or a console variable — source: ADR-0004
- Forbidden: never let client or presentation code include a `Public/Server/` tuning header or reference `UStealthTuningData` — source: ADR-0004
- Forbidden: never prove multi-process criteria in in-process PIE only — source: ADR-0008
- Required: every leak and parity gate includes a negative control the suite detects (here: the tampered values are confirmed present locally) — source: ADR-0008

---

## Acceptance Criteria

*From GDD `design/gdd/tuning-data-config.md` AC-8, scoped to this story:*

- [ ] **AC-8 (CR-4, CR-10, CR-13)**: GIVEN a multi-process client launched with a locally modified tuning asset, a modified ini and a `set` console override (all with values outside the hard ranges), WHEN it plays a round against an unmodified Dev-build server running a non-`Default` preset, THEN every client-side `GetClientTuning()` read equals the server snapshot. A companion static check (`StealthGame.Tuning.NoClientAssetReference`) asserts that client-side code holds no reference to the tuning asset class. (`StealthGame.Tuning.LocalTamperIgnored`)

*(This story is the multi-process runtime half; the static check is story 069.)*

---

## Implementation Notes

*Derived from ADR-0004 Implementation Guidelines 2-3, GDD CR-13 and Edge Cases (local tamper, PIE shared asset), ADR-0008 sections 1, 2, 5:*

- **Scenario.** `Source/StealthTestHarness/Private/Scenarios/LocalTamperIgnoredScenario.cpp`, named scenario `LocalTamperIgnored`, test name `StealthGame.Tuning.MP.LocalTamperIgnored`. The plan lists no node script; whether the generic node of story 020 drives it or a node is added is open (record at implementation). Result JSON per story 018, `replicationSystem=legacy` asserted (story 020).
- **Server.** An unmodified Development host running a non-Default preset (the preset must differ from Default in public keys so that tampered values and server values are distinguishable). How the non-Default preset is selected on the host is open here; options are the `?Tuning=` option with a `HostSelectable` test entry (stories 058-061) or a test preset source; do not use Shipping.
- **Client tamper (three channels, all with values outside the hard ranges).** (1) a locally modified tuning asset, (2) a modified ini entry, (3) a `set` console override on the tuning object. The exact mechanism for (1) is open: the plan creates no `.uasset` in this run (ADR-0009 single-writer rule), so an in-memory modification of the locally loaded preset at scenario start by harness code (the harness module may reference `UStealthTuningData`; the static check of story 069 does not scan `StealthTestHarness`) is the likely shape; record the choice.
- **Pass observation.** Over the whole round, sample every `GetClientTuning()` read in the client process (every read, per the QA spec) and assert each value equals the server snapshot's value for that key; assert `bValid` and epoch are consistent. Record the number of reads in the capture so a vacuous pass (zero reads) fails.
- **Control (QA edge case).** Confirm the tampered values are present locally (read the local asset and the ini/console-modified values through harness-only access and assert they differ from the server's and are out of hard range). Without this, "reads equal the server's" could simply mean the tamper never took effect. Set `negativeControlDetected` accordingly in the result JSON.
- **Companion check.** The static check `StealthGame.Tuning.NoClientAssetReference` (story 069) must pass on the same tree; it is a separate story and is not a dependency in the plan's dependency list (073, 020 only).
- **Deferral.** DEFERRED: multi-process (nightly) per the QA spec, runnable once stories 020 and 073 are Done.
- **Prototype.** None. (The prototype `UStealthSettings` `Config` back door is retired by story 095; this story does not depend on it.)

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/tuning-data-config/story-069-no-client-asset-reference.md`: the static half (no client reference to `UStealthTuningData` or a `Server/` tuning header)
- `production/epics/tuning-data-config/story-073-gamestate-snapshot-public-only.md`: the replicated snapshot and its public-only checks
- `production/epics/tuning-data-config/story-089-no-shipping-back-door-scan.md`: the Shipping binary scan for back doors
- `production/epics/tuning-data-config/story-095-retire-prototype-settings-canonical-fields.md`: removing the prototype `Config` fields
- `production/epics/tuning-data-config/story-020-gauntlet-multiprocess-smoke.md`: the Gauntlet host plus client smoke

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthTestHarness/Private/Scenarios/LocalTamperIgnoredScenario.cpp`
- `Build/Scripts/` node script, only if the story-020 node cannot drive this scenario (open)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: a client with a tampered asset, ini and console override reads only the server snapshot
  - Given: a client with a modified tuning asset, a modified ini and a `set` console override, all outside hard ranges, against an unmodified Dev server running a non-Default preset (DEFERRED: multi-process; test `StealthGame.Tuning.MP.LocalTamperIgnored`)
  - When: it plays a round
  - Then: every `GetClientTuning()` read equals the server snapshot; the companion `NoClientAssetReference` (story 069) passes
  - Edge cases: every read is sampled; the tampered values are confirmed present locally (control)

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: the T3 scenario `Source/StealthTestHarness/Private/Scenarios/LocalTamperIgnoredScenario.cpp` (`StealthGame.Tuning.MP.LocalTamperIgnored`) run by the Gauntlet node must exist and pass (nightly, deferred multi-process), with its result JSON retained

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-073-gamestate-snapshot-public-only.md`, `production/epics/tuning-data-config/story-020-gauntlet-multiprocess-smoke.md`
- Unlocks: None
- Open verification claims: ADR-0008 Verification 3 (Gauntlet topology) [U] and 7 [U]; ADR-0004 Verification Required (1) [U] (story 071). Open: how the non-Default preset is selected on the host, how the asset tamper is produced without creating a `.uasset`, node reuse.
