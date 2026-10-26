# Story 005: Command sinks: motion/command split, control-epoch check and Halt filter

> **Epic**: Crowd & NPC Behaviour
> **Status**: Complete
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: 2026-10-26

## Context

**GDD**: `design/gdd/crowd-npc-behaviour.md`
**Requirement**: `TR-crowd-002`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-crowd-002 = StateTree per NPC (~4 Hz) + follower emitting Move/Look through the IIF; no path-following driving.)*

**ADR Governing Implementation**: ADR-0016: Crowd AI architecture
**Also relevant**: ADR-0010 (factory-owned control epoch `GetControlEpoch`), ADR-0011 (`UPawnCommandComponent` entry points, `CanBegin`, null reply sink)
**ADR Decision Summary**: Every crowd action leaves through `FCrowdCommandSink`, which calls only the pawn's `UPawnCommandComponent`. A restricted `FCrowdMotionSink` exposes only `Move` and `Look`; neither type has an `Attack` method. Each call checks the drive token's control epoch against the pawn's current epoch and drops a stale call; after Halt only `Move(0)` and `Look` pass.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: Plain C++ wrappers around `TWeakObjectPtr<AStealthPawn>`; no post-cutoff API. The epoch accessor is a factory call introduced by ADR-0010; if its final name differs, follow ADR-0010 and note it for `/architecture-review`.

**Why this story**: The sink is the one boundary that makes "an NPC can do nothing a player could not" provable (GDD CNB-R3, R4, T6). Restricting the type, not a runtime flag, is what makes AC-CNB-11 hold by construction. Read GDD CNB-R3, R10 item 5 and AC-CNB-11, AC-CNB-38.

**Control Manifest Rules (Feature layer, manifest 2026-10-25e)**:
- Required: crowd code reaches a pawn only through `FCrowdCommandSink`/`FCrowdMotionSink`, which call only `UPawnCommandComponent` entry points; every sink call checks the pawn's current control epoch; Halt latches every sink synchronously (ADR-0016)
- Forbidden: `MoveTo*`, `AITask_MoveTo`, `RequestDirectMove`, `AddMovementInput`, `SetActorRotation`, `LaunchCharacter`, CMC velocity writes from Crowd code; never give the NPC sink an `Attack` method (ADR-0016)
- Guardrail: per-call cost is a pointer and integer compare, inside the Crowd `Dispatch` scope

---

## Acceptance Criteria

*From ADR-0016 section 2 and Implementation Guidelines, GDD AC-CNB-11 and AC-CNB-38 (the full-scene gates are stories 013 and 031), scoped to the sink types:*

- [x] `FCrowdMotionSink` exposes only `Move` and `Look`; `FCrowdCommandSink` adds the discrete commands (`Traverse`, `InteractBegin/End`, `PrepareBegin/End`, `Hide`); neither type has an `Attack` method (compile-time or reflection check).
- [x] Every sink call compares the drive token's `ControlEpoch` with `UStealthPawnFactory::GetControlEpoch(Pawn)`; a mismatch (or a stale weak pawn) drops the call and increments `StaleEpochDrop`.
- [x] After the Halt latch the sink passes only `Move(0)` and `Look`, including calls from due-time entries and async callbacks in the same frame.
- [x] Sinks call only `UPawnCommandComponent` entry points; a forbidden-API grep finds no `MoveTo*`, `AITask_MoveTo`, `RequestDirectMove`, `AddMovementInput`, `SetActorRotation`, `LaunchCharacter` or CMC velocity write in Crowd code.

---

## Implementation Notes

*Derived from ADR-0016 section 2:*

