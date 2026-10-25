# Review Log: Camera System

## Review — 2026-10-24 — Verdict: NEEDS REVISION
Review mode: full · Workflow: full
Scope signal: L (multi-system integration, 6 formulas, 6 upstream and 9 downstream dependencies, rig ADR (OQ4) plus mask-rendering spike/ADR (OQ5) and a trace-budget decision (OQ10); producer should verify before sprint planning)
Specialists: game-designer + ux-designer + systems-designer + qa-lead + ue-replication-specialist (one combined brief), creative-director (senior verdict)
Blocking items: 14 | Recommended: 17
Summary: The foundation holds. It rests on the user-locked rules: one body, the line-of-sight rule enforced by an eye-origin mask that reads geometry only, and a camera mode that is never observable. The problems were internal consistency and cross-GDD contracts:
- the soft edge stopped the edge push, so turn-in-place never fired;
- F3 hid every pawn beyond 60 m, while the prose said only a cone test applied there;
- the pitch clamp predated SCL ruling D6;
- the concealed boom was 100 cm against Concealment's 60 cm;
- the grade, exile and informant hooks that Assassination, Penalty and Teammate Markers rely on were missing;
- two engine leak paths were unverified: engine camera RPCs, and host-side simulation of masked pawns.
Creative director: NEEDS REVISION. Every fix is a local amendment, and the game's identity is unchanged.
Prior verdict resolved: First review
Findings:
- [BLOCKING] CS-R3 / F1: the soft-edge gain (0 at the limit) and the edge push (needs |r| ≥ Ylim) contradicted each other, so SCL R5.5 never fired.
  - Fixed: the push reads the raw input rate; the gain shapes only the view, floored at `CameraEdgeSoftMinGain` 0.1.
  - Mouse latch `EdgePushLatchSec` 0.15 s added (CD ruling 5).
  - I-CS-5 added (`CameraEdgePushDeg` ≥ 2).
- [BLOCKING] F3 / CS-R5b / Edge Cases: `Pass` required dist ≤ D_max, which hid every pawn beyond 60 m, while the prose said beyond D_max only the cone test applied (a wall leak).
  - Fixed per CD ruling 6: the line-of-sight test runs at every range, at `EyeMaskBeyondHz` 5 Hz beyond 60 m. No pawn is drawn without a line-of-sight result.
- [BLOCKING] CS-R3.1 / F1 / CS-R7 / AC-CS-03 / AC-CS-17: the view pitch was clamped at ±20°, contradicting SCL R9/OQ8 ruling D6 (1P camera −60°/+40°, local only).
  - Fixed per CD ruling 1: one envelope of −60°/+40° in **both** modes (CS-R2 identical aim). New keys `ViewPitchDownLimitDeg` and `ViewPitchUpLimitDeg`.
  - The own-pawn slot is promised at rest pitch only.
  - User to confirm (OQ9).
- [BLOCKING] CS-R11.3 / F2 / Tuning Knobs: `ConcealedBoomCm` 100 put the lens outside the bush dome.
  - Fixed: 60 cm straight back with no shoulder or height offset; hard range 0-70; I-CS-4 (≤ dome radius − 20 cm). This applies Concealment CR-12.
  - OQ7 closed: body yaw locked and edge push off while concealed.
- [BLOCKING] CS-R8: `WorldPoint` was undefined for strikes into bushes.
  - Fixed: the ray's `HideVolume` entry point, projected to the ground (Concealment CON-R12). AC-CS-34 added.
- [BLOCKING] CS-R12.1 / States: "fade when the binding is retired" was an undefined instant.
  - Fixed: `Dying` starts at the local contact frame. A client-local `ViewRelease` fires after `DeathFallSec` (ASN-R22), or after `ExileBeatSec` for an exile (Penalty CR-10, PEN-R16).
- [BLOCKING] CS-R11.1 / CS-R15: grade timing and the informant grade were missing.
  - Fixed: `ApplyTransientGrade` runs only at the local contact frame on participants, never on a whiff (VA-ASN-4).
  - A persistent `SetOwnerGrade(Informant)` is on from the cut frame (VA-NKP-4).
