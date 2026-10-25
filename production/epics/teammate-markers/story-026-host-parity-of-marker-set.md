# Story 026: Host parity of the marker set

> **Epic**: Teammate Markers
> **Status**: Blocked — BLOCKED: ADR-0021 verification item UI-V1 (story 018) has not reported; the serialised marker set includes painted screen positions, which come from the painted path of story 019.
> **Layer**: Presentation
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

The listen host's process holds every PlayerController and every team's C-TeamView, yet its marker module must behave exactly like a same-team remote client's: it reads only its local controller's team view and produces an identical marker set (same handles, fills, screen positions and label state) for the same camera and the same stationary allies. This is what makes the listen host a fair player and keeps the leak tests meaningful (the host's memory holding other teams' data is the accepted IVP L21 caveat, not a marker leak). This story is the serialised-equality half of AC-TMK-07; the instrumented-read half is story 016.

**GDD**: `design/gdd/teammate-markers.md` (TM-R2 listen host; AC-TMK-07, serialised half)
**Requirement**: `TR-tmk-001`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0021: UI/HUD layering, CommonUI and Enhanced Input (section 4: host parity)
**Also governed by**: ADR-0001 (host presentation rule; Roster R6, Session rule 3)
**ADR Decision Summary**: Presentation samples its carriers by value every frame so the listen host (which gets no RepNotify for its own writes) and a remote client run the identical path.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Equality within 1 px needs a fixed camera transform, TAA off and stationary allies so network smoothing cannot differ; both processes serialise through the model dump and the leaf's final projected positions.

**Control Manifest Rules (this layer)**:
- Required: Presentation builds view-models by sampling its carriers every frame by value (the listen host runs the same path) — source: ADR-0021
- Forbidden: Never read another controller's carriers from Presentation — source: ADR-0001, ADR-0021
- Guardrail: `FHudViewModel::Dump()`-style deterministic dumps (stable order, fixed float precision)

---

## Acceptance Criteria

*From GDD `design/gdd/teammate-markers.md` (AC-TMK-07 serialised half) and IVP L21, scoped to this story:*

- [ ] **AC1** (AC-TMK-07, [PIE-2] `HostParity`): **GIVEN** the listen host and one client on the same team, a fixed camera transform, TAA off and stationary allies **WHEN** each serialises its marker set (handles, fills, screen positions, label state) while looking at the same group **THEN** the two sets are equal within 1 px, with a hatched ally and an outline ally in the group.
- [ ] **AC2** (IVP L21): **GIVEN** the listen host on team B and a remote team-A client **THEN** the host's marker set contains only the host's own teammates, and the host process holding other teams' data is recorded as the accepted IVP L21 caveat, not asserted as a marker leak.

---

## Implementation Notes

*Derived from AC-TMK-07 and TM-R2:*

Add a deterministic `Dump()` to the marker model output (stable handle order, fixed float precision) and a leaf-side dump of final projected positions; compare host and client dumps with a 1 px tolerance. Fix the camera by a test-only view-target override on both processes and disable TAA with the console variable the engine reference documents for 5.8.4. Use stationary allies so the simulated proxy's smoothing cannot differ between host and client. Include one hatched and one outline ally so fill parity is covered.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-016-presenter-local-controller-only.md`: the instrumented host-read assertion.
- `story-027-leak-audit-marker-objects-and-canaries.md`: opponent-client leak audits.
- the hud epic: HUD host parity (AC-HUD-20).

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthPresentation/Private/Markers/TeammateMarkerModel.cpp` (deterministic `Dump()`)
- `Source/StealthTestHarness/Private/Markers/HostParityTest.cpp` (CQTest, `StealthGame.Markers.PIE.HostParity.*`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC1**: Given: a listen host and one same-team client, fixed camera transform, TAA off, stationary allies. When: each serialises its marker set (handles, fills, screen positions, label state) while looking at the same group. Then: the two sets are equal within 1 px. Edge cases: a hatched and an outline ally in the group.
- **AC2**: Given: the host on team B and a remote team-A client. When: the host's marker set is read. Then: it contains only the host's own teammates. Edge cases: the IVP L21 host-memory caveat is recorded as accepted, with no assertion.

QL-STORY-READY: ADEQUATE

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: the CQTest `HostParityTest.cpp` OR a documented playtest

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/teammate-markers/story-016-presenter-local-controller-only.md`, `story-018-spike-ui-v1-zero-lag-projection.md`, `story-019-paint-markers-in-world-anchor-leaf.md`; cross-epic `production/epics/participant-state-roster/story-048-pie-host-view-equals-client-view.md`
- Unlocks: None
