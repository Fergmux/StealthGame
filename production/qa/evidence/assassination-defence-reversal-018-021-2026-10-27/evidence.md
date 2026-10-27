# Assassination & Defence Reversal batch evidence (stories asn-018, asn-020, asn-021), 2026-10-27

## What was built

- **One idempotent presentation handler (018/020/021)**: `UEncounterPresentationSubsystem` (Features/Assassination,
  View, every machine) subscribes to the new static `AStealthPawn::OnAnyEncounterStateChanged()` (fired after every
  FActionState / FDeathState present: RepNotify on clients, the server's direct write = ADR-0018 E7) and ticks only the
  pawns whose plan needs it. `Refresh(Pawn, EstimatedServerNow())` evaluates the pure `FEncounterPresentationCore`:
  - Strike (018): the variant's clip with its contact frame at ContactServerTime (the authored clip mapped piecewise
    onto [Contact - StrikeContactSec, Contact] and the recovery to Start + AttackAttemptLockoutSec); a late receipt is
    sought and cross-faded (<= 0.1 s), never restarted; a playing clip off its schedule is sought; the asn-014
    "StrikeContact" target drives a runtime skew-warp window to the contact frame. Counter starts at its contact.
    `FStrikeVariantCore` is the one bearing rule (the slot's `FStrikeWarpPointCore` delegates to it).
  - Fall / linger / removal (020): a reached DeathServerTime wins over the pose (the reversed attacker's lunge runs to
    contact, then Death_Fall); Death_Fall from DeathServerTime (sought when late, held at its end); pawn collision off
    client-locally at +DeathFallSec; VA-ASN-2 removal (`FBodyRemovalEvaluator`, ViewBlock traces from the local camera)
    from +7.2 s, one-frame cut at +9.2 s; a body first seen after the linger is never shown.
  - Own role (021): `UEncounterViewSubsystem` (LocalPlayer) `OnRoleChangePresented` once per FDeathState of the locally
    possessed pawn at its DeathServerTime (or on late arrival), reasons Assassinated / ReversedAgainst / InformantKilled;
    `ApplyTransientGrade` (STAND-IN for Camera 019: camera-modifier post-process beat, art bible 2.7; Reduce screen effects
    CVar) at the victim's DeathServerTime, the bound defender's Counter contact and the attacker's result contact (not a
    whiff); one beat per contact per camera. The server's victim message moved to the client dispatch.
- **Placeholder clips**: new editor command `Stealth.Animations.ImportEncounterClips` (StealthGameEditor) imports the D54
  spike clips onto SK_UEFN_Mannequin via Interchange (animations only), fixes their bone translation units (metres ->
  cm, detected from the pelvis against the reference pose: without it every pose collapsed onto the floor), and wraps
  each in a DefaultSlot montage. Assets: `Content/StealthGame/Animations/Assassination/{A_,AM_}ASN_{Strike,DeathFall,
  Counter}_D54Placeholder.uasset` (6 files, git LFS). Paths and the clip contact frame are config
  (`UEncounterPresentationSettings`, `[/Script/StealthGameplay.EncounterPresentationSettings]` in DefaultGame.ini).
- **Prototype replaced**: `AStealthCharacter::BeginDeathFallPresentation` (ragdoll stand-in) -> `BeginEncounterDeath`
  (flags only); `OnRep_Dying` ragdolls only without an FDeathState (S10 harness `KillCharacter`); concealment visibility
  stops at the death write. StealthBot gains `strike_<kind> [cm] [deg]` and `strikespot [cm] [deg]` (evidence drivers).
- **Build fix**: HEAD 91154d4 failed in unity mode (`SerializeDouble` in two anonymous namespaces of one blob); the reply
  serialiser's helper is now `SerializeReplyDouble`.

## Tests

`full-suite.txt`: `cycle.ps1 -Tests -NoLaunch` (build Result + every test line + counts).
New: `StealthGame.Assassination.StrikeBearingGroups`, `.EncounterPresentationTimeline`, `.BodyRemovalEvaluator`,
`.RoleChangeDispatchOnce`, `.PIE.KillAtArmsLength.RivalAt230cmIsAssassinatedAndFallsAtContactWithNoGameCameraRotation`,
`.PIE.StrikeClipParity.FiveTargetsAtOneBearingPoseAlikeOnServerAndBystander`,
`.PIE.OneBodySequence.NpcPlayerAndInformantFallLingerAndGoAlike`,
`.PIE.TransientGradeAtContact.OnlyParticipantsGradeOnTheirContactFrameAndNeverForAWhiff`,
`.PIE.RoleChangeGate.OwnRoleDispatchFiresOnceAtDeathOnRemoteClientAndListenHost`.

