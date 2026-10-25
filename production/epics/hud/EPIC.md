# Epic: HUD

> **Layer**: Presentation
> **GDD**: design/gdd/hud.md
> **Architecture Module**: HUD in `StealthPresentation` (view-models as plain `USTRUCT`s, widget stack with the HUD-R4 ZOrders, `PresentedOwnState`; the L0 world-anchor leaf also paints Pings rings)
> **Status**: Ready
> **Stories**: Not yet created — run `/create-stories hud`
> **Engine Risk**: HIGH (highest among governing ADRs; engine 5.8, installed 5.8.4)
> **Control Manifest**: 2026-10-25e
> **Build Order**: 19 of 19 (layer order; the risk-first sequence is in the Build Order and Gates section)

## Overview

The HUD is the client-side presentation layer that turns the viewer's own private state, their team's shared state and the round's public state into a small set of screen-space plates, rings, cards and lines. It owns no gameplay rule and no data: every value it shows is already held by the viewing client through exactly three carriers (the Policy's host-parity rule): the owner data on the `APlayerController` (strike and defence results, Penalty's wrong-target tally, exile flag and respawn due time, IIF hold status and reply codes, the owner's own concealment flag), the viewer's C-TeamView (own state and team, teammates' states, Objectives' team view: stage card, own steps, `RivalPrelimCount`), and the GameState's `FRoundPublicState`. It is deliberately quiet: the player's wrong-target tally and brace indicator bottom-left, the team's stage card and race steps top centre, at most three team lines top right, one result plate after each strike lower centre, a one-line banner at round end, and an always-empty screen centre.

The epic delivers the UMG layer stack drawn after post-process (HUD-R4 ZOrders, safe-margin anchors), view-models, plate and card timing at the local contact frame from the RTT-compensated server clock, `PresentedOwnState`, round surfaces with a rebuild on `RoundEpoch` change, the client-local settings hooks (scale, reduce motion, high contrast; the full Settings system is Vertical Slice), the latency warning threshold (`HudLatencyWarnRttSec`), the L0 leaf that paints Pings rings and edge rings (HUD-E22), the text-only round summary (HUD-E21) and the reconnect feed lines, and the subject-kind parity rule enforced by the automated leak suite. Art bible 7.1's rule, nothing on screen is ever about a person who is not an ally, is enforced as testable leak rules (HUD-R2, HUD-R3).

## Governing ADRs

| ADR | Decision Summary | Engine Risk |
|-----|------------------|-------------|
| ADR-0001: Crowd identity rules | Four audience carriers (C-Public allowlist on PlayerState, owner-only C-TeamView on PlayerController, C-Server, GameState public structs); every UE default that reveals player-ness is overridden; one pawn class, one look path, identity blindness. | HIGH |
| ADR-0008: Test and automation strategy for leak and parity tests | Headless pure-C++ cores, multi-client and multi-process leak harness, raw-bunch capture, net-emulation profiles, per-commit canaries. | MEDIUM |
| ADR-0018: Encounter data and outcome presentation timing | Outcome-blind FActionState at the queue slot, contact-time pending writes, owner-only results, MotionWarping (Beta in 5.8.4). | HIGH |
| ADR-0021: UI/HUD layering, CommonUI and Enhanced Input | UMG layer/ZOrder stack drawn after post-process, Common UI for menus only, client-local settings hooks, ping rings in HUD L0. | HIGH |

All governing ADRs are Accepted (user, 2026-10-25). Several are conditionally accepted; the outstanding gates are in Build Order and Gates below.

## GDD Requirements

7 of 7 requirements are covered by an Accepted ADR. Untraced requirements: None.

