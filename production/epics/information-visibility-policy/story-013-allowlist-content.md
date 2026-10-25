# Story 013: The replicated-property allowlist file holds every required row and nothing forbidden

> **Epic**: Information Visibility Policy
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

R11 makes the Policy default-deny at the property level: one checked-in allowlist names every replicated property, its condition and its audience for the Shared Pawn (and its components), `AStealthPlayerState`, `AStealthPlayerController`, each C-TeamView part, the GameState and every other replicated class a client receives. The Policy owns the file, the audience column and the parity tests; Shared Character authors the pawn rows; a row changes only with both owners' sign-off. This story creates the file and its parser and proves its content (the R11 minimum entries with their conditions, a closed world for the pawn, a named deny list, sign-off on every row). The reflection audit (story-015) reads it.

**GDD**: `design/gdd/information-visibility-policy.md`
**Requirement**: `TR-ivp-006` (Single allowlist file, reflection audit and net-config audit on defaults and live instances)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0003: Replication, relevancy and the concealment cull hybrid (section 8); also ADR-0008: Test and automation strategy for leak and parity tests (section 4, allowlist file); ADR-0001: Crowd identity rules
**ADR Decision Summary**: One checked-in JSON allowlist with sections per class family and rows `{class, property, condition, audience, carrier, owners[]}`; the Policy owns the file, Shared Character authors the pawn rows, and a row changes only with both owners' sign-off. Its path is fixed by ADR-0008: `Source/StealthFoundation/Private/Tests/Data/ReplicationAllowlist.json`.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: The file is read by T1 tests and T0 scripts; staging for cooked Development runs is ADR-0008's. Condition names are the engine's (`COND_None`, `COND_OwnerOnly`, `COND_SkipOwner`, `COND_InitialOnly`, `COND_Custom`, ...); `APawn::PlayerState` is private and appears by name.

**Control Manifest Rules (this layer)**:
- Required: **Every new replicated field, fact, audience, condition or replicated class ships with a Visibility Policy row and a signed-off allowlist row**; the reflection audit fails the build otherwise — source: ADR-0001, ADR-0003
- Required: **`AStealthPlayerState` replicates only `PublicHandle` and `PlayerNamePrivate`** — source: ADR-0001
- Forbidden: **Never put team, state, connection, appearance or pawn data on a PlayerState; never give NPCs a PlayerState** — source: ADR-0001

---

## Acceptance Criteria

*From GDD `design/gdd/information-visibility-policy.md` (AC-IVP-42), with the QL-STORY-READY clarification:*

- [ ] **AC-IVP-42** [Unit] `StealthGame.Visibility.AllowlistContent`: GIVEN the allowlist file, WHEN it is parsed, THEN the Shared Pawn section contains every R11 minimum entry with its condition (`PlayerState`, `Controller`, `Owner` as `COND_OwnerOnly`), contains no replicated attachment, no `AbilitySystemComponent`, no second gameplay-state component and no Concealment state, and every row records both owners' sign-off. (R11) Clarification (story-level, QL-STORY-READY): closed-world rule: the Shared Pawn section rows must be a subset of the R11 minimum list; named deny list: bConcealed, bRevealedUntilExit, AbilitySystemComponent, any replicated attachment, any second gameplay-state component.

---

## Implementation Notes

*Derived from the GDD rule R11 and ADR-0003 section 8, ADR-0008 section 4:*

- Create `Source/StealthFoundation/Private/Tests/Data/ReplicationAllowlist.json` with sections for the Shared Pawn (and its components and sub-objects), `AStealthPlayerState`, `AStealthPlayerController`, each `UTeamViewComponentBase` part (Roster part, fallback feed, `FObjectiveTeamView`, `UTeamPingViewComponent`), the GameState and every other replicated actor class a client receives. Row schema `{class, property, condition, audience, carrier, owners[]}`; `owners[]` records both owners' sign-off (Visibility Policy and Shared Character / the carrier's owner).
- Minimum Shared Pawn content (R11): the `AActor` and `ACharacter` sets of R5 held constant, replicated movement, `HeadLook` yaw, `RemoteViewPitch16`, `FActionState`, `TraversalState`, `AppearanceId`, `FDeathState`, and `PlayerState`/`Controller`/`Owner` as `COND_OwnerOnly` (ADR-0001). `AStealthPlayerState` rows: only `PublicHandle` and `PlayerNamePrivate` plus the constant `AActor` rows (ADR-0001 section 4.3 table).
- Deny list for the pawn: `bConcealed`, `bRevealedUntilExit`, `AbilitySystemComponent`, any replicated attachment, any second gameplay-state component beyond the one shared `FActionState` component, any Concealment, team, strike, cooldown or binding field.
- Provide a parser and a T0 parse check under `tools/ci/lint/` (observations only; the gate script decides). The parser is shared with the audit (story-015) and the carrier check (story-014).
- Prototype replaced: `AStealthPlayerState::Team`/`LifeState` replicated `COND_None` (`Source/StealthGame/Private/StealthPlayerState.cpp:12-13`) and `AStealthCharacter` `bConcealed`/`bRevealedUntilExit` (`StealthCharacter.cpp:63-64`) are exactly what this file forbids; nothing is promoted.
- Pawn rows are authored with shared-character-locomotion's sign-off recorded as a row owner by this story (the file records the owners; the SCL epic confirms against `AStealthPawn`).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- story-015: the reflection audit and its negative control
- story-014: carrier/audience consistency against the R4 table
- shared-character-locomotion epic: the pawn class and its real property list

---

## Files

- `Source/StealthFoundation/Private/Tests/Data/ReplicationAllowlist.json`
- `Source/StealthFoundation/Private/Visibility/StealthAllowlistParser.h` / `.cpp` (or a Private test-support header)
- `Source/StealthFoundation/Private/Tests/Visibility/AllowlistContentTest.cpp` (test `StealthGame.Visibility.AllowlistContent`, class `FVisibilityAllowlistContentTest`)
- `tools/ci/lint/` allowlist parse check

*(The module folders do not exist yet; tuning-data-config Exit A creates them.)*

---

## QA Test Cases

- **AC-1**: AllowlistContent (AC-IVP-42)
  - Given: ReplicationAllowlist.json and its parser.
  - When: the file is parsed.
  - Then: the Shared Pawn section contains every R11 minimum entry with its condition (PlayerState, Controller, Owner as COND_OwnerOnly); Shared Pawn rows are a subset of the R11 minimum list (closed world); none of the deny list appears: bConcealed, bRevealedUntilExit, AbilitySystemComponent, any replicated attachment, any second gameplay-state component; every row's owners[] records both IVP and SCL sign-off.
  - Edge cases: a row with only one owner; a deny-listed name added alongside valid rows; a row on the list with the wrong condition. (Tier: T1 plus T0 parse.)

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Visibility/AllowlistContentTest.cpp` (the studio docs' `tests/unit/visibility/`) — must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: None in this epic; tuning-data-config Exit A (module skeleton)
- Unlocks: `production/epics/information-visibility-policy/story-014-carrier-audience-consistency.md`, `production/epics/information-visibility-policy/story-015-replicated-property-audit.md`
