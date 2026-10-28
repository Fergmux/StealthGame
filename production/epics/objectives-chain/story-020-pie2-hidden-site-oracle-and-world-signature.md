# Story 020: PIE-2 hidden-site oracle and completion world signature

> **Epic**: Objectives Chain
> **Status**: Complete
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25f
> **Last Updated**: 2026-10-28

## Context

**GDD**: `design/gdd/objectives-chain.md` (OC-6, OC-10, OC-11, SP-1; Edge Cases "Pillar-1 leak"; AC-OC-08, AC-OC-12)
**Requirement**: `TR-oc-004` (tend predicate identical at every court), `TR-oc-005` (counted and no-effect completions take the same slot path)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0020: Objectives data and team-view sub-structs (Decision sections 5.2-5.4 parity statement; Validation Criteria)
**Other ADRs**: ADR-0008: Test automation and leak parity (T2 tier, byte-parity assertions); ADR-0011: Pawn command interface (owner replies and `FHoldStatus`)
**ADR Decision Summary**: A counted and a no-effect completion run the same IIF path, write the same `FActionState`, and produce the same replication to every non-team connection; the only extra traffic of a counted completion is the team view on the completing team's own controllers (owner-only relevancy). The hold duration is one value for every court by construction. No fixture, court, pawn or GameState property changes on a completion.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (ADR-0020 MEDIUM; ADR-0008 T2 CQTest harness [U] items)
**Engine Notes**: T2 CQTest PIE (`NETWORK_TEST_CLASS(<Scenario>, "StealthGame.Objectives.PIE")`). Replication tracing for the opponent client uses the harness's tracer (properties, RPCs, spawned actors and components); the ring screenshots follow the project's run-and-observe procedure (`.claude/docs/run-and-observe.md`), captured with the dev-loop window capture.

**Control Manifest Rules (this layer)**:
- Required: The hold duration is one value for every court (`GetInteractHoldSec()` has no court parameter) — source: ADR-0020
- Forbidden: Never let anything change on the court, fixture or pawn when a counted completion happens; never add replication for a counted completion beyond the owner-only team view — source: ADR-0020, GDD Edge Cases
- Forbidden: Never use `AllowNoEffect` at Begin — source: ADR-0020
- Guardrail: parity within +- 1 server tick — source: GDD AC-OC-08

These are the two pillar-1 oracle checks of the epic. If a player tending the hidden final could learn from anything (the owner's reply, the hold's timing, a pose, a sound, a ring colour, or what the world looks like to anyone else) that this court is the live one, the race reduces to a hunt. `HiddenSiteOracle` covers the owner: five tends at five different court roles must be byte-identical in everything the owner receives and identical in timing, with the ring Bone for all five until `TendCueDelaySec` and then Marigold only at the own live site (the one intended signal). `CompletionNoWorldSignature` covers everyone else: an opponent watching must receive nothing Objectives-related, in either a counted or a no-effect case, and the rest of the traffic (pawn pose and movement) must be the same set of fields. The ring screenshots are retained evidence for the one deliberate visual signal.

---

## Acceptance Criteria

*From GDD `design/gdd/objectives-chain.md` (AC-OC-08, AC-OC-12), scoped to this story. QL-STORY-READY revised AC1 (trace format) and AC2 (operational definition of "Objectives-related"):*

