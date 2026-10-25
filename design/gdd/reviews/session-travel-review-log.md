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

## Review — 2026-10-25 — Verdict: NEEDS REVISION
Review mode: lean · Workflow: full
Scope signal: L (unchanged: multi-system integration, 8 formulas, ADRs for engine timeouts, replication model, online identity; producer should verify before sprint planning)
Specialists: none (lean, single-session second round; first-round specialist findings were re-checked against the revised text)
Blocking items: 6 | Recommended: 6
Summary: Second round after the 2026-10-24 revisions. All 18 first-round blocking items are resolved in the text (TuningHash/ClientHash echo, server-requested acknowledgement with explicit resend and epoch keys, admission order vs Round R5, Roster API names, F7 absolute deadline, F6 pause, AC rewrites; the F5 golden vectors 2107646860 / 523860287 / 313478722 were recomputed and match; registry values for all 13 Session keys, F1, F5 and F7 match). The new faults are state-machine holes the added rules left behind (unbounded or target-less `Rejoining` for supersede and `Expired` origins, supersede missing for a `Traveling` participant, a Lobby supersede with no named state), one numeric inconsistency (engine timeout 105 s below the F7 worst case of 111 s), two contract drifts (rule 11.1 order vs Round R5; `RejoinToken` refusal without a gate position) and a handshake that Round's `RejoinOnly` push could strand. All six were revised in place in this session; an independent pass must confirm them.
Prior verdict resolved: Yes (18 of 18 first-round blocking items; the "unbounded Rejoining" item was only partly resolved and is re-opened as blocking item 1 below)
Findings:
- [BLOCKING] Core Rules 21 / 11.7 / States: `Rejoining` was bounded only for a `Disconnected` origin; `Expired`-started and supersede-started rejoins had no deadline and no failure target (the supersede-started one has no window to fall back on). REVISED: origin / `RejoinDeadline` / failure-target table in rule 21, state rows, Edge Cases, new AC 66
- [BLOCKING] Core Rule 15 / States: supersede was not defined for a `Traveling` participant, the likeliest crash moment (a client that crashes while loading stays `Traveling` until the F7 deadline, and rule 8 said "supersede" for a state the transition table lacked). REVISED: `Traveling` added to the supersede definition and row; keeps the travel's F7 `Deadline`
- [BLOCKING] Core Rule 15 / States: Lobby supersede used the undefined "Traveling-equivalent" state, so acknowledgement acceptance (c) had no legal state. REVISED: `Rejoining` in Lobby too (failure goes `Dormant`), counted as `Connecting` by rule 11.2
- [BLOCKING] F7 / Core Rule 19 / AC 27: backstop "105 s at defaults" was computed from the T_srvload = 10 example, but with T_srvload at its stated maximum (20 s) the formula gives 111 s, so a config passing AC 27 could still fire before the Deadline. REVISED: 111 s minimum, 120 s configured, F7 constraint names the T_srvload maximum
- [BLOCKING] Core Rules 11.1 / 20: round-start order contradicted Round R5 (R5 pushes `RejoinOnly`, locks, `ResetAll`, travel, and sets `Preparing` only on acceptance; this GDD said `Preparing` came first). REVISED to R5 steps 5b-9 with the rollback; rule 11.3 now says Session applies its `Closed` override rather than "sets" the mode (Round is the only writer)
- [BLOCKING] Core Rules 6 / 7 / AC 5, 58: `RejoinToken` refusal had no position in the rule-7 gate order, so a wrong token could surface as `Busy` or reach supersede, and `GateOrder` could not test it. REVISED: gate 3 now covers kicked key and missing/wrong token; AC 5 and 58 extended. (Also revised: the `Connecting` to `Connected` guard no longer says "mode Open", which a `RejoinOnly` push would have stranded.)
- [RECOMMENDED] AC 32: expected first/last-player share was the 40-NPC value 20%; at 50 NPCs plus 10 players it is 10/60 = 16.7%. REVISED (threshold 25% kept)
- [RECOMMENDED] AC 46: text still said it blocked on Round and Crowd GDDs defining the mechanism; both exist (Round R17.1). REVISED wording; the playtest in (b) still gates implementation sign-off
- [RECOMMENDED] F5: formulas list `ServerSecret` in the hashed input while the encoding note keys SipHash with it as the 128-bit key and puts only the other fields in the message; state one form in the ADR/implementation story (not changed here)
- [RECOMMENDED] F6 / T_host_stall_max: allows host stalls outside Active only; the Generator now promises game-thread frames <= 33 ms in Preparing too, so the F6 wording could cite that (not changed)
- [RECOMMENDED] F4: LHS counts restart time from the crash while the window starts at Hard, so the check is conservative by SessionHardTimeoutSec (12 s) and `ReservedFromLastPacket` is defined but unused; fine as a safety margin, say so (not changed)
- [RECOMMENDED] Rule 11.4: only `Seed` is validated on arrival; `?Round=` and `?Preset=` are server-issued so a bad value is an engine fault, but a cheap `RoundIndex` equality assert against the server's value would close it (not changed)
Specialist disagreements: none (no specialists in lean mode).
Cross-doc follow-ups (main session):
- ADR-0005 line 193 (engine timeouts): connection timeout backstop is now >= 111 s (120 s configured), not 105 s; any harness DefaultEngine.ini value must follow.
- Round & Match: Interactions rows list the Session events but omit `OnLobbyTravelComplete` and `OnRoundTravelStarted`; Round must also tolerate `OnParticipantDisconnected` raised for a failed supersede-rejoin (new, rule 21 origin table) and a Lobby `Rejoining` state (projects `Disconnected`, `bReconnecting` stays false outside Active).
- Roster R11 projection table: add that a supersede-started `Rejoining` in Lobby projects `Disconnected` (no leak) and that a failed supersede-rejoin writes a fresh `DisconnectedAt` / `ConnectionGeneration` bump.
- Registry (design/registry/entities.yaml): `connection_state.values` omits Connecting, Dormant, Rejected, SessionEnded; keys `LivenessCheckIntervalSec`, `LivenessMaxCountedFrameSec`, `PendingLoginTimeoutSec`, `DisplayNameCharLimit` are not registered (TDC registers 13 Session keys; the registry mirrors only 9); add an `EngineConnectionTimeoutSec` note (>= 111 s) to `travel_deadline`.
- Systems index row 5: set to Needs Revision (this review does not approve); dependency list needs no change.
- UX: the three status overlays and rejection strings still need `/ux-design` before epics (already flagged in UI Requirements).
Revisions: 8 groups applied by the reviewer to design/gdd/session-travel.md in this session (rules 6/7, 11.1-11.3, 11.7, 15, 19, 20, 21 + origin table, States rows, F7, Edge Cases x2, AC 5, 27, 32, 46, 58, new AC 66, Status lines). Verdict not upgraded by the revisions; an independent pass must confirm them.
Reviewed-Content-Hash: design/gdd/session-travel.md 1e185cf338c1b17880a81bc7057dcb11fa8a0ee4
Reviewed-Content-Hash: design/registry/entities.yaml 28f7a489149e3fba702986d089b7799719796b39
Reviewed-Content-Hash: .claude/skills/design-review/SKILL.md a63d8fba9b7142eeb43791f061e2e1533c429b43
Reviewed-Content-Hash: design/gdd/tuning-data-config.md 63db04c802c6db06eb0ba3dac93f65fb9fa98068
Reviewed-Content-Hash: design/gdd/participant-state-roster.md 11106ca55e071c8e8f1ab67926f678d5c3ecf306
Reviewed-Content-Hash: design/gdd/round-match-framework.md ab477269caa8bacab048c8719d3029a90a7ce89e
Reviewed-Content-Hash: design/gdd/level-socket-contract.md 170946af3b310140bd789eda4906874349fc59f9
Reviewed-Content-Hash: design/gdd/character-pool-appearance.md 252003e5936635dd0a55fb062f10b42713cbf39d
Reviewed-Content-Hash: design/gdd/game-concept.md 6dbca7dc538ed44d89a5f009dbc8eda18782a2c8
Reviewed-Content-Hash: design/gdd/procedural-city-generator.md 959a9840a217d9c97c54fc5d17c8791257ccf6a4
