## Task Estimate: MVP items 8-11 and a re-baseline of the full MVP
Generated: 2026-10-25 (producer estimate, `/estimate`, unattended run; no sprint history exists)

### Task Description

Estimate the four MVP items the user added on 2026-10-25 (8 informant ping, 9 text-only end-of-round summary, 10 teammate-reconnecting indicator, 11 keyboard walk-slower key), and re-baseline the whole MVP (the 7 original core items plus items 8-11) against the approved GDDs, the ADR set and the first-playable prototype. This replaces the Producer Note figure of "10-12 weeks for the 7 core items; items 8-11 not estimated".

### Capacity assumption (stated explicitly)

- **One human developer plus AI agents.** `project.yaml` `team.size: studio` describes the depth of agent specialists available (producer, unreal-specialist, ue-replication-specialist, qa-lead and so on). It is **not** a headcount. There is exactly one person who reviews, decides, runs the editor and plays.
- **Unit: calendar weeks of 5 focused working days.** Agents write code, tests and docs fast and in parallel; that part is cheap (the 2026-10-24 prototype run and the 2026-10-24/25 design pass, 430 subagent spawns, show it). The things that do **not** compress are: UE build and PIE cycles (multi-client PIE shares one machine, so integration tests run serially), the single-instance editor, human review and approval, editor-only steps and asset-resave hazards (`production/never-resave-assets.md`), engine unknowns, and tests that need a human (see Risks).
- No team of several developers is assumed anywhere. If a second human joins, the serial-verification share does not halve.

### Complexity Assessment

| Factor | Assessment | Notes |
|--------|-----------|-------|
| Systems affected (items 8-11) | Informant Pings (new), HUD, Round, Roster, Session, Teammate Markers, IIF, SCL, Crowd, IVP, TDC | Items 9-11 are additions inside systems that are already MVP; item 8 is a new leaf system that draws in HUD L0 |
| Files likely modified (prototype) | 18 files in `Source/StealthGame` | Prototype only; production code lands in the ADR-0007 module layout (see below) |
| Size of affected prototype files | 2,785 non-blank lines; 326 branch lines (approximates cyclomatic complexity) | Grep counts over the 10 affected classes (headers + sources). Largest: `StealthGameMode.cpp` 927 lines / 145 branch lines; `StealthCharacter.cpp` 603 / 78; `StealthHUD.cpp` 259 / 30 |
| New code vs modification | About 85 % new for items 8-10, 15 % modification for item 11 | The prototype has a T-key ping (about 60 lines: client RPC to team controllers, 4 s ring, 1.5 s cooldown); the approved design replaces nearly all of it |
| Integration points | 11 | Roster C-TeamView carrier, Round Event Log and `FRoundPublicState`, Session grace and rejoin, Camera `GetAimRay`, Concealment `IsInBushOrTransition`, Teammate Markers F3 clamp and `OnAllyMarkerRetired`, HUD L0 layer and F8 feed queue, IIF `IMC_Common`, SCL pace quantisation, Crowd pace rule, IVP rows |
| Test coverage needed | High | Informant Pings 21 ACs; AC-HUD-27, AC-HUD-28 (8 sub-cases a-h), Round AC 12e, Roster AC-PSR-53, AC-SCL-36; most are BLOCKING (leak and parity diffs against opponent clients) |
| Existing patterns available | Partial | Replicated team-only delivery works in the prototype; the C-TeamView part, token bucket, per-slot delta and L0 presenter pattern are all new. Production versions need the core carriers first |

**Key files likely affected (prototype today; production equivalents are named by the GDDs):**
- `Source/StealthGame/Private/StealthCharacter.cpp` -- `OnPingPressed`, `ServerPing`, input binding (T); item 11 input scaling lands in the input layer (IIF R15a), not the pawn
- `Source/StealthGame/Private/StealthGameMode.cpp` -- `HandlePing`; round-end step where `TeamSummary` would be computed
- `Source/StealthGame/Private/StealthHUD.cpp` and `StealthPlayerController.cpp` -- ring drawing, `ClientReceivePing`, banner
- `Source/StealthGame/Private/StealthMovementComponent.cpp` -- pace handling (item 11)
- Not in the prototype at all: reconnect or grace handling, `FRoundResult`, any per-team summary, C-TeamView

