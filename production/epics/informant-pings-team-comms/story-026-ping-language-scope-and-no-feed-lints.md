# Story 026: Ping language, scope and no-feed lints

> **Epic**: Informant Pings & Team Communication
> **Status**: Ready
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/informant-pings-team-comms.md` (PNG-R8, PNG-R11, PNG-R12; AC-PNG-12)
**Requirement**: `TR-png-020` (also `TR-png-017`)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0021: UI/HUD layering, CommonUI and Enhanced Input (no free text on the wire, no ping feed type), with ADR-0011 Technical Setup 2026-10-25b (label, no chat or voice action) and ADR-0007 Technical Setup 2026-10-25b (voice and chat module ban, T0 lint)
**ADR Decision Summary**: Every player-facing string comes from string tables and avoids the banned vocabulary; the MVP has no free-text team chat or in-engine voice (no module, plugin, class or config that enables them); a ping creates no feed line, plate, card or counter.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0021 overall; the lints are file and reflection scans, no post-cutoff API)
**Engine Notes**: ADR-0007 recorded the keys read at startup (`OnlineSubsystemUtils.cpp`, `VoiceModule.cpp`, UE 5.8.4): `[OnlineSubsystem] bHasVoiceEnabled=False` and `[Voice] bEnabled=False` in `Config/DefaultEngine.ini`. Lints belong to the T0 per-commit tier (ADR-0008) and run without an editor.

**Control Manifest Rules (this layer)**:
- Required: the `IA_Ping` binding label is the `ST_Controls` entry "Ping"; no ping string contains a banned word (AC-PNG-12 lint); `FNetTeamPing` holds only `Point`; `IMC_Common` has no chat or voice action — source: ADR-0011 (Technical Setup 2026-10-25b)
- Forbidden: Never add free-text team chat or in-engine voice in the MVP; never give a ping a feed line, plate or per-ping widget; never add a ping feed type, plate, card or counter to `FHudViewModel` or the F8 feed queue — source: ADR-0021, ADR-0007

---

## Acceptance Criteria

*From GDD `design/gdd/informant-pings-team-comms.md` (AC-PNG-12, PNG-R8, PNG-R11), revised at QL-STORY-READY, scoped to this story:*

- [ ] **AC1** (AC-PNG-12, `[Logic]` `StealthGame.Pings.Strings`): **GIVEN** every entry under the Pings string namespace plus the `ST_Controls` "Ping" entry and the "Not now" refusal plate entry (keys listed in the test) **WHEN** scanned **THEN** a whole-word, case-insensitive match finds none of spotted, detected, tracked, enemy, target, dead, ghost, spectator, and the binding label equals "Ping".
- [ ] **AC2** (derived, ADR-0007 Technical Setup 2026-10-25b, PNG-R11): **GIVEN** the repository **WHEN** linted **THEN** no `*.Build.cs` lists Voice, VoiceChat, AudioCapture or AudioCaptureCore, `StealthGame.uproject` enables none of AudioCapture, VoiceChat, EOSVoiceChat, no class uses `UVOIPTalker` or `VoipListenerSynthComponent`, `DefaultEngine.ini` pins `bHasVoiceEnabled=False` and `[Voice] bEnabled=False`, no action in `IMC_Common` has a name matching `(?i)chat|voice|mic|talk`, and `FNetTeamPing` has no string, text or name property.
- [ ] **AC3** (TR-png-017, PNG-R8): **GIVEN** `FHudViewModel` and the F8 feed queue **WHEN** reflected and a ping accept, echo and receive sequence is run with the view-model dump diffed **THEN** no ping feed type, plate, card or counter exists, and the only ping plate is the refusal plate triggered solely by the refusal sequence.

---

## Implementation Notes

*Derived from ADR-0007 Technical Setup 2026-10-25b and ADR-0021 Technical Setup 2026-10-25b:*

Add the Pings rows to the string-lint list used by the HUD and IIF string tests (the same banned vocabulary as the Roster and IIF language rules), list the keys explicitly in the test so a renamed key fails rather than silently dropping out of scope, and use whole-word matching so "Targeting" does not match "target". The voice and chat lints are file scans (state the files scanned as the denominator, and ignore commented-out entries) and a reflection scan of `IMC_Common` and `FNetTeamPing`. AC3 asserts the HUD view-model has no ping-derived item type; the concealed-refusal plate is input feedback owned by story 011 and the HUD, never shown for a received or echoed ping.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 010: creating the `IA_Ping` binding and its label entry.
- Story 011: the refusal plate behaviour.
- hud epic: the general HUD string lint and the F8 feed.
- tuning-data-config epic (T0 lint tier and include lint).

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Private/Tests/Pings/PingStringsTest.cpp` (class `FPingsStringsTest`, name `StealthGame.Pings.Strings`)
- `Source/StealthGameplay/Private/Tests/Pings/PingScopeLintTest.cpp` (class `FPingsScopeLintTest`, name `StealthGame.Pings.ScopeLint`)
- `Source/StealthPresentation/Private/Tests/Pings/PingNoFeedTest.cpp` (class `FPingsNoFeedTest`, name `StealthGame.Pings.NoFeed`)
- `Content/StealthGame/UI/StringTables/ST_Controls` (the "Ping" entry; only if story 010 has not added it)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

**Test: AC1 string lint**
- Given: the enumerated string entries.
- When: scan.
- Then: no banned word; the binding label reads "Ping".
- Edge cases: mixed case "Spotted" matches; "Targeting" is not a whole-word match for "target".

**Test: AC2 voice and chat lint**
- Given: the repo.
- When: lint.
- Then: no `*.Build.cs` lists Voice, VoiceChat, AudioCapture, AudioCaptureCore; `StealthGame.uproject` enables none of AudioCapture, VoiceChat, EOSVoiceChat; no use of `UVOIPTalker` or `VoipListenerSynthComponent`; `DefaultEngine.ini` has `bHasVoiceEnabled=False` and `[Voice] bEnabled=False`; no matching action in `IMC_Common`; `FNetTeamPing` has no `FString`/`FText`/`FName` property.
- Edge cases: a commented-out entry does not count (state the denominator of files scanned).

**Test: AC3 no feed**
- Given: `FHudViewModel` and the F8 feed queue.
- When: reflect; run accept, echo and receive events and diff the view-model dump.
- Then: no ping feed type, plate, card or counter; the only ping plate is the refusal plate, triggered solely by the refusal sequence.
- Edge cases: a received ping from a disconnected sender.

QL-STORY-READY: GAPS (revised) — the string scan scope, whole-word rule and the `IMC_Common` pattern were pinned as above on the qa-lead's proposals.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `PingStringsTest.cpp`, `PingScopeLintTest.cpp` and `PingNoFeedTest.cpp` — must exist and pass (`/story-done` checks that they EXIST; pass/fail is established by `/gate-check` and `/smoke-check`)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/informant-pings-team-comms/story-002-team-ping-wire-types-and-slot-component.md`, `story-010-ia-ping-action-and-send-path.md`
- Unlocks: None
