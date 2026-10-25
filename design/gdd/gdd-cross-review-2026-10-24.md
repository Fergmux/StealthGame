# Cross-GDD Review Report

Date: 2026-10-24
Skill: `/review-all-gdds full` (autonomous run; rigor full)
GDDs Reviewed: 18 of 18 present
Not read: none (all 18 read by section; coverage limits are listed under "Coverage" below)
Focus: full (consistency + design theory + cross-system scenarios)
Context read: game-concept.md (pillars, anti-pillars, MVP), systems-index.md, design/registry/entities.yaml (247 entries, baseline), production/session-logs/decision-log.md (rulings), production/session-state/active.md (locked rules; pending user decisions excluded from new findings)
Engine: Unreal Engine 5.8 (installed 5.8.4), C++

Systems Covered: Tuning Data & Config, Participant State & Roster, Input & Interaction Framework, Character Pool & Appearance, Session & Travel, Round & Match Framework, Information Visibility Policy, Level Socket Contract, Shared Character & Locomotion, Camera System, Crowd & NPC Behaviour, Concealment (Bushes), Assassination & Defence Reversal, NPC-Kill Penalty & Informants, Objectives Chain, Procedural City Generator, Teammate Markers, HUD.

Method: Phase 2 (consistency) and Phase 3 (design theory) ran as parallel game-designer sub-agents; Phase 2 was extended by a coverage-gap pass (2b); Phase 4 (scenarios) ran as a third sub-agent. All reviewers read by section (Grep `^#` + offset/limit) against the registry, each GDD's Summary / Dependencies / Tuning Knobs / Formulas / Cross-References and its "Change requests received (reconciliation 2026-10-24)" table. Full findings with quotes and line numbers are in the appendices.

### Coverage

- 2a dependencies: all 18 GDDs' dependency tables compared with each other and with systems-index rows 1-18.
- 2b rules: Roster, ASN, Round, PEN core rules; Round phase names grepped across all GDDs. Edge Cases sections were not swept (the skill assigns them to `/design-review`), but Phase 4 walked edge paths for 5 scenarios.
- 2c stale references: Cross-References + reconciliation tables of all 18 GDDs; struct shapes (`FStrikeResult`, `FDefenceResult`, `FDeathState`, `FActionState`) grepped everywhere.
- 2d knobs: Tuning Knobs of all GDDs vs TDC Canonical Tunables / Registered Owner Keys / owner invariants; LSC only for rows other GDDs reference (~78 keys not individually checked).
- 2e formulas: main timing invariants, speeds, cull/crossing timings, emergence and witness keys, Generator budget; Generator F1 vs LSC walkable-area band at `NpcCount` 35 not checked.
- 2f ACs: Objectives ACs vs TDC; 16 cross-system knob names grepped in ACs (spot check, not a full sweep).

---

### Consistency Issues

#### Blocking (must resolve before architecture begins)

🔴 **C-B1 — I-OC2: Objectives said "clamp", TDC rejects.** *FIXED this run.* objectives-chain.md Tuning Knobs (`RivalSignalDelayMaxSec`) and AC-OC-22 said the validator clamps Max; TDC I-OC2 and the logged reconciliation ruling (2026-10-24T10:46:07Z, CR-6 "invariants never repair") say reject/warn. Applied: knob row and AC-OC-22 restated as rejection; missing received-table row "TDC CR 12" added.

🔴 **C-B2 — I-OC3 failure mode: Objectives AC says warn, TDC says reject.** *OPEN.* AC-OC-32 (objectives-chain.md ~line 461) "THEN I-OC3 warns"; tuning-data-config.md line 261 registers I-OC3 as `reject`; the Objectives knob row (line 331) names the invariant but no failure mode. The owner (Objectives) never states the outcome, so the correct value is not unambiguous. Recommendation: reject (matches TDC and the P1 probe-release-tell rationale at objectives-chain.md line 337), then AC-OC-32 expects a rejected preset.

🔴 **C-B3 — PEN-R15 cited a Roster field that does not exist.** *FIXED this run.* PEN-R15 said the exile flag came with "the Roster's own Owner-audience state change carrying the causing command's `ClientCommandSeq`"; Roster R6's owner part is `{PublicHandle, Team, State, RoundEpoch}` only. HUD reads both from `FStrikeResult` (ASN-R19). Applied: PEN-R15 now says `bExiled` is carried in the same `FStrikeResult` as the causing `ClientCommandSeq` (ASN-R19). No new field. (Linked: OQ-NKP-1, `bExiled` provisional.)

#### Warnings (should resolve, but won't block)

Details, quotes and proposed text for each are in Appendix A (C-W1..C-W10) and Appendix B (C-W11..C-W22).

- ⚠️ C-W1 Crowd `CrowdDecoyEarlyReleaseMaxSec < ObjectiveInteractSec` is stated but not a registered invariant (safe values 2.8 vs 2.5 break the decoy model).
- ⚠️ C-W2 HUD I-HUD-3/I-HUD-5 silently narrow Penalty `ExileBeatSec` to 0.75-1.2 s; Penalty still says safe 0.5-1.2.
- ⚠️ C-W3 HUD contradicts itself: I-HUD-3 "Warning" (knob row) vs "rejects" (invariant list, TDC).
- ⚠️ C-W4 UI-ASN-1 shows a stale two-field `FStrikeResult`.
- ⚠️ C-W5 Informant countdown `T_due` has no defined carrier and no IVP fact row (**always-ask: schema**).
- ⚠️ C-W6 Roster R6 reads as the closed schema of C-TeamView although `FObjectiveTeamView` rides it (IVP R4).
- ⚠️ C-W7 Concealment lists Penalty as hard dependent; Penalty/index row 14 do not list Concealment.
- ⚠️ C-W8 LSC lines 688/822 use stale `PickRespawnPawn` signatures.
- ⚠️ C-W9 Penalty line 357 says index row 14 not updated; it was.
- ⚠️ C-W10 Concealment line 262 states pending `FActionState.StartServerTime` as present.
- ⚠️ C-W11 Many "index does not list X" / "not written" / "provisional" claims are stale across 12 GDDs.
- ⚠️ C-W12 IIF↔Round and Round↔Crowd each list the other as upstream; Round contradicts itself about Crowd/SCL.
- ⚠️ C-W13 One-sided dependency rows (Camera→Roster/Round, TMK→Round/Session, Camera→Penalty, Generator→SCL); TDC calls TMK Soft, TMK says Hard; index row 12 lacks Objectives.
- ⚠️ C-W14 One quantity, two keys: IIF `LatencyWarnRttSec` vs HUD `HudLatencyWarnRttSec` (breaks TDC CR-15.3) (**always-ask: schema**).
- ⚠️ C-W15 CPA cites non-existent Crowd key `ReplacementMaxDelaySec` (actual `CrowdReplacementMin/MaxDelaySec`).
- ⚠️ C-W16 TDC registered key counts stale (Session 13 vs 12, SCL 51 vs 49, Crowd 50 vs 47, Generator 32 vs 30).
- ⚠️ C-W17 Camera says runtime clamps blend for I-CS-1; TDC rejects; IIF safe min of `CameraToggleLockoutSec` is itself rejected.
- ⚠️ C-W18 Safe ranges rejected by another system's invariant: IVP `TeamPositionFeedHz`, LSC `AmbientEmergencePerSocketMinPerMin`, LSC `EmergenceWitnessCellsMaxCount`.
- ⚠️ C-W19 SCL I-SCL-5 reads a "Crowd pace set" no key holds; rejects SCL-safe values.
- ⚠️ C-W20 Concealment Game Feel quotes crossing ≥ 0.25 s below the 0.30 hard minimum.
- ⚠️ C-W21 TDC CR 10 key renames never applied nor declined in IVP/Camera/LSC (**always-ask: schema**).
- ⚠️ C-W22 Generator `CityGenFinalEligibleTargetCount` safe 4-6 can fall below LSC `FinalCandidatesMinCount` 6; no invariant.

---

### Game Design Issues

#### Blocking

None. No GDD's written rules violate an anti-pillar; no dominant strategy makes a pillar fail outright; no economy state makes a round undecidable (mutual stall is handled by the host abort). Checklist results: 3a progression PASS (mastery only; tally is a cost); 3b attention PASS at the limit (4 active while travelling); 3c 12 candidates checked, 3 warnings; 3d PASS with D-W3 (informants are the catch-up lever, MVP relies on external voice — pending item); 3e PASS with D-W3/D-W4; 3f all 18 fantasies cite a pillar; 3g one identity ("the hidden predator who must pay exposure to race") with two seams.

#### Warnings

⚠️ **D-W1 — The brace only pays against inattentive attackers or as a late timing race** (assassination-defence-reversal, IIF, Crowd T4, concept). Strike has no wind-up; a planted brace is visible; an attentive attacker waits ≤ 4 s then strikes a defender who is defenceless for 56 s. Rational defenders brace in roughly the last ~0.6 s — a de-facto timing window touching the "not a reflex game" anti-pillar. Options: brace-timing telemetry on AC-ASN-33; NPC-like first 0.5 s of brace pose (scope); cooldown toward 45 s; reframe defender fantasy as deterrence. Creative call.

⚠️ **D-W2 — Rushing at a run is unmodelled** (objectives-chain F1, SCL, Crowd). Run cuts chain time 4-6× (50-91 s vs 307-535 s); a ~13 s court-to-court run proves "player" but preliminary courts are ≥ 50 m from enemies, and first-through gets the final alone. Options: model witness probability + telemetry; cap runs to NPC bout length (scope); lower run speed within 400-500; measure at first 10-player test. Creative call.

⚠️ **D-W3 — Camping the final candidates is the trailing team's best answer and the climax depends on it** (objectives-chain SP-2/SP-4/F1/F4/OQ5/OQ9, Penalty, Generator OQ5, concept core loop). Five campers cover 83 % of finals; two free wrong strikes give ~0.91 chance of hitting a rival tender; rival signal (mean delay ~112 s) rarely changes the outcome; killing becomes the only lever, against Pillar 4's "most rounds end by objective". Options: gate the first 5v5 test on OC OQ9/Generator OQ5; 4-zone layouts default; > 6 final candidates; only a preliminary-completer may tend the final (scope); multi-fixture final court (scope); reword the concept convergence line. Creative call. Linked pending: rival signal ON; NpcCount 40→50.

⚠️ **D-W4 — Small-team playtests under-measure Pillar 4 risks** (concept MVP hypothesis, Round R3/F7). At 2v2 campers cover 2 of 6 finals, rival share among non-allies is 0.048 vs 0.111, elimination takes 2 kills. Recommendation: log team size in pacing/stall metrics; count Pillar 4 validation only from 4v4+ or per bucket.

---

### Cross-System Scenario Issues

