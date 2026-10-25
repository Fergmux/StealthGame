# Story 047: Latency icon with hysteresis, three-step stair shape and host exclusion

> **Epic**: HUD
> **Status**: Ready
> **Layer**: Presentation
> **Type**: UI
> **Estimate**: S
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/hud.md` · UX spec `design/ux/hud.md`
**Requirement**: `TR-hud-007`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0021: UI/HUD layering and Common UI
**ADR Decision Summary**: The latency icon is a persistent L1 element (ZOrder 30) in the `Latency` cluster of `FHudViewModel`, driven by `FHudInputs` own-RTT sampled from the local net connection (no carrier on the wire), with its visibility rule evaluated in the plain C++ presenter core and the widget a dumb view that applies a value only on change. `HudLatencyWarnRttSec` (0.20 s, HUD-owned since 2026-10-25) is read through `GetClientTuning()`.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: ADR-0021 engine fact E1: `AddToPlayerScreen` widgets sit in the Player slot (100) below `AddToViewport` widgets, so L1 stays readable over the neutral-view fade. The own RTT is a local net-connection fact (smoothed lag of the owner's connection); a listen-server host has no such connection, so the sampler supplies no RTT and the icon cannot show, with no host branch in the HUD (story 046 greps for one). Rects are measured at 1080p where `H/1080` is 1.0; UI-V4 (story 008) is not exercised here. `UE_LOGF` is preferred over `UE_LOG` in 5.8.

**Control Manifest Rules (this layer)**:
- Required: Presentation reads only its own controller's carriers, the GameState public structs and client-local queries — source: ADR-0001, ADR-0021
- Forbidden: Never create or destroy a widget during a round in response to gameplay — source: ADR-0021
- Guardrail: HUD ≤ 0.3 ms CPU (AC-HUD-22), ≤ 0.5 ms with markers; UI GPU ≤ 0.5 ms at 1080p on the reference GPU — source: ADR-0021

This story delivers HUD-E19: the owner-only connection-quality warning, shape-coded and number-free, with the hysteresis that stops it flickering.

---

## Acceptance Criteria

*From UX spec `design/ux/hud.md` UXA-HUD-28 and the latency clause of GDD `design/gdd/hud.md` AC-HUD-25, scoped to this story:*

- [ ] **AC1** (UXA-HUD-28, scripted RTT in a manual check): **GIVEN** a scripted own RTT sequence **WHEN** the icon is observed, also as the listen host, **THEN** the icon is the three-step stair with a hollow top step, appears above `HudLatencyWarnRttSec` (0.20 s) own RTT, hides only below 0.16 s (0.8 x) after at least 2 s shown, and never appears on the listen-server host; it is driven by own RTT from the local net connection only.
- [ ] **AC2** (AC-HUD-25 latency clause, retained screenshot): **GIVEN** scripted own RTT and a feed line at 1080p **WHEN** the icon is captured **THEN** with scripted own RTT the icon shows above 0.20 s and hides only below 0.16 s after >= 2 s; it sits on the safe-rect edge above the feed's first line and never displaces a feed line; a retained screenshot shows the icon beside a feed line.

---

## Implementation Notes

*Derived from ADR-0021 Decision §4 and `design/ux/hud.md` E19, `design/gdd/hud.md` HUD-R13:*

**What to build.** Two parts, split along the ADR's logic/widget line:

1. **Visibility rule in the presenter core** (plain C++, for example `FHudLatencyHysteresis` in `Source/StealthPresentation/Private/Hud/`, owned by `FHudPresenterCore`): input is own RTT in seconds or "none" from `FHudInputs` (story 003 fills it from the local net connection's smoothed lag; it is absent for a listen host). State: `bShown`, `ShownSince`. Rule: show when RTT is above `HudLatencyWarnRttSec` (strictly above: 0.20 does not show, 0.201 does); once shown, hide only when RTT is below `0.8 x HudLatencyWarnRttSec` (0.16 s at default) and the icon has been shown for at least 2 s; between 0.16 and 0.20 the state holds. The 0.8 factor and the 2 s dwell are constants, not keys (HUD-R13). The qa-lead advises a Logic test of this rule; add it (below) so the manual check is not the only proof.
2. **Widget** `UStealthHudLatencyIconWidget` in `Source/StealthPresentation/Private/Hud/Widgets/` (Widget Blueprint carries layout and style only): a 40 x 40 Obsidian plate at (1772, 66) holding a three-step stair glyph (ascending stepped bars, the art-bible stair profile) in Bone, the top step drawn hollow (outline only) to mean "weak". Shape, not colour, carries the meaning. No number, ping value or text is ever shown (nothing about the connection of anyone else, Session rule 24). Non-urgent entrance 400 ms, exit 600 ms (shared plate timeline, story 016), static while shown. The widget applies the view-model value only on change.

**Placement.** The plate sits on the safe-rectangle edge above the feed's first line and never displaces a feed line; the feed footprint is (1392, 114) to (1812, 214) at 1080p and an informant's top-right bracket clears the plate by 10 px. Positions come from the layout resolver (story 007), not literals. The icon's slot is persistent (never reused by another element; collapsed when hidden).

**Host and RTT source.** Owner-only; the only input is the owner's own RTT from the local net connection. Nothing replicated is read and nothing about any other player's connection exists. The host never shows the icon because it has no connection to itself and so no RTT sample; do not test for the host in the HUD (a grep of the HUD module for `HasAuthority`, `GetNetMode`, `IsListenServer` and the like, story 046, would fail the build).

**Leak and language checks.** The icon is identical for every kind and team of non-ally subject (it depends on nothing but own RTT). No text, so the language lint (story 005) has nothing to check here.

**Does not touch.** The own-RTT sampling (story 003), the feed (story 035), the layer roots (story 006), the host-parity PIE (story 046), the settings hooks (story 052). Prototype: the prototype HUD has no latency icon; nothing is promoted from `Source/StealthGame/Private/StealthHUD.cpp`.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/hud/story-003-hud-input-sampling-and-host-parity-path.md`: sampling own RTT from the local net connection (and its spy test).
- `production/epics/hud/story-046-hud-host-parity-pie.md`: the PIE proof that the host never shows the icon and the HUD module has no host branch.
- `production/epics/hud/story-035-hud-team-feed-queue-f8-and-removal-lines.md`: the feed whose first line the icon sits above.
- `production/epics/hud/story-001-hud-tuning-keys-and-invariants.md`: registering `HudLatencyWarnRttSec`.
- `production/epics/hud/story-017-hud-shared-visual-system-font-face-and-plate-style.md`: the plate style and glyph conventions.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthPresentation/Private/Hud/HudLatencyHysteresis.h`
- `Source/StealthPresentation/Private/Hud/HudLatencyHysteresis.cpp`
- `Source/StealthPresentation/Private/Hud/Widgets/StealthHudLatencyIconWidget.h`
- `Source/StealthPresentation/Private/Hud/Widgets/StealthHudLatencyIconWidget.cpp`
- `Source/StealthPresentation/Private/Hud/HudLayerRoots.cpp` (register the icon on the L1 root, created once at HUD init)
- `Source/StealthPresentation/Private/Tests/HUD/LatencyHysteresisTest.cpp` (class `FHudLatencyHysteresisTest`, name `StealthGame.HUD.LatencyHysteresis`; advisable per the qa-lead, covers the 0.20 / 0.201 / 0.17 / 0.15 script)

The Widget Blueprint `WBP_HudLatencyIcon` (layout and style only) is authored in the editor and is not listed as a file here.

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: Manual check: icon shape and hysteresis
  - Setup: scripted own RTT: 0.20, 0.201, 0.17 after 1.5 s shown, 0.15 after 2.0 s shown. Also run as the listen host.
  - Verify: a three-step stair with a hollow top step. It appears above 0.20 s (0.20 does not show, 0.201 does). It hides only below 0.16 s after at least 2 s shown (still shown at 0.17, and at 0.15 before 2 s). Never shown on the host.
  - Pass condition: states match the script at each point. The RTT source is the local net connection only (story 003 spy covers it). A Logic test of the hysteresis is advisable.
- **AC-2**: Manual check: placement
  - Setup: show the icon with a feed line at 1080p.
  - Verify: plate at (1772,66) 40x40, on the safe-rect edge above the feed's first line, and no feed line displaced.
  - Pass condition: `hud-latency-icon-1080p.png` retained and positions within 2 px.

QL-STORY-READY: ADEQUATE

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: UI
**Required evidence**:
- UI: retained screenshot `production/qa/evidence/hud-latency-icon-1080p.png` (icon beside a feed line), with the scripted-RTT observations (0.20, 0.201, 0.17 after 1.5 s, 0.15 after 2.0 s, host run) recorded in the sign-off `production/qa/evidence/hud-latency-icon-evidence.md`. Gate level: BLOCKING (the UI default; not waived at `qa.level: minimal`).

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/hud/story-003-hud-input-sampling-and-host-parity-path.md`, `production/epics/hud/story-017-hud-shared-visual-system-font-face-and-plate-style.md`, `production/epics/hud/story-001-hud-tuning-keys-and-invariants.md`
- Unlocks: `production/epics/hud/story-046-hud-host-parity-pie.md`, `production/epics/hud/story-054-hud-accessibility-captures-and-input-pass.md`, `production/epics/hud/story-056-hud-layout-evidence-resolutions-and-budget.md`, `production/epics/hud/story-060-hud-run-and-observe-2v2-evidence.md`
