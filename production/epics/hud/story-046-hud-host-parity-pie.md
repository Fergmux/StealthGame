# Story 046: PIE proof of listen-host parity of the settled view-models and ordered event lists

> **Epic**: HUD
> **Status**: Ready
> **Layer**: Presentation
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/hud.md`
**Requirement**: `TR-hud-001`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0001: Crowd identity rules (the listen host reads the same carriers as a client), with ADR-0021: UI/HUD layering and Common UI (sampling, not events) and ADR-0008: Test automation, leak and parity (the PIE harness)
**ADR Decision Summary**: Presentation builds `FHudInputs` from its handles every frame by value, with no dependency on a RepNotify having fired, so the listen host (which gets no RepNotify for its own writes) and a remote client run the identical path; `FHudViewModel::Dump()` is deterministic precisely so host parity (AC-HUD-20) can be asserted by dump equality. ADR-0008 provides the multi-client PIE harness and the tick-indexed dump comparison.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: A listen-server host has no replication delay for its own writes and no RepNotify for them; the parity claim therefore rests on ADR-0021 §4 (sampling every frame). Tests are CQTest `NETWORK_TEST_CLASS` PIE tests with the 60 Hz fixed-step determinism rule; the host's `ServerNowEst` is the server clock itself, so "arrival delay at most 1 tick" is a real bound, not an estimate. HIGH networking domain (ADR-0018 note: MotionWarping is Beta in 5.8.4). The source grep in AC2 must cover `Source/StealthPresentation` only. `UE_LOGF` is preferred over `UE_LOG` in 5.8.

**Control Manifest Rules (this layer)**:
- Required: Presentation builds view-models by sampling its carriers every frame by value (the listen host runs the same path) — source: ADR-0021
- Forbidden: Never depend on a RepNotify having fired; never drive widgets event-style from RepNotify/RPC handlers — the host gets no RepNotify for its own writes — source: ADR-0021
- Guardrail: HUD ≤ 0.3 ms CPU (AC-HUD-22), ≤ 0.5 ms with markers; UI GPU ≤ 0.5 ms at 1080p on the reference GPU — source: ADR-0021

This story proves HUD-R1's parity rule end to end: the host and a same-team client, on the same camera, show the same settled view-models and the same ordered events, and the HUD module contains no branch that could tell them apart.

---

## Acceptance Criteria

*From GDD `design/gdd/hud.md` AC-HUD-20 and the derived HUD-R1 rule, scoped to this story:*

- [ ] **AC1** (AC-HUD-20, `StealthGame.HUD.PIE.HostParity`, verbatim): **GIVEN** the listen-server host and a client on the same team with the same camera transform, **WHEN** each serialises its HUD view-model for the persistent clusters, the feed and the banner after 1 s without carrier changes (settled), and as an ordered event list over a scripted round, **THEN** the settled view-models are identical and the ordered event lists are identical with timestamps excluded; the host never shows the latency icon (extends AC-PSR-42, Session AC 3).
- [ ] **AC2** (derived, HUD-R1, `StealthGame.HUD.PIE.HostParity` no host branch and host timing): **GIVEN** the HUD module sources and a scripted round on the host **WHEN** a grep runs and the host's plate, removal and banner times are measured **THEN** the HUD module has no listen-server or `IsLocallyControlled`-style branch: the host runs the same presenter path as a client (a grep finds none of `HasAuthority`, `IsLocallyControlled`, `GetNetMode`, `IsNetMode`, `ROLE_Authority`, `IsListenServer` in the HUD module) and the host's plate, removal and banner times equal F2/F4/F10 with an arrival delay of at most 1 tick.

---

## Implementation Notes

*Derived from ADR-0021 Decision §4, ADR-0008 and `design/gdd/hud.md` HUD-R1, AC-HUD-20:*

**What to build.** One CQTest PIE scenario file with `NETWORK_TEST_CLASS(HostParity, "StealthGame.HUD.PIE")`, full name `StealthGame.HUD.PIE.HostParity.<Method>`: one method for the settled and ordered-list comparison (AC1), one for the module grep and the host timing (AC2). The 2v2 fixture, the per-client `FHudViewModel::Dump()` capture, the scripted round and the tick-indexed comparison come from IVP story 001 and story 003 of this epic. No new HUD production code is expected; if parity fails, the fix belongs in story 003's sampling path or the state machine that diverges.

**Settled comparison.** Place the host and a same-team client on the same camera transform (the harness pins both cameras to one transform so the world-anchored cluster is comparable). After 1 s without carrier changes, serialise `FHudViewModel::Dump()` for the persistent clusters (brace, tally, objectives, brackets), the feed and the banner on both and assert byte equality. Wall-clock fields are excluded by the dump format (ADR-0021 §4).

**Ordered event list.** Over a scripted round (a strike, a removal, a feed line, a banner), record the cue events and element state changes per client in order and compare them with timestamps excluded. Equality is of order and content, not of time: the host has a near-zero arrival delay (accepted host parity, IVP R10).

**Latency icon.** The host never shows the latency icon: assert the `Latency` cluster is absent in every host dump while a scripted RTT on the client crosses 0.20 s and shows the icon there (story 047 owns the icon). The host's absence must come from the sampler supplying no own RTT for a missing local net connection (story 003), not from a host check in the HUD.

**Module grep.** Assert, as a source lint over `Source/StealthPresentation` (the HUD module), that none of these tokens appears: `HasAuthority`, `IsLocallyControlled`, `GetNetMode`, `IsNetMode`, `ROLE_Authority`, `IsListenServer`. Story 004 owns the include-boundary and forbidden-API lint; add these six tokens to its forbidden list if it does not already carry them, and assert them here through that same lint so the check is one mechanism, not two. Comments and string literals that mention a token must not trip the lint (it matches code tokens), but avoid them anyway.

**Host timing.** On the host, measure the plate show time, removal card time and banner time against the F2, F4 and F10 formulas for the scripted round and assert each equals the formula result within the arrival delay of at most 1 tick (one 60 Hz step). The oracle for "contact" is the strike anim-notify tick on that client, never the HUD's own F2 mapping.

**Does not touch.** The sampling path (story 003), the feed and banner logic (stories 035, 040) and the latency icon (story 047).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/hud/story-003-hud-input-sampling-and-host-parity-path.md`: the sampler and the host path whose parity this proves.
- `production/epics/hud/story-004-hud-include-boundary-and-forbidden-api-lint.md`: the lint mechanism (this story adds six tokens to it at most).
- `production/epics/hud/story-047-hud-latency-icon.md`: the latency icon and its host exclusion implementation.
- `production/epics/hud/story-035-hud-team-feed-queue-f8-and-removal-lines.md` and `story-040-hud-end-banner-f10-and-round-summary-logic.md`: the feed and banner under comparison.
- Roster and Session epics: AC-PSR-42 and Session AC 3, which AC-HUD-20 extends.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthTestHarness/Private/HUD/HostParityTest.cpp` (CQTest, `NETWORK_TEST_CLASS(HostParity, "StealthGame.HUD.PIE")`, `StealthGame.HUD.PIE.HostParity.*`)
- The forbidden-token list of the story 004 lint (add the six host-branch tokens if absent; the file is named by story 004)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: AC-HUD-20 `HostParity`
  - Given: the listen-server host and a client on the same team with the same camera transform.
  - When: each serialises its view-model for the persistent clusters, feed and banner after 1 s without carrier changes, and as an ordered event list over a scripted round.
  - Then: the settled view-models are identical and the ordered event lists are identical with timestamps excluded. The host never shows the latency icon.
  - Edge cases: none.
- **AC-2**: No host branch and host timing
  - Given: the HUD module sources and a scripted round on the host.
  - When: a grep runs and plate, removal and banner times are measured.
  - Then: the grep finds none of `HasAuthority`, `IsLocallyControlled`, `GetNetMode`, `IsNetMode`, `ROLE_Authority`, `IsListenServer` in the HUD module. The host's plate, removal and banner times equal F2/F4/F10 with an arrival delay of at most 1 tick.
  - Edge cases: none.

QL-STORY-READY: ADEQUATE (qa-lead, 2026-10-25; criteria revised after the first pass and re-judged)

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/HUD/HostParityTest.cpp`, test `StealthGame.HUD.PIE.HostParity` OR a documented playtest. Gate level: BLOCKING (the Integration default).

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/hud/story-003-hud-input-sampling-and-host-parity-path.md`, `production/epics/hud/story-035-hud-team-feed-queue-f8-and-removal-lines.md`, `production/epics/hud/story-040-hud-end-banner-f10-and-round-summary-logic.md`, `production/epics/hud/story-047-hud-latency-icon.md`
- Unlocks: `production/epics/hud/story-061-hud-mvp-exit-5v5-presentation-verification.md`
