# Story 002: Spike S2, PlayerState trims hold across seamless travel and rejoin

> **Epic**: Information Visibility Policy
> **Status**: Blocked — BLOCKED: needs the session-travel epic's seamless-travel and retained-PlayerState rejoin stories built (ADR-0005), which the prototype lacks; run when they land. (ADR-0005 spike S3 reported PASS 2026-10-25: production/qa/spikes/2026-10-25_S3-seamless-travel.md — the S3 part of this gate is cleared.)
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

The 2026-10-25 S2 spike passed within its testable scope: with the ADR-0001 section 4.3 trims, login, the player list, the owner HUD and possession keep working, and no per-player field reaches other clients. Travel and rejoin were not testable because the prototype has neither. Those are the two places where a PlayerState is most likely to leak or be re-created: re-associating a rejoiner's retained PlayerState calls `SetOwner`, which would broadcast a connection event to every client if `Owner` were not `COND_OwnerOnly` (E16), and a fresh PlayerState on rejoin changes the player array. This spike is the condition that lets ADR-0001's S2 gate be marked fully reported and feeds the frozen-player-array test.

**GDD**: `design/gdd/information-visibility-policy.md`
**Requirement**: `TR-ivp-006` (Single allowlist file, reflection audit and net-config audit on defaults and live instances); also `TR-ivp-003` (UE default overrides of R5)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0001: Crowd identity rules (sections 4.3, 4.4); also ADR-0005: Session, seamless travel and match-scoped state stores
**ADR Decision Summary**: `AStealthPlayerState` replicates only `PublicHandle` and `PlayerNamePrivate` plus held-constant `AActor` rows; every other engine default is disabled or `COND_OwnerOnly`, `Owner` is `COND_OwnerOnly`, and the in-round player array never changes (a rejoiner re-adopts its retained PlayerState).
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: `SetShouldUpdateReplicatedPing` is protected in 5.8.4, so the flag is set inside the PlayerState class (spike amendment). Seamless travel is disabled in single-process PIE unless `net.AllowPIESeamlessTravel=1`; travel results come from multi-process standalone (control manifest). This spike owns the [U] claim S2 in ADR-0001 and relies on ADR-0005's retained-PlayerState adoption in `InitPlayerState`.

**Control Manifest Rules (this layer)**:
- Required: **`AStealthPlayerState` replicates only `PublicHandle` and `PlayerNamePrivate`** — source: ADR-0001
- Required: **A rejoiner adopts its retained PlayerState in `InitPlayerState`**; `OnDeactivated` does not destroy the PlayerState while admission is `RejoinOnly` — source: ADR-0005
- Forbidden: **Never toggle `bIsInactive` or remove a PlayerState from the array between `Preparing` and the next travel; never spawn a fresh PlayerState for a reclaim** — source: ADR-0005
- Guardrail: **Seamless travel is disabled in single-process PIE** unless `net.AllowPIESeamlessTravel=1` — source: ADR-0005

---

## Acceptance Criteria

*From the story's spike definition (S2 travel and rejoin conditions left open in ADR-0001's Amendments):*

- [ ] GIVEN AStealthPlayerState with the ADR-0001 section 4.3 trims, WHEN login, a multi-process seamless ServerTravel and a rejoin that re-adopts the retained PlayerState (no new PlayerState, no SetOwner broadcast) occur, THEN every non-owner client receives only PublicHandle, PlayerNamePrivate and the constant AActor rows for every PlayerState, the player array count/order/names are unchanged through the rejoin, and the S2 verdict is extended to travel and rejoin in production/qa/spikes/.

---

## Implementation Notes

*Derived from ADR-0001 sections 4.3, 4.4, 8 and ADR-0005:*

- The "constant AActor rows" are the explicit list in ADR-0001 section 4.3: `bReplicateMovement`, `Role`/`RemoteRole`, `bHidden`, `bTearOff`, `bCanBeDamaged`, `Instigator`, each listed in the allowlist. Do not use an implicit "anything constant".
- `AStealthPlayerState::Owner`, `PlayerId` and `UniqueId` are `COND_OwnerOnly`; `Score`, `StartTime`, `bIsABot`, `bIsInactive`, `bIsSpectator`, `bOnlySpectator`, `bFromPreviousLevel`, `CompressedPing` are disabled. If this spike fails for a property, ADR-0001 section 8 holds the written per-property fallback (pinned-constant re-enable, sanitised `COND_InitialOnly` ids, a no-`SetOwner` rejoin path); a failure is handled by a superseding ADR, not by editing ADR-0001.
- Runs are multi-process (listen host plus clients, real UDP). The seamless travel and rejoin paths are the session-travel epic's; this spike only observes them. Use the capture mechanism chosen in story-001.
- No negative control is stated for the spike; add one: run the same scenario with the `PlayerState` `Owner` trim removed and show the `SetOwner` broadcast is detected, so the capture is known to see it.
- Prototype: reference only (`Spikes/StealthIdentitySpike.*`, analyzers in `production/qa/spikes/2026-10-25_tools/`).
- Unblocks when: the session-travel epic's seamless-travel and retained-PlayerState rejoin stories are built and ADR-0005 spike S3 has reported.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- story-001: multi-process S1 capture mechanism and the pointer check
- story-003: S14 and S1 rejoin and PawnRecovery possession
- story-020: the frozen-player-array acceptance test (AC-IVP-22)

---

## Files

- `production/qa/spikes/<date>_S2-travel-rejoin.md` (the verdict)
- Spike tooling under `tools/ci/` or `Source/StealthTestHarness/Private/Capture/` reused from story-001

---

## QA Test Cases

- **AC-1**: S2 seamless travel and rejoin PlayerState trims
  - Given: AStealthPlayerState with the ADR-0001 section 4.3 trims.
  - When: login, a multi-process seamless ServerTravel and a rejoin that re-adopts the retained PlayerState occur (no new PlayerState, no SetOwner broadcast).
  - Then: every non-owner receives only PublicHandle, PlayerNamePrivate and the constant AActor rows for every PlayerState; player array count, order and names unchanged through the rejoin; the S2 verdict is extended in production/qa/spikes/.
  - Edge cases: "constant AActor rows" must be the explicit list from ADR-0001 4.3. No negative control stated (the story adds one, see Implementation Notes). (Tier: T3.)

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: a written spike report `production/qa/spikes/<date>_S2-travel-rejoin.md` with the capture artefacts (a documented measurement, not a unit test).

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/information-visibility-policy/story-001-spike-s1-multiprocess-raw-bunch-capture.md`; session-travel epic (seamless-travel and retained-PlayerState rejoin stories, ADR-0005 spike S3); participant-state-roster epic (PlayerState carrier C-Public)
- Unlocks: `production/epics/information-visibility-policy/story-003-spike-s14-s1-rejoin-and-pawnrecovery-possession.md`, `production/epics/information-visibility-policy/story-020-frozen-player-array.md`