- [x] **AC1** (AC-OC-08 [PIE-2] `StealthGame.Objectives.PIE.HiddenSiteOracle`): **GIVEN** a human tending its hidden final, a locked P2, a completed P1, an opponent site and its own live site **WHEN** each tend runs to `D` and a server/owner trace is recorded **THEN** the owner's replies and `FHoldStatus` messages are byte-exact across all five cases (full serialised reply and `FHoldStatus` payload bytes with the pawn id and timestamp fields normalised out), Begin-to-Complete timing agrees within +- 1 server tick, the four non-live cases change no state, and the ring is Bone for all five until `TendCueDelaySec` then Marigold only at the own live site; screenshots (ring only) `production/qa/evidence/oc-tend-{hidden,locked,completed,opponent,live}.png`.
- [x] **AC2** (AC-OC-12 [PIE-2] `StealthGame.Objectives.PIE.CompletionNoWorldSignature`): **GIVEN** a counted completion and a no-effect completion at two courts **WHEN** an opponent client's received replication (properties, RPCs, spawned actors and components) is traced over the 2 s around each **THEN** the opponent receives **zero properties or RPCs whose owning class or struct is on the Objectives list** (`UObjectiveTeamViewComponent`, `FObjectiveTeamView`, any RPC declared in an Objectives header, and any property listed in the ADR-0003 Objectives allowlist rows) in both cases, the remaining traffic (pawn pose and movement) is the same set of **field names** in both (values ignored), and no court, fixture or pawn property changes.

---

## Implementation Notes

*Derived from ADR-0020 Decision sections 5.2-5.4 and the GDD AC-OC-08/AC-OC-12:*

Harness: listen server with 2 clients, graybox layout (or the fixture layout) with a known `FSiteAssignment`. For AC1 one human pawn performs five tends in a shuffled order at: the hidden final (its own team's final, still locked), the locked P2, a completed P1, the opponent's live P1 and its own live P1, each held to `D`. Trace per tend: the owner's reply batch and `FHoldStatus` stream as serialised bytes with pawn ids and server-time stamps normalised out (the QA-lead's operational format); compare all five. Timing: Begin-to-Complete within +- 1 server tick across the five. State: the four non-live tends change no site state (read through the core, or the debug dump) and no team view. The only deliberate difference allowed: the ring's Marigold at the own live site after `TendCueDelaySec`, which is client presentation from the view (story 016's `RingIsMarigold`), not part of the server trace.

Screenshots: capture ring only. Setup: graybox build, two-client PIE, the tender stands at each court type and starts a hold; capture 0.3 s after Begin (the early, Bone frame, `-early` suffix) and 1.5 s after Begin (the late frame) for each: ten captures, the five late frames retained as `oc-tend-{hidden,locked,completed,opponent,live}.png` and the early frames as `oc-tend-<kind>-early.png`. Pass: all five early frames Bone; the late frame Marigold only in `oc-tend-live.png`, Bone in the other four; files retained on disk in `production/qa/evidence/` when the story closes.

