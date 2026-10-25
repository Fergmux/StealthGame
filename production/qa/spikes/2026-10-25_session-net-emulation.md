# Session net-emulation spike: packet rate, burst outages, RTT and map load under StealthLeak1 and StealthClean, 2026-10-25

> **Story:** `production/epics/session-travel/story-002-net-emulation-measurement-spike.md` (TR-session-006; GDD Open Question 4, F3/F4/F7)
> **Engine:** UE 5.8.4 Development **editor binary in `-game`** (uncooked), legacy `IpNetDriver`, every process capped at 60 fps, one machine
> **Verdict:** **PASS with CONCERNS** — two valid 30-minute sessions (5 min lobby, one seamless round travel, ~25 min active round with 3 moving players; all 3 clients stayed connected for the whole session in both runs). Every Session default is **confirmed**: SessionSoftTimeoutSec 3, SessionHardTimeoutSec 12, RejoinWindowSec 120, TravelLoadTimeoutSec 60, TravelAckGraceSec 15. **CONCERNS:** loopback packet emulation cannot produce Wi-Fi bursts (Bernoulli loss only), so `P_burst` for the Hard bound still needs the Wi-Fi playtest; load times are uncooked-editor times; the client packet rate never fell to the GDD's idle-lobby 5/s (it stays at the frame rate).

**Run validity** (adopted for this story, readiness fix): all 3 clients connected from join to the host's end in both runs (client end reason "server connection lost" = the host's scripted close at 1800 s), the travel completed, and every packet id matched across processes (one-way latency computed for 99 % of sent packets; the rest are the emulated losses).

## Measurements

Packet-level timestamps on every process (`UStealthTimingIpConnection`: flush time at the sender, processing time at the receiver, matched by packet id; one machine, one QPC clock), raw engine RTT samples (`APlayerController::UpdatePing`), and the server's receive gaps per client (what Session's silence clock sees).

| Quantity | StealthLeak1 (PktLag 100 ms, 1 % loss, both sides) | StealthClean |
|---|---|---|
| **R_c** client→server packets/s, lobby (idle, pawns standing) | 59.3-59.5 | 57.8 |
| R_c, round (moving) | 59.3-59.4 | 47.6 (host frame-bound: mean host frame 20.7 ms) |
| R_c, during the round travel (minimum) | 9.6-10.3 | 7.6-9.1 |
| **RTT_p99** (engine samples, lobby / round) | 265.7 / 263.4 ms (median 232-246) | 45.1 / 58.6 ms |
| One-way server→client p99 (round) | 133.0 ms (median 117) | 27.4 ms |
| One-way client→server p99 (round) | 133.0 ms | 27.9 ms |
| Loss, client→server (round) | 0.95-1.08 % | 0 |
| **Burst-outage distribution** (runs of consecutive lost client→server packets, round, 3 clients, 25 min) | 1: 2688, 2: 24, 6: 2, 7: 1 (longest 7 packets ≈ 117 ms) | none (3 single-id reorders) |
| Server receive gap per client, outside travel (max) | 83 ms; 0 gaps > 250 ms | 188 ms; 0 gaps > 250 ms |
| Server receive gap during the travel (max) | 1.44 s (28 gaps > 0.25 s, 3 > 1 s, 0 > 2 s) | 1.18 s (3 > 1 s, 0 > 2 s) |
| **T_srvload** (ServerTravel → PostSeamlessTravel) | 6.43 s | 6.59 s |
| **t_load** seamless (client travel start → acknowledgement) | 7.21-7.32 s | 7.06-7.08 s |
| t_load full map (S4 rejoin, cold `L_Prototype` load) | 14.8 s; join handshake + spawn ≈ 2.3 s (run s4-b) | — |
| Hard-travel reconnect (S3 `s3-d`, server ready → client ack) | 14.2-15.8 s after the server's 19.3 s reload | — |
| Listen-host frame (60 fps cap): mean / p99 of per-second p99 / max | 16.95 / 28.7 / 886 ms (crowd spawn and travel) | 20.7 / 32.0 / 881 ms |

## Defaults: confirmed or revised (F3, F4, F7)

