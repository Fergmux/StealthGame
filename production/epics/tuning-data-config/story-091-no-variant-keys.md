# Story 091: AC-15 NoVariantKeys

> **Epic**: Tuning Data & Config
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/tuning-data-config.md`
**Requirement**: `TR-tdc-002`; GDD AC-15 (CR-5)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0004: Tuning data pipeline and validator
**ADR Decision Summary**: The schema is declared once in UPROPERTY metadata and compiled into a generated table; the field name is the canonical key, key names are globally unique, and no per-player, per-team, per-viewer or NPC-versus-player variant of a key may exist (Pillar 1, CR-5).
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: LOW
**Engine Notes**: No post-cutoff API for this story. It reads the generated schema table (`GetTuningSchema()`), which exists in every build; no UPROPERTY metadata is read, so the editor-only metadata caveat (`WITH_METADATA` is `WITH_EDITORONLY_DATA` [V `CoreMiscDefines.h:31`]) does not apply to the key-name check. The plan places the test in the editor module; a test over the compiled table could also live in Foundation. Placement follows the plan and is not re-decided here.

**Control Manifest Rules (this layer)**:
- Required: Every gameplay number is a field of a `_Public` or `_Server` tuning sub-struct; a hardcoded gameplay literal is a defect — source: ADR-0004
- Forbidden: Never give a tuning accessor a pawn, controller, team, viewer or participant parameter — source: ADR-0004
- Required: Every leak, parity, statistical and lint gate includes a negative control the suite detects — source: ADR-0008

---

## Acceptance Criteria

*From GDD `design/gdd/tuning-data-config.md` (AC-15), as amended to make "no variant" operational (amendment 091 REVISED):*

- [ ] **AC-15 (CR-5)**: reflecting the schema finds no key matching `^(Npc|Player|Bot|Human|Team|Viewer)` except the exact allowlist NpcCount, NpcKillStrikeLimit, TeamSize, TeamPositionFeedHz, TeamPositionFeedGridCm, TeamZoneRadiusCm and the prefix `Teammate`; the allowlist is asserted exact (no extra entries), and "no variant" is made operational: no other schema key equals an allowlisted key with a prefix or suffix from {Npc, Player, Bot, Human, Team, Viewer} added or removed. (`StealthGame.Tuning.NoVariantKeys`)

---

## Implementation Notes

*Derived from ADR-0004 section 2 and the GDD AC-15 text:*

The GDD leaves the second clause ("none of these is a per-team or per-viewer variant of another key") to milestone review. The amendment turns it into a mechanical rule; that rule is what this test enforces.

- Test: `StealthGame.Tuning.NoVariantKeys`, class `FTuningNoVariantKeysTest`, file `Source/StealthGameEditor/Private/Tests/Tuning/NoVariantKeysTest.cpp`, flags `EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter` inside `#if WITH_DEV_AUTOMATION_TESTS`.
- Structure the logic as a pure function over a `TConstArrayView<FTuningSchemaRow>` (key names only) so the test can run it on the live `GetTuningSchema()` and on seeded fixture schemas. Findings come back as a list; the test asserts on the list.
- Regex rule: every key matching `^(Npc|Player|Bot|Human|Team|Viewer)` must either be one of the six exact allowlist names or start with `Teammate`. The allowlist is a single constant in the test (exact strings); a test assertion compares it with the literal list in the AC so an extra or removed entry fails ("allowlist exact").
- Variant rule: for each exact allowlisted key K and each token T in {Npc, Player, Bot, Human, Team, Viewer}, form the candidates T+K, K+T, K with a leading T removed (when K starts with T) and K with a trailing T removed (when it ends with T). Reject any candidate that is non-empty, differs from K and is itself a schema key. Whether the same rule also applies to `Teammate`-prefixed keys is not stated in the inputs and is open; apply it to the six exact keys only and record the choice in the test comment.
- Negative control (spec edge case, ADR-0008 s3): a fixture schema containing `PlayerNpcCount` (or `PlayerX` for the regex rule) must be reported, and the test passes only if the control is detected. Use `FStealthNegativeControl` (story 010, under `Source/StealthFoundation/Public/Server/Testing/` per G1) to wrap it.
- Scope note: at this point only owners whose epics have registered sub-structs contribute rows (story 034's pending-owner baseline lists the rest), so the live-schema pass is partial by construction and tightens as owner epics land. The test needs no change when they do.
- Prototype: none. The prototype `UStealthSettings` fields (for example `NpcCount`, `MaxPlayersPerTeam`) are replaced by the schema keys and are not scanned.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/tuning-data-config/story-044-schema-complete.md`: schema completeness, suffix rules and reflection hygiene (AC-3a).
- `production/epics/tuning-data-config/story-066-tuning-service-and-parity.md`: the accessor-parameter check (AC-21).
- `production/epics/tuning-data-config/story-034-doc-lint-schema-row-match.md`: the owner-doc match and pending-owner baseline.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- Source/StealthGameEditor/Private/Tests/Tuning/NoVariantKeysTest.cpp

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: AC-15 key-name allowlist and variant rule
  - Given: the reflected schema keys.
  - When: matched against `^(Npc|Player|Bot|Human|Team|Viewer)`.
  - Then: only NpcCount, NpcKillStrikeLimit, TeamSize, TeamPositionFeedHz, TeamPositionFeedGridCm, TeamZoneRadiusCm and the `Teammate` prefix match; the allowlist is exact (an extra entry fails); no other key equals an allowlisted key with a prefix or suffix from {Npc, Player, Bot, Human, Team, Viewer} added or removed.
  - Edge cases: a seeded `PlayerNpcCount` fails.

---

## Test Evidence

*Governed by `qa.level`: tests are required (qa.level is full).*

**Story Type**: Logic
**Required evidence**:
- Logic: the automation test `StealthGame.Tuning.NoVariantKeys` in `Source/StealthGameEditor/Private/Tests/Tuning/NoVariantKeysTest.cpp` must exist and pass.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-044-schema-complete.md`
- Unlocks: None
- Open verification claims: the negative-control helper from `production/epics/tuning-data-config/story-010-negative-control-helper.md` is used but is not a direct dependency in the plan (confirm it is done); whether the variant rule applies to `Teammate`-prefixed keys is not stated in the GDD or amendments (open); the live-schema pass is partial until owner epics register their rows.
