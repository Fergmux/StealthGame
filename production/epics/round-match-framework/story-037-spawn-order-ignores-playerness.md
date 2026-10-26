# Story 037: Spawn order sigma is identical when player flags are flipped and differs with a different secret

> **Epic**: Round & Match Framework
> **Status**: Complete
> **Layer**: Core
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25f
> **Last Updated**: 2026-10-26

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-011` (Single shuffled spawn list in one frame, server-secret order); GDD AC 55; rules R23 (a), R6 P6, formula F5.
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0006: Seeds and deterministic/secret random streams
**ADR Decision Summary**: 128-bit `ServerSecret`, in-project SipHash-2-4 as `H64`, counter-mode secret streams for hidden draws, PCG32 for the public layout stream; domain registry append-only. This story applies the Pillar 1 guard of the `SpawnOrder` domain (ADR-0006 section 4; GDD R23 (a), Session rule 25e): the spawn order is a server-secret shuffle independent of player-ness, so a player is never identifiable by spawn position, and the order differs under a different secret.
**ADR Version**: ADR-0006 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: LOW (ADR-0006 knowledge risk)
**Engine Notes**: No engine API and no post-cutoff API is touched (plain integer SipHash and Fisher-Yates). The ADR-0006 verification item that applies is the golden-vector fixtures on CI; the test uses two fixed fixture secrets from `RoundTestSeeds` and never OS entropy or wall-clock time.

**Control Manifest Rules (this layer)** (Round-relevant rules sit in the Foundation Layer Rules, section Seeds and random streams):
- Required: Every draw comes from `FSecretStream` (hidden decisions, from `GetServerSubSeed`) or `FIntStream` (public geometry and decoration only) (ADR-0006).
- Forbidden: Never log, trace, replicate, put in a URL or expose to Blueprint `ServerSecret`, `MatchNonce`, server sub-seeds or stream keys (only digests and `(Domain, Attempt, StreamTag, counter)` tuples) (ADR-0006).
- Guardrail: Random streams: hidden draws well under 0.05 ms/frame (ADR-0006).

**Prototype disposition**: none specific. The prototype's `FRandomStream`-based randomness (ADR-0006 Alternative 1) is replaced, not ported.

---

## Acceptance Criteria

*From GDD `design/gdd/round-match-framework.md` AC 55, scoped to this story:*

- [ ] GIVEN the same secret and seed with the player flags of the spawn list flipped, THEN an identical sigma; a different secret with the same seed gives a different sigma.

---

## Implementation Notes

*Derived from ADR-0006 and GDD R23 (a), R6 P6, F5:*

- **Input independence.** Round never reads `IsPlayer`, team or participant state when placing, ordering spawns or choosing emergence sockets (GDD R23 (a); Level Socket Contract R5.4). The shuffle's input is a canonically ordered list of `D` entries whose order does not depend on any player flag; the player flag rides along as payload after the permutation is drawn. Flipping the flags of the list therefore cannot change `sigma`.
- **Secret dependence.** `GetServerSubSeed(SpawnOrder, LayoutAttempt)` is keyed by the 128-bit `ServerSecret` (ADR-0006 section 3), so a different secret with the same public `Seed` gives a different `sigma`. The public seed drives only layout geometry, the socket set and client decoration (ADR-0006 guideline 2).
- **Fixture choice.** Choose the fixture secret pair so the two permutations are known to differ (otherwise small `D` could collide). Flip every flag, not one. Both secrets come from the `RoundTestSeeds` fixture constants (story 005), through the fixed-secret seam (story 002).
- **Reuse.** The shuffle function is the one from story 036; this story adds no production code beyond making the player flag a post-shuffle payload if story 036 left it in the shuffle input.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 036 (`production/epics/round-match-framework/story-036-spawn-order-uniform.md`): uniformity of the rank per slot (this story's dependency).
- Story 038 (`production/epics/round-match-framework/story-038-placement-invariant.md`): the F5 zone-share invariant and `GlobalShare`.
- Story 039 (`production/epics/round-match-framework/story-039-spawn-one-frame-and-gate.md`): the one-frame spawn in sigma order.
- Level Socket Contract R5.4 (placement independent of player-ness) and Session rule 25e: owned by their epics.

---

## Files

*Planned files only. Tests live under the engine test roots (`tests/README.md`), not under `tests/`. `/dev-story` replaces this list with the files it actually changed.*

- `Source/StealthGameplay/Private/Round/Start/SpawnOrder.cpp` (shared with story 036)
- `Source/StealthGameplay/Private/Tests/Round/SpawnOrderIgnoresPlayernessTest.cpp` (class `FRoundSpawnOrderIgnoresPlayernessTest`, test name `StealthGame.Round.SpawnOrderIgnoresPlayerness`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: Spawn order sigma is identical when player flags are flipped and differs with a different secret.
  - Given: the same secret and seed, with the player flags of the spawn list flipped.
  - When: sigma is computed for the original and the flipped list, then with a different fixture secret and the same seed.
  - Then: sigma is identical for the original and flipped flags.
  - Then: sigma differs for the different secret.
  - Edge cases: choose the fixture secret pair so the permutations are known to differ, otherwise small D could collide; flip every flag, not one.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Round/SpawnOrderIgnoresPlayernessTest.cpp` (automation test `StealthGame.Round.SpawnOrderIgnoresPlayerness`, class `FRoundSpawnOrderIgnoresPlayernessTest`) — must exist and pass.

**Status**: [x] Created and passing (`StealthGame.Round.SpawnOrderIgnoresPlayerness`, 2026-10-26)

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-036-spawn-order-uniform.md`
- Unlocks: None
- Cross-epic: `production/epics/session-travel` (`GetServerSubSeed` and `ServerSecret`; stories not yet created), `production/epics/tuning-data-config` (module skeleton and StealthTestHarness bootstrap; stories not yet created)

---

## Completion Notes
**Completed**: 2026-10-26 (RMF Exit B batch: stories 030-047, Preparing, spawn order and win rules)
**Criteria**: 1/1 passing (P6 through the real core with every player flag of the NPC slots flipped gives the same sigma; S1 vs S2 with the same seed differ)
**Deviations**: None
**Test Evidence**: `Source/StealthGameplay/Private/Tests/Round/SpawnOrderIgnoresPlayernessTest.cpp`; full suite: `production/qa/evidence/rmf-exit-b-preparing-wins-2026-10-26/full-suite.txt` (189/189 Success)
**Run result**: N/A - a server ordering property (no visible difference by design)
**Reviewed by**: lead-programmer focused review (sonnet, 15 findings, fixes in production/qa/evidence/rmf-exit-b-preparing-wins-2026-10-26/evidence.md) + self-review, RMF Exit B batch
