# Story 018: Strike clip variants, local warp and contact scheduling from the server clock

> **Epic**: Assassination & Defence Reversal
> **Status**: Ready
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/assassination-defence-reversal.md` (ASN-R16, ASN-R21 idempotent presentation; VA-ASN-1, VA-ASN-3(c); AC-ASN-01, AC-ASN-21)
**Requirement**: `TR-asn-010`, `TR-asn-008`, `TR-asn-012`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0018: Encounter data and outcome presentation timing (§5.1 one idempotent handler), with ADR-0013 (one AnimBP reads `FActionState`/`FDeathState`) and ADR-0002 §9 (`EstimatedServerNow()`)
**ADR Decision Summary**: `OnRep_ActionState`, `OnRep_DeathState`, a re-open and the per-frame tick of a pawn with a scheduled clip all call one state-derived `FEncounterPresentation::Refresh(Pawn, Now = EstimatedServerNow())`; the strike clip is played so its contact frame lands at `ContactServerTime`, sought and cross-faded (≤ 0.1 s) when late and never snapped, with the warp target set locally from `P_attacker_local + WarpPoint`.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (animation and networking; MotionWarping is Beta, ADR-0013 animation is HIGH)
**Engine Notes**: One AnimBP and PoseSearch set serve every pawn (ADR-0013); the three strike entry clips are root-motion montages with a motion-warping window named `"StrikeContact"`; the target is set with `AddOrUpdateWarpTargetFromLocation` on every machine (story 014 disables replication of targets). The server mesh does not produce bone transforms headless, so AC-ASN-21's bone log forces pose tick and bone refresh every frame on the server and the bystander. The Game Animation Sample assets stay unmodified (never-resave list); clips used here are new assets or retargets, created through the editor tooling, not by this story's code. On the listen host there is no RepNotify for its own writes: `WriteActionState` calls `Refresh` directly (E7).

**Control Manifest Rules (this layer)**:
- Required: Client presentation goes through one idempotent `Refresh(EstimatedServerNow())` called from every RepNotify, re-open and the host's direct path; clips are seeked, never restarted — source: ADR-0018, ADR-0017
- Forbidden: Never present an outcome on receipt, snap, or use arrival time except as the unsynced-clock fallback; never use a reticle, lock-on, outline or camera rotation applied by the game for the attacker — source: ADR-0018, GDD ASN-R8
- Required: Clip variant by bearing (never by outcome); warp yaw ≤ `StrikeWarpYawLimitDeg`, warp translation ≤ `StrikeWarpLimitCm` — source: GDD ASN-R16
- Guardrail: `Refresh` runs only on RepNotify and while a clip is scheduled

The strike clip reaches contact at `StrikeContactSec` (0.40 s) after execution and is identical up to contact for every outcome and victim kind. One of three entry variants is chosen from the bearing θ (|θ| < 30° `Strike_F`; otherwise `Strike_L60` / `Strike_R60` by sign, ±30° exactly takes the side variant). The bush preface plays `BushStrikeEmergeSec` before the same contact segment (pose start from `FActionState.StartServerTime`). The attacker's screen never shows a reticle, lock-on marker, outline or game-applied camera rotation; the owner's strike pose starts on server confirmation (no predicted body pose).

---

## Acceptance Criteria

*From GDD `design/gdd/assassination-defence-reversal.md` (AC-ASN-01, AC-ASN-21), scoped to this story:*

- [ ] **AC1** (AC-ASN-01, [Integration/BLOCKING] `StealthGame.Assassination.KillAtArmsLength`): **GIVEN** an unbraced rival player standing with its capsule axis 230 cm (horizontal) from the attacker's **WHEN** the attacker presses Attack with the camera ray on it **THEN** the victim's Roster state becomes `Informant` (reason `Assassinated`), exactly one `OnPawnKilled(Victim, Attacker, Player, Strike)` fires in the Attack's slot, and the victim starts `Death_Fall` at `ContactServerTime` (± 1 frame); **AND** on the attacker's screen no reticle, lock-on marker, outline or camera rotation applied by the game appears at any time (captures every 100 ms from the press to contact; evidence `asn-01-attacker-*.png`).
- [ ] **AC2** (AC-ASN-21 parity group, [Integration/BLOCKING][Net] `StealthGame.Assassination.StrikeClipParity`): **GIVEN** strikes on an NPC, a rival, an informant, a braced defender and an empty spot, all at the same bearing so the variant and `WarpPoint` are equal, with the attacker's mesh forced to tick pose and refresh bones every frame on the server and on a bystander client **THEN** the attacker's bone transforms in actor-local (pawn-relative) space from execution to contact differ by ≤ 0.5 cm (VA-ASN-3c) between the server and the bystander's simulated proxy at the same server time (loss-free, clip seeked from `FActionState.StartServerTime`) and across the five targets.
- [ ] **AC3** (AC-ASN-21 bearing group, [Integration/BLOCKING][Net] and [Visual/BLOCKING] evidence `asn-21-*.png`): **GIVEN** a separate set of strikes at varied positions **THEN** θ = ±29° gives `Strike_F`, and θ = +30°, +75°, −30° and −75° give the side variant (`Strike_L60` / `Strike_R60`) of that sign per ASN-R16's bearing rule, with warp yaw ≤ 30° and warp translation ≤ `StrikeWarpLimitCm` (240) in every case; the contact-frame captures are retained.

---

## Implementation Notes

*Derived from ADR-0018 §5.1 and GDD ASN-R16, VA-ASN-1:*

`Refresh` precedence: a valid `FDeathState` (≠ −1) wins over any `FActionState` pose (except Concealment's hidden-body latch). **Strike:** set the local `"StrikeContact"` target from `P_attacker_local + WarpPoint`; play the `Variant` clip so its contact frame lands at `ContactServerTime`; if late, seek and cross-fade ≤ 0.1 s; never snap. **Counter:** starts at `ContactServerTime` (never on receipt; the defender holds `Brace_Hold` until then). **Unsynced clock** (`IsServerClockSynced()` false, roughly the first 5 s after travel): present on receipt, never earlier than receipt, logged. Clip length is `AttackAttemptLockoutSec` (1.75 s); arm held out until contact + 0.20 s; the recovery runs to 1.75 s; a whiff plays the same clip into empty air; on a reversal the attacker's clip is cut at contact into `Death_Fall` (story 020). `Strike_BushPreface` is `BushStrikeEmergeSec` (0.35 s) with the leaf crossing at 0.35 s.

The attacker's camera: no snap, aim assist or magnetism (IIF R10; IIF story 064 `no-ally-attack-no-aim-assist`, Camera story 016 aim ray). The `Attack` payload comes only from the camera aim ray. AC1's screenshot series is the AC-ASN-01 evidence; AC3's captures are the AC-ASN-21 `[Visual]` evidence.

Variant choice and the warp numbers come from the same pure core as the reach test (story 002) and the `WarpPoint` core (story 012), so server and client agree.

**Prototype replaced:** the prototype's immediate-effect strike (no clip, victim ragdolls at the slot, attacker held one second via `HoldInPlace`) and `ApplyDeathPresentation` are replaced; the prototype's `Source/StealthGame/Private/StealthCharacter.cpp` strike input path (`ServerAssassinate`) is removed in story 027.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-020-client-fall-linger-and-occlusion-removal.md`: `Death_Fall`, linger and removal.
- `story-021-role-change-dispatch-and-transient-grade.md`: own-role dispatch and the contact-frame grade.
- `story-022-culled-victim-opens-falling-and-late-actors.md`: late-opened actors (BLOCKED, S10).
- `story-014-warp-targets-not-replicated-lunge-outcome-blind.md`: the component that carries the local target.

