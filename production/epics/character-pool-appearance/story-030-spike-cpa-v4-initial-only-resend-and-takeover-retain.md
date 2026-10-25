# Story 030: Spike CPA-V4: initial-only re-sent on relevancy re-entry, retained across takeover

> **Epic**: Character Pool & Appearance
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/character-pool-appearance.md`
**Requirement**: `TR-cpa-004`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-cpa-004 = Appearance before first replication, immutable per pawn; deck conserved; `DrawReplacement` same tick; `TakeNpcCard` on adoption.)*

**ADR Governing Implementation**: ADR-0012: Character pool data and appearance allocation (conditional gate CPA-V4, risk R5; amendment 2026-10-24 corrected the expectation)
**ADR Decision Summary**: A `COND_InitialOnly` property is re-sent on relevancy re-entry after the concealment cull's explicit channel close (the re-opened channel creates a new client actor whose initial bunch carries it), and is **retained, not re-sent,** after a takeover flips the controller on a stable NetGUID (no channel close, so no new initial bunch).
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0012)
**Engine Notes**: Engine path read as [V] (E10: a relevancy close keeps no replicator; initial-only properties are force-compared on an initial send); this spike confirms it end to end through ADR-0003's explicit close. Overlaps the information-visibility-policy spike S10 (story 005): reuse its harness and findings, do not repeat the NetGUID work.

**Why this story**: Story 036 (re-entry, takeover, host parity) is Blocked until this reports. Read ADR-0012 Verification Required CPA-V4 and the amendment text for it, and ADR-0003 §5.8.

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: `AppearanceId` replicated `COND_InitialOnly` with a RepNotify; a re-opened actor's appearance is applied only by its initial `OnRep_AppearanceId` (ADR-0012)
- Forbidden: Never rely on a re-send at takeover; `RefreshPresentation` never sets a mesh or material
- Guardrail: n/a

---

## Acceptance Criteria

*From ADR-0012 Verification Required CPA-V4 (as corrected):*

- [ ] **AC-1** GIVEN a pawn culled by the concealment cull's explicit channel close, WHEN it re-enters relevancy (5 leave and return cycles), THEN the new client actor's initial bunch carries the `COND_InitialOnly` property with the same value.
- [ ] **AC-2** GIVEN a takeover that flips the controller on a stable NetGUID (including two takeovers in one tick), THEN the client's value is unchanged, no `OnRep` fires and no new initial bunch is sent (raw-bunch capture).
- [ ] **AC-3** GIVEN both runs, THEN `production/qa/spikes/<date>_CPA-V4.md` states PASS or FAIL (or INCONCLUSIVE for a partial run).

---

## Implementation Notes

- Raw-bunch capture comes from the multi-process harness (information-visibility-policy story 001, `StealthTestHarness`).
- Record whether the engine used legacy replication or Iris (the project runs `net.Iris.UseIrisReplication:0`, spike S1 to S14 logs).

**Prototype disposition**: none.

---

## Out of Scope

- Story 036: the production tests; information-visibility-policy story 005 (S10) owns cull/NetGUID findings

---

## Files

- `production/qa/spikes/<date>_CPA-V4.md`
- `Source/StealthTestHarness/Private/CharacterPool/CpaV4Probe.cpp`

---

## QA Test Cases

*QL-STORY-READY: ADEQUATE (qa-lead); spike report (needs the harness module).*

**Test: initial-only resend after cull**
- Given: a pawn culled by the concealment cull's explicit channel close.
- When: it re-enters relevancy.
- Then: the new client actor's initial bunch carries `AppearanceId` with the same value.
- Edge cases: repeated cull and return cycles (5).

**Test: takeover retains value**
- Given: a takeover that flips the controller on a stable NetGUID.
- When: the raw-bunch capture is read.
- Then: the client's value is unchanged, no OnRep fires, no new initial bunch is sent.
- Edge cases: two takeovers in one tick.

**Test: report**
- Given: both runs.
- When: the report is written.
- Then: `production/qa/spikes/<date>_CPA-V4.md` states PASS or FAIL.
- Edge cases: as story 026.

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: Integration
**Required evidence**:
- Integration: `production/qa/spikes/<date>_CPA-V4.md` (documented spike report with raw-bunch capture)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/information-visibility-policy/story-005-spike-s10-reopen-behaviour-and-netguid.md`, `production/epics/shared-character-locomotion/story-001-pawn-class-default-subobjects.md`
- Unlocks: `production/epics/character-pool-appearance/story-036-relevancy-reentry-takeover-listen-host-parity.md`
