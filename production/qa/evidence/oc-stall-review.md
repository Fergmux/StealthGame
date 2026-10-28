# Review Evidence: AC-OC-18 NeverATimerNeverAReveal

- **Story**: `production/epics/objectives-chain/story-018-stall-pressure-and-cross-system-checks.md`
- **AC**: AC2 = AC-OC-18 [Review] `NeverATimerNeverAReveal` (AC3 = AC-OC-24, see the last section)
- **Date**: 2026-10-28
- **GDDs reviewed**:
  1. `design/gdd/objectives-chain.md`
  2. `design/gdd/round-match-framework.md`
  3. `design/gdd/concealment-bushes.md`
  4. `design/gdd/crowd-npc-behaviour.md`
- **Search terms** (word-start, case-insensitive): clock, timer, dwell, unlock after, reveal, ping, hint
- **Hit list**: `production/qa/evidence/oc-stall-review-hits.txt` (185 hits: Objectives 28, Round 83, Concealment 46, Crowd 28; "unlock after" matched no line)
- **Method**: every hit line was read and judged against the question below. Where a line was ambiguous on its own, nearby GDD lines were read. Verdicts are one sentence each.

## Summary

**Question**: does any rule, in any of the four GDDs, add a round clock, countdown, dwell timer, time-based unlock, or positional/identity reveal **in response to waiting**? The governing rules are SP-3 (waiting earns nothing), SP-6 (both teams stall: nothing intervenes in game) and SP-7 (the response to a stall defect is structural levers only: never a timer, a reveal, a ping or a marker).

**Overall finding: PASS. 0 FAIL hits out of 185.**

Hits fall into these groups, none driven by waiting:

- **Rules that forbid** timers, clocks, dwell timers and reveals (Objectives SP-3/SP-4/SP-7, Round R16/R20/R23, Concealment CON-R7, Crowd R-bush rule, plus tests AC-OC-02/16, Round test 47/18, AC-CON-12 that assert it).
- **Timers started by a player or system event**: the 3 s IIF hold (player Interact), the Hide enter/exit transitions, the defence window, the disconnect grace and pawn-loss retry (connection events), informant respawn (a kill), the OC-12 StageSignal release (a counted completion, on a global 30 s grid, scheduled only while a publication is pending), Preparing-phase waits (not `Active`), and the post-latch `EndedMinDwellSec` / `EndedAutoResetSec` (after the result is fixed).
- **Crowd's own ambient schedule**: tell clocks, bush dwell, head holds, idle timers. These are NPC behaviour drawn without reference to players, objectives or waiting.
- **Reporting metrics only**: F5 `ObjectiveFinalDwellDefectSec`, Round F7 `StallFlagMinutes`, `TargetRoundMinutes`, the Concealment entry log. A stall is reported as a defect and fixed structurally, never answered with a timer.
- **Player actions**: a team ping, a bush emergence strike, a context prompt.
- **Word false positives**: loading-screen "reveal", "Post-Round Reveal" (only at `Ended`), "hint" as UI prompt or "Strike hint", "ping" as network latency, "clock" as CPU clock or server-time sync.

Observations that are not FAILs:

- Round OQ14 (round-match-framework.md:807): on a host abort/void the destruction of player pawns before travel can be a one-off player/NPC difference. It is triggered by a host command (SP-6 allows the host to abort), not by waiting, and Round already lists it as an open question for the ReplicationSpecialist.
- Concealment line 281 says Objectives' stall pressure is "the only thing that pushes a hider out". This is satisfied by the structural race (SP-1 to SP-7); AC-CON-24 measures it and routes failures to more objective pressure, never a timer or reveal.

## Hits

Short names: OC = `objectives-chain.md`, RM = `round-match-framework.md`, CON = `concealment-bushes.md`, CNB = `crowd-npc-behaviour.md`.

