# Story 015: The replicated-property audit fails the build on any property missing from the allowlist or with a different condition

> **Epic**: Information Visibility Policy
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

A non-team client must not be able to tell a player pawn from an NPC pawn by any replicated byte, and the cheapest way to break that is to add one replicated property, with one wrong condition, to a pawn, PlayerState, PlayerController, GameState or C-TeamView part. This story builds the build-gate audit that enumerates replicated properties and conditions by reflection and compares them with the checked-in allowlist (story-013), so the next such property fails the build instead of shipping. It protects Pillar 1 (Hide in Plain Sight) at the source, and it feeds the per-commit gate (ADR-0008 section 8) and the AC-IVP-33/44 ship gates through the leak catalogue.

**Scope note (story-level):** the audit machinery enumerates `GetLifetimeReplicatedProps` through the class rep layout on class default objects (CDOs) and on instances spawned in a test world. At the time of this story it runs over the production classes that exist (PlayerState, PlayerController, GameState and C-TeamView parts from participant-state-roster, session-travel and round-match-framework) and over a harness stand-in pawn. The Shared Pawn rows are exercised on `AStealthPawn` when the shared-character-locomotion pawn story lands; that is tracked there, and the shared-character-locomotion epic must add this test to its pawn story's acceptance criteria. The Iris descriptor path (S11) is out of scope while Iris stays pinned off.

**GDD**: `design/gdd/information-visibility-policy.md`
**Requirement**: `TR-ivp-006` (Single allowlist file, reflection audit and net-config audit on defaults and live instances)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0003: Replication, relevancy and the concealment cull hybrid (section 8); also ADR-0008: Test and automation strategy for leak and parity tests (sections 1, 3, 4); ADR-0001: Crowd identity rules (section 4.3 and Validation Criteria carrier audit)
**ADR Decision Summary**: One checked-in allowlist data file, owned by the Visibility Policy with the Shared Pawn rows authored by Shared Character, lists every replicated property a client may receive. The build-gate audit enumerates `GetLifetimeReplicatedProps` through each class's rep layout on the CDO (including Blueprint replication lists) and on live instances spawned in a test world (components, sub-objects, registered sub-object lists), and fails on any property missing from the allowlist or with a different condition.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: ADR-0003 section 8 relies on the replicated-property rep layout being enumerable by reflection on CDOs and on spawned instances, including components, sub-objects and the registered sub-object list (`AActor::bReplicateUsingRegisteredSubObjectList`, ADR-0001 E11 [V]). ADR-0001 E1-E5 and E17 [V] fix the engine default property sets the allowlist must match (`APawn::PlayerState` is private and replicates with no condition by default; `AActor::Owner` is push-based; the 5.8.4 `APlayerState` set is the E17 list). The T1 test runs in a test-created game world under `UnrealEditor-Cmd -nullrhi` (ADR-0008 section 1). The spike results on file (`production/qa/spikes/2026-10-25_S1-S2-S14.md`) already show the RepMap decoding approach in the prototype spike; nothing in this story depends on an unreported spike.

**Control Manifest Rules (this layer)**:
- Required: **Every new replicated field, fact, audience, condition or replicated class ships with a Visibility Policy row and a signed-off allowlist row**; the reflection audit fails the build otherwise — source: ADR-0001, ADR-0003
- Required: **Every leak, parity, statistical and lint gate includes a negative control the suite detects** — source: ADR-0008
- Forbidden: **Never compile negative-control types or test seams into Shipping** (`StealthTestHarness` or `WITH_STEALTH_TEST_SEAMS` only) — source: ADR-0008
- Guardrail: **Tests**: per-commit pipeline ≤ 10 min — source: ADR-0008

---

## Acceptance Criteria

*From GDD `design/gdd/information-visibility-policy.md`, scoped to this story:*

- [ ] **AC-IVP-01** [Unit] `StealthGame.Visibility.ReplicatedPropertyAudit`: GIVEN the checked-in allowlist, WHEN the audit enumerates by reflection the replicated properties and conditions of the Shared Pawn (with components and subobjects from instances spawned in a test world), PlayerState, PlayerController, GameState and every client-received actor class, including Blueprint lists, THEN any property absent from the allowlist or with a different condition fails the build. Negative control: a test-only class with one unlisted replicated property fails. (R11; Iris descriptor path [S11])

---

## Implementation Notes

*Derived from ADR-0003 section 8, ADR-0008 sections 1, 3 and 4, and ADR-0001 Validation Criteria:*

