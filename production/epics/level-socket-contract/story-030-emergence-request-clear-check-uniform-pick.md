# Story 030: Emergence — kind-blind request, clear check and uniform pick

> **Epic**: Level Socket Contract
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/level-socket-contract.md`
**Requirement**: `TR-lsc-006`
*(At write time: TR-lsc-006 = Shared emergence sockets; `PickRespawnPawn` of an unseen crowd pawn)*

**ADR Governing Implementation**: ADR-0014: Layout service, seeded generation and runtime navmesh
**ADR Decision Summary**: `RequestEmergence(Kind, RequestSeq)` (served only in `Live`) uses one procedure for every kind: the clear check reads pawn positions from a server spatial hash against each socket's precomputed dilated W(O) cell set and ExitPoint radius (no run-time traces), and selection is uniform among clear sockets from `FSecretStream(GetServerSubSeed(EmergencePick, RequestSeq), "LSC_EMRG")`, counter-based and identity-blind.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: No engine API; pawn positions arrive through the injected `ILayoutPawnQuery` (Crowd's 2 m hash, ADR-0016). The service returns a socket; Crowd spawns through the pawn factory.

**Why this story**: One shared way into and out of the crowd for NPCs is the core of the "no spawn tell" promise. Read first: R5 items 1, 3-4, R9 item 1, AC-7, AC-9, AC-10.

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: Draw sites use ADR-0006 shapes (counter-as-Attempt per `RequestSeq`) and the `LSC_EMRG` tag (ADR-0014)
- Forbidden: Selection never reads `IsPlayer`, team or participant state; `%` on a draw (GDD R5.4, ADR-0006)
- Guardrail: layout runtime services at most 0.1 ms average in `Active` (ADR-0014)

---

## Acceptance Criteria

*From GDD AC-7, AC-9 and AC-10, scoped to this story:*

- [ ] GIVEN one `RequestEmergence` of each kind (Replacement, AmbientEntry, AmbientExit, PawnRecovery), THEN pose, animation, walk-in path, delay rule and eligible sockets are identical; GIVEN a request of kind `InformantRespawn` or `Rejoin`, THEN it is refused and logged and no pawn is created (`Emergence.KindBlind`, `Emergence.RespawnKindsRejected`)
- [ ] GIVEN a socket, WHEN an NPC, a player or an informant stands in W(O) dilated by 2 m, or a pawn is within 3 m of the ExitPoint, or the socket was used under 20 s ago, THEN it is not selected at 0 s, 30 s and 90 s of waiting (never relaxed); GIVEN two same-tick requests for one socket, THEN the lower sequence id takes it and the other re-selects in that tick (`Emergence.ClearCheck.<Npc|Player|Informant>`, `Emergence.SameTickOrder`)
- [ ] GIVEN 5 clear sockets and 5,000 requests with pinned sub-seeds, THEN picks are uniform (chi-square < 13.277, df 4); changing the requester's `IsPlayer`, team or participant state does not change any pick; inserting extra draws in another domain does not change any pick (`Emergence.UniformAmongClear`, `Emergence.BlindToIdentity`)

---

## Implementation Notes

*Derived from GDD R5 and ADR-0014 7.2:*

- Kinds: `Replacement`, `AmbientEntry`, `AmbientExit`, `PawnRecovery`; `Kind` never affects eligible sockets, timing, pose, animation, walk-in path or presentation. No player pawn emerges in normal play: `InformantRespawn` and `Rejoin` are refused and logged (informant respawn uses takeover, story 033). `PawnRecovery` (an Active participant whose pawn was lost without a resolved kill) uses the same clear check.
- Clear check: no pawn **of any kind** inside W(O) dilated by `EmergenceWitnessDilationCm` (2 m); none within `EmergenceClearRadiusCm` (3 m) of the ExitPoint; at least `EmergenceSocketCooldownSec` (20 s) since last use (exactly 20.0 s and 3.0 m are clear). If none is clear the request retries every `EmergenceRetrySec` (1 s) in server sequence order; **never relaxed**.
- Same-tick requests resolve in sequence order; the second re-selects in the same tick.
- Selection: uniform among clear sockets, rejection-sampled, counter-based keyed by request sequence id (least-recently-used was rejected: it makes the next socket predictable).
- The created pawn is Crowd's business; presentation is the no-pop story (032). If the occlusion predicate cannot be met the layout fails (story 016); an effect is never added to hide it.

---

## Out of Scope

- Story 031: kill decoupling and rate bound; story 032: no visible pop; Crowd's schedule and spawning (`production/epics/crowd-npc-behaviour/EPIC.md`)

---

## Files

- `Source/StealthGameplay/Private/Core/Layout/EmergenceService.cpp`, `EmergenceService.h`
- `Source/StealthGameplay/Private/Tests/Layout/EmergenceRequestTest.cpp`

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE).*

- **AC-1** `Emergence.KindBlind`, `Emergence.RespawnKindsRejected`
  - Given: one request of each kind; then InformantRespawn and Rejoin.
  - When: served.
  - Then: pose, animation, walk-in path, delay rule and eligible sockets are identical across the four; the other two kinds are refused, logged, and create 0 pawns.
  - Edge cases: PawnRecovery uses the same clear check.
- **AC-2** `Emergence.ClearCheck.<Npc|Player|Informant>`, `Emergence.SameTickOrder`
  - Given: a socket with an NPC / player / informant in W(O) dilated 2 m; or a pawn within 3 m of the ExitPoint; or last used 19.9 s ago.
  - When: requests are made at 0 s, 30 s, 90 s of waiting; two same-tick requests for one socket.
  - Then: never selected at any of the 3 times (never relaxed); lower sequence id wins, the other re-selects in the same tick.
  - Edge cases: 20.0 s and 3.0 m boundary are clear.
- **AC-3** `Emergence.UniformAmongClear`, `Emergence.BlindToIdentity`
  - Given: 5 clear sockets, 5,000 requests, pinned sub-seeds.
  - When: picks are tallied.
  - Then: chi-square < 13.277 (df 4); flipping IsPlayer/team/participant state changes 0 picks; extra draws in another domain change 0 picks.
  - Edge cases: rejection sampling, no `%`.

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Layout/EmergenceRequestTest.cpp` — `StealthGame.Layout.Emergence.KindBlind`, `.RespawnKindsRejected`, `.ClearCheck.<Npc|Player|Informant>`, `.SameTickOrder`, `.UniformAmongClear`, `.BlindToIdentity` must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/level-socket-contract/story-016-predicates-emergence-witness-alley.md`, `production/epics/level-socket-contract/story-022-selection-secret-draw-streams.md`
- Cross-epic: `production/epics/crowd-npc-behaviour/EPIC.md` (`ILayoutPawnQuery`, scheduler; stub until it exists)
- Unlocks: `production/epics/level-socket-contract/story-031-emergence-kill-decoupling-and-rate-bound.md`, `production/epics/level-socket-contract/story-032-emergence-no-visible-pop.md`
