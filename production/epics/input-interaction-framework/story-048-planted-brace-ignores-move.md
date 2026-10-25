# Story 048: AC 78 PlantedBraceIgnoresMove

> **Epic**: Input & Interaction Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/input-interaction-framework.md`
**Requirement**: `TR-iif-002`, `TR-iif-006`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

This story implements the planted brace from the IIF side. A bracing pawn neither walks nor turns: `Move` input is accepted (never rejected `NotNow`), ignored for translation and body yaw while the slot is `Bracing`, never cancels the brace and sends no reply; only head-look follows `Look`. The brace ends only through its own rules (End after debounce, cap, forced run, reversal) and after it ends Move works again. The same holds for an AI-issued `Move`, because the human and AI paths enter through the same entry point and the CMC sees the same per-tick input stream from both sources.

It follows story 034 (the brace clamps Run and rejects Traverse), which establishes the Bracing legality rows. The planted behaviour itself (position and body-yaw invariance) is Locomotion's mechanism (SCL R5.2, confirmed by Assassination ASN-R3 / OQ9); the IIF's contract is what it forwards. ADR-0010 is the governing ADR because the forwarding target is `UStealthMovementComponent`'s one input path (section on movement shaping, saved moves, and the published state the IIF reads); ADR-0011 section 1 (`Move`/`Look` entry points, last-intent re-application) and the GDD R7 note define the IIF side. GDD rules: R3, R7 (planted brace note) and R8.

**ADR Governing Implementation**: ADR-0010: Shared Pawn: CMC locomotion, spawn factory and possession parity
**Other ADRs**: ADR-0011: Pawn command interface and input pipeline
**ADR Decision Summary**: Every body is one `AStealthPawn : ACharacter` on `UStealthMovementComponent` (CMC subclass) with one kind-blind input path; movement shaping is one function per rule for every pawn, and the IIF `Move`/`Look` entry points feed that same path for humans (via `ServerMove`) and AI (direct server calls).
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0010 HIGH, ADR-0011 HIGH)
**Engine Notes**: ADR-0010 Knowledge Risk is HIGH (Networking possession parity, CMC prediction). Relevant facts: E8 (CMC override points `ScaleInputAcceleration`, `PhysicsRotation`, `GetMaxSpeed`, and `FSavedMove_Character` compressed flags carry the custom gait data; human Move/Look reach the server only through `ServerMove`). Mover, ChaosMover and NetworkPrediction types are forbidden (E11). Shaping state used by prediction must be saved and restored in `FSavedMove_Stealth` (V item P1 in ADR-0010); that is Locomotion's verification, not this story's.

**Control Manifest Rules (this layer)**:
- Required: Human Move/Look travel only in `ServerMove`; discrete commands only in `AStealthPlayerController::ServerPawnCommand` — ADR-0011
- Required: Movement shaping is one function per rule for every pawn, reads only tuning values, and runs identically on the server and in owner prediction — ADR-0010
- Forbidden: Never add a Move/Look RPC or a Look event — ADR-0011
- Forbidden: Never move a pawn through `RequestDirectMove`/`RequestPathMove` or AI path following; AI movement enters only through `UPawnCommandComponent::Move` — ADR-0010
- Forbidden: Never carry caller identity (`IsAI`, `IsPlayer`, controller, PlayerState, team) in an entry point, payload, event or listener — ADR-0011
- Forbidden: Never read controller kind, PlayerState or team in a movement rule — ADR-0010
- Guardrail: Movement shaping well under 5 % of the 3.0 ms CMC game-thread line — ADR-0010

---

## Acceptance Criteria

*From GDD `design/gdd/input-interaction-framework.md`, scoped to this story:*

- [ ] AC 78 (R7/R8, Logic, U `PlantedBraceIgnoresMove`): **GIVEN** a bracing pawn **WHEN** Move input with non-zero magnitude and a Look turn of 90° are issued for 2 s **THEN** the pawn's horizontal position and body yaw are unchanged, only head-look moves, the brace is not cancelled and no reply is sent; **AND** the same holds for an AI-issued `Move`.

---

## Implementation Notes

*Derived from ADR-0011 Decision sections 1, 4, 5 and GDD R7, adopting ADR-0010's movement path:*

- **IIF contract.** `Move(Direction, Gait)` while the slot is `Bracing` is accepted (stage 2 does not reject continuous input; Move is never a queue item, ADR-0002 section 3) and the pawn's published locomotion path receives a zero translation and a zero body-yaw target; the head-look target follows `Look`, clamped by Locomotion's head clamp read from its data. The brace is not cancelled, no `OnActionCancelled`, no reply. Requested Run is clamped to Walk (story 034).
- **Where the invariance lives.** Position and body-yaw invariance itself (planted brace, SCL R5.2) is implemented and verified in the shared-character-locomotion epic (cross-epic). This story verifies that the IIF forwards zero translation and body-yaw and a head target to the Locomotion stub.
- **Known weakness of the GDD criterion (QL-STORY-READY GAPS).** Position, body yaw and head-look are Locomotion-owned and absent from `ISharedPawnStateView` (which exposes only `IsRunning`, `IsInTraversal`, `CommandedDisplacementCm`), and a plain-C++ Logic test has no CMC; "only head-look moves" has no numeric head clamp. As written the oracle needs a CMC (PIE) or a named stub. The qa-lead proposes asserting on a Locomotion stub (see QA Test Cases). The GDD AC text stays the authority.
- **Test seam missing from the plan, called out by the qa-lead.** A Locomotion stub that records the forwarded move vector, body-yaw target and head target must exist; story 095 owns the named doubles (SharedPawnStateView/Locomotion). Until it lands, declare a test-local stub against the story 002 interface.
- **AI path.** An AI `Move` is held as an intent and re-applied every movement tick (ADR-0011 section 1); the planted rule applies identically because it lives below the entry point (no kind check anywhere).
- **After the brace ends** Move works again; the brace still ends at the cap.
- **Prototype.** New code, no prototype counterpart for the planted rule; the prototype brace (`OnDefencePressed/Released`, `ServerSetDefence`) did not touch movement and is replaced, not ported.
- **Tuning keys.** None owned here; `DefenceDurationSec` 4.0 (Assassination) bounds the brace; the head clamp is Locomotion data.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 034 (`story-034-brace-clamps-run-traverse.md`): Run clamp and Traverse rejection while Bracing.
- Story 047 (`story-047-forced-run-cancels-brace.md`): forced run and traversal cancel.
- Story 085 (`story-085-look-parity-observed.md`) and story 081 (`story-081-look-latency.md`): Look observed on an opponent client and Look latency.
- shared-character-locomotion epic (cross-epic): real position and yaw invariance of a planted pawn.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- Source/StealthGameplay/Public/Server/Core/PawnCommandCore.h (Bracing Move/Look forwarding output)
- Source/StealthGameplay/Private/Core/PawnCommandCore.cpp
- Source/StealthGameplay/Private/Tests/Input/PlantedBraceIgnoresMoveTest.cpp (`FInputPlantedBraceIgnoresMoveTest`, `StealthGame.Input.PlantedBraceIgnoresMove`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation. (At tiers where the QL-STORY-READY gate is skipped — `minimal`, or `lean`/`solo` review mode — no qa-lead specs exist, and this section reads: N/A — no qa-lead specs at this tier; implement against the Acceptance Criteria above.)*

*Test cases not yet defined — run /qa-plan to generate them.*

QL-STORY-READY: GAPS
- Position, body yaw and head-look are Locomotion-owned and absent from `ISharedPawnStateView`, and a plain-C++ Logic test has no CMC; "only head-look moves" has no numeric head clamp. As written the oracle needs either a CMC (PIE) or an unnamed stub.
- QA-lead proposed rewrite (unconfirmed; the GDD AC stays the authority): "...THEN the Move translation and body-yaw targets forwarded to the Locomotion stub are zero for 2 s, only the head-look target follows the Look turn (clamped by the Locomotion head clamp, read from its data), the brace is not cancelled and no reply is sent; position/yaw invariance itself is verified in the shared-character-locomotion epic (cross-epic)."

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Input/PlantedBraceIgnoresMoveTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/input-interaction-framework/story-034-brace-clamps-run-traverse.md`; cross-epic: shared-character-locomotion (stories not yet written) for the planted-brace movement rule (SCL R5.2) and head clamp; cross-epic: round-match-framework (stories not yet written) for the resolution queue
- Unlocks: None
