# Story 026: Opponents get no trace of a concealed pawn, co-occupant rivals are blind, allies see the marker and informants have no x-ray

> **Epic**: Concealment (Bushes)
> **Status**: Blocked — BLOCKED: spikes S9 (`production/epics/information-visibility-policy/story-004-spike-s9-cull-latency-and-explicit-channel-close.md`) and S10 (`production/epics/information-visibility-policy/story-005-spike-s10-reopen-behaviour-and-netguid.md`) have not reported Update 2026-10-25: spike S10 reported FAIL (production/qa/spikes/2026-10-25_S10.md) — the in-flight cull/un-cull race can leave a client without the pawn (~1-2 % under loss), the re-created pawn renders one un-evaluated frame, and presentation times need ADR-0002's clock estimator; a superseding ADR to ADR-0003 must decide the fixes before this story can proceed.
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/concealment-bushes.md` (CON-R6, CON-R10, AC-CON-02, AC-CON-03, AC-CON-04)
**Requirement**: `TR-con-007`, `TR-con-008`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0017: Concealment component and per-viewer presentation (§5 latch, §7 per-viewer result), with ADR-0003 (cull hybrid)
**ADR Decision Summary**: After the cull lands a concealed pawn has no actor, shadow, decal, sound or aim/Strike-hint hit on any non-ally client; co-occupants who are not allies hold no actor for each other (the relevancy rule stays `owner ∨ ally ∨ ¬concealed`, no same-bush clause); allies hold each other with the body hidden and show the hatched marker; informant viewers have no x-ray.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: This story is the observation of the S9/S10-dependent cull end to end under PIE with 2 clients (and a third client for the rival/ally pairs where needed); packet captures use the harness capture hook. The hatched marker assertion needs the Teammate Markers epic built.

**Control Manifest Rules (this layer)**:
- Required: Client presentation reads only `IsAllyBodyHidden` / the C-TeamView flag; allies learn concealment from the C-TeamView entry — source: ADR-0017, ADR-0001
- Forbidden: No same-bush relevancy clause; informants never get an x-ray — source: ADR-0017, IVP R9a
- Guardrail: A negative control that passes means the harness is broken

---

## Acceptance Criteria

*From GDD `design/gdd/concealment-bushes.md` (AC-CON-02, AC-CON-03, AC-CON-04), scoped to this story:*

- [ ] **AC1** (AC-CON-02, [PIE-2][1P/3P] `StealthGame.Concealment.NoTraceWhileCulled`): **GIVEN** a concealed opponent for 10 s **WHEN** the viewer orbits the bush at 2, 5, 10 and 25 m at 8 bearings in each mode (64 samples) **THEN** no actor, shadow, decal, sound or aim/Strike-hint hit exists for that pawn at any sample (control: an Outside pawn at the same spot is found at every sample); a packet capture of the 10 s with the bush occupied and with it empty shows no bunch for the concealed pawn's channel.
- [ ] **AC2** (AC-CON-03, [PIE-2][U] `StealthGame.Concealment.CoOccupantsBlind`): **GIVEN** two rival players concealed in one bush **THEN** neither client holds the other's pawn; **GIVEN** two allies **THEN** each holds the other with the body hidden and the hatched marker (the marker assertion waits for the Teammate Markers epic).
- [ ] **AC3** (AC-CON-04, [PIE-2] `StealthGame.Concealment.InformantNoXray`): **GIVEN** an informant viewer whose rival is concealed and whose ally is concealed **THEN** the rival has no actor and the ally shows the hatched marker (marker part waits for Teammate Markers).

---

## Implementation Notes

*Derived from ADR-0017 Decision §5, §7 and CON-R6/R10:*

All three checks are CQTest PIE tests under `StealthGame.Concealment.PIE`. For AC1 the sample positions are 4 radii × 8 bearings per camera mode; sound and decal presence are queried on the viewer client, and the aim and Strike-hint traces use the presentation aim channel. The control (an Outside pawn at the same spot is found at every sample) proves the harness can see a pawn. Informants have no x-ray (IVP R9a): their view of a concealed rival equals an active opponent's.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-025-cull-push-wiring.md`: the cull timing observation.
- `story-024-is-concealed-from-truth-table.md`: the unit-level truth table.
- Teammate Markers epic: the hatched marker rendering.

---

## Files

- `Source/StealthTestHarness/Private/Tests/PIE/NoTraceWhileCulledTest.cpp` (`NETWORK_TEST_CLASS(NoTraceWhileCulled, "StealthGame.Concealment.PIE")`)
- `Source/StealthTestHarness/Private/Tests/PIE/CoOccupantsBlindTest.cpp` (`NETWORK_TEST_CLASS(CoOccupantsBlind, "StealthGame.Concealment.PIE")`)
- `Source/StealthTestHarness/Private/Tests/PIE/InformantNoXrayTest.cpp` (`NETWORK_TEST_CLASS(InformantNoXray, "StealthGame.Concealment.PIE")`)

---

## QA Test Cases

*Written by qa-lead at story creation (QL-STORY-READY: ADEQUATE).*

- **AC1**: NoTraceWhileCulled
  - Given: a Concealed opponent for 10 s.
  - When: the viewer orbits at 2, 5, 10 and 25 m, 8 bearings, in each camera mode (64 samples).
  - Then: no actor, shadow, decal, sound or aim hit exists at any sample, and the packet capture shows no bunch for the channel; control: an Outside pawn is found at every sample.
- **AC2**: CoOccupantsBlind
  - Given: two rival players Concealed in one bush, then two allies.
  - When: each client's actor list is read.
  - Then: rivals hold no pawn for each other; allies hold each other with the body hidden.
  - Edge cases: the hatched-marker assertion is deferred until the Teammate Markers epic exists.
- **AC3**: InformantNoXray
  - Given: an informant viewer with a Concealed rival and a Concealed ally.
  - When: the viewer's actors are read.
  - Then: the rival has no actor and the ally shows the marker (marker part deferred as above).

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: the three PIE tests above — must exist and pass, OR a documented playtest

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `story-009-crowd-perception-filter-and-steering-exclusion.md`, `story-020-fresh-actor-latch-and-ally-hidden-read.md`, `story-024-is-concealed-from-truth-table.md`, `story-025-cull-push-wiring.md`; IVP S9 and S10 spike stories (blockers); teammate-markers epic (stories not yet written)
- Unlocks: `story-035-prototype-retirement-and-regression.md`
