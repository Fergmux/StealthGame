# Story 038: View-pitch and cadence parity; spawn and export order uncorrelated with player-ness

> **Epic**: Character Pool & Appearance
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/character-pool-appearance.md`
**Requirement**: `TR-cpa-004`, `TR-cpa-006`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-cpa-004 = Appearance before first replication, immutable per pawn; deck conserved; `DrawReplacement` same tick; `TakeNpcCard` on adoption. TR-cpa-006 = One look path for every viewer and pawn kind; no keyed material parameters.)*

**ADR Governing Implementation**: ADR-0001: Crowd identity rules (with ADR-0008 harness)
**ADR Decision Summary**: Identical net priority, update frequency and relevancy for all pawns; `RemoteViewPitch` has the same source and smoothing for player and NPC pawns; one spawn list of all `D` pawns is spawned in one frame in a server-secret shuffled order; network-id first-export order must show no correlation with player-ness.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0001, ADR-0008)
**Engine Notes**: In legacy replication dynamic NetGUIDs are assigned at first export per connection in priority order, with a connection's own pawn boosted; Iris is unverified (GDD OQ-5). **AC-1's replication backend and the compared quantity, and AC-2(b)'s statistic, are named in the test header from the OQ-5 ADR** (QL-STORY-READY fix). The project runs legacy replication (`net.Iris.UseIrisReplication:0`).

**Why this story**: AC-CP-16b and AC-CP-17: the distributional parity tests that catch a tell no property list shows. **Readiness:** the GDD blocks AC-CP-16b on Shared Character and Crowd; this story cannot pass until their pawn and NPC controller are implemented (dependencies). Overlaps session-travel `story-074-spawn-order-uncorrelated-with-player-ness.md` and shared-character-locomotion `story-022-remote-view-pitch-clamped.md`: reuse their capture, CPA owns the deal-side ranks.

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: One spawn list of all `D` pawns spawned in one frame in a server-secret shuffled order independent of player-ness (CP-10, ADR-0012)
- Forbidden: Never let net dormancy, update frequency or priority differ by class
- Guardrail: total pawn replication against the 50 KB/s budget is Shared Character's (story 051 records it)

---

## Acceptance Criteria

*From GDD AC-CP-16b and AC-CP-17; wording per QL-STORY-READY:*

- [ ] **AC-1** GIVEN 60 s of recorded replication on the remote client for 10 player and 10 NPC pawns, THEN the distributions of `RemoteViewPitch` (the backend's equivalent and the quantity compared are named in the test header) and of replicated-movement update intervals do not differ between classes (two-sample Kolmogorov-Smirnov, alpha 0.001); a seeded pitch offset on player pawns makes it reject (test `StealthGame.CharacterPool.PIE.ViewPitchAndCadenceParity`).
- [ ] **AC-2** GIVEN 200 seeded deals spawned headless with 3 remote connections at `(P, N) = (10, 50)` (`D = 60`), THEN (a) the sum of the `P` participant spawn ranks per deal has mean 305 and variance 2541.7 (`P (D^2 - 1) / 12 * (D - P) / (D - 1)`; z-test per deal set, alpha 0.001) and (b) per connection the wire-level first-export order shows the same rank-sum test with no correlation to player-ness; a seeded "players spawn first" bug fails (test `...PIE.SpawnAndExportOrderUncorrelated`).

---

## Implementation Notes

- Mean and variance for the F-8 reference `(10, 40)`, `D = 50`: 255 and `10 * 208.25 * 40 / 49`.
- Wire-level capture comes from the multi-process raw-bunch harness (information-visibility-policy story 001).
- Standard error over 200 deals: about 3.57 (mean) and about 255 (variance); state the acceptance bounds in the test.

**Prototype disposition**: none.

---

## Out of Scope

- Story 037: class-shape parity; Story 051: bandwidth record; Shared Character's own tier and cadence tests

---

## Files

- `Source/StealthTestHarness/Private/CharacterPool/ViewPitchAndCadenceTest.cpp`
- `Source/StealthTestHarness/Private/CharacterPool/SpawnExportOrderTest.cpp`

---

## QA Test Cases

*QL-STORY-READY: GAPS → AC-1 quantity and AC-2(b) statistic named, with the qa-lead's proposed text; not re-run (unattended; decision logged).*

**Test: pitch and cadence parity**
- Given: 60 s of recorded replication, 10 player and 10 NPC pawns.
- When: comparing distributions.
- Then: two-sample KS does not reject at alpha 0.001 for the view-pitch quantity or for movement update intervals.
- Edge cases: a seeded pitch offset on player pawns makes it reject.

**Test: spawn and export order**
- Given: 200 seeded deals spawned headless, 3 remote connections, `(P,N) = (10,50)`.
- When: ranks are summed.
- Then: the participant-rank sum per deal has mean 305 and variance 2541.7 by z-test at alpha 0.001; the per-connection export check passes the same test.
- Edge cases: a seeded "players spawn first" bug fails.

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/CharacterPool/ViewPitchAndCadenceTest.cpp`, `SpawnExportOrderTest.cpp` — automation tests `StealthGame.CharacterPool.PIE.ViewPitchAndCadenceParity`, `...PIE.SpawnAndExportOrderUncorrelated`

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/shared-character-locomotion/story-022-remote-view-pitch-clamped.md`, `production/epics/session-travel/story-074-spawn-order-uncorrelated-with-player-ness.md`, `production/epics/information-visibility-policy/story-001-spike-s1-multiprocess-raw-bunch-capture.md`, `production/epics/character-pool-appearance/story-013-allocator-facade-signature-and-call-order.md`
- Unlocks: None
