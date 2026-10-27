# Story 093: Static scan and prototype retirement

> **Epic**: Input & Interaction Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/input-interaction-framework.md`
**Requirement**: `TR-iif-001`, `TR-iif-003`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

ADR-derived story (no numbered GDD acceptance criterion): the criterion is the ADR-0011 Validation Criteria "Static scan" plus the Migration Plan's replace-not-port list.

**ADR Governing Implementation**: ADR-0011: Pawn command interface and input pipeline
**ADR Decision Summary**: ADR-0011 puts all gameplay commands in one reliable RPC on `AStealthPlayerController` (`ServerPawnCommand`), none on the pawn; makes server-meaningful input actions Boolean with only the default trigger (Hold, HoldAndRelease, Tap, Pulse and Chorded banned); requires `UEnhancedInputUserSettings` for rebinding; and decides that nothing in the prototype is promoted: the prototype input code and Server RPCs are replaced, not ported.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Engine fact E4: `UPlayerMappableInputConfig` is deprecated since 5.3 in favour of `UEnhancedInputUserSettings`; E12: `bIgnoreAllPressedKeysUntilRelease` covers Boolean actions only, which is why server-meaning actions must be Boolean. The reflection half uses `UFunction::FunctionFlags & FUNC_Net` over the pawn class and its component classes; the asset half uses the asset registry (the same lint as story 007). Raw Input is not used (deprecated in 5.8).

**Control Manifest Rules (this layer)**:
- Forbidden: Never add a Move/Look RPC or a Look event; never add a gameplay RPC on the pawn; the client never names the pawn it commands; `PawnRef` is never an object-pointer field — source: ADR-0011
- Forbidden: Never use Hold/HoldAndRelease/Tap/Pulse/Chorded triggers for server-meaningful actions — the client would own hold timing — source: ADR-0011
- Forbidden: Never use `UPlayerMappableInputConfig` or the Raw Input plugin; never make a gameplay command a Common UI action — source: ADR-0011
- Required: Prototype input code (legacy binds and Server RPCs on `AStealthCharacter`, cooldown on the PlayerState, NPC path-following movement) is replaced, not ported, after the ADR is Accepted — source: ADR-0011
- Forbidden: Never move a pawn through `RequestDirectMove`/`RequestPathMove` or AI path following; AI movement enters only through `UPawnCommandComponent::Move` — source: ADR-0010

---

## Acceptance Criteria

*From ADR-0011 Validation Criteria ("Static scan") and Migration Plan, phrased as one testable criterion:*

- [ ] **GIVEN** the project source, the Shared Pawn class and its components, and the `IA_*` assets **WHEN** the static scan runs **THEN** the pawn and its components declare no gameplay `UFUNCTION(Server|Client|NetMulticast)`; no server-meaning `IA_*` action carries a Hold, HoldAndRelease, Tap, Pulse or Chorded trigger; there is no `UPlayerMappableInputConfig` use; and the prototype input and RPC code is removed, not ported (the named symbols of the Implementation Notes are absent from the project modules). *(Source: ADR-0011 Validation Criteria "Static scan" and Migration Plan.)*

---

## Implementation Notes

*Derived from ADR-0011 Validation Criteria, Migration Plan and Implementation Guidelines:*

**What this story builds.** A static scan with two halves and the removal of the prototype code it checks for. Reflection half (automation test, runs in the editor build): enumerate the UFunctions of `AStealthPawn` and its components and fail on any with `FUNC_Net` declared in a project module class (the qa-lead's allow-list rule: functions declared in engine classes are ignored, so the CMC `ServerMove` family on the pawn's movement component is not a failure; any net UFunction declared in a project class of the pawn is). Asset half: reuse story 007's trigger lint list (the server-meaning `IA_*` set) rather than a second definition; fail on a Hold, HoldAndRelease, Tap, Pulse or Chorded trigger on them. Source half (a CI grep step): zero hits for `UPlayerMappableInputConfig` and for each retired prototype symbol in the project modules, with a denominator check (scanned N files, N > 0, else fail).

**Named prototype symbols to remove (replaced, not ported):**
- `StealthCharacter` `SetupPlayerInputComponent` binds (legacy gameplay binds);
- `OnAssassinatePressed`;
- `OnDefencePressed` / `OnDefenceReleased`;
- `OnInteractPressed` / `OnInteractReleased`;
- Server RPCs `ServerAssassinate(Target, StrikePoint)`, `ServerSetDefence`, `ServerSetInteracting`, `ServerPing`;
- `StealthPlayerController` input mode and controller RPC code (input setup moves to `AStealthPlayerController::SetupInputComponent`, story 094);
- `StealthNpcController` movement (AI movement not through the player path; crowd movement enters only through `UPawnCommandComponent::Move`, ADR-0010 and ADR-0016).
- Added by Assassination story asn-027 (2026-10-27; already removed and scanned by `StealthGame.Assassination.PrototypeRetiredScan`, `Source/StealthGameplay/Private/Tests/Assassination/PrototypeRetiredScanTest.cpp`, whose list this scan reuses): `KillCharacter`, `StartDefence`, `StopDefence`, `SetDefenceStance`, `ApplyDeathPresentation`, `HandleAssassinationRequest`, `HandleDefenceRequest`, `bDying`, `bPreparingDefence`, `LastDefenceActivationTime`, the `UStealthSettings` keys `AttackerRecoveryTime`, `BodyLifetime`, `BlindStrikeRadius`, `AssassinationConeHalfAngle`, `AssassinationServerTolerance` (also absent from `Config/*.ini`), `ResolveAssassination`, `CanStartDefence`, `DefenceCooldownRemaining`, `ShouldEndDefence`, `EStealthAssassinationOutcome`, `EStealthTargetKind`, `IsPreparingDefence`, `FindAssassinationTarget`, `BeginEncounterDeath`, `OnRep_Dying`, `DefenceActivationTime`, `HandleEncounterCounterPose`.
Per the plan's prototype disposition the prototype is not promoted; `StealthRulesTests.cpp` is kept only as the unit-test style template (`StealthGame.Rules.*`). Cooldown fields on the PlayerState and `FindAssassinationTarget` lock-on-style targeting are also on ADR-0011's replace list; their removal belongs to the assassination epic and is named here only so the scan does not whitelist them.

**Gaps to resolve in the test design (qa-lead).** (1) Define the net-UFunction rule by allow-list as above. (2) "Server-meaning IA_*" reuses story 007's list. (3) "Removed, not ported" is testable only as absence of the named symbols and files above. The reflection half runs in automation; the grep half runs as a CI step.

**Test name.** `StealthGame.Input.StaticScan` (class `FInputStaticScanTest`), per the qa-lead.

**Prototype.** This story is the retirement of the prototype code listed above; no prototype counterpart is ported.

**Tuning keys (defaults).** None.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/input-interaction-framework/story-007-input-assets-and-trigger-lint.md`: authoring the `IA_*` assets and the trigger lint itself (this story reuses its list).
- `production/epics/input-interaction-framework/story-094-controller-transport-skeleton.md`: the replacement controller transport and input setup.
- `production/epics/input-interaction-framework/story-020-ingress-clamp-and-drop.md`: the replacement validation path (ingress and stage 2).
- `production/epics/input-interaction-framework/story-013-flush-sends-end.md`: the replacement release-safety behaviour.
- `production/epics/input-interaction-framework/story-019-remove-sample-ctrl-jog-toggle.md`: removal of the sample's Ctrl jog toggle (D37).
- `production/epics/input-interaction-framework/story-068-no-owner-data-on-pawn.md`: replicated-property scan of the pawn.

---

## Files

- `Source/StealthGameplay/Private/Tests/Input/StaticScanTest.cpp` — class `FInputStaticScanTest`, name `StealthGame.Input.StaticScan` (reflection and asset-registry halves)
- `tools/ci/iif-static-scan.sh` — CI grep half with the denominator check (planned)
- `Source/StealthGame/StealthCharacter.cpp` and `.h`, `Source/StealthGame/StealthPlayerController.cpp` and `.h`, `Source/StealthGame/StealthNpcController.cpp` and `.h` — prototype input, RPC and movement code removed (the files are edited down; do not touch `Content/` or any `.uasset`)

---

## QA Test Cases

*Test cases not yet defined — run /qa-plan to generate them.*

*QL-STORY-READY: GAPS (qa-lead):*
- Gap 1: "gameplay UFUNCTION(Server|Client|NetMulticast)" excludes legitimate engine RPCs (the CMC `ServerMove` family on the pawn's component); define by allow-list: functions declared in engine classes are ignored; any net UFunction declared in project module classes of the pawn is a failure.
- Gap 2: "server-meaning IA_*" needs a list; reuse the story 007 lint list rather than a second definition.
- Gap 3: "removed, not ported" is not testable; rewrite as absence of named symbols and files (`OnAssassinatePressed`, `OnDefencePressed/Released`, `OnInteractPressed/Released`, `ServerAssassinate`, `ServerSetDefence`, `ServerSetInteracting`, `ServerPing`, prototype `SetupPlayerInputComponent` binds, `StealthPlayerController` input mode and RPCs, `StealthNpcController` movement).

*Draft spec from the qa-lead (unconfirmed), test `StaticScanAndPrototypeRetirement` (`StealthGame.Input.StaticScan`):* Given reflection over `AStealthPawn` and its components, when UFunctions with `FUNC_Net` declared in project modules are listed, then none. Given an asset registry scan of server-meaning `IA_*`, then no Hold/HoldAndRelease/Tap/Pulse/Chorded trigger. Given a source grep of project modules, then zero hits for `UPlayerMappableInputConfig` and for each retired prototype symbol. Edge cases: denominator check (scanned N files, N > 0, else fail); the CI step runs the grep half (the reflection half in automation).

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Input/StaticScanTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/input-interaction-framework/story-020-ingress-clamp-and-drop.md`, `production/epics/input-interaction-framework/story-013-flush-sends-end.md`, `production/epics/input-interaction-framework/story-019-remove-sample-ctrl-jog-toggle.md`; cross-epic: shared-character-locomotion (stories not yet written; reparented pawn Blueprint with the sample's input bindings removed, ADR-0010 migration); cross-epic: crowd-npc-behaviour (stories not yet written; crowd follower replacing `StealthNpcController` movement)
- Unlocks: None
