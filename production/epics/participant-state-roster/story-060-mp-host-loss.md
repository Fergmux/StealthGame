# Story 060: Host loss empties every client's roster store

> **Epic**: Participant State & Roster
> **Status**: Blocked — BLOCKED: spike S5 (listen-host loss) has not reported. Unblocks when S5 reports (quit notice and crash detection bounds, F6).
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/participant-state-roster.md`
**Requirement**: `TR-roster-001`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0005: Session, seamless travel and match-scoped state stores
**ADR Decision Summary**: ADR-0005 keeps the Roster store in the host process only: every mutating API asserts authority and the game thread, and on a client process the stores exist but stay empty (a client may later host). There is no host migration in MVP; a deliberate host quit sends `ClientSessionEnding` and waits for acknowledgements before `EndMatch`, and a host crash is detected by each client's host-loss clock (F6). `EndMatch` runs on every session exit path, so a former host keeps nothing.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: No new post-cutoff APIs; the 5.6 `DispatchPostLogin` deprecation applies to login overrides; `UE_LOGF` for logging. Verification required: **S5** (listen-host loss: quit notice and crash detection bounds, F6; engine timeout backstop values effective in packaged Development builds), S3 and S4. S5 has not reported.

**Control Manifest Rules (this layer)**:
- Required: `EndMatch` runs on every session exit path; a store that survives into a new session is a defect — source: ADR-0005
- Required: every mutating API asserts authority and the game thread, and on a client process the stores exist but stay empty — source: ADR-0005
- Required: T3 code lives in `StealthTestHarness`; multi-process criteria are proved multi-process standalone, never in in-process PIE only — source: ADR-0008
- Forbidden: Never use host migration paths; never let a client message start, extend or restart a deadline — source: ADR-0005
- Forbidden: Never store anything authoritative on a PlayerController, PlayerState, GameMode or GameState — source: ADR-0005
- Guardrail: Session silence clocks and ack bookkeeping inside the 0.5 ms line; every wait bounded by a server-measured clock — source: ADR-0005

---

## Acceptance Criteria

*From GDD `design/gdd/participant-state-roster.md`, scoped to this story:*

- [ ] **AC-PSR-40c** [MP-3][S5] `StealthGame.Roster.MP.HostLoss`: GIVEN a running round, WHEN the host process is killed, THEN on every client `ClientRosterStore.Num() == 0` and no Roster query returns a value other than the not-authority result.

---

## Implementation Notes

*Derived from ADR-0005 sections 2, 7 and Implementation Guidelines 3, 12, and GDD R2, R10 (client query answers), R11 ("Host loss (listen server)"):*

- **Blocker, stated precisely:** spike S5 has not reported. It must establish how a client observes a killed listen host (quit notice versus crash detection bounds, the F6 host-loss clock, engine timeout backstops in packaged Development builds). **Unblocks when** S5 reports; this story also depends on Story 058 for the multi-process travel harness.
- Rule being proved (R11): the store lives in the host process; the match ends for every client and the Roster is not recovered; there is no host migration in MVP. Client behaviour after host loss belongs to Session & Travel (`production/epics/session-travel/EPIC.md`).
- Client-side answers (R2, R10): the stores exist but stay empty on a client; every Roster query on a client returns its client-side answer: `ResolveVictimKind` returns `NotTargetable(Stale)`, `IsPawnBound` and `IsTeamEliminated` return false, `CountActive`, `CountInformant`, `CountRoster` return 0, mutators return `Rejected_NotAuthority`, each reporting to the fault sink (Story 004 defines the per-call table). The GDD phrase "the not-authority result" is looser than this list; assert the client-side answers.
- Test shape (T3, [MP-3]): a standalone multi-process run, a listen host and clients in a running round; before the kill, establish a baseline that the client store is already dormant by design (QA report: `Num() == 0` is trivially true on a client without a pre-loss baseline, so also assert a pre-loss check that the host's store is populated and the clients' queries return the client-side answers while the host is alive). Kill the host process; on every client assert the store is empty and queries return the client-side answers.
- Open point: `ClientRosterStore.Num()` is not defined anywhere else in the design. Name the accessor (for example the store dump hook's record count on the client's `URosterStore`) before implementing the assertion; the dump hook is shared with Stories 057, 058, 059.
- Waits are conditions with a timeout counted in server frames or integer milliseconds, bounded by the F6 host-loss detection bound; record the legacy replication system.
- Prototype disposition: no prototype counterpart (the prototype kept team and life state on the PlayerState and GameMode, which exist on every client; replaced by the host-only game-instance store).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 004 (Every roster call on a client is rejected or answers not-targetable and faults): the per-call client answer table.
- Story 058 (Roster identity and team state survive seamless and hard travel unchanged): travel persistence and the shared multi-process harness.
- Story 057 (Two client processes on one machine with distinct identity keys get distinct records): same-machine identity.
- Story 059 (A client rejoining within grace reclaims its record, pawn and informant status): rejoin within grace.
- Session & Travel host-loss UX (`production/epics/session-travel/EPIC.md`): what clients display after host loss.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthTestHarness/Private/Roster/StealthRosterHostLossTest.cpp` (T3 multi-process node, name `StealthGame.Roster.MP.HostLoss`)
- `Source/StealthTestHarness/Private/Roster/StealthRosterStoreDumpHook.h` and `.cpp` (shared store dump hook; client-side record count accessor)
- Header names follow ADR-0007: `Stealth` prefix, unique project-wide.

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation. (At tiers where the QL-STORY-READY gate is skipped — `minimal`, or `lean`/`solo` review mode — no qa-lead specs exist, and this section reads: N/A — no qa-lead specs at this tier; implement against the Acceptance Criteria above.)*

*Test cases not yet defined — run /qa-plan to generate them.*

**Flagged gap (QL-STORY-READY: GAPS; also BLOCKED by S5)**: ClientRosterStore.Num() is not defined anywhere else in the design. A client store is dormant by design so Num() == 0 is trivially true without a pre-loss baseline. "Not-authority result" does not match count and ResolveVictimKind queries, which return 0, false or NotTargetable(Stale). Needs: name the accessor, add a pre-loss check, list the expected client query results.

**Provisional spec (qa-lead, pending gap resolution)**:

- **AC-1**: host loss empties every client's roster store.
  - Given: a running round with a listen host and clients.
  - When: the host process is killed.
  - Then: on every client the store is empty and client queries return the client-side answers.
  - Edge cases: client behaviour after host loss belongs to Session & Travel (R11).

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: automated test `Source/StealthTestHarness/Private/Roster/StealthRosterHostLossTest.cpp` (`StealthGame.Roster.MP.HostLoss`) OR a documented playtest; `qa.level` is full, so the automated test is required.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/participant-state-roster/story-058-mp-travel-persistence.md`
- Cross-epic: `production/epics/session-travel/EPIC.md` (host-loss clock F6, session end); spike S5 must report before this story is unblocked
- Unlocks: None