| File:line | Term(s) | Excerpt (<= 25 words around the term) | Verdict |
|---|---|---|---|
| OC:13 | timer, reveal | "the stall-pressure rule: never a timer, never a reveal" | Not waiting-driven: states the prohibition itself. |
| OC:15 | reveal | "dependents: HUD, Round, IIF, Post-Round Reveal, Telemetry" | Not waiting-driven: system name in a dependency list (Post-Round Reveal runs at `Ended` only). |
| OC:19 | timer | "the Input & Interaction Framework's hold timer" | Not waiting-driven: the 3 s hold is started by a player's Interact and owned by the IIF. |
| OC:21 | clock | "Without it the round has no clock but the other team's knives" | Not waiting-driven: states that no clock exists. |
| OC:39 | timer | "It does not own: ... the hold timer, cancels and prompts (IIF)" | Not waiting-driven: ownership list; the hold is IIF's and player-started. |
| OC:67 | timer | "same pose, timer and events, no change" | Not waiting-driven: the hold timer of a player-started tend that completes with no effect (AllowNoEffect). |
| OC:69 | timer | "up to 4 pawns tend at once with independent IIF timers" | Not waiting-driven: per-tender 3 s holds, each started by a tend action. |
| OC:84 | clock, timer | "No clock. A release is a Round TimerFired(StageSignal) item owned by Objectives, scheduled ... only while a publication is pending" | Not waiting-driven: OC-12 rival release is triggered by a counted completion on a global 30 s grid, and with nothing pending no timer exists. |
| OC:87 | reveal | "the signal is the only pressure a stalled team feels ... without any reveal" | Not waiting-driven: an integer count released after a rival completion, with no site, time or identity. |
| OC:107 | reveal | "the Policy may release the stage timeline and assignment to PostRound (VS reveal)" | Not waiting-driven: release happens only in `Ended`, after the result is fixed. |
| OC:111 | clock, timer, dwell, reveal | "Never a timer, never a reveal ... no system adds a round clock, a countdown, a dwell timer, a time-triggered unlock, a reveal" | Not waiting-driven: SP-3 is the rule that forbids all of them. |
| OC:116 | timer, reveal | "No penalty, timer or reveal is added against it" | Not waiting-driven: SP-4 forbids a timer or reveal against camping the final. |
| OC:119 | timer, reveal, ping | "Never a timer, a reveal, a ping or a marker." | Not waiting-driven: SP-7 limits the stall response to structural levers. |
| OC:143 | timer | "TimerFired(StageSignal) items" | Not waiting-driven: the OC-12 release item, scheduled only by a counted completion. |
| OC:270 | timer | "the tend runs fully (pose, ring in Bone, timer) and completes with no effect" | Not waiting-driven: the 3 s hold started by a player action; identical for all four court states. |
| OC:272 | timer | "If a rival-signal release tick falls after the latch: the pending release is discarded" | Not waiting-driven: OC-12 release grid edge case; nothing is scheduled after the latch. |
| OC:276 | reveal | "occupancy is public, so the reply reveals nothing" | Not waiting-driven: a rule that keeps a Begin rejection from revealing anything. |
| OC:284 | hint | "the round proceeds with the clue (never a sharper hint)" | Not waiting-driven: forbids escalating the clue; no time component. |
| OC:292 | ping | "Informant Pings & Team Communication ... draws a team-only place ring and reads nothing from Objectives" | Not waiting-driven: a team ping is a player action and reads no objective state. |
| OC:317 | reveal | "Post-Round Reveal & Results (#21, VS) | Released stage timeline" | Not waiting-driven: released in `Ended` (`PostRound`) only. |
| OC:337 | dwell | "ObjectiveFinalDwellDefectSec ... F5 final-dwell defect threshold (reporting only)" | Not waiting-driven: F5 is a reporting metric; it triggers no in-game effect. |
| OC:395 | timer | "the StageSignal timer is a TimerFired item owned by Objectives" | Not waiting-driven: change-request note for the OC-12 release item. |
| OC:434 | clock | "WHEN the fake clock advances 60 min with no completion, THEN no state changes" | Not waiting-driven: AC-OC-02 test asserting waiting changes nothing. |
| OC:441 | timer | "team 0's RivalPrelimCount becomes 1 exactly at t_rel = 30 x ceil((100 + Delta_1)/30) s" | Not waiting-driven: AC-OC-09 release grid, triggered by team 1's completion at t_c. |
| OC:450 | clock, timer | "no Objectives timer exists (an OC-12 release timer exists only while a release is pending, and none is)" | Not waiting-driven: AC-OC-16 test asserting waiting earns nothing. |
| OC:453 | clock, timer, dwell, reveal | "no rule adds a round clock, dwell timer, time-based unlock or positional reveal in response to waiting" | Not waiting-driven: this is AC-OC-18 itself, the criterion under review. |
| OC:484 | ping | "a teammate or informant who knows the live court may ping it" | Not waiting-driven: a team ping is a player action costing an action; carries no role data. |
| OC:485 | timer, reveal | "never a penalty, timer or reveal" | Not waiting-driven: open question whose stated response excludes timer and reveal. |
| RM:3 | ping | "pings cleared at the round-end step" | Not waiting-driven: status note; team pings are cleared at round end. |
| RM:13 | clock | "with no clock and no enemy head-count" | Not waiting-driven: states Round shows no clock. |
| RM:15 | reveal | "dependents: ... Objectives Chain, HUD, Lobby UI, Post-Round Reveal" | Not waiting-driven: system name in a dependency list. |
| RM:19 | timer, reveal | "there is no match time limit and Round adds no timer or reveal that could end or steer a round" | Not waiting-driven: a rule that forbids timers and reveals. |
| RM:21 | clock | "never shows a clock or a head-count of rivals" | Not waiting-driven: a rule that forbids a clock. |
| RM:27 | clock | "the line 'no clock, only the other team'" | Not waiting-driven: framing statement that no clock exists. |
| RM:29 | clock, timer | "From then on the only clock is the other team. There is no timer to watch" | Not waiting-driven: player-fantasy statement that no timer exists. |
| RM:31 | clock, reveal | "(3) no clock, countdown or enemy count is ever shown before Ended" | Not waiting-driven: forbids a clock; the loading-screen "reveal" is the one scheduled start (R7). |
| RM:33 | timer, reveal | "P4 A Race, Not a Hunt ('never a timer, never a reveal'; the log's EndReason measures it)" | Not waiting-driven: pillar statement; EndReason is a log measure. |
| RM:80 | reveal | "Round waits until every Connected client is world-ready or WorldReadyTimeoutSec (10) passes" | Not waiting-driven: a Preparing-phase readiness bound before `Active`; the "reveal" is the loading screen release (R7). |
| RM:83 | clock | "Its grace clock starts at ActivePhaseStart (F4)." | Not waiting-driven: the disconnect grace, driven by a connection event, not by gameplay waiting. |
| RM:86 | timer | "it is not a game timer and it does not run in any other phase" | Not waiting-driven: PreparingWatchdogSec bounds a hung `Preparing` and never runs in `Active`. |
| RM:90 | clock, reveal | "no countdown or 'starting in' text is ever shown" | Not waiting-driven: R7 start-of-round release at a scheduled server time; forbids a countdown. |
| RM:99 | clock, timer, dwell, reveal, ping | "EndedMinDwellSec and EndedAutoResetSec count from this step, so Reset can never run between the two steps" | Not waiting-driven: R9 round-end ordering; dwell and auto-reset start only after the result is fixed; "reveal" is the VS Post-Round Reveal via the Policy. |
| RM:101 | timer | "cancel every Round timer and invalidate RoundEpoch-stamped work" | Not waiting-driven: R10 Reset cleanup, runs after the round. |
| RM:105 | clock | "Nobody may keep a feature-local clock or timestamp to order events" | Not waiting-driven: R11 queue ordering key; a rule against local clocks. |
| RM:106 | timer | "Only genuinely separate events (a timer, a deferred outcome) become new items" | Not waiting-driven: queue mechanism for event-driven items. |
| RM:108 | timer | "Round's own timers do not use the engine timer manager: they live in a Round-owned due-time heap" | Not waiting-driven: implementation detail of how event-created timers are held. |
| RM:119 | timer | "TimerFired ... Round (grace expiry GraceExpiry; pawn-loss retry ...), Penalty (InformantRespawn), Objectives (StageSignal release)" | Not waiting-driven: closed set of event-created timers (connection loss, kill, completion, strike, hide transition, Crowd schedule); none created by waiting. |
| RM:125 | timer | "TimerFired(ConcealmentTransition), which keeps resolving so an in-flight hide transition completes" | Not waiting-driven: post-latch completion of a player-started hide transition. |
| RM:140 | clock, timer, dwell, reveal | "no timer anywhere that can end or change the outcome of an Active round: Round adds no dwell timer, no sudden-death rule and no countdown" | Not waiting-driven: R16 forbids them and lists only event-driven Round timers. |
| RM:148 | timer | "start the grace timer, a record {P, GraceExpiry, ExpectedEpoch ...}" | Not waiting-driven: disconnect grace started by a connection event. |
| RM:149 | timer | "Rejoin inside grace: cancel the grace timer" | Not waiting-driven: connection event cancels the grace. |
| RM:150 | timer | "The grace timer starts at the supersede instant" | Not waiting-driven: started by a supersede connection event. |
| RM:151 | timer | "Grace expiry (a TimerFired item): RequestTransition(P, Informant, Disconnect ...)" | Not waiting-driven: converts a disconnected player, driven by a connection loss. |
| RM:154 | clock | "the grace clock starts at ActivePhaseStart" | Not waiting-driven: disconnect before `Active`, a connection event. |
| RM:159 | timer | "the grace timer is cancelled and the participant repossesses their pawn" | Not waiting-driven: reconnect cancels the grace. |
| RM:175 | reveal, ping | "NeverReleased (DealDigest, TuningHash, mailbox order, connection and ping data, Debug items)" | Not waiting-driven: release classes; "ping" is network latency, and releases are only in `Ended`. |
| RM:180 | clock, reveal | "no client presentation may show elapsed Active time (AC 31)" | Not waiting-driven: forbids any elapsed-time clock; "reveal" is the scheduled start. |
| RM:192 | timer | "Grace timers, the hand-over table ... Cancelled in Reset" | Not waiting-driven: reset table. |
| RM:209 | clock, timer, dwell | "(d) Round adds no dwell timer and shows no clock." | Not waiting-driven: R23 guard that forbids them. |
| RM:252 | timer | "TimerFired for respawn; StrikeLimit transitions inside the slot" | Not waiting-driven: informant respawn timer started by a strike-limit transition. |
| RM:258 | reveal | "Post-Round Reveal (#21) | reads in Ended" | Not waiting-driven: `Ended`-only reader. |
| RM:403 | timer | "A StallFlag round is reported as an Objectives stall-pressure defect, never answered with a Round timer." | Not waiting-driven: F7 reporting metric; the response is explicitly not a timer. |
| RM:428 | timer | "If a defender's brace expires (TimerFired) and a strike on them arrives in the same tick" | Not waiting-driven: defence window started by a player's Prepare. |
| RM:431 | timer | "If a grace expiry and the same participant's rejoin are due in the same tick" | Not waiting-driven: connection-event edge case. |
| RM:432 | timer | "If two participants' grace timers expire in the same tick" | Not waiting-driven: disconnect grace edge case. |
| RM:433 | timer | "If a participant rejoins and drops again just before the first grace timer fires" | Not waiting-driven: connection-event edge case. |
| RM:455 | timer | "the grace-expiry TimerFired item for the already-kicked participant is a no-op" | Not waiting-driven: host kick and grace edge case. |
| RM:459 | timer | "If a participant disconnects during Ended or Reset: no grace timer, no transition" | Not waiting-driven: no timer outside `Active`. |
| RM:468 | clock | "the field is zero during Active, so the public struct is no clock source" | Not waiting-driven: a rule that keeps clients without a clock source. |
| RM:470 | reveal | "that client keeps its loading screen until it holds ExpectedPawnCount pawns or RevealPawnWaitMaxSec has passed" | Not waiting-driven: loading-screen bound at round start, per-client. |
| RM:476 | ping | "If a disconnected player's ping, bIsInactive flag or PlayerState presence would change on other clients: forbidden" | Not waiting-driven: "ping" is network latency; the rule forbids a leak. |
| RM:497 | clock | "the start of the Preparing travel bound and the readiness-barrier clock" | Not waiting-driven: `Preparing` travel bound, before `Active`. |
| RM:511 | timer | "informant-respawn timers as TimerFired items" | Not waiting-driven: respawn timer started by an informant conversion (a kill). |
| RM:512 | timer | "Round adds no timer and no stall pressure (Objectives owns it)." | Not waiting-driven: a rule that Round adds no timer. |
| RM:516 | reveal | "Post-Round Reveal & Results (#21, VS) | The Event Log in Ended" | Not waiting-driven: `Ended`-only. |
| RM:521 | ping | "Round clears every team ping part in the round-end step" | Not waiting-driven: ping cleanup at round end; a team ping is a player action. |
| RM:538 | timer | "Non-binding pacing target. No timer exists." | Not waiting-driven: TargetRoundMinutes is read only by F7 reporting. |
| RM:539 | timer | "F7 reporting only ... Never a timer and never a rule." | Not waiting-driven: StallFlagMinutes flags a defect in the log; nothing intervenes. |
| RM:545 | timer | "Defensive abort of a stuck Preparing. Not a match timer." | Not waiting-driven: `Preparing`-only watchdog. |
| RM:547 | reveal | "Never shown as a countdown." | Not waiting-driven: StartLeadSec for the scheduled start, a one-off before `Active`. |
| RM:549 | reveal | "the longest a client ... keeps its loading screen while still missing some of ExpectedPawnCount pawns" | Not waiting-driven: per-client loading-screen bound at round start. |
| RM:552 | timer | "It starts only after the result is fixed, so it cannot change an outcome and is not a match timer." | Not waiting-driven: EndedAutoResetSec runs post-result. |
| RM:569 | reveal | "Active reveal. The first frame of Active is the world already populated" | Not waiting-driven: start-of-round presentation; the rule bans any marking sound or effect. |
| RM:570 | reveal | "The polished reveal is Vertical Slice (Post-Round Reveal & Results)." | Not waiting-driven: `Ended` banner text only. |
| RM:578 | clock, reveal | "Every client leaves the briefing state at the same scheduled server time" | Not waiting-driven: one-off scheduled start, not an in-round clock. |
| RM:592 | reveal | "never show elapsed Active time, a countdown or opponent counts before Ended" | Not waiting-driven: presentation rule that forbids a clock or countdown. |
| RM:609 | timer | "Round passes ExpectedEpoch on every timer transition" | Not waiting-driven: stale-timer guard for grace-driven transitions. |
| RM:614 | timer | "defence-window end is a TimerFired item" | Not waiting-driven: defence window started by a player's Prepare. |
| RM:618 | timer | "informant respawn timers are TimerFired items" | Not waiting-driven: respawn started by an informant conversion. |
| RM:642 | reveal | "RequestFallbackTravel, reveal flag, ShouldAdmitExpired | ALREADY DONE" | Not waiting-driven: change-log row for Session items (travel/start flag), not an `Active` rule. |
| RM:655 | timer | "StageSignal as an Objectives TimerFired item; Event Log StageSignalReleased" | Not waiting-driven: the OC-12 release item. |
| RM:663 | reveal | "bound a late loader's loading screen" | Not waiting-driven: loading-screen bound at round start. |
| RM:665 | timer | "the post-latch timer lane (EndedMinDwell/EndedAutoReset)" | Not waiting-driven: post-result lane. |
| RM:666 | timer | "TimerFired subkinds CrowdSchedule, ConcealmentTransition (post-latch exemption), BodyRelease, BodyRemoval" | Not waiting-driven: closed subkind list, each event-created or Crowd's own schedule. |
| RM:667 | clock | "'GameState server clock' for presentation reads as EstimatedServerNow()" | Not waiting-driven: clock-sync wording fix for the scheduled start. |
| RM:673 | ping | "Clear the team ping parts at the round-end step" | Not waiting-driven: ping cleanup at round end. |
| RM:702 | clock | "GIVEN a participant Disconnected at P6 (fake clock) ... the grace expires at ActivePhaseStart + G_eff" | Not waiting-driven: test 9 of the disconnect grace. |
| RM:710 | timer | "the Round timer heap is empty" | Not waiting-driven: test 13a that Reset clears timers. |
| RM:727 | clock, timer | "a fake clock advanced 24 h in Active with no items, THEN the phase is Active and the timer heap holds only grace and pawn-loss retries" | Not waiting-driven: test 47 `NoTimerEndsActiveRound` asserts waiting changes nothing. |
| RM:734 | clock | "the fake clock advances 10 x TargetRoundMinutes, THEN no outcome, phase Active, empty queue" | Not waiting-driven: test 18 asserts waiting yields no outcome. |
| RM:741 | clock | "GIVEN a fake clock and an Active participant, WHEN its client goes silent at t, THEN takeover occurs in [t + 3.0, t + 3.3] s" | Not waiting-driven: test 22 of the disconnect grace timeline. |
| RM:742 | clock | "GIVEN a rejoin 1 s before grace expiry (fake clock), THEN the participant stays Active" | Not waiting-driven: test 23a of rejoin inside grace. |
| RM:752 | timer | "WHEN the first timer fires, THEN Rejected_StaleConnection" | Not waiting-driven: test 49 of the grace generation guard. |
| RM:756 | timer | "starts no LateJoinTakeoverTimeoutSec timer of its own" | Not waiting-driven: informant re-admission, a connection event. |
| RM:766 | clock | "each client's loading-screen dismiss time (on its server clock) is >= ActiveAtServerTime" | Not waiting-driven: test of the scheduled start. |
| RM:767 | reveal | "it drops its loading screen at ActiveAtServerTime + RevealPawnWaitMaxSec" | Not waiting-driven: test of the late-loader bound. |
| RM:784 | clock, timer | "WHEN OnParticipantRejoined(Superseded) arrives at t_s + G_eff - 1, THEN control is handed back" | Not waiting-driven: test 62 of supersede grace. |
| RM:795 | timer, ping | "Round-owned timer heap; ... PlayerState ping/presence policy (with Session)" | Not waiting-driven: ADR open question; "ping" is network latency. |
| RM:797 | reveal | "CLOSED - declined for MVP by the user 2026-10-25: no name labels over the held pawns at Ended" | Not waiting-driven: a declined `Ended` reveal. |
| RM:804 | clock | "(b) client clock-sync error for the scheduled start (AC 30a)" | Not waiting-driven: spike list for the start. |
| RM:807 | reveal | "On an abort or void in Active ... the destroy replicates as a one-off player/NPC difference. ... it is an uncontrolled reveal" | Not waiting-driven: triggered by a host abort/void (SP-6), not by waiting; a Round open question on a defect, not a rule. |
| CON:18 | timer | "It adds no timer of any kind." | Not waiting-driven: states Concealment has no timer. |
| CON:29 | reveal | "The emergence is a risky reveal, not the bush's reward (AC-CON-13)." | Not waiting-driven: the emergence follows the player's own strike. |
| CON:33 | timer | "P4 ('hiding is a pause, not a strategy': no reward for staying, no timer either; stall pressure is the Objectives Chain's)" | Not waiting-driven: states no timer. |
| CON:61 | timer | "the server commits concealment (a Round queue timer item, fixed data time, never an anim notify)" | Not waiting-driven: 0.5 s enter commit started by the player's Hide(Enter). |
| CON:65 | clock | "One clock: replicated server start time." | Not waiting-driven: pose-sync timestamp for presentation. |
| CON:75 | timer, dwell, reveal | "No dwell timer exists: no minimum or maximum stay, no warning, no reveal." | Not waiting-driven: CON-R7 forbids a dwell timer and reveal. |
| CON:77 | clock | "(0.45 s) on the server clock (CON-R6a ...)" | Not waiting-driven: exit crossing of a player-issued Hide(Exit). |
| CON:84 | dwell | "carries no companion data (no concealed-since, no dwell)" | Not waiting-driven: un-cull happens at a kill's death write; carries no dwell data. |
| CON:93 | dwell | "Crowd exits it later through an ordinary Hide(Exit) drawn from its NPC dwell distribution" | Not waiting-driven: Crowd's own NPC schedule. |
| CON:99 | clock, dwell | "with a dwell distribution that has no cap (conflict C7: this version stands)" | Not waiting-driven: NPC dwell is Crowd's own draw; explicitly uncapped. |
| CON:101 | timer, dwell | "fixed, if needed, by raising the cost of emerging, never by a dwell timer" | Not waiting-driven: entry log is metrics; response excludes a dwell timer. |
| CON:103 | hint | "The host's aim and Strike-hint traces ignore hidden non-allies" | Not waiting-driven: word false positive; "Strike hint" is a UI trace. |
| CON:115 | timer | "Commits, leaf crossings and transition ends are Round queue timer items" | Not waiting-driven: items created by Hide transitions. |
| CON:126 | timer | "queue timer items, phase gate (Active/Ended/Reset), R17 bound-AI rule" | Not waiting-driven: dependency table. |
| CON:130 | ping | "no ping from inside a bush" | Not waiting-driven: a ping is a player action; this rule refuses it from a bush. |
| CON:150 | dwell | "D_N, D_P | float (min) | Crowd dwell distribution; entry log | Mean dwell per entry" | Not waiting-driven: formula variable (measured mean dwell). |
| CON:154 | dwell | "Player dwell matters as much as entries: D_P = 1.0 min would triple the player term, so dwell is logged with entries." | Not waiting-driven: F1 example; dwell is logged, not acted on. |
| CON:234 | timer, dwell | "If a concealed player stays a long time: nothing happens (no timer)." | Not waiting-driven: edge case stating that no timer exists. |
| CON:245 | timer, reveal | "Concealment adds no timer or reveal." | Not waiting-driven: edge case stating that none exists. |
| CON:250 | dwell | "If NPC dwell were capped: any stay longer than the cap would mark a player; NPC dwell has no cap." | Not waiting-driven: NPC dwell stays uncapped. |
| CON:267 | timer | "queue timer items, phase gate, R17, Event Log slice" | Not waiting-driven: dependency table. |
| CON:269 | dwell | "NPC bush use at F2 rate, occupancy-blind, uncapped dwell" | Not waiting-driven: Crowd's own bush behaviour. |
| CON:281 | timer | "Concealment has no timer, so Objectives' stall pressure (SP-1 to SP-8, SP-8 in place) is the only thing that pushes a hider out." | Not waiting-driven: states no Concealment timer; the pressure is Objectives' structural race (SP-7), measured by AC-CON-24. |
| CON:283 | ping | "to reject a ping from a sender in or moving through a bush" | Not waiting-driven: a ping is a player action; refused inside a bush. |
| CON:306 | dwell, reveal | "Deliberately absent: any dwell, capacity or reveal key." | Not waiting-driven: states no such key exists. |
| CON:342 | timer | "Free look, slow sway only, muted colour; no countdown, no meter | no timer" | Not waiting-driven: UI spec states no timer. |
| CON:354 | hint | "A hint, never a gate." | Not waiting-driven: a context prompt computed locally from own state, not a reveal. |
| CON:355 | timer | "No icon, meter, timer, 'hidden'/'detected' text or stealth wording" | Not waiting-driven: UI spec bans a timer. |
| CON:357 | hint | "Strike hint | Attacker | Absent for any concealed non-ally" | Not waiting-driven: states the hint reveals nothing about a concealed pawn. |
| CON:374 | dwell | "NPC bush row T5, rate and dwell (conflict C7: Concealment version stands)" | Not waiting-driven: cross-reference table. |
| CON:375 | ping | "the concealed-sender ping rule (D26): no ping from inside a bush" | Not waiting-driven: cross-reference table. |
| CON:384 | dwell | "no dwell cap (log-normal median 15 s, sigma 0.8)" | Not waiting-driven: change request to Crowd for NPC behaviour. |
| CON:397 | dwell | "NPC dwell stays as is (no heavier tail requested: long dwell is an accepted P2 tell)" | Not waiting-driven: Crowd's own dwell, left unchanged. |
| CON:421 | clock | "'GameState server clock' read as EstimatedServerNow()" | Not waiting-driven: change-log entry. |
| CON:424 | timer | "Round R13 TimerFired(ConcealmentTransition) with post-latch exemption" | Not waiting-driven: change-log entry for hide-transition timers. |
| CON:431 | clock | "WHEN the client's server clock reaches Start + BushEnterSwapSec, THEN on both clients the body ... disappear" | Not waiting-driven: AC for the player-started enter swap. |
| CON:433 | hint | "no actor, shadow, decal, sound or aim/Strike-hint hit exists for that pawn at any sample" | Not waiting-driven: AC that a concealed pawn is not revealed. |
| CON:445 | dwell, reveal | "AC-CON-12 NoDwellTimer: ... pawn concealed for 30 min ... THEN it is still concealed" | Not waiting-driven: AC that asserts no dwell timer, reveal or timeout key exists. |
| CON:459 | hint | "the host's aim trace and Strike hint miss the pawn" | Not waiting-driven: AC for host parity; no reveal. |
| CON:460 | dwell | "the log yields per-kind entry rates and mean dwell, and F5 evaluated at the measured n_P and n_N" | Not waiting-driven: F5 reporting metric. |
| CON:463 | timer, reveal | "fixed by more objective pressure (Objectives Chain), never by a timer or a reveal (P4 design test)" | Not waiting-driven: AC-CON-24 measures, and routes failures to objective pressure, never a timer or reveal. |
| CON:467 | ping | "a pawn in Emerging counts as concealed for the ping refusal" | Not waiting-driven: ping eligibility predicate. |
| CON:478 | dwell | "Can Crowd meet r_N 0.20/min ... with occupancy-blind choice and an uncapped dwell" | Not waiting-driven: open question for Crowd's behaviour parity. |
| CON:482 | reveal | "Should the VS Post-Round Reveal release concealed positions at Ended? (MVP: no)" | Not waiting-driven: `Ended`-only, and MVP says no. |
| CON:484 | clock | "Does seeking hide clips from the replicated server clock (CON-R6a) hold within one frame" | Not waiting-driven: animation-sync spike. |
| CON:485 | clock | "CON-R6a (start-time clock, carried by ContactServerTime meanwhile)" | Not waiting-driven: closed schema question about a pose timestamp. |
| CNB:49 | timer | "the same per-pawn random timer, player or NPC; it is off in MVP" | Not waiting-driven: B8 idle overlay, an off-in-MVP ambient idle, not tied to objectives. |
| CNB:78 | clock | "An independent clock with mean CrowdHeadHoldIntervalSec (600 s, 0.10/min) turns the NPC's next G2 glance into a 2.5-6 s hold" | Not waiting-driven: Crowd's own ambient tell schedule (T13); no read of players or waiting. |
| CNB:84 | clock | "the T4 clock decides when a brace is due" | Not waiting-driven: Crowd's own ambient brace schedule. |
| CNB:86 | timer, dwell | "There is no dwell cap, no dwell timer and nothing ever times a hider out" | Not waiting-driven: NPC chooses its own stay; the rule forbids a dwell timer. |
| CNB:88 | clock | "that tell's exponential clock restarts at the gap's end" | Not waiting-driven: Crowd's own tell-clock rule (chain-gap guard). |
| CNB:112 | timer | "reaction latencies are sampled timers, so a cut never changes behaviour statistics" | Not waiting-driven: AI reaction latencies, per NPC. |
| CNB:135 | ping | "T18 | Ping (IA_Ping, team-only place ring ...) | none: pinging plays no pawn animation" | Not waiting-driven: a team ping is a player action with no world-visible deviation. |
| CNB:201 | clock | "tell j is a_j = r_N,j / (1 - (Lambda - r_N,j) * T_gap)" (formula text "chain-gap compensation ... the gap is anchored at the visible moment") | Not waiting-driven: F2 sampler math for NPC tell clocks. |
| CNB:225 | clock | "A Monte Carlo of the eight tell clocks under the gap rule (2 x 10^6 simulated NPC-minutes ...)" | Not waiting-driven: F2 example for Crowd's tell rates. |
| CNB:283 | timer | "reaction delays are sampled timers, so behaviour statistics do not change" | Not waiting-driven: server-cost cut order for AI. |
| CNB:316 | clock | "it is discarded and that tell's clock restarts at the gap's end (R7)" | Not waiting-driven: Crowd's own tell-clock edge case. |
| CNB:319 | timer, dwell | "there is no dwell cap and no timer on hiders." | Not waiting-driven: states no timer on hiders. |
| CNB:335 | timer | "the follower and timers use elapsed time, not frame counts" | Not waiting-driven: AI hitch handling. |
| CNB:381 | dwell | "NPC bush use at the F2 rate through the one Hide path (occupancy-blind, uncapped dwell ...)" | Not waiting-driven: dependency table. |
| CNB:383 | ping | "its Tell Matrix row T18 (no world-visible deviation)" | Not waiting-driven: dependency table. |
| CNB:420 | timer, dwell | "Not a cap and not a timer on hiders (locked 'no dwell timer'; Concealment deliberately has no dwell key)" | Not waiting-driven: tuning key for the NPC's own stay; states no timer. |
| CNB:421 | clock | "CrowdReversalIntervalSec / CrowdHeadHoldIntervalSec ... T12, T13 (G6 clock, 0.10/min)" | Not waiting-driven: tuning keys for Crowd's ambient tell clocks. |
| CNB:445 | timer | "each NPC's idle phase, weight-shift timer and gaze timer start from its own random offset" | Not waiting-driven: per-pawn animation desync. |
| CNB:484 | dwell | "CON-R3, CON-R4, CON-R11, CON-R14; F1, F2; AC-CON-12, 15, 16, 19; edge cases on co-occupants and dwell" | Not waiting-driven: cross-reference table. |
| CNB:487 | ping | "PNG-R2 (no pawn animation, no replicated pawn state; accepted Pillar 2 exception)" | Not waiting-driven: cross-reference table. |
| CNB:493 | dwell | "reconcile IVP Cross-References item 4 ('meeting F5 and F7 on dwell time') with the F5 note" | Not waiting-driven: IVP parity metric; Crowd treats dwell parity as advisory. |
| CNB:495 | timer | "for every idle pawn with per-pawn random timers" | Not waiting-driven: shared idle overlay change request. |
| CNB:499 | dwell | "its change request 3 to Crowd is applied (occupancy-blind choice, no dwell cap" | Not waiting-driven: change-log entry. |
| CNB:528 | dwell | "Concealed and transitioning pawns excluded from RVO ... dwell unchanged" | Not waiting-driven: change-log entry. |
| CNB:529 | dwell | "Occupancy-blind bush choice, no dwell cap, r_N 0.19 at k = 1.5" | Not waiting-driven: change-log entry. |
| CNB:566 | clock | "no NPC shows two different tells within CrowdTellChainGapSec" (AC text: "the restarted exponential clocks") | Not waiting-driven: AC-CNB-31 tests Crowd's own tell clocks. |
| CNB:571 | dwell | "NPC versus logged player dwell is reported with Policy F7's statistics." | Not waiting-driven: AC-CNB-15 advisory metric; no in-game effect. |
| CNB:588 | clock | "(CPU model and clock recorded with the evidence ...)" | Not waiting-driven: word false positive; CPU clock speed in a performance test. |

