# Spike S3: stores and team views survive seamless travel in multi-process standalone, 2026-10-25

> **Story:** `production/epics/session-travel/story-003-spike-s3-seamless-travel-persistence.md` (TR-session-003)
> **Governs:** ADR-0005 §2, §4, §5 (E5, E19, E21), Validation Criterion S3, Session GDD Open Question 1, Roster AC-PSR-40a · **Engine:** UE 5.8.4 Development editor binary in `-game`, legacy `IpNetDriver`, StealthLeak1 on every process
> **Verdict:** **PASS** — listen host + 2 client processes, two seamless round travels (plus a third with a disconnected participant and one hard travel): at each round's first tick every one of the four game-instance stores equalled its pre-travel content, and every participant had acknowledged its owner-only C-TeamView with the server's hash. **Same-map travel through the transition map works in 5.8.4** (`L_Prototype` → `/Engine/Maps/Entry` → `L_Prototype`), so `LobbyMap` and `MatchMap` need not be distinct maps. **Hard travel was exercised as the fallback** and also kept every store, once Session treats the connection closes of a hard travel as travel (finding 3).

## Questions and answers

| # | Question | Answer (run `s3-d`, TransitionMap = `/Engine/Maps/Entry`) | Result |
|---|---|---|---|
| Q1 | After two seamless travels, does every game-instance store equal its pre-travel value? | Travels 1 and 2: Session, Roster, Round and Tuning-preset stand-in stores byte-identical (`pre<n>` vs `post<n>` content strings) at the first round tick. Also identical across travel 3 (C2 disconnected) and travel 4 (hard) | **PASS** |
| Q2 | Does every client's C-TeamView equal the server's before the first round tick? | The server waits for an owner-only acknowledgement carrying the CRC of the replicated `FStealthSpikeTeamView` from every participant expected back (host locally); all 3/3 (travels 1, 2, 4) and 2/2 (travel 3) matched the server's hash before the tick; no ceiling hit | **PASS** |
| Q3 | Same-map travel through a transition map in 5.8.4, or distinct LobbyMap/MatchMap from day one? | Same map works: 3 seamless same-map travels per run in 4 runs (`s3-a`..`s3-d`). With TransitionMap empty the engine creates an unnamed dummy world (`/Temp/Untitled_1`) and that also worked (`s3-c`); `s3-d` uses the configured `/Engine/Maps/Entry` (logged: `Bringing World /Engine/Maps/Entry.Entry up for play`) | **Same map OK** |
| Q4 | Was hard travel exercised as the fallback? | Yes, travel 4 (`bUseSeamlessTravel = false`): clients received `ClientTravel`, reconnected with the same `?IdentityKey=` (relative travel keeps the URL options), were re-bound by key in `PostLogin`, acknowledged, and the stores matched | **Exercised** |
| E5 | Controller and PlayerState replaced on seamless arrival | Every participant got a new controller and PlayerState (new NetGUIDs) at each seamless travel; the view was rewritten from the Roster store | **Confirmed** |
| E19 | Remote controllers arrive after the host | The host was re-bound in `PostSeamlessTravel` first in every travel; remote arrivals 0.06-0.6 s later through `HandleSeamlessTravelPlayer` | **Confirmed** |
| E21 | PlayerStates of disconnected participants are carried | C2 was closed by the server mid-round before travel 3; its retained PlayerState (`OnDeactivated` kept it) was carried into the new world without a controller (`bFromPreviousLevel = 1`), and C2's later rejoin **adopted that same PlayerState** (same NetGUID) after the travel | **Confirmed** |

## Measurements (`s3-d`, uncooked editor binary, 3 processes on one machine)

