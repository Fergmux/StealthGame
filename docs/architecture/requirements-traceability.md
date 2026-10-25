# Architecture Traceability Index
Last Updated: 2026-10-25
Engine: Unreal Engine 5.8 (installed 5.8.4)
Source review: `docs/architecture/architecture-review-2026-10-25.md` (full mode, workflow full; updated by its "Re-check (2026-10-25)" section)

## Coverage Summary
- Total requirements: 177 (170 baseline IDs from architecture.md Appendix A, registered permanent; 7 new from the 2026-10-24 ADR-GDD sync)
- Covered by an Accepted ADR (✅): 0 (0 %) — every ADR-0001..0023 is `Proposed`
- Covered by a Proposed ADR (🟡): 175 (98.9 %)
- Partial (⚠️): 0 — TR-con-005 and TR-pen-006 were closed by the user decisions of 2026-10-25 (`StartServerTime` adopted; `T_due` on the C-TeamView owner part). TR-round-013 moved to 🟡 at the re-check (ADR-0023 amended to Round R19)
- Gaps (❌): 0

🟡 is not a pass state: it caps the verdict at CONCERNS until each ADR is moved to `Accepted` with `/architecture-decision accept ADR-NNNN` (after its spikes and the user's decisions). Stories citing these TR-IDs stay Blocked while their ADR is Proposed.

## Full Matrix

| Requirement ID | GDD | System | Requirement | ADR Coverage | Status |
|---|---|---|---|---|---|
| TR-tdc-001 | tuning-data-config.md | Tuning | Every gameplay number is a key in one cooked data-asset class of per-owner sub-structs; presets listed by a catalog | ADR-0004 | 🟡 Proposed |
| TR-tdc-002 | tuning-data-config.md | Tuning | C++ schema row per key (type, default, hard range, audience, latch); editor metadata is a mirror; schema defaults pinned to GDD defaults by test | ADR-0004, ADR-0008 | 🟡 Proposed |
| TR-tdc-003 | tuning-data-config.md | Tuning | Server-only resolve/validate/freeze at `Preparing` with the CR-6 ladder and fallback rungs (`FallbackLevel`, `ValidationReport`) | ADR-0004 | 🟡 Proposed |
| TR-tdc-004 | tuning-data-config.md | Tuning | Cross-field invariants reject or warn, never clamp; safe ranges linted against all invariants at other keys' defaults | ADR-0004 | 🟡 Proposed |
| TR-tdc-005 | tuning-data-config.md | Tuning | Immutable `ValidatedTuning` per round; `TuningHash` (server-only) and `ClientHash` (public keys) | ADR-0004 | 🟡 Proposed |
| TR-tdc-006 | tuning-data-config.md | Tuning | ServerOnly/All audience split; client snapshot replicated; client face typed to the public struct (compile-time guarantee) | ADR-0004, ADR-0007 | 🟡 Proposed |
| TR-tdc-007 | tuning-data-config.md | Tuning | World-scoped accessor correct in multi-client PIE; no per-pawn/team/viewer parameter | ADR-0004 | 🟡 Proposed |
| TR-tdc-008 | tuning-data-config.md | Tuning | Latch classes Round/Action/Use honoured by every consumer | ADR-0004 | 🟡 Proposed |
| TR-tdc-009 | tuning-data-config.md | Tuning | No back doors: no `Config` properties, no ini/CVar overrides in Shipping; Shipping always resolves `Default` | ADR-0004 | 🟡 Proposed |
| TR-tdc-010 | tuning-data-config.md | Tuning | PIE-only hot-reload re-validation, debounced, routed to the PIE server world | ADR-0004 | 🟡 Proposed |
| TR-tdc-011 | tuning-data-config.md | Tuning | Client tuning check rides Session's single acknowledgement (no TDC RPC/timer) | ADR-0004 | 🟡 Proposed |
| TR-roster-001 | participant-state-roster.md | Roster | Game-instance store of `ParticipantRecord`s surviving seamless and hard travel; `BeginMatch`/`EndMatch` | ADR-0005 | 🟡 Proposed |
| TR-roster-002 | participant-state-roster.md | Roster | Single writer of lifecycle state through `RequestTransition` with the legal-transition table and result precedence; batch-only reset/end | ADR-0005 | 🟡 Proposed |
| TR-roster-003 | participant-state-roster.md | Roster | Game-thread-only mutation; events queued and dispatched FIFO after the outermost call; `EventSeq` +1 per event | ADR-0002 | 🟡 Proposed |
| TR-roster-004 | participant-state-roster.md | Roster | Pawn binding states with bind-before-visible spawn and crowd-adoption paths; tombstones | ADR-0010 | 🟡 Proposed |
| TR-roster-005 | participant-state-roster.md | Roster | O(1) allocation-free server queries; on clients return 0/false and report to the fault sink | ADR-0010 | 🟡 Proposed |
| TR-roster-006 | participant-state-roster.md | Roster | Carriers C-Public (PlayerState allowlist), C-TeamView (owner-only PlayerController), C-Server (never replicated) | ADR-0001 | 🟡 Proposed |
| TR-roster-007 | participant-state-roster.md | Roster | Engine PlayerState defaults disabled or constant; frozen in-round `PlayerArray`; no PlayerState byte change on a transition | ADR-0001 | 🟡 Proposed |
| TR-roster-008 | participant-state-roster.md | Roster | Opponent counts never leave the server before `Ended` | ADR-0001 | 🟡 Proposed |
| TR-roster-009 | participant-state-roster.md | Roster | All presentation, incl. the listen host, reads only C-TeamView and public structs | ADR-0001, ADR-0007 | 🟡 Proposed |
| TR-roster-010 | participant-state-roster.md | Roster | Disconnect/reclaim keyed by IdentityKey; epoch token and connection generation guard every timer | ADR-0005 | 🟡 Proposed |
| TR-roster-011 | participant-state-roster.md | Roster | Pawn replication configuration identical for NPC and player and invariant under possession | ADR-0001 | 🟡 Proposed |
| TR-iif-001 | input-interaction-framework.md | IIF | One non-replicated command component per pawn with the seven entry points; no caller identity anywhere | ADR-0011 | 🟡 Proposed |
| TR-iif-002 | input-interaction-framework.md | IIF | Human Move/Look through CMC saved moves; AI through the same entry points; one control rotation, body-yaw rule and pitch path | ADR-0010, ADR-0011 | 🟡 Proposed |
| TR-iif-003 | input-interaction-framework.md | IIF | Discrete commands as Enhanced Input edges; the Hold trigger never carries server meaning | ADR-0011, ADR-0021 | 🟡 Proposed |
| TR-iif-004 | input-interaction-framework.md | IIF | Two-stage server validation (ingress with token buckets; command check at the queue slot) | ADR-0011 | 🟡 Proposed |
| TR-iif-005 | input-interaction-framework.md | IIF | Hold timers as queue items from server time; 0.10 s release debounce for every pawn | ADR-0002 | 🟡 Proposed |
| TR-iif-006 | input-interaction-framework.md | IIF | One action slot per pawn, priority Attack > Prepare > Interact > Hide | ADR-0002 | 🟡 Proposed |
| TR-iif-007 | input-interaction-framework.md | IIF | Exactly one owner reply per resolution-voided command; ingress rejections silent | ADR-0002, ADR-0011 | 🟡 Proposed |
| TR-iif-008 | input-interaction-framework.md | IIF | Server-only action hub with `OnActionStarted/Cancelled/Completed` in one world-level hub | ADR-0011 | 🟡 Proposed |
| TR-iif-009 | input-interaction-framework.md | IIF | Replicated fixed-size `FActionState` with one RepNotify; nothing else per action replicated except owner data | ADR-0003 | 🟡 Proposed |
| TR-iif-010 | input-interaction-framework.md | IIF | Attack payload from the camera aim ray; no lock-on, no target-dependent presentation | ADR-0011 | 🟡 Proposed |
| TR-iif-011 | input-interaction-framework.md | IIF | Input contexts swap on owner state (UX only); AI subject to the same token buckets | ADR-0011 | 🟡 Proposed |
| TR-iif-012 | input-interaction-framework.md | IIF | Input latency budget (F5) met for human commands | ADR-0011 | 🟡 Proposed |
| TR-cpa-001 | character-pool-appearance.md | CPA | Pool as one cooked data asset of appearance rows; 6-8 models, ≤ 24 appearances, stable `AppearanceId` | ADR-0012 | 🟡 Proposed |
| TR-cpa-002 | character-pool-appearance.md | CPA | `AllocateAppearances` once per round; balanced deck, model-stratified subset, unbiased Fisher-Yates | ADR-0012 | 🟡 Proposed |
| TR-cpa-003 | character-pool-appearance.md | CPA | Server-private streams from `GetServerSubSeed(AppearanceDeal)`; DRBG-class primitive for Replace/Take | ADR-0006, ADR-0012 | 🟡 Proposed |
| TR-cpa-004 | character-pool-appearance.md | CPA | Appearance before first replication, immutable per pawn; deck conserved; `DrawReplacement` same tick; `TakeNpcCard` on adoption | ADR-0012 | 🟡 Proposed |
| TR-cpa-005 | character-pool-appearance.md | CPA | All pool assets resident and precached before leaving `Preparing`; no first-draw load | ADR-0012, ADR-0022 | 🟡 Proposed |
| TR-cpa-006 | character-pool-appearance.md | CPA | One look path for every viewer and pawn kind; no keyed material parameters | ADR-0001, ADR-0012 | 🟡 Proposed |
| TR-cpa-007 | character-pool-appearance.md | CPA | Statistical indistinguishability gate G-DECK automated | ADR-0008 | 🟡 Proposed |
| TR-session-001 | session-travel.md | Session | Listen-server direct join; ordered admission gates at PreLogin; capacity `2 × TeamSize` | ADR-0005 | 🟡 Proposed |
| TR-session-002 | session-travel.md | Session | Persistent 128-bit IdentityKey per install with launch/PIE override | ADR-0005 | 🟡 Proposed |
| TR-session-003 | session-travel.md | Session | Round travel by seamless `ServerTravel ?Seed=` with the Roster reset ordering | ADR-0005 | 🟡 Proposed |
| TR-session-004 | session-travel.md | Session | Public seed vs server-secret keyed sub-seeds | ADR-0006 | 🟡 Proposed |
| TR-session-005 | session-travel.md | Session | Single round acknowledgement (seed, layout hash, tuning hash, pool hash) within the travel deadline | ADR-0005 | 🟡 Proposed |
| TR-session-006 | session-travel.md | Session | Liveness silence clock, rejoin window, supersede, client auto-rejoin | ADR-0005 | 🟡 Proposed |
| TR-session-007 | session-travel.md | Session | Session never spawns/frees pawns or PlayerStates on connection events; default login pawn disabled | ADR-0005 | 🟡 Proposed |
| TR-session-008 | session-travel.md | Session | Host parity: the host's view reads the replicated view path | ADR-0001 | 🟡 Proposed |
| TR-session-009 | session-travel.md | Session | No host migration; session-ending notice; client host-loss detection | ADR-0005 | 🟡 Proposed |
| TR-session-010 | session-travel.md | Session | Leak-surface checklist (PlayerState fields, ping, join timing) enforced via the allowlist | ADR-0001 | 🟡 Proposed |
| TR-round-001 | round-match-framework.md | Round | Five-phase machine in a game-instance store surviving travel | ADR-0005 | 🟡 Proposed |
| TR-round-002 | round-match-framework.md | Round | One queue per round, `Key = (ServerTickIndex, Seq)`, per-source mailboxes, permuted intake | ADR-0002 | 🟡 Proposed |
| TR-round-003 | round-match-framework.md | Round | Drain once per frame in `TG_PostPhysics`; due-time heap; resolvers per item type; inline follow-ons; Roster events flushed before the next item | ADR-0002 | 🟡 Proposed |
| TR-round-004 | round-match-framework.md | Round | Single-assignment latch and the two-step round end (latch step; round-end step at the deciding strike's death write) | ADR-0002 | 🟡 Proposed |
| TR-round-005 | round-match-framework.md | Round | Objective and elimination wins; no draw, no clock | ADR-0002 | 🟡 Proposed |
| TR-round-006 | round-match-framework.md | Round | Preparing readiness sequence, spawn gate and watchdog | ADR-0005, ADR-0014 | 🟡 Proposed |
| TR-round-007 | round-match-framework.md | Round | Disconnect policy: blend hand-over, guarded grace timer, forfeit/void | ADR-0005, ADR-0010 | 🟡 Proposed |
| TR-round-008 | round-match-framework.md | Round | Server-only Event Log ring with release classes; never through logs or traces | ADR-0007 | 🟡 Proposed |
| TR-round-009 | round-match-framework.md | Round | `FRoundPublicState` identical bytes for every viewer; `ActiveAtServerTime` zeroed in Active; bounded late-loader reveal | ADR-0003 | 🟡 Proposed |
| TR-round-010 | round-match-framework.md | Round | Host commands on the game instance with a net-mode check | ADR-0005 | 🟡 Proposed |
| TR-round-011 | round-match-framework.md | Round | Single shuffled spawn list in one frame, server-secret order | ADR-0006 | 🟡 Proposed |
| TR-round-012 | round-match-framework.md | Round | Playtest instrumentation (brace timing, run share, final camping, team-size bucket) in the Event Log | ADR-0023 | 🟡 Proposed |
| TR-ivp-001 | information-visibility-policy.md | IVP | `CanViewerKnow` server query, default deny, seven viewer classes | ADR-0001 | 🟡 Proposed |
| TR-ivp-002 | information-visibility-policy.md | IVP | One carrier kind per audience | ADR-0001 | 🟡 Proposed |
| TR-ivp-003 | information-visibility-policy.md | IVP | UE default overrides of R5 (pawn links, Instigator, possession ForceNetUpdate, RemoteViewPitch16, spectator) | ADR-0001 | 🟡 Proposed |
| TR-ivp-004 | information-visibility-policy.md | IVP | Concealment hybrid: quantised server cull for non-allies + client presentation hide | ADR-0003 | 🟡 Proposed |
| TR-ivp-005 | information-visibility-policy.md | IVP | Relevancy varies by concealment and relation only | ADR-0003 | 🟡 Proposed |
| TR-ivp-006 | information-visibility-policy.md | IVP | Single allowlist file, reflection audit and net-config audit on defaults and live instances | ADR-0003, ADR-0008 | 🟡 Proposed |
| TR-ivp-007 | information-visibility-policy.md | IVP | Leak catalogue with CRITICAL ship gate; per-commit canaries | ADR-0003, ADR-0008 | 🟡 Proposed |
| TR-ivp-008 | information-visibility-policy.md | IVP | Engine spectator state banned; client-local no-pawn view | ADR-0001 | 🟡 Proposed |
| TR-ivp-009 | information-visibility-policy.md | IVP | Teammate fallback position feed on C-TeamView | ADR-0003 | 🟡 Proposed |
| TR-ivp-010 | information-visibility-policy.md | IVP | Iris rerun gate if Iris is enabled | ADR-0003, ADR-0008 | 🟡 Proposed |
| TR-ivp-011 | information-visibility-policy.md | IVP | Presentation modules cannot include server query headers | ADR-0001, ADR-0007 | 🟡 Proposed |
| TR-lsc-001 | level-socket-contract.md | LSC | Layout service world subsystem with registered providers; Round talks only to it | ADR-0014 | 🟡 Proposed |
| TR-lsc-002 | level-socket-contract.md | LSC | Integer Layout Manifest; clients build with no RNG and recompute `LayoutHash` | ADR-0014 | 🟡 Proposed |
| TR-lsc-003 | level-socket-contract.md | LSC | Zero replicated actors; bush/court components built locally | ADR-0003, ADR-0014 | 🟡 Proposed |
| TR-lsc-004 | level-socket-contract.md | LSC | Server nav build, Validate and Select within attempt/total budgets; ≤ 33 ms slices | ADR-0014 | 🟡 Proposed |
| TR-lsc-005 | level-socket-contract.md | LSC | Reseed and graybox fallback travel | ADR-0014 | 🟡 Proposed |
| TR-lsc-006 | level-socket-contract.md | LSC | Shared emergence sockets; `PickRespawnPawn` of an unseen crowd pawn | ADR-0014, ADR-0019 | 🟡 Proposed |
| TR-lsc-007 | level-socket-contract.md | LSC | Round-start placement and spawn gate | ADR-0014 | 🟡 Proposed |
| TR-lsc-008 | level-socket-contract.md | LSC | Pillar-1 placement constraints (no area type for spawns/objectives) | ADR-0014 | 🟡 Proposed |
| TR-lsc-009 | level-socket-contract.md | LSC | Server-only query API for site assignment and team spawns | ADR-0014 | 🟡 Proposed |
| TR-scl-001 | shared-character-locomotion.md | SCL | One Shared Pawn class; one spawn factory; takeover function | ADR-0010 | 🟡 Proposed |
| TR-scl-002 | shared-character-locomotion.md | SCL | Disconnect never destroys a pawn (`PawnLeavingGame` override, same-frame AI possession) | ADR-0010 | 🟡 Proposed |
| TR-scl-003 | shared-character-locomotion.md | SCL | Possession parity and stop-and-blend hand-over; `ForceNetUpdate` deferral | ADR-0003, ADR-0010 | 🟡 Proposed |
| TR-scl-004 | shared-character-locomotion.md | SCL | CMC locomotion, speed tiers, state restrictions, replicated traversal set (spike) | ADR-0010 | 🟡 Proposed |
| TR-scl-005 | shared-character-locomotion.md | SCL | One look path; `RemoteViewPitch16` written from `Look` for every pawn | ADR-0001, ADR-0003 | 🟡 Proposed |
| TR-scl-006 | shared-character-locomotion.md | SCL | One AnimBP, motion-matching database, chooser and pose set; `ActionPose`-driven states | ADR-0013 | 🟡 Proposed |
| TR-scl-007 | shared-character-locomotion.md | SCL | Significance-only tiers by screen height; caps; client-local tie key; server always-ticks pose | ADR-0013 | 🟡 Proposed |
| TR-scl-008 | shared-character-locomotion.md | SCL | ≤ 4 ms animation at 60 characters; ≤ 36 KB/s pawn replication per client (working values, `NpcCount` 50) | ADR-0013, ADR-0022 | 🟡 Proposed |
| TR-scl-009 | shared-character-locomotion.md | SCL | Net parity config (15 Hz fixed, priority 3.0, `DORM_Never`, no cull distance, no pawn multicast) | ADR-0003 | 🟡 Proposed |
| TR-scl-010 | shared-character-locomotion.md | SCL | Smoothing parity and input-starvation rule | ADR-0003, ADR-0010 | 🟡 Proposed |
| TR-scl-011 | shared-character-locomotion.md | SCL | Footstep path parity from one step clock | ADR-0013 | 🟡 Proposed |
| TR-scl-012 | shared-character-locomotion.md | SCL | Published server state (`IsRunning`, `IsInTraversal`, commanded displacement) | ADR-0010 | 🟡 Proposed |
| TR-scl-013 | shared-character-locomotion.md | SCL | One capsule, collision profile and physics asset | ADR-0010 | 🟡 Proposed |
| TR-camera-001 | camera-system.md | Camera | Local-only toggle; nothing replicated about camera mode | ADR-0003, ADR-0015 | 🟡 Proposed |
| TR-camera-002 | camera-system.md | Camera | View envelope −60/+40 in both modes relative to body facing | ADR-0015 | 🟡 Proposed |
| TR-camera-003 | camera-system.md | Camera | 3P line-of-sight rule: boom geometry + eye-origin visibility mask hiding mesh, shadow and RT | ADR-0015, ADR-0022 | 🟡 Proposed |
| TR-camera-004 | camera-system.md | Camera | Mask reads geometry only; α = 0 counts as not rendered for IIF, SCL and the strike hint | ADR-0015 | 🟡 Proposed |
| TR-camera-005 | camera-system.md | Camera | One shared `GetAimRay` | ADR-0015 | 🟡 Proposed |
| TR-camera-006 | camera-system.md | Camera | `GetViewState` for HUD/markers; no-pawn view states | ADR-0015, ADR-0021 | 🟡 Proposed |
| TR-camera-007 | camera-system.md | Camera | Audio listener at the eye pivot | ADR-0015 | 🟡 Proposed |
| TR-camera-008 | camera-system.md | Camera | Eye-mask trace cost bounded at 60 pawns (time slicing beyond 60 m) | ADR-0015 | 🟡 Proposed |
| TR-crowd-001 | crowd-npc-behaviour.md | Crowd | Server-only Crowd Director subsystem | ADR-0016 | 🟡 Proposed |
| TR-crowd-002 | crowd-npc-behaviour.md | Crowd | StateTree per NPC (~4 Hz) + follower emitting `Move`/`Look` through the IIF; no path-following driving | ADR-0016 | 🟡 Proposed |
| TR-crowd-003 | crowd-npc-behaviour.md | Crowd | Async path queries ≤ 2 starts/frame; navmesh frozen at `OnLayoutReady`; 2 m spatial hash | ADR-0014, ADR-0016 | 🟡 Proposed |
| TR-crowd-004 | crowd-npc-behaviour.md | Crowd | Identity blindness: no crowd code reads player/binding/team/state | ADR-0001 | 🟡 Proposed |
| TR-crowd-005 | crowd-npc-behaviour.md | Crowd | Counter-based draws from the crowd server sub-seed | ADR-0006 | 🟡 Proposed |
| TR-crowd-006 | crowd-npc-behaviour.md | Crowd | ≤ 1.5 ms AI + nav; global-only cuts; never changes replication | ADR-0016 | 🟡 Proposed |
| TR-crowd-007 | crowd-npc-behaviour.md | Crowd | Participant-bound AI driving and takeover eligibility supply | ADR-0016 | 🟡 Proposed |
| TR-crowd-008 | crowd-npc-behaviour.md | Crowd | Ambient emergence and replacement via shared sockets and `DrawReplacement` | ADR-0016 | 🟡 Proposed |
| TR-crowd-009 | crowd-npc-behaviour.md | Crowd | Dormant until `OnRoundActive`; Halt in the round-end step | ADR-0016 | 🟡 Proposed |
| TR-crowd-010 | crowd-npc-behaviour.md | Crowd | Baseline and Tell Matrix rates as data with calibration counters | ADR-0016, ADR-0023 | 🟡 Proposed |
| TR-con-001 | concealment-bushes.md | Concealment | One component per pawn; server-only record and state machine | ADR-0017 | 🟡 Proposed |
| TR-con-002 | concealment-bushes.md | Concealment | Entry/exit legality through IIF predicates; no occupancy check | ADR-0017 | 🟡 Proposed |
| TR-con-003 | concealment-bushes.md | Concealment | Hide volumes block non-hiding pawns | ADR-0017 | 🟡 Proposed |
| TR-con-004 | concealment-bushes.md | Concealment | Owner-side warp for the enter transition (S-CON-1) | ADR-0017 | 🟡 Proposed |
| TR-con-005 | concealment-bushes.md | Concealment | Hide clips scheduled from replicated server start time | ADR-0003, ADR-0017 | 🟡 Covered — `StartServerTime` adopted by user decision 2026-10-25 (ADR-0017 §8 A) |
| TR-con-006 | concealment-bushes.md | Concealment | Fail-closed client latch; hide removes body, attachments, shadow, Lumen and RT | ADR-0017, ADR-0022 | 🟡 Proposed |
| TR-con-007 | concealment-bushes.md | Concealment | `IsConcealedFrom` server-only; blind-strike spot zones | ADR-0017 | 🟡 Proposed |
| TR-con-008 | concealment-bushes.md | Concealment | Server cull per the hybrid; un-cull in the same server step | ADR-0003, ADR-0017 | 🟡 Proposed |
| TR-con-009 | concealment-bushes.md | Concealment | Rustle parity; NPCs through the same path | ADR-0017 | 🟡 Proposed |
| TR-con-010 | concealment-bushes.md | Concealment | Concealment log in the Event Log | ADR-0017 | 🟡 Proposed |
| TR-asn-001 | assassination-defence-reversal.md | Assassination | Attack resolver re-checks live state at its slot; victim resolution on server capsules | ADR-0002 | 🟡 Proposed |
| TR-asn-002 | assassination-defence-reversal.md | Assassination | Defence sampled at Attack execution by key compare; server-only defence component | ADR-0002 | 🟡 Proposed |
| TR-asn-003 | assassination-defence-reversal.md | Assassination | Outcome-blind `FActionState` at the slot; outcome writes at contact via the pending-write queue | ADR-0003, ADR-0018 | 🟡 Proposed |
| TR-asn-004 | assassination-defence-reversal.md | Assassination | Uniform forced net updates for every outcome and victim kind | ADR-0003 | 🟡 Proposed |
| TR-asn-005 | assassination-defence-reversal.md | Assassination | Owner-only `FStrikeResult`/`FDefenceResult`; idempotent state-derived presentation | ADR-0018 | 🟡 Proposed |
| TR-asn-006 | assassination-defence-reversal.md | Assassination | `OnPawnKilled` emitted inside the slot | ADR-0002 | 🟡 Proposed |
| TR-asn-007 | assassination-defence-reversal.md | Assassination | One body sequence for every victim; possession kept through the fall | ADR-0013, ADR-0018 | 🟡 Proposed |
| TR-asn-008 | assassination-defence-reversal.md | Assassination | Nothing outcome-caused presented before contact (server clock scheduling) | ADR-0018 | 🟡 Proposed |
| TR-asn-009 | assassination-defence-reversal.md | Assassination | No replicated effect actors, no multicast | ADR-0003 | 🟡 Proposed |
| TR-asn-010 | assassination-defence-reversal.md | Assassination | Clients present contact-timed outcomes from one RTT-compensated estimate of server time with a stated accuracy budget (ASN-R21/R23, HUD-R6; added after TD-ARCHITECTURE concern 3) | ADR-0002 | 🟡 Proposed |
| TR-pen-001 | npc-kill-penalty-informants.md | Penalty | Server strike count and owner tally keyed by `ParticipantId`, round-scoped | ADR-0019 | 🟡 Proposed |
| TR-pen-002 | npc-kill-penalty-informants.md | Penalty | Exile on the Nth NPC kill through the Roster; body released to the crowd | ADR-0019 | 🟡 Proposed |
| TR-pen-003 | npc-kill-penalty-informants.md | Penalty | Respawn as a due-time-heap item at `T_due`; Absent path for disconnected victims | ADR-0002, ADR-0019 | 🟡 Proposed |
| TR-pen-004 | npc-kill-penalty-informants.md | Penalty | Takeover of an unseen crowd pawn in one call stack; retries and widening | ADR-0019 | 🟡 Proposed |
| TR-pen-005 | npc-kill-penalty-informants.md | Penalty | Informant-kill invariant: perception identical to an NPC kill | ADR-0001, ADR-0019 | 🟡 Proposed |
| TR-pen-006 | npc-kill-penalty-informants.md | Penalty | Owner-only data (tally, `bExiled`, `T_due` on the C-TeamView owner part, user decision 2026-10-25) | ADR-0001, ADR-0019 | 🟡 Covered — carrier decided 2026-10-25 (ADR-0001 §6, ADR-0019 §6) |
| TR-pen-007 | npc-kill-penalty-informants.md | Penalty | Informant restrictions enforced by their owners; Tell row T7 | ADR-0019 | 🟡 Proposed |
| TR-pen-008 | npc-kill-penalty-informants.md | Penalty | Event Log rows | ADR-0019 | 🟡 Proposed |
| TR-oc-001 | objectives-chain.md | Objectives | Chain data asset of `FObjectiveDef` rows | ADR-0020 | 🟡 Proposed |
| TR-oc-002 | objectives-chain.md | Objectives | Court binding from `FSiteAssignment` at `OnLayoutReady`, server-only | ADR-0020 | 🟡 Proposed |
| TR-oc-003 | objectives-chain.md | Objectives | `FObjectiveTeamView` rides C-TeamView; pushed at Active entry | ADR-0020 | 🟡 Proposed |
| TR-oc-004 | objectives-chain.md | Objectives | Tend predicate reads only site, public occupancy and legality class | ADR-0020 | 🟡 Proposed |
| TR-oc-005 | objectives-chain.md | Objectives | Completion gate inside the hold-timer slot; `DeclareObjectiveWin` | ADR-0002, ADR-0020 | 🟡 Proposed |
| TR-oc-006 | objectives-chain.md | Objectives | `RivalPrelimCount` with fresh per-publication delay and one global release grid | ADR-0006, ADR-0020 | 🟡 Proposed |
| TR-oc-007 | objectives-chain.md | Objectives | Stall-pressure rules (no timer, no reveal) | ADR-0020 | 🟡 Proposed |
| TR-citygen-001 | procedural-city-generator.md | Generator | Pure function of attempt seed, kit version and params; asserts on server sub-seeds | ADR-0014 | 🟡 Proposed |
| TR-citygen-002 | procedural-city-generator.md | Generator | Integer-only RNG and canonical iteration; golden seeds stable across CPUs/compilers | ADR-0006, ADR-0008 | 🟡 Proposed |
| TR-citygen-003 | procedural-city-generator.md | Generator | Twelve-stage pipeline with integer pre-validator; time-sliced | ADR-0014 | 🟡 Proposed |
| TR-citygen-004 | procedural-city-generator.md | Generator | ISM per module (Nanite), batch add, static mobility; ≤ 5,000 instances, ≤ 20,000 shapes | ADR-0014, ADR-0022 | 🟡 Proposed |
| TR-citygen-005 | procedural-city-generator.md | Generator | Deterministic client decoration without collision | ADR-0014 | 🟡 Proposed |
| TR-citygen-006 | procedural-city-generator.md | Generator | Server spawn + nav ≤ 3 s; client build ≤ 3 s p95 | ADR-0014 | 🟡 Proposed |
| TR-citygen-007 | procedural-city-generator.md | Generator | Manifest ≤ 3,500 entries | ADR-0014 | 🟡 Proposed |
| TR-citygen-008 | procedural-city-generator.md | Generator | Failure modes → reseed or graybox fallback | ADR-0014 | 🟡 Proposed |
| TR-tmk-001 | teammate-markers.md | Markers | Marker set only from the local C-TeamView (+ feed); no world iteration | ADR-0001 | 🟡 Proposed |
| TR-tmk-002 | teammate-markers.md | Markers | Anchor on the resolved `PawnRef`'s visual root; null semantics | ADR-0021 | 🟡 Proposed |
| TR-tmk-003 | teammate-markers.md | Markers | Fill states from C-TeamView state and phase (`RoundEnded` before `Ended` = no change) | ADR-0021 | 🟡 Proposed |
| TR-tmk-004 | teammate-markers.md | Markers | Screen-space overlay after post-process, no depth test, constant size, edge clamp | ADR-0021 | 🟡 Proposed |
| TR-tmk-005 | teammate-markers.md | Markers | `IsMarkedAlly` and `OnAllyMarkerRetired` client-local API | ADR-0021 | 🟡 Proposed |
| TR-tmk-006 | teammate-markers.md | Markers | View gate through Camera `GetViewState` | ADR-0021 | 🟡 Proposed |
| TR-hud-001 | hud.md | HUD | Reads only the three carriers | ADR-0001 | 🟡 Proposed |
| TR-hud-002 | hud.md | HUD | Subject-kind parity rule and automated leak suite | ADR-0008 | 🟡 Proposed |
| TR-hud-003 | hud.md | HUD | Layer/ZOrder stack after post-process; safe-margin anchors | ADR-0021 | 🟡 Proposed |
| TR-hud-004 | hud.md | HUD | Server-clock timing: plates at local contact frame, tally with plate, `PresentedOwnState` | ADR-0018, ADR-0021 | 🟡 Proposed |
| TR-hud-005 | hud.md | HUD | Round surfaces from `FRoundPublicState`; rebuild on `RoundEpoch` change | ADR-0021 | 🟡 Proposed |
| TR-hud-006 | hud.md | HUD | Client-local settings hooks (scale, reduce motion, high contrast) | ADR-0021 | 🟡 Proposed |
| TR-hud-007 | hud.md | HUD | Latency warning threshold (`HudLatencyWarnRttSec`; duplicate retired 2026-10-25) | ADR-0021 | 🟡 Proposed |
| TR-roster-012 | participant-state-roster.md | Roster | Server-only `OnViewRebound` after `RebindView`; feature team views rebuild from it | ADR-0005, ADR-0020 | 🟡 Proposed |
| TR-iif-013 | input-interaction-framework.md | IIF | `PossessionEpoch` owner-only via `ClientSetPossessionEpoch`; stale epochs rejected at ingress | ADR-0011 | 🟡 Proposed |
| TR-round-013 | round-match-framework.md | Round | Event Log ring bound; kill timeline protected from eviction; `TruncatedCount`; telemetry section | ADR-0023, ADR-0007 | 🟡 Proposed (re-check 2026-10-25: ADR-0023 §1, §5, R-4 and its GDD row now follow Round R19) |
| TR-ivp-012 | information-visibility-policy.md | IVP | Owner-only clock-exchange RPC pair, timestamps only | ADR-0002 | 🟡 Proposed |
| TR-scl-014 | shared-character-locomotion.md | SCL | Engine CMC net settings pinned and boot-tested against SCL keys (I-SCL-15) | ADR-0010, ADR-0004 | 🟡 Proposed |
| TR-con-011 | concealment-bushes.md | Concealment | Unbiased secret-stream spot pick (`BushSpotPick` domain) | ADR-0017, ADR-0006 | 🟡 Proposed |
| TR-asn-011 | assassination-defence-reversal.md | Assassination | Owner results as entries of the batched `ClientCommandReplies` RPC | ADR-0011, ADR-0018 | 🟡 Proposed |

## Known Gaps
No requirement lacks an ADR. Partial rows and what closes them:

| TR-ID | What is missing | Closes when |
|---|---|---|
| ~~TR-con-005~~ | Closed 2026-10-25: `FActionState.StartServerTime` adopted (user decision); ADR-0003/0017/0018 amended | — |
| ~~TR-pen-006~~ | Closed 2026-10-25: `T_due` carried on the C-TeamView owner part (user decision); ADR-0001 §6 and ADR-0019 §6 amended; HUD F7 live countdown | — |

Closed at the re-check (2026-10-25): TR-round-013 (ADR-0023 amended by the architecture-review fix pass to match Round R19).

Not traced as TRs (non-GDD sources): art bible 8.x budgets (ADR-0022), game-concept VCS concern and coding standards (ADR-0009).

## Superseded Requirements
None. Three baseline texts were registered in their synced wording (no ID change): TR-camera-006 (Camera-published fade drawn by the HUD fade layer, ADR-0021 sync), TR-camera-008 (rate bands at every range, never a cutoff, ADR-0015), TR-iif-002 (Crowd follower issues `Move` by input emulation, ADR-0011/0016 sync).