---

## Files

- `Source/StealthGameplay/Public/View/Features/Assassination/EncounterPresentation.h` (`FEncounterPresentation::Refresh`)
- `Source/StealthGameplay/Private/Features/Assassination/EncounterPresentation.cpp`
- `Source/StealthGameplay/Private/Features/Assassination/StrikeVariantCore.h` and `.cpp` (bearing to variant, plain C++)
- `Source/StealthTestHarness/Private/Assassination/KillAtArmsLengthTest.cpp` (`StealthGame.Assassination.PIE.KillAtArmsLength`)
- `Source/StealthTestHarness/Private/Assassination/StrikeClipParityTest.cpp` (`StealthGame.Assassination.PIE.StrikeClipParity`)
- `production/qa/evidence/asn-01-attacker-*.png` and `production/qa/evidence/asn-21-*.png` (retained captures)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

QL-STORY-READY: GAPS
- AC1: "no reticle, lock-on marker, outline or camera rotation" is judged by eye and the asn-01 evidence has no named sign-off; proposed rewrite (unconfirmed): the presentation log records 0 game-applied camera rotation or aim-assist entries and 0 reticle/lock-on/outline widget entries from the press to contact; the 5 captures (0, 100, 200, 300, 400 ms) are retained and signed off in `production/qa/evidence/asn-01-evidence.md`.
- AC2: the sampling rate and bone set are unstated; proposed rewrite (unconfirmed): all skeleton bones, sampled every frame at a stated fixed rate, actor-local, from execution to contact; max per-bone delta ≤ 0.5 cm between server and bystander and across the five targets.
- AC3: the `[Visual]` pass criterion is undefined and ±30° is a float boundary; proposed rewrite (unconfirmed): the pure core is tested at exactly ±29°, ±30° and ±75° (θ = ±30° exactly gives the side variant) and the PIE run at ±29.0° and ±30.0° ± 0.01°; each retained contact-frame capture shows the named variant's clip, and the lead signs it off in `asn-21-evidence.md`.

