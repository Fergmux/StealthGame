# Architecture Review Report

Date: 2026-10-25
Mode: full
Workflow: full
Engine: Unreal Engine 5.8 (installed 5.8.4, CL 58902052)
GDDs Reviewed: 18 MVP system GDDs (design/gdd/*.md; game-concept.md, systems-index.md and gdd-cross-review-2026-10-24.md read for context) + design/registry/entities.yaml
ADRs Reviewed: 23 (ADR-0001..ADR-0023, all `Proposed`; ADR-0024/0025 deferred by design)
Reviewer: independent (/architecture-review, autonomous mode); technical-director (Phase 4) and unreal-specialist (Phase 5) consulted as sub-agents; the blocking finding was re-verified in the ADR text

Reviewed-Content-Hash: docs/architecture/adr-0001-crowd-identity-rules.md b861cd2dbc7b55a24d08a8f8b2cee28a25481872
Reviewed-Content-Hash: docs/architecture/adr-0002-resolution-queue-round-end-staging.md 2ee1dee68b21b52d1735ba2fe6ffbe86ab0510ae
Reviewed-Content-Hash: docs/architecture/adr-0003-replication-relevancy-concealment-cull.md fafcf90d28a0782883a01bb79ef9b01430373ce9
Reviewed-Content-Hash: docs/architecture/adr-0004-tuning-data-pipeline-validator.md e848f484524b60195c19ee51632194c72daeaada
Reviewed-Content-Hash: docs/architecture/adr-0005-session-seamless-travel-state-stores.md 30cc95d4ce21bf4df055b024d3f5961bff210c37
Reviewed-Content-Hash: docs/architecture/adr-0006-seeds-random-streams.md 78d2d19a4cdf0c77695cbe4710dd8f587711b6c7
Reviewed-Content-Hash: docs/architecture/adr-0007-code-modules-include-boundaries.md dd9dfa3edae7b5e27568b1f3b97a3698aeac2b5d
Reviewed-Content-Hash: docs/architecture/adr-0008-test-automation-leak-parity.md 468ead270c88a7c2c36e61c59fa32f04712ec9f7
Reviewed-Content-Hash: docs/architecture/adr-0009-version-control-binary-assets.md 05fc0c3756bcb9e797b725062f1966848cea6582
Reviewed-Content-Hash: docs/architecture/adr-0010-shared-pawn-cmc-possession.md cee5c3dc07d10b70386ae834778df4387cc1a3a0
Reviewed-Content-Hash: docs/architecture/adr-0011-pawn-command-interface-input.md 7838689f4d1807f9d761ae81939bac5f571b74fc
Reviewed-Content-Hash: docs/architecture/adr-0012-character-pool-appearance-allocation.md 1574653761eb70e90c9643e904ec0451c29f55da
Reviewed-Content-Hash: docs/architecture/adr-0013-animation-motion-matching-significance.md 186dd380e3eb4ad41ea0b032a025bb46d58407ca
Reviewed-Content-Hash: docs/architecture/adr-0014-layout-service-generation-navmesh.md bae3e27f5664d95e710c67237fe80ad1438c2687
Reviewed-Content-Hash: docs/architecture/adr-0015-camera-los-envelope.md ab307b1a0e39a3e05fd46dbe5ca426f87b29594c
Reviewed-Content-Hash: docs/architecture/adr-0016-crowd-ai-architecture.md 4c11b4adf21c2eb7fcf40f001e96ed95f9ee3723
Reviewed-Content-Hash: docs/architecture/adr-0017-concealment-per-viewer-presentation.md 860af24e5b2907c7469cce1d3ac751c43bdf08d2
Reviewed-Content-Hash: docs/architecture/adr-0018-encounter-outcome-timing.md ba5b0bab2e29bc005543f136dfea0ead5cbadc86
Reviewed-Content-Hash: docs/architecture/adr-0019-informant-lifecycle-takeover.md 2714c02829745a2d7d5845320ed9c842612724f7
Reviewed-Content-Hash: docs/architecture/adr-0020-objectives-data-team-view.md c9b01938f7b4dbc95671f5e00505e6c26da4ca6e
Reviewed-Content-Hash: docs/architecture/adr-0021-ui-hud-layering-commonui.md ea476817ec5b486cb3e824b6156d4118e9575370
Reviewed-Content-Hash: docs/architecture/adr-0022-rendering-performance-budgets.md 93b8539f3bcde74f208278aafd97258cde8cc982
Reviewed-Content-Hash: docs/architecture/adr-0023-playtest-telemetry-hooks.md 4abe1d1c3ab480559124ccc0d93c7ad46fde4ed5
Reviewed-Content-Hash: design/gdd/assassination-defence-reversal.md 37ccacdd9bd76ad853977fb33be87e243085b100
Reviewed-Content-Hash: design/gdd/camera-system.md 21f94a1f50d1fb635a16657383a7d5b36c6cfa87
Reviewed-Content-Hash: design/gdd/character-pool-appearance.md 8ec42eeb251eab663076620d6adadb251088d3b8
Reviewed-Content-Hash: design/gdd/concealment-bushes.md c6ceb16f2b389264e5d69a8437bb72868df18504
Reviewed-Content-Hash: design/gdd/crowd-npc-behaviour.md cb5a02b9d6f1111495736074d067248965a04fce
Reviewed-Content-Hash: design/gdd/game-concept.md 8dccb622e17a7f1dc58349dd2baa54d52fc36d32
Reviewed-Content-Hash: design/gdd/gdd-cross-review-2026-10-24.md 3c5b5d1c9faf25998e07b76e01a4bf269841dad0
Reviewed-Content-Hash: design/gdd/hud.md 8eefe3da6268eb2566076f1659a46c4726d54087
Reviewed-Content-Hash: design/gdd/information-visibility-policy.md 2c4160f79d3e3822b80172db6dd1147e0eb856cc
Reviewed-Content-Hash: design/gdd/input-interaction-framework.md 4759241f4c2ac3bf570a9660f449360975cf0374
Reviewed-Content-Hash: design/gdd/level-socket-contract.md a8310a985e5c3684507752cb6a2d7c32cc521fdd
Reviewed-Content-Hash: design/gdd/npc-kill-penalty-informants.md d0998191537a02408eaf9fcf0d7c2b9b1ddeba6a
Reviewed-Content-Hash: design/gdd/objectives-chain.md bb247ec67de54231de83c83dbfc17e57fed33869
Reviewed-Content-Hash: design/gdd/participant-state-roster.md e7223d4f08ab5671ba4a21df5a742e4d78c39592
Reviewed-Content-Hash: design/gdd/procedural-city-generator.md 7b91fc168d2beeaaa2fdcee49ffea9e3e7a25ea5
Reviewed-Content-Hash: design/gdd/round-match-framework.md ec6a7ecb985d6e7d252083367f0beacb1ad59ea3
Reviewed-Content-Hash: design/gdd/session-travel.md 26102a596afc758aff5c85e01efc2502063d65e4
Reviewed-Content-Hash: design/gdd/shared-character-locomotion.md 0ad02c8cc3f836d7ada07710ba62318ec00f0640
Reviewed-Content-Hash: design/gdd/systems-index.md f2a36200d3309b93728e348e2d7961a6fc63cd69
Reviewed-Content-Hash: design/gdd/teammate-markers.md 485310be9052a6f893423f7b9500a0c63928be58
Reviewed-Content-Hash: design/gdd/tuning-data-config.md 5552c1511e5c085d3bd61e5406cfc370b48934e6
Reviewed-Content-Hash: docs/architecture/tr-registry.yaml 7f8e2e41f512c577be66ec1f902be63f08c21080
Reviewed-Content-Hash: docs/architecture/architecture.md fbd7ba9c423f1dfa9cc6b886633870d46ed81104
Reviewed-Content-Hash: docs/engine-reference/unreal/PLUGINS.md 4a7765252f7cd3774bfb2af2b40715cfafac5eab
Reviewed-Content-Hash: docs/engine-reference/unreal/VERSION.md b83420d1df349e3d48f80a323496b07d7e37914e
Reviewed-Content-Hash: docs/engine-reference/unreal/breaking-changes.md 7aad70a8c4b45e1269618b6ef02247eaa7585c39
Reviewed-Content-Hash: docs/engine-reference/unreal/current-best-practices.md 84434704bf5129944731298240b0854706b3ccba
Reviewed-Content-Hash: docs/engine-reference/unreal/deprecated-apis.md 9369070e611a0ae2de960c4c419bab1fcfc3085f
Reviewed-Content-Hash: docs/engine-reference/unreal/modules/animation.md 22b215d25348939d9f3b1791d4b083252b9966d2
Reviewed-Content-Hash: docs/engine-reference/unreal/modules/audio.md 26d7beb487b226cf9ba970ada91a566b6a33b17d
Reviewed-Content-Hash: docs/engine-reference/unreal/modules/input.md 5771e81964e6252ff5db2761c79be9402d4c18d8
Reviewed-Content-Hash: docs/engine-reference/unreal/modules/navigation.md 82fdfed513824a4832f3d0562fa7eabef25de921
Reviewed-Content-Hash: docs/engine-reference/unreal/modules/networking.md 05e96461186c3349240f8d3e41e21a02886f936d
Reviewed-Content-Hash: docs/engine-reference/unreal/modules/physics.md 0c80abde74bd5c12e78d0aa1f00ce378e5bb6625
Reviewed-Content-Hash: docs/engine-reference/unreal/modules/rendering.md f4c0a9bfc1f4fb4d3244c596e3d61c3d6f7b0b72
Reviewed-Content-Hash: docs/engine-reference/unreal/modules/ui.md 5dc9788a910030b8754bdc987716d5e94e9173da
Reviewed-Content-Hash: project.yaml c7fe5f5b9ec92a5dd046aaa64c24a37c1c6c2737
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

Freshness check: no prior architecture-review report (RECEIPT: NONE); full review run. docs/consistency-failures.md does not exist (not stamped, not created).

---

## Traceability Summary

Total requirements: **177** (170 baseline IDs from architecture.md Appendix A, now registered as permanent; 7 new IDs from the 2026-10-24 ADR-GDD sync)

| Status | Count |
|---|---|
| ✅ Covered (Accepted ADR) | 0 |
| 🟡 Covered (Proposed ADR) | 174 |
| ⚠️ Partial | 3 |
| ❌ Gap | 0 |
| ❓ Not assessed | 0 |

Method: every ADR's `## GDD Requirements Addressed` table (present in all 23) was mapped to the TR baseline. GDD rule IDs that no ADR table cites were then grepped across ADR bodies (Roster R1/R2/R5/R7/R8, Round R5/R8/R10/R21, IVP R9/R15, LSC R3/R10, Generator R-G4/R-G6/R-G12). Each is covered by an ADR body, or is GDD-level logic housed in an ADR-defined store (for example the Roster transition table in ADR-0005's `FRosterCore`). The 2026-10-24 "ADR-NNNN sync" rows in all 18 GDDs were read to find requirements added or reworded since the baseline. The full matrix is in `docs/architecture/requirements-traceability.md`.

Coverage rests entirely on `Proposed` ADRs; `create-control-manifest`, `create-epics`, `create-stories` and `gate-check` require `Accepted`. Route out: `/architecture-decision accept ADR-NNNN` per ADR, in the bootstrap order (ADR-0009 → ADR-0007/0008 → spikes S1/S2/S14 → ADR-0001 → …).

### Partial coverage

| TR-ID | Requirement | Why partial |
|---|---|---|
| TR-con-005 | Hide clips scheduled from replicated server start time | `FActionState.StartServerTime` is a reserved slot pending the user (Concealment CR-10 b, known); ADR-0017 §8 runs the `HidePoseStartServerTime` stopgap |
| TR-pen-006 | Owner-only tally, `bExiled`, `T_due` | `T_due` carrier pending the user (C-W5 / AQ-02, known); ADR-0001 §6 slot, HUD F7 interim |
| TR-round-013 (new) | Event Log ring bound; kill timeline protected from eviction; `TruncatedCount`; telemetry section | Round R19 (synced 2026-10-24) protects kill-timeline entries; ADR-0023 §5, risk R-4 and Related still say "ring unchanged" and list the protection as an open change request. ADR-0023 has no Amendments section |

### TR registry changes
- `docs/architecture/tr-registry.yaml` bootstrapped (was `requirements: []`): version 2, 177 entries; `registry.sh validate` clean (0 duplicate, 0 status, 0 layout, 0 path rows).
- All 170 provisional baseline IDs kept unchanged (no ID was taken, so no renames). ADRs and the architecture doc may now cite them without "(provisional)".
- New: TR-roster-012 (`OnViewRebound` after `RebindView`), TR-iif-013 (`PossessionEpoch` via `ClientSetPossessionEpoch`), TR-round-013 (Event Log bound and protection), TR-ivp-012 (owner clock-exchange RPC pair), TR-scl-014 (engine CMC net settings boot-tested against I-SCL-15), TR-con-011 (`BushSpotPick` unbiased draw), TR-asn-011 (owner results in the batched `ClientCommandReplies` RPC).
- Registered in synced wording: TR-camera-006, TR-camera-008 (rate bands, never a cutoff), TR-iif-002.
- Observation: only 55 distinct TR-IDs appear in ADR text; most ADR tables cite GDD rule IDs. Add TR-IDs to each ADR's GDD Requirements table at acceptance so stories can trace them.

## Coverage Gaps (no ADR exists)

None. Every requirement has at least one ADR. Not traced as TRs: art bible 8.x budgets (ADR-0022), game-concept VCS concern (ADR-0009).

---

## Cross-ADR Conflicts

Phase 4 was run by the technical-director (sub-agent); the blocking item was re-verified in the ADR text by this reviewer. Known pending user decisions and reserved slots were excluded.

### 🔴 Conflict 1: ADR-0002 vs ADR-0018 (also ADR-0017, ADR-0019, ADR-0007) — BLOCKING
Type: Integration (type ownership and module direction)
- **ADR-0002 claims** (Key Interfaces): `FQueueItem` and `FStagedWrite` live in **StealthFoundation `Public/View`** as closed `TVariant`s naming `FDeathWritePayload`, `FCounterPayload`, `FRoundEndPayload` and one `FTimerPayload`. The registration surface is `RegisterResolver` / `RegisterTimerResolver` / `SetTerminalDecision` / `SetEventFlushBarrier`; there is no staged-write applier registration.
- **ADR-0018 claims** (Key Interfaces, l.263-271): `FDeathWritePayload` and `FCounterPayload` are declared in **StealthGameplay Features/Assassination `Public/Server`**. ADR-0017 declares `FConcealTransitionPayload` (l.233) and ADR-0019 `FInformantRespawnPayload` (l.236) in their feature modules. All use `FPawnHandle`, which no ADR declares.
- **ADR-0007 claims:** lint rule (e): Foundation includes nothing from StealthGameplay; "no lower-layer file names a feature type".
- **Impact:** the Foundation queue header cannot compile as specified; per-feature timer payloads have no slot in the closed variant; the staging pass has no defined route to apply a `DeathWrite` or `CounterPose`; server-only payloads would sit in a `Public/View` header.
- **Resolution options:**
  1. **(Recommended)** Amend ADR-0002 to own ID-only payload structs and `FPawnHandle` in Foundation `Public/Server`; make `FTimerPayload` a generic fixed shape (for example `{FSubjectRef Subject; uint32 Generation; uint32 Seq; uint8 Phase}`); add `RegisterStagedWriteApplier(EStagedWriteType, TWeakInterfacePtr<…>)`. ADR-0017/0018/0019 then reference those types.
  2. Type-erase the payloads (fixed bytes plus a kind tag) so features keep their own structs. This loses type safety and makes headless specs harder.

### 🟡 Conflict 2: ADR-0003 vs ADR-0018 — NON-BLOCKING (Integration)
`FActionState` is declared twice: ADR-0003 (l.226-231) with `uint8 Pose` and `WithNetSerializer` only; ADR-0018 (l.227-234) with `EActionPose Pose`, `WithNetSerializer` + `WithIdentical`, plus `FFixedWarpOffset`. The field set and 144-bit width agree. Recommended: ADR-0003's block points to ADR-0018's declaration (the semantic owner).

### 🟡 Conflict 3: ADR-0001 vs ADR-0003 (and ADR-0010) — NON-BLOCKING (Pattern)
ADR-0001 §4.2 says the possession `ForceNetUpdate` is "deferred to the next fixed-cadence boundary". ADR-0003 §2 drops unscoped calls (the cadence carries the change) and rejects "defer with a flag" (Alternative 6); ADR-0010 §4.1 follows ADR-0003. Recommended: amend ADR-0001's wording to match ADR-0003.

### 🟡 Conflict 4: ADR-0011 vs ADR-0007 — NON-BLOCKING (header placement)
ADR-0011 places `UPawnCommandComponent` and `ISharedPawnStateView` in `StealthGameplay/Core/Public` (neither `View/` nor `Server/`); ADR-0007 lint rule (a) fails that. Recommended: `Public/Server/Core/`, with the component forward-declared in the pawn header.

### 🟡 Conflict 5: ADR-0017 vs ADR-0001 — NON-BLOCKING (write path)
ADR-0017 §3 has Concealment set the C-TeamView teammate `bIsConcealed` "through the Roster's team-view API". ADR-0001 says `URosterTeamViewComponent` is written only by the Roster push step and exposes no setter. Recommended: add `SetTeammateConcealed(FParticipantId, bool)` to the Roster server face (Concealment is the only caller, the Roster the only writer) and align the registry `write_access`.

### 🟡 Conflict 6: ADR-0017 vs ADR-0018 / ADR-0002 — NON-BLOCKING (missing hook)
ADR-0017 needs `OnConcealedBodyRemoved(Victim)` at the server teardown of a pawn killed while concealed. ADR-0018 §6.2 `TimerFired(BodyRemoval)` only calls `RequestTeardown`, and ADR-0002 has no teardown observer. Recommended: ADR-0018's BodyRemoval resolver calls Concealment's hook through a consumer-owned interface.

### 🟡 Conflict 7: ADR-0017 vs ADR-0004 — NON-BLOCKING (naming)
`FConcealClientTuning` (ADR-0017 l.257) breaks ADR-0004's `F<Owner>Tuning_Public` convention reached through `FStealthClientTuning`. Recommended: rename to `FConcealmentTuning_Public`.

### 🟡 Conflict 8: ADR-0019 vs ADR-0005 / ADR-0001 / ADR-0012 — NON-BLOCKING (unapplied amendment)
ADR-0019 Related item 8 asks that `RequestTransition(StrikeLimit)` release (not retire) the pawn, with the Roster calling `ReleaseToCrowd` internally. ADR-0005's Amendments applied items 3, 7 and 9 only, and ADR-0012 (l.155-157) names Round/Penalty as the `ReleaseToCrowd` callers. The Roster and Penalty GDDs already say "release, not retire". Recommended: amend ADR-0005 and ADR-0012's caller wording.

### 🟡 Conflict 9: requesting ADRs vs their own records — NON-BLOCKING (record staleness)
Requests that their targets have already applied are still written as open in ADR-0010 (l.448), 0011 (l.423), 0016, 0017, 0018, 0019, 0020 and 0021. ADR-0019/0020/0021 have no Amendments section, and the Status lines of ADR-0018..0021 still gate acceptance on those answers. Recommended: a one-line "requests answered (2026-10-24)" note in each ADR.

### Other ADR consistency findings (this reviewer)
- ADR-0023 §5, R-4 and Related are stale against Round R19 (see TR-round-013).
- ADR-0008's GDD Requirements table still says G-DECK uses a "checked-in seed file", but its own §9 amendment moved the seeds to a `constexpr` array. The CPA GDD sync row asked this review to align ADR-0008: fix the table row.
- No ADR names the Roster rebind event, which the Roster GDD now calls `OnViewRebound(ParticipantId)` (ADR-0020 sync). Name it in ADR-0005 (owner) and ADR-0020 (subscriber).

### Verified clean (Phase 4)
- `ETimerKind` / `EHoldTimerKind` contain every requested kind (including `ConcealmentTransition`, `BodyRelease`, `BodyRemoval`, `ConditionCancel`).
- All 14 seed domains (including `BushSpotPick` = 14) match their consumers.
- `FActionState` has 5 fields / 144 bits everywhere, with the single writer `WriteActionState`. Single writers hold for `FDeathState`, the latch, `EventSeq` and the Event Log; forced-update reasons are consistent.
- `ICrowdControl`, `ITakeoverEligibilitySource`, `FRespawnPick`, `IInteractTargetOwner`, `GetScreenFade`, `GetViewState` / `GetAimRay` are consistent with their callers.
- Registry: 46 state entries with one owner each; no duplicate contract.
- Tick order is consistent (drain `TG_PostPhysics`, cull `TG_PostUpdateWork`, hand-over and Crowd `TG_PrePhysics`).
- The game-thread budget is identical in architecture.md and ADR-0022 and sums to 12.0 ms, with every per-ADR claim inside its line. Animation ≤ 4.0 ms (GT ≤ 2.0), GPU 14.4 ms including the 0.5 ms UI line, bandwidth ≤ 50 KB/s (pawns ≤ 30).
- Test naming is `StealthGame.<System>.<Scenario>` everywhere.
- Not covered: field-by-field `FObjectiveTeamView` against ADR-0021's consumers, the ADR-0013/0015 eye-mask flag contract, the registry's `api_decisions` / `forbidden_patterns`.

---

## ADR Dependency Order

`adr-dep-graph.sh`: 23 ADRs, 151 `Depends On` edges, `NO_DEPS_SECTION` none, `UNFILLED_DEPS` none.

🔴 **Dependency cycles (as declared; one CYCLE line each):**
- ADR-0001 → ADR-0007 → ADR-0008
- ADR-0002 → ADR-0006 → ADR-0007 → ADR-0008
- ADR-0004 → ADR-0001
- ADR-0005 → ADR-0001 → ADR-0004
- ADR-0006 → ADR-0005
- ADR-0007 → ADR-0001 → ADR-0002 → ADR-0004
- ADR-0008 → ADR-0001 → ADR-0004 → ADR-0006 → ADR-0007
- ADR-0017 → ADR-0018
- ADR-0018 → ADR-0017

Blocked by the cycles (on no cycle themselves): ADR-0003, 0010-0016, 0019-0023. `ORDER:` emitted only ADR-0009 and ADR-0024.

Interpretation: the Foundation cycles come from two ADRs whose `Depends On` cells list **content or placement inputs**, not acceptance prerequisites. ADR-0007 says "None for the module skeleton itself. Content placement depends on ADR-0001, 0002, 0004". ADR-0008 depends on ADR-0001 ("what the leak suites assert"), ADR-0004 and ADR-0006. As declared, they contradict architecture.md's bootstrap order (ADR-0009 → ADR-0007/0008 → spikes → ADR-0001), because ADR-0001 itself depends on ADR-0007/0008.
- Edges to cut: ADR-0007 → {0001, 0002, 0004} and ADR-0008 → {0001, 0004, 0006}. Move them to the Ordering Note as "content inputs; suites land with those ADRs".
- ADR-0017 ↔ ADR-0018 is a genuine mutual dependency: accept them as a pair (say so in both Ordering Notes), or cut ADR-0017 → ADR-0018 to a consumer-owned interface.
- ADR-0004 → ADR-0024 points at a deferred, non-existent ADR (it only applies if Iris is adopted): reword it as conditional.

⚠️ Every edge points to a `Proposed` ADR, so no ADR can be safely implemented until its prerequisites are Accepted.

### Recommended ADR Implementation Order (after the cuts above)
Foundation:
1. ADR-0009 Version control (blocked on the user)
2. ADR-0007 Code modules and include boundaries
3. ADR-0008 Test automation, leak and parity (with ADR-0007), then spikes S1/S2/S14
4. ADR-0001 Crowd identity rules
5. ADR-0004 Tuning data pipeline
6. ADR-0005 Session, seamless travel, state stores
7. ADR-0006 Seeds and random streams
8. ADR-0002 Resolution queue and round-end staging (after its blocking amendment)
9. ADR-0003 Replication, relevancy, concealment cull

Core:

10. ADR-0010 Shared pawn, CMC, possession
11. ADR-0011 Pawn command interface and input
12. ADR-0012 Character pool, ADR-0014 Layout service, ADR-0015 Camera (in parallel)
13. ADR-0013 Animation (requires ADR-0015; spike R3 on 3060-class hardware)

Feature:

14. ADR-0016 Crowd AI (requires ADR-0014, ADR-0011)
15. ADR-0017 + ADR-0018 Concealment and encounter timing (as a pair)
16. ADR-0019 Informant lifecycle, ADR-0020 Objectives

Presentation / cross-cutting:

17. ADR-0021 UI/HUD, ADR-0022 Rendering budgets, ADR-0023 Telemetry

---

## GDD Revision Flags (Architecture → Design Feedback)

No HIGH engine finding contradicts a GDD. One verified MEDIUM finding underlies GDD rules. It is advisory and recorded here only: the systems index was not changed, because Phase 5b's update applies to HIGH findings and this run owns no index row.

| GDD | Assumption | Reality (engine-reference / headers) | Action |
|---|---|---|---|
| shared-character-locomotion.md (net config l.122; `U_eff` with `T_tick = 1/NetServerMaxTickRate` l.345/354; I-SCL-10 l.570; l.631) | The listen host's server tick is `1/NetServerMaxTickRate`, a configured multiple of `PawnNetUpdateHz`, checked at startup | On a listen server `NetServerMaxTickRate` applies only with `bClampListenServerTickRate=true`, which also caps the host's render rate ([V] `GameEngine.cpp:1742`, `NetDriver.h:869`). Otherwise `T_tick` is the host's variable frame time | Redefine `T_tick` on the host frame time (or adopt the clamp) and restate I-SCL-10; coordinate with ADR-0003 (pending user choice) |
| round-match-framework.md (SCL CR-4 row l.642; R23(h)) | "`NetServerMaxTickRate` multiple of `PawnNetUpdateHz`, ≥ 60" is an enforceable guard | Same | Reword the guard together with the SCL revision |

---

## Engine Compatibility Issues

### Engine Audit Results
Engine: Unreal Engine 5.8, installed 5.8.4. ADRs with an Engine Compatibility section: **23 / 23**. Each has the Engine, Domain, Knowledge Risk, References Consulted, Post-Cutoff APIs Used and Verification Required fields.

- **Deprecated API references:** none relied on. Every hit is a name the ADR rejects or replaces:
  - `PhysicsMover` / `GetCharacterOwner`: ADR-0010 uses `GetOwnerAdapter()`.
  - `DispatchPostLogin`: ADR-0005 uses `OnPostLogin`.
  - `UPlayerMappableInputConfig` / Raw Input: ADR-0011 uses `UEnhancedInputUserSettings`.
  - `SetSkeletalMesh`: ADR-0012 uses `SetSkeletalMeshAsset`.
  - `UE_LOG`: policy text only; no call sites.
- **Stale version references:** none. The 5.7 mentions are correct history.
- **Post-cutoff API conflicts between ADRs:** none.
  - All ADRs assume the legacy NetDriver ([V] `net.Iris.UseIrisReplication` defaults to 0, `IrisConfig.cpp:15`).
  - ADR-0001 and ADR-0020 agree that the push model is off by default.
  - ADR-0015, ADR-0017 and ADR-0022 agree on hidden-primitive behaviour.
  - The two `SetVisibility` overloads cited both exist.

### Engine Specialist Findings (unreal-specialist)
1. **MEDIUM: the engine-reference docs give a dead CVar name.** `breaking-changes.md` and `modules/input.md` say `EnhancedInput.IgnoreHeldKeysOnFlush`; the registered name is `EnhancedInput.IgnoreHeldDigitalActionKeysOnFlush` ([V] `EnhancedPlayerInput.cpp:47-49`). ADR-0011/0021 and the IIF GDD use the correct name. Fix the reference docs (outside this run's write scope; logged).
2. **MEDIUM: listen-server tick (ADR-0003 server-tick row, ADR-0002 Verification (3)).**
   - See GDD Revision Flags. ADR-0003's [U] becomes [V].
   - The "NetServerMaxTickRate ≥ 60 and a multiple of PawnNetUpdateHz" startup check is a no-op on the default config.
   - ADR-0003's `NetClientTicksPerSecond ≥ MaxConnections × MaxFps` check needs a defined MaxFps.
   - Pending user choice: the clamp or a frame-time restatement (logged).
3. **MEDIUM: MotionWarping is Beta in 5.8.4** ([V] `.uplugin` has `IsBetaVersion: true`, `VersionName 0.1`), and neither ADR that relies on it records this:
   - ADR-0010 uses it for the traversal warp.
   - ADR-0018 E5 applies `DISABLE_REPLICATED_PROPERTY_FAST` to the protected `WarpTargets` ([V] `UnrealNetwork.h:414`, `MotionWarpingComponent.cpp:299`).
   - Action: add "MotionWarping: Beta" to both Knowledge Risk rows, and put the protected-member disable on the engine-upgrade checklist.
4. **LOW-MEDIUM: ADR-0016's plugins are not enabled.** `StateTree` and `GameplayStateTree` are missing from `StealthGame.uproject` ([V]) and from every pending-action list. Logged beside the CommonUI enable.
5. **LOW: `bEnableEnhancedInputSupport` still defaults to false in 5.8** ([V] `CommonInputSettings.h:114`, `ConfigRestartRequired`). The ADRs' pin value `true` is correct; the editor must restart after it is set.
6. **LOW: errors in the reference docs (no ADR relies on them):**
   - `deprecated-apis.md` lists `DOREPLIFETIME()` as deprecated; it is not.
   - `modules/networking.md` mis-comments `COND_InitialOnly`.
   - The networking examples still use `UE_LOG`.
7. **Anti-pattern areas checked and holding:**
   - Owner-only `APawn::PlayerState` / `Controller` and `AActor::Owner` via the reset helpers ([V] `Pawn.cpp:1307-1308`, `ActorReplication.cpp:563-569`, `UnrealNetwork.h:407-510`). The residual risk is behavioural and belongs to spikes S1/S14.
   - Relevancy override with explicit channel close; it bans `ForceNetRelevant` / `ForceActorRelevantNextUpdate`.
   - CMC compressed flags.
   - UMG drawn over the camera fade.
   - CPD dither with Masked blend and the Nanite prepass (RC-3).
   - Hidden-proxy behaviour ([V] `PrimitiveComponent.cpp:2106-2122`).
   - Engine URO keys on player control ([V] `SkinnedMeshComponent.cpp:205-210`), so ADR-0013's bypass is justified.

   Also verified clean:
   - `NetClientTicksPerSecond=200` default; `CommonUI.AutoFlushPressedKeys` true; `SearchThrottleTime` exists.
   - Plugin flags: PoseSearch, Chooser, AnimationBudgetAllocator, SignificanceManager, GameplayStateTree and CommonUI are not Beta; Iris is Beta.
   - Gameplay Cameras is Experimental and not adopted.

   Not covered: most [V] citations were trusted (about 15 spot-checked, all matched), and ADR-0007/0008's C++20 and test-naming claims were not checked against `.claude/rules/test-standards.md`.

---

## Architecture Document Coverage

- All 18 MVP systems in systems-index.md appear in architecture.md's layer map. Systems 19-30 (VS/Alpha/Full Vision) are out of build scope by design.
- No orphaned architecture: Resolution Queue & Server Clock, Seeds & RNG, Event Log, the game-instance store container and the fault sink all derive from Round and Session GDD rules.
- **architecture.md is stale after ADR authoring.** This review did not edit it, because it is not this skill's output.
  - "ADR Audit" still says no ADRs exist and 0/170 are covered.
  - Appendix A still marks every ID provisional; they are now permanent.
  - The Platform layer lists AnimationBudgetAllocator, which ADR-0013 rules out.
  - The Required ADRs rows for ADR-0013/0015 keep superseded mechanisms (its authoring-status note says the ADR governs).
- **Data Flow and API Boundaries do not name contracts the ADRs introduced:** the `ClientCommandReplies` batched owner RPC, `ClientSetPossessionEpoch`, the owner clock-exchange RPC pair, `IInformantBodyProvider`, `ICrowdPerceptionFilter`, `IStrikeTallySource`, `IGaitLegalitySource` and the Roster rebind event. Refresh architecture.md (data flow, API boundaries, ADR audit, Appendix status) once the amendments land.

---

## Verdict: FAIL

The verdict is FAIL because of one blocking cross-ADR conflict (Conflict 1). ADR-0002's Foundation queue types name feature payload structs that ADR-0017/0018/0019 declare in StealthGameplay, and there is no staged-write applier registration; ADR-0007's include rule forbids that direction.

Apart from that:
- Requirement coverage is complete: 0 gaps, 3 partial.
- Engine targeting is consistent, with no HIGH engine issue.
- All coverage rests on `Proposed` ADRs, so the verdict stays capped at CONCERNS even after the blocker is fixed.

### Blocking Issues (must resolve before PASS)
1. Amend ADR-0002 (Conflict 1, option 1) and update ADR-0017/0018/0019 to reference Foundation-owned payloads and `FPawnHandle`.
2. Break the declared dependency cycles:
   - Move the content/placement inputs out of ADR-0007's and ADR-0008's `Depends On`.
   - Pair ADR-0017/0018 explicitly, or cut one edge.
   - Reword ADR-0004's conditional edge to ADR-0024.
3. The user accepts the ADRs, in the bootstrap order, after spikes S1/S2/S14 (and R1-R3 etc. where named) and the pending user decisions.

### Required ADRs
No new ADR is required for MVP coverage. Required amendments, most foundational first:
1. ADR-0002: payload ownership and `RegisterStagedWriteApplier` (blocking); reword Verification (3) as a host frame-rate condition.
2. ADR-0007 / ADR-0008: the `Depends On` cells (cycles); ADR-0008's G-DECK row in its GDD table.
3. ADR-0001: possession `ForceNetUpdate` wording; the Roster `SetTeammateConcealed` setter.
4. ADR-0003: point its `FActionState` block to ADR-0018; mark the server-tick row [V] and settle the listen-server tick basis.
5. ADR-0005 / ADR-0012: `ReleaseToCrowd` inside `RequestTransition(StrikeLimit)`; name `OnViewRebound`.
6. ADR-0011: move the headers under `Public/Server/Core/`.
7. ADR-0017 / ADR-0018: BodyRemoval → Concealment hook; `FConcealmentTuning_Public`; the pair ordering note. ADR-0010 / ADR-0018: add MotionWarping Beta to Knowledge Risk.
8. ADR-0019..0023: "requests answered (2026-10-24)" notes; bring ADR-0023 §5, R-4 and Related in line with Round R19.

---

## Handoff

Immediate actions:
1. Amend ADR-0002 (blocking).
2. Fix the Foundation `Depends On` cycles in ADR-0007/0008 and pair ADR-0017/0018.
3. Apply the non-blocking amendments above, then re-run `/architecture-review`.

Pre-gate checklist:
- ✅ Unreal test root `Source/StealthGame/Private/Tests/` (prototype module; ADR-0007 moves production tests into the module layout)
- ❌ `.github/workflows/tests.yml`: run `/test-setup` (needs the VCS decision first)
- ❌ `design/accessibility-requirements.md`: run `/ux-design`
- ❌ `design/ux/interaction-patterns.md`: run `/ux-design`

Re-run `/architecture-review` after the amendments and after each ADR is accepted, to confirm coverage moves from 🟡 to ✅. The closing "what next" question was skipped (autonomous mode); the recommended next step is the ADR-0002 amendment.

New pending user decisions (appended to production/session-state/active.md):
- The listen-server tick basis: the clamp, or a frame-time restatement (recommended: frame-time).
- Enable the `StateTree` / `GameplayStateTree` plugins.
- Engine-reference doc corrections (outside this run's write scope).

---

## Fixes applied after review (2026-10-25)

Applied by the architecture-review fix pass, an autonomous single editing agent. Every change is listed in its target ADR's "Amendments" section as "architecture-review fix 2026-10-25". No ADR Status was changed: all ADRs stay `Proposed`. The verdict line above is left unchanged for the independent re-check. Decisions are logged in `production/session-logs/decision-log.md` under `architecture-review-fix`. No technical-director consult was needed, because no finding required a real trade-off.

**Conflict 1 (BLOCKING): payload ownership and staged-write appliers**
- **ADR-0002** has a new §2a. All of the following move to `StealthFoundation/Public/Server/StealthQueuePayloads.h`, and the queue types leave `Public/View`:
  - every `FQueueItem` and `FStagedWrite` payload;
  - `FPawnHandle { TWeakObjectPtr<APawn> }`, whose generation is the weak pointer's serial number ([V] `WeakObjectPtr.h`);
  - `FSubjectRef`;
  - a generic `FTimerPayload {Kind, Subject, Generation, Seq, Phase}`, with a mapping table for every `ETimerKind`;
  - `FConnectionTransitionPayload`, added because that item type had no payload.
- **ADR-0002 §6** adds `RegisterStagedWriteApplier(EStagedWriteType, TWeakInterfacePtr<IStagedWriteApplier>)`, called from the composition root.
  - `RoundEndStep` routes to `ITerminalDecision`.
  - A write with no applier is dropped, logged and reported to the fault sink.
  - `ResolvePawnHandle` is added. Key Interfaces, guidelines and validation are updated to match.
- **ADR-0018** no longer declares the death-write and counter-pose payloads.
  - `UEncounterService` implements `IStagedWriteApplier` and is registered for `DeathWrite`/`CounterPose`.
  - The stray `bReopen` field is removed.
- **ADR-0017** drops `FConcealTransitionPayload` in favour of the `FTimerPayload` mapping.
- **ADR-0019** drops `FInformantRespawnPayload` in favour of the `FTimerPayload` mapping.
- **ADR-0011, ADR-0016 and ADR-0020** record their mappings (`FCommandPayload`/`FHoldTimerPayload` values, `CrowdSchedule`, `StageSignal`).
- **ADR-0007** module table, §4 and Key Interfaces now state two things:
  - Foundation holds the payloads, `IStagedWriteApplier` and `IEventFlushBarrier`.
  - A type that a Foundation container names is declared in Foundation, and gameplay modules register resolvers and appliers by type.

**Dependency cycles (blocking item 2)**
- ADR-0007's Depends On is now "None". ADR-0008's Depends On is now ADR-0007 and ADR-0009. Their content inputs (ADR-0001, 0002, 0004, 0006) moved to the Ordering Notes.
- ADR-0004's edge to ADR-0024 is reworded as conditional (Iris only).
- ADR-0017 and ADR-0018 are an acceptance pair. The edge is kept only in ADR-0018 (ADR-0018 → ADR-0017), and both Ordering Notes state the pair.
- `adr-dep-graph.sh` now reports no CYCLE line. Its ORDER is: 0007, 0009, 0008, 0001, 0004, 0005, 0006, 0002, 0003, 0010, 0014, 0011, 0012, 0015, 0016, 0020, 0013, 0017, 0018, 0022, 0019, 0021, 0023.

**Non-blocking conflicts**
- **C2:** ADR-0003's `FActionState` block now points to ADR-0018's declaration, which is the single owner. ADR-0003 keeps the fixed-width rule.
- **C3:** ADR-0001 §4.2 and its guideline now read "dropped; the next cadence update carries the change". Ruling: ADR-0003, the replication owner, decides. This still meets ADR-0001's no-off-cadence invariant.
- **C4:**
  - ADR-0011's server pieces move to `StealthGameplay/Public/Server/Core/`. The pawn header forward-declares the component, and `ISharedPawnStateView` gets a `Private/` adapter.
  - The same rule is applied to ADR-0010's labels: the factory goes to `Public/Server/Core/`, and the pawn and movement headers go to `Public/View/Core/`.
- **C5:** `URosterStore::SetTeammateConcealed(FParticipantId, bool)` is added in ADR-0001, with notes in ADR-0005 and ADR-0017. Concealment is the only caller, and the Roster push step is the only writer.
- **C6:** ADR-0018 declares the consumer-owned `IBodyRemovalObserver`, which the BodyRemoval resolver calls for every body before `RequestTeardown`. Concealment implements it (ADR-0017).
- **C7:** `FConcealClientTuning` is removed. ADR-0017's pure functions take ADR-0004's client face `FStealthClientTuning` and read `FConcealmentTuning_Public` and `FAssassinationTuning_Public` through it. This is the client face rather than a bare rename, because the stopgap reads keys from two owners.
- **C8:** in ADR-0005, `RequestTransition(StrikeLimit)` now releases the pawn rather than retiring it, and the Roster calls `ReleaseToCrowd` internally. ADR-0012's caller and pairing wording is updated to match.
- **C9:** a "requests answered" note is added to the Amendments of ADR-0010, 0011, 0016, 0017, 0018, 0019, 0020, 0021 and 0023. ADR-0019, 0020, 0021 and 0023 gained an Amendments section. Their Status lines were not edited (Status is reserved).
- **Other findings:**
  - ADR-0008's G-DECK row now says `constexpr` seed array.
  - `OnViewRebound` is mapped to `URosterStore::OnParticipantViewRebound` in ADR-0005 and ADR-0020.
- **TR-round-013:** ADR-0023's §1, diagram, §5, R-4, GDD row and Related item (1) now match Round R19: the kill timeline is protected from eviction and `TruncatedCount` is kept per type.

**Engine items**
- **Listen-server tick:** the reviewer's option (b) is applied, logged as `design`.
  - ADR-0002 Verification (3) and ADR-0003's §1 server-tick row are restated on the host frame time. ADR-0003 gains a new E17 [V] (`GameEngine.cpp:1742`) and a `MaxFps` definition.
  - GDDs updated: `shared-character-locomotion.md` (R13, F9 `U_eff`/`T_frame`, I-SCL-10, dependency item 4, change-request row), `round-match-framework.md` (R23(h), CR-4 row, change-request row) and the `tuning-data-config.md` invariant row.
  - The clamp-flag option (a) is appended to active.md as a pending user item (Config edit).
- **MotionWarping:** recorded as Beta in 5.8.4 ([V] uplugin) in the Knowledge Risk rows of ADR-0010 and ADR-0018, with an engine-upgrade checklist note.
- **StateTree/GameplayStateTree enablement:** still the existing pending user item. The `.uproject` was not edited.

**architecture.md and registry**
- **architecture.md:** the ADR Audit is rewritten (ADR-0001..0023 Proposed, per-ADR conflict and fix status, 174/177 covered, links to this report and to `requirements-traceability.md`). Appendix A IDs are marked permanent, and the 7 new IDs are referenced. Nothing else changed.
- **docs/registry/architecture.yaml:**
  - Revised: `staged_presentation_writes`, `c_teamview`, `owner_is_concealed`, `encounter_staged_writes`, `listen_host_client_update_throttle`.
  - Added: contracts `queue_payload_types`, `staged_write_applier_registration`, `body_removal_observer` and `roster_teammate_concealed_input`; api decision `listen_host_server_tick`; forbidden pattern `feature_declared_queue_payload`.
  - `registry.sh validate` is clean.

**Not applied** (out of this pass's scope, or left for the re-check):
- engine-reference doc corrections;
- architecture.md Data Flow / API Boundaries / Platform-layer / Required-ADR row refresh;
- `requirements-traceability.md` and `tr-registry.yaml` status (the re-check updates TR-round-013);
- TR-IDs in the ADR GDD tables (at acceptance).

---

## Re-check (2026-10-25)

Independent re-check by an agent that wrote none of these documents (`/architecture-review`, scoped as a re-check, autonomous mode). Each finding above and each fix claimed in "Fixes applied after review" was checked against the current ADR text (read by section) and the registries. ADR Status was not touched: all 23 ADRs are still `Proposed`.

Checks run:
- `adr-dep-graph.sh`: 23 ADRs, 134 edges, **no CYCLE line**, no `NO_DEPS_SECTION`/`UNFILLED_DEPS`. ORDER: 0007, 0009, 0008, 0001, 0004, 0005, 0006, 0002, 0003, 0010, 0014, 0011, 0012, 0015, 0016, 0020, 0013, 0017, 0018, 0022, 0019, 0021, 0023. ADR-0024 no longer appears.
- `registry.sh validate`: entities 248, tr 177, architecture 308; duplicate 0, tr_unknown 0, path_missing 0, status 0, layout 0.
- A cross-ADR grep of every payload, handle and interface name: `F*Payload`, `FPawnHandle`, `FSubjectRef`, `IStagedWriteApplier`, `RegisterStagedWriteApplier`, `StealthQueuePayloads`, `FConcealClientTuning`.

### Per-finding status

| # | Finding | Status | Evidence |
|---|---|---|---|
| C1 🔴 | Foundation queue types name feature payloads; no `FPawnHandle`; no staged-write applier | **RESOLVED** | ADR-0002 §2a declares all of them in Foundation `Public/Server/StealthQueuePayloads.h`: every `FQueueItem`/`FStagedWrite` payload, `FPawnHandle` (weak `APawn`), `FSubjectRef`, and the generic `FTimerPayload` with a mapping row for every `ETimerKind`. §6 and Key Interfaces add `RegisterStagedWriteApplier`, `IStagedWriteApplier`, `ResolvePawnHandle`, the `RoundEndStep` → `ITerminalDecision` route and the no-applier drop. ADR-0017, 0018 and 0019 now declare no payload: the only remaining mentions of their old payload names are in Amendments lines that record the removal. ADR-0018 `UEncounterService : IStagedWriteApplier`. ADR-0007's module table, §4 corollary and Key Interfaces list the Foundation-owned types, so lint rule (e) is satisfied: every type a payload names is Foundation's own or an engine type. Exception: see N1 |
| C2 | `FActionState` declared twice | **RESOLVED** | ADR-0003 §1 keeps the wire rule only and points to ADR-0018's declaration (Foundation `Public/View`) |
| C3 | Possession `ForceNetUpdate` "deferred" vs "dropped" | **RESOLVED** | ADR-0001 §4.2 and its guideline now say "dropped; the next fixed-cadence update carries the change", which matches ADR-0003 §2 and ADR-0010 |
| C4 | ADR-0011 headers in `Core/Public` | **RESOLVED** | ADR-0011 now places them in `Public/Server/Core/`, with a forward-declared component and a `Private/` adapter. ADR-0010's headers follow the same rule (`Public/Server/Core/`, `Public/View/Core/`). No ADR still says `Core/Public` |
| C5 | Concealment writes C-TeamView without a setter | **RESOLVED** | `URosterStore::SetTeammateConcealed` appears in ADR-0001 Key Interfaces (l.269), ADR-0005 and ADR-0017 §1/§3, and in the registry contract `roster_teammate_concealed_input` |
| C6 | No body-removal hook | **RESOLVED** | ADR-0018 §6.2 declares the consumer-owned `IBodyRemovalObserver`, called before `RequestTeardown`. ADR-0017's `UConcealmentService` implements it. Registry `body_removal_observer` |
| C7 | `FConcealClientTuning` naming | **RESOLVED** | Removed. ADR-0017's pure functions take ADR-0004's `FStealthClientTuning` |
| C8 | `StrikeLimit` release, not retire | **RESOLVED** | ADR-0005 §2 (l.137) and ADR-0012 l.157/l.427 caller wording |
| C9 | Stale "open request" records | **RESOLVED** (as recommended) | ADR-0010, 0011, 0016, 0017, 0018, 0019, 0020, 0021 and 0023 now have a "Requests answered" note in their Amendments. The body text was not edited, so some request wording still reads as open. Example: ADR-0017 §3 l.104 and ADR-0016's Ordering Note l.59 still say "needs an ADR-0002 amendment". Status lines are reserved and unchanged. Informational only |
| Cycles | 9 declared cycles | **RESOLVED** | ADR-0007 Depends On: None. ADR-0008 Depends On: ADR-0007 and ADR-0009. ADR-0004's link to ADR-0024 is now a conditional note. ADR-0017 and ADR-0018 are an acceptance pair (the edge is ADR-0018 → ADR-0017 only). The graph script reports no cycle |
| Other | ADR-0008 G-DECK table row | **RESOLVED** | l.231 now reads "`constexpr` array" |
| Other | Roster rebind event unnamed | **RESOLVED** | `URosterStore::OnParticipantViewRebound` in ADR-0005 l.406/l.412 and ADR-0020 §6 / Key Interfaces |
| TR-round-013 | ADR-0023 stale against Round R19 | **RESOLVED** | ADR-0023 §1 (l.88), the diagram (l.181), R-4 (l.286), the GDD row (l.296) and Related (1) are marked as answered. The TR moves from ⚠️ to 🟡 |
| Engine 2 | Listen-server tick | **RESOLVED** (option b) | ADR-0003 E17 [V] and the server-tick row are restated on `T_frame`, and `MaxFps` is defined. ADR-0002 Verification (3) is restated. In the GDDs, SCL R13/F9/I-SCL-10, Round R23(h) and the TDC invariant row match, and no GDD or ADR keeps a `NetServerMaxTickRate` startup check. The clamp option is still a pending user item |
| Engine 3 | MotionWarping Beta not recorded | **RESOLVED** | Recorded in the Knowledge Risk rows of ADR-0010 (l.27) and ADR-0018 (l.27), with the upgrade-checklist note |
| Engine 1, 6 | Engine-reference doc errors | **OPEN** (outside the ADR scope; write-protected path) | Unchanged; already a pending user item |
| Engine 4 | StateTree/GameplayStateTree not enabled | **OPEN** (pending user; `.uproject` edit) | Unchanged |
| Phase 6 | architecture.md stale | **PARTIAL** | The ADR Audit and Appendix A were refreshed. Not yet refreshed: Data Flow, API Boundaries, the Platform layer (AnimationBudgetAllocator) and the Required-ADR rows for ADR-0013/0015 (see N2) |

Counts: **16 RESOLVED** (1 blocking conflict, 8 non-blocking conflicts, the cycles, 2 other ADR findings, TR-round-013, 2 engine items), **1 PARTIAL** (architecture.md), **2 OPEN** (both are pending user or out-of-scope items, and neither is an ADR defect).

### New findings (introduced by, or exposed by, the fixes)

All LOW and non-blocking. No new 🔴 or 🟡 conflict.
- **N1 (LOW, type ownership):** ADR-0002 §2a's `FCommandPayload` names `FReplySinkId`, described as a "Foundation `View`/`Server` id", but no ADR declares it. ADR-0011 uses it only as opaque, in `FCommandOrigin`. `FDebugPayload` also has no declared shape. ADR-0002 should own both in `StealthQueuePayloads.h` before the queue story starts, or ADR-0007 lint (e) cannot be checked for that header.
- **N2 (LOW, architecture.md drift):** architecture.md's API Boundaries sketch (l.419-430) still puts `FSubjectRef` under Foundation `Public/View`. It also shows the old queue shape: `EQueueItemType {Command, …, InformantRespawn, GraceExpiry, RivalSignal}` and `FRespawnPayload`/`FGracePayload`. This contradicts ADR-0002 §2a, which makes `FSubjectRef` server-only. The ADR governs. Fold this into the pending architecture.md refresh.
- **N3 (LOW, mapping detail):** ADR-0002's `FTimerPayload` table gives `StageSignal` `Subject` = court fixture and `Phase` = team. ADR-0020 §5 step 3 schedules `{ViewingTeam, Ordinal}` and names no court. ADR-0020 should state which court fills `Subject`, or the table should say "none", and `Phase` should be identified as the viewing team.
- **Observation (no action):** ADR-0017 implements an interface declared in ADR-0018's Assassination header but no longer lists ADR-0018 under Depends On. This is consistent with the acceptance-pair note in both Ordering Notes.

Verified clean beyond the claimed fixes:
- Payload names are used consistently across ADR-0002, 0011, 0016, 0017, 0018, 0019 and 0020. Each timer owner's amendment matches its row in ADR-0002's mapping table, apart from N3.
- ADR-0011's `EActionType` and `FClientCommandSeq` are Foundation `Public/View` types, as §2a assumes.
- `FSubjectRef` users (ADR-0023 telemetry) are server-only.
- The registry's new contracts and the `feature_declared_queue_payload` forbidden pattern match the ADR text.

### Coverage after the re-check

177 requirements: ✅ 0 · 🟡 175 · ⚠️ 2 (TR-con-005, TR-pen-006, both waiting on user schema decisions) · ❌ 0. `requirements-traceability.md` is updated to match. `tr-registry.yaml` needs no change: no new IDs, no reworded requirements, and the registry carries no coverage status.

### Updated Verdict: CONCERNS

The blocking conflict is resolved and no blocking conflict remains, so the verdict moves from FAIL to CONCERNS. CONCERNS is the ceiling while coverage rests on `Proposed` ADRs; PASS needs:
- every ADR moved to Accepted by the user;
- the two pending-user partials decided;
- the architecture.md refresh, which closes N2 and Phase 6.

Recommended before acceptance: fix N1 and N3 in ADR-0002/0020 (both one line), then refresh architecture.md's Data Flow and API Boundaries.

Re-check receipt (latest stamp wins):
Reviewed-Content-Hash: docs/architecture/adr-0001-crowd-identity-rules.md 3b1fb4fda97e458f4540a51ca1a221135a0a839f
Reviewed-Content-Hash: docs/architecture/adr-0002-resolution-queue-round-end-staging.md 330816539cd8de15e2f0570bc93a5f1fdc936ac3
Reviewed-Content-Hash: docs/architecture/adr-0003-replication-relevancy-concealment-cull.md 7d1144473848f551b8045b8cae06a5a6b89010aa
Reviewed-Content-Hash: docs/architecture/adr-0004-tuning-data-pipeline-validator.md c9298eef954986222752309bc40647773576f41e
Reviewed-Content-Hash: docs/architecture/adr-0005-session-seamless-travel-state-stores.md d406831030647d478162f3c4e3c4aa683df15539
Reviewed-Content-Hash: docs/architecture/adr-0006-seeds-random-streams.md 78d2d19a4cdf0c77695cbe4710dd8f587711b6c7
Reviewed-Content-Hash: docs/architecture/adr-0007-code-modules-include-boundaries.md 908b3d8527786efba3abf1f33e7ff43f149c2041
Reviewed-Content-Hash: docs/architecture/adr-0008-test-automation-leak-parity.md e630375f94984fa9eedef69ad0b918077f95ba2f
Reviewed-Content-Hash: docs/architecture/adr-0009-version-control-binary-assets.md 05fc0c3756bcb9e797b725062f1966848cea6582
Reviewed-Content-Hash: docs/architecture/adr-0010-shared-pawn-cmc-possession.md ead108adcd6b311f4526d5f99ecaf5369705f4f7
Reviewed-Content-Hash: docs/architecture/adr-0011-pawn-command-interface-input.md 293dee6b629778870c4a7e49d84a132207bd3e2b
Reviewed-Content-Hash: docs/architecture/adr-0012-character-pool-appearance-allocation.md 48ecac4c381ac517351c59228634d6efff354056
Reviewed-Content-Hash: docs/architecture/adr-0013-animation-motion-matching-significance.md 186dd380e3eb4ad41ea0b032a025bb46d58407ca
Reviewed-Content-Hash: docs/architecture/adr-0014-layout-service-generation-navmesh.md bae3e27f5664d95e710c67237fe80ad1438c2687
Reviewed-Content-Hash: docs/architecture/adr-0015-camera-los-envelope.md ab307b1a0e39a3e05fd46dbe5ca426f87b29594c
Reviewed-Content-Hash: docs/architecture/adr-0016-crowd-ai-architecture.md 26f0e767e21d5f4cd72f4499f4ce8706a83f92f6
Reviewed-Content-Hash: docs/architecture/adr-0017-concealment-per-viewer-presentation.md bff4318db77ce1f0f38b9c011bbac7de87341494
Reviewed-Content-Hash: docs/architecture/adr-0018-encounter-outcome-timing.md d8d47b4b8e29cbdfb2b1b4316933d1430b1a3f50
Reviewed-Content-Hash: docs/architecture/adr-0019-informant-lifecycle-takeover.md 39d01d0b2fa9873b8bdf42cd4941cd1ea4d1f30a
Reviewed-Content-Hash: docs/architecture/adr-0020-objectives-data-team-view.md b9452ce1c1a8000cecad9937f48aad4cc0d5cf54
Reviewed-Content-Hash: docs/architecture/adr-0021-ui-hud-layering-commonui.md c9e1365fc128e795e057e637983f6b466dcaea8e
Reviewed-Content-Hash: docs/architecture/adr-0022-rendering-performance-budgets.md 93b8539f3bcde74f208278aafd97258cde8cc982
Reviewed-Content-Hash: docs/architecture/adr-0023-playtest-telemetry-hooks.md 4ad165db5ca9eb7fbf8073be752a06e3ec41eac9
Reviewed-Content-Hash: design/gdd/assassination-defence-reversal.md 37ccacdd9bd76ad853977fb33be87e243085b100
Reviewed-Content-Hash: design/gdd/camera-system.md 21f94a1f50d1fb635a16657383a7d5b36c6cfa87
Reviewed-Content-Hash: design/gdd/character-pool-appearance.md 8ec42eeb251eab663076620d6adadb251088d3b8
Reviewed-Content-Hash: design/gdd/concealment-bushes.md c6ceb16f2b389264e5d69a8437bb72868df18504
Reviewed-Content-Hash: design/gdd/crowd-npc-behaviour.md cb5a02b9d6f1111495736074d067248965a04fce
Reviewed-Content-Hash: design/gdd/game-concept.md 8dccb622e17a7f1dc58349dd2baa54d52fc36d32
Reviewed-Content-Hash: design/gdd/gdd-cross-review-2026-10-24.md 3c5b5d1c9faf25998e07b76e01a4bf269841dad0
Reviewed-Content-Hash: design/gdd/hud.md 8eefe3da6268eb2566076f1659a46c4726d54087
Reviewed-Content-Hash: design/gdd/information-visibility-policy.md 2c4160f79d3e3822b80172db6dd1147e0eb856cc
Reviewed-Content-Hash: design/gdd/input-interaction-framework.md 4759241f4c2ac3bf570a9660f449360975cf0374
Reviewed-Content-Hash: design/gdd/level-socket-contract.md a8310a985e5c3684507752cb6a2d7c32cc521fdd
Reviewed-Content-Hash: design/gdd/npc-kill-penalty-informants.md d0998191537a02408eaf9fcf0d7c2b9b1ddeba6a
Reviewed-Content-Hash: design/gdd/objectives-chain.md bb247ec67de54231de83c83dbfc17e57fed33869
Reviewed-Content-Hash: design/gdd/participant-state-roster.md e7223d4f08ab5671ba4a21df5a742e4d78c39592
Reviewed-Content-Hash: design/gdd/procedural-city-generator.md 7b91fc168d2beeaaa2fdcee49ffea9e3e7a25ea5
Reviewed-Content-Hash: design/gdd/round-match-framework.md ea6d50f08cdbe2bcff2da3ffd066a9372927eea6
Reviewed-Content-Hash: design/gdd/session-travel.md 26102a596afc758aff5c85e01efc2502063d65e4
Reviewed-Content-Hash: design/gdd/shared-character-locomotion.md 9d7697dbddb79d0017810813a84c4a1d676b79ef
Reviewed-Content-Hash: design/gdd/systems-index.md f2a36200d3309b93728e348e2d7961a6fc63cd69
Reviewed-Content-Hash: design/gdd/teammate-markers.md 485310be9052a6f893423f7b9500a0c63928be58
Reviewed-Content-Hash: design/gdd/tuning-data-config.md 370d42a9fcc6461e2ebefe65190b7bde9e3b5ad5
Reviewed-Content-Hash: docs/architecture/tr-registry.yaml 7f8e2e41f512c577be66ec1f902be63f08c21080
Reviewed-Content-Hash: docs/architecture/architecture.md adf42d32e07cc9eae4c22079319e9a35a37619ba
Reviewed-Content-Hash: docs/engine-reference/unreal/PLUGINS.md 4a7765252f7cd3774bfb2af2b40715cfafac5eab
Reviewed-Content-Hash: docs/engine-reference/unreal/VERSION.md b83420d1df349e3d48f80a323496b07d7e37914e
Reviewed-Content-Hash: docs/engine-reference/unreal/breaking-changes.md 7aad70a8c4b45e1269618b6ef02247eaa7585c39
Reviewed-Content-Hash: docs/engine-reference/unreal/current-best-practices.md 84434704bf5129944731298240b0854706b3ccba
Reviewed-Content-Hash: docs/engine-reference/unreal/deprecated-apis.md 9369070e611a0ae2de960c4c419bab1fcfc3085f
Reviewed-Content-Hash: docs/engine-reference/unreal/modules/animation.md 22b215d25348939d9f3b1791d4b083252b9966d2
Reviewed-Content-Hash: docs/engine-reference/unreal/modules/audio.md 26d7beb487b226cf9ba970ada91a566b6a33b17d
Reviewed-Content-Hash: docs/engine-reference/unreal/modules/input.md 5771e81964e6252ff5db2761c79be9402d4c18d8
Reviewed-Content-Hash: docs/engine-reference/unreal/modules/navigation.md 82fdfed513824a4832f3d0562fa7eabef25de921
Reviewed-Content-Hash: docs/engine-reference/unreal/modules/networking.md 05e96461186c3349240f8d3e41e21a02886f936d
Reviewed-Content-Hash: docs/engine-reference/unreal/modules/physics.md 0c80abde74bd5c12e78d0aa1f00ce378e5bb6625
Reviewed-Content-Hash: docs/engine-reference/unreal/modules/rendering.md f4c0a9bfc1f4fb4d3244c596e3d61c3d6f7b0b72
Reviewed-Content-Hash: docs/engine-reference/unreal/modules/ui.md 5dc9788a910030b8754bdc987716d5e94e9173da
Reviewed-Content-Hash: project.yaml c7fe5f5b9ec92a5dd046aaa64c24a37c1c6c2737
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
