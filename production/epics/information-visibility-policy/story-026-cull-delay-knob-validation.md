# Story 026: The cull delay knob is computed by F3 and validated to 100-1000 ms, a multiple of 50, and at least L99 + 1/f_net

> **Epic**: Information Visibility Policy
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

`ConcealCullDelayMs` decides how long a concealed pawn stays visible to the network before the quantised cull lands. Too short and the cull lands before the client's hide completes, too long and it is a timing signal, and a value off the 50 ms grid or below the measured latency breaks the guarantee that a cull never precedes the local hide (F3). This story implements the F3 computation and the tuning-data-config validator rows that reject unsafe values, so a tuning change can never silently weaken the guarantee. S12 (story-006) may later change the default pair; this story fixes the rule, not the default.

**GDD**: `design/gdd/information-visibility-policy.md`
**Requirement**: `TR-ivp-004` (Concealment hybrid: quantised server cull for non-allies + client presentation hide)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0003: Replication, relevancy and the concealment cull hybrid (section 5.3, F3); also ADR-0004: Tuning data pipeline and validator (invariant rows); ADR-0008: Test and automation strategy for leak and parity tests (sections 1, 4)
**ADR Decision Summary**: The cull deadline is the first global-grid point at or after `CommitServerTime + ConcealCullDelayMs` (IVP F3: default 250 ms). A new cross-field constraint goes through a TDC change request and a row in `TuningInvariants.cpp`; invariant `Check` functions are pure, take `const` input, and reject or warn, never repair.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: This story is plain C++ arithmetic and validation with no post-cutoff engine API. The only engine-dependent input, the measured L99, is produced by S12 (story-006); until then the validator uses the design value `L99 = 150 ms`. `f_net` is `PawnNetUpdateHz` (15 Hz, ADR-0003 section 1).

**Control Manifest Rules (this layer)**:
- Required: **Every reject/warn invariant carries its GDD ID; a new cross-field constraint goes through a TDC change request and a row in `TuningInvariants.cpp`** — source: ADR-0004
- Required: **Invariant `Check` functions are pure and take `const` input; only F-1 changes a value (to the key's hard range); invariants reject or warn, never repair** — source: ADR-0004
- Forbidden: **Never repair a failed invariant by clamping a dependent key** — source: ADR-0004
- Required: **Every gameplay number is a field of a `_Public` or `_Server` tuning sub-struct**; a hardcoded gameplay literal is a defect — source: ADR-0004

---

## Acceptance Criteria

*From GDD `design/gdd/information-visibility-policy.md`, scoped to this story:*

- [ ] **AC-IVP-28** [Unit] `StealthGame.Visibility.CullDelayKnobValidation`: GIVEN `L99 = 150` and `f_net = 15`, WHEN F3 is computed and the key is loaded, THEN F3 gives 250 ms, and TDC validation rejects values outside 100-1000, not a multiple of 50, or below `L99 + 1/f_net`. (F3)

---

## Implementation Notes

*Derived from ADR-0003 section 5.3, IVP F3 and ADR-0004 invariant rules:*

- **F3.** `D_cull = 50 ms x ceil((L99 + 1/f_net) / 50 ms)`. With `L99 = 150 ms` and `f_net = 15 Hz`: `150 + 66.67 = 216.67 ms`, `ceil(216.67 / 50) = 5`, so `D_cull = 250 ms`. Implement the computation in the Visibility Policy's plain-C++ core and use it in the test.
- **Validator rows.** Add rows to `TuningInvariants.cpp` for `ConcealCullDelayMs` (a key in the `_Server` tuning sub-struct): reject a value outside 100-1000, reject a value that is not a multiple of 50, reject a value below `L99 + 1/f_net`. The cross-field constraint arrives through a TDC change request (the manifest rule), and each row carries its GDD ID (AC-IVP-28 / F3). Rows reject; they never clamp a dependent key.
- **Where the validator lives.** The validator pipeline is tuning-data-config's (TDC-B, ADR-0004); this story depends on it existing and adds the rows through the change request.
- **Boundary cases (QA).** Boundaries 100, 1000, 125 and 1050: 125 is rejected by the multiple-of-50 rule; 1050 is rejected by the range rule; 100 and 1000 sit on the range edges and are multiples of 50, so each is judged by the range rule (accepted at the edge) and then by the `L99 + 1/f_net` rule (100 is below 216.67 at the design inputs, so the combined validator rejects it); 200 is in range and a multiple of 50 but below 216.67, so it must be rejected; 250 is accepted. Assert the outcome per rule so a failure names the rule.
- **Plain C++ with injected inputs.** `L99` and `f_net` are explicit inputs (never read from the engine or the clock in the test), so the test is deterministic (ADR-0008 section 4).
- **Spike note.** S12 (story-006) may change the default (`ConcealCullDelayMs` and F4's `T_uncull`); it changes the default value, not the rule, and the new pair is recorded for tuning-data-config and Concealment as a cross-GDD invariant.
- **Prototype disposition.** None; the prototype has no cull delay knob.
- **Files and modules.** Do not exist yet: created by tuning-data-config Exit A; the validator pipeline is TDC-B.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- story-025: cull landing on the global grid (AC-IVP-11)
- story-006: measuring L99 and recomputing F3 and F4 (S12)
- story-029: un-cull lead bound (F4, AC-IVP-24)
- tuning-data-config epic: the validator pipeline and the `ConcealCullDelayMs` key definition

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthFoundation/Private/Visibility/` (F3 computation in the Policy core)
- `TuningInvariants.cpp` (new rows for `ConcealCullDelayMs`; the file's path is owned by tuning-data-config)
- `Source/StealthFoundation/Private/Tests/Visibility/CullDelayKnobValidationTest.cpp` (class `FVisibilityCullDelayKnobValidationTest`, test `StealthGame.Visibility.CullDelayKnobValidation`)

---

## QA Test Cases

- **AC-1**: Cull delay knob validation (AC-IVP-28)
  - Given: `L99 = 150`, `f_net = 15`, and the TDC validator row in `TuningInvariants.cpp`.
  - When: F3 is computed and the key is loaded.
  - Then: F3 gives 250 ms; TDC validation rejects values outside 100-1000, values not a multiple of 50, and values below `L99 + 1/f_net`.
  - Edge cases: boundaries 100, 1000, 125, 1050; 200 is in range and a multiple of 50 but below 216.67, so it must be rejected. (Tier: T1.)

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Visibility/CullDelayKnobValidationTest.cpp` (the studio docs' `tests/unit/visibility/`) — must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: tuning-data-config Exit B (the TDC pipeline and validator, ADR-0004), tuning-data-config Exit A (module skeleton, test harness)
- Unlocks: None
