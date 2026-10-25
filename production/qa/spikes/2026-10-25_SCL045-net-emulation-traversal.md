# SCL spike 045: net emulation (ST1 starvation) and traversal under emulation (E12), 2026-10-25

> **Story:** `production/epics/shared-character-locomotion/story-045-net-emulation-traversal-spike.md` · **Governs:** ADR-0010 Verification Required (traversal spike, ST1, E12; §6, §7), GDD OQ2/OQ3, SCL Exit B
> **Engine:** UE 5.8.4, legacy `IpNetDriver` (Iris off), CQTest in-process PIE: listen host + 3 clients, `AStealthGameMode`, `L_Prototype`, headless (`-nullrhi`)
> **Net profile:** `StealthLeak1` (PktLag 100 ms, PktLoss 1 %) loaded by name and applied by CQTest to the server and every client driver (100 ms each way; ~225 ms RTT, see `production/qa/smoke-2026-10-25_net-emulation-profiles.md`)
> **Outcome:** the spike has no pass/fail (QA clarification); all five items are answered with a number or a decision below.

## Answers

| # | Item (AC) | Answer |
|---|---|---|
| 1 | **ST1 starvation forced-update cadence (Hz)** | **4.0 Hz** with the pinned `MAXCLIENTUPDATEINTERVAL` 0.25 s: forced steps every 0.253 s (median; p10 0.251, p90 0.257, n = 20), first forced step 0.355 s after the loss starts (0.10 s of in-flight moves + 0.25 s), last step 1.36-1.37 s after (forced updates stop at `MaxClientForcedUpdateDuration` 1.40 s; 5 steps per 2.5 s starvation, 5/5 runs). Each forced step moved the pawn **50.6 cm** (200 cm/s × 0.25 s): the engine keeps the **last acceleration**, it does not brake. Per-frame alternative: with the interval set to one frame the engine still enforces `max(interval, DeltaSeconds + 0.06 s)` (`PlayerController.cpp:5583-5584`), giving **13.6 Hz** (73.6 ms; 14.7 cm steps) — a true per-frame zero-input update needs the ADR's own `ForcePositionUpdate` override, not the engine knob. The 4 Hz vs per-frame choice stays with stories 050/051 (AC-23/24), as planned. |
| 2 | **Traversal end-position error, n ≥ 100 per kind, vs 64 cm** | **vault (0.8 m wall), n = 102:** owner vs server median 0.0 cm, p99 1.8, max 3.5; proxies (204 observations) median 2.0 cm, p99 12.7, max 20.8 → **306/306 within 64 cm**. **climb (1.6 m block), n = 102:** owner p99 0.04 cm; proxies median 0.0, p99 0.12, max 30.7 → **306/306 within 64 cm**. Optional **hurdle (1.2 m wall), n = 30:** owner max 0.01 cm, proxies max 5.8 → 90/90 within. After a 1 s settle every owner and proxy matched the server within 0.6 cm (no lasting divergence, no snap). |
| 3 | **Binary decision on proxy root motion (E12)** | **Follow replicated movement** (not `RepRootMotion` montage playback) for every pawn. Evidence: proxies received active `RepRootMotion` data on 88-93 % of action frames, but the sample's proxy montage path is `OnRep_TraversalResult`, which fires only when the replicated result changes — it ran on only 16 of 468 proxy observations (repeating the same obstacle never re-fires it); on the other 97 % the proxies simply followed replicated movement in `MOVE_Flying` and still ended within 0.1-21 cm of the server. A montage-driven proxy path is therefore not needed for position parity and is a kind-dependent hazard (it depends on a replicated sample struct); the cosmetic montage plays from the production `TraversalState` (ADR-0010 §6). |
| 4 | **The sample's movement-mode name during traversal** | **`MOVE_Flying`** (CMC `GetMovementName()` "Flying") on the server, the owner and every proxy in 100 % of 234 completed traversals, back to `Walking` after. |
| 5 | **Final traversal heights** | **Decision: vault 0.8 m, climb 1.6 m, hurdle 1.2 m are final.** The sample resolves them as: 0.8 m × 0.2 m wall → `Hurdle` (102/102), 1.2 m × 0.2 m wall → `Hurdle` (30/30), 1.6 m × 2.0 m block → `Mantle` (102/102). (The sample's `Vault` action needs a lower back floor and never occurred on these shapes; the kit's "vault" is the sample's `Hurdle`.) Action durations on the server: 1.43 s (0.8 and 1.2 m), 2.90 s (1.6 m). |

**Also recorded (story notes):**
- **PktLag direction** (needed by story 050's RTT definition): PktLag applies to the outgoing packets of the process that sets it; here every driver set it, so 100 ms each way (~225 ms RTT). Client-only application gives ~120 ms RTT (tdc-017 smoke record).
- **Client-initiated start lead:** the owner starts 0.12 s before the server (one-way lag + a frame; p99 0.134 s), so mid-action the owner is 80-110 cm ahead of the server (median 81 cm at 0.8 m, 105 cm at 1.6 m). This is time offset, not error: the end positions agree. ADR-0010's server-authoritative start from a queue slot removes this lead.
- **Sample bug found:** `OnRep_TraversalResult` does not fire for a repeated identical traversal, so proxies normally skip the traversal animation (they slide through the motion in `Flying`). Not promoted; the production path replaces it (ADR-0010 forbids the sample's `TraversalResult` replication).
- One climb lane (start 160 cm from the end of the 6 m block) failed the sample's own `TryTraversalAction` check every time (51/51 `noStart`, `TraversalCheckFailed`); the two lanes nearer the middle succeeded 102/102. Recorded as the sample's ledge-reach limit near block ends, not a network effect; n counts completed traversals.

## Method (reproduction)

```
tools/dev/cycle.ps1 -NoLaunch
UnrealEditor-Cmd.exe StealthGame.uproject -ExecCmds="Automation RunTests StealthSpike.Locomotion.PIE.NetEmulationTraversal; Quit" -unattended -nullrhi -nosound -StealthHarnessRunId=scl045-final
python -I tools/ci/mp/analyze_scl045.py Saved/Harness/scl045-final --json <out>
```

- Test: `Source/StealthTestHarness/Private/Tests/PIE/Spikes/NetEmulationTraversalSpike.cpp`, CQTest `NETWORK_TEST_CLASS(NetEmulationTraversal, "StealthSpike.Locomotion.PIE")`, methods `Starvation` and `Traversal` (both Success; about 8 min in total). Named `StealthSpike.*` so the per-commit `StealthGame.` gate never runs it (ADR-0008 §8, 10-minute cap). No crowd (NpcCount 0 for the run, restored after).
- **Starvation:** client 1 drives forward for 1.5 s (real `ServerMove` stream), then its net driver's outgoing loss is set to 100 % for 2.5 s while it keeps "holding W"; every server frame the server-side pawn location is sampled; 5 repeats per configuration (pinned 0.25 s, then interval = one frame). Steps in the first 0.15 s after the loss starts are moves already in the 100 ms lag queue and are excluded.
- **Traversal:** the map's sample course blocks (`LevelBlock_Traversable`, whose ledge splines are authored per placed instance) are staged to the kit heights identically in the server and every client PIE world (in-place 0.8 m walls and a 1.6 m block lowered into the ground; three 2 m walls moved to open floor and lowered to 1.2 m; PIE-only, no asset is changed). Each client's pawn is teleported by the server to its lane start facing the obstacle, then the owning client walks forward and calls the sample's own `GetTraversalCheckInputs` → `TryTraversalAction` (the Space-key path), as a player does. Start/end are read from the sample's `DoingTraversalAction` on the server and owner, and from the replicated movement mode (or the flag) on proxies; positions at the end and after a 1 s settle are compared with the server's.

## Limits

- In-process PIE (the AC's setting); multi-process traversal is story 049's T3 run.
- Player pawns only (no NPC traversal exists yet; NPC parity of traversal is stories 048/053).
- Headless: proxies' animation is not rendered, so the visual effect of the missing proxy montage is inferred from the flag, not seen.
- Host frame rate ~120 fps (the engine's forced-update floor scales with it: DeltaSeconds + 60 ms).

## Evidence

`production/qa/evidence/story-045-net-emulation-traversal-spike/`: `scl045-analysis.json`, gzipped `scl045-starvation.jsonl` and `scl045-traversal.jsonl` (every trial with server/owner/proxy start, end and settled positions, modes, root-motion counters and the sample's traversal result), `automation-run.txt`.

## Unblocks

Stories 047, 048, 049, 050, 051, 053, 055 of shared-character-locomotion (their status notes cite this report); ADR-0010's conditional traversal gate is updated by its owner (amendment appended to ADR-0010 the same day).