- [BLOCKING] CS-R15 / Interactions: Teammate Markers CR-3.
  - Fixed: a `GetViewState()` Live/Neutral read, and the fade layer sits above markers and below plates.
- [BLOCKING] CS-R12.2 / CS-R1: neutral view mechanism and data access.
  - Fixed: a client-local camera actor that never uses spectator state (IVP R13).
  - Owner-audience client data only, with no server query headers (AC-IVP-17; AC-CS-39).
- [BLOCKING] CS-R1 [verify]: the claim that the camera "sends nothing" ignored engine camera-update RPCs, which could leak the lens transform and so the mode.
  - Fixed: `ServerUpdateCamera` and client-side camera updates are disabled; `bAutoManageActiveCameraTarget` is false; the listener is overridden; AC-CS-27 traces the network.
- [BLOCKING] CS-R1 [verify]: on the listen-server host, masked pawns could skip authoritative animation tick, root motion and notifies.
  - Fixed: always-tick for all pawns. "Not rendered" is the mask's flag, never the engine's was-recently-rendered test. AC-CS-28 added.
- [BLOCKING] AC-CS-03 / AC-CS-04: untestable as written.
  - Fixed: explicit input specs, gain values of 1 / 0.5 / 0.1, and a latch test (AC-CS-38).
- [BLOCKING] AC-CS-05: the oracle was unsound.
  - Fixed: the 25-point oracle is a superset of the 5 probes, traced from the real eye; "visible" means α > 0; samples are taken after settling; pitch samples extended to −60 and +40.
- [BLOCKING] Dependencies / Cross-References: statuses were stale (Concealment, Assassination, Penalty, Crowd and Teammate Markers listed as "not written"), and the index numbers were wrong (Teammate Markers is 17, HUD is 18). Fixed, with rows added for Penalty, Crowd and IVP R13.
- [RECOMMENDED] F3 cost: the "about 150 traces per frame" figure was underived. Fixed: the worst case is 245 per frame, the cone prefilter runs first, async traces add one frame of latency, and the budget goes to technical-director (OQ10).
- [RECOMMENDED] CS-R5b: the lagged pivot could see over a wall on a descent. Fixed: the mask origin height is min(lagged, real eye) (AC-CS-29).
- [RECOMMENDED] CS-R5b render method. Fixed:
  - custom primitive data or a per-pawn MID;
  - `SetVisibility(false)` at α = 0;
  - no per-frame `CastShadow` toggles (virtual shadow map invalidation).
