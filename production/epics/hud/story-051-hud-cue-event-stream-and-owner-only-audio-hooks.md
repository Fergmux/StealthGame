# Story 051: HUD cue events with timestamps, owner-only 2D UI-bus, never spatialised

> **Epic**: HUD
> **Status**: Ready
> **Layer**: Presentation
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/hud.md`
**Requirement**: `TR-hud-002`, `TR-hud-004`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0021: UI/HUD layering, CommonUI and Enhanced Input (with ADR-0008 for the leak rules the cues must obey)
**ADR Decision Summary**: `FHudPresenterCore` owns every presentation state machine and turns an `FHudInputs` snapshot plus `ServerNowEst` and `LocalNow` into an `FHudViewModel` whose deterministic `Dump()` lists the cue events fired each frame; timed elements are sampled from `now − t0` in C++, never from widget animation assets. The HUD owns when a cue fires, the audio-director owns the sound, and no cue may carry information that a plate or card has not yet shown.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Engine surface: none for the cue stream itself (plain C++ core, no UWorld/UObject); the module still builds against 5.8.4 / C++20. The cue event list is data in the view-model; the owner-only 2D playback hook (a UI-bus submix send, `PlaySound2D` equivalent) sits in the thin `UStealthHudPresenter` adapter and accepts a null sound asset. ADR-0021 engine facts do not constrain this story beyond E3 (timing sampled in the frame that is rendered). `UE_LOGF` is preferred over `UE_LOG` in 5.8; the null-sound path must log nothing at warning level.

**Control Manifest Rules (this layer)**:
- Required: Presentation timing uses only `EstimatedServerNow()`; plate and card timelines are sampled from `now − t0` in C++ — source: ADR-0021, ADR-0002
- Forbidden: Never sample plate or card timelines from widget animation assets — source: ADR-0021
- Guardrail: HUD: ≤ 0.3 ms CPU (AC-HUD-22), ≤ 0.5 ms with markers; UI GPU ≤ 0.5 ms at 1080p on the reference GPU (inside ADR-0022's frame budget) — source: ADR-0021

This story implements the cue half of the GDD "Event Feedback & Cues" row (VA-HUD-6): the presenter emits cue events with timestamps into the view-model dump so the leak suite can prove that no cue ever differs by subject kind or fires before its beat, and an owner-only 2D hook plays them. No audio asset is required in MVP.

---

## Acceptance Criteria

*From GDD `design/gdd/hud.md` (Game Feel / Visual-Audio VA-HUD-6, "Event Feedback & Cues" row) and UX spec cue table (`design/ux/hud.md` lines 725-745), scoped to this story:*

- [ ] **AC1** (derived, HUD GDD "Event Feedback & Cues" row and VA-HUD-6, `StealthGame.HUD.CueEvents`): **GIVEN** the cue triggers **WHEN** the presenter runs **THEN** it emits cue events with timestamps into the view-model dump: tally tick, threshold tick, result cue (a distinct cue id per outcome and one for the defence plate), removal cue, stage tone, rival tick, ready-again tone, ping tick, `PingConcealedRefused`; each fires once at its beat's `t_show` and never before it.
- [ ] **AC2** (derived, VA-HUD-6 and Roster R10, `StealthGame.HUD.CueEvents`): **GIVEN** the cue records **WHEN** inspected **THEN** every cue is owner-only or team-only, 2D on the UI bus, never spatialised and never attached to a pawn; every cue has a text twin in the dump; cue ids for an NPC victim and an informant victim are identical (Roster R10) and no cue differs by subject kind before `t_show`.
- [ ] **AC3** (derived, "Event Feedback & Cues is Vertical Slice", `StealthGame.HUD.CueEvents`): **GIVEN** no audio asset exists in MVP **WHEN** a cue fires with a null sound **THEN** the hook accepts it without error or warning.

---

## Implementation Notes

*Derived from ADR-0021 Decision §4 and GDD VA-HUD-6:*

**Where it lives.** A small `FHudCueEvents` helper beside `FHudPresenterCore` (plain C++, `Source/StealthPresentation/Private/Hud/`), called by the cluster step functions already built by stories 016, 022, 024 (and the cluster stories behind them). The view-model gets a per-frame list of `FHudCueEvent { FName CueId; double Timestamp; EHudCueAudience Audience; FText TextTwin; }` that `Dump()` prints in stable order with fixed float precision. The 2D playback hook is an interface (`IHudCuePlayer::Play(FName CueId, USoundBase* Sound)`) implemented in `UStealthHudPresenter` against the UI-bus 2D path; a null `Sound` returns quietly (no `ensure`, no log above Verbose).

**The oracle table (UX spec cue table, `design/ux/hud.md` lines 725-745).** Embed it as the test fixture so the text twins and audiences have an oracle:

| Cue | Fired at | Audience | Text twin |
|---|---|---|---|
| Result cue, one id per outcome (Target eliminated, Wrong target, No contact, Reversed, Strike reversed) | the plate's show time `t_show` (after contact, so legal); identical for `WrongTarget` whatever the victim kind | Self | the plate |
| Tally tick; threshold tick when `n >= L - 1` | with the plate; chosen by `n` alone | Self | squares, numeral, triangle |
| Removal cue | with the card entrance (`OnRemovalCardEntered`), never with the plate | Self | the card |
| Stage tone | an own counted completion | Team | stage card turn-over and own step fill |
| Rival tick | a rival-steps rise, once per update | Team | rival step fill |
| Brace ready-again (soft) | when the numeral resolves to Ready | Self only | Ready ring |
| Ping tick | when a ring appears (the sender hears it on the echo) | Team | the ring |
| `PingConcealedRefused` | with the "Not now" plate, limited by `RejectPlateCooldownSec` | Self only | the "Not now" plate |
| Being targeted | never | n/a | none by design (ASN-R6) |

The rows for brace protected / protection ended and the rejection tone live in the same table; where their clusters exist (stories 018, 019, 032) the same audience, 2D and twin rules are asserted for them by the same test, but their timing is owned by those stories. Gamepad haptic twins are Vertical Slice and not built.

**Rules that matter.** "Never before `t_show`": the result cue and the tally tick fire with the plate at the local contact frame (F2 `t_show`), never at the server write; the removal cue fires with the card (`OnRemovalCardEntered`), never with the plate; the exile plate and card are separate beats. The distinct cue id per outcome is legal because it follows contact (CR-HUD-15); `WrongTarget` carries one id whatever the victim kind, so an NPC victim and an informant victim produce identical cue ids, identical timestamps (within the test's tolerance of zero ticks) and identical text twins (Roster R10, AC-HUD-03 family). Cue audience is a closed enum {Self, Team}; there is no cue to opponents and no cue on being targeted, ever. Cues never attach to a pawn and never spatialise: the hook has no world-location parameter at all, so it cannot be spatialised by construction.

**Traps.** Do not fire a cue from a RepNotify or RPC handler (sampling, not events; the host gets no RepNotify). A cue that fires on a rebuild is a replay bug: HUD-R17 says no cue after a rebuild (story 044 owns that proof; the cue stream must expose the cue list so it can be checked). Timestamps are `ServerNowEst`-based view-model time, excluded from cross-client equality only where the GDD says wall-clock fields are excluded.

**Decisions that apply.** D11 (removal wording: the removal cue's twin is the "Informant" card), D26 (`PingConcealedRefused` owner-only, limited by `RejectPlateCooldownSec`). Out of this story: sound assets, mixing, the ping arrival tick's sound (Informant Pings VA-PNG-5 owns it), haptics.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/hud/story-002-hud-view-model-and-presenter-core-shell.md`: the dump format this story extends.
- `production/epics/hud/story-016-hud-plate-timeline-f3-and-reduce-motion.md`, `story-022-hud-result-and-defence-plate-flow.md`, `story-024-hud-removal-card-timing-f4-logic.md`: the beats whose `t_show` the cues follow.
- `production/epics/hud/story-031-hud-notice-slot-priority-and-rate-limits.md`: the `NotNow` limiter the refusal cue shares.
- `production/epics/hud/story-050-hud-ping-refusal-owner-only-pie.md`: the multi-client proof that the refusal cue is owner-only.
- `production/epics/hud/story-044-hud-reset-and-rebuild-logic.md`: no cue after a rebuild.
- Audio assets and the audio-director's sound palette: Vertical Slice (Event Feedback & Cues).

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthPresentation/Private/Hud/HudCueEvents.h` and `HudCueEvents.cpp` (`FHudCueEvent`, `EHudCueAudience`, cue id table)
- `Source/StealthPresentation/Private/Hud/HudViewModel.cpp` (cue list in `Dump()`)
- `Source/StealthPresentation/Private/Hud/StealthHudPresenter.cpp` (owner-only 2D UI-bus hook accepting a null sound)
- `Source/StealthPresentation/Private/Tests/HUD/CueEventsTest.cpp` (class `FHudCueEventsTest`, name `StealthGame.HUD.CueEvents`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: cue timing
  - Given: each cue's triggering beat (tally tick, threshold tick, result cue per outcome plus defence, removal, stage tone, rival tick, ready-again, ping tick, `PingConcealedRefused`).
  - When: the dump is read.
  - Then: each fires once at its beat's `t_show` and never before, with a timestamp.
  - Edge cases: none.
- **AC-2**: audience and text twins
  - Given: the cue records.
  - When: inspected.
  - Then: each is owner-only or team-only, 2D on the UI bus, not spatialised, not attached to a pawn. Each has a text twin. Cue ids for an NPC victim and an informant victim are identical, and no cue differs by subject kind before `t_show`.
  - Edge cases: embed the UX cue table (`design/ux/hud.md` lines 725-745) so the twin text has an oracle.
- **AC-3**: null sound
  - Given: a cue with a null sound asset.
  - When: it fires.
  - Then: no error or log warning.
  - Edge cases: none.

QL-STORY-READY: ADEQUATE

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthPresentation/Private/Tests/HUD/CueEventsTest.cpp` (`StealthGame.HUD.CueEvents`) — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)
- Gate: BLOCKING

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/hud/story-002-hud-view-model-and-presenter-core-shell.md`, `production/epics/hud/story-016-hud-plate-timeline-f3-and-reduce-motion.md`, `production/epics/hud/story-022-hud-result-and-defence-plate-flow.md`, `production/epics/hud/story-024-hud-removal-card-timing-f4-logic.md`
- Unlocks: None within this epic (see the EPIC Definition of Done)