- **What to build.** An audit that, for each class named by an allowlist section, enumerates replicated properties and their conditions through the class's rep layout, first on the CDO (including Blueprint replication lists) and then on live instances spawned in a test-created game world (components, sub-objects and registered sub-object lists included). Any property missing from the allowlist, or present with a different condition, fails. The audit reads the allowlist through the parser delivered by story-013 (row schema `{class, property, condition, audience, carrier, owners[]}` at `Source/StealthFoundation/Private/Tests/Data/ReplicationAllowlist.json`).
- **Classes covered now.** Production classes that exist when this story is implemented (`AStealthPlayerState`, `AStealthPlayerController`, GameState and the `UTeamViewComponentBase` parts) plus a harness stand-in pawn that mirrors the ADR-0001 section 4.1 resets. "Every other client-received replicated actor class" is discovered by reflection over replicating `AActor` subclasses, not by a hand-kept list.
- **Where it runs.** Tier T1 (ADR-0008 section 1): `IMPLEMENT_SIMPLE_AUTOMATION_TEST` (or a spec) inside `#if WITH_DEV_AUTOMATION_TESTS`, in the owning module's `Private/Tests/Visibility/`, class `FVisibilityReplicatedPropertyAuditTest`, name `StealthGame.Visibility.ReplicatedPropertyAudit`, flags `EditorContext | ProductFilter`, headless with `-nullrhi`. The test does no wall-clock waiting and reads no unseeded randomness (ADR-0008 section 4).
- **Negative control (in the GDD criterion; ADR-0008 section 3).** A test-only class with exactly one unlisted replicated property must make the inner audit check fail; the test passes only when that failure is detected, and fails the suite otherwise. The control type lives only in `StealthTestHarness` or in a `#if WITH_DEV_AUTOMATION_TESTS` block of the test file, never in runtime production code, so it cannot ship.
- **Prototype disposition.** Reference only: `Source/StealthGame/Private/Spikes/StealthIdentitySpike.cpp` (RepMap decoding pattern). Replaced, never promoted: the prototype's `AStealthPlayerState` `Team`/`LifeState` at `COND_None` (`Source/StealthGame/Private/StealthPlayerState.cpp:12-13`) and `AStealthCharacter` `bConcealed`/`bRevealedUntilExit` (`StealthCharacter.cpp:63-64`) are exactly the rows this audit exists to reject; they are deleted by their owning stories, not allowlisted.
- **Shared Pawn rows.** Authored by shared-character-locomotion; the audit machinery here must already accept them, but running it over `AStealthPawn` is that epic's pawn story (it adds this test to its acceptance criteria). The ADR-0003 section 8 net-config audit on live instances (AC-IVP-45) is story-054, not this story.
- **Module and folders.** Modules and folders do not exist yet: they are created by tuning-data-config Exit A (module skeleton, compile guard, include lint, ADR-0008 harness, CI). Audit code goes under `Source/StealthFoundation/Private/Visibility/`; the allowlist and fixtures under `Private/Tests/Data/`.
- **Spike gates.** None. Iris descriptor-path reruns (S11) apply only if Iris is ever enabled (story-046 gate).
- **Cross-epic.** The audit's coverage grows as participant-state-roster, session-travel and round-match-framework land their carriers; each of those epics adds its rows with both owners' sign-off.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- story-013: the allowlist file content and its parser (this story consumes them)
- story-054: net-config differential on class defaults and live instances (AC-IVP-45)
- story-019: possession canary (per-commit changelist check)
- story-044: raw-bunch schema parity on the wire (AC-IVP-02)
- story-046: Iris re-run gate
- shared-character-locomotion epic: running this audit over `AStealthPawn`

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthFoundation/Private/Visibility/` (reflection audit machinery; file names chosen by the story)
- `Source/StealthFoundation/Private/Tests/Visibility/ReplicatedPropertyAuditTest.cpp` (class `FVisibilityReplicatedPropertyAuditTest`, test `StealthGame.Visibility.ReplicatedPropertyAudit`)
- `Source/StealthFoundation/Private/Tests/Data/ReplicationAllowlist.json` (read only here; created by story-013)
- Harness stand-in pawn and the negative-control class (harness-only or inside a `WITH_DEV_AUTOMATION_TESTS` block), under `Source/StealthTestHarness/` or the test file

---

## QA Test Cases

- **AC-1**: Replicated property audit with negative control (AC-IVP-01)
  - Given: the checked-in allowlist; production classes that exist plus a harness stand-in pawn, enumerated on CDOs and on instances in a test world, including Blueprint subclasses.
  - When: the audit enumerates replicated properties and conditions by reflection.
  - Then: any property absent from the allowlist or with a different condition fails; negative control: a test-only class with one unlisted replicated property must be detected or the suite fails.
  - Edge cases: components and subobjects; the negative-control class lives only in the harness or a `WITH_DEV_AUTOMATION_TESTS` block; Shared Pawn rows on `AStealthPawn` are tracked in the shared-character-locomotion epic. (Tier: T1.)

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Visibility/ReplicatedPropertyAuditTest.cpp` (the studio docs' `tests/unit/visibility/`) — must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/information-visibility-policy/story-013-allowlist-content.md`; tuning-data-config Exit A (module skeleton, ADR-0008 harness, CI); production carrier classes from participant-state-roster, session-travel and round-match-framework as they land (the audit runs over whichever exist)
- Unlocks: `production/epics/information-visibility-policy/story-044-raw-bunch-schema-parity.md`, `production/epics/information-visibility-policy/story-053-scope-additions-2026-10-25.md`, `production/epics/information-visibility-policy/story-054-net-config-runtime-differential.md`, `production/epics/information-visibility-policy/story-055-clock-exchange-owner-only-audit.md`; shared-character-locomotion epic pawn story (must run this audit over `AStealthPawn`)
