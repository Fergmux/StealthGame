# ADR-0023: Playtest telemetry hooks (pre-Telemetry GDD)

## Status
Accepted — 2026-10-25, by the user (blanket acceptance of ADR-0001..0023, recorded by the orchestrator). **Conditional:** every spike or verification item named in the earlier status text below must still report before implementation depends on it; a failed spike is handled by a superseding ADR, not by editing this one.

*Earlier status text (kept for its gating conditions):* Proposed — awaiting user acceptance. It needs ADR-0002, ADR-0007 and ADR-0008 Accepted (queue keys and round-end step, the Event Log store and logging policy, the harness and T0 lint) and should be Accepted before the first 10-player internal playtest (architecture.md Required ADRs row ADR-023, priority Should). The Telemetry & Analytics GDD (#26, Alpha) will own the consumer side; this ADR fixes only the producer hooks, the record format and the export rule.

## Date
2026-10-24

## Last Verified
2026-10-24 (engine claims read against the UE 5.8.4 install at `C:/Program Files/Epic Games/UE_5.8`; design against `round-match-framework.md` R19, R21, F7, F8; `crowd-npc-behaviour.md` CNB-R7, CNB-R16, F2, F6, change request 20; `assassination-defence-reversal.md` AC-ASN-33, OQ-ASN-3/4; `objectives-chain.md` F5; `gdd-cross-review-2026-10-24.md` D-W1..D-W4 and "Notes for /create-architecture")

## Decision Makers
Autonomous design pipeline (`/architecture-decision`, rigor full, review mode full); technical-director (TD-ADR) and unreal-specialist validation; acceptance reserved for the user.

**Gate record (2026-10-24):** TD-ADR CONCERNS → REVISED; engine validation MINOR NOTES → applied. Details under `## Validation Criteria` → "Gate results".

## Summary
The first 10-player playtests must answer four open design-theory questions (brace timing D-W1, run share D-W2, final-court camping D-W3, team-size bias D-W4) and re-derive the crowd's red-herring rates from measured player rates (CNB-R7/R16), long before the Alpha Telemetry GDD exists — without opening any new path by which a client, a log file or a trace could learn hidden facts during a round. This ADR adds a bounded, server-only, `NeverReleased` **telemetry section to the Round Event Log store**, fed by kind-blind probes at owners' existing server decision points and by kind-blind pattern detectors that read published server pawn state for every pawn; the section is serialised by the Event Log store's single disk writer only after the round has terminated (`Ended`, or `Reset` for a round that never reached `Ended`), never through `UE_LOG`, trace or analytics channels, and nothing in the game ever reads it back.

## Engine Compatibility

| Field | Value |
|-------|-------|
| **Engine** | Unreal Engine 5.8 (installed 5.8.4, CL 58902052) |
| **Domain** | Core (server-side data recording and file export) |
| **Layer** | Foundation (the telemetry section lives in the Event Log store, `StealthFoundation`, architecture.md Foundation "Event Log & fault sink"); the probes and detectors are Feature-layer call sites; the Required ADRs table calls it cross-cutting |
| **Knowledge Risk** | LOW — Core file I/O, JSON writing and task launching are pre-cutoff and stable; 5.8's only relevant change is the `UE_LOGF` logging guidance (ADR-0007), which this ADR keeps out of the data path entirely |
| **References Consulted** | `docs/engine-reference/unreal/VERSION.md`, `deprecated-apis.md` (`UE_LOGF`), `current-best-practices.md` (logging); `docs/architecture/architecture.md` (Foundation "Event Log & fault sink", Data Flow §4 save/load path, Required ADRs row ADR-023, Open Questions AQ-14, AQ-16, TR-round-012, TR-crowd-010); ADR-0001 (kind-blind principle, Roster as the only kind query), ADR-0002 (queue key, round-end step), ADR-0005 (Event Log archives in the game-instance store), ADR-0006 (secrets never stored), ADR-0007 §6 (logging policy, plain-C++ Event Log types, single disk writer), ADR-0008 (T0 lint, T3 harness, T4 statistics), ADR-0011 (action hub `OnActionStarted`), ADR-0016 (§4.6 recordable virtual-input stream, Enables row), ADR-0017 (concealment log slice), ADR-0022 (CPU budget table); engine source read this session (below) |
| **Post-Cutoff APIs Used** | None |
| **Verification Required** | **TEL-V1** export off the game thread: the snapshot copy at the export point costs ≤ 0.2 ms GT and the background write of a worst-case round (≤ 2 MB JSON) completes before the next round's `Preparing` P3, on the reference listen host. **TEL-V2** zero leakage: a T3 multi-process run with the telemetry section enabled shows no telemetry type name or field value in `Saved/Logs`, the Insights trace, the CSV profiler output or any client packet capture, and no telemetry file exists on disk before the phase leaves `Active`. **TEL-V3** detector parity: the same scripted motion fed to an NPC pawn and to a bot-driven player pawn (ADR-0008 T2) yields identical tell counts from every detector |

**Engine facts this ADR relies on** ([V] read in 5.8.4 source this session; [D] engine reference; [U] unverified):

| # | Fact | Tag |
|---|---|---|
| E1 | `TJsonWriter` / `TJsonWriterFactory::Create(FArchive*)` and `TJsonStringWriter` exist in the `Json` runtime module | [V] `Runtime/Json/Public/Serialization/JsonWriter.h:84-88, 742, 778-783` |
| E2 | `FFileHelper::SaveStringToFile(FStringView, const TCHAR*, EEncodingOptions, IFileManager*, uint32)` exists | [V] `Runtime/Core/Public/Misc/FileHelper.h:196` |
| E3 | `UE::Tasks::Launch(DebugName, Lambda)` (free function) launches a task and returns a `TTask` handle that can be waited on | [V] `Runtime/Core/Public/Tasks/Task.h:17 (example), 299` (TD/engine gate) |
| E7 | `SaveStringToFile` with `EEncodingOptions::AutoDetect` switches to UTF-16 on any non-ANSI character; `ForceUTF8WithoutBOM` avoids it; the file manager creates the directory tree; `IFileManager::Move` exists | [V] `FileHelper.h:196`; `FileManagerGeneric.cpp:114` (TD/engine gate) |
| E8 | `UWorld::AsyncLineTraceByChannel` exists (not deprecated); results arrive on the game thread one frame later through `FTraceDelegate`; `EAsyncTraceType::Test` returns hit/no-hit only | [V] `Engine/Classes/Engine/World.h:2522` (TD/engine gate) |
| E9 | The `Json` runtime module is `Runtime/Json/Json.Build.cs`; `TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>` writes condensed JSON | [V] `JsonWriter.h:778-783` (TD/engine gate) |
| E4 | `FPaths::ProjectSavedDir()` exists | [V] `Runtime/Core/Public/Misc/Paths.h:290` |
| E5 | The engine ships analytics providers (`Analytics`, `AnalyticsCSV`, `AnalyticsLog`, `AnalyticsET`, `AnalyticsHorde`, `AnalyticsSwrve`, `AnalyticsVisualEditing`, `TelemetryUtils`) that record string-keyed events as they happen | [V] `Engine/Source/Runtime/Analytics/` (directory listing); behaviour of each provider [U] (not adopted, Alternative 3) |
| E6 | `UE_LOGF` is the 5.8-preferred logging macro; logs land in `Saved/Logs` on the host and are readable mid-round | [V] ADR-0007 (`LogMacros.h:199`); Round R19 |

## ADR Dependencies

| Field | Value |
|-------|-------|
| **Depends On** | ADR-0002 (queue key `FQueueKey`, `RoundEpoch`, the round-end step that enters `Ended`), ADR-0005 (`URoundMatchStore` holds Event Log archives across travel), ADR-0007 (Event Log store placement in `StealthFoundation` `Public/Server/`, plain-C++ rule, single disk writer, logging policy and lint), ADR-0008 (T0 lint extension, T2/T3 parity and leak tests, T4 offline statistics). Soft: ADR-0011 (action hub), ADR-0016 (Crowd accepted-tell hook), ADR-0017 (Concealment server records), ADR-0018 (strike outcome at the slot), ADR-0020 (objective completions and court table) — each adds one probe call at an existing server decision point |
| **Enables** | Telemetry & Analytics GDD (#26, Alpha) — consumes the export format; Crowd red-herring re-derivation per playtest build (CNB-R7, F2); resolution of D-W1..D-W4 by the creative-director with data; AC-ASN-33, Objectives F5 and Round F7 reporting |
| **Blocks** | The first 10-player playtest's Pillar-4 evidence (without these hooks D-W1..D-W4 stay unmeasured); the Crowd rate re-derivation story |
| **Ordering Note** | The telemetry section can be built as soon as the Event Log store exists (ADR-0007). Each probe lands with its owner's story; a missing probe yields an empty, explicitly flagged block in the export, never a fabricated zero |

## Context

### Problem Statement
Four design-theory warnings from the cross-GDD review are explicitly "creative calls pending data": the brace may degrade into a last-0.6 s timing window (D-W1), running may dominate the objective chain (D-W2), camping the final candidates may be the trailing team's only lever (D-W3), and small-team playtests may under-measure all of it (D-W4). The crowd's red-herring rates are placeholders that CNB-R7 requires to be "re-derived offline per playtest build from logged player rates", and its motion parity needs logged human patterns. The Round Event Log (R19) was designed as "the raw material for playtest analysis", but its entry list has no brace, run, court-presence or tell record, and the Telemetry GDD is an Alpha system. The first 10-player test is the earliest point where this data exists, and without hooks it is lost.

The data is dangerous by nature: it records exactly which pawns are players, where they ran and where they camped. During a round it is the most valuable cheat payload in the game, and on a listen server the host process holds it (IVP R10).

### Constraints
- **Server-only until the round has terminated** (Round R19: nothing released in `Preparing`/`Active`; disk write only on entering `Ended` or `Reset`). Telemetry is never released to any client in any phase, including the VS Post-Round Reveal.
- **Never through logs or traces** (R19, ADR-0007 §6): no `UE_LOG`/`UE_LOGF`, output log, `ClientMessage`, `ensureMsgf`/`checkf` text, `TRACE_*`, `CSV_*` or stat names carry telemetry content.
- **Plain C++ store types** (ADR-0007): no `USTRUCT`/`UCLASS`/`UPROPERTY`, no `ToString`; the Event Log store's disk writer is the only text conversion.
- **Kind-blind runtime** (ADR-0001, architecture principle 1): recording must not create a code path whose behaviour or cost differs by controller kind; the only kind branch allowed is the Roster's server-side query, and its result may choose only which server-only counter increments.
- **No runtime adaptation** (CNB-R7): rates are re-derived offline; nothing in the game reads telemetry.
- **No new replicated data, RPC or tuning key** (always-ask schema changes are out of this run's scope).
- **Budget**: telemetry fits inside ADR-0022's "Engine/world tick, other" 2.0 ms listen-host GT line.
- **Privacy**: internal trusted playtests only (IVP R10, AQ-14); no display names, identity keys, addresses or ping in the export.

### Requirements
- Round F7 per-round values and Objectives F5 stall metrics exported with every terminated round (aborts and voids included, so `AbortedActiveShare` is computable).
- AC-ASN-33: strikes by outcome, NPC kills per rival kill, reversals, braces, pair trades per match.
- D-W1: brace lead time before contact, kills on defenders whose defence is cooling down, time from a defender's brace end to the strike.
- D-W2: per-participant run share of Active time, run bouts with location context and witnesses, team chain timing.
- D-W3: per-participant dwell at each final-candidate court before and after the team's own preliminaries, coverage of final candidates by campers, strikes near final candidates, the final tender's fate.
- D-W4: team sizes at `Active` and minimum during `Active`, a bucket key on every round record.
- CNB-R16 / F2: per-tell occurrence counts and exposure minutes for each player participant and for NPCs (T1-T5, T12, T13), chain-gap compliance, human reaction delays to strikes (G4/R8), face-back rates (C3), and the F6 motion-feature windows.
- Memory bounded under the 10-player worst case; AQ-16 answered.

## Decision

### 1. Where the data lives: a telemetry section of the Event Log store

The Event Log store (`StealthFoundation`, `Public/Server/`, plain C++ core owned by a facade subsystem; ADR-0007) gains a second, **separate** container beside the R19 ring: `FPlaytestTelemetryCore`.

- **Separate storage, no eviction of game events.** The R19 ring and its truncation rule are Round's and this ADR does not change them (R19, synced 2026-10-24, protects the kill timeline from eviction, §5); telemetry has its own fixed-capacity event buffer (`kTelemetryMaxEvents = 16384`, code constant) and fixed-size aggregate tables. When the buffer is full, further events of that type are counted in `DroppedByType[]`, never written over the kill timeline and never over older telemetry.
- **Release class `NeverReleased`** for the whole section (R19's provisional classes): it is exported to the server's disk after termination and is never replicated, never shown in any HUD or reveal, never included in `FRoundResult`. The Information Visibility Policy needs no allowlist row (nothing replicates); the Policy's release-class table gains the section name for completeness (GDD sync item).
- **Compiled out of Shipping.** `STEALTH_WITH_PLAYTEST_TELEMETRY` is `!UE_BUILD_SHIPPING`; in Shipping the sink is a null object and the detectors are not registered. Shipping inclusion, consent and upload are the Alpha Telemetry GDD's decisions.
- **Runtime switch.** Enabled by default in non-Shipping server processes; `-StealthTelemetry=0` disables it (the sink becomes the null object). The switch is read once at session open; it is not tuning (ADR-0004), it changes no gameplay value, and its state is written in the export header.
- **Lifetime.** Cleared at `Preparing` P3 together with the ring's header; recorded only while the phase is `Active` (probes outside `Active` are ignored, except the terminal records of §5); exported at termination (§6); freed once the export task has taken its snapshot. It is **not** archived in `URoundMatchStore` (ADR-0005 keeps only the R19 log archives).

### 2. Two kinds of producers, both kind-blind

**2.1 Owner probes** — one call per event type, at the server decision point that already exists, on the path every pawn takes. The call passes the subject as `FSubjectRef` (ADR-0002), never a kind flag:

| Probe | Owner call site (existing, kind-blind) | Fields (besides `Key` or `ServerTime`, `RoundEpoch`, subject refs) |
|---|---|---|
| `StrikeResolved` | Assassination resolver at the slot (ADR-0018), after the outcome is decided | attacker, victim, outcome `{Kill, Reversal, Whiff}`, `VictimBraceAgeSec` (contact time − victim's current brace start, or −1), `VictimSinceBraceEndSec` (contact − victim's last brace end, or −1), `VictimDefenceReady` (bool), `DistToNearestCourtCm`, `NearestCourtIsFinalCandidate`, `TendingAtContact` (bool) |
| `BraceEnded` | IIF slot state machine (ADR-0011) on leaving `Bracing` | subject, start, end, cause `{Reversal, Expired, Cancelled, Preempted, Removed}` |
| `TendEnded` | Objectives hold timer completion or cancel (ADR-0020) | subject, court, start, end, `{Counted, NoEffect, Cancelled(reason)}`, court is final candidate |
| `BushTransition` | Concealment server record transition (ADR-0017; NPCs use the same path) | subject, bush, `{Enter, Exit}` |
| `NpcTellAccepted` | Crowd Director when an NPC's tell clock fires and the tell is accepted after the chain-gap guard (ADR-0016) | NPC ordinal, tell id, `GapSincePreviousTellSec` — the **only** NPC-only probe, because "accepted" is a Crowd sampler concept with no player equivalent; it is used to check the sampler against F2, not to compare kinds |

**2.2 Pattern detectors** — `FTellDetectorCore` (plain C++ in `StealthGameplay/Features/Telemetry/`, server-only, headless-testable; a Feature folder because it reads Objectives' court table, so the "Core never includes Features" rule of ADR-0007 holds) runs over **every** Shared Pawn's published server state (CMC speed tier and `IsRunning`/`IsInTraversal` (ADR-0010), `FActionState.Pose` (ADR-0011), `HeadLook` yaw and control rotation, position), at a fixed 10 Hz phase-staggered cadence, the same code for every pawn:

| Detector | Detects | Output |
|---|---|---|
| Run bout (T1) | `IsRunning` rising/falling edges | `RunBout{subject, start, end, distanceCm, startDistToNearestCourtCm, startCourtIsFinalCandidate, Witnesses[≤ kMaxWitnesses = 16] (FSubjectRef), WitnessOverflowCount}` — every pawn whose eye cone (±55° / ±38°, Camera F3 geometry) contains the runner within `kWitnessRangeCm = 3000` and whose `ViewBlock` line test is clear at bout start; the detector records witness **refs only** (no kind or team); the sink resolves them (§2.3), so "non-ally player witnesses" is computed at write time. Traces are `AsyncLineTraceByChannel` with `EAsyncTraceType::Test` (E8); the bout context is captured when the traces are issued, the delegate is bound weakly, and a pawn gone by the result counts as no witness |
| Traversal (T2) | `IsInTraversal` rising edge | count + kind `{Vault, Climb}` |
| Brace (T4) | `FActionState.Pose == Brace` rising edge | count (the `BraceEnded` probe carries timing) |
| Abrupt reversal (T12) | speed below `kStopCmPerSec = 30` then heading change ≥ 120° then walking resumes, all within 1.5 s | count |
| Head hold (T13) | `HeadLook` holding within ±10° of a pawn within 15 m for > 2.5 s, excluding holds where the looked-at pawn is itself facing the looker within 60° (the published-state definition of a conversation partner, same as C3) or where a goods/prop context lies within ±10° and 3 m (from LSC prop records once they exist; until then no goods exclusion for any pawn) — never from Crowd's internal conversation binding, so the exclusion is kind-blind; covered by TEL-V3 | count |
| Reaction to a strike (G4/R8) | for every `OnActionStarted(Attack)`: each pawn with eye LOS within 15 m (same predicate as CNB-R8); first `HeadLook` yaw change ≥ 20° toward the stimulus within 2.0 s | `ReactionDelay{subject, delaySec or none}` |
| Face-back (C3) | a pawn comes to rest within 150 cm of a resting pawn and faces it within 60° | `FaceBack{faced subject, replied within 3 s (bool), delaySec}` |
| Court presence (D-W3) | 1 Hz sample: distance from each pawn to each court ≤ `kCourtDwellRadiusCm = 600` | aggregate `DwellSec[subject][court]`, split at the subject's team's first and second preliminary completion |
| Motion windows (F6) | 30 s windows of ordinary walking with no tell in the window | the nine F6 features per window per subject (mean speed, speed variance, pause rate, pause length, heading-change rate, head-yaw rate, glance hold length, path straightness, full-pace share); table sized for 120 windows per subject key (2 × 30 min); overflow counted in `DroppedByType` |
| Exposure (all) | one clock per subject key: seconds the pawn existed and was in the round (not dead, not culled from the world) | `ExposureSec[subject key]` — the same clock for every pawn; "player minutes in state `Active`" vs "NPC minutes alive and unbound" are derived by the sink/analysis from the binding segments (§2.3), never by the detector |

Detector thresholds are **code constants versioned as `DetectorVersion`** in the export header, not tuning keys: they are analysis definitions, change no gameplay and must be identical for every pawn. A threshold change bumps `DetectorVersion`, so analysis never mixes definitions.

**2.3 The kind label and binding segments.** Producers and detectors never pass or compute a kind or team. At write time the sink resolves each `FSubjectRef` (subject and witnesses) through the Roster's server-side query (`GetParticipant`, ADR-0001 — the sanctioned kind query) into either a `ParticipantId` + team or an NPC ordinal, and that result selects only which server-only row is written; nothing reads the row back during the round (ADR-0001 sync item: "telemetry row selection" is a permitted consumer of the Roster result).

Per-subject data is keyed by **(pawn, binding epoch)**, not by pawn: informant takeover, `HandOver`/`HandBack` and `ReleasedToCrowd` move a pawn between NPC and player control mid-round, and mixing the two would corrupt the r_P/r_N calibration. On every binding change of a pawn (the Roster's binding write, observed through the same server event the Roster already raises), the detector core closes that pawn's open run bout, brace, hold, dwell segment and motion window at the change time and opens new ones under the new epoch. The export lists each segment with its resolved label and start/end time.

### 3. What is recorded, per question

| Question | Recorded (raw) | Derived offline (analysis tool) |
|---|---|---|
| Round F7 | Round's own F7 values at `Ended` (`DurationSec`, `PacingRatio`, `StallFlag`, `EndReason`) copied from the R19 header/entries | `ObjectiveEndShare`, `AbortedActiveShare` across rounds, per bucket |
| Objectives F5 | Objectives' per-team F5 fields at `Ended` (already R19 entries) | `StallDefect` share per bucket |
| AC-ASN-33 | `StrikeResolved`, `BraceEnded` | strikes by outcome, NPC kills per rival kill, reversals, braces, pair trades (reversal followed within 10 s by a kill of the same defender) per match |
| D-W1 brace timing | `VictimBraceAgeSec`, `VictimSinceBraceEndSec`, `VictimDefenceReady` on every strike; brace start/end/cause | distribution of brace lead before reversals; share of kills on defenders cooling down; attacker wait after a brace ends; share of braces that end `Expired` without contact |
| D-W2 run share | `RunBout` (distance, court context, witnesses) for every pawn; `TendEnded` timeline | per-participant run share of Active time; chain time per team vs run time inside each completion gap; witnessed-run fraction; NPC run bouts for the T1 baseline |
| D-W3 final camping | `DwellSec` per subject per court split by the team's preliminary progress; `StrikeResolved` court context; `TendEnded` on final candidates | camper-seconds at final candidates before the team's own P2; distinct final candidates covered (> 50 % presence over the last 60 s); trailing-team camping share; final tender killed while tending |
| D-W4 team size | header: `TeamSizeAtActive[2]`, `TeamSizeMinDuringActive[2]`, `TeamSizeBucket` (`1v1` … `5v5`, `Uneven`), `NpcCount`, `FinalCandidateCount` | every Pillar-4 metric reported per bucket; validation counts only `4v4` and `5v5` rounds (D-W4 recommendation) |
| CNB-R16 / F2 calibration | per subject key (pawn, binding epoch): counts of T1, T2 (vault, climb), T3 (full/short tends), T4, T5 (enter/exit), T12, T13; `ExposureSec` (one clock for every pawn; player-`Active` and NPC-unbound minutes derived from the labelled binding segments); `NpcTellAccepted` | measured `r_P` per tell, NPC realised rates vs F2 accepted rates, chain-gap compliance in play (AC-CNB-31), posterior bands per F2 robustness test |
| CNB G4/R8, C3, F6 | `ReactionDelay`, `FaceBack`, motion windows | human reaction-time distribution, face-back reply rate, F6 balanced-accuracy inputs (human windows; NPC windows for comparison) |

High-rate commands (`Move`, `Look`) are never logged raw (R19); detectors summarise them on the server. ADR-0016 §4.6's raw virtual-input stream is a **harness capture** (ADR-0008 T3), not part of this export.

### 4. Bounds and budget

- **Memory** (server process): events ≤ 16,384 × ≤ 64 B ≈ 1.0 MB worst case; aggregates (≤ 96 subject keys × ≤ 16 courts dwell, tell counters, ≤ 96 × 120 motion windows × 9 floats ≈ 0.4 MB) ≈ 0.5 MB. Expected 10-player 12-minute round: ≈ 2,500 events ≈ 160 KB.
- **CPU** (listen host GT, inside ADR-0022's "Engine/world tick, other" 2.0 ms line): ≤ **0.15 ms** mean, ≤ 0.3 ms p99 at 60 pawns (`NpcCount` 50 since 2026-10-25). Detector work is phase-staggered (60 pawns × 10 Hz ≈ 10 pawn evaluations per frame at 60 fps); court presence at 1 Hz is ≤ 50 × 16 distance checks; witness and reaction traces are async `ViewBlock` line tests issued only on run-bout starts and strikes (≤ 49 per strike). Insights scope `Stealth.Budget.Telemetry` (ADR-0022 §Key Interfaces); its name is a constant, never data.
- **Network**: zero (nothing replicates; no RPC).
- **Disk**: one JSON file per terminated round, ≤ 2 MB worst case.

### 5. Answering AQ-16 (Event Log memory and archive)

- **Ring.** At `RoundLogMaxEntriesCount` 4,096 the ring is ≤ 400 KB (F8). A 10-player round's ordinary volume is a few hundred entries, but R19's `Voided` rate limit is per source per second, so a 10-source invalid-command flood over a 12-minute round can produce ≈ 7,200 `Voided` entries. Round R19 (synced 2026-10-24, answering this ADR's change request) therefore **protects the kill timeline from eviction**: `KillResolved`, `Reversal`, `StateTransition`, `StrikeLimit`, `ObjectiveStage`, `FinalCompleted` and `WinLatched` entries occupy ring slots but are never dropped by truncation (their count is bounded by the round's kills, transitions and stages); when the ring is full the oldest **non-protected** entry is dropped and R19's per-type `TruncatedCount` increments. A flood can therefore evict only non-protected entries (`Voided`, `Rejected_Latched`, connection rows and similar). This ADR copies R19's per-type `TruncatedCount` into the telemetry header and the export, so a truncated round is visible to analysis. The telemetry section never shares or evicts ring slots (§1). Worst-case ring memory is unchanged (protected entries are counted inside the 4,096 slots; F8).
- **Archive.** No persisted archive is read back by the game. The VS Post-Round Reveal reads the current round's in-memory log in `Ended` only (R19); `URoundMatchStore` keeps the R19 log archives for the session in memory (ADR-0005; ≤ 400 KB × rounds played), and the telemetry section is freed after export. The disk files are for offline analysis only.

### 6. Export: after termination, one writer, off the game thread

- **When.** On entering `Ended` (inside the round-end step's completion, after ADR-0002's staged writes have applied) — or on entering `Reset` for a round that never reached `Ended` (`Aborted`, `Void(…)`, `PreparingFailed(…)`), or when Round's terminal **`SessionEnded` latch** is set. Session end is "a terminal event, not a phase" (Round line 96, AC-43): no phase change occurs and Round flushes the log, so the export guard is **phase ∈ {`Ended`, `Reset`} OR the `SessionEnded` latch is set**; the export function asserts that guard and refuses otherwise (never during `Preparing` or a live `Active`).
- **How.** On the game thread the store copies the R19 header, the round's F7/F5 values and the telemetry section into an immutable snapshot (POD copy, ≤ 0.2 ms, TEL-V1). A task launched with the free function `UE::Tasks::Launch` (E3) serialises the snapshot with a condensed `TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>` writer (E1, E9) — the Event Log store's single disk writer (ADR-0007 §6) — and writes it with `FFileHelper::SaveStringToFile(…, EEncodingOptions::ForceUTF8WithoutBOM)` (E2, E7) to `<ProjectSavedDir>/StealthPlaytest/<SessionId>/<RoundIndex>_<EndReason>.json.tmp`, then renames it to `.json` with `IFileManager::Move`, so a partly written file never looks complete (E4, E7). The snapshot owns its data, so `Reset` cannot free memory under the task. The facade keeps the `TTask` handle and waits on it in `Deinitialize`; the `SessionEnded` export runs **synchronously** on the game thread because process exit is not known to drain user tasks [U]. The next round's P3 waits for no export; a write still running at the next P3 continues on its own snapshot. The `StealthFoundation` module adds `Json` as a private dependency (ADR-0007 sync item).
- **Content.** `SchemaVersion`, `DetectorVersion`, build version, `SessionId` (random per session, not an identity), `RoundIndex`, `EndReason`, `Seed`, `LayoutHash`, `ProviderKind`, `PresetId`, `TuningHash`, the team-size block, `NpcCount`, `FinalCandidateCount`, the court table (court id, final-candidate flag, position), `TruncatedCount` by type, `DroppedByType`, the probe/detector records of §2-§3, and the F7/F5 values. **Excluded:** `DealDigest`, mailbox order, server sub-seeds (never stored, ADR-0006), display names, identity keys, addresses, ping and connection data, `AppearanceId` per participant.
- **Diagnostics.** The writer may emit one `UE_LOGF(LogStealthFoundation, Display, …)` line with the file path, byte count and success flag only — never a field value (safe from a worker thread). A failed write is reported as a fault code through the fault sink, marshalled to the game thread (`AsyncTask(ENamedThreads::GameThread, …)`) unless the fault sink is documented thread-safe [U].
- **Collection.** The playtest coordinator copies the files from the host after the session. Clients record and write nothing.

### 7. Analysis side (offline)

Analysis scripts live with ADR-0008's T4 tools (`tools/ci/stats/playtest/`), read the JSON files, and **emit observations, never verdicts** (`.claude/scripts` rule): per-bucket tables for F7, F5, AC-ASN-33, D-W1..D-W4, and the F2 re-derivation inputs. Reports go to `production/qa/playtests/<date>/`. Review thresholds (AC-ASN-33's review bands, F7's `ObjectiveEndShare > 0.5`, F5's defect thresholds) are read by the creative-director and game-designer; no script changes a tuning value.

### Architecture Diagram

```
 SERVER PROCESS ONLY (listen host)                              phase: Active ──► Ended/Reset
 ┌──────────────────────────────────────────────────────────────────────────────────────────┐
 │ Owner decision points (kind-blind)          Detectors (FTellDetectorCore, every pawn,     │
 │  ASN resolver ─ StrikeResolved                10 Hz staggered; 1 Hz court presence)        │
 │  IIF slot ───── BraceEnded                    reads published pawn state only              │
 │  OC hold ────── TendEnded                       │ RunBout, T2/T4/T12/T13, ReactionDelay,    │
 │  CON record ─── BushTransition                  │ FaceBack, DwellSec, MotionWindow          │
 │  Crowd ──────── NpcTellAccepted                 │                                           │
 │        └──────────────► IStealthTelemetrySink ◄─┘ (null object in Shipping / when disabled) │
 │                                 │ subject → Roster.GetParticipant (only kind query)          │
 │  Event Log store (StealthFoundation, Server/, plain C++)                                    │
 │   ├─ R19 ring + header (Round's; kill timeline eviction-protected; PostRoundEligible / …)    │
 │   └─ FPlaytestTelemetryCore (separate bounded buffers; NeverReleased)                       │
 │                                 │ on Ended / Reset-after-abort / SessionEnded only          │
 │                         snapshot (GT, ≤0.2 ms) ──► UE::Tasks ──► TJsonWriter ──► Saved/StealthPlaytest/…json
 └──────────────────────────────────────────────────────────────────────────────────────────┘
   ✗ no UE_LOG/UE_LOGF content · ✗ no TRACE_/CSV_/stat names · ✗ no replication/RPC · ✗ nothing reads it back
```

### Key Interfaces

```cpp
// StealthFoundation/Public/Server/EventLog/StealthTelemetrySink.h  (server-only guard first, ADR-0007)
#include "StealthServerOnlyGuard.h"

enum class ETelemetryStrikeOutcome : uint8 { Kill, Reversal, Whiff };
enum class ETelemetryBraceEnd : uint8 { Reversal, Expired, Cancelled, Preempted, Removed };

struct FTelemetryStrikeResolved { FQueueKey Key; FSubjectRef Attacker; FSubjectRef Victim;
    ETelemetryStrikeOutcome Outcome; float VictimBraceAgeSec; float VictimSinceBraceEndSec;
    bool bVictimDefenceReady; float DistToNearestCourtCm; bool bNearestCourtIsFinalCandidate; bool bTendingAtContact; };
struct FTelemetryBraceEnded  { double ServerTime; FSubjectRef Subject; double StartTime; ETelemetryBraceEnd Cause; };
struct FTelemetryTendEnded   { double ServerTime; FSubjectRef Subject; uint8 CourtId; double StartTime; uint8 Result; };
struct FTelemetryBushTransition { double ServerTime; FSubjectRef Subject; uint16 BushId; bool bEnter; };
struct FTelemetryNpcTellAccepted { double ServerTime; FSubjectRef Npc; uint8 TellId; float GapSincePreviousTellSec; };

/** Server-only sink for playtest telemetry. Implementations never log, trace or replicate. */
class IStealthTelemetrySink
{
public:
    virtual ~IStealthTelemetrySink() = default;
    virtual void Record(const FTelemetryStrikeResolved& E) = 0;
    virtual void Record(const FTelemetryBraceEnded& E) = 0;
    virtual void Record(const FTelemetryTendEnded& E) = 0;
    virtual void Record(const FTelemetryBushTransition& E) = 0;
    virtual void Record(const FTelemetryNpcTellAccepted& E) = 0;
};

// Obtained from the Event Log facade; never null (null object when disabled or in Shipping).
IStealthTelemetrySink& UStealthEventLogSubsystem::GetTelemetrySink();
```

Plain structs only (no `USTRUCT`, no `ToString`), per ADR-0007. The detector core (`FTellDetectorCore`, `StealthGameplay/Private/Telemetry/`) takes an injected `IPawnStateView` (published state of every pawn) and the sink, so it is unit-testable headless (ADR-0008 T1).

### Implementation Guidelines

1. Telemetry content **must never** appear as an argument of `UE_LOG`/`UE_LOGF`, `ensureMsgf`/`checkf`, `ClientMessage`, `TRACE_*`, `CSV_*` or a stat/scope name; the ADR-0007 lint **must** include every `FTelemetry*` type name and `FPlaytestTelemetryCore`.
2. The telemetry section **must** be exported only after termination (guard: phase `Ended` or `Reset`, or Round's `SessionEnded` latch set), by the Event Log store's disk writer, off the game thread from an owned snapshot (synchronously only for `SessionEnded`), via a `.tmp` file renamed on completion.
3. Probe call sites **must** be on the path every pawn takes and **must not** be conditional on controller kind, team or participant state; producers and detectors **must never** pass or compute a kind or team (witnesses are recorded as refs).
4. Detectors **must** run the same code at the same cadence for every Shared Pawn, read only published server state, keep one exposure clock per subject key, and close every open record on a binding change (subject key = pawn + binding epoch).
5. No gameplay, AI, HUD or presentation code **may** read the telemetry section; tuning **must not** change at runtime from it (CNB-R7).
6. Telemetry **must not** add a replicated property, RPC, tuning key or `USTRUCT`; any such need is a change request to the user.
7. Detector thresholds **must** be code constants under `DetectorVersion`; changing one **must** bump the version.
8. The section **must** be compiled out of Shipping and **must** be a null object when `-StealthTelemetry=0`.
9. The export **must not** contain `DealDigest`, mailbox order, sub-seeds, display names, identity keys, addresses, ping or `AppearanceId` per participant.

## Alternatives Considered

### Alternative 1: New entry types in the R19 ring
- **Description**: Add `BraceEnded`, `RunBout`, `Dwell` and tell entries to the existing Event Log ring.
- **Pros**: One container; Post-Round Reveal could use the same entries.
- **Cons**: Thousands of telemetry entries would compete with the kill timeline under R19's oldest-first truncation; telemetry is `NeverReleased` while most ring entries are `PostRoundEligible`, so the Reveal's release filter would have to split them per entry; it changes the Round GDD's entry schema (a GDD change for every new hook).
- **Rejection Reason**: Eviction risk and release-class mixing; a separate section of the same store keeps the single-writer and plain-C++ guarantees without either cost.

### Alternative 2: Write telemetry through a dedicated log category or file logger
- **Description**: `UE_LOGF(LogStealthTelemetry, …)` to a separate log file, parsed offline.
- **Pros**: Trivial to implement; streaming, survives crashes.
- **Cons**: Writes mid-round to a host-readable file and the output log; violates R19 and ADR-0007 §6 directly.
- **Rejection Reason**: Forbidden by the locked logging policy.

### Alternative 3: Engine analytics providers (`AnalyticsCSV`, `AnalyticsLog`, `AnalyticsET`) or Insights custom trace channels
- **Description**: Record events through `IAnalyticsProvider` or a custom trace channel and export with engine tools.
- **Pros**: Existing tooling; Insights timelines.
- **Cons**: They record as events happen (mid-round, host-readable; ET uploads); string-keyed attributes require text conversion of the data outside the single writer; trace captures are host-readable during play (R19).
- **Rejection Reason**: Breaks "export only after termination" and "only one text conversion"; the Alpha Telemetry GDD may revisit an upload provider for post-round files only.

### Alternative 4: Client-side recording (each client records what it saw)
- **Description**: Clients log their own view (e.g. whether an attacker was visible before a strike) and upload after the match.
- **Pros**: Captures perception data the server cannot (what was on screen, device class).
- **Cons**: Needs a new upload RPC or file collection from 10 machines; client files are tamperable; device-class reporting is a new client→server field (schema change).
- **Rejection Reason**: Out of scope for MVP; recorded for the Alpha Telemetry GDD. The input-device mix needed by CNB R3.4 is collected by the playtest coordinator's sheet; a client-reported device field was declined for MVP by the user 2026-10-25.

### Alternative 5: Owners push every tell (no detectors)
- **Description**: Each owner system emits its own tell events for players and NPCs.
- **Pros**: Exact semantics per owner.
- **Cons**: T12/T13, reactions and face-backs have no owner decision point for players (they are emergent motion); owner-emitted NPC tells measure the sampler, not the motion, so r_P and r_N would be measured by different mechanisms.
- **Rejection Reason**: Calibration needs one measurement for both kinds; detectors over published state give that, owners keep only the outcome probes.

## Consequences

### Positive
- D-W1..D-W4, AC-ASN-33, F5, F7 and the CNB F2 re-derivation all become measurable at the first 10-player test, with team-size bucketing built into every record.
- No new leak surface during a round: nothing replicates, logs or traces, and nothing touches disk before termination.
- Player and NPC rates are measured by the same detectors, so calibration compares like with like (TEL-V3).
- The R19 log, its truncation rule and its release classes are unchanged.

### Negative
- The host process holds the telemetry in memory during the round (as it already holds the Event Log); a modified host could read it. Accepted under IVP R10's trusted-playtest rule until ADR-0024.
- Detector definitions are approximations (e.g. witnesses use the server's view of head look and the eye-cone geometry, not what a client actually rendered); analysis must treat them as estimates.
- A crash mid-round loses that round's telemetry (no mid-round write by design).
- Perception and device data (client-side) are not collected before Alpha.

## Risks
- **R-1 A log or trace leak through a convenience diagnostic.** *Mitigation*: T0 lint over all telemetry type names; TEL-V2 leak run per nightly (ADR-0008).
- **R-2 Detector cost creeps past 0.15 ms.** *Mitigation*: staggered cadence, async traces, Insights scope; fall back to fewer witness traces (count only within 15 m) — never to per-kind sampling.
- **R-3 Detector bias between kinds** (e.g. NPC head-look smoothing differs from human Look). *Mitigation*: TEL-V3 parity on identical scripted motion; a detector that fails parity is fixed, not calibrated around.
- **R-4 Ring truncation in flood rounds.** The kill timeline is protected from eviction by Round R19 (synced 2026-10-24), so a flood evicts only non-protected entries. *Mitigation for the residual*: `TruncatedCount` by type exported; a round whose protected entries alone would exceed the ring (not reachable at MVP sizes: the protected count is bounded by kills, transitions and stages) is reported by Round's F8 sanity check.
- **R-5 Export collides with the next round.** *Mitigation*: snapshot-owned task; TEL-V1 timing check.
- **R-6 Analysts over-read small samples.** *Mitigation*: per-bucket reporting, D-W4 rule, scripts emit observations only.

## GDD Requirements Addressed

| GDD System | Requirement | How This ADR Addresses It |
|------------|-------------|--------------------------|
| round-match-framework.md R19 | Event Log server-only, never through logs/traces, written to disk only on `Ended`/`Reset`, release classes | §1 separate `NeverReleased` section in the same store; §6 export rule and single writer; Implementation Guidelines 1-2 |
| round-match-framework.md F7 | Pacing metrics logged per round; shares across rounds; aborts counted | §3 F7 row; §6 export on `Reset` after aborts/voids |
| round-match-framework.md F8 / AQ-16; R19 (TR-round-013) | Event Log memory under the 10-player worst case; archive need; ring bound with the kill timeline protected from eviction and per-type `TruncatedCount` | §4 bounds; §5 (R19's eviction-protected kill timeline adopted, `TruncatedCount` exported, no persisted archive read back) |
| crowd-npc-behaviour.md CNB-R16, CNB-R7, F2 | Tell Matrix rates re-derived offline from logged player rates; never adapted at runtime | §2.2 detectors, §3 calibration row; Implementation Guideline 5 |
| crowd-npc-behaviour.md G4/R8, C3, F6, CR 20 | Logged human reaction times, face-back rates, motion features; one shared pipeline | §2.2 `ReactionDelay`, `FaceBack`, motion windows; §7 analysis |
| assassination-defence-reversal.md AC-ASN-33, OQ-ASN-3/4 | Strikes by outcome, NPC kills per rival kill, reversals, braces, pair trades | `StrikeResolved`, `BraceEnded`; §3 AC-ASN-33 row |
| objectives-chain.md F5 | Stall metrics from the Event Log | §3 F5 row (exported with the round) |
| gdd-cross-review-2026-10-24.md D-W1..D-W4 | Brace timing, run share, final-court camping, team-size bucketing in telemetry before the Telemetry GDD | §3 D-W1..D-W4 rows |

## Performance Implications
- **CPU**: ≤ 0.15 ms mean / ≤ 0.3 ms p99 listen-host GT inside ADR-0022's 2.0 ms residual line; ≤ 0.2 ms GT once per round for the export snapshot; serialisation off the game thread.
- **Memory**: ≤ ~1.1 MB worst case per round in the server process, freed after export.
- **Load Time**: None.
- **Network**: None.

## Migration Plan
The prototype (`Source/StealthGame`) has no Event Log; nothing migrates. Build order: (1) the Event Log store (ADR-0007/Round story) with the telemetry section and null object; (2) the export writer and TEL-V1/V2 tests; (3) the detector core with T1 unit tests and the TEL-V3 T2 parity test; (4) each owner probe with its owner's story; (5) the analysis scripts before the first 10-player test.

## Validation Criteria
- TEL-V1, TEL-V2, TEL-V3 pass (Verification Required).
- `StealthGame.Round.TelemetryExportAfterTermination`: in every phase before termination the export function refuses and no file exists; after `Ended`, after an `Active` abort (`Reset`) and after `SessionEnded` raised **while the phase is still `Active`** exactly one complete `.json` file exists per round (no `.tmp` left).
- TEL-V3 includes a binding-change case: a pawn taken over as an informant mid-bout yields one closed NPC segment and one new player segment, with no record spanning both.
- `StealthGame.Round.TelemetryNoLogLeak` (T0 lint): a seeded `UE_LOGF` with an `FTelemetryStrikeResolved` argument is watched to fail once.
- `StealthGame.Crowd.TellDetectorParity` (T2): identical scripted motion on an NPC pawn and a bot-driven player pawn gives identical counts for every detector.
- A scripted 10-player round's export reproduces hand-computed AC-ASN-33 counts, run share, dwell seconds and the team-size bucket.

**Gate results (2026-10-24, one combined sub-agent call):** TD-ADR **CONCERNS** (6 items) → all **REVISED** in this draft: `SessionEnded` export guard (terminal latch, not a phase; synchronous write) and a SessionEnded-in-Active test case; witnesses recorded as refs and one exposure clock per subject, with kind resolved only in the sink (ADR-0001 sync item); per-subject data keyed by (pawn, binding epoch) with records closed on every binding change and a TEL-V3 case; `Json` dependency recorded as an ADR-0007 sync item; ADR-0016/ADR-0008 sync item for the virtual-input stream; T13 exclusion defined from published state, motion-window table sized for 2 × 30 min, detector core moved to `Features/Telemetry/`. Engine validation (unreal-specialist lens) **MINOR NOTES** → applied: E3 citation corrected (free `UE::Tasks::Launch` at `Task.h:299`), condensed JSON, `ForceUTF8WithoutBOM`, `.tmp` + `IFileManager::Move`, task handle waited in `Deinitialize`, fault report marshalled to the game thread, async trace pattern (E8). Remaining [U]: process-exit task draining, fault-sink thread safety, `ViewBlock` channel present in config (pending user, ADR-0015).

## Related
- Depends on ADR-0002, ADR-0005, ADR-0007, ADR-0008; probes in ADR-0011, ADR-0016, ADR-0017, ADR-0018, ADR-0020; budget line in ADR-0022. Superseded in part by the Alpha Telemetry & Analytics GDD's ADR when it exists (consumer side, upload, consent, Shipping).
- **GDD sync / change requests (not applied by this ADR):** (1) **Round** R19: name the telemetry section as a `NeverReleased` part of the Event Log store and its export on `Reset` after aborts; consider capping `Voided` entries per source per round (or protecting the kill timeline from eviction) — AQ-16 finding. **Answered** by Round R19's 2026-10-24 sync (kill timeline protected from eviction, per-type `TruncatedCount`, telemetry section named `NeverReleased` with export on `Reset` after aborts); §5 and R-4 now follow it; (2) **Assassination** AC-ASN-33 says "the Event Log yields … braces": braces come from the telemetry section of the same store; (3) **Information Visibility Policy** release-class table: add the telemetry section under `NeverReleased`; (4) **Crowd** CR 20: the shared pipeline is this ADR (producer) plus the Alpha Telemetry GDD (consumer); input-device mix stays a manual playtest-sheet item until a client-reported field is approved (would be a schema change, user decision); (5) **ADR-0001**: name "telemetry row selection" as a permitted consumer of the Roster's kind query; (6) **ADR-0007**: `StealthFoundation` gains a private `Json` module dependency, and `StealthGameplay/Features/Telemetry/` is a new feature folder; (7) **ADR-0016** Enables row says ADR-0023 provides "the recorded virtual-input stream" — that stream is ADR-0016 §4.6 + an ADR-0008 T3 harness capture, not part of this export; ADR-0016 and ADR-0008 should say so; (8) **ADR-0022** §4.2 already books this ADR's 0.15 ms inside the residual 2.0 ms line.

## Amendments
Architecture-review fix pass (`docs/architecture/architecture-review-2026-10-25.md`); decision unchanged, Status unchanged (Proposed).
- §1, Architecture Diagram, §5, R-4, GDD Requirements row, Related item (1): aligned with Round R19 (synced 2026-10-24). The kill timeline (`KillResolved`, `Reversal`, `StateTransition`, `StrikeLimit`, `ObjectiveStage`, `FinalCompleted`, `WinLatched`) is protected from ring eviction, only non-protected entries are truncated, and per-type `TruncatedCount` is exported; the Round change request is marked answered. TR-round-013 is now fully addressed by this ADR — architecture-review fix 2026-10-25 (TR-round-013; other finding).
- Requests answered (2026-10-24): Related items (5) ADR-0001 telemetry row selection, (6) ADR-0007 `Json` dependency and `Features/Telemetry/` folder, and (7) ADR-0016/ADR-0008 virtual-input stream attribution are applied by their owners. Item (8) needs no change. The GDD sync items (2)-(4) remain with the GDD owners — architecture-review fix 2026-10-25 (Conflict 9).

User decisions (2026-10-25); decision unchanged in substance, Status unchanged (Accepted).
- Alternative (client-reported data) — user decision 2026-10-25: a client-reported input-device field is declined for MVP; the manual playtest sheet stays.
- Evidence playtests — user decision 2026-10-25: a headless (dedicated-style) evidence server is deferred to Alpha; the IVP R10 non-competing-host rule stands (AC-IVP-36).
- Scope — user decision 2026-10-25: design-theory D-W1..D-W3 scope options are deferred until telemetry from the first playtest (the probes of §2.2 supply it).
- user decision 2026-10-25: `NpcCount` default 50 (was ~40, user-locked default changed by the user), so 5v5 has 60 characters; CPU line restated at 60 pawns.

Technical Setup 2026-10-25 (Technical Director gate findings; Pings traceability TR-png-*); decision unchanged, Status unchanged (Accepted).
- §2 probes (Informant Pings, system 20, PNG-R5 and AC-PNG-13): the Pings service keeps server-only round counters of accepted pings and of drops by reason (`Phase`, `Latch`, `State`, `Pawn`, `Concealed`, `Range`, `NonFinite`, `Bucket`), each split by the sender's role class (`Active` / `Informant`), and appends `PingAccepted {RoundTimeMs, Subject, Point}` rows to the telemetry section (`Team` and `RoleClass` are resolved by the sink from the `FSubjectRef` at write time, §2.3; the producer never computes them; the drop counters are aggregates held in the Pings service from Preparing P3 and copied into the section at export, so `Phase` drops outside `Active` are kept; corrected by architecture review 2026-10-25b) (a `NeverReleased` part of the Event Log, exported only after `Ended` like every other probe). Rows carry no display name, identity key or participant id in any client-visible form; the sender is passed as `FSubjectRef` like every §2.1 probe, so the export can join rows to positions so AC-PNG-13's follow-up share (an ally pawn within 8 m of the ring centre within 10 s) can be computed offline. Nothing is sent to clients and nothing goes through `UE_LOG`.
