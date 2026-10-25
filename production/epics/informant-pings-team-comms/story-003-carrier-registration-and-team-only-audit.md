# Story 003: Ping carrier registration and team-only audience audit

> **Epic**: Informant Pings & Team Communication
> **Status**: Ready
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/informant-pings-team-comms.md` (PNG-R6 carrier, PNG-R9, IVP `TeamPing` row; AC-PNG-03)
**Requirement**: `TR-png-012`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0003: Replication, relevancy and the concealment cull hybrid (§6 Owner and Team data on the wire, §8 allowlist and audit); carrier model from ADR-0001 §5
**ADR Decision Summary**: Every replicated property of every actor class a client receives is on a checked-in allowlist with an audience, audited by an automated pass; Team-audience data lives only on C-TeamView parts of the receiving player's own `AStealthPlayerController`, and the team-only rule is enforced by the absence of data, not by a client-side filter. Default deny on every carrier.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0003 networking)
**Engine Notes**: The audit reads replicated-property metadata through reflection (`GetLifetimeReplicatedProps` registrations); no post-cutoff API is needed. The component must be on the controller's registered sub-object list (ADR-0001 E11). ADR-0003 spikes S9, S10, S12 and S13 gate other behaviour (the concealment cull), not this audit.

**Control Manifest Rules (this layer)**:
- Required: the replicated-property allowlist carries a row per carrier part with its audience; the audit fails on an unlisted replicated property — source: ADR-0003
- Required: `UTeamPingViewComponent` is registered only on `AStealthPlayerController` with Team audience — source: ADR-0001, ADR-0003
- Forbidden: Never put a ping field on `AStealthPawn`, `AStealthPlayerState`, the GameState or any `All`-audience carrier — source: ADR-0001, ADR-0003

---

## Acceptance Criteria

*From GDD `design/gdd/informant-pings-team-comms.md` (AC-PNG-03, PNG-R6, PNG-R9), scoped to this story:*

- [ ] **AC1** (AC-PNG-03, `[Logic]` `StealthGame.Pings.AuditCarrier`): **GIVEN** the build **WHEN** the IVP R11 replicated-property audit runs **THEN** `UTeamPingViewComponent` is registered only on `AStealthPlayerController`, its fields match the allowlist row with Team audience, and no ping field exists on `AStealthPawn`, `AStealthPlayerState` or the GameState.
- [ ] **AC2** (derived, ADR-0003 §8 and the IVP fact matrix): **GIVEN** the allowlist file and the IVP fact matrix **WHEN** checked **THEN** both hold the `UTeamPingViewComponent` / `TeamPing` rows (Team audience), the component is on the controller's registered sub-object list, and deleting the allowlist row or adding the component to the pawn makes the audit fail (negative controls watched failing).

---

## Implementation Notes

*Derived from ADR-0003 Decision §6 and §8 and ADR-0001 §5:*

This story adds the Pings rows to the artefacts the Information Visibility Policy epic owns; it does not build the audit. Add the `UTeamPingViewComponent` row (properties of `FTeamPingView`, audience `Team`, carrier `AStealthPlayerController`) to the allowlist file created by `information-visibility-policy/story-013-allowlist-content.md`, and confirm the `TeamPing` row exists in the R3 matrix fixture (`story-011-matrix-per-fact.md`). Register the component on the controller in the composition root's controller constructor via the registered sub-object list. Negative controls (ADR-0008): one run adds the component to `AStealthPawn` in a test-only subclass behind `WITH_STEALTH_TEST_SEAMS`, one deletes the allowlist row from a fixture copy; each must make the audit fail. IVP `story-053-scope-additions-2026-10-25.md` is Blocked on this component existing and carries the wider audience assertions; this story only supplies the component and its rows.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 013: the two-team PIE proof that opponent machines receive nothing.
- IVP epic: the audit tool itself, the matrix, and the cross-part "never to opponents" inspection (`story-015`, `story-021`).
- Story 002: the component and struct definitions.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthFoundation/Private/Tests/Data/ReplicationAllowlist.json` (add the `UTeamPingViewComponent` section)
- `Source/StealthGame/Private/StealthPlayerController.cpp` (component registration in the constructor)
- `Source/StealthGameplay/Private/Tests/Pings/AuditCarrierTest.cpp` (class `FPingsAuditCarrierTest`, name `StealthGame.Pings.AuditCarrier`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

**Test: AC1 carrier registration audit**
- Given: the IVP R11 replicated-property audit.
- When: run on the build.
- Then: `UTeamPingViewComponent` registered only on `AStealthPlayerController`; fields match the Team-audience allowlist row; no ping field on `AStealthPawn`, `AStealthPlayerState` or the GameState.
- Edge cases: the component added to the pawn makes the audit fail.

**Test: AC2 allowlist, fact matrix, sub-object list and negative controls**
- Given: the allowlist file, the IVP fact matrix, and the controller's sub-object list.
- When: check for the `UTeamPingViewComponent` / `TeamPing` rows; then run the audit with the allowlist row deleted, and again with the component on the pawn.
- Then: rows and sub-object entry present with Team audience; both mutated runs fail.
- Edge cases: both failures must be seen before the clean pass is trusted (test-standards).

QL-STORY-READY: ADEQUATE

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Pings/AuditCarrierTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/informant-pings-team-comms/story-002-team-ping-wire-types-and-slot-component.md`; cross-epic `production/epics/information-visibility-policy/story-013-allowlist-content.md`, `story-015-replicated-property-audit.md`
- Unlocks: `production/epics/informant-pings-team-comms/story-013-opponent-sees-nothing.md`; `production/epics/information-visibility-policy/story-053-scope-additions-2026-10-25.md`