- [RECOMMENDED] F4: the example's frame 11 was corrected to frame 10. I-CS-1 is now a runtime clamp (blend ≤ lockout − 1 frame).
- [RECOMMENDED] F5 / CS-R13: the slider was a no-op above about 61° at 21:9. Fixed with a per-aspect slider top (AC-CS-18).
- [RECOMMENDED] F6 / CS-R14: radial versus per-axis dead zone was ambiguous. Fixed: scaled radial with the direction preserved, and a diagonal example. The edge-to-edge time is 0.9 s.
- [RECOMMENDED] CS-R10: R3 click for the shoulder swap misfires during stick look. Changed to D-pad Right (IIF change request).
- [RECOMMENDED] CS-R13: Reduce camera motion was extended to cover the toggle dolly, the boom recovery and the FOV ease. The body-driven view turn stays, and is a comfort watch item in OQ3.
- [RECOMMENDED] UI Requirements: the mask is now taught by a scripted graybox corner. The hint moves to the first live 3P frame. The neutral view shows Penalty's countdown.
- [RECOMMENDED] Crowd: export `EvaluateViewEnvelope` (F1) for NPC virtual look, with push durations taken from human telemetry; the Tell Matrix row remains requested.
- [RECOMMENDED] AC-CS-09: rewritten as a pixel diff against a capture with the dummy deleted (≤ 2/255).
- [RECOMMENDED] AC-CS-13: p99 ≤ 1.0 ms added, a named CPU, and a worst case with every pawn in the cone and hidden.
- [RECOMMENDED] AC-CS-24 / AC-CS-25: statistically weak. Rewritten: a paired design with n ≥ 12 and a Wilcoxon test (otherwise advisory); p90 over ≥ 20 events.
- [RECOMMENDED] AC-CS-01: made deterministic (seeded; `ServerMove` payload bytes compared).
- [RECOMMENDED] AC-CS-26: made two-sided per CD ruling 7 (3P dominated by 1P is an accepted cost, held to a test), with a neighbour-visibility metric. If 1P wins by more than 15 %, the default-mode question goes to the user (OQ11).
- [RECOMMENDED] Rules without ACs. Added AC-CS-29 to AC-CS-39: pivot cap and correction snap, squeeze and swap, FOV ease, Reduce motion, grade hooks, view state and layer order, beyond-range line of sight, concealed no-turn, mouse latch, include check.
- [RECOMMENDED] AC-CS-11 now compares against the oracle, not a 1P capture. AC-CS-17 was renamed to `FirstPersonHeadHidden` and covers −60°.
Specialist disagreements:
1. **Pitch range for 3P.** The main review proposed −60°/+40° in both modes. The panel argued that D6 covered first person only, and that in 3P the own body leaves the frame at those pitches, so 3P should be a separate decision. The creative director ruled for both modes on fairness grounds: CS-R2 promises identical aim, and a bush at the feet must be strikeable in either mode. The framing cost was accepted. Because this extends D6, the user is asked to confirm (OQ9).
2. **3P dominance.** The game-designer called 3P being dominated by 1P BLOCKING. The creative director ruled it an accepted cost of the user lock, held to a two-sided read-parity test. 3P stays the default unless that test fails, and then the default goes to the user.
Change requests raised (not applied to other GDDs; Camera Cross-References items 1-11):
- IIF: shoulder swap on D-pad Right; `WorldPoint` from Camera; "rendered" means the mask flag.
- SCL: always-tick on the host; the R11 mask flag; R9 pitch in both modes.
- Concealment: safe maximum 80 → 70 under I-CS-4; publish the dome radius.
- Crowd: `EvaluateViewEnvelope` and the Tell Matrix row.
- HUD: hint timing, `GetViewState()`, countdown plate.
- TDC: five new keys and I-CS-4/5.
- Onboarding: the scripted mask corner.
Pending user decisions added: OQ9 (pitch in both modes, CD-recommended, applied provisionally); OQ12 (tap/hold toggle and a left-handed default shoulder, scope, not applied). OQ11 is conditional on the AC-CS-26 result.
Note: the target document was revised after this review, so it is deliberately not stamped below (a re-review must run in full).
Reviewed-Content-Hash: design/registry/entities.yaml dfeacb02917f751cb1dfd88cf490c7ec3d8d2d0f
Reviewed-Content-Hash: .claude/skills/design-review/SKILL.md a63d8fba9b7142eeb43791f061e2e1533c429b43
Reviewed-Content-Hash: design/gdd/shared-character-locomotion.md 13f87f4eae08c99906862f29de8e94cc59796e3d
Reviewed-Content-Hash: design/gdd/input-interaction-framework.md 90b88857c2a98e951356a90c5878a80f231c47ca
Reviewed-Content-Hash: design/gdd/information-visibility-policy.md d8fd90df42d37734c67ecba262aefd4ca1d353b8
Reviewed-Content-Hash: design/gdd/concealment-bushes.md dae06ab7f91d65fbb7e2c82c6d4fa3e7b710d877
Reviewed-Content-Hash: design/gdd/assassination-defence-reversal.md 92b2933d7e7100547848bb6450f1aea75ece2cae
Reviewed-Content-Hash: design/gdd/npc-kill-penalty-informants.md f06247d43d0cf957747d3c8f7003659075ce1f30
Reviewed-Content-Hash: design/gdd/teammate-markers.md fa6747053069b90ca709dd4b0b3aa5ab56eb2efa
Reviewed-Content-Hash: design/gdd/crowd-npc-behaviour.md d5494369a7ed46d882c863563dd3228f5d2281ab
Reviewed-Content-Hash: design/gdd/game-concept.md 8dccb622e17a7f1dc58349dd2baa54d52fc36d32
Reviewed-Content-Hash: design/art/art-bible.md 23373eb6ce1b1161988b4d931c1bbd545bd33f34