Scenarios walked: 5
1. S1 Reversal at the final court (elimination win and the tender's Complete in the same tick)
2. S2 Third wrong strike from inside a bush → exile → informant takeover → Crowd replacement
3. S3 (swapped — an informant can never be in a bush: IIF R7, PEN-R12, CNB-R11, CON) (a) concealed Active player killed by a blind bush strike through the countdown; (b) informant killed by an attacker at tally 1
4. S4 Disconnect/rejoin of a concealed player and of a player mid-brace / mid-tend
5. S5 Ended → Reset → Preparing → Active with a late joiner

#### Blockers

🔴 **S-B1 — S1 — Round, Crowd, IIF, Objectives, HUD.** Round R9 stages the round-end *publication* to the deciding strike's death write (ASN-R23) but performs the crowd halt, human-pawn stops (R9; Crowd line 456, AC-CNB-22) and hold releases (IIF line 421) in the latch step. Every viewer sees the whole city brake up to 0.40 s (0.75 s from a bush) before the deciding blow lands — a presentation ASN-R23 forbids — and the winning tender sees the ring cancelled ("Illegal") before the win. Whether the score increment is held with the publication is unstated. Owner: Round R9 (move halt, stops, hold releases and public-state change into the staged step). Not unambiguous.

🔴 **S-B2 — S1 — Roster, Teammate Markers, HUD.** `EndRoundAll` (Roster `RoundEnded`) is written in the latch slot while the replicated phase is still `Active` (publication staged). TM-R5 (teammate-markers.md line 81): outside `Ended`, "`RoundEnded` produces no marker" → every ally marker on both teams flickers out before contact and reappears; HUD's `ActiveAlive` viewer state is undefined in that window (hud.md line 129). Owners: TM-R5 and HUD viewer table (ignore `RoundEnded` until `Phase = Ended`), or Round stages `EndRoundAll` with S-B1. Not unambiguous.

🔴 **S-B3 — S4 — NPC-Kill Penalty.** A disconnected-but-Active player (AI-driven pawn) assassinated during grace: the state table (line 158) puts them in Awaiting with a timer; at `T_due` PEN-R9.1 voids the timer because they are not Connected; reclaim is defined only from Absent (line 169) → a returner after `T_due` can be stranded with no pawn and a black view. Owner: Penalty (a cause on a disconnected participant goes to Absent, keeps `t_lost`, schedules no timer). Not unambiguous.

🔴 **S-B4 — S5 — Round, Concealment, Crowd.** A client still loading at `ActiveAtServerTime` keeps its loading screen until it holds `ExpectedPawnCount` pawns (round-match-framework.md line 455, R7); once Active, concealed pawns are culled for every viewer and kills leave vacancies, so the count may never be reached — unbounded loading screen. Owner: Round (release on the existing acknowledgement, as Session test 48 does for rejoiners, or a time bound). Not unambiguous; a world-ready stage would be the already-pending Round CR-13(d) (schema).

#### Warnings

⚠️ **S-W1 — S2 — Penalty, TDC.** W-NKP-5 (`ExileCardTimeoutSec ≥ ExileBeatSec + 0.5`) ignores strike contact delay (≤ 1.2 s): `ExileBeatSec` 1.2 + timeout 1.7 passes, but a bush strike's card is due at 1.95 s → screen goes black before the card, breaking PEN-R16. Owner Penalty W-NKP-5: require `ExileCardTimeoutSec ≥ StrikeContactSec + BushStrikeEmergeSec + ExileBeatSec + 0.3`.

⚠️ **S-W2 — S3a — Concealment, Camera.** Undefined value of owner/team `IsConcealed` between the kill and the death write for a player killed in a bush (CON-R9 only says the victim leaves occupancy). If it clears at the kill, the victim's camera pulls out of the leaves before contact (ASN-R6) and CON-R6b rule 3 could un-hide the body for allies. Owner Concealment: both flags stay true until body removal/retirement; Camera holds that input to the death presentation.

#### Info

ℹ️ S-I1 — S1 — Objectives: a final Complete rejected by the latch is not logged → objective-win share slightly under-counted (OC F5).
ℹ️ S-I3 — S2 — CPA, Crowd: CPA line 273 "replaced by a same-look NPC" vs CNB-R9.2 uniform draw over vacant cards; CPA should cite `DrawReplacement()` (unambiguous text fix).
ℹ️ S-I4 — S4 — Objectives, Roster: OC voids a final completion while `Unresponsive`; Round's "Connected" includes `Unresponsive`; consistent outcome, but OC-10 should name the connection state it reads.

---

### GDDs Flagged for Revision

| GDD | Reason | Type | Priority |
|-----|--------|------|----------|
| round-match-framework.md | S-B1 (R9 staging covers publication only), S-B4 (unbounded loading screen); C-W12, C-W11, C-W13; D-W4 | Scenario / Consistency | Blocking |
| npc-kill-penalty-informants.md | S-B3 (disconnected victim stranded); S-W1; C-W2, C-W5, C-W7, C-W9 | Scenario / Consistency | Blocking |
| teammate-markers.md | S-B2 (`RoundEnded` before `Ended` hides markers) | Scenario | Blocking |
| hud.md | S-B2 (`ActiveAlive` precondition); C-W3, C-W14 | Scenario / Consistency | Blocking |
| objectives-chain.md | C-B2 (I-OC3 warn vs reject); D-W2, D-W3; S-I1, S-I4 | Consistency / Design Theory | Blocking |
| assassination-defence-reversal.md | D-W1 (brace dynamics); C-W4 | Design Theory / Consistency | Warning |
| crowd-npc-behaviour.md | S-B1 consumer (line 456, AC-CNB-22); C-W1 | Scenario / Consistency | Warning |
| input-interaction-framework.md | S-B1 consumer (line 421); C-W12, C-W14, C-W17 | Scenario / Consistency | Warning |
| concealment-bushes.md | S-W2; C-W7, C-W10, C-W20 | Scenario / Consistency | Warning |
| camera-system.md | S-W2; C-W17, C-W21 | Scenario / Consistency | Warning |
| shared-character-locomotion.md | C-W19; D-W2 | Consistency / Design Theory | Warning |
| procedural-city-generator.md | C-W22, C-W16; D-W3 | Consistency / Design Theory | Warning |
| level-socket-contract.md | C-W8, C-W18, C-W21; D-W3 | Consistency / Design Theory | Warning |
| information-visibility-policy.md | C-W5, C-W18, C-W21 | Consistency | Warning |
| tuning-data-config.md | C-W16, C-W13; mirrors for C-W1, C-W2, C-W22, S-W1 if accepted | Consistency | Warning |
| character-pool-appearance.md | C-W15; S-I3 | Consistency | Warning |
| participant-state-roster.md | C-W6 | Consistency | Warning |
| session-travel.md | C-W11 | Consistency | Warning |
| game-concept.md | D-W3 convergence line; D-W1 "test suspects" dynamic | Design Theory | Warning |

---

### Verdict: FAIL

FAIL: blocking issues remain that must be resolved before architecture begins (C-B2, S-B1, S-B2, S-B3, S-B4). Two blocking consistency findings (C-B1, C-B3) were fixed in this run.

### If FAIL — required actions before re-running

1. **objectives-chain.md** — decide I-OC3's failure mode (recommended: reject) and align AC-OC-32 with TDC line 261 (C-B2).
2. **round-match-framework.md** — R9: stage the crowd halt, human-pawn stops, hold releases and public-state change with the round-end publication when the deciding item is a strike; state whether the score increment is staged (S-B1). Then align Crowd line 456 / AC-CNB-22 and IIF line 421.
3. **teammate-markers.md + hud.md** (or Round, as part of S-B1) — define marker/HUD behaviour for `RoundEnded` received while the phase is still `Active` (S-B2).
4. **npc-kill-penalty-informants.md** — define the path for a disconnected Active participant killed during grace (Absent, keep `t_lost`, no timer) (S-B3).
5. **round-match-framework.md** — bound the late loader's loading screen (ack-based or time-based) (S-B4).

Re-run: `/review-all-gdds since-last-review` (or `consistency`) after the fixes; the per-GDD independent re-reviews (Re-review policy) still apply to the flagged GDDs.

### Fixes applied this run (autonomous, logged in production/session-logs/decision-log.md)

| Finding | File | Change |
|---|---|---|
| C-B1 | objectives-chain.md | `RivalSignalDelayMaxSec` row: "clamp" → "reject" + CR-6 note; AC-OC-22 last two cases → preset rejected; received-table row "TDC CR 12" added |
| C-B3 | npc-kill-penalty-informants.md | PEN-R15: `bExiled` rides `FStrikeResult` with the causing `ClientCommandSeq` (ASN-R19); Roster state change carries no command sequence |

### Notes for /create-architecture

- ADR-001 (crowd identity / C-TeamView carrier) must define the carrier for owner-only `T_due` (C-W5, pending) and state that feature team sub-structs ride C-TeamView (C-W6).
- The round-end staging model (S-B1/S-B2) is an architecture-level ordering contract between Round, ASN's pending-write queue, Roster, Crowd, IIF, TMK and HUD; resolve it before the replication/ordering ADR.
- Tuning validator: invariants reject or warn, never clamp (CR-6); several owner safe ranges currently produce rejected presets (C-W2, C-W17, C-W18, C-W19, C-W22) — the validator ADR should include a "safe range must satisfy all invariants at defaults of the other keys" lint.
- Playtest instrumentation from D-W1..D-W4 (brace timing, run share, final-camping, team-size bucketing) belongs in the telemetry hooks even before the Telemetry GDD (Alpha).

---

### Re-check after fixes (2026-10-24)

Fix pass run after this report (autonomous; one creative-director + systems-designer consultation for S-B1..S-B4; rulings in production/session-logs/decision-log.md under `review-all-gdds-fixes`). Every touched GDD carries a "Cross-review 2026-10-24" row in its "Change requests received" table. IDs unchanged except new ACs (Round 12c, 12d, 30c; AC-NKP-48).

#### Blockers — all resolved

| Finding | Resolution | Where |
|---|---|---|
| C-B2 | I-OC3 **rejects** (TDC wins); AC-OC-32 expects a rejected preset (0.5 + 0.25 < 0.8) and keeps the hard-range clamp case | objectives-chain.md `TendCueDelaySec` row, AC-OC-32 |
| S-B1 | **One round-end ordering contract, two server steps.** Latch step (in the slot, server-only): `Terminated`, queue closed, later commands/AI/timer items `Rejected_Latched` with their `NotNow` replies held, holds frozen (cannot complete, not cancelled), locomotion untouched. Round-end step = the deciding strike's death write (or the latch step for any non-strike win): phase `Ended` + `FRoundResult` + **`MatchScore` increment (staged)**, `EndRoundAll`, `OnRoundEnded`/`PostRound`, Crowd Halt and stop-and-blend of every NPC, AI- and human-driven pawn, frozen holds cancel, held replies sent, `EndedMinDwellSec`/`EndedAutoResetSec` start. Earlier strikes' death writes still play | round-match-framework.md R9, R15, transitions table, Game Feel, Edge Cases, AC 12a-12c; crowd-npc-behaviour.md CNB-R12, Game Feel, AC-CNB-22; input-interaction-framework.md Edge Cases (shared final site; phase leaves Active); information-visibility-policy.md R2.1, L22, viewer-class text, AC-IVP-20 |
| S-B2 | Both: `EndRoundAll` moved into the round-end step with a forced net update on written records, **and** TM-R5 / HUD treat a `RoundEnded` arriving before `Phase = Ended` as no change (different actors, unspecified arrival order) | round-match-framework.md R9, AC 12d; teammate-markers.md TM-R5, AC-TMK-10; hud.md States `Ended` row, Edge Cases; participant-state-roster.md `EndRoundAll`; npc-kill-penalty-informants.md state table (`OnRosterRoundEnded` at the round-end step) |
| S-B3 | A removal cause on a participant whose Roster `Connection = Disconnected` goes to **Absent**, `t_lost` = slot, no timer (`Unresponsive` counts as connected); reclaim from Absent covers return inside or after grace; the later grace expiry is a logged no-op; PEN-R9.1 uses the same connection test | npc-kill-penalty-informants.md PEN-R8, PEN-R9.1, state table, Edge Cases, AC-NKP-48; round-match-framework.md R17.2; participant-state-roster.md disconnect edge case |
| S-B4 | A client lifts its loading screen at `ActiveAtServerTime` once it holds `ExpectedPawnCount` pawns **or** `RevealPawnWaitMaxSec` (new Round key, 2.0 s, All, latch Round) has passed, provided it holds its own pawn or its own state is no longer `Active`; always on `Phase = Ended`; timed from `Active` arrival if the clock is unsynced; late pawns are kind-blind. The acknowledgement is already complete (P5), as for rejoiners (Session test 48). No new replicated stage (CR-13 (d) stays pending) | round-match-framework.md Player Fantasy anchor 1, P7, R7, Edge Cases, Tuning Knobs, AC 30c; tuning-data-config.md Round row (17 keys); design/registry/entities.yaml |

#### Warnings and info addressed

| Finding | Resolution |
|---|---|
| S-W1 | W-NKP-5 is now `ExileCardTimeoutSec ≥ StrikeContactSec + BushStrikeEmergeSec + ExileBeatSec + 0.3` (1.85 s at defaults) in Penalty's `ExileCardTimeoutSec` row and TDC |
| S-W2 | CON-R9: a victim killed in a bush keeps Owner/Team `IsConcealed` true until body removal; Camera CS-R12.1 holds the own `IsConcealed` input into the death presentation |
| S-I3 | CPA Edge Cases cite the uniform `DrawReplacement()` draw (no "same-look NPC") |
| S-I4 | OC-10 (c) reads Session's connection state `Connected` (not the Roster's collapsed value) |
| C-W2 | Penalty `ExileBeatSec` safe 0.75-1.2, bounded by I-HUD-3/I-HUD-5; TDC note; registry revised |
| C-W3 | HUD `HudRemovalCardDelaySec` row: I-HUD-3 rejects |
| C-W4 | UI-ASN-1 uses the four-field `FStrikeResult` |
| C-W6 | Roster R6 notes that feature team sub-structs ride C-TeamView (IVP R4) |
| C-W8 | LSC outgoing request 8 marked superseded; AC-53 uses `PickRespawnPawn(Team, RequestSeq, ExcludePawn = null)` |
| C-W9 | Penalty index note corrected (CR-14 applied to row 14) |
| C-W10 | Concealment IIF row marks `FActionState.StartServerTime` pending (CR 10(b)) |
| C-W11 | Stale "not in the index / not written / provisional" claims corrected in Session, IIF, CPA, Round, Roster, IVP, LSC, SCL, Camera, Crowd, Generator and Penalty (dependency tables, intros and Cross-References statuses; historical outgoing-request lists in IVP/LSC and Session OQ12 left as written) |
| C-W13 (GDD rows) | Downstream rows added: Roster→Camera, Round→Camera and Teammate Markers, Session→Teammate Markers, Penalty→Camera, SCL→Generator |
| C-W15 | CPA uses `CrowdReplacementMinDelaySec`/`CrowdReplacementMaxDelaySec` |
| C-W16 | TDC counts: Session 13, SCL 51, Crowd 50, Generator 32 (preamble form accepted) |
| C-W18 | IVP `TeamPositionFeedHz` safe 1.5-4; LSC `EmergenceWitnessCellsMaxCount` safe 30-40, `AmbientEmergencePerSocketMinPerMin` safe 0.4-0.55; registry revised |
| C-W20 | Concealment Game Feel: crossing ≥ 0.30 s |

