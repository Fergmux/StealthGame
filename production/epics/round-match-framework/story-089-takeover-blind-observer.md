# Story 089: Blind observers cannot identify the taken-over pawn better than chance (exact binomial, alpha 0.05, power 0.8)

> **Epic**: Round & Match Framework
> **Status**: Blocked — BLOCKED: same ADR-0010 spikes as story 088 (S14, H1 and P1 and the OQ1(d) takeover-blend spike have not reported), plus the observer protocol shared with Session AC 46. Unblocks when those spikes report, story 088 is Done and the shared observer protocol is fixed with the session-travel epic; then re-check the story with `/story-readiness`.
> **Layer**: Core
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25f
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-007` (Disconnect policy: blend hand-over, guarded grace timer, forfeit/void); GDD AC 28b ([PIE-3] [SPIKE]); rules R17, R23 (e).
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0010: Shared Pawn: CMC locomotion, spawn factory and possession parity
**ADR Decision Summary**: One `AStealthPawn` on CMC, `UStealthPawnFactory` as the only creator and possessor, kind-blind possession and hand-over pass, disconnect never destroys a pawn. The observable requirement behind this story is ADR-0010's purpose for the hand-over blend (section 4.3) and for "disconnect never destroys a pawn" (section 5): a vanishing, appearing, stopping or sprinting pawn would identify the human, so the hand-over must be a Pillar 1 non-event for observers.
**ADR Version**: ADR-0010 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0010 knowledge risk)
**Engine Notes**: Same ADR-0010 Verification items as story 088: **S14**, **H1**, **P1** (and ST1), plus the GDD's OQ1(d) takeover-blend spike. The playtest clips are produced by the same engine path that story 088 measures; no new engine API is used. Evidence is human, not automated, so no [U] item beyond the spikes is touched.

**Control Manifest Rules (this layer)**:
- Required: `PawnLeavingGame` hands the pawn to an `AStealthCrowdController` in the same frame without issuing a command or changing the binding; it unpossesses only during world teardown or seamless travel and does not bump the control epoch (ADR-0010).
- Forbidden: Never destroy the pawn in `PawnLeavingGame`; never destroy-and-respawn on disconnect or takeover - a vanishing/appearing pawn identifies a human (ADR-0010).
- Guardrail: Pawn network: `TraversalState` 7 bytes per change; total within the <= 36 KB/s pawn line (SCL F9) (ADR-0010); no layer guardrail targets a blind-observer playtest.

**Prototype disposition**: None (new documented playtest; no prototype equivalent).

**Where it sits**: Exit B (Core), the human half of the takeover Pillar 1 proof (R23 (e)): story 088 measures kinematics against a statistical baseline, this story asks people. Its evidence is a documented playtest rather than an automated test. Spike-gated and sequenced after story 088.

---

## Acceptance Criteria

*From GDD `design/gdd/round-match-framework.md` AC 28b (revised in `rmf-ac-overrides.md`), scoped to this story:*

- [ ] Before any session the playtest doc declares k, the effect size and n (smallest n with exact-binomial power >= 0.8 at that effect size, alpha 0.05); the counts of correct identifications are recorded and the one-sided exact binomial result against p = 1/k is p >= 0.05.

---

## Implementation Notes

*Derived from GDD AC 28b and R17, R23 (e); the observer protocol is shared with Session AC 46:*

- **Pre-registration.** Before the first session the playtest doc declares `k` (candidate pawns shown per clip), the effect size (the smallest above-chance identification rate worth detecting) and `n` (the number of observers or trials: the smallest `n` whose exact-binomial power at that effect size is >= 0.8 at alpha 0.05, one-sided). The doc's date must precede the first recorded session; a doc dated after the first session fails the gate.
- **Clips.** Each clip shows `k` candidate pawns, one of which is a taken-over pawn (silent-then-rejoining player, the story 088 scenario); the observer names it. Clips use fixed seeds so they are reproducible. Observers are blind: they are told nothing about which pawn or how takeover looks.
- **Analysis.** Record the number of correct identifications out of the trials. One-sided exact binomial test against `p = 1/k`; pass if the result is `p >= 0.05` (not significantly better than chance). Show the computation in the doc.
- **Shared protocol.** The observer protocol is shared with Session AC 46 (a Session-side blind-observer test); keep one protocol document or cross-reference it so the two epics do not diverge. Open: confirm the shared protocol and who owns the document against `design/gdd/session-travel.md` AC 46.
- **Evidence.** A documented playtest in `production/qa/evidence/` (file name chosen when written, for example `round-takeover-blind-observer-playtest.md`; it holds the pre-registration, the raw counts and the exact-binomial result). Use the project's playtest report template (`/playtest-report`) for the structure.
- **Gate.** Blocked until the same spikes as story 088 report and story 088 is Done (it supplies the clips' behaviour and the baseline). `/story-readiness` re-checks.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 088 (`production/epics/round-match-framework/story-088-takeover-kinematics.md`): the automated kinematic trace test (this story's dependency).
- Story 087 (`production/epics/round-match-framework/story-087-takeover-no-pawn-property-delta.md`): the property-delta test.
- Session AC 46 and the session-travel epic's own blind-observer test (a related but separate test sharing the protocol).
- Any change to the takeover blend itself (Shared Character and crowd-npc-behaviour epics); this story only measures it.

---

## Files

*Planned files only. Tests live under the engine test roots (`tests/README.md`), not under `tests/`. `/dev-story` replaces this list with the files it actually changed.*

- `production/qa/evidence/round-takeover-blind-observer-playtest.md` (documented playtest: pre-registration, counts, exact-binomial result; name planned, `/dev-story` confirms)
- `Source/StealthTestHarness/Private/Round/TakeoverBlindObserverTest.cpp` (optional clip-generation helper with fixed seeds, CQTest `NETWORK_TEST_CLASS(TakeoverBlindObserver, "StealthGame.Round.PIE")`; the evidence itself is the playtest doc; the GDD scenario name is `StealthGame.Round.TakeoverBlindObserver`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: Blind observers identify the taken-over pawn no better than chance, by a one-sided exact binomial against p = 1/k.
  - Given: a playtest doc that declares k, the effect size and n before any session (n = smallest with exact-binomial power >= 0.8 at that effect size, alpha 0.05).
  - When: n observers each identify the taken-over pawn among k candidates per clip, and the counts of correct identifications are recorded.
  - Then: the one-sided exact binomial result against p = 1/k is p >= 0.05.
  - Edge cases: k, effect size and n are written to the doc before the first session, and a doc dated after the first session fails the gate; the clips use fixed seeds; evidence is the documented playtest in `production/qa/evidence/`; the story stays blocked on the same spikes as story 088 and on Session AC 46's observer protocol.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: a documented playtest in `production/qa/evidence/` (planned `round-takeover-blind-observer-playtest.md`) with the pre-registration dated before the first session - must exist and show p >= 0.05. No automated test is required; any helper test follows the CQTest exemption in `.claude/rules/test-standards.md`.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-088-takeover-kinematics.md`
- Unlocks: None
- Cross-epic: `production/epics/shared-character-locomotion` (the hand-over blend under test; stories not yet created), `production/epics/crowd-npc-behaviour` (takeover blend and crowd baseline; stories not yet created), `production/epics/session-travel` (the observer protocol shared with Session AC 46; stories not yet created)
