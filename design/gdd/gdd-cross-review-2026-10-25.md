# Cross-GDD Review Report

Date: 2026-10-25
Skill: `/review-all-gdds full` (autonomous run; rigor full; workflow full, so all 8 sections are validated)
GDDs Reviewed: 19 of 19 present
Not read: none. All 19 were read by section; the limits of that reading are listed under "Coverage" below.
Focus: full (consistency + design theory + cross-system scenarios)
Context read: game-concept.md (pillars, anti-pillars, MVP), systems-index.md, design/registry/entities.yaml (299 entries, used as the value baseline), production/design-closeout/*.md (decisions D1-D15 and the closeout handoff lists), production/session-state/active.md (resolved and pending user decisions, which are not re-raised), and the previous report gdd-cross-review-2026-10-24.md (FAIL, updated to CONCERNS).
Engine: Unreal Engine 5.8 (installed 5.8.4), C++

Systems Covered: Tuning Data & Config, Participant State & Roster, Input & Interaction Framework, Character Pool & Appearance, Session & Travel, Round & Match Framework, Information Visibility Policy, Level Socket Contract, Shared Character & Locomotion, Camera System, Crowd & NPC Behaviour, Concealment (Bushes), Assassination & Defence Reversal, NPC-Kill Penalty & Informants, Objectives Chain, Procedural City Generator, Teammate Markers, HUD, Informant Pings & Team Communication (new since the last review).

**Section completeness (full tier).** All 19 GDDs carry all 8 required sections: Overview, Player Fantasy, Detailed Design (the alias for Detailed Rules), Formulas, Edge Cases, Dependencies, Tuning Knobs and Acceptance Criteria. All 19 also carry a Summary and a Cross-References table.

**Method.** The GDDs total about 2.5 MB, so their sections could not be pasted into sub-agent prompts as the skill's parallel contract asks. That deviation was logged in the decision log. Instead, each sub-agent got the shared context in its prompt: the D1-D15 decisions, the resolved decisions, the open items of the last review, and the section line numbers per GDD. Each agent then read its own sections by Grep `^#` plus offset/limit, against the registry.

The consistency pass was split six ways. All six ran on Sonnet:
- 2A: dependencies and cross-references (2a, 2c)
- 2B: knob ownership against TDC and the registry (2d)
- 2C: formula chains (2e)
- 2D: rules and acceptance criteria for the state and flow cluster (2b, 2f)
- 2E: rules and acceptance criteria for the world and body cluster (2b, 2f)
- 2F: whether the closeout pass actually landed

Phase 3 (design theory) ran on Opus and Phase 4 (scenarios) on Sonnet. The main session re-checked every Blocking candidate against the GDD text before accepting it. One candidate, 2E-B4, was downgraded to a Warning (C-W13); the reasoning is given there. Full per-phase findings with quotes and line numbers are in Appendices A-H.

### Coverage

- **2a dependencies:** the Dependencies and Cross-References sections of all 19 GDDs were compared pairwise and against systems-index rows 1-18 and 20. Rule IDs were spot-checked by Grep.
- **2b/2f rules and acceptance criteria:** read in full or by targeted ranges for Roster, Session, Round, Penalty, ASN, IIF, IVP, HUD, TMK, Pings, SCL, Camera, Crowd, CON, CPA, LSC, PCG and OC. Some gaps remain:
  - Not fully read: IIF acceptance criteria, most IVP and Roster acceptance criteria, and HUD UI Requirements.
  - IVP was grepped only for D3 and D14.
- **2d knobs:** the Tuning Knobs section of all 19 GDDs against TDC's Canonical Tunables, Registered Owner Keys and invariants, and against the registry. Stale-value greps found no live hits for any of these: NpcKillStrikeLimit "1-5", the old NpcCount ranges, "315 s", BushEnterSwapSec 0.45, an unprefixed LatencyWarnRttSec, the old CR-10 names, or a 10 cm TeamPing quantisation. Every shipped default passes every registered invariant, so the Default preset loads.
- **2e formulas:** all nine assigned chains were checked; chains 6-9 more briefly. Not checked: the CPA formulas, the TDC Formulas section (only its invariant rows), and LSC D1, D7, D8 and D9.
- **Closeout landing:** about 65 items were checked. 52 landed, 13 are ADR or art-bible items (all landed), 3 landed partly and 2 did not land.
- **Limit:** no sub-agent had a shell. The 27 lines over 2000 characters were read with Read and `Grep -o`, so a truncated read is possible on those lines.

---

### Consistency Issues

#### Blocking (must resolve before architecture begins)

Each item was checked by the main session against the GDD text.

🔴 **C-B1: AC-ASN-35 cannot pass alongside HUD F10 and ASN-R23** (from 2D-B2). The two sides:
- **The AC.** `assassination-defence-reversal.md:594`, AC-ASN-35: "when the kill ends the round, no client (host included) shows the round-end presentation before the deciding strike's `DeathServerTime`". `DeathServerTime` = `ContactServerTime` (ASN line 95).
- **The rules.** ASN-R23 stages the round-end presentation to the deciding strike's **death write**, which happens at `ContactServerTime − StrikeNetLeadSec`, 0.15 s earlier (ASN line 572). `hud.md` F10 (line ~342) adds: "Bystanders (no own plate or fall) show the banner at `t_phase`, up to `StrikeNetLeadSec` before a fall they can see: the accepted I-ASN-4 residual."
- **Consequence.** The rules accept a residual of up to 0.15 s that the AC forbids, so the AC fails by design.
- **Fix (design call; the AC is the odd one out).** Either restate AC-ASN-35's last clause as "not before the death write (`DeathServerTime − StrikeNetLeadSec`), with the I-ASN-4 residual", or make HUD hold the bystander banner until `DeathServerTime`.

🔴 **C-B2: SCL R5.3 still says a stop can be shorter than 0.5 s and that OQ15 is open; D2 closed it** (from 2E-B1).
- `shared-character-locomotion.md:68` (R5.3) says: "A stop begun below a pace … takes `v / d` and can be shorter than 0.5 s, down to a few centimetres at a key tap; whether that start-abort needs a braking-time floor is open, OQ15."
- SCL F2, AC-SCL-50 and OQ15 (closed by D2) say every stop lasts at least `StopMinSec` (0.5 s), using `d_eff = min(d, v / StopMinSec)`.
- The two statements of one rule in the same GDD cannot both hold.
- **Fix:** rewrite the R5.3 parenthetical to the D2 floor.

🔴 **C-B3: LSC R11.5 still defines final-eligible courts at 60 m; D4 set 44 m** (from 2E-B2 and 2F-B1).
- `level-socket-contract.md:219` (the graybox authoring checklist) says: "Final-eligible courts are those at least 60 m path from every start cell".
- V-SEP (`:145`), the knob row (`:585`) and the Generator all say `StartToFinalMinPathCm` is 44 m, "lowered from 60 m by decision D4".
- The graybox would be authored to a different eligibility rule from the one Validate applies.
- **Fix:** LSC line 219 should say "≥ `StartToFinalMinPathCm` (44 m)".

🔴 **C-B4: PCG cites a key that exists nowhere** (from 2E-B3).
- `procedural-city-generator.md:80` (G-PRE list) says: "V-CLUE (`ClueTag` shared by ≤ `ClueTagCourtsMaxCount`, now an LSC predicate)".
- No GDD or registry entry defines `ClueTagCourtsMaxCount`. LSC V-CLUE (`level-socket-contract.md:158`) uses `ClueMatchMaxCount` (3, owned by Objectives).
- **Fix:** rename the reference in PCG.

🔴 **C-B5: IIF depends on a due-time epsilon that Round never defines** (from 2F-B2).
- **The reference.** `input-interaction-framework.md:280` says: "The heap treats an item as due when `t_now ≥ DueTime − ε` (the comparison is Round's; see the cross-doc follow-up on ε)".
- **The test.** IIF's hold-completion AC (line 779) expects stamping "on the first drain with `t_now ≥ DueTime − 1e-4` (180 ticks at 60 Hz)".
- **The owner.** Round R11/R12 (`round-match-framework.md:108-110`) and its AC (`:711`) use `DueTime <= DrainStartTime` with no epsilon. The closeout follow-up was never adopted.
- **Consequence.** Under Round's rule, floating-point accumulation can stamp a 3.0 s hold on tick 181 instead of 180, so the IIF AC can fail against the owner's rule.
- **Fix:** Round adopts an epsilon (and states it in R12 and AC 711), or IIF drops ε and its AC follows Round.

#### Warnings (should resolve, but won't block)

Details, quotes and proposed text are in the appendices (the source IDs are given in brackets). Items tagged *carried* were open in the 2026-10-24 report.

**Dependencies and stale references**
- ⚠️ **C-W1:** Pings links are not reciprocal [2A-W1..W3]:
  - IIF's Dependencies has no Pings row, although Pings and IIF R13/R15 say the `IA_Ping` row was added.
  - Session does not list Pings back.
  - HUD's reading of `FTeamPingView` (the L0 ping slot) is recorded in neither HUD Upstream nor Pings Downstream.
- ⚠️ **C-W2:** TMK reads HUD's `HudSafeMarginPct`, but the edge is missing from the TMK Upstream table, the HUD Downstream table and index row 17 [2A-W4].
- ⚠️ **C-W3:** IIF Upstream omits Camera, TMK, LSC and Objectives, and Camera and IIF disagree on Hard versus Soft. Round Downstream omits CPA and IIF and still contradicts itself on Crowd and SCL [2A-W5, 2A-W6, 2D-W11; *carried* C-W12].
- ⚠️ **C-W4:** Peer-contract Hard/Soft labels disagree between IVP and CPA, LSC and Session. Session and SCL each list the other only as downstream [2A-W7].
- ⚠️ **C-W5:** SCL omits LSC and Camera upstream, and its Crowd row does not record the D2 (`StopMinSec`) and D14 (idle gaze) edges [2A-W8].
- ⚠️ **C-W6:** Penalty does not list Concealment, although Concealment lists Penalty as a hard dependent [2A-W9, 2D-W10; *carried* C-W7].
- ⚠️ **C-W7:** The systems-index "Depends On" column lags the GDDs on rows 3, 4, 7, 8, 9, 10, 11, 12, 15 and 17 [2A-W10..W12, 2E-W4, 2F-W11; *carried* C-W13 and "Crowd row lacks Camera"]:
  - row 8 lacks SCL;
  - row 11 lacks Camera;
  - row 12 lacks Objectives;
  - row 15 lacks Concealment.
- ⚠️ **C-W8:** Smaller one-sided or mislabelled dependency rows [2A-W13]:
  - Penalty to Crowd;
  - ASN to CPA;
  - Concealment's hard role for Crowd versus Crowd's "Soft";
  - Roster's "none" versus its Crowd row;
  - OC to PCG;
  - Session to CPA.
- ⚠️ **C-W9:** Stale status and "not written / pending" claims [2A-W14, 2F-W2, 2F-W3, 2F-W4, 2F-W10]:
  - Things that now exist but are still described as missing or future:
    - HUD `:603` says "No ADR exists yet"; ADR-0001 to 0023 exist.
    - IVP `:18`, `:497` and `:501` say ADR-001, Concealment and TMK are "not written"; all three exist.
    - TDC `:433` and `:463` call Pings "VS"; Pings is MVP.
  - Follow-ups and open items that are already done:
    - PCG `:438` follow-ups (b) to (d) are all done.
    - Round CR 21 (`:623`) is still worded as open, although D9 is applied.
    - The ASN header and its review log `:61` say the replication re-verification is pending. The review log `:76` and the decision log say it was done.
  - Status labels that no longer match:
    - Sibling status cells are stale in CON, ASN, OC, Penalty, Round, IVP and HUD.
    - Header status lines say "Reviewed - NEEDS REVISION", while the index says "In Review".

**Knobs, registry and invariants**
- ⚠️ **C-W10:** `CrowdDecoyEarlyReleaseMaxSec < ObjectiveInteractSec` is still not a registered invariant, and the Max key is not in the registry [2A-W15, 2B-W6, 2C-W4, 2E-W6; *carried* C-W1].
- ⚠️ **C-W11:** Camera I-CS-1 still says the runtime clamps, while TDC rejects. IIF `CameraToggleLockoutSec` safe values 0.2-0.217 and Camera's blend hard maximum of 0.29 both fail I-CS-1 [2A-W16, 2B-W5, 2E-W5; *carried* C-W17, mostly resolved].
- ⚠️ **C-W12:** SCL I-SCL-5 still reads a "Crowd pace set" that no key holds. `WalkSpeedScaleMinMult` safe values 0.85-0.90 violate it [2A-W17, 2B-W4, 2E-W7; *carried* C-W19].
- ⚠️ **C-W13 (high priority):** `IdleGazeDelaySec` (D14) is registered inconsistently [2B-W1, 2E-B4, 2F-W6]:
  - SCL's knob row (`shared-character-locomotion.md:529`) says `ServerOnly` / `Use`.
  - The registry (`entities.yaml:4844-4845`) says `Client` / `Round`.
  - TDC's SCL row lists `StopMinSec` as the D2 addition but never names this key.
  - AC-SCL-35 (`KeysRegistered`, "every key in Tuning Knobs exists with the stated audience and latch") fails until the three agree.
  - Graded Warning, not Blocking: the owner GDD is unambiguous and the registry and TDC are mirrors. 2E graded it Blocking.
  - Fix: registry and TDC follow SCL (`ServerOnly` / `Use`).
- ⚠️ **C-W14:** TDC key counts are stale [2B-W2, 2B-W3, 2E-W12]:
  - Camera: 48 in the Downstream table against 49 elsewhere.
  - LSC: "about 79" against about 84.
  - "I-SCL-1 to I-SCL-14", although I-SCL-15 exists.
- ⚠️ **C-W15:** LSC `LayoutWalkableCellsMaxCount` default 3,250 lies outside its own safe range 2,000-2,700, in both LSC and the registry, so the Default preset logs a warning at every load [2B-W7].
- ⚠️ **C-W16:** `StartToFinalMinPathCm` (D4) is not in the registry, although the closeout and decision log say the registry was updated. Other cross-system keys named by TDC invariants are also unregistered: `CrowdFullPacePct`, `WalkSpeedScaleMinMult`, `WalkStopSec`, `InputStarvationSec`, `PrelimCandidatesPerStageMinCount`, `BushEnterDurSec` and `CameraToggleBlendSec` [2B-W8, 2F-W7].
- ⚠️ **C-W17:** Nine owner safe-range values are rejected by a reject-invariant at the other keys' defaults [2B-W9 a-i, 2C-W1]:
  - ASN `StrikeBearingLimitDeg` 91-95;
  - Camera eye-cone minima;
  - LSC reseed and budget;
  - LSC Prelim/Final candidate counts against PCG's court target;
  - Session `SessionSoftTimeoutSec` 4-5;
  - Crowd `CrowdMaxIdleSec` 15-19.9;
  - Penalty `ExileBeatSec` 1.1-1.2, rejected by I-HUD-6, which Penalty does not cite (the safe range 0.75-1.2 is satisfiable only up to about 0.95 with W-NKP-5);
  - SCL `WalkSpeedCmPerSec` 125-130.8;
  - CON `BushEnterSwapSec` 0.66-0.7.
- ⚠️ **C-W18:** Four warn-only invariants (I-OC2, W-NKP-5, W-NKP-3 and I-ASN-7) have their bound inside the owner's safe range [2B-W10].
- ⚠️ **C-W19:** Leftover couplings with no invariant [2B-W11, 2E-W8]:
  - IVP `ConcealCullDelayMs` must be a multiple of 50.
  - PCG `CityGenZoneCandidatesCount` against LSC `ZoneCandidatesMinCount`.
  - `ReachDetourMult` and `DetourMaxMult` are two keys held equal by I-LSC-4.
  - `StopMinSec` against I-SCL-4 and `WalkStopSec`/`RunStopSec`.
  - TMK `TeammateMarkerLargeMult` uses a discrete hard range that TDC F-1 does not support.
  - Round's `NpcCount` floor omits I-LSC-1.

**Formulas**
- ⚠️ **C-W20:** OC F1 uses a run speed of 3.5 m/s but SCL's default is 450 cm/s, so `T_rush` should read 41-73 s, not 50-91 s. This affects D-W2 [2C-W2].
- ⚠️ **C-W21:** Concealment F1/F2 still use the pre-D7 bush rate r_N 0.19; Crowd ships 0.20. Recomputed: E[O] is 0.43, m0 is 0.349, and the D1 cap is 45 % [2C-W3, 2E-W11].
- ⚠️ **C-W22:** Two Pings formula gaps [2C-W5, 2C-W6]:
  - Pings F1's 100 ms clock-skew tolerance holds only at the default refill rate (25 ms at 2.0/s).
  - The Pings edge ring reuses TMK F3 without stating its glyph-size or hysteresis inputs.
- ⚠️ **C-W23:** Layout density mismatches [2C-W7..W9]:
  - LSC D6's zone-area need at NpcCount 35 is about 1,800 m², not 1,467 m²; roughly half of layouts fail zone fill at NpcCount 35-40.
  - LSC D4's walk-share rationale uses the 12-minute target, while OC models about 9 minutes.
  - Crowd F3 gives N_E 13-16, while LSC D5 and the registry allow 12.
- ⚠️ **C-W24:** The final-court margin under D4 is 0.48 m against a default jitter of one cell (2 m), and the OQ9 enumeration script that would settle it has not been run [2C-W10].
- ⚠️ **C-W25:** The camera mask-off threshold of 20 cm (D6) is below the geometric minimum free boom length of about 21 cm (capsule radius 35 minus sweep radius 14). F2's "18 cm" example cannot occur [2C-W11].
- ⚠️ **C-W26:** SCL F9 cites "IIF AC 49" for `B_forced`, but the ActionPose bandwidth test is AC 52. The ActionPose delay U is described three different ways across SCL, IIF and IVP [2C-W14].
- ⚠️ **C-W27:** Timing gaps [2C-W15]:
  - No invariant ties `EndedMinDwellSec` to the HUD banner delay.
  - `StartLeadSec` 0.25 s is below the p99 one-way delay plus one send interval at high latency.
  - IIF F6's "0.1 s of walking" ignores the SCL start ramp.
  - Round F4's `L_soft` and `L_hard` bounds are 0.25 s wider than Session F2's.
  - LSC says "63 pawns" where it means NPCs.
  - No listen-host upload figure is given anywhere (about 280 KB/s worst case).
- ⚠️ **C-W28 (outside GDD scope):** ADR-0003, ADR-0010, ADR-0018 and architecture.md:725 still state a 30 KB/s pawn line, while SCL F9 outputs 31.1 KB/s against a 36 KB/s line [2C-W13].

**Rules (state and flow cluster)**
- ⚠️ **C-W29:** Penalty PEN-R8 says Round triggers respawn scheduling for every cause. An informant kill is not a Roster transition, so Penalty schedules that one itself [2D-W1, S-I4].
- ⚠️ **C-W30:** The kick path is told inconsistently [2D-W2..W4, S-I6]:
  - Session's transition table has no kick-to-`Expired` row, although Round R17.2 cites rule 13 for it.
  - Round R17.2 says a kicked participant can return, but Session refuses a kicked key.
  - A kicked teammate shows "reconnecting" to allies for the whole grace period (design call).
- ⚠️ **C-W31:** Roster R8 retires the binding inside `RequestTransition`, which carries no `RetireAtServerTime` for TM-R6 to use [2D-W5].
- ⚠️ **C-W32:** A ping pressed between the kill slot and contact is "accepted" per the Pings edge case. The sender's binding is already retired, so it is dropped silently, and the edge case covers only the Active-to-Informant window (design call) [2D-W6, S-I5].
- ⚠️ **C-W33:** Stale element lists [2D-W7, 2D-W8]:
  - HUD-R2's list of world-anchored elements omits the ping rings.
  - TM-R1 and its Roster row omit `RetireAtServerTime` and `bReconnecting`.
  - HUD-R1 says E01-E20.
- ⚠️ **C-W34:** D15 is in no GDD. Session UI Requirements still put buttons on status overlays, which ADR-0021 L4 no longer allows below ZOrder 100, and HUD-R4 calls L4 a `HitTestInvisible` plain overlay [2D-W9, S-I3].
- ⚠️ **C-W35:** A final `Complete` rejected by the latch is still not logged as a tend, so the OC F5 objective-win share is slightly under-counted [2A-W18, 2D-W12; *carried* S-I1].

**Rules (world and body cluster)**
- ⚠️ **C-W36:** The D14 trigger is defined two ways: SCL R8.9 says the control rotation is unchanged, while Crowd G1 says no `Look` was issued. SCL also says a menu-open pawn "keeps last movement state", while IIF and the pause-menu spec say zero Move [2E-W1, S-I2].
- ⚠️ **C-W37:** D3 (host significance tiering) is not carried into Camera's "always-tick / identical pose-tick counts" wording or into AC-CS-28. SCL R11.5 keeps a stale "rate 1" parenthetical [2E-W3].
- ⚠️ **C-W38:** SCL R2a.4 and LSC AC-14 say a replacement carries "the victim's card". CPA CP-12(a) draws uniformly over the vacant cards [2E-W9].
- ⚠️ **C-W39:** AC-ASN-39 and AC-ASN-12 still describe a "hash" pick, and the ASN disposition rows (`:506`, `:519`) still name `SpotSalt`. The pick is now the `BushSpotPick` stream [2E-W10, 2F-W8].
- ⚠️ **C-W40:** Two registry wording issues [2E-W13, 2F-W5]:
  - The registry's `respawn_region` wording ("of the NEARER anchor") can be read as a union of both teams' discs. LSC and Penalty say the team's own anchor, strictly nearer than the partner's.
  - `entities.yaml:519` still points at CPA OQ-14, which moved to Penalty OQ-NKP-13.
- ⚠️ **C-W41 (process):** Two TDC process gaps [2F-W1, 2F-W9]:
  - The TDC review log has no "Closeout cross-doc pass 2026-10-25" entry; the other 19 logs have one.
  - TDC's I-ASN-1 row does not restate the 290 cm cap.

---

### Game Design Issues

#### Blocking

None. No written rule violates an anti-pillar, no dominant strategy makes a pillar fail outright, and no economy state makes a round undecidable.

Checklist results:
- 3a progression: PASS.
- 3b attention: PASS, at the limit of 4. The ping folds into the team channel and the walk-slower key is optional.
- 3c dominant strategies: 3 new warnings.
- 3d economy: PASS. Pings are a catch-up lever for the team that is losing members, and D-W3 is carried.
- 3e pacing: 1 warning.
- 3f pillars: PASS. All 19 fantasies cite a pillar; there are 2 emergent risks.
- 3g coherence: PASS, with one language seam.

#### Warnings

⚠️ **D-W5: Ping chains give the team a near-continuous, through-wall position for one walker** (Pings PNG-R1/R6/R7, F1, F3, OQ5, OQ7) [D3-W1].
- **Mechanism:**
  - At the default refill (`PingBucketPerSec` 0.5, burst 2), one informant can re-mark a suspect every 2 s.
  - A walker covers 2.8 m in 2 s, inside the 4 m ring, and each new ring replaces the last.
  - Two informants alternating give a 1 s refresh.
- **Why it matters:**
  - In effect this is a hand-built tracking marker on a person, which touches the anti-pillar "NOT an identification-tools game".
  - It is paid for by an informant with nothing left to lose.
- **Options:**
  - (a) Set the OQ7 failure threshold before the first playtest.
  - (b) Add a no-refresh rule within 4 m of the sender's own live ring (rule change).
  - (c) Set `PingBucketPerSec` to 0.2-0.25.
  - (d) Shorten `PingLifetimeSec` to 4-5 s together with (c).
- Creative call.

⚠️ **D-W6: Pinging from inside a bush turns concealment into a productive spotter post** (Pings Edge Cases versus CON Player Fantasy "a brief cover, not a post" and the anti-pillar "NOT a hide-and-wait game") [D3-W2]. A concealed player sees out, can mark for the team, and pays no exposure. Options include refusing pings from a concealed sender, or treating a ping as a concealment-breaking act. Creative call.

⚠️ **D-W7: The D14 idle gaze layer makes watching a fixed spot nearly free** (SCL R8.9, Crowd G1, Crowd Tell Matrix T13) [D3-W3, 2E-W2, 2C-W12].
- **The concern.** After 3 s of no look input, the head scans like an NPC, so a camper's head no longer betrays where the camera is watching.
- **Effect on camping.** This makes the camping concern of D-W3 cheaper.
- **The T13 accounting gap.** The idle-scan holds of 3-7 s exceed the 2.5 s head-hold threshold of tell T13, but they are not counted in the 0.10/min NPC head-hold rate. SCL and Crowd also define the hold differently: held at the clamp, versus held on a passer-by.
- **Recommendation:** reconcile T13 with the idle-scan layer, and add camping-duration telemetry. Creative call on the trade-off between this and the parity goal of D14.

⚠️ **D-W8: The ping has no row in the Crowd Tell Matrix** (Crowd CNB-R16 T1-T17 against Pings PNG-R2) [D3-W4]. Its accepted exception to Pillar 2 ("it carries only what the sender saw") is invisible to anyone auditing tells from the Crowd side. This is a document fix: add a T-row stating that the ping has no world-visible deviation and why that is accepted.

⚠️ **D-W9: Round's pacing and Pillar 4 thresholds do not match the concept** (Round F7 against the concept's MVP Definition and OC F1) [D3-W5].
- Round's objective-end target is above 0.5 with no team-size filter. The concept asks for at least 70 % in 4v4-and-larger buckets.
- The stall flag is set at 1,440 s, against a modelled round of about 9 minutes.
- D-W4 is resolved in the concept but not propagated to Round.

⚠️ **D-W10: "Exile" names two different events in player-facing language** (ASN rival-kill plate "Target exiled" against the Penalty removal flow) [D3-W6].
- D11 renamed the removal card to "Informant" / "Too many wrong targets".
- Penalty prose and fantasy still say "exiled", and the Penalty fantasy still says "voice and text" continue (text chat is VS).
- Recommendation: one term per event.

**Prior design-theory warnings (2026-10-24):**
- D-W1 (brace timing) and D-W2 (rushing at a run): unchanged, and deferred by the user to playtest telemetry. C-W20 corrects the D-W2 rush figure to 41-73 s.
- D-W3 (camping the final): same severity. D-W7 makes camping cheaper, while informant scouting with pings gives the trailing team a second option.
- D-W4 (small-team playtests): partly resolved; see D-W9.

---

### Cross-System Scenario Issues

Scenarios walked: 5
1. S1 Informant sighting to kill: ping, ally reads it, strike on a rival in cooldown, the rival becomes an informant, then the ring, T_due and takeover follow.
2. S2 Pause menu during contact (D14/D15): menu opened while bracing, tending, concealed, or holding walk-slower; strike during the idle-gaze window; round ends while the modal is open.
3. S3 Disconnect / kick / failed supersede-rejoin (D9).
4. S4 Killed informant (D10): RetireBinding InformantKilled, then Camera fade, no removal card, T_due countdown, respawn takeover, CPA draw and TMK exit.
5. S5 Final-court finish with a rival strike pending, through the two-step round-end contract, then Ended, Reset, Preparing and the world-ready stage.

The contracts fixed on 2026-10-24 still hold: the two-step round end, Absent for disconnected victims, the late-loader bound, IsConcealed until body removal, and W-NKP-5. S1, S4 and S5 are otherwise consistent end to end.

#### Blockers

Each item was checked by the main session against the GDD text.

🔴 **S-B1: S3/S4, Roster, HUD, Teammate Markers. A false "{Ally} — back" line** (from 2D-B1 and Phase 4 S-B1).
- **The rule.** `hud.md:86` HUD-R10a prints "{Ally name} — back" when `bReconnecting` goes true to false "while that ally's `State` is unchanged". It assumes a clear at grace expiry comes with a State change, "and nothing for an informant ally".
- **The flag.** `participant-state-roster.md:108` (R6) clears `bReconnecting` by time, at `GraceExpiryMs`, "which for an `Informant` is the same instant even though an informant has no grace of its own".
- **Paths that print a false "back" while the ally is still gone:**
  - An informant ally still disconnected at that instant: no State change, since Round R17.2 converts only `Active` participants.
  - An Active ally killed while disconnected. That ally is now an Informant, with its flag still true until `GraceExpiryMs`.
  - A supersede-from-live rejoin that outlasts `G_eff`.
- **Test gap.** AC-HUD-28 and AC-PSR-53 test only Active-ally cases.
- **Fix (design call).** Either Roster R6 clears the flag at grace expiry only together with a State change and keeps it until reclaim or `EndRoundAll` otherwise, or HUD-R10a prints "back" only on an observed reclaim (a Connection projection back to `Connected`).

🔴 **S-B2: S3, Session, Round, Roster, Crowd. A supersede-from-live rejoin leaves pawn control and the grace basis undefined** (Phase 4 S-B2).
- **Session's rule.** `session-travel.md:421` and AC 17 (`:612`): when a live connection is superseded, "Only `OnParticipantRejoined(Superseded)` is raised" and `OnParticipantDisconnected` does not fire.
- **What Round and Roster assume.** Round R17.3 (`round-match-framework.md:150`), Roster R11 projection details (`participant-state-roster.md:220`) and Session `:168` all speak of "a grace timer started at the supersede". Round also says "the pawn is already AI-driven, so no second takeover blend happens".
- **What is undefined.**
  - No event starts that timer: R17.2 starts grace only on `OnParticipantDisconnected`.
  - R17.1 hands control *back* on `OnParticipantRejoined(Superseded)` while the rejoiner is still loading, for up to the rejoin window.
  - If the supersede lands before the 3 s soft timeout, nothing AI-drives the pawn during that load.
- **Open questions.** Who drives the pawn during the load? Does the participant convert at `G_eff` from the supersede, or after a fresh grace? Phase 4 notes this also conflicts with Session rule 25f (f).
- **Owners:** Session (raise an event, or define the supersede as starting grace) and Round R17 (takeover on supersede and the grace basis).

#### Warnings

⚠️ **S-W1: S5, Penalty, Crowd, Round. A takeover pawn escapes the round-end stop.** An informant takeover committed earlier in the same drain as an objective-win latch is released from Crowd but not yet human-driven. Round R9 (f) does not stop it, so it keeps walking while the city halts, which marks it as the informant. Owner: Round R9 (f), which should also stop pawns in hand-over.

⚠️ **S-W2: S5, HUD, TMK, Round. Late removal line and marker exit have no rule.** An ally's removal line and marker exit that fall due after the round-end step are unspecified, although `TeamSummary` already counts that ally as eliminated. Owners: HUD and TMK (drop or show them under the banner).

⚠️ **S-W3: S5, Round, Roster, Crowd, Pings. The round-end step has no stated budget.** One server step forces net updates on every Roster record and on the GameState, clears pings, halts the crowd and stops about 60 pawns, and no frame or bandwidth budget is stated for it. Owner: Round, together with ADR-0002 and ADR-0022.

#### Info

ℹ️ S-I1: S1 / S4, Pings and HUD. A removed sender's ping ring keeps its name label for up to 8 s.
(The other Phase 4 info items, S-I2 to S-I6, are merged into C-W36, C-W34, C-W29, C-W32 and C-W30.)

---

### GDDs Flagged for Revision

| GDD | Reason | Type | Priority |
|-----|--------|------|----------|
| hud.md | S-B1 (false "back" line, HUD-R10a); C-B1 (F10 bystander banner vs AC-ASN-35); C-W1, C-W2, C-W9, C-W33, C-W34; S-W2; S-I1 | Scenario / Consistency | Blocking |
| participant-state-roster.md | S-B1 (R6 clears `bReconnecting` by time); S-B2 (R11 assumes grace at supersede); C-W31, C-W33 | Scenario / Consistency | Blocking |
| session-travel.md | S-B2 (supersede raises no disconnect, grace timer undefined); C-W1, C-W4, C-W30, C-W34 | Scenario / Consistency | Blocking |
| round-match-framework.md | S-B2 (R17.3 grace basis and pawn control); C-B5 (due-time ε owner); C-W3, C-W9, C-W30, C-W35; D-W9; S-W1, S-W3 | Scenario / Consistency / Design Theory | Blocking |
| assassination-defence-reversal.md | C-B1 (AC-ASN-35 last clause); C-W9, C-W17, C-W39; D-W10 | Consistency / Design Theory | Blocking |
| shared-character-locomotion.md | C-B2 (R5.3 stale stop rule); C-W5, C-W12, C-W13, C-W17, C-W19, C-W26, C-W36, C-W37, C-W38; D-W7 | Consistency / Design Theory | Blocking |
| level-socket-contract.md | C-B3 (60 m vs 44 m); C-W15, C-W16, C-W19, C-W23, C-W24, C-W38, C-W40 | Consistency | Blocking |
| procedural-city-generator.md | C-B4 (`ClueTagCourtsMaxCount`); C-W9, C-W19, C-W24 | Consistency | Blocking |
| input-interaction-framework.md | C-B5 (ε on due-time); C-W1, C-W3, C-W11, C-W26, C-W27, C-W36 | Consistency | Blocking |
| informant-pings-team-comms.md | C-W1, C-W22, C-W32; D-W5, D-W6, D-W8; S-I1 | Design Theory / Consistency | Warning (high) |
| crowd-npc-behaviour.md | C-W8, C-W10, C-W23, C-W36; D-W7, D-W8 | Consistency / Design Theory | Warning |
| concealment-bushes.md | C-W9, C-W17, C-W21; D-W6 | Consistency / Design Theory | Warning |
| npc-kill-penalty-informants.md | C-W6, C-W9, C-W17, C-W29; D-W10 | Consistency / Design Theory | Warning |
| objectives-chain.md | C-W9, C-W18, C-W20, C-W35 | Consistency | Warning |
| camera-system.md | C-W11, C-W14, C-W17, C-W25, C-W37 | Consistency | Warning |
| tuning-data-config.md | C-W9, C-W13, C-W14, C-W16, C-W41; mirrors for C-W10, C-W12, C-W17-C-W19 | Consistency | Warning |
| information-visibility-policy.md | C-W4, C-W9, C-W19 | Consistency | Warning |
| teammate-markers.md | C-W2, C-W19, C-W33; S-W2 | Consistency / Scenario | Warning |
| character-pool-appearance.md | C-W4, C-W8, C-W38 | Consistency | Warning |
| systems-index.md (not a GDD) | C-W7 | Consistency | Warning |
| design/registry/entities.yaml (not a GDD) | C-W13, C-W16, C-W40 | Consistency | Warning |

---

### Verdict: FAIL

FAIL: 7 blocking issues must be resolved before architecture work proceeds on the affected contracts: C-B1 to C-B5, S-B1 and S-B2.
- C-B2, C-B3 and C-B4 are one-line text fixes that propagate D2 and D4 or correct a key name.
- C-B1, C-B5, S-B1 and S-B2 need a short design ruling each.

There are also 51 warnings: 41 consistency (C-W1 to C-W41), 6 design (D-W5 to D-W10) and 3 scenario (S-W1 to S-W3). There is 1 info item (S-I1). The architecture is already accepted (ADR-0001 to 0023, conditional), so the blockers mainly touch ADR-0002 (queue and due-time), ADR-0005 (Session acknowledgement and rejoin), ADR-0018 (strike presentation) and ADR-0019 (informant flow).

### If FAIL — required actions before re-running

1. **assassination-defence-reversal.md / hud.md (C-B1):** align AC-ASN-35's round-end clause with the accepted I-ASN-4 residual (the death write), or make HUD F10 hold the bystander banner until `DeathServerTime`.
2. **shared-character-locomotion.md (C-B2):** rewrite the R5.3 parenthetical (line 68) to the D2 stop floor (`StopMinSec`, every stop ≥ 0.5 s; OQ15 closed).
3. **level-socket-contract.md (C-B3):** line 219 should use "≥ `StartToFinalMinPathCm` (44 m)".
4. **procedural-city-generator.md (C-B4):** line 80 should use `ClueMatchMaxCount`.
5. **round-match-framework.md + input-interaction-framework.md (C-B5):** adopt one due-time comparison. Either Round R12 and AC 711 gain an ε (IIF suggests 1e-4 s), or IIF line 280 and its AC (line 779) drop ε and follow Round.
6. **participant-state-roster.md + hud.md (S-B1):** make the "back" line fire only on an observed reclaim, or stop R6 clearing `bReconnecting` by time for informants and disconnected victims. Add AC cases for an informant ally and for an ally killed while disconnected.
7. **session-travel.md + round-match-framework.md (+ participant-state-roster.md) (S-B2):** define supersede-from-live. Which event starts grace, who drives the pawn while the rejoiner loads, and is conversion at `G_eff` from the supersede? Then align Round R17.1/R17.3, Roster R11 and Session `:168`/`:421`/AC 17.

Re-run: `/review-all-gdds since-last-review` (or `consistency`) after the fixes. The per-GDD re-reviews (Re-review policy) apply to the flagged GDDs.

### Notes for the fix pass

- **High-value warnings that are cheap to fix:**
  - C-W13 (`IdleGazeDelaySec` audience, latch and TDC row);
  - C-W16 (register `StartToFinalMinPathCm`);
  - C-W9 (stale status and "not written" claims);
  - C-W21 (re-run Concealment F1 at r_N 0.20);
  - C-W20 (OC run speed);
  - C-W25 (mask-off threshold against the geometric minimum);
  - C-W15 (LSC default outside its safe range).
- **Decisions that need the user:**
  - D-W5, D-W6 and D-W7 are creative calls on the new MVP ping and on D14. Their rule-change options are always-ask (`scope_changes` / rule change).
  - The TDC lint suggested last time ("safe range must satisfy all invariants at the other keys' defaults") would catch C-W17 and C-W18 automatically.

---


## Appendix A — Phase 2A dependencies and cross-references (detail, sub-agent output verbatim)

### Phase 2A findings: dependency bidirectionality (2a) and stale references (2c)

Run: /review-all-gdds full, 2026-10-25, Phase 2 sub-agent 2A. Report only; no GDD, registry, index or ADR was edited.
STATUS: COMPLETE (final write). Counts: 0 Blocking, 18 Warnings (2A-W1..W18).

#### Coverage

Checked (read in full, Dependencies sections, all 19 GDDs): TDC 327-366, Roster 493-519, IIF 468-500, CPA 306-331, Session 442-471, Round 483-521, IVP 378-419, LSC 514-541, SCL 479-513, Camera 509-542, Crowd 350-387, CON 255-284, ASN 308-336, PEN 346-379, OC 288-321, PCG 316-337, TMK 344-394, HUD 395-443, Pings 216-239. Systems-index rows 1-30 (lines 46-77) compared against every GDD's upstream list.

Cross-References read in full (incl. "Change requests received" tables): all 19 GDDs (Pings, TMK, OC, CPA, CON, ASN, PEN, Camera, Crowd, HUD, IVP, IIF, LSC, Roster, PCG, Session, Round, SCL, TDC).

Rule IDs confirmed present by Grep (definition lines): PNG-R1..R12, HUD-R1..R17 (incl. R6a, R10a), HUD-E01..E09/E20..E22, TM-R1..R14 (incl. R4a), CS-R1..R15, OC-1..OC-17, CNB-R1..R16 and Tell Matrix T1..T17, CON-R1..R16 (incl. R4a, R6a, R6b), ASN-R1..R23, PEN-R1..R17. `IsInBushOrTransition` exists in CON, IVP R9c and Crowd. docs/architecture holds ADR-0001..0023.

Also confirmed present by Grep: Roster R1-R12, Round R9/R13/R15/R17/R17.9/R18/R20/R23, IVP R7.4a/R7.5/R7.6/R7.8/R13/R14, Session rules 3-25 (24, 25), SCL R8 and R8.9 (idle gaze, lines 529/750), Crowd T13 and T17, PNG-R10 (round-end clear). Roster accepts Disconnect for an `Expired` connection (Roster lines 224, 692; decision D9 applied).

Prose scan for unwritten systems: Lobby UI (#19), Post-Round Reveal (#21), Diegetic Audio (#22), Event Feedback & Cues (#23), Settings & Accessibility (#24), Onboarding (#25), Telemetry (#26), Anti-Cheat (#27), Dedicated Server (#28), Matchmaking (#29), Progression (#30) are all index rows (Not Started): no Blocking. No GDD references a system that is absent from the index. The pause menu (decision D14/D15) has a spec at design/ux/pause-menu.md ("In Design", not yet `/ux-review`ed) and no systems-index row; "main menu" (Session lines 21, 29, 48, 508) has design/ux/main-menu.md; both are UX specs, not systems, so no finding beyond the note in 2A-W8.

Carried over from 2026-10-24 (re-checked): C-W7 still present (W9); C-W12 still present (W6); C-W13 index row 12 lacks Objectives still present (W10), TDC TMK Soft/Hard FIXED; Crowd row 11 lacks Camera still present (W11); C-W1 still present (W15); C-W17 still present (W16); C-W19 still present (W17); C-W22 FIXED (TDC I-PCG-5 line 273, registry safe range now 6-8, entities.yaml 1764); S-I1 still present (W18).

Could not do: Bash `sed` on over-length lines (no Bash tool in this agent); long lines were read through Read and Grep -o instead. Registry value baselines were not audited (not in 2a/2c scope), apart from the C-W22/C-W1 spot checks.

#### Blocking

None found so far (no reference to a rule, field, key or formula that does not exist among those checked).

#### Warnings

##### 2A-W1  IIF Dependencies omit the new Pings dependent
- Pings, Dependencies upstream, line 225: "Input & Interaction Framework (3) ... Yes: IIF R13/R15 add the ping row (user decision 2026-10-25)". IIF R13 (line 187), R15 row (line 210) and Cross-References (line 619, 696) do name `IA_Ping`.
- IIF, Dependencies Downstream table (lines 484-498): no Pings row. Index row 3 does not list Pings either (correct direction: row 20 lists IIF).
- Fix: IIF adds a Downstream row "Informant Pings (20): `IA_Ping` binding in `IMC_Common`; controller RPC, never a pawn command; Hard" (IIF owner). Rule 2a (Pings must be listed back) is otherwise met for Roster, IVP, Round, Camera, HUD, PEN, TDC, TMK (soft).

##### 2A-W2  Session does not list Pings back
- Pings, line 230: "Session & Travel (5) | host parity, travel and reconnect events | Soft | ... One-directional". Index row 20 lists Session & Travel (soft).
- Session Dependencies Downstream (lines 457-469): no Pings row (Session mentions the team ping only in Open Questions line 684, item 7).
- Project rule (design-docs.md): dependencies must be bidirectional. Fix: Session adds a soft Downstream row, or the design call is recorded that soft peer-contract reads are exempt (applies equally to W6).

##### 2A-W3  HUD reads Pings content, but neither table records HUD as a dependent of Pings
- HUD line 423 (Downstream, Pings row): "The HUD also reads Pings' content (`FTeamPingView` through the C-TeamView ping part, PNG-R7) to draw it, so the link runs both ways".
- HUD Upstream table (lines 401-414) has no Pings row; Pings Downstream (lines 235-239: EFC, Settings, Telemetry) has no HUD row; index row 18 does not list Pings. Fix: HUD Upstream gets a Pings row (soft, content), Pings Downstream a HUD row (Hard), or both state the link is a one-way layer contract (design call).

##### 2A-W4  TMK reads HUD keys, but the edge is recorded in the wrong table
- TMK line 368 (Downstream): "HUD (#18) | the marker layer as L0 (HUD-R4) ...; HUD owns `HudSafeMarginPct` and UI scale, which Markers read | Soft".
- HUD Downstream (lines 418-424) has no TMK row; TMK Upstream (lines 350-361) has no HUD row; index row 17 lacks HUD. HUD Upstream line 411 lists TMK (soft). So TMK->HUD (reads `HudSafeMarginPct`, UI scale) is unrecorded as a dependency by both. Fix: TMK Upstream and HUD Downstream add the row; index row 17 adds HUD (soft).

##### 2A-W5  IIF Upstream omits four systems whose interfaces the IIF consumes; labels disagree for Camera
- IIF Upstream (lines 474-480) lists TDC, Roster, Round, SCL, IVP only. Other GDDs record IIF depends on them:
  - Camera: Camera Downstream line 531 "Input & Interaction Framework (3) | `GetAimRay()` and `WorldPoint`; masked = not rendered (R10, Strike hint) | Hard"; IIF line 487 lists Camera only in its Downstream table as "(soft peer)". Hard vs soft disagree and IIF Upstream has no Camera row.
  - Teammate Markers: TMK line 367 "IIF ... `IsMarkedAlly(Pawn)` ... Hard (for the honest-client rule)"; IIF line 493 puts TMK in its Downstream table ("none taken; supplies `IsMarkedAlly(Pawn)` for R10 ... IIF consumes"), i.e. wrong table. Index rows 3 and 17 list neither.
  - Level Socket Contract: LSC Downstream line 540 "Input & Interaction Framework (3) | Bush geometry from the Manifest ... | Soft"; IIF Upstream has no LSC row; index row 3 lacks LSC.
  - Objectives Chain: OC Downstream line 312 "Input & Interaction Framework (3) | Team-audience live-site set ... `CanBegin` predicate ... | Hard"; IIF lists OC only as its own dependent (line 491).
- Fix owner: IIF (add the four upstream rows with agreed labels) and index row 3; Camera/IIF Hard-vs-Soft is a design call (the dependency is hard for `WorldPoint` correctness).

##### 2A-W6  Round Downstream omits CPA and IIF, and still contradicts itself about Crowd and SCL (carried over from 2026-10-24 C-W12)
- Round Upstream lists CPA (line 495, "Hard (caller role)"), IIF (line 496, "Hard") and Crowd (line 498) and SCL (line 497). CPA lists Round as upstream (line 314) and IIF lists Round as upstream (line 478), so Round Downstream should list both; it does not (lines 502-519).
- Carried over C-W12: Round line 519 "Crowd & NPC Behaviour, Concealment, Shared Character | Consume `OnRoundActive` ... (whether Crowd and SCL belong here or only upstream is C-W12, open)" while the same GDD lists Crowd and SCL as Upstream (lines 497-498). Index row 6 lists SCL but not Crowd; index row 11 lists Round (hard).
- Fix: design call on peer labelling (declare "mutual peer contract" rows on both sides), then Round and index row 6.

##### 2A-W7  Peer-contract Hard/Soft labels disagree between IVP and the implementing GDDs
- IVP peer table (lines 394-398) says "Hard for SCL / Session / IIF / CPA / LSC". Implementing GDDs: SCL line 506 Hard (agrees); IIF line 480 Hard (agrees); Session lists IVP only as a Downstream soft row (line 460) and not as Upstream; CPA lines 317 and 328 label IVP "Soft"; LSC line 525 labels IVP "Soft".
- Fix: IVP (restate as "soft for the Policy, hard for implementer only where the implementer says so") or CPA, LSC, Session (raise to Hard); design call. Same pattern: Session line 463 and SCL line 498 each list the other under Downstream only (index row 5 says Session depends on SCL, soft); neither lists the other under Upstream.

##### 2A-W8  SCL Dependencies miss two upstream edges and do not record D2 / D14 on the Crowd row
- SCL Upstream (lines 485-490): TDC, Roster, IIF, CPA. Index row 9 lists "Level Socket Contract (soft peer)"; SCL records LSC only under Downstream (line 505, "both directions"). Camera: Camera Downstream line 532 lists SCL (mask flag, soft), SCL Upstream has no Camera row and index row 9 lacks Camera.
- SCL Downstream Crowd row (line 500) lists entry points, speed scale, handover and contact reaction but not the two edges added by decisions D2 and D14: Crowd consumes SCL's `StopMinSec` (SCL line 528 "also read by Crowd for NPC stops"; Crowd line 357 and AC-CNB-04(c)) and Crowd reads `IdleGazeDelaySec` and the shared idle-scan parameters (SCL line 529 "Crowd G1 reads it"; Crowd line 73 G1). Crowd Upstream line 357 records both; SCL Cross-References (617-686) have no row for them.
- Fix: SCL (add Upstream rows for LSC (soft) and Camera (soft); extend the Crowd Downstream row with `StopMinSec` F2 and idle-gaze layer R8.9).

##### 2A-W9  Penalty does not list Concealment (carried over from 2026-10-24 C-W7)
- CON Downstream line 276: "NPC-Kill Penalty & Informants (#14) | `IsInBushOrTransition` for takeover eligibility | Hard".
- PEN Upstream (lines 352-362) has no Concealment row; index row 14 has no Concealment; PEN Cross-References line 502 "Concealment change request 6 | Use `IsInBushOrTransition` ... | ALREADY DONE (PEN-R9 via LSC/IVP R9c)" and PEN-R9 (lines 96-106) never names the predicate (eligibility goes through `CheckTakeOverEligibility`, SCL R2a, and IVP R9c). CON line 284 still says "Penalty and Objectives should list Concealment".
- Fix: design call. Either Concealment downgrades the PEN row (indirect via IVP R9c) or Penalty adds a Concealment row and index row 14 is amended.

##### 2A-W10  Index row 12 lacks Objectives, row 15 lacks Concealment (carried over from C-W13)
- CON Downstream line 280: "Objectives Chain (#15) | ... Hard (for P4)"; OC Upstream line 304: "Concealment (Bushes) (#12) | ... Hard peer for P4". Index row 12 (Depends On) lacks Objectives Chain; index row 15 lacks Concealment. Fix: systems-index (or declare it a peer note).

##### 2A-W11  Crowd missing from index row 11 Camera dependency (carried over)
- Crowd line 387 "only Camera is still not listed (Cross-References 14, 21)"; Crowd Cross-References item 21 (line 509). Index row 11 (line 58) still lacks Camera System; Camera Downstream line 536 lists Crowd (soft). Fix: systems-index.

##### 2A-W12  Systems-index Depends On lags the GDD tables
Index (authoritative) versus GDD upstream lists:
- Row 4 CPA (index: TDC, Roster soft, Round): CPA lines 312-317 also list Session (soft), LSC (soft), IVP (soft).
- Row 7 IVP: IVP line 388 lists TDC (soft); TDC line 363 lists IVP (soft). Missing in index.
- Row 8 LSC: LSC line 526 lists SCL (soft, values). Missing.
- Row 10 Camera (index: IIF, SCL, TDC): Camera lines 520-525 add Roster, Round, IVP, CON, ASN, PEN (soft); Roster line 517 and Round lines 517 and IVP line 411 list Camera as dependent.
- Row 15 OC: CON hard peer (see W10).
- Row 17 TMK (index: Roster, IVP, CON, Camera, TDC): TMK lines 357-361 add ASN, SCL, Round, Session, PEN (soft), and HUD per W4.
- Row 3 IIF: see W5. Row 25 Onboarding does not list IIF though IIF line 496 lists Onboarding as a soft dependent (not written; low).
- Fix owner: systems-index (orchestrator).

##### 2A-W13  One-sided or label-mismatched rows (smaller)
- PEN Downstream (lines 368-377) lacks Crowd, although Crowd Upstream line 369 "NPC-Kill Penalty & Informants (14) | Exile body release timing and `AmbientExit` request ... Soft (downstream user too)".
- ASN Upstream (lines 312-321) lacks CPA, although CPA Downstream line 326 lists "NPC-Kill Penalty & Informants (14) / Assassination & Defence Reversal (13) ... Soft (rule consumers)" (CP-12b) and ASN Downstream line 330 lists CPA the other way.
- CON Upstream line 268 says Crowd is "Hard for any build that leaves the team" for NPC bush use; Crowd Downstream line 380 lists Concealment only as "Their Tell Matrix rows ... Soft", so the hard supplier role is not acknowledged.
- Roster Upstream line 495 "none"; Crowd Downstream line 376 lists Roster as a dependent of Crowd (grace driving, AC-PSR-40b, 43). Roster line 512 lists Crowd as Soft.
- OC Downstream line 315 lists Procedural City Generator ("Must produce courts that pass LSC V-FAIR ... final-eligible target 6"); PCG Upstream (lines 322-327) has no Objectives row, index row 16 lacks it, and PCG line 317 says "no dependents ... a leaf" and that consumers include Objectives. Direction and presence disagree.
- Session Upstream lists CPA (line 452, PoolHash, Soft) but CPA Downstream (lines 323-329) has no Session row.
- Fix owner: each named GDD; low priority, bundle in one dependency-table pass.

##### 2A-W14  Stale "not written / provisional / pending / not in the index" claims
- HUD line 603: "No ADR exists yet (`docs/architecture/` holds only the TR registry)". False: ADR-0001..0023 exist in docs/architecture/ (accepted). HUD should change.
- IVP Cross-References line 497 "**Concealment (not written):**" and line 501 "**Teammate Markers (not written):**". Both GDDs exist (In Review). IVP should change.
- Round line 521 "The index should add Level Socket Contract, Character Pool & Appearance and the Input & Interaction Framework to Round's dependencies"; index row 6 already lists all three. Round line 497 "Hard at runtime, **provisional**" for SCL and line 494 "(partly provisional)" for LSC although both are In Review.
- CON line 284 "Change requests: add Round, TDC and Crowd (hard) and Camera (soft) as Concealment upstream"; index row 12 has all four. IIF line 500 "The change requests are: add Roster and Round ... in the index": done in row 3. OC line 321 "add Crowd (soft) and Session (soft)": done in row 15.
- Cross-References Status cells carrying superseded review states: OC lines 382-388 ("Reviewed, MAJOR REVISION NEEDED" for LSC, Round, TDC), ASN lines 469-482 ("Reviewed, NEEDS REVISION" / "MAJOR REVISION NEEDED"), PEN lines 449-464, CON lines 365-375 ("Designed (pending review)"), Camera lines 646-648 ("designed (pending review)"), CPA lines 403-405 ("provisional") and Roster line 497 "Every row is **provisional** until the other GDD exists" with line 501 "API names to be reconciled (OQ14)". All referenced GDDs are now "In Review" in the index.
- PCG line 337 "TDC lists it as Hard with 'size limits, retry budget, own keys (provisional)'"; TDC line 355 now reads "own keys (registered, see Registered Owner Keys)".
- PCG line 438 (follow-ups 12): "(b) ... a cross-field invariant `CityGenCourtTargetCount >= ...`", "(c) the systems index risk table ... still says 'integer-grid FRandomStream'", "(d) Objectives OQ 9 still says 'LSC floor still 4'". All three are now false: TDC has I-PCG-5 and I-PCG-6 (lines 273, 606), index line 229 reads "integer-grid generation on the project's PCG32 streams (ADR-0006; `FRandomStream` is not used for layout)", OC line 485 reads "the LSC floor is now 6". Item (a) is done in the registry (entities.yaml line 1765).
- Pings "stale" statements that are now false: TDC line 433 "Informant Pings (VS): register the migrated prototype keys" and TDC line 463 "`PingLifetime`, `PingCooldown` ... Informant Pings (Vertical Slice)"; Pings is MVP (index row 20; TDC lines 358 and 495 register five keys). Pings line 226 (Camera row): "Pings are a third consumer of the same function (no Camera change)" although Camera line 539 now lists Pings as a Hard dependent.
- Round line 623 (change request 21, lean re-review): "confirm that the Roster accepts `RequestTransition(Informant, Disconnect, ...)` for a `Connection` Session has marked `Expired`" is still worded as open; Roster lines 224 and 692 state it is accepted (D9). Round's received table (lines 627-665) has no row recording the closure.
- Round line 592 ("Written (Formulas/Edge Cases still open there)" for LSC) and PCG line 415 ("MAJOR REVISION NEEDED 2026-10-24 ... re-review pending") are superseded status text.
- Fix: owners of the listed GDDs; a docs-only pass.

##### 2A-W15  Decoy early-release maximum still has no registered invariant (carried over from 2026-10-24 C-W1)
- Crowd lines 85 and 415 define `CrowdDecoyEarlyReleaseMinSec`/`MaxSec`; TDC line 268: "I-OC3 | Objectives | `TendCueDelaySec + 0.25 >= CrowdDecoyEarlyReleaseMinSec` ... and `TendCueDelaySec <= ObjectiveInteractSec - 1.0` | reject". No TDC invariant or Crowd/OC rule states `CrowdDecoyEarlyReleaseMaxSec < ObjectiveInteractSec`. The registry (entities.yaml line 1998) holds only `CrowdDecoyEarlyReleaseMinSec`; the Max key does not appear in the registry. Fix: design call (add invariant to TDC and the Crowd/OC knob tables) and registry entry for the Max key.

##### 2A-W16  Camera still says the runtime clamps I-CS-1; IIF safe minimum still rejected (carried over from C-W17)
- Camera line 587: "I-CS-1: `CameraToggleBlendSec` <= `CameraToggleLockoutSec` - 1 frame. The lint enforces it, and the runtime also clamps the blend"; TDC line 240 classes I-CS-1 as "reject" and TDC rule CR-6 says invariants never repair. IIF line 543: `CameraToggleLockoutSec` default 0.30, hard 0-1, safe 0.2-0.5; Camera line 576: `CameraToggleBlendSec` default 0.20, hard 0-0.29. A safe lockout of 0.2 with the default blend of 0.20 violates I-CS-1 (rejected preset). Fix: Camera (remove "runtime clamps") and IIF (safe minimum above 0.2 + one frame) or Camera (lower blend default); design call.

##### 2A-W17  SCL I-SCL-5 still reads a "Crowd pace set" (carried over from C-W19)
- SCL line 579: "I-SCL-5 min(Crowd pace set) >= `WalkSpeedScaleMinMult`"; SCL line 522: `WalkSpeedScaleMinMult` default 0.80, hard 0.6-1.0, safe 0.75-0.90. The pace set is the fixed {0.8, 0.9, 1.0} of R5.1 (line 66), so safe values 0.85 and 0.90 violate the invariant although inside the safe range, and no key holds the pace set. Fix: SCL (name the set's minimum as a constant or narrow the safe range to 0.75-0.80).

##### 2A-W18  Latched final Complete is still not logged (carried over from S-I1)
- OC lines 257-260: the second final "returns `Rejected_Latched` and its hold ends by the normal blend"; OC line 245: "`FinalCompleted` | Team T's completion times since `Active`" counts only the latching completion. No text adds an Event Log row or counter for a final Complete rejected by the latch (OC F5 objective-win share). Fix: OC (and Round R19 entry list) if the measurement is wanted; design call.

#### Checked and consistent (brief)

- Pings row 20: every system it lists upstream lists it back except IIF (W1), Session (W2) and HUD content (W3): Roster line 510 (Hard), IVP line 412 (Hard), Round line 516 (Hard), Camera line 539 (Hard), HUD line 423 (Hard), PEN line 372 (Hard), TDC line 358 (Hard), TMK line 369 (Soft). Hard/soft labels agree. Index row 20 matches Pings Upstream exactly. Pings Downstream (EFC 23, Settings 24, Telemetry 26) are index-listed, not-yet-written VS/Alpha systems (Warning-level by rule, no action).
- TDC Downstream: all 15 hard dependents named in line 366 (rows 3, 4, 6, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 20) list TDC upstream with Hard; Session, IVP soft rows agree. C-W13 half "TDC calls TMK Soft vs TMK Hard" is fixed (TDC line 357 Hard).
- Roster Downstream agrees with every GDD that lists Roster (Session, Round, IVP, SCL, ASN, PEN, CON, OC, TMK, Pings, HUD, Crowd, CPA, IIF, LSC, Camera).
- HUD Upstream vs each supplier Downstream: Roster, Round, IVP, ASN, PEN, OC, CON, IIF, TMK, Camera, Session, TDC all list HUD with matching labels; index row 18 matches HUD Upstream.
- ASN, PEN, OC, CON, Camera, Crowd (D2/D14 Crowd side), TMK, PCG, LSC, SCL, CPA pairs not named in W1-W13 reconcile (both directions present, labels agree).
- Decision D2: Crowd line 357 and AC-CNB-04(c) consume `StopMinSec`; SCL lines 223, 528, AC-SCL-50 own it. D14: Crowd line 73 G1 and SCL line 529 agree; no pause-menu GDD is named as an owner in the Dependencies tables (pending in prose scan).
- Rule IDs in Cross-References rows checked so far exist under the declared names (see Coverage).

#### GDD flags

| GDD | Finding IDs | Priority |
|---|---|---|
| input-interaction-framework.md | W1, W5, W14 | High (new Pings row; four missing upstream rows) |
| informant-pings-team-comms.md | W2, W3 | Medium |
| session-travel.md | W2, W7, W13 | Medium |
| hud.md | W3, W4, W14 | Medium (stale ADR claim) |
| teammate-markers.md | W4, W5 | Low |
| round-match-framework.md | W6, W14 | Medium |
| information-visibility-policy.md | W7, W14 | Low |
| shared-character-locomotion.md | W7, W8 | Medium |
| npc-kill-penalty-informants.md | W9, W13, W14 | Medium |
| concealment-bushes.md | W9, W10, W13, W14 | Low |
| objectives-chain.md | W10, W13, W14 | Low |
| crowd-npc-behaviour.md | W11, W13 | Low |
| camera-system.md | W5, W14 | Low |
| character-pool-appearance.md | W7, W13, W14 | Low |
| level-socket-contract.md | W5, W7 | Low |
| procedural-city-generator.md | W13, W14 | Low |
| participant-state-roster.md | W13, W14 | Low |
| assassination-defence-reversal.md | W13, W14 | Low |
| tuning-data-config.md | W14 (Pings still "VS" at lines 433, 463), W15 | Medium |
| systems-index.md | W10, W11, W12 | Medium (orchestrator) |

Additions to rows above from W15-W18: crowd-npc-behaviour.md and objectives-chain.md (W15, W18), camera-system.md and input-interaction-framework.md (W16), shared-character-locomotion.md (W17), round-match-framework.md (W14, W18), participant-state-roster.md (W14 note on Round CR 21 closure).

## Appendix B — Phase 2B knob ownership, registry and TDC (detail, sub-agent output verbatim)

### Review-all-GDDs Phase 2B: tuning-knob ownership and registry/TDC value consistency (2026-10-25)

Status: COMPLETE. Report only; no GDD, registry, index or ADR was edited. Severity rubric as briefed (referenced key or invariant that does not exist = Blocking; duplicate owner or default/safe value rejected at other keys' defaults = Warning; Blocking if the shipped Default preset itself would be rejected).

Result: 0 Blocking, 11 Warnings (2B-W9 and 2B-W10 are grouped, with lettered sub-items).

#### Coverage

Checked:
- TDC: Canonical Tunables, Registered Owner Keys (lines 78-126), Formulas F-1 to F-4 and the whole owner-invariant table (161-288), Downstream table and Tuning Knobs (327-380).
- All 18 owner Tuning Knobs sections (ASN, Camera, CPA, CON, Crowd, HUD, Pings, IVP, IIF, LSC, Penalty, OC, Roster, PCG, Round, Session, SCL, TMK), read in full.
- Key counts recounted from the owner tables and compared with TDC: Round 17, Session 13, Roster 2, IIF 25, CPA 3, SCL 53, Camera 49, IVP 4, Crowd 53, CON 11, ASN 16, Penalty 5, OC 11, PCG 32, TMK 7, Pings 5, HUD 21, LSC about 84 (my count; see 2B-W3).
- Registry: all 299 entry names listed; default, hard range, safe range, audience and latch compared against the owner tables for every mirrored constant (about 200 keys); all D-decision entries read in full.
- Every shipped default was tested against every TDC owner invariant (I-1 to I-5, I-RMF, I-ST, I-IIF, I-LSC, I-SCL, I-CS, I-CNB, I-CON, I-ASN, W-ASN, I-NKP/W-NKP, I-OC, I-PCG, I-TM, I-HUD, I-PNG): all pass (I-ASN-7, I-ASN-8, I-PCG-6, I-CNB-1, I-CNB-2 and I-LSC-1 pass with zero margin).
- Stale-value greps over design/gdd (excluding review logs): `NpcKillStrikeLimit` 1-5, `NpcCount` 40 and old ranges, `315 s`, `BushEnterSwapSec` 0.45, `LatencyWarnRttSec`, old CR-10 names, `HudReadHoldScale`, TeamPing 10 cm quantisation.

Could not check / limits:
- Formula-level re-derivations (Crowd I-CNB-5 tell-chain bound, O-CNB-6/7, IVP F5, LSC D6 beyond the defaults, ASN `r_c` capsule radius value for I-ASN-1 at the safe corners) were not recomputed; the SCL capsule radius was not located, so I-ASN-1 at the corners is unverified.
- Audience/latch of owner keys was compared with the registry only; TDC's per-owner Audience/Latch summary column was read, not cross-tested key by key.
- The Read tool did not truncate any line I needed; no `sed` was available (no Bash tool), so the 27 over-length lines were reached only via Grep `-o` or a one-line Read.

#### Blocking

None.

#### Warnings

##### 2B-W1 SCL `IdleGazeDelaySec` (D14): audience and latch disagree between SCL and the registry; TDC does not name the key
- SCL Tuning Knobs line 529: `| IdleGazeDelaySec | float | SCL | 3 | 1-10 | 2-5 | ServerOnly | Use |`. Registry entities.yaml 4832-4846: `audience: Client`, `latch: Round`, and `referenced_by` lists `design/gdd/tuning-data-config.md`.
- TDC never mentions `IdleGazeDelaySec` or D14 (grep: no hits). TDC line 110 (SCL row) says "52 ... plus `StopMinSec`" = 53, while the SCL table has 53 rows including both `StopMinSec` and `IdleGazeDelaySec`; so TDC's 52 either already includes the key unnamed or is one too high.
- The scan parameters (SCL R8.9 line 107: sweeps U(30, 60) deg every U(3, 7) s; Crowd G1 line 73) are unkeyed constants ("to be registered with Crowd change request 22"), so the D14 behaviour is not fully tunable from data (known, tied to Crowd OQ-14).
- Change: SCL and registry must agree on audience/latch (the head scan is server-drawn per SCL R8.9, so ServerOnly/Use in SCL is internally consistent; the registry is the odd one out); TDC SCL row should name the key and fix the count.

##### 2B-W2 TDC Downstream table still says 48 Camera keys
- TDC line 351: "48 Camera keys (`Client`)"; TDC line 111 and the Camera table (lines 550-584) give 49 (after `CameraMaskOffHysteresisCm`, D6). Change TDC line 351.

##### 2B-W3 TDC LSC key count is "about 79"; the LSC table lists about 84
- TDC line 109: "about 79 (incl. `LayoutExactRefineMaxCount` ...)". LSC Tuning Knobs (lines 549-618), counting names (pairs counted per name, the two SCL traversal heights excluded): about 84. Every other owner has an exact count, and the AC-3a schema test counts names. Change TDC (state the exact number).

##### 2B-W4 (carried over from 2026-10-24 C-W19) SCL I-SCL-5 still reads a "Crowd pace set" that no key holds
- SCL line 579: "I-SCL-5 min(Crowd pace set) ≥ `WalkSpeedScaleMinMult`"; SCL line 198: "min(set) ≥ `WalkSpeedScaleMinMult` (I-SCL-5)". The set {0.8, 0.9, 1.0} is a constant (IIF IA_WalkSlower, SCL R5.1), `CrowdFullPacePct` is a different quantity. TDC line 237 evaluates I-SCL-5 as reject.
- SCL line 522 still gives `WalkSpeedScaleMinMult` safe 0.75-0.90 (hard 0.6-1.0), so 0.81-0.90 are rejected (above the set minimum 0.8) and the "1.0: no drift" sentence in the row is unreachable.
- Change: SCL restates I-SCL-5 as `WalkSpeedScaleMinMult ≤ 0.8` (constant), safe 0.75-0.80; TDC row text follows.

##### 2B-W5 (carried over from 2026-10-24 C-W17) Camera I-CS-1 text says "clamps"; IIF lockout range still collides with it
- Camera line 587: "The lint enforces it, and the runtime also clamps the blend". TDC CR-6 and TDC line 240 treat I-CS-1 as reject (never clamp).
- IIF line 543 `CameraToggleLockoutSec` default 0.30, hard 0-1, safe 0.2-0.5; Camera line 576 `CameraToggleBlendSec` default 0.20, hard 0-0.29. I-CS-1 needs blend ≤ lockout − 1 frame: lockout 0.2-0.217 (safe) is rejected at the blend default; blend hard max 0.29 is rejected at the lockout default 0.30 (0.29 > 0.2833 at 60 fps); lockout 0 (hard) rejects every blend. Registry 1694-1708 repeats IIF's range.
- Change: Camera drops "runtime also clamps"; IIF raises the safe minimum to about 0.25 (and its hard minimum above the blend hard max), or Camera lowers the blend hard max to 0.28.

##### 2B-W6 (carried over from 2026-10-24 C-W1) Crowd `CrowdDecoyEarlyReleaseMaxSec < ObjectiveInteractSec` is still not a registered invariant
- Crowd line 415: "Max < `ObjectiveInteractSec`; Max ≥ `D`: early releases become completions". TDC owner-invariant table (lines 221-282) has no row for it; `CrowdDecoyEarlyReleaseMaxSec` is also absent from the registry (only the Min key is registered).
- It is live: Crowd safe maximum is 3.0 (= the OC default) and the default 2.8 breaks at OC safe values 2.5-2.8. Change: TDC adds the invariant (warn or reject), Crowd cites it; optionally register the Max key.
- Other C-W items re-checked: C-W22 is fixed (I-PCG-5 at TDC line 273; PCG safe range 6-8 at PCG line 354).

##### 2B-W7 `LayoutWalkableCellsMaxCount` default sits outside its own safe range
- LSC line 555 (one row for the Min/Max pair): default 2,000 / 3,250, hard 1,500-4,000, safe "2,000-2,700"; registry 2983-2997 repeats `value: 3250`, `safe_range: 2,000-2,700` (widened by the NpcCount 50 change). The shipped Default therefore logs a safe-range warning at every load (not a rejection; CR-6 warns, keeps). Change LSC and registry: give the Max key its own safe range containing 3,250 (for example 2,700-3,600) and add a Min <= Max note.

##### 2B-W8 Cross-system keys with no registry constant, including the D4 key
- Closeout D4 says "LSC D5/V-SEP and registry updated", but `StartToFinalMinPathCm` (LSC line 585, 4,400; used in PCG lines 67/354/438/531, OC line 485, LSC lines 145/336) has no entry in entities.yaml (grep: no hit; only a note on `CityGenFinalEligibleTargetCount` at 1764).
- Also absent although read by two or more GDDs and named in TDC invariants: `CrowdFullPacePct` (Crowd, IIF, SCL), `WalkSpeedScaleMinMult` (I-SCL-5/6), `WalkStopSec` and `InputStarvationSec` (I-SCL-4/12/15), `PrelimCandidatesPerStageMinCount` (I-PCG-6), `BushEnterDurSec` (I-CON-a), `CameraToggleBlendSec` (I-CS-1), `CrowdDecoyEarlyReleaseMaxSec` (2B-W6). The registry is a subset by design (299 entries against about 400 keys), so this is a gap list, not a violation of a rule; the D4 claim is the only stated promise.
- Change: registry mirror (orchestrator) adds `StartToFinalMinPathCm` first; the rest are optional.

##### 2B-W9 Safe range values that a registered reject-invariant refuses at the other key's default
Each is a value inside the owner's stated safe range that fails the invariant when the other keys sit at their defaults.
- a. I-ASN-8 `StrikeBearingLimitDeg ≤ 60 + StrikeWarpYawLimitDeg`: ASN line 345 safe 80-95 (registry 3583-3593 same), bound 90 at yaw 30; 91-95 rejected. Change ASN safe max to 90.
- b. I-CS-3: Camera line 571 `EyeConeHalfHDeg` safe 52-60 against `FovHMaxDeg`/2 = 54 at default 108, `EyeConeHalfVDeg` safe 36-42 against `FovVMaxDeg`/2 = 37.5; 52-53.9 and 36-37.4 rejected. Change Camera safe minima to 54 and 38.
- c. I-LSC-2 `(1 + LayoutMaxReseedAttemptsCount) × LayoutAttemptTimeoutSec ≤ LayoutTotalBudgetSec`: LSC lines 550-552 reseeds safe 3-6 (6 gives 49 > 45), `LayoutTotalBudgetSec` safe 30-60 (30-41 fail against 7 × 6 = 42). Change LSC (reseed safe 3-5, budget safe 42-60) and the registry rows 2922-2933, 2966-2980.
- d. I-PCG-6 `CityGenCourtTargetCount ≥ FinalCandidatesMinCount + 4 × PrelimCandidatesPerStageMinCount − 2`: LSC line 599 safe `PrelimCandidatesPerStageMinCount` 3-5 (4 gives 20 > 16) and `FinalCandidatesMinCount` TeamSize+1 to 8 (7 and 8 give 17 and 18 > 16) are rejected at the Generator default 16; PCG line 357 safe 16-18. Change LSC safe ranges (Prelim 3, Final 6) or widen PCG's hard/safe maximum.
- e. I-CNB-1 `CrowdIdleMinSec ≥ SessionSoftTimeoutSec + 1`: Session line 479 `SessionSoftTimeoutSec` safe 2-5 against Crowd default 4; Soft 4 and 5 rejected. Change Session safe max to 3.
- f. I-CNB-2 `CrowdMaxIdleSec ≥ max(CrowdIdleMaxSec, CrowdConversationMaxSec)`: Crowd line 402 `CrowdMaxIdleSec` safe 15-25 against `CrowdConversationMaxSec` default 20 (safe up to 20); 15-19.9 rejected. Change Crowd safe min to 20 (registry 2085-2099 same).
- g. I-HUD-6 with I-HUD-3 (`HudRemovalCardDelaySec = ExileBeatSec`): Penalty line 389 `ExileBeatSec` safe 0.75-1.2 against `DeathFallSec + NoPawnFadeSec − HudRemovalCardInSec` = 1.1 at defaults; 1.1-1.2 rejected. Penalty's text cites only I-HUD-3 and I-HUD-5; HUD line 462 safe delay is 0.7-1.0, so ExileBeat 1.01-1.2 is also outside HUD's safe range. Change Penalty safe max to 1.0 and cite I-HUD-6 (registry 2413-2426 follows).
- h. I-SCL-6 `WalkTurnRateDegPerSec ≤ 0.573 × WalkSpeedCmPerSec × WalkSpeedScaleMinMult`: SCL line 521 `WalkSpeedCmPerSec` safe 125-160; at defaults (turn 60, MinMult 0.8) the bound is 57.3 at 125, so 125-130.8 are rejected. Change SCL safe min to 131 (or lower the turn-rate default).
- i. I-CON-a `BushEnterDurSec ≥ BushEnterSwapSec + ConcealCullDelayMs/1000` (CON F3 line 174): CON line 293 `BushEnterSwapSec` safe 0.46-0.7 against default enter 0.9 and cull delay 250 ms; 0.66-0.7 rejected. Change CON/registry (1571-1585) safe max to 0.65.

##### 2B-W10 Warn-only invariants whose bound lies inside the owner's safe range (a safe value warns at the others' defaults)
- a. I-OC2: OC lines 330-332 safe Min 30-60, Max 150-200, Q 20-45; `W_eff` warns below 163 s: Max 150-162 and Min 33-60 warn at the other defaults. Rejection (< 100) is not reachable in the safe ranges.
- b. W-NKP-5 `ExileCardTimeoutSec ≥ StrikeContactSec + BushStrikeEmergeSec + ExileBeatSec + 0.3`: Penalty line 391 safe 1.5-2.5 against 1.85 at defaults (1.5-1.84 warn); `ExileBeatSec` 1.2 gives 2.25 > the 2.0 default.
- c. W-NKP-3 `StrikeOutReleaseGraceSec ≥ AttackAttemptLockoutSec`: Penalty safe 1.75-2.5 against ASN safe lockout up to 2.5; 1.75-1.99 and lockout 2.01-2.5 warn at the other default.
- d. I-ASN-7 warns at the ASN safe corner (240 / 0.30 = 800 cm/s); ASN line 368 already says so (documented, no change needed other than aligning safe ranges).
- Change: tighten each owner safe range to the warn-free band, or state the warning band explicitly in the knob row.

##### 2B-W11 Other cross-key couplings with no registered invariant, and quantity-naming leftovers
- IVP line 427: `ConcealCullDelayMs` "must be a multiple of 50"; hard 100-1000 admits 125, and TDC CR-6 forbids repair. Needs an invariant or a hard-range step note.
- PCG line 356 `CityGenZoneCandidatesCount` (hard "2 or 4") against LSC line 576 `ZoneCandidatesMinCount` (hard 2-4, default 2): two names for "candidate zones per layout" with no `≥` rule (LSC 3 and PCG 2 or 4 conflict). Change TDC/LSC/PCG.
- LSC `ReachDetourMult` and `DetourMaxMult` are still two keys held equal by I-LSC-4 (TDC line 126 flags it; both mirrored in the registry, 3246 and 2247). Retire one.
- SCL `StopMinSec` (hard 0.2, safe 0.4) can sit below the 0.5 s sudden-stop bound that I-SCL-4 states for `WalkStopSec` (SCL line 527); no invariant ties the two keys (a 0.4 s tap stop is allowed in the safe range).
- TMK line 435 `TeammateMarkerLargeMult` hard "1.0 or 1.5" and "values other than the two steps are rejected"; TDC F-1 clamps continuous ranges and has no discrete-set rule (CR-14 settings may be outside F-1, but the registry row 4647-4661 presents it as a hard range).
- Round line 360 `NpcCount` floor `max(2 × TeamSize, 24 − P)` omits the I-LSC-1 floor (35 at defaults); consistent with TDC's hard minimum 6 but not with its safe minimum 35.

#### Checked and consistent

- Per-owner key counts equal to TDC lines 104-121: Round 17, Session 13, Roster 2, IIF 25, CPA 3, Camera 49 (line 111; line 351 stale, 2B-W2), IVP 4, Crowd 53, CON 11, ASN 16 (3 canonical + 13), Penalty 5, OC 11, PCG 32, TMK 7, Pings 5, HUD 21; SCL 53 (see 2B-W1).
- No key is owned by two GDDs with differing values: canonical rows (TDC lines 84-92) equal the owner tables for `TeamSize`, `NpcCount`, `AssassinationRangeCm`, `DefenceDurationSec`, `DefenceCooldownSec`, `NpcKillStrikeLimit`, `ObjectiveInteractSec`, `TargetRoundMinutes`; `LatencyWarnRttSec` exists only as retired (IIF line 542, registry 2891 `status: deprecated`, `replaced_by: HudLatencyWarnRttSec`); every live mention of the old name carries the Hud prefix or says retired.
- D2 `StopMinSec`: SCL 0.5 / 0.2-1.0 / 0.4-0.7 / Client / Round = registry 4816-4830 = TDC line 110; Crowd lines 65, 357, 551 consume the same key.
- D6 `CameraMaskOffHysteresisCm`: Camera 10 / 5-30 / 8-15 = registry 4849-4863 = TDC line 111.
- D7 `CrowdBushIntervalSec` default 300, hard 120-320, safe 240-315 = registry 1954-1968; line 416 "lowered from 315" is explanatory.
- D8 HUD hard minimum 1.25: HUD lines 456 and 463 = registry 2647 and 2760 = TDC line 276; I-ASN-3 ceiling 1.2167 below it.
- D4 `StartToFinalMinPathCm` 4,400 (44 m): LSC lines 145, 336, 585; PCG lines 67, 354, 518, 531; OC line 485 agree; `CityGenFinalEligibleTargetCount` 6, safe 6-8; I-PCG-5 and I-PCG-6 present (but see 2B-W8 for the missing registry key).
- D12 W-NKP-4: hard-range corner 1.2 + 2.0 + 1.5 = 4.7 ≤ 5 (`InformantRespawnDelaySec` hard minimum), so "unreachable in hard ranges" is true.
- `BushEnterSwapSec` safe 0.46-0.7: CON line 293 = CON F3 line 179 = registry 1571-1585; no live 0.45.
- `NpcCount` 50 (hard 6-100, safe 35-50) TDC line 85 = registry 3102-3128; `NpcKillStrikeLimit` 3, hard 2-5, safe 2-4 = TDC line 89 = Penalty line 387 = registry 3129-3150; no live "1-5" (only a review log) and no live NpcCount 40 range (three explanatory notes). TDC F-3 arithmetic (0.249, 10, p range, 0.0197 to 0.868, 2.5) re-verified.
- IVP 10 cm is the team position feed grid (`TeamPositionFeedGridCm`, IVP line 150), not TeamPing; TeamPing quantisation is 0.1 cm in IVP lines 81 and Pings line 151 and no stale "10 cm" remains in a GDD (only in review logs).
- CR-10 renames: no old name in any live GDD or registry row (only in TDC's rename log, lines 126 and 434, and in review logs).
- Closeout "registry missing keys" list (f): all present in entities.yaml (IIF 10 keys, OC 5, Round 5 plus `RevealPawnWaitMaxSec`, TMK 5, HUD 6, Session 4, `StopMinSec`, `IdleGazeDelaySec`, `CameraMaskOffHysteresisCm`, `CityGenCourtTargetCount`). `pawn_network_budget` registry line 1323 now reads 31.1 KB/s against the 36 KB/s line.
- Pings (g): five keys in TDC line 120 and the Pings table agree on default, hard, safe, audience and latch; all five in the registry (4200-4275); I-PNG-1 in TDC line 282 matches Pings line 253 (warn); Pings is a controller RPC outside the IIF transport buckets, so I-IIF-1/2 are unaffected.
- Invariants cited by owners that exist in TDC with the same failure mode: I-ASN-1 to 8, W-ASN-9 to 11, I-HUD-1 to 7, I-TM-1, I-PCG-1 to 6, I-OC1 to 3, I-LSC-1 to 4, I-CS-1 to 5, I-CNB-1 to 5, I-CON-a to e, I-IIF-1/2 (22 ≤ 30, 12 ≤ 20), I-RMF, I-ST, W-NKP-2 to 5.
- All shipped defaults pass every registered invariant (no Default-preset rejection).

#### GDD flags

| GDD | Finding IDs | Priority |
| ---- | ---- | ---- |
| tuning-data-config | 2B-W1, W2, W3, W6, W8, W11 | Medium |
| shared-character-locomotion | 2B-W1, W4, W9h, W11 | Medium |
| camera-system | 2B-W5, W9b | Medium |
| input-interaction-framework | 2B-W5 | Medium |
| crowd-npc-behaviour | 2B-W6, W9f | Medium |
| npc-kill-penalty-informants | 2B-W9g, W10b, W10c | Medium |
| level-socket-contract | 2B-W3, W7, W9c, W9d, W11 | Medium |
| procedural-city-generator | 2B-W8, W9d, W11 | Low |
| session-travel | 2B-W9e | Low |
| assassination-defence-reversal | 2B-W9a, W10c, W10d | Low |
| concealment-bushes | 2B-W9i | Low |
| objectives-chain | 2B-W6, W10a | Low |
| information-visibility-policy | 2B-W11 | Low |
| teammate-markers | 2B-W11 | Low |
| round-match-framework | 2B-W11 | Low |
| entity registry (design/registry/entities.yaml) | 2B-W1, W7, W8, W9c, W9f, W9g, W9i | Medium |

## Appendix C — Phase 2C formula compatibility (detail, sub-agent output verbatim)

### Phase 2C findings: formula compatibility (checklist 2e), review-all-gdds 2026-10-25

Status: COMPLETE (final). Report only; no GDD, registry, index or ADR was edited. Result: 0 Blocking, 15 Warnings (2C-W1 to 2C-W15). Two carried-over items re-checked: C-W1 still present (2C-W4), C-W22 closed with a residual risk (2C-W10).

#### Coverage

Method: every formula below was recomputed by hand from the GDD's own variables and compared with the registry baseline (`design/registry/entities.yaml`) where the value is registered. The Bash tool was not available to this agent, so long lines were read with Read or Grep `-o` context; no truncated line affected a conclusion.

Chains checked:
1. Locomotion -> Crowd / Concealment / ASN / OC / Camera / IIF: SCL F1-F3 (incl. D2 stop floor), F9, F10 -> ASN F1 (reach, flee 151.75 cm), Crowd F1-F4 (paces, stop floor, pause model), CON F3(d), OC F1/F2, Camera F1-F6, IIF F1-F6. Checked.
2. Strike timing chain: ASN `StrikeContactSec` / `BushStrikeEmergeSec` -> Penalty F2 / W-NKP-5 -> HUD F2-F4, F7, F10 (D8 minimums, I-HUD-1/2/3/5/6) -> TMK F6 -> Camera death fade. Checked.
3. Crowd density at NpcCount 50: Crowd F1/F2/F3/F5, CON F1/F2 (D1, D7), IVP F2/F5/F7/F8, LSC D4/D5/D6/D6b, Generator F1/F2/F6, ASN F5, OC F3/F4, base rate 5/55. Checked.
4. Layout: LSC V-SEP / D5 (44 m, 44.4 m bound), Generator S4/F1/F6/F7, `FinalCandidatesMinCount` 6, I-PCG-5/6, emergence keys. Checked (arithmetic re-derived; the real pass rate needs the OQ 9 script).
5. Network budgets: SCL F9 (31.1 vs 36 KB/s), IVP F6 (13 B/entry, 104 B/s), Pings F4, Roster F7, Session F8, TMK F5, ADR 0003/0010/0018 and `architecture.md` figures. Checked.
6. Pings: lifetime 8 s, bucket, range, quantisation 0.1 cm, bandwidth, edge clamp vs TMK F3 (bias ramp). Checked.
7. Round timing: Session F2/F4/F7, Round F4/F6, Roster F6, `StartLeadSec`, `EndedMinDwellSec`, `RevealPawnWaitMaxSec`, Penalty T_due. Checked (brief).
8. Camera: mask hysteresis 20/30 cm vs SCL capsule and sweep radius; `CameraToggleLockoutSec` vs I-CS-1. Checked.
9. D14 idle gaze vs Crowd G1/G2/G6 and T13. Checked (brief, one estimate).

NOT CHECKED: Character Pool F-formulas (CPA line 116; only the cited 120-180 B figure was seen via grep); TDC Formulas section (only the invariant rows I-HUD, I-PCG, I-CNB, W-NKP-5 were read); LSC D1, D2 (arithmetic only), D7, D8, D9 internals; Crowd F6 parity thresholds (arithmetic only); IIF acceptance criteria beyond AC 49/52/53; registry formula output_range text beyond `pawn_network_budget` and `socket_counts`.

#### Blocking

None. No upstream formula's output at default values falls outside a downstream formula's valid input so that a stated rule or AC cannot hold, and no formula references an undefined key. (`StartToFinalMinPathCm` is defined in LSC but has no registry entry: see 2C-W10.)

#### Warnings

##### 2C-W1. `ExileBeatSec` safe range 0.75-1.2 is not satisfiable in its upper part (I-HUD-6, W-NKP-5)
- Registry `ExileBeatSec` safe_range 0.75-1.2 (entities.yaml ~line 2424); TDC line 116 "safe range 0.75-1.2 (bounded by I-HUD-3 and I-HUD-5)".
- I-HUD-3 sets `HudRemovalCardDelaySec = ExileBeatSec`; I-HUD-6 (TDC line 280) rejects if `HudRemovalCardDelaySec + HudRemovalCardInSec > DeathFallSec + NoPawnFadeSec`. At defaults `Beat + 0.6 <= 1.2 + 0.5`, so `Beat <= 1.1`. Beat 1.2 gives 1.8 > 1.7: rejected.
- W-NKP-5 (TDC line 265), bush variant: `2.0 >= 0.4 + 0.35 + Beat + 0.3` gives `Beat <= 0.95`; Beat 1.0 gives 2.05, 1.2 gives 2.25 (warn).
- Effective safe range at defaults is 0.75-0.95 (warning-free), 0.75-1.1 (not rejected). Lower end: I-HUD-5 `Beat >= 0.25 + 0.5` leaves 0.05 s margin.
- Also: `HudResultPlateMaxHoldBackSec` 1.25 + Beat 0.8 = 2.05 > `ExileCardTimeoutSec` 2.0, so on the fallback path (no `ContactServerTime` in the result) the card can land after the Camera timeout; the documented behaviour (PEN-R16) covers it, but it is the default, not a corner.
- Change: Penalty (owner) and TDC/registry restate the safe range (0.75-0.95) or raise `ExileCardTimeoutSec` with it.

##### 2C-W2. OC F1 uses v_run 3.5 m/s; SCL `RunSpeedCmPerSec` default is 450 (4.5 m/s)
- OC line 165 "v / v_run ... 1.4 / 3.5 ... Shared Character & Locomotion (provisional)"; SCL F1 and registry `RunSpeedCmPerSec` 450 (safe 400-500).
- `T_rush = kappa*L/v_run + n*D`: L 170: 1.2*170/4.5 + 9 = 54.3 s (stated 67.3); L 120: 41 s (stated 50); L 240: 73 s (stated 91). OC line 173 "T_rush 50-91 s, the absolute floor" should read 41-73 s. The walk branch (1.4 m/s) matches SCL, so T_chain 307-535 s and T_round 7-11 min are unaffected. Matters for the run-dominance concern (cross-review 2026-10-24 lines 639-646): the floor is 18-19 % lower than stated.
- Change: Objectives.

##### 2C-W3. Concealment F1 occupancy figures still use r_N = 0.19 (shipped 0.20 since D7)
- CON F1 example and range (line 153): NPC term `50 * 0.19 * 0.345 / 9 = 0.36`, "E[O] about 0.41 ... 0.19 ... 0.46"; Crowd F2 ships 0.20/min (`CrowdBushIntervalSec` 300 s).
- At 0.20: 50*0.20*0.345/9 = 0.383, +0.046 = 0.429; N_B 8, n_N 50: 0.431 + 0.052 = 0.483; N_B 14, n_N 35: 0.172 + 0.030 = 0.202. m0 = 1 - e^-0.429 = 0.349 (stated 0.34), so the D1 cap `max(40 %, m0 + 10 pp)` is 45 % at defaults, not 44 %; CON-R12's "about 34 % at defaults" is 35 %. Small, but the worked numbers in CON-R12 / AC-CON-24(c) should be refreshed. F2's 0.19 as the requirement and 0.20 as shipped are consistent.
- Change: Concealment.

##### 2C-W4. Carried over from 2026-10-24 C-W1: `CrowdDecoyEarlyReleaseMaxSec` < `ObjectiveInteractSec` is still not an invariant
- Crowd line 415: Max default 2.8, safe 2.2-3.0, hard 1.0-5.0, "Max < `ObjectiveInteractSec`". Registry `ObjectiveInteractSec` hard 2.0-6.0, safe 2.5-4.5. TDC has I-CNB-1..5 only (no I-CNB-6 anywhere).
- Values inside both safe ranges break the decoy model (early releases become completions; Crowd F2 `c` 0.75 and Objectives F3 `P_full` 0.70 stop holding): OIS 2.5 with Max 2.8 or 3.0; hard OIS 2.0 with the default 2.8.
- Change: Crowd + TDC (reject invariant I-CNB-6).

##### 2C-W5. Pings F1 clock-skew tolerance holds only at the default refill rate
- Pings F1: server tolerates epsilon "(100 ms of refill at defaults)". epsilon is a constant 0.05 token, so tolerance = 0.05 / `PingBucketPerSec`: 100 ms at 0.5/s, 25 ms at the 2.0/s range maximum. At 2.0/s a press the client allows can be dropped silently (no refusal feedback, PNG-R3) by 25 ms of skew. Low. Change: Pings (state the formula or narrow the range).

##### 2C-W6. Pings edge ring reuses TM F3 without stating its glyph size and hysteresis inputs
- PNG-R7: ring "clamped ... as a 32 px broken ring ... the same clamp rule as Teammate Markers F3". TM F3 derives `x_min = m*W_v + W/2` from the glyph W/H (28 x 32 at 1080p, F1) and includes the behind-camera bias ramp (sigma, k_b 0.25, eps_b 0.5) and `delta_h`. By reference Pings inherits the ramp (no copy needed), but a 32 x 32 ring gives a 2 px different horizontal inset from the markers and the text does not say whether W/H scale with `u * s_ui`. Low. Change: Pings (one line: W = H = 32 * u * s_ui; sigma and delta_h apply).

##### 2C-W7. LSC D6 misstates the zone-area requirement at the low end of the NpcCount safe range
- LSC line 379: "A_z >= 1,564 m² ... (1,467 m² when the share term's 8 binds, as at `NpcCount` 35), and the `NpcCount` floor is 35 (the V-AREA term binds; the share term gives 34)".
- 1,564 is right at NpcCount 50: (5 + 9 + 3 - 1) / (1.5 * 0.006818) = 1,564. The 1,467 figure reuses NpcCount 50's rho_p: 15 / (1.5 * 0.006818) = 1,467. At NpcCount 35 the largest valid city is A_max = 45 * (35 + 10) = 2,025 cells = 8,100 m², rho_p = 45 / 8,100 = 0.005556, NpcZoneMin 8, NpcZoneMax = min(floor(1.5 * 0.005556 * A_z) - 5, floor(0.6 * 35 / 2) = 10); W >= 3 needs the density term >= 10, i.e. `A_z >= 15 / (1.5 * 0.005556)` = 1,800 m². At NpcCount 40, A = 8,800: 1,760 m². With A_z "about 1,500-2,000", NpcCount 35-40 fails V-ZONEFILL on roughly half the layouts, so the Generator's p_g (design >= 0.30, F6) is not independent of NpcCount, and the floor formula (share term 34, V-AREA 35) omits the density-cap term.
- At the default (50) everything passes. Change: LSC (correct the 1,467 figure, add the density-cap bound to the NpcCount floor or narrow the safe range to 40/45-50).

##### 2C-W8. LSC D4 walk-share rationale uses the non-binding 12-minute target; OC F1 says round length is about 9 minutes
- LSC D4: `S = L / (v * T * 60)`, `T = TargetRoundMinutes` 12, band 0.119-0.238; rationale "above about a quarter ... a walking contest". OC F1/F3/F4 explicitly use `T_round` about 9 min (chain 6.7 min + 2 min contest) and say they "never use the non-binding 12-minute target". With T = 9: L 120: 0.159, L 170: 0.225, L 240: 0.317. The upper half of the band, and OC's own proposed lever "raise the chain target toward 220 m" (0.29), exceed LSC's "a quarter" threshold. Against the chain time itself (402 s at L 170) the walk is 30 %.
- Not a rule failure (the band is a placement predicate on L, not on S), but D4's stated rationale and OC's pacing finding disagree. Change: LSC (restate D4 against `T_round` or drop the "quarter" threshold); OC OQ on chain target should cite it.

##### 2C-W9. Crowd F3 gives N_E 13-16; LSC D5 and the registry allow 12
- Crowd F3 table: "N_E int 13-16 layout (LSC D5)". LSC D5 `N_E = clamp(ceil(A / 700), 12, 24)`; at the Generator's default band A = 8,028-9,216 m² (F1), A <= 8,400 gives 12 (e.g. 8,184 m²: ceil(11.69) = 12). Registry `socket_counts` output_range "N_E 12-13".
- At N_E = 12: lambda = 0.75 * 12 / 60 = 0.150/s (9.0 entries/min), D mean 16.7 s, p95 10 + ln 20 / 0.15 = 30.0 s, V-bar 1.95 NPCs, exit-cancel capacity 2.25 kills/min. All inside the stated limits (d_max 60 s); only the stated range is stale. Change: Crowd (range 12-16).

##### 2C-W10. Carried over from 2026-10-24 C-W22: formally closed, but the S4 margin at default jitter is 0.5 m and the registry has no `StartToFinalMinPathCm`
- Closed items verified: TDC I-PCG-5 (line 273), Generator safe 6-8 (line 354), registry note (line 1764), TDC I-PCG-6 (16 courts).
- Arithmetic re-derived: `x + y <= 110 - 1.08 * F`; F = 44 gives 62.48 and (31, 31) has x + y = 62 (margin 0.48 m); the bound is F <= (110 - 62) / 1.08 = 44.4 m; {(9,50), (31,31), (50,9)} are 29.1 / 29.1 / 58 m apart (>= 24 m). 
- Residual: the margin is smaller than one jitter step. `CityGenSkeletonJitterCount` defaults to 1 cell (2 m); S4 says every 1 m of outward junction movement needs F 0.93 m lower, so a 2 m outward jitter of the (31, 31) junction leaves 5 final-eligible courts (G-PRE fails) unless a replacement court exists. The GDD marks the 200-skeleton script as "still to run" (OQ 9); until it runs, p_g (F6 design >= 0.30) is unverified and could push `E_attempts` and `P_fallback` up. Whether F7 jitter moves the axis junction is not stated.
- Registry: no entry for `StartToFinalMinPathCm` (grep finds none; decision D4 says the registry was updated). LSC line 585 holds the value (4,400, hard 4,000-10,000, safe 4,000-4,400 on the generator). Change: Generator (state whether jitter applies to S4 anchors), registry (add the key).

##### 2C-W11. Camera mask-off threshold (free boom < 20 cm) is below the geometric minimum free length at the default capsule radius
- CS-R4: eye pivot on the capsule axis; CS-R6: sweep radius `CameraProbeRadiusCm` 14; ASN/CON/SCL: `r_c` 35 (provisional, range 30-45). Backed flat against a wall the lens centre cannot be closer than `r_c - 14 = 21 cm` to the pivot (assuming the sweep result is the sphere-centre distance). The mask turns off only below 20 cm (D6, CS-R5/R6, `CameraMaskOffHysteresisCm` 10, back on above 30), so at r_c 35 the mask-off state is not reached by wall contact; it needs r_c <= 33. F2's example "Backed against a wall (free length 18 cm): ... and the mask off" is not reachable at the default radius (18 would need the wall 32 cm from the axis).
- Effect: the 20/30 band is effectively dead at defaults, which is harmless for hysteresis but means near-eye 3P keeps the mask on (cost and false-hide budget, AC-CS-05). Change: Camera (retune the threshold to about 25 cm or re-derive from `r_c - CameraProbeRadiusCm`; fix the F2 example).

##### 2C-W12. D14 idle gaze adds head holds the T13 rate budget does not count
- T13 (Crowd line 130) = "head hold > 2.5 s on a passer-by", NPC rate 0.10/min = only the G6 clock (line 78); AC-CNB-16 requires every G2 hold <= 2.5 s unless G6. D14 (SCL R8.9, Crowd G1) arms a head scan after `IdleGazeDelaySec` 3 s without `Look`, with sweeps held U(3, 7) s: every scan hold exceeds the 2.5 s T13 threshold, and any sweep that lands on a passer-by is a T13-equivalent event for NPCs, idle players and menu-open players alike, not accepted-rate counted.
- G2 glances come every 4-10 s with 0.4-2.5 s holds, so the no-`Look` gap is about 5.5 s and the layer is armed for roughly (5.5 - 3) / 7 = 35 % of an NPC's time (rough, ignoring the 6 m target condition), i.e. well beyond the standing 26 % pause share. A rough coincidence rate (sweep every 5 s, about 0.2 chance a pawn is within the sweep sector) gives an ambient NPC T13 rate of the order of 0.1-0.4 per minute, comparable to or above the 0.10 accepted rate, which would drop the T13 posterior below the 0.50 target (weaker P2 tell) and break the AC-CNB-16 count unless G1 sweeps are excluded or subtracted like R8's share (F2 `r_amb`).
- Also `IdleGazeDelaySec` safe 2-5 s: at 2 s the layer is armed in every G2 gap. Change: Crowd (state whether G1 holds count as T13; subtract an ambient term from G6 or exclude sweeps from the T13 definition and AC-CNB-16), SCL (note the dependency). Estimate only; a playtest log decides.

##### 2C-W13. ADR / architecture pawn-bandwidth line is still 30 KB/s
- SCL F9 and R12: 31.1 KB/s worst case against a 36 KB/s line (raised from 30 for NpcCount 50, 2026-10-25); registry `pawn_network_budget` and `architecture.md` line 875 (TR-scl-008, 36 KB/s) agree. Still 30: ADR-0003 lines 76 and 345 ("pawns <= 30 KB/s worst case (SCL F9)"), ADR-0010 line 429, ADR-0018 line 81, `architecture.md` line 725, `architecture-review-2026-10-25.md` line 174. At default values the GDD's own output (31.1) exceeds the ADR's stated line (30).
- Sum check (all inside 50 KB/s): 30.1 pawns + 1.0 forced (already inside 31.1) + IIF owner status <= 0.5 + IVP feed 0.104 + Pings 0.054 + Roster/C-TeamView about 0.1 gives about 31.9 KB/s, 64 %. Change: ADR owners (a sync pass; not edited here).

##### 2C-W14. SCL F9 `B_forced` citation and the three definitions of the ActionPose delay U
- SCL F9 table: `B_forced` "IIF AC 49"; IIF AC 49 is `StrikeHintParity`; the ActionPose bandwidth test is IIF AC 52 (<= 1.0 KB/s) and AC 53 is owner status (<= 0.5 KB/s). Stale citation, same 1,000 B/s value.
- U is stated three ways: SCL F9 "U_eff ... the replication delay the IIF requires (<= 1/12 s)" (IIF contains no 1/12 s or 0.083 requirement); IIF F2 "U at most one server frame because every write forces a net update" (<= 16.7-33 ms); IVP F7 example "U is uniform over 0-66.7 ms (sigma 19.3 ms)" (the 15 Hz movement period). They are different channels (movement vs forced `FActionState`), each identical for all pawns, so parity holds; with U <= dt_server the F7 example sigma would be 5-10 ms and the 1.9 ms shift limit about 0.5-1 ms (n = 1,236 unchanged). Change: SCL (citation and wording), IVP (example names its channel).

##### 2C-W15. Round/Session timing keys without a guarding invariant, and minor stale bounds
- `EndedMinDwellSec` (3, safe 2-5) vs HUD `HudBannerMaxDelaySec` (1.6, safe 1.4-1.8, hard 2.5) and the fall + fade (1.2 + 0.5 = 1.7 s): TDC I-RMF-2 relates only `EndedAutoResetSec` to the dwell. At the safe low of 2 s a reset can execute 0.2 s after a banner shown at 1.8 s; no cross-key check. At defaults 3 - 1.6 = 1.4 s of banner is guaranteed. Change: TDC (warn `EndedMinDwellSec >= HudBannerMaxDelaySec + 1.0`).
- `StartLeadSec` ("must exceed the worst one-way delay plus one send interval"): IVP L99 150 ms + 1/15 s = 217 ms <= 0.25 (margin 33 ms); the safe low 0.2 fails, and at the CON network ceiling L99 250 ms (317 ms) the default fails. Change: Round (raise default/safe floor to 0.3 or tie it to `LeakNetProfile`).
- IIF F6 "15 cm of commanded displacement is about 0.1 s of walking at about 150 cm/s": SCL walk is 140 cm/s and starts at a_walk 350 cm/s², so from rest 15 cm takes sqrt(30/350) = 0.29 s (the grace is 0.25 s). The Moved-cancel is about 3 x slower than stated; harmless to the hold, fix the sentence (IIF).
- Round F4 `L_soft` 2.75-3.3 s and `L_hard` 11.75-12.3 s vs Session F2 3.0-3.3 and 12.0-12.3 (Session: `L_min = T + d_min`): lower bounds 0.25 s too wide in Round; upper bounds and T_informant 30-42.3 s are right.
- LSC line 350 "valid only with about 63 or more pawns" for 13,000 m² (3,250 cells): 3,250 / 45 = 72.2 pawns total, i.e. NpcCount >= 63 at TeamSize 5; the word should be NPCs.
- No listen-host egress figure exists in any GDD or ADR checked (Session F8 gives only ingress, 0.43 KB/s): 9 clients x 31.1 KB/s worst case is about 280 KB/s (2.2 Mbit/s) upload, typical 9 x 16 = 145 KB/s. Not a GDD error; worth one line in SCL F9 / ADR-0003.

#### Checked and consistent (arithmetic verified)

- SCL F2: d_walk = 112/0.6 = 186.7, a_walk 350, a_run 750, d_run 600; D2 floor d_eff = min(d, v0/0.5): 280 at 140, 224 at 112 (> 187), 900 at 450 (> 600), so crowd-pace and run stops are unchanged (0.60 s / 33.6 cm at 112, 0.675 / 42 at 126, 0.75 / 52.5 at 140: Crowd's "0.35-0.55 m" holds); s_rw = (202,500 - 19,600)/1,200 = 152.4 cm. F3: r = 140/(60 pi/180) = 134 cm, 112/1.047 = 107 cm; I-SCL-6 0.573 * 112 = 64.2. F9: 59 * 15 * 34 = 30.1 + 1.0 = 31.1 KB/s; U_eff 66.7 ms (<= 83.3); f = 17, b = 34 gives 35.1 KB/s; I-SCL-8 3*450/15 = 90 cm.
- ASN F1: run flee D = 112.5 + 60.75 = 173.25, X = 240 at d0 = 151.75; walk flee D = 53.9, X = 203.9; 112 cm/s flee X = 192.7; D2 floor changes B(v) by about 2 cm at v < 94 cm/s only. I-ASN-1 200 - 35 - 15 = 150 <= 240. F3 Duty 4.033/60 = 6.7 %. F5 at NpcCount 50: p 0.0909, q 0.0848, rho 0.00732, m 0.9078, pi 0.992, E[N] 2.976, E[RivalKills] 0.2757, P(strike-out) 0.976, P(>=1 kill) 0.232, NPC kills per rival kill 10.7.
- CON F3: (a) 0.9 >= 0.75; (b) 0.45 >= 0.25; (c) 0.35 >= 0.25; (d) 95/0.5 = 190 <= 210 (Swap 0.45: 211); (f) 0.75 <= 1.2; T_uncull = 150 + 66.7 + 33 = 250 ms; IVP F3 D_cull = 50 * ceil(216.7/50) = 250 (350 at L99 250, 400 at 300). F4 boundary 2.35 m. F2 r_N,req 0.6333 * r_P (0.19 at 0.3; shipped 0.20 tolerates r_P 0.316).
- Penalty: floor 2.1 s; hard-max 4.3 / 4.7 s < D >= 5; F4 P_res 0.475 / 0.659 / 0.996; F5 R = 29.18; T_walk 32.1 / 64.3 s; F3 budget D <= 19.5 (starved) and <= 29.5 (nominal).
- Crowd F1: shares 0.25/0.25/0.20/0.30 give sum w*s 1.09, NPCs 16.1/18.3/7.3/8.3, densities 0.0073/0.0083/0.0042/0.0031, Market 1.47 x, residential 0.55 x. F2: run 0.148, brace 0.020, reversal 0.15, head hold 0.10, decoy 0.0046 (LR 23.3 on full-length tends), Lambda 0.683, chain-gap a(bush) 0.238, a(run) 0.180; at NpcCount 35 Lambda 0.889, (Lambda - r_j) * T_gap <= 0.23. F3: lambda 0.1625, D mean 16.2 s, p95 28.4 s, V-bar 2.11, exit-cancel 2.44/min. F4: f = 2.7 / 21.6 = 12.5 %, P_th = 3 + 1 = 4. F5: 44.2 ms/s, 0.74 ms/frame. F6: SE 0.0114.
- OC F1: T_chain 306 / 402 / 535 s (L 120 / 170 / 240), T_round 7.1-10.9 min; F2 E_court 9 s, E_tell 4 s; F3 P_full 0.699 (0.768 at 35 NPCs same interval; 0.749 / 0.655 at T_round 7 / 11); P_short 0.87; F4 LR_sig 2.38, W_eff 165, bound 163 s, mean delay 112.5 s, latest 195 s.
- HUD: F1 6.9 % / 7.6 % (16:10); F2 HoldBack 1.25 >= 1.2; F3 plate 3.85 s, card 5.4 s; F4 OwnRemovalHoldCap 1.25 >= 1.2, I-HUD-5 0.8 >= 0.75, I-HUD-6 1.4 <= 1.7, example card 21.20, black 22.10; F5 "0:43", sweep 0.287; F7 first value about 8.4-9 (card) and 12.9 (killed informant). W-NKP-5 at defaults 2.0 >= 1.85.
- TMK: F1 sizes (19x24, 37x43, 56x64, 42x48; K, b), F3 examples (q' = (1810, 398), (110, 844), bias ramp 650 / 785 px, 245 px over 30 deg), F4 L = 64, P = 5,280, F6 retire (exit starts +0.25 / +0.60; gone +0.55 / +0.90; T_Rmax 1.5 >= 1.2 - 0.15), F7 S(0.5) = 0.5. `RetireAtServerTime` = `ContactServerTime` (ASN line 332).
- Pings: epsilon 100 ms at default; range 6,150 cm; runner 68 cm in 150 ms; ring thirds 2.67 s; B_conn 50 B/s, 60 s window (10 + 5 * 0.5 * 60) * 20 / 60 = 53.3 <= 54; FVector_NetQuantize10 0.1 cm stated consistently; IVP F6 13 B/entry and 104 B/s match TMK F5 (3/2 = 1.5 <= 2.0).
- Camera: F1 gains 1 / 0.5 / 0.1, soft-edge traverse about 0.22 s (stated 0.2), last degree at g_min 0.05 = 0.11 s; F2 186.1 cm / 258.3 cm; F3 delays 133 / 183 / 317 ms, 295 traces; F4 u(0.151 s) = 0.85, lens 28 cm, 14 m/s; I-CS-1 (<=) consistent with F4 (<); F5 128 deg / 42.3 deg / 61.1 deg / 85.6-107.5 deg; F6 63.6, 45.0, 30.0 deg/s.
- IIF: F1 completion 3.000-3.017 s; F2 W_real [0.10, 4.05); F3 sum B 11, sum R 6, load 300/s, example 6 admits; F4 T_ack 0.483; F5 L_observer 0.30 s, L_confirm 0.20 s; F6 accept 190 / cancel 240 cm.
- LSC: D2 budgets (180, 200, 40); D4 S 0.169; D5 N_E 13, N_B 9, N_T 11, N_C >= 16; D6 at NpcCount 50 (NpcZoneMin 9, Max 13, W 5, 1.47 x rho_p, T_contact 15.1 / 13.3 s with v_run 4.5); D6b E_R 8.73 / 3.41; V-AREA floor `ceil(2000/45) - 10` = 35. Generator F1 (T 2,007-2,304, band [2,046, 2,260] at N 64, A_max 2,700), F5 (5.76 s <= 7; 6 x 7 + 3 = 45 <= 45), F6 (p 0.707 / 0.801 / 0.823; P_fallback 0.063 / 0.006 / 0.003 %), F7 (16 m, 9.4 %, 13 %).
- Session / Round: F2 3.30 / 12.30 s; F4 82.5 <= 120, 132 s from last packet; F7 t_ready 66, Deadline 81, backstop >= 111 (120 configured); Round F4 G_eff 30, T_informant 30-42.3 s; F5 GlobalShare 0.167; F6 T_needed 122.25 <= 150; Roster F7 90 B (22 + 4 x 17) <= 96. IVP F2 SE 0.0175, upper 0.559; F7 n = 1,236; F8 h_fail 16.
- Carried over C-W22: closed (see 2C-W10 for the residual). Carried over C-W1: still open (2C-W4).

#### GDD flags

| GDD | finding IDs | priority |
|---|---|---|
| npc-kill-penalty-informants.md / tuning-data-config.md / registry (ExileBeatSec) | 2C-W1 | Medium |
| objectives-chain.md | 2C-W2, 2C-W8 (OQ cite) | Low |
| concealment-bushes.md | 2C-W3 | Low |
| crowd-npc-behaviour.md | 2C-W4 (carried over from 2026-10-24 C-W1), 2C-W9, 2C-W12 | Medium |
| tuning-data-config.md | 2C-W4, 2C-W15 (EndedMinDwellSec check) | Medium |
| informant-pings-team-comms.md | 2C-W5, 2C-W6 | Low |
| level-socket-contract.md | 2C-W7, 2C-W8, 2C-W15 (wording) | Medium |
| procedural-city-generator.md / registry | 2C-W10 (residual of 2026-10-24 C-W22) | Medium |
| camera-system.md | 2C-W11 | Low |
| shared-character-locomotion.md | 2C-W12 (dependency note), 2C-W14 | Low |
| information-visibility-policy.md | 2C-W14 (F7 example) | Low |
| input-interaction-framework.md | 2C-W15 (F6 sentence) | Low |
| round-match-framework.md | 2C-W15 (StartLeadSec, L_soft bounds) | Low |
| ADR-0003, ADR-0010, ADR-0018, architecture.md (not GDDs) | 2C-W13 | Medium |

## Appendix D — Phase 2D rules and ACs, state/flow cluster (detail, sub-agent output verbatim)

### Phase 2D findings: STATE / FLOW cluster (checklist 2b rule contradictions, 2f acceptance-criteria cross-check)

Status: FINAL for this run. Report only; no GDD, registry, index or ADR was edited. Date 2026-10-25.
Totals: 2 Blocking, 12 Warnings (3 of them carried over from 2026-10-24).

#### Coverage

Read in full (Detailed Design / Core Rules, States and Transitions, Interactions):
- Roster R1-R12, states table, invariants, F6, F7 (participant-state-roster.md lines 33-305, 376-424)
- Session rules 1-25, States and Transitions, Interactions, UI Requirements, kick edge case (session-travel.md 41-227, 425-430, 497-520)
- Round R1-R23, States and Transitions, Interactions, Dependencies (round-match-framework.md 35-260, 483-522)
- Penalty PEN-R1..R17, States, Interactions, Dependencies (npc-kill-penalty-informants.md 41-197, 346-380)
- ASN-R1..R23, States, Interactions, UI Requirements (assassination-defence-reversal.md 33-163, 446-457)
- Pings Core Rules, States, Interactions, F1-F4, Edge Cases, Dependencies (informant-pings-team-comms.md 41-243)
- HUD-R1..R17, States, Interactions, F10 (hud.md 39-159, 327-343)
- TMK TM-R1..R14, States, Interactions (teammate-markers.md 47-145)
- IIF R1-R8 and the edge cases on holds, latch, menu, possession (input-interaction-framework.md 40-129, 420-449, 468-501)
- IVP fact table rows 56-87 and the PostRound lines.
- Camera CS-R8 and CS-R12.1 (camera-system.md 180, 226-237).

Acceptance Criteria read in full: Round (666-769), Penalty (520-573), ASN (543-608), HUD (641-680), TMK (522-568), Session (640-672), Pings (313-342). Sampled only: Roster (AC-PSR-15..21, 53), IVP (AC-IVP-04, 20, 45, 46), IIF (AC 5, 44, 64).

NOT checked (limits of this run): IIF R9-R15 body text beyond the edge cases and the IIF AC section as a whole (only grepped); IVP R2-R15 prose and most IVP ACs; Roster Edge Cases and most of its ACs (AC-PSR-01..14, 22..52, 54+); Session Edge Cases other than the kick case; HUD UI Requirements and Tuning Knobs; TMK F1-F8; Round F4 and the Penalty/ASN formulas; Objectives Chain (read only through grep for menu, latch, `TendCompleted`). No Bash tool was available, so lines over 2000 chars were read through Read and may be truncated.

Consistency checks run per assignment item:
1. Reason values and events (Roster vs callers): checked, 2 findings (2D-W1, 2D-W5).
2. D9 kick path and failed supersede: checked, consistent; 3 wording findings (2D-W2, W3, W4); 1 Blocking on the reconnect indicator for informants (2D-B1).
3. Round-end two-step contract: checked against Roster, Penalty, ASN, IIF, IVP, TMK, HUD, Pings; one AC conflict (2D-B2).
4. D11 wording and own-removal for `InformantKilled`: consistent (details below).
5. Pings vs IVP, Round, IIF, HUD, TMK, Penalty, Camera: one edge-case inconsistency (2D-W6), one stale HUD sentence (2D-W7).
6. TeamSummary, `bReconnecting`, `T_due`, `ContactServerTime`, `RetireAtServerTime`: carrier, audience and timing agree except 2D-B1 and 2D-W5.
7. D14/D15: one gap (2D-W9).
8. AC cross-check: one conflicting pair (2D-B2).

#### Blocking

##### 2D-B1 HUD "back" line fires for a teammate informant who has not come back (HUD-R10a vs Roster R6)
- Roster R6 (participant-state-roster.md line 108): `bReconnecting` is true while the teammate's `Connection` projection is `Disconnected`, phase `Active`, `State` Active or Informant, and server time is before `GraceExpiryMs`, "which for an `Informant` is the same instant even though an informant has no grace of its own"; "it clears at reclaim, at grace expiry, at `EndRoundAll` and at `OnRosterReset`".
- HUD-R10a (hud.md line 86): "when it changes `true -> false` while that ally's `State` is unchanged (the ally reclaimed its slot), one line `{Ally name} - back`. A clear that comes with a `State` change in the same update (grace expired: forfeit) shows no 'back' line".
- Situation: an ally who is already an Informant disconnects (Round R17.5 releases its pawn at once, state stays Informant). At `GraceExpiryMs` the Roster clears the flag with no `State` change, because an informant has no conversion. HUD-R10a reads that as a reclaim and prints "{Ally} - back" while the ally is still gone (and may never return). For an Active ally the same clear comes with the `State` change, so only the informant case is wrong. AC-HUD-28 and AC-PSR-53 test only an Active ally, so no AC catches it.
- Fix is a design call, one of: Roster R6 (hold the flag true for an Informant until reclaim, or drop the grace clear for informants), or HUD-R10a (suppress "back" unless the clear is a reclaim, which the HUD cannot tell from the one bit alone, so the Roster change is cleaner). Add an informant case to AC-HUD-28 and AC-PSR-53 whichever way it goes.

##### 2D-B2 AC-ASN-35 final clause cannot pass together with HUD F10 / ASN-R23 (round-end presentation timing for bystanders)
- AC-ASN-35 (assassination-defence-reversal.md line 594): "when the kill ends the round, no client (host included) shows the round-end presentation before the deciding strike's `DeathServerTime`".
- ASN-R23 (line 132) and Round R9 stage the replicated `Ended` to the deciding strike's death write, which is `ContactServerTime - StrikeNetLeadSec`, i.e. 0.15 s before `DeathServerTime` at defaults (Round R9: "0.25 s after the slot at defaults").
- HUD F10 (hud.md lines 342-343): "Bystanders (no own plate or fall) show the banner at `t_phase`, up to `StrikeNetLeadSec` before a fall they can see: the accepted I-ASN-4 residual", and "Every other viewer shows the banner at its own `t_phase`". The listen host receives no RepNotify delay, so it shows the banner at the death write, 0.15 s before `DeathServerTime`.
- A bystander or the victim's ally with one-way delay under 150 ms shows the banner before `DeathServerTime`, so AC-ASN-35 fails while HUD F10 is implemented as written. IVP line 41 ("so nothing about the end is released before the blow") has the same ambiguity.
- Fix: AC-ASN-35 (relax to "not before the death write, up to `StrikeNetLeadSec` before `DeathServerTime` as the accepted I-ASN-4 residual"), or HUD F10 (gate bystanders to `DeathServerTime`, which means the HUD needs the contact time of a strike it was not part of: not available to it). Design call; the AC edit matches ASN-R23's own wording.

#### Warnings

##### 2D-W1 Penalty PEN-R8 says Round triggers respawn scheduling for "every cause"; the informant-kill cause has no Round trigger
- PEN-R8 (npc-kill-penalty-informants.md line 92): "For every cause Round triggers the scheduling by calling `IInformantBodyProvider::OnInformantAwaitingBody(P, Cause, t_lost)` after its elimination check".
- Round R18 note (round-match-framework.md line 164): Round calls it only for an applied `Active -> Informant` transition with Reason Assassinated, ReversedAgainst or StrikeLimit. An informant kill is "Not a transition" (Roster line 269), so Round has no event.
- Penalty's own States table (line 172: "`RetireBinding(..., Reason = InformantKilled)` inline (D10); schedule") and ADR-0019 line 129 have Penalty schedule it inline. Compatible in practice; PEN-R8's sentence is wrong for one of its five causes. Fix: Penalty PEN-R8 ("every `Active -> Informant` cause"; the informant-kill cause is scheduled by Penalty in the kill slot).

##### 2D-W2 Session's transition table has no kick row; Round cites the wrong rule for it
- Session table (session-travel.md line 195) sends a kick `Connected/Unresponsive -> Disconnected`; the only `Disconnected -> Expired` row (line 203) is "window lapses". The same-tick straight-to-`Expired` rule is only in the kick edge case (line 429) and AC 59.
- Round R17.2 (line 148) cites "Session rule 13 and its kick edge case"; rule 13 is "Lobby has no rejoin windows".
- Fix: Session States and Transitions (add the kick row and event order); Round R17.2 (cite the edge case, not rule 13).

##### 2D-W3 Round R17.2 says a kicked participant's return takes R18's informant path; Session refuses the key
- Round R17.2: "a return, inside or after the grace, takes R18's informant path". Session rule 7 gate 3 and AC 9 / AC 59: a kicked key is `Refused` in every mode, so it cannot return.
- Fix: Round R17.2 wording (return applies to a dropped player, not a kicked one).

##### 2D-W4 (design call) A kicked teammate shows "reconnecting" to its team for `G_eff`
- A kicked participant is `Expired`, which projects `Disconnected` (Roster R11), so `bReconnecting` is true until `GraceExpiryMs` (Roster R6) and HUD-R10a / TM-R11 print "reconnecting" for a player who cannot return (Session gate 3). This follows from decision D9 (no instant conversion, no timing tell) but tells the team something untrue. Kick is a host debug command in MVP. Accept as is, or clear the flag for a kicked key. No document is wrong today.

##### 2D-W5 Roster: the retire inside `RequestTransition` has no source for `RetireAtServerTime`
- Roster R8 (line 147): an `Active -> Informant` change "retires the participant's live binding (R9)" in the same call. R9 (line 181): `RetireBinding(Pawn, RetireAtServerTime [, Reason])`, "the caller passes the time". R8's signature and ASN-R10's call `RequestTransition(V, Informant, Assassinated)` carry no time, yet R6 (line 108) needs the kill's `ContactServerTime` in the teammate entry and TM-R6 step 2 depends on it (AC-TMK-12 f, AC-TMK-13).
- Only the informant-kill path (`RetireBinding` called by Penalty) passes the time explicitly. Fix: Roster R8 (an optional `RetireAtServerTime` argument on `RequestTransition`, or state that Assassination calls `RetireBinding` itself) and ASN-R10 (name the call that carries `ContactServerTime`).

##### 2D-W6 Pings edge case claims a ping pressed in the strike window is accepted; the binding is already retired
- Pings Edge Cases (informant-pings-team-comms.md line 196): between a strike's slot and its contact "if the state is already `Informant`, an informant may ping, so the ping is accepted either way and is identical".
- PNG-R5 step 2 requires a bound pawn; Roster R8 retires the live binding of an Assassinated or Reversed participant in the slot, and an exiled participant's pawn is released to the crowd, so from the slot until the takeover the sender is `Informant` with no bound pawn and the ping is dropped at step 2 (PNG-R2, line 197). The client also gates on raw own `State` and bound pawn (PNG-R3).
- Result: a victim who pings in the 0.25-0.6 s window gets no echo (PNG-R7: no local prediction), which is an indirect pre-contact cue against ASN-R6 ("no cue before contact"); the sentence in the edge case is wrong either way. Fix: Pings Edge Cases (state the real behaviour) and decide whether the client gate should use `PresentedOwnState` (HUD-R6a) so the press is still accepted until the fall. Design call.

##### 2D-W7 HUD-R2 still lists the only world-anchored HUD elements without the ping rings
- HUD-R2 (hud.md line 49): "The only HUD elements anchored in the world are the teammate markers, the interact ring and the context prompt". HUD-R4 L0 and HUD-R11 (lines 54, 94) add team ping rings (HUD-E22), anchored to a world point. HUD-R2's own rule (nothing anchored to a person) still holds, but the list is stale. Fix: HUD-R2 (add the ping ring, anchored to a point, never a pawn).

##### 2D-W8 Stale field and element lists in Teammate Markers and HUD
- TM-R1 (teammate-markers.md line 54) and its Roster interaction row (line 131): teammate entry `{PublicHandle, State, PawnRef, IsConcealed}` / `{PublicHandle, State, PawnRef}`; Roster R6 (line 108) now also carries `RetireAtServerTime` and `bReconnecting`, which TM-R6 step 2 and TM-R11 read. TM-R1's "exactly two inputs" sentence is incomplete. Fix: Teammate Markers TM-R1 and Interactions row.
- HUD-R1 intro (hud.md line 43): "Element ids (HUD-E01 to HUD-E20)"; E21 and E22 exist. Fix: HUD.

##### 2D-W9 Session UI Requirements still show buttons on status overlays; D15 / ADR-0021 allow none below ZOrder 100
- Session UI Requirements (session-travel.md line 515): status overlays "Connection lost. Reconnecting..." "with a Cancel option" and "The session has ended." "with an OK button". HUD-R4 (hud.md lines 59-61) puts Session's overlays in L4 (ZOrder 60), a plain `HitTestInvisible` overlay, menus at 100. ADR-0021 line 373 (D15) splits them: display-only L4 plus a Session-owned Menu-root modal pushed through `PushMenu`.
- Neither GDD states the split. Fix: Session UI Requirements (name the modal part for Cancel and OK, owned by Session, pushed at ZOrder 100) and HUD-R4 (name it as the one Session element above L4).

##### 2D-W10 Carried over from 2026-10-24 (C-W7): Penalty does not list its dependency on Concealment
- Concealment line 276 still lists "NPC-Kill Penalty & Informants (#14) | `IsInBushOrTransition` for takeover eligibility | Hard". Penalty's Upstream table (lines 350-362) and its index note (line 364) still have no Concealment row. Fix: Penalty Dependencies (add the row, or via Crowd), or Concealment line 276 (Soft / "via Crowd"). Design call.

##### 2D-W11 Carried over from 2026-10-24 (C-W12): IIF-Round and Round-Crowd cycles not labelled; Round contradicts itself on Crowd and SCL
- Round line 519 still lists "Crowd & NPC Behaviour, Concealment, Shared Character" as downstream with "whether Crowd and SCL belong here or only upstream is C-W12, open", while lines 497-498 list Crowd and SCL as upstream. Neither IIF (lines 478-479) nor Round (line 496) nor Crowd uses the words "peer contract" or "contained cycle" for the IIF-Round and Round-Crowd pairs (grep found none). Fix: Round Dependencies, IIF Dependencies, Crowd Dependencies.

##### 2D-W12 Carried over from 2026-10-24 (S-I1): a final Complete rejected by the latch is not logged as a tend
- Objectives edge cases (objectives-chain.md lines 257-260) say the second final "returns `Rejected_Latched` and its hold ends by the normal blend"; the log row is `TendCompleted(ParticipantId, court, Counted | NoEffect)` (line 395, Round R19) with no entry for a latched Complete, so `ObjectiveEndShare` (F5/F7) is slightly under-counted. Round logs only a generic `Rejected_Latched` row. Fix: Objectives F5 (log `TendCompleted(NoEffect)` for it).

#### Checked and consistent

- D10 `InformantKilled`: carried only by `RetireBinding` (Roster R9 line 181), `Rejected_IllegalReason` as a transition reason (R8 check 6, AC-PSR-20 reason axis), "Not a transition" (line 269, AC-PSR-21). Penalty PEN-R7 table, PEN-R16, States row (line 172) and AC-NKP-15/44, Camera CS-R12.1 (line 234) and HUD-R9 / `InformantBodied` row / AC-HUD-29 all agree: view fades from `DeathServerTime + DeathFallSec` over `NoPawnFadeSec`, no removal card, no exile cue, countdown after the fade. ADR-0015/0018/0019 carry the same value.
- D11 wording: HUD-R9 title "Informant", subtitles "Too many wrong targets" (exile) / "Eliminated for the round" (assassinated, reversed), second line "You watch for your team now."; Penalty PEN-R16 and AC-NKP-20, concept line 308 and ASN UI-ASN-1/5 agree; "Target exiled" is only the `RivalEliminated` plate (ASN UI-ASN-1, AC-ASN-16, HUD-R6). Plate strings "Wrong target", "No contact", "Reversed", "Strike reversed" match between ASN and HUD.
- D9 kick path: Roster R11 (line 224), Round R15 (c), R17.2, AC 26c and Session line 429 / AC 59 agree (same-tick `Expired`, no generation bump, `Disconnect` request `Applied`, `OnRejoinWindowExpired` ignored, `Void(KickedLastActive)`). Failed supersede-rejoin: Session lines 131 and 168, Roster line 220, Round R17.3 agree (fresh `DisconnectedAt`, generation +1, old grace timer `Rejected_StaleConnection`, new timer). Lobby `Rejoining` projects `Disconnected`, `bReconnecting` false outside `Active` (Roster R6, Session rule 15, HUD-R10a).
- Round-end two-step contract: Round R9, Roster R7 `EndRoundAll`, Penalty States row (line 176), ASN-R21/R23 and AC-ASN-43 (earlier strike's death write after the round-end step still plays), IIF edge cases (holds frozen at the latch, cancelled with `Illegal` at the round-end step, `NotNow` replies held), IVP R2 rule 1 / AC-IVP-20, TM-R5, HUD `Ended` row and AC 12a-12e all tell the same story. Pings: PNG-R2 / R5 step 1 drop pings from the latch, PNG-R10 clears every slot in the round-end step (Round R9 (a) says the same), clients hide rings on `Ended` (HUD `Ended` row); live rings keep showing between latch and round-end step, which is consistent with "locomotion untouched".
- `TeamSummary`: Round R9 (a) (zero before `Ended`, computed at the latch, published at the round-end step), IVP `RoundTeamSummary` row, HUD-R14 / AC-HUD-27 and Round AC 12e agree on content, audience and timing.
- `bReconnecting` carrier and audience: Roster R6, IVP `TeammateReconnecting` row, TM-R11 / AC-TMK-26, HUD-R10a, Session rule 24 and AC-PSR-53 agree (Team audience, one bit, false outside `Active`), except the informant clear (2D-B1).
- `T_due`, `WrongTargetTally`, `LastResultSeq`, `ContactServerTime`, `RetireAtServerTime`: Roster R6, Penalty PEN-R3/R15, IVP rows 66-71, HUD-R7 / F7 and ASN-R19 agree on field names, audience and the "never per strike" discipline.
- Pings vs IVP (`TeamPing` Team audience, `FVector_NetQuantize10` 0.1 cm, one slot per team member), Camera (CS-R8: Pings read `Origin`/`Direction` only), IIF (`IA_Ping` in `IMC_Common`, controller RPC not a pawn command, so brace/hide/hold states do not block it and a menu-open `End` flush does not touch it), HUD (ring drawn in the HUD L0 ping slot; Pings owns content, HUD owns layer, rebuild rule HUD-R17), TMK (edge clamp F3 reused, rings beneath markers) and Penalty (PEN-R12 last row, awaiting informant cannot ping): consistent apart from 2D-W6 and 2D-W7.
- D14: the IIF owns "held actions end on menu open" (IIF R4 and the edge case at line 445, AC 5 R4 `FlushSendsEnd`); ASN-R3 (an early-ended brace burns the full cooldown) and the hold rules of Objectives are compatible with it. Neither ASN nor Objectives mentions the menu case; a brace ends after the 0.10 s debounce and the cooldown is spent. No contradiction.
- AC numeric agreement checked: Penalty AC-NKP-10/25/48 `T_due` values vs PEN-R8/R11 (115.0, 120.0, 118.0); AC-NKP-07 grace 2.0 s inclusive vs PEN-R5; AC-NKP-18 floor 4.7 s vs F2; HUD AC-HUD-14 (60 s cooldown, "Brace in 0:43", "Returning in {s}") vs HUD-R12/F7; HUD latency 0.20 / 0.16 s; HUD AC-HUD-17 banner cap 1.6 s vs F10; TMK AC-TMK-12/13 (0.25 s hold, 0.60 s bush, 1.5 s cap) vs F6; ASN AC-ASN-20 (death write 0.15 s before contact) vs Round R9 (0.25 s / 0.60 s after the slot); Session AC 59 and AC 66 vs Roster R11 and Round R17; Roster AC-PSR-53 vs HUD AC-HUD-28 for an Active ally.

#### GDD flags

| GDD | finding IDs | priority |
|---|---|---|
| hud | 2D-B1 (or Roster), 2D-B2 (design call), 2D-W7, 2D-W8, 2D-W9 | high |
| participant-state-roster | 2D-B1 (design call), 2D-W5, 2D-W4 (design call) | high |
| assassination-defence-reversal | 2D-B2, 2D-W5 | high |
| informant-pings-team-comms | 2D-W6 (design call) | medium |
| round-match-framework | 2D-W2, 2D-W3, 2D-W11 | low |
| session-travel | 2D-W2, 2D-W9 | low |
| npc-kill-penalty-informants | 2D-W1, 2D-W10 | low |
| teammate-markers | 2D-W8 | low |
| input-interaction-framework | 2D-W11 | low |
| objectives-chain (outside this cluster) | 2D-W12 | low |
| concealment-bushes (outside this cluster) | 2D-W10 | low |
| information-visibility-policy | 2D-B2 (wording of R2 rule 1 "before the blow", optional) | low |

## Appendix E — Phase 2E rules and ACs, world/body cluster (detail, sub-agent output verbatim)

### Phase 2E: World / Body cluster, rule contradictions (2b) and AC cross-check (2f)

Date: 2026-10-25. Report only (no GDD, registry, index or ADR edited). STATUS: COMPLETE (all seven checklist items covered; see Coverage for limits).

Counts: 4 Blocking, 13 Warnings.

#### Coverage

Checked (read in full or by range, line numbers from the files as of this run):
- SCL: Core Rules R1-R19, States and Transitions, F1-F2, Dependencies, Tuning Knobs (all rows, invariants), Acceptance Criteria (all, AC-SCL-01..50), Open Questions OQ13-OQ16.
- Camera: Core Rules CS-R1-R15, Dependencies, Tuning Knobs and invariants, Acceptance Criteria (all).
- Crowd: Core Rules CNB-R1-R16 (incl. Tell Matrix), States, Interactions, Dependencies, Tuning Knobs, Acceptance Criteria (all).
- Concealment: CON-R1-R16, F1-F4, Acceptance Criteria (all).
- ASN: ASN-R8-R19 (victim resolution, strike, bodies), F1-F4, Acceptance Criteria (all).
- LSC: Core Rules R1-R11, D5, D6, Acceptance Criteria (all), knob rows for StartToFinalMinPathCm / FinalCandidatesMinCount / InformantRespawnPathCm.
- PCG: R-G1-R-G7, S4/S5/S7 rows, knob rows, Acceptance Criteria (all).
- OC: OC-1-OC-17, Stall Pressure SP-1-SP-8, F1-F5, Edge Cases, Acceptance Criteria (all).
- CPA: CP-1-CP-19, Acceptance Criteria (all). Penalty: PEN-R7, PEN-R12/R13. IIF: R7 table, R15a, menu-open edge case. Round R9 (round-end ordering). TDC: SCL row, invariant table lines 237-275, registry-mirror paragraph. Registry: StopMinSec, IdleGazeDelaySec, CameraMaskOffHysteresisCm, CameraToggleLockoutSec, InformantRespawnPathCm, location respawn_region. systems-index rows 10-11. Previous cross-review items C-W1, C-W17, C-W19, C-W22 and Crowd-row-lacks-Camera.

Not checked or only partly:
- TDC core rules beyond the rows listed above (other reviewers' scope); IVP and HUD were only grepped (no D3/D14 text found in IVP; HUD not read).
- `design/ux/pause-menu.md` (registry says it consumes `IdleGazeDelaySec`) was not read; it is outside the 19 MVP GDDs.
- No Bash tool in this run, so the 27 over-long lines were read with Read offset/limit; none was truncated in the ranges read.
- Formula arithmetic was spot-checked (SCL F2/AC-SCL-50 vectors, ASN F1 / AC-ASN-02 / AC-ASN-38, CPA AC-CP-01 / AC-CP-17, PCG S4 x+y <= 110 - 1.08F, LSC D5 N_C = 16) and all agreed.

#### Blocking

##### 2E-B1. SCL R5.3 still says sub-pace stops can be shorter than 0.5 s and that the braking floor is "open, OQ15"; F2 / AC-SCL-50 (D2) say every stop lasts at least StopMinSec
- SCL Core Rules R5.3 (line 68): "(A stop begun below a pace, during the first 0.27 s of a start, takes `v / d` and can be shorter than 0.5 s, down to a few centimetres at a key tap; whether that start-abort needs a braking-time floor is open, OQ15.)"
- SCL F2 (lines 212, 226): "`t_stop = v0 / d_eff = max(v0 / d_g, StopMinSec)`" and "Every stop to rest from any speed v0 > 0 lasts at least `StopMinSec`"; AC-SCL-50 requires 0.5 s for v0 20-93 cm/s; OQ15 (line 771) is "CLOSED 2026-10-25".
- The two cannot both be true for the same stop, and R5.3 is the implementation source. Crowd (stop paragraph, line 65; AC-CNB-04 (c)) already treats OQ15 as closed.
- Change: SCL R5.3 (replace the parenthetical with "see F2 stop floor, D2"). Mechanical fix, no design call.

##### 2E-B2. LSC R11.5 graybox authoring checklist still says final-eligible courts are "at least 60 m path from every start cell"; V-SEP and the knob say 44 m (D4)
- LSC Core Rules R11.5 (line 219): "Final-eligible courts are those at least 60 m path from every start cell with path parity between zone pairs; there is no 'centre band' rule."
- LSC V-SEP (line 145): "Every start cell >= `StartToFinalMinPathCm` (44 m; lowered from 60 m by decision D4 2026-10-25 ...)"; knob row (line 585) default 4,400; D5 feasibility note (line 336) and PCG S4/S5/AC-G39 all use 44 m.
- The checklist is what a level designer follows when authoring the graybox that must still reach `FinalCandidatesMinCount` 6 final-eligible courts; a 60 m authoring rule is the exact condition D4 found infeasible for 6 courts. The D4 closeout updated V-SEP and D5 but missed R11.5.
- Change: LSC R11.5 (60 m -> `StartToFinalMinPathCm` 44 m; better: cite the key, not a number). Mechanical fix.

##### 2E-B3. PCG R-G5.3 cites a key `ClueTagCourtsMaxCount` that does not exist; the owner key is `ClueMatchMaxCount`
- PCG Core Rules R-G5.3 (line 80): "V-CLUE (`ClueTag` shared by <= `ClueTagCourtsMaxCount`, now an LSC predicate)".
- LSC V-CLUE (line 158): "shared by at most `ClueMatchMaxCount` (3; Objectives-owned, read only) courts"; OC-15, OC edge case (line 284), AC-OC-28, TDC and the registry all use `ClueMatchMaxCount`. A grep of all design files finds `ClueTagCourtsMaxCount` only in PCG line 80.
- Impact: a programmer implementing G-PRE V-CLUE from the Generator GDD looks for a key that no schema holds. (The OC `ClueTooBroad` edge-case closeout is correct and consistent.)
- Change: PCG R-G5.3 (rename to `ClueMatchMaxCount`). Mechanical fix.

##### 2E-B4. `IdleGazeDelaySec`: owner GDD and registry disagree on audience and latch, and TDC's SCL key row does not include it, so AC-SCL-35 cannot pass
- SCL Tuning Knobs (line 529): audience `ServerOnly`, latch `Use`, range 1-10, safe 2-5, default 3.
- Registry `entities.yaml` (lines 4832-4848): audience `Client`, latch `Round`, referenced_by includes `design/gdd/tuning-data-config.md`.
- TDC SCL row (line 110): "52 (recounted ...; plus `StopMinSec` by decision D2 ...)" with no mention of `IdleGazeDelaySec`; a grep of TDC for `IdleGaze`, `idle gaze` and `D14` returns nothing. The TDC registry-mirror paragraph (line 124) lists the new keys `StopMinSec`, `CameraMaskOffHysteresisCm` and `CityGenCourtTargetCount` but not `IdleGazeDelaySec`.
- AC-SCL-35 (line 738): "GIVEN the TDC schema, WHEN the CR-15 lint runs, THEN every key in Tuning Knobs exists with the stated audience and latch": this fails for `IdleGazeDelaySec` on both counts (missing from TDC, and registry differs from the stated audience/latch). The key is read only by the server-side idle layer (SCL R8.9 "On the server"), so `ServerOnly`/`Use` looks right and the registry row looks copied from `StopMinSec` (`Client`/`Round`).
- Change: design call on audience/latch (recommend SCL's `ServerOnly`, `Use`), then registry row and TDC SCL key count/row follow.

#### Warnings

##### 2E-W1. D14 idle-gaze trigger is defined two ways (SCL: control rotation unchanged; Crowd: no `Look` issued)
- SCL R8.9 (line 107): "any pawn whose control rotation has not changed for more than `IdleGazeDelaySec` (3 s)".
- Crowd CNB-R6 G1 (line 73): "whenever the NPC has issued no `Look` for more than `IdleGazeDelaySec`".
- Compatible only if the crowd virtual-look layer (CNB-R3.4, virtual camera yaw through `EvaluateViewEnvelope`) never re-issues an identical `Look` every tick. If it does, a still NPC "issues Look" continuously and never scans while a still human scans at 3 s (a Pillar 1 tell). AC-SCL-49 (c) only tests "an NPC with no Crowd Look command", so the case is untested.
- Change: Crowd G1 should adopt SCL's wording; AC-SCL-49 should add an NPC case that re-issues an unchanged `Look` each tick. Owners: Crowd (wording), SCL (AC).

##### 2E-W2. D14 idle scan and the head-hold tell T13 are not reconciled
- SCL R8.8 (line 106): "A head held at its clamp for more than 2.5 s is the art bible 'head hold' tell, by design."
- Crowd Tell Matrix T13 (line 130): "Head hold > 2.5 s on a passer-by ... holds on goods or a conversation partner are context, not T13"; the only NPC source is G6 (0.10/min).
- SCL R8.9: every idle pawn holds each scan sweep (30-60° offset, clamped to 70°) for 3-7 s. A sweep that lands at the clamp or on a passer-by is a > 2.5 s hold that neither definition accounts for. Parity holds (same layer for every pawn), but F2's T13 posterior assumes the only NPC holds are G6, and the two GDDs define T13 differently (clamp vs passer-by).
- Change: design call. State that idle-scan holds are excluded from T13 (context holds, like G3/G5) and from the F2 telemetry count, or count them in F2; then SCL R8.8 and Crowd T13 use one definition.

##### 2E-W3. D3 (host significance tiering) not carried into Camera wording, AC-CS-28 or SCL's own R11.5 lead-in
- Camera CS-R1 (line 64): "On the host, every pawn ticks its pose and refreshes its bones whatever its render state, using always-tick"; AC-CS-28 (line 732): "A and B have identical pose-tick counts"; also lines 495, 519, 664.
- SCL R11.5 (line 118): opens with "(always-tick at rate 1 for every pawn; Camera change request 2)", then states D3: "tiered with the same significance bands ... no host-only rate-1 posing; the pose still ticks every frame with accumulated delta time ... only full evaluation is rate-limited".
- A masked dummy has alpha 0, so h = 0 and it is tier D-culled on the host; a correct D3 build evaluates it less often than the in-view dummy B, so "identical pose-tick counts" can fail a compliant build depending on whether a "tick" is the TickPose call or an evaluation. The testable requirement (root motion within 0.1 cm, identical notify sequence) is already in AC-CS-28.
- Change: Camera CS-R1, edge case (line 495), dependency row (line 519) and AC-CS-28: reword to the observable result and drop "refreshes its bones" / "identical pose-tick counts" (or define pose-tick as the TickPose call count); SCL R11.5: drop the stale "at rate 1" parenthetical.

##### 2E-W4. systems-index row 11 still omits Camera from Crowd's dependency list (carried over from 2026-10-24, "Crowd dependency row lacks Camera")
- Crowd Dependencies row (line 368) now lists Camera System (10), but its Bidirectionality note (line 387) says "only Camera is still not listed", and `systems-index.md` line 58 (row 11) still lists no Camera.
- Change: systems-index row 11 (add Camera System, soft); then delete the stale sentence in Crowd line 387.

##### 2E-W5. IIF safe range of `CameraToggleLockoutSec` includes values I-CS-1 rejects (carried over from 2026-10-24 C-W17, mostly resolved)
- Camera now documents the lint plus runtime clamp (CS-R10, I-CS-1, AC-CS-16) and hard range 0-0.29 for the blend. Still present: IIF knob row (line 543) hard 0-1, safe 0.2-0.5, and Camera F4 T_lock range 0.2-0.5 (line 414), while I-CS-1 needs lockout >= 0.217 s at the default 0.20 s blend (TDC line 240: reject).
- Change: IIF knob row and Camera F4 range (safe 0.22-0.5, or cite I-CS-1). Low priority.

##### 2E-W6. Crowd decoy early-release maximum vs `ObjectiveInteractSec` still not a registered invariant (carried over from 2026-10-24 C-W1)
- Crowd knob row (line 415): `CrowdDecoyEarlyReleaseMaxSec` default 2.8, safe 2.2-3.0, "Max < `ObjectiveInteractSec`" in prose only. TDC invariants I-CNB-1..5 (lines 241-245) and I-OC1..3 (266-268) do not include it.
- `ObjectiveInteractSec` hard range 2.0-6.0, safe 2.5-4.5 (OC-14): Max 2.8 with D 2.5 (both inside safe ranges) turns early releases into completions and breaks the F3 75/25 model.
- Change: Crowd (add `CrowdDecoyEarlyReleaseMaxSec < ObjectiveInteractSec` as an I-CNB rule) and TDC owner-invariant table.

##### 2E-W7. `I-SCL-5` still reads a "Crowd pace set" that no key holds (carried over from 2026-10-24 C-W19)
- SCL Tuning Knobs invariants (line 579): "I-SCL-5 min(Crowd pace set) >= `WalkSpeedScaleMinMult`"; the pace set is the constant {0.8, 0.9, 1.0} (F1 line 198); neither SCL nor Crowd holds a pace-set key (Crowd stop paragraph: "owns no pace-set key").
- SCL safe range for `WalkSpeedScaleMinMult` is still 0.75-0.90 (line 522), so values 0.85-0.90, which are inside the safe range, are rejected by I-SCL-5 (TDC line 237: reject). The 2026-10-24 proposed fix (I-SCL-5 = `WalkSpeedScaleMinMult` <= 0.8, safe range 0.75-0.80) is not applied.
- Change: SCL line 522 and 579 (and TDC line 237 wording).

##### 2E-W8. `StopMinSec` has no invariant against `WalkStopSec` / `RunStopSec`, and I-SCL-15 assumes stops last at most max(WalkStopSec, RunStopSec)
- SCL knobs: `StopMinSec` hard 0.2-1.0, safe 0.4-0.7 (line 528); `WalkStopSec` hard 0.5-1.2, safe 0.5-0.8 (line 527). Safe combinations such as StopMinSec 0.7 with the default WalkStopSec 0.60 make every stop from 112 cm/s last 0.7 s (`max(v0/d_g, StopMinSec)`), so AC-SCL-06 (0.60 s), the F2 output table, Crowd arrival-distance maths (53/42/34 cm) and AC-SCL-23 (stopped within 1.0 s) no longer hold.
- I-SCL-15 (TDC line 239): `MaxClientForcedUpdateDuration >= InputStarvationSec + max(WalkStopSec, RunStopSec) + 0.25`; I-SCL-12 uses `RunStopSec` the same way. A stop floor above both makes those bounds optimistic.
- Change: SCL invariants / TDC (add `StopMinSec <= WalkStopSec` and extend the I-SCL-12 / I-SCL-15 terms with `max(..., StopMinSec)`), or narrow StopMinSec's safe range to <= WalkStopSec.

##### 2E-W9. SCL R2a.4 and LSC AC-14 say the informant's replacement carries "the victim's card"; CPA draws uniformly over all vacant cards
- SCL R2a.4 (line 52): "Crowd's replacement for it is created by `SpawnSharedPawn` with the victim's card through an emergence socket"; LSC AC-14 (line 757): "Crowd receives exactly one `Replacement` request carrying the victim's card".
- CPA CP-12(a) (line 59): `DrawReplacement()` returns "a card chosen uniformly at random from the currently vacant NPC cards ... With a single vacancy this returns the victim's own appearance"; LSC R5b.4 (line 116) and Crowd R9.2 agree with CPA.
- With two or more vacancies (a backlog) the SCL text and LSC AC-14 are false; AC-14's single-kill fixture passes only by accident.
- Change: SCL R2a.4 and LSC AC-14 wording (draw from vacant cards; victim's card only when it is the sole vacancy).

##### 2E-W10. AC-ASN-39 / AC-ASN-12 still describe a hash pick although the pick is the `BushSpotPick` stream
- ASN-R8.2 (line 63) and F4 (CON line 194, ASN dependency row line 318) are correct (`BushSpotPick`, `UniformBelow`). AC-ASN-39 (line 598) is titled `BushVictimHash` and reads "1,000 fixed hash inputs"; AC-ASN-12 (line 559) says "(Hash pick: AC-ASN-39". Cross-reference row at line 519 still says "filter-then-pick with `SpotSalt`" (historical record, acceptable). The test name is a stable identifier but the Given clause names inputs that do not exist ("hash inputs" vs injected stream seeds).
- Change: ASN AC-ASN-39 and AC-ASN-12 wording (and optionally the test name). Cosmetic.

##### 2E-W11. Concealment F1/F2 worked examples still use r_N = 0.19; Crowd now ships 0.20 (D7)
- CON F2 (line 168): "Concealment recommends Crowd ship r_N = 0.19/min for margin"; F1 example uses r_N 0.19 (NPC term 0.36, lambda 0.410, m0 0.34). Crowd R7 (line 86), knob row (line 416), AC-CNB-15: 300 s = 0.20/min.
- Not contradictory (0.20 >= r_N,req 0.19), but F1's lambda/m0 worked figures (and the D1 cap example "about 34 % at defaults") are computed at 0.19; at 0.20 lambda is about 0.425 and m0 about 0.35.
- Change: Concealment F1/F2 examples. Cosmetic.

##### 2E-W12. TDC SCL row says invariants "I-SCL-1 to I-SCL-14" while the table and SCL list I-SCL-15
- TDC line 110: "I-SCL-1 to I-SCL-14"; TDC lines 237-239 and SCL line 579 and AC-SCL-35 cover I-SCL-1 to I-SCL-15.
- Change: TDC line 110 (and, with 2E-B4, the key count). Cosmetic.

##### 2E-W13. Registry `respawn_region` wording can be read as "within 45 m of whichever anchor is nearer"
- Registry (line 519): "walkable cells within InformantRespawnPathCm (45 m path) of the NEARER anchor of the zone pair (per team: the anchor of that team's own zone ...)".
- LSC R3 (line 75), R5b.2 (line 114) and PEN-R7 (line 80): within 45 m of the team's own anchor and strictly nearer to it than to the partner zone's anchor. The parenthetical in the registry gets there, but "of the NEARER anchor" reads as a union of both teams' discs.
- Change: registry wording to the LSC/Penalty form. CPA OQ-14 correctly leaves the Pillar-1 review of the restriction to Penalty and LSC.

#### Checked and consistent
- D1: CON-R12 ("max(40 %, m0 + 10 points), one cap"), AC-CON-24 (c) and F1's m0 definition agree; no stale double cap or absolute 40 % found in CON, Crowd, IVP or TDC text read.
- D2 stop floor: SCL F2 vectors and AC-SCL-50 (0.5/0.5/0.5/0.5/0.60/0.75 s walk, 0.5/0.75 s run) match d_walk 187 and d_run 600; Crowd consumes `StopMinSec` (line 65, AC-CNB-04 (c)); AC-SCL-06/23/42, AC-ASN-02/38 (a_b 187/600), ASN F1 braking, OC t_settle (0.5-1.5 s), Camera and CON add no sub-0.5 s stop. No AC still tests the old sub-0.5 s stop.
- D14 parameters: SCL R8.9 / knob / AC-SCL-49 / Crowd G1 / Crowd dependency row use the same numbers (3 s, 30-60° yaw, every 3-7 s); the layer changes only the HeadLook target, so Camera CS-R3 (view relative to body facing, control rotation = head target) and IIF line 445 (held actions end on menu open, no AI hand-over) are unaffected. IIF and Camera carry no D14 text and need none.
- D6: CS-R5/R6, `CameraMaskOffHysteresisCm` (10; off at 20 cm, on at 30 cm), AC-CS-41, AC-CS-05 oracle and mask origin (capsule axis), AC-CS-23 retagged document review; Camera Downstream row and CS-R8 list Informant Pings.
- CON-R9 vs CS-R12.1 vs ASN-R10/R17 vs AC-ASN-44 / AC-CON-26: victim keeps `IsConcealed` until body removal, `bDiedConcealed` written only in the death write, body hidden for every non-owner; consistent in all five. BushSpotPick naming consistent in CON R12/F4, ASN-R8.2, ASN dependency row, AC-CON-14.
- Informants never enter bushes: IIF R7 table (Hide Enter/Exit rejected), PEN-R12 and PEN-R13, SCL R6, CON-R3 (1) and AC-CON-06, CNB-R11 (`Hiding` never eligible), AC-CON-30; no contrary text found.
- Layout: LSC V-SEP 44 m, D5 N_C >= 16 (6 + 12 - 2), `FinalCandidatesMinCount` 6, PCG S4 (3 + 3 final-eligible, x + y <= 110 - 1.08 F, 44.4 m bound, G-PRE guard 47.5 m), S5 anchor wording (review-log form), PCG court target 16-18, TDC I-PCG-5/6, AC-G39 (46 m rejected inside the 8 % guard) all agree; C-W22 (final-eligible target vs LSC 6) is resolved by I-PCG-5 and the PCG 6-8 safe range. OC SP-2/SP-4 (6 candidates, 3 distinct finals, no court > 50 %) match LSC R9.4 / D8. Penalty PEN-R7, LSC R3/R5b.2/AC-18 and CPA CP-12(b) use the same respawn-region rule and widening steps.
- ClueMatchMaxCount: OC-15, OC edge case (line 284), AC-OC-13/28, LSC V-CLUE, TDC, registry agree (only PCG line 80 deviates, 2E-B3).
- Crowd R12 halt vs Round R9 two-step contract (latch step vs round-end step, Halt raised in the round-end step, locomotion untouched in between) and AC-CNB-22/38 agree; SCL R6 RoundEnded row and Camera CS-R12.4 agree. Crowd CNB-R9.2 `DrawReplacement()` in the spawn tick matches CPA CP-12(a)/CP-11 conservation and AC-CP-10/11a.
- Walk-slower: IIF R15a (Left Ctrl 0.8, Left Alt 0.9), SCL R5.1/F1 (pace set {0.8, 0.9, 1.0}), Crowd R3.1 (`CrowdFullPacePct` 55, C4 active, T16 yes), AC-SCL-36 and AC-CNB-03 agree.
- SCL lunge item: SCL States `Disabled` row (line 162) and ASN-R16 agree that a death write never cuts a strike clip's warped root motion; SCL R9 no longer says "user to confirm Camera OQ9"; SCL Dependencies bidirectionality paragraph (line 513) is marked met; SCL R2a.3 / OC tend / decoy early release (U(0.8, 2.8), I-OC3 0.25 + 1.0 >= 0.8) consistent.
- Pitch -60/+40 both modes, view yaw +-70 with 5° elastic push, AC-CS-03 vs AC-SCL-18 tolerances (21.5° vs 20.1°) are compatible. CPA AC-CP-01/17 arithmetic verified.

#### GDD flags

| GDD | Finding IDs | Priority |
|---|---|---|
| shared-character-locomotion | 2E-B1, 2E-B4, 2E-W1 (AC), 2E-W2, 2E-W3, 2E-W7, 2E-W8, 2E-W9 | High (B1, B4), Medium |
| level-socket-contract | 2E-B2, 2E-W9 | High (B2), Low |
| procedural-city-generator | 2E-B3 | High (one-token fix) |
| tuning-data-config | 2E-B4, 2E-W6, 2E-W7, 2E-W8, 2E-W12 | High (B4), Medium |
| crowd-npc-behaviour | 2E-W1, 2E-W2, 2E-W4, 2E-W6 | Medium |
| camera-system | 2E-W3, 2E-W5 | Medium |
| assassination-defence-reversal | 2E-W10 | Low |
| concealment-bushes | 2E-W11 | Low |
| input-interaction-framework | 2E-W5 | Low |
| systems-index | 2E-W4 | Low |
| design/registry/entities.yaml | 2E-B4, 2E-W13 | High (B4), Low |
| objectives-chain, character-pool-appearance, information-visibility-policy | none | n/a |

## Appendix F — Phase 2F closeout landing verification (detail, sub-agent output verbatim)

### Phase 2F: Closeout landing verification (review-all-gdds full, 2026-10-25)

Report only: no GDD, registry, index, ADR or art bible was edited. All evidence is file:line from Grep/Read of the working tree on 2026-10-25.
Inputs: production/design-closeout/2026-10-25_rereview-followups.md and 2026-10-25_gdd-pass-remaining.md (both read whole).

Result counts (items below): LANDED 52, PARTIAL 3, NOT LANDED 2, NOT A GDD ITEM (ADR/art bible, all LANDED) 13, NOT CHECKED 2.

#### 1. Decisions D1-D15

| Item | Target docs | Result | Evidence |
|---|---|---|---|
| D1 CON-R12 / AC-CON-24(c) cap = max(40 %, m0 + 10 pp) | concealment-bushes; ADR-0017; control-manifest | LANDED | concealment-bushes.md:95 (CON-R12, "one cap, decided 2026-10-25, D1"), :460 (AC-CON-24(c) "same single cap"); ADR-0017:416; control-manifest.md:894. No absolute-40 % double cap remains in any GDD. |
| D2 StopMinSec 0.5 s stop floor, Crowd uses same key | SCL, TDC, Crowd, registry, art bible B4, ADR-0010, ADR-0004 | LANDED | SCL:212 (F2), :223 and :528 (knob, hard 0.2-1.0, safe 0.4-0.7), :751 (AC-SCL-50), :771 (OQ15 CLOSED); TDC:110, :124; Crowd:65, :357, :551; registry entities.yaml:4816-4829; art-bible.md:1883 (B4 "Every stop to rest lasts >= 0.5 s", 0.35-0.55 m); ADR-0010:180, :476; ADR-0004:442. |
| D3 SCL OQ16 uniform TickPose escalation on listen host | SCL, ADR-0022, ADR-0013/0010 | LANDED | SCL:118, :121, :344, :728 (AC-SCL-29, spike R3), :772 (OQ16 CLOSED); ADR-0022:135, :365; decision-log.md:4911. |
| D4 StartToFinalMinPathCm lowered to 44 m, FinalCandidatesMinCount stays 6 | PCG, LSC, registry, TDC, OC, ADR-0014 | PARTIAL | LANDED in: procedural-city-generator.md:67 (arithmetic), :354, :518 (AC-G39), :531 (OQ9 CLOSED); level-socket-contract.md:145 (44 m), :336 (feasibility), :585 (4,400; safe 4,000-4,400); objectives-chain.md:485; ADR-0014:502. STALE in level-socket-contract.md:219: "Final-eligible courts are those at least 60 m path from every start cell" (see 2F-B1). Registry has no StartToFinalMinPathCm entry and TDC does not list it (the decision-log text says "registry updated"; sibling LSC keys such as ZoneSeparationPathCm are also unregistered, so this is consistent but the claim is not true; see 2F-W7). |
| D5 PCG S5 anchor wording from review log | PCG, LSC, ADR-0014 | LANDED | procedural-city-generator.md:68 ("plain street cell (never a junction) ... decision D5 2026-10-25"), :518 (AC-G39); ADR-0014:503. |
| D6 Camera: oracle and mask on capsule axis; CameraMaskOffHysteresisCm 10; AC-CS-23 doc review; hide latency > 60 m stated | Camera, TDC, registry, ADR-0015 | LANDED | camera-system.md:130, :148, :170, :569 (key 5-30 / safe 8-15), :765 (oracle origin), :876 (AC-CS-23 "Doc review/ADVISORY"), :614 (stated limit, <= 0.42 s beyond 60 m); TDC:111; registry entities.yaml:4849; ADR-0015:378. |
| D7 Crowd bush default 300 s, registry updated | Crowd, registry, TDC | LANDED | crowd-npc-behaviour.md:86, :122, :224, :416 (default 300, safe 240-315), :567; registry entities.yaml:1954-1967 and :678; TDC:124. No stale 315 default in CON/IVP/OC/TM/TDC/index. |
| D8 HudResultPlateMaxHoldBackSec / HudOwnRemovalHoldCapSec hard min 1.25 s | HUD, TDC, registry | LANDED | hud.md:201, :244, :456, :463; TDC:124, :276; registry entities.yaml:2653-2656 and :2766-2769 (hard 1.25-2.0, safe 1.25-1.5). |
| D9 Roster accepts Disconnect for Expired (kick path); Session/Roster/Round aligned | Roster, Session, Round | LANDED | participant-state-roster.md:224 (R11 kick path), :692 (AC-PSR-20 axis); session-travel.md:429; round-match-framework.md:136, :148 ("confirmed by Roster R11 ... D9"), :737 (AC 26c). |
| D10 InformantKilled applied wherever not yet applied | Roster R8, registry, AC-PSR-20 | LANDED | participant-state-roster.md:157, :181 (on RetireBinding, ServerOnly log), :692; registry entities.yaml:84; npc-kill-penalty-informants.md:86, :172; camera-system.md:234; hud.md:81. |
| D11 HUD OQ5 removal wording, recorded | HUD, Penalty, index | LANDED | hud.md:543 (HUD-E10 title "Informant", subtitles "Too many wrong targets" / "Eliminated for the round", OQ5 closed D11), :75 (HUD-R9); systems-index.md:31; no "Exiled"/"Eliminated" card title remains in HUD, Penalty, Roster, Camera, ASN, ux/hud. |
| D12 TDC W-NKP-4 kept as guard, "unreachable in hard ranges" | TDC, Penalty | LANDED | tuning-data-config.md:264, :606; npc-kill-penalty-informants.md:478. |
| D13 Shipping-cook preset exclusion deferred to ADR-0004 amendment, noted in TDC OQ | TDC | LANDED | tuning-data-config.md:656 (OQ-13). |
| D14 Idle gaze layer, no AI hand-over | SCL, Crowd, pause-menu spec | PARTIAL | LANDED: shared-character-locomotion.md:107 (R8.9), :529 (IdleGazeDelaySec 3, 1-10, safe 2-5), :750 (AC-SCL-49); crowd-npc-behaviour.md:73 (G1 uses SCL layer), :357; registry entities.yaml:4832; design/ux/pause-menu.md exists (PM-1 referenced :30). NOT in TDC: tuning-data-config.md does not list IdleGazeDelaySec (see 2F-W6). Pause-menu body text not re-read (NOT CHECKED beyond existence). |
| D15 ADR-0021 L4 rule (display-only below ZOrder 100, Session-owned modal at 100) | ADR-0021 | NOT A GDD ITEM (LANDED) | adr-0021-ui-hud-layering-commonui.md:96, :101; decision-log.md:4901-4907. |

#### 2. Per-GDD "xdoc" follow-ups (rereview-followups.md)

| GDD / item | Target docs | Result | Evidence |
|---|---|---|---|
| IVP: registry team_position_fallback_feed 0-520, npc_bush_use_rate_target 0.633 | registry | LANDED | entities.yaml:1069 (0-520), :678 (0.633). |
| Roster: NPK cite SetOwnerTally / SetOwnerRespawnDue + ExpectedEpoch | Penalty | LANDED | npc-kill-penalty-informants.md:61, :142, :352; participant-state-roster.md:107, :111, :813 (AC-PSR-54). |
| Roster: registry connection_state wording | registry | LANDED | entities.yaml:186-189. |
| Roster: ADR-0001 S1/S2 fallback | ADR-0001 | NOT A GDD ITEM (LANDED) | adr-0001-crowd-identity-rules.md:180 (S2 fallback), :461. |
| Roster: pending decision 18 InformantKilled | Roster, registry | LANDED | see D10. |
| Penalty: registry informant_respawn.chain order | registry | LANDED | entities.yaml:340-341 (pick, release, takeover, card). |
| Penalty: TDC W-NKP-4 dead guard | TDC | LANDED | see D12. |
| Concealment: ASN-R8.2 / AC-ASN-39 "stateless hash SpotSalt" -> BushSpotPick | ASN, ADR-0017 | LANDED (with stale history rows) | assassination-defence-reversal.md:63, :318, :598; ADR-0017:416. Residual "SpotSalt" wording in disposition rows: ASN:506, :519 (see 2F-W8). |
| Concealment: registry exit_occlusion_timing_chain (a)-(f) | registry | LANDED | entities.yaml:1018-1020. |
| Concealment: registry BushEnterSwapSec safe lower bound | registry | LANDED | entities.yaml:1580, :1583-1584 (see section 4). |
| Pings: art bible 7.x row 9 (informant-only / spatial tick / feed line) | art bible | NOT A GDD ITEM (LANDED) | art-bible.md:1211 (team ping, any teammate, allies only), :2687 (row 9: "non-positional 2D UI tick ... no feed line and no plate or card"), :2607, :850-857, :1853-1855. |
| Pings: IVP TeamPing and ADR-0011 l457 "10 cm quantised" -> 0.1 cm | IVP, ADR-0011 | LANDED | information-visibility-policy.md:81; adr-0011-pawn-command-interface-input.md:461. (IVP:150 "10 cm grid" is the team position feed, a different field.) |
| Pings: Camera CS-R8 add Pings consumer | Camera | LANDED | camera-system.md:180, :301, :539, :651. |
| Pings: ADR-0001 slot-index rule Entries[TeamSize] | ADR-0001 | NOT A GDD ITEM (LANDED) | adr-0001:165, :463; informant-pings-team-comms.md:68. |
| ASN: SCL + ADR-0018 confirm lunge not interrupted by death write | SCL, ADR-0018 | LANDED | shared-character-locomotion.md:162; adr-0018-encounter-outcome-timing.md:143. |
| ASN: TDC I-ASN-1 at 290 cm | TDC | NOT LANDED (conditional, low) | tuning-data-config.md:249 states the I-ASN-1 formula only; the 290 cm wording appears only at assassination-defence-reversal.md:188. The follow-up said "when it restates the invariant"; TDC does not restate it, so no contradiction (2F-W9). |
| OC: systems-index item 15 Depends On adds Crowd + Session | systems-index | LANDED | systems-index.md:62. |
| OC: entities.yaml ClueMatchMaxCount, ObjectivesPerStageCount, bRivalSignalEnabled, ObjectiveStallGapDefectSec, ObjectiveFinalDwellDefectSec | registry | LANDED | entities.yaml:4430, :4446, :4462, :4475, :4490. |
| OC: Pings GDD confirm no stage/card/court data | Pings | LANDED | informant-pings-team-comms.md:68 ("carries no stage, objective card, court, site, clue or other world-state data"). |
| OC: concept "~12 min median" vs F1 ~9 min | game-concept | LANDED | game-concept.md:318 (median near F1's ~9 min; 12 stays a tuning target). |
| IIF: entities.yaml IIF knobs (InteractFacingHalfAngleDeg, InteractMoveGraceSec, UnmatchedEndPerSec, AbuseLogDropCount, AbuseLogWindowSec, BucketInteract*, BucketPrepare*, BucketHidePerSec) | registry | LANDED | entities.yaml:4279-4413. |
| IIF: Round due-time epsilon | Round | NOT LANDED | input-interaction-framework.md:280 says the heap treats an item as due when `t_now >= DueTime - eps` ("the comparison is Round's; see the cross-doc follow-up on eps"), :288 eps = 1e-4 s. round-match-framework.md:108-110 and :711 (AC 45) state `DueTime <= DrainStartTime` with no epsilon; no eps anywhere in Round (see 2F-B2). |
| IIF: systems-index IIF adds IVP hard dep | systems-index | LANDED | systems-index.md:50. |
| IIF: TDC CR-10 landed? | TDC | LANDED | tuning-data-config.md:57 (CR-10), :485 ("IIF CR-10 ... APPLIED"). |
| IIF: OC CR-7 superseded | OC | LANDED | objectives-chain.md:417 ("IIF CR 7 ... DECLINED (superseded)"). |
| Camera: SCL R9 drop "user to confirm Camera OQ9" | SCL | LANDED | shared-character-locomotion.md:109, :764 ("confirmed by the user 2026-10-25, Camera OQ9 closed"). |
| Camera: ADR-0015 245 -> 295 traces, 50 -> 60 chars | ADR-0015 | NOT A GDD ITEM (LANDED) | adr-0015-camera-los-envelope.md:32, :118, :377. |
| Camera: ADR-0022 mask cost | ADR-0022 | NOT A GDD ITEM (LANDED) | adr-0022-rendering-performance-budgets.md:363-365. |
| Camera: TDC hysteresis key | TDC, registry | LANDED | see D6. |
| Round: Roster + Session confirm Expired transition (CR-21) | Roster, Session | LANDED | see D9. |
| Round: registry StartLeadSec, PreparingWatchdogSec, MaxTeamImbalanceCount, PawnLossRecoveryMaxCount, RoundLogMaxEntriesCount | registry | LANDED | entities.yaml:4506, :4522, :4537, :4552, :4568. |
| Round: Penalty/HUD TeamSummary.WrongTargetCount reads KillResolved | Round, Penalty, HUD | LANDED | round-match-framework.md:99; npc-kill-penalty-informants.md:142; hud.md:109. |
| TM: register TeammateMarkerAnchorCm, EdgeGapSize, FadeSec, LabelFocusSize, LargeMult | registry | LANDED | entities.yaml:4584, :4599, :4615, :4631, :4647. |
| TM: Pings copies F3 ramp | Pings | LANDED | informant-pings-team-comms.md:264 (k_b 0.25, eps_b 0.5); teammate-markers.md:195, :204-206. |
| TDC: stale NpcKillStrikeLimit "1-5" in HUD/Penalty/ASN | HUD, Penalty, ASN | LANDED | hud.md:276; npc-kill-penalty-informants.md:219, :275, :357; assassination-defence-reversal.md:245 (all 2-5); registry entities.yaml:3140-3143 (hard 2-5, safe 2-4). |
| TDC: stale NpcCount ranges in CON/IVP/Crowd/OC formula tables | CON, IVP, Crowd, OC | LANDED | concealment-bushes.md:147; information-visibility-policy.md:308; crowd-npc-behaviour.md:207; objectives-chain.md:208 (all safe 35-50, hard 6-100, default 50). |
| TDC: registry I-4 "warns" -> "rejects"; sentinel fields EndedAutoResetSec / SessionHeartbeatIntervalSec | registry, TDC | LANDED | entities.yaml:852-853; tuning-data-config.md:124. |
| CPA: Penalty + LSC own Pillar-1 review (CPA OQ-14) | CPA, Penalty, LSC | PARTIAL | LANDED: character-pool-appearance.md:535 (ownership moved), npc-kill-penalty-informants.md:590 (OQ-NKP-13). STALE: registry entities.yaml:519 still reads "Pillar-1 review of the restriction pending, CPA OQ-14" (2F-W5). |
| CPA: ADR-0008 constexpr seed array | ADR-0008 | NOT A GDD ITEM (LANDED) | adr-0008-test-automation-leak-parity.md:130, :233, :272. |
| Crowd: bush target 315 -> 300 | see D7 | LANDED | see D7. |
| Crowd: registry npc_bush 0.633 | registry | LANDED | entities.yaml:678. |
| Crowd: SCL stale PENDING-USER walk-slower row | SCL | LANDED | no PENDING-USER tag in shared-character-locomotion.md. |
| Crowd: TDC register CR-22 keys | TDC | LANDED (as a recorded decision) | tuning-data-config.md:124 and :657 (OQ-14: CR-22 unkeyed constants deliberately not mirrored); decision-log.md:4897. |
| Crowd: ADR-0002 CrowdSchedule timer kind | ADR-0002 | NOT A GDD ITEM (LANDED) | adr-0002-resolution-queue-round-end-staging.md:89, :112, :244, :455. |
| SCL: registry pawn_network_budget 26 -> 31.1 KB/s vs 36 line | registry | LANDED | entities.yaml:1323-1326. |
| SCL: systems-index deps change CR-12 | systems-index | LANDED (one nit) | shared-character-locomotion.md:513 ("met"); systems-index.md:52, :53, :56, :59 list SCL / LSC peer. Nit: row 8 (LSC) Depends On does not list SCL (2F-W11). |
| SCL: Crowd names pace-set key | Crowd | LANDED | crowd-npc-behaviour.md:65 (reads StopMinSec, owns no pace-set key). |
| SCL: R3 host capture with 9 clients | SCL | LANDED | shared-character-locomotion.md:728. |
| SCL: art bible B4 | art bible | NOT A GDD ITEM (LANDED) | art-bible.md:1883. |
| HUD: registry HudPersistentAreaLimitPct, HudResultPlateOutSec, HudRemovalCardHoldSec, HudRemovalCardOutSec, HudFeedLineCount, HudObjectiveStepSec | registry | LANDED | entities.yaml:4664-4739. |
| HUD: result_show_time -> ContactServerTime | registry | LANDED | entities.yaml:1363-1364. |
| HUD: ADR-0021 list HUD-E22 L0, HUD-E21 L3 | ADR-0021 | NOT A GDD ITEM (LANDED) | adr-0021:91, :95, :367-372. |
| HUD: D8 hard min 1.25 | see D8 | LANDED | see D8. |
| HUD: OQ5 wording | see D11 | LANDED | see D11. |
| Session: ADR-0005 l193 timeout 111/120 s | ADR-0005 | NOT A GDD ITEM (LANDED) | adr-0005-session-seamless-travel-state-stores.md:193, :424. |
| Session: Round Interactions OnLobbyTravelComplete / OnRoundTravelStarted | Round | LANDED | round-match-framework.md:492. |
| Session: handle OnParticipantDisconnected from failed supersede-rejoin | Round | LANDED | round-match-framework.md:150. |
| Session: Roster R11 Lobby Rejoining projection + ConnectionGeneration bump | Roster, Session | LANDED | participant-state-roster.md:214, :218-220; session-travel.md:168. |
| Session: registry connection_state values + 4 Session keys | registry | LANDED | entities.yaml:186-187; :4755, :4770, :4785, :4800. |
| LSC: registry team_spawn_zone.respawn_region nearer-anchor wording | registry | LANDED | entities.yaml:519 (nearer-anchor; but see CPA OQ-14 pointer above). |
| LSC: ADR-0014 7.3 per-zone-pair region, 6 40-byte graybox hash | ADR-0014 | NOT A GDD ITEM (LANDED) | adr-0014-layout-service-generation-navmesh.md:170, :500, :501. |
| LSC: NPK + ADR-0019 4.3 respawn-region wording | Penalty, ADR-0019 | LANDED | npc-kill-penalty-informants.md:80, :358; level-socket-contract.md:117, :482; adr-0019:44, :396 (ADR-0019 closeout note present; its 4.3 text carries no conflicting wording). |
| LSC: OC ClueTooBroad edge case uses ClueMatchMaxCount | OC | LANDED | objectives-chain.md:284, :466. |

#### 3. gdd-pass-remaining.md items

| Item | Target docs | Result | Evidence |
|---|---|---|---|
| Roster Dependencies status cells -> In Review | participant-state-roster.md | LANDED | review-log entry; Roster Dependencies table uses "In Review" (no stale token found by grep). |
| Session: kick wording, Rejoining projection, status cells | session-travel.md | LANDED | session-travel.md:168, :429; no stale status tokens. |
| Round: CR-21 confirmed, Interactions rows, OnParticipantDisconnected, status cells | round-match-framework.md | PARTIAL | CR-21, Interactions and R17.2 landed (:136, :148, :150, :492). Dependencies table is "In Review" (:506) but the Cross-References table still reads "Written" for the six sibling GDDs at :589-594 (2F-W3). |
| SCL bidirectionality paragraph marks CR-12 met | shared-character-locomotion.md | LANDED | :513. |
| PCG, LSC status cells | PCG, LSC | LANDED | procedural-city-generator.md:415-420 and level-socket-contract.md:671-678 (all "In Review"). ("Written" at PCG:421 / LSC:679 is the art bible, not a GDD.) |
| IVP: NpcCount ranges, TeamPing 0.1 cm, status cells | IVP | PARTIAL | NpcCount (:308) and 0.1 cm (:81) landed; Dependencies / Cross-References cells "In Review" (:475-490) except :491 hud "Reviewed, revisions applied" (2F-W3). |
| Pings: PNG-R6 no stage/card/court; F3 ramp; status cells | Pings | LANDED | informant-pings-team-comms.md:68, :264; status cells "In Review". |
| Status cells: IIF, TM, CPA | IIF, TM, CPA | LANDED | review-log entries for all three; no stale status tokens found in the three GDDs. |
| Art bible 7.x row 9; B4 restated for D2 | art bible | NOT A GDD ITEM (LANDED) | art-bible.md:2687, :1883. |
| Replication verification of AC-ASN-05, 20, 21, 44, 45 | ASN GDD, review log, decision log | LANDED (process evidence); AC text not re-read | assassination-defence-reversal-review-log.md:75-77 (specialist re-verified, ACs rewritten, negative controls); decision-log.md:5133-5139 (AC-ASN-45 test seam). Stale markers: review log :61 "[OPEN] ... not done" and GDD header ASN:3 "still pending" (2F-W2). AC text spot-check: see Coverage. |
| Concealment BushEnterSwapSec 0.46 in F3, knob table, registry | CON, registry, TDC | LANDED | see section 4. |
| Decision-log: D11 removal wording | decision-log | LANDED | decision-log.md:4989-4995. |
| Decision-log: AC-ASN-45 rewrite | decision-log | LANDED | :5133-5139. |
| Decision-log: D4 value 44 m (<= 44.4 m) | decision-log | LANDED | :4997-5003. |
| Decision-log: Penalty OQ-NKP-13 ownership | decision-log | LANDED | :5005-5011. |
| Decision-log: D10 on RetireBinding, not transition reason | decision-log | LANDED | :5013-5019, correction :5029-5035. |
| Decision-log: AC-SCL-50 test values (93 cm/s) | decision-log | LANDED | :5021-5027 (SCL:751 AC-SCL-50 lists 93; SCL:226 says "about 94" for the same boundary, 93.5 exact: cosmetic). |
| Review-log "Closeout cross-doc pass 2026-10-25" notes | design/gdd/reviews/ | PARTIAL | Present in 19 of 20 logs (all 17 named plus IIF and TM). ABSENT: tuning-data-config-review-log.md (TDC changed: TDC:110, :111, :124, :264, :656, :657) (2F-W1). |

#### 4. Specific checks

- BushEnterSwapSec safe lower bound 0.46: concealment-bushes.md F3 table :179 and prose :188, knob table :293 (safe 0.46-0.7); registry entities.yaml:1580 (safe_range 0.46-0.7) and :1583; TDC:124. LANDED. Remaining "0.45" hits are BushExitCrossSec defaults or the explicit explanation that Swap 0.45 gives 211 cm/s at Reach 60.
- Status cells (GDD sibling status columns still pre-review): see 2F-W3.
- Header status lines: see 2F-W4.

#### Blocking

- 2F-B1 (D4 PARTIAL, LSC internal contradiction): level-socket-contract.md:219 (level-design recipe) still says "Final-eligible courts are those at least 60 m path from every start cell"; the same GDD at :145 and :585 and the Generator (procedural-city-generator.md:67, :354, :531) now say StartToFinalMinPathCm = 44 m (4,400 cm; safe 4,000-4,400). An author following the recipe applies 60 m, which admits only about 2 final-eligible courts per corner (PCG S4) and re-opens the OQ9 failure. Fix: change :219 to "at least `StartToFinalMinPathCm` (44 m)".
- 2F-B2 (IIF vs Round due-time rule, NOT LANDED): input-interaction-framework.md:280 and :288 define hold completion as `t_now >= DueTime - eps` (eps 1e-4 s) and say "the comparison is Round's"; round-match-framework.md:108-110 and AC 45 at :711 define the heap by `DueTime <= DrainStartTime` with no epsilon. Round never adopted the follow-up, so the owner of the comparison does not state the IIF's rule; a Round implementation from its own text hits the 3.0 / (1/60) float-edge the IIF epsilon exists to prevent (hold completes one drain late). Fix: add eps (reference the IIF constant) to Round R12 and AC 45.

#### Warnings

- 2F-W1 (process): tuning-data-config-review-log.md has no "Closeout cross-doc pass 2026-10-25" entry although TDC was changed in the pass.
- 2F-W2 (process / stale status): assassination-defence-reversal-review-log.md:61 still reads "[OPEN] Replication-specialist re-verification ... not done in lean mode" and assassination-defence-reversal.md:3 header says the re-verification is "still pending", while review log :76 and decision-log.md:5133 record it done.
- 2F-W3 (process, stale status cells in sibling-GDD tables; the closeout lists did not name these files): concealment-bushes.md:366-375 ("Designed", "Designed (pending review)", and :365 "Reviewed (NEEDS REVISION, revisions applied)"); assassination-defence-reversal.md:469-482 ("Reviewed, NEEDS REVISION", "Reviewed, MAJOR REVISION NEEDED" for Round and TDC); objectives-chain.md:382-388 (same tokens; its :292 snapshot note covers a different table); npc-kill-penalty-informants.md:449-464 ("Reviewed, NEEDS REVISION; reconciled", Round / LSC / TDC "MAJOR REVISION NEEDED"); round-match-framework.md:589-594 ("Written" for six sibling GDDs); information-visibility-policy.md:491 (hud "Reviewed, revisions applied"); hud.md:584 (game-concept "Under Review (concept)" while concept is Approved). All systems-index rows 1-18 and 20 are "In Review". Other MVP GDDs are clean (their "not written" cells point at Vertical Slice / Alpha systems 21-28, which are correctly unwritten).
- 2F-W4 (process, header status lines): every changed GDD states "Reviewed - NEEDS REVISION 2026-10-25 ..." (informant-pings-team-comms.md:3 alone says "In Review"), consistent in meaning with index "In Review" but not literally. Four headers carry stale open-item text: procedural-city-generator.md:3 ("2 blocking items open ... pool geometry is Open Question 9; re-review after a decision", closed by D4 at :531); round-match-framework.md:3 ("kick-path confirmation with Roster/Session outstanding", confirmed :136/:148); shared-character-locomotion.md:3 ("2 blocking items remain", OQ15/OQ16 closed by D2/D3 at :771-772); hud.md:3 ("HUD keys missing from the entity registry remain a cross-document follow-up", all registered at entities.yaml:4664-4739).
- 2F-W5 (stale pointer): registry entities.yaml:519 (team_spawn_zone.respawn_region) still says "Pillar-1 review of the restriction pending, CPA OQ-14"; ownership moved to Penalty OQ-NKP-13 (CPA keeps no open item, character-pool-appearance.md:535).
- 2F-W6 (registry gap): D14 key IdleGazeDelaySec is registered (entities.yaml:4832) and in the SCL knob table (:529) but is not in the TDC owner table (tuning-data-config.md:110 counts only StopMinSec as new for SCL); the data-driven validator reads TDC. D2 and D6 keys were added to TDC, D14 was not.
- 2F-W7 (registry gap): StartToFinalMinPathCm (D4, 44 m) has no registry entry and is not listed in TDC although the decision log says the registry was updated; the sibling LSC distance keys are also unregistered, so register the group together.
- 2F-W8 (stale reference to a removed element, history rows): assassination-defence-reversal.md:506 and :519 still describe Concealment CR 10(d) as "filter-then-pick with `SpotSalt`" ("ALREADY DONE"); `SpotSalt` no longer exists (replaced by the `BushSpotPick` stream, :63). concealment-bushes.md:391 quotes the original request the same way. Add "(now BushSpotPick)" to the disposition rows.
- 2F-W9 (conditional, low): TDC I-ASN-1 row (tuning-data-config.md:249) does not carry the 290 cm cap wording; ASN F1 (:188) does. No contradiction (hard maximum 250 stands), only the optional restatement is missing.
- 2F-W10 (stale text): information-visibility-policy.md:18 says "ADR-001 (crowd identity rules, not yet written)" (docs/architecture/adr-0001-crowd-identity-rules.md exists, Accepted); :497 and :501 list "Concealment (not written)" and "Teammate Markers (not written)" in the change-request list (both GDDs exist).
- 2F-W11 (low): systems-index.md row 8 (Level Socket Contract) Depends On does not list Shared Character & Locomotion although row 9 lists LSC as a soft peer and shared-character-locomotion.md:513 calls the bidirectionality change request met.
- 2F-W12 (outside GDD scope, none found): every ADR / art-bible-only item named in the closeout files was found landed (ADR-0001, 0002, 0004, 0005, 0008, 0010, 0011, 0014, 0015, 0018, 0019, 0021, 0022, art-bible B4 and 7.x row 9). No outside-GDD-scope gap to report.

#### GDD flags

| GDD | Finding IDs | Priority |
|---|---|---|
| level-socket-contract | 2F-B1, 2F-W11 | High |
| round-match-framework | 2F-B2, 2F-W3, 2F-W4 | High |
| input-interaction-framework | 2F-B2 | High |
| tuning-data-config | 2F-W1, 2F-W6, 2F-W7, 2F-W9 | Medium |
| assassination-defence-reversal | 2F-W2, 2F-W3, 2F-W4, 2F-W8 | Medium |
| shared-character-locomotion | 2F-W4, 2F-W6 | Low |
| procedural-city-generator | 2F-W4 | Low |
| hud | 2F-W3, 2F-W4 | Low |
| concealment-bushes | 2F-W3, 2F-W8 | Low |
| objectives-chain | 2F-W3 | Low |
| npc-kill-penalty-informants | 2F-W3 | Low |
| information-visibility-policy | 2F-W3, 2F-W10 | Low |
| informant-pings-team-comms | 2F-W4 | Low |
| character-pool-appearance | 2F-W5 (registry pointer) | Low |
| systems-index | 2F-W11 | Low |
| registry (entities.yaml) | 2F-W5, 2F-W6, 2F-W7 | Medium |

#### Coverage

Checked: every D1-D15, every per-GDD xdoc follow-up in rereview-followups.md, every PARTIAL / REMAINING item in gdd-pass-remaining.md, review-log notes for all 20 logs, all six named decision-log entries, BushEnterSwapSec, status-cell and header scans of all 19 MVP GDDs.
AC-ASN-05, 20, 21, 44, 45 opening text was spot-checked after the table was written: assassination-defence-reversal.md:552 (6 independent fresh scenario instances), :572 (all other pawns stationary), :573 (parity group at the same bearing), :603 (`Culled` precondition asserted), :604 (defensive path, no such reason in MVP) match the rewrite described in the review log; the full AC bodies (zero-tolerance bound, negative controls) were not read to the end.
NOT CHECKED: design/ux/pause-menu.md body (D14 owner) beyond confirming it exists and cites PM-1.

## Appendix G — Phase 3 design theory (detail, sub-agent output verbatim)

### /review-all-gdds full — Phase 3: Game Design Holism (2026-10-25)

Inputs read (by section; no whole-file reads of GDDs over 40 KB): the Player Fantasy of all 19 system GDDs (ASN, Camera, CPA, CON, Crowd, HUD, Pings, IVP, IIF, LSC, Penalty, OC, Roster, PCG, Round, Session, SCL, TMK, TDC). Pings: Summary to Edge Cases (1-215) and Open Questions (344-355). Targeted greps: SCL R8.9 idle gaze layer (line 107), `StopMinSec`/`IdleGazeDelaySec` knobs (528-529), brace entry clip (67, 80-82); Crowd CNB-R16 Tell Matrix (114-134); ASN-R4/R5/R7; OC F1 (158, 173-174), F3 `P_full` (199-215), F4 (233), `RivalPrelimCount` (61-135); PEN Player Fantasy and PEN-R14; IIF R15a binding (187, 209); HUD plate and tally text (30, 67, 71, 270); Round F7 `ObjectiveEndShare` (390-401) and `FRoundResult.TeamSummary` (175); TMK reconnecting suffix (102); concept MVP items 9-10 (333-334), pacing (318), open items (375-378). Prior pass: `production/session-state/review-all-gdds-2026-10-24-phase3.md`. The registry was not needed (values taken from the GDDs).

**Verdict: 0 Blocking, 6 Warnings.** Prior D-W1 and D-W2: unchanged (deferred to telemetry, not re-raised). D-W3: severity unchanged, but its inputs moved: one new factor makes camping cheaper (D3-W3) and one adds a non-camping lever for the trailing team (informant scouting with pings). D-W4: partly resolved in the concept, not propagated to Round F7 (D3-W5).

#### Checklist

- **3a Progression loop competition: PASS.** Still no kill count, score or per-player progress. The new elements are not goals and do not compete with the race. The round summary holds per-team aggregates and is released only at `Ended` (Round 175, concept 333). The wrong-target tally is a cost and an upper bound. Pings are a verb, not a counter (PNG-R8: no feed line, no counter). The informant role is still strictly reduced. Mastery remains the only progression.
- **3b Attention budget: PASS, at the limit (4 active, unchanged).** Travel (Active): reading the crowd, finding the court from the clue, anticipating threats for the brace, and the team channel = 4. The ping folds into the team channel: one aimed press, no hold, no acknowledgement, no feedback when refused (PNG-R3, OQ3). Received rings are glanced at like markers. The walk-slower modifier (held Left Ctrl 0.8 / Left Alt 0.9, IIF 209) is **optional**: Crowd T16 makes slow walking ordinary at the per-head base rate, and full pace is ordinary too (`CrowdFullPacePct` 55). So pace matching is not a required fifth channel. The idle gaze layer is automatic (SCL R8.9). Tend: 2-3. Informant: walk, watch, ping, stay unread = 3. Watch item: if playtests show players holding Ctrl to match a slow local cluster as routine, pace becomes a fifth channel. Track it with the walk-slower share in telemetry. No finding.
- **3c Dominant strategies: WARN (3 new).** Checked: Active-player pings as an un-telled advantage (P2 justification PNG-R2 holds: a ping carries only what the sender saw, the same as external voice, and opponents receive nothing (PNG-R6, R9)); ping as a "this citizen is a player" proxy (no: never sent to opponents, never names a pawn, the trace ignores pawns); informant ping spam (bucket 2 + 0.5/s, one slot per sender: spam is bounded, but see D3-W1); pinging from inside a bush (D3-W2); walk-slower as a camping tool (no: slow pace is base rate, and it gives no information advantage); idle gaze layer and "stand still and watch" (D3-W3); round summary and reconnect indicator leaks (no: the summary is per-team aggregates at `Ended` only, with no per-participant link (Round 175, IVP OQ7/VS); the reconnect suffix is Team audience only and leaves the marker shape unchanged (TMK 102)); stop floor (no new strategy; brace entry is a distance-matched stance clip from any walk speed (SCL 67), so the stop floor neither masks an early brace nor adds a parry window); informant baiting (an informant acting suspicious to draw a strike costs the striker a tally tick and a T6 confession, and costs the informant ~15.5 s plus the walk. Healthy: a chosen deviation that serves P2 and the hunter's doubt).
- **3d Economy (non-traditional resources): PASS, with D-W3 carried.** Information faucets: the crowd (own eyes), markers, steps, `RivalPrelimCount` (ON, 30-165 s delay on a 30 s grid, latest 195 s), and now pings. Ping sink: the token bucket (burst 2, 1 per 2 s) and one live ring per sender. Strikes: the allowance L−1 = 2, sink only. The tally ticks for informant kills but they are not strikes (PEN-R14). Brace: 60 s cooldown, unchanged. Informant respawns: unlimited, cost ~15.5 s of black plus the walk. Objective progress: permanent. **Balancing loops:** eliminating rivals gives the losing team more informants, so it gains more pings. That is a catch-up loop, and an intended one. Exile from wrong targets shrinks the reckless team. **Runaway:** the race lead still never decays (D-W3), but informant pings now give the trailing team a second lever besides camping: scouting final candidates for tenders (Pings anchor 1, "the ring lands by a court where someone has been loitering"). This only applies if the trailing team has informants. No undecidable state: mutual stalls remain SP-6 plus host abort, which is accepted.
- **3e Difficulty and pacing curve: WARN (1).** OC F1 models ~9 min (`T_chain` + ~2 min contest) against the concept's ~12 min target. The concept and OC both call 12 min non-binding, and LSC has a lever (chain length toward 220 m). Round F7's stall and abort thresholds are still anchored to 12 min (`DurationSec` 1440 s stall = 2.7× the modelled round), and its P4 target (> 0.5, no team-size bucket) is weaker than the concept's validation (≥ 70 % of ≥ 20 rounds, 4v4+ buckets) (D3-W5). `NpcCount` 50: the rival share among non-allies at 5v5 falls from 5/45 = 0.111 to 5/55 = 0.091; at 2v2 it is 2/52 = 0.038. `P_full` was re-based to 0.70 by rescaling the decoy-tend rate (OC 208, 215), so tend reads are unchanged. Blind strikes and bush picks get slightly costlier. That is consistent with P3 and widens the 2v2-vs-5v5 gap behind D-W4. Nothing scales with elapsed time (no timer, P4).
- **3f Pillar alignment: PASS, no written anti-pillar violation; 2 emergent risks.** All 19 Player Fantasies cite at least one pillar by name (Pings: P1 primary, P2, P3, Fellowship). Anti-pillar "NOT an identification-tools game": the ping is a place (4 m ring, never attached to a pawn, PNG-R1), markers are allies-only, the reconnect suffix is team-only, and the summary is aggregates at `Ended`. **But** ping chains on one walker approach a sticky tracker (D3-W1). Anti-pillar "NOT a reflex game": the stop floor (D2) strengthens it (no sudden stop for any pawn, T11). Brace entry is unchanged (ASN-R5 sampled at execution; D-W1 deferred). Anti-pillar "NOT a hide-and-wait game": the bush plus ping spotter post (D3-W2) and idle-gaze watching (D3-W3) both lower the cost of waiting.
- **3g Fantasy coherence: PASS with 1 language seam.** The new fantasy, Pings' "Your eyes are still in the city", fits the shared identity ("the hidden predator who pays exposure to race") and gives Penalty's watcher role its payoff. The informant and Active halves are coherent. **Seam:** "exile" means two different things in player-facing language (D3-W6). Smaller drift: the PEN fantasy still says "voice and text to the team continue throughout", but PNG-R11 has no in-game text in MVP (external text only).

#### Blocking

None.

#### Warnings

##### D3-W1. Ping chains give the team a near-continuous, through-wall position for one walker, paid for by an expendable informant
- **GDDs:** Pings PNG-R1 (4 m ring, place not person), PNG-R6 (one slot per sender; a new ping replaces the old ring at once), PNG-R7 (rings show to allies regardless of their line of sight; edge-clamped when off-screen), F1 (`PingBucketPerSec` 0.5 = the **top** of its CD-capped safe range; burst 2), F3 (`PingLifetimeSec` 8), OQ5 (sparse street = one figure in the ring), OQ7 (chain metric); concept user decision 4 ("no sticky tracking marker"); anti-pillar "NOT an identification-tools game"; PEN Player Fantasy (informant walk-only; the downtime ~15.5 s is the role's cost); PEN-R14 (an informant kill ticks the tally but is not a strike).
- **Reasoning:**
  - At the default refill, one informant can re-mark every 2 s. A walker at full pace (140 cm/s) covers 2.8 m in 2 s, less than the 4 m ring radius. So a re-mark every refill keeps the suspect inside a live ring at all times, and each new ring replaces the last with no fade.
  - Two informants alternating give a 1 s refresh. Allies see the ring through walls and as an edge marker when off-screen. In a sparse street the ring holds one figure (OQ5).
  - In effect this is a tracking marker on a person, assembled by hand. The CD ruled manual shadowing to be intended informant work. The concern here is the *cadence*, not the existence of shadowing.
  - P2 is met formally: shadowing is a visible deviation. But it is paid for by a body with nothing left to lose. Killing it costs the shadowed player a tally tick and a T6 confession in public, and the informant is back within ~15.5 s plus a walk. So the tell has almost no price for the team that uses it.
  - Late in a lopsided round, the losing team can have up to 4-5 informants. Catch-up through information is healthy. Catch-up through a tracker is the identification-tools anti-pillar.
- **Options:**
  - (a) Keep the defaults, and set the OQ7 failure threshold now, before the first playtest. Example: the share of rings that are chain links above X % or chains longer than N links. An unbounded "measure it" cannot fail.
  - (b) Add a no-refresh rule: a ping within 4 m of the sender's own live ring is refused, or does not reset its age, so one sender cannot keep a single walker lit.
  - (c) Lower `PingBucketPerSec` to 0.2-0.25 (one per 4-5 s). A walker then leaves the 4 m ring between marks (5.6-7 m travelled), so the ring stays "here, recently" and not "here, now".
  - (d) Shorten `PingLifetimeSec` toward 4-5 s together with (c), so that rings stop overlapping into a trail.
- **Creative call:** yes (it bears on a CD ruling and on concept decision 4). **Always-ask:** (b) yes (rule change); (a), (c), (d) no (knob or test-plan).

##### D3-W2. Pinging from inside a bush turns concealment into a productive spotter post
- **GDDs:** Pings Edge Cases ("an Active player may ping from inside a bush … not an `OnConcealmentBroken` trigger"), PNG-R2 P2 justification; CON Player Fantasy ("a pause, a held breath … never a fortress"; "It is a brief cover, not a post: the time-concealed budget of AC-CON-24 (≤ 8 % of round time) is the whole of the role"), CON pillar line ("hiding is a pause, not a strategy: no reward for staying"); anti-pillar "NOT a hide-and-wait game"; P4.
- **Reasoning:**
  - Before pings, a concealed player had no productive verb: no Interact, no Prepare (prior pass, SP-8). Staying in a bush earned nothing.
  - With pings, a player in a bush that overlooks a court or a route can watch from the "look-out" (CON anchor 2), which no opponent can see, and mark sightings precisely for the team. That is a reward for staying.
  - External voice could already relay this. The ping adds precision (a 4 m ring that allies see through walls) and makes the role cheap to perform.
  - Informants cannot hide (PEN fantasy), so the role falls to an *Active* player who is out of the race while hidden. That is the hide-and-wait pattern P4 forbids, now with a job attached.
  - The only guard is a measurement (AC-CON-24 ≤ 8 %), not a rule.
- **Options:**
  - (a) Refuse `IA_Ping` while the sender's own pawn is concealed. This is owner-known, so nothing is sent. It costs nothing under P1, because pinging is invisible either way, and voice stays available.
  - (b) Keep it, and split the ping telemetry (OQ6) by concealment state. Add "pings sent while concealed" to AC-CON-24's analysis, with a threshold.
  - (c) Keep it, and accept the voice-equivalence argument explicitly in CON's fantasy, so the "no reward for staying" line stays true.
- **Creative call:** yes. **Always-ask:** (a) yes (rule change in Pings and Concealment); (b), (c) no.

##### D3-W3. The idle gaze layer hides the gaze of a watcher who holds the camera still, which makes "stand and watch" cheaper (D-W3 worsened)
- **GDDs:** SCL R8.9 (line 107: after `IdleGazeDelaySec` 3 s without a look change the server overlays the NPC head scan; "It changes only the replicated `HeadLook` target, never the control rotation, so the player's camera and aim do not move"), SCL knob row 529 ("Low: an attentive player who holds the view still gets a wandering head"); Crowd T13 (head hold > 2.5 s on a passer-by, LR ≈ 0.5), T14 (a stand is base rate up to `CrowdMaxIdleSec` 20 s); decision D14 (a long stand is a player-chosen risk, like AFK); Camera Player Fantasy (P2: "watching from cover costs a body standing in the open"); OC SP-4 (camping the final candidates); anti-pillar "NOT a hide-and-wait game".
- **Reasoning:**
  - The D14 rationale is correct for P1: a menu-open or idle player must not freeze into a tell.
  - The layer cannot tell idle from *deliberately still*. A player who fixes the camera on a court, a doorway or a standing suspect has their body's gaze held for 0.5 s past the T13 threshold (2.5 s → 3 s). After that, the head scans like an NPC while the camera keeps watching.
  - What remains for an observer is position and stand length. Stands up to 20 s are base rate (T14), so a camper who repositions every ≤ 20 s shows almost nothing.
  - So the most useful passive act in the game, watching a fixed spot, now carries no involuntary tell and almost no chosen one. The knob note treats the wandering head as a *cost* to the attentive player, when it is actually an advantage.
  - Effect on D-W3: watching the final candidates gets cheaper. Effect on P2: "watching from cover costs a body standing in the open" now means the body only, not where it looks.
- **Options:**
  - (a) Accept, and record in Crowd and SCL that P2's price for watching is body position and stand length only (gaze is never a tell for a still camera). Then make T14's 20 s rule carry the load, for example by also logging repeated returns to the same spot.
  - (b) Raise `IdleGazeDelaySec` within its 2-5 safe range (for example 5 s), to make T13 holds visible for longer. The trade-off is a longer frozen head on a menu-open player.
  - (c) Point the scan toward the control rotation (sweeps centred on where the camera looks, not free), so a long hold still reads as "looking that way" while the head is never frozen.
  - (d) Add a playtest metric: the share of final-candidate kills made by a striker who stood within 15 m of the court for over 10 s.
- **Creative call:** yes. **Always-ask:** (c) yes (rule change; the animation layer behaves differently); (a), (b), (d) no.

##### D3-W4. Pings have no Crowd Tell Matrix row, and their P2 exception is argued only in the Pings GDD
- **GDDs:** Crowd CNB-R16 ("the authoritative list"; Camera got T17 "none, not an advantage"); Pings PNG-R9 (the only body correlate is the gaze, T13), PNG-R2 (P2 exception: "a team advantage that shows no readable deviation"); concept P2 design test.
- **Reasoning:**
  - The ping is the second advantage in the game that has no tell. Camera mode is the other, and it is "not an advantage" (T17).
  - The ping *is* an advantage (Fellowship), and it is accepted under an explicit P2 exception.
  - The Tell Matrix is the cross-system ledger that P2 is audited against. Without a row there, the exception, its OQ8 fallback (informant-only) and its body correlate (T13, which D3-W3 now weakens) are invisible to anyone checking P2 from the Crowd side.
- **Options:**
  - (a) Add a row T18 "Team ping: no visible deviation; accepted P2 exception (PNG-R2); body correlate T13; owner Pings; fallback OQ8".
  - (b) Leave it as is, and cross-reference from PNG-R9 only.
- **Creative call:** no (bookkeeping). The exception itself is already a creative decision on record. **Always-ask:** no.

##### D3-W5. Round pacing and P4 validation thresholds no longer match the concept or the OC model (D-W4 partly resolved)
- **GDDs:** OC F1 (lines 173-174: ~9 min round; "travel cannot carry 12 minutes"; 12 min non-binding); concept line 318 (median < 6 min triggers OC levers; `ObjectiveEndShare` per team-size bucket, only 4v4+ counts) and the pillar validation (≥ 70 % of ≥ 20 rounds); Round F7 (lines 390-401: `ObjectiveEndShare` target > 0.5 with no team-size filter; `StallFlag` at `DurationSec` > 1440 s and the active-abort flag at 720 s, both anchored to the 12 min target).
- **Reasoning:**
  - Three documents now carry three P4 thresholds: concept ≥ 0.70 per 4v4+ bucket, Round F7 > 0.5 unbucketed, OQ/AC wording elsewhere > 0.5.
  - Round's stall detector fires at 24 min, 2.7× the modelled 9 min round. A round that stalls for 15 minutes past its natural length is not flagged. That is the "wins by waiting" case P4 asks to catch.
  - D-W4's team-size bucketing reached the concept but not the formula that computes the metric. A 2v2-heavy playtest will still pass Round F7. With `NpcCount` 50 the gap grows: rival share 0.038 at 2v2 against 0.091 at 5v5.
- **Options:**
  - (a) Re-base Round F7 on OC F1: stall at 2 × `T_round` modelled (~18 min) or a separate `StallMinutes` knob. Add `TeamSize` to the log row, and compute `ObjectiveEndShare` per bucket with the concept's 0.70 / ≥ 20 rounds.
  - (b) Keep Round F7 as a lenient floor, and state in it that the concept's bucketed 0.70 is the pillar gate.
  - (c) Revisit the 12 min target itself (adopt ~9 min, or lengthen the LSC chain toward 220 m per OC 174).
- **Creative call:** (c) yes (target round length); (a), (b) no (they align with decisions already made). **Always-ask:** no.

##### D3-W6. "Exile" names two different events in player-facing language
- **GDDs:** HUD lines 30, 49, 67 and ASN Player Fantasy (the plate for killing a rival reads "Target exiled"); PEN Player Fantasy line 27 ("then the word: **Exiled**" for the third wrong target), against PEN anchor 1 and the confirmed removal card ("Informant" / "Too many wrong targets" / "Eliminated for the round"); PEN fantasy "voice and text to the team continue throughout" against PNG-R11 (no in-game text).
- **Reasoning:**
  - The attacker's best moment ("Target exiled") and the attacker's worst penalty ("Exiled") share one word.
  - The blade victim's own card says "Eliminated for the round", not "exiled". So "exile" means "struck by a rival" on one screen and "removed for wrong targets" on another.
  - The PEN fantasy's headline word is no longer what the card shows. This blurs the P3 reading of cost: was I punished, or was I beaten?
- **Options:**
  - (a) Reserve "exile" for the penalty, and rename the rival-kill plate (for example "Rival struck" or "Rival removed"; text owned by HUD and ASN, approved by the writer or CD).
  - (b) Reserve "exile" for the rival plate, and update the PEN fantasy to the card wording.
  - (c) Keep both, and accept the overlap as diegetic ("the city exiles").
- **Creative call:** yes (player-facing language). **Always-ask:** no (text only).

##### Status of prior findings (2026-10-24)
- **D-W1 (brace pays only against inattentive attackers / late timing race): UNCHANGED.** The stop floor (D2) does not alter it: brace entry is a stance clip from any walk speed (SCL 67), not a masked ordinary stop. Deferred to ADR-0023 telemetry.
- **D-W2 (rushing at a run unmodelled): UNCHANGED.** `NpcCount` 50 adds NPC runners (T1 0.148/min per NPC), so a short run is slightly better covered. Long runs are still proof. Deferred.
- **D-W3 (camping the final candidates is the trailing team's best answer): SEVERITY UNCHANGED, inputs moved.** Worsened by D3-W3 (camping gaze masked). Partly offset by informant scouting with pings (a non-camping lever, available only to a team that has informants). `RivalPrelimCount` ON confirms its role: mean delay ~112 s, latest 195 s. Deferred.
- **D-W4 (small-team playtests under-measure P4): PARTLY RESOLVED.** The concept now buckets by team size (line 318). Not propagated to Round F7 (see D3-W5).

#### GDD flag table

| GDD | Finding IDs | Priority |
|---|---|---|
| informant-pings-team-comms | D3-W1 (chain cadence, OQ7 threshold), D3-W2 (ping while concealed), D3-W4 (P2 exception cross-reference) | High |
| shared-character-locomotion | D3-W3 (R8.9 idle gaze masks deliberate watching; knob 529 rationale) | High |
| crowd-npc-behaviour | D3-W3 (T13/T14 carry the watching tell), D3-W4 (no Tell Matrix row for pings) | Medium |
| concealment-bushes | D3-W2 ("no reward for staying" vs a ping spotter post; AC-CON-24 split) | Medium |
| round-match-framework | D3-W5 (F7 threshold 0.5 vs concept 0.70; no team-size bucket; stall 1440 s vs ~9 min model) | Medium |
| objectives-chain | D3-W3 (camping cheaper, D-W3), D3-W5 (F1 9 min vs 12 min target) | Medium |
| npc-kill-penalty-informants | D3-W1 (informant downtime is the only price of shadowing), D3-W6 (fantasy "Exiled" vs card; "voice and text") | Low |
| hud | D3-W6 ("Target exiled" plate wording) | Low |
| assassination-defence-reversal | D3-W6 (plate wording in Player Fantasy); D-W1 unchanged | Low |
| game-concept | D3-W1 (decision 4 "no sticky tracking marker" vs chain cadence), D3-W5 (12 min target vs 9 min model) | Low |
| camera-system | D3-W3 (P2 line "watching costs a body in the open" now position-only) | Low |

## Appendix H — Phase 4 scenario walkthroughs (detail, sub-agent output verbatim)

### /review-all-gdds full - Phase 4: Cross-System Scenario Walkthrough (2026-10-25)

Status: COMPLETE. Report only; no GDD, registry or index was edited. Evidence base: grep + ranged reads of Roster, Session, Round, Penalty, ASN, IIF, IVP (not re-read), HUD, TMK, Pings, SCL, Camera, Crowd, CON, OC, CPA, plus `design/ux/pause-menu.md` (used only to resolve the D14/D15 pause-menu behaviour that the GDDs reference but do not specify).

Verified still holding (not re-raised): two-step round end (R9 latch step, then round-end step; Rejected_Latched with held NotNow replies; holds frozen then cancelled; Crowd Halt + stop-and-blend; staged MatchScore); Absent on a removal cause for a Disconnected participant (PEN-R8 line 94); bush-killed victim keeps IsConcealed until body removal (ASN-R10 line 79, ASN-R17); late loader / RevealPawnWaitMaxSec (Round P7 line 80); W-NKP-5 formula; TM-R5 / HUD ignore a RoundEnded record arriving before Phase = Ended (Round R9 line 99, HUD States line 135, TM-R5 line 81).

#### Scenarios walked

- S1 Informant sighting to kill (ping ring -> ally strike on a rival in brace cooldown -> plate -> rival becomes informant)
- S2 Pause menu during contact (D14/D15): braced, tending, hidden, walk-slower, strike in the idle-gaze window, round end with menu open
- S3 Disconnect / kick / failed supersede-rejoin (D9)
- S4 Killed informant (D10)
- S5 Final-court finish (objective win with a rival strike pending; two-step round end; Ended -> Reset -> Preparing with world-ready stage)

No scenario was swapped.

---

#### S1 - Informant sighting to kill

**Trigger.** An informant presses `IA_Ping` aiming at ground near a suspect. An ally reads the ring and strikes a rival standing in it. The rival is in the *cooldown* of Prepare, not in the brace window (cooldown only blocks a new Prepare, ASN-R1 line 42; the brace is sampled at the slot, ASN-R5 line 53), so the strike is a Kill.

**Activation order.**
1. Pings: PNG-R3 (line 55) client computes the point from `GetAimRay()` world query only; PNG-R4 (line 57) reliable `ServerTeamPing`, not a pawn command; PNG-R5 (line 59-64) server validates (phase Active and not latched; State Active|Informant with a bound pawn; range from eye pivot; token); PNG-R6 (line 68) writes one slot per sender in `FTeamPingView` on every teammate controller. Data is point + sender handle + PingSeq + ServerTime only: no pawn id, stage, court, clue.
2. HUD/TMK: PNG-R7 (line 70) shows the ring in HUD L0 (HUD-R4 line 54, ZOrder 10) beneath markers; off-screen it clamps to the safe rectangle as a 32 px broken ring with no label (TM-R9 / F3 reuse). Valid input for each consumer.
3. Round queue: Attack slot. ASN-R8 line 61-64 resolves V; ASN-R10 line 77 Kill: `RequestTransition(V, Informant, Assassinated)` then `OnPawnKilled(..., Strike)`. Roster flushes events; Round runs the elimination check; only if not latched calls `OnInformantAwaitingBody(P, Assassinated, slot)` (Round R18 note, line 164).
4. Penalty: no strike, no tally (PEN-R2 line 54-57: Cause Strike but VictimKind Player). F2 (PEN line 228): `T_due = max(t_lost+15, DeathServerTime+DeathFallSec+NoPawnFadeSec, now)`; `SetOwnerRespawnDue` writes T_due to the victim's owner part (Roster line 107, 111).
5. Attacker: `FStrikeResult{RivalEliminated}` shown at the local contact frame (ASN-R19 line 103-108; ASN-R23).
6. Contact (death write, ASN-R21): fall, retire; victim team TMK marker exits half-faded at `RetireAtServerTime` (TM-R6 line 85); allies' removal line "eliminated for the round" at max(State arrival, OnAllyMarkerRetired) (HUD-R10 line 84); the victim sees the removal card "Informant / Eliminated for the round" (D11).
7. The victim's live ring (if it pinged as Active) stays to expiry (PNG-R10 line 82, Pings Edge Cases line 195); the victim cannot ping while awaiting a body (PNG-R2 line 51, Edge line 197). After PEN-R9 takeover (lines 96-106) it pings under identical rules; its pawn is a crowd pawn taken from the team's respawn region, invisible on the wire (PEN-R9 line 106).

**Data flow.** Every output feeds a valid input: the ring carries no identity, so the strike and the ring never share a datum; `RetireAtServerTime` = `ContactServerTime` is the same clock ASN-R21 schedules from; `T_due` is Owner-only.

**Player experience.** Ally: ring appears with a UI tick, ally walks to the area, reads people, strikes, plate at contact. Rival team: marker exit and feed line at the blow; a ring that sender placed earlier remains on their screens for up to 8 s with the (now removed) sender's name (S-I1). No audio, no cue to the rival's team that a ring pointed at their position (PNG-R9).

**Failure modes checked.** (a) Ping accepted between strike slot and contact: defined (Pings line 196). (b) Strike that decides the round: latch at the slot, pings dropped from the latch (PNG-R5 step 1), rings visible until the round-end step clears them (PNG-R10), defined. (c) Double reward: ping grants nothing; attacker gets RivalEliminated only; no tally. (d) Informant respawn pick racing the ring: unrelated state. No blocker in S1.

---

#### S2 - Pause menu during contact (D14/D15)

**Trigger.** Escape opens the in-session menu (ZOrder 100, HUD-R4 line 60-61; spec `design/ux/pause-menu.md`). The world keeps running.

**Activation order.**
1. `PushMenu` calls `EndAllOutstanding(MenuOpen)` synchronously (pause-menu.md line 188; IIF R4 line 68 and edge line 445): an End for every outstanding Begin. Input mode flips to `IMC_Menu` (no `IA_*`), so zero Move, zero Look, and `IA_Ping`, `IA_WalkSlower` are unreachable (IIF R13 line 187).
2. **Bracing.** The End executes after `ReleaseDebounceSec` (0.10 s). A pending End inside the debounce still counts as braced (ASN-R5 line 53), so the brace lasts 0.10 s past the menu key, then the planted stance ends; the 60 s cooldown was burned at execution (IIF R8 line 153). The stance exit is a normal brace End: no tell beyond what a released brace already shows.
3. **Tending.** End -> early release resets progress to 0 (IIF R8 line 152). The pawn stays at its stand point (blocks at most one of four faces, OC edge line 278).
4. **Hidden in a bush.** Hide Enter/Exit is an atomic transition, not a Begin/End (IIF R9 line 157), so the pawn stays concealed; the idle gaze layer runs on the server for the concealed pawn (SCL R8.9 line 107) but its pawn is culled for everyone, so no tell. Prepare and Interact were already `NotNow` inside a bush (IIF R9).
5. **Walk-slower.** `IA_WalkSlower` only scales the Move vector client-side (IIF R15a line 214); with zero Move the pawn brakes by the baseline, floored at `StopMinSec` 0.5 s (SCL line 212, 226). No residual state.
6. **Silence clock / hand-over.** The client keeps its normal command stream (pause-menu.md line 190), so Session's silence clock never reaches `SessionSoftTimeoutSec` (Session rules 18-19, line 120-121) and Round never hands over (D14: no AI hand-over, SCL R8.9 line 107).
7. **Idle-gaze window.** After `IdleGazeDelaySec` (3 s) of unchanged control rotation the server adds sweeps of 30-60 deg every 3-7 s to `HeadLook`, identically for players, menu-open players and NPCs (SCL R8.9; CNB G1 line 73). A strike inside the 3 s window meets a pawn with a still head, the same as any NPC mid-pause (Session rule 25f line 156 requires common pauses of at least Soft + 1 s).
8. **Enemy strike on the menu-open pawn.** Ordinary ASN-R10 path: not braced after the debounce, so Kill; victim learns at contact (ASN-R6). The menu auto-closes when the removal card or end banner presents (pause-menu.md line 75, PM-R7 line 329: at presented time only), so it adds no pre-contact signal.
9. **Round ends while open.** Latch step is server-only; round-end step sets Ended, force-updates records, halts the crowd and stops human pawns by the server path (Round R9 line 99 (f)). The menu is system-dismissed when the banner presents (pause-menu.md line 75), and the banner may wait up to `HudBannerMaxDelaySec` for a pending own plate or fall (HUD-R14 line 109). Held replies and cancelled holds need nothing from the menu.

**Data flow.** All steps consume existing contracts; the pause menu adds no message (PM-R1).

**Failure modes.** (a) The GDD set does not state the menu's input effect on Move in one place: SCL says "keeps its position and last movement state" while IIF R3 and pause-menu.md PM-R2 say zero Move and baseline braking (S-I2). (b) D15 ("overlays needing buttons push a Session-owned modal at ZOrder 100; layers below 100 are display-only") appears in no GDD: HUD-R4 line 61 makes L4 a `HitTestInvisible` plain overlay and Session (rules 24-25, line 513-515) names no modal (S-I3). (c) Long stand over 20 s stays a player-chosen risk, accepted (SCL R8.9). No blocker.

---

#### S3 - Disconnect / kick / failed supersede-rejoin (D9)

##### S3a - Connection expires, host kicks
**Trigger.** An Active player is Disconnected (AI-driven pawn, grace running). The host kicks (host debug, Session line 216).
**Order.** Session: `Disconnected(Kicked)` -> `Expired` same tick, `OnRejoinWindowExpired` same tick, key added to the refused list (Session line 57, 216). Roster: `Expired` projects Disconnected, generation not bumped (Roster R11 line 224). Round: `OnParticipantDisconnected` takes R17.2 (line 148) with `Kicked = true`; the same-tick window expiry is ignored for a kicked participant (R17.4 line 151); grace expires at `G_eff`; `RequestTransition(Informant, Disconnect, gen)` is `Applied` for an Expired connection (Roster line 224, Round line 148); `ReleaseToCrowd`; R15 (c) `Void(KickedLastActive)` if it was the last Active (Round line 136). Penalty: Absent, no timer (PEN-R8 line 94); a later return is refused. HUD/TMK: `bReconnecting` is true while Disconnected, phase Active and before `GraceExpiryMs` (Roster line 108), so allies get "X - reconnecting" (HUD-R10a line 86) and the label suffix (TM-R11 line 102), then at expiry the removal line (State change, no "back" line). Pings: PNG-R2 requires connected, so no new ping; the last ring expires normally (PNG-R10, Pings line 210).
**Experience.** Allies are told "reconnecting" about a player the host removed and who cannot return (S-I6). Opponents see an ordinary walking pawn then an NPC (Pillar 1 respected).

##### S3b - Failed supersede-rejoin
**Trigger.** A second connection with the same key supersedes a live (Connected) one and then fails to finish loading.
**Order.** Session rule 15/21: old connection closed, record -> `Rejoining`, deadline `t_admit + TravelLoadTimeoutSec` (default 60 s, Session line 356); `OnParticipantRejoined(Superseded)` only on success; **no** `OnParticipantDisconnected` at the start (Session AC 17 line 612, edge line 421). Roster: `Rejoining` projects Disconnected, so the generation bumps and `DisconnectedAt` is set at the supersede (Roster line 220). On failure: `Disconnected` with a fresh `DisconnectedAt`, `OnParticipantDisconnected` raised, generation bumped (Session line 131); Round takes R17.2 with a new grace (Round line 150).
**Data flow problem.** Round R17.3 sub-bullet (line 150) and Roster line 220 both speak of "any grace timer started at the earlier supersede", but the only event that starts Round's grace is `OnParticipantDisconnected`, which Session says does not fire at the supersede. See S-B1 and S-B2.
**Pings.** A superseding or rejoining client cannot ping (not Connected; Rejoining projects Disconnected). Rings it placed expire normally; its token bucket is keyed by `ParticipantId` and survives (PNG-R10, Pings line 202).

**Result.** Two blockers (S-B1 messaging, S-B2 grace/driver contradiction), one info (S-I6).

---

#### S4 - Killed informant (D10)

**Trigger.** An attacker kills an embodied informant during the informant's live ring.
**Order.**
1. Attack slot: ASN-R8 candidate is `ResolveVictimKind = Informant` (a legal target, IIF edge line 456). ASN-R10 Kill: no `RequestTransition` (VictimKind is not Player), `OnPawnKilled(..., Informant, Strike)` (line 77). Roster `NoOp_AlreadyInState` for any Informant -> Informant request, no evaluation (Round AC line 721).
2. Penalty: no strike (PEN-R2 requires VictimKind Npc); tally +1 (PEN-R3 line 61); `FStrikeResult{WrongTarget}` byte-identical to an NPC kill (ASN-R19 line 108). Penalty calls `RetireBinding(VictimPawn, RetireAtServerTime = ContactServerTime, Reason = InformantKilled)` inline (PEN-R7 line 86; Roster line 181); the Roster writes the reason only to its ServerOnly log. A second strike resolves AlreadyResolved (PEN edge line 308).
3. Penalty schedules the respawn itself: States table row 172 "Embodied -> Awaiting ... schedule, t_lost = slot"; F2 floor `DeathServerTime + DeathFallSec + NoPawnFadeSec` = t_lost + 2.1 s, `T_due = t_lost + 15 s`; `SetOwnerRespawnDue`. (Round R18 line 164 only names Assassinated/ReversedAgainst/StrikeLimit transitions as triggers: S-I4.)
4. Contact: death write; Camera `ViewRelease(InformantKilled)` keyed on the own pawn's death state, fade over `NoPawnFadeSec` from `DeathServerTime + DeathFallSec` (Camera line 234, CS-R12.1); HUD `InformantBodied` shows no card and no exile cue, moves to `InformantAwaiting` when the fade completes (HUD line 134); countdown "Returning in {s}" from T_due (HUD-R15 line 115, F7). Roster published nothing at the slot except the Owner `T_due`, which HUD does not present until the fade ends.
5. Allies: TMK marker retires (TM-R6: PawnRef null with no feed entry), exits half-faded at `RetireAtServerTime` (line 85); HUD-R10 fires only on a State change, so there is **no** removal line (correct: no State change) and no ally learns more than the marker leaving.
6. Crowd/CPA: CNB-R9.2 adds one vacancy at the scheduled death-write time for any VictimKind and cancels one upcoming exit (line 94); CNB-R9.3 states the informant victim is identical to an NPC or player victim. CPA CP-12 line 59: the informant's card goes `NpcVacant`; `DrawReplacement()` returns a card uniform over vacant cards when the replacement emerges (10-60 s). At T_due Penalty takes an unseen crowd pawn in the team region (PEN-R9), `TakeNpcCard` moves that pawn's card NpcLive -> BoundLive; the NPC pawn count is one lower until the kill's replacement emerges (CPA CP-13c line 60), self-correcting.
7. Pings: the ring stays to expiry (PNG-R10, Pings line 201); the dead informant cannot ping (no bound pawn after the slot, PNG-R5 step 2). A ping pressed between the kill slot and contact is dropped silently (S-I5).

**Data flow.** All hand-offs valid. The informant has no State change, so Roster/HUD/Camera key on `FDeathState` of the own pawn, which Camera and HUD both specify consistently (Camera line 234, HUD line 134, Penalty PEN-R16 line 148).
**Failure modes checked.** Same-drain double kill (defined, PEN edge 308); takeover candidate struck in its commit tick (PEN edge 309); EndRoundAll before respawn (Roster edge line 443, binding stays retired); feedback loop (kill -> vacancy -> exit cancelled -> replacement) bounded by the NPC-identical vacancy model. No blocker.

---

#### S5 - Final-court finish (objective win with a rival strike pending)

**Trigger.** A leading-team tender's `HoldTimer(Complete)` on the final fixture resolves at slot k; a rival's Attack on a different defender of the same team resolved at slot j < k (contact at +0.40 s, death write at `ContactServerTime - StrikeNetLeadSec`); the rival-signal release for an earlier preliminary is still pending.

**Order.**
1. Slot j (strike): Kill, defender -> Informant at the slot (ASN-R10), `OnInformantAwaitingBody` scheduled (round not latched), `RetireAtServerTime` carried; death write pending (ASN-R21).
2. Slot k: `HoldTimer(Complete)` re-validated (OC-10), Objectives applies the final in the same slot (IIF R8 line 129); Round `TryTerminate(Win)`; latch step. Because the deciding item is not a strike, the round-end step is the latch step (Round R9 line 99, TryTerminate line 138). Pending OC rival-signal releases are discarded at the latch (OC-12.4 line 84, edge line 272). A lower-priority timer or Attack after k returns `Rejected_Latched` (OC edge lines 257-260).
3. Round-end step (same slot): (a) Phase Ended + `FRoundResult` incl. `TeamSummary[2]` (EliminatedCount counts the defender because its State is Informant in the latch record), pings cleared (PNG-R10); (b) `EndRoundAll`; (c) MatchScore staged; (d) held replies; (e) `OnRoundEnded`, Penalty cancels its items; (f) Crowd Halt (CNB-R12 line 106) and stop-and-blend of every pawn, both through the SCL braking path where the D2 floor makes every stop at least `StopMinSec` 0.5 s (SCL line 212, 226); (g) holds cancel.
4. After the step: the defender's death write falls due and still plays "on a pawn that is already stopping" (Round R9 line 99, ASN-R21); Disabled starts a new baseline stop (a new stop latches its own v0, so D2 holds).
5. Clients: HUD enters Ended, hides every ring, fades persistent clusters over 600 ms, then banner + summary at the F10 show time (HUD-R14 line 109, States line 135); the victim viewer's banner waits for its own fall start, capped by `HudBannerMaxDelaySec`.
6. Ended -> Reset after `EndedMinDwellSec`/host command (R9 (h), R10 line 101) -> Lobby travel -> next `StartRound` -> Preparing: `OnRosterReset` clears pings and token buckets (PNG-R10), `RoundEpoch` change rebuilds HUD (HUD-R17), TMK markers go Absent with no hold (TM-R6 step 5). World-ready stage: after the acknowledgement and while Preparing each Connected client sends `ServerWorldReady(FAckEpoch)` once (Session line 87); Round waits for all Connected or `WorldReadyTimeoutSec` 10 s then sets `ActiveAtServerTime = now + 0.25 s` (Round P7 line 80). A late client is not disconnected; its loading screen releases at `ExpectedPawnCount` pawns or `RevealPawnWaitMaxSec` (verified earlier contract).

**Data flow.** Valid: the latch record is the single source for Result, TeamSummary and held replies; all three Roster/GameState/HUD arrival-order hazards are handled by the "RoundEnded before Ended is no change" rules.
**Failure modes.** (a) A takeover committed earlier in the same drain is possessed at the next frame start and was released from Crowd, so it is neither "AI-driven" nor "human-driven" at the round-end step (S-W1). (b) The pending defender removal: the TeamSummary already counts it eliminated, but the ally removal line and marker exit fall due after Ended and no rule says whether they show (S-W2). (c) The round-end step carries a burst (forced net update of every Roster record + GameState + ping part clear + stop of ~50 pawns) in one frame on the listen host (S-W3).

---

#### Blockers

**S-B1. HUD "back" line fires on a reconnecting-flag clear that is not a reclaim (S3; Roster, HUD, Session, Round).**
Step: any time `bReconnecting` goes true -> false while the ally's State is unchanged and the ally has not returned.
Failure mode: contradictory player messaging. HUD-R10a (hud.md:86): "when it changes `true -> false` while that ally's `State` is unchanged (the ally reclaimed its slot), one line '{Ally name} - back'". The Roster clears the flag by time, not by reclaim: roster:108 "false otherwise, so it clears at reclaim, at grace expiry, at `EndRoundAll`", with `GraceExpiryMs = max(DisconnectedAt, ActivePhaseStart) + G_eff` "which for an Informant is the same instant even though an informant has no grace of its own". Three paths print a false "back" while the ally is still gone: (a) an Informant ally who stays disconnected past `G_eff` (no State change at that instant); (b) an Active ally killed while disconnected (becomes Informant at the kill, Penalty Absent, roster edge line 224 / round:148), whose flag stays true until `G_eff` and then clears with State unchanged; (c) an ally in a supersede-from-live `Rejoining` that outlasts `G_eff` (30 s default) because the loading deadline is `TravelLoadTimeoutSec` 60 s (session:131, 356), after which the failure re-raises `bReconnecting` true ("reconnecting" again). AC-HUD-28 (hud.md:676) tests only the two Active-ally cases (reclaim, grace expiry).
Owner: Roster R6 (flag semantics: clear only on reclaim or add a distinct terminal value) with HUD-R10a (trigger "back" on a reclaim signal, not on any flag clear; extend AC-HUD-28 with the informant, killed-while-disconnected and supersede cases). TMK TM-R11 label suffix has the same dependency (teammate-markers.md:102).

**S-B2. Supersede-from-live: contradictory grace/driver contract (S3; Session, Round, Roster).**
Step: a live (Connected) participant is superseded and the new connection is `Rejoining`.
Failure mode: broken/undefined state transition. Session says no disconnect event fires (session:612 AC 17 "`OnParticipantDisconnected` does not fire"; edge session:421 "Only `OnParticipantRejoined(Superseded)` is raised, and the pawn is never handed away and back"), so Round R17.2 (round:148) never starts grace or the AI hand-over. Round and Roster assume the opposite: round:150 "any grace timer started at the earlier supersede holds a stale generation ... the pawn is already AI-driven, so no second takeover blend happens", roster:220 "so a grace timer started at the supersede is voided as stale". Consequences left undefined: (1) who drives the pawn during `Rejoining` when the supersede lands before the 3 s soft timeout (no `OnParticipantUnresponsive` has fired): a still, undriven pawn for up to 60 s, against Session rule 25f (f) (session:156 "never freezes in a way NPCs do not", blocking AC #46) and the 20 s `CrowdMaxIdleSec` bound (CNB-R10.2, crowd:99); (2) whether the participant converts at `G_eff` during the load (Round's reading) or only after a fresh grace from the failure (Session's reading), giving roughly 60 s + 30 s of absence before conversion; (3) at failure Round R17.2 says "if the pawn is not already AI-driven, take over at once" while R17.3 says it is already AI-driven.
Owner: Session rule 21 (state whether a supersede raises a disconnect event or a Round-visible "supersede started") and Round R17 (add the hand-over and grace trigger for supersede-from-live, or state there is none and bound the still pawn); Roster R11 line 220 to match.

#### Warnings

**S-W1. Informant takeover committed in the same drain before the latch escapes the round-end stop (S5; Penalty, Crowd, SCL, Round).** PEN-R9 step 5 (npc-kill-penalty-informants.md:101) commits the AI-to-player hand-over "executed at the next frame start"; CNB-R11 (crowd:104) releases the pawn from the Crowd ledger and CNB-R10.5 (crowd:102) holds the last Move until the first human command or `HandoverHoldSec` 0.8 s. A takeover at slot j < k with an objective-win latch at slot k (round-end step = latch slot, Round R9 line 99) leaves a pawn that is no longer Crowd-driven (Halt misses it) and not yet human-driven (R9 (f) names "every NPC and AI-driven pawn" and "human-driven pawns"). It keeps walking while the city halts, a one-pawn "who is the informant" tell at Ended; Round R9 and PEN-R10 (line 108) do not cover a commit already done. Owner: Round R9 (f) (stop every pawn including one with a pending possession change) and Shared Character R2a/R4.

**S-W2. Ally removal feed line and marker exit that fall due after the round-end step are unspecified (S5; HUD, TMK, Round, ASN).** Round R9 line 99 says an earlier strike's death write "still plays" after the step; TeamSummary counts that ally as eliminated (round:99 EliminatedCount from the latch record). HUD-R10 (hud.md:84) times the line at max(State arrival, `OnAllyMarkerRetired`); HUD enters Ended and fades persistent clusters over 600 ms (hud.md:135), edge lines 372-373 cover only the own card and RoundEnded records. TM-R8 keeps markers drawn in Ended while a pawn exists (teammate-markers.md:93) and TM-R6 keeps retiring. No rule says whether the line is shown over the banner, dropped, or whether the marker exit plays; the summary can read "eliminated 1" while no line ever showed. Owner: HUD-R10 / HUD States (Ended row) and TMK TM-R6.

**S-W3. Round-end step burst, no stated budget (S5; Round, Crowd, Pings).** One server step does: Phase Ended + `FRoundResult` forced net update; `EndRoundAll` with a forced net update on every Roster record; ping part clear on every controller; held-reply flush; `OnRoundEnded`; Crowd Halt and server-driven stop-and-blend for every NPC and human pawn; hold cancels (round:99 (a)-(h)). On the listen host that is a single-frame compounding spike (CNB-R15 budgets ~1.5 ms per frame for server AI + nav, crowd:112; no budget is stated for the step). Owner: Round R9 / its performance note (ADR-0002 §8.4) to state the budget and allow spreading the stop commands over the same frame set without breaking the "same step" contract.

#### Info

- **S-I1 (S1).** A ring from a sender who has since been removed keeps its name label for up to 8 s (Pings line 195, PNG-R7 line 70): the name resolves via `PublicHandle` while the HUD feed already says "eliminated for the round". Harmless, presentation only. Owner: Pings.
- **S-I2 (S2).** SCL R8.9 (shared-character-locomotion.md:107) says a menu-open pawn "keeps its position and last movement state", while IIF R3 (line 58), IIF edge line 445 and pause-menu.md PM-R2 (line 324) specify zero Move and baseline braking with the D2 floor. Same outcome intended; reword in SCL. Owner: SCL.
- **S-I3 (S2).** D15 (Session-owned modal at ZOrder 100; layers below 100 display-only) appears in no GDD; HUD-R4 (hud.md:61) calls L4 a `HitTestInvisible` plain overlay and Session names no modal contract. Owner: HUD-R4 and Session rules 24-25.
- **S-I4 (S4).** PEN-R8 (npc-kill-penalty-informants.md:92) says "For every cause Round triggers the scheduling by calling `OnInformantAwaitingBody`", but an informant kill has no Roster transition, so Round R18 (round:164) never fires it; Penalty's States table row 172 schedules itself. Ownership of the scheduling call for `InformantKilled` should be stated once. Owner: Penalty PEN-R8 / Round R18 note.
- **S-I5 (S4).** A ping pressed by an informant between its kill slot and contact is dropped at step 2 (no bound pawn after `RetireBinding`); Pings edge line 196 covers only the Active -> Informant window. No leak (silent drop), only an unacknowledged press. Owner: Pings Edge Cases.
- **S-I6 (S3).** A host-kicked Active player is shown to allies as "reconnecting" (HUD-R10a, TM-R11 suffix) for `G_eff` although the key is refused for the session (session:57, 216). Host-debug only; pillar-safe. Owner: Roster R6 / Session (kicked flag may suppress the bit if desired).

#### GDD flag table

| GDD | finding IDs | priority |
|---|---|---|
| participant-state-roster | S-B1, S-B2, S-I6 | High |
| hud | S-B1, S-W2, S-I3 | High |
| session-travel | S-B2, S-I3, S-I6 | High |
| round-match-framework | S-B2, S-W1, S-W2, S-W3, S-I4 | High |
| teammate-markers | S-B1, S-W2 | Medium |
| npc-kill-penalty-informants | S-W1, S-I4 | Medium |
| shared-character-locomotion | S-W1, S-I2 | Medium |
| crowd-npc-behaviour | S-W1 (CNB-R11 / R10.5 interaction) | Low |
| informant-pings-team-comms | S-I1, S-I5 | Low |
| assassination-defence-reversal | S-W2 (ASN-R21 write after round-end step) | Low |
| input-interaction-framework, camera-system, concealment-bushes, objectives-chain, character-pool-appearance, level-socket-contract, procedural-city-generator, information-visibility-policy, tuning-data-config | none (contracts verified consistent in these scenarios) | none |


---

## Fix pass 2026-10-25

Applied the same day by the main session plus five Sonnet fix agents, each owning a separate set of files. It follows rulings D21-D29 (`production/design-closeout/2026-10-25_rereview-followups.md`) and this report's fix text. Ground rules for the pass:
- No GDD Status line or systems-index Status cell was changed.
- Decisions are in `production/session-logs/decision-log.md` (tag "cross-review fix").
- Every changed GDD's review log has a "Cross-review fix 2026-10-25" entry.
- `design/registry/entities.yaml` validates with `yaml.safe_load`.
- Line numbers are after the edits. Abbreviations: ASN = assassination-defence-reversal, CON = concealment-bushes, PEN = npc-kill-penalty-informants, OC = objectives-chain, TMK = teammate-markers, PNG = informant-pings-team-comms, IDX = systems-index.md, REG = design/registry/entities.yaml.

**Tally.**
- Blocking: 7 of 7 resolved.
- Warnings: 48 resolved, 1 partial, 1 open, across the 50 itemised warnings (41 C-W, 6 D-W, 3 S-W). The verdict line above says 51.
- S-I1: resolved.

### Blocking
| ID | Status | Where | Note |
|---|---|---|---|
| C-B1 | RESOLVED | ASN:595 (AC-ASN-35), ASN-R23; hud.md:365 (F10), AC-HUD-17 | D21. The AC asserts no client shows the round end before the death write (`DeathServerTime − StrikeNetLeadSec`). Bystanders may show it up to 0.15 s before `DeathServerTime`. |
| C-B2 | RESOLVED | shared-character-locomotion.md:68 | R5.3 now uses the D2 floor `d_eff = min(d, v / StopMinSec)`: every stop lasts ≥ 0.5 s, and OQ15 is closed. |
| C-B3 | RESOLVED | level-socket-contract.md:220 | "≥ `StartToFinalMinPathCm` (44 m, D4)". |
| C-B4 | RESOLVED | procedural-city-generator.md:80 | Now `ClueMatchMaxCount`. |
| C-B5 | RESOLVED | round-match-framework.md:105, :110, :555, AC 45 :724; input-interaction-framework.md:280, :288, :783 | D22: `DueEpsilonMs` = 1 ms, due when `DueServerTime <= NowServerTime + 1 ms`. IIF cites Round, and the 3.0 s hold still stamps on tick 180. ADR-0002 amendment added. |
| S-B1 | RESOLVED | participant-state-roster.md:108, AC-PSR-53; hud.md:86 (HUD-R10a), AC-HUD-28 :690; information-visibility-policy.md:81; TMK:100, AC-TMK-26 | D23: `bReconnecting` clears only on an event. The new Team field `ReconnectEndCause` {None, Rejoined, GraceExpired, Cleared} drives the HUD line: "back" for Rejoined only, "— disconnected" for GraceExpired (suppressed when an Active ally is converted, where "eliminated" shows instead), nothing for Cleared. ADR-0001 amendment added. |
| S-B2 | RESOLVED | session-travel.md:100, :131-136, :168, :203, rule 25f, AC 17/51/66; round-match-framework.md:148, :150 (R17.1-R17.3); participant-state-roster.md:220, :227, :464 | D24: a supersede raises `OnParticipantDisconnected(Superseded)` at that instant, and grace and the window run from it. The pawn is AI-held until the rejoiner possesses, then `OnParticipantRejoined(Superseded)` fires. A failed rejoin does not restart grace. ADR-0005 amendment added. |

### Consistency warnings
| ID | Status | Where | Note |
|---|---|---|---|
| C-W1 | RESOLVED | input-interaction-framework.md:497; session-travel.md:467; hud.md:423, :436; PNG:244 | Pings links are reciprocal. |
| C-W2 | RESOLVED | TMK:357; hud.md:436; IDX row 17 | TMK→HUD (`HudSafeMarginPct`). |
| C-W3 | RESOLVED | input-interaction-framework.md:481-484; round-match-framework.md:524-527; IDX rows 3, 6 | Camera Hard on both sides; the Crowd/SCL–Round peer contract and the IIF–Round cycle are labelled. |
| C-W4 | RESOLVED | information-visibility-policy.md:393-399; character-pool-appearance.md:317, :330; level-socket-contract.md:526; session-travel.md:454, :463; IDX rows 4, 5, 8 | Implementers label the Policy Hard; the Policy side is Soft. |
| C-W5 | RESOLVED | shared-character-locomotion.md:491 + Camera row; Crowd row (D2, D14); IDX row 9 | |
| C-W6 | RESOLVED | PEN:363; IDX row 14 | Concealment is a Hard upstream. |
| C-W7 | RESOLVED | IDX rows 3-12, 14-18, 25 (Depends On column only) | |
| C-W8 | RESOLVED | PEN:379; ASN:322; crowd-npc-behaviour.md:381; CON deps; Roster deps intro; procedural-city-generator.md:327; character-pool-appearance.md:328 | |
| C-W9 | PARTIAL | IVP:18, :202, :501, :505, :613; hud.md:616; tuning-data-config.md:433, :463; PCG:438; Round CR 21; ASN review log :61 | The stale text is fixed. **Left by rule:** the header Status lines (ASN:3 "re-verification still pending", PCG:3, round-match-framework.md:3, shared-character-locomotion.md:3, hud.md:3) and the sibling status cells in the Cross-References tables go to the main session. |
| C-W10 | RESOLVED | crowd-npc-behaviour.md:418, :436; TDC I-CNB-6; REG | `CrowdDecoyEarlyReleaseMaxSec` is registered. `ObjectiveInteractSec` safe range is now 2.9-4.5 (OC:327, TDC:90). |
| C-W11 | RESOLVED | camera-system.md:587, :576, :414; input-interaction-framework.md:547 | Reject with no clamp; blend hard max 0.28; lockout safe 0.25-0.5. |
| C-W12 | RESOLVED | shared-character-locomotion.md:524, :581; Crowd R3.1 | I-SCL-5 is `WalkSpeedScaleMinMult <= 0.8`; safe 0.75-0.80. |
| C-W13 | RESOLVED | REG `IdleGazeDelaySec` (ServerOnly / Use); tuning-data-config.md:110; shared-character-locomotion.md:531 | |
| C-W14 | RESOLVED | tuning-data-config.md:109 (LSC 80), :110 (SCL 53, I-SCL-1..16), :351 (Camera 49) | |
| C-W15 | RESOLVED | level-socket-contract.md:556-557 | Max safe range 2,700-3,600; Min safe range 2,000-2,025. |
| C-W16 | RESOLVED | REG `StartToFinalMinPathCm` (4,400 cm; hard 4,000-10,000; safe 4,000-4,400 on the generator) + 11 invariant-named keys + `StallFlagMinutes`; TDC registry note | About 38 LSC-only keys that no invariant names are still unregistered (follow-up). |
| C-W17 | RESOLVED | ASN:346; Camera eye-cone row :571; LSC:551-553, :601; session-travel.md:483; crowd-npc-behaviour.md (`CrowdMaxIdleSec`); PEN:391; shared-character-locomotion.md (`WalkSpeedCmPerSec`); CON (`BushEnterSwapSec`); hud.md F4 table | All nine are narrowed to the satisfiable part and mirrored in REG and TDC. |
| C-W18 | RESOLVED | OC:330-332; PEN:391-393; ASN:369; ASN `AttackAttemptLockoutSec` 1.5-2.0 | |
| C-W19 | RESOLVED | TDC I-IVP-1, I-PCG-7, I-SCL-16; level-socket-contract.md:594 (`ReachDetourMult` derived, I-LSC-4 retired); TMK:438 (discrete set); Round F5 `NpcCount` cites I-LSC-1 | |
| C-W20 | RESOLVED | OC:165, :173 | `v_run` 4.5 m/s; `T_rush` 41 / 54 / 73 s. |
| C-W21 | RESOLVED | CON:154 + Player Fantasy, R12, AC-CON-24(c), F2, OQ3 | r_N 0.20: E[O] 0.43, m0 0.349, cap 45 %. |
| C-W22 | RESOLVED | PNG:132, :59, :70, :269 | ε = `PingBucketPerSec × PingSkewTolSec` (0.1 s at every rate); edge-ring inputs stated. |
| C-W23 | RESOLVED | level-socket-contract.md:318-323 (D4 at ~9 min), :380 (D6 about 1,800 m²), :220-221; crowd-npc-behaviour.md:237 (`N_E` 12-16) | LSC recommends raising the `NpcCount` safe minimum from 35 to 45. That has not been applied; it is a follow-up. |
| C-W24 | **OPEN** | level-socket-contract.md:337, :587, LSC OQ15; procedural-city-generator.md:67, :531 | Recorded as unverified: 0.48 m margin against a 2 m jitter step. It closes only when the PCG OQ9 enumeration script runs. |
| C-W25 | RESOLVED | camera-system.md:148, :170, :286, :569; REG; TDC:111; ADR-0004, ADR-0015 | The mask-off threshold is raised to 25 cm (on at 35 cm), which amends D6, because the geometric minimum free length is about 21 cm. The F2 example now reads 21 cm. |
| C-W26 | RESOLVED | shared-character-locomotion.md:359 (AC 52); input-interaction-framework.md:306 (U owner); IVP F7 | |
| C-W27 | RESOLVED | round-match-framework.md:547 (W-RMF-5), F4 bounds = Session F2, `StartLeadSec` safe 0.25-0.5; IIF F6 (0.29 s from rest); level-socket-contract.md:351 (NPCs); shared-character-locomotion.md:363 (listen-host upload ≈ 280 KB/s) | |
| C-W28 | RESOLVED | ADR-0003:76, :345; ADR-0010:429; ADR-0018:81, :396; architecture.md:725 | 36 KB/s line, 31.1 KB/s worst case. |
| C-W29 | RESOLVED | PEN:92 | Penalty schedules the informant-kill respawn itself. |
| C-W30 | RESOLVED | session-travel.md:196 (kick → `Expired` row); round-match-framework.md:148; participant-state-roster.md:224 | A kicked participant cannot return; the flag clears silently. |
| C-W31 | RESOLVED | participant-state-roster.md:146, AC-PSR-57; ASN:333 | Optional `RetireAtServerTime` on `RequestTransition`. |
| C-W32 | RESOLVED | PNG:199, AC-PNG-18 | From the resolving slot onward the ping is dropped silently (no tone, which would tell the victim early). |
| C-W33 | RESOLVED | hud.md:43 (E01-E22), HUD-R2 ping rings; TMK:53; participant-state-roster.md:297-298 | |
| C-W34 | RESOLVED | session-travel.md:519-523, AC 62/67; hud.md:53 (HUD-R4), HUD-E20, AC-HUD-24 | D15 (ADR-0021 L4 split) cited. |
| C-W35 | RESOLVED | round-match-framework.md:170, AC 63; OC:250, :260, :395 | Logged as `TendCompleted(NoEffect, Latched = true)`. |
| C-W36 | RESOLVED | shared-character-locomotion.md:107 (R8.9); crowd-npc-behaviour.md:73 (G1); input-interaction-framework.md:449 | One trigger (control rotation unchanged); zero Move and Look while the menu is open. |
| C-W37 | RESOLVED | camera-system.md:64, :664, :734 (AC-CS-28); SCL R11.5 | |
| C-W38 | RESOLVED | shared-character-locomotion.md:52; level-socket-contract.md:759 | CPA CP-12(a) uniform draw. |
| C-W39 | RESOLVED | ASN:507, :520, :560, :599 | Now the `BushSpotPick` stream. |
| C-W40 | RESOLVED | REG:519; level-socket-contract.md:114 | Own anchor, strictly nearer; pointer to Penalty OQ-NKP-13. |
| C-W41 | RESOLVED | reviews/tuning-data-config-review-log.md (closeout entry); tuning-data-config.md:249 (290 cm) | |

### Design-theory warnings (D25-D29)
| ID | Status | Where | Note |
|---|---|---|---|
| D-W5 | RESOLVED | PNG:68 (PNG-R6), AC-PNG-16; TDC I-PNG-1 | D25: one live ring per sender. |
| D-W6 | RESOLVED | PNG:51, :55, :201, AC-PNG-17; hud.md:108, AC-HUD-31; CON:130, :283; IVP:82 | D26: a concealed sender is refused with an owner-only tone (`PingConcealedRefused`) plus the "Not now" plate; the server drops it with reason `Concealed`. |
| D-W7 | RESOLVED | shared-character-locomotion.md:107, :531; crowd-npc-behaviour.md:73, :130 (T13) | D27 accepted. T13 counts only holds made with live look input; idle-scan holds are excluded. Camping telemetry is recommended. |
| D-W8 | RESOLVED | crowd-npc-behaviour.md:135 (T18 Ping), AC-CNB-30; PNG:81; IVP:82 | D28. |
| D-W9 | RESOLVED | round-match-framework.md:390-403 (F7), :538 (`StallFlagMinutes` 18); OC:173 | D29: cites OC F1 (~9 min). The Pillar 4 gate is ≥ 0.70 per bucket B ≥ 4. |
| D-W10 | RESOLVED | ASN:25, :453, :566; hud.md (plate); PEN:27, :39, :584; art bible 7.3; accessibility-requirements; ux/hud.md; ux/interaction-patterns.md; game-concept.md:308 | D29: the rival-kill plate is now "Target eliminated", and "exile" is used only for the NPC-kill removal. Penalty text chat removed (voice only). |

### Scenario warnings and info
| ID | Status | Where | Note |
|---|---|---|---|
| S-W1 | RESOLVED | round-match-framework.md:99 (R9 f), AC 12b/64 | The round-end step also halts pawns in hand-over. |
| S-W2 | RESOLVED | hud.md:84, AC-HUD-30 :693; TMK:89, AC-TMK-34 | From `Ended` on, pending ally lines are dropped and `Held`/`Exiting` markers complete instantly. |
| S-W3 | RESOLVED | round-match-framework.md:99, AC 65; ADR-0002, ADR-0022 amendments | Budget: ≤ 2.0 ms added host game-thread time and a burst of about 3.1 KB at 9 clients (the ping-part and PlayerState figures are estimates). Spike verification is pending. |
| S-I1 | RESOLVED | PNG:335 (AC-PNG-19); hud.md:99 | The ring stays until expiry; the name label drops at the sender's retirement. |

### Residual follow-ups (not review items)
- **I-SCL-15 boot check.** At `WalkSpeedScaleMinMult` 0.75, the bottom of its safe range, `T_stop,max` = 0.8 s, which needs `MaxClientForcedUpdateDuration` ≥ 1.30 s. The pinned value is 1.25 (ADR-0010 config). Either narrow `WalkSpeedScaleMinMult` safe to 0.80 or raise the pinned value.
- **`NpcCount` safe minimum.** LSC recommends 35 → 45.
- **LSC registry gap.** About 38 LSC-only keys are still not in REG.
- **"Target eliminated" width.** It is 17 characters against 13; the HUD plate width must be re-checked in the UI story.
- **Hand-over Halt.** Crowd and SCL should confirm that Halt on a hand-over pawn holds under either controller (Round S-W1).
- **Line endings.** `level-socket-contract.md` may have been rewritten with CRLF line endings, so expect diff noise.
- **Statuses.** Header Status lines and status cells are left to the main session (C-W9).

Recommended next step: `/review-all-gdds since-last-review`.
