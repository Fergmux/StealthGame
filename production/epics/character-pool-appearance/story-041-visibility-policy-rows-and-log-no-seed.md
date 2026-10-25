# Story 041: Visibility rows for AppearanceId and a log with no seed

> **Epic**: Character Pool & Appearance
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/character-pool-appearance.md`
**Requirement**: `TR-cpa-006`, `TR-cpa-003`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-cpa-006 = One look path for every viewer and pawn kind; no keyed material parameters. TR-cpa-003 = Server-private streams from `GetServerSubSeed(AppearanceDeal)` (`FSecretStream`, SipHash counter mode) for Replace; `Take` reserved (no draw).)*

**ADR Governing Implementation**: ADR-0001: Crowd identity rules (with ADR-0012)
**ADR Decision Summary**: The fact `AppearanceId` has audience `All` (`CanViewerKnow` always true for every viewer class); the fact "which participant owns this look" is `ServerOnly` and released only to `PostRound` viewers if Round chooses; the Round Event Log header carries `DealDigest`, the active set, counts, extras and flags, never the root seed or entropy, and cards are `Discarded` at `Reset`.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0001, ADR-0012)
**Engine Notes**: The Information Visibility Policy and Round's Event Log are not coded yet. **AC-1 is DEFERRED until the Visibility Policy exists, or runs against a policy fake; AC-2 can run now** (QL-STORY-READY). A seeded seed value in the log must make the log test fail.

**Why this story**: AC-CP-27 closes the loop between CPA's two facts and the policy table. Read GDD Interactions (Information Visibility Policy row), AC-CP-27.

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: Only `DealDigest`, active set, counts, extras and flags are logged, server-only (ADR-0012)
- Forbidden: Never log, replicate or reflect the root sub-seed, stream state or participant-to-card map
- Guardrail: n/a

---

## Acceptance Criteria

*From GDD AC-CP-27 (the audience half is deferred or faked per QL-STORY-READY):*

- [ ] **AC-1** GIVEN `CanViewerKnow(Viewer, Subject, AppearanceId)` for every viewer class in the Visibility GDD, THEN it is true for all, and the look-owner query is false for every viewer except `PostRound`, where Round releases it; a viewer class added to the enum without a row fails (test `StealthGame.CharacterPool.Policy.AudienceRows`; DEFERRED, or against a policy fake, until the Visibility Policy exists).
- [ ] **AC-2** GIVEN the Round Event Log after a deal and a `Reset`, THEN it contains `DealDigest` and the active set but never the root seed or entropy (a seeded seed value in the log fails the test) and cards are `Discarded` at `Reset` (test `...Policy.LogHasNoSeed`).

---

## Implementation Notes

- The log is a Round-owned, server-only structure; CPA supplies the fields. Use a log fake until Round lands.
- Overlaps information-visibility-policy `story-013-allowlist-content.md`: this story covers the two CPA rows only.

**Prototype disposition**: none.

---

## Out of Scope

- The policy table itself and the allowlist (information-visibility-policy epic); Round's Event Log implementation (round-match-framework epic)

---

## Files

- `Source/StealthGameplay/Private/Tests/CharacterPool/VisibilityRowsTest.cpp`

---

## QA Test Cases

*QL-STORY-READY: GAPS → AC-1 deferred or faked with the qa-lead's proposed text; AC-2 can run now; not re-run (unattended; decision logged).*

**Test: audience rows (AC-1, DEFERRED or fake)**
- Given: `CanViewerKnow(Viewer, Subject, AppearanceId)` for every viewer class and the look-owner query.
- When: each is called.
- Then: `CanViewerKnow` is true for all; the look-owner query is false for every viewer except `PostRound`.
- Edge cases: a new viewer class without a row fails.

**Test: log has no seed (AC-2)**
- Given: the Round Event Log after a deal and a `Reset`.
- When: its contents are read.
- Then: `DealDigest` and the active set present; the root seed and entropy absent; cards `Discarded` at `Reset`.
- Edge cases: a seeded seed value in the log fails the test.

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthGameplay/Private/Tests/CharacterPool/VisibilityRowsTest.cpp` — automation tests `StealthGame.CharacterPool.Policy.AudienceRows`, `...Policy.LogHasNoSeed`

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/character-pool-appearance/story-013-allocator-facade-signature-and-call-order.md`, `production/epics/information-visibility-policy/story-013-allowlist-content.md`
- Unlocks: None
