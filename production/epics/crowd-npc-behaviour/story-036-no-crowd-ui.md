# Story 036: No crowd UI, debug draw or indicator in Shipping

> **Epic**: Crowd & NPC Behaviour
> **Status**: Ready
> **Layer**: Feature
> **Type**: UI
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/crowd-npc-behaviour.md`
**Requirement**: `TR-crowd-001`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-crowd-001 = Server-only Crowd Director subsystem.)*

**ADR Governing Implementation**: ADR-0016: Crowd AI architecture
**Also relevant**: GDD UI Requirements (read that section only), ADR-0007 (server-only headers, Shipping exclusions)
**ADR Decision Summary**: Crowd has no UI. No crowd debug, NPC indicator or notice may exist in Shipping; the Director is server-only and replicates nothing, so observers see only the pawn state the command interface produces.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: Debug draws and the recordable virtual-input stream are wrapped in `#if !UE_BUILD_SHIPPING`; Insights scopes follow the engine's non-Shipping tracing rules. Capture uses a Shipping-configuration listen server and client.

**Why this story**: A single debug marker or "NPC" label visible to a player would end the game's premise; AC-CNB-25 is the guard that none ships. It is a UI-type story only because its evidence is retained screenshots. Read GDD AC-CNB-25 and the UI Requirements section.

**Control Manifest Rules (Feature layer, manifest 2026-10-25e)**:
- Required: crowd debug tooling lives under `#if !UE_BUILD_SHIPPING` (ADR-0016); evidence retained in `production/qa/evidence/` (coding standard: UI stories retain a screenshot of each screen touched)
- Forbidden: shipping a crowd UI, debug draw or indicator in Shipping (AC-CNB-25) (ADR-0016)
- Guardrail: Shipping builds contain no `Crowd.*` Insights scopes or `FCrowdVirtualInputFrame` symbol

---

## Acceptance Criteria

*From GDD `design/gdd/crowd-npc-behaviour.md` AC-CNB-25 [UI/BLOCKING], scoped to this story:*

- [ ] (AC-CNB-25) GIVEN a Shipping-configuration listen server and client, WHEN each viewer class's screen is captured during a round with kills, emergences and a takeover, THEN no crowd debug, NPC indicator or notice is drawn (retained screenshots).
- [ ] Crowd debug draws, `FCrowdVirtualInputFrame` recording and the `Crowd.*` Insights scopes are compiled out of or disabled in Shipping (source scan and symbol check).

---

## Implementation Notes

*Derived from ADR-0016 Implementation Guidelines:*

- Viewer classes to capture: a participant on each team, a spectating or eliminated viewer if that state exists at MVP, and the listen-server host's own screen; each during a round with kills, emergences and a takeover in progress.
- Source scan: grep Crowd sources for `DrawDebug*`, `GEngine->AddOnScreenDebugMessage`, `UE_LOG` use (banned anyway) and any `UMG` includes (Crowd includes no Presentation header).
- Symbol check: confirm via the Shipping build's symbol/link map or a compile-time static assertion that the debug-only symbols are absent.

**Prototype disposition** (`production/first-playable-report.md`, `Source/StealthGame`): None; the prototype HUD draws teammate markers but no crowd overlay. Do not port `StealthBot` console output paths into Crowd.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- The HUD and Teammate Markers epochs: the actual player-facing UI
- Story 010: the recording symbol itself (this story only checks it is absent in Shipping)

---

## Files

- `production/qa/evidence/<date>_crowd-no-ui-*.png` (one per viewer class)
- `production/qa/evidence/crowd-no-ui-evidence.md`
- `Source/StealthGameplay/Private/Tests/Crowd/CrowdShippingScanTest.cpp`

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY, 2026-10-25).*

- **AC-1**: `Crowd.NoCrowdUi`
  - Setup: Shipping-configuration listen server and client; a round with kills, emergences and a takeover
  - Verify: capture each viewer class's screen during those events
  - Pass condition: no crowd debug, NPC indicator or notice is drawn; screenshots retained in `production/qa/evidence/`
- **AC-2**: Shipping exclusions
  - Setup: Shipping build and a source scan
  - Verify: check crowd debug draws, `FCrowdVirtualInputFrame` recording and the `Crowd.*` Insights scopes
  - Pass condition: all are compiled out or disabled in Shipping

---

## Test Evidence

**Story Type**: UI
**Required evidence**:
- UI: retained screenshots of each viewer class in `production/qa/evidence/` plus `production/qa/evidence/crowd-no-ui-evidence.md`

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/crowd-npc-behaviour/story-004-director-core-ledger-and-phase-gating.md`
- Unlocks: None
