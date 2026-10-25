# Story 054: AC-32 FrozenImmutable: FValidatedTuning has no mutator

> **Epic**: Tuning Data & Config
> **Status**: Complete
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: 2026-10-25

## Context

**GDD**: `design/gdd/tuning-data-config.md`
**Requirement**: `TR-tdc-005` (text lives in `docs/architecture/tr-registry.yaml`); GDD AC-32, CR-7

*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

Exit B-2 of the epic. CR-7 freezes the validated tuning at `Preparing` and stamps it with `TuningHash`; the freeze is only meaningful if no server system can change the set afterwards. ADR-0004 section 5 makes `FValidatedTuning` immutable by construction (const members exposed through `const&`, held as `TSharedRef<const FValidatedTuning>`, no mutator). This story proves it two ways: static checks inside the automation test, and a negative compile probe that tries to write a member and must fail to compile, with a positive control (a const read of the same member compiles) so the probe cannot fail for an unrelated reason. The hot-reload path (CR-12, PIE only) is the one sanctioned way to produce a new set and is excluded here (story 085).

**ADR Governing Implementation**: ADR-0004: Tuning data pipeline and validator
**ADR Decision Summary**: `FValidatedTuning` is immutable after construction, exposed through `const&` only and held as `TSharedRef<const FValidatedTuning>`; the hash is the project's own FNV-1a-64 over the F-2 byte stream and the frozen hash must not change through the round.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: LOW
**Engine Notes**: Pre-cutoff APIs only. The compile probe depends on UBT `-SingleFile=` behaviour in the story 005 runner, which is [V `BuildMode.cs:880-943`] in ADR-0008 Verification 4 (observed once by story 005). The exact compiler error substring the probe expects is not in the inputs: it is open and must be copied from the first observed failure of the probe.

