# Story 084: AC 48 PawnReplicatedValueParity

> **Epic**: Input & Interaction Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/input-interaction-framework.md`
**Requirement**: `TR-iif-009`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0003: Replication, relevancy and the concealment cull hybrid
**ADR Decision Summary**: ADR-0003 makes every pawn one class with one replication configuration, requires fixed-width serialisers for outcome-bearing structs, and gates the whole replicated surface through one checked-in allowlist audit (property, owning class, condition, audience, carrier) that enumerates replicated properties on class defaults and live instances; `PlayerState`, `Controller` and `Owner` are `COND_OwnerOnly` on the pawn.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: ADR-0003 (networking, Knowledge Risk HIGH). The project uses the legacy NetDriver (ADR-0001/ADR-0003); Iris is production-ready in 5.8 but is not the replication system here, so the per-property byte capture (Network Insights) is taken on the legacy driver and the story must state this and the capture method (qa-lead gap 2). Relevant verification: S13 (cadence and correction parity for PC- versus AI-possessed pawns as seen by a third client) and the fixed-width `NetSerialize` byte-identity check.

**Control Manifest Rules (this layer)**:
- Required: Outcome-bearing replicated structs serialise to a fixed bit width — source: ADR-0003
- Required: A new replicated property, condition or replicated class does not ship without an allowlist row signed off by its owners; the audit fails the build otherwise — source: ADR-0003
- Forbidden: Never put owner-only data on the pawn — source: ADR-0011
- Guardrail: Replication: pawns at a fixed 15 Hz within the 36 KB/s worst-case line (60 pawns) — source: ADR-0003

---

## Acceptance Criteria

*From GDD `design/gdd/input-interaction-framework.md`, scoped to this story:*

- [ ] AC 48 (Integration, N `PawnReplicatedValueParity`): **GIVEN** a bound pawn and an NPC pawn **WHEN** an opponent client dumps every replicated property's value and per-property bytes (Network Insights) **THEN** the property sets match; **AND** `PlayerState` and `Owner` are null on both; **AND** the bytes per update match for the same state.

---

## Implementation Notes

*Derived from ADR-0003 Decision sections 1, 4, 6 and 8 and ADR-0011 Decision section 7; GDD R12 ("What replicates"):*

**What this story builds.** The runtime parity test from the opponent client's point of view: one participant-bound pawn and one NPC pawn in the same action state, replicated to an opponent client under net emulation (profile N: PktLag 50 ms per direction, 1 percent loss), with every replicated property dumped (names, values, per-property bytes). The property name sets must match, `PlayerState` and `Owner` must be null on both on that client (they are `COND_OwnerOnly`), and bytes per update must match for the same state. This is the observed side of GDD R12 and ADR-0003's allowlist: nothing the IIF replicates (`FActionState` only, fixed width) lets an observer separate a human pawn from an NPC.

**Gaps to resolve in the test design (qa-lead).** "Values match" cannot hold for position and similar fields. Define the comparison: property name sets are equal; for state-equal fields (`Pose`, `Seq`, `Variant`, timestamps given equal inputs) the values are equal; per-property bit sizes are equal for equal state. State which replication system is active (legacy NetDriver per ADR-0003) and the capture method (Network Insights per-property bytes, trace file).

**Test notes.** One bound pawn and one NPC pawn in the same action state, test per `Pose`; retain the trace file under `production/qa/evidence/`. The `FActionState` fixed-width serialisation is ADR-0003 section 4 (bit widths owned by ADR-0018); the allowlist file and its audit are owned by the information-visibility-policy epic, so this story reads them and does not edit them.

**Prototype.** New code, no prototype counterpart (the prototype replicated per-feature pawn state with no allowlist).

**Tuning keys (defaults).** None for this story. Net config comes from the SCL keys such as `PawnNetUpdateHz` 15 (ADR-0003 section 1).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/input-interaction-framework/story-068-no-owner-data-on-pawn.md`: static check that only `FActionState` is IIF-written on the pawn and owner data lives on the PlayerController.
- `production/epics/input-interaction-framework/story-069-force-net-update-on-write.md`: forced net update per `ActionPose` write.
- `production/epics/input-interaction-framework/story-082-action-pose-parity-observed.md`: observed onset-to-offset durations and cadence.
- `production/epics/input-interaction-framework/story-087-action-pose-bandwidth.md`: ActionPose bandwidth at 50 pawns.

---

## Files

- `Source/StealthTestHarness/Private/PawnReplicatedValueParityTest.cpp` — CQTest `NETWORK_TEST_CLASS(PawnReplicatedValueParity, "StealthGame.Input.PIE")`, full name `StealthGame.Input.PIE.PawnReplicatedValueParity.<Method>`; net-emulation profile N (planned)
- `production/qa/evidence/iif-pawn-replicated-value-parity.utrace` and summary — retained Network Insights trace (planned names)

---

## QA Test Cases

*Test cases not yet defined — run /qa-plan to generate them.*

*QL-STORY-READY: GAPS (qa-lead):*
- Gap 1: "dumps every replicated property's value ... values match" cannot hold for position and similar fields; define: property name sets equal; for state-equal fields (`Pose`, `Seq`, `Variant`, timestamps given equal inputs) values equal; per-property bit sizes equal for equal state.
- Gap 2: Network Insights per-property bytes under Iris (production-ready in 5.8): state which replication system is active and the capture method.

*Draft spec from the qa-lead (unconfirmed), test `PawnReplicatedValueParity`:* Given an N profile, an opponent client, one bound pawn and one NPC pawn in the same action state, when replicated properties are dumped (names, per-property bytes), then name sets are equal, per-property sizes are equal for equal state, and `PlayerState` and `Owner` are null on both on the opponent client. Edge cases: test per `Pose`; trace file retained under `production/qa/evidence`.

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/PawnReplicatedValueParityTest.cpp` (CQTest `StealthGame.Input.PIE.PawnReplicatedValueParity`) must exist and pass, with the Network Insights trace retained in `production/qa/evidence/` (`/story-done` checks that the test EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/input-interaction-framework/story-069-force-net-update-on-write.md`; cross-epic: information-visibility-policy (stories not yet written; replication allowlist and audit owner); cross-epic: shared-character-locomotion (stories not yet written; pawn net configuration); cross-epic: crowd-npc-behaviour (stories not yet written; NPC pawn in the same action state)
- Unlocks: None
