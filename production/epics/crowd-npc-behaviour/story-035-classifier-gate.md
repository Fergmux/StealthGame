# Story 035: Classifier gate: NPC motion features cannot identify NPCs (playtest)

> **Epic**: Crowd & NPC Behaviour
> **Status**: Blocked — evidence pending playtest (2026-10-27): the offline gate `tools/analysis/crowd_classifier_gate.py` is written and its selftest passes (identical distributions PASS, a 1 SD shift FAILS, < 600 human windows reports INSUFFICIENT DATA); AC-CNB-28 needs >= 600 human 30 s windows from >= 30 playtest participants (no playtest has run) and >= 2,400 NPC windows from the nightly recording of story 034 (blocked). Not met, not waived
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: 2026-10-27

## Context

**GDD**: `design/gdd/crowd-npc-behaviour.md`
**Requirement**: `TR-crowd-011`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-crowd-011 = Follower drives Move/Look through a virtual input layer with device profiles; a recordable input stream is replayable through a bot client for parity tests (CNB-R3.4, F6, AC-CNB-27/28).)*

**ADR Governing Implementation**: ADR-0016: Crowd AI architecture
**Also relevant**: ADR-0023 (telemetry: player tell rates and motion data come from the playtest pipeline), GDD OQ12 (one telemetry pipeline must supply logged player rates, reaction and face-back rates and motion profiles)
**ADR Decision Summary**: Motion profiles are data re-derived per playtest build; the F6 classifier gate (AC-CNB-28) is the check that the profiles make NPC motion unclassifiable against humans. It blocks Alpha and is advisory for MVP.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: No new engine API. The evidence is statistical analysis of recorded windows (held-out balanced accuracy per feature), produced offline from playtest telemetry and the harness's NPC windows.

**Why this story**: The parity gate (story 034) shows the two pipelines match; this gate shows that the profiles in use match what real humans do. It needs real human data, so it cannot close until a playtest has run; it is recorded as ADVISORY for MVP and BLOCKING for Alpha, as in the GDD. Read GDD F6 and AC-CNB-28, and OQ6, OQ12.

**Control Manifest Rules (Feature layer, manifest 2026-10-25e)**:
- Required: profiles are data (`UCrowdMotionProfileData`), replaced without code changes; telemetry recorded server-side only (ADR-0016, ADR-0023)
- Forbidden: adapting rates or profiles at runtime from observed behaviour (offline re-derivation per playtest build, steps capped at 25 %) (ADR-0016)
- Guardrail: analysis is offline; no runtime cost

---

## Acceptance Criteria

*From GDD `design/gdd/crowd-npc-behaviour.md` AC-CNB-28 [Playtest/BLOCKING for Alpha, ADVISORY for MVP], scoped to this story:*

- [ ] (AC-CNB-28) GIVEN at least 600 human 30 s windows of ordinary walking from at least 30 participants and at least 2,400 NPC windows, WHEN each feature's held-out balanced accuracy is computed, THEN every BA is <= 0.55 with a one-sided upper bound <= 0.58; results are kept in `production/qa/evidence/`.
- [ ] The gate is recorded as BLOCKING for Alpha and ADVISORY for MVP in the evidence file header, with the date and the playtest build identifier.

---

## Implementation Notes

*Derived from GDD F6 and AC-CNB-28:*

- Window source: human windows from the playtest telemetry pipeline (OQ12), restricted to ordinary walking (no run, traversal or action); NPC windows from the nightly harness recording (story 034 stream, same feature computation).
- The nine F6 features and the balanced-accuracy definition are in the GDD; implement the analysis as a script under `tools/` (offline), not in game code.
- If a feature exceeds 0.55 the fix is a new `UCrowdMotionProfileData` (data), re-run; never a runtime filter.
- Until human data exists this story stays open; its status is "evidence pending playtest", reported at `/story-done` as not met rather than waived.

**Prototype disposition** (`production/first-playable-report.md`, `Source/StealthGame`): None.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 034: pipeline parity on synthetic streams
- Story 023: tell-rate calibration
- The telemetry epoch (ADR-0023): the pipeline that records human windows

---

## Files

- `tools/analysis/crowd_classifier_gate.py` (offline analysis script)
- `production/qa/evidence/crowd-classifier-gate-<date>.md`

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY, 2026-10-25). Playtest evidence; deferred until human data exists.*

- **AC-1**: `Crowd.ClassifierGate`
  - Setup: at least 600 human 30 s windows of ordinary walking from at least 30 participants, and at least 2,400 NPC windows
  - Verify: compute held-out balanced accuracy per feature
  - Pass condition: every BA <= 0.55 with one-sided upper bound <= 0.58; results kept in `production/qa/evidence/`
- **AC-2**: gate status
  - Setup: the classifier-gate report
  - Verify: inspect the header
  - Pass condition: the gate is recorded as BLOCKING for Alpha and ADVISORY for MVP

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration (playtest): `production/qa/evidence/crowd-classifier-gate-<date>.md` — documented playtest analysis

**Status**: [ ] Script and evidence header created (`tools/analysis/crowd_classifier_gate.py`, `production/qa/evidence/crowd-classifier-gate-2026-10-27.md`); the gate itself is not met until playtest data exists

---

## Dependencies

- Depends on: `production/epics/crowd-npc-behaviour/story-023-tell-calibration-gate.md`, `production/epics/crowd-npc-behaviour/story-034-pipeline-parity.md`
- Unlocks: None
