# Cross-GDD Review Report (2026-10-25b)

Date: 2026-10-25 (second review of the day)
Skill: `/review-all-gdds since-last-review` (autonomous run; rigor full, so all 8 sections apply)
Prior review: `design/gdd/gdd-cross-review-2026-10-25.md` (FAIL, 7 blocking; fix pass appended at its end)
GDDs Reviewed: 19 of 19 present. This was a re-verification of the fix pass, not a fresh full read. See "Scope".
Not read: none. Every GDD was read in targeted ranges.
Focus: since-last-review. Checks run: fix verification; consistency (2a-2f) on the edited regions; design theory (3c, 3f) on rulings D25-D29 only; scenarios on the D23/D24/D25/D26 paths.
Engine: Unreal Engine 5.8 (installed 5.8.4), C++

**File name.** This report is named `...-2026-10-25b.md`, not `...-2026-10-25-b.md`. `review-scope.sh` picks the prior review with `sort | tail -1`. Under `sort`, `-b` falls before `.md`, so the next run would have chosen the old report as its baseline. `25b` falls after it.

### Scope

- **The script found nothing to review.** `review-scope.sh` returned `CHANGED: (none)`:
  - The project is not a git repo, so the script falls back to file modification times.
  - The fix pass edited the GDDs first and appended its table to the prior report afterwards, so every GDD looks older than the report.
- **What was reviewed instead** (logged as a decision): the fix pass touched 18 of the 19 GDDs, plus ADRs, the art bible, the registry and the systems index. This run reviewed all of those regions:
  - every Blocking fix, checked by the main session against the GDD text;
  - every warning fix, checked by three Sonnet sweeps;
  - a stale-remnant sweep for rulings D21-D29 over all GDDs, ADRs and UX specs;
  - scenarios re-walked: S3' (supersede-from-live: rejoin within grace, rejoin after `G_eff`, pawn killed during the load), S6 (disconnected ally killed and still away at grace expiry), S7 (concealed ping, then two pings in a row) and S8 (kick in the same frame as grace expiry).
- **Method.** The GDDs total about 2.5 MB, so the sub-agents got briefs with `file:line` pointers instead of pasted sections, as in the prior run (logged). The three sub-agents (`game-designer`, Sonnet) each hit the turn limit once and were resumed. Their partial gaps are listed under Coverage.

### Coverage

- **Blocking fixes:** all 7 were read at their cited lines by the main session.
- **Warnings checked:**
  - Fully: C-W1-C-W16 and C-W21-C-W41.
  - Spot-checked: C-W17 (all 9 narrowed ranges against the registry), C-W18 (the four invariant rows are present; where each bound sits inside the owner range was not recomputed), C-W19 and C-W20.
  - D-W5-D-W10, S-W1-S-W3 and S-I1: checked.
- **Registry:** `entities.yaml` was parsed with `yaml.safe_load` before and after this pass's edits (now 290 constants, no duplicates). Every GDD's Tuning Knobs table was compared to the registry by script.
- **Not done:**
  - No full re-run of the 3a-3g design-theory checklist. The fix pass changed no progression, economy or pillar text beyond D25-D29.
  - Not swept for D23/D24 remnants: the UX specs other than `ux/hud.md` and `ux/pause-menu.md`.

---

### Verification of the 7 prior Blocking items