## Review — 2026-10-25 — Verdict: NEEDS REVISION
Review mode: lean · Workflow: full
Scope signal: L (6 formulas, 6 upstream and 10 downstream dependencies, rig ADR-0015, mask spike OQ5 and trace budget OQ10; producer should verify before sprint planning)
Specialists: none (lean, single-session)
Blocking items: 2 (both fixed in the GDD text; verdict kept until a re-review) | Recommended: 7
Summary: Second-round review. All 14 first-round blockers were checked against the text and are resolved, and the F1/F2/F4/F5/F6 worked examples, AC thresholds and registry-owned key values were re-derived and hold. Two new defects came from changes made after the first review: the worst-case pawn count was stale after the user raised `NpcCount` to 50 (59 other pawns, not 49), and the new Informant Pings GDD (#20) consumes `GetAimRay()` while Camera did not list it as a dependent.
Prior verdict resolved: Yes (14 of 14 blockers; 17 recommended items applied or accepted)
Findings:
- [BLOCKING] F3 / AC-CS-13 / OQ10 / OQ5: the worst case was 49 pawns x 5 = 245 traces, from `NpcCount` 40. At `NpcCount` 50 with 10 players there are 60 characters, so 59 others and 295 traces (about 1.5 ms at 5 us). AC-CS-13 tested the wrong worst case.
  - Fixed: F3, AC-CS-13 (59 dummies), OQ5, OQ10, CS-R5c and the Visual/Audio note now say 59 / 295 / 1.5 ms.
- [BLOCKING] Dependencies / Interactions / Cross-References / CS-R8: Informant Pings & Team Comms (#20) lists Camera as a Hard dependency and uses `GetAimRay()` and the eye pivot, but Camera listed no such dependent (the bidirectional-dependency rule).
  - Fixed: added to the Downstream table, the Interactions table and Cross-References. CS-R8 now states the `FAimRay` return contract (ADR-0015 §5) and the trace channels (`ViewBlock` for the world, an object-type Pawn query for pawns), and says Pings read only `Origin` and `Direction`.
- [RECOMMENDED] AC-CS-05: the oracle is traced from the real eye socket, but the mask origin is the capsule axis (CS-R4) and the 1P camera sits 8 cm forward of it. "Zero false reveals" can fail by a few centimetres of head offset alone. Pick one oracle origin (the 1P lens position is the natural one) or add a stated tolerance. Not edited: it is a design choice for the author.
- [RECOMMENDED] CS-R6 / CS-R5b: the mask turns off below 20 cm free length with no hysteresis, while the 60 cm head-hide has 15 cm. Alley flicker at 20 cm is possible. Add a hysteresis value as a tuning key. Not edited: it needs a new value.
- [RECOMMENDED] AC-CS-23 is tagged [P] but is a document review ("when it is reviewed"). Retag it as a document or lint check.
- [RECOMMENDED] Rules lacking ACs: strike-warp view hold and close third person. Fixed: AC-CS-40 and AC-CS-41 added.
- [RECOMMENDED] F3 `fails` was undefined in the formula. Fixed: defined as consecutive failed tests, reset on `Pass`.
- [RECOMMENDED] Dependency status columns go stale (they predate the reconciliation). Fixed: the source note now names the systems-index as authoritative. The statuses themselves were not refreshed.
- [RECOMMENDED] Hide latency beyond 60 m: with `EyeMaskHideFailCount` 2 at 5 Hz a pawn that steps behind a wall stays drawn for up to 0.4 s (0.13 s at 15 Hz). It only keeps a pawn already seen, so it leaks nothing, but Game Feel only states the reveal bound. Add the hide bound to Game Feel.
Cross-doc follow-ups (main session):
- systems-index.md: Camera System row status to "Needs Revision" (this verdict), or "In Review" once the main session accepts the applied fixes.
- shared-character-locomotion.md R9 still reads "user to confirm, Camera OQ9" and "first-person camera pitch -60 to +40" (its pitch line in the Camera change-request list); OQ9 was confirmed by the user 2026-10-25, so drop "user to confirm".
- adr-0015-camera-los-envelope.md: Verification item (3) and the Consequences text use 245 traces and "50 pawns"; update to 295 traces and 60 characters. Check ADR-0022's mask cost line the same way.
- informant-pings-team-comms.md: its Camera row says "no Camera change". Still true; Camera now lists Pings as a Hard-for-Pings dependent.
- tuning-data-config.md: when the hysteresis for the 20 cm mask-off threshold is added, register it as a Camera key.
Note: the target document was revised after this review, so it is deliberately not stamped below (a re-review must run in full).
Reviewed-Content-Hash: design/registry/entities.yaml 28f7a489149e3fba702986d089b7799719796b39
Reviewed-Content-Hash: .claude/skills/design-review/SKILL.md a63d8fba9b7142eeb43791f061e2e1533c429b43
Reviewed-Content-Hash: design/gdd/shared-character-locomotion.md 6df3690b6fb980c603a0a0be65d9a6224c0b57b9
Reviewed-Content-Hash: design/gdd/input-interaction-framework.md ed42829b7c7aea9557521fb4c45d124224835244
Reviewed-Content-Hash: design/gdd/information-visibility-policy.md 9ce1d9677b38048e71176b7dd0b44959ce5c99f5
Reviewed-Content-Hash: design/gdd/concealment-bushes.md a7075ec56f19ea43b29d27c845e98d280ed6797f
Reviewed-Content-Hash: design/gdd/assassination-defence-reversal.md d89bac971a0a37d20024e0b32fb89559467cf5ab
Reviewed-Content-Hash: design/gdd/npc-kill-penalty-informants.md 12fe9795d9b3f3a6a34b36063e17bccc2f3ad910
Reviewed-Content-Hash: design/gdd/teammate-markers.md a99e945ffb425c4da6bd1094f01b6fd9ced99ff6
Reviewed-Content-Hash: design/gdd/crowd-npc-behaviour.md 242977e3dcc0185347fd1053741e6f305c1e42fa
Reviewed-Content-Hash: design/gdd/informant-pings-team-comms.md 6f5397e9f8ef2526f8d2c6c107100ba7a2db673b
Reviewed-Content-Hash: design/gdd/hud.md 05f2d8dac61a985f97a45a83f56d07c421369931
Reviewed-Content-Hash: design/gdd/game-concept.md 6dbca7dc538ed44d89a5f009dbc8eda18782a2c8
Reviewed-Content-Hash: design/art/art-bible.md ba9f28821285a7933cad48bee9f4ccf28499ac5e

## Closeout cross-doc pass 2026-10-25
- D6 applied: AC-CS-05 oracle and mask use the capsule axis origin; `CameraMaskOffHysteresisCm` 10 (off at 20 cm, on at 30 cm); AC-CS-23 retagged document review; hide latency beyond 60 m stated as a Game Feel limit.
- Ref: see production/design-closeout/2026-10-25_rereview-followups.md and 2026-10-25_gdd-pass-remaining.md.

## Cross-review fix 2026-10-25
- C-W11: I-CS-1 says reject (no runtime clamp), CS-R7 text and AC-CS-16 follow; `CameraToggleBlendSec` hard 0-0.29 -> 0-0.28 (F4 range too).
- C-W14: Camera key count recounted from the Tuning Knobs table: 49.
- C-W17(b): `EyeConeHalfHDeg` / `EyeConeHalfVDeg` safe minima 52 / 36 -> 54 / 38 (I-CS-3).
- C-W25: mask-off threshold 20 cm -> 25 cm (on above 35 cm, hysteresis key unchanged at 10) because the geometric minimum free boom is 21 cm; F2 example corrected to 21 cm.
- C-W37: CS-R1, edge case, dependency row, host note and AC-CS-28 reworded to the observable result under D3 tiering (no "always-tick", no identical pose-tick counts).
- D-W7 / D27: P2 Player Fantasy line records that watching costs body position and stand length only.

### Cross-review fix 2026-10-25 — main-session follow-ups
- C-W11 follow-up: F4 T_lock range follows the IIF lockout safe range 0.25-0.5. C-W25 threshold change (25/35 cm) mirrored into TDC, registry, ADR-0004 and ADR-0015 (amends D6).