## AC3 (DEFERRED): AC-OC-24 Tell Matrix row T3

**Source**: `design/gdd/crowd-npc-behaviour.md`, CNB-R16 "Baseline and Tell Matrix", row T3 at line 120. Exact text:

> | T3 | Objective interaction | 3 s two-handed pose at a court fixture | Yes: decoy tending, 0.0046/min (all courts uniformly, occupied courts included; a full fixture is refused and another court chosen) | 0.65-0.75 for full-length tends (the strongest honest read, P4; Objectives F3 `P_full`); early releases reported separately (`P_short`, alarm > 0.90) | Objectives + IIF + CNB (AC-OC-21, AC-OC-24) |

Checked against the AC3 requirements:

| Requirement | Row T3 | Result |
|---|---|---|
| 3 s pose (agreeing with `ObjectiveInteractSec`) | "3 s two-handed pose at a court fixture" | Present. The row does not name `ObjectiveInteractSec` itself; the value agrees. |
| Red herring yes (decoy and feint) | "Yes: decoy tending, 0.0046/min" | Decoy is present. A player feint is not mentioned in the row. |
| F3 band 0.65-0.75 | "0.65-0.75 for full-length tends ... Objectives F3 `P_full`" | Present. |
| References the Objectives GDD | "Objectives F3 `P_full`" and owner "Objectives + IIF + CNB (AC-OC-21, AC-OC-24)" | Present. |
| AC-CNB-14 passes | `AC-CNB-14` is defined at crowd-npc-behaviour.md:570; no run recorded here | Not run. |

**Status: DEFERRED** (not failed). The row exists and matches on pose, decoy, band and Objectives reference, but it does not state the feint half of "decoy and feint", and AC-CNB-14 has not been run. **Blocking owner: Crowd & NPC Behaviour** (AC-CNB-14 is that epic's test). Re-run this check when the Crowd epic lands the Tell Matrix rows; if the owner adds "feint" to the row (or the game-designer accepts the decoy-only wording) and AC-CNB-14 passes, record "row text matches; AC-CNB-14 run pending in the Crowd epic" or the pass. Not counted against this story's BLOCKING evidence.

## Sign-off

- qa-lead: agent review 2026-10-28 (qa-lead subagent) — human sign-off PENDING
- game-designer: PENDING (human)