### Effort Estimate

**Items 8-11 together (increment on top of a built core):**

| Scenario | Weeks | Assumption |
|----------|-------|------------|
| Optimistic | 2.5 | Shared carriers (C-TeamView, HUD L0 layer, Session rejoin, Event Log) are already built and verified by the core; no PIE disconnect harness problems |
| Expected | 3.5 | Normal pace, one round of review per item, one PIE debugging pass per item |
| Pessimistic | 6 | The C-TeamView carrier or HUD L0 layer is not ready when the ping starts, the PIE disconnect and rejoin harness needs real engineering, Ctrl/Alt modifier conflicts and feel tuning |

**Recommended budget for items 8-11: 3.5 weeks (about 17 working days).** If the shared carriers are *not* ready, add about 4 working days to the ping (building the C-TeamView base inside it).

**Full MVP, re-baselined (all 11 items):**

| Scenario | Weeks | Months | Assumption |
|----------|-------|--------|------------|
| Optimistic | 18.5 | about 4.3 | Spikes S1/S2/S14 results hold, 50 NPCs fit the 3060-class budget, traversal replication survives 100 ms / 1 % loss, no ADR rework |
| Expected | 24 | about 5.5 | Normal pace; one significant rework (most likely crowd performance or traversal replication) |
| Pessimistic | 36 | about 8.3 | Two of the named technical risks bite, or 10-player real-network behaviour differs from in-process PIE |

**Recommended budget: 24 weeks.** Simple sums of the pessimistic cases are used, not root-sum-square, because the risks share causes (the same carriers, the same single PIE machine).

### Per-item breakdown (items 8-11)

Two columns: the conventional bottom-up days (Method B yardstick below) and the planning days after the uniform 0.6 calibration factor (see Re-baseline). Half-day rounding.

| # | Item | Bottom-up days (opt / exp / pess) | Planning days (opt / exp / pess) | What the work is |
|---|------|-----------------------------------|----------------------------------|------------------|
| 8 | Informant ping (system 20) | 8 / 12.5 / 20 | 5 / 7.5 / 12 | `UTeamPingViewComponent` carrier and lifecycle 1.5; `ServerTeamPing` RPC, 4-step validation, token bucket with skew tolerance, drop counters 1.5; `IA_Ping`, ping-point rule (two traces, eye-pivot range, third-person re-trace), client bucket mirror, concealed refusal 1.5; HUD L0 presenter (stepped stroke, label, label drop at `OnAllyMarkerRetired`, edge clamp, no-replay, `Ended` hide) 2; tests (9 logic, 9 PIE, bandwidth, 1 UI) 3.9; ADR-0001/0011/0021 amendments, TDC keys, IVP row 0.5; 15 % integration reserve 1.6 |
| 9 | End-of-round text summary | 3 / 5 / 8 | 2 / 3 / 5 | `FRoundResult.TeamSummary` computed at the round-end step from the Event Log (`KillResolved` wrong targets, stages, eliminated incl. forfeit) 2; HUD-E21 two lines at the F10 show time 1; Round AC 12e, AC-HUD-27, IVP no-pre-`Ended` leak check 1.5; evidence screenshot 0.5 |
| 10 | Teammate-reconnecting indicator | 5 / 8.5 / 14 | 3 / 5 / 8.5 | Roster `bReconnecting` plus `ReconnectEndCause` with the event-only clear semantics (ruling D23) 3; HUD feed lines and edge detection (F8 queue, no replay) 1.5; marker label suffix TM-R11 0.5; AC-HUD-28 (a-h) and AC-PSR-53 plus a PIE disconnect/rejoin/supersede/kick test hook 3; evidence 0.5 |
| 11 | Keyboard walk-slower | 1.5 / 2.5 / 4.5 | 1 / 1.5 / 3 | `IA_WalkSlower` Axis1D in `IMC_Common` (Ctrl 0.8, Alt 0.9, both 0.8; keyboard only; no effect while Run; no replicated field) 1; Crowd pace rule change (interim full-pace rule lifted, Crowd R3.1) 0.5; logic test of modifier combinations plus a server-sees-only-Move-magnitude check 0.5; feel check 0.5 |
| | **Total** | **17.5 / 28.5 / 46.5** | **11 / 17 / 28.5** | = 2.2 / 3.4 / 5.7 weeks, quoted as 2.5 / 3.5 / 6 |

