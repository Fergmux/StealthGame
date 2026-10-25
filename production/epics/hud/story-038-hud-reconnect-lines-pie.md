# Story 038: PIE proof of every reconnect line case (AC-HUD-28 a-h)

> **Epic**: HUD
> **Status**: Blocked — BLOCKED: spikes S2 (IVP story 002) and S14 (IVP story 003) have not reported (ADR-0001 conditional gate: rejoin and seamless-travel behaviour)
> **Layer**: Presentation
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/hud.md`
**Requirement**: `TR-hud-001`, `TR-hud-005`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0001: Crowd identity rules (rejoin behaviour and the carrier set), with ADR-0008: Test automation, leak and parity (the multi-client PIE harness) and ADR-0021: UI/HUD layering and Common UI
**ADR Decision Summary**: The HUD reads only the viewer's own carriers, so an opponent has no carrier from which a reconnect line could be built; ADR-0008 fixes the PIE harness (multi-process CQTest, dumps keyed by tick index, replicated-property logs and RPC lists compared against a control run) that proves it. HUD-R10a derives reconnecting and back lines only from observed `bReconnecting` edges and the same-update `ReconnectEndCause`. ADR-0001 is Accepted conditional on spikes S2 and S14, which decide rejoin and seamless-travel behaviour; this story rests on both.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: This is a HIGH networking-domain story (ADR-0018 note: MotionWarping is Beta in 5.8.4 and the networking domain is HIGH risk). The rejoin and supersede paths exercise `PlayerState` trimming and seamless-travel behaviour that spikes S2 and S14 have not yet confirmed on 5.8.4, which is why the story is Blocked. The HUD side is the same plain presenter path as story 037; tests are CQTest `NETWORK_TEST_CLASS` with the 60 Hz fixed step of the GDD determinism rule. Log with `UE_LOGF`.

**Control Manifest Rules (this layer)**:
- Required: Team feed reconnecting/back lines come only from the teammate `bReconnecting` bit (HUD-R10a); the marker label adds "· reconnecting" and the marker shape never changes — source: ADR-0021 (user decision 2026-10-25)
- Forbidden: Never iterate actors, read a pawn's `PlayerState`/`Controller`/`Owner`, call `IsPlayerControlled`/`IsBotControlled`/`CanViewerKnow`, include a `Public/Server/` header, or write anything replicated from Presentation — source: ADR-0021, ADR-0001
- Guardrail: HUD ≤ 0.3 ms CPU (AC-HUD-22), ≤ 0.5 ms with markers; UI GPU ≤ 0.5 ms at 1080p on the reference GPU — source: ADR-0021

This story is the network proof of story 037's rules. It runs every HUD-R10a case on real 2v2 clients, including the three that need a rejoin or a kick, and shows that nothing about the disconnect reaches the other team.

---

## Acceptance Criteria

*From GDD `design/gdd/hud.md` AC-HUD-28 (`StealthGame.HUD.PIE.ReconnectLines`), split into four criteria, scoped to this story. Cases (a)-(h) are quoted from the AC text:*

- [ ] **AC1** (AC-HUD-28 (a), (b), `StealthGame.HUD.PIE.ReconnectLines`): **GIVEN** 2v2 PIE and the Roster's `ReconnectEndCause` (written with every clear of `bReconnecting`), **WHEN** each of the following is run for a team-0 ally and observed on its team-0 ally's dump: (a) an `Active` player disconnects and rejoins inside the grace, **THEN** exactly "{name} — reconnecting" at the flag set and "{name} — back" at the `Rejoined` clear; (b) an `Active` player lets the grace expire, **THEN** "{name} — reconnecting" then the HUD-R10 removal line only, with no "back" line and no other reconnect line.
- [ ] **AC2** (AC-HUD-28 (c), (d), (e)): **GIVEN** the same 2v2 PIE fixture, **WHEN** (c) an **informant** ally disconnects and is still away at `GraceExpiryMs`, (d) an `Active` ally is **killed while disconnected**, (e) a **superseding rejoin that outlasts `G_eff`** (loading past the grace) is run, **THEN** (c) "{name} — reconnecting" at the set, no clear and no line at `GraceExpiryMs` (the flag stays true and the label suffix persists, Roster AC-PSR-53 (b)), and "back" only on a later `Rejoined` clear; (d) its removal line at its marker's retirement, the flag stays true (the label suffix persists) through its `NoOp_AlreadyInState` grace expiry with no line, and "back" only on a later `Rejoined` clear (Roster AC-PSR-53 (c)), with no "— disconnected" line in any case; (e) no "back" line before the rejoiner is possessed, and "back" only with a `Rejoined` clear (or the (b)/(c) outcome if the grace ran out first).
- [ ] **AC3** (AC-HUD-28 (f), (g), (h)): **GIVEN** the same fixture, **WHEN** (f) a flagged ally is **kicked** (Session `bKicked`), (g) the round ends while an ally is flagged, (h) a viewer rebuilds after a `Rejoined` clear, **THEN** (f) `Cleared` and no line from the clear; (g) `Cleared`, no line, and no new line after `Ended`; (h) no line replays.
- [ ] **AC4** (AC-HUD-28 closing clause): **GIVEN** every case (a)-(h) and a control run without the disconnect, **WHEN** the dumps, replicated-property logs and RPC lists are compared, **THEN** in every case both team-1 clients' dumps, replicated-property logs and RPC lists equal the control run, and the disconnected player's own HUD shows no feed line about itself.

---

## Implementation Notes

*Derived from ADR-0008 (PIE leak and parity harness) and `design/gdd/hud.md` HUD-R10a, AC-HUD-28:*

**What to build.** One CQTest PIE scenario file with one `TEST_METHOD` per case group, using `NETWORK_TEST_CLASS(ReconnectLines, "StealthGame.HUD.PIE")` so the full names are `StealthGame.HUD.PIE.ReconnectLines.<Method>` (test-standards CQTest exemption). The multi-process harness, the 2v2 fixture, the dump capture and the RPC and replicated-property logging come from `production/epics/information-visibility-policy/story-001-spike-s1-multiprocess-raw-bunch-capture.md` and story 003 of this epic; this story only scripts the disconnect and kick scenarios on top of them. No new HUD production code is expected; if a case fails, fix it in story 037 or 035, not here.

**Determinism.** Fixed step 60 Hz, identical seeds, dumps keyed by tick index with wall-clock fields excluded. The "control run" is the same seed with no disconnect, kick or rejoin; "equal to the control" means the team-1 dumps, replicated-property logs and RPC lists are equal tick for tick. The oracle for each line is the observing team-0 ally's `FHudViewModel::Dump()` feed cluster, never a log message.

**Case notes.**
- (a)/(b) Disconnect an `Active` team-0 player through the harness's drop hook, rejoin inside the grace for (a), let `GraceExpiryMs` pass for (b). In (b) the only lines are "reconnecting" and the HUD-R10 removal line, and no "back": the Roster writes `GraceExpired` only with the applied `Active → Informant` conversion.
- (c) Make the ally an informant first, then drop it; assert that at `GraceExpiryMs` no clear arrives and the label suffix persists, then rejoin and expect "back". (The informant marker label is a markers-epic surface; this story asserts only the feed.)
- (d) Strike the disconnected `Active` ally while it is away; assert reconnecting, then the removal line at the marker's retirement, no line at its `NoOp_AlreadyInState` grace expiry, "back" only on a later `Rejoined`.
- (e) Delay the rejoiner's possession past `G_eff` using the harness's load stall; assert "back" is absent until possession.
- (f)/(g)/(h) Kick through Session's host action, end the round while flagged, rebuild a viewer (story 044's trigger) after a `Rejoined` clear; assert no line, no new line after `Ended`, and no replay.
- Disconnected player's own HUD: dump its feed cluster after the rejoin and assert it holds no line naming itself.

**Blocking.** The rejoin, supersede and seamless-travel behaviour here is what ADR-0001's conditional gate (spikes S2 and S14, IVP stories 002 and 003) decides. Do not start until both report. The tests may be written against the harness fixture earlier, but the story's cases must not be marked passing on an unconfirmed rejoin model.

**Does not touch.** The reconnect line logic (story 037) and the F8 queue and removal line (story 035). The Roster's flag writes (participant-state-roster epic; its own PIE is story 051 there, which this story consumes as a fixture, not a duplicate).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/hud/story-037-hud-reconnect-lines-logic.md`: the edge rules themselves.
- `production/epics/hud/story-036-hud-team-feed-opponent-and-round-end-pie.md`: opponent isolation and round-end proofs for removal lines.
- `production/epics/hud/story-045-hud-rebuild-no-replay-pie.md`: the owner's own disconnect, rejoin and `RoundEpoch` rebuild proof (plates, cards, tally).
- `production/epics/participant-state-roster/story-051-pie-teammate-reconnecting-flag.md`: the Roster's own proof of the flag and cause writes.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthTestHarness/Private/HUD/ReconnectLinesTest.cpp` (CQTest, `NETWORK_TEST_CLASS(ReconnectLines, "StealthGame.HUD.PIE")`, `StealthGame.HUD.PIE.ReconnectLines.*`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: AC-HUD-28 cases (a) and (b)
  - Given: 2v2 PIE with `ReconnectEndCause`, observed on a team-0 ally's dump. (a) an Active player rejoins inside the grace. (b) the grace expires.
  - When: each case runs.
  - Then: (a) "reconnecting" then "back". (b) "reconnecting" then the HUD-R10 removal line only, with no "back" and no other reconnect line.
  - Edge cases: blocked on IVP stories 002 and 003. DEFERRED until they report.
