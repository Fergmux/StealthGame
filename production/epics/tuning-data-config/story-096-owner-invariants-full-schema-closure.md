# Story 096: AC-30 closure: every owner invariant over the full schema

> **Epic**: Tuning Data & Config
> **Status**: Blocked — BLOCKED: closes only after all 18 owner epics register their schema sub-structs and their invariant rows; a dependency block on other epics, not an ADR or spike gate; unblocks when story 034's pending-owner baseline is empty
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/tuning-data-config.md`
**Requirement**: `TR-tdc-004`; GDD AC-30 (owner invariants), aggregate half
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0004: Tuning data pipeline and validator
**ADR Decision Summary**: One invariant table (`TuningInvariants.cpp`, rows `{Id, Owner, Check}`) holds every non-offline cross-field row, each named by its GDD ID. A failing `Reject` row rejects the whole preset and a `Warn` row logs and keeps it. Offline rows are not in the table; their owners' CI or load-time checks enforce them. Rows enter the table through a TDC change request (CR-15.6), never as a check inside an owner system.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: LOW
**Engine Notes**: No post-cutoff API for this story; the work is plain C++ in the pipeline core (`FTuningPipelineCore`, no `UWorld`) with logging through `UE_LOGF` on `LogStealthFoundation`. Nothing here is a [U] engine claim.

**Control Manifest Rules (this layer)**:
- Required: Invariant `Check` functions are pure and take `const` input; only F-1 changes a value (to the key's hard range); invariants reject or warn, never repair — source: ADR-0004
- Required: Every reject/warn invariant carries its GDD ID; a new cross-field constraint goes through a TDC change request and a row in `TuningInvariants.cpp` — source: ADR-0004
- Forbidden: Never repair a failed invariant by clamping a dependent key — source: ADR-0004
- Forbidden: Never disable, skip or reseed a failing test to pass a gate; never mark a gate Done that was never watched to fail — source: ADR-0008

---

## Acceptance Criteria

*From GDD `design/gdd/tuning-data-config.md` (AC-30, aggregate half), as amended (amendment 096 REVISED):*

- [ ] **AC-30 (aggregate half)**: a meta-test iterates `GetTuningInvariants()` and asserts every row id is either backed by a registered fixture preset (reject rows: breaking only that row yields rejection naming exactly that ID; warn rows: exactly one warning naming the ID; I-NKP-1 per scope; I-PCG-5 and I-PCG-6 reject fixtures) or declared offline and unevaluated at preset load; the Default preset validates with every owner invariant passing and no warning; the W-NKP-4 left-side minimum over the hard ranges is at most the minimum InformantRespawnDelaySec. Each owner epic supplies its own rows and fixtures (CR-15.6). (`StealthGame.Tuning.OwnerInvariants`)

---

## Implementation Notes

*Derived from ADR-0004 section 3 and Implementation Guidelines 5 and 6:*

This story is blocked and not sprint-startable. The AC iterates a table that is filled one owner epic at a time, so it can close only when the last owner has registered its sub-struct and rows. The unblock condition is concrete: story 034's pending-owner baseline (`tools/ci/lint/knob-schema-pending.txt`) is empty. The QA review noted the AC is open-ended and could be split per owner as epics land; this plan keeps one closing story, and each owner epic supplies its own rows and fixtures as it lands (CR-15.6). When unblocking, re-run story readiness: the owner-epic story list is not enumerable from this epic.

- Mechanism (already built by story 055, `OwnerInvariantMechanism`): row evaluation with `Pass`/`Warn`/`Reject` and context (`bIsPublic`, `PresetId`), mixed rows reporting the worst result and naming the clause, and offline rows not evaluated at preset load. This story adds no mechanism; it adds the aggregate meta-test and any missing fixtures.
- Meta-test: `StealthGame.Tuning.OwnerInvariants`, class `FTuningOwnerInvariantsTest`, file `Source/StealthFoundation/Private/Tests/Tuning/OwnerInvariantsTest.cpp`, flags `EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter` inside `#if WITH_DEV_AUTOMATION_TESTS`. It iterates `GetTuningInvariants()` (non-offline rows only) and for each row id requires either a registered fixture preset or an explicit offline declaration. A row with neither fails the test (spec edge case).
- Fixture rules from the GDD: a reject-row fixture breaks only that row (example in the GDD: `StrikeNetLeadSec` 0.25 with `StrikeContactSec` 0.40 for I-ASN-4; `SessionSoftTimeoutSec` 4 with `SessionHardTimeoutSec` 5 and `CrowdIdleMinSec` 6 for I-ST-1, because the hard minimum of `SessionHardTimeoutSec` is 5 and a Hard below 5 would clamp and pass); the report names exactly that ID. A warn-row fixture is accepted with exactly one warning naming the ID. Fixtures are owned by each owner epic.
- I-NKP-1 per scope: `NpcKillStrikeLimit` 1 warns in a non-selectable preset with `bTestAllowsStrikeLimitOne`; is clamped to 2 with a warning in one without the flag; and is rejected in a `HostSelectable` preset (the flag itself is rejected there). I-PCG-5 (`CityGenFinalEligibleTargetCount` below `FinalCandidatesMinCount`) and I-PCG-6 (`CityGenCourtTargetCount` 15 at the defaults of the other keys) each need a reject fixture.
- W-NKP-4 (defensive, decision D12): `1.2 + DeathFallSec + NoPawnFadeSec <= InformantRespawnDelaySec` is unreachable inside the hard ranges, so no fixture can trigger it. The test is a schema check that the minimum of the left side over the hard ranges is at most the minimum `InformantRespawnDelaySec`, so the warning can never fire.
- Default preset: validates with every owner invariant passing and no warning, using the log-capture seam (story 009) to assert no warning.
- Offline rows (ADR-0004 s3 lists O-CNB-6, O-CNB-7, I-SCL-10/11/14 and the LSC layout predicate) are declared offline and must not be evaluated at preset load.
- The ADR also queues two warn rows requested by other ADRs (ADR-0017 body removal before informant respawn; ADR-0018 attack lockout versus strike contact). They enter `TuningInvariants.cpp` only once the TDC GDD lists them; whether they are listed by the time this story unblocks is open.
- Negative control (ADR-0008 s3): the meta-test is watched to fail once by a synthetic row with no fixture and no offline declaration, and the test passes only if that control is detected.
- Prototype: none.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/tuning-data-config/story-055-owner-invariant-mechanism.md`: the owner-invariant mechanism over a synthetic schema fixture (AC-30 mechanism half).
- `production/epics/tuning-data-config/story-048-invariants-i1-i5.md`: invariants I-1 to I-5 (AC-17).
- `production/epics/tuning-data-config/story-034-doc-lint-schema-row-match.md`: owns the pending-owner baseline whose emptiness unblocks this story.
- `production/epics/tuning-data-config/story-094-safe-range-lint-baseline.md`: the safe-range lint over the same invariants.
- Owner epics (other epics): register their sub-structs, invariant rows and fixtures.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- Source/StealthFoundation/Private/Tuning/StealthTuningInvariants.cpp
- Source/StealthFoundation/Private/Tests/Tuning/OwnerInvariantsTest.cpp

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: AC-30 owner invariants over the full schema
  - Given: `GetTuningInvariants()` rows; per-owner fixture presets.
  - When: the meta-test iterates every row.
  - Then: each id is backed by a fixture (reject: only that row broken yields rejection naming exactly that ID; warn: exactly one warning naming the ID; I-NKP-1 per scope; I-PCG-5 and I-PCG-6 reject fixtures) or declared offline and unevaluated at load; Default validates with no warning; the W-NKP-4 left-side minimum over the hard ranges is at most the minimum `InformantRespawnDelaySec`.
  - Edge cases: a row with neither a fixture nor an offline declaration fails; the story unblocks when story 034's pending baseline is empty.

---

## Test Evidence

*Governed by `qa.level`: tests are required (qa.level is full).*

**Story Type**: Logic
**Required evidence**:
- Logic: the automation test `StealthGame.Tuning.OwnerInvariants` in `Source/StealthFoundation/Private/Tests/Tuning/OwnerInvariantsTest.cpp` must exist and pass.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-055-owner-invariant-mechanism.md`, and every owner epic's schema-row story (other epics; not yet enumerable; unblocks when story 034's pending-owner baseline `tools/ci/lint/knob-schema-pending.txt` is empty)
- Unlocks: None
- Open verification claims: the list of owner-epic schema-row stories is not defined in this epic; whether the two ADR-queued warn rows are listed in the TDC GDD by unblock time is open.
