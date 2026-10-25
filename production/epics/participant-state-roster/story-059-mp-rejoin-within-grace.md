# Story 059: A client rejoining within grace reclaims its record, pawn and informant status

> **Epic**: Participant State & Roster
> **Status**: Blocked — BLOCKED: spike S8 (PawnLeavingGame override hand-over) has not reported. S3 (production/qa/spikes/2026-10-25_S3-seamless-travel.md) and S4 (production/qa/spikes/2026-10-25_S4-rejoin-adoption.md) reported PASS 2026-10-25 and the multi-process harness exists (tools/ci/mp/run-mp.ps1); unblocks when S8 reports.
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/participant-state-roster.md`
**Requirement**: `TR-roster-010`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0005: Session, seamless travel and match-scoped state stores
**Also governed by**: ADR-0010: Shared Pawn: CMC locomotion, spawn factory and possession parity
**ADR Decision Summary**: ADR-0005 makes a rejoin (fresh login, known key) adopt the retained PlayerState in `InitPlayerState` (`SetPlayerState`, `SetOwner(this)`, `OnReactivated`), so no new PlayerState ever exists and the player array never gains or loses an entry; the participant is `Rejoining` until its acknowledgement, and a supersede drops the old connection mapping, closes the old connection `Superseded` and binds the new one (latest wins). ADR-0010 guarantees that disconnect never destroys a pawn: `PawnLeavingGame` hands it to a crowd controller in the same frame, the binding stays `BoundLive`, and a returning Active participant repossesses it through the hand-over pass; a returning informant receives an adopted crowd pawn.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Post-cutoff: `UMotionWarpingComponent::GetOwnerAdapter()` (5.8) and MotionWarping Beta in 5.8.4 (ADR-0010); the 5.6 `DispatchPostLogin` deprecation applies to login overrides (ADR-0005); `UE_LOGF`. Verification required: **S4** (retained PlayerState re-attached, supersede), **S8** (`PawnLeavingGame` hand-over on a real connection close), S3 (travel harness), S14, H1 (AI to player hand-over gap). S4 and S8 have not reported.

**Control Manifest Rules (this layer)**:
- Required: a rejoiner adopts its retained PlayerState in `InitPlayerState`; `OnDeactivated` does not destroy the PlayerState while admission is `RejoinOnly` — source: ADR-0005
- Required: `PawnLeavingGame` never destroys the pawn; a possession change of a live pawn is committed in a queue slot and executed in the hand-over pass at the next frame start — source: ADR-0010
- Required: the Roster teammate entry carries `bReconnecting`, cleared only by a Roster event (Rejoined, GraceExpired or Cleared), never by a timer — source: ADR-0001, ADR-0005
- Forbidden: Never spawn a fresh PlayerState for a reclaim; never toggle `bIsInactive` or remove a PlayerState from the array between `Preparing` and the next travel — source: ADR-0005
- Forbidden: Never let Session spawn, free, hide or possess a pawn, or call `RequestTransition` — source: ADR-0005
- Guardrail: listen-host frame time at most 1/60 s (p99 under the profiled load) — source: ADR-0002, ADR-0003

---

## Acceptance Criteria

*From GDD `design/gdd/participant-state-roster.md`, scoped to this story:*

- [ ] **AC-PSR-40b** [MP-3][S4][S8] `StealthGame.Roster.MP.RejoinWithinGrace`: GIVEN an Active client that drops within grace, WHEN it rejoins, THEN:
  - it gets `Reclaimed` with the same id, handle, team and state, with exactly one `OnParticipantReclaimed`, also when two sockets present the same key;
  - it repossesses the same pawn;
  - during grace the pawn is never destroyed, and its longest idle interval is <= `CrowdMaxIdleSec` (Crowd key, provisional);
  - an Informant who rejoins is still an Informant and receives an adopted crowd pawn (Player Fantasy anchor 3).

---

## Implementation Notes

*Derived from ADR-0005 section 5, ADR-0010 sections 3.2, 4.3, 5, and GDD R9, R11 ("Rejoin", "Duplicate connection for one key"), Edge Cases ("Disconnect and rejoin"):*

- **Blocker, stated precisely:** S4 (retained PlayerState re-attachment, two clients on one machine, supersede) and S8 (the `PawnLeavingGame` override keeps the pawn and a crowd AI controller possesses it in the same server frame on a real connection close) have not reported; S3 also gates the multi-process travel harness this node reuses. **Unblocks when** S4 and S8 report PASS and Stories 054 and 057 are done. Do not start before then.
- Behaviour under test (R11): `RegisterParticipant` with a matching key returns `Reclaimed(Id)` with the same `ParticipantId`, `PublicHandle`, `Team`, `State` and `AppearanceHistory`, `Connection = Connected`, the retained PlayerState re-associated (`RebindView`) and C-TeamView re-pushed. A reclaimed `Active` participant within grace repossesses its still-bound pawn (hand-over pass, ADR-0010 section 4.3, AI to player, with the last-command hold and `bAwaitingFirstOwnerMove`); a reclaimed `Informant` with no live pawn is "awaiting pawn" and receives an adopted crowd pawn from NPC-Kill Penalty & Informants through `TakeOverCrowdPawn` (R9; Session readmits only after the acknowledgement). Reconnecting never changes state.
- Duplicate connection (R11, decision D24): two sockets presenting the same key: the newest connection wins; Session raises `OnParticipantDisconnected(Superseded)` at the supersede instant, closes the old connection `Superseded`, and the record is reclaimed exactly once, with exactly one `OnParticipantReclaimed`. The old pawn is AI-driven until the rejoiner is possessed.
- During grace the pawn is never destroyed (R9 hand-over hook; Session rule 22). The "longest idle interval is at most `CrowdMaxIdleSec`" clause is held until "idle" is defined (QA report: a speed threshold epsilon is needed) and `CrowdMaxIdleSec` (Crowd-owned, provisional) is final.
- Test shape (T3, [MP-3]): a standalone multi-process run, an Active client that drops within grace and rejoins, a second case with an Informant, and a duplicate-socket case. The store dump hook (shared with Stories 057, 058, 060) reads the record; use `Reclaimed` results and the event spy for the single `OnParticipantReclaimed`. Waits are conditions with frame or millisecond timeouts; record the legacy replication system.
- Open point: define "idle" as pawn speed below a stated epsilon before the idle-interval clause is asserted, and take `CrowdMaxIdleSec` from the Crowd epic when final (`production/epics/crowd-npc-behaviour/EPIC.md`).
- Prototype disposition: no prototype counterpart (the prototype destroyed a leaving player's pawn and had no reclaim); the prototype `StealthGameMode.cpp` participant bookkeeping is replaced.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 029 (Reclaim restores the same record for active and informant participants): the pure-core reclaim (same-frame double registration).
- Story 030 (An informant's released pawn becomes an NPC and a reclaim adopts a different pawn): the core informant release and adoption.
- Story 054 (Disconnect grace expires on the injected clock and stale-connection timers are rejected): grace timing.
- Story 057 (Two client processes on one machine with distinct identity keys get distinct records): same-machine distinct keys.
- Story 056 (A disconnect hand-over never decelerates faster than the crowd stop decel and never destroys the pawn): the takeover blend.
- Story 051 (A teammate's reconnecting flag and end cause follow the event-only clear rules): teammate flag.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthTestHarness/Private/Roster/StealthRosterRejoinWithinGraceTest.cpp` (T3 multi-process node, name `StealthGame.Roster.MP.RejoinWithinGrace`)
- `Source/StealthTestHarness/Private/Roster/StealthRosterStoreDumpHook.h` and `.cpp` (shared store dump hook)
- `Source/StealthTestHarness/Private/Roster/StealthRosterPawnIdleProbe.h` (idle-interval probe; held until "idle" is defined)
- Header names follow ADR-0007: `Stealth` prefix, unique project-wide.

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation. (At tiers where the QL-STORY-READY gate is skipped — `minimal`, or `lean`/`solo` review mode — no qa-lead specs exist, and this section reads: N/A — no qa-lead specs at this tier; implement against the Acceptance Criteria above.)*

