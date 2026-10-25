# Review Log: Input & Interaction Framework

## Review — 2026-10-24 — Verdict: NEEDS REVISION
Review mode: full · Workflow: full
Scope signal: L (multi-system integration, 6 formulas, several ADRs needed: IIF command component, hold-timer queue items, Look parity; producer should verify before sprint planning)
Specialists: systems-designer, qa-lead, ue-replication-specialist, creative-director (senior). The panel was capped at 3 specialists by an orchestrator instruction after an API usage-limit interruption.
Blocking items: 26 (main review 3, systems-designer 4, qa-lead 12, ue-replication-specialist 7; overlapping items merged when fixes were applied) | Recommended: 26
Summary: The creative-director judged the architecture sound and pillar-aligned: one command model for humans and AI, intent decided on the server, no aim assist. Every blocker could be fixed within the existing structure. The top issues were:
1. Hidden-site oracle via client `TargetId` / `OutOfReach`.
2. Hold completion, cap end and debounce expiry ran outside Round's queue, so strike outcomes were not deterministic (P3).
3. The listen-server host bypassed ingress.
4. The transport bucket could silently drop an Attack.
5. `PawnRef` was accepted unchecked.
6. The reply model contradicted itself.
7. Several acceptance criteria could not fail.
8. The knob table did not conform to CR-15.

