# Story 016: Owner-only cooldown end time on the PlayerController, defence state never replicated

> **Epic**: Assassination & Defence Reversal
> **Status**: Ready
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: S
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/assassination-defence-reversal.md` (ASN-R18; UI-ASN-3; AC-ASN-36)
**Requirement**: `TR-asn-013`, `TR-asn-002`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0018: Encounter data and outcome presentation timing (§1.5 `UDefenceOwnerComponent`)
**ADR Decision Summary**: The owner's cooldown end time travels only as `double CooldownEndServerTime` on a replicated `UActorComponent` of the owning `AStealthPlayerController` (owner-only), written at `Prepare(Begin)` execution as `LastPrepareActivation + DefenceCooldownSec` and copied on re-pawn; the pawn-side defence component is server-only and has no replicated property.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (networking; owner-only component replication on the legacy driver)
**Engine Notes**: An owner data component on the PlayerController follows ADR-0001's carrier model (IVP fact row `DefenceState`, Owner, PlayerController, already exists; an allowlist row is required). Component replication is `COND_OwnerOnly`; the component is not a pawn property so pawn byte parity is unaffected. The brace window itself is the IIF's `FHoldStatus` (IIF stories 074 and 088).

**Control Manifest Rules (this layer)**:
- Required: Owner data lives on the PlayerController component, never on the pawn; one allowlist row and one Visibility Policy row per replicated field — source: ADR-0001, ADR-0003
- Forbidden: `LastPrepareActivation` and `LockoutEnd` never reach any client; the `UDefenceStateComponent` has no replicated properties — source: ADR-0018, GDD ASN-R18
- Guardrail: One double per owner; written on `Prepare(Begin)` and on re-pawn only

`UDefenceOwnerComponent { double CooldownEndServerTime = −1 }` is the only defence datum a client ever holds, and only the owner holds it. The HUD brace indicator reads it (story 024) to count the 60 s Cooling numeral; it is derived from the same stamp as story 003's cooldown so the displayed end and the server's `Cooldown` verdict agree.

---

## Acceptance Criteria

*From GDD `design/gdd/assassination-defence-reversal.md` (AC-ASN-36, ASN-R18) and ADR-0018 §1.5, scoped to this story:*

- [ ] **AC1** (AC-ASN-36, [Integration/BLOCKING][Net] `StealthGame.Assassination.DefenceStateServerOnly`): **GIVEN** a braced player P **WHEN** all traffic is captured **THEN** only P's own PlayerController receives P's cooldown end time; no client receives `LastPrepareActivation` or `LockoutEnd`; and the defence component has no replicated properties (reflection check).
- [ ] **AC2** (ASN-R18 / ADR-0018 §1.5, [Integration/BLOCKING]): **GIVEN** a `Prepare(Begin)` that executes at t = 100 with `DefenceCooldownSec` 60 **THEN** the owner's `CooldownEndServerTime` becomes 160.0, a re-pawn by `PawnRecovery` copies the stamp's end (or stamps now plus 60 when unknown), and a re-press during the window or cooldown leaves the value unchanged.

---

## Implementation Notes

*Derived from ADR-0018 §1.5 and GDD ASN-R18:*

Attach `UDefenceOwnerComponent` to `AStealthPlayerController` as a default subobject (replicated, `COND_OwnerOnly`); register it on the C-TeamView-style owner-part carrier list the Visibility Policy epic defines so the owner-part ordering rules hold across controller swaps (IVP story 014 carrier-audience-consistency; Roster story 041 team-view repush on controller swap). Write sites: the `Prepare(Begin)` execution follow-on (same slot as the stamp in story 003) and the re-pawn copy. The component never decides anything on the server; the server's verdict is the stamp.

Capture test: reuse the story 012 capture rig and assert on the PlayerController's replicated components for P and for every other connection; add the reflection check to the generic replicated-property audit as a specific expectation (the pawn-side `UDefenceStateComponent` registers zero lifetime properties).

**Prototype replaced:** the prototype replicates the defence state as `bPreparingDefence` on the pawn (visible to every client) and `LastDefenceActivationTime` on the PlayerState; both are retired by story 027.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-003-defence-state-component-and-cooldown.md`: the server stamp and `Cooldown` verdict.
- `story-024-owner-result-matcher-and-plate-timing.md`: HUD display of the cooldown and brace square.
- IIF stories 074 and 088: `FHoldStatus` protected-on-confirm and owner status bandwidth.

---

## Files

- `Source/StealthGameplay/Public/View/Features/Assassination/DefenceOwnerComponent.h` (`UDefenceOwnerComponent`, owner-only)
- `Source/StealthGameplay/Private/Features/Assassination/DefenceOwnerComponent.cpp`
- `Source/StealthGameplay/Private/Features/Assassination/PreparePredicate.cpp` (stamp write also updates the owner component)
- `Source/StealthTestHarness/Private/Assassination/DefenceStateServerOnlyTest.cpp` (`StealthGame.Assassination.PIE.DefenceStateServerOnly`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

QL-STORY-READY: GAPS
- AC1: "no client receives `LastPrepareActivation` or `LockoutEnd`" is not checkable by value search; proposed rewrite (unconfirmed): the capture's replicated-property list for every connection contains only `UDefenceOwnerComponent::CooldownEndServerTime` (owner PlayerController, owner connection only); no property named or typed as `LastPrepareActivation` or `LockoutEnd` is dirty on any connection; the pawn-side component registers 0 lifetime properties.
- AC2: the re-pawn has two branches with no fixtures and the "now" is unspecified; proposed rewrite (unconfirmed): with an injected clock: known stamp end 160.0 gives 160.0 after re-pawn; unknown stamp at t = 130 gives 190.0 (± 1 tick); a re-press at t = 110 and t = 170 leaves 160.0 unchanged and causes no component write.

- **AC-1**: Defence state server-only
  - Given: a braced player P and 3 connections
  - When: all traffic is captured and the component is reflected
  - Then: only P's PlayerController receives `CooldownEndServerTime`, no client receives the server-only stamps, and the pawn defence component has 0 replicated properties
  - Edge cases: a controller swap re-pushes only to the new owner; a spectator connection receives nothing
- **AC-2**: Cooldown end value
  - Given: `Prepare(Begin)` executing at t = 100 with 60 s cooldown, an injected clock
  - When: the stamp is written, a re-pawn occurs, and a re-press occurs
  - Then: `CooldownEndServerTime` = 160.0, re-pawn copies the end (or stamps now + 60 when unknown), and the re-press leaves it unchanged
  - Edge cases: a re-press inside the brace window; a re-press at cooldown end ± 1 tick; unknown stamp

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: `DefenceStateServerOnlyTest.cpp` (PIE, 3 clients, traffic capture) — must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `story-003-defence-state-component-and-cooldown.md`, `story-004-wire-structs-fixed-width.md`; cross-epic `production/epics/information-visibility-policy/story-014-carrier-audience-consistency.md`, `story-015-replicated-property-audit.md`; `production/epics/participant-state-roster/story-041-team-view-repush-on-controller-swap.md`; `production/epics/input-interaction-framework/story-056-assassination-cooldown-no-reset.md`
- Unlocks: `story-024-owner-result-matcher-and-plate-timing.md`