*Test cases not yet defined — run /qa-plan to generate them.*

**Flagged gap (QL-STORY-READY: GAPS; also BLOCKED by S4, S8)**: "longest idle interval is at most CrowdMaxIdleSec" has no definition of "idle" (a speed threshold) and the key is provisional (owned by Crowd). Needs idle defined as speed below a stated epsilon and the Crowd value once final.

**Provisional spec (qa-lead, pending gap resolution)**:

- **AC-1**: a client rejoining within grace reclaims its record, pawn and informant status.
  - Given: a multi-process run with an Active client that drops within grace, and a second case with an Informant.
  - When: it rejoins; also two sockets present the same key.
  - Then: Reclaimed with the same id, handle, team, state, one OnParticipantReclaimed. It repossesses the same pawn. During grace the pawn is never destroyed. An Informant stays Informant and receives an adopted crowd pawn.
  - Edge cases: the idle-interval check is held until "idle" is defined.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: automated test `Source/StealthTestHarness/Private/Roster/StealthRosterRejoinWithinGraceTest.cpp` (`StealthGame.Roster.MP.RejoinWithinGrace`) OR a documented playtest; `qa.level` is full, so the automated test is required.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/participant-state-roster/story-054-disconnect-grace-timing.md`, `production/epics/participant-state-roster/story-057-mp-same-machine-clients-distinct.md`
- Cross-epic: `production/epics/session-travel/EPIC.md`, `production/epics/crowd-npc-behaviour/EPIC.md` (`CrowdMaxIdleSec`) and `production/epics/shared-character-locomotion/EPIC.md` (hand-over hook); spikes S4 and S8 must report before this story is unblocked
- Unlocks: None
