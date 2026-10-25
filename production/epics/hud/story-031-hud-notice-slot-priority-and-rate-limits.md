# Story 031: Result and Notice slot priority, one-shot drops and the rejection, informant and ping-refusal limiters

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

**ADR Governing Implementation**: ADR-0021: UI/HUD layering, CommonUI and Enhanced Input
**ADR Decision Summary**: `FHudPresenterCore` owns HUD-R13 slot priorities and every one-shot notice rule, driven by a fake clock and fake carriers in headless specs. The owner reply sink (ADR-0011 `ClientCommandReplies`) is consumed in order by `UStealthHudPresenter`. `FHudInputs.PingConcealedRefusalSeq` is sampled every frame from `FStealthClientViewHandles::PingView`, and `LastSeenRefusalSeq` is set at `Bind`, so a ping refusal reaches the same `RejectPlateCooldownSec` limiter as a rejection reply.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Engine surface: none (plain C++ core, no UWorld/UObject); the module still builds against 5.8.4 / C++20. The once-per-profile third-person flag lives in `UStealthHudLocalStore` (a `ULocalPlayerSubsystem`), outside the core; the core receives it as an input value and reports "consumed" as an output event. The concealed-ping refusal is client-local and is never sent to the server; the listen host uses the same path.

**Control Manifest Rules (this layer)**:
- Required: `FHudInputs.PingConcealedRefusalSeq` is sampled every frame from `FStealthClientViewHandles::PingView`; `FHudPresenterCore.LastSeenRefusalSeq` is set at `Bind`; a change submits one `NotNow` refusal to the existing `RejectPlateCooldownSec` limiter, which shows the "Not now" plate and plays the owner-only 2D `PingConcealedRefused` cue together or neither; the listen host uses the same path — source: ADR-0021 (Technical Setup 2026-10-25b)
- Forbidden: Never add a ping feed type, plate, card or counter to `FHudViewModel` or the F8 feed queue (the concealed-refusal plate is input feedback, never shown for a received or echoed ping) — source: ADR-0021 (Technical Setup 2026-10-25b)
- Guardrail: HUD ≤ 0.3 ms CPU (AC-HUD-22), ≤ 0.5 ms with markers; UI GPU ≤ 0.5 ms at 1080p on the reference GPU — source: ADR-0021

Two transient slots in the lower-centre column (Result and Notice) are shared by many messages. This story is the arbiter: which message wins, which are dropped rather than queued, and how often each repeatable plate may appear. The choice is by priority alone, never by what the viewer looks at, which keeps it identical for every kind of non-ally subject (HUD-R3).

---

## Acceptance Criteria

*From GDD `design/gdd/hud.md` HUD-R13 and ADR-0021 (Technical Setup 2026-10-25b); UX spec UXA-HUD-14 and UXA-HUD-16 restated; scoped to this story:*

- [ ] **AC1** (HUD-R13, UXA-HUD-14; `StealthGame.HUD.NoticeSlots` slot cases): **GIVEN** simultaneous candidates for each slot **WHEN** `Step` runs **THEN** the Result slot picks removal card > result/defence plate > informant plate and the Notice slot picks returning countdown > third-person hint > strike hint; a one-shot notice arriving while its slot is occupied is dropped and never queued (the informant plate; the third-person hint, whose once-per-profile flag is consumed only when actually shown and retried on the next third-person entry); the strike hint, being level-triggered, returns on the frame a higher notice leaves.
- [ ] **AC2** (UXA-HUD-16; `StealthGame.HUD.NoticeSlots` limiter cases): **GIVEN** rejection replies "Not now", "Busy", "Too far" and `Cooldown`, and informant presses **WHEN** replies arrive in bursts **THEN** a rejection plate shows at most once per `RejectPlateCooldownSec` per code (exactly-at-cooldown boundary tested), a `Cooldown` reply shows no plate, and a different code arriving while a plate shows replaces it in one step; the informant plate shows at most once per `InformantPlateCooldownSec` and a press before the card has collapsed shows nothing.
- [ ] **AC3** (ADR-0021 Technical Setup 2026-10-25b, ruling D26; `StealthGame.HUD.NoticeSlots` ping-refusal case): **GIVEN** a change of `FHudInputs.PingConcealedRefusalSeq` **WHEN** the limiter is applied **THEN** one `NotNow` refusal is submitted to the existing limiter: the "Not now" plate and the owner-only 2D `PingConcealedRefused` cue appear together or neither; `LastSeenRefusalSeq` is set at `Bind`, so the first bind shows no plate.