| TR-ID | Requirement | ADR Coverage |
|-------|-------------|--------------|
| TR-hud-001 | Reads only the three carriers | ADR-0001 ✅ |
| TR-hud-002 | Subject-kind parity rule and automated leak suite | ADR-0008 ✅ |
| TR-hud-003 | Layer/ZOrder stack after post-process; safe-margin anchors | ADR-0021 ✅ |
| TR-hud-004 | Server-clock timing: plates at local contact frame, tally with plate, `PresentedOwnState` | ADR-0018, ADR-0021 ✅ |
| TR-hud-005 | Round surfaces from `FRoundPublicState`; rebuild on `RoundEpoch` change | ADR-0021 ✅ |
| TR-hud-006 | Client-local settings hooks (scale, reduce motion, high contrast) | ADR-0021 ✅ |
| TR-hud-007 | Latency warning threshold (`HudLatencyWarnRttSec`; duplicate retired 2026-10-25) | ADR-0021 ✅ |

## Prototype Disposition

The code in `Source/StealthGame` is marked PROTOTYPE (`production/first-playable-report.md`). Production code follows ADR-0007 modules, and nothing is promoted until the ADR that governs it is built against.

**Replaces** `Source/StealthGame/Private/StealthHUD.cpp` / `Public/StealthHUD.h` (281 + 40 lines: an `AHUD` canvas pass) and the message plumbing in `StealthPlayerController.*` and `StealthGameMode.cpp` (`MessagePlayer`, `MessageController`, `MessageTeam`, `MessageAll`). The prototype HUD showed kill confirmation, civilian count, defence state and cooldown, stage progress, enemy-stage alert, round banner and teammate markers, seen on clients and passing the first-playable networked tests; the production HUD is UMG and carrier-driven (ADR-0021). The prototype leaks (it reads a team field replicated on every pawn) are not promoted.

## Build Order and Gates

- **Depends on (epics)**: participant-state-roster, round-match-framework (`FRoundPublicState`, client `EstimatedServerNow()`), information-visibility-policy, assassination-defence-reversal and npc-kill-penalty-informants (owner data), objectives-chain (team view), concealment-bushes, input-interaction-framework (reply sink), tuning-data-config, camera-system (view state), session-travel (reconnect), teammate-markers (soft)
- **Gates and risks**: ADR-0021 conditional gate: UI-V1..UI-V4 and the HUD and Teammate Markers GDD re-reviews. UI is a HIGH-risk domain for 5.8 (Enhanced Input and Common UI unified). Carries the HUD halves of MVP items 9 (round-summary lines, HUD-E21, AC-HUD-27) and 10 (reconnect feed lines, AC-HUD-28 a-h), both gated on the Round Event Log and Session rejoin existing. UI stories require retained screenshots in `production/qa/evidence/`. Days-per-AC (0.52) is UI-evidence-heavy; confirm at story breakdown. As the last Presentation epic it also owns the cross-cutting **MVP exit verification** (a final 5v5 evidence pass over the Presentation surfaces; the 10-player network pass itself is owned by procedural-city-generator).
- **Estimate**: about 12 bottom-up days (human-paced yardstick, `production/estimates/2026-10-25_mvp-rebaseline.md`, Method B work packages); planning figure about 7.2 days after the uniform 0.6 calibration. Low confidence: no sprint history exists, so run the 2-week calibration spike and re-baseline.
- **Acceptance criteria in the GDD**: about 23 (counted as GIVEN-form criteria in the Acceptance Criteria section).
- **MVP build order**: risk-first per `design/gdd/systems-index.md`: graybox map and mannequin-variant pool first, then camera toggle and traversal replication, then the Meshy pool swap, then the generator.

## Definition of Done

- Every story implemented, reviewed and closed with `/story-done`
- Every acceptance criterion of design/gdd/hud.md verified
- Logic and Integration stories have passing tests in the test root (`Source/<Module>/Private/Tests/`)
- Visual/Feel and UI stories have retained screenshots in `production/qa/evidence/`
  (a lead sign-off too for Visual/Feel)
- Stories embed Control Manifest version 2026-10-25e and every governing ADR's conditional gate has reported before dependent work is marked Done
