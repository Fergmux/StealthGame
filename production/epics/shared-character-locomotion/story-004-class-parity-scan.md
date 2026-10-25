# Story 004: Class parity scan: no observable path branches on controller kind, with a negative control

> **Epic**: Shared Character & Locomotion
> **Status**: Complete
> **Layer**: Core
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: 2026-10-25

## Context

**GDD**: `design/gdd/shared-character-locomotion.md` (R1 One class; AC-SCL-01)
**Requirement**: `TR-scl-001`, `TR-scl-006`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0001: Crowd identity rules (also ADR-0010 kind-blind code, ADR-0013 AnimBP inputs)
**ADR Decision Summary**: Pillar 1 holds only if one pawn class, one command path and one look path make every observable behaviour identical for NPC and player; no property, function, Blueprint branch, AnimBP read or audio path may test `IsPlayerControlled`, `IsLocallyControlled` (for anything observable), controller class, `PlayerState` presence or Roster state.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: The sample Blueprints contain no `GetPlayerState` or `IsPlayerControlled` (spike S14 static audit), but `CBP_SandboxCharacter.CanSprint` and the AnimBP property access `SandboxCharacter.IsLocallyControlled` branch on local control, and `EventBeginPlay` binds by `GetLocalRole` once. The scan therefore has to read Blueprint graphs (DataValidation-hosted audit, ADR-0007/0008) as well as C++.

**Control Manifest Rules (this layer)**:
- Required: every Shared Pawn uses the one AnimBP, PoseSearch database set and chooser set (ADR-0013); movement shaping is one function per rule for every pawn and reads only tuning values (ADR-0010).
- Forbidden: never read controller kind, PlayerState or team in a movement rule; never select an animation asset, layer or graph by controller, team, participant state or `AppearanceId` (ADR-0010, ADR-0013); `IsLocallyControlled` only in owner-local presentation (camera, own head hide, input binding).
- Guardrail: lint runs in CI only.

---

## Acceptance Criteria

*From GDD `design/gdd/shared-character-locomotion.md`, scoped to this story:*

- [x] **AC-SCL-01** [U] `ClassParity` (scan half; the tuning-key clause is NOT ASSESSED, carried over to SCL-003, see Completion Notes): GIVEN the Shared Pawn class, its components, AnimBP and audio code, WHEN a static scan and a reflection dump run, THEN no observable path branches on controller class, `IsPlayerControlled`, `PlayerState` presence or Roster state (only the R9 owner-local list), and every SCL tuning key has exactly one value (no per-kind variant).

*QA clarification (create-stories 2026-10-25, not a GDD change):* the forbidden-symbol list is ADR-0013 section 1 plus `IsBotControlled`, committed as a fixture; scan roots are the pawn C++ and Blueprint class (including `CBP_SandboxCharacter.CanSprint`), `UStealthMovementComponent`, `UStealthAnimInstance`, the AnimBP, and the step-clock and footstep code; assets are loaded read-only; the negative control on the sample snapshot must report at least one hit.

---

## Implementation Notes

*Derived from ADR-0001 section 1, ADR-0010 "Kind-blind code" and ADR-0013 section 1:*

