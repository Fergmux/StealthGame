# Story 015: F2 result show time, matching fallback and duplicate/late rules

> **Epic**: HUD
> **Status**: Ready
> **Layer**: Presentation
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/hud.md`
**Requirement**: `TR-hud-004`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0018: Encounter data and outcome presentation timing (§5.3 owner results, the positional matcher; §5.4 tally and exile), with ADR-0021 (§4 presenter core)
**ADR Decision Summary**: A result is shown at `t_show = min(max(t_c,local, t_arrival), t_arrival + HudResultPlateMaxHoldBackSec)`, where `t_c` is the owner's strike contact. Since the 2026-10-25 user decision (OQ15) the attacker's `t_c` comes from `FStrikeResult.ContactServerTime`; the positional matcher on the owner's own `FActionState` write remains for the defender's `FDefenceResult` and as the attacker-side fallback. Late processing never drops an unshown, non-duplicate result in live play; duplicates are dropped by `(PossessionEpoch, Seq)` order and a last-8 ring.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Engine surface: none (plain C++ core, no UWorld/UObject); the module still builds against 5.8.4 / C++20. ADR-0018's HIGH networking domain applies to the data this core consumes, not to the core: UE does not order an RPC against property replication (E6), which is exactly why F2 has a matching fallback and a Pending state; nothing here may rely on arrival order. MotionWarping is Beta in 5.8.4 (it drives the strike clip, not this timing). The offset between server and local time is the one slewed offset of ADR-0002 (`EstimatedServerNow()`), supplied to the core as a number so tests can fake it. Use `UE_LOGF` for diagnostics.

**Control Manifest Rules (this layer)**:
- Required: Presentation timing uses only `EstimatedServerNow()`; plate and card timelines are sampled from `now − t0` in C++ — source: ADR-0021, ADR-0002
- Forbidden: Never depend on a RepNotify having fired; never drive widgets event-style from RepNotify/RPC handlers — the host gets no RepNotify for its own writes — source: ADR-0021
- Guardrail: HUD ≤ 0.3 ms CPU (AC-HUD-22), ≤ 0.5 ms with markers; UI GPU ≤ 0.5 ms at 1080p on the reference GPU — source: ADR-0021

A strike's outcome is decided in one server queue slot but must appear on the owner's screen at the local contact frame, for every outcome and every victim kind, so that no plate timing encodes who was struck (HUD-R3). This story builds the pure arithmetic and bookkeeping of that rule: F2's show time, the Pending state and positional matching for results without a time, the duplicate and ordering drops, the last-8 ring, and the difference between live play (a late result still shows) and a rebuild (the replay window applies). It produces a show time per result and nothing else: the plate widgets, text and cues are story 022.

---

## Acceptance Criteria

*From GDD `design/gdd/hud.md` AC-HUD-08 (`StealthGame.HUD.ResultShowTime`), split by case group, scoped to this story:*

- [ ] **AC1** (AC-HUD-08, `StealthGame.HUD.ResultShowTime`, primary path): **GIVEN** {arrival, contact} pairs {1.000, 1.350} → 1.350; {1.120, 1.050} → 1.120; {1.000, 1.000} → 1.000; {1.000, 2.250} → 2.250; {1.000, 3.000} → 2.250 (cap); a result carrying `ContactServerTime` 5.400 (offset 4.050) → 1.350 with no `FActionState` observed **THEN** the F2 show time equals the stated value in every case (default `HudResultPlateMaxHoldBackSec` 1.25).
- [ ] **AC2** (AC-HUD-08, `StealthGame.HUD.ResultShowTime`, fallback and drops): **GIVEN** a fallback (result without a time, e.g. `FDefenceResult`, arrival 1.000): a matching `FActionState` arriving 0.1 s after the result (`ContactServerTime` 5.400, offset 4.050) → shown at its contact 1.350; never arriving → 2.250; a stale `FActionState` of the previous strike present at arrival → ignored (stays `Pending`); a duplicate seq, a lower seq and a seq from an earlier `PossessionEpoch` → dropped; a 9th distinct seq evicts the oldest ring entry (after 9 distinct seqs the ring, read through a test accessor, holds exactly seqs 2-9).
- [ ] **AC3** (AC-HUD-08 plus the logged HUD-R17 / F2 resolution, `StealthGame.HUD.ResultShowTime`, live versus rebuild): **GIVEN** a result first processed 1.5 s after its show time in live play **THEN** it is shown on processing; **GIVEN** a result pending across a same-controller rebuild with `t_show` = T and `HudReplayWindowSec` 1.0 **THEN** it is shown if the rebuild completes at T + 1.000 and not at T + 1.001. Resolution of the HUD-R17 / F2 tension (decision logged): a rebuild discards every showing element, but an unshown pending result stays subject to the replay window on a same-controller rebuild; on reconnect, travel and `RoundEpoch` change the controller or seq space changes and pending results are dropped.

---

## Implementation Notes

*Derived from ADR-0018 §5.3 and §5.4, ADR-0021 §4 and GDD HUD-R6, HUD-R17, F2:*

**What to build.** A plain C++ class `FHudResultTimer` in `Source/StealthPresentation/Private/Hud/`, owned by `FHudPresenterCore` and stepped from `Step`. Inputs, all by value inside `FHudInputs` (story 003 samples them): the owner's reply-sink entries `FStrikeResult` and `FDefenceResult` consumed in order (ADR-0011 `ClientCommandReplies`), the own pawn's `FActionState {Pose, Seq, ContactServerTime}` for the pawn handle captured when the command was sent, the slewed offset (`offset = ServerNowEst − LocalNow`), and the tuning snapshot values `HudResultPlateMaxHoldBackSec` (1.25, hard minimum 1.25, invariant I-HUD-1 owned by story 001) and `HudReplayWindowSec` (1.0). Output: per result a state `Pending` / `Scheduled(t_show)` / `Shown` / `Dropped` plus its `t_show`, and the ring and `seq_lastShown` exposed through a test accessor under `WITH_DEV_AUTOMATION_TESTS`.

**Formula (F2).** `t_c,local = ContactServerTime − offset`; `t_show = min( max(t_c,local, t_arrival), t_arrival + HudResultPlateMaxHoldBackSec )`. `ContactServerTime` is read from the result itself when present (`FStrikeResult.ContactServerTime`, Owner-only, ASN-R19; primary since 2026-10-25) and only otherwise from the matching `FActionState`. Do the arithmetic in `double`; the test pairs are exact and the 1.000 / 1.001 rebuild boundary must be reproducible, so compare with `<=` on the same doubles the test builds, not with a tolerance.

**Matching fallback (defender and attacker-side fallback).** On the pawn handle captured when the command was sent (not the currently possessed pawn: an exile releases it to the crowd at the slot), a strike pose (defender: `Counter`) whose `Seq` differs from the last consumed write and whose `ContactServerTime` is later than the last consumed contact time. That write is consumed and supplies `t_c`. Until it is observed the result stays `Pending` and `t_c,local` is undefined; if it has not been observed by `t_arrival + HudResultPlateMaxHoldBackSec`, `t_show` is that cap. A stale `FActionState` of the previous strike, present at arrival, must not match (its `Seq` equals the last consumed one or its contact time is not later); this is the test's stale case. Arrival order of the result and the write does not matter (E6): implement the matcher as a state-derived refresh run every `Step`, not as two event handlers. `FOwnResultMatcher` is declared by ADR-0018 §5.3 in `StealthGameplay/Public/View` and owned by the assassination epic; this core consumes its observations as `FHudInputs` fields and must not re-implement the capture of the pawn handle (that belongs to input-interaction-framework story 004's reply sink and the matcher's own story).

**Drops.** A result is dropped only if `seq ≤ seq_lastShown` or `seq` is in the last-8 ring, with the pair `(PossessionEpoch, Seq)` compared lexicographically (an earlier epoch is lower). A 9th distinct seq evicts the oldest ring entry. In live play an unshown, non-duplicate result is never dropped for lateness: processed after its `t_show`, it shows on processing (PEN-R16, D9). The ring and `seq_lastShown` are cleared on reconnect, travel and `RoundEpoch` change (a new PlayerController may restart `ClientCommandSeq`); keep one `Clear()` that story 044 calls.

**Live versus rebuild (HUD-R17).** `HudReplayWindowSec` applies only to rebuilds: a pending result whose `t_show` passed more than `HudReplayWindowSec` before the rebuild completes is not shown. The logged resolution of the HUD-R17 / F2 tension is: a same-controller rebuild (`OnRosterReset`) discards every showing element but keeps an unshown pending result under the replay window; on reconnect, travel and `RoundEpoch` change the controller or the seq space changes and pending results are dropped. Expose `OnRebuild(RebuildKind, CompletedAt)` with `RebuildKind = SameController | NewController` so story 044 only calls it; do not implement the wider rebuild here.

**Why outcome-independence matters.** `FStrikeResult` is enqueued at the end of the resolving drain for every outcome (ASN-R19, IIF R6), so `t_arrival` and `t_c,local` carry no outcome or victim-kind information. This core must therefore take no input that depends on the outcome: the outcome enum is not an input to the timer at all. A review check: the signature of the timing function has no `EStrikeOutcome` parameter.

**Out of this story:** the plate widgets, strings, stamps and cues (story 022), the F3 timeline (story 016), the tally move at `t_show` (story 020), the exile card (story 024), the PIE proof of the pipeline (stories 022, 028).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/hud/story-016-hud-plate-timeline-f3-and-reduce-motion.md`: what the alpha does after `t_show`.
- `production/epics/hud/story-022-hud-result-and-defence-plate-flow.md`: per-outcome plate text, glyph, the defender's Pending hold wiring and the result cue.
- `production/epics/hud/story-020-hud-wrong-target-tally-logic.md`: applying `TallyAfter` at the plate's `t_show`.
- `production/epics/hud/story-044-hud-reset-and-rebuild-logic.md`: calling `Clear()` and `OnRebuild` from the HUD-R17 procedure.
- `production/epics/hud/story-003-hud-input-sampling-and-host-parity-path.md`: sampling the reply sink and `FActionState` into `FHudInputs`.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthPresentation/Private/Hud/HudResultTimer.h`
- `Source/StealthPresentation/Private/Hud/HudResultTimer.cpp`
- `Source/StealthPresentation/Private/Hud/HudPresenterCore.cpp` (own and step the timer; expose the result-slot schedule in `FHudViewModel`)
- `Source/StealthPresentation/Private/Tests/HUD/ResultShowTimeTest.cpp` (class `FHudResultShowTimeTest`, name `StealthGame.HUD.ResultShowTime`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: Primary path
  - Given: (arrival, contact) pairs {1.000, 1.350}, {1.120, 1.050}, {1.000, 1.000}, {1.000, 2.250}, {1.000, 3.000}, and `ContactServerTime` 5.400 with offset 4.050 and no `FActionState` observed.
  - When: F2 is evaluated.
  - Then: show times are 1.350, 1.120, 1.000, 2.250, 2.250 (cap), and 1.350.
  - Edge cases: cap uses default `HudResultPlateMaxHoldBackSec` 1.25.
- **AC-2**: Fallback and drops
  - Given: an `FDefenceResult` arriving at 1.000 with a matching `FActionState` at 1.100 (`ContactServerTime` 5.400, offset 4.050). Also: no `FActionState` ever; a stale previous-strike `FActionState`; duplicate seq, lower seq, earlier `PossessionEpoch`; 9 distinct seqs.
  - When: results are processed.
  - Then: shown at 1.350. Never arriving shows at 2.250. A stale `FActionState` is ignored (stays Pending). Duplicate, lower and earlier-epoch seqs are dropped. After 9 distinct seqs the ring (read through a test accessor) holds exactly seqs 2-9.
  - Edge cases: none.
- **AC-3**: Live vs rebuild
  - Given: a result first processed 1.5 s after its show time in live play. A result pending across a same-controller rebuild with `t_show` = T, `HudReplayWindowSec` 1.0.
  - When: processing and rebuild occur.
  - Then: the live result shows on processing. The pending result is shown if the rebuild completes at T + 1.000 and not at T + 1.001.
  - Edge cases: same-controller means `OnRosterReset`. On reconnect, travel and `RoundEpoch` change, pending results are dropped (story 044). A rebuild discards every showing element.

QL-STORY-READY: ADEQUATE (qa-lead, 2026-10-25; criteria revised after the first pass and re-judged)

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthPresentation/Private/Tests/HUD/ResultShowTimeTest.cpp` (`StealthGame.HUD.ResultShowTime`) — must exist and pass. Gate level BLOCKING.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/hud/story-002-hud-view-model-and-presenter-core-shell.md`
- Unlocks: `production/epics/hud/story-022-hud-result-and-defence-plate-flow.md`, `production/epics/hud/story-024-hud-removal-card-timing-f4-logic.md`, `production/epics/hud/story-040-hud-end-banner-f10-and-round-summary-logic.md`, `production/epics/hud/story-044-hud-reset-and-rebuild-logic.md`