#### Still open (no blocker)

- Design calls: C-W1 (Crowd decoy Max < `ObjectiveInteractSec` invariant), C-W7 (Penalty↔Concealment direction), C-W12 (IIF↔Round, Round↔Crowd peer labelling; Round downstream row still names Crowd/SCL), C-W13 TDC TMK Soft/Hard and index row 12 Objectives, C-W17 (I-CS-1 clamp text, IIF lockout range), C-W19 (I-SCL-5), C-W22 (I-PCG-5), Crowd row 11 still lacks Camera.
- Always-ask, already in active.md pending: C-W5 (`T_due` carrier), C-W14 (duplicate latency key), C-W21 (TDC CR 10 renames); Round CR-13 (d) world-ready stage.
- Design theory D-W1..D-W4 (creative calls; scope options pending); S-I1 (latched final Complete not logged).

#### Updated verdict: CONCERNS

All blocking findings (C-B1..C-B3, S-B1..S-B4) are resolved; warnings remain. Systems-index rows 6, 14, 15, 17 and 18 returned from Needs Revision to In Review. The per-GDD re-reviews (Re-review policy) still apply, and `/review-all-gdds since-last-review` should confirm the R9 two-step contract against Assassination, Objectives and HUD before the replication/ordering ADR.

---

## Appendix A — Phase 2 consistency findings (detail)

> C-B1 and C-B3 below were FIXED after this pass (see 'Fixes applied this run'). Line numbers are as read before the fixes.

### /review-all-gdds full — Phase 2 (Cross-GDD Consistency) — 2026-10-24

Status: COMPLETE, PARTIAL COVERAGE (turn-limited run; the uncovered areas are listed per item below. Nothing that was not read is reported as clean.)

#### Coverage per checklist item

- **2a Dependency bidirectionality.** I checked these against systems-index rows 1-30 (lines 45-76): the Dependencies sections of ASN (308-336), Penalty (339-371), Objectives (288-319) and HUD (392-440), the Roster Interactions table (279-297), and the Concealment dependency rows (grep 261-280). **Not read:** the Dependencies sections of TDC, IIF, CPA, Session, Round, IVP, LSC, SCL, Camera, Crowd, Generator and TMK. I only spot-checked these through the reconciliation-table claims of the GDDs above.
- **2b Rule contradictions.** Read: Roster R1-R12, its states and transition table, and I1-I7 (39-298). Also read: ASN-R17-R22, Round phase table (207-223), PEN-R8 and PEN-R15, and the locked defaults in TDC, ASN, Penalty and Objectives. Grepped Round phase names across all GDDs; no unknown phase name was found. **Not read:** the edge-case sections. I did not cross-check death, disconnect or round-end timing beyond the ASN/Roster/Penalty text quoted below.
- **2c Stale references.** Read the Cross-References and reconciliation tables of ASN (462-531), Objectives (376-415), LSC (662-721) and HUD (CR list 423-440). I grepped struct shapes across all GDDs: `FStrikeResult`, `FDefenceResult`, `FDeathState` and `FActionState`. I also grepped `PickRespawnPawn`, `T_due`, `FObjectiveTeamView`, C-TeamView and the `Hide` action name. **Not read:** the Cross-References tables of TDC, Roster, IIF, CPA, Session, Round, IVP, SCL, Camera, Crowd, CON, Penalty, Generator and TMK.
- **2d Knob ownership.** Read: TDC Canonical Tunables, the Registered Owner Keys table, and the full owner-invariant list (76-123, 208-277). Read in full: the Tuning Knobs tables of ASN, Penalty, Objectives, HUD, Round and Session. Grepped individual keys for Crowd, Camera, LSC, IVP and TMK. Key counts match TDC for ASN (16), HUD (21) and Penalty (5). Locked defaults are identical everywhere I checked: 200 cm, 4.0 s, 60 s, L = 3, 3.0 s and 40 NPCs. **Not verified:** the key counts for IIF (26), SCL (49), Camera (48), Crowd (47), LSC (~78) and Generator (30).
- **2e Formula compatibility.** Checked I-ASN-5w against Crowd's replacement delay; W-NKP-2 and W-NKP-4; I-HUD-3, -5 and -6 against Penalty and ASN ranges; I-OC1 and I-OC3 against the Crowd decoy keys; walk 140 / run 450 cm/s in SCL, ASN, Penalty and Generator; and `TravelLoadTimeoutSec` / `LayoutTotalBudgetSec` in Session, LSC and Round. **Not done:** the LSC D6 to Crowd and Generator area chain, and IVP relevancy against TMK and HUD.
- **2f Acceptance criteria.** Only the Objectives ACs were checked against TDC invariants (AC-OC-11, -22, -32). The ACs of the other GDDs were not cross-checked.

#### Blocking

**C-B1. I-OC2 is a "clamp" in Objectives but a rejection in TDC.**
- Objectives Tuning Knobs, line 329, `RivalSignalDelayMaxSec`: "warn below 163 s, clamp Max up to give 100 s; and `Max ≥ Min + 30` (clamp)".
- AC-OC-22, line 450: "THEN Max is clamped up so `W_eff` = 100 … THEN Max is clamped to 150".
- TDC line 260 I-OC2: "reject / warn (TDC never repairs a value … applied as a rejection; reconciliation ruling)". TDC line 479 says it was "DECLINED as a clamp … change request 12 back to Objectives", and line 424 says "restate OC Tuning Knobs accordingly".
- Objectives' received table (lines 400-415) has no row for TDC CR 12.

Why it matters: AC-OC-22 and TDC's preset validation cannot both pass. Owner: TDC (CR-6 enforcement rule, plus the logged reconciliation ruling).

Fix in objectives-chain.md:
- Line 329: replace both "clamp" phrases with "reject below 100 s (TDC CR-6)" and "`Max ≥ Min + 30` (reject)".
- AC-OC-22: change the last two GIVENs to "THEN the preset is rejected and the CR-6 fallback applies".
- Add a received-table row "TDC CR 12 | I-OC2 reject, not clamp | APPLIED".

Fix unambiguous: yes. Always-ask: no.

**C-B2. I-OC3 failure mode: Objectives says warn, TDC says reject.**
- AC-OC-32, line 461: "GIVEN `TendCueDelaySec` 0.4 with Crowd's `CrowdDecoyEarlyReleaseMinSec` 0.8, THEN I-OC3 warns".
- TDC line 261 I-OC3: "… | reject".
- The Objectives knob row (line 331) only names the invariant.

Why it matters: the AC and TDC's owner-invariant test cannot both pass. A warn-only I-OC3 also lets the P1 probe-release tell ship in a preset.

Owner: Objectives owns the meaning, but its own GDD never states the failure mode, so the owner is ambiguous. Fix: decide reject (recommended, matching TDC and the P1 rationale in line 331), then change AC-OC-32 to "THEN the preset is rejected". Also note that the value 0.4 is first clamped to the hard minimum 0.5, which still fails (0.75 < 0.8).

Fix unambiguous: no. Always-ask: no.

**C-B3. PEN-R15 depends on a Roster field that does not exist.**
- Penalty line 137, PEN-R15: "the exile flag (`bExiled`, plus the Roster's own Owner-audience state change carrying the causing command's `ClientCommandSeq` so HUD can gate the message on the plate)".
- Roster R6 (line 107): the C-TeamView owner part is exactly `{PublicHandle, Team, State, RoundEpoch}`. Roster R12 events carry no command sequence.
- HUD (line 404) actually reads "`bExiled` + causing `ClientCommandSeq`" from `FStrikeResult {ClientCommandSeq, Result, TallyAfter, bExiled}` (ASN-R19).

Owner: Roster for the carrier, ASN for `FStrikeResult`. Minimal fix: in PEN-R15, replace "plus the Roster's own Owner-audience state change carrying the causing command's `ClientCommandSeq`" with "carried in the same `FStrikeResult` as the causing `ClientCommandSeq` (ASN-R19)". This needs no new field.

Fix unambiguous: yes. Always-ask: no. Linked pending item: OQ-NKP-1 (`bExiled` is provisional).

#### Warnings

**C-W1. The decoy early-release maximum must stay below the hold time, but no invariant enforces it.**
- Crowd line 411: `CrowdDecoyEarlyReleaseMaxSec` defaults to 2.8 (safe 2.2-3.0, hard 1.0-5.0), and the row says "Max < `ObjectiveInteractSec` … Max ≥ D: early releases become completions".
- TDC's I-CNB-1..5 (lines 236-240) and I-OC1..3 do not include this constraint.
- `ObjectiveInteractSec` has safe 2.5-4.5 (TDC line 90; Objectives line 325). Two values that are each inside their safe range (OIS 2.5 with Max 2.8) break the decoy model, which is the P1 red herring.

Owner: Crowd. Fix: add `CrowdDecoyEarlyReleaseMaxSec < ObjectiveInteractSec` (reject) as I-CNB-6 in the Crowd Tuning Knobs and the TDC owner-invariant table.

Fix unambiguous: no (adding an invariant is a rule decision). Always-ask: no.

