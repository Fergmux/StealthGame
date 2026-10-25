# Story 045: Opponents receive nothing but allowlisted movement updates after a kill of a player, NPC or informant

> **Epic**: Participant State & Roster
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/participant-state-roster.md`
**Requirement**: `TR-roster-008`, `TR-roster-011`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0001: Crowd identity rules
**Also governed by**: ADR-0008: Test and automation strategy for leak and parity tests
**ADR Decision Summary**: ADR-0001 delivers team data only through owner-only C-TeamView components on each PlayerController (never relevant to an opponent) and keeps `AStealthPlayerState` a closed public allowlist, so a team-0 elimination can change no byte that reaches an opponent except ordinary pawn movement and animation. The informant-kill invariant makes a kill of a player, an NPC or an informant indistinguishable to everyone else. ADR-0008 makes such leak gates a five-tier test system with mandatory negative controls, a fault-sink seam, and a startup assertion that the game NetDriver is the legacy replication system, on which the property-level logging works.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: ADR-0001 post-cutoff items: `APawn::RemoteViewPitch16` (5.6) and the `net.Iris.UseIrisReplication` default of 0 (legacy driver, Iris pinned off). Verification: S2 (PlayerState trims; PASS 2026-10-25, legacy driver, property-level logging, one-process PIE, about 2 % loss; not a byte-exact or multi-machine test), S1, S14. ADR-0008 (CQTest `FPIENetworkComponent`, `EAutomationTestFlags` is an enum class and every test needs an application-context flag, one `Automation RunTests` per `-ExecCmds`) has verification items for harness startup and capture that are not yet reported.

**Control Manifest Rules (this layer)**:
- Required: Every leak, parity, statistical and lint gate includes a negative control the suite detects — source: ADR-0008
- Required: The game NetDriver runs the legacy replication system and the harness asserts it at startup; T2/T3 tests are bound by frames or server time and assert and record the active replication system — source: ADR-0001, ADR-0008
- Required: Team-audience data travels only in a `UTeamViewComponentBase` subclass on the receiving teammate's own PlayerController; owner-audience data only on the owner's PlayerController — source: ADR-0001
- Forbidden: Never put team, state, connection, appearance or pawn data on a PlayerState — source: ADR-0001
- Forbidden: Never use wall-clock time as a test pass/fail predicate (only as a failure ceiling); never prove multi-process criteria in in-process PIE only — source: ADR-0008
- Forbidden: Never disable, skip or reseed a failing test to pass a gate; never mark a gate Done that was never watched to fail — source: ADR-0008
- Guardrail: C-TeamView pushes change-only, at most 1 per frame, within the 50 KB/s per-client budget — source: ADR-0001

This story delivers the PIE differential leak test for a kill: after a team-0 player, then an NPC, then an informant is killed, the server state is `Informant` in the same frame, the teammate sees it within 250 ms at a fixed 100 ms lag, and the opponent receives, in the next 2,000 ms, only allowlisted pawn movement and animation updates (zero PlayerState, zero PlayerController updates), with identical `(class, property)` membership across the three cases. It exists because an elimination must be invisible to the other team (Pillar 1). It realises GDD R6 (carriers and audiences) and R10 (informant-kill invariant).

---

## Acceptance Criteria

*From GDD `design/gdd/participant-state-roster.md`, scoped to this story:*

- [ ] **AC-PSR-38** [PIE-3][S2] `StealthGame.Roster.PIE.EliminationLeakDiff`: GIVEN an instrumented build that logs every replicated `(actor class, property)` update each client receives, WHEN a team-0 player is assassinated, THEN:
  - the server state is `Informant` in the same server frame;
  - the teammate's C-TeamView shows `Informant` within 250 ms at the fixed 100 ms lag (the Game Feel target);
  - on the opponent client, the `(class, property)` updates received in the 2 000 ms after the kill are a subset of a checked-in allowlist (pawn movement and animation properties only), with zero `PlayerState` and zero `PlayerController` updates;
  - the same holds when the victim is an NPC, and when the victim is an informant (control cases). The three received sets have identical `(class, property)` membership (Player Fantasy anchor 2; R6, R10 informant-kill invariant).

---

## Implementation Notes

*Derived from ADR-0001 sections 4-5 and Implementation Guidelines, ADR-0008 and GDD R6, R10, R12:*

- **Test shape.** CQTest network test `NETWORK_TEST_CLASS(EliminationLeakDiff, "StealthGame.Roster.PIE")` in `Source/StealthTestHarness/Private/Roster/StealthRosterEliminationLeakDiffTest.cpp` (full name `StealthGame.Roster.PIE.EliminationLeakDiff.<Method>`). PIE listen server with 3 clients plus the host (team-0 victim and teammate, team-1 opponent), `WithGameMode(AStealthGameMode)`, fixed emulation for the gating run (PktLag 100 ms, no variance, PktLoss 0 %), legacy driver asserted and recorded at startup. Every wait is a condition with a timeout counted in server frames or integer milliseconds; hitting the timeout fails the test. The Soak run (PktLoss 2 %, variance +-20 ms) repeats it and is advisory.
- **The instrument.** Use the capture mechanism ADR-0008 defines for T2. The S2 spike measured property-level reception with `LogRepProperties=VeryVerbose` on the legacy driver and decoded parents by `ClassReps` index (`production/qa/spikes/2026-10-25_S1-S2-S14.md`, section 0); reuse that approach or its harness successor. It is not a byte-exact comparison: bunch sizes and value contents are not compared (spike limits).
- **Log scope (qa-lead gap).** Scope the comparison to the victim pawn's actor and the actors a teammate-relevant observer could receive for it, not to every actor in the world; otherwise unrelated replication (GameState, crowd updates) breaks "subset of allowlist". Record the scoping rule in the test.
- **The allowlist.** The checked-in allowlist (for example `StealthRosterLeakAllowlist.h`) lists the pawn movement and animation `(class, property)` pairs only, taken from the pawn properties the ADR-0003 allowlist row set and the IVP pawn rows approve. The S1/S2 spike table of what a non-owner received for the prototype pawn (RemoteRole, ReplicatedMovement, Role, Instigator, RemoteViewPitch16, ReplicatedBasedMovement, `bProxyIsCharacterMovementActive`, ReplicatedMovementMode and the like) shows the shape but includes prototype fields (`bConcealed`, `bDying`, `HeldUntilServerTime`) that must not be allowlisted. The list must be created and signed off with this story.
- **Determinism (qa-lead gap).** "Identical membership across three runs" is flaky unless motion is scripted: use fixed scripted movement and a fixed seed, and run the three victim cases (player, NPC, informant) from identical starting state, ideally in separate rounds.
- **Kill driver.** The Assassination and NPC-Kill Penalty epics are not declared dependencies, so drive the kill through the Roster server calls the way Assassination would (`RequestTransition(victim, Informant, Assassinated, ..., RetireAtServerTime)`, retire the binding, then the pawn's death handling), and the informant case through `RetireBinding(Pawn, T, InformantKilled)`; replace the stub drivers when those epics land. Assert server state `Informant` in the same server frame as the transition (state is written immediately, R8), and the teammate's C-TeamView entry shows `Informant` within 250 ms (count in server frames at the fixed profile, not wall clock).
- **Zero PlayerState and PlayerController updates.** The opponent client must receive none for the victim participant or anyone else after the kill (story 040 allowlist, story 037 owner-only carrier).
- **Needs the push step.** The teammate C-TeamView check needs a working Roster push step and `URosterTeamViewComponent` replication; story 037 supplies only the struct and serializer, and 038/041 the feed and rebind. See story 041's Open point; if the component is not yet in place when this story starts, it is a blocker.
- **Open point (negative control):** the manifest requires every leak gate to include a negative control the suite detects, but the qa-lead spec lists none. Agree with the qa-lead on one (for example a test-seam-only property that changes on elimination and must make the diff fail) before closing.
- **Open point:** the 250 ms teammate latency is a Game Feel target measured in time; ADR-0008 forbids wall-clock pass/fail. Define it as a frame or server-time bound at the fixed profile and record the conversion.
- **Prototype disposition:** supersedes the prototype's behaviour where team and life state replicate to every client on the PlayerState (`Source/StealthGame/Public/StealthPlayerState.h`, known issue 2 of `production/first-playable-report.md`), replaced by story 040; `Source/StealthGame/Private/Spikes/StealthIdentitySpike.*` is reference only (spike evidence method), never promoted.
- **Header naming (ADR-0007).** Stealth prefix, project-unique, test-only under the harness `Private/Roster/`: `StealthRosterLeakAllowlist.h`, `StealthRosterReplicationCapture.h`.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 040 (PlayerState replicates only the allowlist and engine defaults are disabled or constant): owns the PlayerState allowlist this test relies on.
- Story 037 (A maximal C-TeamView net-serializes in at most 96 bytes): owns the carrier struct.
- Story 014 (Active to Informant applies for each entering reason with the correct pawn disposition): owns the transition the kill triggers.
- Story 046 (Non-owned pawns carry null PlayerState, Controller and Owner for players and NPCs alike): raw pawn pointer dump.
- Story 047 (No opponent count reaches a client before the round ends): opponent counts.
- Story 050 (Killing an informant produces the same attacker-side RPCs, HUD and cues as killing an NPC): attacker-side parity, not the opponent's received set.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- `Source/StealthTestHarness/Private/Roster/StealthRosterEliminationLeakDiffTest.cpp` (CQTest network test, full name `StealthGame.Roster.PIE.EliminationLeakDiff`)
- `Source/StealthTestHarness/Private/Roster/StealthRosterLeakAllowlist.h` (checked-in `(class, property)` allowlist)
- `Source/StealthTestHarness/Private/Roster/StealthRosterReplicationCapture.h` (per-client replicated property update capture and scoping)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation. (At tiers where the QL-STORY-READY gate is skipped — `minimal`, or `lean`/`solo` review mode — no qa-lead specs exist, and this section reads: N/A — no qa-lead specs at this tier; implement against the Acceptance Criteria above.)*

*Test cases not yet defined — run /qa-plan to generate them.*

**Flagged gap (QL-STORY-READY: GAPS):** log scope undefined. If it covers every actor, unrelated replication (GameState, crowd updates) breaks "subset of allowlist". The allowlist content is not given. "Identical (class, property) membership" across three runs is flaky unless motion is scripted. Needs: scope to the victim and teammate-relevant actors with a checked-in allowlist; fixed scripts and seeds.

**Provisional spec (qa-lead, pending gap resolution):**

- **AC-1**: opponents receive nothing but allowlisted movement updates after a kill.
  - Given: an instrumented build, legacy driver, fixed 100 ms lag, the checked-in allowlist.
  - When: a team-0 player, then an NPC, then an informant are killed.
  - Then: server state Informant in the same frame. Teammate view shows Informant within 250 ms. In the following 2,000 ms the opponent receives only allowlisted updates, zero PlayerState and zero PlayerController updates. The three sets have identical membership.
  - Edge cases: timeouts counted in frames; Soak runs advisory.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/Roster/StealthRosterEliminationLeakDiffTest.cpp` — automated test must exist and pass (OR a documented playtest; `qa.level` is full, so the automated test is required). The advisory Soak run results are recorded separately and never gate.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/participant-state-roster/story-040-player-state-replicated-allowlist.md`, `production/epics/participant-state-roster/story-014-transition-active-to-informant.md`, `production/epics/participant-state-roster/story-037-team-view-serialized-size.md`
- Unlocks: `production/epics/participant-state-roster/story-047-pie-opponent-counts-never-replicated.md`, `production/epics/participant-state-roster/story-050-pie-informant-kill-looks-like-npc-kill.md`
