# Story 016: The victim among reachable bush occupants is an unbiased draw from the server-secret BushSpotPick stream

> **Epic**: Concealment (Bushes)
> **Status**: Complete
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25f
> **Last Updated**: 2026-10-29

## Context

**GDD**: `design/gdd/concealment-bushes.md` (CON-R12, F4 victim formula, AC-CON-14 pick part)
**Requirement**: `TR-con-011`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0017: Concealment component and per-viewer presentation (§7 the pick), with ADR-0006 (seed domain 14 `BushSpotPick`, `UniformBelow`)
**ADR Decision Summary**: The victim is `Reachable[UniformBelow(|Reachable|)]` drawn from `FSecretStream(GetServerSubSeed(BushSpotPick, LayoutAttempt), "CON_SPOT")` keyed by `(ServerTickIndex, AttackerCommandSeq, BushId)`; it replaces the earlier `H(SpotSalt, …) mod |Reachable|`, is unbiased (Lemire multiply-shift with rejection), never uses `RoundSeed`, a public seed or `mod`, and the stream is injectable in tests and never replicated.
**ADR Version**: 2026-10-24 (ADR-0017); ADR-0006 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0017 overall; this story is plain integer code, ADR-0006 risk LOW)
**Engine Notes**: PCG32 integer streams are bit-identical across machines; no engine random facility is used. The `BushSpotPick` domain value (14) was appended by the ADR-0006 amendment.

**Control Manifest Rules (this layer)**:
- Required: The spot pick uses the secret `BushSpotPick` stream with an unbiased ranged draw — source: ADR-0017
- Forbidden: Never use `RoundSeed`, a public seed or `mod` for the bush spot pick — source: ADR-0017, ADR-0006
- Guardrail: One draw per strike; the uniform pick is intended (an entry-order pick would make a watched NPC entrant reliable cover)

---

## Acceptance Criteria

*From GDD `design/gdd/concealment-bushes.md` (AC-CON-14, F4), scoped to this story:*

- [ ] **AC1** (AC-CON-14 pick part, [U] `StealthGame.Concealment.SpotPick`): **GIVEN** 2 reachable candidates and 1,000 fixed injected stream seeds **THEN** each is picked 450–550 times (50 ± 5 %), exactly one victim dies per strike, and `TargetPawn` naming the culled pawn gives the identical victim as `TargetSpot` for the same seed.
- [ ] **AC2** (AC-CON-14 filter-before-pick, [U]): **GIVEN** 2 candidates and an injected reach predicate that rejects one **THEN** the other is picked in 1,000 of 1,000 seeds (filter before pick); the d = 2.34 m hit / 2.36 m whiff and |Δz| boundary assertions belong to Assassination's F1 reach test and are cross-referenced, with a real-F1 integration check deferred until that epic exists.
- [ ] **AC3** (derived, ADR-0006, Manifest Forbidden): **GIVEN** the pick **THEN** it uses `FSecretStream(GetServerSubSeed(BushSpotPick, LayoutAttempt), "CON_SPOT")` keyed by `(ServerTickIndex, AttackerCommandSeq, BushId)` and `UniformBelow`, never `RoundSeed`, a public seed or modulo; the stream is injectable and never replicated.

---

## Implementation Notes

*Derived from ADR-0017 Decision §7 and GDD F4:*

Provide a small helper `PickSpotVictim(Reachable, Stream, Key)` that Assassination calls after applying its F1 reach filter; Concealment tests it with a fake reach predicate. `UniformBelow(n)` is the unbiased index draw (ADR-0006 §6, Lemire multiply-shift with rejection, no `%`); a forced-rejection vector must exercise the retry loop. Two strikes in one tick with different `BushId`s give independent draws. The draw is a function of server-only state, so no client can predict or bias it. `BushVictimMode` (playtest A/B only) selects UniformHash (default), EarliestCommitted or All; only UniformHash is covered here.

**Coverage note**: the AC-CON-14 d = 2.34/2.36 m and |Δz| boundary assertions have no home until the assassination-defence-reversal epic has stories; whoever writes that epic's F1 reach story must carry them, otherwise they are covered nowhere.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-015-spot-zone-and-occupants.md`: the candidate set.
- assassination-defence-reversal epic: F1 reach filter and the resolver that calls the helper.
- Session epic: the secret-stream infrastructure itself.

---

## Files

- `Source/StealthGameplay/Features/Concealment/Private/ConcealmentSpotPick.cpp`
- `Source/StealthGameplay/Private/Tests/Concealment/SpotPickTest.cpp` (class `FConcealmentSpotPickTest`, name `StealthGame.Concealment.SpotPick`)

---

## QA Test Cases

*Written by qa-lead at story creation (QL-STORY-READY: GAPS on AC2 of the first draft; revised as above, re-check ADEQUATE).*

- **AC1**: PickDistribution
  - Given: 2 reachable candidates and 1,000 fixed injected stream seeds.
  - When: the pick runs once per seed.
  - Then: each candidate is picked 450–550 times; exactly one victim dies per strike; TargetPawn naming the culled pawn and TargetSpot give the same victim for the same seed.
  - Edge cases: swapping the candidate order (`CommitSeq`) must not change the distribution.
- **AC2**: FilterBeforePick
  - Given: 2 candidates and an injected reach predicate that rejects one.
  - When: the pick runs across 1,000 seeds.
  - Then: the other candidate is picked 1,000 of 1,000.
  - Edge cases: all candidates rejected gives a whiff with no draw; the real-F1 check is deferred.
- **AC3**: SecretStreamUse
  - Given: the pick helper with a recording stream.
  - When: it is invoked.
  - Then: it uses `FSecretStream(GetServerSubSeed(BushSpotPick, LayoutAttempt), "CON_SPOT")` keyed by (ServerTickIndex, AttackerCommandSeq, BushId); it calls `UniformBelow`; it never reads RoundSeed or a public seed and never uses modulo; the stream is injectable and never replicated.
  - Edge cases: a forced rejection vector exercises the retry loop; two strikes in one tick with different BushIds give independent draws.

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `SpotPickTest.cpp` under `Source/StealthGameplay/Private/Tests/Concealment/` — must exist and pass

**Status**: [x] Created and passing (2026-10-29): StealthGame.Concealment.SpotPick; evidence production/qa/evidence/concealment-001-026-2026-10-29/

Run result: N/A - headless core rule (no player-observable surface of its own); the integrated behaviour is observed in the batch's 3-player PIE run (con-*.png)

---

## Dependencies

- Depends on: `story-015-spot-zone-and-occupants.md`; session-travel `production/epics/session-travel/story-011-server-sub-seed-server-only.md` and `story-008-secret-stream-counter-independence.md`
- Unlocks: `story-033-blind-strike-parity-and-kill-timing.md`

## Completion Notes
**Completed**: 2026-10-29
**Criteria**: AC1-AC3 passing (unbiased draw from the server-secret BushSpotPick stream; filter before pick).
**Deviations**: The pick stays in Assassination's VictimResolverCore (it owns the outcome slot); Concealment supplies the occupant list through IBushSpotQuery, replacing ASN-006's fake bush query.
**Test Evidence**: production/qa/evidence/concealment-001-026-2026-10-29/
**Reviewed by**: self code review 2026-10-29 (APPROVED; notes in the evidence)
