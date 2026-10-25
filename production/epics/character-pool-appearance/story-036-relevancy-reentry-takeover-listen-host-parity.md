# Story 036: Relevancy re-entry restores the id; takeover keeps it; the host sees what clients see

> **Epic**: Character Pool & Appearance
> **Status**: Blocked — BLOCKED: spike CPA-V4 (story 030) has not reported; the re-send-on-re-entry and retained-after-takeover behaviour of `COND_InitialOnly` is unproven end to end.
> **Layer**: Core
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/character-pool-appearance.md`
**Requirement**: `TR-cpa-004`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-cpa-004 = Appearance before first replication, immutable per pawn; deck conserved; `DrawReplacement` same tick; `TakeNpcCard` on adoption.)*

**ADR Governing Implementation**: ADR-0012: Character pool data and appearance allocation
**ADR Decision Summary**: A re-opened actor's appearance is applied only by its initial `OnRep_AppearanceId` (a relevancy re-open sends the initial bunch again, E10); a takeover on a stable NetGUID sends nothing and the client keeps the value it holds; the listen host's local render state of a pawn equals the remote client's.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0012)
**Engine Notes**: CQTest naming `StealthGame.CharacterPool.PIE.<Scenario>.<Method>`; the AC-CP-18b wording of the GDD is corrected by the ADR-0012 amendment: assert **unchanged across takeover** (no OnRep, no new initial bunch), not "re-sent".

**Why this story**: AC-CP-18b with the V4 correction. Read GDD AC-CP-18b and ADR-0012 amendment for CPA-V4. **AC-3 revised per QL-STORY-READY**: "render state" is defined as (mesh asset, material instance pointer, forced LOD, shadow flags).

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: `AppearanceId` replicated `COND_InitialOnly` with a RepNotify; a re-open applies only via the initial `OnRep` (ADR-0012)
- Forbidden: Never rely on a re-send at takeover; `RefreshPresentation` never sets a mesh or material
- Guardrail: n/a

---

## Acceptance Criteria

*From GDD AC-CP-18b (corrected by the CPA-V4 amendment):*

- [ ] **AC-1** GIVEN a pawn leaves and re-enters relevancy on the remote client (5 times), THEN it returns with the same `AppearanceId` (test `StealthGame.CharacterPool.PIE.RelevancyReentryRestoresId`).
- [ ] **AC-2** GIVEN a takeover on a stable NetGUID (and two takeovers in one tick), THEN the client's `AppearanceId` is unchanged and `OnRep_AppearanceId` does not fire.
- [ ] **AC-3** GIVEN the listen host and one remote client, THEN for every pawn (concealed and dead pawns included) the host's render state, defined as (mesh asset, material instance pointer, forced LOD, shadow flags), equals the remote client's for the same pawn (test `...PIE.ListenHostParity`).

---

## Implementation Notes

- The concealment cull's explicit channel close (ADR-0003 §5.8) is the re-entry mechanism; reuse the information-visibility-policy harness.
- Pillar-1 assertions on the remote client; the host comparison is the one place both views are read.

**Prototype disposition**: none.

---

## Out of Scope

- Story 030: the spike; Story 040: uniform presentation across pawn classes

---

## Files

- `Source/StealthTestHarness/Private/CharacterPool/RelevancyAndHostParityTest.cpp`

---

## QA Test Cases

*QL-STORY-READY: GAPS → AC-3 "render state" defined, with the qa-lead's proposed text; not re-run (unattended; decision logged).*

**Test: relevancy re-entry**
- Given: a pawn leaving and re-entering relevancy on the remote client.
- When: it returns.
- Then: the same `AppearanceId`.
- Edge cases: leave and return 5 times.

**Test: takeover**
- Given: a takeover on a stable NetGUID.
- When: the controller flips.
- Then: the client's `AppearanceId` is unchanged and `OnRep_AppearanceId` does not fire.
- Edge cases: two takeovers in one tick.

**Test: listen host parity**
- Given: the host and one remote client.
- When: the render-state tuple is read for all pawns.
- Then: the tuples are equal per pawn.
- Edge cases: concealed and dead pawns.

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/CharacterPool/RelevancyAndHostParityTest.cpp` — automation tests `StealthGame.CharacterPool.PIE.RelevancyReentryRestoresId`, `...PIE.ListenHostParity`

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/character-pool-appearance/story-030-spike-cpa-v4-initial-only-resend-and-takeover-retain.md`, `production/epics/character-pool-appearance/story-027-appearance-id-property-and-apply-path.md`
- Unlocks: None
