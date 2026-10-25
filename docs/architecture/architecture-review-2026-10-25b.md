# Architecture Review Report

Date: 2026-10-25
Mode: full
Workflow: full
Engine: Unreal Engine 5.8 (installed 5.8.4, CL 58902052)
GDDs Reviewed: 19 system GDDs (design/gdd/*.md: MVP systems 1-18 and 20 Informant Pings); game-concept.md, systems-index.md and the three gdd-cross-review reports read for context
ADRs Reviewed: 23 (ADR-0001..ADR-0023, all `Accepted` 2026-10-25, several conditional on spikes)
Reviewer: independent (/architecture-review full, autonomous mode); technical-director (Phase 4) and unreal-specialist (Phase 5) consulted as sub-agents; four sonnet sub-agents ran the Phase 2/3 sweeps and one ran Phase 6; findings spot-checked in the source text by the reviewer
Supersedes: docs/architecture/architecture-review-2026-10-25.md (its updated verdict CONCERNS and its 201/201 coverage count)

Reviewed-Content-Hash: docs/architecture/adr-0001-crowd-identity-rules.md df8a978ebf0ee685461a0c79b7c4d0c5b88d0456
Reviewed-Content-Hash: docs/architecture/adr-0002-resolution-queue-round-end-staging.md fd3bb5267f1010f3cf998c7762ea568db0124688
Reviewed-Content-Hash: docs/architecture/adr-0003-replication-relevancy-concealment-cull.md 105df85bb4856db39bc56eda6dee7b0692df90b6
Reviewed-Content-Hash: docs/architecture/adr-0004-tuning-data-pipeline-validator.md dcb34f89dcefefbf6e856be624cddf07f9426f6f
Reviewed-Content-Hash: docs/architecture/adr-0005-session-seamless-travel-state-stores.md 0281017a54996be5bf4b58ed1c96369c7484453a
Reviewed-Content-Hash: docs/architecture/adr-0006-seeds-random-streams.md 90d4d47e642ef67d11832da6b3db5937b4267dc5
Reviewed-Content-Hash: docs/architecture/adr-0007-code-modules-include-boundaries.md 3ca1ccd8f384eb019ea1cab37dd58db9d63e6a7e
Reviewed-Content-Hash: docs/architecture/adr-0008-test-automation-leak-parity.md 920a079096ebfab2c70f414071785bf39405d9bd
Reviewed-Content-Hash: docs/architecture/adr-0009-version-control-binary-assets.md 7764c5c6ab73606c1d5b355a86bf31c8927211e7
Reviewed-Content-Hash: docs/architecture/adr-0010-shared-pawn-cmc-possession.md ea4b765675ae446ad60e4c4c00addfd58cc90bd8
Reviewed-Content-Hash: docs/architecture/adr-0011-pawn-command-interface-input.md 4d7260ef60c10ba9eaae91512b036bc7877142c4
Reviewed-Content-Hash: docs/architecture/adr-0012-character-pool-appearance-allocation.md 348a5e57a1071e6c4017dc0d246179a9325fb540
Reviewed-Content-Hash: docs/architecture/adr-0013-animation-motion-matching-significance.md 4bcf2bb562b75fddae589c47492abbf88d2d4a62
Reviewed-Content-Hash: docs/architecture/adr-0014-layout-service-generation-navmesh.md 7c75a2cccb992f48355626acb85092071876a7a6
Reviewed-Content-Hash: docs/architecture/adr-0015-camera-los-envelope.md 7189421edcc454321292c7e8ff4c130f2c69966f
Reviewed-Content-Hash: docs/architecture/adr-0016-crowd-ai-architecture.md 25c5ddb6660f00a9beec1ea4fee17ec3553ef17f
Reviewed-Content-Hash: docs/architecture/adr-0017-concealment-per-viewer-presentation.md 863f740db23c6c43de4aa8bcac606267c71a7acf
Reviewed-Content-Hash: docs/architecture/adr-0018-encounter-outcome-timing.md 07d913065c482812c1b3f743378bccaad4266ee4
Reviewed-Content-Hash: docs/architecture/adr-0019-informant-lifecycle-takeover.md 24ff71144d1971809e79c051a367b36364b08e03
Reviewed-Content-Hash: docs/architecture/adr-0020-objectives-data-team-view.md 13f788a2bd97a2f8f46904286c78be6713f95e15
Reviewed-Content-Hash: docs/architecture/adr-0021-ui-hud-layering-commonui.md da8c1c56fe575cb554016af747dc1e49375afb10
Reviewed-Content-Hash: docs/architecture/adr-0022-rendering-performance-budgets.md c9110274c3bfb290478a6e125ec1825fc2db3ec8
Reviewed-Content-Hash: docs/architecture/adr-0023-playtest-telemetry-hooks.md 5fad7aaf50be469f7b56413a870add3837f81d59
Reviewed-Content-Hash: design/gdd/assassination-defence-reversal.md 075b4c30d688ef9900e123585c7b8a975cb84ce8
Reviewed-Content-Hash: design/gdd/camera-system.md e690307bb7761bab18d0a0dcb1e465f61900fa21
Reviewed-Content-Hash: design/gdd/character-pool-appearance.md 24a7b8b3ce91621b2afd3ef3575231d1dc5bdf59
Reviewed-Content-Hash: design/gdd/concealment-bushes.md 401ab2291fb02beb16dca680666d5aa633c37739
Reviewed-Content-Hash: design/gdd/crowd-npc-behaviour.md 592bb1e815816ab552d6398dfe142aff3fdf0116
Reviewed-Content-Hash: design/gdd/game-concept.md c5529bcb565f78fee2390da7440a9b3c8b9c3785
Reviewed-Content-Hash: design/gdd/gdd-cross-review-2026-10-24.md 883fe8df5607c356f672029991ef4ac2214858dc
Reviewed-Content-Hash: design/gdd/gdd-cross-review-2026-10-25.md 378bde3b9adcfbc4229ad75b4442b53eaca67daa
Reviewed-Content-Hash: design/gdd/gdd-cross-review-2026-10-25b.md 820e1ce6e9a6697e22e078ba85df781ee2352254
Reviewed-Content-Hash: design/gdd/hud.md 532cceeb5a446f7d573086591e7812ed3286bfc8
Reviewed-Content-Hash: design/gdd/informant-pings-team-comms.md 762fc93baa952e9269eca84c6bdb0a93ff4b886d
Reviewed-Content-Hash: design/gdd/information-visibility-policy.md 6edb8863eeb5f93976bd079c12c9d4b3a97f7e81
Reviewed-Content-Hash: design/gdd/input-interaction-framework.md 7bdbbffd67711ec1d2df4d54103ab5c855d4f511
Reviewed-Content-Hash: design/gdd/level-socket-contract.md 8bcf928e40f7aa02cb2c9a3e7e2f998a3a8b2f51
Reviewed-Content-Hash: design/gdd/npc-kill-penalty-informants.md cf65bb4b56b41a4d9e6414c2add85029ee7c800c
Reviewed-Content-Hash: design/gdd/objectives-chain.md 9482b2986b72a9e9d74353855037707d9de0eb43
Reviewed-Content-Hash: design/gdd/participant-state-roster.md 202222e9b8b63f9bd9916096a35be38b7b64a343
Reviewed-Content-Hash: design/gdd/procedural-city-generator.md b6a48d2c98001c66bb09280c152285808cdf762e
Reviewed-Content-Hash: design/gdd/round-match-framework.md 090072912ac9711dd7ab5b046073dad37e19345b
Reviewed-Content-Hash: design/gdd/session-travel.md 46240ec3ad2335a02d50beed22dcbbb47f687cc3
Reviewed-Content-Hash: design/gdd/shared-character-locomotion.md c2a168bc695745dd7910a6e74b78b2f5c32ee874
Reviewed-Content-Hash: design/gdd/systems-index.md df9c94f8248a40d3a6910eb3263c6d4b49da045e
Reviewed-Content-Hash: design/gdd/teammate-markers.md 692341e03b4af26f2ff8ccb9dea3601bcea1edaf
Reviewed-Content-Hash: design/gdd/tuning-data-config.md 96554152523d76c5c726258f1db23721fa30242a
Reviewed-Content-Hash: docs/architecture/tr-registry.yaml 6ba61f8683f25d25e518ca46b3738a86707472c2
Reviewed-Content-Hash: docs/architecture/architecture.md 36b0ed294f12c53b565ab060c61b702ac582ab73
Reviewed-Content-Hash: docs/engine-reference/unreal/PLUGINS.md a678cbc1f8fa7e7c4bed4bf6431d4062364f1489
Reviewed-Content-Hash: docs/engine-reference/unreal/VERSION.md 1b754bfb7ed000b476d808a38e7ecaedc0b3463e
Reviewed-Content-Hash: docs/engine-reference/unreal/breaking-changes.md a53507be06befaca08240bb578bf2e9e492138fb
Reviewed-Content-Hash: docs/engine-reference/unreal/current-best-practices.md 84434704bf5129944731298240b0854706b3ccba
Reviewed-Content-Hash: docs/engine-reference/unreal/deprecated-apis.md 74ecb699d5eeda5157750f5af1a8c09825a75408
Reviewed-Content-Hash: docs/engine-reference/unreal/modules/animation.md 37ed610acf939e369aac723bb8b492298bbbfb0e
Reviewed-Content-Hash: docs/engine-reference/unreal/modules/audio.md 26d7beb487b226cf9ba970ada91a566b6a33b17d
Reviewed-Content-Hash: docs/engine-reference/unreal/modules/input.md 0f0e8b29bda35f2922894eaa1fe2350dba1b6c28
Reviewed-Content-Hash: docs/engine-reference/unreal/modules/navigation.md 82fdfed513824a4832f3d0562fa7eabef25de921
Reviewed-Content-Hash: docs/engine-reference/unreal/modules/networking.md 5041641b231021e8937308bc9b305d1faf335943
Reviewed-Content-Hash: docs/engine-reference/unreal/modules/physics.md 0c80abde74bd5c12e78d0aa1f00ce378e5bb6625
Reviewed-Content-Hash: docs/engine-reference/unreal/modules/rendering.md f4c0a9bfc1f4fb4d3244c596e3d61c3d6f7b0b72
Reviewed-Content-Hash: docs/engine-reference/unreal/modules/ui.md 5dc9788a910030b8754bdc987716d5e94e9173da
Reviewed-Content-Hash: project.yaml 29f0088dc379712d6e962f333fd9b23c600c4f13
Reviewed-Content-Hash: .claude/docs/technical-preferences.md 5c2f06283325bac83d7b4206d498bd85ae2c2725
Reviewed-Content-Hash: .claude/skills/architecture-review/SKILL.md 1a9d28aadbcbd26b5fc6b8ed1c64d73ae82b7b73
Reviewed-Content-Hash: .claude/skills/architecture-review/references/1-load.md 9a65759ebdbb2cb1f9744775bfed18cb25e76347
Reviewed-Content-Hash: .claude/skills/architecture-review/references/2-extract-requirements.md d08ea0b0e9ab9760b0d8bfe0cd9115f0bc40f8e2
Reviewed-Content-Hash: .claude/skills/architecture-review/references/3-traceability.md bb7b6b65c5e5e5b8869fa692413aa831615dede7
Reviewed-Content-Hash: .claude/skills/architecture-review/references/4-conflicts.md 0d30f8173b23248a32523a365b606dd1ce9642ab
Reviewed-Content-Hash: .claude/skills/architecture-review/references/5-engine.md 318ecbc1361d430badaf5dd8ce4cfc6ab0dff0ca
Reviewed-Content-Hash: .claude/skills/architecture-review/references/6-document-coverage.md dcce78482ec2ce60048989b6f9d1f797dd0a9a0e
Reviewed-Content-Hash: .claude/skills/architecture-review/references/7-report.md a007fd66bdac87812dc272e4b14cd231174e979d
Reviewed-Content-Hash: .claude/skills/architecture-review/references/8-write.md bf6d8946fc7b9e19eda7f65503a5c8f593a0d2b1
Reviewed-Content-Hash: .claude/skills/architecture-review/references/9-handoff.md fd5f681f4c3517c6a2e388c6d78075d82dded22e

Freshness check: prior report `architecture-review-2026-10-25.md` (Mode full, Workflow full, so it covers this run) stamped every ADR, every GDD, the TR registry, `architecture.md`, the engine reference and `project.yaml`, and all of them are now CHANGED or NEW (new GDDs: `informant-pings-team-comms.md`, `gdd-cross-review-2026-10-25.md`, `-25b.md`). No UNRESOLVED line. Per the skill, a non-GDD/ADR input change means a full re-run, not a single-gdd scope. `docs/consistency-failures.md` does not exist (not stamped, not created).

Denominators: 25 files in `design/gdd/` = 19 system GDDs (MVP systems 1-18 and 20) + `game-concept.md`, `systems-index.md` and 3 cross-review reports (context only). 23 ADRs, all with `## Status`, `## Decision`, `## GDD Requirements Addressed`, `## Engine Compatibility`, `## ADR Dependencies` and `## Performance Implications` (no structural gap). Engine: Unreal 5.8 (installed 5.8.4).

Method: the ADRs and GDDs are 45-225 KB each, so every read was by section. Four sonnet sub-agents re-verified all 201 registry rows against the ADR text (`## GDD Requirements Addressed`, `## Decision`, Amendments) and swept each GDD's Detailed Rules, Formulas, Dependencies, Tuning Knobs and Acceptance Criteria for unregistered requirements. A technical-director sub-agent (Phase 4) and an unreal-specialist sub-agent (Phase 5, checking against the 5.8.4 engine source) worked independently. A sonnet sub-agent ran Phase 6. Every finding used below was spot-checked in the source text by this reviewer before it was acted on.

---

## Traceability Summary

Total requirements: **214** (201 from registry v3 + 13 registered by this review; registry now v4)

| Status | Count |
|---|---|
| ✅ Covered (Accepted ADR) | **193** |
| 🟡 Covered (Proposed ADR) | 0 |
| ⚠️ Partial | **21** |
| ❌ Gap | **0** (2 found, both closed in-review, see below) |
| ❓ Not assessed | 0 |

**The previous index overstated coverage.** It recorded "201/201 ✅" after the Technical Setup amendments. The row-by-row check found ADR text covering only part of 18 of those rows. It also found 13 architectural requirements missing from the registry (3 of the new rows are partial). The full matrix is in `docs/architecture/requirements-traceability.md`.

### Gaps found and closed in-review (no ADR existed)

| TR-ID | Layer | Requirement | Closed by |
|---|---|---|---|
| TR-roster-013 (new) | **Foundation** (Roster) | `PublicHandle` is random 32-bit from a server-only source, unique in the match, never reused, with no order (Roster R3). It is the only participant reference clients receive. | ADR-0006 append-only amendment: domain **15 `PublicHandle`** (counter-as-Attempt; collisions discarded). |
| TR-scl-015 (new) | Core (SCL) | Server idle-gaze layer for every pawn kind, from a server-only counter stream keyed by stable pawn ordinal (SCL R8.9, D14). | ADR-0006 domain **16 `IdleGaze`**. The layer rules remain partial; see TR-scl-005. |

Without these two amendments the verdict would be **FAIL** (a Foundation-layer requirement uncovered). Both follow ADR-0006's existing consumption shapes, and both GDDs already named ADR-0006 as the source. Logged as a decision; the user should glance at ADR-0006's new rows.

### Partial coverage (21)

| TR-ID | Layer | Missing ADR text |
|---|---|---|
| TR-tdc-012 (new) | Foundation | ADR-0004's schema types (`bool/int32/float/enum`), `typeTag` and canonical `TuningHash` form have no soft-reference type. The Crowd `*Asset` keys (StateTree, motion profile) need one, plus the GDD's validator rule. |
| TR-roster-002 | Foundation | The legal-transition table, R8 result precedence and batch-only reset are GDD content housed in ADR-0005's `FRosterCore`; no ADR names the GDD table as normative. |
| TR-roster-003 | Foundation | "`EventSeq` +1 per event of any type" is unstated (ADR-0005 owns `EventSeq`). |
| TR-roster-005 | Foundation | The O(1) pawn→participant lookup structure (AC-PSR-28) and "on clients return 0/false and report to the fault sink" are unstated. |
| TR-roster-008 | Foundation | "Opponent counts never leave the server before `Ended`" is only implied (ADR-0002 §8 `TeamSummary`, ADR-0001 §5). |
| TR-ivp-001 | Core | `CanViewerKnow` signature, `GetViewerClass` and the fact matrix are defined only in architecture.md API Boundaries; default-deny is stated for properties only. |
| TR-round-006 | Core | Round P1-P7 sequencer, `PreparingWatchdogSec`, abort codes and the P5b gate are in no ADR. |
| TR-round-007 | Core | Grace/forfeit/void logic is deferred to the GDD by ADR-0002 §8.1 (minor). |
| TR-round-008 | Core | The Event Log ring (entry types, release classes, writer interface) has no owning ADR; it is split across ADR-0002, ADR-0007 and ADR-0023. |
| TR-round-009 | Core | Public round struct details (`Phase` 0 = `Unknown`, `PhaseChangeCount`, zeroed times, re-seeding the recreated GameState) go beyond ADR-0003 §6. |
| TR-lsc-007 | Core | The "no pawn spawned for, possessed by or relevant to a client before `Connected`" gate: only the possession half is decided (ADR-0010 §3.2). It must be reconciled with ADR-0003 §5.4 relevancy. |
| TR-lsc-010 (new) | Core | `ClueTag` exposure is now noted in ADR-0014, but no V-CLUE Reject predicate is listed. |
| TR-scl-005 | Core | The SCL R8 `HeadLook` filter is placed in the pawn tick (ADR-0001 diagram, ADR-0016) but its rules have no owner: rate limit, yaw byte, observer filter, head hold. |
| TR-scl-015 (new) | Core | Stream now in ADR-0006; the layer itself lives in the unowned `HeadLook` filter. |
| TR-png-014, -016, -017, -019 | Feature | Ring dedupe and stale-arrival rules, label lookup and fallback, "no feed line", rebuild-without-replay and hide on `Ended` are not in ADR-0021. |
| TR-png-018, -024 | Feature | ADR-0008 has no parity-test rows for AC-PNG-01/-02/-14. The own-ring latency target is in no ADR. |
| TR-png-020 | Feature | "In-engine voice/chat disabled" is stated in no ADR (negative requirement, low risk). |

**Foundation layer (systems-index: Tuning Data & Config, Participant State & Roster): 1 gap found (TR-roster-013), closed in-review; 0 open; 5 partials.**

### Coverage corrections (claimed ADR was wrong or incomplete; now ✅)

- TR-iif-005 and TR-iif-006 → ADR-0011 (previously attributed to ADR-0002).
- TR-scl-008 bandwidth line → ADR-0003 (not ADR-0022).
- TR-session-010 + ADR-0005 and ADR-0006.
- TR-asn-001 and TR-asn-002 + ADR-0018.
- TR-png-003, -007 and -012 had over-claimed ADRs removed.
- TR-png-005: ADR-0015's CS-R8 line said Pings send "the resulting ground point" of `GetAimRay()`, which contradicts PNG-R3. It is corrected (own world-only rule, never `WorldPoint`).
- Verified closed: TR-con-005 (`StartServerTime` in ADR-0003/0017/0018) and TR-pen-006 (`T_due` on the C-TeamView owner part).

### TR registry changes (v3 → v4)

- **New (13):**
  - TR-roster-013 `PublicHandle` source
  - TR-roster-014 `bReconnecting` + `ReconnectEndCause`
  - TR-tdc-012 soft-reference keys
  - TR-session-011 `RejoinToken`
  - TR-session-012 engine-timeout backstop
  - TR-scl-015 idle gaze
  - TR-lsc-010 `ClueTag`/V-CLUE
  - TR-crowd-011 virtual input layer + replay parity
  - TR-con-012 `bDiedConcealed`
  - TR-asn-012 warp handling
  - TR-asn-013 owner cooldown carrier
  - TR-tmk-007 `RetireAtServerTime`
  - TR-hud-008 HUD cost and construct-once
- **Reworded (6, `revised: 2026-10-25`):**
  - TR-tdc-008: four latch classes, including `Session`.
  - TR-session-005: `ClientHash`, `LayoutAttempt`, `ServerWorldReady`.
  - TR-cpa-003: `Take` reserved, no draw.
  - TR-citygen-003: one worker task, not game-thread time-slicing.
  - TR-camera-008: three rate bands.
  - TR-scl-003: `ForceNetUpdate` suppressed, not deferred.
- **Deprecated:** none.
- **No renumbering.**
- `registry.sh validate`: duplicate 0, tr_unknown 0, path_missing 0, status 0, layout 0.
- One pre-existing STATUS row in `design/registry/entities.yaml` (`ReachDetourMult: derived`) was set to `active`. Its note already records it as a derived alias.

Not traced as TRs: art bible 8.x budgets (ADR-0022), game-concept VCS concern (ADR-0009).

---

## Cross-ADR Conflicts

Phase 4 was run by the technical-director (sub-agent). **No 🔴 blocking conflict.** Eight 🟡 non-blocking conflicts were found; six were fixed in-review as text corrections, and two remain open.

Known conflict-prone areas: `docs/consistency-failures.md` does not exist. From the prior report, queue payload ownership and Technical Setup/Closeout amendments not propagated into ADR bodies recur. Most of this run's findings are the second kind.

| # | Conflict | Type | Status |
|---|---|---|---|
| 1 | **ADR-0013 vs ADR-0022/0001.** ADR-0013's diagram, Guidelines 2 and 5, Consequences, R9 and Performance still described a rate-1 listen host. Closeout D3 (same ADR §4, ADR-0022 §4.2, ADR-0001 §3) makes host tiering the default. Guideline 2 contradicted the §4 mechanism. | Pattern / performance | **Fixed** (option 1, harmonise). It also carries the engine finding E-1 below. |
| 2 | **ADR-0011 vs ADR-0021/0007.** The concealed-ping refusal must produce the HUD "Not now" plate and tone (HUD-R13, AC-PNG-17). ADR-0021's presenter samples only `FStealthClientViewHandles`, and no handle carries a ping refusal. The gate's module and its `PresentedRole` source are also unstated. | Integration gap | **Open.** Recommended: a Pings `Public/View` `IPingClientView` (monotonic `ConcealedRefusalSeq` + bucket mirror) added to the handles, with the gate in the Pings View helper. One amendment line each in ADR-0007, ADR-0011 and ADR-0021. |
| 3 | **ADR-0007 vs ADR-0005/0010/0011.** Pings "subscribes" to `InvalidateAllBindings`, which is a travel step, not an event. "Clears itself" could also be read as resetting the bucket at travel, against ADR-0011. | Integration | **Fixed** (option 1). The view-part clear binds to `OnRoundTravelStarted`; the bucket and parts reset at `OnRosterReset`. |
| 4 | **ADR-0023 amendment vs ADR-0023 §1/§2.3.** Ping rows passed `Team`/`RoleClass` from the producer, and `Phase` drop counters would be ignored outside `Active`. | Data contract | **Fixed** (option 1). The sink resolves them from `FSubjectRef`; counters are aggregated in the service and copied at export. |
| 5 | **ADR-0017/0018/0003 vs ADR-0003 §5.7.** Several places still named the retired `MarkKilledConcealed` / `KilledConcealed` interim: ADR-0018 §3.2 said "a killed-concealed pawn stays closed", and ADR-0003's diagram said "KilledConcealed stays". Implemented literally, these call a removed method. | Integration (stale API) | **Fixed** (all references now the `bDiedConcealed` death-write re-open) |
| 6 | **ADR-0019 vs ADR-0001/0002/0007/0011.** ADR-0019 placed Pings in the Vertical Slice; it has been MVP since 2026-10-25. | Scope | **Fixed** |
| 7 | **ADR-0017 amendment.** It cited "§4 server queries"; the server queries are §7. | Reference | **Fixed** |
| 8 | **ADR-0007 body vs its Technical Setup amendment.** The module table and both interface lists lacked Pings and `ITeamPingReset`. | Registry | **Fixed** |

Prior LOW items:
- **N1 resolved.** ADR-0002 §2a declares `FReplySinkId` and `FDebugPayload` in `StealthQueuePayloads.h`. Leftover: ADR-0011 has no back-reference to the reply-sink table ADR-0002 says it owns.
- **N3 resolved.** The `StageSignal` mapping is Subject none, Phase = viewing team, in both ADR-0002 and ADR-0020. The ADR-0020 amendment's section reference was corrected (§7).

**Performance budget sum.** The ADR-0022 listen-host game-thread allocation sums to exactly 12.0 ms p95:

| Line | ms |
|---|---|
| Animation | 2.0 |
| CMC | 3.0 |
| Crowd + nav | 1.5 |
| Queue/Roster | 0.5 |
| Replication | 1.5 |
| Eye mask + misc | 1.0 |
| HUD | 0.5 |
| Residual | 2.0 |

Every owner claim fits its line. These are known working values to re-verify at 60 characters in R3/RC-1:
- CMC projects about 3.75 ms at 50 NPCs.
- ADR-0003's replication call rate was sized for 50 pawns.
- ADR-0022's own amendment projects 13.1 ms before tiering.

GPU: 14.4 + 2.2 ms headroom. Network: about 31.5-36.3 KB/s steady of 50 KB/s. Round-end spike: 14.2 ms worst. No budget conflict.

Minor internal ADR inconsistencies, not fixed and LOW:
- ADR-0023 §4 memory (1.5 MB) vs its Performance line (1.1 MB); trace and distance counts still sized for 50 pawns.
- ADR-0015 Performance says ≤ 1.2 ms of task-thread physics, while its closeout says about 1.5 ms.
- The ping handler has no named budget line (negligible).
- ADR-0011 counts IIF in the queue line, and ADR-0022 also lists it under misc (harmless double count).

---

## ADR Dependency Order

`adr-dep-graph.sh`: 23 ADRs, 134 edges, **no CYCLE**, no `NO_DEPS_SECTION`, no `UNFILLED_DEPS`. All depended-on ADRs are Accepted, so there are no unresolved dependencies. Four referenced placeholders do not exist: ADR-0024 (Iris / dedicated server, Alpha) and ADR-0025 (server view check). ADR-0004 documents these as deferred; ADR-0015's reference to ADR-0025 states no caveat.

### Recommended ADR Implementation Order (topologically sorted; script ORDER)
Foundation (no dependencies):
1. ADR-0007 Code modules and include boundaries
2. ADR-0009 Version control and binary assets

Depends on Foundation:

3. ADR-0008 Test automation, leak and parity harness (0007, 0009)
4. ADR-0001 Crowd identity rules (0007, 0008, 0009)
5. ADR-0004 Tuning data pipeline and validator
6. ADR-0005 Session, seamless travel and state stores
7. ADR-0006 Seeds and random streams
8. ADR-0002 Resolution queue and round-end staging
9. ADR-0003 Replication, relevancy and concealment cull

Core:

10. ADR-0010 Shared pawn, CMC and possession
11. ADR-0014 Layout service, generation and navmesh
12. ADR-0011 Pawn command interface and input
13. ADR-0012 Character pool and appearance
14. ADR-0015 Camera, LOS and envelope

Feature:

15. ADR-0016 Crowd AI
16. ADR-0020 Objectives
17. ADR-0013 Animation, motion matching and significance
18. ADR-0017 Concealment presentation
19. ADR-0018 Encounter outcome timing
20. ADR-0022 Rendering and performance budgets
21. ADR-0019 Informant lifecycle and takeover

Presentation:

22. ADR-0021 UI/HUD layering (CommonUI)
23. ADR-0023 Playtest telemetry hooks

ADR-0017 and ADR-0018 form an acceptance pair (edge 0018 → 0017 only).

---

## GDD Revision Flags (Architecture → Design Feedback)

**No GDD revision flags; no systems-index status changed.**

- The one HIGH engine finding (E-1) is an ADR-side defect. SCL GDD R11.5 already states the behaviour that 5.8.4 requires: on the host the pose ticks every frame, montage time, root motion and notifies are never dropped, and only evaluation is rate-limited. ADR-0013 was aligned to it.
- One design-text defect was fixed directly: `assassination-defence-reversal.md` l.120 and the Interactions-table row l.157 still said "a concealed victim is never re-opened". That contradicts the same GDD's ASN-R10/R17/R21 and the approved `bDiedConcealed` rule.

Noted for the GDD owners, no action taken:
- Crowd CNB-R15 still names the "50-character spike", while F5 and ADR-0016 use 60 characters. It is the spike's name; the count is stale.
- Session F5's seed table should gain domains 15 and 16.

---

## Engine Compatibility Issues

### Engine Audit Results
Engine: Unreal Engine 5.8 (installed 5.8.4, CL 58902052). ADRs with an Engine Compatibility section: **23 / 23**. Every ADR states 5.8 (installed 5.8.4).

**Deprecated API references: none.** Every hit against `deprecated-apis.md` is a prohibition or a replacement:
- `UE_LOG` appears only as "never `UE_LOG`"; 11 ADRs prescribe `UE_LOGF`.
- `PhysicsMover` is only noted as removed (ADR-0010 chose CMC).
- `DOREPLIFETIME` is not deprecated.
- `TSharedPtr<const FValidatedTuning>` is a non-UObject struct.
- No legacy input bindings, Cascade, World Composition, RawInput, `FSmartName` or `Cpp17`.
- The specialist also confirmed ADR-0003 uses the 5.5+ net-config setters and ADR-0005 avoids `DispatchPostLogin` (deprecated 5.6).

**Stale version references:** none.

**Post-cutoff API conflicts between ADRs:** none. All ADRs assume the legacy NetDriver (`net.Iris.UseIrisReplication=0`, `DefaultEngine.ini:108`).

**Project facts confirmed:** StateTree, GameplayStateTree, CommonUI, MotionWarping (Beta), PoseSearch, Chooser and AnimationWarping are enabled. Mover and NetworkPrediction are **disabled** (`.uproject:83-89`, ADR-0010 amendment). Iris is off. The project is a git repo with LFS (`origin/main`).

The previously reported engine-reference errors are corrected in the docs: the Enhanced Input flush CVar name, `DOREPLIFETIME`, and the `COND_InitialOnly` comment.

### Engine Specialist Findings (unreal-specialist)

1. **HIGH, ADR-0013 §4 / Guidelines / R9: listen-host tiering vs authoritative root motion.**
   - `USkeletalMeshComponent::ShouldTickAnimation()` is non-virtual. Under external control it returns only `bExternalUpdate` (`SkeletalMeshComponent.cpp:1919-1923`), so an update-rate skip on authority also skips root-motion extraction for a server-simulated pawn in a montage (assassination, traversal). That changes authoritative movement.
   - The ADR's own text also contradicted itself (conflict 1).
   - **Fixed in text:** §4 now requires every host pose to update each frame, with only evaluation tiered, and the §3 rate-1 override applies on the host. R9 carries the engine note.
   - **Residual risk:** the `TickPose` override and the external-control toggling are fragile across engine upgrades. They stay gated by ANIM-V1/ANIM-V3, the traversal spike and R3 (already conditions of ADR-0013's acceptance), and go on the engine-upgrade checklist.
2. **MEDIUM, ADR-0010 E11 and architecture.md:45: the evidence said Mover and NetworkPrediction were enabled.** Both are disabled. Fixed.
3. **MEDIUM, ADR-0003: listen-host `ServerReplicateActors` cost.** With `NetClientTicksPerSecond=2400` and `bClampListenServerTickRate=false`, the host considers all 9 connections every rendered frame, up to `MaxFps` 240. `NetUpdateFrequency` bounds bandwidth, but not consider/relevancy/prioritisation cost (60 pawns × 9). **Action:** RC-1 captures the host uncapped and at its real frame cap. Whether `MaxFps` is enforced was not checked.
4. **MEDIUM, ADR-0005 §6: "120 s configured".** Nothing is configured yet; the engine base is 60 s (`BaseEngine.ini:1855-1856`), and the PIE multiplier differs from packaged builds. Fixed to "to be configured", naming the `IpNetDriver` section.
5. **MEDIUM, ADR-0016 E13: `AAIController::OnPossess` adds a replicated `UGameplayTasksComponent`, an identity tell.** Verified that ADR-0010 already has the countermeasure: a non-replicated default subobject on every pawn (§2 l.110, guideline l.320). Keep the leak-suite subobject inventory blocking.
6. **LOW:**
   - ADR-0011 E2's "docs use the wrong CVar name" note was fixed.
   - ADR-0012 CPA-V4 expects a `COND_InitialOnly` re-send after takeover; it should expect "retained, not re-sent". Left [U] for S10.
   - `p.EnableCharacterAccelerationReplication=1` (inherited) is in no ADR. List it in ADR-0003's config table and the leak inventory.
   - ADR-0022 E1/E3 were annotated for the defensive `r.Substrate`/`r.RayTracing` pins.
   - ADR-0008: single-process PIE runs hard travel unless `net.AllowPIESeamlessTravel=1`, so S3/S4 evidence must come from Gauntlet multi-process.
7. **Verified as written:** ADR-0010 GameNetworkManager pins and ADR-0021 Common UI items (`bEnableEnhancedInputSupport`, next-tick `FlushPressedKeys`, `OnActiveInputModeChanged`).

Specialist coverage: ADR-0002, 0004, 0006, 0007, 0009, 0015, 0017-0020 and 0023 were checked at heading level only. ADR-0014's runtime NavMeshBoundsVolume sizing and ADR-0012's PSO precache facts were not checked.

Engine-reference gaps (not errors), cited by the ADRs:
- No StateTree page.
- No seamless-travel/PlayerState material in `modules/networking.md`.
- `modules/rendering.md` is stale on Nanite skinned meshes and WPO.
- The 5.8.4 `r.MegaLights.EnableForProject` help text still says Experimental.

---

## Architecture Document Coverage

- **Systems:** every system in `systems-index.md` (MVP 1-18 and 20) appears in architecture.md's layer map and module tables. 19 and 21-30 are explicitly deferred.
- **Orphans:** none. Every module traces to a GDD or an ADR. Telemetry probes have ADR-0023 and no GDD by design (GDD 26 is Alpha).
- **Prior drift N2 resolved:** the API Boundaries queue types, `FSubjectRef` placement and `FTimerPayload` match ADR-0002 §2a. `ITeamPingReset`, `Features/Pings/`, `FNetTeamPing`, `UTeamPingService` and `UTeamPingViewComponent` are reflected.
- **Remaining thin Data Flow for Pings (LOW):**
  - Tuning → Pings keys.
  - Session reconnect → Pings bucket.
  - Pings → telemetry counters.
  - Penalty/Informants eligibility edge (PNG-R2).
  - HUD `Ended` ring hide.
  - The Data Flow timer list (l.293-295) omits `EndedMinDwell` and `EndedAutoReset`.
- **Stale status text fixed in architecture.md:**
  - l.45: Iris and Substrate pins; Mover and NetworkPrediction disabled.
  - l.700 and l.773: ADR-0009 Accepted, repo initialised.
  - l.757: "a Proposed ADR".
  - l.765: ADR-001 "Status stays Proposed".
  - l.777: ADR-013 "before Accepted".
  - l.850: AQ-03.
- **Stale text fixed in control-manifest.md l.788:** CommonUI is enabled.
- **Not fixed, user's process choice:**
  - control-manifest.md l.212 and ADR-0009 l.184 keep the `_snapshots/` rule "until VCS exists". The condition is now met.
  - ADR-0009's Status line still says repository init awaits an instruction. Status lines are not edited by this review.

---

## Verdict: CONCERNS

- All 214 requirements trace to an Accepted ADR. 193 are fully covered, 21 are partial, and none is an open gap.
- There is no blocking cross-ADR conflict, no dependency cycle, and no deprecated API.
- The engine HIGH finding is fixed in text; its residual risk is already a spike gate.
- Phase 6 ran.

CONCERNS rather than PASS because of the 21 partial rows (5 in the Foundation layer), 2 open non-blocking conflicts, and the engine MEDIUM items that are measurement work (RC-1 host replication cost).

FAIL was avoided only because the two uncovered requirements (TR-roster-013 Foundation, TR-scl-015 Core) were closed in-review by the ADR-0006 domain amendment. **If the user rejects that amendment, the verdict is FAIL.**

### Blocking Issues (must resolve before PASS)
Not a FAIL verdict, so nothing blocks. These must be resolved to reach PASS:
1. Close the 5 Foundation partials: ADR-0004 soft-reference type (TR-tdc-012); ADR-0005/0001 Roster statements (TR-roster-002/003/005/008).
2. Close the Core partials:
   - an owner for the SCL `HeadLook` filter, plus the idle-gaze layer (TR-scl-005/015);
   - the IVP query API (TR-ivp-001);
   - the Round sequencer, Event Log and public struct (TR-round-006/007/008/009);
   - the pre-`Connected` spawn/relevancy gate (TR-lsc-007);
   - V-CLUE (TR-lsc-010).
3. Resolve conflict 2: the ping-refusal route to the HUD (ADR-0007/0011/0021).
4. Pings presentation partials: ADR-0021 ring rules and ADR-0008 parity rows (TR-png-014/016/017/018/019/020/024).

### Required ADRs
No new ADR is strictly required; each partial closes with an amendment to an existing ADR. Two new ADRs would give clearer ownership, most foundational first:
1. **Round sequencer and Event Log.** Covers TR-round-006/008/009 and the P5b gate. Alternatively, amend ADR-0005 and ADR-0023.
2. **Shared Pawn head-look filter.** Covers SCL R8 + R8.9: the filter in the pawn tick, rate limit, yaw quantisation, observer filter and idle gaze. Alternatively, amend ADR-0010.

---

## Fixes applied by this review (2026-10-25b)

All are text corrections or the logged ADR-0006 append. No ADR Status line was changed (verified: 0 diff lines touch a Status block).

- **ADR-0001:** ADR-0009 and the Iris pin recorded as done. §5 teammate entry and `FTeamViewTeammateEntry` now carry `bReconnecting` + `ReconnectEndCause`, with the event-only clearing rule.
- **ADR-0002:** the Round R13 change request is marked applied.
- **ADR-0003:** the diagram's killed-concealed rule.
- **ADR-0004:** `StopMinSec` safe range (0.4-0.6).
- **ADR-0005:** the `ConnectionTimeout` value is "to be configured", with the engine base and section named.
- **ADR-0006:** domains 15 `PublicHandle` and 16 `IdleGaze`, in the table and the enum, plus an Amendments section.
- **ADR-0007:** Pings clear binding (conflict 3); Pings and `ITeamPingReset` in the module table and interface lists.
- **ADR-0010:** E11 Mover/NetworkPrediction disabled.
- **ADR-0011:** E2 note; `IMC_Common` list includes Walk slower and Ping.
- **ADR-0013:** listen-host harmonisation with D3 plus the SCL R11.5 constraint and the engine note (conflict 1, E-1).
- **ADR-0014:** `ClueTag` delivered.
- **ADR-0015:** CS-R8 ping-point line (TR-png-005).
- **ADR-0017:** retired `MarkKilledConcealed` references; answered requests marked applied; §7 reference; Pings in Enables.
- **ADR-0018:** killed-concealed re-open, Depends On, byte figures (208-bit `FActionState`, 65-bit `FDeathState`).
- **ADR-0019:** Pings MVP; `T_due` field name.
- **ADR-0020:** §7 reference; `ClueTag` delivered.
- **ADR-0021:** E9 CommonUI now enabled; amendment requests marked applied.
- **ADR-0022:** E1/E3 config pins.
- **ADR-0023:** ping telemetry rows and counters (conflict 4).
- **architecture.md:** l.45, 700, 757, 765, 773, 777, 850 (stale status text).
- **control-manifest.md:** l.788 (CommonUI enabled).
- **project.yaml:** comment l.62 ("ADR-0022 is still Proposed" → Accepted, conditional).
- **design/registry/entities.yaml:** `ReachDetourMult` status `derived` → `active`.
- **assassination-defence-reversal.md:** l.120 and l.157 (killed-concealed re-open).

Informational, not changed: dated amendment trailers reading "Status unchanged (Proposed)" are history (they were true when written). `control-manifest.md` (2026-10-25e) predates these amendments; regenerate it with `/create-control-manifest`.

## Story linkage (Phase 3b, informational)

A concurrent session is creating epics and stories (`production/epics/`, created about 05:06 today).
- At review close: 19 epic folders, 25 stories (all in `shared-character-locomotion`, still growing), and all 201 v3 TR-IDs cited across epics.
- Of the 13 new TR-IDs, only TR-scl-015 is already cited (the concurrent session read registry v4). The other 12 are not yet in any epic or story; `/create-epics` or `/create-stories` should pick them up.
- The stories' test paths are MISSING, as expected before implementation: `Source/StealthGameplay/...` does not exist yet.
- Two stories place tests under `Source/StealthTestHarness/Private/Locomotion/`, not the `Source/<Module>/Private/Tests/` root in `directory-structure.md`. Check this against ADR-0008 when the stories are reviewed.

## Handoff

1. **Immediate actions (top 3):**
   - amend ADR-0010 (or write a head-look ADR) for the `HeadLook` filter and idle gaze (TR-scl-005/015);
   - amend ADR-0004 for soft-reference tuning keys (TR-tdc-012);
   - amend ADR-0007/0011/0021 for the ping-refusal HUD route (conflict 2), then close the Round partials (TR-round-006/008/009).
2. **Pre-gate checklist:**
   - ✅ `Source/StealthGame/Private/Tests` and `Source/StealthGameEditor/Private/Tests`
   - ✅ `.github/workflows/tests.yml`
   - ✅ `design/accessibility-requirements.md`
   - ✅ `design/ux/interaction-patterns.md`
3. **Then:** regenerate the control manifest (`/create-control-manifest`), because ADR text changed after manifest 2026-10-25e.
4. **Rerun trigger:** re-run `/architecture-review` after each ADR amendment above to verify coverage improves.

Autonomous mode: the closing question was not asked. Recommended next step: write the missing amendments, then `/gate-check pre-production`. All pre-gate items are ✅ and no gap is open. CONCERNS is advisory.
