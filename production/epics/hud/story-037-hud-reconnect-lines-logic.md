# Story 037: HUD-R10a reconnect line rules from observed bReconnecting edges and ReconnectEndCause

> **Epic**: HUD
> **Status**: Ready
> **Layer**: Presentation
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/hud.md`
**Requirement**: `TR-hud-001`, `TR-hud-005`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0001: Crowd identity rules (carriers and the Team-audience entry fields), with ADR-0021: UI/HUD layering and Common UI (presenter core, F8 feed)
**ADR Decision Summary**: The HUD reads only the viewer's own controller carriers and the GameState public structs; every HUD value is sampled by value every frame by the plain C++ `FHudPresenterCore`, never driven from a RepNotify. The team feed (F8) is a presenter-core state machine, and `FHudViewModel::Dump()` exposes it deterministically so the headless test and the PIE parity suites can read it. Reconnecting and back lines are produced only from the teammate `bReconnecting` bit and the `ReconnectEndCause` of the same entry update (HUD-R10a, user decision 2026-10-25, ruling D23).
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Engine surface: none (plain C++ core, no UWorld/UObject); the module still builds against 5.8.4 / C++20. The edge detector runs inside `FHudPresenterCore::Step` on an `FHudInputs` snapshot, so it never depends on a RepNotify having fired (ADR-0021 §4); the listen host, which gets no RepNotify for its own writes, runs the same path. Log with `UE_LOGF` rather than `UE_LOG` (preferred in 5.8).

**Control Manifest Rules (this layer)**:
- Required: Team feed reconnecting/back lines come only from the teammate `bReconnecting` bit (HUD-R10a); the marker label adds "· reconnecting" and the marker shape never changes — source: ADR-0021 (user decision 2026-10-25)
- Forbidden: Never depend on a RepNotify having fired; never drive widgets event-style from RepNotify/RPC handlers — the host gets no RepNotify for its own writes — source: ADR-0021
- Guardrail: HUD ≤ 0.3 ms CPU (AC-HUD-22), ≤ 0.5 ms with markers; UI GPU ≤ 0.5 ms at 1080p on the reference GPU — source: ADR-0021

This story owns the reconnect line half of MVP item 10: the decision of when a "{Ally} — reconnecting" or "{Ally} — back" line is created, and when none is. It adds two line types to the team feed queue built in story 035; it draws nothing (the feed widget is a view of the existing feed cluster) and it proves nothing across a network (story 038).

---

## Acceptance Criteria

*From GDD `design/gdd/hud.md` HUD-R10a (ruling D23) and AC-HUD-28 (the logic half), scoped to this story:*

- [ ] **AC1** (HUD-R10a, `StealthGame.HUD.ReconnectLines` edge table): **GIVEN** a team-0 ally entry update where `bReconnecting` goes `false → true`, and entry updates where it goes `true → false` with each `ReconnectEndCause` **WHEN** the presenter core steps over them **THEN** `false → true` yields one line "{name} — reconnecting"; `true → false` with `Rejoined` yields one line "{name} — back" (the only path to a "back" line); with `GraceExpired` no reconnect line is created (the removal line of HUD-R10 is that ally's only line); with `Cleared` or `None` no line is created; and no "— disconnected" line exists for any cause.
- [ ] **AC2** (HUD-R10a, `StealthGame.HUD.ReconnectLines` observed-edge rule): **GIVEN** a rebuild or a first entry with the flag already `true`, an informant ally or a killed-while-disconnected ally that stays flagged, and a superseding rejoin **WHEN** the presenter core steps over them **THEN** the HUD acts only on an observed edge and reads the cause from the same update (never the cause field alone), so a rebuild or a first entry with the flag already true shows no line; an informant ally or a killed-while-disconnected ally stays flagged with no clear line; and a superseding rejoin prints "back" only with a `Rejoined` clear.
- [ ] **AC3** (HUD-R10a and F8, `StealthGame.HUD.ReconnectLines` line mechanics): **GIVEN** reconnect lines created under load **WHEN** they are queued and the owner is the flagged player or `Phase` reaches `Ended` **THEN** they use the F8 queue and timings, carry no time, ping or reason, are never created on the disconnected player's own screen, and none is created at or after `Ended`.

---

## Implementation Notes

*Derived from ADR-0021 Decision §4 (presenter core, F8, HUD-R17) and `design/gdd/hud.md` HUD-R10a:*

**What to build.** A small plain C++ edge detector inside the presenter core (a private helper next to the feed code of story 035, for example `FHudReconnectEdges` in `Source/StealthPresentation/Private/Hud/`), keyed by the ally's `PublicHandle`. `FHudInputs` (story 003 fills it from the own controller's C-TeamView, `URosterTeamViewComponent`) carries, per teammate entry, `bReconnecting` and `ReconnectEndCause` (`uint8` enum: `None`, `Rejoined`, `GraceExpired`, `Cleared`). The detector remembers the last sampled `bReconnecting` per handle. In `Step`:

- `false → true` since the last sample: enqueue one `Reconnecting` line.
- `true → false`: read `ReconnectEndCause` from the same sample. `Rejoined` enqueues one `Back` line; every other value (`GraceExpired`, `Cleared`, `None`) enqueues nothing.
- Anything else (no change, a first sample for a handle, the first sample after `Bind` or a rebuild) only updates the remembered value and enqueues nothing. This is how "a rebuild replays nothing" is met without a special case: the remembered map is primed from the first sample and cleared on the HUD-R17 triggers (story 044 owns the trigger wiring; this story exposes `Reset()` on the helper).

**Why the cause is read from the same sample.** The Roster writes `ReconnectEndCause` in the same net update as every clear of the flag, and it is `None` while the flag is true or was never set. Reading the cause alone (for example "cause is `Rejoined`, so print back") would replay a "back" line after a rebuild whose sample still carries the last cause. The edge plus the same-sample cause is the only legal trigger. The Roster clears the flag only on an event, never on a timer (D23), so the HUD never infers a clear from time.

**Cases the table must produce, from the GDD.**
- `GraceExpired` is written only with the ally's applied `Active → Informant` conversion (forfeit); HUD-R10's "{Ally} — eliminated for the round" (story 035) is that ally's only line. One fact gets one line.
- An informant ally that stays disconnected stays flagged until a Roster event; at `GraceExpiryMs` nothing changes (no clear, no line). A later `Rejoined` clear yields "back".
- An `Active` ally killed while disconnected becomes an Informant but stays flagged through its `NoOp_AlreadyInState` grace expiry: removal line at its marker retirement (story 035), no line from the grace expiry, "back" only on a later `Rejoined` clear.
- A superseding rejoin that outlasts `G_eff` keeps the flag until the rejoin completes: "back" never prints before possession, only with a `Rejoined` clear.
- A kick (`bKicked`, `Cleared`), a removed record, and round end (`EndRoundAll`, `Cleared`) produce no line. The Roster writes the flag false outside `Active` with cause `Cleared`, so no line appears in `Preparing`, `Ended` or `Reset`; the HUD-R10 round-end rule (story 035) drops pending lines at `Ended` anyway, and this story must not create one at or after `Ended` even if a sample shows an edge.

**Own screen and opponents.** The detector skips the entry whose `PublicHandle` equals the owner's own handle, so a disconnected player's own HUD shows no feed line about itself (its own status is Session's L4 overlay). Opponents need no code: the carrier is per team, so no opponent entry exists in `FHudInputs` (HUD-R1; a feed line can only come from the viewer's own C-TeamView).

**Text.** Lines are `FText` from `ST_Hud` (story 005): `Hud.Feed.Reconnecting` "{Ally} — reconnecting" and `Hud.Feed.Back` "{Ally} — back", with the name resolved from C-Public `DisplayName` by `PublicHandle`, or `Hud.Ally.Unresolved` "Ally" when unresolved. Use the approved characters of the UX string table (em dash). The line carries only the name: no time, ping, reason or cause. The two types are text only (no Cochineal triangle; the triangle is finality and a reconnect is not, UX E06). Names are ellipsised at the plate width by the widget, not here.

**Feed mechanics.** Both line types go through the F8 queue exactly like a removal line: `HudFeedLineCount` visible, life `HudFeedLineLifeSec`, eviction only after `HudFeedMinDwellSec`, pending queue of at most 4. Do not add a second queue or a separate timing. Keys are read through `GetClientTuning()` (registered by story 001); no value is hardcoded.

**Does not touch.** The marker label suffix "· reconnecting" (teammate-markers epic, TM-R11; markers read the flag alone and never `ReconnectEndCause`). The F8 queue itself, the removal line and the round-end discard (story 035). The HUD-R17 rebuild wiring (story 044). Prototype code: the prototype has no reconnect lines; nothing is promoted from `Source/StealthGame/Private/StealthHUD.cpp` or the `MessageTeam` plumbing, which story 059 removes.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/hud/story-035-hud-team-feed-queue-f8-and-removal-lines.md`: the F8 queue, the removal line, the round-end discard and the feed widget content.
- `production/epics/hud/story-038-hud-reconnect-lines-pie.md`: the multi-client proof of cases (a)-(h) and the control-run equality.
- `production/epics/hud/story-044-hud-reset-and-rebuild-logic.md`: the HUD-R17 triggers that call the detector's `Reset()`.
- `production/epics/hud/story-003-hud-input-sampling-and-host-parity-path.md`: sampling the entries into `FHudInputs`.
- Teammate markers epic: the "· reconnecting" marker label suffix (TM-R11).

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthPresentation/Private/Hud/HudReconnectEdges.h`
- `Source/StealthPresentation/Private/Hud/HudReconnectEdges.cpp`
- `Source/StealthPresentation/Private/Hud/HudPresenterCore.cpp` (call the detector from `Step`, add the `Reconnecting` and `Back` feed line types)
- `Source/StealthPresentation/Private/Tests/HUD/ReconnectLinesTest.cpp` (class `FHudReconnectLinesTest`, name `StealthGame.HUD.ReconnectLines`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: Edge table
  - Given: entry updates with `bReconnecting` and `ReconnectEndCause`.
  - When: processed.
  - Then: `false->true` makes one "{name} — reconnecting". `true->false` with `Rejoined` makes one "{name} — back" (the only path). `GraceExpired`, `Cleared` and `None` make no reconnect line. No "— disconnected" line exists for any cause.
  - Edge cases: none.
- **AC-2**: Observed-edge rule
  - Given: a rebuild or first entry with the flag already true. An informant ally and a killed-while-disconnected ally. A superseding rejoin.
  - When: processed.
  - Then: no line on rebuild or first entry. No clear line for the informant and killed-while-disconnected allies. "back" only with a `Rejoined` clear. The cause is read from the same update, never alone.
  - Edge cases: none.
- **AC-3**: Line mechanics
  - Given: reconnect lines under load.
  - When: queued.
  - Then: they use the F8 queue and timings, text carries only the name, none appears on the disconnected player's own screen, and none at or after `Ended`.
  - Edge cases: none.

QL-STORY-READY: ADEQUATE

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthPresentation/Private/Tests/HUD/ReconnectLinesTest.cpp`, test `StealthGame.HUD.ReconnectLines` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later). Gate level: BLOCKING (the Logic default).

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/hud/story-035-hud-team-feed-queue-f8-and-removal-lines.md`; `production/epics/participant-state-roster/story-009-connection-projection.md`
- Unlocks: `production/epics/hud/story-038-hud-reconnect-lines-pie.md`, `production/epics/hud/story-044-hud-reset-and-rebuild-logic.md`
