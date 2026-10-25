# Review Log: Information Visibility Policy

## Review — 2026-10-24 — Verdict: NEEDS REVISION
Review mode: full · Workflow: full
Scope signal: XL (cross-cutting Core system, 8 formulas, 20+ dependents, ADR-001 plus spikes S9-S15; producer should verify before sprint planning)
Specialists: ue-replication-specialist, security-engineer, qa-lead, creative-director (senior verdict)
Blocking items: 7 | Recommended: 4
Summary: The structure is sound (one default-deny query, seven viewer classes, one carrier per audience, a full engine-default override table and leak catalogue), but the newer sibling GDDs pulled it in three unsafe directions: Crowd asked to loosen F5 to k = 2.0, Objectives asked for a `RivalStageStep` Team fact whose timing could confirm a watched tend, and evidence playtests leaned on a competing listen host. The creative-director ruled (A) k stays 1.5 and Crowd raises r_N, (B) the rival signal is denied as specified and allowed only stage-level with a fresh per-event delay and a global release quantum, (C) listen-host trusted-host rule now, headless server a pending user decision, (D) `SiteKnownToTeam` is presentation only and ingress accepts any court. Revisions were applied in place across two sessions (the first was interrupted mid-R7); an independent re-review is required.
Prior verdict resolved: First review
Findings:
- [BLOCKING] F5 / Tuning Knobs: k range 1.0-2.0 invited Crowd's k = 2.0 request; the cull exposes every hide city-wide -> fixed: k range 1.0-1.5, 1.5 is the ceiling, rationale paragraph under F5, AC-IVP-43 harness check (ruling A).
- [BLOCKING] R7.6 / Edge Cases / Tuning Knobs / AC-IVP-30: automatic client-side-hide fallback if Crowd misses F5 -> fixed: no automatic fallback, escalation to creative-director and user, client-side hide internal dev builds only (L29 CRITICAL), AC-IVP-30 and AC-IVP-44 gate external builds.
- [BLOCKING] R3 / L34 / OQ4: Objectives OC-12 `RivalStageStep` (Team fact) leaks sub-stage timing and confirms feints -> fixed: denied as specified; R3 row and OQ4 state the allowed shape (preliminaries only, never the final step, fresh per-event delay, one global release quantum, no member identity); AC-IVP-40; change request to Objectives in OQ4 and Cross-References item 8.
- [BLOCKING] R7.4a / R3 `SiteKnownToTeam`: gating IIF court ingress on site status is an oracle -> verified applied: ingress accepts any court fixture, `SiteKnownToTeam` drives presentation and progress credit only; AC-IVP-41 added (ruling D).
- [BLOCKING] R10 / OQ8: evidence playtests with a competing listen host contaminate F5/F8 data -> fixed: non-competing trusted host or host-team exclusion now, AC-IVP-36; headless server logged as a pending user decision (scope_changes, ruling C).
- [BLOCKING] Acceptance Criteria: rules cited AC-IVP-37, 38, 39, 41, 42, 45 which were never defined -> fixed: AC-IVP-36..45 added (possession canary, spectator ban, strike-id oracle parity, court ingress parity, allowlist content, net-config runtime differential, rival-signal shape, k ceiling, external-build critical gate, evidence host).
- [BLOCKING] Dependencies: Crowd, Concealment and Objectives listed as "Not written" while their GDDs exist and conflict -> fixed: statuses updated, conflicts named (Crowd k = 2.0 declined per C7; Objectives OC-12 denied), Concealment change request 12 (narrow its k range to 1.0-1.5).
- [RECOMMENDED] Edge Cases / AC-IVP-12 / Visual table: provisional `BushTransition` name -> fixed: `BushEnter` / `BushExit` per Concealment CON-R5.
- [RECOMMENDED] AC-IVP-33: catalogue range L1-L28 stale after L29-L34 were added -> fixed: L1-L34, L29 listed as dev-build Acc.
- [RECOMMENDED] OQ3: restated against k = 1.5 and the Concealment version of bush use (occupancy-blind, no dwell cap, r_N about 0.19/min); change request to Crowd T5, CR 1 and OQ1.
- [RECOMMENDED] AC-IVP-34: no longer deferred; fails until Crowd T5 cites k = 1.5.
Specialist disagreements: Crowd (k = 2.0, P_hide 0.20, game-designer wanting 0.30-0.40) vs Policy/Concealment (k = 1.5); creative-director ruled for 1.5 and orchestrator conflict C7 chose the Concealment version. Objectives (OC-12 approved at its CD-GDD-ALIGN with a team-labelled count) vs this review's ruling B (denied as specified); Objectives must revise OC-12 before the IVP adds a row.
Change requests not applied (other owners' GDDs): crowd-npc-behaviour.md T5, change request 1, OQ1 to k = 1.5; objectives-chain.md OC-12, F4, AC-OC-09/10 and its IVP change request 3; concealment-bushes.md F5 table k range 1.0-1.5.
Note: the target document was revised after this review, so it is deliberately not stamped below (a re-review must run in full).
Reviewed-Content-Hash: design/registry/entities.yaml dfeacb02917f751cb1dfd88cf490c7ec3d8d2d0f
Reviewed-Content-Hash: .claude/skills/design-review/SKILL.md a63d8fba9b7142eeb43791f061e2e1533c429b43
Reviewed-Content-Hash: design/gdd/crowd-npc-behaviour.md 74cc103f40a86310e995f9d20cb3cadadc047bbc
Reviewed-Content-Hash: design/gdd/objectives-chain.md 3611facfb79f5c98605dfcf5314126da5602c6c3
Reviewed-Content-Hash: design/gdd/concealment-bushes.md ed2d2ba1c4cfd9081cd01d62ce6b2a592668b0c5
Reviewed-Content-Hash: design/gdd/game-concept.md 8dccb622e17a7f1dc58349dd2baa54d52fc36d32
