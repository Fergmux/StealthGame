# Story 029: The un-culled pawn exists on the non-ally client before the exit-transition's outward leaf-plane instant

> **Epic**: Information Visibility Policy
> **Status**: Blocked — BLOCKED: [Dep:Concealment] needs the Concealment exit-transition asset (concealment-bushes); spikes story-005 (S10, T_reopen) and story-006 (S12) have not reported. Update 2026-10-25: S12 reported (production/qa/spikes/2026-10-25_S12.md, 250/250 confirmed, measured L99 133.4 ms, T_reopen p99 ~20 ms); S10 reported FAIL (production/qa/spikes/2026-10-25_S10.md) and needs a superseding ADR to ADR-0003, so this story stays blocked.
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

When a concealed pawn leaves its bush, the server un-culls it in the same step and every non-ally connection has to receive a brand-new actor (a relevancy re-open creates a new client actor, ADR-0003 E11) before the body becomes visible at the outward leaf-plane instant of the exit clip. If the actor arrives late the body pops in (a timing channel) or the exit animation plays on nothing. This story proves the lead bound of IVP F4: the exit-transition asset's `T_exit_occluded` is at least `T_uncull` (250 ms at defaults), and in a lossy 3-client run the re-opened actor exists on the non-ally client before that instant in at least 99 % of 200 exits. It protects Pillar 1 because a late or early re-open would distinguish a bush exit from an ordinary NPC walking into view. It feeds the concealment cull gate; the criterion is provisional until spikes S10 and S12 report.

**GDD**: `design/gdd/information-visibility-policy.md`
**Requirement**: `TR-ivp-004` (Concealment hybrid: quantised server cull for non-allies + client presentation hide)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0003: Replication, relevancy and the concealment cull hybrid (section 5.6; F4 is defined in the GDD)
**ADR Decision Summary**: On `HideExit`, on `OnConcealmentBroken` or on a cull cancelled during `CullPending`, Concealment pushes `bConcealed = false` inside the queue slot; the cull record returns to `Visible` immediately, the pawn is considered in the same frame (the `FActionState` write forces an update) and every client gets its channel opened then; clients show the body only at the outward leaf-plane instant, with `T_exit_occluded >= T_uncull` (IVP F4, Concealment owns the clip timing).
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: E11 (re-open creates a new client actor; NetGUID reuse and AnimBP initialisation are [U], owned by S10, story-005); E15 (listen-host client throttle, `NetClientTicksPerSecond`) [V], without which skipped clients would wait for the pawn's next regular update (about 67-83 ms, different per client) and break F4's `T_uncull` assumption; E10 in-flight cull/un-cull race [U, S10]. `T_reopen` comes from story-005 and the recomputed `T_uncull` from story-006 (S12).

**Control Manifest Rules (this layer)**:
- Required: **The concealment cull lands on the global boundary grid and closes the channel explicitly per denied connection in the cull tick**, with no client acknowledgement wait; un-cull happens in the same server step as the state change — source: ADR-0003
- Required: **Client code tolerates a relevancy destroy and initialises a re-created actor from replicated state only** — source: ADR-0003
- Forbidden: **Never call `ForceNetRelevant()` or `ForceActorRelevantNextUpdate()` on a pawn** — source: ADR-0003
- Guardrail: **Replication**: relevancy override O(1) per pawn-connection pair, inside the 1.5 ms replication line — source: ADR-0003

---

## Acceptance Criteria

*From GDD `design/gdd/information-visibility-policy.md` (AC-IVP-24), scoped to this story:*

- [ ] **AC-IVP-24** [Unit][Doc][S10][Dep:Concealment] `StealthGame.Visibility.UncullLeadBound`: GIVEN Concealment's exit-transition asset, WHEN `T_exit_occluded` is read from its leaf-plane notify, THEN it is ≥ `T_uncull` (250 ms); in a [PIE-3][Net] run the non-ally client creates the re-opened actor before the outward leaf-plane instant in ≥ 99 % of 200 exits. (F4)

---

## Implementation Notes

*Derived from ADR-0003 section 5.6 and 5.8, F4 and the Validation Criteria (S10, S12):*