### Re-baseline of the full MVP

**Method A: the Producer Note anchor, adjusted.** The standing figure is 10-12 weeks for the 7 core items (16+ pessimistic), made on 2026-10-24 before the prototype and before the cross-review rulings. Adjustments since:

| Adjustment | Weeks | Basis |
|-----------|-------|-------|
| Start from the midpoint of the standing figure | 11 | Producer Note |
| `NpcCount` 40 to 50 and the 3060-class budget still unmeasured (prototype: 50 characters at 89.6 FPS, 11.2 ms game thread, on an RTX 5090) | +0.75 | User decision 2026-10-25; ADR-0013/0022 |
| Teammate Markers effort S to M | +0.5 | User decision 2026-10-25 (TM OQ12) |
| Spec growth after the note: about 750 acceptance criteria across the 18 core GDDs, cross-review rulings D23-D35, the four-module layout (ADR-0007) and test harness (ADR-0008) | +1 | Gate-check and GDD set |
| Prototype credit: round, assassination, objectives, bushes, procgen kit and the Meshy pipeline are proven end to end; the 2026-10-24 note did not count this | -1.5 | `production/first-playable-report.md` |
| **Method A core** | **about 12 (opt 10, pess 18)** | Still low confidence |

**Method B: bottom-up work packages for the core (agent-assisted, conventional yardstick).** Yardstick: new replicated component with RPC and validation 1.5 days; presenter or widget 1.5-2; input action plus local rule 1; server rule or aggregation 1-1.5; logic AC 0.1; integration PIE AC 0.25 (shared harness); UI evidence AC 0.5; 15 % integration reserve. Effort-column S/M/L from the Systems Index is used only as a relative weight to place packages, not as a unit.

| Work package | Days | Notes |
|---|---|---|
| Module skeleton (ADR-0007), TDC pipeline and validator (ADR-0004), test and leak-parity harness (ADR-0008), headless CI | 12 | Prototype is one module and 12 tests |
| Roster, Session and Travel, seamless-travel state stores, reconnect grace, C-TeamView carriers | 14 | Spikes S1/S2/S14 done |
| Round and Match, resolution queue, Event Log, round-end staging | 14 | |
| Information Visibility Policy, audiences, leak tests | 10 | |
| Input and Interaction Framework, pawn command interface | 8 | |
| Shared Character and Locomotion (CMC, motion matching, significance, net parity, traversal) | 16 | Traversal under latency unproven |
| Camera | 4 | |
| Character Pool and Appearance (mannequin variants), then Meshy swap | 6 + 5 | Meshy arms 17 % short |
| Crowd and NPC Behaviour (50 NPCs, gaze catalogue, Tell Matrix) | 14 | |
| Concealment (per viewer) | 8 | |
| Assassination and Defence Reversal | 12 | |
| NPC-Kill Penalty and Informants | 8 | |
| Objectives Chain | 6 | |
| Level Socket Contract and graybox map | 4 | |
| Teammate Markers and HUD | 8 + 12 | |
| Procedural City Generator | 14 | Built last |
| 50-character performance pass, net-emulation pass, 10-player integration pass | 6 + 4 + 6 | |
| Gross | 191 | |
| Prototype credit | -15 | |
| **Method B core** | **176 days = about 35 weeks (opt 25, pess 49)** | |

