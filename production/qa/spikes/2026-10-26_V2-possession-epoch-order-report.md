# Spike V2: ClientSetPossessionEpoch always precedes ClientRestart (ADR-0011 verification item V2)

> **Story:** `production/epics/input-interaction-framework/story-009-spike-v2-possession-epoch-order.md` · **Date:** 2026-10-26 · **Verdict:** **PASS** on every path that could be exercised (161/161 runs: takeover, same-pawn re-possession after an Unresponsive hand-over, `ClientRetryClientRestart`, takeover of a culled pawn, and a fresh controller's first possession; each without emulation and under PktLag 50 ms per direction + 1 % loss where repeatable). **Not exercised:** a mid-round rejoin (the rejoined player gets no pawn until Round R17 / Session rejoin adoption exist) and the engine's null-pawn branch (not provoked in single-process PIE). No inversion observed.

## Setup

| Item | Value |
|---|---|
| Engine build | 5.8.4-58902052+++UE5+Release-5.8, legacy replication |
| Topology | single-process PIE, listen server + 1 client, `L_Prototype`, `AStealthGameMode` (CQTest `NETWORK_TEST_CLASS`) |
| Controller | the production `AStealthPlayerController` of story iif-094: `OnPossess` increments `ServerPossessionEpoch` and sends `ClientSetPossessionEpoch(N)` **before** `Super::OnPossess` (which sends `ClientRestart`) |
| Trace | development-only `AStealthPlayerController::GetPossessionTrace()`: server `OnPossess` (epoch, pawn GUID); owner `ClientSetPossessionEpoch` receipt; owner `ClientRestart` with `OwnerPossessionEpoch` at that moment and the pawn's network GUID (identical on both sides, so each owner restart is matched to its own possession; a late resend of an earlier possession that lands inside a later run is checked against its own epoch) |
| Simplifications (the story allows them) | Round R17.1 and the ADR-0010 hand-over pass do not exist: possession changes are direct `UnPossess` / `Possess` calls; `FScopedPossessionParity` does not exist either |
| Emulation | `FPacketSimulationSettings{PktLag 50, PktLoss 1}` on the server and client NetDrivers (50 ms per direction) |
| Harness | `Source/StealthTestHarness/Private/Tests/Spikes/SpikeV2PossessionEpochOrderTest.cpp`, `StealthSpike.Input.PIE.SpikeV2PossessionEpochOrder.EpochPrecedesClientRestartOnEveryPath` (throwaway) |
| Raw data | `production/qa/evidence/story-009-spike-v2-possession-epoch-order/v2-results.json`, `run-log-excerpt.txt` |

Pass rule per run: on the owner, `ClientSetPossessionEpoch(N)` is received before every `ClientRestart` of that possession,
and `OwnerPossessionEpoch == N` at each of them (same-pawn re-possession must still advance N).

## Results

| Path | How it is exercised | No emulation | PktLag 50 / 1 % loss | Verdict |
|---|---|---|---|---|
| Takeover | the player possesses a crowd pawn; the crowd AI adopts the old body | 20/20 | 20/20 | PASS |
| Same pawn after an Unresponsive hand-over | `UnPossess`; a temporary AI controller holds the pawn 6 frames; the same player re-possesses the same pawn: the epoch advanced every run | 20/20 | 20/20 | PASS |
| `ClientRetryClientRestart` | a takeover immediately followed by the server's `ClientRetryClientRestart` for the same pawn (the engine then skips the duplicate restart: the owner had acknowledged) | 20/20 | 20/20 | PASS |
| Takeover of a culled pawn ("null-pawn resend") | takeover of a crowd pawn made irrelevant to the connection (IVP S9 hook) after its channel closed | 20/20 | 20/20 | PASS (ordering); the null-pawn branch itself was **not provoked**: `ClientRestart` resolved the pawn in every run (null restarts 0) |
| First possession of a fresh controller (join) | the client's initial join: `ClientSetPossessionEpoch(1)` then `ClientRestart` | 1/1 | (not repeatable) | PASS |
| Mid-round rejoin (ST-001 hook) | the hook reconnects the client (5 attempts per profile) | NOT EXERCISED | NOT EXERCISED | the rejoined controller is never given a pawn mid-round, so no `OnPossess` happens |

In an earlier run of the same harness one emulated takeover window also received a late `ClientRestart` belonging to the
previous possession (owner epoch still the previous value, which is correct for that possession); the harness now
attributes restarts by pawn GUID, and that case is a pass, not an inversion.

## Findings

1. The ordering holds by construction: `ClientSetPossessionEpoch` and `ClientRestart` are both reliable client RPCs on
   the controller's channel, the epoch RPC is queued first, and reliable bunches are delivered in order also under
   loss and lag. No path showed a restart before its epoch.
2. Same-pawn re-possession advances the epoch (the server counts possessions, not pawns); the listen host (both fields
   on one object) does not double count (its `ServerPossessionEpoch` and `OwnerPossessionEpoch` stay separate fields).
3. Not verified here, and why: the null-pawn branch (`ClientRestart(null)` → `ServerCheckClientPossessionReliable` →
   resend) needs the pawn to be unresolvable on the owner when the restart executes, which single-process PIE did not
   produce even with the pawn's channel closed before possession; the mid-round rejoin needs Round R17 / Session rejoin
   adoption. Both resends travel on the same channel after the epoch RPC, so the same ordering argument applies; record
   them as open items for the multi-process harness (ADR-0008 T3) when those systems exist.

## Consequences

- Stories iif-071 and iif-091 are no longer blocked on V2. Story 091 still depends on story iif-025 (the epoch gate)
  and on ADR-0010's hand-over pass (the possession-step hold seam); story 071 still depends on the Roster's
  Owner-audience state and the HUD's presented-role gate (cross-epic).
- Story iif-025 can stamp and gate on `(PossessionEpoch, Seq)` as ADR-0011 section 2 describes.
