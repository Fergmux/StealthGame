# Story 032: A disconnect, grace expiry or rejoin never cancels or exposes a concealed pawn instantly

> **Epic**: Concealment (Bushes)
> **Status**: Blocked — BLOCKED: spikes S9 (`production/epics/information-visibility-policy/story-004-spike-s9-cull-latency-and-explicit-channel-close.md`) and S10 (`production/epics/information-visibility-policy/story-005-spike-s10-reopen-behaviour-and-netguid.md`) have not reported; the former-ally cull at the release boundary depends on them Update 2026-10-25: spike S10 reported FAIL (production/qa/spikes/2026-10-25_S10.md) — the in-flight cull/un-cull race can leave a client without the pawn (~1-2 % under loss), the re-created pawn renders one un-evaluated frame, and presentation times need ADR-0002's clock estimator; a superseding ADR to ADR-0003 must decide the fixes before this story can proceed.
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/concealment-bushes.md` (CON-R11, Edge Cases lifecycle, AC-CON-16)
**Requirement**: `TR-con-001`, `TR-con-008`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0017: Concealment component and per-viewer presentation (§3 ownership changes), with ADR-0003 §5.5 (Policy re-evaluates former allies on the Roster's binding event)
**ADR Decision Summary**: A bound-AI hold (disconnect grace, Round R17) never issues Hide; a transition in flight when control passes completes (Enter to Concealed, Exit to Outside); the pawn stays concealed until killed or released. On `ReleaseToCrowd` it is a concealed NPC, culled for former allies at the same boundary (their body was already hidden locally, so nothing pops), and Crowd later exits it through an ordinary `Hide(Exit)` drawn from its NPC dwell distribution. A rejoiner resumes its concealed pawn and learns its state from the owner component.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: The former-ally cull is the Policy's mechanism (IVP `story-030-release-to-crowd-while-concealed.md`); this story observes the Concealment side. S9/S10 verify the close and re-open that the release depends on.

**Control Manifest Rules (this layer)**:
- Required: A transition in flight when control passes completes; a concealed pawn is never instantly un-hidden — source: ADR-0017
- Forbidden: Never issue Hide from a bound-AI hold — source: ADR-0017, Round R17
- Guardrail: Former allies' cull at the same boundary with their marker entry already cleared

---

## Acceptance Criteria

*From GDD `design/gdd/concealment-bushes.md` (AC-CON-16, CON-R11), scoped to this story:*

- [ ] **AC1** (AC-CON-16, [PIE-2] `StealthGame.Concealment.DisconnectRelease`): **GIVEN** a concealed player who disconnects **THEN** the pawn stays concealed under AI hold and issues no Hide; at grace expiry it becomes a concealed NPC culled for former allies at the same boundary; with a deterministic Crowd stub that exits after 20 s, it then exits by an ordinary `Hide(Exit)` with the rustle, never an instant un-hide.
- [ ] **AC2** (derived, CON-R11): **GIVEN** a transition in flight when control passes **THEN** it completes (Enter to Concealed, Exit to Outside); a rejoiner resumes its concealed pawn and learns its state from the owner component (pose None).

---

## Implementation Notes

*Derived from CON-R11 and ADR-0017 §3:*

The AI hold is Round R17's: it never issues Hide, so the in-flight transition simply completes through the post-latch-exempt resolver (story 006). On `ReleaseToCrowd` the Roster binding event drives the Policy's re-evaluation of former allies (IVP story-030); Concealment adds nothing but leaves the state Concealed. Crowd's later exit enters through the same IIF Hide path as any NPC (story 007), with the rustle (story 029). The rejoin check overlaps story 023 AC2 and reuses its fixture.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- IVP `story-030-release-to-crowd-while-concealed.md`: the Policy's former-ally re-evaluation.
- `story-023-owner-and-team-concealed-flags.md`: the owner flag on rejoin.
- Round epic: the R17 grace and forfeit mechanics.

---

## Files

- `Source/StealthTestHarness/Private/Tests/PIE/DisconnectReleaseTest.cpp` (`NETWORK_TEST_CLASS(DisconnectRelease, "StealthGame.Concealment.PIE")`, with a deterministic Crowd stub)
- `Source/StealthGameplay/Private/Tests/Concealment/TransitionSurvivesHandoverTest.cpp` (class `FConcealmentTransitionSurvivesHandoverTest`)

---

## QA Test Cases

*Written by qa-lead at story creation (QL-STORY-READY: ADEQUATE).*

- **AC1**: DisconnectRelease (verbatim AC-CON-16)
  - Given: a Concealed player who disconnects.
  - When: grace expires, with a Crowd stub that exits after 20 s.
  - Then: the pawn stays concealed and issues no Hide; at grace expiry it is culled for former allies at the same boundary; it then exits by an ordinary Hide(Exit) with the rustle and never an instant un-hide.
- **AC2**: TransitionSurvivesHandover
  - Given: a transition in flight when control passes, and a rejoiner.
  - When: control changes.
  - Then: Enter completes to Concealed and Exit to Outside; the rejoiner resumes the pawn and learns its state from the owner component (pose None).

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: the PIE `DisconnectReleaseTest.cpp` and `TransitionSurvivesHandoverTest.cpp` — must exist and pass, OR a documented playtest

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `story-006-round-phase-lifecycle-and-no-dwell.md`, `story-020-fresh-actor-latch-and-ally-hidden-read.md`, `story-023-owner-and-team-concealed-flags.md`, `story-025-cull-push-wiring.md`; IVP S9, S10 spike stories (blockers) and `story-030-release-to-crowd-while-concealed.md`; round-match-framework epic R17 grace stories
- Unlocks: `story-035-prototype-retirement-and-regression.md`
