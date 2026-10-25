# Story 023: The cull predicate equals owner OR ally OR NOT concealed across all 72 combinations (provisional on S9)

> **Epic**: Information Visibility Policy
> **Status**: Blocked — BLOCKED: provisional pending spike story-004 (S9, relevancy override replacing Super and L27 based-on clause); implement after S9 reports. Includes the PlayerController->FViewerId server-side cache and the pure cull predicate (owner or ally or not concealed)
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

A pawn's relevancy to a connection may depend on its concealment state and the viewer's relation, and on nothing else: never on binding, controller kind or state, because any such dependency is a detector for player-ness. This story builds the pure predicate behind `AStealthPawn::IsNetRelevantFor` and the server-side `PlayerController -> FViewerId` cache it reads, and proves by exhaustive truth table that the predicate equals `owner OR ally OR NOT concealed`. It is the logic core that story-024 wires into the engine's relevancy hook, and it is provisional until the S9 spike (story-004) confirms that replacing `Super` entirely behaves as ADR-0003 specifies.

**GDD**: `design/gdd/information-visibility-policy.md`
**Requirement**: `TR-ivp-005` (Relevancy varies by concealment and relation only)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0003: Replication, relevancy and the concealment cull hybrid (section 5.4); also ADR-0001: Crowd identity rules (sections 4, 5); ADR-0008: Test and automation strategy for leak and parity tests (sections 1, 4)
**ADR Decision Summary**: `AStealthPawn::IsNetRelevantFor` replaces `Super` entirely: `Visible` or `CullPending` is relevant; `Culled` is relevant only to the pawn's owner or an ally, else denied; a concealed NPC is denied for every connection. The viewer is resolved from `RealViewer` through a server-side `PlayerController -> FViewerId` cache (never from client input); an unknown viewer is denied for a culled subject.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: ADR-0003 E1 [V]: `APawn::IsNetRelevantFor` returns true for `bAlwaysRelevant`, `RealViewer == Controller`, `IsOwnedBy(ViewTarget/RealViewer)`, `this == ViewTarget`, `ViewTarget == Instigator`, `IsBasedOnActor` in both directions and forwards to a movement base, which is why the override must not call `Super`; E8 [V]: `AActor::IsNetRelevantFor` is virtual. L27 (a pawn standing on a concealed pawn keeps it relevant through the based-on clause) is verified by the S9 spike (story-004). The predicate itself is plain C++ with no `UWorld` dependency (ADR-0007 section 5).

**Control Manifest Rules (this layer)**:
- Required: **`AStealthPawn::IsNetRelevantFor` does not call `Super`**; its only inputs are the Policy's cull record and the viewer's relation — source: ADR-0003
- Forbidden: **Never give a pawn dormancy, a cull distance, `bOnlyRelevantToOwner`, `bNetUseOwnerRelevancy`, a per-instance priority or team relevancy; never change net config per instance, kind or on possession** — source: ADR-0003
- Forbidden: **Never call `ForceNetRelevant()` or `ForceActorRelevantNextUpdate()` on a pawn** — source: ADR-0003
- Guardrail: **Replication**: relevancy override O(1) per pawn-connection pair, inside the 1.5 ms replication line — source: ADR-0003

---

## Acceptance Criteria

*From GDD `design/gdd/information-visibility-policy.md`, scoped to this story:*

- [ ] **AC-IVP-23** [Unit][S9] `StealthGame.Visibility.RelevancyTruthTable`: GIVEN every combination of {bound, unbound} × {PC, AI} × {Active, Informant, AwaitingPawn} × {concealed, not} × {owner, ally, non-ally}, WHEN the cull predicate is evaluated, THEN it equals `owner ∨ ally ∨ ¬concealed`; binding, controller kind and state never change it; a pawn based on a concealed pawn does not keep it relevant (L27); no pawn becomes dormant. (R6)

---

## Implementation Notes

*Derived from ADR-0003 section 5.4 and Technical Setup amendment 2026-10-25b, and ADR-0008 sections 1 and 4:*

