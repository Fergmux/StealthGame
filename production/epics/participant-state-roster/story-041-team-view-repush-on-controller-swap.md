# Story 041: A replaced controller or reclaim re-pushes C-TeamView within one server frame

> **Epic**: Participant State & Roster
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/participant-state-roster.md`
**Requirement**: `TR-roster-012`, `TR-roster-006`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0005: Session, seamless travel and match-scoped state stores
**Also governed by**: ADR-0020: Objectives data and team-view sub-structs; ADR-0001: Crowd identity rules
**ADR Decision Summary**: ADR-0005 makes the four game-instance stores the only authoritative match state precisely because seamless travel replaces every PlayerController and PlayerState; on arrival and on rejoin the Roster rewrites the view fields from the record through `RebindView`, which is independent of world wiring, and raises the server-only `URosterStore::OnParticipantViewRebound(FParticipantId)` after the Roster part is re-pushed. ADR-0020 has Objectives subscribe to that event and rewrite its team-view part inline. ADR-0001 fixes C-TeamView as push-model components on the controller, pushed on change only, at most once per frame, with `PawnRef`s cleared at `InvalidateAllBindings` and re-pushed after each controller swap and on reclaim.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: ADR-0005 uses no new post-cutoff API (`DispatchPostLogin` is deprecated since 5.6: use `PostLogin`/`OnPostLogin`; `UE_LOGF`). Verification items: S3 (seamless and hard travel, controller replacement, re-bind, C-TeamView re-push; not yet reported), S4 (reconnect, retained PlayerState; not yet reported), S5. ADR-0001 items S1, S2, S14. The component replicates only if the owning controller sets `bReplicateUsingRegisteredSubObjectList` (default false in 5.8.4; ADR-0020 E2). This story's automation world simulates the swap in-process, so it does not replace S3.

**Control Manifest Rules (this layer)**:
- Required: Team-audience data travels only in a `UTeamViewComponentBase` subclass on the receiving teammate's own PlayerController; owner-audience data only on the owner's PlayerController — source: ADR-0001
- Required: The Roster push step is the only writer of the replicated C-TeamView part (change-only, at most once per server frame, coalesced) — source: ADR-0001
- Required: Authoritative match state lives only in the four game-instance stores, keyed by `ParticipantId`, never by controller or PlayerState pointer — source: ADR-0005
- Required: Registrations are weak (`TWeakInterfacePtr`/`TWeakObjectPtr`), cleared in `Deinitialize`/`EndPlay`; a dead registration is a fault-sink report, never a crash — source: ADR-0007
- Forbidden: Never store anything authoritative on a PlayerController, PlayerState, GameMode or GameState; never carry match state across travel on them — source: ADR-0005
- Forbidden: Never use a team-filtered actor, a per-team replica actor or a team relevancy rule — source: ADR-0001
- Guardrail: C-TeamView pushes change-only, at most 1 per frame, within the 50 KB/s per-client budget — source: ADR-0001

This story delivers `RebindView` on the Roster store and the server-only `OnViewRebound` notification, and proves that a replaced PlayerController (a seamless-travel swap, simulated in an automation world) and a reclaim each receive a C-TeamView equal to the server's projection within one server frame, listing only the owner's team. It exists because travel and rejoin replace controllers, and a stale or empty view would leave a teammate marker or HUD wrong. It realises GDD R6 (re-push after every controller swap and on reclaim), R11 (reclaim) and R12 (the rebind notification).

---

## Acceptance Criteria

*From GDD `design/gdd/participant-state-roster.md`, scoped to this story:*

- [ ] **AC-PSR-49** [Int] `StealthGame.Roster.TeamViewRepushOnControllerSwap`: GIVEN a client's PlayerController is replaced (a seamless-travel swap simulated in an automation world) and, separately, a reclaim occurs, WHEN the server processes it, THEN the new controller's C-TeamView equals the server's projection within 1 server frame, and it lists only the owner's team (R6, R11).

---

## Implementation Notes

*Derived from ADR-0005 section 2 ("RebindView is independent of world wiring"), ADR-0020 section 6, ADR-0001 section 5 and GDD R6, R11, R12:*

- **`RebindView(FParticipantId)`** on `FRosterCore`/`URosterStore`: writes the participant's view fields (the PlayerState view fields) and rebuilds and writes the C-TeamView Roster part from the record, for the controller currently associated with the participant. It must not depend on providers registered in `OnWorldBeginPlay`, so that the host's rebind in `PostSeamlessTravel` can run before world `BeginPlay`.
- **`OnViewRebound`.** After the Roster part has been re-pushed, raise `URosterStore::OnParticipantViewRebound(FParticipantId)` (ADR-0005 C++ name for the GDD's `OnViewRebound(ParticipantId)`) synchronously on the server, at the end of every `RebindView`. It is not a lifecycle event: it consumes no `EventSeq`, queues nothing and never reaches a client (GDD R12, "Rebind notification"). Subscribers (Objectives, ADR-0020 section 6) bind weakly from the composition root and rewrite their own team-view parts inline; the Roster knows nothing of them. A rebind that runs before world wiring reaches no feature subscriber, which is correct because feature parts are round-scoped and start empty.
- **Re-push triggers.** (1) A controller swap: `RebindView` forces the participant's view dirty so the next push step (at most once per server frame) writes the full part to the new controller's `URosterTeamViewComponent`. (2) A reclaim (`RegisterParticipant` with a known key returns `Reclaimed`, story 029): the same path. `PawnRef` entries were cleared at `InvalidateAllBindings` (travel start) and are re-pushed here from the binding table.
- **Audience.** The pushed part contains only the owner's own block and that owner's teammates (same team); no opponent entry ever appears (R6). Build it from the core's teammate-view projection that stories 021 and 038 use, so the test compares the component's stored value against that projection.
- **Frame bound.** "Within 1 server frame" is counted in server frames (ticks), never wall-clock: after `RebindView` (or reclaim) the test advances at most one push-step cycle and asserts equality.
- **Automation world.** Create a world with a participant and teammates, create a fresh `APlayerController` carrying a `URosterTeamViewComponent` as the "replacement", call `RebindView`, run one frame. Because there is no remote client in this world, equality is checked on the server-side component state, which is what would replicate; the wire arrival and a real travel are covered by stories 045 and 058 (PIE and multi-process). The component requires the owning controller to set `bReplicateUsingRegisteredSubObjectList = true`; set it in the test controller so the fixture matches `AStealthPlayerController`.
- **`OnViewRebound` has no GDD acceptance criterion** (the qa-lead notes TR-roster-012 is claimed by this story but no AC asserts the notification). This story implements the notification as R12 describes it and does not add a test case for it; follow-up: add an AC to the GDD (a spy subscriber sees exactly one call per `RebindView`, after the Roster part is written, with no `EventSeq` consumed) and a story or a test case for it.
- **Open point:** which story implements `URosterTeamViewComponent` and the Roster push step is not named in the plan (story 037 defines the struct and serializer, 038 uses a push-step spy). This story needs a real component and push step; if neither exists when this story starts, implement the minimal component (`UTeamViewComponentBase` in `Public/View/`, `URosterTeamViewComponent`) here and record it so stories 045 and 051 reuse it.
- **Prototype disposition:** no prototype counterpart. The prototype keeps team and life state on the replicated PlayerState and has no per-controller view (`Source/StealthGame/Public/StealthPlayerState.h`); nothing is ported.
- **Header naming (ADR-0007).** Stealth prefix, project-unique, under `View/` or `Server/`: `StealthRosterTeamView.h`, `StealthTeamViewComponentBase.h`, `StealthRosterService.h` (the delegate lives on the `Server/` facade).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 037 (A maximal C-TeamView net-serializes in at most 96 bytes): owns the struct and its serializer.
- Story 038 (SetOwnerTally and SetOwnerRespawnDue feed only the owner view with the full precedence): owns the owner view cache and feed.
- Story 029 (Reclaim restores the same record for active and informant participants): owns reclaim in the core.
- Story 042 (PawnRef resolves in either arrival order of pawn and team-view entry): client-side `PawnRef` resolution.
- Story 058 (Roster identity and team state survive seamless and hard travel unchanged): real seamless and hard travel, blocked on spike S3.
- Story 045 (Opponents receive nothing but allowlisted movement updates after a kill of a player, NPC or informant) and story 051 (A teammate's reconnecting flag and end cause follow the event-only clear rules in all six cases): PIE wire proof.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- `Source/StealthFoundation/Private/Roster/StealthRosterCore.h` (add `RebindView` input handling, view-dirty forcing)
- `Source/StealthFoundation/Private/Roster/StealthRosterCore.cpp`
- `Source/StealthFoundation/Public/Server/StealthRosterService.h` (add `RebindView`, `OnParticipantViewRebound` delegate)
- `Source/StealthFoundation/Private/Roster/StealthRosterStore.cpp`
- `Source/StealthFoundation/Public/View/StealthRosterTeamView.h` (`URosterTeamViewComponent`, if not present)
- `Source/StealthFoundation/Public/View/StealthTeamViewComponentBase.h` (`UTeamViewComponentBase`, if not present)
- `Source/StealthFoundation/Private/Roster/StealthRosterPushStep.cpp` (Roster push step, if not present)
- `Source/StealthFoundation/Private/Tests/Roster/StealthRosterTeamViewRepushTest.cpp` (class `FRosterTeamViewRepushOnControllerSwapTest`, test name `StealthGame.Roster.TeamViewRepushOnControllerSwap`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation. (At tiers where the QL-STORY-READY gate is skipped — `minimal`, or `lean`/`solo` review mode — no qa-lead specs exist, and this section reads: N/A — no qa-lead specs at this tier; implement against the Acceptance Criteria above.)*

**Automated test specs (Integration):**

- **AC-1**: a replaced controller or reclaim re-pushes C-TeamView within one server frame.
  - Given: an automation world with a participant and teammates, a simulated controller replacement (a new APlayerController plus RebindView), and a separate reclaim.
  - When: the server processes each.
  - Then: the new controller's C-TeamView equals the server's projection within 1 server frame and lists only the owner's team.
  - Edge cases: no AC asserts OnViewRebound (TR-roster-012); recommend a follow-up.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthFoundation/Private/Tests/Roster/StealthRosterTeamViewRepushTest.cpp` — automated test must exist and pass (OR a documented playtest; `qa.level` is full, so the automated test is required).

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/participant-state-roster/story-037-team-view-serialized-size.md`, `production/epics/participant-state-roster/story-038-owner-view-feed.md`, `production/epics/participant-state-roster/story-029-reclaim-core.md`
- Unlocks: `production/epics/participant-state-roster/story-058-mp-travel-persistence.md`
