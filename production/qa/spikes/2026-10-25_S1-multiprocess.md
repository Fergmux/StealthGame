# Spike S1 (multi-process): no identity pointer reaches a non-owner, 2026-10-25

> **Story:** `production/epics/information-visibility-policy/story-001-spike-s1-multiprocess-raw-bunch-capture.md`
> **Governs:** ADR-0001 §4.1 condition S1 (multi-process half), ADR-0008 §6 (capture mechanism) · **Engine:** UE 5.8.4 Development editor binary in game mode, legacy `IpNetDriver` (Iris off, asserted per process)
> **Verdict:** **PASS** — `APawn::PlayerState`, `APawn::Controller` and `AActor::Owner` reached **0** non-owner changelists (server send side) and **0** steady-state non-owner receipts (client receive side), for player and NPC pawns, across spawn, takeover, AI hand-over and kill. **Negative control detected** (PlayerState at `COND_None`: 35 non-owner sends, 18 steady non-owner receipts). Mechanism chosen and recorded as an ADR-0008 amendment. Non-pointer differentiators re-measured (recorded, not gated): **both still differ** (expected until ADR-0003 §2 / SCL-005 follow-ups land on the production pawn).

## Questions and answers

| # | Question (AC) | Answer | Result |
|---|---|---|---|
| Q1 | Do PlayerState, Controller or Owner appear in any non-owner changelist, for either pawn kind, across spawn, takeover and AI hand-over, multi-process under StealthLeak2? | No. Server: 0 non-owner sends of any of the three (66 k NPC + 6.7 k player non-owner pawn bunches). Clients: 0 steady-state non-owner receipts on C1, C2 and C3 (73 k pawn bunches received). | **PASS** |
| Q2 | Does the same capture detect the negative control (pawn PlayerState at `COND_None`)? | Yes. Server: 28 player + 7 NPC non-owner sends of `Pawn.PlayerState`; every client received it in steady state (6 each) plus in takeover transitions; Controller and Owner stayed owner-only in the same run. | **PASS** (control detected) |
| Q3 | Which capture mechanism wins (NetTrace analysis or a harness-only receive hook)? | A harness-only `UActorChannel` receive/send hook (`UStealthCaptureActorChannel`) bracketing the engine's own replication log points. Recorded as an ADR-0008 amendment. | **Decision** |
| Q4 | Byte level where the mechanism allows | Per-bunch payload bit length is captured exactly (receive and send). Per-property bit length and property values are not exposed by the engine log points; the property set, order and condition are. | **CONCERNS** (bunch-level bytes only; see limits) |
| Q5 | Re-measure RemoteViewPitch16 and CharacterInputState | Both still player-only on the wire: RemoteViewPitch16 4,175 non-owner receipts for player pawns vs 0 for NPCs; CharacterInputState 58 vs 0. Recorded, not gated. | Recorded (still differ) |

## Setup (reproduction)

- Build: `tools/dev/cycle.ps1 -NoLaunch` (StealthGameEditor Win64 Development).
- Run (one machine; a second machine was not available):
  ```
  powershell -File tools/ci/mp/run-mp.ps1 -Scenario S1 -Clients 3 -ServerProfile StealthLeak2 -ClientProfile StealthLeak2 -Capture -Trims 1  -NpcCount 12 -RunId s1-trims-b
  powershell -File tools/ci/mp/run-mp.ps1 -Scenario S1 -Clients 3 -ServerProfile StealthLeak2 -ClientProfile StealthLeak2 -Capture -Trims 17 -NpcCount 12 -RunId s1-negctl-b
  python -I tools/ci/mp/analyze_s1.py Saved/Harness/s1-trims-b  --json <out>
  python -I tools/ci/mp/analyze_s1.py Saved/Harness/s1-negctl-b --json <out>
  ```