- Token: `{TWeakObjectPtr<AStealthPawn> Pawn; uint32 ControlEpoch; bool bHalted;}` (driver kind is the sink's type, never a Roster read). `FCrowdCommandSink` derives from `FCrowdMotionSink` so a restricted driver is a motion sink by construction.
- Continuous `Move(FVector2D, EGait)` and `Look(FRotator)` are never queue items (ADR-0002 section 3); the command component re-applies the last `Move` as a held intent (ADR-0011). Discrete commands enter the command component with a null reply sink.
- Before a discrete command the StateTree task calls the IIF's side-effect-free `CanBegin` with 10 cm / 10 degree margins; a rejection abandons the behaviour and the target is not retried for 10 s (ADR-0016 section 2, dry run). The dry-run helper lives with the sink so tasks cannot skip it.
- Halt filter: `bHalted` is set by the Director's Halt (story 031); this story proves the filter itself.
- The build grep is wired into the ADR-0008 include lint so it runs in CI; comments are excluded.

**Prototype disposition** (`production/first-playable-report.md`, `Source/StealthGame`): Replaces the direct `MoveToLocation` and movement calls in `Source/StealthGame/Private/StealthNpcController.cpp`. The prototype `StealthBot` console driver is a test aid and is not touched.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 031 (`story-031-halt-and-handover-epoch.md`): Halt sequencing and the 100-handover epoch gate (AC-CNB-38)
- Story 013 (`story-013-identity-and-command-path-gate.md`): the 10-minute scene gate (AC-CNB-02, AC-CNB-11)
- The IIF epic (`production/epics/input-interaction-framework/EPIC.md`): the command component itself

---

## Files

- `Source/StealthGameplay/Features/Crowd/Public/Server/CrowdSink.h`
- `Source/StealthGameplay/Features/Crowd/Private/CrowdSink.cpp`
- `Source/StealthGameplay/Public/Server/Core/StealthPawnFactory.h`
- `Source/StealthGameplay/Private/Core/PawnFactory/StealthPawnFactory.cpp`
- `Source/StealthGameplay/Public/Server/Core/StealthPawnControlQuery.h`
- `Source/StealthGameplay/Private/Tests/Crowd/CrowdSinkTest.cpp`
- `tools/ci/lint/crowd-lint.sh`
- `tools/ci/lint/rule-crowd.awk`
- `tools/ci/lint/selftest/run-crowd-lint-selftest.sh`
- `tools/ci/lint/selftest/crowd/`

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY, 2026-10-25).*

- **AC-1**: `StealthGame.Crowd.SinkVocabulary`
  - Given: the `FCrowdMotionSink` and `FCrowdCommandSink` types
  - When: a `static_assert` or reflection check runs
  - Then: the motion sink exposes only Move and Look; the command sink adds discrete commands; neither has Attack
  - Edge cases: inherited or template members; an overload named Attack
- **AC-2**: `StealthGame.Crowd.SinkStaleEpochDrop`
  - Given: a sink holding drive-token epoch E, then the factory epoch bumped
  - When: Move, Look and a discrete command are called, and again with a destroyed weak pawn
  - Then: each call is dropped, `StaleEpochDrop` increments once per call, nothing reaches the pawn
  - Edge cases: bump in the same tick; a matching epoch passes; a null pawn does not crash
- **AC-3**: `StealthGame.Crowd.SinkHaltFilter`
  - Given: the Halt latch is set
  - When: in the same frame Move(v), Look, a due-time entry and an async callback are issued
  - Then: only Move(0) and Look reach the pawn
  - Edge cases: ordering before and after the latch within the frame; relatching is idempotent
- **AC-4**: `StealthGame.Crowd.SinkForbiddenApis`
  - Given: the Crowd source tree and sink calls recorded against a mock command component
  - When: the grep and the call recording run
  - Then: zero hits for the forbidden APIs; every sink output is a command-component entry point
  - Edge cases: comments excluded; empty allowlist

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Crowd/CrowdSinkTest.cpp` — must exist and pass

**Status**: [x] Created and passing (`StealthGame.Crowd.Sink.Vocabulary`, `StealthGame.Crowd.Sink.EpochGate`, `StealthGame.Crowd.Sink.HaltFilter`, `StealthGame.Crowd.Sink.ForbiddenApis`; 2026-10-26)

---

## Dependencies

- Depends on: `production/epics/crowd-npc-behaviour/story-003-crowd-tuning-keys-and-profile-data.md`
- Unlocks: `production/epics/crowd-npc-behaviour/story-006-crowd-controller-and-look-pitch.md`, `production/epics/crowd-npc-behaviour/story-009-follower-shaping-pace-and-stops.md`, `production/epics/crowd-npc-behaviour/story-028-bound-pawn-restricted-driver.md`, `production/epics/crowd-npc-behaviour/story-031-halt-and-handover-epoch.md`, `production/epics/crowd-npc-behaviour/story-034-pipeline-parity.md`

## Completion Notes
**Completed**: 2026-10-26 (Crowd & NPC Behaviour batch, stories cnb-001..018; the production crowd drives L_Prototype, D53)
**Criteria**: 4/4 passing
**Deviations**: The control epoch lives on the pawn factory (bumped before Possess; StealthPawnControl::GetControlEpoch / GetSpawnOrdinal).
**Test Evidence**: `StealthGame.Crowd.Sink.Vocabulary`, `StealthGame.Crowd.Sink.EpochGate`, `StealthGame.Crowd.Sink.HaltFilter`, `StealthGame.Crowd.Sink.ForbiddenApis`; full suite 275/275: `production/qa/evidence/crowd-npc-behaviour-001-018-2026-10-26/full-suite.txt`
**Run result**: 3-player PIE on L_Prototype with the production crowd (40 NPCs, the NpcCount default): `production/qa/evidence/crowd-npc-behaviour-001-018-2026-10-26/evidence.md`
**Reviewed by**: self-review against the code-review skill during the batch; findings and fixes in `production/qa/evidence/crowd-npc-behaviour-001-018-2026-10-26/evidence.md`