| Travel | Kind | ServerTravel → PostSeamlessTravel (T_srvload) | ServerTravel → first round tick | Client travel → acknowledgement |
|---|---|---|---|---|
| 1 | seamless | 6.08 s | 6.99 s | C1 6.40 s, C2 6.56 s |
| 2 | seamless | 5.75 s | 8.55 s | 6.15 s, 8.27 s |
| 3 | seamless, C2 absent | 6.18 s | 6.87 s | C1 6.46 s; C2 rejoined +43.6 s, adopted |
| 4 | hard | 19.35 s (to the new world's arrival) | 35.40 s | 35.18 s, 33.51 s |

No client received any engine entered/left/renamed message (spike PlayerState returns `ShouldBroadCastWelcomeMessage` false; 0 `localizedMessage` records). The four earlier runs (`s3-a`..`s3-c`) were development runs; `s3-c` repeats every result above with an empty TransitionMap.

## Findings for ADR-0005 and the Session/Roster stories

1. **Same-map seamless travel works in 5.8.4** with the configured transition map and even without one; the provisional same-map travel (Session OQ1, story 053) stands. The project now sets `TransitionMap=/Engine/Maps/Entry.Entry` (engine content, no project asset; cooked automatically as a GameMapsSettings map).
2. **Re-binding by key works on both paths**: `HandleSeamlessTravelPlayer` (seamless; the connection survives, so the key is read from `UNetConnection::RequestURL`) and `PostLogin` (hard; a fresh login with the same options). `RequestURL` is the full URL (`/Game/…?Name=…?IdentityKey=…`), so it must be parsed with `FURL`, not `UGameplayStatics::ParseOption` (the first development run bound nobody because of this).
3. **A hard travel closes every client connection and `Logout` runs for each before the server's map switch** (not during world teardown). Unless Session suspends connection-state changes while a hard (fallback) travel is in progress, every participant is marked Disconnected and its generation bumps, so the Session store differs after the travel (run `s3-b`). The spike sets a `bHardTravelInProgress` flag from the travel start to the round's first tick; with it the store is unchanged. This belongs in the fallback-travel story (rule 11.9) and ADR-0005 §4 ("Fallback travel reuses the same path"): Session must treat closes during its own hard travel as travel, not leave.
4. Retained-PlayerState adoption across a seamless travel works (E21 + §5 together).
5. `Logout` runs after the controller lost its connection (E20); the key is recovered through the PlayerState's bound participant (and `GNetConnectionBeingCleanedUp` is not exported, see ST-001).

## Setup (reproduction)

```
powershell -File tools/dev/cycle.ps1 -NoLaunch
powershell -File tools/ci/mp/run-mp.ps1 -Scenario S3 -Clients 2 -ServerProfile StealthLeak1 -ClientProfile StealthLeak1 -NpcCount 8 `
  -ServerUrlOptions '?game=/Script/StealthTestHarness.StealthSpikeSessionGameMode' -IdentityKeys `
  -ExtraArgs '-StealthHarnessMaxSeconds=780 -ini:Game:[/Script/StealthGame.StealthSettings]:bAutoStartRound=False' -RunId s3-d
python -I tools/ci/mp/analyze_s3.py Saved/Harness/s3-d --json <out>
```

- Throwaway spike code in the ADR-0005 shape: `Source/StealthTestHarness/Private/Spikes/Session/StealthSpikeSession.{h,cpp}` — four `UGameInstanceSubsystem` stand-in stores (created only in harness processes), `AStealthSpikeSessionGameMode` (seamless travel on; `Login` stashes a pending reclaim, the controller's `InitPlayerState` adopts it; `HandleStartingNewPlayer` re-binds from the stores; `PostSeamlessTravel` records T_srvload and carried PlayerStates), `AStealthSpikePlayerState` (owner-only `TeamView`, no welcome messages, retained mid-round), `AStealthSpikePlayerController` (`ServerSpikeAck` owner-only reliable RPC).
- Scenario `S3` in `Source/StealthTestHarness/Private/Scenarios/StealthHarnessScenarioSession.cpp`: lobby 5 s → travel 1 → round 8 s → travel 2 → round 8 s → server closes C2 → travel 3 → round held 40 s (C2 rejoins at +30 s) → hard travel 4 → round 8 s → end. Each travel bumps the Roster epoch and snapshots every store first; the round's first tick waits for all expected acknowledgements (120 s failure ceiling, never reached).
- Runner changes (`StealthHarnessRun.{h,cpp}`): a client may tolerate an expected disconnect (rejoin, hard travel) and is told about each new connection; an offline ceiling ends a client that never comes back.
- Clients log in with `-IdentityKeys`: `?IdentityKey=<slot hex8>5374656c0000000000000000`.

## Limits

- One machine, uncooked editor binary (load times are not cooked-build load times; the net-emulation spike, story 002, reports map loads under its own run).
- Stand-in stores, not the production Session/Roster/Round/Tuning stores; the spike proves the engine mechanism and the ADR-0005 shape, the production stores re-prove it (Roster AC-PSR-40a, story participant-state-roster/058).
- The prototype game mode and pawns (pawns respawned by the prototype each round).

## Evidence

- `production/qa/evidence/story-003-spike-s3-seamless-travel-persistence/` — `s3-c.analysis.json`, `s3-d.analysis.json`, gzipped record files and launch lines of both runs.
