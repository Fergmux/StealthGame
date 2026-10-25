# Story 004: Anchor, fed position and null-reference interpretation

> **Epic**: Teammate Markers
> **Status**: Ready
> **Layer**: Presentation
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

Where a marker sits depends on what the client can see of the ally. A resolved `PawnRef` gives a tracked marker anchored to the pawn's visual root (the skeletal mesh component's world location, never a head bone, because bone evaluation can be skipped on a hidden mesh, which is exactly the concealed-ally case) plus the anchor height. A null `PawnRef` is ambiguous on a UE client (no pawn, or a pawn not replicated to me), so the rule uses the fallback position feed as the server's explicit signal: null plus a feed entry means `Fed` (never a retirement), null with no feed entry is a retirement candidate. A resolved ref always beats a feed entry. Fed positions are stepped, never interpolated, because an extrapolated position would be a guess drawn as fact. This story adds the feed input to `FMarkerInputs` and the pure anchor and classification logic; staleness is story 005 and the real connection is story 029.

**GDD**: `design/gdd/teammate-markers.md` (TM-R4, TM-R4a, F2)
**Requirement**: `TR-tmk-002`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0021: UI/HUD layering, CommonUI and Enhanced Input (section 5)
**Also governed by**: ADR-0001 (carriers); ADR-0003 (the feed is added in the same update the pawn leaves the connection's scope; its conditional gates S9 and S10 affect only story 029)
**ADR Decision Summary**: The model has anchor sources `Tracking` pawn, `Fed` position and `Held` cached anchor; it reads world positions only for resolved `PawnRef`s of its own C-TeamView entries (the pawn's skeletal-mesh component location) and fallback-feed positions.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: The anchor reads `USkeletalMeshComponent::GetComponentLocation()` through a thin seam so a test can supply a hidden mesh and assert zero bone reads. The simulated proxy's network smoothing is already in that location (Shared Character & Locomotion). On a UE client a replicated reference to an unmapped actor reads as null exactly like a cleared one, which is why the feed is the discriminator.

**Control Manifest Rules (this layer)**:
- Required: The teammate fallback feed is added in the same frame a teammate's channel goes missing, to teammate connections only; every entry carries a `uint8 Seq` — source: ADR-0003
- Forbidden: Never read a pawn field other than its location; never anchor on a bone — source: ADR-0021, ADR-0001
- Guardrail: Zero per-frame heap allocation in the model

---

## Acceptance Criteria

*From GDD `design/gdd/teammate-markers.md` (AC-TMK-20, TM-R4, TM-R4a, F2; the real-connection half of AC-TMK-30 is story 029), scoped to this story:*

- [ ] **AC1** (AC-TMK-20, [U] `AnchorFromVisualRoot`): **GIVEN** a tracked ally whose body is presentation-hidden and moving under root motion, and a fed ally **WHEN** the anchor is computed each frame **THEN** it equals the mesh world location (or the fed feet position) plus `TeammateMarkerAnchorCm`, keeps updating while the body is hidden, and reads no bone.
- [ ] **AC2** (TM-R4a, model half of AC-TMK-30): **GIVEN** an entry **WHEN** its `PawnRef` resolves **THEN** it is `Tracking` and any feed entry is ignored; **WHEN** the ref is null and a feed entry exists **THEN** it is `Fed` and never a retirement; **WHEN** the ref is null and no feed entry exists **THEN** it is a retirement candidate and the hold starts (story 006).
- [ ] **AC3** (TM-R4, F2): **GIVEN** a `Fed` entry **THEN** fed positions are stepped (shown where the server last put them, never interpolated or extrapolated), and the glyph centre is `q = p - (0, g + H/2)` with `g = round(4u)` (4 px at 1080p).

---

## Implementation Notes

*Derived from ADR-0021 §2, §5 and TM-R4, TM-R4a, F2:*

`A = R + (0, 0, h_a)` where `R` is the visual root of a resolved pawn or the fed `Pos` (the pawn's feet; feed `Pos` is feet per IVP R8, CR-2), and `h_a` is `TeammateMarkerAnchorCm` (195 default: stature 175 + headwear cap 7 + 13 clearance). Projection itself is not here: the model hands the world anchor to the leaf, which projects inside `OnPaint` (story 019). `g = round(4 * u)` with `u = H_v / 1080` is the constant pixel gap so the glyph tip never touches the head at long range (2 to 8 px).

The `Tracking / Fed / retirement-candidate` classification is one function of `(PawnRef resolves, feed entry present)`: resolved gives `Tracking`, null with feed gives `Fed`, null without feed gives the hold start. Stated invariant to preserve in a comment: the feed entry and the `PawnRef` are fields of the same C-TeamView entry on the same owner-only PlayerController, and the server adds the feed entry in the same update the pawn leaves the connection's scope, so a client never sees "null, no feed" for a pawn that still exists for longer than one update.

Prototype replaced: the prototype anchors on the character's head position inside `AStealthHUD::DrawTeammateMarkers`; not promoted.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-005-fallback-feed-staleness.md`: the `Stale` state.
- `story-006-lifecycle-retire-hold-takeover-direct-switch.md`: the hold that starts on a retirement candidate.
- `story-029-feed-handoff-and-null-ref-on-real-connection.md`: the real-connection tests (GUID unmapped, scope exit, binding cleared).
- `story-019-paint-markers-in-world-anchor-leaf.md`: projection and drawing.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthPresentation/Private/Markers/MarkerAnchor.h` and `MarkerAnchor.cpp`
- `Source/StealthPresentation/Private/Markers/MarkerTypes.h` (feed entry `{PublicHandle, Pos, Seq}`)
- `Source/StealthPresentation/Private/Tests/Markers/MarkerAnchorTest.cpp` (names `StealthGame.Markers.AnchorFromVisualRoot`, `StealthGame.Markers.NullRefInterpretation`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC1**: Given: a tracked ally with a hidden mesh under root motion, and a fed ally. When: the anchor is computed each frame. Then: anchor = mesh world location (or fed feet position) + `TeammateMarkerAnchorCm` (195) every frame, updates while hidden, and a mock mesh records 0 bone reads. Edge cases: hidden and visible give the same anchor; a non-default anchor value.
- **AC2**: Given: an entry with its `PawnRef` resolved, null with a feed entry, and null with none. When: interpreted. Then: `Tracking` (feed ignored), `Fed` (never a retirement), retirement candidate (hold starts). Edge cases: ref and feed both present gives `Tracking`.
- **AC3**: Given: a `Fed` entry with two successive feed positions. When: frames pass between updates. Then: the position is stepped (no interpolation or extrapolation), and the glyph centre `q = p - (0, g + H/2)` with `g = round(4u) = 4` px at 1080p. Edge cases: `g` at 720p and 1440p.

QL-STORY-READY: ADEQUATE

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthPresentation/Private/Tests/Markers/MarkerAnchorTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/teammate-markers/story-002-marker-model-teamview-source-and-validation.md`; cross-epic `production/epics/information-visibility-policy/story-032-team-fallback-feed-budget.md` (the feed carrier shape)
- Unlocks: `production/epics/teammate-markers/story-005-fallback-feed-staleness.md`, `story-006-lifecycle-retire-hold-takeover-direct-switch.md`, `story-019-paint-markers-in-world-anchor-leaf.md`, `story-029-feed-handoff-and-null-ref-on-real-connection.md`
