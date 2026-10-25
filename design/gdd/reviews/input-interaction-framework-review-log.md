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