- Processes: 1 listen host + 3 clients, each `UnrealEditor.exe StealthGame.uproject -game -nullrhi` (4 processes, real UDP on loopback). `-PktEmulationProfile=StealthLeak2` on **every** process (PktLag 100 ms each way, 2 % loss of each process's outgoing packets; measured RTT 215 ms median, p90 218 ms).
- Trims: `-StealthHarnessTrims=1` sets `stealth.Spike.IdentityTrims` = 1 (ADR-0001 §4.1: the three pointers at `COND_OwnerOnly` on the prototype pawn, condition-only reset); the negative control is 17 (= 1 + new bit 16, which puts `APawn::PlayerState` back to `COND_None`). The server's layout record confirms the conditions in force (`Pawn.PlayerState` OwnerOnly vs None; Controller and Owner OwnerOnly in both).
- Scenario `S1` (`Source/StealthTestHarness/Private/Scenarios/StealthHarnessScenarioS1.cpp`), server time after the crowd (12 NPCs) exists: +1 s C1's and C2's pawns and the two nearest NPCs are placed 4-6 m in front of C3 (the passive non-owning observer); +6 s C1 takes over the nearest crowd pawn (its former body is handed to that NPC's AI controller); +12 s C2 takes over; +18 s re-placement in view; +24 s C1 kills an NPC (`strike_npc`, real server rule path; a replacement NPC spawns later); +30 s C1 takes over again; +40 s end. C1 and C2 feed real movement input, look (pitch) input and Enhanced Input walk/sprint injections (`IA_Walk`, `IA_Sprint`) all run long.
- Run length: 41.5 s after all three clients joined (40 s after the crowd existed); 34 distinct pawn NetGUIDs over the run (players' pre-round and round bodies, 12 NPCs, one replacement). Host frame rate 210-227 fps median (uncapped, `-nullrhi`).

## Capture mechanism (ADR-0008 §6) — the winner and why

**Harness-only receive/send hook:** `UStealthCaptureActorChannel` (`Source/StealthTestHarness/Private/Capture/StealthNetCapture.{h,cpp}`), swapped in for the `Actor` channel definition on every net driver class default when a process starts with `-StealthNetCapture` (no ini change; the harness module never ships, ADR-0007).
- Client: its `ReceivedBunch` override brackets the engine's receive path; inside the bracket an any-thread output device collects the engine's own log points from `FRepLayout::ReceiveProperties` (`ReceiveProperties: Owner=<class>` and `ReceiveProperties_r: Parent=N`, `LogRepProperties VeryVerbose`, compiled into Development) and `Received RPC` (`LogRepTraffic`). One record per bunch: channel, actor, NetGUID, payload bits, open/close/reliable flags, per replicated object the parent indices in order, RPC names.
- Server: the `Replicate <actor>, bNetInitial, bNetOwner` line opens a pending block; `SendProperties` / `SendProperties_r` lines fill it; the channel's `SendBunch` override attributes it to the connection (client name), with the bunch bits. RPC sends are recorded from `Sent RPC`.
- Each process also dumps every replicated class's layout (ClassReps index → property, lifetime condition) so parents decode per process; the server writes pawn truth every 0.1 s (NetGUID → player / NPC, by PlayerController) for the owner/non-owner join.

**Why not NetTrace:** it needs Unreal Insights' trace analysis to extract per-connection property data (no headless exporter in the installed 5.8.4 toolchain was found to give per-bunch property handles), and the trace's property names would still need the same layout mapping. The hook gives per-process records directly, runs in any non-Shipping build, and was validated by its own negative control (Q2) before the trims result was trusted.

**Per-actor record vs ADR-0008 §6 list:** class ✓, property handles and order ✓, bit-length class ✓ (bunch level), condition ✓ (layout records), components/subobjects ✓ (no replicated subobject appeared on the pawn), RPC set ✓. **NetGUID class (null / mapped / unmappable) of a pointer's value: not captured** — the log points carry the handle, not the value. Production stories that need it (AC-IVP-02's NetGUID-class column) must add a value decode (e.g. a `FNetBitReader` replay in the hook) or rely on absence (which is what S1 needs).

## Results

**Trims (run `s1-trims-b`, identity trims 1):**

| Side | Pawn bunches (non-owner / owner) | Identity pointers to non-owners |
|---|---|---|
| Server send (`tx`) | NPC 65,903 / 3; player 6,659 / 22 | **0** (owner sends: PlayerState, Controller, Owner 6 each to players' own bodies, 3 each at the takeover re-delivery of a crowd body) |
| C1 receive | 21,344 NPC + 1,636 player non-owner; 409 owner | **0 steady**; 2 each in a takeover transition (C1 is the new owner, E12 re-delivery) |
| C2 receive | 21,417 NPC + 1,632 player non-owner; 400 owner | **0 steady**; 1 each in its takeover transition |
| C3 receive (observer) | 21,344 NPC + 3,254 player non-owner; 391 owner | **0** |

A receipt is "transition" when the receiving client owned that pawn within ±0.6 s of an ownership change (truth sampled every 0.1 s, one-way lag 0.12 s); all identity receipts outside a client's own bodies are 0.

**Negative control (run `s1-negctl-b`, trims 17):** server non-owner sends of `Pawn.PlayerState`: 28 (player) + 7 (NPC bodies at hand-over); clients' steady non-owner receipts of `Pawn.PlayerState`: C1 8 (6 steady), C2 10 (6 steady), C3 12 (6 steady). Controller and Owner: 0 to non-owners in the same run. The capture detects the control.

**Full non-owner parent union (trims run), by server-truth kind:**

| Kind | Parents received by non-owners |
|---|---|
| NPC | RemoteRole, ReplicatedMovement, Role, Instigator, ReplicatedBasedMovement, ReplicatedMovementMode, bProxyIsCharacterMovementActive, AppearanceIndex, **bConcealed** (bush), **bDying** (kill) |
| Player | the same base set, **RemoteViewPitch16** (4,175), **CharacterInputState** (58), **HeldUntilServerTime** (2) |

**Bunch bits (non-owner, non-open pawn bunches):** NPC median 279 bits (p10 192, p90 306); player median 303 (p10 43, p90 333). The p10 of 43 bits is the RemoteViewPitch16-only update that NPCs never send — the two differentiators below also show at bunch level.

**Re-measure (recorded, not gated):** RemoteViewPitch16 still reaches non-owners only for player-controlled pawns (AI pitch never changes); `CBP_SandboxCharacter.CharacterInputState` (`COND_SkipOwner`) still only for players (set only by the owner's `UpdateInputState_Server` RPC on walk/sprint input). No RPC reached a non-owner for any pawn in steady state. Both are already scheduled: ADR-0001 §2 / ADR-0003 §2 write RemoteViewPitch16 from HeadLook for every pawn; the sample input state is removed or driven server-side for every pawn on the production pawn (ADR-0001 amendment 3 of the 2026-10-25 report).

## Limits

- One machine (two machines not available); 4 processes on loopback with emulated loss and lag on every process.
- Uncooked Development editor binary in `-game` mode, not a cooked Development build (Gauntlet and cook are story tdc-020/023). The replication code path is the same binary code.
- Prototype pawn (`BP_CrowdCharacter` on `AStealthCharacter`) with the spike trims; production re-proof on `AStealthPawn` is AC-IVP-02/37 (stories 044/019).
- Bunch-level byte accounting only (Q4). Per-property values and pointer NetGUID classes are not decoded.
- Log volume: `LogRepProperties VeryVerbose` writes ~300 MB of engine log per client per 40 s run; the capture records themselves are 7-25 MB per process (gzipped 0.2-0.7 MB).

## Evidence

- `production/qa/evidence/story-001-spike-s1-multiprocess-raw-bunch-capture/` — `s1-trims-b.analysis.json`, `s1-negctl-b.analysis.json`, gzipped record files of both runs (`*.jsonl.gz`), launch command lines.
- Code: `Source/StealthTestHarness/Private/Capture/StealthNetCapture.{h,cpp}`, `Source/StealthTestHarness/Private/Scenarios/StealthHarnessRun.{h,cpp}`, `StealthHarnessScenarioS1.cpp`; spike bit 16 in `Source/StealthGame/Private/Spikes/StealthIdentitySpike.{h,cpp}`; tools `tools/ci/mp/run-mp.ps1`, `harness_records.py`, `analyze_s1.py`.

## What this unblocks

The S1 condition of ADR-0001 is now reported for the multi-process case. Stories that waited on S1 alone become implementable on the S1 side; each still waits on its other gates (S9, S10, S13, S14, AStealthPawn) — see the story status notes.
