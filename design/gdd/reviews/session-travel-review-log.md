# Review Log: Session & Travel (design/gdd/session-travel.md)

## Review — 2026-10-24 — Verdict: NEEDS REVISION
Review mode: full · Workflow: full
Scope signal: L (multi-system integration with Roster, Round, TDC, Level Socket Contract and Character Pool; 8 formulas; ADRs needed for engine timeouts, replication model, online identity; producer should verify before sprint planning)
Specialists: network-programmer (also covered the rule-25 replication leak checklist), systems-designer, qa-lead; senior: creative-director. game-designer, ue-replication-specialist and security-engineer were launched but stopped by an API usage limit; the orchestrator then capped the review at 3 specialists. Security review is recorded as Open Question 13.
Blocking items: 18 | Recommended: 22
Summary: The Player Fantasy, pillar mapping and core architecture hold (sealed round, single acknowledgement, silent drop and rejoin); the F5 golden vectors and every worked example were re-verified. The faults were contract drift against the now-written Roster, Round and revised TDC GDDs (TuningHash echo, admission-mode order that refused every round start, stale Roster API names and the Expired-to-Retired mapping) and holes in the connection state machine (unnamed and unresendable acknowledgement, supersede without re-acknowledgement, no lobby-arrival trigger, unbounded Rejoining, deadline that a slow layout build could blow for every client). The creative-director ruled NEEDS REVISION (specification fixes, not a redesign) and adjudicated: PoolHash rides the single acknowledgement; the rejoin secret follows Roster (before the first external playtest).
Prior verdict resolved: First review
Findings:
- [BLOCKING] Core Rules 11.5-11.6: full TuningHash in FRoundReadiness and echo; must be client-recomputed ClientHash, explicit resend, ParamsMismatch, PresetId from TDC store, absolute deadline, ParticipantId-keyed bookkeeping [main review, network-programmer, qa-lead]
- [BLOCKING] Core Rules 11.2: requires admission Open but Round R5 pushes RejoinOnly first, so every start is refused [main review]
- [BLOCKING] Interactions / States: Roster canonical API, Expired is not Retired, NotifyConnectionChanged never called, supersede mapping drop [main review]
- [BLOCKING] Core Rules 11.6: acknowledgement transport unnamed; replicated-struct resend impossible [network-programmer]
- [BLOCKING] Core Rules 11.6: stale, duplicate and old-connection acknowledgements; epoch-keyed client marker [network-programmer]
- [BLOCKING] F7 / rule 21: no absolute bound for Rejoining [network-programmer]
- [BLOCKING] Rule 15: supersede keeps Connected state for a client with no snapshot [network-programmer]
- [BLOCKING] Rule 12 / 20: lobby travel has no Traveling-to-Connected trigger; two writers of Open; lobby drops opened a window [network-programmer, systems-designer]
- [BLOCKING] F1 / rule 23: Full both retryable and non-retryable [systems-designer]
- [BLOCKING] F7: slow server load plus layout build can time out every client; +RTT term wrong [systems-designer]
- [BLOCKING] F6: client could declare the session ended during its own or the host's map load [systems-designer]
- [BLOCKING] Edge Cases: "window expires during Preparing" contradicted rule 8 [systems-designer, qa-lead]
- [BLOCKING] Rule 19 / AC 27: engine timeouts unnamed and inconsistent (InitialConnectTimeout, backstop formula) [network-programmer, systems-designer, qa-lead]
- [BLOCKING] Rule 6 / OQ 9: rejoin secret deferred to Alpha, Roster requires it before the first external playtest [main review, creative-director]
- [BLOCKING] Acceptance Criteria 13, 14, 42: must use ClientHash [qa-lead]
- [BLOCKING] Acceptance Criteria 32, 40: nondeterministic or wrong for a fake clock [qa-lead]
- [BLOCKING] Acceptance Criteria 46, 15, 48, 10, 4: playtest disguised as PIE, unkillable PIE process, uncontrolled load time, missing reason codes, wrong cell count [qa-lead]
- [BLOCKING] Acceptance Criteria: 13 rules without a criterion (lobby travel, sub-seed server-only, admission push, no login pawn, pending login, responsive, identity, kick, names, lobby notices, UI strings, game feel, fallback) [qa-lead]
- [RECOMMENDED] Tuning Knobs: CR-15 columns, key-name convention, Session latch class, Hard >= Soft + 2 validation [main review, systems-designer]
- [RECOMMENDED] Dependencies / Cross-References: Roster, Round, TDC, LSC, CPA now written [main review]
- [RECOMMENDED] Core Rules 11.9: no API for the Level Socket Contract graybox fallback travel [main review]
- [RECOMMENDED] Core Rules 6.3/7: kicked-key gate after Busy gate [network-programmer, systems-designer]
- [RECOMMENDED] Rule 25 b/d: extra PlayerState leak fields, AI hand-over owner change, owner-only RPCs, rejoin PlayerState path [network-programmer]
- [RECOMMENDED] Rule 18: hitch clamp as Session accumulator; sustained slow frames [network-programmer, systems-designer]
- [RECOMMENDED] F5: uint32 and logical shifts, H64 domain tags, public API refuses server domains, RoundIndex cap, near-uniform [systems-designer]
- [RECOMMENDED] F2 L_min; F4 false 60 s claim (T_busy); F1 Occupied double count; Connecting blocking start; ShutdownFlushSec; F8 range; Rejoining-from-Expired double expiry; PendingLoginTimeoutSec scope [systems-designer]
- [RECOMMENDED] Acceptance Criteria 3, 11, 12, 37, 38, 39, 47 and full test names [qa-lead]
- [RECOMMENDED] Core Rules 11.6: PoolHash in the single acknowledgement (CD adjudication) [main review, creative-director]
Specialist disagreements: PoolHash separate acknowledgement (Character Pool CP-18) vs single acknowledgement -> creative-director: single acknowledgement. Rejoin secret Alpha (this GDD) vs before external playtest (Roster) -> Roster. No specialist disagreed with another on blocking items.
Revisions: 40 applied by the reviewer in the same session (autonomous pipeline); independent re-review required. Cross-GDD change requests for Round, Roster, TDC, Character Pool, Level Socket Contract and the systems index are listed in the GDD's Cross-References.
Reviewed-Content-Hash: design/gdd/session-travel.md 9e742c64a4a79cf931dd42fba35efb5dcb6a0d46
Reviewed-Content-Hash: design/registry/entities.yaml dfeacb02917f751cb1dfd88cf490c7ec3d8d2d0f
Reviewed-Content-Hash: .claude/skills/design-review/SKILL.md a63d8fba9b7142eeb43791f061e2e1533c429b43
Reviewed-Content-Hash: design/gdd/tuning-data-config.md 4c00659ec2fded9575197cc975bcde9241b96b93
Reviewed-Content-Hash: design/gdd/participant-state-roster.md 7090ac55a320a45ab62d598f7ec0661d468cc8e3
Reviewed-Content-Hash: design/gdd/round-match-framework.md 0a20ed3e6deebf0dcc36a11ec171861efbd91583
Reviewed-Content-Hash: design/gdd/level-socket-contract.md 262eb5056ce08175a3eebf6cac899bb141684101
Reviewed-Content-Hash: design/gdd/character-pool-appearance.md 68259041af50a0d709b82752d79be73e07113961
Reviewed-Content-Hash: design/gdd/game-concept.md 8dccb622e17a7f1dc58349dd2baa54d52fc36d32