| Knob (default) | Rule | Measured inputs | Result |
|---|---|---|---|
| `SessionSoftTimeoutSec` (3) | F3: ≥ 2·RTT_p99 + 1 s and ≥ 3 / R_c | 2·0.266 + 1 = **1.53 s**; 3 / 9.6 = 0.31 s (lowest R_c, during travel; Session's clock is suspended while Traveling anyway) | **Confirmed** (3 ≥ 1.53) |
| `SessionHardTimeoutSec` (12) | F3: exceed the longest measured burst × 1.5; validation Hard ≥ Soft + 2 | longest outage outside travel 0.19 s; during travel 1.44 s → 2.2 s | **Confirmed** (Wi-Fi bursts not measurable here: CONCERN) |
| `RejoinWindowSec` (120) | F4: ≥ M·(t_restart + t_connect + t_load + A) + T_busy | 1.5 · (30 [playtest default] + 2.3 + 14.8 + 3) + 6.4 = **81.6 s** | **Confirmed** (120 ≥ 81.6) |
| `TravelLoadTimeoutSec` (60) | F7: covers the client load from travel start | seamless 7.3 s; hard-travel reconnect 35.2 s (S3) | **Confirmed** |
| `TravelAckGraceSec` (15) | F7: time every client needs after `t_ready` | seamless: clients ack 0.8-0.9 s after the server's map is ready (S3: up to 2.5 s); hard travel 14.2-15.8 s, but `Deadline = max(t_travel + 60, t_ready + 15)` = 60 s there | **Confirmed** (note: on a hard travel the grace term alone would be tight; the 60 s term carries it) |
| `T_srvload` range (1-20 s) | F7 input | 6.4-6.6 s seamless; 19.3 s hard (uncooked) | inside the range |

No knob change request is raised (TDC knob registry unchanged).

## Findings

1. **The client packet rate is frame-driven, not keepalive-driven**: an idle client with a pawn sends ~58-59 packets/s at 60 fps (GDD F3 assumed ≥ 5/s in an idle lobby, from the 0.2 s keepalive). F3's `3 / R_c` term is therefore far below Soft in every phase; it only matters during a map load (R_c ≈ 8-10/s), when Session's clock is suspended. The GDD's "R_c = 5 in Lobby" example is pessimistic, not wrong.
2. **Emulated loss is independent per packet**: bursts are mostly single packets; the longest run was 7 packets (≈ 117 ms) in 75 min of client-time. A Hard of 12 s has a > 100× margin over everything measured on loopback; real false-timeout risk (F3 `P_burst`) remains a Wi-Fi playtest question (story 068).
3. **Map loads** (uncooked editor binary): ~6.5 s server, ~7.2 s client for the seamless travel, ~15 s for a cold client load; cooked Development builds will be faster, so these are upper estimates for F4/F7.
4. Listen-host frames hitch to ~0.9 s at crowd spawn/travel; outside those the per-second p99 stays ≤ 32 ms, below `LivenessMaxCountedFrameSec` 0.1 s (F2).

## Setup (reproduction)

```
powershell -File tools/dev/cycle.ps1 -NoLaunch
powershell -File tools/ci/mp/run-mp.ps1 -Scenario Soak -Clients 3 -ServerProfile StealthLeak1 -ClientProfile StealthLeak1 -NpcCount 12 `
  -PacketTiming -MaxFps 60 -ServerUrlOptions '?game=/Script/StealthTestHarness.StealthSpikeSessionGameMode' -IdentityKeys -Duration 1800 `
  -ExtraArgs '-StealthHarnessLobbySeconds=300 -StealthHarnessMaxSeconds=2100 -ini:Game:[/Script/StealthGame.StealthSettings]:bAutoStartRound=False' -RunId soak-l1
(same with StealthClean, -RunId soak-cl)
python -I tools/ci/mp/analyze_net.py Saved/Harness/soak-l1 --json <out>
```

- Scenario `Soak` (`Source/StealthTestHarness/Private/Scenarios/StealthHarnessScenarioSession.cpp`): 300 s lobby (prototype pawns standing, no round), seamless round travel through the S3 spike game mode (acknowledged before the round's first tick), then an active round with the prototype crowd; each client walks a deterministic random heading that changes every 3 s.
- Measurement code: `Source/StealthTestHarness/Private/Spikes/Session/StealthSpikeMeasure.{h,cpp}`, `StealthTimingIpConnection.h` (enabled by `-StealthPacketTiming`, which points the IP net driver defaults at the timing connection class); analyser `tools/ci/mp/analyze_net.py`.
- Wall-clock time is used only as the run length and failure ceilings; the numbers above are measurements, not pass predicates.

## Limits

- One machine, loopback, emulated lag/loss (Bernoulli), 3 clients (not 9); uncooked editor binary.
- `t_restart` (relaunching after a crash) is the GDD's playtest value (30 s), not measured here.

## Evidence

- `production/qa/evidence/story-002-net-emulation-measurement-spike/` — `soak-l1.analysis.json`, `soak-cl.analysis.json` (per phase and connection), launch lines. Raw records (≈ 60 MB per run) stay in `Saved/Harness/soak-l1|soak-cl` on the run machine.