- **AC-1**: Kill at arm's length
  - Given: an unbraced rival standing with its capsule axis 230 cm horizontal from the attacker's
  - When: the attacker presses Attack with the camera ray on the victim
  - Then: the victim is `Informant`/`Assassinated`, exactly one `OnPawnKilled(Victim, Attacker, Player, Strike)` fires in the slot, `Death_Fall` starts at `ContactServerTime` ± 1 frame, and the attacker's screen has no game-applied reticle, marker, outline or camera rotation (captures every 100 ms)
  - Edge cases: 230 cm versus the reach limit; a moving victim; the host as attacker
- **AC-2**: Strike clip parity
  - Given: strikes on an NPC, a rival, an informant, a braced defender and an empty spot at the same bearing, mesh forced to tick pose and refresh bones every frame on the server and the bystander
  - When: bone transforms are logged from execution to contact
  - Then: actor-local transforms differ by ≤ 0.5 cm between server and bystander proxy at the same server time and across the five targets
  - Edge cases: a late-received `FActionState` is seeked, not snapped (cross-fade ≤ 0.1 s); a reversal clip cut after contact is not compared; an unsynced clock presents on receipt
- **AC-3**: Bearing groups
  - Given: strikes at varied positions: θ = ±29°, +30°, +75°, −30°, −75°
  - When: the variant and warp are logged and the contact frames are captured
  - Then: ±29° gives `Strike_F`, the other four give the side variant of their sign, warp yaw ≤ 30°, warp translation ≤ 240 cm, and the captures are retained with sign-off
  - Edge cases: θ exactly on ±30°; a bush strike with `BushPreface` (0.35 s) still picks the variant by bearing; a whiff picks the variant by the `WorldPoint` bearing

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: `KillAtArmsLengthTest.cpp` and `StrikeClipParityTest.cpp` (headless where the AC allows) — must exist and pass
- Visual: retained captures `asn-01-attacker-*.png` and `asn-21-*.png` in `production/qa/evidence/` with a lead sign-off in `production/qa/evidence/asn-21-evidence.md` (the AC-ASN-21 `[Visual/BLOCKING]` tag)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `story-002-reach-f1-pure-function.md`, `story-012-slot-write-outcome-blind-and-warp-point.md`, `story-013-death-write-staging-and-after-latch.md`, `story-014-warp-targets-not-replicated-lunge-outcome-blind.md`; cross-epic `production/epics/shared-character-locomotion/story-041-body-and-anim-set-parity.md`; `production/epics/camera-system/story-016-aim-ray.md`; `production/epics/input-interaction-framework/story-064-no-ally-attack-no-aim-assist.md`
- Unlocks: `story-017-no-warning-and-no-observer-cue.md`, `story-020-client-fall-linger-and-occlusion-removal.md`, `story-021-role-change-dispatch-and-transient-grade.md`, `story-022-culled-victim-opens-falling-and-late-actors.md`