- Two parts. (1) A recorded read of `T_exit_occluded` from the leaf-plane notify of Concealment's exit-transition asset, compared with `T_uncull` as confirmed by story-006 (250 ms unless S12 records a larger pair for TDC and Concealment). (2) A multi-client run that performs 200 bush exits and records, per exit and per non-ally client, the server time at which the re-opened actor is created against the server time of the outward leaf-plane instant. At least 198 of 200 must pass (exactly 198 passes, 197 fails); compare in server time (`EstimatedServerNow` on the client mapped to server time), never wall time.
- Run under a lossy profile (`StealthLeak1` and `StealthLeak2`, ADR-0008 section 5) with at least 3 clients; packet loss is not seedable, so assert bounds. Assert the active replication system (legacy) and record it in the result.
- The un-cull must happen in the same server step as the state change (ADR-0003 section 5.6). This story does not implement the un-cull; it measures the production path built by story-024 (cull audience, `IConcealmentCullSource::SetCullState`, `FConcealmentCullTickFunction`) with Concealment's real exit. Until the concealment-bushes exit asset exists a harness caller cannot stand in for the clip timing, which is why the story is blocked.
- A re-open creates a new client actor; the new actor initialises from replicated state through `RefreshPresentation(EstimatedServerNow)` (section 5.8), so client registries must tolerate the destroy and re-create (story-005 measures this).
- If the measured pass rate is below 99 %, the remedy is a larger `T_exit_occluded` in Concealment's asset or a larger `ConcealCullDelayMs`/`T_uncull` pair through the TDC change-request path, not a relaxation of this criterion.
- Spike gates: story-005 (S10, `T_reopen` against the F4 value of 33 ms) and story-006 (S12, L99 and the recomputed pair) must have reported; the criterion is provisional until then.
- Prototype: Replaces: nothing directly (the prototype has no exit-transition lead bound; `bConcealed` replication and `UpdateConcealmentVisibility` in `Source/StealthGame/Private/StealthCharacter.cpp` are replaced by story-024). Promotes: nothing. Reference only: `Source/StealthGame/Private/Spikes/StealthIdentitySpike.*`.
- Cross-epic: needs the concealment-bushes epic's exit-transition asset and leaf-plane notify (`T_exit_occluded`) and its `HideExit` push into the Policy.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- story-005: spike S10, re-open behaviour, NetGUID reuse and `T_reopen` measurement
- story-006: spike S12, cull delay against measured L99 and the recomputed F3/F4 pair
- story-024: the cull record, cull tick and un-cull path under test
- story-025: cull timing on the way in (`ConcealCullDelayMs` and the global grid)
- story-027: the local hide at the swap (the way in, presentation side)
- concealment-bushes epic: authoring the exit-transition clip and notify

---

## Files

*The code and test files this story creates or changes (planned):*

- `Source/StealthTestHarness/Private/Tests/PIE/UncullLeadBoundTest.cpp` (T2, `NETWORK_TEST_CLASS(UncullLeadBound, "StealthGame.Visibility.PIE")`; or T3 `StealthGame.Visibility.MP.UncullLeadBound` under `Source/StealthTestHarness/Private/Scenarios/` with a node in `Build/Scripts/` if the lossy run needs separate processes)
- `Source/StealthFoundation/Private/Tests/Visibility/UncullLeadBoundTest.cpp` (T1, `FVisibilityUncullLeadBoundTest`, `StealthGame.Visibility.UncullLeadBound`, the threshold arithmetic: 198 passes, 197 fails, `T_exit_occluded >= T_uncull`)
- `production/qa/evidence/uncull-lead-bound-evidence.md` (the recorded `T_exit_occluded` read and the 200-exit result)

*Note: the `StealthFoundation` and `StealthTestHarness` module folders are created by tuning-data-config Exit A (they do not exist yet).*

---

## QA Test Cases

- **AC-1**: Un-cull lead bound (AC-IVP-24)
  - Given: Concealment's exit-transition asset, and 200 exits in a PIE-3 run under a lossy profile.
  - When: `T_exit_occluded` is read from the leaf-plane notify, and the non-ally client's actor creation time is compared with the outward leaf-plane instant.
  - Then: `T_exit_occluded >= T_uncull` (250 ms); the re-opened actor exists before the outward leaf-plane instant in >= 99 % of 200 exits (at least 198).
  - Edge cases: exactly 198 passes and 197 fails; compare in server time, not wall time.

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: a T2 or T3 test under `Source/StealthTestHarness/` OR a documented playtest or spike report (here: the recorded `T_exit_occluded` read plus the 200-exit run, `production/qa/evidence/uncull-lead-bound-evidence.md`)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/information-visibility-policy/story-005-spike-s10-reopen-behaviour-and-netguid.md`, `production/epics/information-visibility-policy/story-006-spike-s12-cull-delay-vs-measured-l99.md`, `production/epics/information-visibility-policy/story-024-concealment-cull-audience.md`; concealment-bushes epic (the exit-transition asset with its leaf-plane notify and the `HideExit` push)
- Unlocks: None