- **AC-2**: AC-HUD-28 cases (c), (d), (e)
  - Given: (c) an informant ally still away at `GraceExpiryMs`. (d) an Active ally killed while disconnected. (e) a superseding rejoin outlasting `G_eff`.
  - When: each case runs.
  - Then: (c) no clear and no line at expiry, "back" only on a later `Rejoined`, label suffix persists. (d) removal line at marker retirement, flag stays true through the `NoOp_AlreadyInState` grace expiry with no line. (e) no "back" before possession, and "back" only with a `Rejoined` clear.
  - Edge cases: none.
- **AC-3**: AC-HUD-28 cases (f), (g), (h)
  - Given: (f) a flagged ally is kicked. (g) the round ends while an ally is flagged. (h) a viewer rebuilds after a `Rejoined` clear.
  - When: each case runs.
  - Then: (f) `Cleared`, no line. (g) `Cleared`, no line, no new line after `Ended`. (h) no line replays.
  - Edge cases: none.
- **AC-4**: Control equality
  - Given: every case (a)-(h) plus a control run without the disconnect.
  - When: dumps, replicated-property logs and RPC lists are compared.
  - Then: both team-1 clients' dumps, replicated-property logs and RPC lists equal the control. The disconnected player's own HUD shows no feed line about itself.
  - Edge cases: none.

DEFERRED: AC-1 depends on IVP stories 002 and 003 (spikes S2 and S14) reporting; the other criteria run on the same fixture and are held with it.

QL-STORY-READY: ADEQUATE

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/HUD/ReconnectLinesTest.cpp`, test `StealthGame.HUD.PIE.ReconnectLines` OR a documented playtest; logs retained in `production/qa/evidence/hud-reconnect-lines-evidence.md`. Gate level: BLOCKING (the Integration default).

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/hud/story-036-hud-team-feed-opponent-and-round-end-pie.md`, `production/epics/hud/story-037-hud-reconnect-lines-logic.md`, `production/epics/hud/story-003-hud-input-sampling-and-host-parity-path.md`; `production/epics/participant-state-roster/story-051-pie-teammate-reconnecting-flag.md`; `production/epics/information-visibility-policy/story-002-spike-s2-seamless-travel-and-rejoin-playerstate-trims.md`, `story-003-spike-s14-s1-rejoin-and-pawnrecovery-possession.md`
- Unlocks: `production/epics/hud/story-060-hud-run-and-observe-2v2-evidence.md`, `production/epics/hud/story-061-hud-mvp-exit-5v5-presentation-verification.md`
