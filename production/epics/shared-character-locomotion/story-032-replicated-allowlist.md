# Story 032: The pawn's replicated set equals the allowlist, with no multicast RPC

> **Epic**: Shared Character & Locomotion
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: [set by /dev-story when implementation begins]

## Context

**GDD**: `design/gdd/shared-character-locomotion.md` (R13 replicated-property allowlist, R19; AC-SCL-43; spike S1 findings)
**Requirement**: `TR-scl-009`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0003: Replication, relevancy and the concealment cull hybrid (section 8), with ADR-0001
**ADR Decision Summary**: One checked-in allowlist data file (owned by the Visibility Policy, pawn rows authored here) lists every replicated property, owning class, condition, audience and carrier; the build-gate audit enumerates `GetLifetimeReplicatedProps` through each class's rep layout on the class default object and on live instances (components and subobjects included) and fails on any property missing or with a different condition, and on any multicast RPC on the pawn. Forbidden on the pawn: replicated attachments, an `AbilitySystemComponent` or other replicated gameplay-state component beyond the one shared action-state component, and any concealment, team, strike, cooldown or binding field.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Spike S1 measured these non-allowlist replicated properties on the prototype pawn and sample Blueprint: `CBP_SandboxCharacter.CharacterInputState` (`COND_SkipOwner`, set only by the owning client's RPC on walk, sprint, strafe or aim toggles) and `CBP_SandboxCharacter.TraversalResult` (`COND_SimulatedOnly`, set only through the player's traversal RPC). Both must be absent from the production pawn (ADR-0001 amendment recorded in `production/qa/spikes/2026-10-25_S1-S2-S14.md` section 6 item 3). ADR-0003 section 8 also allows `FDeathState` on the pawn, which the GDD AC list omits; the test fixture carries the ADR's list.

**Control Manifest Rules (this layer)**:
- Required: the replicated-property allowlist is enumerated from the rep layout (not hand-listed); the pawn replicates only the allowlist plus the S1 outcome for `Owner`/`Controller`/`PlayerState`.
- Forbidden: no multicast RPC on the pawn; never replicate `IsRunning`, `IsInTraversal` or `CommandedDisplacementCm`; never use the sample's `Traversal_ServerImplementation`/`TraversalResult` replication; never add a gameplay RPC on the pawn.
- Guardrail: pawn network at most 36 KB/s per client.

---

## Acceptance Criteria

*From GDD `design/gdd/shared-character-locomotion.md`, scoped to this story:*

- [ ] **AC-SCL-43** [U] `ReplicatedAllowlist`: GIVEN a reflection dump of the Shared Pawn class and a spawned instance (components and subobjects included), WHEN replicated properties and RPCs are enumerated, THEN the replicated set equals {replicated movement, `HeadLook`, `RemoteViewPitch`, `ActionPose`, `TraversalState`, `AppearanceId`} plus the S1 outcome for `Owner`/`Controller`/`PlayerState`, there are 0 multicast RPCs, and `IsRunning`/`IsInTraversal` are not replicated (R13, R19).

*QA clarification (create-stories 2026-10-25, not a GDD change):* "replicated movement" is expanded to the named engine properties as a fixture; the sample's `CharacterInputState` and `TraversalResult` must not appear; `FDeathState` is also allowed per ADR-0003 section 8; `TraversalState` is provisional pending spike 045.

---

## Implementation Notes

*Derived from ADR-0003 section 8 and the S1 spike:*

- Author the pawn rows of the allowlist data file (`Source/StealthFoundation/Private/Tests/Data/ReplicationAllowlist.json`, row schema `{class, property, condition, audience, carrier, owners[]}`, path fixed by ADR-0008); the Visibility Policy epic owns the file and the audience column, and a row changes only with both owners' sign-off (IVP R11). Enumerate the rep layout on the class default object and on a spawned instance in a test world.
- Remove or neutralise the sample Blueprint's `CharacterInputState` and `TraversalResult` on the reparented pawn: NPCs and players must drive any equivalent through the same command path (ADR-0011) or the pawn derives it server-side for every pawn. This is a Content step (never-resave hazards; edit by explicit path through MCP, with a pre-change snapshot).
- `IsRunning` and `IsInTraversal` remain unreplicated server state (story 013); the owner copies are local.
- **Prototype replaced**: prototype per-feature replicated fields on `AStealthCharacter` (`bConcealed`, `bDying`, `HeldUntilServerTime` observed on the wire by spike S1) are exactly what this audit forbids; the Migration Plan of ADR-0001 replaces them.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 029: pointer shape on the wire (the three pointers).
- Story 030: net-config values.
- The `FActionState`, `FDeathState` and `TraversalState` serialisers (IIF/Assassination epics and traversal stories).

---

## Files

- `Source/StealthFoundation/Private/Tests/Data/ReplicationAllowlist.json` (pawn rows; file owned by the Visibility Policy epic)
- `Source/StealthGameplay/Private/Tests/Locomotion/ReplicatedAllowlistTest.cpp` (planned)
- Content: reparented sample Blueprint edit (explicit path via MCP)

---

## QA Test Cases

*QL-STORY-READY: ADEQUATE (after clarification). Test `StealthGame.Locomotion.ReplicatedAllowlist`.*

Test: replicated allowlist (AC-SCL-43)
  Given: a reflection dump of the Shared Pawn class and a spawned instance, components and subobjects included
  When: replicated properties and RPCs are enumerated
  Then: the replicated set equals the allowlist fixture (the named engine movement properties, `HeadLook`, `RemoteViewPitch`, `ActionPose`, `TraversalState` (provisional), `AppearanceId`, `FDeathState`) plus the S1 outcome for `Owner`/`Controller`/`PlayerState`; the sample's `CharacterInputState` and `TraversalResult` do not appear; 0 multicast RPCs; `IsRunning` and `IsInTraversal` are not replicated
  Edge cases: the movement entry must be the exact engine property names from a fixture; `TraversalState` may change with spike 045

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Locomotion/ReplicatedAllowlistTest.cpp`, test name `StealthGame.Locomotion.ReplicatedAllowlist` — must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/shared-character-locomotion/story-001-pawn-class-default-subobjects.md`, `story-029-pawn-pointer-shape.md`; `production/epics/information-visibility-policy/EPIC.md` (allowlist file ownership)
- Unlocks: `production/epics/shared-character-locomotion/story-020-contact-brush-parity.md` (its "no replicated field added" check reuses this audit)
