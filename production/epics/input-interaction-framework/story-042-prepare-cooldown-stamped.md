# Story 042: AC 17 PrepareCooldownStampedAtExecution

> **Epic**: Input & Interaction Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/input-interaction-framework.md`
**Requirement**: `TR-iif-005`, `TR-iif-006`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

This story fixes the moment at which the brace (Prepare) cooldown is taken: at execution of `Prepare(Begin)`, not at End. A 0.1 s tap therefore burns the full `DefenceCooldownSec` (60 s), so an unused or early-released brace still costs the cooldown and no one can probe a brace for free. The IIF's part is the ordering: `OnActionStarted(Prepare)` fires once inside the Begin's own slot, which is the stimulus the Assassination epic's subscriber uses to stamp the cooldown; the IIF never stamps or reads the cooldown itself.

It follows story 037 (the hold timer machinery) and prepares story 043 (mashing Prepare cannot reset anything). In ADR-0011 it is section 5, Prepare ("the cooldown is Assassination's, stamped at execution") with section 6 (events fire only inside queue slots) and section 7 (the owner's cooldown end time is Assassination's owner data on the PlayerController). GDD rule: R8 (Prepare). Real stamp verification belongs to the Assassination epic.

**ADR Governing Implementation**: ADR-0011: Pawn command interface and input pipeline
**ADR Decision Summary**: Prepare is a protected window that starts at execution and ends at `End` or `CapEnd`; the cooldown is Assassination's, stamped at execution, and `OnActionStarted(Prepare)` fires inside the Begin's queue slot.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Command path is MEDIUM in ADR-0011's Engine Compatibility; the input-layer HIGH rating does not apply to this story. No V1-V6 gate. Pure server logic over injected time.

**Control Manifest Rules (this layer)**:
- Required: Action-hub events fire only inside queue slots and are native (not Blueprint-assignable) — ADR-0011
- Required: Owner-only data (`FHoldStatus`, rejection codes, owner results, cooldown end) lives on the PlayerController carrier family — ADR-0011
- Required: Hold ends are `ScheduleAt` queue items; the release debounce is server-side and identical for every pawn — ADR-0011
- Forbidden: Never put owner-only data on the pawn — ADR-0011
- Forbidden: Never time a hold on the IIF's own tick, `FTimerManager` or a client timestamp — ADR-0011
- Forbidden: Never carry caller identity in an entry point, payload, event or listener — ADR-0011
- Guardrail: Command interface: stage 2 a few microseconds per frame at the IIF F3 worst case, inside the 0.5 ms queue line — ADR-0011

---

## Acceptance Criteria

*From GDD `design/gdd/input-interaction-framework.md`, scoped to this story:*

- [ ] AC 17 (R8, Logic, U `PrepareCooldownStampedAtExecution`): **GIVEN** a 0.1 s Prepare tap **WHEN** the cooldown is read **THEN** it ends at t_start + `DefenceCooldownSec` (60 s).

---

## Implementation Notes

*Derived from ADR-0011 Decision sections 5, 6 and 7:*

- **Prepare semantics (section 5).** A protected window that starts at execution and ends at `End` or `CapEnd` at `t_start + DefenceDurationSec`; the cooldown is Assassination's, stamped at execution. While `Bracing`, requested Run is clamped (never cancels) and `Traverse` is rejected; an externally forced run or traversal cancels `Moved`.
- **What the IIF owns here.** The IIF's observable is event order: `OnActionStarted(Prepare)` fires exactly once at `t_start`, inside the Begin's slot and before the End resolves; no event or stamp occurs at End time. Assassination subscribes to the hub and stamps `cooldown_end = ServerNow() + DefenceCooldownSec`; the IIF contains no cooldown field, no clock read for cooldown and no `DefenceCooldownSec` use. The cooldown test (`now - LastActivation >= DefenceCooldownSec`) is Assassination's (GDD F-section note).
- **Known weakness (QL-STORY-READY GAPS).** "When the cooldown is read" names no observable in this epic and "0.1 s tap" is ambiguous (held 0.1 s versus same-frame). The qa-lead's proposed reading: the Begin executes at `t_start`, End sent at `t_start + 0.1`; `OnActionStarted(Prepare)` fires once at `t_start`; the Assassination stub's stamped cooldown end equals `t_start + DefenceCooldownSec` regardless of End time. The GDD AC text stays the authority; the real stamp is verified in the Assassination epic.
- **Test seam.** An Assassination stub subscribed to `OnActionStarted(Prepare)` that stamps `cooldown_end = now + DefenceCooldownSec` and counts calls; `DefenceCooldownSec` read from tuning (owned by Assassination, 60). Declared against the story 003 hub; named doubles in story 095.
- **Same-frame tap.** The 0.05 s hard minimum brace gives the same result; the same-frame case itself is story 046.
- **Prototype.** Replaces `OnDefencePressed/Released` and `ServerSetDefence` in `StealthCharacter.cpp`; the prototype keeps the cooldown on the PlayerState (ADR-0011 migration: replaced, not ported).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 043 (`story-043-prepare-mash-no-reset.md`): repeated Begins during an active or cooling brace.
- Story 044 (`story-044-debounce-merge.md`): the release debounce itself.
- Story 046 (`story-046-same-frame-tap-is-short-brace.md`): Begin and End in one mailbox in one frame.
- Story 056 (`story-056-assassination-cooldown-no-reset.md`): nothing else resets the cooldown.
- Assassination epic (cross-epic, stories not yet written): the real stamp, `DefenceCooldownSec` and the `Cooldown` predicate.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- Source/StealthGameplay/Private/Core/PawnCommandCore.cpp (Prepare execution path, event ordering)
- Source/StealthGameplay/Private/Tests/Input/PrepareCooldownStampedAtExecutionTest.cpp (`FInputPrepareCooldownStampedAtExecutionTest`, `StealthGame.Input.PrepareCooldownStampedAtExecution`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation. (At tiers where the QL-STORY-READY gate is skipped — `minimal`, or `lean`/`solo` review mode — no qa-lead specs exist, and this section reads: N/A — no qa-lead specs at this tier; implement against the Acceptance Criteria above.)*

*Test cases not yet defined — run /qa-plan to generate them.*

QL-STORY-READY: GAPS
- The cooldown is stamped and read by Assassination, not the IIF; "when the cooldown is read" names no observable in this epic, and "0.1 s tap" is ambiguous (held 0.1 s versus same-frame).
- QA-lead proposed rewrite (unconfirmed; the GDD AC stays the authority): "GIVEN a Prepare held 0.1 s (Begin executed at t_start, End sent at t_start + 0.1) WHEN the brace ends THEN OnActionStarted(Prepare) fired once at t_start, before the End resolved, and Assassination stub's stamped cooldown end = t_start + DefenceCooldownSec (60 s) regardless of End time." Real stamp verified in the Assassination epic.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Input/PrepareCooldownStampedAtExecutionTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/input-interaction-framework/story-037-hold-completion-epsilon.md`; cross-epic: assassination (stories not yet written) for the real cooldown stamp and predicate; cross-epic: round-match-framework (stories not yet written) for the real resolution queue; cross-epic: tuning-data-config (stories not yet written) for the knobs
- Unlocks: `production/epics/input-interaction-framework/story-043-prepare-mash-no-reset.md`
