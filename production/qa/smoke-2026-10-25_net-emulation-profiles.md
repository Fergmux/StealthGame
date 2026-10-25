# Smoke record: net emulation profiles StealthLeak1, StealthLeak2, StealthClean (story tdc-017)

> **Story:** `production/epics/tuning-data-config/story-017-net-emulation-profiles.md` · **Date:** 2026-10-25 · **Engine:** UE 5.8.4 (Development editor binary, `-game -nullrhi`)
> **Result:** **PASS** — three profiles defined once each in `Config/DefaultEngine.ini` with the stated values; each loads by name through `FPacketSimulationSettings::LoadEmulationProfile` with the loaded values equal to the stated ones (a missing name is rejected); the measured RTT of both leak profiles is recorded in the comment beside each profile.

## Checks

| Check | Method | Observed | Result |
|---|---|---|---|
| Profiles exist, no duplicate section | `grep -c "^\[PacketSimulationProfile\." Config/DefaultEngine.ini` | 3 sections: `StealthLeak1`, `StealthLeak2`, `StealthClean`; each name once | PASS |
| 5.8 signature of the loader | Engine source `Engine/Classes/Engine/NetDriver.h:596` | `ENGINE_API bool LoadEmulationProfile(const TCHAR* ProfileName);` returns false when `[PacketSimulationProfile.<Name>]` is absent (`NetEmulationHelper.cpp:417-460`); it resets every field before applying the section | Confirmed (the ADR's [U] name is right) |
| Load by name, values | Harness boot check `-StealthHarnessProfileCheck` (`Source/StealthTestHarness/Private/Scenarios/StealthHarnessRun.cpp`): a fresh `FPacketSimulationSettings` with `PktLag` pre-set to 999 per profile, then `LoadEmulationProfile(<Name>)` | StealthLeak1 loaded=1 PktLag=100 PktLagVariance=0 PktLoss=1; StealthLeak2 loaded=1 100/0/2; StealthClean loaded=1 0/0/0 (the 999 was reset); control `StealthNoSuchProfile` loaded=0 (settings untouched) — identical in all 10 processes of the five runs | PASS |
| Applied by `-PktEmulationProfile=` | Each process's boot record reads its game net driver's `PacketSimulationSettings` after start | Every process launched with a profile reported exactly that profile's values; processes without one reported 0/0/0 | PASS |
| RTT recorded (no threshold) | Scenario `Rtt`, listen host + 1 client, 40 s, engine-reported `UNetConnection::AvgLag` and `APlayerState::ExactPing` per second after a 5 s warm-up (35 samples per side) | See table; written into the ini comments | Recorded |

| Run | Profile on | Client AvgLag RTT median (p10-p90) | Server AvgLag RTT median (p10-p90) |
|---|---|---|---|
| rtt2-clean-both | both (StealthClean) | 17.3 ms (15.9-19.4) | 17.4 ms (15.9-19.8) |
| rtt2-hleak1-client | client only (StealthLeak1) | 120.9 ms (118.8-122.7) | 120.5 ms (119.2-122.1) |
| rtt2-leak1-both | host and client (StealthLeak1) | 225.8 ms (223.0-227.1) | 225.2 ms (223.2-227.1) |
| rtt2-hleak2-client | client only (StealthLeak2) | 120.4 ms (117.6-122.3) | 120.6 ms (118.5-122.4) |
| rtt2-leak2-both | host and client (StealthLeak2) | 226.2 ms (223.4-228.2) | 226.1 ms (223.4-228.4) |

**PktLag semantics ([U] resolved):** PktLag delays only the **outgoing** packets of the process that applies the profile. On one side the RTT is the clean 17 ms + 100 ms; on both sides + 200 ms. ADR-0008 §5's "one-way on the client's outgoing side" therefore gives ~120 ms RTT; ADR-0002's "symmetric PktLag 100 ms" means the profile on **both** processes (~225 ms). The T3 runs of the IVP spikes S1 and S9 (same day) applied the profile on every process. T2 (CQTest) applies the loaded profile to the server and every client driver (`PIENetworkComponent.cpp:217-222`), i.e. symmetric.

## Reproduction

```
powershell -File tools/dev/cycle.ps1 -NoLaunch
powershell -File tools/ci/mp/run-mp.ps1 -Scenario Rtt -Clients 1 -ClientProfile StealthLeak1 -RunId rtt2-hleak1-client -Duration 40 -ExtraArgs "-StealthHarnessProfileCheck"
powershell -File tools/ci/mp/run-mp.ps1 -Scenario Rtt -Clients 1 -ServerProfile StealthLeak1 -ClientProfile StealthLeak1 -RunId rtt2-leak1-both -Duration 40 -ExtraArgs "-StealthHarnessProfileCheck"
(same for StealthLeak2, and StealthClean on both)
python -I tools/ci/mp/analyze_rtt.py Saved/Harness/<run> ...
```

Evidence: `production/qa/evidence/story-017-net-emulation-profiles/` (`rtt-analysis.txt`, gzipped record files and launch lines of the five runs).
