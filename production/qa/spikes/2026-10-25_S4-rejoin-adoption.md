# Spike S4: a retained PlayerState is adopted on rejoin with no PlayerState channel or array change on other clients, 2026-10-25

> **Story:** `production/epics/session-travel/story-004-spike-s4-rejoin-playerstate-adoption.md` (TR-session-006, TR-session-007)
> **Governs:** ADR-0005 §1 and §5 (E9, E10, E15, E20), Validation Criterion S4, Session GDD OQ2/OQ11 · **Engine:** UE 5.8.4 Development editor binary in `-game`, legacy `IpNetDriver`, StealthLeak1 on every process, ADR-0001 trims 3 (pawn pointers and PlayerState trims owner-only)
> **Verdict:** **PASS (CONCERNS recorded)** — listen host + 3 client processes; C1 left mid-round and rejoined with its `-IdentityKey=`. On both non-owner clients the raw-bunch capture shows **0** PlayerState actor-channel opens or closes, **0** player-array changes and **0** `ClientReceiveLocalizedMessage`; no client received an engine entered/left/renamed message in any phase; the three distinct keys never superseded each other (0 supersedes) while the supersede control (C3 joining with C1's key) produced exactly 1. **Adoption in `InitPlayerState` works** (same PlayerState NetGUID before and after); the fallback (spawn then destroy in the same frame) is not needed. **CONCERN:** the adoption produces one property-less PlayerState bunch (Role/RemoteRole only, 41 bits) on every non-owner ~0.19 s later — a timing trace of the rejoin (finding 2).

## Questions and answers (run `s4-b`; `s4-a` repeats Q1-Q4)

| # | Question | Answer | Result |
|---|---|---|---|
| Q1 | PlayerState actor-channel open/close on a non-owner during the rejoin | C2: 0, C3: 0 (rejoiner's PlayerState and any PlayerState), window = drop − 0.5 s … rejoin connected + 5 s (38.9 s) | **PASS** |
| Q2 | Player-array change on a non-owner | 0 in the window on C2 and C3; the array held the same 4 NetGUIDs from the last initial join to the end of the run (also through the supersede control) | **PASS** |
| Q3 | `ClientReceiveLocalizedMessage` on any client, any phase | 0 RPC receipts in the capture on C2/C3, 0 on the owner, and 0 calls of the controller override on every client over the whole run (joins, rename to C<n>, drop, rejoin, supersede) | **PASS** |
| Q4 | Two clients on one machine with distinct `-IdentityKey=` never supersede each other | 0 supersedes over the run with 3 distinct keys; control: C3 rejoining with C1's key → exactly 1 supersede (and the superseded PlayerState was re-attached to C3's new controller, same NetGUID) | **PASS** (control detected) |
| Q5 | Does adoption in `InitPlayerState` work, or is the fallback needed? | Works: `Login` stashed the retained PlayerState, the new controller's `InitPlayerState` adopted it (`SetPlayerState`, `SetOwner`, `OnReactivated`); bind records show the same PlayerState NetGUID (18) for the first login, the rejoin and the supersede; `InitNewPlayer` skipped re-registration and rename | **Adoption works; fallback not needed** |
| — | Properties a non-owner received for the rejoiner's PlayerState in the window | Role, RemoteRole only (one 41-bit bunch at adoption + 0.19 s); baseline window: name and team at join | **CONCERNS** (finding 2) |

**Rejoin time** (C1, uncooked editor binary): `disconnect` → standalone default map → `Rejoin` issued after 5 s + map load (16 s after the drop); issue → connected and ready 17.1 s (handshake plus a ~14 s uncooked `L_Prototype` load). Load times for F4 come from story 002's report.

## Findings

1. **ADR-0005 §5's adoption path holds in 5.8.4**: no PlayerState is created on rejoin, so nothing opens or closes on other clients and the array is untouched. The same path serves supersede (the old controller's PlayerState is detached first, so its later destruction neither logs out nor deactivates the PlayerState).
2. **CONCERN — role-only PlayerState bunch at an ownership change.** When the retained PlayerState gets its new owner, every non-owner receives one PlayerState bunch carrying only Role/RemoteRole (no value changes: 41 bits), ~one-way latency after the adoption. The same shape appears for any PlayerState whose controller changes (e.g. C3's own PlayerState when it left for the control). Non-owners therefore see *that something happened* to a PlayerState at the rejoin instant (a timing trace, not identity: PlayerStates are not secret). Candidate leak-catalogue entry for IVP story 047; the production stories should check whether keeping the owner unchanged at deactivation (or a different adoption order) removes it, and the Visibility Policy decides whether it matters (the drop itself produced no bunch).
3. `GNetConnectionBeingCleanedUp` is not exported; `Logout` finds the participant through the PlayerState's bound record (ST-001 finding 1).
4. Keys survive relative travel and rejoin URLs; the null online subsystem replaces `?Name=` (ST-001 finding 3).

## Setup (reproduction)

```
powershell -File tools/dev/cycle.ps1 -NoLaunch
powershell -File tools/ci/mp/run-mp.ps1 -Scenario S4 -Clients 3 -ServerProfile StealthLeak1 -ClientProfile StealthLeak1 -NpcCount 8 -Capture -Trims 3 `
  -ServerUrlOptions '?game=/Script/StealthTestHarness.StealthSpikeSessionGameMode' -IdentityKeys `
  -ExtraArgs '-StealthHarnessMaxSeconds=400 -ini:Game:[/Script/StealthGame.StealthSettings]:bAutoStartRound=False' -RunId s4-b
python -I tools/ci/mp/analyze_s4.py Saved/Harness/s4-b --json <out>
```

- Scenario `S4` (`Source/StealthTestHarness/Private/Scenarios/StealthHarnessScenarioSession.cpp`): the host starts the round when all 3 clients joined; C1 leaves 12 s after it sees the round (the engine `disconnect` path via `FStealthConnectionEventHook::Drop(ClientQuit)`), waits on its default map, then `Rejoin`s with its own key; C3 issues the supersede control 55 s into the round; the host ends at 100 s.
- Spike game classes as in S3 (`Spikes/Session/StealthSpikeSession.{h,cpp}`): mid-round `Logout` keeps the PlayerState (`OnDeactivated` does not destroy) and the body (`PawnLeavingGame` unpossesses); the rejoin re-possesses the body.
- Raw-bunch capture: the ADR-0008 harness channel hook (`-StealthNetCapture`, `LogRepProperties VeryVerbose`), plus a `playerArray` record whenever a client's array changes and a `localizedMessage` record from the spike controller's override.

## Limits

- One machine, 3 clients, one mid-round rejoin per run (two runs); the prototype pawn and game mode with the spike classes, not the production Session/Roster.
- The rejoin is a clean leave (socket close); a silent drop followed by Hard timeout is the same adoption path but was not run here (the hook's `Silent` mode supports it).

## Evidence

- `production/qa/evidence/story-004-spike-s4-rejoin-playerstate-adoption/` — `s4-b.analysis.json`, `s4-a.analysis.json`, gzipped records and launch lines of both runs.