| ID | Verdict | Evidence |
|---|---|---|
| C-B1 | HOLDS | AC-ASN-35 (`assassination-defence-reversal.md:595`) now asserts "not before the death write (`DeathServerTime − StrikeNetLeadSec`)" and allows bystanders up to 0.15 s before `DeathServerTime`. ASN-R23 "Everyone, when the outcome ends the round" (`:132`) and HUD F10 edge case (`hud.md:365`) state the same bound. No "never before `DeathServerTime`" text remains. |
| C-B2 | HOLDS | SCL R5.3 (`shared-character-locomotion.md:68`) uses `d_eff = min(d, v / StopMinSec)`, says every stop lasts at least `StopMinSec`, and marks OQ15 closed. It agrees with F2 (`:226`) and AC-SCL-50. |
| C-B3 | HOLDS | `level-socket-contract.md:220` says "≥ `StartToFinalMinPathCm` (44 m, decision D4)" and explicitly retires the 60 m rule. |
| C-B4 | HOLDS | `procedural-city-generator.md:80` uses `ClueMatchMaxCount`. |
| C-B5 | HOLDS, with a citation fixed | Round R11/R12 (`round-match-framework.md:105`, `:110`), the constants list (`:555`) and AC 45 (`:724`) define `DueEpsilonMs` = 1 ms. IIF F1 (`input-interaction-framework.md:280`, `:288`, `:783`) cites it. IIF cited "Round AC 711", which is the old report's line number, not an AC; it is corrected to AC 45 (fixed). The tick arithmetic holds: 2.9833 + 0.001 < 3.0, so the hold stamps on tick 180. |
| S-B1 | HOLDS on the Roster side; the HUD side contradicted it (N-B1, now fixed) | Roster R6 (`participant-state-roster.md:108`) and AC-PSR-53 clear `bReconnecting` only on an event. HUD-R10a and AC-HUD-28 still promised a "— disconnected" line that the Roster can never trigger. See N-B1. |
| S-B2 | HOLDS, with an ordering gap fixed | Session rule 15 (`session-travel.md:100`), the rule-21 origin rows (`:131`-`:135`), the transition row (`:203`), Round R17.2/R17.3 (`round-match-framework.md:148`, `:150`) and Roster R11 (`participant-state-roster.md:220`, `:227`, `:464`) agree on D24. Rule 15 raised the event before the Roster notify, so Round could capture a stale generation; fixed (S3' below). |

---

### Consistency Issues

#### Blocking (must resolve before architecture begins)

None open. Three were found and fixed in this pass:

🔴→✅ **N-B1: the HUD "— disconnected" line could never fire, and two HUD ACs failed against a correct Roster** (introduced by the S-B1 fix).
- **The HUD side.** HUD-R10a (`hud.md:89`), the edge case "If an ally stays gone past the grace" (`:383`) and AC-HUD-28 (c)/(d) (`:690`) printed "{Ally} — disconnected" at a `GraceExpired` clear for an informant ally.
- **The Roster side** (owner of the flag):
  - R6 (`participant-state-roster.md:108`) writes `GraceExpired` only with the applied `Active → Informant` conversion: "an `Informant` has no grace of its own, so this event never applies to one".
  - AC-PSR-53 (b) and (c) require no update at grace expiry for an informant ally, or for an ally killed while disconnected.
- **Consequence.** `GraceExpired` therefore always arrives with the conversion, where HUD-R10a already suppressed the line. The line was dead text, and AC-HUD-28 (c)/(d) contradicted AC-PSR-53 (b)/(c).
- **Fix applied** (logged). The HUD follows the owner and the line type is retired.
  - Changed: HUD-R10a `GraceExpired` bullet, the `:383` edge case, AC-HUD-28 (b)-(d), the Roster R6 HUD sentence and the `:298` HUD row, AC-PSR-53 (e), Round AC 66, IVP `:81`, and Teammate Markers `:321`/`:322`.
  - **Rejected alternative:** a Roster clear for informant allies at `G_eff`. That would be a timer-only clear, which ruling D23 forbids.
  - **Behaviour now.** An informant ally who stays away keeps the "· reconnecting" label suffix until it rejoins ("back") or the round ends.
  - **Revisit if** the team needs a "this ally is not coming back" message for informants. That would need a design ruling, not a text fix.

🔴→✅ **N-B2 (outside GDD scope): the ADR-0005 body still stated the pre-D24 supersede rule.**
- `adr-0005-session-seamless-travel-state-stores.md:185` said "no `OnParticipantDisconnected`", while the ADR's own amendment (`:428`) and Session rule 15 say the opposite.
- Fixed: the body now states D24.

🔴→✅ **N-B3 (outside GDD scope): the control manifest defined `bReconnecting` as true "inside `DisconnectGraceSec`", a timer-bounded rule that D23 removed.**
- `docs/architecture/control-manifest.md:215`.
- Fixed: the flag is now event-cleared.

#### Warnings (should resolve, but won't block)

⚠️ **N-W1: The registry is still missing about 105 owner-table rows (about 130 names) outside LSC.**
- **Where:**
  - By count: Camera 23 rows, Crowd 22, Generator 22, SCL 32, Concealment 4, TDC 1 (`BlindGuessWarnPct`, a schema constant).
  - Six IVP test-threshold rows (`information-visibility-policy.md:439-444`) use a non-CR-15 column layout.
  - Four keys are named by registered invariants: `WalkTurnRateDegPerSec` (I-SCL-6), `RunExitHysteresisCmPerSec` (I-SCL-1), `TraversalHeightToleranceCm` (I-SCL-2) and `CityGenZoneCandidatesCount` (I-PCG-7). That is the C-W16 class, so **C-W16 is PARTIAL, not RESOLVED**.
- **Why it matters:** TDC AC-3b ("every owned key matches … a `design/registry/entities.yaml` constant") fails until these are mirrored.
- **Recommendation:** a scripted registry pass using the same method as follow-up (c) below. Some of the Crowd rows may be the CR-22 constants deliberately left unmirrored (OQ-14); check them first.
- TDC `:124` now records this gap honestly, replacing the earlier "registry mirror completed" wording.

⚠️ **N-W2: The D27 idle-gaze trigger has no input tolerance** (SCL R8.9 `shared-character-locomotion.md:107`, Crowd G1).
- **The rule:** the trigger is "control rotation unchanged" for `IdleGazeDelaySec`.
- **The risk:** a resting hand on a high-DPI mouse can produce micro-deltas. A human pawn would then never enter the shared idle layer while NPCs do, so a still head among scanning heads becomes a Pillar 1 tell.
- **Recommendation:** add an angular tolerance, applied the same way to every pawn (for example, total change under 0.5° over the delay counts as unchanged), and an AC that injects jittered input.

⚠️ **N-W3: The same-frame order of a host kick and a grace expiry is unstated** (Round R17.2 `:148`, R15 (c), Session `:168`).
- **Already fixed in this pass (logged):** a `Disconnected(Kicked)` for a participant with a live grace timer now sets `Kicked = true` on the existing record, with no second timer or takeover.
- **Still open:** if the expiry drains first, the kick finds an `Informant`, and R15 (c) `Void(KickedLastActive)` versus `ForfeitElimination` can flip with the order.
- **Fix:** state where the kick enters relative to the drain (Session's same-tick order line, or a Round rule).

⚠️ **N-W4: The concealed-ping refusal is under-specified at the bush-state edges** (Pings PNG-R3 versus AC-PNG-17 at `informant-pings-team-comms.md:333`).
- AC-PNG-17 (c) refuses "`Exiting` before the outward crossing" and has no `Emerging` case.
- The server gate `IsInBushOrTransition` (PNG-R5 step 2; Concealment `:242`) covers all of `Entering`, `Concealed`, `Exiting` and `Emerging`.
- **The gap:** a client gate that allows a press after the crossing would send RPCs that the server silently drops.
- **Fix:** state the client gate as `IsInBushOrTransition`, or define the post-crossing exception on both sides, and add an `Emerging` case to AC-PNG-17.

⚠️ **C-W9 (carried, PARTIAL by rule):** the header Status lines (ASN:3, PCG:3, Round:3, SCL:3, HUD:3) and the sibling status cells in the Cross-References tables are stale. Left to the main session; this run did not change them either.

⚠️ **C-W24 (carried, OPEN by design):** the final-court margin under D4 is 0.48 m against a 2 m jitter step. It closes only when the PCG OQ9 enumeration script runs (`level-socket-contract.md:337`, `procedural-city-generator.md:67`).

---

### Game Design Issues

#### Blocking

None. D25-D29 violate no anti-pillar.

#### Warnings

⚠️ **D-W5 (carried, residual): one live ring per sender (D25) limits clutter, not tracking.**
- `informant-pings-team-comms.md` PNG-R6 itself says the rule was "structural … now stated as a rule".
- One informant can still re-mark a walker every 2 s at `PingBucketPerSec` 0.5. Each new ring replaces the last, and five senders can cover one target.
- OQ7 (`:365`) still has no numeric failure threshold.
- **Recommendation (creative call; not decided here):**
  - Before the first playtest, set the OQ7 trigger, for example "chains of at least 3 rings within 4 m and 3 s in more than 25 % of informant-minutes".
  - Hold the review's no-refresh option (b) as the prepared lever.
- D26 does close the bush spotter post, and D27/D28 hold (see Scenarios).

---

### Cross-System Scenario Issues

Scenarios walked: 4, plus variants.
1. **S3' Supersede-from-live:**
   - The rejoin completes within grace: consistent, after the ordering fix below.
   - The rejoin outlasts `G_eff`: consistent (R18 informant path, `GraceExpired` clear, removal line only).
   - The old pawn is killed during the load: consistent. The participant becomes an Informant, Penalty Absent applies with no timer, the flag persists, and on rejoin R18 runs and "back" prints. The Session `:436` wording is fixed.
2. **S6 Disconnected ally killed, still away at grace expiry, rejoins as informant:** consistent after N-B1.
3. **S7 Concealed ping, then an ally pings, then two pings after the exit:** consistent. The refusal spends no token, and the second ping replaces the first ring. See N-W4 and N-I1.
4. **S8 Kick in the same frame as grace expiry:** see N-W3.

#### Blockers

None open.

🔴→✅ **S3' generation-capture race (Session, Round, Roster).**
- **The race.** Session rule 15 raised `OnParticipantDisconnected(Superseded)` and only then sent `NotifyConnectionChanged`. Round R17.2 captures `ExpectedConnectionGeneration` when the timer starts, so a synchronous handler could capture the pre-bump generation.
- **Consequence.** The grace conversion would then be `Rejected_StaleConnection`, and the pawn would convert only on the `WindowBeforeGrace` defect path. That is an immediate conversion, which is the timing tell R17.2 forbids.
- **Fix applied** (logged): rule 15 now sends the notify first, matching the order of every other disconnect (ADR-0005 §5: `ConnectionState` change, then the Roster notify, then events).
- Graded Warning by the sweep. It is listed here because, unfixed, it was a broken state transition.

#### Warnings

See N-W3 (S8).

#### Info

- ℹ️ **N-I1, S7 (PNG-R10 versus PNG-R5):**
  - PNG-R10 says a ping pressed just before entering a bush is accepted, but the server gate reads `IsInBushOrTransition` when the RPC arrives. A ping RPC landing after a queued Hide commits is dropped silently.
  - Fix: state the evaluation point.
- ℹ️ **N-I2, S3':**
  - An ally converted at `G_eff` shows no flag while still `Rejoining`, while an ally who was already an Informant shows the flag for its whole absence.
  - This follows from D23 and is harmless.
- ℹ️ **N-I3, S6:** "back" prints for an ally who was eliminated while away. This is accepted under D23.
- ℹ️ **N-I4:** the S-W3 amendment in ADR-0022 (`:368`) gives 2.0 ms but omits the 3.1 KB burst figure. ADR-0002 (`:460`) gives both.
- ℹ️ **N-I5:** `docs/architecture/architecture-review-2026-10-25.md:174` still says "pawns ≤ 30 KB/s". It is a dated snapshot, so it was left as is.

---

### Verification of prior warnings (summary)

- **C-W1 to C-W41:**
  - HOLDS: all except the four below.
  - C-W9: PARTIAL by rule.
  - C-W16: PARTIAL (N-W1).
  - C-W24: OPEN by design.
  - C-W25: PARTIAL. The ADR-0015 body (`:100`, `:127`) still said 20 cm; fixed to 25/35 cm, so it now holds.
- **D-W5 to D-W10:**
  - D-W6 to D-W9: HOLD.
  - D-W10: PARTIAL. The art bible `:1631`, `:2689` and `:2820` still read "Exiled" / "target exiled"; fixed to the D11 wording, so it now holds.
  - D-W5: residual (above).
- **S-W1 to S-W3, S-I1:** hold.
  - S-W1: nothing in Crowd or SCL contradicts R9 (f). The "Halt holds under either controller" confirmation is still the known follow-up.

---

### Follow-up adjudications (requested)

**(a) I-SCL-15 boot check: raise the pinned `MaxClientForcedUpdateDuration` from 1.25 s to 1.40 s.**
- **The problem.** At the 1.25 s pin, two values inside their safe ranges failed the boot check:
  - `WalkSpeedScaleMinMult` 0.75 needs 0.25 + 0.6 / 0.75 + 0.25 = 1.30 s.
  - `RunStopSec` 0.76-0.9 (safe 0.6-0.9) needs up to 0.25 + 0.9 + 0.25 = 1.40 s.
  - So narrowing `WalkSpeedScaleMinMult` alone would not have been consistent.
- **Why 1.40 s:** it is the smallest value that every combination of the safe ranges of the Round-latched keys (`WalkStopSec`, `WalkSpeedScaleMinMult`, `RunStopSec`) passes at `InputStarvationSec` 0.25.
- **`InputStarvationSec` is outside this.** It is pinned to `MAXCLIENTUPDATEINTERVAL` by the boot equality, so changing it is an `.ini` edit that re-pins both values (stated in TDC).
- **Cost:** nil. Zero-acceleration forced updates on a pawn already at rest change nothing visible. 1.40 s is well under the engine cap of 5 s and the 3 s soft timeout.
- **Applied in:**
  - `Config/DefaultGame.ini` (value and comment);
  - ADR-0010 `:183` and `:472` (and the stale "outside write scope" sentence);
  - TDC I-SCL-15 row (`:240`);
  - SCL `WalkStopSec` row (`:529`) and the I-SCL-15 text (`:581`).
- **Rejected:**
  - narrowing `WalkSpeedScaleMinMult` to 0.80 (`RunStopSec` still fails);
  - narrowing both safe ranges (costs tuning room, and would need three more mirror edits).

**(b) `NpcCount` safe minimum: adopted, 35 → 45. The I-LSC-1 reject floor stays at 35.**
- **Why:**
  - At 35-44, about half of unconstrained layouts fail V-ZONEFILL, and the Generator's zone disc clears the requirement by only 4-11 % (LSC D6, `:380`). At 45 the margin is 16-33 %.
  - The default of 50 is unaffected.
  - A safe range only warns, so a performance fallback below 45 (if spike R3 fails at 60 characters) still loads, with a warning and the R11.5 2,000 m² graybox rule.
- **Applied in:**
  - TDC canonical row (`:85`);
  - Round F5 (`:360`);
  - the formula-table variables in Concealment (`:148`), Crowd (`:187`, `:208`), IVP (`:309`) and Objectives (`:208`);
  - LSC (`:380` adoption note, `:556` note now citing the I-LSC-1 floor);
  - the registry formula note.
- `LayoutWalkableCellsMinCount` safe range 2,000-2,025 is unchanged. It remains valid, because it is derived from the reject floor.

**(c) LSC-only keys: registered now, not deferred.**
- **Count.** The real gap was 47 names in 35 rows, not "about 38". The prior count took one name per paired row. No other GDD reads any of them and no invariant names them (grep-verified).
- **Why now:** TDC AC-3b (CI doc lint) requires every owned key to match a registry constant, so deferring would leave a known lint failure.
- **How:** the mirror was generated by script from the LSC Tuning Knobs table (default, hard, safe; `ServerOnly` / `Round` per the LSC table preamble; † rows noted as provisional). The registry still loads with `yaml.safe_load`, with no duplicates.
- TDC `:124` is updated.
- I first logged a deferral, then reversed it once I saw AC-3b. Both entries are in the decision log.

---

### Fixes applied in this pass (all intra-line; no Status line or systems-index Status cell changed)

- **N-B1 alignment:**
  - `hud.md` `:86`, `:89`, `:383`, `:690`;
  - `participant-state-roster.md` `:108`, `:298`, `:819`;
  - `information-visibility-policy.md` `:80`, `:81`;
  - `teammate-markers.md` `:321`, `:322`;
  - `round-match-framework.md` `:787`.
- **Roster, Session and Round rules:**
  - `participant-state-roster.md:108`, `information-visibility-policy.md:81` and `hud.md:86`: `Cleared` causes worded as in Roster R6 (removal in Lobby, host kick, round end; a kick does not remove the record).
  - `session-travel.md:100` (notify before event, S3') and `:436` (Informant exception).
  - `round-match-framework.md:148` (kick with a live timer, S8) and `:632` (`StallFlagMinutes` registered).
- **ADRs and other docs:**
  - `docs/architecture/adr-0005-...:185`, `docs/architecture/control-manifest.md:215`, `docs/architecture/adr-0015-...:100`, `:127`;
  - `design/art/art-bible.md` `:1631`, `:2689`, `:2820`;
  - `game-concept.md:91` (T1-T18).
- **Smaller GDD text fixes:**
  - `input-interaction-framework.md` `:280`, `:783` (Round AC 45) and `:504`;
  - `concealment-bushes.md` `:130`, `:283` (`IsInBushOrTransition`);
  - `procedural-city-generator.md:26` and `level-socket-contract.md:29` (ADR-0014);
  - `informant-pings-team-comms.md:234`;
  - `level-socket-contract.md:284` (`ReachDetourMult` derived).
- **Systems index and review logs:**
  - `systems-index.md` row 11 Depends On: added "NPC-Kill Penalty & Informants (soft)" (Crowd `:370`, Penalty `:359`/`:379`).
  - A "Cross-review 2026-10-25b" entry in each changed GDD's review log.
- **Decisions (a)-(c):** as listed above.

---

### GDDs Flagged for Revision

| GDD | Reason | Type | Priority |
|-----|--------|------|----------|
| tuning-data-config.md + design/registry/entities.yaml | N-W1 registry gap (about 130 names, including 4 invariant-named keys) | Consistency | Warning |
| shared-character-locomotion.md, crowd-npc-behaviour.md | N-W2 idle-gaze input tolerance and AC | Design / Consistency | Warning |
| round-match-framework.md, session-travel.md | N-W3 kick-versus-expiry order | Scenario | Warning |
| informant-pings-team-comms.md | N-W4 client gate and `Emerging` case; D-W5 OQ7 threshold; N-I1 | Consistency / Design | Warning |
| level-socket-contract.md, procedural-city-generator.md | C-W24 (open until the OQ9 script runs) | Consistency | Warning |
| ASN, PCG, Round, SCL, HUD headers and status cells | C-W9 (left by rule) | Process | Warning |

The systems index was not changed, by instruction: no Status cells.

---

### Verdict: CONCERNS

- **Blocking: 0 open.** Three were found and fixed in this pass (N-B1 in the GDDs; N-B2 and N-B3 in ADR and manifest text). The S3' ordering gap was also fixed. All 7 prior Blocking items hold.
- **Open warnings: 7.**
  - New: N-W1 to N-W4.
  - Carried: D-W5 (residual), C-W9 (by rule), C-W24 (by design).
- **Info items: 5** (N-I1 to N-I5).

The architecture can proceed on the affected contracts (ADR-0001, 0002, 0005, 0018, 0019). N-W1 should be closed before the TDC doc-lint story, and N-W2 before the SCL/Crowd idle-gaze story.

**Recommended next step:** a scripted registry pass for N-W1, using the same method as (c). Then `/design-review` on `informant-pings-team-comms.md` for N-W4 and D-W5.
