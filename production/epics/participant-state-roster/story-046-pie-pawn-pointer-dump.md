# Story 046: Non-owned pawns carry null PlayerState, Controller and Owner for players and NPCs alike

> **Epic**: Participant State & Roster
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/participant-state-roster.md`
**Requirement**: `TR-roster-006`, `TR-roster-011`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0001: Crowd identity rules
**ADR Decision Summary**: Pillar 1 ("an enemy cannot tell a player from an NPC") can be broken by one Unreal default: every player pawn replicates its `PlayerState`, `Controller` and `Owner` to every client. ADR-0001 resets those three inherited link properties to `COND_OwnerOnly` on `AStealthPawn` (condition-only overload, push flag of `Owner` kept), so non-owners receive them for neither pawn kind; the test is absence from every non-owner changelist, not "equal null". Teammates get the pawn link only as a `PawnRef` through C-TeamView, and a written fallback (disable the pointers for every pawn, give the owner its link through its own controller) exists if S1 or S14 fail.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Post-cutoff: `APawn::RemoteViewPitch16` (5.6; the 8-bit `RemoteViewPitch` is deprecated, `COND_ReplayOnly`) [V]; `net.Iris.UseIrisReplication` default 0, legacy NetDriver with Iris pinned off in `Config/DefaultEngine.ini` [V]. `APawn::PlayerState` is private, so its reset goes by name (a misspelled name is a fatal startup error, not a compile error). Verification required: S1 raw-bunch parity (the three pointers absent from every non-owner changelist for player and NPC pawns alike, under `LeakNetProfile`), S2, S14; harness startup assertion that the game NetDriver runs the legacy replication system. S1 reported 2026-10-25: PASS for the three pointers, FAIL for full pawn parity (that part is Story 049).

**Control Manifest Rules (this layer)**:
- Required: `AStealthPawn` resets `PlayerState` (by name), `Controller` and `Owner` to `COND_OwnerOnly` with the condition-only `ResetReplicatedLifetimeProperty` overload (`RESET_REPLIFETIME_CONDITION` for the public members) — source: ADR-0001
- Required: T2/T3 tests are bound by frames or server time and assert and record the active replication system; every leak, parity, statistical and lint gate includes a negative control the suite detects — source: ADR-0008
- Required: `AStealthPlayerController` clears `Owner`/`PlayerState` client-locally on its previous pawn — source: ADR-0001
- Forbidden: Never branch on `IsPlayerControlled()`, `IsBotControlled()`, controller class, `PlayerState` presence, team or Roster state in pawn, component, AnimBP or audio code for anything another client can observe — source: ADR-0001
- Forbidden: Never use the per-actor active-override toggle for the owner-only pointers; never read `Owner` or `PlayerState` on a pawn the owner does not currently possess — source: ADR-0001
- Guardrail: C-TeamView pushes change-only, at most one per frame, within the 50 KB/s per-client budget — source: ADR-0001

---

## Acceptance Criteria

*From GDD `design/gdd/participant-state-roster.md`, scoped to this story:*

- [ ] **AC-PSR-39** [PIE-3][S1] `StealthGame.Roster.PIE.PawnPointerDump`: GIVEN a fixed-seed layout with all 4 player pawns and 20 NPC pawns placed inside every client's cull radius, and a modified client that dumps `PlayerState`, `Controller` and `Owner` for every pawn it receives (resolved value and raw received bunch), plus every PlayerController and teammate entry it receives, WHEN it dumps after settling and again after each scripted event (spawn, kill, informant adoption, disconnect hand-over, rejoin), THEN:
  - the received pawn count equals 24 at every dump;
  - all three pointers are null on every non-owned pawn, player and NPC alike, and the raw bunch contents are identical in kind (no unmappable NetGUID for player pawns only);
  - the client receives no PlayerController but its own;
  - its C-TeamView lists only its own team (R6; retires the prototype `IsNpc()` leak).

---

## Implementation Notes

*Derived from ADR-0001 Implementation Guidelines (section 4.1, 4.2, Validation Criteria S1) and GDD R6 "Pawn-level link":*

- This is a harness test story. It builds the modified-client dump hook and the fixed-seed layout, then asserts what R6 requires: `APawn::PlayerState`, `APawn::Controller` and `AActor::Owner` are absent from every non-owner changelist for player and NPC pawns alike. Per ADR-0001 section 4.1 the test is "absent from the changelist", not "resolves to null": a player pawn's `Owner` is an owner-only PlayerController, so a non-owner would otherwise receive an unmappable NetGUID where an NPC pawn's value is null, and that difference is a tell even when both resolve to null. The same holds for an AI pawn's server-only AIController `Owner`, so check both kinds.
- Dump both views, as S1 did (`production/qa/spikes/2026-10-25_S1-S2-S14.md`): the resolved value on the receiving client and the raw received property bunch (send side: `SendProperties_r` parent indices per connection; receive side: `ReceiveProperties_r` parent indices, decoded through the `ClassReps` index). Property-level logging on the legacy driver is acceptable, as in S1, but record that it is not a byte-exact bunch comparison.
- The dump also lists every PlayerController the client received (must be only its own) and every teammate entry in its C-TeamView (only its own team, R6; entries come from Story 037's `FTeamView`). The owner's own pawn is the one pawn allowed to carry the three pointers.
- Scripted events (R9, R11): spawn, kill, informant adoption (crowd adoption, `BindPawn` on an Unbound pawn), disconnect hand-over (the `PawnLeavingGame` override; spike S8 has not reported, so drive it with the available hand-over path and do not assert S8-only behaviour here), rejoin. After each event the dump is repeated.
- Gating runs use fixed net emulation (PktLag 100 ms, no variance, PktLoss 0 %); each check waits on a condition with a timeout counted in server frames; a separate non-gating [Soak] run at PktLoss 2 % and +-20 ms is advisory. Assert and record that the active replication system is legacy (ADR-0001 section 7; ADR-0008).
- Negative control (ADR-0008, manifest): the suite must also run once with the three resets withheld (for example the spike's `stealth.Spike.IdentityTrims` bit 1 off, or an equivalent harness-only toggle behind `WITH_STEALTH_TEST_SEAMS`) and detect the leak, so the gate is shown not to be vacuous.
- Open point: the QA report flags "received pawn count equals 24 at every dump" as undefined against the scripted spawn and kill events (does a killed body persist, does spawn precede settling). Pin the expected count per event before implementing the assertion; until then treat the 24 as the settled count and record the per-event count in the test fixture.
- Open point: "identical in kind" for raw bunches needs a measurable definition. Use: a property is present or absent from the changelist, and for any present NetGUID property, mapped versus unmapped, identical across player and NPC pawns.
- Open point: the plan does not tag this story [Dep:Shared Character], but the three resets live on `AStealthPawn` (ADR-0001 section 4.1; shared-character-locomotion epic). Until `AStealthPawn` is in the production module the harness can only be validated against the spike configuration; confirm the sequencing with the producer.
- Prototype disposition: retires the prototype `IsNpc()` leak (`AStealthCharacter::IsNpc()` in `Source/StealthGame/Private/StealthCharacter.cpp`, derived from PlayerState presence and used by `StealthHUD.cpp` and `StealthGameMode.cpp`); that code is replaced, not ported (ADR-0007 prototype promotion rule). `Source/StealthGame/Private/Spikes/StealthIdentitySpike.*` is reference only for the dump method and is never promoted.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 040 (PlayerState replicated allowlist): the PlayerState property allowlist and engine-default treatment table.
- Story 037 (Maximal C-TeamView net-serializes in at most 96 bytes): the `FTeamView` struct and `NetSerialize`.
- Story 045 (Opponents receive nothing but allowlisted movement updates after a kill): per-kill replicated-update diff, PlayerState and PlayerController update absence.
- Story 049 (Player and NPC pawns replicate identically): full pawn parity (rates, dormancy, subobjects, `RemoteViewPitch16`, AnimBP `IsLocallyControlled` branch).
- Story 044 (Public player list is identical before and after eliminations and disconnects): player-list widget dump.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthTestHarness/Private/Roster/StealthRosterPawnPointerDumpTest.cpp` (CQTest `NETWORK_TEST_CLASS(PawnPointerDump, "StealthGame.Roster.PIE")`, exempt from the `F<System><Scenario>Test` class-name rule per `.claude/rules/test-standards.md`)
- `Source/StealthTestHarness/Private/Roster/StealthRosterPawnDumpHook.h` and `.cpp` (modified-client dump hook; unique `Stealth` prefixed names per ADR-0007)
- `Source/StealthTestHarness/Private/Roster/StealthRosterLeakLayout.h` (fixed-seed 4 players + 20 NPCs layout fixture)
- Test seams only behind `StealthTestHarness` or `WITH_STEALTH_TEST_SEAMS`, never compiled into Shipping.

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation. (At tiers where the QL-STORY-READY gate is skipped — `minimal`, or `lean`/`solo` review mode — no qa-lead specs exist, and this section reads: N/A — no qa-lead specs at this tier; implement against the Acceptance Criteria above.)*

