# Spike V3: server NetGUID resolution of a culled pawn (ADR-0011 verification item V3)

> **Story:** `production/epics/input-interaction-framework/story-010-spike-v3-netguid-server-resolution.md` · **Date:** 2026-10-26 · **Verdict:** **FAIL for the raw engine call (cases d and e1), PASS for the ingress resolver with the two guards this spike added** (all five cases match)

## Setup

| Item | Value |
|---|---|
| Engine build | 5.8.4-58902052+++UE5+Release-5.8 |
| Replication | legacy (`net.Iris.UseIrisReplication=0`, ADR-0001 / AQ-01) |
| Function under test | server `FNetGUIDCache::GetObjectFromNetGUID(Guid, bIgnoreMustBeMapped = false)` through `UNetDriver::GetNetGuidCache()`, the call `IngestCommand`'s `ResolvePawnRefGuid` makes (`Source/StealthGameplay/Private/Core/CommandIngress.cpp`) |
| Topology | single-process PIE, listen server + 1 client, `L_Prototype`, `AStealthGameMode` (CQTest `NETWORK_TEST_CLASS`) |
| Culling mechanism | `IsNetRelevantFor` false for the test connection through the IVP S9 spike hook (`StealthSpikeHooks::SetRelevancyOverride`), because ADR-0003's production cull does not exist yet; the spike waits until the server's actor channel for that pawn is closed (`culledChannelClosed: true`) |
| Harness | `Source/StealthTestHarness/Private/Tests/Spikes/SpikeV3NetGuidServerResolutionTest.cpp`, `StealthSpike.Input.PIE.SpikeV3NetGuidServerResolution.FiveGuidCasesResolveAsRecorded` (throwaway, outside the `^StealthGame.` gate) |
| Raw data | `production/qa/evidence/story-010-spike-v3-netguid-server-resolution/v3-results.json`, `run-log-excerpt.txt` |

Run: `UnrealEditor-Cmd.exe StealthGame.uproject -ExecCmds="Automation RunTests StealthSpike.Input.PIE.SpikeV3; Quit" -unattended -nullrhi` → `Result={Success}`.

## Results

| Case | GUID | Raw engine call | Ingress resolver (`ResolvePawnRefGuid`) | Expected | Verdict (resolver) |
|---|---|---|---|---|---|
| (a) GUID the server assigned to a pawn relevant to the connection | 96 | the pawn | the pawn | pawn | PASS |
| (b) GUID of a pawn culled for the connection (channel closed) | 108 | the pawn | the pawn | pawn | PASS |
| (c) never-assigned GUID | 4294967264 | null | null | null | PASS |
| (d) stale GUID of a pawn destroyed earlier (90 frames before, no GC yet) | 78 | **the destroyed actor** (`IsActorBeingDestroyed`) | null | null | PASS (raw FAIL) |
| (e1) the default GUID (ObjectId 1, a pattern the server never assigns) | 1 | **ensure** (`!NetGUID.IsDefault()`, `PackageMapClient.cpp:3745`) then null | null | null | PASS (raw FAIL) |
| (e2) an unassigned static GUID (odd ObjectId) | 4294967267 | null | null | null | PASS |

No log line above Warning in the guarded run. The first run (before the guards) failed case (d) (the resolver
returned the destroyed crowd pawn) and raised the engine ensure on case (e1), with a log error.

## Findings

1. **A culled pawn's GUID still resolves on the server (b)**, exactly as ADR-0011 section 3.1 step 4 assumes:
   resolution grants nothing, so the `CanViewerKnow` and `PawnRefAgreementCm` gates (section 9) stay the only
   admission. The resolved pawn is never evidence of visibility.
2. **The cache answers a destroyed actor until garbage collection (d).** UE 5.8.4 no longer marks a destroyed actor
   garbage at `Destroy`, so the weak pointer in the GUID cache still resolves it. `ResolvePawnRefGuid` now treats an
   invalid or `IsActorBeingDestroyed` pawn as absent; stage 2 step 0 (pawn valid, not pending teardown) remains the
   authoritative check at the slot.
3. **A client can make the server ensure (e1)** by sending `PawnRefGuidValue = 1`: `GetObjectFromNetGUID` ensures on
   the default GUID. `ResolvePawnRefGuid` now refuses `IsDefault()` before asking the cache. (A non-shipping ensure is a
   log error and a one-time callstack, a cheap grief vector; Shipping has no ensure, the guard is still right.)
4. Two connections share one NetDriver GUID cache on the server, so the answer does not depend on which connection
   sent the GUID (the story's edge case); the listen host's own commands carry a weak handle instead (V6, story 024).

## Consequences

- `IngestCommand` (stage 1) resolves `PawnRefGuid` only through `ResolvePawnRefGuid`, never the raw cache call.
- Story iif-020 (ingress) keeps the strip-self and the count; stories 061-063 (PawnRef) can rely on: resolved pawn ⇒
  valid and not being destroyed; absent ⇒ the Attack resolves as `TargetSpot`.
- ADR-0011 E7 note for the next ADR sync: "the server cache resolves culled pawns (V3 PASS) and destroyed actors until
  GC (guarded); the default GUID ensures (guarded)".