**Control Manifest Rules (this layer)**:
- Required: server code reads only `GetServerTuning()` / `FValidatedTuning`, never the raw asset — source: ADR-0004
- Required: every leak, parity and lint gate includes a negative control the suite detects; compile probes carry a positive control — source: ADR-0008
- Forbidden: never disable, skip or reseed a failing test to pass a gate; never mark a gate Done that was never watched to fail — source: ADR-0008
- Forbidden: never hash with `GetTypeHash`, `HashStringFNV1a` or a struct memory image (the hash read at `Ended` is the story 045 hasher's) — source: ADR-0004

---

## Acceptance Criteria

*From GDD `design/gdd/tuning-data-config.md`, scoped to this story:*

- [x] **AC-32 (CR-7)**: GIVEN a frozen set, WHEN any server system attempts to modify it outside the PIE hot-reload path, THEN the API offers no mutator (const access, compile-level), and the frozen hash is unchanged at `Ended`. (`StealthGame.Tuning.FrozenImmutable`)

---

## Implementation Notes

*Derived from ADR-0004 section 5 (Freeze, hashes, self-check), Key Interfaces, and ADR-0008 section 7 (compile-fail probes):*

- **Header.** `Source/StealthFoundation/Public/Server/StealthValidatedTuning.h` (server-only: guard include first, `Public/Server/`). Per ADR-0004 Key Interfaces the type exposes `Values()`, `TuningHash()`, `ClientHash()`, `SchemaVersion()`, `PresetId()`, `Revision()`, `FallbackLevel()` and `Report()`, all `const`, returning `const&` or values. Members are `const`; the copy-assignment operator is deleted or the members are const so assignment fails. It is a plain C++ struct (not a USTRUCT or UObject). It is held as `TSharedRef<const FValidatedTuning>` (the resolve result of story 050 carries a `TSharedPtr<const FValidatedTuning>`).
- **Ordering flag (inconsistency).** Story 050 already returns `TSharedPtr<const FValidatedTuning>`, but the plan lists `StealthValidatedTuning.h` only here, after 050. Either 050 creates a minimal declaration that this story finalises, or the file is created in 050; `/dev-story` should settle it. This story owns the immutability guarantee either way.
- **Static checks inside the test.** `static_assert` that every public member function is `const`-qualified and that `std::is_assignable_v` and `std::is_copy_assignable_v` are false for `FValidatedTuning`, plus a check that the holder type is `TSharedRef<const FValidatedTuning>`. These are the in-process half of "const access, compile-level".
- **Compile probe.** `tools/ci/compile-probes/MutateValidatedTuning.cpp` (the plan's `TOOLS/` means `tools/ci/`), run by the story 005 runner (header comments `// EXPECT-ERROR: "<substring>"` and `// TARGET-MODULE: <module>`; the target module is not stated in the inputs and is open, a Foundation or composition-root module are both plausible because the probe only needs the Foundation header). The probe attempts to write a member through a `const FValidatedTuning&` (and through a `TSharedRef<const FValidatedTuning>`) and must fail to compile. The positive control is a second probe or control file that performs a const read of the same member and must compile. Both are listed in the probe set so the runner reports the pair. The runner and its gate are story 005; do not edit them beyond adding probe files.
- **const_cast.** The earlier draft spec noted `const_cast` is not covered; the revised spec keeps it out: a deliberate cast is a code-review matter, not a type-system guarantee. State this in a comment in the test.
- **Frozen hash at Ended.** Drive a round to `Ended` with `FStubRound` (story 052): resolve, freeze, record `TuningHash`, run the stub round through its phases to `Ended`, read `TuningHash` again and assert it is unchanged. The hash value comes from the story 045 hasher; the test does not recompute it independently.
- **Excluded.** The hot-reload path (story 085) is the only sanctioned producer of a new frozen set (with `Revision + 1`) and is not exercised here.
- **Test placement and naming.** `Source/StealthFoundation/Private/Tests/Tuning/FrozenImmutableTest.cpp`, class `FTuningFrozenImmutableTest`, name `StealthGame.Tuning.FrozenImmutable`, `IMPLEMENT_SIMPLE_AUTOMATION_TEST` with `EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter`, inside `#if WITH_DEV_AUTOMATION_TESTS`. The automation test cannot itself launch UBT; the probe pair is evidenced by the story 005 runner's observations (per-commit when a `Public/Server/` header changed, otherwise nightly per ADR-0008 section 8).
- **Prototype.** None.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/tuning-data-config/story-005-compile-probe-runner-and-guard-probe.md`: the probe runner, its gate and the guard probe
- `production/epics/tuning-data-config/story-050-resolve-fallback-to-default.md`: `Resolve()` and the result that carries the frozen set
- `production/epics/tuning-data-config/story-045-hasher-golden.md`: the hasher whose output is compared at `Ended`
- `production/epics/tuning-data-config/story-052-round-session-stubs.md`: `FStubRound`
- `production/epics/tuning-data-config/story-085-hot-reload-pie.md`: the PIE hot-reload path that legitimately produces a new set
- `production/epics/tuning-data-config/story-068-server-key-not-on-client.md`: the other compile-fail probe (server-only key on the client face)

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthFoundation/Public/Server/StealthValidatedTuning.h`
- `Source/StealthFoundation/Private/Tests/Tuning/FrozenImmutableTest.cpp`
- `tools/ci/compile-probes/MutateValidatedTuning.cpp`
- a const-read positive-control probe file beside it (name proposed: `tools/ci/compile-probes/ReadValidatedTuningControl.cpp`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: `FValidatedTuning` offers only const access (compile level) and the frozen hash is unchanged at `Ended` (test `StealthGame.Tuning.FrozenImmutable` plus the compile probe)
  - Given: `FValidatedTuning` held as `TSharedRef<const FValidatedTuning>`; the `MutateValidatedTuning.cpp` probe and its const-read control; `FStubRound`
  - When: the static asserts run; the probe and the control are compiled (story 005 runner); the round is driven to `Ended`
  - Then: only const accessors exist; the mutation probe fails with the expected message and the const read of the same member compiles; `TuningHash` is unchanged at `Ended`
  - Edge cases: the hot-reload path is excluded (story 085)

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Tuning/FrozenImmutableTest.cpp` must exist and pass (`StealthGame.Tuning.FrozenImmutable`); the probe pair `tools/ci/compile-probes/MutateValidatedTuning.cpp` and its control must be reported by the story 005 runner (mutation fails, control compiles)

**Status**: [x] Created and passing (`StealthGame.Tuning.FrozenImmutable`)

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-050-resolve-fallback-to-default.md`, `production/epics/tuning-data-config/story-005-compile-probe-runner-and-guard-probe.md`, `production/epics/tuning-data-config/story-052-round-session-stubs.md`
- Unlocks: None
- Open verification claims: the plan Note says the compile probe depends on story 005 (its `-SingleFile=` behaviour is [V `BuildMode.cs:880-943`], observed in 005). Open items found while writing: the expected compiler error substring, the probe's `TARGET-MODULE`, and where `StealthValidatedTuning.h` is first created (050 versus this story).

---

## Completion Notes
**Completed**: 2026-10-25
**Criteria**: 1/1 passing (static_asserts: FValidatedTuning is not copy-, move- or otherwise assignable, each of the eight accessors is a const member function returning a value or a const reference, the result holds TSharedPtr<const FValidatedTuning> convertible to TSharedRef<const FValidatedTuning>; at run time a round on FStubRound is driven Preparing (resolve, freeze, OnTuningReady) to Playing to Ended, and TuningHash, every frozen value and the hasher's hash of the frozen values are unchanged at Ended, a second resolve producing a distinct object; compile probes: MutateValidatedTuning.cpp failed with the expected text and its const-read control compiled, gate PASS (2 probes) in `production/qa/evidence/story-054-validated-tuning-frozen-immutable/probe-run-pass.txt`)
**Watched to fail**: the first probe run used a guessed EXPECT-ERROR (C3490) and the gate reported FAIL "failed with a different message" (`probe-run1-wrong-expect.txt`); the observed MSVC text, error C3892 "you cannot assign to a variable that is const", was then copied into the probe header
**Decisions**: probe TARGET-MODULE and CONTROL-MODULE StealthFoundation; the control lives at `tools/ci/compile-probes/controls/ReadValidatedTuningControl.cpp` (the runner globs only the top-level *.cpp, so a control beside the probe would be read as a probe); the header was created in story 050 and finalised here; const_cast is noted in the test as a code-review matter
**Files changed beyond the list**: the control path is `tools/ci/compile-probes/controls/ReadValidatedTuningControl.cpp` (the proposed name, one folder down)
**Deviations**: None
**Test Evidence**: `Source/StealthFoundation/Private/Tests/Tuning/FrozenImmutableTest.cpp`; `production/qa/evidence/story-054-validated-tuning-frozen-immutable/` (stamp, probe runs, full suite); full suite 70/70 green (`tools/dev/cycle.ps1 -Tests -NoLaunch`: Result: Succeeded, tests exit=0)
**Run result**: N/A — immutable value type and compile probes; nothing player-visible
**Reviewed by**: focused code review (self-applied, unattended run): no defects left
