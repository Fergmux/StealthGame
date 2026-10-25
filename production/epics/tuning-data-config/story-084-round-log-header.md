# Story 084: AC-20 RoundLogHeader (provisional on Round)

> **Epic**: Tuning Data & Config
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/tuning-data-config.md`
**Requirement**: `TR-tdc-005`, `TR-tdc-003` (text lives in `docs/architecture/tr-registry.yaml`); GDD AC-20 (Round, CR-7)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0004: Tuning data pipeline and validator (sections 3 and 5)
**ADR Decision Summary**: Round writes `TuningHash`, `PresetId`, `SchemaVersion`, `FallbackLevel`, `Revision` and the warnings into the header of the server-only Round Event Log (GDD CR-7); TDC supplies that data from the immutable `FValidatedTuning` (`TuningHash()`, `PresetId()`, `SchemaVersion()`, `FallbackLevel()`, `Revision()` and `Report()`), and the round is flagged `HotReloaded` when the PIE hot-reload path ran. `TuningHash` is never replicated.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: Plain C++ with no `UWorld` dependency, headless under `-nullrhi`; no post-cutoff API. The risk rating follows the plan's snapshot-replication range for stories 070-084; this story does not exercise replication itself. Provisional on Round: the real Round Event Log does not exist in this epic and the header is the buffer of `FStubRound`.

**Control Manifest Rules (this layer)**:
- Required: Event Log types are plain C++ (no `USTRUCT`/`UCLASS`/`UPROPERTY`) with no text conversion besides the `Ended`/`Reset` disk writer — source: ADR-0007
- Forbidden: never pass Event Log content to `UE_LOG`/`UE_LOGF`/`ensureMsgf`/`checkf`/`ClientMessage`/`TRACE_*`/`CSV_*`/`SCOPE_CYCLE_COUNTER` names; never route queue Event Log entries through the output log — source: ADR-0007, ADR-0002
- Required: the tuning hash is the project's own FNV-1a-64 over the F-2 byte stream; the header carries the frozen value, never a recomputation by another hash — source: ADR-0004
- Required: every Logic story ships T1 tests — source: ADR-0008

---

## Acceptance Criteria

*From GDD `design/gdd/tuning-data-config.md` AC-20, scoped to this story:*

- [ ] **AC-20 (Round, CR-7)**: GIVEN a completed round against the Round stub, WHEN the server-only Round Event Log header is read, THEN it contains `TuningHash`, `PresetId`, `SchemaVersion`, `FallbackLevel`, `Revision`, the validation warnings and the `HotReloaded` flag. (`StealthGame.Tuning.RoundLogHeader`; provisional on Round; client readability before `Ended` is covered by AC-16)

---

## Implementation Notes

*Derived from ADR-0004 sections 3, 5 and Key Interfaces (`FValidatedTuning`), GDD CR-7, ADR-0007 section 6, ADR-0008 sections 2 and 4, and global amendment G1:*

- **Production code.** `Source/StealthFoundation/Private/Tuning/StealthTuningService.cpp`: when the pipeline freezes (or fails over to a fallback rung), the service builds the header data from the `FValidatedTuning` it holds: `TuningHash()`, `PresetId()`, `SchemaVersion()`, `FallbackLevel()` (0 requested, 1 Default, 2 schema defaults), `Revision()`, the `ValidationReport` warnings from `Report()`, and a `HotReloaded` flag (false unless the PIE hot-reload path of story 085 published a set), and hands them to the Round via the `OnTuningReady` path and the accessor; Round (here `FStubRound`) writes them into its Round Event Log header buffer. TDC never calls Round; the data flows out through `OnTuningReady` (ADR-0004 section 6). How the header data is carried from the service to the stub is not specified by the inputs beyond this: record the chosen interface at implementation.
- **Header type.** The stub's header buffer is a plain C++ struct in `Source/StealthFoundation/Public/Server/Testing/` (story 052, G1) with no `USTRUCT`/`UPROPERTY` and no text conversion; the test reads the fields directly. The warnings are the validator's per-key warning entries (key, authored value, effective value), not formatted strings, unless story 046's report type dictates otherwise.
- **Test.** `Source/StealthFoundation/Private/Tests/Tuning/RoundLogHeaderTest.cpp`, class `FTuningRoundLogHeaderTest`, name `StealthGame.Tuning.RoundLogHeader`, flags `EditorContext | ProductFilter` inside `#if WITH_DEV_AUTOMATION_TESTS`. Steps: complete a round against `FStubRound` using one warning-producing preset (for example a preset with a value inside the hard range and outside its safe range, which keeps the value and raises one safe-range warning, AC-26 style); read the header buffer; assert each field equals the frozen `FValidatedTuning` value (`HotReloaded` false), including that the warnings list equals `Report()`'s warnings. A second case (QA edge case): force a fallback so the header shows `FallbackLevel` 1 and the `PresetId` of the rung that applied (use the `ITuningPresetSource` seam of story 050 to make the requested preset fail an invariant).
- **Not replicated.** The header is server-only; this story adds no replicated field and does not touch the snapshot. Client readability before `Ended` is AC-16 (story 083).
- **Dependency gaps to flag.** The header data comes from `FValidatedTuning` (declared in `StealthValidatedTuning.h`, story 054) and the service (story 066), and the frozen-after-resolve flow comes from the pipeline core (stories 050-053); this story's dependency list is 053 and 052 only, and 053 depends on 051 and 052 (chain 050, 048, 045, ...). Whether `FValidatedTuning` and the service exist when this story starts is open; record the prerequisites at implementation and add dependencies if needed.
- **Prototype.** None.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/tuning-data-config/story-083-full-hash-not-replicated.md`: no `TuningHash` in replicated state during Active; `TuningHash` in the header at Ended (AC-16)
- `production/epics/tuning-data-config/story-053-fallback-failed.md`: the `Failed` ladder result and `TuningLoadFailed`
- `production/epics/tuning-data-config/story-085-hot-reload-pie.md`: the hot-reload path that sets `HotReloaded` true
- `production/epics/tuning-data-config/story-052-round-session-stubs.md`: the Round stub and its header buffer
- `production/epics/tuning-data-config/story-054-validated-tuning-frozen-immutable.md`: `FValidatedTuning` and its immutability

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthFoundation/Private/Tuning/StealthTuningService.cpp`
- `Source/StealthFoundation/Private/Tests/Tuning/RoundLogHeaderTest.cpp`

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: after a completed round against the Round stub, the header holds the frozen values
  - Given: a completed round against `FStubRound` with one warning-producing preset (test `StealthGame.Tuning.RoundLogHeader`, provisional on Round)
  - When: the header buffer is read
  - Then: it contains `TuningHash`, `PresetId`, `SchemaVersion`, `FallbackLevel`, `Revision`, the validation warnings and the `HotReloaded` flag, each equal to the frozen values (`HotReloaded` false)
  - Edge cases: a fallback round shows `FallbackLevel` 1

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Tuning/RoundLogHeaderTest.cpp` (`StealthGame.Tuning.RoundLogHeader`) must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-053-fallback-failed.md`, `production/epics/tuning-data-config/story-052-round-session-stubs.md`, `production/epics/tuning-data-config/story-054-validated-tuning-frozen-immutable.md`, `production/epics/tuning-data-config/story-066-tuning-service-and-parity.md` (added after review: FValidatedTuning and the service supply the header data)
- Unlocks: None
- Open verification claims: provisional on Round (the real Round Event Log arrives with the Round epic). No engine [U] claim applies. Open: the service-to-stub header interface and the prerequisite stories (`FValidatedTuning`, service).
