# Story 087: AC-14c HotReloadAbsentPackaged

> **Epic**: Tuning Data & Config
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/tuning-data-config.md`
**Requirement**: `TR-tdc-010`; GDD AC-14c (CR-12)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0004: Tuning data pipeline and validator
**ADR Decision Summary**: PIE hot-reload exists only under `#if WITH_EDITOR`: the store subscribes to `FCoreUObjectDelegates::OnObjectPropertyChanged` there and nowhere else, so packaged, standalone-server and Shipping builds read assets once per round and never watch them.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: LOW
**Engine Notes**: No post-cutoff API for this story. `OnObjectPropertyChanged` is an editor-only delegate [V `UObjectGlobals.h:3293`]. Under the amended AC the `StealthGame` Game target has `WITH_EDITOR` 0, and a test that runs inside the editor always sees `WITH_EDITOR` 1, so the "no subscription when WITH_EDITOR is 0" half cannot be tested in-editor. It is covered by a source lint plus a Game-target build and binary scan. Whether the Game target builds on the dev machine without a cooked-content step is not stated in the inputs and is open. The Game-target half is deferred to a build that exists (see Implementation Notes).

**Control Manifest Rules (this layer)**:
- Required: Hot-reload, `?Tuning=` and console-command code sits inside `WITH_EDITOR` / `!UE_BUILD_SHIPPING` blocks (also `!UE_BUILD_TEST` for `stealth.tuning.preset`) — source: ADR-0004
- Required: Lints and analysis scripts emit observations only; pass/fail lives in the gate script — source: ADR-0008
- Required: Every leak, parity, statistical and lint gate includes a negative control the suite detects — source: ADR-0008

---

## Acceptance Criteria

*From GDD `design/gdd/tuning-data-config.md` (AC-14c), as amended for the source-lint form (amendment 087 REVISED and AMENDMENTS 2):*

- [ ] **AC-14c (CR-12)**: a source lint asserts every reference to `FCoreUObjectDelegates::OnObjectPropertyChanged` and the hot-reload sentinel sits inside `#if WITH_EDITOR`, and the `StealthGame` Game target (WITH_EDITOR 0) builds and its binary lacks the hot-reload sentinel, with a positive control (a Development editor build of the same sentinel is found); the Shipping scan half is story 089. (`StealthGame.Build.HotReloadGuardedByWithEditor`)

---

## Implementation Notes

*Derived from ADR-0004 section 9, Implementation Guideline 10, ADR-0008 sections 1, 3 and 8, and amendments G2 and G5:*

The original plan wording ("a static build-flag test in a Dev build") could not observe `WITH_EDITOR` 0 from inside the editor. The amended AC splits the check into a source lint (runs everywhere, seconds) and a Game-target build plus binary scan (needs a build, so it is deferred until one is available and then runs in the nightly job).

- Source lint (G2 part a): a T0 script under `tools/ci/lint/` that emits one `file:line rule message` observation per reference to `OnObjectPropertyChanged` or to the hot-reload sentinel found outside an `#if WITH_EDITOR` region. It scans `Source/StealthFoundation` first (G5 scope for source scans); other modules are scanned only if they reference the delegate. The script name is not given in the plan and is chosen here: `tools/ci/lint/hot-reload-guard-lint.sh`, with its fixtures under `tools/ci/lint/selftest/`. The gate script (story 011) decides pass/fail; the lint only emits.
- Selftest (G2 part a): a clean fixture passes and a seeded fixture with a reference outside `#if WITH_EDITOR` is reported by file:line (negative control, ADR-0008 s3). The T0 gate step runs it.
- Wrapper (G2 part b): `StealthGame.Build.HotReloadGuardedByWithEditor` in `Source/StealthFoundation/Private/Tests/Build/HotReloadGuardedByWithEditorTest.cpp` runs the script through `FPlatformProcess` and asserts exit code and observations. Flags `EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter`, inside `#if WITH_DEV_AUTOMATION_TESTS`; class `FBuildHotReloadGuardedByWithEditorTest`.
- Game-target scan: build the `StealthGame` Game target (WITH_EDITOR 0) and scan its binary for the hot-reload sentinel using the scan from story 088 (`tools/ci/sentinel-scan.ps1`, ASCII and UTF-16LE). Positive control: the same scan must find the sentinel in a Development editor build, so a stripped sentinel cannot pass vacuously. Both scans need a build and are deferred to the nightly job; until then the story's source-lint half is the observable evidence and the deferred half is recorded as NOT ASSESSED, never a pass.
- Sentinel header: the plan lists `Source/StealthFoundation/Private/Tuning/StealthTuningHotReloadSentinel.h`, while story 088 declares all back-door sentinels in `StealthTuningSentinels.h`. The two would duplicate the hot-reload literal. Resolve it at implementation: there must be exactly one literal per feature (story 088 rule). If 088 already declares the hot-reload sentinel, reference that one and do not add a second header. This is an open inconsistency in the plan.
- Prototype: none; no prototype code touches this path.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/tuning-data-config/story-085-hot-reload-pie.md`: the hot-reload code itself and its PIE behaviour (AC-14a).
- `production/epics/tuning-data-config/story-086-hot-reload-rejected.md`: the reject paths of the hot-reload filter (AC-14b).
- `production/epics/tuning-data-config/story-088-backdoor-code-sentinels.md`: declares the sentinels and the Development-binary positive-control scan.
- `production/epics/tuning-data-config/story-089-no-shipping-back-door-scan.md`: the Shipping binary scan half of AC-22 and AC-14c.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- tools/ci/lint/hot-reload-guard-lint.sh
- tools/ci/lint/selftest/hot-reload-guard-selftest.sh (plus clean and seeded fixtures beside it)
- Source/StealthFoundation/Private/Tests/Build/HotReloadGuardedByWithEditorTest.cpp
- Source/StealthFoundation/Private/Tuning/StealthTuningHotReloadSentinel.h (only if story 088 has not already declared the sentinel; see notes)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: AC-14c source lint and Game-target scan
  - Given: sources referencing `FCoreUObjectDelegates::OnObjectPropertyChanged` and the hot-reload sentinel; the `StealthGame` Game target (WITH_EDITOR 0).
  - When: the source lint runs; the Game target is built and scanned.
  - Then: every reference sits inside `#if WITH_EDITOR` (a seeded outside reference is detected); the Game binary lacks the sentinel while an editor or Development build contains it.
  - Edge cases: the Shipping scan half is story 089.

---

## Test Evidence

*Governed by `qa.level`: tests are required (qa.level is full).*

**Story Type**: Logic
**Required evidence**:
- Logic: the automation test `StealthGame.Build.HotReloadGuardedByWithEditor` in `Source/StealthFoundation/Private/Tests/Build/HotReloadGuardedByWithEditorTest.cpp` must exist and pass.
- Script story (G2): the selftest under `tools/ci/lint/selftest/` (clean pass and seeded violation detected) must run in the T0 gate step.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-085-hot-reload-pie.md`, `production/epics/tuning-data-config/story-088-backdoor-code-sentinels.md`
- Unlocks: None
- Open verification claims: the Game-target build and binary scan are deferred to a build that exists; the duplicate sentinel header between this story and 088 is unresolved in the plan.
