# Story 047: Ten-player real-network session and sign-off

> **Epic**: Procedural City Generator
> **Status**: Blocked — BLOCKED: spikes R1 and R4 (generator-scale, stories 041-043) and the net-emulation and traversal spike (`shared-character-locomotion/story-045-net-emulation-traversal-spike.md`) have not reported, and story 046 (the 10-connection Gauntlet pass on one machine) is its precondition; a real-network session before the budgets and loss behaviour are known would only rediscover them.
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: epic exit criterion in `production/epics/procedural-city-generator/EPIC.md` ("10-player integration pass, about 6 days: 5v5 over a real network, not in-process PIE"); `production/milestones/M1-mvp-core.md`; `production/estimates/2026-10-25_mvp-rebaseline.md` (pessimistic case: "10-player real-network behaviour differs from in-process PIE"); `design/gdd/procedural-city-generator.md` (R-G7, F5, AC-G24)
**Requirement**: `TR-citygen-003`, `TR-citygen-006`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. This is the human-in-the-loop half of the pass the epic owns by decision: the three core technical risks include "10 humans over a real network (the prototype's 10-player run was in-process)".)*

**ADR Governing Implementation**: ADR-0008: Test and automation strategy for leak and parity tests (ADR-0014 for the layout path measured)
**ADR Decision Summary**: Integration stories carry a T2/T3 test or a documented playtest; evidence is retained; multi-process criteria are never proved in in-process PIE only. A documented playtest with signed evidence is the accepted form for a real-network session, which no automation tier can run.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (ADR-0008; real-network behaviour is the thing being measured)
**Engine Notes**: Ten participants (5v5; bots allowed for non-human slots) on separate machines over a real network (LAN or internet; "not loopback only" is shown by distinct hosts and a measured RTT above 0 in the evidence). Cooked Development or packaged Test build, recorded by build id. No MCP toolset or editor script is part of the evidence path.

**Control Manifest Rules (this layer)**:
- Required: Evidence retained in `production/qa/evidence/`; a session names the build, the seeds and the machine inventory — source: coding-standards, ADR-0008
- Forbidden: Never count in-process PIE or a loopback-only run as this evidence; never close with an unresolved S1 — source: ADR-0008, qa severity scale
- Guardrail: 0 crashes in any retained 10-player round (M1 quality gate)

---

## Acceptance Criteria

*From the epic exit criterion and the M1 milestone, scoped to this story:*

- [ ] **AC1** (epic exit criterion, `RealNetwork.TenPlayerSession`): **GIVEN** 10 participants (5v5; bots allowed for non-human slots) on separate machines over a real network (not in-process PIE, not loopback only) **THEN** at least 3 consecutive rounds on fresh generated seeds complete to `Ended`, every client reports the host's `LayoutHash`, and the briefing shows no reseed cue (with at least one round that reseeded, attempt > 0, so "no reseed cue" is not vacuous).
- [ ] **AC2** (`RealNetwork.DifferencesRecorded`): **GIVEN** the session evidence document and the defect log **THEN** every S1 defect is resolved, every S2 defect is resolved or ruled by the producer with a logged reason and date, and the differences from in-process PIE are recorded in a fixed table (Manifest arrival time, traversal replication under real loss, join order, Preparing duration) with the measured value and unit in every cell.
- [ ] **AC3** (sign-off): **GIVEN** the evidence document in `production/qa/evidence/` **THEN** it names the build id, the seeds and the machine inventory and carries qa-lead and producer signatures with dates.

---

## Implementation Notes

*Derived from the epic exit criterion and the QL-STORY-READY review:*

Run after story 046 passes. The counts (>= 3 consecutive rounds, one with attempt > 0) are a proposal: no source figure exists (logged decision); the round count covers round restart and the layout redraw without a used seed. A round that reseeds can be provoked with a seed known to fail attempt 0 (from the batch audit, not a golden seed: golden seeds are test data). The evidence document records per client: the `LayoutHash`, Manifest arrival time (seconds from `RequestLayout` to that client's ack), join order (the observed connection sequence), Preparing duration (seconds), and a traversal-replication note under the observed loss; the table's metric per row is fixed in its header before the session. Defects use the QA severity scale: an S1 must be fixed before a build goes out and the M1 gate requires 0 crashes, so an S1 cannot be ruled; an S2 may be ruled by the producer with a logged, dated reason. State how bot machines are counted in the inventory. The PROCEED/PIVOT D38 measurement is taken in the playtest block of the M1 milestone, not here, though the same rounds may feed it.

Prototype replaced: the prototype's 10-player functional run (in-process, seed 13135) is replaced as the integration proof by stories 046 and 047.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-046-ten-connection-gauntlet-integration-pass.md`: the automated one-machine run.
- The PROCEED/PIVOT D38 measurement: M1 milestone / HUD epic exit evidence.
- Fixing defects found: stories raised through `/bug-report` against their owning epics.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `production/qa/evidence/[date]_citygen-10player-real-network.md` (evidence document, defect log, differences table, signatures)
- `production/qa/evidence/[date]_citygen-10player-real-network-*.png` and `.json` (screenshots and any harness logs)

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: AC1 and AC3 ADEQUATE; AC2 revised and ADEQUATE).*

- **AC1** `RealNetwork.TenPlayerSession`
  - Given: 10 participants (5v5, bots allowed) on separate machines over a real network, with the build ID recorded.
  - When: 3 consecutive rounds run on fresh generated seeds.
  - Then: every round reaches `Ended`. Every client's `LayoutHash` equals the host's. The briefing shows no reseed cue.
  - Edge cases: to keep "no reseed leaking" from passing vacuously, require >= 1 round with attempt > 0. State how bot machines are counted, and how "not loopback only" is shown (distinct hosts, measured RTT > 0).
- **AC2** `RealNetwork.DifferencesRecorded`
  - Given: the session evidence document and the S1/S2 defect log.
  - When: the document is checked.
  - Then: every S1 is resolved. Every S2 is resolved, or has a producer ruling with a logged reason and date. The table has four rows (Manifest arrival time, traversal replication under real loss, join order, Preparing duration), each with a measured value and unit in every cell.
  - Edge cases: fix the metric per row in the table header, for example arrival time as seconds from `RequestLayout` to ack per client, and join order as the observed connection sequence. An S1 ruled instead of fixed fails the criterion.
- **AC3** sign-off
  - Given: the evidence document in `production/qa/evidence/`.
  - When: the sign-off is checked.
  - Then: it names the build ID, the seeds and the machine inventory, and carries qa-lead and producer signatures with dates.
  - Edge cases: a missing field or an unsigned document means incomplete.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: a documented playtest, `production/qa/evidence/[date]_citygen-10player-real-network.md`, signed by qa-lead and producer, with the retained screenshots

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/procedural-city-generator/story-046-ten-connection-gauntlet-integration-pass.md`; cross-epic: all Foundation, Core and Feature epics' Done state; `shared-character-locomotion/story-055-net-emulation-pass-exit.md`
- Unlocks: the epic's Definition of Done (exit criterion) and the M1 "playable 10-player round" criterion evidence