- The scan has three parts: (1) a C++ source scan over `Core/SharedPawn`, `Core/Animation` and the step-clock files for `IsPlayerControlled`, `IsLocallyControlled`, `IsBotControlled`, `GetController`, `GetPlayerState`, `Cast<APlayerController>` and Roster binding queries, with an explicit allowlist for the R9 owner-local uses (camera, own head hide, input binding), each allowlist entry named by file and function; (2) a Blueprint and AnimBP graph walk of the single Blueprint child of `AStealthPawn` and its AnimBP for the same reads (`K2Node_CallFunction::GetTargetFunction`, property-access nodes); (3) a reflection dump asserting every SCL tuning key has one value and no key name carries an NPC or player variant.
- **Negative control is part of this story**: the scan must run against the unmodified sample AnimBP (`ABP_SandboxCharacter`, which reads `IsLocallyControlled`) and FAIL there; keep that fixture as a committed snapshot or generated report so the failure is demonstrable without touching the live asset. Story 005 removes the branch and the live scan then passes. A scan that has never been seen failing is not known to scan anything (`.claude/rules/test-standards.md`).
- Make the scan fail closed: an asset the walker cannot open or a graph node type it cannot classify is reported NOT ASSESSED and fails the test, never passes silently.
- **Prototype replaced**: prototype `AStealthCharacter::IsNpc`, `GetTeam`, `IsInformant`, `IsActiveAgent` and `IsTeammateOfLocalViewer` read the pawn's PlayerState and are exactly what this scan forbids; they are replaced, not ported (spike S14 lists them). The prototype spike `Source/StealthGame/Private/Spikes/StealthIdentitySpike.cpp` shows a working readback of replication conditions and can seed the reflection dump.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 005: removing the sample AnimBP's `IsLocallyControlled` branch so the live scan passes.
- Story 032: replicated-property allowlist and RPC enumeration.
- Story 041: body, collision, RVO and animation-set parity.

---

## Files

