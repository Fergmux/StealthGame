# Story 095: Retire the eight canonical Config fields of UStealthSettings

> **Epic**: Tuning Data & Config
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/tuning-data-config.md`
**Requirement**: `TR-tdc-009`; GDD CR-13 (No back doors) and the Cross-References migration order (no AC id)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0007: Code modules and include boundaries (section 8, prototype promotion rule; the tuning accessor itself is ADR-0004)
**ADR Decision Summary**: Nothing in the prototype `Source/StealthGame` module is promoted into a production module before its governing ADR is built against. Promotion is a rewrite, not a file move, and `UStealthSettings` (`Config` properties in `DefaultGame.ini`) violates TDC CR-13 and is replaced, not migrated.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: ADR-0007 is MEDIUM (5.8 C++20 floor, `UE_LOGF`); nothing in this story uses a post-cutoff API beyond that. The acceptance check runs a PIE listen server plus one client, which depends on the ADR-0008 T2/PIE environment (CQTest headless is [U], story 019). The retained screenshot is a normal windowed PIE capture, not a headless one (`.claude/docs/run-and-observe.md`: `HighResShot <size> filename=<abs path>`, size before `filename=`, no `Quit` in the same `-ExecCmds`).

**Control Manifest Rules (this layer)**:
- Forbidden: Never promote prototype code (`Source/StealthGame`) into a production module before its governing ADR is Accepted; promotion is a rewrite, not a file move (`UStealthSettings` and `StealthRules` are replaced) — source: ADR-0007
- Forbidden: Never mark tuning types Config/DefaultConfig/GlobalUserConfig/PerObjectConfig; never read a gameplay value from `UDeveloperSettings`, ini, `Saved/Config`, command line (Shipping) or a console variable — source: ADR-0004
- Required: Server code reads only `GetServerTuning()`/`FValidatedTuning`; client and presentation code reads only `GetClientTuning()` — source: ADR-0004

---

## Acceptance Criteria

*From GDD CR-13 and the Prototype Disposition, as amended (amendment 095 REVISED):*

- [ ] With the fields mapped to the eight canonical keys removed from `UStealthSettings` and their `[/Script/StealthGame.StealthSettings]` ini entries deleted, in a PIE listen-server plus one client session the round reaches Active, `GetServerTuning()` returns the Default values of the eight canonical keys, a screenshot at Active is retained in `production/qa/evidence/`, and the 12 prototype tests stay green.

---

## Implementation Notes

*Derived from the TDC GDD migration order ("add the asset and schema with current values; add the accessor; convert call sites; remove the migrated properties and the ini section"; removal is a gate before any external playtest), ADR-0007 s8 and ADR-0004 section 6:*

This is the CR-13 gate story: it removes the prototype's live back door for the eight canonical keys. The values are re-declared as schema defaults pinned by test (stories 040 and 044), never migrated. The remaining `UStealthSettings` fields stay until their owner epics replace them, so the class is not deleted here and no file is deleted (deletion is an always-ask category, ADR-0007 s8).

- Field mapping (read from `Source/StealthGame/Public/StealthSettings.h` at planning time; re-read it before editing): `TeamSize` from `MaxPlayersPerTeam`, `NpcCount` from `NpcCount` (prototype 40, schema Default 50), `AssassinationRangeCm` from `AssassinationRange`, `DefenceDurationSec` from `DefenceDuration`, `DefenceCooldownSec` from `DefenceCooldown`, `NpcKillStrikeLimit` from `NpcKillLimit`, `ObjectiveInteractSec` from `ObjectiveInteractTime`. No obvious prototype counterpart for `TargetRoundMinutes` was found at planning time; the GDD calls it non-binding with no timer, so confirm there is nothing to remove. This mapping is an observation, not a decision recorded in an input; verify each by reading the header.
- Reader sites found by grep at planning time (rerun the grep, it must be repeated because the list is not exhaustive): `Source/StealthGame/Private/StealthCharacter.cpp` (objective progress, assassination reach), `StealthGameMode.cpp` (team assignment, NPC count, strike reach, NPC-kill removal), `StealthHUD.cpp` (civilian-kill counter), `StealthObjective.cpp` (interaction time) and any reader of the same fields in `StealthNpcController.cpp`, `StealthPlayerController.cpp` and `StealthMovementComponent.cpp`. Convert each server reader to `GetServerTuning()` and each client or HUD reader to `GetClientTuning()` for the keys whose audience is `All` (every key above except `NpcCount` and `TargetRoundMinutes`, which are `ServerOnly`). Remember the client face returns nullptr (invalid) before the snapshot is valid; never substitute a local default.
- The prototype GameMode drives its own round, so it must call `BeginResolve()` on its Preparing-equivalent entry (ADR-0004 s6) and treat `OnTuningFailed` as a failed round. The Round epic does not exist yet; keep this call site minimal and remove nothing else.
- Behaviour change to expect and not treat as a regression: the Default `NpcCount` is 50 (prototype 40), and the hard ranges are tighter than the prototype's clamps (GDD: cooldown minimum 30 versus 1, range minimum 100 versus 50, interaction minimum 2.0 versus 0.5, `NpcCount` 6 to 100 versus 0 to 200). The 12 prototype tests (6 `StealthGame.Rules.*`, 6 across `StealthGame.EditorTools.*`) must stay green unchanged (the `Rules` tests call `FStealthRules` functions with explicit arguments; the EditorTools specs were not inspected for this plan, so check them); if one needs a change, stop and report.
- Ini cleanup: delete the eight keys' lines under `[/Script/StealthGame.StealthSettings]` in `Config/DefaultGame.ini`. At planning time that section held only `CharacterClass` and `AppearanceMeshes` lines (none of the eight), so the deletion may already be vacuous. Check `Config/*.ini` and `Saved/Config` overrides; leave the section and the remaining fields untouched.
- Observation (retained screenshot): launch a PIE listen server plus one client, wait for the round to reach Active, and capture with `HighResShot 1280x720 filename=<abs path>` (or `tools/dev/capture-window.ps1`) to `production/qa/evidence/<date>_tdc-canonical-keys-prototype-round.png`. The file must exist and be non-empty. Do not mass-save assets; no asset change is needed here.
- If the PIE environment cannot run headless, this story is not headless anyway (a windowed observation); the PIE fallback of story 019 does not weaken it.
- Prototype: this story REPLACES the canonical-key fields of `Source/StealthGame/Public/StealthSettings.h` (not migrated). The other `UStealthSettings` fields stay until their owner epics replace them.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/tuning-data-config/story-066-tuning-service-and-parity.md`: the accessor `UStealthTuningService` this story reads through.
- `production/epics/tuning-data-config/story-058-tuning-store-default-preset.md`: the store and Default resolution.
- `production/epics/tuning-data-config/story-040-canonical-keys-and-faces.md`: declares the eight keys and their defaults.
- `production/epics/tuning-data-config/story-092-no-hardcoded-gameplay-scan.md`: the literal scan (does not cover the prototype module).
- Remaining `UStealthSettings` fields (other tuning, the character pool): replaced by their owner epics.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- Source/StealthGame/Public/StealthSettings.h
- Source/StealthGame/Private/StealthGameMode.cpp (and the other prototype reader sites found by grep)
- Config/DefaultGame.ini
- production/qa/evidence/<date>_tdc-canonical-keys-prototype-round.png

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: canonical fields retired and the prototype round still plays
  - Given: the eight canonical fields and their `[/Script/StealthGame.StealthSettings]` ini entries removed; a PIE listen server plus one client.
  - When: the round runs.
  - Then: the round reaches Active; `GetServerTuning()` returns the Default values of the eight canonical keys; a screenshot at Active is retained in `production/qa/evidence/` (file exists, non-empty); the 12 prototype tests are green.
  - Edge cases: a missed reader site found by grep; PIE fallback per story 019.

---

## Test Evidence

*Governed by `qa.level`: tests are required (qa.level is full).*

**Story Type**: Integration
**Required evidence**:
- Integration: the PIE listen-server plus one-client observation, with the retained screenshot `production/qa/evidence/<date>_tdc-canonical-keys-prototype-round.png` (documented playtest), and the 12 prototype tests passing in the per-commit gate.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-066-tuning-service-and-parity.md`, `production/epics/tuning-data-config/story-058-tuning-store-default-preset.md`
- Unlocks: None
- Open verification claims: the field-to-key mapping and reader-site list are planning-time observations to re-verify; whether `TargetRoundMinutes` has any prototype field is unconfirmed; the ini entries may already be absent.