*Test cases not yet defined — run /qa-plan to generate them.*

**Flagged gap (QL-STORY-READY: GAPS)**: "received pawn count equals 24 at every dump" conflicts with the scripted "spawn" and "kill" events; the AC does not say whether a killed body persists or whether spawn comes before settling. "Identical in kind" for raw bunches is undefined. Needs the count per event and a measurable "kind" (NetGUID mapped or unmapped, property present).

**Provisional spec (qa-lead, pending gap resolution)**:

- **AC-1**: non-owned pawns carry null PlayerState, Controller and Owner.
  - Given: a fixed-seed layout with 4 player pawns and 20 NPCs in cull radius, a modified client dumping resolved and raw values.
  - When: it dumps after settling and after each scripted event (spawn, kill, informant adoption, hand-over, rejoin).
  - Then: all three pointers null on non-owned pawns, raw contents match in kind, only its own PlayerController received, its C-TeamView lists only its own team. Pawn count 24 (per the clarified event rules).
  - Edge cases: S1 passed for the three pointers.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: automated test `Source/StealthTestHarness/Private/Roster/StealthRosterPawnPointerDumpTest.cpp` (`StealthGame.Roster.PIE.PawnPointerDump`) OR a documented playtest; `qa.level` is full, so the automated test is required.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/participant-state-roster/story-040-player-state-replicated-allowlist.md`, `production/epics/participant-state-roster/story-037-team-view-serialized-size.md`
- Cross-epic: test harness per ADR-0008; the pawn-pointer resets live on `AStealthPawn` (`production/epics/shared-character-locomotion/EPIC.md`, see Open point)
- Unlocks: None
