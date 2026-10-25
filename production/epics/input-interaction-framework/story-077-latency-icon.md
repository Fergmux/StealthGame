# Story 077: AC 68 LatencyIcon

> **Epic**: Input & Interaction Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: UI
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/input-interaction-framework.md`
**Requirement**: `TR-iif-012`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0021: UI/HUD layering, CommonUI and Enhanced Input
**Other ADRs**: ADR-0011: Pawn command interface and input pipeline
**ADR Decision Summary**: ADR-0021 fixes the HUD as hit-test-invisible UMG layers fed by plain-USTRUCT view-models that a headless presenter builds from the owner's own data (own RTT included). ADR-0011 section 11 states the input latency budget (F5) and that above HUD's `HudLatencyWarnRttSec` the owner sees the latency icon, with no gameplay compensation.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: No post-cutoff API is used by the icon itself. It lives in the L1 persistent layer (ADR-0021 engine fact E1, `AddToPlayerScreen` at the HUD-R4 ZOrder). Own RTT comes from the owner's own net connection's smoothed lag, never from the replicated `PlayerState` ping byte (`design/ux/hud.md`, E19).

**Control Manifest Rules (this layer)**:
- Forbidden: Never deliver replies as replicated owner-only properties; never put owner-only data on the pawn — source: ADR-0011
- Guardrail: Command interface: uplink at most 20 reliable RPCs/s per connection; at most one reply RPC per connection per drain — source: ADR-0011

---

## Acceptance Criteria

*From GDD `design/gdd/input-interaction-framework.md`, scoped to this story:*

- [ ] AC 68 (UI 7, UI, N `LatencyIcon`): **GIVEN** RTT 180 ms, then 220 ms **WHEN** the HUD is observed **THEN** the icon is hidden at 180 ms and shown at 220 ms. Screenshot: `production/qa/evidence/iif-latency-icon.png`.

---

## Implementation Notes

*Derived from ADR-0011 Decision section 11 and ADR-0021 Decision sections 1, 4 and 9:*

**What this story builds.** The owner-only latency warning icon of GDD UI Requirements item 7 (HUD element E19): a small icon at the top-right edge of the safe area, above the feed's first line, shown when the owner's own RTT exceeds `HudLatencyWarnRttSec`. The threshold key is owned by the HUD (default 0.20 s, hard 0.10-0.50); the IIF's duplicate `LatencyWarnRttSec` was retired by user decision 2026-10-25 (TDC CR-15.3), so this story reads the HUD key and must not add a second one. The IIF's contribution is F5's statement that a human command is confirmed in `L_confirm = RTT + dt_server + dt_client` and that no compensation is applied: the icon is information, not a mechanism.

**HUD behaviour the test must account for.** `design/ux/hud.md` E19 adds hysteresis: the icon is hidden again only below 0.8 x the threshold and after at least 2 s shown, it is never shown on the listen-server host (the host has no RTT) and it is driven by nothing but own RTT. The AC order (180 ms, then 220 ms) starts hidden, so the hysteresis does not change the expected result, but the test must run on a remote client, not on the host, and must wait for the RTT estimate to settle before each capture.

**Why this is a UI story in the IIF epic.** TR-iif-012 (input latency budget) includes the owner-visible warning; the numeric budgets are measured by stories 080 and 081. This story proves only that the warning appears and disappears around the threshold, from owner-known data.

**Known gaps and seams (qa-lead).** (1) "RTT 180 ms" has no named source: use the HUD's own RTT estimate (read the estimate, not the setting) and a settle time such as "after the estimate has held the value for 2 s". (2) The threshold boundary is undefined: tuning is 0.200 s; the test should assert hidden at 0.200 and shown above (the AC itself states 180 and 220). (3) One screenshot cannot show both hidden and shown: capture both and name the hidden-state companion file. (4) State the packet-loss setting beside the lag (the net-emulation profile default is 1 percent; the qa-lead recommends 0 percent for a stable estimate). (5) The seam to inject or read the HUD RTT estimate is not owned by any other story; this story adds it (a test-only RTT source on the presenter).

**Prototype.** New code, no prototype counterpart.

**Tuning keys (defaults).** `HudLatencyWarnRttSec` 0.20 (HUD-owned, read at the point of use). No IIF key.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/input-interaction-framework/story-080-latency-budget.md`: the numeric F5 budget (p95 owner confirm and observer OnRep) under net emulation.
- `production/epics/input-interaction-framework/story-081-look-latency.md`: the local look latency.
- `production/epics/input-interaction-framework/story-076-rejection-plate.md`: the rejection plate.
- `production/epics/input-interaction-framework/story-004-reply-sink-and-drain-end-flush.md`: the reply delivery path (the icon does not use it).

---

## Files

- `Source/StealthPresentation/Private/` — latency-icon view-model and L1 widget wiring (exact file names per the HUD epic; planned)
- `production/qa/evidence/iif-latency-icon.png` — retained capture, icon shown at 220 ms
- `production/qa/evidence/iif-latency-icon-hidden.png` — retained companion capture, icon hidden at 180 ms (name to be confirmed by the author)

---

## QA Test Cases

*Test cases not yet defined — run /qa-plan to generate them.*

*QL-STORY-READY: GAPS (qa-lead):*
- Gap 1: "RTT 180 ms" has no source. Name the estimator (the HUD's RTT estimate) and a settle time (for example "after the estimate has held the value for 2 s"), and state whether the 1 percent loss default applies.
- Gap 2: threshold boundary and hysteresis are undefined (`HudLatencyWarnRttSec` 0.20 s); add "hidden at 0.200, shown above".
- Gap 3: one screenshot cannot show both hidden and shown; require two captures.

*Draft check from the qa-lead (unconfirmed):* Manual check "latency icon threshold". Setup: N profile with fixed lag giving a HUD RTT estimate of 180 ms, then 220 ms (read the estimate, not the setting), 0 percent loss. Verify: wait for the estimate to settle; capture the HUD at each. Pass condition: icon absent at 180 ms, present at 220 ms; screenshots `production/qa/evidence/iif-latency-icon.png` (shown) and a retained hidden-state companion capture.

---

## Test Evidence

**Story Type**: UI
**Required evidence**:
- UI: a retained screenshot of each screen touched, in `production/qa/evidence/` — `production/qa/evidence/iif-latency-icon.png` (shown) and the hidden-state companion capture

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/input-interaction-framework/story-004-reply-sink-and-drain-end-flush.md`; cross-epic: hud (stories not yet written; own-RTT source, `HudLatencyWarnRttSec`, latency icon E19); cross-epic: tuning-data-config (stories not yet written; net-emulation profile in the test harness)
- Unlocks: None