---

## Implementation Notes

*Derived from ADR-0021 §4, GDD HUD-R13, HUD-E15/E16/E17 and UX elements E11, E14-E17:*

**What to build.** Slot arbitration and limiter logic inside `FHudPresenterCore` (`Source/StealthPresentation/Private/Hud/`), with no widget code (story 032 draws the plates).

**Slot priority (HUD-R13).**
- Result slot: removal card (its own band, preempts) > result plate / defence plate > informant plate.
- Notice slot: returning countdown (only in `InformantAwaiting`, where nothing else can use the slot) > third-person hint > strike hint.
- The slot's occupant is chosen by priority alone, never by what the viewer is looking at.
- One-shot notices (informant plate, third-person hint) arriving while their slot is occupied are dropped, never queued. The third-person hint's once-per-profile flag (held in `UStealthHudLocalStore`, passed in as a value) is consumed only when the hint is actually shown; when the slot is busy it is retried on the next third-person entry (`GetViewState() = Live`).
- The strike hint is level-triggered (story 030 supplies `bStrikeHintWanted`): it shows whenever its predicate holds and the Notice slot is free, including the frame a higher notice leaves.

**Rate limiters.**
- Rejection plate (HUD-E15): codes "Not now" (`NotNow`), "Busy" (`Busy`), "Too far" (`OutOfReach`); at most once per `RejectPlateCooldownSec` per code (IIF-owned key, read via client tuning); a `Cooldown` rejection shows no plate (the Cooling numeral says it); a different code arriving while a plate shows replaces it in one step; timeline 400 ms / 1.5 s / 600 ms (timeline phases are story 016; this story decides whether a plate shows). Replies to a pawn struck in the same drain are presented no earlier than its contact time (story 022's plate path; keep the limiter downstream of that hold).
- Informant plate (HUD-E16): "Informants can only observe", at most once per `InformantPlateCooldownSec`, shown only from presses caught under `IMC_Informant` (ADR-0021 §8), so it never appears before the card has collapsed; a press before the card has collapsed shows nothing (dropped, not deferred). Timeline 400 ms / 2 s / 600 ms.
- Ping refusal (HUD-R13, ruling D26): `FHudInputs.PingConcealedRefusalSeq` is sampled every frame from `FStealthClientViewHandles::PingView`; `LastSeenRefusalSeq` is set at `Bind` (so a rebuild or first bind shows nothing); a change submits one `NotNow` refusal to the same limiter, which shows the "Not now" plate and emits the `PingConcealedRefused` cue event together or neither. The core emits the cue as an event in its output; story 051 routes it to the owner-only 2D UI bus.

**Traps.** Never queue a dropped one-shot notice (the rate limits already bound repeats). The ping-refusal plate is input feedback: it is never shown for a received or echoed ping, and the core must not add a ping feed type, plate, card or counter to `FHudViewModel` or the F8 feed queue. The presenter never reads concealment state: the ping refusal arrives as a sequence number only.

**Tests.** `Source/StealthPresentation/Private/Tests/HUD/NoticeSlotsTest.cpp`, class `FHudNoticeSlotsTest`, name `StealthGame.HUD.NoticeSlots`. Drive `Step` with a fake clock, fake reply entries and injected candidate flags. Include the exactly-at-cooldown boundary (a reply at `RejectPlateCooldownSec` exactly after the previous plate) and the `Bind` case for `LastSeenRefusalSeq`.

**Prototype.** The prototype message plumbing (`MessagePlayer`, `MessageController`, `MessageTeam`, `MessageAll`) shows text with no priority or limiter; it is retired by story 059 and none of it is promoted.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/hud/story-030-hud-strike-hint-predicate-and-lockout.md`: the strike hint predicate.
- `production/epics/hud/story-033-hud-returning-countdown-f7.md`: the F7 countdown text and its Notice-slot rules.
- `production/epics/hud/story-032-hud-rejection-informant-and-hint-widgets.md`: the plate and hint widgets and their placement.
- `production/epics/hud/story-016-hud-plate-timeline-f3-and-reduce-motion.md`: plate timeline phases and Reduce motion.
- `production/epics/hud/story-050-hud-ping-refusal-owner-only-pie.md`: PIE proof that the ping refusal is owner-only.
- `production/epics/hud/story-051-hud-cue-event-stream-and-owner-only-audio-hooks.md`: routing of the `PingConcealedRefused` cue.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthPresentation/Private/Hud/HudNoticeSlots.h` and `.cpp` (slot arbitration and limiters, used by `FHudPresenterCore`)
- `Source/StealthPresentation/Private/Hud/HudPresenterCore.cpp` (wire `Step`, `Bind` and `LastSeenRefusalSeq`)
- `Source/StealthPresentation/Private/Hud/HudInputs.h` (reply entries, `PingConcealedRefusalSeq`)
- `Source/StealthPresentation/Private/Tests/HUD/NoticeSlotsTest.cpp` (class `FHudNoticeSlotsTest`, name `StealthGame.HUD.NoticeSlots`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: slot priority
  - Given: simultaneous candidates for each slot.
  - When: `Step` runs.
  - Then: Result slot: card > result/defence plate > informant plate. Notice slot: countdown > third-person hint > strike hint. A one-shot notice arriving while its slot is occupied is dropped, never queued. The third-person once-per-profile flag is consumed only when shown, and retried on the next entry.
  - Edge cases: the strike hint is level-triggered and returns when a higher notice leaves.
- **AC-2**: rejection and informant limiters
  - Given: rejection replies "Not now", "Busy", "Too far", `Cooldown`, and informant presses.
  - When: replies arrive in bursts.
  - Then: at most one plate per `RejectPlateCooldownSec` per code. `Cooldown` shows none. A different code replaces a showing plate in one step. The informant plate shows at most once per `InformantPlateCooldownSec`, and none before the card has collapsed.
  - Edge cases: exactly-at-cooldown boundary.
- **AC-3**: ping refusal
  - Given: `PingConcealedRefusalSeq` changes.
  - When: the limiter is applied.
  - Then: the "Not now" plate and the `PingConcealedRefused` cue appear together or neither. `LastSeenRefusalSeq` is set at `Bind`, so there is no plate on first bind.
  - Edge cases: none.

QL-STORY-READY: ADEQUATE

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthPresentation/Private/Tests/HUD/NoticeSlotsTest.cpp` (`StealthGame.HUD.NoticeSlots`) — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`; gate BLOCKING)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/hud/story-002-hud-view-model-and-presenter-core-shell.md`, `production/epics/hud/story-014-hud-presented-own-state-and-viewer-state-machine.md`, `production/epics/hud/story-016-hud-plate-timeline-f3-and-reduce-motion.md`; `production/epics/input-interaction-framework/story-029-rejection-code-set.md`
- Unlocks: `production/epics/hud/story-032-hud-rejection-informant-and-hint-widgets.md`, `production/epics/hud/story-033-hud-returning-countdown-f7.md`, `production/epics/hud/story-034-hud-informant-presentation.md`, `production/epics/hud/story-048-hud-interact-ring-and-context-prompt.md`, `production/epics/hud/story-050-hud-ping-refusal-owner-only-pie.md`, `production/epics/hud/story-052-hud-accessibility-settings-hooks.md`
