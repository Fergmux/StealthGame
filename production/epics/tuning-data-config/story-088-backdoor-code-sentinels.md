# Story 088: Back-door code behind guards with sentinel literals (AC-22 positive control)

> **Epic**: Tuning Data & Config
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/tuning-data-config.md`
**Requirement**: `TR-tdc-009`; GDD AC-22 (CR-2, CR-13), positive-control half
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0004: Tuning data pipeline and validator
**ADR Decision Summary**: Shipping always resolves `Default`: the `?Tuning=` parser, `stealth.tuning.preset` (also excluded from Test) and `stealth.tuning.dump` are not compiled into it. Each excluded block holds a unique sentinel literal for the AC-22 binary scan, and the same scan must find every sentinel in the Development binary so a linker-stripped sentinel cannot make the Shipping scan pass vacuously.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: LOW
**Engine Notes**: No post-cutoff API for this story; logging uses `UE_LOGF` (5.8) [V `LogMacros.h`]. The claim that a referenced literal survives dead-stripping in a Development build, in both ASCII and UTF-16LE forms, is an expectation of ADR-0004 s9 and is not marked [V] there. This story's scan is the observation that settles it. Which binary is scanned (a module DLL, or the monolithic executable) depends on the link mode of the target and is open until a build is inspected.

**Control Manifest Rules (this layer)**:
- Required: Hot-reload, `?Tuning=` and console-command code sits inside `WITH_EDITOR` / `!UE_BUILD_SHIPPING` blocks (also `!UE_BUILD_TEST` for `stealth.tuning.preset`) — source: ADR-0004
- Forbidden: Never mark tuning types Config/DefaultConfig/GlobalUserConfig/PerObjectConfig; never read a gameplay value from `UDeveloperSettings`, ini, `Saved/Config`, command line (Shipping) or a console variable — source: ADR-0004
- Required: Lints and analysis scripts emit observations only; pass/fail lives in the gate script — source: ADR-0008

---

## Acceptance Criteria

*From GDD `design/gdd/tuning-data-config.md` (AC-22 positive-control half, per plan story 088):*

- [ ] **AC-22 positive-control half**: the `?Tuning=` option parser, `stealth.tuning.preset` (excluded from Test too) and `stealth.tuning.dump` console commands and the hot-reload hook sit inside `!UE_BUILD_SHIPPING` / `!UE_BUILD_TEST` / `WITH_EDITOR` blocks that each contain a unique sentinel literal referenced once at registration, and a Development-binary scan finds every sentinel (ASCII and UTF-16LE), so a stripped sentinel cannot make the Shipping scan pass vacuously.

---

## Implementation Notes

*Derived from ADR-0004 section 9, Implementation Guidelines 2 and 10, ADR-0008 sections 3 and 8, and amendment G2:*

- Guard map (CR-13, ADR-0004 s9): `?Tuning=` parsing and `stealth.tuning.dump` inside `#if !UE_BUILD_SHIPPING`; `stealth.tuning.preset` inside `#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST`; the hot-reload hook (story 085) inside `#if WITH_EDITOR`. In Shipping, `ResolveRoundPreset` returns `Default` unconditionally. `HostSelectable` is honoured only in Development and Test.
- Sentinels: one unique literal per feature (four: option parser, preset command, dump command, hot-reload hook), declared in `Source/StealthFoundation/Private/Tuning/StealthTuningSentinels.h`. Each is referenced exactly once at registration, for example logged once through `UE_LOGF` on `LogStealthFoundation`, so the compiler and linker cannot strip it. The literal text must be unique across the project so a scan hit identifies one feature. The sentinel text is not itself a gameplay value, so it is not a tuning key.
- Register the commands in `Source/StealthFoundation/Private/Tuning/StealthTuningCommands.cpp`. `stealth.tuning.preset <name>` sets the stored preset name on `UStealthTuningStore` (effect at the next `Preparing`, story 090); `stealth.tuning.dump` prints preset, hashes, fallback level and values (CR-13). Tuning values are not secret, so logging them is allowed.
- Scan script: `tools/ci/sentinel-scan.ps1` (the plan's `TOOLS/sentinel-scan.ps1`). It reads a binary, searches for each sentinel as ASCII bytes and as UTF-16LE bytes, and emits one observation per sentinel (found or missing, with the encoding). It emits observations only; pass/fail is decided by a gate script. With no input binary it reports NOT ASSESSED (nonzero), never a pass.
- Positive control and negative control (ADR-0008 s3): run against a Development binary, every sentinel must be found. A selftest strips one sentinel from a copy of the input (or scans a fixture binary without it) and the gate must fail, proving a missing sentinel is detected.
- G2 for this script story: (a) a selftest under `tools/ci/selftest/` (a clean Development scan passes; a seeded missing sentinel is detected); (b) an automation wrapper `StealthGame.Build.BackDoorSentinelScan` (wrapper name chosen here; the plan gives none) in `Source/StealthFoundation/Private/Tests/Build/BackDoorSentinelScanTest.cpp` that runs the script through `FPlatformProcess` and asserts exit code and observations. Flags `EditorContext | ProductFilter` inside `#if WITH_DEV_AUTOMATION_TESTS`.
- The Shipping half is story 089 and needs a packaged Shipping build; this story needs only a Development build.
- Prototype: none. The prototype `UStealthSettings` is retired by story 095, not here.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/tuning-data-config/story-085-hot-reload-pie.md`: the hot-reload behaviour inside the `WITH_EDITOR` block.
- `production/epics/tuning-data-config/story-058-tuning-store-default-preset.md`: the store and `ResolveRoundPreset` plumbing this story guards.
- `production/epics/tuning-data-config/story-087-hot-reload-absent-packaged.md`: source lint and Game-target scan for the hot-reload sentinel.
- `production/epics/tuning-data-config/story-089-no-shipping-back-door-scan.md`: the packaged Shipping scan and the Shipping `?Tuning=Playtest` observation.
- `production/epics/tuning-data-config/story-090-preset-change-next-round.md`: the behaviour of `stealth.tuning.preset` across rounds.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- Source/StealthFoundation/Private/Tuning/StealthTuningCommands.cpp
- Source/StealthFoundation/Private/Tuning/StealthTuningSentinels.h
- tools/ci/sentinel-scan.ps1
- tools/ci/selftest/sentinel-scan-selftest.ps1 (plus fixtures)
- Source/StealthFoundation/Private/Tests/Build/BackDoorSentinelScanTest.cpp

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: Development-binary sentinel scan (positive control)
  - Given: the `?Tuning=` parser, `stealth.tuning.preset` (excluded from Test too), `stealth.tuning.dump` and the hot-reload hook each in a `!UE_BUILD_SHIPPING` / `!UE_BUILD_TEST` / `WITH_EDITOR` block with a unique sentinel literal referenced once at registration.
  - When: `sentinel-scan.ps1` runs over a Development binary, ASCII and UTF-16LE.
  - Then: every sentinel is found (positive control); stripping one makes the scan fail.
  - Edge cases: a sentinel optimised out of the binary; one literal per feature (no shared literal).

---

## Test Evidence

*Governed by `qa.level`: tests are required (qa.level is full).*

**Story Type**: Integration
**Required evidence**:
- Integration: the scan run over a Development binary, recorded as a documented run, plus the automation wrapper `StealthGame.Build.BackDoorSentinelScan` in `Source/StealthFoundation/Private/Tests/Build/BackDoorSentinelScanTest.cpp` (must exist and pass).
- Script story (G2): the selftest under `tools/ci/selftest/` (clean pass and seeded missing sentinel detected).

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-058-tuning-store-default-preset.md`, `production/epics/tuning-data-config/story-085-hot-reload-pie.md`
- Unlocks: `production/epics/tuning-data-config/story-087-hot-reload-absent-packaged.md`, `production/epics/tuning-data-config/story-089-no-shipping-back-door-scan.md`, `production/epics/tuning-data-config/story-090-preset-change-next-round.md`
- Open verification claims: that referenced sentinels survive in a Development binary in both encodings is observed here for the first time; the scanned binary (module DLL versus monolithic exe) is open.