- `Source/StealthFoundation/Public/Server/Testing/StealthClassParityScan.h` (shared fixture parser, C++ scanner and judge; test seam, `WITH_STEALTH_TEST_SEAMS`)
- `Source/StealthGameplay/Private/Tests/Locomotion/ClassParityTest.cpp` (test `StealthGame.Locomotion.ClassParity.Source`)
- `Source/StealthGameEditor/Private/Tests/ClassParityAnimBpAudit.cpp` (Blueprint walker; tests `StealthGame.Locomotion.ClassParity.Blueprint`, `.ClassParity.NegativeControl`, and SCL-005's `AnimBpNoLocalControlRead`)
- `Source/StealthGameEditor/StealthGameEditor.Build.cs` (`BlueprintGraph` private dependency)
- `tests/fixtures/scl/class-parity-fixture.txt` (forbidden symbols, local-control set, scan roots, R9 allowlist)
- `tests/fixtures/scl/sample-animbp-negative-control.txt` (the walk of the unmodified sample, the committed negative control)

---

## QA Test Cases

*QL-STORY-READY: ADEQUATE (after clarification). Test `StealthGame.Locomotion.ClassParity`.*

Test: class parity scan (AC-SCL-01)
  Given: the scan fixture (forbidden symbols and the R9 owner-local allowlist) and the reflection dump of the tuning schema
  When: the static scan and the reflection dump run over the scan roots
  Then: 0 forbidden-symbol hits outside the allowlist; every tuning key has exactly one value (no per-kind variant)
  Negative control (mandatory): the same scan on the unmodified sample snapshot (`_snapshots/2026-10-24_sample-blueprints-pre-resave/`) must report at least one hit (`SandboxCharacter.IsLocallyControlled` in `ABP_SandboxCharacter`; `CanSprint` in `CBP_SandboxCharacter`); if it passes on the sample the scan is broken and the story fails
  Edge cases: assets load read-only (never-resave list); an asset the walker cannot open is NOT ASSESSED and fails; the owner-local first-person head hide is on the allowlist

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Locomotion/ClassParityTest.cpp`, test name `StealthGame.Locomotion.ClassParity` — must exist and pass; the recorded negative-control failure on the unmodified sample AnimBP is attached to the story's completion notes

**Status**: [x] Created and passing (`StealthGame.Locomotion.ClassParity.Source`, `.Blueprint`, `.NegativeControl`, 2026-10-25); negative-control failure recorded

---

## Dependencies

- Depends on: `production/epics/shared-character-locomotion/story-001-pawn-class-default-subobjects.md`, `story-003-tuning-keys-registered.md`
- Unlocks: `production/epics/shared-character-locomotion/story-005-animbp-islocallycontrolled-removal.md`

---

## Completion Notes
**Completed**: 2026-10-25 (with SCL-005, one commit per D41)
**Criteria**: AC-SCL-01 scan half passing; tuning-key clause NOT ASSESSED (carryover).
- C++ scan (`ClassParity.Source`): comment-stripped identifier scan of `Private/Core/SharedPawn`, `Public/View/Core`, `Private/Core/Animation`, `Public/View/Animation` (8 files) with scope = enclosing function; 0 hits, 0 allowlist entries needed. Scanner self-checks (scope tracking, comments ignored) and a seeded `IsLocallyControlled()` branch detected (negative control).
- Blueprint walk (`ClassParity.Blueprint`): `CBP_SandboxCharacter` and `ABP_SandboxCharacter` loaded read-only (packages asserted not dirty), every graph and node exported by reflection (owned subobjects such as anim property-access bindings included, comments excluded); 0 hits outside the allowlist, 15 references allowed (`SetupCamera` / `SetupInput` reading the pawn's own controller: R9 camera and input binding, owner-client only per spike S14), 0 not assessed, 0 stale entries.
- Negative control (`ClassParity.NegativeControl`): (a) the committed walk of the unmodified sample (17 references: `ABP_SandboxCharacter.Get_AOValue` `IsLocallyControlled`, `CBP_SandboxCharacter.CanSprint` `IsLocallyControlled`, plus the 15 now-allowlisted owner-local reads) fails the judge; (b) a transient Blueprint seeded with an `IsLocallyControlled` call is caught by the walker. The live failing run on the unmodified sample is in `production/qa/evidence/story-004-class-parity-scan/negative-control-run.txt`.
- Fail-closed: an unloadable root, a Blueprint with no graphs, a null graph or node, a missing or empty C++ root, an unreadable or malformed fixture/report are NOT ASSESSED and fail. Watched: the first run reported two header roots NOT ASSESSED (a `FindFilesRecursive` clear-list bug), fixed.
- **NOT ASSESSED (carryover)**: "every SCL tuning key has exactly one value (no per-kind variant)" needs the SCL tuning schema from `story-003-tuning-keys-registered.md` (TDC Exit B, not in this sprint). The test prints the carryover line and claims nothing for it; SCL-003 must add the reflection-dump check.

**Decisions** (logged): Blueprint walk by generic reflection export rather than per-node-type handlers (no node type can be skipped silently); negative control as a committed generated report (the sample `_snapshots/` are untracked) plus a seeded transient Blueprint; shared code in a Foundation test-seam header; forbidden list = ADR-0013 section 1 + `IsBotControlled` + controller/PlayerState types + `Roster*` prefixes; local-control set from spike S14 (`IsLocallyControlled`, `IsLocallyViewed`, `IsLocalController`, `IsLocalPlayerController`); `GetLocalRole` is not forbidden (role, not kind; audited in SCL-005).
**Deviations**: the Blueprint pawn root is `CBP_SandboxCharacter`, still a child of the prototype `AStealthCharacter` (not yet of `AStealthPawn`); its native parent is not in the C++ roots until the reparent Content step. Test names split by scanner (`ClassParity.Source` / `.Blueprint` / `.NegativeControl`) under the planned `StealthGame.Locomotion.ClassParity` prefix.
**Test Evidence**: `production/qa/evidence/story-004-class-parity-scan/test-run.txt` (build `Result: Succeeded`; all `StealthGame.` tests Success), `negative-control-run.txt`, `stamp.txt`
**Run result**: N/A — a static scan and reflection test; nothing player-visible (the player-visible half is SCL-005)
**Reviewed by**: focused code review (self-applied, unattended run): one defect fixed (unneeded `Kismet` editor dependency removed); the `FindFilesRecursive` defect was caught by the fail-closed test
**Readiness**: depends on SCL-003 (Ready, not built): proceeded per the sprint plan, which schedules the scan half only and carries the tuning-key clause; Manifest Version 2026-10-25e -> 2026-10-25f (no 25f rule applies to the scan); lint `ac_below_min` accepted (one compound GDD criterion).
