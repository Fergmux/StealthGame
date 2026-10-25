# Story 001: HUD tuning keys with ranges, client audience and cross-field invariants

> **Epic**: HUD
> **Status**: Ready
> **Layer**: Presentation
> **Type**: Config/Data
> **Estimate**: S
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/hud.md`
**Requirement**: `TR-hud-004`, `TR-hud-007`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0021: UI/HUD layering, CommonUI and Enhanced Input (config pinned by the ADR-0004 tuning pipeline)
**ADR Decision Summary**: Every HUD value is client-local presentation read through the client tuning face (`GetClientTuning()`); plate and card timelines are sampled from `now - t0` and every read hold is multiplied by `HudReadHoldMult`. The keys are registered under owner `HUD`, audience `Client`, latch `Use`, with one scalar per key and no team, role, outcome or victim-kind variant, so a timing key can never become a leak axis (GDD Tuning Knobs, CR-15.8 identity-tell review).
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Engine surface: none beyond the tuning types of `StealthFoundation` (plain C++ structs, `UPROPERTY` metadata, validator rows); the module still builds against 5.8.4 / C++20. The keys carry no engine API. They feed ADR-0021 timelines that are sampled in C++ (never UMG animation assets) and `UE_LOGF` is preferred over `UE_LOG` for any validator diagnostics in 5.8. ADR-0021 verification items UI-V1..UI-V4 do not apply to this story.

**Control Manifest Rules (this layer)**:
- Required: Presentation reads only its own controller's carriers, the GameState public structs and client-local queries — source: ADR-0001, ADR-0021
- Forbidden: Never iterate actors, read a pawn's `PlayerState`/`Controller`/`Owner`, call `IsPlayerControlled`/`IsBotControlled`/`CanViewerKnow`, include a `Public/Server/` header, or write anything replicated from Presentation — source: ADR-0021, ADR-0001
- Guardrail: HUD ≤ 0.3 ms CPU (AC-HUD-22), ≤ 0.5 ms with markers; UI GPU ≤ 0.5 ms at 1080p on the reference GPU — source: ADR-0021

This story registers the data every later HUD story reads. No HUD code consumes a key here; each cluster story (014-060) reads its own keys through the client face.

---

## Acceptance Criteria

*From GDD `design/gdd/hud.md` Tuning Knobs (21 keys, invariants I-HUD-1..I-HUD-7) and TDC CR-14 / CR-15, scoped to this story:*

- [ ] **AC1** (derived, GDD Tuning Knobs + TDC CR-15, smoke check `production/qa/smoke-*.md` and `StealthGame.Tuning.HudKeys`): **GIVEN** the tuning data with the 21 HUD keys (`HudSafeMarginPct`, `HudPersistentAreaLimitPct`, `HudResultPlateInSec`, `HudResultPlateHoldSec`, `HudResultPlateOutSec`, `HudResultPlateMaxHoldBackSec`, `HudReplayWindowSec`, `HudPreemptExitSec`, `HudRemovalCardInSec`, `HudRemovalCardHoldSec`, `HudRemovalCardOutSec`, `HudRemovalCardDelaySec`, `HudOwnRemovalHoldCapSec`, `HudFeedLineCount`, `HudFeedLineLifeSec`, `HudFeedMinDwellSec`, `HudObjectiveStepSec`, `HudBannerMinPlateSec`, `HudBannerMaxDelaySec`, `HudReadHoldMult`, `HudLatencyWarnRttSec`) **WHEN** each key is read through `GetClientTuning()` and through a server-audience read **THEN** each key has the GDD default, hard range and safe range, owner `HUD`, audience `Client` and latch `Use`; the server-audience read of each of the 21 keys returns not-found; and no file under a `Server/` path references them. The story embeds the 21-row default / hard / safe table (GDD Tuning Knobs) as expected data.
- [ ] **AC2** (derived, GDD Tuning Knobs I-HUD-1..I-HUD-7, `StealthGame.Tuning.HudInvariants`): **GIVEN** a preset violating each of I-HUD-1..I-HUD-7 (for example `HudResultPlateMaxHoldBackSec` < `StrikeContactSec + BushStrikeEmergeSec + 1/60`; `HudOwnRemovalHoldCapSec` below the same bound; `HudRemovalCardDelaySec` != `ExileBeatSec` (float compare, |delta| <= 1e-6 counts as equal); `HudFeedMinDwellSec` > `HudFeedLineLifeSec`; `ExileBeatSec` < `HudResultPlateInSec + 0.5`; `HudRemovalCardDelaySec + HudRemovalCardInSec` > `DeathFallSec + NoPawnFadeSec`; `HudBannerMinPlateSec` > `HudBannerMaxDelaySec`) **WHEN** the validator runs **THEN** the preset is rejected naming the invariant; `HudReplayWindowSec` < 0.5 only warns; the defaults pass; and equality at each `>=` bound passes.
- [ ] **AC3** (derived, TDC CR-14, `StealthGame.Tuning.HudKeys`): **GIVEN** the key registry **WHEN** every `Hud*` key is enumerated **THEN** each resolves to exactly one scalar (no team, role, outcome or victim-kind variant field), and the registry has no key for UI scale, text scale, Reduce motion, Reduce screen effects, high contrast or streamer alias (client-local settings, TDC CR-14).

---

## Implementation Notes

*Derived from ADR-0021 §4 (time and read holds) and the ADR-0004 tuning pipeline (client face, owner invariants):*

Add the 21 keys to the client face (`StealthClientTuning.h`, created by `production/epics/tuning-data-config/story-040-canonical-keys-and-faces.md`) as one `FHudTuning` member group, with `UPROPERTY` metadata (`ClampMin`/`ClampMax` for the hard range, `SafeMin`/`SafeMax` for the safe range, `Latch=Use`, owner `HUD`) in the same style story 040 uses for the existing owners. Add the matching schema rows and Default-preset values (the preset assets themselves are TDC story 056's; this story only supplies the values). Every key is audience `Client`: none may be added to `StealthServerTuning.h` or any `Public/Server/` header, and the server-audience read of each key must fail (TDC `story-066-tuning-service-and-parity.md` gives the read paths: `GetClientTuning()` and the server face).

Expected data (embed in the test as the oracle; GDD Tuning Knobs, default / hard / safe):

| Key | Type | Default | Hard | Safe |
|---|---|---|---|---|
| `HudSafeMarginPct` | float | 5 | 3-10 | 3-6 |
| `HudPersistentAreaLimitPct` | float | 8 | 7-12 | 8-9 |
| `HudResultPlateInSec` | float | 0.25 | 0.1-0.5 | 0.2-0.3 |
| `HudResultPlateHoldSec` | float | 3.0 | 1.0-6.0 | 2.0-4.0 |
| `HudResultPlateOutSec` | float | 0.6 | 0.2-1.0 | 0.4-0.8 |
| `HudResultPlateMaxHoldBackSec` | float | 1.25 | 1.25-2.0 | 1.25-1.5 |
| `HudReplayWindowSec` | float | 1.0 | 0.5-3.0 | 0.75-1.5 |
| `HudPreemptExitSec` | float | 0.25 | 0.1-0.4 | 0.2-0.3 |
| `HudRemovalCardInSec` | float | 0.6 | 0.3-1.0 | 0.5-0.8 |
| `HudRemovalCardHoldSec` | float | 4.0 | 2.0-6.0 | 3.0-5.0 |
| `HudRemovalCardOutSec` | float | 0.8 | 0.4-1.2 | 0.6-1.0 |
| `HudRemovalCardDelaySec` | float | 0.8 | 0.5-1.2 | 0.7-1.0 |
| `HudOwnRemovalHoldCapSec` | float | 1.25 | 1.25-2.0 | 1.25-1.5 |
| `HudFeedLineCount` | int32 | 3 | 1-4 | 3-3 |
| `HudFeedLineLifeSec` | float | 6.0 | 3.0-12.0 | 5.0-8.0 |
| `HudFeedMinDwellSec` | float | 2.0 | 0.5-4.0 | 1.5-3.0 |
| `HudObjectiveStepSec` | float | 0.5 | 0.25-1.0 | 0.4-0.6 |
| `HudBannerMinPlateSec` | float | 1.0 | 0-1.5 | 0.5-1.0 |
| `HudBannerMaxDelaySec` | float | 1.6 | 1.0-2.5 | 1.4-1.8 |
| `HudReadHoldMult` | float | 1.0 | 1.0-2.0 | 1.0-1.5 |
| `HudLatencyWarnRttSec` | float | 0.20 | 0.10-0.50 | 0.15-0.30 |

Invariants go in the owner-invariant table of `StealthTuningInvariants.cpp` (mechanism: `production/epics/tuning-data-config/story-055-owner-invariant-mechanism.md`, base table `story-048-invariants-i1-i5.md`), each row a Pass / Warn / Reject with the invariant id in the message:

- I-HUD-1 (reject) `HudResultPlateMaxHoldBackSec >= StrikeContactSec + BushStrikeEmergeSec + 1/60`.
- I-HUD-2 (reject) `HudOwnRemovalHoldCapSec >= StrikeContactSec + BushStrikeEmergeSec + 1/60`.
- I-HUD-3 (reject) `HudRemovalCardDelaySec = ExileBeatSec`, float compare with |delta| <= 1e-6 treated as equal.
- I-HUD-4 (reject) `HudFeedMinDwellSec <= HudFeedLineLifeSec`; (warning) `HudReplayWindowSec >= 0.5`.
- I-HUD-5 (reject) `ExileBeatSec >= HudResultPlateInSec + 0.5`.
- I-HUD-6 (reject) `HudRemovalCardDelaySec + HudRemovalCardInSec <= DeathFallSec + NoPawnFadeSec` (defaults 1.4 <= 1.7).
- I-HUD-7 (reject) `HudBannerMinPlateSec <= HudBannerMaxDelaySec`.

Traps: (1) the invariants are evaluated on clamped values (GDD), and the I-HUD-4 warning threshold (0.5) equals the hard minimum of `HudReplayWindowSec`, so the warning is reachable only if the validator checks the raw value before clamping; confirm against the F-1 clamp order of TDC story 046 and, if the warning is unreachable, pin the row's existence in the test and record that finding in the story rather than loosening the range. (2) I-HUD-1, -2, -3, -5 and -6 read keys owned by other epics (`StrikeContactSec`, `BushStrikeEmergeSec`, `ExileBeatSec`, `DeathFallSec`, `NoPawnFadeSec`); if an owner key is not registered when this story starts, supply it as a fixture value from its GDD default and log the gap against that owner epic, never register the foreign key here. (3) `HudRemovalCardDelaySec` is pinned equal to `ExileBeatSec` (one beat for every removal, cross-review C-W3), so a preset that changes one must change the other. (4) `HudFeedLineCount` safe range is the single value 3 (art bible 7.3 row 12).

Settings that are not keys (TDC CR-14): UI scale, text scale, Reduce motion, Reduce screen effects, high-contrast mode and the streamer alias mask are client-local settings, fixed at 100 % / 100 % / off in MVP (HUD-R16); the registry must not grow keys for them. Constants (stepped entrance `k = 4`, text floors 18 / 22 / 28 px, plate stroke 2 px, brace indicator 56 px, interact ring 48 px, Result slot at 70 % height) stay in code, not in data.

Prototype disposition: the prototype HUD (`Source/StealthGame/Private/StealthHUD.cpp`) has no tuning; nothing is replaced here.

Decisions that apply: D11 (removal wording), D16 (the 8 % cap and the 16:10 binding case behind `HudPersistentAreaLimitPct`). Out of this story: consuming any key (each cluster story reads its own).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/hud/story-007-hud-layout-resolver-and-persistent-area-budget.md`: reads `HudSafeMarginPct` and `HudPersistentAreaLimitPct` and runs the A_persist lint.
- `production/epics/hud/story-015-hud-result-show-time-f2.md` and `story-016-hud-plate-timeline-f3-and-reduce-motion.md`: read the result-plate keys.
- `production/epics/hud/story-024-hud-removal-card-timing-f4-logic.md`: reads the removal-card keys.
- `production/epics/hud/story-035-hud-team-feed-queue-f8-and-removal-lines.md`: reads the feed keys.
- `production/epics/hud/story-040-hud-end-banner-f10-and-round-summary-logic.md`: reads the banner keys.
- Registering the foreign owner keys named in I-HUD-1..I-HUD-6: their owner epics.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthFoundation/Public/View/StealthClientTuning.h` (edit: the `FHudTuning` group of 21 keys)
- `Source/StealthFoundation/Private/Tuning/StealthTuningInvariants.cpp` (edit: owner rows I-HUD-1..I-HUD-7)
- `Source/StealthFoundation/Private/Tests/Tuning/HudTuningKeysTest.cpp` (class `FTuningHudKeysTest`, name `StealthGame.Tuning.HudKeys`; the embedded 21-row oracle, server-audience not-found, no-variant and no-settings-key checks)
- `Source/StealthFoundation/Private/Tests/Tuning/HudTuningInvariantsTest.cpp` (class `FTuningHudInvariantsTest`, name `StealthGame.Tuning.HudInvariants`)
- Default preset values: the `DA_TuningDefault` preset asset is a Content asset owned by TDC story 056 (named in prose only; this story supplies the 21 values to it)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: 21 HUD keys registered with defaults, ranges and client audience
  - Given: tuning data with the 21 HUD keys. The story embeds the GDD Tuning Knobs 21-row default / hard / safe table as expected data.
  - When: each key is read through `GetClientTuning()` and through a server-audience read.
  - Then: each key matches default, hard range, safe range, owner `HUD`, audience `Client`, latch `Use`. The server-audience read returns not-found, and no file under a `Server/` path references the keys.
  - Edge cases: a missing or extra key name fails. Config/Data gate is ADVISORY (smoke check).
- **AC-2**: I-HUD-1..7 validator
  - Given: one violating preset per invariant (including `HudOwnRemovalHoldCapSec` < `StrikeContactSec + BushStrikeEmergeSec + 1/60`), plus the defaults and boundary presets.
  - When: the validator runs on each.
  - Then: each violating preset is rejected naming its invariant. `HudReplayWindowSec` < 0.5 only warns. Defaults pass, and equality at each `>=` bound passes.
  - Edge cases: the I-HUD-3 compare treats |delta| <= 1e-6 as equal.
- **AC-3**: no variant dimensions and no client-local settings keys
  - Given: the key registry.
  - When: all `Hud*` keys are enumerated.
  - Then: each resolves to exactly one scalar (no team, role, outcome or victim-kind variant field). There is no key for UI scale, text scale, Reduce motion, Reduce screen effects, high contrast or streamer alias.
  - Edge cases: none.

QL-STORY-READY: ADEQUATE (qa-lead, 2026-10-25; criteria revised after the first pass and re-judged)

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Config/Data
**Required evidence**:
- Config/Data: smoke check pass (`production/qa/smoke-*.md`); the Logic validator cases `Source/StealthFoundation/Private/Tests/Tuning/HudTuningKeysTest.cpp` and `HudTuningInvariantsTest.cpp` also run in the automation suite.
- Gate: ADVISORY (the Config/Data default and the qa-lead spec note); it becomes BLOCKING only if `testing.strict.config` is set in `project.yaml`.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-040-canonical-keys-and-faces.md`, `production/epics/tuning-data-config/story-066-tuning-service-and-parity.md` (the client face read path), and the TDC validator story that registers owner keys (TD-REG-1): `production/epics/tuning-data-config/story-037-close-td-reg-1.md`, with the owner-invariant mechanism `production/epics/tuning-data-config/story-055-owner-invariant-mechanism.md`
- Unlocks: `production/epics/hud/story-047-hud-latency-icon.md`