**Reconciliation.** The two methods differ about 3x. Method A is a top-down guess made when the design was not written; Method B uses a human-paced yardstick that agents compress by an unknown amount. Neither is calibrated, because there is no sprint history. The re-baseline uses the geometric mean of the two (about 0.6 of Method B) and applies the **same 0.6 factor** to items 8-11, so the new items and the core are on one yardstick:

| Piece | Optimistic | Expected | Pessimistic |
|---|---|---|---|
| Core (7 original items, 18 systems) | 16 | 20.5 | 30 |
| Items 8-11 | 2.5 | 3.5 | 6 |
| **Full MVP** | **18.5** | **24** | **36** |

Versus the standing note: the core moves from 10-12 to about 20-21 weeks expected, and the whole MVP lands at about 24. Most of the move is the core, not the new items: items 8-11 are about 15 % of the total. The full-rigor regime (every GDD's ACs implemented, BLOCKING leak and parity tests, the four-module layout) is what the old figure did not count.

### MVP-core checkpoint (revised)

The Producer Note's "MVP-core at about week 6" (round, assassination/informants, objectives, crowd, bushes on a graybox map, 2-client PIE) is not reachable at the stated rigor. On the same yardstick the checkpoint is about **week 13-15** (about 124 Method B days in those packages, times 0.6, plus the share of foundations). A playable build for the MVP reading-beats-chance hypothesis exists **today**: the prototype, which already runs 2-10 player rounds. Use it for the first playtest while the production core is built; it does not satisfy the full-rigor evidence gates.

### Confidence: Low

- No sprint history and no measured velocity: this estimate is not calibrated to the team's velocity. That is a risk, not padding; the ranges are wide on purpose.
- The two methods disagree about 3x, and the 0.6 blend is a judgement.
- Three core technical risks are unmeasured: traversal replication under loss, 50 motion-matched characters on 3060-class hardware, 10 humans over a real network (the prototype's 10-player run was in-process).
- Items 8-11 themselves are well specified (approved GDDs, numbered ACs), so their *scope* confidence is Medium; the *effort* confidence is Low because it depends on core carriers that do not exist yet.

### Risk Factors

| Risk | Likelihood | Impact | Mitigation |
|------|-----------|--------|------------|
| Velocity is uncalibrated; the real agent-assist factor is far from 0.6 | High | Whole schedule +/- 50 % | Run the calibration spike below; re-baseline at the end of it |
| Reconnect indicator needs a PIE disconnect, rejoin, supersede and kick test harness that does not exist (AC-HUD-28 a-h) | Medium | +1 to +2 weeks on item 10 | Build the harness hook in the Session work; time-box a spike |
| AC-PNG-10 requires 3 reviewers who did not author the UI to blind-sort greyscale captures; a solo developer cannot supply them unaided | High | Blocks closing item 8 as written | Flag for the user: either fresh-context agents count as reviewers, or the criterion moves to ADVISORY. Not changed here |
| Ping and reconnect depend on C-TeamView, HUD L0, Markers F3 clamp and `OnAllyMarkerRetired`, all built late in the order | Medium | +4 days on the ping if built early | Schedule items 8 and 10 after HUD and Markers, not before |
| Walk-slower key collides with the sample's Ctrl jog toggle (first-playable controls: Ctrl toggles jog) and Alt has OS and PIE side effects | Medium | +1 day | Remap the jog toggle at implementation; test Alt in PIE and standalone |
| 50 NPCs at 3060-class budget unmeasured (ADR-0022 4 ms animation budget) | Medium | +1 to +3 weeks | Measure early on target hardware; significance LOD levers already in ADR-0013 |
| Traversal replication under 100 ms / 1 % loss unproven (SCL High-Risk row) | Medium | +1 to +2 weeks | Net-emulation spike before building traversal on it |
| All 23 ADRs are still Proposed | Medium | Rework if a decision flips | Accept ADRs in build order, each before its code is promoted from the prototype |
| Pings could expose Pillar 1 leaks via the controller carrier | Low | High | AC-PNG-01/-02/-03 are BLOCKING and use the opponent-as-host run |

### Dependencies

| Dependency | Status | Impact if Delayed |
|-----------|--------|-------------------|
| C-TeamView carrier and `bReconnecting` fields (Roster) | Designed, not built | Items 8 and 10 each +2-4 days |
| HUD L0 layer, F8 feed queue, F10 banner timing | Designed, prototype HUD is canvas-drawn | Items 8, 9, 10 blocked |
| Round Event Log and `FRoundPublicState` | Designed, not built | Item 9 blocked |
| Session grace and rejoin (`NotifyConnectionChanged`) | Designed, not built; prototype has none | Item 10 blocked |
| Camera `GetAimRay`, Concealment `IsInBushOrTransition` | Designed, prototype partial | Item 8 point rule and refusal blocked |
| SCL pace quantisation (R5.1) and Crowd pace set | Designed | Item 11 feels wrong without it |
| ADR amendments (0001 carrier, 0011 input and RPC, 0021 L0 slot) | Stated in the GDD as amendments; check they are applied | Item 8 cannot be promoted from prototype |

### Suggested Breakdown

| # | Sub-task | Estimate (planning) | Notes |
|---|----------|----------|-------|
| 1 | Velocity calibration spike: production Roster plus Round slice with its ACs, time-boxed | 2 weeks | Re-baseline the whole figure at the end; this is the largest uncertainty reducer |
| 2 | Core build in the existing risk-first order (round, assassination/informants, objectives on graybox, crowd, bushes; then camera and traversal; then Meshy; then procgen) | about 20.5 weeks expected | Includes the spike's 2 weeks |
| 3 | Item 9 round summary | 3 days | After Round Event Log and HUD banner |
| 4 | Item 11 walk-slower | 1.5 days | Anytime after IIF and SCL pace set; cheapest, good early win |
| 5 | Item 10 reconnect indicator | 5 days | After Session grace plus HUD feed |
| 6 | Item 8 informant ping | 7.5 days | After HUD L0, Markers clamp, Camera aim ray |
| | **Total** | **about 24 weeks** | Items 8-11 = 17 days = 3.5 weeks |

### Notes and Assumptions

- No sprint history found in `production/sprints/` (the directory does not exist); this estimate is not calibrated to the team's velocity. The only calibration data are the one-day prototype run and the two-day design pass, neither of which is production-grade work.
- "Weeks" are one developer's 5-day weeks. Agent parallelism is credited only in the 0.6 factor; PIE verification is treated as serial.
- The 0.6 factor is a judgement, not a measurement. It applies equally to items 8-11 and the core. After the calibration spike, replace it with the measured ratio.
- The seven core items span 18 of the 19 MVP systems; system 20 (Informant Pings) is item 8. The "Alpha cumulative" figure in the concept's Scope Tiers table moves accordingly (about 9-15 months; the Vertical Slice and Alpha increments themselves were **not** re-estimated here and were set against the old MVP velocity, so they are probably understated).
- Acceptance criteria counts (about 750 for the core, 21 for pings) come from Grep over the GDDs; counts are approximate because GDDs tag ACs in two formats.
- Prototype line counts are Grep counts of non-blank lines; branch lines approximate cyclomatic complexity and include comments and strings.
- Dates: all dated 2026-10-25. Estimate does not include the Vertical Slice, Alpha or any playtest scheduling.
- Items over 10 days (ping, 12.5 conventional days) should be split into stories via `/create-stories` (suggested split: carrier and validation; input and point rule; HUD presenter; test suite).
