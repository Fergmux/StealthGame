# Story 005: ST_Hud string table with the approved wording and the language-rule lint

> **Epic**: HUD
> **Status**: Ready
> **Layer**: Presentation
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/hud.md`
**Requirement**: `TR-hud-002` (nearest entry: the language rule supports the leak rules)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0021: UI/HUD layering, CommonUI and Enhanced Input §4 (strings), with ADR-0001
**ADR Decision Summary**: All player-facing text comes from a string table (`ST_Hud`), never from data on the wire; dynamic values are formatted locally (names from C-Public `DisplayName` by `PublicHandle`, numbers from owner data). The language-rule word list is linted over the string table (AC-HUD-12 support). ADR-0001 keeps text off every replicated struct the HUD consumes, so no string can carry a kind, a team or a count of opponents.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Engine surface: `FText` and string-table assets (`UStringTable`), plus reflection over replicated structs for AC3; the tests run in the automation framework with no UWorld. The module still builds against 5.8.4 / C++20. The string table is a Content asset (named in prose only); the keys and approved texts are mirrored in a checked-in expected-data source so the diff has an oracle that is not the asset itself. `UE_LOGF` is preferred over `UE_LOG` in 5.8. Em dashes and middle dots in the approved text must survive the asset round-trip (UTF-8 source, no ASCII-isation).

**Control Manifest Rules (this layer)**:
- Required: Player-facing text comes from string tables — source: ADR-0021
- Forbidden: Never carry text on the wire — source: ADR-0021
- Guardrail: HUD ≤ 0.3 ms CPU (AC-HUD-22), ≤ 0.5 ms with markers; UI GPU ≤ 0.5 ms at 1080p on the reference GPU — source: ADR-0021

The language rule is a Pillar 1 control: a banned word or a digit-bearing template in the table could disclose a count, a clock or a killer. This story fixes the table and the lint before any cluster story picks a string.

---

## Acceptance Criteria

*From GDD `design/gdd/hud.md` AC-HUD-12 (language rule) and `design/ux/hud.md` UXA-HUD-19, scoped to this story:*

- [ ] **AC1** (derived, UX spec string table lines 257-290 and UXA-HUD-19, `StealthGame.HUD.StringTableLint`): **GIVEN** `ST_Hud` and the UX string table as expected data **WHEN** the test diffs keys and texts **THEN** any missing, extra or differently-worded key fails, and the table holds every player-facing HUD string with the exact approved text (D11 and D29 wording: "Informant", "Too many wrong targets", "Eliminated for the round", "You watch for your team now.", "Target eliminated", "Wrong target", "No contact", "Reversed", "Strike reversed", "Wrong targets: {n}", "Brace", "Brace in {m}:{ss}", "Returning in {s}", "Returning", "{Ally} — eliminated for the round", "{Ally} — reconnecting", "{Ally} — back", the banner phrasings, the summary lines, "The round was called off", "The host will begin the next round", "Informants can only observe", "Not now", "Busy", "Too far", "Third person shows only what your character can see."); the word "Exiled" and "exile" appear in no player-facing string (UXA-HUD-19).
- [ ] **AC2** (derived, AC-HUD-12 support and ADR-0021 §4, `StealthGame.HUD.StringTableLint`): **GIVEN** the lint **WHEN** it scans `ST_Hud` (and planted offenders) **THEN** it fails on dead, lives, revive, spectator, ghost, tracked, registered, detected, spotted, "left", "remaining", an opponent count, any digit-bearing template outside the allowlist {Tally.Line, Brace.Cooling, Countdown, Banner.Score, Summary.Own, Summary.Rival} (the two Summary templates exist only at `Ended` and are exempt from the opponent-count ban), and any time format (`\d+:\d\d`) outside the "Brace in" and "Returning in" templates; the clean table passes.
- [ ] **AC3** (derived, ADR-0001 and ADR-0021 §4, `StealthGame.HUD.StringTableLint`): **GIVEN** the replicated structs the HUD consumes (`FRoundPublicState`, the C-TeamView parts, `FStrikeResult`, `FDefenceResult`, `FHoldStatus`, `FActionState`, `FDeathState`) **WHEN** a reflection scan runs over them (nested structs and arrays included) **THEN** no `FText`/`FString` property is found; C-Public `DisplayName` is the sole exemption, read only through the name resolver ("Ally" when unresolved).

---

## Implementation Notes

*Derived from ADR-0021 §4 "Strings", UX spec string table `ST_Hud` and ADR-0001:*

Create the `ST_Hud` string table (Content asset, named in prose only) with the keys of the UX spec table (`design/ux/hud.md` lines 257-290): `Hud.Brace.Label`, `Hud.Brace.Cooling`, `Hud.Glyph.Unbound`, `Hud.Tally.Line`, `Hud.Step.Own` / `Hud.Step.Rival`, `Hud.Prompt.Tend` / `.SlipIn` / `.SlipOut`, `Hud.Hint.Strike`, `Hud.Reject.NotNow` / `.Busy` / `.OutOfReach`, `Hud.Informant.Plate`, `Hud.Hint.ThirdPerson`, `Hud.Result.RivalEliminated` / `.WrongTarget` / `.NoContact` / `.Reversed`, `Hud.Result.Defence`, `Hud.Card.Title`, `Hud.Card.Sub.Exile` / `.Sub.Removed`, `Hud.Card.Line2`, `Hud.Feed.Removed`, `Hud.Feed.Reconnecting` / `.Back`, `Hud.Ally.Unresolved`, `Hud.Marker.Reconnecting`, `Hud.Countdown` / `Hud.Countdown.Zero`, `Hud.Banner.Victory` / `.Defeat`, `Hud.Banner.Reason.Objective` / `.Elimination`, `Hud.Banner.Score`, `Hud.Summary.Own` / `.Rival`, `Hud.CalledOff`, `Hud.Lobby.Wait`. The expected-data source is a checked-in text file (`Source/StealthPresentation/Private/Tests/HUD/Data/ST_Hud.expected.txt`, UTF-8) generated from the UX table, so a changed word is a deliberate two-file change. The approved texts contain em dashes and middle dots (for example "{Ally} — eliminated for the round", "{Name} · reconnecting", "Your house {a} · Rival house {b}"); compare against the UX table, never an ASCII rendering.

Wording decisions that apply: D11 (user-ratified removal wording of 2026-10-25: title "Informant"; subtitles "Too many wrong targets" and "Eliminated for the round") and D29 (plate "Target eliminated" for the owner's rival kill; "exile" is reserved for the NPC-kill removal, and no player-facing string says "Exiled" or "exile"; the key names `Sub.Exile` and `RivalEliminated` are internal ids and not player-facing).

Language lint (a pure function over the table, so planted offenders can be unit-tested): banned words dead, lives, revive, spectator, ghost, tracked, registered, detected, spotted, plus "left" and "remaining" as whole words (match on word boundary so "leftover" is a deliberate review item, not a silent pass); an opponent count (a digit-bearing template that names a rival or opponent quantity); digit-bearing templates permitted only for {Tally.Line, Brace.Cooling, Countdown, Banner.Score, Summary.Own, Summary.Rival}; time formats `\d+:\d\d` permitted only in the "Brace in" (`Hud.Brace.Cooling`) and "Returning in" (`Hud.Countdown`) templates. The two Summary templates (E21) show only at `Ended` and are exempt from the opponent-count ban. No string names the strike limit as a number, a clock, a count of opponents, a killer, a kind or a team.

The reflection test (AC3) walks the replicated structs by `UScriptStruct` and flags any `FTextProperty` or `FStrProperty`/`FNameProperty` carrying player-facing text, recursing through nested structs and `TArray` inner properties. The only exemption is C-Public `DisplayName` (the allowlisted ADR-0001 field), which the HUD reads solely through the name resolver and which falls back to `Hud.Ally.Unresolved` ("Ally"). The struct list is a constant in the test; a new replicated struct consumed by the HUD must be added to it in the same change.

Prototype disposition: the prototype's hard-coded strings and `MessagePlayer` / `MessageController` / `MessageTeam` / `MessageAll` message plumbing in `StealthPlayerController.*` / `StealthGameMode.cpp` carry text on the wire; they are not promoted and are removed by story 059.

Out of this story: runtime collection of displayed strings in a 3-minute round (story 053 `NoClockNoCounts`), localisation extraction, and any widget that uses a string.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/hud/story-053-hud-no-clock-no-counts-pie.md`: AC-HUD-12 runtime collection of every displayed string.
- `production/epics/hud/story-059-hud-retire-prototype-canvas-and-message-plumbing.md`: removing the prototype message plumbing that carries text on the wire.
- `production/epics/hud/story-017-hud-shared-visual-system-font-face-and-plate-style.md`: the display face and string-width re-measure (HUD-UX-8).
- `production/epics/hud/story-002-hud-view-model-and-presenter-core-shell.md`: the `FText` fields the table feeds.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthPresentation/Private/Hud/HudStrings.h` (key constants and the `FText` lookup helper)
- `Source/StealthPresentation/Private/Hud/HudStrings.cpp`
- `Source/StealthPresentation/Private/Hud/HudLanguageLint.h` (pure function over a key/text list)
- `Source/StealthPresentation/Private/Hud/HudLanguageLint.cpp`
- `Source/StealthPresentation/Private/Tests/HUD/StringTableLintTest.cpp` (class `FHudStringTableLintTest`, name `StealthGame.HUD.StringTableLint`)
- `Source/StealthPresentation/Private/Tests/HUD/Data/ST_Hud.expected.txt` (the expected key/text oracle, UTF-8)
- The `ST_Hud` string-table asset is a Content asset (named in prose only)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: `ST_Hud` diff against the UX string table
  - Given: `ST_Hud` and the UX string table (`design/ux/hud.md` lines 257-290) as expected data.
  - When: the test diffs keys and texts.
  - Then: any missing, extra or differently-worded key fails. "Exiled" and "exile" appear in no player-facing string.
  - Edge cases: the approved texts contain em dashes and middle dots; compare against the UX table, not an ASCII rendering.
- **AC-2**: language-rule lint
  - Given: `ST_Hud` plus planted offenders.
  - When: the lint runs.
  - Then: it fails on dead, lives, revive, spectator, ghost, tracked, registered, detected, spotted, "left", "remaining", an opponent count, any digit-bearing template outside the allowlist {Tally.Line, Brace.Cooling, Countdown, Banner.Score, Summary.Own, Summary.Rival}, and any `\d+:\d\d` time format outside the "Brace in" and "Returning in" templates.
  - Edge cases: the two Summary templates are exempt from the opponent-count ban (Ended only). The clean table passes.
- **AC-3**: no text in replicated structs
  - Given: `FRoundPublicState`, C-TeamView parts, `FStrikeResult`, `FDefenceResult`, `FHoldStatus`, `FActionState`, `FDeathState`.
  - When: a reflection scan runs.
  - Then: no `FText`/`FString` property is found. C-Public `DisplayName` is the sole exemption, read only through the name resolver ("Ally" when unresolved).
  - Edge cases: scan nested structs and arrays.

QL-STORY-READY: ADEQUATE (qa-lead, 2026-10-25; criteria revised after the first pass and re-judged)

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthPresentation/Private/Tests/HUD/StringTableLintTest.cpp` (`StealthGame.HUD.StringTableLint`) — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`)
- Gate: BLOCKING (Logic default).

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/hud/story-002-hud-view-model-and-presenter-core-shell.md`
- Unlocks: `production/epics/hud/story-017-hud-shared-visual-system-font-face-and-plate-style.md`, `production/epics/hud/story-035-hud-team-feed-queue-f8-and-removal-lines.md`, `production/epics/hud/story-040-hud-end-banner-f10-and-round-summary-logic.md`, `production/epics/hud/story-053-hud-no-clock-no-counts-pie.md`
