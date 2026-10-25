# Spike ST-001: PIE harness hook forces drop, rejoin, supersede and host kick, 2026-10-25

> **Story:** `production/epics/session-travel/story-001-harness-connection-event-hook.md` (TR-session-006)
> **Governs:** ADR-0005 §5 (rejoin, supersede, kick), GDD Open Question 2 · **Engine:** UE 5.8.4 Development editor, legacy `IpNetDriver`
> **Verdict:** **PASS** — in a PIE listen server with 3 clients (fixture map `L_Prototype`, `AStealthGameMode`, no game-code edit) the hook produced each event, every time, through the real net driver: (a) a drop (client quit, server-side close and a silent outage), (b) a same-key rejoin on a new `UNetConnection`, (c) a second live connection carrying a live participant's key, (d) a host kick. 5 of 5 consecutive runs passed (the determinism bar adopted for this story), ~2.5 s of PIE per run.

## Questions and answers

| # | Question | Answer | Result |
|---|---|---|---|
| a | Drop a chosen client's connection | Three modes. `ClientQuit`: the engine's own `UEngine::HandleDisconnect` path (close bunch, client travels to its default map); the server logs it out once. `ServerClose`: `UNetConnection::Close()` on the server; logout observed, client loses its connection. `Silent`: 100 % packet loss both ways on the client driver (`DO_ENABLE_NET_TEST`): the connection stays `USOCK_Open` and the server's `LastReceiveRealtime` did not move for 60 frames; it moved again after `ResumeFromSilent` | **PASS** |
| b | Same IdentityKey reconnects as a rejoin | `Rejoin(handle, key)` browses the client world context to its last server (`FWorldContext::LastRemoteURL`) with `?IdentityKey=`; the server saw a new `UNetConnection` carrying the same key (2 logins, 1 logout for key 1) | **PASS** |
| c | A second connection with the same key supersedes a live one | `Supersede(handle, key)` refuses unless a live connection carries the key and it is not the caller's own; client 2 re-joined with client 1's key and the server had **two** live connections with key 1 at once (the game code has no supersede yet, so both stay: story 058/059 own the rule) | **PASS** |
| d | The host kicks a chosen participant | `Kick(key)` calls `AGameSession::KickPlayer`, which destroys the controller; destroying a remote controller closes its control channel, so the connection closed and the client lost it; one logout for key 3 | **PASS** |
| D | Deterministic? | 5/5 runs (`run2`, the full `StealthGame.` suite run, `run3`, `run4`, and a re-run on the final build), each with the same exact event counts: 5 logins, 7 logouts | **PASS** |

## Findings for ADR-0005 and the stories that use the hook

1. **`UNetConnection::GNetConnectionBeingCleanedUp` is not exported** (no `ENGINE_API` in 5.8.4): a game module cannot link it (LNK2001). ADR-0005 §1's `Logout` override names it as one of two ways to find the closing participant; only the other one works from game code: a weak controller → participant map recorded at login (the hook does this; the S4 spike game mode resolves it through the PlayerState's bound participant). Recorded for the ADR-0005 amendment list.
2. **`UNetConnection::RequestURL` is the whole login URL** (`/Game/…/Map?Name=…?IdentityKey=…?SplitscreenCount=1`), not an options string: `UGameplayStatics::ParseOption` on it returns "" (it needs a leading `?`). Parse it as a `FURL` (the hook's `IdentityKeyOf`). The `Options` string `Login`/`PreLogin` receive does start with `?`. Relevant to story 026 (IdentityKey) and 031 (join options stripped).
3. **`?Name=` is replaced** by the null online subsystem's nickname (`FergsPC-<hex>`) in the login URL the server receives, in PIE and in `-game`: display names cannot come from `?Name=` as sent (story 032).
4. **A kick or server close logs `LogNet: Error: UEngine::BroadcastNetworkFailure` on the client**: every PIE test that drops a client must declare it as an expected error, or the automation framework fails the test on the log line.
5. Without game support, a same-key second connection simply co-exists: the hook reports the shape, the game's Session decides (rule 15).

## Setup (reproduction)

```
powershell -File tools/dev/cycle.ps1 -NoLaunch
UnrealEditor-Cmd.exe StealthGame.uproject -ExecCmds="Automation RunTests StealthGame.Session.PIE.ConnectionEventHook; Quit" -unattended -nullrhi
```

- Hook: `Source/StealthTestHarness/Private/Session/StealthConnectionEventHook.{h,cpp}` (`FStealthConnectionEventHook`; clients are named by world-context handle so calls survive the map loads a rejoin causes; observations come from `FGameModeEvents` login/logout events; keys from `IdentityKeyForSlot(slot)` = `<slot as 8 hex>5374656c0000000000000000`, the format `tools/ci/mp/run-mp.ps1 -IdentityKeys` also uses).
- Test: `Source/StealthTestHarness/Private/Tests/PIE/Session/ConnectionEventHookPieTest.cpp`, `StealthGame.Session.PIE.ConnectionEventHook.DropRejoinSupersedeAndKickEachProduceTheirEvent`: every wait is a state condition or a frame count; 120 s per step is only a failure ceiling. It runs in the per-commit `StealthGame.` gate (~3 s).
- The same hook is used by the multi-process spikes S3 and S4 (client-side `Drop`/`Rejoin` in a `-game` process).

## Limits

- Single-process PIE and one machine; the multi-process use is in the S3/S4 reports.
- The silent drop is packet-loss emulation, not a pulled cable: the OS socket stays healthy.
- IdentityKey is only a login option until story 026 adds the client subsystem and `-IdentityKey=`.

## Evidence

- `production/qa/evidence/story-001-harness-connection-event-hook/test-run.txt` (the five runs and the full-suite result).
