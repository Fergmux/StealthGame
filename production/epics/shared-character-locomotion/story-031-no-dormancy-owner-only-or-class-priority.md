# Story 031: No dormancy, owner-only relevancy or class priority on any pawn

> **Epic**: Shared Character & Locomotion
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: [set by /dev-story when implementation begins]

## Context

**GDD**: `design/gdd/shared-character-locomotion.md` (R13 relevancy and dormancy bans, owned here; AC-SCL-41; IIF R12)
**Requirement**: `TR-scl-009`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0003: Replication, relevancy and the concealment cull hybrid (section 1; relevancy changes only through section 5's concealment cull)
**ADR Decision Summary**: No Shared Pawn uses net dormancy (`DORM_Never`, no `SetNetDormancy` or `FlushNetDormancy` call on a pawn anywhere), `bOnlyRelevantToOwner`, a net cull distance, a per-class or per-instance priority or relevancy override, or teammate-only always-relevance. The only relevancy variation is the concealment cull, keyed on concealment state alone.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: The owner connection's engine-internal priority boost for its own view target is the only per-connection difference and acts only on the owner's own connection.

**Control Manifest Rules (this layer)**:
- Required: pawn net configuration as in story 030.
- Forbidden: no `SetNetDormancy`/`FlushNetDormancy` on a pawn (grep ban); never call `ForceNetRelevant` or `ForceActorRelevantNextUpdate` for a pawn; no per-class priority or cull-distance entries in config.
- Guardrail: none.

---

## Acceptance Criteria

*From GDD `design/gdd/shared-character-locomotion.md`, scoped to this story:*

- [ ] **AC-SCL-41** [P] `NoDormancyOwnerOnlyOrClassPriority`: GIVEN the Shared Pawn class default and 50 live instances over 60 s including possession changes, a takeover and 30 s idle, WHEN settings are read each second and the source is scanned, THEN `NetDormancy` = `DORM_Never`, `bOnlyRelevantToOwner` = false and `NetPriority` = 3.0 on every sample, there are 0 `SetNetDormancy` calls and 0 per-class priority or cull-distance entries in config; AND the only relevancy change observed is the concealment cull, which occurs only while a pawn is committed concealed (the Visibility Policy owns the cull test).

*QA clarification (create-stories 2026-10-25, not a GDD change):* the concealment-cull clause is deferred to the Visibility Policy epic and asserted there when the cull exists.

---

## Implementation Notes

*Derived from ADR-0003 section 1:*

- This is mostly a guard story: a once-per-second sampler over 50 live instances, a source scan (grep ban on `SetNetDormancy`, `FlushNetDormancy`, `ForceNetRelevant`, `ForceActorRelevantNextUpdate`, `SetNetCullDistanceSquared`) and a config scan of `Config/*.ini` for per-class priority or cull-distance entries. Include the possession changes, one takeover (story 028) and 30 s idle in the run.
- **Prototype replaced**: none (the prototype never set these); this is the regression guard.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 030: the dump of equal settings.
- The concealment cull and its tests (`production/epics/information-visibility-policy/EPIC.md`, `production/epics/concealment-bushes/EPIC.md`).

---

## Files

- `Source/StealthGameplay/Private/Tests/Locomotion/NoDormancyOwnerOnlyOrClassPriorityTest.cpp` (planned)
- `tests/lint/` source-scan rule (location per the ADR-0008 include-lint mechanism; planned)

---

## QA Test Cases

*QL-STORY-READY: ADEQUATE (after clarification; concealment-cull clause deferred). Test `StealthGame.Locomotion.NoDormancyOwnerOnlyOrClassPriority`.*

Test: no dormancy, owner-only relevancy or class priority (AC-SCL-41)
  Given: the Shared Pawn class default and 50 live instances over 60 s, including possession changes, a takeover and 30 s idle
  When: settings are read each second and the source and config are scanned
  Then: `NetDormancy` = `DORM_Never`, `bOnlyRelevantToOwner` = false and `NetPriority` = 3.0 on every sample; 0 `SetNetDormancy` calls; 0 per-class priority or cull-distance entries in config
  Edge cases: the concealment-cull clause (only relevancy change, only while committed concealed) is asserted when the Visibility Policy epic builds the cull; the test overlaps AC-SCL-16 on purpose (regression guard)

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthGameplay/Private/Tests/Locomotion/NoDormancyOwnerOnlyOrClassPriorityTest.cpp`, test name `StealthGame.Locomotion.NoDormancyOwnerOnlyOrClassPriority` — must exist and pass (not player-observable)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/shared-character-locomotion/story-030-net-config-parity.md`, `story-028-informant-respawn-takes-over-unseen-crowd-pawn.md`
- Unlocks: None