**C-W2. HUD invariants silently narrow `ExileBeatSec` to 0.75-1.2 s.**
- I-HUD-3 (HUD 473, TDC 268): `HudRemovalCardDelaySec = ExileBeatSec` (reject), and the card delay's hard range is 0.5-1.2.
- I-HUD-5 (HUD 475): `ExileBeatSec ≥ HudResultPlateInSec + 0.5` = 0.75 at defaults.
- Penalty line 381 gives `ExileBeatSec` hard 0-2.0 and safe 0.5-1.2, describes the behaviour at 0, and mentions neither invariant.
- So Penalty-safe values from 0.5 to 0.74 are rejected presets.

Owner: Penalty for the key, HUD for the invariants. Fix: set Penalty safe to 0.75-1.2 and add "bounded by HUD I-HUD-3 and I-HUD-5" to line 381. This also needs a TDC owner-key note.

Fix unambiguous: yes (text alignment to registered invariants). Always-ask: no.

**C-W3. HUD contradicts itself on whether I-HUD-3 warns or rejects.**
- HUD line 459, the `HudRemovalCardDelaySec` row: "Warning I-HUD-3 if ≠ `ExileBeatSec`".
- HUD line 473 and TDC line 268 say I-HUD-3 "rejects (raised from a warning at HUD's design review)".

Fix: change line 459 to "I-HUD-3 rejects the preset if ≠ `ExileBeatSec`". Fix unambiguous: yes. Always-ask: no.

**C-W4. UI-ASN-1 shows a stale two-field `FStrikeResult`.**
- ASN line 450, UI-ASN-1 data column: `FStrikeResult {ClientCommandSeq, Result}`.
- ASN-R19 (line 103), HUD 403 and Penalty 61 all use `{ClientCommandSeq, Result, TallyAfter, bExiled}`.

Owner: ASN-R19. Fix: line 450 uses the four-field shape, or reads "`FStrikeResult.Result` (R19)".

Fix unambiguous: yes. Always-ask: no. Linked pending item: OQ-NKP-1.

**C-W5. The informant countdown end `T_due` has no defined carrier and no IVP fact row.**
- PEN-R15 (line 137) gives `T_due` to the owning connection "(IVP Owner audience)".
- Penalty line 318 says the countdown "resume[s] from C-TeamView's owner push".
- Roster R6 (line 107) does not list `T_due` in the C-TeamView owner part.
- The IVP fact table has no `T_due` or countdown row (grep: 0 matches).
- HUD reads it (HUD 145, 287, HUD-E11).

Owner: Penalty for the data, IVP for the audience row, Roster or ADR-001 for the carrier. Fix: add an IVP R3 row "Own informant respawn due time (`T_due`) | SA N, SI Y, TM N, OP N | Owner | carrier per ADR-001", and name the carrier in PEN-R15.

Fix unambiguous: no. Always-ask: yes (schema_changes: a replicated field on a named carrier).

**C-W6. Roster R6 lists only its own fields as C-TeamView's content, but other systems use C-TeamView as a carrier.**
- Roster line 106-108: C-TeamView "carries: the owner's own `{PublicHandle, Team, State, RoundEpoch}`; one entry per teammate `{PublicHandle, State, PawnRef, IsConcealed}`".
- Objectives line 62 (OC-5): "`FObjectiveTeamView`, carried by C-TeamView, IVP R4".
- IVP lines 72 and 74, and HUD lines 146 and 405, say the same.
- Not a contradiction in data, but implementers will read R6 as a closed schema.

Owner: Roster (carrier) with IVP R4. Fix: append to Roster R6 "Feature team-audience sub-structs ride the same controller view under IVP R4 (MVP: Objectives `FObjectiveTeamView`, OC-5); the Roster owns none of their contents."

Fix unambiguous: yes (documents an existing IVP ruling). Always-ask: no.

**C-W7. Penalty does not list its dependency on Concealment (one-directional).**
- Concealment line 276 lists "NPC-Kill Penalty & Informants (#14) | `IsInBushOrTransition` for takeover eligibility | Hard".
- Penalty's Upstream table (345-355), its Bidirectionality note (371) and systems-index row 14 (line 60) do not list Concealment.

Fix: add a Penalty upstream row "Concealment (Bushes) (12) | `IsInBushOrTransition` (takeover eligibility, via Crowd CNB-R11) | Hard or Soft", and add Concealment to index row 14. Alternatively, if eligibility is read only through Crowd, change Concealment line 276 to Soft or "via Crowd".

Fix unambiguous: no. Always-ask: no.

**C-W8. LSC still shows stale `PickRespawnPawn` signatures.**
- LSC line 688, outgoing CR 8: `PickRespawnPawn(Team, RequestSeq)`.
- LSC line 822, AC: `PickRespawnPawn(Team)`.
- R5b, line 113 and 699, and the Penalty and CPA text use `PickRespawnPawn(Team, RequestSeq, ExcludePawn)`.

Fix: line 822 uses the three-argument form with `ExcludePawn = null`, and line 688 gains "(superseded by Penalty CR-5)". Fix unambiguous: yes. Always-ask: no.

**C-W9. Penalty says the systems-index update was not applied, but it was.**
- Penalty line 357: "adding them [LSC, Crowd, CPA, IIF, Camera] to row 14's Depends On is a change request … not applied here".
- systems-index line 60 (row 14) already lists all five.

Fix: line 357 becomes "applied to row 14 (2026-10-24)". Fix unambiguous: yes. Always-ask: no.

**C-W10. Concealment presents a pending field as if it already exists.**
- Concealment line 262, IIF dependency: "`FActionState` (pose and start server time)".
- IIF, SCL and ASN define `FActionState {Pose, Seq, Variant, ContactServerTime, WarpPoint}`. `StartServerTime` is the pending Concealment CR 10(b).

Fix: line 262 reads "`FActionState` (pose; `StartServerTime` pending CR 10(b), CON-R9 interim)". Fix unambiguous: yes. Always-ask: no. Linked pending item: CON `FActionState.StartServerTime`.

#### Checked and consistent (no finding)

- **ASN keys and invariants.** ASN's 16 keys and their defaults and ranges match TDC (rows 86-88, 115). I-ASN-1..8 and W-ASN-9 have identical text in ASN and TDC.
- **I-ASN-5w.** 9.2 < 10, consistent with Crowd `CrowdReplacementMinDelaySec` 10 (hard 3-30).
- **Penalty keys.** The 5 Penalty keys and W-NKP-2..5 match TDC.
- **Camera fade.** `NoPawnFadeSec` is 0.5 in Camera, Penalty, HUD and TMK.
- **HUD keys and invariants.** HUD has 21 keys, matching TDC; I-HUD-1..7 have the same text.
- **Round and Session keys.** The Round and Session Tuning Knobs agree with TDC rows 104-105 and the I-ST and W-RMF entries.
- **Objectives timing.** `ObjectiveInteractSec` is 3.0 with hard 2.0-6.0 in TDC and Objectives. Crowd's r_N is 0.0057 (interval 10,500) in both Crowd and Objectives.
- **Struct shapes.** `FActionState` has the same five fields in IIF, SCL and ASN. `FStrikeResult` has the same four fields in ASN-R19, HUD and Penalty. `FDefenceResult {Reversed}` is identical. The extra `FDeathState.bDiedConcealed` field in Concealment is the pending item.
- **Round phases.** Lobby, Preparing, Active, Ended and the transient Reset are used consistently.
- **Locomotion speeds.** Walk 140 and run 450 are consistent in SCL, ASN, Penalty and Generator.

#### GDD flags

| GDD | Reason | Priority |
|---|---|---|
| objectives-chain.md | C-B1 (I-OC2 clamp vs reject; missing TDC CR 12 row), C-B2 (I-OC3 warn vs reject) | High |
| npc-kill-penalty-informants.md | C-B3 (PEN-R15 phantom Roster `ClientCommandSeq`), C-W2, C-W5, C-W7, C-W9 | High |
| hud.md | C-W3 (row says warning, invariant rejects); consumer side of C-W5 | Low |
| assassination-defence-reversal.md | C-W4 (UI-ASN-1 stale struct shape) | Low |
| crowd-npc-behaviour.md | C-W1 (unregistered Max < D constraint) | Medium |
| information-visibility-policy.md | C-W5 (no `T_due` fact row) | Medium |
| participant-state-roster.md | C-W6 (C-TeamView content list reads as closed) | Low |
| concealment-bushes.md | C-W7 (dependency direction), C-W10 (pending field stated as present) | Low |
| level-socket-contract.md | C-W8 (stale `PickRespawnPawn` signatures) | Low |
| tuning-data-config.md | C-W1 / C-W2 need owner-invariant table updates if accepted | Low |
| All of the GDDs listed as not read under 2a, 2c, 2d and 2f | Coverage gap: not fully cross-checked in this run (see Coverage) | Re-run recommended |

## Appendix B — Phase 2b consistency coverage-gap findings (detail)

### /review-all-gdds full — Phase 2b (Cross-GDD Consistency, coverage-gap pass) — 2026-10-24

Status: COMPLETE, PARTIAL COVERAGE. No new Blocking findings. 12 new Warnings (C-W11 to C-W22). Findings C-B1..3 and C-W1..10 from Phase 2 are not repeated here.

#### Coverage

- **2a Dependencies.** Read the Dependencies sections of TDC (317-355), Roster (486-510), IIF (463-495), CPA (306-331), Session (426-454), Round (471-506), IVP (370-411), LSC (512-539), SCL (472-505), Camera (505-537), Crowd (348-385), Generator (316-337) and TMK (336-386), plus Penalty downstream (359-369). Each was compared with systems-index rows 1-18 (lines 47-64) and with the counterpart tables.
- **2c Stale references.** Read the Cross-References tables (including outgoing CRs and received tables) of TDC (382-490), Roster (559-611), IIF (601-680), CPA (389-442), Session (504-550), Round (569-631), IVP (462-522), SCL (607-664), Camera (628-686), Crowd (464-523), CON (360-416), PEN (436-502) and Generator (411-462). TMK's received table (388-404) was also read. Rule IDs were grepped: CS-R2/R8/R10, CON-R4a, HUD-R6/R12, OC-6/7/8/14, AC-PSR-40b/42-45, IVP L23/R14 (all exist). Rename targets and retired keys were grepped too. APPLIED rows were spot-verified in their targets: Crowd decoy keys, `CrowdMaxIdleSec`, Round CR-7(b) names and retired waits (Round 77, 533), TDC I-3 and AC-17, SCL `TakeOverCrowdPawn`, and Generator AC-G15.
- **2d Knobs.** Read the full Tuning Knobs tables of IIF, CPA, Session, Roster, IVP, SCL, Camera, Crowd, Generator, TMK and CON. Compared them against TDC Canonical (78-96), Registered Owner Keys (98-123) and owner invariants (208-272). For LSC, only the cross-referenced rows were grepped, so the full LSC table (about 78 keys) was not counted. Six keys were grepped in the registry `entities.yaml`.
- **2e Formula chains.** Checked these chains:
  - SCL braking feeding ASN (lines 167-179) and Crowd `CrowdStopDecel`.
  - SCL walk speed and CON F3d.
  - IVP `ConcealCullDelayMs`/`T_uncull` against the CON crossing keys.
  - IVP `TeamPositionFeedHz` against TMK I-TM-1.
  - LSC emergence keys against Crowd I-CNB-3/4.
  - LSC witness and fairness keys against Generator I-PCG-2/3.
  - Generator budget sum (manifest + decor = cap).
  - LSC V-AREA/V-DENSITY against `NpcCount`.
- **2f ACs.** Grepped the ACs that name other systems' knobs (16 keys across all GDDs). Read TDC AC-17 and AC-18 against I-2/I-3/I-4, and IVP AC-IVP-11/12. Read SCL AC-SCL-20/24. AC-OC-11 is consistent with TDC's hard-range clamp. AC-OC-32 is already C-B2.

#### Blocking

None new.

#### Warnings