AC2 uses the harness tracer on the opponent client around a counted completion (a team tending its live site) and a no-effect completion (a tend at a non-live court), 2 s window each (boundaries +- 1 frame). Definition of "Objectives-related" is the list in AC2 (the QA-lead's operational rewrite): compare by owning class or struct, not by judgement. The "same set of field names" check ignores values and compares the pawn pose and movement fields received by the opponent in the two cases.

Prototype replaced: the prototype replicated each site's `bCompleted`, `InteractorCount` and tending state to every client and the objective actor's `bCompleted` flip was visible to all; the world-signature check is the regression guard against any port of that.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-019-pie2-team-view-ingress-and-informant-parity.md`: team-view contents, ingress acceptance and informant parity.
- `story-016-presentation-view-helpers-and-ring-rule.md`: the ring rule function (unit level).
- IIF epic: the IIF-side reply and timing parity (`story-083-rejection-timing-identity-blind.md`, `story-082-action-pose-parity-observed.md`).

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthTestHarness/Private/Tests/PIE/Objectives/HiddenSiteOracleTest.cpp, CompletionNoWorldSignatureTest.cpp`
- `Source/StealthGameplay/Private/Core/InteractHoldStandIn.cpp` (FHoldStatus to the owner, identical at every court)
- `production/qa/evidence/oc-tend-{hidden,locked,completed,opponent,live}.png and the -early frames, production/qa/evidence/oc-tend-evidence.md`

---

## QA Test Cases

*Written by qa-lead at story creation (QL-STORY-READY: GAPS; the AC1 trace format and the AC2 operational definition were applied above and the specs cover the revised text). The developer implements against these — do not invent new test cases during implementation.*

- **AC-1** (`PIE.HiddenSiteOracle`): five tends indistinguishable to the owner
  - Given: two-client PIE; five tends by one human at: hidden final, locked P2, completed P1, opponent site, own live site; server and owner trace recording (normalised payload bytes)
  - When: each tend runs to D
  - Then: owner replies and `FHoldStatus` messages byte-exact across all five; Begin-to-Complete timing within +- 1 server tick; the four non-live cases change no state; ring Bone for all five until `TendCueDelaySec` then Marigold only at the own live site
  - Edge cases: first tend and last tend order shuffled; high-latency client; screenshots (ring only) per the block below.
- **AC-1 screenshots**: `production/qa/evidence/oc-tend-{hidden,locked,completed,opponent,live}.png`
  - Setup: graybox build, two-client PIE, tender stands at each court type, hold started, capture 0.3 s after Begin (Bone) and 1.5 s after Begin (Marigold check) for each; 10 captures, 5 retained per name for the later frame plus 5 for the early frame under `-early` suffix
  - Verify: ring colour at the early frame and at the late frame for every court type
  - Pass condition: all five early frames Bone; the late frame Marigold only in `oc-tend-live.png` and Bone in the other four; files retained on disk in `production/qa/evidence/` at story close
- **AC-2** (`PIE.CompletionNoWorldSignature`): opponent sees nothing of a completion
  - Given: two-client PIE; opponent client replication tracer (properties, RPCs, spawned actors and components)
  - When: a counted completion and a no-effect completion at two courts; trace the 2 s around each
  - Then: opponent receives zero Objectives-list properties or RPCs in both cases; the remaining traffic (pawn pose, movement) is the same field set in both; no court, fixture or pawn property changes
  - Edge cases: unrelated traffic ignored by the Objectives list but must not differ between cases; trace window boundaries +- 1 frame.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: the two T2 PIE tests under `Source/StealthTestHarness/Private/Tests/PIE/Objectives/` — must exist and pass
- Retained screenshots (ring only): `production/qa/evidence/oc-tend-{hidden,locked,completed,opponent,live}.png` plus the `-early` frames, and the sign-off record `production/qa/evidence/oc-tend-evidence.md`

**Status**: [x] Created and passing (2026-10-28): StealthGame.Objectives.PIE.HiddenSiteOracle, CompletionNoWorldSignature; evidence production/qa/evidence/objectives-chain-013-023-2026-10-28/

Run result: OBSERVED - oc-tend-*.png (Bone at 0.3 s for all five, Marigold at 1.5 s only at the own live court)

---

## Dependencies

- Depends on: `production/epics/objectives-chain/story-007-interact-predicate-and-feint-parity.md`, `production/epics/objectives-chain/story-008-completion-gate-and-stage-effects.md`, `production/epics/objectives-chain/story-016-presentation-view-helpers-and-ring-rule.md`, `production/epics/objectives-chain/story-019-pie2-team-view-ingress-and-informant-parity.md`; cross-epic: `production/epics/input-interaction-framework/story-082-action-pose-parity-observed.md`, `production/epics/input-interaction-framework/story-083-rejection-timing-identity-blind.md`; hud epic (the ring drawing the screenshots capture)
- Unlocks: `production/epics/objectives-chain/story-021-pie2-end-to-end-cacao-chain.md`

## Completion Notes
**Completed**: 2026-10-28
**Criteria**: AC1-AC2 passing. The wire trace of AC2 is the in-process replicated-field recorder (every replicated property of every replicated actor and component on the opponent client, per frame) since the raw-bunch capture is multi-process only; RPCs: no Objectives header declares one.
**Deviations**: court sockets are not replicated (AStealthCourtSocket), so "no court or fixture property changes" holds structurally.
**Test Evidence**: production/qa/evidence/objectives-chain-013-023-2026-10-28/
**Reviewed by**: self code review 2026-10-28 (notes in the evidence)