Rulings: replies come in two tiers (silent at ingress, exactly one reply to a `ReplySink` at resolution). The transport bucket must be at least the per-pawn sums, plus an End-flood cap. Look parity is a requirement and AC in this GDD; Locomotion owns the mechanism. The specialist disagreements are recorded in the GDD, where they were resolved per these rulings.
Prior verdict resolved: First review
Revisions applied after review: 24 (by the reviewing agent, in autonomous mode). Re-review pending.
Findings:
- [BLOCKING] Tuning Knobs: table lacked the CR-15 columns (Type, Owner, Latch, Unit); keys without CR-15 suffixes (BucketInteract, TransportBucket, AbuseLogThreshold, LatencyWarnRttMs). FIXED: table rebuilt, keys renamed/split.
- [BLOCKING] Tuning Knobs: one quantity, two names (`InteractRangeCm` vs TDC's `ObjectiveInteractRangeCm`). FIXED here; change request to TDC.
- [BLOCKING] UI Requirements 3: prompts drawn from static level data, but site roles are secret (Level Socket R9). FIXED: geometry plus Objectives' Team-audience live-site state.
- [BLOCKING] Core Rules R5/R6/R8: hidden-site oracle via client `TargetId`. FIXED: ingress drops unknown or Policy-invisible ids through the same path.
- [BLOCKING] Core Rules R8 / Formulas F1-F2: hold completion, cap and debounce expiry ran outside Round's queue; "completion wins" conflicted with total order. FIXED: `HoldTimer(Complete|CapEnd|DebounceExpiry)` queue items, key order decides.
- [BLOCKING] Core Rules R5: a same-frame Begin+End made a tap into a full-cap brace. FIXED: stage 2 runs at the queue slot; mailbox FIFO.
- [BLOCKING] Core Rules R5: key stamping and stage-2 timing inconsistent with Round R12. FIXED: `Submit(Command, SourceMailbox)`; stage 2 in the resolver.
- [BLOCKING] Core Rules R2: the listen-server host bypassed ingress. FIXED: `IngestCommand(Source=HostLocal)`; replies queued.
- [BLOCKING] Formulas F3 / Tuning: cooldown mashing drained the transport bucket, so an Attack could be dropped silently. FIXED: B30 R20 (≥ 2× per-pawn sums), unmatched-End cap, client suppresses Prepare during a known cooldown.
- [BLOCKING] Formulas F6: the cancel-range validation failed inside its own safe range. FIXED: `InteractCancelHysteresisCm`.
- [BLOCKING] Core Rules R10: `PawnRef` acceptance undefined server-side. FIXED: `CanViewerKnow` gate plus `PawnRefAgreementCm`.
- [BLOCKING] Core Rules R6 / Edge Cases / ACs: reply model contradictory ("if it has one", "dropped with no reply"); `FHoldStatus` lacked an outcome. FIXED: two tiers, `ReplySink`, code mapping, `Outcome`.
- [BLOCKING] Core Rules R3: Look/aim parity asserted, not specified. FIXED: R3a contract plus AC 54; mechanism change request to Locomotion.
- [BLOCKING] Acceptance Criteria: AC 44 used a KS test as its pass rule; AC 12 could not fail; AC 36 used the wrong latency; AC 34 was vague; AC 16/20 had no defined results; AC 45 compared property sets, not values; AC 46 pixel-identical full screenshots; unnumbered section-E bullets; uncovered rules (R13, R12 forced update, facing, F4 resync, L_look, transport/abuse, no-ally/no-aim-assist, lockout slot, wrong phase, UI 2/3/5/7). FIXED: rewritten, renumbered, ACs 54-75 added.
- [RECOMMENDED] Formulas F5: L_confirm and L_observer ranges wrong. FIXED.
- [RECOMMENDED] Formulas F2/F5: U defined two ways. FIXED (U ≤ one server frame under forced update; verify under Iris).
- [RECOMMENDED] Formulas F2: tick quantisation; a hard-0 debounce could hide the pose. FIXED (hard 0.05-0.2).
- [RECOMMENDED] Formulas F3: load figure (4 buckets, not 6 types) and self-contradictory 8 Hz example. FIXED.
- [RECOMMENDED] Formulas F4: ring stalled at confirmation; T_ack shorter than one resend. FIXED (2·RTT).
- [RECOMMENDED] Game Feel: overshoot claim wrong. FIXED.
- [RECOMMENDED] Core Rules R8: release within the debounce of D still completes. STATED.
- [RECOMMENDED] Core Rules R6: facing failure had no code. FIXED (`OutOfReach`).
- [RECOMMENDED] Formulas: ε needs double-precision time. FIXED.
- [RECOMMENDED] Tuning: `PrepareLatencyForgivenessSec` used in no formula. RETIRED.
- [RECOMMENDED] Core Rules R12: dormancy / `bOnlyRelevantToOwner` bans. ADDED.
- [RECOMMENDED] Core Rules R8 / F6: commanded displacement ill-defined (AI RequestDirectMove; E pressed while W held). FIXED: net displacement while Move intent ≠ 0, `InteractMoveGraceSec` 0.25 s, AI path-following through `Move`.
- [RECOMMENDED] Core Rules R5.1: End flood unbounded. FIXED (`UnmatchedEndPerSec`).
- [RECOMMENDED] Core Rules R5d: `AllowNoEffect` must depend only on (site, class). ADDED.
- [RECOMMENDED] Core Rules R5.1: `PossessionEpoch` in `ClientCommandSeq`. ADDED.
- [RECOMMENDED] Acceptance Criteria: boundary values (AC 37), tick tolerances (AC 32/33), capture method (AC 31), RPC hook (AC 26), Locomotion baseline (AC 24), perf hardware/scopes (AC 51-53). FIXED.
- [OPEN] Cross-GDD: Round (HoldTimer item type, Submit signature, R11/R12 wording, timer sort key, ObjectiveCompletion as follow-on), Objectives (Team-audience site set, follow-on, AllowNoEffect rule), TDC (key rename, invariants), Locomotion (R3a mechanism, relevancy bans), Visibility Policy (`CanViewerKnow` for PawnRef, site visibility). See GDD Cross-References change requests 4, 7, 10-13 and Open Question 11.
Reviewed-Content-Hash: design/gdd/input-interaction-framework.md 6c07f1b2a4892acb3829193b7e814f637d04cc61
Reviewed-Content-Hash: design/registry/entities.yaml dfeacb02917f751cb1dfd88cf490c7ec3d8d2d0f
Reviewed-Content-Hash: .claude/skills/design-review/SKILL.md a63d8fba9b7142eeb43791f061e2e1533c429b43
Reviewed-Content-Hash: design/gdd/round-match-framework.md 0a20ed3e6deebf0dcc36a11ec171861efbd91583
Reviewed-Content-Hash: design/gdd/participant-state-roster.md 7090ac55a320a45ab62d598f7ec0661d468cc8e3
Reviewed-Content-Hash: design/gdd/tuning-data-config.md 4c00659ec2fded9575197cc975bcde9241b96b93
Reviewed-Content-Hash: design/gdd/level-socket-contract.md 262eb5056ce08175a3eebf6cac899bb141684101
Reviewed-Content-Hash: design/gdd/systems-index.md 2adb098db430a1b7d1c77e1238171c3948bf08eb
Reviewed-Content-Hash: design/gdd/game-concept.md 8dccb622e17a7f1dc58349dd2baa54d52fc36d32
(The GDD hash is of the reviewed, pre-revision text. The revised document will read CHANGED, so the re-review runs in full.)

## Review — 2026-10-25 — Verdict: NEEDS REVISION
Review mode: lean · Workflow: full
Scope signal: L (unchanged: multi-system integration, 6 formulas, several ADRs; producer should verify before sprint planning)
Specialists: none (lean mode, single-session analysis)
Blocking items: 3 (all fixed in text; verdict not upgraded, a re-review decides) | Recommended: 5
Summary: All 26 first-round blockers are resolved in the text: the CR-15 knob table, the two-tier reply model, queue-resident hold timers, host ingress, the transport bucket sizing, the `PawnRef` gate, the Look-parity contract and the rewritten acceptance criteria. Arithmetic checked and correct: F3 (6 admitted of 16; Σ B 11, Σ R 6/s), F4 (T_ack 0.483 s), F5 (L_confirm, L_observer examples) and F6 (190/240 cm). The second pass found three internal contradictions left behind by the 2026-10-24 reconciliation and ADR sync (an edge case and AC 6 still dropped hidden-court ids at ingress, and F1 still described a per-drain poll stamped after intake). All three were fixed, but they touch ingress, timer order and a test, so the document needs one more read.
Prior verdict resolved: Yes (first-round blockers all resolved; 3 new blockers found)
Findings:
- [BLOCKING] Edge Cases (hidden-site probe): said a hidden-court `TargetId` is dropped silently at ingress, contradicting R5.1, AC 71 and IVP R7.4a ruling D (every court accepted, resolves as `AllowNoEffect`). FIXED: only non-target ids are dropped.
- [BLOCKING] Acceptance Criteria 6: asserted the same stale drop of a hidden-site Interact, so it would fail against AC 71. FIXED.
- [BLOCKING] Formulas F1 / AC 32: said the IIF polls active Interacts each drain and stamps `HoldTimer(Complete)` after intake. R8 and ADR-0002 §4 schedule it at Begin with `ScheduleAt`, and Round R12 stamps an already-due item before intake. FIXED: F1, its Output Range sentence about same-frame order, and AC 32 now match R8 and Round R12.
- [RECOMMENDED] Core Rules R6: ingress tier still listed "Policy-invisible" ids; also did not say that Round's own pre-resolver drops (`WrongPhase`, `StaleConnection`, `MailboxOverflow`) are silent. FIXED.
- [RECOMMENDED] Dependencies: Information Visibility Policy was marked Soft although R10's `PawnRef` gate needs `CanViewerKnow` (x-ray oracle otherwise). FIXED: Hard for R10, soft for vocabulary.
- [RECOMMENDED] Dependencies: Settings & Accessibility row listed "debounce constants", contradicting OQ1 (no release tolerance). FIXED. R8 "not a setting in MVP" reworded to "server-only tuning key, never a player-facing setting".
- [RECOMMENDED] Cross-References: change requests 4, 7 and 12 still described an ingress site filter (superseded by ruling D) and a three-kind `HoldTimer`. FIXED: marked superseded, `ConditionCancel` added.
- [RECOMMENDED] Acceptance Criteria: no test for a reversal ending a brace (`Reversed`, `ServerEndTime = ContactServerTime`), the planted brace ignoring Move, or the `PossessionEpoch` gate. ADDED as AC 77-79.
- [OPEN] ε in the due-time comparison is applied in F1 as `t_now ≥ DueTime − ε`, but Round R12 states the test as `DueTime <= DrainStartTime`. Needs a Round decision (see follow-ups).
- [OPEN] Verify in the first 2-client playtest or spike V1: exactly-one-`Completed` on input flush, ForceNetUpdate under Iris (note: pending decision A4 pins Iris off, so the Iris-specific "verify" notes may be moot), `RemoteViewPitch` parity.

Cross-doc follow-ups (main session):
- `design/registry/entities.yaml`: it lists only 19 IIF-sourced entries. IIF knobs missing from it: `InteractFacingHalfAngleDeg`, `InteractMoveGraceSec`, `UnmatchedEndPerSec`, `AbuseLogDropCount`, `AbuseLogWindowSec`, `BucketInteract*`, `BucketPrepare*`, `BucketHidePerSec`. Present: `AckSlackSec`, `BucketAttack*`, `BucketHideSize`, `CameraToggleLockoutSec`, `CommandRangeSlackCm`, `InteractCancelHysteresisCm`, `InteractMoveCancelDistCm`, `InteractRangeCm`, `PawnRefAgreementCm`, `ReleaseDebounceSec`, `TransportBucket*` and the plate cooldowns. `LatencyWarnRttSec` is correctly `deprecated` (replaced by `HudLatencyWarnRttSec`).
- `design/gdd/round-match-framework.md` R12: decide whether the due test carries ε (1e-4 s, double precision) or is exact on double absolute `DueTime`; the IIF F1 text defers to Round's comparison. Also confirm R12's `MailboxMaxItemsPerDrain` 32 drops stay silent with no IIF reply (IIF R6 now says so).
- `design/gdd/systems-index.md`: IIF row 3 "Depends On" should add Information Visibility Policy (hard, for `CanViewerKnow`) and Shared Character & Locomotion is already listed in the other direction (change request 2).
- `design/gdd/tuning-data-config.md`: still owes the two cross-field invariants on the transport bucket and the `ObjectiveInteractRange` → `InteractRangeCm` mapping (IIF change request 10); verify they landed.
- `design/gdd/objectives-chain.md`: the stale "Team-audience live-site set for ingress filter" wording from IIF change request 7 is superseded; Objectives should not implement an ingress filter.
Reviewed-Content-Hash: design/gdd/input-interaction-framework.md ed42829b7c7aea9557521fb4c45d124224835244
Reviewed-Content-Hash: design/registry/entities.yaml 28f7a489149e3fba702986d089b7799719796b39
Reviewed-Content-Hash: .claude/skills/design-review/SKILL.md a63d8fba9b7142eeb43791f061e2e1533c429b43
Reviewed-Content-Hash: design/gdd/round-match-framework.md f019aa4e04092b43aee6b74f2462438418205597
Reviewed-Content-Hash: design/gdd/participant-state-roster.md 11106ca55e071c8e8f1ab67926f678d5c3ecf306
Reviewed-Content-Hash: design/gdd/tuning-data-config.md 89d049074c81e71b16f1269736cbe2016d977f9c
Reviewed-Content-Hash: design/gdd/level-socket-contract.md 170946af3b310140bd789eda4906874349fc59f9
Reviewed-Content-Hash: design/gdd/systems-index.md b195f89efa215d57e7630933f75bbbc02bdd9ba8
Reviewed-Content-Hash: design/gdd/game-concept.md 6dbca7dc538ed44d89a5f009dbc8eda18782a2c8
Reviewed-Content-Hash: design/gdd/objectives-chain.md 2458e588b87bb7d1954477782e9a5f2b75118ff6
Reviewed-Content-Hash: design/gdd/information-visibility-policy.md 9ce1d9677b38048e71176b7dd0b44959ce5c99f5
Reviewed-Content-Hash: design/gdd/shared-character-locomotion.md dc19c6207e69961a896ea3c6268187b563001dca
(The GDD hash is of the revised text as left by this review, so the next check reads UNCHANGED until the document is edited again; the verdict stands until a re-review.)