**C-W11. Many dependency and status claims are stale against the current index.**
- Claims that "the index does not list X / change request" are now false:
  - Session 435 (TDC); IIF 472-473 (Roster, Round).
  - CPA 313-314, 331 (Roster, Round).
  - Round 473 "lists three upstream", 506; Roster 510 (Crowd, CPA, IIF, LSC).
  - IVP 372, 411; LSC 514 "lists one upstream".
  - SCL 505; Camera 507 "lists no dependents"; Crowd 360-361, 385; Generator 317.
- Index rows 3-17 (lines 49-63) already contain every one of those entries.
- Status labels that still read "not written" or "provisional" for GDDs that are now designed:
  - Session 428 and 514; Session OQ 12 at 664.
  - CPA 317 and 403-405; IIF 606-607 and 615-617 (Roster 591 says `GetLegalityClass` is done).
  - LSC 523 and 531-535; SCL 490, 499 and 617-623.
  - Camera 534 and 640; Round 504 and 589-593.
  - PEN 443, which cites Round R18's removed "no `TakeNpcCard`" sentence (Round 617 says it was applied).

Why it matters: readers will re-raise index change requests that were already applied, and will treat designed contracts as provisional.
- Owner: each listed GDD. The index is the source.
- Fix: replace each claim with "listed in index row N (verified 2026-10-24)", and each status with "Designed".
- Fix unambiguous: yes. Always-ask: no.

**C-W12. Mutual "upstream" cycles are not declared as peer contracts.**
- IIF 473 lists Round as a Hard upstream ("Round must accept it"). Round 484 lists the IIF as a Hard upstream ("Owns the queue-item contract Round accepts"). Index rows 3 and 6 each list the other, and neither GDD lists the other as downstream.
- Round 486 lists Crowd as an upstream that is "Hard at runtime". Round 504 lists "Crowd & NPC Behaviour, Concealment, Shared Character | … | Soft | Not yet written" as downstream. Crowd lists Round as both a Hard upstream (360) and a Hard downstream (373). Index row 6 omits Crowd.

Why it matters: the design order and the hard/soft judgement are ambiguous, and Round contradicts itself on Crowd and SCL.
- Owner: Round (with IIF and Crowd).
- Fix: in Round 504, remove Crowd and SCL (both already appear at 485-486) and set Concealment to "Designed". Label the IIF↔Round and Round↔Crowd pairs "runtime peer contract (contained cycle)" in both GDDs, the same pattern IIF 474 uses for SCL.
- Fix unambiguous: no. Always-ask: no.

**C-W13. Some soft dependencies are not acknowledged by the upstream system, and two other links disagree.**

Missing rows on the upstream side:
- Camera 516-517 reads Roster (owner binding) and Round (phase). Neither Roster 492-508 nor Round 488-504 lists Camera.
- TMK 351-352 reads Round (`FRoundPublicState` phase) and Session. Neither Round's nor Session's downstream table lists TMK.
- Camera 521 reads Penalty's `ExileBeatSec`/`ExileCardTimeoutSec` and its informant state, but Penalty's downstream table (361-369) omits Camera.
- Generator 326 takes capsule, step and slope values from SCL (applied there as SCL 656, R17.1), but SCL's downstream table (487-503) omits the Generator.

Other mismatches:
- TDC 347 rates TMK as Soft; TMK 348 says Hard.
- Index row 12 (line 58) lacks Objectives Chain, although CON CR 8 (line 386) asks for "Objectives Chain as a hard P4 peer" and OC implements SP-8/AC-CON-24.

Owners: Roster, Round, Session, Penalty, SCL, TDC, and the index.

Fix:
- Add a downstream row in each upstream GDD, Soft, with the interface quoted above.
- Change TDC 347 to Hard. This matches TMK, because the markers cannot run without their keys.
- Index row 12: add "Objectives Chain (soft peer)".

Fix unambiguous: yes for the GDD rows; no for TDC 347 and index row 12. Always-ask: no.

**C-W14. One quantity has two registered keys with two owners.**
- IIF 537: `LatencyWarnRttSec` 0.20, hard 0.1-0.4, safe 0.15-0.25, All, "RTT above which the owner-only latency icon shows (F5)".
- HUD 468: `HudLatencyWarnRttSec` 0.20, hard 0.10-0.50, safe 0.15-0.30, "IIF registered no key; HUD owns it". That statement is false.
- TDC 107 counts IIF's key among IIF's 26 keys, and TDC 120 gives the quantity to HUD.
- `entities.yaml` has both keys active (lines 2570 and 2826).
- IVP 354 and AC-IVP-12 cite the IIF name.

Why it matters: this breaks CR-15.3. A preset can make the icon threshold and the IIF/IVP threshold diverge.
- Owner: TDC CR-15.3 decides which key survives.
- Fix: keep one key and retire the other in its GDD, in TDC and in the registry.
  - Recommendation: keep HUD's key, because HUD presents the icon. Then point IIF F5/428, IVP 354 and AC-IVP-12 at it.
  - Delete "IIF registered no key" from HUD 468.
- Fix unambiguous: no. Always-ask: yes (schema_changes: deletes a registered key).

**C-W15. CPA cites a Crowd key name that does not exist.**
- CPA 349: "`ReplacementMaxDelaySec` and the replacement delay distribution (Crowd, provisional)".
- Crowd 424 names `CrowdReplacementMinDelaySec` / `CrowdReplacementMaxDelaySec` (10 / 60, F3), and the registry confirms this at line 2041.
- Owner: Crowd.
- Fix: CPA 349 should read "`CrowdReplacementMinDelaySec`/`CrowdReplacementMaxDelaySec` (Crowd F3)", without "provisional".
- Fix unambiguous: yes. Always-ask: no.

**C-W16. TDC's registered key counts are stale.**

TDC 100 says the owner table is authoritative. Counted rows against TDC's figures:

| Owner | Owner table | TDC says |
|---|---|---|
| Session (456-474; Session's own CR 3 at line 519 says "13 keys") | 13 | 12 (lines 105, 351, 487) |
| SCL (513-563) | 51 | 49 (line 110) |
| Crowd (391-427, including `NpcCount`) | 50 | 47 (line 113) |
| Generator (344-372) | 32 | 30 (line 118) |

The counts for IIF (26), Camera (48), CPA (3), IVP (4), Roster (2), CON (11) and TMK (7) match.

The Generator's table also gives Owner, Audience and Latch only in its preamble (line 340). TDC 109 accepts that form for LSC only.
- Owner: the owner tables are authoritative.
- Fix: TDC 105 and 351 → 13, 110 → 51, 113 → 50, 118 → 30 → 32. Add "preamble form accepted" to the Generator row at line 118.
- Fix unambiguous: yes. Always-ask: no.

**C-W17. I-CS-1's failure mode differs between Camera and TDC, and IIF's safe range can produce a rejected preset.**
- Camera 581, I-CS-1: "`CameraToggleBlendSec` ≤ `CameraToggleLockoutSec` − 1 frame. The lint enforces it, and the runtime also clamps the blend". Camera 570 says "Must be < `CameraToggleLockoutSec`".
- TDC 235: I-CS-1..5 "reject" (the CR-6 ruling: TDC never repairs a value).
- IIF 538: `CameraToggleLockoutSec` safe 0.2-0.5. At Camera's default blend of 0.20, any lockout below 0.217 is rejected, so IIF's safe minimum is a rejected preset.
- Owners: Camera for I-CS-1; IIF for the range.
- Fix:
  - Camera 581: replace the clamp clause with "TDC rejects the preset (CR-6)".
  - Camera 570: change to "≤ `CameraToggleLockoutSec` − 1 frame (I-CS-1)".
  - IIF 538: safe 0.25-0.5, and add "bounded below by Camera I-CS-1".
- Fix unambiguous: no (it changes IIF's range). Always-ask: no.

**C-W18. Some safe-range values are rejected by registered invariants, and the non-owner rows do not say so.** (Same pattern as C-W2.)
- **IVP 420.** `TeamPositionFeedHz` has safe range 1-4. I-TM-1 (TDC 266) requires `TeammateMarkerFeedStaleSec` ≥ 3/Hz, and TMK's default of 2.0 s (TMK 417) rejects 1.0-1.49 Hz.
- **LSC 563.** `AmbientEmergencePerSocketMinPerMin` has safe range 0.4-1.0. I-CNB-3 (TDC 238) caps it at (60/80)·(1−0.25) = 0.5625 with Crowd's defaults, so values from 0.57 to 1.0 are rejected.
- **LSC 557.** `EmergenceWitnessCellsMaxCount` has safe range 25-40. I-PCG-2 requires it to be at least `CityGenWitnessCellsTargetCount` (default 30), so 25-29 are rejected.
- Owners: the invariant owners (TMK, Crowd, Generator); the rows to fix are in IVP and LSC.
- Fix: IVP 420 safe 1.5-4 with "bounded by I-TM-1"; LSC 563 safe 0.4-0.55 with "bounded by I-CNB-3"; LSC 557 safe 30-40 with "bounded by I-PCG-2".
- Fix unambiguous: yes. Always-ask: no.
- Linked pending item: TMK OQ14 / CR-11c. I-TM-1 rejects presets even while F5 is inactive (TMK 417).

**C-W19. I-SCL-5 reads a value no key holds, and it rejects part of SCL's own safe range.**
- SCL 569, I-SCL-5: "min(Crowd pace set) ≥ `WalkSpeedScaleMinMult`". TDC 233 evaluates it as a preset invariant (reject).
- No Crowd key holds a pace set (Crowd 391-427). The set is SCL's constant {0.8, 0.9, 1.0} (SCL 190).
- As written, values of `WalkSpeedScaleMinMult` (SCL 514; safe 0.75-0.90) above 0.80 are rejected, including the "1.0" case that row describes.
- Owner: SCL.
- Fix: I-SCL-5 becomes "`WalkSpeedScaleMinMult` ≤ 0.8 (lowest member of the R5.1 pace set, a constant)". SCL 514 safe range becomes 0.75-0.80, and the "1.0" sentence is removed.
- Fix unambiguous: no. Always-ask: no.

**C-W20. Concealment's Game Feel quotes a crossing minimum below the hard minimum.**
- CON 342: "crossing ≥ 0.25 s, default 0.35 s".
- CON 296 gives `BushStrikeEmergeSec` hard 0.30-0.6, and TDC 114 says "hard minimum 0.30 s".
- Fix: CON 342 should read "crossing ≥ 0.30 s (hard minimum; ≥ `T_uncull` 0.25 s)".
- Fix unambiguous: yes. Always-ask: no.

**C-W21. TDC's rename requests were never applied or logged by their owners.**
- TDC 122-123 and 422 (change request 10) ask for:
  - `ConcealServerCullEnabled` → `bConcealServerCull` (IVP).
  - `EyeMaskHideFails` → `EyeMaskHideFailCount` (Camera).
  - `StickDeadZone` / `StickOuterZone` → `…Frac` (Camera).
  - `LayoutSliceWorkUnitsMax` → `LayoutSliceWorkUnitsLimit` (LSC).
  - `RespawnPoolMinExpected` → `RespawnPoolMinExpectedCount` (LSC).
- A grep finds none of the new names in IVP, Camera or LSC. Camera 566 and 576 and IVP 422 still use the old names. Neither the IVP received table (502-522) nor Camera's (677-686) has a row for this request.
- TDC says AC-3a fails until the keys are renamed.
- Owners: IVP, Camera, LSC.
- Fix: apply TDC's exact names in each table and in every reference, or log a DECLINED row.
- Fix unambiguous: yes (the names are given). Always-ask: yes (schema_changes: renames data-asset keys).

**C-W22. The Generator's final-eligible target can fall below LSC's minimum, and no invariant prevents it.**
- Generator 354: `CityGenFinalEligibleTargetCount` 6, hard 3-8, safe 4-6, "never below LSC `FinalCandidatesMinCount`"; "6 > `TeamSize` so a full team cannot camp".
- LSC 431 and 709: `FinalCandidatesMinCount` 6, hard 3-8.
- TDC's I-PCG-1..4 (262-265) do not include this constraint.

Why it matters: Generator safe values of 4-5 make every generated layout fail LSC selection. The result is a reseed or fallback storm in `Generated` mode, and the anti-camping rationale is lost.
- Owner: Generator.
- Fix: add I-PCG-5 "`CityGenFinalEligibleTargetCount` ≥ `FinalCandidatesMinCount`" (reject) to Generator 354 and to TDC. Set Generator's safe range to 6-8.
- Fix unambiguous: no. Always-ask: no.

#### Checked and consistent

- SCL walk braking (140·0.8/0.6 = 187) and run braking (450/0.75 = 600) match ASN 167 and Crowd `CrowdStopDecel`. Stop distances (34 / 53 cm) match.
- CON F3d: 60 cm / 0.5 s = 120 ≤ 140 cm/s.
- `T_uncull` 0.25 ≤ both crossing keys. Swap + cull: 0.5 + 0.25 ≤ 0.9.
- TMK: hold + half-fade = contact at 0.40 s; 2/15 Hz = 0.13 s; I-TM-1 passes at defaults.
- Generator: manifest 3,500 + decor 1,500 = 5,000 cap.
- I-CNB-4, I-PCG-3 and I-LSC-2 pass at defaults.
- I-IIF-1/2 sums are 22 and 12. I-ST-1..3, I-CS-1..5, I-TM-1 and I-CNB-1..5 have the same text in owner and TDC (apart from C-W17).
- IIF's consumed defaults match their owners: `AttackAttemptLockoutSec`, `BushReenterCooldownSec` 6, `PawnNetUpdateHz` 15 (12-17), `ObjectiveInteractSec`.
- CON I-CS-4: 70 = 90 − 20, and the Camera hard maximum is 70.
- TDC AC-17 and AC-18 are consistent with I-2, I-3 and I-4. Round's retired waits are marked retired (77, 533).
- Every APPLIED row spot-checked was really applied.

#### GDD flags

| GDD | Reason | Priority |
|---|---|---|
| hud.md / input-interaction-framework.md | C-W14 (duplicate latency key; false "IIF registered no key") | Medium |
| procedural-city-generator.md | C-W22; C-W16 table form | Medium |
| shared-character-locomotion.md | C-W19; C-W11; C-W13 (no Generator row) | Medium |
| round-match-framework.md | C-W12 (self-contradicting Crowd/SCL rows); C-W11; C-W13 | Medium |
| camera-system.md | C-W17; C-W21; C-W11 | Medium |
| information-visibility-policy.md | C-W18; C-W21; C-W11 | Low |
| level-socket-contract.md | C-W18 (two rows); C-W21; C-W11 | Low |
| tuning-data-config.md | C-W16 counts; C-W13 (TMK Soft); C-W22 and C-W14 if accepted | Low |
| character-pool-appearance.md | C-W15; C-W11 | Low |
| concealment-bushes.md | C-W20 | Low |
| input-interaction-framework.md | C-W12; C-W17 range; C-W11 | Low |
| session-travel.md, participant-state-roster.md, npc-kill-penalty-informants.md, crowd-npc-behaviour.md | C-W11; C-W13 missing rows | Low |
| systems-index.md | C-W13 (row 12 lacks Objectives); C-W12 (row 6 lacks Crowd peer) | Low |

#### Still not covered

- **Edge Cases sections** of all GDDs, beyond Phase 2's reads.
- **LSC's full knob table** (about 78 keys: count, defaults, I-LSC-1 D6 symbols). Only the cross-referenced rows were checked.
- **Generator F1 against the LSC V-AREA band at low `NpcCount`.** At `NpcCount` 35 the band is 2,000-2,025 cells, while `CityGenWalkableTolPct` 5 is about ±100 cells. Whether F1 clamps inside the band was not read.
- **IVP feeding HUD formulas** beyond I-TM-1 and the latency key.
- **Rule IDs not confirmed by grep**: CS-R12.2, CS-R11.3, IVP R9a (sub-numbered, not headed). They may exist as sub-items.
- **Registry**: checked for 10 names only. The decision log was not grepped.
- **ACs**: spot-checked only. Full AC-versus-owner-value sweeps of the 13 GDDs were not done.

## Appendix C — Phase 3 design-theory findings (detail)

### /review-all-gdds full — Phase 3: Game Design Holism (2026-10-24)

Inputs read: game-concept.md (whole); active.md pending list; by section: OC (Summary to OQ), ASN (Summary to Game Feel, OQ, AC-ASN-33), PEN (Summary to PEN-R9, Knobs, Game Feel), CON (Summary to F4, OQ), HUD (Summary, Fantasy, R1-R6a, Game Feel, UI inventory, OQ), Crowd (Fantasy, Tell Matrix, run rule, OQ), Round (Summary, Fantasy, R3, F7), TMK and Camera (Summary, Fantasy), SCL (Fantasy, F1 speeds, run knobs), IIF (Fantasy, UI-4), Generator OQ. Player Fantasy only: TDC, Roster, IVP, CPA, LSC, Session, Generator. Registry not needed (GDD values used). **Verdict: 0 Blocking, 4 Warnings.**

#### Checklist

- **3a Progression loop competition**: checked for kill counts, scoreboards, strike tallies, rival progress, informant role. No kill count or per-player score exists (only `MatchScore` in `FRoundPublicState`). The wrong-target tally is a cost, not a goal. The informant role is strictly reduced (walk only, no actions). **PASS.**
- **3b Attention budget**: travel phase: reading the crowd, finding the court from the clue (no waypoint, OC-15), guessing threats for the brace (no warning, ASN-R6) and talking to the team (markers plus external voice) = 4 active. Blending is the default output of the controls (IIF fantasy; walk speed fixed at 1.0 for now, T16). Camera mode is chosen once. Bush use is occasional. HUD lines are glanced at, not managed. Tend phase: 2-3. **PASS, at the limit.** If the in-game ping (pending) is added it must not become a fifth channel to manage. HUD OQ12 (strike-hint noise) is already tracked.
- **3c Dominant strategies**: 12 candidates checked. PASS: bush camping (SP-8, AC-CON-24, no Interact or Prepare in a bush), strike-from-bush at objectives (no bush within 6 m of a court), brace uptime (6.7 %, F3), suicide/respawn loops (none possible), body-blocking (OC-8), 1P vs 3P (pending), rival-signal correlation (F4 LR ≤ 2.4), mirroring NPCs (intended). **WARN:** brace timing (D-W1), running rush (D-W2), final-candidate camping plus spending the strike allowance (D-W3).
- **3d Economy (non-traditional resources)**: sources and sinks mapped for defence (cooldown sink 60 s), strike allowance (L−1 = 2 per player per round), active lives (none come back), informant time (15 s black plus a walk from the home quarter), objective progress (permanent, OC-4) and concealment (no dwell cost). Anti-snowball: one Active player can still finish the whole chain, and a smaller team shows fewer tells. Catch-up: informants, but in MVP only through external voice (linked to the pending informant-ping item). A lead in the race never goes away, so after rival count 2 the trailing team's only lever is killing (part of D-W3). No unrecoverable or undecidable state: a mutual stall is SP-6 plus host abort, which is accepted. **PASS with D-W3.**
- **3e Difficulty curve**: within a round, the shape is quiet preliminaries (secret courts ≥ 50 m from the enemy zone), then the final. The climax is not guaranteed by any mechanic (D-W3). Across a session: no carry-over and a fresh seed each round; start corners are already Generator OQ5. Scaling: `NpcCount` does not scale with team size (Round R3). At 2v2 the base rate p falls from 0.111 to 0.048 and camping covers 2/6 candidates instead of 5/6, so small-team playtests do not show the 5v5 dynamics (D-W4). Nothing scales with elapsed time (SP-3). **PASS with D-W3, D-W4.**
- **3f Pillar alignment**: all 18 Player Fantasies cite at least one pillar by text (foundation GDDs: P1/P2/P4; features: P1-P4). Anti-pillar scan: no enemy-anchored HUD element (HUD-R2/R3); strike hint identical for every kind of target and never shown for concealed pawns (IIF UI-4); markers on allies only (TMK); no round clock (Round, SP-7); cooldown and returning countdowns are owner-only. **No written violation.** One emergent reflex risk: D-W1.
- **3g Fantasy coherence**: the per-system lines form one identity, "the hidden predator who must pay exposure to race". Examples: OC "Three seconds in the light", ASN "certainty bought with exposure", SCL "a perfect disguise until I spend it", IIF "the controls keep you a citizen", CON "Gone, but not safe", PEN "the city stops your hand, not your eyes", HUD "only what is yours to know", TMK "the nod across the plaza". **Two seams:** the defender's "bait" fantasy (D-W1), and the concept's convergence beat compared with OC's "quiet advantage" (D-W3).

#### Findings

##### Blocking
None.

##### D-W1. The brace only works against attackers who are not looking, or as a timing race
- **GDDs:** ASN Player Fantasy ("only pays if someone is already committed"), ASN-R5 (brace checked at Attack execution), Game Feel ("One press, no wind-up input"; "brace before a committed striker is in reach"), F3 (60 s cooldown "used or not"); IIF anchor ("If they keep coming, you were right"); Crowd T4 (player posterior for a brace 0.5-0.7; NPC brace rate 0.025/min); concept Key Dynamics ("test suspects — approach a suspect and watch whether they brace").
- **Reasoning:**
  - The attacker moves last with full information: the strike has no wind-up, and a planted brace is visible before the attacker presses.
  - An attacker who sees a brace can walk on, wait at most 4 s, then strike a target that is defenceless for 56 s and has just shown itself as probably a player.
  - Against an attentive attacker, a brace can only reverse a strike if it starts too late to be seen and answered. The attacker cannot react to a brace that started less than about RTT + `Brace_In` 0.25 s + reaction 0.25 s ≈ 0.6 s before their strike executes (at 100 ms RTT). That is about 15 % of the 4 s window.
  - So rational defenders will brace late, which is a timing window in practice. It is also decided on the attacker's side by reaction speed (seeing the brace and aborting). This is the "parry window" the "NOT a reflex game" anti-pillar exists to prevent.
  - The concept's "test suspects" dynamic turns an early brace into a confession. That makes the defender's "bait" fantasy depend on attacker inattention.
- **Recommendation (options):**
  - (a) Keep the rules, but extend AC-ASN-33 to log, for each reversal, the time from brace start to strike execution, and to count attacks on a defender within 60 s of a brace they did not reverse. If most reversals come from braces started < 0.6 s before execution, the window is real.
  - (b) Make the first ~0.5 s of `Brace_In` share a pose with a common NPC stop or adjust, so a brace cannot be recognised early. This widens the unreadable window by design instead of by latency.
  - (c) Move `DefenceCooldownSec` toward the 45 s end of its safe range, so an early, visible brace costs less.
  - (d) Rewrite the defender fantasy as deterrence and escort play rather than bait.
- **Creative call:** yes. **Always-ask:** (b) yes (scope_changes: new or changed animation asset); (a), (c), (d) no.

##### D-W2. The payoff of rushing at a run is not modelled anywhere
- **GDDs:** OC F1 (`T_rush` 50-91 s against `T_chain` 307-535 s); SCL Tuning (`RunSpeedCmPerSec` 450 against walk 140; "High: run dominates routes"; no limit on how long a run lasts); Crowd run rule (NPC runs are "2-6 s bouts", T1 0.185/min, player posterior 0.5-0.65); OC SP-2 / LSC R9.3 (preliminaries ≥ 50 m from the enemy zone); OC-5 (the team that finishes its preliminaries first learns the final first).
- **Reasoning:**
  - Running cuts chain time by 4-6×. A run from one court to the next (~57 m) lasts ~13 s. That is longer than any NPC run, so it is proof of a player to anyone who sees it.
  - Its cost is only probabilistic: someone on the enemy team (or an enemy informant) has to see it. Preliminary courts are deliberately far from enemies.
  - The speed advantage compounds, because the first team to finish its preliminaries gets the final to itself.
  - The design test for P2 ("advantage must be a visible deviation") is met formally. Whether the deviation is costly enough is untested. If it is not, P4 becomes a speedrun and P2 becomes nominal.
- **Recommendation (options):**
  - (a) Add an F-model of the chance a run is witnessed, plus telemetry: share of travel spent running, and the share of objective wins with duration < 2 × `T_rush`.
  - (b) Cap run bouts at the NPC distribution (≤ 6 s) with a lockout before running again.
  - (c) Lower `RunSpeedCmPerSec` within its safe range of 400-500.
  - (d) Accept, measured at the first 10-player test.
- **Creative call:** yes. **Always-ask:** (b) yes (rule and knob change, scope_changes); (a), (c), (d) no.

##### D-W3. Camping the final candidates is the trailing team's best answer, and the round's climax depends on it
- **GDDs:** OC SP-4 ("a whole team could still post one camper on five of the six candidates"), SP-2 (with 2 corner zones both teams know the final region), OC-6/F3 (`P_full` 0.70), F1 ("about 2 min of final contest", with no mechanism that produces it), F4 (mean signal delay ≈ 112 s, latest 195 s), OC OQ5 / OQ9; PEN `NpcKillStrikeLimit` 3; Generator OQ5; concept Core Loop ("both teams converge on the shared final target where contact is unavoidable").
- **Reasoning:**
  - The team that finishes its preliminaries first tends a final the rivals cannot locate. The final contest happens only through region knowledge and camping.
  - With 5 campers on 6 candidates, 5/6 ≈ 83 % of finals are watched.
  - Within the 3 s tend, a camper within ~5 m can close and strike, and a tender cannot brace (exclusive action slot).
  - Each camper has 2 wrong strikes that do not remove them (L−1). Against tenders seen at a candidate court (P = 0.70 player), 2 strikes give ≈ 0.91 chance of hitting a rival.
  - Rival count 2 arrives on average after the leader's final leg, so it rarely changes anything. The trailing team's only lever is killing, which works against P4 ("killing is how you protect your race") and the validation target `ObjectiveEndShare` > 0.5.
  - The standing-still tell (T14 > 20 s) can be avoided by cycling stall pauses, so it does not make camping expensive.
- **Recommendation (options):**
  - (a) Keep, and treat OC OQ9 / Generator OQ5 as gate items for the first 5v5 playtest (track `ObjectiveEndShare`, F5 `FinalDwell`, and kills within 10 m of final candidates).
  - (b) Make 4-zone layouts the default now, so the final region is not public.
  - (c) Raise `FinalCandidatesMinCount` above 6.
  - (d) Adopt OC OQ5 (only a pawn that completed a preliminary may tend the final).
  - (e) Allow several fixtures in the final court, so the tender chooses where to stand.
  - (f) Reword the concept's convergence line to match the hidden final.
- **Creative call:** yes. **Always-ask:** (d) and (e) yes (rule changes, scope_changes); (a)-(c) and (f) no. **Linked pending items:** RivalPrelimCount ON in MVP; NpcCount 40→50.

##### D-W4. Small-team playtests will under-measure the Pillar 4 risks
- **GDDs:** concept MVP hypothesis ("online 2v2–5v5"); Round R3 (`TeamSize` 1-4 are test values; no `NpcCount` auto-scaling); OC SP-4; Round F7.
- **Reasoning:** camping, blind-strike odds and the elimination path all scale with team size:
  - candidate coverage: 2/6 at 2v2 against 5/6 at 5v5;
  - base rate p: 0.048 against 0.111;
  - eliminating a team: 2 kills against 5.

  A 2v2-heavy MVP playtest will make `ObjectiveEndShare` look healthier than 5v5.
- **Recommendation:** record `TeamSize` in F7 and OC F5. Count P4 validation (`ObjectiveEndShare`, StallDefect, AC-ASN-33 bands) only from rounds of 4v4 or larger. Alternatively, apply the targets per team-size bucket.
- **Creative call:** no. **Always-ask:** no.

#### GDD flag table

| GDD | Reason | Priority |
|---|---|---|
| assassination-defence-reversal | D-W1: brace dynamics and anti-reflex risk; extend AC-ASN-33 | High |
| objectives-chain | D-W2 (rush absent from F1/SP rules), D-W3 (camping, climax, signal timing) | High |
| game-concept | D-W3 convergence line; D-W1 "test suspects" dynamic and defender fantasy | Medium |
| shared-character-locomotion | D-W2: no run-cost model or bout limit | Medium |
| level-socket-contract / procedural-city-generator | D-W3: final region public with 2 zones; `FinalCandidatesMinCount` | Medium |
| crowd-npc-behaviour | D-W2: NPC run bouts 2-6 s make any long player run proof; D-W1 T4 brace posterior | Low |
| npc-kill-penalty-informants | D-W3: the 2-strike allowance makes endgame strikes at the final cheap | Low |
| round-match-framework | D-W4: log TeamSize and filter F7 by team size | Low |
| input-interaction-framework | D-W1: anchor moment assumes an early brace pays | Low |

## Appendix D — Phase 4 scenario walkthroughs (detail)

### /review-all-gdds full — Phase 4 (Cross-System Scenario Walkthrough) — 2026-10-24

Status: COMPLETE. Reviewer: game-designer. Read-only on design files. Known items (C-B1..3, C-W1..17, D-W1..4, active.md pending list) are cited, not re-reported.

**Scenarios walked: 5.**
- S1: Reversal at the final court.
- S2: Third wrong strike from a bush.
- S3: Killed in a bush with countdown, plus an informant kill. The literal "informant in a bush" case is impossible, so substitutes were walked.
- S4: Disconnect and rejoin (concealed / mid-brace / mid-tend).
- S5: Ended -> Reset -> Preparing -> Active with a late joiner.

**Verdict: 4 Blockers, 2 Warnings, 3 Info.**

#### S1. Reversal at the final court (elimination win vs Complete in one tick)
**Chain:**
1. IIF/Round R12: the Attack (intake) and the tender's `HoldTimer(Complete)` (due-time heap) are stamped in a defined order.
2. **Complete first:** OC-10 -> `DeclareObjectiveWin` -> latch. The Attack returns `Rejected_Latched` and its owner gets `NotNow` (IIF 101).
3. **Attack first:** ASN-R5/R10 reversal -> `RequestTransition(A, Informant, ReversedAgainst)` -> `OnPawnKilled(Reversal)`. Penalty counts no strike (PEN-R2.1) and schedules A's respawn. Round R15(a) gives `EliminationWin(V.team)`, and `TryTerminate` runs R9 in the slot. Publication is staged to the death write (R9, ASN-R23). The tender's Complete returns `Rejected_Latched` and its hold ends `Illegal` (IIF 421).
4. HUD F10 shows the banner. A's removal card is suppressed (HUD 78). A's respawn is cancelled (PEN-R10, Roster 436).

The winner is the same team in both orders; only `EndReason` differs.

**S-B1 (Blocker): when a strike decides the round, only the publication is staged; the latch's other visible effects are not.**
- Systems: Round, Crowd, IIF, OC, HUD. Step: 3.
- The rules:
  - Round R9 (line 99) stages the publication ("the latch itself, the queue close, `EndRoundAll` and the score still happen in the slot"). The same R9 makes "the whole crowd hold… in the latch step" and stops human pawns "in the same step".
  - Crowd line 456 and AC-CNB-22 (line 560) say the same.
  - IIF 421 releases the tender's hold `Illegal` at the latch.
- Failure mode:
  - Every viewer, bystanders on honest clients included, sees all pawns start braking up to `T_v` (0.40 s, or 0.75 s from a bush) before the deciding blow. That is an All-audience pre-contact cue, forbidden by ASN-R23 ("Everyone… round-end presentation… staged").
  - The winning tender sees its ring cancel before the win, which is contradictory messaging.
  - It is also unstated whether the slot-time `MatchScore` increment inside `FRoundPublicState` (R20: "Every Round mutation forces a net update") is held back.
- Resolution (owner: Round R9): keep the latch, queue close and `EndRoundAll` (server) in the slot. When the deciding item is a strike, move the Crowd Halt, the human stop-and-blend, the hold releases of non-struck pawns and the `FRoundPublicState` mutation (phase, score, result) into the ASN pending-write step. Then align Crowd 456, AC-CNB-22 and IIF 421.
- Fix unambiguous: no. Always-ask: no.

**S-B2 (Blocker): `RoundEnded` C-TeamView entries arrive while the phase is still `Active`.**
- Systems: Roster, TMK, HUD. Step: 3.
- The rules:
  - `EndRoundAll` runs in the slot. Roster line 140 says "every `Active` or `Informant` record becomes `RoundEnded`", and this replicates at once (R6).
  - TM-R5 (line 81): "Outside `Ended`… `RoundEnded` produces no marker".
- Failure mode:
  - Every ally marker on both teams vanishes for 0.4-0.75 s, then reappears frozen when the staged `Ended` lands. That is a flicker and a pre-contact "round over" cue.
  - HUD-R10 (line 84) already ignores `RoundEnded` for feed lines. But `ActiveAlive` requires `PresentedOwnState = Active` (hud.md 129), so that state is undefined in the window.
- Resolution (owners: TMK TM-R5, HUD viewer table): treat `RoundEnded` received while `Phase = Active` as no change until `Phase = Ended` arrives. Alternative: Round stages the replicated `EndRoundAll` write with S-B1.
- Fix unambiguous: no. Always-ask: no.

**S-I1 (Info): a final completion that loses to the latch is not logged.**
- Systems: OC, Round. Step: 3.
- A Complete that returns `Rejected_Latched` is not logged as `TendCompleted`, so F7/F5 `ObjectiveEndShare` under-counts near-simultaneous finals.
- Resolution (owner: OC F5): log `TendCompleted(NoEffect)` for it.
- Fix unambiguous: no. Always-ask: no.

#### S2. Third wrong strike from inside a bush
**Chain:**
1. IIF R7 / CON-R7: Attack is legal while concealed.
2. CON-R9: the strike breaks concealment (`Emerging`, `BushStrike`, crossing at +0.35 s, contact at +0.75 s).
3. ASN-R8 resolves an NPC kill.
4. PEN-R2 counts strike 3. PEN-R4: `StrikeLimit` plus release in the same Roster call (Roster 290). The card goes `BoundLive -> NpcLive` (CPA 85). `bExiled` is set and `T_due` = slot + 15 s.
5. If P was the last Active player, R15(a) applies, with S-B1 staging.
6. Crowd CNB-R10.4: the body finishes its lockout, then takes `AmbientExit` in the next exit slot. It reports `AlleyExit`, so it is never re-adopted (CNB-R11, `ExcludePawn`).
7. CNB-R9.2: a vacancy is opened, one exit is cancelled, and a replacement arrives 10-60 s later.
8. HUD: the plate shows at +0.75 s and the card at +1.55 s. The card-entrance event releases the Camera view (PEN-R16, HUD-R9).
9. Allies: `State`/`PawnRef` arrive at the slot, the marker is gone before contact (accepted, TM-R6 line 89), and the feed line follows (HUD-R10).
10. At `T_due`: PEN-R9 pick -> `TryReleaseNpc` -> `TakeNpcCard` -> `TakeOverCrowdPawn`. The marker comes back with an outline (TM-R7).

Population and cards balance, and every output is a valid next input.

**S-W1 (Warning): W-NKP-5 ignores the contact delay, so the view can go black before the exile card.**
- Systems: Penalty, TDC, HUD, Camera, Concealment. Step: 8.
- The rules:
  - PEN-R16 (line 141): Camera fades "if no card entrance occurs within `ExileCardTimeoutSec`… of the owner's own `State → Informant`… arriving". The State arrives at the slot.
  - The card is due at contact (`StrikeContactSec` + `BushStrikeEmergeSec`, up to 1.2 s by I-ASN-3) + `ExileBeatSec`.
  - W-NKP-5 (TDC 258) checks only `ExileCardTimeoutSec >= ExileBeatSec + 0.5`.
- Failure mode: `ExileBeatSec` 1.2 with `ExileCardTimeoutSec` 1.7 passes every check, yet a bush strike's card is due at 1.95 s. The view blacks out before the card, contradicting "never black before the card". At defaults the bush-strike margin is only about 0.45 s minus one-way latency.
- Resolution (owner: Penalty W-NKP-5, mirrored in TDC): `ExileCardTimeoutSec >= StrikeContactSec + BushStrikeEmergeSec + ExileBeatSec + 0.3`.
- Fix unambiguous: no. Always-ask: no.

**S-I2 (Info): HUD F2 strike-write matching for an exiled attacker.**
- HUD F2 matches the plate to "the pawn that issued the command", but that pawn is released in the slot, so the client must cache it at press.
- Linked pending item: HUD OQ15 (`ContactServerTime` in `FStrikeResult`, active.md 96). No new finding.

**Covered:**
- CON 242: strike-out happens only after emerging.
- PEN-R5 / Roster 423: the grace for the released body.
- CNB-R9.1: exit rate.
- CON-R9 interim: the killed NPC co-occupant stays culled.
- HUD 78: an exile that ends the round shows no card.

#### S3. Killed in a bush with the countdown; informant killed by an attacker with tally 1
**Substitution:** an informant can never be in a bush. IIF R7 and PEN-R12 forbid Hide; CNB-R11 `Hiding` and CON 241 make bush pawns ineligible for takeover; CON 238 makes grace expiry produce an NPC. This case is covered. I walked two substitutes instead.

**(a) Concealed Active player killed by a blind bush strike:**
1. ASN-R8 rule 2 resolves the kill.
2. CON-R9 removes the victim from occupancy in the slot.
3. Roster sets `Assassinated` and C-TeamView updates.
4. `T_due` = max(slot + 15, fall + fade).
5. The death write lands at +0.60 s. The body stays culled for non-allies (CON-R9 interim).
6. `Dying` -> fade -> countdown. The `T_due` carrier is already C-W5.

**(b) Informant killed by an attacker with tally 1:**
1. `WrongTarget`: tally 1 -> 2, strikes stay at 1 (PEN-R2.2, R3). The danger triangle shows at n ≥ L−1 (accepted, PEN-R14.1).
2. `RetireBinding` runs inline. The card goes `BoundLive -> NpcVacant` (CPA 86), and Crowd replaces it.
3. No removal card is shown; fall, fade, countdown (HUD-R9 line 81, Camera 232).
4. Allies see the outline marker retire with no feed line (HUD-R10). This is consistent: they are entitled to ally state.

**S-W2 (Warning): `IsConcealed` for a pawn killed while concealed is undefined between the slot and the death write.**
- Systems: Concealment, Camera, TMK, ASN. Step: (a)2-5.
- The rules:
  - CON-R9 (line 84) says only that the victim "leaves… the occupancy record" at the slot.
  - Camera reads own `IsConcealed` directly for the 60 cm boom and the vignette (camera-system 64, 219, 272), outside the `OnRoleChangePresented` gate.
  - CON-R6b rule 3 (line 70) clears an ally's body latch "when… `IsConcealed` is false".
- Failure mode:
  - If the flag clears at the slot, the victim's camera leaves the leaves up to 0.75 s before contact. That is a warning ASN-R6 forbids; ASN-R23's catch-all covers it only by inference.
  - Allies could see the dead ally standing in the bush before the death write.
- Resolution (owner: Concealment CON-R2/R9): owner `IsConcealed` and the C-TeamView flag stay true for a killed-concealed pawn until removal or retirement, and rule 3 does not apply to it. Camera lists own `IsConcealed` among the ASN-R23-gated inputs.
- Fix unambiguous: no. Always-ask: no.

**S-I3 (Info): CPA's "same-look" replacement wording is too strong.**
- CPA 273: "replaced by a same-look NPC". CNB-R9.2 draws "uniform over vacant cards", which gives the same look only with one vacancy.
- Fix (owner: Crowd): CPA 273 cites `DrawReplacement()`.
- Fix unambiguous: yes. Always-ask: no.

#### S4. Disconnect and rejoin mid-round
**(a) Concealed Active player:**
1. R17.1/R17.2: AI hold, and the pawn stays concealed (CON-R11, CNB 321).
2. A rejoin inside grace resumes the concealed pawn and the owner component supplies `IsConcealed` (CON 239, AC-CON-32).
3. At grace expiry the pawn becomes a concealed NPC, culled for its former allies, and exits naturally (CON 238). The returner then goes through R18 -> PEN-R9 takeover.

**(b) Mid-brace / mid-tend:**
1. R17.1 cancels a held Prepare (its cooldown is already spent) and cancels an interaction. A disconnect is treated as an End `Released` (IIF 431); OC 265 resets tend progress.
2. The completion validity check (Round 448) voids it.
3. A rejoin keeps the defence stamp (ASN-R18).
4. A superseded connection's commands are dropped by generation (Round 456).
5. A grace-expiry item racing a rejoin is voided by `ExpectedConnectionGeneration` (R17.4).

**S-B3 (Blocker): Penalty has no rule for a participant killed while disconnected, so the returner can be stranded.**
- Systems: Penalty, Round, Roster. Step: an Active participant is disconnected (AI-driven, still `BoundLive`, "a kill on it is a `Player` kill", R17.2) and is assassinated during grace.
- The rules:
  - The PEN state table (line 158) has `Active(k)` + own `Assassinated` -> **Awaiting**, schedule.
  - At `T_due`, PEN-R9.1 requires `Connected`, otherwise "the item is voided".
  - Reclaim is defined only from **Absent** (line 169).
  - Absent is entered only by a disconnect *while* Awaiting, Picking or Embodied, or by grace expiry *while Active* (lines 160-167). Here grace expiry finds an Informant and returns `NoOp_AlreadyInState` (R17.4).
- Failure mode:
  - A returner after `T_due` is "Awaiting" with no pending item and no defined transition. That is undefined behaviour and possibly a permanent black view.
  - A returner before `T_due` happens to work.
- Resolution (owner: Penalty, states table and PEN-R9/R10): when a cause occurs for a participant who is not `Connected`, go to **Absent** with `t_lost` = slot and schedule no item. Equivalently, a voided `InformantRespawn` item for a disconnected participant moves to Absent. Add an AC alongside AC-NKP-11.
- Fix unambiguous: no. Always-ask: no.

**S-I4 (Info): OC and Round use different "connected" definitions for a final completion.**
- OC 265 voids a completion while the participant is `Unresponsive`. Round R15 requires "Connected", and Roster 210 maps Session `Unresponsive` to Roster `Connected`.
- OC's stricter check runs first, so the outcome is consistent. But with `bHandOverOnUnresponsive = false` the lagging tender's ring completes with no effect.
- Resolution (owner: OC): OC-10 names which connection state it reads (Session's).
- Fix unambiguous: no. Always-ask: no.

#### S5. Ended -> Reset -> Preparing -> Active, with a late joiner
**Chain:**
1. `Ended` dwells for ≥ 3 s (R9), then R10 `Reset`: admission Closed, timers cancelled, pawns destroyed, lobby travel, then Lobby / unlocked / Open. TMK goes Absent (TM-R6.5) and HUD goes to Lobby.
2. R5 `StartRound` refuses if anyone is Dormant or Unresponsive (step 3) and runs the `PreviewPreset` deck pre-check. On acceptance: admission lock -> `ResetAll` (`OnRosterReset` clears Penalty and the feature stores; the HUD tally goes to 0 on the new `RoundEpoch`) -> travel with a fresh seed.
3. R6 P1 params -> P2 TDC freeze and `OnTuningReady(TuningHash)` -> P3 `RequestLayout(Seed, TuningHash)` together with `AllocateAppearances` (deck reshuffle) -> P4 LSC `OnLayoutReady` (seed checked) -> P5 Session ack carrying `ClientHash` and `PoolHash` -> P5b -> P6 one-frame shuffled spawn through `SpawnSharedPawn`, with `AppearanceId` set before first replication -> P7 world-ready (the second stage is pending, so the full 10 s wait applies) -> R7 Active.
4. **Late joiner:** a new key gets admission `RejoinOnly` and is refused. A Dormant record is refused by `ShouldAdmitExpired` (R18). A rostered participant who drops and returns re-enters the P5 wait set before P6, or takes R17.3 after P6. Every output is a valid input.

**S-B4 (Blocker): a client that is slow at round start can keep its loading screen up indefinitely.**
- Systems: Round, Concealment, IVP, Crowd, Session. Step: P7 -> R7 for a client still receiving pawns.
- The rules:
  - Round edge case line 455: "that client keeps its loading screen until it holds `ExpectedPawnCount` pawns". R7 says the same, and `ExpectedPawnCount = D` is constant (R20).
  - Once Active starts, concealed NPCs are culled for **every** viewer (CON-R10) and concealed rival players for every non-ally.
  - Kills remove bodies, and a vacancy lasts until the replacement 10-60 s later (CNB-R9.2).
- Failure mode:
  - NPCs hide at about 0.19/min each with a median stay of 15 s (CON-R14), so a few pawns are usually culled. A client that has not reached D within the first seconds of Active may never hold D. That is an unbounded black screen, the opposite of R6's "a slow loader is bounded".
  - The pawn count also changes during the round, so "hold D" is not a valid predicate once Active has started.
- Resolution (owner: Round R7 and edge case 455): once `Phase = Active`, release on "holds every pawn the server has made relevant to this connection as of the ack/world-ready report", or on a bounded `WorldReadyTimeoutSec` after `ActiveAtServerTime`, never on D. Session test 48 (a rejoiner released at ack) already uses the ack form.
- Fix unambiguous: no. Always-ask: no if it uses the existing ack; yes (schema_changes) if it needs the pending world-ready stage (linked pending item: Round CR-13(d), active.md 97).

**Covered:**
- The F6 watchdog bound.
- The P5 stale-ack nonce.
- A repeated `ResetAll` index advances the epoch (R5).
- `LastResult` is kept through Reset (R9, R21).
- No pawns move and no commands are accepted in Preparing (R6, CNB-R12).

#### GDD flags

| GDD | Reason | Priority |
|---|---|---|
| round-match-framework.md | S-B1 (latch effects not staged: crowd halt, holds, score), S-B4 (loading screen keyed on constant D), S-B2 alternative | High |
| npc-kill-penalty-informants.md | S-B3 (no Absent path for a participant killed while disconnected), S-W1 (W-NKP-5 ignores contact delay) | High |
| teammate-markers.md | S-B2 (`RoundEnded` before `Ended` hides every marker) | High |
| hud.md | S-B2 (`ActiveAlive` precondition in staging window) | Medium |
| crowd-npc-behaviour.md | S-B1 (line 456, AC-CNB-22 Halt at the slot) | Medium |
| input-interaction-framework.md | S-B1 (IIF 421 hold release at the latch) | Medium |
| concealment-bushes.md | S-W2 (`IsConcealed` of a killed-concealed pawn undefined) | Medium |
| camera-system.md | S-W2 (own `IsConcealed` not gated by `OnRoleChangePresented`) | Medium |
| tuning-data-config.md | S-W1 (W-NKP-5 mirror) | Low |
| objectives-chain.md | S-I1 (log a latched Complete), S-I4 (which connection state OC-10 reads) | Low |
| character-pool-appearance.md | S-I3 ("same-look" replacement wording) | Low |