PIE measurements (headless suite run):
- KillAtArmsLength: axes 230.0 cm; contact = death 8.284; Death_Fall at 8.284 / 8.287 / 8.287 (server, client 1,
  client 2); one OnPawnKilled; the attacker machine's control rotation delta 0.0000 deg press -> contact.
- StrikeClipParity: five targets, one variant (Strike_F), one horizontal WarpPoint (150, 0) cm, one contact time; per-bone
  delta (82 skinned bones) server vs bystander 6.6-21.1 cm, target vs NPC 4.1-12.5 cm (two suite runs), worst bones calf / foot / ball
  (the sample AnimBP's per-machine foot placement): **AC2's 0.5 cm bound is not met** and reported as a warning until
  SCL-041's parity AnimBP (story 018 left In Progress).
- OneBodySequence: fall start +0.000..+0.007 s, collision off +1.200..+1.206 s, hidden +7.199..+8.831 s (or by the
  server's removal at +9.2), identical across NPC / player / informant within one frame; no blocking hit, no effect
  component, no material change.
- TransientGradeAtContact: whiff 0 calls; reversal: reversed attacker 1 call at +0.000, defender at +0.002, bystander 0;
  kill: attacker +0.011, victim +0.002, ally 0; dilation 1.0.
- RoleChangeGate: listen-host victim dispatch at +0.000, remote-client victim at +0.004, one each, Assassinated, no early
  own-role message (the host's informant state arrived before its DeathServerTime and was not followed).

## Run (run-and-observe, `.claude/docs/run-and-observe.md`)

Editor launched by `tools/dev/cycle.ps1`; 3-player PIE (listen server + 2 clients, floating windows, L_Prototype,
50 NPCs) through the Unreal MCP (StartPIE, RunPlayConsoleCommand with StealthBot, `log LogStealthGameplay Verbose`);
windows captured with a burst variant of `tools/dev/capture-window.ps1` (job tmp `burst.ps1`); every kept image looked
at, none black. Teams: host Jaguar, client 1 Eagle (solo), client 2 Jaguar.

| Folder | Shows |
|---|---|
| `story-018-strike-clip-variants-warp-and-contact-scheduling/asn-01-attacker-00..10.png` | Host kills the 230 cm rival (client 1): the lunge from the attacker's own camera every ~0.14 s; no reticle, lock-on, outline or camera rotation; round ends (Jaguar win) |
| `.../asn-21-{plus,minus}{29,31,75}-<variant>-{contact,after}.png` | `strikespot 200 <deg>`: +-29 Strike_F, +-31 / +-75 Strike_R60 / Strike_L60, translation warp toward the spot |
| `story-020-.../asn-22-{npc,player,informant}-*.png` | Each kind: contact, 0.8 s, 1.2 s, linger end, and (player, informant) the frames before / after the one-frame grace cut |
| `story-021-.../01-attacker-camera-graded-at-contact.png`, `02-same-camera-ungraded-7s-later.png` | The transient grade on the attacker's camera at contact vs the same camera 7 s later |
| `story-021-.../pie-role-change-and-grade-log.txt`, `story-020-.../pie-presentation-log-excerpt.txt` | ReversedAgainst / InformantKilled / Assassinated dispatches at each DeathServerTime; presentation rows per machine |

Bearing log (server rows): `strikespot 200 29` -> Strike_F; `30` -> Strike_F (a bot spot recomputed at exactly 30.0 deg
lands a hair inside; the pure rule's exact-30 case is the headless test's); `31` / `75` -> Strike_R60; `-29` -> Strike_F;
`-31` / `-75` -> Strike_L60. No `LogStealthFault` line in the session. Observed and fixed during the run: the first import
left the clips' bone translations in metres (every pose a heap on the floor); fixed in the import command.
Observed, not fixed (story 024's): the defender's "Reversal!" plate shows on receipt, before contact (the asn-015 stand-in).

Lead sign-offs pending: asn-21 contact-frame captures (art-director), asn-22 body sequence
(`production/qa/evidence/story-020-client-fall-linger-and-occlusion-removal-evidence.md`).

## Decisions

Logged in the decision log (dev-story, 2026-10-27): cross-epic proceed; D54 placeholder import; piecewise clip mapping;
runtime skew-warp window; clock stand-ins; client-local hide + ViewBlock occlusion; grade stand-in + Reduce screen
effects CVar; role-change reason stand-in; one grade beat per contact; AC3 +-30 boundary through the slot; victim message
at the dispatch; the unity build fix; the D54 unit fix; translation-only warp; AC2 bone bound advisory until SCL-041.
