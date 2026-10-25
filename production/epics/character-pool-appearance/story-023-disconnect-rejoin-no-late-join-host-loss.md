# Story 023: Disconnect, rejoin, no late join and host loss keep the ledger conserved

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

**ADR Governing Implementation**: ADR-0012: Character pool data and appearance allocation
**ADR Decision Summary**: A disconnected participant's pawn is driven by the Crowd NPC controller at once with its card staying `BoundLive`; rejoin inside `DisconnectGraceSec` re-possesses the same pawn and card; expiry releases the card to the NPC side (`ReleaseCard`) with no visible change; no late join exists; host loss ends the session and the next session re-deals.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0012)
**Engine Notes**: Controller handoff is Crowd's and Shared Character's (not coded yet): use a stub controller and an injected clock. Session admission and host-loss are not coded: use a Session adapter fake.

**Why this story**: CP-13 keeps `D` and every `c_a` unchanged across leave and rejoin and forbids a late-join card. AC-CP-13 is "Blocked by Crowd (controller handoff); the ledger half runs now with a stub controller" (GDD); AC-CP-30 and AC-CP-31 depend on Session. **The criteria below are revised per QL-STORY-READY: the Session-dependent halves of AC-2 and AC-3 are DEFERRED until Session's admission and host-loss implementations exist; until then they verify only the allocator side against a fake** (a fake Session would otherwise test only itself). Read GDD CP-13, AC-CP-13, 30, 31, Edge Cases (Disconnect, join and rejoin).

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: `ReleaseCard` pairs with the Roster's `ReleaseToCrowd` in one call stack (ADR-0012)
- Forbidden: Never destroy the pawn in `PawnLeavingGame`; never hand a disconnected pawn to a frozen controller (a tell) (ADR-0010)
- Guardrail: n/a

---

## Acceptance Criteria

*From GDD AC-CP-13, AC-CP-30, AC-CP-31; AC-2 and AC-3 revised per QL-STORY-READY:*

- [ ] **AC-1** GIVEN a participant disconnects (injected clock), THEN in the same server tick the pawn is possessed by the Crowd NPC controller (a stub until Crowd lands) and its card stays `BoundLive`; rejoin before `DisconnectGraceSec` restores the same pawn and card; expiry leaves `D` and every `c_a` unchanged with the pawn keeping its `AppearanceId`; a pawn gone at expiry leaves its card `NpcVacant` and refilled; a participant `Disconnected` at Round P6 still gets a bound AI-driven pawn (test `...Ledger.DisconnectAndRejoin`).
- [ ] **AC-2** GIVEN a Session adapter fake returning `InProgress` for an unknown identity key during `Preparing`, `Active` and `Ended`, THEN the allocator call count stays 0 (test `...Ledger.NoLateJoinCard`); the check against the real Session is DEFERRED until Session's admission lands.
- [ ] **AC-3** GIVEN two sessions in sequence, WHEN the second deals, THEN no ledger or stream state is shared with the first (test `...Ledger.HostLossRedeals`, ledger half only); the host-loss path itself is DEFERRED until Session's host-loss implementation lands.

---

## Implementation Notes

- Boundary of the grace timing: rejoin exactly at the boundary is a case in the test (Round owns `DisconnectGraceSec`, default 30 s).
- Participant-state-roster story 054 (`story-054-disconnect-grace-timing.md`) owns the Roster side of the grace clock; this story's stub controller and Roster fake must match its interface.
- Revisit and complete the deferred halves when Session lands (note it in `/story-done`).

**Prototype disposition**: none.

---

## Out of Scope

- Crowd's controller handoff blend (shared-character-locomotion story 027, crowd epic); Session admission and host-loss tests (session-travel epic)

---

## Files

- `Source/StealthGameplay/Private/Tests/CharacterPool/DisconnectRejoinLedgerTest.cpp`
- `Source/StealthGameplay/Private/Tests/CharacterPool/StubCrowdController.h`

---

## QA Test Cases

*QL-STORY-READY: GAPS → AC-2 and AC-3 revised (adapter-only, deferred halves) with the qa-lead's proposed text; not re-run (unattended; decision logged).*

**Test: disconnect and rejoin (AC-1, stub controller)**
- Given: an injected clock, a stub Crowd controller and a dealt ledger.
- When: a participant disconnects, then rejoins before `DisconnectGraceSec`; in a second case expires; in a third has the pawn gone at expiry.
- Then: in the same tick the pawn is possessed by the stub and its card stays `BoundLive`; rejoin restores the same pawn and card; expiry leaves `D` and `c_a` unchanged and the pawn keeps its `AppearanceId`; a missing pawn leaves the card `NpcVacant` and refilled; a participant `Disconnected` at Round P6 still gets a bound AI-driven pawn.
- Edge cases: rejoin exactly at the grace boundary.

**Test: no late join card (AC-2, adapter only)**
- Given: a Session adapter fake returning `InProgress`.
- When: an unknown id connects in `Preparing`, `Active` and `Ended`.
- Then: the allocator call count stays 0.
- Edge cases: DEFERRED for the real Session.

**Test: host loss redeal (AC-3, ledger only)**
- Given: two sessions in sequence.
- When: the second session deals.
- Then: no ledger or stream state is shared with the first.
- Edge cases: DEFERRED for the real host-loss path.

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthGameplay/Private/Tests/CharacterPool/DisconnectRejoinLedgerTest.cpp` — automation tests `StealthGame.CharacterPool.Ledger.DisconnectAndRejoin`, `...Ledger.NoLateJoinCard`, `...Ledger.HostLossRedeals` (OR a documented playtest for the deferred halves)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/character-pool-appearance/story-021-release-card-strike-ordering-and-own-card-respawn.md`, `production/epics/character-pool-appearance/story-022-ledger-conservation-under-event-mix.md`, `production/epics/participant-state-roster/story-054-disconnect-grace-timing.md`
- Unlocks: None