- **What to build.** (1) A pure cull predicate in a plain-C++ core taking only the subject's concealment state, the viewer relation (owner, ally, non-ally) and an explicit based-on-concealed input; it returns `owner OR ally OR NOT concealed`. (2) The server-side `PlayerController -> FViewerId` cache maintained at login, rejoin and controller swap (never from client input), with default deny for an unknown viewer on a culled subject. Test name `StealthGame.Visibility.RelevancyTruthTable`, tier T1, class `FVisibilityRelevancyTruthTableTest`.
- **Truth table.** All 72 combinations (2 x 2 x 3 x 2 x 3): {bound, unbound} x {PC, AI} x {Active, Informant, AwaitingPawn} x {concealed, not} x {owner, ally, non-ally}. Assert that binding, controller kind and state never change the result.
- **L27.** The based-on input is an explicit predicate parameter: a pawn standing on a concealed pawn must not keep it relevant (the override never consults `IsBasedOnActor`). Whether the engine's relevancy path honours this when `Super` is replaced is S9's claim (story-004); this story encodes the rule in the predicate.
- **Dormancy.** Assert that no pawn becomes dormant by checking that `NetDormancy` stays unset (`DORM_Never`), per ADR-0003 section 1.
- **Spawn gate (context only).** ADR-0003's Technical Setup amendment adds a per-connection spawn gate checked first in `IsNetRelevantFor`, stored in the same viewer cache. That gate is not part of this criterion's truth table; it composes with the cull by AND and is tested by Session, Round and shared-character-locomotion (LSC AC-33, Round AC 41). Reserve the gate field in the cache design, but do not test it here.
- **Plain-C++ core.** The predicate and cache sit behind a thin facade with no `UWorld` dependency (ADR-0007 section 5, manifest "plain C++ cores"); the engine hook in `AStealthPawn` is wired in story-024.
- **Prototype disposition.** Replaced, never promoted: the prototype replicates `bConcealed` to every client and hides client-side (`Source/StealthGame/Private/StealthCharacter.cpp:63-64`, `UpdateConcealmentVisibility`); it has no relevancy override.
- **Spike gates.** story-004 (S9): this story is provisional until S9 reports; record in the test report whether S9 had reported.
- **Files and modules.** Do not exist yet: created by tuning-data-config Exit A.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- story-024: wiring the predicate into `IsNetRelevantFor`, the cull record and the cull tick (AC-IVP-10)
- story-025: cull timing on the global boundary grid (AC-IVP-11)
- story-004: the S9 spike this story is provisional on
- story-009, story-010: viewer-class computation and default-deny query (this story consumes them)
- shared-character-locomotion epic: `AStealthPawn` net config and the spawn gate

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthFoundation/Public/Server/StealthVisibilityPolicy.h` (viewer cache and cull predicate declarations; additional `Stealth*Visibility*.h` as needed)
- `Source/StealthFoundation/Private/Visibility/` (predicate core and `PlayerController -> FViewerId` cache implementation)
- `Source/StealthFoundation/Private/Tests/Visibility/RelevancyTruthTableTest.cpp` (class `FVisibilityRelevancyTruthTableTest`, test `StealthGame.Visibility.RelevancyTruthTable`)

---

## QA Test Cases

- **AC-1**: Relevancy truth table (AC-IVP-23)
  - Given: all 72 combinations of {bound, unbound} x {PC, AI} x {Active, Informant, AwaitingPawn} x {concealed, not} x {owner, ally, non-ally}.
  - When: the pure cull predicate is evaluated.
  - Then: it equals owner OR ally OR NOT concealed; binding, controller kind and state never change the result; a pawn based on a concealed pawn does not keep it relevant (L27); no pawn becomes dormant.
  - Edge cases: the L27 based-on input is an explicit predicate parameter; dormancy asserted by `NetDormancy` staying unset. (Tier: T1.)

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Visibility/RelevancyTruthTableTest.cpp` (the studio docs' `tests/unit/visibility/`) — must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/information-visibility-policy/story-004-spike-s9-cull-latency-and-explicit-channel-close.md`, `production/epics/information-visibility-policy/story-009-viewer-class-invariants.md`, `production/epics/information-visibility-policy/story-010-query-default-deny.md`; tuning-data-config Exit A (module skeleton)
- Unlocks: `production/epics/information-visibility-policy/story-024-concealment-cull-audience.md`
