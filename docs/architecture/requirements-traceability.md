# Architecture Traceability Index
Last Updated: 2026-10-25 (architecture review 2026-10-25b: full re-run; 13 new TR-IDs; coverage re-verified row by row)
Engine: Unreal Engine 5.8 (installed 5.8.4)
Source review: `docs/architecture/architecture-review-2026-10-25b.md` (full mode, workflow full; supersedes the counts of `architecture-review-2026-10-25.md`)

## Coverage Summary
- Total requirements: 214 (201 from tr-registry v3, including the 24 Pings IDs; **13 new IDs registered by the 2026-10-25b review**: TR-roster-013/014, TR-tdc-012, TR-session-011/012, TR-scl-015, TR-lsc-010, TR-crowd-011, TR-con-012, TR-asn-012/013, TR-tmk-007, TR-hud-008)
- Covered by an Accepted ADR (✅): **193 (90 %)** — all 23 ADRs Accepted 2026-10-25 (several conditional on spikes; see each ADR's Status and `control-manifest.md`)
- Covered by a Proposed ADR (🟡): 0
- Partial (⚠️): **21** — the previous index's "201/201 covered" overstated coverage: the 2026-10-25b row-by-row check found ADR text that covers only part of 18 existing rows, and 3 of the new rows are partial (see Known Gaps)
- Gaps (❌): **0** — two requirements with no ADR (TR-roster-013 `PublicHandle` random source, Foundation; the TR-scl-015 idle-gaze stream, Core) were closed in-review by an append-only ADR-0006 amendment (domains 15 and 16)

"Accepted" here means the ADR text is the governing decision. A conditionally accepted ADR still gates implementation on its named spikes; stories inherit those conditions through `/story-readiness`.

## Full Matrix

| Requirement ID | GDD | System | Requirement | ADR Coverage | Status |
|---|---|---|---|---|---|
| TR-tdc-001 | tuning-data-config.md | Tuning | Every gameplay number is a key in one cooked data-asset class of per-owner sub-structs; presets listed by a catalog | ADR-0004 | ✅ Accepted |
| TR-tdc-002 | tuning-data-config.md | Tuning | C++ schema row per key (type, default, hard range, audience, latch); editor metadata is a mirror; schema defaults pinned to GDD defaults by test | ADR-0004, ADR-0008 | ✅ Accepted |
| TR-tdc-003 | tuning-data-config.md | Tuning | Server-only resolve/validate/freeze at `Preparing` with the CR-6 ladder and fallback rungs (`FallbackLevel`, `ValidationReport`) | ADR-0004 | ✅ Accepted |
| TR-tdc-004 | tuning-data-config.md | Tuning | Cross-field invariants reject or warn, never clamp; safe ranges linted against all invariants at other keys' defaults | ADR-0004 | ✅ Accepted |
| TR-tdc-005 | tuning-data-config.md | Tuning | Immutable `ValidatedTuning` per round; `TuningHash` (server-only) and `ClientHash` (public keys) | ADR-0004 | ✅ Accepted |
| TR-tdc-006 | tuning-data-config.md | Tuning | ServerOnly/All audience split; client snapshot replicated; client face typed to the public struct (compile-time guarantee) | ADR-0004, ADR-0007 | ✅ Accepted |
| TR-tdc-007 | tuning-data-config.md | Tuning | World-scoped accessor correct in multi-client PIE; no per-pawn/team/viewer parameter | ADR-0004 | ✅ Accepted |
| TR-tdc-008 | tuning-data-config.md | Tuning | Latch classes Session/Round/Action/Use honoured by every consumer | ADR-0004 | ✅ Accepted |
| TR-tdc-009 | tuning-data-config.md | Tuning | No back doors: no `Config` properties, no ini/CVar overrides in Shipping; Shipping always resolves `Default` | ADR-0004 | ✅ Accepted |
| TR-tdc-010 | tuning-data-config.md | Tuning | PIE-only hot-reload re-validation, debounced, routed to the PIE server world | ADR-0004 | ✅ Accepted |
| TR-tdc-011 | tuning-data-config.md | Tuning | Client tuning check rides Session's single acknowledgement (no TDC RPC/timer) | ADR-0004 | ✅ Accepted |
| TR-tdc-012 | tuning-data-config.md | Tuning | Soft-reference asset keys (`*Asset` suffix, e.g. Crowd StateTree and motion-profile assets) have a schema type, a canonical `TuningHash` form and a validator rule | ADR-0004 | ⚠️ Partial (ADR-0004 schema types, `typeTag` and canonical hash form cover no soft reference) |
| TR-roster-001 | participant-state-roster.md | Roster | Game-instance store of `ParticipantRecord`s surviving seamless and hard travel; `BeginMatch`/`EndMatch` | ADR-0005 | ✅ Accepted |
| TR-roster-002 | participant-state-roster.md | Roster | Single writer of lifecycle state through `RequestTransition` with the legal-transition table and result precedence; batch-only reset/end | ADR-0005 | ⚠️ Partial (legal-transition table, R8 result precedence and batch-only reset are GDD content housed in ADR-0005's store; no ADR states them) |
| TR-roster-003 | participant-state-roster.md | Roster | Game-thread-only mutation; events queued and dispatched FIFO after the outermost call; `EventSeq` +1 per event | ADR-0002, ADR-0005 | ⚠️ Partial (`EventSeq` +1 per event unstated) |
| TR-roster-004 | participant-state-roster.md | Roster | Pawn binding states with bind-before-visible spawn and crowd-adoption paths; tombstones | ADR-0010 | ✅ Accepted |
| TR-roster-005 | participant-state-roster.md | Roster | O(1) allocation-free server queries; on clients return 0/false and report to the fault sink | ADR-0010 | ⚠️ Partial (O(1) lookup structure and the client fault-sink rule unstated) |
| TR-roster-006 | participant-state-roster.md | Roster | Carriers C-Public (PlayerState allowlist), C-TeamView (owner-only PlayerController), C-Server (never replicated) | ADR-0001 | ✅ Accepted |
| TR-roster-007 | participant-state-roster.md | Roster | Engine PlayerState defaults disabled or constant; frozen in-round `PlayerArray`; no PlayerState byte change on a transition | ADR-0001 | ✅ Accepted |
| TR-roster-008 | participant-state-roster.md | Roster | Opponent counts never leave the server before `Ended` | ADR-0001, ADR-0002 | ⚠️ Partial (only implied by ADR-0002 §8 `TeamSummary` and ADR-0001 §5) |
| TR-roster-009 | participant-state-roster.md | Roster | All presentation, incl. the listen host, reads only C-TeamView and public structs | ADR-0001, ADR-0007 | ✅ Accepted |
| TR-roster-010 | participant-state-roster.md | Roster | Disconnect/reclaim keyed by IdentityKey; epoch token and connection generation guard every timer | ADR-0005 | ✅ Accepted |
| TR-roster-011 | participant-state-roster.md | Roster | Pawn replication configuration identical for NPC and player and invariant under possession | ADR-0001 | ✅ Accepted |
| TR-iif-001 | input-interaction-framework.md | IIF | One non-replicated command component per pawn with the seven entry points; no caller identity anywhere | ADR-0011 | ✅ Accepted |
| TR-iif-002 | input-interaction-framework.md | IIF | Human Move/Look through CMC saved moves; AI through the same entry points; one control rotation, body-yaw rule and pitch path | ADR-0010, ADR-0011 | ✅ Accepted |
| TR-iif-003 | input-interaction-framework.md | IIF | Discrete commands as Enhanced Input edges; the Hold trigger never carries server meaning | ADR-0011, ADR-0021 | ✅ Accepted |
| TR-iif-004 | input-interaction-framework.md | IIF | Two-stage server validation (ingress with token buckets; command check at the queue slot) | ADR-0011 | ✅ Accepted |
| TR-iif-005 | input-interaction-framework.md | IIF | Hold timers as queue items from server time; 0.10 s release debounce for every pawn | ADR-0011, ADR-0002 | ✅ Accepted (primary ADR-0011 §5; review 2026-10-25b) |
| TR-iif-006 | input-interaction-framework.md | IIF | One action slot per pawn, priority Attack > Prepare > Interact > Hide | ADR-0011 | ✅ Accepted (ADR-0011 §4; was mis-attributed to ADR-0002) |
| TR-iif-007 | input-interaction-framework.md | IIF | Exactly one owner reply per resolution-voided command; ingress rejections silent | ADR-0002, ADR-0011 | ✅ Accepted |
| TR-iif-008 | input-interaction-framework.md | IIF | Server-only action hub with `OnActionStarted/Cancelled/Completed` in one world-level hub | ADR-0011 | ✅ Accepted |
| TR-iif-009 | input-interaction-framework.md | IIF | Replicated fixed-size `FActionState` with one RepNotify; nothing else per action replicated except owner data | ADR-0003 | ✅ Accepted |
| TR-iif-010 | input-interaction-framework.md | IIF | Attack payload from the camera aim ray; no lock-on, no target-dependent presentation | ADR-0011 | ✅ Accepted |
| TR-iif-011 | input-interaction-framework.md | IIF | Input contexts swap on owner state (UX only); AI subject to the same token buckets | ADR-0011 | ✅ Accepted |
| TR-iif-012 | input-interaction-framework.md | IIF | Input latency budget (F5) met for human commands | ADR-0011 | ✅ Accepted |
| TR-cpa-001 | character-pool-appearance.md | CPA | Pool as one cooked data asset of appearance rows; 6-8 models, ≤ 24 appearances, stable `AppearanceId` | ADR-0012 | ✅ Accepted |
| TR-cpa-002 | character-pool-appearance.md | CPA | `AllocateAppearances` once per round; balanced deck, model-stratified subset, unbiased Fisher-Yates | ADR-0012 | ✅ Accepted |
| TR-cpa-003 | character-pool-appearance.md | CPA | Server-private streams from `GetServerSubSeed(AppearanceDeal)` (`FSecretStream`, SipHash counter mode) for Replace; `Take` reserved (no draw) | ADR-0006, ADR-0012 | ✅ Accepted |
| TR-cpa-004 | character-pool-appearance.md | CPA | Appearance before first replication, immutable per pawn; deck conserved; `DrawReplacement` same tick; `TakeNpcCard` on adoption | ADR-0012 | ✅ Accepted |
| TR-cpa-005 | character-pool-appearance.md | CPA | All pool assets resident and precached before leaving `Preparing`; no first-draw load | ADR-0012, ADR-0022 | ✅ Accepted |
| TR-cpa-006 | character-pool-appearance.md | CPA | One look path for every viewer and pawn kind; no keyed material parameters | ADR-0001, ADR-0012 | ✅ Accepted |
| TR-cpa-007 | character-pool-appearance.md | CPA | Statistical indistinguishability gate G-DECK automated | ADR-0008 | ✅ Accepted |
| TR-session-001 | session-travel.md | Session | Listen-server direct join; ordered admission gates at PreLogin; capacity `2 × TeamSize` | ADR-0005 | ✅ Accepted |
| TR-session-002 | session-travel.md | Session | Persistent 128-bit IdentityKey per install with launch/PIE override | ADR-0005 | ✅ Accepted |
| TR-session-003 | session-travel.md | Session | Round travel by seamless `ServerTravel ?Seed=` with the Roster reset ordering | ADR-0005 | ✅ Accepted |
| TR-session-004 | session-travel.md | Session | Public seed vs server-secret keyed sub-seeds | ADR-0006 | ✅ Accepted |
| TR-session-005 | session-travel.md | Session | Single round acknowledgement (seed, LayoutAttempt, layout hash, public-key `ClientHash`, pool hash; `TuningHash` never replicated) within the travel deadline, then the second-stage `ServerWorldReady` / `OnParticipantWorldReady` | ADR-0005 | ✅ Accepted |
| TR-session-006 | session-travel.md | Session | Liveness silence clock, rejoin window, supersede, client auto-rejoin | ADR-0005 | ✅ Accepted |
| TR-session-007 | session-travel.md | Session | Session never spawns/frees pawns or PlayerStates on connection events; default login pawn disabled | ADR-0005 | ✅ Accepted |
| TR-session-008 | session-travel.md | Session | Host parity: the host's view reads the replicated view path | ADR-0001 | ✅ Accepted |
| TR-session-009 | session-travel.md | Session | No host migration; session-ending notice; client host-loss detection | ADR-0005 | ✅ Accepted |
| TR-session-010 | session-travel.md | Session | Leak-surface checklist (PlayerState fields, ping, join timing) enforced via the allowlist | ADR-0001, ADR-0005, ADR-0006 | ✅ Accepted (items c and e: ADR-0005 §1, ADR-0006 `SpawnOrder`) |
| TR-session-011 | session-travel.md | Session | 128-bit `RejoinToken` from server OS entropy, delivered by owner-only RPC, required on rejoin and supersede (Session rule 6) | ADR-0005 | ✅ Accepted |
| TR-session-012 | session-travel.md | Session | Engine connection timeouts as backstop: `ConnectionTimeout >= max(SessionHardTimeoutSec, T_prep_max) + 30`, `InitialConnectTimeout >= PendingLoginTimeoutSec + 10`, validated at `OpenSession` (Session rule 19, F7) | ADR-0005 | ✅ Accepted (120 s still to be configured) |
| TR-round-001 | round-match-framework.md | Round | Five-phase machine in a game-instance store surviving travel | ADR-0005 | ✅ Accepted |
| TR-round-002 | round-match-framework.md | Round | One queue per round, `Key = (ServerTickIndex, Seq)`, per-source mailboxes, permuted intake | ADR-0002 | ✅ Accepted |
| TR-round-003 | round-match-framework.md | Round | Drain once per frame in `TG_PostPhysics`; due-time heap; resolvers per item type; inline follow-ons; Roster events flushed before the next item | ADR-0002 | ✅ Accepted |
| TR-round-004 | round-match-framework.md | Round | Single-assignment latch and the two-step round end (latch step; round-end step at the deciding strike's death write) | ADR-0002 | ✅ Accepted |
| TR-round-005 | round-match-framework.md | Round | Objective and elimination wins; no draw, no clock | ADR-0002 | ✅ Accepted |
| TR-round-006 | round-match-framework.md | Round | Preparing readiness sequence, spawn gate and watchdog | ADR-0005, ADR-0014 | ⚠️ Partial (Round P1-P7 sequencer, `PreparingWatchdogSec`, abort codes and the P5b gate in no ADR) |
| TR-round-007 | round-match-framework.md | Round | Disconnect policy: blend hand-over, guarded grace timer, forfeit/void | ADR-0005, ADR-0010 | ⚠️ Partial (grace/forfeit/void decision logic deferred to the GDD by ADR-0002 §8.1) |
| TR-round-008 | round-match-framework.md | Round | Server-only Event Log ring with release classes; never through logs or traces | ADR-0007 | ⚠️ Partial (Event Log ring entry types, release classes and writer interface have no owning ADR) |
| TR-round-009 | round-match-framework.md | Round | `FRoundPublicState` identical bytes for every viewer; `ActiveAtServerTime` zeroed in Active; bounded late-loader reveal | ADR-0003 | ⚠️ Partial (byte-identical public round struct details beyond ADR-0003 §6) |
| TR-round-010 | round-match-framework.md | Round | Host commands on the game instance with a net-mode check | ADR-0005 | ✅ Accepted |
| TR-round-011 | round-match-framework.md | Round | Single shuffled spawn list in one frame, server-secret order | ADR-0006 | ✅ Accepted |
| TR-round-012 | round-match-framework.md | Round | Playtest instrumentation (brace timing, run share, final camping, team-size bucket) in the Event Log | ADR-0023 | ✅ Accepted |
| TR-ivp-001 | information-visibility-policy.md | IVP | `CanViewerKnow` server query, default deny, seven viewer classes | ADR-0001 | ⚠️ Partial (`CanViewerKnow` signature, viewer classes and fact matrix defined only in architecture.md API Boundaries) |
| TR-ivp-002 | information-visibility-policy.md | IVP | One carrier kind per audience | ADR-0001 | ✅ Accepted |
| TR-ivp-003 | information-visibility-policy.md | IVP | UE default overrides of R5 (pawn links, Instigator, possession ForceNetUpdate, RemoteViewPitch16, spectator) | ADR-0001 | ✅ Accepted |
| TR-ivp-004 | information-visibility-policy.md | IVP | Concealment hybrid: quantised server cull for non-allies + client presentation hide | ADR-0003 | ✅ Accepted |
| TR-ivp-005 | information-visibility-policy.md | IVP | Relevancy varies by concealment and relation only | ADR-0003 | ✅ Accepted |
| TR-ivp-006 | information-visibility-policy.md | IVP | Single allowlist file, reflection audit and net-config audit on defaults and live instances | ADR-0003, ADR-0008 | ✅ Accepted |
| TR-ivp-007 | information-visibility-policy.md | IVP | Leak catalogue with CRITICAL ship gate; per-commit canaries | ADR-0003, ADR-0008 | ✅ Accepted |
| TR-ivp-008 | information-visibility-policy.md | IVP | Engine spectator state banned; client-local no-pawn view | ADR-0001 | ✅ Accepted |
| TR-ivp-009 | information-visibility-policy.md | IVP | Teammate fallback position feed on C-TeamView | ADR-0003 | ✅ Accepted |
| TR-ivp-010 | information-visibility-policy.md | IVP | Iris rerun gate if Iris is enabled | ADR-0003, ADR-0008 | ✅ Accepted |
| TR-ivp-011 | information-visibility-policy.md | IVP | Presentation modules cannot include server query headers | ADR-0001, ADR-0007 | ✅ Accepted |
| TR-lsc-001 | level-socket-contract.md | LSC | Layout service world subsystem with registered providers; Round talks only to it | ADR-0014 | ✅ Accepted |
| TR-lsc-002 | level-socket-contract.md | LSC | Integer Layout Manifest; clients build with no RNG and recompute `LayoutHash` | ADR-0014 | ✅ Accepted |
| TR-lsc-003 | level-socket-contract.md | LSC | Zero replicated actors; bush/court components built locally | ADR-0003, ADR-0014 | ✅ Accepted |
| TR-lsc-004 | level-socket-contract.md | LSC | Server nav build, Validate and Select within attempt/total budgets; ≤ 33 ms slices | ADR-0014 | ✅ Accepted |
| TR-lsc-005 | level-socket-contract.md | LSC | Reseed and graybox fallback travel | ADR-0014 | ✅ Accepted |
| TR-lsc-006 | level-socket-contract.md | LSC | Shared emergence sockets; `PickRespawnPawn` of an unseen crowd pawn | ADR-0014, ADR-0019 | ✅ Accepted |
| TR-lsc-007 | level-socket-contract.md | LSC | Round-start placement and spawn gate | ADR-0014 | ⚠️ Partial (spawn/relevancy gate before `Connected` unspecified; only the possession half is in ADR-0010 §3.2) |
| TR-lsc-008 | level-socket-contract.md | LSC | Pillar-1 placement constraints (no area type for spawns/objectives) | ADR-0014 | ✅ Accepted |
| TR-lsc-009 | level-socket-contract.md | LSC | Server-only query API for site assignment and team spawns | ADR-0014 | ✅ Accepted |
| TR-lsc-010 | level-socket-contract.md | LSC | Per-court public `ClueTag` as a pure function of published geometry, rule version in `GeneratorKitVersion`, V-CLUE Reject predicate reading `ClueMatchMaxCount` | ADR-0014, ADR-0020 | ⚠️ Partial (ADR-0014 exposes `ClueTag` but lists no V-CLUE predicate) |
| TR-scl-001 | shared-character-locomotion.md | SCL | One Shared Pawn class; one spawn factory; takeover function | ADR-0010 | ✅ Accepted |
| TR-scl-002 | shared-character-locomotion.md | SCL | Disconnect never destroys a pawn (`PawnLeavingGame` override, same-frame AI possession) | ADR-0010 | ✅ Accepted |
| TR-scl-003 | shared-character-locomotion.md | SCL | Possession parity and stop-and-blend hand-over; possession causes no forced net update of its own (`ForceNetUpdate` suppressed) | ADR-0003, ADR-0010 | ✅ Accepted |
| TR-scl-004 | shared-character-locomotion.md | SCL | CMC locomotion, speed tiers, state restrictions, replicated traversal set (spike) | ADR-0010 | ✅ Accepted |
| TR-scl-005 | shared-character-locomotion.md | SCL | One look path; `RemoteViewPitch16` written from `Look` for every pawn | ADR-0001, ADR-0003 | ⚠️ Partial (SCL R8 `HeadLook` filter placed in the pawn tick by ADR-0001/0016, but its rules have no owning ADR) |
| TR-scl-006 | shared-character-locomotion.md | SCL | One AnimBP, motion-matching database, chooser and pose set; `ActionPose`-driven states | ADR-0013 | ✅ Accepted |
| TR-scl-007 | shared-character-locomotion.md | SCL | Significance-only tiers by screen height; caps; client-local tie key; server always-ticks pose | ADR-0013 | ✅ Accepted |
| TR-scl-008 | shared-character-locomotion.md | SCL | ≤ 4 ms animation at 60 characters; ≤ 36 KB/s pawn replication per client (working values, `NpcCount` 50) | ADR-0013, ADR-0003 | ✅ Accepted (bandwidth line is ADR-0003, not ADR-0022) |
| TR-scl-009 | shared-character-locomotion.md | SCL | Net parity config (15 Hz fixed, priority 3.0, `DORM_Never`, no cull distance, no pawn multicast) | ADR-0003 | ✅ Accepted |
| TR-scl-010 | shared-character-locomotion.md | SCL | Smoothing parity and input-starvation rule | ADR-0003, ADR-0010 | ✅ Accepted |
| TR-scl-011 | shared-character-locomotion.md | SCL | Footstep path parity from one step clock | ADR-0013 | ✅ Accepted |
| TR-scl-012 | shared-character-locomotion.md | SCL | Published server state (`IsRunning`, `IsInTraversal`, commanded displacement) | ADR-0010 | ✅ Accepted |
| TR-scl-013 | shared-character-locomotion.md | SCL | One capsule, collision profile and physics asset | ADR-0010 | ✅ Accepted |
| TR-camera-001 | camera-system.md | Camera | Local-only toggle; nothing replicated about camera mode | ADR-0003, ADR-0015 | ✅ Accepted |
| TR-camera-002 | camera-system.md | Camera | View envelope −60/+40 in both modes relative to body facing | ADR-0015 | ✅ Accepted |
| TR-camera-003 | camera-system.md | Camera | 3P line-of-sight rule: boom geometry + eye-origin visibility mask hiding mesh, shadow and RT | ADR-0015, ADR-0022 | ✅ Accepted |
| TR-camera-004 | camera-system.md | Camera | Mask reads geometry only; α = 0 counts as not rendered for IIF, SCL and the strike hint | ADR-0015 | ✅ Accepted |
| TR-camera-005 | camera-system.md | Camera | One shared `GetAimRay` | ADR-0015 | ✅ Accepted |
| TR-camera-006 | camera-system.md | Camera | `GetViewState` for HUD/markers; no-pawn view states | ADR-0015, ADR-0021 | ✅ Accepted |
| TR-camera-007 | camera-system.md | Camera | Audio listener at the eye pivot | ADR-0015 | ✅ Accepted |
| TR-camera-008 | camera-system.md | Camera | Eye-mask trace cost bounded at 60 pawns: the same line-of-sight traces at every range in rate bands (60 Hz within EyeMaskNearCm, 15 Hz to EyeMaskRangeCm, 5 Hz beyond), never a distance cutoff (CS-R5, F3) | ADR-0015 | ✅ Accepted |
| TR-crowd-001 | crowd-npc-behaviour.md | Crowd | Server-only Crowd Director subsystem | ADR-0016 | ✅ Accepted |
| TR-crowd-002 | crowd-npc-behaviour.md | Crowd | StateTree per NPC (~4 Hz) + follower emitting `Move`/`Look` through the IIF; no path-following driving | ADR-0016 | ✅ Accepted |
| TR-crowd-003 | crowd-npc-behaviour.md | Crowd | Async path queries ≤ 2 starts/frame; navmesh frozen at `OnLayoutReady`; 2 m spatial hash | ADR-0014, ADR-0016 | ✅ Accepted |
| TR-crowd-004 | crowd-npc-behaviour.md | Crowd | Identity blindness: no crowd code reads player/binding/team/state | ADR-0001 | ✅ Accepted |
| TR-crowd-005 | crowd-npc-behaviour.md | Crowd | Counter-based draws from the crowd server sub-seed | ADR-0006 | ✅ Accepted |
| TR-crowd-006 | crowd-npc-behaviour.md | Crowd | ≤ 1.5 ms AI + nav; global-only cuts; never changes replication | ADR-0016 | ✅ Accepted |
| TR-crowd-007 | crowd-npc-behaviour.md | Crowd | Participant-bound AI driving and takeover eligibility supply | ADR-0016 | ✅ Accepted |
| TR-crowd-008 | crowd-npc-behaviour.md | Crowd | Ambient emergence and replacement via shared sockets and `DrawReplacement` | ADR-0016 | ✅ Accepted |
| TR-crowd-009 | crowd-npc-behaviour.md | Crowd | Dormant until `OnRoundActive`; Halt in the round-end step | ADR-0016 | ✅ Accepted |
| TR-crowd-010 | crowd-npc-behaviour.md | Crowd | Baseline and Tell Matrix rates as data with calibration counters | ADR-0016, ADR-0023 | ✅ Accepted |
| TR-crowd-011 | crowd-npc-behaviour.md | Crowd | Follower drives `Move`/`Look` through a virtual input layer with device profiles; a recordable input stream is replayable through a bot client for parity tests (CNB-R3.4, F6, AC-CNB-27/28) | ADR-0016, ADR-0008 | ✅ Accepted |
| TR-con-001 | concealment-bushes.md | Concealment | One component per pawn; server-only record and state machine | ADR-0017 | ✅ Accepted |
| TR-con-002 | concealment-bushes.md | Concealment | Entry/exit legality through IIF predicates; no occupancy check | ADR-0017 | ✅ Accepted |
| TR-con-003 | concealment-bushes.md | Concealment | Hide volumes block non-hiding pawns | ADR-0017 | ✅ Accepted |
| TR-con-004 | concealment-bushes.md | Concealment | Owner-side warp for the enter transition (S-CON-1) | ADR-0017 | ✅ Accepted |
| TR-con-005 | concealment-bushes.md | Concealment | Hide clips scheduled from replicated server start time | ADR-0003, ADR-0017 | ✅ Accepted — `StartServerTime` adopted by user decision 2026-10-25 (ADR-0017 §8 A) |
| TR-con-006 | concealment-bushes.md | Concealment | Fail-closed client latch; hide removes body, attachments, shadow, Lumen and RT | ADR-0017, ADR-0022 | ✅ Accepted |
| TR-con-007 | concealment-bushes.md | Concealment | `IsConcealedFrom` server-only; blind-strike spot zones | ADR-0017 | ✅ Accepted |
| TR-con-008 | concealment-bushes.md | Concealment | Server cull per the hybrid; un-cull in the same server step | ADR-0003, ADR-0017 | ✅ Accepted |
| TR-con-009 | concealment-bushes.md | Concealment | Rustle parity; NPCs through the same path | ADR-0017 | ✅ Accepted |
| TR-con-010 | concealment-bushes.md | Concealment | Concealment log in the Event Log | ADR-0017 | ✅ Accepted |
| TR-asn-001 | assassination-defence-reversal.md | Assassination | Attack resolver re-checks live state at its slot; victim resolution on server capsules | ADR-0002, ADR-0018 | ✅ Accepted (capsule victim resolution: ADR-0018 §2.1-2.2) |
| TR-asn-002 | assassination-defence-reversal.md | Assassination | Defence sampled at Attack execution by key compare; server-only defence component | ADR-0002, ADR-0018 | ✅ Accepted (defence components: ADR-0018 §1.5, §4.1) |
| TR-asn-003 | assassination-defence-reversal.md | Assassination | Outcome-blind `FActionState` at the slot; outcome writes at contact via the pending-write queue | ADR-0003, ADR-0018 | ✅ Accepted |
| TR-asn-004 | assassination-defence-reversal.md | Assassination | Uniform forced net updates for every outcome and victim kind | ADR-0003 | ✅ Accepted |
| TR-asn-005 | assassination-defence-reversal.md | Assassination | Owner-only `FStrikeResult`/`FDefenceResult`; idempotent state-derived presentation | ADR-0018 | ✅ Accepted |
| TR-asn-006 | assassination-defence-reversal.md | Assassination | `OnPawnKilled` emitted inside the slot | ADR-0002 | ✅ Accepted |
| TR-asn-007 | assassination-defence-reversal.md | Assassination | One body sequence for every victim; possession kept through the fall | ADR-0013, ADR-0018 | ✅ Accepted |
| TR-asn-008 | assassination-defence-reversal.md | Assassination | Nothing outcome-caused presented before contact (server clock scheduling) | ADR-0018 | ✅ Accepted |
| TR-asn-009 | assassination-defence-reversal.md | Assassination | No replicated effect actors, no multicast | ADR-0003 | ✅ Accepted |
| TR-asn-010 | assassination-defence-reversal.md | Assassination | Clients present contact-timed outcomes from one RTT-compensated estimate of server time with a stated accuracy budget (ASN-R21/R23, HUD-R6; added after TD-ARCHITECTURE concern 3) | ADR-0002 | ✅ Accepted |
| TR-pen-001 | npc-kill-penalty-informants.md | Penalty | Server strike count and owner tally keyed by `ParticipantId`, round-scoped | ADR-0019 | ✅ Accepted |
| TR-pen-002 | npc-kill-penalty-informants.md | Penalty | Exile on the Nth NPC kill through the Roster; body released to the crowd | ADR-0019 | ✅ Accepted |
| TR-pen-003 | npc-kill-penalty-informants.md | Penalty | Respawn as a due-time-heap item at `T_due`; Absent path for disconnected victims | ADR-0002, ADR-0019 | ✅ Accepted |
| TR-pen-004 | npc-kill-penalty-informants.md | Penalty | Takeover of an unseen crowd pawn in one call stack; retries and widening | ADR-0019 | ✅ Accepted |
| TR-pen-005 | npc-kill-penalty-informants.md | Penalty | Informant-kill invariant: perception identical to an NPC kill | ADR-0001, ADR-0019 | ✅ Accepted |
| TR-pen-006 | npc-kill-penalty-informants.md | Penalty | Owner-only data (tally, `bExiled`, `T_due` on the C-TeamView owner part, user decision 2026-10-25) | ADR-0001, ADR-0019 | ✅ Accepted — carrier decided 2026-10-25 (ADR-0001 §6, ADR-0019 §6) |
| TR-pen-007 | npc-kill-penalty-informants.md | Penalty | Informant restrictions enforced by their owners; Tell row T7 | ADR-0019 | ✅ Accepted |
| TR-pen-008 | npc-kill-penalty-informants.md | Penalty | Event Log rows | ADR-0019 | ✅ Accepted |
| TR-oc-001 | objectives-chain.md | Objectives | Chain data asset of `FObjectiveDef` rows | ADR-0020 | ✅ Accepted |
| TR-oc-002 | objectives-chain.md | Objectives | Court binding from `FSiteAssignment` at `OnLayoutReady`, server-only | ADR-0020 | ✅ Accepted |
| TR-oc-003 | objectives-chain.md | Objectives | `FObjectiveTeamView` rides C-TeamView; pushed at Active entry | ADR-0020 | ✅ Accepted |
| TR-oc-004 | objectives-chain.md | Objectives | Tend predicate reads only site, public occupancy and legality class | ADR-0020 | ✅ Accepted |
| TR-oc-005 | objectives-chain.md | Objectives | Completion gate inside the hold-timer slot; `DeclareObjectiveWin` | ADR-0002, ADR-0020 | ✅ Accepted |
| TR-oc-006 | objectives-chain.md | Objectives | `RivalPrelimCount` with fresh per-publication delay and one global release grid | ADR-0006, ADR-0020 | ✅ Accepted |
| TR-oc-007 | objectives-chain.md | Objectives | Stall-pressure rules (no timer, no reveal) | ADR-0020 | ✅ Accepted |
| TR-citygen-001 | procedural-city-generator.md | Generator | Pure function of attempt seed, kit version and params; asserts on server sub-seeds | ADR-0014 | ✅ Accepted |
| TR-citygen-002 | procedural-city-generator.md | Generator | Integer-only RNG and canonical iteration; golden seeds stable across CPUs/compilers | ADR-0006, ADR-0008 | ✅ Accepted |
| TR-citygen-003 | procedural-city-generator.md | Generator | S1-S12 + G-PRE pipeline on one off-game-thread worker task over pure data, integer pre-validator; client Manifest build and server validation time-sliced | ADR-0014 | ✅ Accepted |
| TR-citygen-004 | procedural-city-generator.md | Generator | ISM per module (Nanite), batch add, static mobility; ≤ 5,000 instances, ≤ 20,000 shapes | ADR-0014, ADR-0022 | ✅ Accepted |
| TR-citygen-005 | procedural-city-generator.md | Generator | Deterministic client decoration without collision | ADR-0014 | ✅ Accepted |
| TR-citygen-006 | procedural-city-generator.md | Generator | Server spawn + nav ≤ 3 s; client build ≤ 3 s p95 | ADR-0014 | ✅ Accepted |
| TR-citygen-007 | procedural-city-generator.md | Generator | Manifest ≤ 3,500 entries | ADR-0014 | ✅ Accepted |
| TR-citygen-008 | procedural-city-generator.md | Generator | Failure modes → reseed or graybox fallback | ADR-0014 | ✅ Accepted |
| TR-tmk-001 | teammate-markers.md | Markers | Marker set only from the local C-TeamView (+ feed); no world iteration | ADR-0001 | ✅ Accepted |
| TR-tmk-002 | teammate-markers.md | Markers | Anchor on the resolved `PawnRef`'s visual root; null semantics | ADR-0021 | ✅ Accepted |
| TR-tmk-003 | teammate-markers.md | Markers | Fill states from C-TeamView state and phase (`RoundEnded` before `Ended` = no change) | ADR-0021 | ✅ Accepted |
| TR-tmk-004 | teammate-markers.md | Markers | Screen-space overlay after post-process, no depth test, constant size, edge clamp | ADR-0021 | ✅ Accepted |
| TR-tmk-005 | teammate-markers.md | Markers | `IsMarkedAlly` and `OnAllyMarkerRetired` client-local API | ADR-0021 | ✅ Accepted |
| TR-tmk-006 | teammate-markers.md | Markers | View gate through Camera `GetViewState` | ADR-0021 | ✅ Accepted |
| TR-tmk-007 | teammate-markers.md | Markers | Marker retire hold aligned to contact via Team-only `RetireAtServerTime` written only in the retiring C-TeamView update (TM-R6, F6) | ADR-0001, ADR-0021 | ✅ Accepted |
| TR-hud-001 | hud.md | HUD | Reads only the three carriers | ADR-0001 | ✅ Accepted |
| TR-hud-002 | hud.md | HUD | Subject-kind parity rule and automated leak suite | ADR-0008 | ✅ Accepted |
| TR-hud-003 | hud.md | HUD | Layer/ZOrder stack after post-process; safe-margin anchors | ADR-0021 | ✅ Accepted |
| TR-hud-004 | hud.md | HUD | Server-clock timing: plates at local contact frame, tally with plate, `PresentedOwnState` | ADR-0018, ADR-0021 | ✅ Accepted |
| TR-hud-005 | hud.md | HUD | Round surfaces from `FRoundPublicState`; rebuild on `RoundEpoch` change | ADR-0021 | ✅ Accepted |
| TR-hud-006 | hud.md | HUD | Client-local settings hooks (scale, reduce motion, high contrast) | ADR-0021 | ✅ Accepted |
| TR-hud-007 | hud.md | HUD | Latency warning threshold (`HudLatencyWarnRttSec`; duplicate retired 2026-10-25) | ADR-0021 | ✅ Accepted |
| TR-hud-008 | hud.md | HUD | HUD cost: game thread <= 0.3 ms, render thread <= 0.2 ms; no widget constructed during `Active` (AC-HUD-22) | ADR-0021 | ✅ Accepted |
| TR-roster-012 | participant-state-roster.md | Roster | Server-only `OnViewRebound` after `RebindView`; feature team views rebuild from it | ADR-0005, ADR-0020 | ✅ Accepted |
| TR-roster-013 | participant-state-roster.md | Roster | `PublicHandle` is a random 32-bit value from a server-only random source, unique in the match, never reused, carrying no order (Roster R3) | ADR-0006, ADR-0001, ADR-0005 | ✅ Accepted (ADR-0006 domain 15 `PublicHandle`, added by review 2026-10-25b; was a gap) |
| TR-roster-014 | participant-state-roster.md | Roster | C-TeamView teammate entry carries Team-audience `bReconnecting` + `ReconnectEndCause`, set on Disconnected during Active and cleared only by an event (Rejoined/GraceExpired/Cleared), never by a timer (Roster R6) | ADR-0001, ADR-0005, ADR-0021 | ✅ Accepted (ADR-0001 struct and §5 text aligned by review 2026-10-25b) |
| TR-iif-013 | input-interaction-framework.md | IIF | `PossessionEpoch` owner-only via `ClientSetPossessionEpoch`; stale epochs rejected at ingress | ADR-0011 | ✅ Accepted |
| TR-round-013 | round-match-framework.md | Round | Event Log ring bound; kill timeline protected from eviction; `TruncatedCount`; telemetry section | ADR-0023, ADR-0007 | ✅ Accepted (re-check 2026-10-25: ADR-0023 §1, §5, R-4 and its GDD row now follow Round R19) |
| TR-ivp-012 | information-visibility-policy.md | IVP | Owner-only clock-exchange RPC pair, timestamps only | ADR-0002 | ✅ Accepted |
| TR-scl-014 | shared-character-locomotion.md | SCL | Engine CMC net settings pinned and boot-tested against SCL keys (I-SCL-15) | ADR-0010, ADR-0004 | ✅ Accepted |
| TR-scl-015 | shared-character-locomotion.md | SCL | Server-side idle gaze layer for every pawn kind after `IdleGazeDelaySec` of unchanged look input (deadzone `IdleGazeLookDeadzoneDegPerSec`); changes only the replicated `HeadLook`; server-only counter stream keyed by stable pawn ordinal (SCL R8.9) | ADR-0006, ADR-0001, ADR-0016 | ⚠️ Partial (stream: ADR-0006 domain 16, added by review 2026-10-25b; layer rules sit in the unowned `HeadLook` filter, see TR-scl-005) |
| TR-con-011 | concealment-bushes.md | Concealment | Unbiased secret-stream spot pick (`BushSpotPick` domain) | ADR-0017, ADR-0006 | ✅ Accepted |
| TR-con-012 | concealment-bushes.md | Concealment | A pawn killed while concealed re-opens at the death write carrying `FDeathState.bDiedConcealed = true`; every non-owner client keeps the body presentation-hidden (CR-10a, CON-R9) | ADR-0003, ADR-0017, ADR-0018 | ✅ Accepted |
| TR-asn-011 | assassination-defence-reversal.md | Assassination | Owner results as entries of the batched `ClientCommandReplies` RPC | ADR-0011, ADR-0018 | ✅ Accepted |
| TR-asn-012 | assassination-defence-reversal.md | Assassination | Server lunge warps to the fixed `WarpPoint`, never re-tracks the victim; `UMotionWarpingComponent::WarpTargets` not replicated, targets set locally from replicated state (ASN-R16) | ADR-0018, ADR-0010 | ✅ Accepted |
| TR-asn-013 | assassination-defence-reversal.md | Assassination | Defence cooldown end carried owner-only as `CooldownEndServerTime` on a PlayerController component (`UDefenceOwnerComponent`, ASN-R18) | ADR-0018 | ✅ Accepted |
| TR-png-001 | informant-pings-team-comms.md | Pings | PNG-R1: a ping is a momentary place-only ground mark shown to the sender's team only for `PingLifetimeSec`; it never attaches to, follows or is computed from a pawn, the server never receives a pawn identity with it, and there is one ping kind | ADR-0001, ADR-0011, ADR-0021 | ✅ Accepted |
| TR-png-002 | informant-pings-team-comms.md | Pings | PNG-R2, PNG-R5 steps 1-2: a sender may ping only in phase `Active` before Round's latch, with Roster `State` `Active` or `Informant`, a bound pawn and a live connection; one rule for Active players and informants (no role branch, identical entry shape) | ADR-0011, ADR-0002 | ✅ Accepted (ADR-0002/0011 Technical Setup amendments) |
| TR-png-003 | informant-pings-team-comms.md | Pings | PNG-R2, PNG-R3, PNG-R5 step 2 (rulings D26, D33): a concealed sender cannot ping; the server uses Concealment's `IsInBushOrTransition` (true for every state but `Outside`, so `Emerging` counts) and the owning client the same predicate, fail-closed; only this refusal gives owner-only feedback (refusal tone + 'Not now' plate), invisible to everyone else | ADR-0011, ADR-0017 | ✅ Accepted (the HUD route for the refusal cue is a non-blocking conflict, review 2026-10-25b) |
| TR-png-004 | informant-pings-team-comms.md | Pings | PNG-R3: `IA_Ping` is one Boolean press action (default trigger) in `IMC_Common` for both Active and Informant contexts, rebindable, never routed through Common UI, no hold/chord/repeat, and it never ends or interrupts an outstanding pawn command | ADR-0011, ADR-0021 | ✅ Accepted |
| TR-png-005 | informant-pings-team-comms.md | Pings | PNG-R3: the ping point is the world query of Camera `GetAimRay()` on `ViewBlock` (pawns and bush foliage ignored; third person re-traced from the eye pivot), dropped at most 300 cm to the first walkable surface; with no hit or no walkable ground nothing is sent | ADR-0015, ADR-0011, ADR-0007 | ✅ Accepted (ADR-0015 CS-R8 line corrected by review 2026-10-25b: own world-only ping-point rule, never `WorldPoint`) |
| TR-png-006 | informant-pings-team-comms.md | Pings | F2: range is measured from the sender pawn's eye pivot on both machines (client `\|P - E\| <= PingRangeCm`; server `<= PingRangeCm + PingRangeSlackCm`), never from the camera; non-finite points are dropped | ADR-0011, ADR-0015 | ✅ Accepted |
| TR-png-007 | informant-pings-team-comms.md | Pings | PNG-R3, edge case C-W32: the client gate reads `PresentedOwnState` and the bound-pawn state, never the raw own `State`; from the own fall start until a body is bound nothing is sent; presses in the strike window are dropped server-side with no token, reply, echo, tone or plate | ADR-0011 | ✅ Accepted |
| TR-png-008 | informant-pings-team-comms.md | Pings | PNG-R4: transport is `ServerTeamPing(FNetTeamPing {FVector_NetQuantize10 Point})`, a reliable Server RPC on the sender's own `AStealthPlayerController`; it is not a pawn command (never `ServerPawnCommand`, the action slot, the resolution queue or `FActionState`), names no pawn, and the listen host uses the same path | ADR-0011, ADR-0002, ADR-0005 | ✅ Accepted (ADR-0002 Technical Setup amendment (outside the drain)) |
| TR-png-009 | informant-pings-team-comms.md | Pings | PNG-R5: server validation in order phase/latch, state/pawn/concealment, range, bucket; the first failure drops silently with the drop reason counted; failures before the bucket step spend no token; no line-of-sight check; acceptance stamps `ServerTime = ServerNow()` and advances the sender's wrapping `PingSeq` | ADR-0011, ADR-0023 | ✅ Accepted (ADR-0011/0023 Technical Setup amendments) |
| TR-png-010 | informant-pings-team-comms.md | Pings | F1: per-participant token bucket (`PingBucketSize`, `PingBucketPerSec`, server tolerance `epsilon = PingBucketPerSec * PingSkewTolSec` with constant 0.1 s), full at `Active` entry, keyed by `ParticipantId` so it survives reconnect, reset only at `OnRosterReset`; the client keeps a strict mirror | ADR-0011, ADR-0004, ADR-0007 | ✅ Accepted (ADR-0007/0011 Technical Setup amendments) |
| TR-png-011 | informant-pings-team-comms.md | Pings | PNG-R6: the carrier is the C-TeamView part `UTeamPingViewComponent` with `FTeamPingView`, a fixed-size array with one slot per team member (slot i = Roster team slot i, never compacted or reordered), each `{SenderHandle, PingSeq, Point, ServerTime}`; no stage, court, site, clue or `RoundEpoch` data; pushed change-only, coalesced per server frame | ADR-0001, ADR-0003 | ✅ Accepted |
| TR-png-012 | informant-pings-team-comms.md | Pings | PNG-R6, PNG-R9, IVP `TeamPing` row: team-only delivery by relevancy (owner-only PlayerController); opponent machines hold no ping part, entry, RPC, widget or sound, the host included when it is an opponent; the part is on the replicated-property allowlist with Team audience only on `AStealthPlayerController` | ADR-0001, ADR-0003 | ✅ Accepted |
| TR-png-013 | informant-pings-team-comms.md | Pings | PNG-R6 (ruling D25): one active ring per sender; a new accepted ping overwrites the sender's slot and replaces its ring at once on every client; the replacing ping still spends a token and nothing is refunded | ADR-0001, ADR-0021 | ✅ Accepted |
| TR-png-014 | informant-pings-team-comms.md | Pings | PNG-R7, F3: a teammate shows a ring for each new `(SenderHandle, PingSeq)` whose age `EstimatedServerNow() - ServerTime` is below `PingLifetimeSec`; age is shown by stepped stroke weight at thirds (never opacity), negative ages clamp to 0, stale arrivals are never shown; the sender sees its ring only from the server echo; one 2D UI tick on arrival | ADR-0021, ADR-0002 | ⚠️ Partial (dedupe, stale-arrival, stroke and age-clamp rules not in ADR-0021) |
| TR-png-015 | informant-pings-team-comms.md | Pings | PNG-R7, VA-PNG-2: rings draw in HUD L0 beneath teammate markers with no depth test, collapse with L0 in the `Neutral` view, and clamp off-screen or behind-camera rings to the safe rectangle as unlabelled edge rings using the Teammate Markers F3 clamp, bias ramp and hysteresis | ADR-0021, ADR-0015 | ✅ Accepted |
| TR-png-016 | informant-pings-team-comms.md | Pings | PNG-R7: the ring label is the sender's C-Public `DisplayName` by `SenderHandle` ('Ally' if unresolved); it is dropped at that ally's `OnAllyMarkerRetired` while the ring stays to expiry; the sender's own screen keeps its own-name label | ADR-0021 | ⚠️ Partial (`DisplayName` lookup, 'Ally' fallback and label drop at `OnAllyMarkerRetired` not in ADR-0021) |
| TR-png-017 | informant-pings-team-comms.md | Pings | PNG-R8: a ping creates no team-feed line, plate, card or counter in the MVP | ADR-0021 | ⚠️ Partial (ADR-0021 feed-line list does not state that Pings add none) |
| TR-png-018 | informant-pings-team-comms.md | Pings | PNG-R9: pinging is invisible outside the team: no pawn gesture, pose, sound, `ActionPose`, replicated pawn field, `ForceNetUpdate` or relevancy change, nothing on `FRoundPublicState`, C-Public or any All-audience carrier, no world audio/VFX; the handler reads only the sender's own pawn (Crowd Tell Matrix Ping row: no tell) | ADR-0011, ADR-0002, ADR-0003, ADR-0001 | ⚠️ Partial (no ADR-0008 parity-test row for AC-PNG-01/-02) |
| TR-png-019 | informant-pings-team-comms.md | Pings | PNG-R10: every ping part is cleared at `OnRosterReset`, at travel (`InvalidateAllBindings`) and in Round's round-end step; clients hide every ring on entering `Ended`; a HUD rebuild marks present entries shown (no replay); a disconnected sender's ring expires normally | ADR-0002, ADR-0001, ADR-0005, ADR-0007, ADR-0021 | ⚠️ Partial (ring rebuild without replay and hide on `Ended` not in ADR-0021) |
| TR-png-020 | informant-pings-team-comms.md | Pings | PNG-R11, PNG-R12: no in-game text chat or in-engine voice in the MVP (no free text on the wire); every ping string avoids the banned vocabulary and the binding label reads 'Ping' | ADR-0021, ADR-0003 | ⚠️ Partial (in-engine voice/chat disabled is stated in no ADR) |
| TR-png-021 | informant-pings-team-comms.md | Pings | F4: ping bandwidth stays within 80 B/s per receiving teammate connection (about 50 B/s steady at defaults) and 0 B to opponent connections; one changed slot costs at most 32 B (per-element delta; a dynamic array without a fast-array serializer is not allowed) | ADR-0003, ADR-0001 | ✅ Accepted |
| TR-png-022 | informant-pings-team-comms.md | Pings | Tuning Knobs: the five keys of owner `InformantPings` (`PingLifetimeSec`, `PingBucketSize`, `PingBucketPerSec`, `PingRangeCm`, `PingRangeSlackCm`) with their audiences and latches, and the cross-field warning I-PNG-1, are registered in the tuning schema | ADR-0004 | ✅ Accepted |
| TR-png-023 | informant-pings-team-comms.md | Pings | PNG-R5, AC-PNG-13: server-only statistics of accepted pings and drops by reason, split by role class, feed playtest telemetry; never per person in any client-visible form | ADR-0023 | ✅ Accepted (ADR-0023 Technical Setup amendment) |
| TR-png-024 | informant-pings-team-comms.md | Pings | Game Feel, AC-PNG-14: own ring visible p95 <= own RTT + 50 ms after the press; a teammate shows it within its one-way delay plus one server frame of acceptance | ADR-0001, ADR-0002, ADR-0008 | ⚠️ Partial (own-ring latency target and echo-only display in no ADR; no ADR-0008 row for AC-PNG-14) |

## Known Gaps
No requirement lacks an ADR (214/214). 21 rows are partial; each closes with a one-paragraph amendment to the named ADR (or a story-level decision where the GDD is normative):

| TR-ID | What is missing | Suggested owner |
|---|---|---|
| TR-tdc-012 | ADR-0004 schema types, `typeTag` and canonical hash form cover no soft reference | ADR-0004 |
| TR-roster-002 | legal-transition table, R8 result precedence and batch-only reset are GDD content housed in ADR-0005's store; no ADR states them | ADR-0005 (name Roster R8 table normative) |
| TR-roster-003 | `EventSeq` +1 per event unstated | ADR-0005 |
| TR-roster-005 | O(1) lookup structure and the client fault-sink rule unstated | ADR-0005 / ADR-0010 |
| TR-roster-008 | only implied by ADR-0002 §8 `TeamSummary` and ADR-0001 §5 | ADR-0001 |
| TR-round-006 | Round P1-P7 sequencer, `PreparingWatchdogSec`, abort codes and the P5b gate in no ADR | ADR-0005 or a Round ADR |
| TR-round-007 | grace/forfeit/void decision logic deferred to the GDD by ADR-0002 §8.1 | ADR-0002 §8.1 / Round ADR |
| TR-round-008 | Event Log ring entry types, release classes and writer interface have no owning ADR | ADR-0023 or a Round Event Log ADR |
| TR-round-009 | byte-identical public round struct details beyond ADR-0003 §6 | ADR-0003 §6 |
| TR-ivp-001 | `CanViewerKnow` signature, viewer classes and fact matrix defined only in architecture.md API Boundaries | ADR-0001 (or a new IVP ADR) |
| TR-lsc-007 | spawn/relevancy gate before `Connected` unspecified; only the possession half is in ADR-0010 §3.2 | ADR-0010 / ADR-0003 |
| TR-lsc-010 | ADR-0014 exposes `ClueTag` but lists no V-CLUE predicate | ADR-0014 |
| TR-scl-005 | SCL R8 `HeadLook` filter placed in the pawn tick by ADR-0001/0016, but its rules have no owning ADR | ADR-0010 (HeadLook filter) |
| TR-scl-015 | stream: ADR-0006 domain 16, added by review 2026-10-25b; layer rules sit in the unowned `HeadLook` filter, see TR-scl-005 | ADR-0010 (HeadLook filter) |
| TR-png-014 | dedupe, stale-arrival, stroke and age-clamp rules not in ADR-0021 | ADR-0021 |
| TR-png-016 | `DisplayName` lookup, 'Ally' fallback and label drop at `OnAllyMarkerRetired` not in ADR-0021 | ADR-0021 |
| TR-png-017 | ADR-0021 feed-line list does not state that Pings add none | ADR-0021 |
| TR-png-018 | no ADR-0008 parity-test row for AC-PNG-01/-02 | ADR-0008 |
| TR-png-019 | ring rebuild without replay and hide on `Ended` not in ADR-0021 | ADR-0021 |
| TR-png-020 | in-engine voice/chat disabled is stated in no ADR | ADR-0003 / ADR-0021 |
| TR-png-024 | own-ring latency target and echo-only display in no ADR; no ADR-0008 row for AC-PNG-14 | ADR-0021 / ADR-0008 |

Closed earlier: TR-con-005 and TR-pen-006 (user decisions 2026-10-25), TR-round-013 (re-check 2026-10-25).

Not traced as TRs (non-GDD sources): art bible 8.x budgets (ADR-0022), game-concept VCS concern and coding standards (ADR-0009).

## Superseded Requirements
None. Reworded in place by the 2026-10-25b review (same IDs, `revised: 2026-10-25`): TR-tdc-008 (four latch classes), TR-session-005 (`ClientHash`, `LayoutAttempt`, `ServerWorldReady`), TR-cpa-003 (`Take` reserved, no draw), TR-citygen-003 (one worker task; client build and validation time-sliced), TR-camera-008 (three rate bands), TR-scl-003 (possession `ForceNetUpdate` suppressed). Earlier: Three baseline texts were registered in their synced wording (no ID change): TR-camera-006 (Camera-published fade drawn by the HUD fade layer, ADR-0021 sync), TR-camera-008 (rate bands at every range, never a cutoff, ADR-0015), TR-iif-002 (Crowd follower issues `Move` by input emulation, ADR-0011/0016 sync).
