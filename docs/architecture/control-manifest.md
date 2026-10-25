# Control Manifest

> **Engine**: Unreal Engine 5.8 (installed 5.8.4, CL 58902052)
> **Last Updated**: 2026-10-25
> **Manifest Version**: 2026-10-25c (same-day update after the user scope decisions of 2026-10-25; see "Changes in 2026-10-25c" and "Changes in 2026-10-25b" at the end)
> **ADRs Covered**: ADR-0001, ADR-0002, ADR-0003, ADR-0004, ADR-0005, ADR-0006, ADR-0007, ADR-0008, ADR-0009, ADR-0010, ADR-0011, ADR-0012, ADR-0013, ADR-0014, ADR-0015, ADR-0016, ADR-0017, ADR-0018, ADR-0019, ADR-0020, ADR-0021, ADR-0022, ADR-0023 (all **Accepted** 2026-10-25; several conditionally, see "Conditional acceptance")
> **Status**: Active — regenerate with `/create-control-manifest update` when ADRs change

`Manifest Version` is the date this manifest was generated. Stories embed it when
they are written, and `/story-readiness` compares a story's version with this one
to find stories written against old rules. It always equals `Last Updated`: the
same date, for different readers.

This manifest is a programmer's quick reference taken from the Accepted ADRs, the
technical preferences and the engine reference docs. For the reason behind a rule,
read its ADR.

## Source ADRs

| ADR | Title (file) | Status | Layer |
|---|---|---|---|
| ADR-0001 | Crowd identity rules (`adr-0001-crowd-identity-rules.md`) | Accepted 2026-10-25, conditional | Foundation |
| ADR-0002 | Resolution queue and round-end staging (`adr-0002-resolution-queue-round-end-staging.md`) | Accepted 2026-10-25 | Foundation |
| ADR-0003 | Replication, relevancy and the concealment cull (`adr-0003-replication-relevancy-concealment-cull.md`) | Accepted 2026-10-25, conditional | Foundation |
| ADR-0004 | Tuning data pipeline and validator (`adr-0004-tuning-data-pipeline-validator.md`) | Accepted 2026-10-25 | Foundation |
| ADR-0005 | Session, seamless travel and state stores (`adr-0005-session-seamless-travel-state-stores.md`) | Accepted 2026-10-25 | Foundation |
| ADR-0006 | Seeds and random streams (`adr-0006-seeds-random-streams.md`) | Accepted 2026-10-25 | Foundation |
| ADR-0007 | Code modules and include boundaries (`adr-0007-code-modules-include-boundaries.md`) | Accepted 2026-10-25 | Foundation |
| ADR-0008 | Test automation and leak/parity suites (`adr-0008-test-automation-leak-parity.md`) | Accepted 2026-10-25 | Foundation |
| ADR-0009 | Version control and binary assets (`adr-0009-version-control-binary-assets.md`) | Accepted 2026-10-25 (Git + Git LFS) | Foundation (process) |
| ADR-0010 | Shared Pawn, CMC and possession (`adr-0010-shared-pawn-cmc-possession.md`) | Accepted 2026-10-25, conditional | Core |
| ADR-0011 | Pawn command interface and input (`adr-0011-pawn-command-interface-input.md`) | Accepted 2026-10-25, conditional | Core |
| ADR-0012 | Character pool and appearance allocation (`adr-0012-character-pool-appearance-allocation.md`) | Accepted 2026-10-25, conditional | Core |
| ADR-0013 | Animation, motion matching and significance (`adr-0013-animation-motion-matching-significance.md`) | Accepted 2026-10-25, conditional | Core |
| ADR-0014 | Layout service, generation and navmesh (`adr-0014-layout-service-generation-navmesh.md`) | Accepted 2026-10-25 | Core (service) / Feature (City Generator provider) |
| ADR-0015 | Camera and line-of-sight envelope (`adr-0015-camera-los-envelope.md`) | Accepted 2026-10-25 | Core |
| ADR-0016 | Crowd AI architecture (`adr-0016-crowd-ai-architecture.md`) | Accepted 2026-10-25, conditional | Feature |
| ADR-0017 | Concealment and per-viewer presentation (`adr-0017-concealment-per-viewer-presentation.md`) | Accepted 2026-10-25, conditional | Feature |
| ADR-0018 | Encounter outcome timing (`adr-0018-encounter-outcome-timing.md`) | Accepted 2026-10-25, conditional | Feature (wire structs in Foundation `Public/View`) |
| ADR-0019 | Informant lifecycle and takeover (`adr-0019-informant-lifecycle-takeover.md`) | Accepted 2026-10-25, conditional | Feature |
| ADR-0020 | Objectives data and team view (`adr-0020-objectives-data-team-view.md`) | Accepted 2026-10-25, conditional | Feature |
| ADR-0021 | UI/HUD layering and Common UI (`adr-0021-ui-hud-layering-commonui.md`) | Accepted 2026-10-25, conditional | Presentation |
| ADR-0022 | Rendering and performance budgets (`adr-0022-rendering-performance-budgets.md`) | Accepted 2026-10-25, conditional | Presentation / cross-cutting |
| ADR-0023 | Playtest telemetry hooks (`adr-0023-playtest-telemetry-hooks.md`) | Accepted 2026-10-25 | Foundation (store) / Feature (probes) |

ADRs left out: none (23 of 23 Accepted, per `adr-status.sh` on 2026-10-25).

Rules were extracted from each ADR's `Decision` (its `Implementation Guidelines`
list), `Alternatives Considered`, `Performance Implications` and `Engine
Compatibility` sections. Where the engine reference disagrees with an ADR's
source-verified fact, the ADR wins (see "Engine-reference corrections" below).

### Conditional acceptance

The user accepted all 23 ADRs on 2026-10-25. For the ADRs below the acceptance
is **conditional**: every spike or verification item named in the ADR's earlier
status text must still report before implementation depends on it, and a failed
spike is handled by a superseding ADR (then a regenerated manifest), not by
editing the ADR. A rule that rests on an unreported spike carries the spike ID in
its text (for example "[U, R4]", "spike R2 decides", "provisional pending the
50-character spike"), and each layer's Engine API Constraints ends with a
**Spikes owning [U] claims** bullet. A story that depends on such a rule is not
ready until its spike has reported.

| ADR | Outstanding gate(s) before dependent implementation |
|---|---|
| ADR-0001 | Spikes S1, S2, S14 (S7, S11 only if Iris is ever considered) |
| ADR-0003 | Spikes S9, S10, S12, S13 |
| ADR-0010 | Traversal under net emulation, S8, S14, H1, P1 |
| ADR-0011 | Engine Compatibility verification items (V1-V6); `input-interaction-framework.md` re-review |
| ADR-0012 | CPA-V1..CPA-V5 |
| ADR-0013 | R3 on RTX 3060-class hardware; ANIM-V1..ANIM-V6 |
| ADR-0016 | 60-character spike (`NpcCount` 50 + 10 players; AC-CNB-26 capture on the reference host); §10 cost rule provisional until then |
| ADR-0017 | S-CON-1, R5, S9/S10 (`FActionState.StartServerTime` and `FDeathState.bDiedConcealed` approved 2026-10-25) |
| ADR-0018 | Through its dependencies: S1, S2, S10, S14; amendment requests to owning ADRs |
| ADR-0019 | Amendment requests to owning ADRs (the `T_due` carrier was decided 2026-10-25: C-TeamView owner part) |
| ADR-0020 | Objectives Chain GDD re-review; game-designer sign-off on the two GDD interpretations (§2, §5.3) — no spike |
| ADR-0021 | UI-V1..UI-V4; HUD and Teammate Markers GDD re-reviews |
| ADR-0022 | R1, R2, R3 on RTX 3060-class hardware at 1080p; pending project-config items (no `Config/` edit) |

Unconditional (no outstanding spike in their status text): ADR-0002, ADR-0004,
ADR-0005, ADR-0006, ADR-0007, ADR-0008, ADR-0009 (repository init and first
commit still need an explicit user instruction), ADR-0014, ADR-0015, ADR-0023.
Their own [U] engine claims are still listed under each layer's
**Spikes owning [U] claims** bullet.

**Review:** TD-MANIFEST (technical-director) returned CONCERNS on the
2026-10-25 draft (precision fixes, no inaccuracies); all ten flagged items and
both optional edits were applied, and the GPU frame figure was corrected to
ADR-0022's 14.6 ms. This 2026-10-25 regeneration after acceptance changed only
the ADRs' Status lines, so the rule content is carried forward unchanged and
TD-MANIFEST was not re-run (lean: no rule changed).

---

## Foundation Layer Rules

*Applies to: identity and replication policy, the resolution queue, tuning, session and travel, seeds, module layout, tests, version control, the Event Log store*

### Required Patterns

**Identity and replication policy**
- **Every body is an `AStealthPawn`**: one class, one component set, no per-instance replication or net-config difference — source: ADR-0001
- **`AStealthPawn` resets `PlayerState` (by name), `Controller` and `Owner` to `COND_OwnerOnly`** with the condition-only `ResetReplicatedLifetimeProperty` overload (`RESET_REPLIFETIME_CONDITION` for the public members); `AStealthPlayerState` resets its own `Owner`, `PlayerId` and `UniqueId` to `COND_OwnerOnly` — source: ADR-0001
- **`AStealthPlayerState` replicates only `PublicHandle` and `PlayerNamePrivate`** — source: ADR-0001
- **`AStealthPlayerController` clears `Owner`/`PlayerState` client-locally on its previous pawn** — source: ADR-0001
- **AI issues every command through `UPawnCommandComponent` entry points and moves only through the CMC input and control-rotation consumers a human uses; nothing writes a pawn's velocity or rotation directly for AI** — source: ADR-0001
- **Team-audience data travels only in a `UTeamViewComponentBase` subclass on the receiving teammate's own PlayerController; owner-audience data only on the owner's PlayerController** — source: ADR-0001
- **The Roster part carries owner `{Handle, Team, State, RoundEpoch, WrongTargetTally, LastResultSeq, T_due}` (last three Owner-only) and per teammate `{Handle, State, PawnRef, bIsConcealed, RetireAtServerTime}`; Penalty supplies tally and `T_due` only through `SetOwnerTally` / `SetOwnerRespawnDue`, and the retire time comes through `RetireBinding(Pawn, RetireAtServerTime)`; the Roster push step is the only writer** — source: ADR-0001, ADR-0019
- **`WrongTargetTally`/`LastResultSeq` are written to C-TeamView only on reclaim, rejoin and `OnRosterReset`; `RetireAtServerTime` is non-zero only in the retiring update (the kill's `ContactServerTime` or the release slot time)** — source: ADR-0001, ADR-0018, ADR-0019
- **Every new replicated field, fact, audience, condition or replicated class ships with a Visibility Policy row and a signed-off allowlist row**; the reflection audit fails the build otherwise — source: ADR-0001, ADR-0003
- **The game NetDriver runs the legacy replication system and the harness asserts it at startup** — source: ADR-0001
- **Pawn net config is set only from tuning values, through the 5.5+ setters (`SetNetUpdateFrequency`, `SetMinNetUpdateFrequency`, `SetNetCullDistanceSquared`), identically for every pawn** — source: ADR-0003
- **`AStealthPawn::IsNetRelevantFor` does not call `Super`**; its only inputs are the Policy's cull record and the viewer's relation — source: ADR-0003
- **`ForceNetUpdate()` on a pawn forwards only inside `FStealthForcedNetUpdate` with an allowed reason, at most once per pawn per frame**; the unscoped possession call is dropped and the 15 Hz cadence carries the change — source: ADR-0003, ADR-0001
- **`PreReplication` writes `RemoteViewPitch16` from `HeadLook` for every pawn, after `Super`** — source: ADR-0003
- **Every simulated proxy uses exponential smoothing with the same tuning values**, including the listen-server simulated smoothing times — source: ADR-0003
- **Outcome-bearing replicated structs serialise to a fixed bit width** — source: ADR-0003
- **The concealment cull lands on the global boundary grid and closes the channel explicitly per denied connection in the cull tick**, with no client acknowledgement wait; un-cull happens in the same server step as the state change — source: ADR-0003
- **The teammate fallback feed is added in the same frame a teammate's channel goes missing, to teammate connections only; every entry carries a `uint8 Seq` advanced on every feed tick** — source: ADR-0003
- **Client code tolerates a relevancy destroy and initialises a re-created actor from replicated state only** — source: ADR-0003

**Resolution queue**
- **Every interacting state change resolves as an item (or an inline follow-on in an item's slot) of the round's single queue**; nothing outside the drain mutates gameplay state — source: ADR-0002
- **Events are ordered only with `Compare`/`GetKey`/`CurrentKey`** — source: ADR-0002
- **Command RPC bodies and AI sources only push into mailboxes** — source: ADR-0002
- **Gameplay timers are `ScheduleAt` items in the due-time heap** — source: ADR-0002
- **Resolvers re-validate against live state at the slot and run follow-ons inline**; a deferred outcome is a new item with its own key and a logged `CauseKey` — source: ADR-0002
- **Mailbox and tie permutations come from `GetServerSubSeed(SeqOrder|TieRank, ServerTickIndex)`** — source: ADR-0002
- **Outcome-revealing presentation writes are typed staged writes**, decide nothing new, are dropped on `Reset`, and are applied only through the applier registered for their type (or `ITerminalDecision` for `RoundEndStep`); a write with no live applier is dropped, logged and fault-reported — source: ADR-0002
- **Resolvers call `RequestTeardown` and treat actors pending teardown as gone** — source: ADR-0002
- **Only Round's `TryTerminate` calls `Latch`**; after the latch the queue rejects every item with `Rejected_Latched` except parked `HostCommand`s, the post-latch lane and the `ConcealmentTransition` exemption; it holds the IIF's `NotNow` replies, freezes (not cancels) holds and leaves locomotion untouched — source: ADR-0002
- **The latch step replicates and presents nothing; the round-end step runs in one server step, bound to the deciding slot's latest death write by `CauseKey` (or inline when the slot staged none),** and calls `ICrowdControl::Halt()` plus the kind-blind stop-and-blend for every live pawn — source: ADR-0002
- **Every queue item payload, staged-write payload, `FPawnHandle` and `FSubjectRef` is declared in `StealthFoundation/Public/Server/`**; feature timers map onto `FTimerPayload` — source: ADR-0002, ADR-0007
- **Interfaces held by `TWeakInterfacePtr` are `UINTERFACE`s; plain cores take explicitly registered raw pointers** — source: ADR-0002
- **Client presentation schedules only from `EstimatedServerNow()`; the clock-exchange RPCs carry only timestamps** — source: ADR-0002

**Tuning**
- **Every gameplay number is a field of a `_Public` or `_Server` tuning sub-struct**; a hardcoded gameplay literal is a defect — source: ADR-0004
- **Server code reads only `GetServerTuning()`/`FValidatedTuning`; client and presentation code reads only `GetClientTuning()`** — source: ADR-0004
- **Invariant `Check` functions are pure and take `const` input; only F-1 changes a value (to the key's hard range); invariants reject or warn, never repair** — source: ADR-0004
- **Every reject/warn invariant carries its GDD ID; a new cross-field constraint goes through a TDC change request and a row in `TuningInvariants.cpp`** — source: ADR-0004
- **`Action`-latched values are captured into the action record at start on server and owning client** — source: ADR-0004
- **The schema table is regenerated by the commandlet and committed with every tuning field change; `SchemaVersion` is bumped when the fingerprint changes** — source: ADR-0004
- **Hot-reload, `?Tuning=` and console-command code sits inside `WITH_EDITOR` / `!UE_BUILD_SHIPPING` blocks (also `!UE_BUILD_TEST` for `stealth.tuning.preset`)** — source: ADR-0004
- **The tuning hash is the project's own FNV-1a-64 over the F-2 byte stream, with the committed golden fixture** — source: ADR-0004
- **Server tuning-ready bookkeeping is keyed by `ParticipantId`, never replicated, cleared at every epoch** — source: ADR-0004

**Session and travel**
- **The game mode derives from `AGameModeBase`** — source: ADR-0005
- **The round acknowledgement has two stages: `ClientRequestRoundAck`/`ServerRoundAck` (hashes) and, once per epoch in `Preparing`, `ServerWorldReady(FAckEpoch)` when the client holds `ExpectedPawnCount` pawns; Session raises `OnParticipantWorldReady`; the second stage never fails and never disconnects** — source: ADR-0005
- **`HandleStartingNewPlayer` is a no-op; every pawn comes from `UStealthPawnFactory`** — source: ADR-0005, ADR-0010
- **Authoritative match state lives only in the four game-instance stores**, keyed by `ParticipantId`, never by controller or PlayerState pointer — source: ADR-0005
- **A rejoiner adopts its retained PlayerState in `InitPlayerState`**; `OnDeactivated` does not destroy the PlayerState while admission is `RejoinOnly` — source: ADR-0005
- **`AStealthPlayerState::ShouldBroadCastWelcomeMessage` returns false in every phase** — source: ADR-0005
- **`InitNewPlayer` does not call `Super` and reads only `IdentityKey`, `Name` and `RejoinToken`; `FRoundParams` is written in `InitGameState`** — source: ADR-0005
- **Every Session RPC is owner-only; the participant is derived from the receiving connection** — source: ADR-0005
- **Every wait is bounded by a server-measured clock** — source: ADR-0005
- **Ack records, kick list, silence clocks and connection maps are keyed by `ParticipantId` (or `UNetConnection` for the live mapping) and never replicated** — source: ADR-0005
- **Host commands are registered on the game instance with the net-mode/host check** — source: ADR-0005
- **`EndMatch` runs on every session exit path** — source: ADR-0005
- **Login overrides use `PostLogin`/`OnPostLogin`** — source: ADR-0005

**Seeds and random streams**
- **Every draw comes from `FSecretStream` (hidden decisions, from `GetServerSubSeed`) or `FIntStream` (public geometry and decoration only)** — source: ADR-0006
- **Ranged draws use `UniformBelow`/`UniformInRange`; shuffles are Fisher-Yates over canonically sorted arrays** — source: ADR-0006
- **A new hidden purpose appends a new `ESeedDomain` value and stream tag, recorded by ADR amendment** — source: ADR-0006
- **Continuous values (server-only, secret streams only) map one 64-bit draw to `u = ((Bits >> 11) + 0.5) · 2^-53`, apply a closed-form inverse CDF (normal by AS241/PPND16), and convert once to an integer tick or millisecond count clamped to tuning bounds** — source: ADR-0006
- **Generator code runs inside `FPublicOnlyScope`** — source: ADR-0006
- **Golden vectors (SipHash, PCG32, Mix32, seed derivation, Generator goldens) are committed and run on every CI platform**; changing a primitive replaces its vectors in the same change — source: ADR-0006
- **Tests inject entropy through `IEntropySource`** — source: ADR-0006

**Modules and boundaries**
- **Code lives in one of the six modules (`StealthFoundation`, `StealthGameplay`, `StealthPresentation`, `StealthGame`, `StealthGameEditor`, `StealthTestHarness`)**; a new module needs an ADR amendment — source: ADR-0007
- **Every header in a `Public/` folder sits under `View/` (client-safe) or `Server/` (server-only)**, except the module header and `StealthServerOnlyGuard.h` — source: ADR-0007
- **Every `Public/Server/` header includes `StealthServerOnlyGuard.h` first** — source: ADR-0007
- **`StealthPresentation` declares `STEALTH_PRESENTATION_MODULE=1` as a `PrivateDefinitions` entry, never a `PublicDefinitions` entry** — source: ADR-0007
- **Every UCLASS in a `Server/` header declares `NotBlueprintType, NotBlueprintable`; every server UINTERFACE declares `meta=(CannotImplementInterfaceInBlueprint)`** — source: ADR-0007
- **Project header file names are unique across all modules** (`Stealth` prefix plus system name) — source: ADR-0007
- **A service with both faces is split into a `View/` facade and a `Server/` implementation** — source: ADR-0007
- **Lower layers declare the interfaces they consume; a type a Foundation container names is declared in Foundation** — source: ADR-0007
- **Server wiring happens only in `AStealthGameMode::InitGame` and the GameMode's `OnWorldBeginPlay` handler; client wiring only in `StealthPresentation` through `UStealthPresentationBootstrap::Bind`** — source: ADR-0007
- **Subsystem ordering uses `Collection.InitializeDependency<T>()`** — source: ADR-0007
- **Registrations are weak (`TWeakInterfacePtr`/`TWeakObjectPtr`), cleared in `Deinitialize`/`EndPlay`; a dead registration is a fault-sink report, never a crash** — source: ADR-0007
- **Queue, Roster, tuning validator, seed mixer and appearance deck are plain C++ cores (`F…Core`) with no `UWorld` dependency, behind a subsystem facade** — source: ADR-0007
- **New code logs with `UE_LOGF` and a `LogStealth…` category** — source: ADR-0007
- **Event Log types are plain C++ (no `USTRUCT`/`UCLASS`/`UPROPERTY`) with no text conversion besides the `Ended`/`Reset` disk writer** — source: ADR-0007
- **A reflective or Blueprint request for a server-only value on a client returns invalid and uses `ensure`, never `check`** — source: ADR-0007
- **Debug commands such as `stealth.round.dumplog` are `#if !UE_BUILD_SHIPPING` and refuse unless the phase is `Ended`/`Reset`** — source: ADR-0007
- **A Blueprint-referenced class that moves module ships with its `[CoreRedirects]` entry in the same change; affected Blueprints are resaved by explicit path** — source: ADR-0007
- **The include lint (rules a-j) runs in CI and pre-commit** — source: ADR-0007

**Tests**
- **Every Logic story ships T1 tests; every Integration story ships a T2 or T3 test or a documented playtest** — source: ADR-0008
- **Every leak, parity, statistical and lint gate includes a negative control the suite detects** — source: ADR-0008
- **Test names: `StealthGame.<System>.<Scenario>` (T1), `NETWORK_TEST_CLASS(<Scenario>, "StealthGame.<System>.PIE")` (T2), `StealthGame.<System>.MP.<Scenario>` (T3); runs select with `^StealthGame.` or `Group:`** — source: ADR-0008
- **T2 tests set `WithGameMode(AStealthGameMode)` and load their fixture map; travel scenarios are T3** — source: ADR-0008
- **T1 tests live in the owning module's `Private/Tests/<System>/`; T2 and T3 code lives in `StealthTestHarness`** — source: ADR-0008
- **T2/T3 tests bound by frames or server time and assert and record the active replication system** — source: ADR-0008
- **Lints and analysis scripts emit observations only; pass/fail lives in the gate script** — source: ADR-0008

**Event Log telemetry section**
- **The telemetry section is exported only after termination (phase `Ended`/`Reset` or the `SessionEnded` latch), by the Event Log store's disk writer, off the game thread from an owned snapshot, via a `.tmp` file renamed on completion** — source: ADR-0023
- **Detector thresholds are code constants under `DetectorVersion`; changing one bumps the version** — source: ADR-0023
- **The section is compiled out of Shipping and is a null object when `-StealthTelemetry=0`** — source: ADR-0023
- **The ADR-0007 lint includes every `FTelemetry*` type name and `FPlaytestTelemetryCore`** — source: ADR-0023

**Version control (process)**
- **`.uasset` and `.umap` are stored via LFS (or the chosen system's binary storage) and are lockable** — source: ADR-0009
- **Until a locking-capable remote exists, only one session saves `Content/` assets at a time; until VCS exists, every editor edit batch is preceded by a `_snapshots/` copy including `Config/` and `StealthGame.uproject`** — source: ADR-0009
- **Once locking exists, lock an unmergeable asset before editing and unlock after commit/abandon; agents lock explicit paths before saving through MCP tools; the pre-commit lock check passes** — source: ADR-0009
- **A redirector is committed together with the moved asset and its referencers** — source: ADR-0009
- **Before the first commit the template's Godot/Unity ignore sections are removed and `build/`/`bin/` anchored or removed (`git check-ignore -v Build/Scripts/x.cs` prints nothing); `.gitattributes` lists one pattern per line** — source: ADR-0009
- **Commits use Conventional Commits with a story/task ID and a design-doc/ADR reference** — source: ADR-0009
- **The Roster teammate entry carries Team-audience `bReconnecting`, derived only by the Roster push step from Session's `Connection` projection (true while `Disconnected` inside `DisconnectGraceSec` during `Active`; false otherwise)** — source: ADR-0001, ADR-0005 (user decision 2026-10-25)
- **`UTeamPingViewComponent` (`FTeamPingView`, a fixed-size array with one slot per team member, no `RoundEpoch` of its own) is a C-TeamView part written only by the Informant Pings server handler, cleared at `OnRosterReset`, travel and Round's round-end step** — source: ADR-0001, ADR-0002 (user decision 2026-10-25)
- **`FRoundResult.TeamSummary[2]` is computed in the latch step and published only in the round-end step (all zero before)** — source: ADR-0002 (user decision 2026-10-25)

### Forbidden Approaches

- **Never branch on `IsPlayerControlled()`, `IsBotControlled()`, controller class, `PlayerState` presence, team or Roster state** in pawn, component, AnimBP or audio code for anything another client can observe — source: ADR-0001
- **Never add an NPC subclass, an NPC-only component, a lightweight NPC pawn or a Mass crowd representation** — every difference is a Pillar-1 detector — source: ADR-0001
- **Never use the per-actor active-override toggle for the owner-only pointers** — source: ADR-0001
- **Never read `Owner` or `PlayerState` on a pawn the owner does not currently possess** — source: ADR-0001
- **Never put team, state, connection, appearance or pawn data on a PlayerState; never give NPCs a PlayerState** — more leak surface for the same work — source: ADR-0001
- **Never change the in-round player array; never enter the engine spectator state** — source: ADR-0001
- **Never use a team-filtered actor, a per-team replica actor or a team relevancy rule** — source: ADR-0001
- **Never enable ReplicationGraph** — it filters actors, not properties, and conflicts with the cull path — source: ADR-0001, ADR-0003
- **Never switch to Iris now** — off by default in 5.8.4, Beta-flagged, condition behaviour unverified; deferred to ADR-0024 — source: ADR-0001, ADR-0003
- **Never feed anything except screen height and the eye-mask rendered flag into animation tiering, URO, budget allocation, mesh LOD or audio concurrency; never a gameplay fact** (no AI-specific animation budget or LOD) — source: ADR-0001
- **Never carry `T_due` anywhere but the owner's C-TeamView owner part (no reply entry, pawn field or client derivation)** — source: ADR-0001, ADR-0019
- **Never give a pawn dormancy, a cull distance, `bOnlyRelevantToOwner`, `bNetUseOwnerRelevancy`, a per-instance priority or team relevancy; never change net config per instance, kind or on possession** — source: ADR-0003
- **Never call `ForceNetRelevant()` or `ForceActorRelevantNextUpdate()` on a pawn** — source: ADR-0003
- **Never rely on the relevancy override alone to cull** — the channel closes 1-6 s late with jitter (a timing channel) — source: ADR-0003
- **Never hide concealment client-side only or replicate a concealed flag** (internal development switch only); never use dormancy or distance culling for concealment — source: ADR-0003
- **Never re-open a culled pawn in a strike's outcome slot; a killed-concealed victim re-opens only at the death write, with `bDiedConcealed = true`** — source: ADR-0003
- **Never skip `FActionState.StartServerTime` in an `FActionState` write, and never send a feed entry without advancing its `Seq`** — source: ADR-0003, ADR-0017, ADR-0018
- **Never mutate gameplay state outside the drain; never resolve inline in an RPC body or AI source** — source: ADR-0002
- **Never order events with a feature-local clock, timestamp, tick-function order or `FTimerManager`** — cross-actor tick order is unspecified and join-order biased — source: ADR-0002
- **Never use `FTimerManager`, a feature tick or a latent action for a gameplay timer** — source: ADR-0002
- **Never order commands by client-supplied or estimated send time (lag compensation)** — spoofable timing channel, contradicts Round R12 — source: ADR-0002
- **Never re-submit or re-stamp an outcome** — source: ADR-0002
- **Never use the public seed, join order or a rotating index for permutations** — source: ADR-0002
- **Never end the round in one step at the latch, or through an `OnRoundEnded` subscriber chain** — reveals the end before contact; subscriber order is unspecified — source: ADR-0002
- **Never use untyped deferred-lambda "pending flag" writes; never apply a staged write inline from the staging resolver** — source: ADR-0002
- **Never destroy actors inline in a resolver** — source: ADR-0002
- **Never stop human pawns at round end by input loss** — source: ADR-0002
- **Never schedule client presentation from `GetServerWorldTimeSeconds()`, the arrival time of a replicated value or a client wall clock** — uncorrected one-way lag, float delta — source: ADR-0002
- **Never send gameplay data in the clock-exchange RPCs** — source: ADR-0002
- **Never route queue Event Log entries through `UE_LOG`/`UE_LOGF`, the output log or trace channels** — source: ADR-0002
- **Never declare a type that `FQueueItem` or `FStagedWrite` names in a feature module** — source: ADR-0002, ADR-0007
- **Never mark tuning types `Config`/`DefaultConfig`/`GlobalUserConfig`/`PerObjectConfig`; never read a gameplay value from `UDeveloperSettings`, ini, `Saved/Config`, command line (Shipping) or a console variable** — a live back door (TDC CR-13) — source: ADR-0004
- **Never let client or presentation code include a `Public/Server/` tuning header or reference `UStealthTuningData`; never let server code read the raw asset** — source: ADR-0004
- **Never give a tuning accessor a pawn, controller, team, viewer or participant parameter** — source: ADR-0004
- **Never read UPROPERTY metadata at runtime for ranges** — `WITH_METADATA` is editor-only, so cooked builds lose the clamp — source: ADR-0004
- **Never store tuning as `UDataTable` rows or per-domain assets** — no typed client face; one preset must be one validation unit — source: ADR-0004
- **Never repair a failed invariant by clamping a dependent key** — source: ADR-0004
- **Never deliver the tuning snapshot by reliable client RPC** — source: ADR-0004
- **Never silence a safe-range lint finding by editing the baseline without citing a GDD change request or OQ** — source: ADR-0004
- **Never hash with `GetTypeHash`, `HashStringFNV1a` or a struct memory image** — source: ADR-0004
- **Never derive from `AGameMode`; never use `AddInactivePlayer`/`FindInactivePlayer` or address-keyed reclaim** — fights Round's phases, dedupes by address, leaks disconnects — source: ADR-0005
- **Never call `RestartPlayer` or `SpawnDefaultPawnFor`** — source: ADR-0005, ADR-0010
- **Never store anything authoritative on a PlayerController, PlayerState, GameMode or GameState; never carry match state across travel on them** — source: ADR-0005
- **Never toggle `bIsInactive` or remove a PlayerState from the array between `Preparing` and the next travel; never spawn a fresh PlayerState for a reclaim** — source: ADR-0005
- **Never put a secret in a travel URL or login option; client code never parses travel URL options** (read `FRoundParams`) — source: ADR-0005
- **Never use a multicast Session RPC** — source: ADR-0005
- **Never let a client message start, extend or restart a deadline** — source: ADR-0005
- **Never let Session spawn, free, hide or possess a pawn, or call `RequestTransition`** — source: ADR-0005
- **Never hard-travel every round** (fallback only if spike S3 fails), **never run rounds as level streaming in one persistent world, never keep the PlayerController across seamless travel** — source: ADR-0005
- **Never register host commands on a PlayerController, pawn, HUD or cheat manager** — source: ADR-0005
- **Never override `DispatchPostLogin`** (deprecated 5.6) — source: ADR-0005
- **Never use `FMath::Rand`, `FMath::FRand`, `FMath::RandRange`, `FRandomStream`, `std::rand`, `<random>` engines or time-seeded randomness in gameplay or server code** — `FRandomStream` is an invertible 32-bit LCG with float-derived ranges (CI grep) — source: ADR-0006
- **Never log, trace, replicate, put in a URL or expose to Blueprint `ServerSecret`, `MatchNonce`, server sub-seeds or stream keys** (only digests and `(Domain, Attempt, StreamTag, counter)` tuples) — source: ADR-0006
- **Never reuse or renumber an `ESeedDomain` value** — source: ADR-0006
- **Never use `%` on a draw, floats in a ranged draw, or an unordered container as draw input** — source: ADR-0006
- **Never take a continuous draw on `FIntStream`, on a client, or to pick among discrete options** — source: ADR-0006
- **Never use PCG32 (or any invertible PRNG) for secret draws; never use OpenSSL/PlatformCrypto DRBG or Blake3 keyed mode as the stream** — source: ADR-0006
- **Never let a file under `CityGenerator/` include `Public/Server/Random/`** — source: ADR-0006, ADR-0007
- **Never let a `Public/View/` header include a `Server/` header** — source: ADR-0007
- **Never let a file under `StealthGameplay/*/Core/` include a `Features/` path; never let a lower layer name a feature type** — source: ADR-0007
- **Never put a header in a module's root `Public/` folder** (other than the module header / guard) — source: ADR-0007
- **Never put `BlueprintCallable`, `BlueprintPure`, `BlueprintReadOnly`, `BlueprintReadWrite` or `BlueprintAssignable` on a member of a server type** — source: ADR-0007
- **Never register an implementation from `IModuleInterface::StartupModule` or from a feature's own `BeginPlay`** — source: ADR-0007
- **Never list `StealthTestHarness` in a Target.cs `ExtraModuleNames`; no runtime module depends on `StealthTestHarness` or `StealthGameEditor`** — source: ADR-0007
- **Never pass Event Log content, `DealDigest`, server sub-seeds, bindings or mailbox order to `UE_LOG`/`UE_LOGF`/`ensureMsgf`/`checkf`/`ClientMessage`/`TRACE_*`/`CSV_*`/`SCOPE_CYCLE_COUNTER` names** — source: ADR-0007
- **Never use `UE_LOG` in new code** (allowed only in pre-ADR code until rewritten) — source: ADR-0007
- **Never promote prototype code (`Source/StealthGame`) into a production module before its governing ADR is Accepted; promotion is a rewrite, not a file move** (`UStealthSettings` and `StealthRules` are replaced) — source: ADR-0007, ADR-0001, ADR-0010, ADR-0011, ADR-0020, ADR-0021
- **Never split layers into seven runtime modules, keep the single prototype module, use Game Feature plugins per feature, or mark Presentation `ClientOnly` now** — source: ADR-0007
- **Never use wall-clock time as a test pass/fail predicate** (only as a failure ceiling); never use wall-clock waits, unseeded randomness or shared mutable state in tests — source: ADR-0008
- **Never compile negative-control types or test seams into Shipping** (`StealthTestHarness` or `WITH_STEALTH_TEST_SEAMS` only) — source: ADR-0008
- **Never make a gate depend on the MCP toolsets or Python editor scripting** — the editor script guard gives false results — source: ADR-0008
- **Never disable, skip or reseed a failing test to pass a gate; never mark a gate Done that was never watched to fail** — source: ADR-0008
- **Never prove multi-process criteria in in-process PIE only; never select tiers with `Automation RunFilter`** (it pulls in engine tests) — source: ADR-0008
- **Never let gameplay, AI, HUD or presentation code read the telemetry section; never change tuning at runtime from it** — source: ADR-0023
- **Never add a replicated property, RPC, tuning key or `USTRUCT` for telemetry** (a change request to the user) — source: ADR-0023
- **Never pass telemetry content to `UE_LOG`/`UE_LOGF`, `ensureMsgf`/`checkf`, `ClientMessage`, `TRACE_*`, `CSV_*` or a stat/scope name; never write telemetry through a log category, file logger, engine analytics provider or Insights channel; never add telemetry entries to the R19 ring** — source: ADR-0023
- **Never export `DealDigest`, mailbox order, sub-seeds, display names, identity keys, addresses, ping or per-participant `AppearanceId`** — source: ADR-0023
- **Never initialise a repository, commit, push, or change `.gitignore`/`.gitattributes` (agents) without explicit user instruction** — source: ADR-0009
- **Never use Make Writable; never save an asset on the never-resave list (reject a commit modifying one); never run redirector fix-up on sample folders** — source: ADR-0009
- **Never commit `Binaries/`, `Intermediate/`, `DerivedDataCache/`, `Saved/`, `_snapshots/`, `_backup_pre_rebase/`; never commit binaries to plain Git without LFS** — source: ADR-0009
- **Never add a teammate connection field other than the one `bReconnecting` bit (no ping, time, reason or address), and never send it to a non-teammate** — source: ADR-0001, ADR-0005 (user decision 2026-10-25)
- **Never write a non-zero `TeamSummary` (or any per-team count) into `FRoundPublicState` before `Ended`** — source: ADR-0002 (user decision 2026-10-25)

### Performance Guardrails

- **Queue drain + staging + teardown + Roster flush**: ≤ 0.5 ms listen-host game thread — source: ADR-0002
- **Clock exchange**: < 50 B/s per client after warm-up — source: ADR-0002
- **Server frame**: the drain runs once per server frame after CMC and AI ticks; listen-host frame time ≤ 1/60 s (p99 under the profiled load), measured by the harness frame-time capture, not a `NetServerMaxTickRate` setting — source: ADR-0002, ADR-0003
- **Replication**: relevancy override O(1) per pawn-connection pair, inside the 1.5 ms replication line; pawns at a fixed 15 Hz within 36 KB/s worst case (60 pawns; working value since `NpcCount` 50); fallback feed ≤ 104 B/s per client — source: ADR-0003
- **Tuning**: 0 per frame; validation < 1 ms at `Preparing`; < 100 KB memory; snapshot ≤ 2,048 bytes per client per round — source: ADR-0004
- **Session**: silence clocks and ack bookkeeping inside the 0.5 ms line; travel load bounded by `TravelLoadTimeoutSec` (60 s) — source: ADR-0005
- **Random streams**: hidden draws well under 0.05 ms/frame — source: ADR-0006
- **Identity policy**: C-TeamView pushes change-only, ≤ 1 per frame, within the 50 KB/s per-client budget — source: ADR-0001
- **Tests**: per-commit pipeline ≤ 10 min; nightly cook + T3 estimated 1-2 h [U] — source: ADR-0008
- **Telemetry**: ≤ 0.15 ms mean / ≤ 0.3 ms p99 listen-host GT (inside the 2.0 ms residual line); export snapshot ≤ 0.2 ms GT once per round; ≤ ~1.1 MB per round — source: ADR-0023

### Engine API Constraints (5.8.4)

- **Net-config members are setter-only since 5.5**: use `SetNetUpdateFrequency`/`SetMinNetUpdateFrequency`/`SetNetCullDistanceSquared`; `NetServerMaxTickRate` via `GetNetServerMaxTickRate`/`SetNetServerMaxTickRate` — source: ADR-0003
- **`RemoteViewPitch16` / `SetRemoteViewPitch(double)` (5.6)** replace the 8-bit `RemoteViewPitch` — source: ADR-0001, ADR-0003
- **`APawn::PlayerState` is private**: reset it by name with `ResetReplicatedLifetimeProperty` (a misspelled name is a fatal startup error, not a compile error); `RESET_REPLIFETIME_CONDITION` works only for accessible members — source: ADR-0001
- **A listen server is not clamped by `NetServerMaxTickRate`** unless `bClampListenServerTickRate` is set; the harness asserts `NetClientTicksPerSecond` ≥ `MaxConnections × MaxFps` — source: ADR-0002, ADR-0003
- **`UE_LOGF` (5.8)** is the logging macro for new code — source: ADR-0007
- **C++20 is the floor (5.8)**; `PrivateDefinitions` reach every TU of the module under unity build and shared/private PCH (keep the CI self-test) — source: ADR-0007
- **`TargetConfigurationDenyList: ["Shipping", "Test"]` on the `StealthTestHarness` `.uproject` entry**; `ExtraModuleNames` has no configuration check — source: ADR-0007
- **CQTest `FPIENetworkComponent` + `FNetworkComponentBuilder`** for T2; `EAutomationTestFlags` is an enum class and every test needs an application-context flag; one `Automation RunTests` per `-ExecCmds` — source: ADR-0008
- **Seamless travel is disabled in single-process PIE** unless `net.AllowPIESeamlessTravel=1`; travel results come from multi-process standalone — source: ADR-0005
- **`FFileHelper::SaveStringToFile` with `EEncodingOptions::AutoDetect` switches to UTF-16 on non-ANSI text**; telemetry export uses `ForceUTF8WithoutBOM` — source: ADR-0023
- **Spikes owning [U] claims**: S1, S2, S14 (ADR-0001); drain spawn/estimator (ADR-0002); S9, S10, S12, S13, fixed-width byte capture (ADR-0003); T-1 snapshot atomicity (ADR-0004); S3, S4, S5 (ADR-0005); harness CQTest headless and capture (ADR-0008) — source: ADR-0001..0008

---

## Core Layer Rules

*Applies to: the Shared Pawn and movement, possession, the pawn command interface and input, the character pool, animation and significance, the layout service, the camera*

### Required Patterns

**Shared Pawn, movement and possession**
- **The Shared Pawn uses `UStealthMovementComponent` (a CMC subclass)** — source: ADR-0010
- **Every pawn is created by `UStealthPawnFactory::SpawnSharedPawn`**: deferred spawn → `ApplyAppearance` → `BindPawn` (if a participant) → `FinishSpawning` → possess, in one call stack; a bind rejection destroys the pawn before `FinishSpawning` — source: ADR-0010
- **Every spawn sets `Instigator == Self` and spawns no replicated child actor** — source: ADR-0010
- **`TakeOverCrowdPawn` checks eligibility and calls `BindPawn` itself; no card move is committed unless it returned `Applied`** — source: ADR-0010
- **Every `AStealthPawn` carries the same default subobjects**, including a non-replicated `UGameplayTasksComponent` and `UStealthMotionWarpingComponent` with `WarpTargets` replication disabled — source: ADR-0010, ADR-0016, ADR-0018
- **`Possess`/`UnPossess` are called only by the factory (spawn, hand-over pass incl. `ReleaseBody`, takeover) and `AStealthPlayerController::PawnLeavingGame`, always through `FScopedPossessionParity`** — source: ADR-0010
- **After every possession the control rotation is set from the pawn's `HeadLook` world rotation** — source: ADR-0010
- **A live pawn's possession change is committed in a queue slot, bumps the control epoch, waits for landing if `IsInTraversal`, and executes in the hand-over pass at the next frame start**; the last-command hold covers every frame until the new controller's first command or `HandoverHoldSec` — source: ADR-0010
- **`PawnLeavingGame` hands the pawn to an `AStealthCrowdController` in the same frame** without issuing a command or changing the binding; it unpossesses only during world teardown or seamless travel and does not bump the control epoch — source: ADR-0010
- **Movement shaping is one function per rule for every pawn**, reads only tuning values, and runs identically on the server and in owner prediction; shaping state used by prediction is saved/restored in `FSavedMove_Stealth` — source: ADR-0010
- **The gait clamp's participant-state input comes only from `IGaitLegalitySource`** — source: ADR-0010
- **Starvation zeroes acceleration in `ForcePositionUpdate`; `AGameNetworkManager` forced-update and discrepancy values come from `Config/DefaultGame.ini` and equal the SCL tuning keys (boot test); excess late moves are discarded** — source: ADR-0010
- **The AI → player hold simulates with `PerformMovement`** — source: ADR-0010
- **Traversal starts only from a server-validated queue slot and writes `TraversalState` inside the `TraversalStateWrite` forced-update scope; one proxy root-motion policy applies to every pawn** — source: ADR-0010
- **`FellOutOfWorld` and `OutsideWorldBounds` use `RequestTeardown`** — source: ADR-0010

**Pawn command interface and input**
- **Every pawn carries exactly one non-replicated `UPawnCommandComponent`; every discrete command from any source enters through its entry points and the queue; only `UPawnCommandComponent::WriteActionState` writes `FActionState`** — source: ADR-0011
- **Human Move/Look travel only in `ServerMove`; discrete commands only in `AStealthPlayerController::ServerPawnCommand`; `PawnRef` travels as a NetGUID value** — source: ADR-0011
- **The listen host's input goes through `IngestCommand(HostLocal)` with the same ingress and reply flush** — source: ADR-0011
- **`PossessionEpoch` is server-owned and delivered only by `ClientSetPossessionEpoch` before `ClientRestart`; ingress stamps the controller's recorded `PossessedControlEpoch`, never the pawn's current control epoch** — source: ADR-0011
- **Ingress drops are silent; stage 2 runs only at the command's queue slot and in the stated order; the per-pawn token bucket is consumed only on execution** — source: ADR-0011
- **A resolution-voided command produces exactly one reply, flushed at the end of the drain's resolution step in one owner-only RPC per connection; `Rejected_Latched` replies `NotNow` through the held-reply buffer** — source: ADR-0011
- **Hold ends are `ScheduleAt` queue items; the release debounce is server-side and identical for every pawn** — source: ADR-0011
- **Feature predicates are registered weakly from the composition root per `EActionType`** — source: ADR-0011
- **Action-hub events fire only inside queue slots and are native (not Blueprint-assignable)** — source: ADR-0011
- **Owner-only data (`FHoldStatus`, rejection codes, owner results, cooldown end) lives on the PlayerController carrier family** — source: ADR-0011
- **Gameplay input is bound on `AStealthPlayerController`; held commands map `Started` → Begin and both `Completed` and `Canceled` → End; server-meaningful actions are Boolean** — source: ADR-0011
- **Ends are idempotent per Begin; a Common UI switch to `Menu` produces the Ends synchronously; `UPawnCommandClient` sends End for every outstanding Begin before any context swap, menu open, focus loss, possession change or `ClientRestart`; context changes set `bIgnoreAllPressedKeysUntilRelease`** — source: ADR-0011
- **The Attack payload comes only from Camera's `GetAimRay()`; `PawnRef` is honoured only through `CanViewerKnow` and `PawnRefAgreementCm`** — source: ADR-0011, ADR-0015
- **Rebinding uses `UEnhancedInputUserSettings`** — source: ADR-0011

**Character pool and appearance**
- **The pool is one `UCharacterPoolData` asset with hard references to every mesh and static material instance, held by `UCharacterPoolSubsystem` for the session** — source: ADR-0012
- **One merged single-slot skeletal mesh per model/accessory variant and one `UMaterialInstanceConstant` per appearance** — source: ADR-0012
- **`AppearanceId` is written only by `UStealthPawnFactory` (via `ApplyAppearance(Id)`) before `FinishSpawning`, exactly once, replicated `COND_InitialOnly` with a RepNotify; `ApplyAppearanceVisuals` is the only function that sets a pawn's mesh or material** — source: ADR-0012
- **The pawn class default and its Blueprint child have no skeletal mesh assigned; mesh components keep an empty per-component `SkinCacheUsage` override array** — source: ADR-0012
- **Every allocator draw comes from `FSecretStream(GetServerSubSeed(AppearanceDeal, 0), Tag("CPA_DEAL"|"CPA_REPL"))` with `UniformBelow`/`FisherYates` over sorted inputs** — source: ADR-0012
- **The deal input is an opaque set of `FParticipantId`, sorted by the facade** — source: ADR-0012
- **`VacateCard` is called for every pawn death or removal with no kind parameter; `TakeNpcCard`, `ReleaseCard` and `RecoverOwnCard` run in the same server call stack as their Roster counterpart; a `DrawReplacement` ticket commits in the same stack as its `SpawnSharedPawn`** — source: ADR-0012
- **Clients and the listen host warm all `A` pool rows before Session's acknowledgement; `r.PSOPrecache.ProxyCreationStrategy` is 1 and asserted at startup** — source: ADR-0012
- **Character-pool tests live in `Source/StealthGameplay/Private/Tests/CharacterPool/` (editor validator tests in `StealthGameEditor`); G-DECK seeds are a `constexpr` array** — source: ADR-0012

**Animation and significance**
- **Every Shared Pawn uses the one AnimBP, the one PoseSearch database set and the one chooser set** — source: ADR-0013
- **Engine URO is off (`bEnableUpdateRateOptimizations = false`) for every pawn mesh; external tick-rate control is on for every pawn on non-authority clients (`AlwaysTickPose`, accumulated `ExternalDeltaTime`, alpha set every frame) and off on authority** — source: ADR-0013
- **Tier inputs are only `h` (view depth and FOV) and Camera's eye-mask flag; ties break on the client-local `TierTieKey`** — source: ADR-0013
- **Motion-matching search cadence is time-based (`SearchThrottleTime` in seconds)** — source: ADR-0013
- **On authority every pawn uses `AlwaysTickPoseAndRefreshBones` at rate 1; root-motion, traversal and `PoseLocked` states run at rate 1 on every client; a D-culled pawn's montage position is re-derived from replicated state at promotion** — source: ADR-0013
- **Footsteps come only from `UStealthStepClockComponent` using the rendered smoothed position; every footstep uses `SC_Footsteps`, one sound class, one attenuation, one priority, no per-pawn parameter** — source: ADR-0013
- **`C_anim` is measured per ADR-0013 §5 (GT + worker sum, p95, Insights) and reported with the GT share** — source: ADR-0013

**Layout service**
- **Round and every feature reach layout only through `UStealthLayoutService`** — source: ADR-0014
- **The only runtime navmesh build trigger is `RemoveNavigationBuildLock(ENavigationBuildLock::Custom, ELockRemovalRebuildAction::Rebuild)` by the layout service** — source: ADR-0014
- **Everything spawned after `OnLayoutReady` has `CanEverAffectNavigation` false on every component** — source: ADR-0014
- **Clients build only from a complete `FLayoutManifest`; `AStealthLayoutGeometry` and every layout-derived component are `bReplicates = false`, created locally** — source: ADR-0014
- **Kit instances are added with batch `AddInstances(…, bUpdateNavigation = false)`, before component registration where possible** — source: ADR-0014
- **Every V-* predicate exists once, written against `ILayoutMeasureBackend`** — source: ADR-0014
- **Time slicing bounds work by `LayoutSliceWorkUnitsLimit`; a wall-clock guard may only defer work** — source: ADR-0014
- **Draw sites use the ADR-0006 shapes and stream tags `LSC_SITE`, `LSC_PLCE`, `LSC_ZONE`, `LSC_EMRG`, `LSC_RSPN`** — source: ADR-0014
- **Graybox marker classes are editor-only (`StealthGameEditor`, `bIsEditorOnlyActor`); the baked asset stores records as an opaque versioned byte blob** — source: ADR-0014
- **`bAllowClientSideNavigation=false`** (clients hold no navigation data) — source: ADR-0014

**Camera**
- **The camera is computed only in `AStealthPlayerCameraManager`** — source: ADR-0015
- **`bUseClientSideCameraUpdates = false` and `bAutoManageActiveCameraTarget = false` on `AStealthPlayerController`** — source: ADR-0015
- **On a non-local controller the camera manager computes only the eye-only POV** — source: ADR-0015
- **The mask reads only geometry (positions, capsules, `ViewBlock` traces)**; no pawn is drawn in third person without a passing eye-origin trace; distance changes only the test rate — source: ADR-0015
- **α = 0 hides via `SetVisibility(false, Propagate)`; pawn primitives keep `bCastHiddenShadow` and `bAffectIndirectLightingWhileHidden` false** — source: ADR-0015
- **The dither uses the single `CPD_EyeMaskAlpha` custom-primitive-data slot** — source: ADR-0015
- **The camera writes `MaskAlpha` to the pawn's local presentation state; the Shared Pawn's apply function arbitrates with Concealment's latch** — source: ADR-0015
- **A pawn with α = 0 or blocked from the eye produces a `TargetSpot` in the aim ray** — source: ADR-0015
- **Pawnless viewers use the client-local `AStealthNeutralViewActor`** — source: ADR-0015
- **Every sight test (camera mask, aim world test, LSC sight predicates) uses the `ViewBlock` channel** — source: ADR-0015
- **`IA_WalkSlower` (`IMC_Common`; Left Ctrl 0.8, Left Alt 0.9) only scales the keyboard Move vector on the owning client before `ServerMove`; SCL quantises the received magnitude to the shared pace set** — source: ADR-0011 (user decision 2026-10-25)
- **`IA_Ping` (`IMC_Common`) sends only `AStealthPlayerController::ServerTeamPing(FNetTeamPing {Point})` (point only), reliable, validated server-side in order by phase, role, range from the eye pivot (`PingRangeCm + PingRangeSlackCm`) and then its own `ParticipantId`-keyed token bucket (`PingBucketSize`, `PingBucketPerSec`, tolerance 0.05 token); failures drop silently and only an accepted ping spends a token; the ping point is the `ViewBlock` aim-ray hit dropped to walkable ground, range measured from the eye pivot on client and server** — source: ADR-0011 (user decision 2026-10-25)

### Forbidden Approaches

- **Never reference a Mover, ChaosMover or NetworkPrediction type** from code, Blueprint or asset — Experimental, PhysicsMover removed in 5.8, parity facts are CMC-verified — source: ADR-0010
- **Never create or possess a pawn with `RestartPlayer`, `SpawnDefaultPawnFor`, `SpawnDefaultPawnAtTransform`, `SpawnDefaultController` or a `SpawnActor` of a pawn class outside the factory** — source: ADR-0010
- **Never call `BindPawn` from a takeover caller** — source: ADR-0010
- **Never add a component on possession** — source: ADR-0010
- **Never call `StopMovementImmediately`, `SetDefaultMovementMode` or `Super::Restart` from `AStealthPawn::Restart`; never zero velocity in `PawnClientRestart`; never mask engine possession behaviour after the fact** — authoritative state replicates before any mask — source: ADR-0010
- **Never set control rotation from the actor rotation after possession** — source: ADR-0010
- **Never set `bOnlyAllowAutonomousTickPose` or change server pose ticking per controller** — source: ADR-0010, ADR-0013
- **Never destroy the pawn in `PawnLeavingGame`; never destroy-and-respawn on disconnect or takeover** — a vanishing/appearing pawn identifies a human — source: ADR-0010
- **Never read controller kind, PlayerState or team in a movement rule; never read a PlayerState for the gait clamp** — source: ADR-0010
- **Never move a pawn through `RequestDirectMove`/`RequestPathMove` or AI path following (`bUseAccelerationForPaths`)**; AI movement enters only through `UPawnCommandComponent::Move` — source: ADR-0010
- **Never replay excess late moves as a catch-up** — source: ADR-0010
- **Never advance the CMC's client timestamps or call `ForcePositionUpdate` during the AI → player hold** — the server would reject the owner's first moves — source: ADR-0010
- **Never use the sample's `Traversal_ServerImplementation`/`TraversalResult` replication or a multicast for traversal** — source: ADR-0010
- **Never replicate `IsRunning`, `IsInTraversal` or `CommandedDisplacementCm`** (owner-local copies for prompts only) — source: ADR-0010
- **Never destroy a pawn inside a resolver except via `RequestTeardown`; never destroy a Shared Pawn in `Reset`** — source: ADR-0010
- **Never execute a hand-over inline in the queue slot** — source: ADR-0010
- **Never carry caller identity (`IsAI`, `IsPlayer`, controller, PlayerState, team) in an entry point, payload, event or listener; the command component never calls `IsPlayerControlled`, `IsLocallyControlled`, `IsBotControlled` or a Roster binding query** — source: ADR-0011
- **Never add a Move/Look RPC or a Look event; never add a gameplay RPC on the pawn; the client never names the pawn it commands; `PawnRef` is never an object-pointer field** — source: ADR-0011
- **Never special-case `NM_ListenServer` in input ingress** — source: ADR-0011
- **Never treat a resolved `PawnRef` as evidence of visibility** — source: ADR-0011
- **Never validate everything at ingress (single stage)** — breaks source parity and the total order — source: ADR-0011
- **Never rate-limit `End` per pawn or drop it at the transport bucket when it matches an outstanding Begin** — source: ADR-0011
- **Never let a reply code depend on a target pawn's identity, team, concealment or brace state; never deliver replies as replicated owner-only properties** — source: ADR-0011
- **Never time a hold on the IIF's own tick, `FTimerManager` or a client timestamp; never debounce releases client-side** — source: ADR-0011
- **Never let `AllowNoEffect` vs `Reject` depend on anything but the site and legality class** — source: ADR-0011
- **Never relay action-hub events to a client except through the Visibility Policy** — source: ADR-0011
- **Never put owner-only data on the pawn** — source: ADR-0011
- **Never use Hold/HoldAndRelease/Tap/Pulse/Chorded triggers for server-meaningful actions** — the client would own hold timing — source: ADR-0011
- **Never let the condition sweep cancel a hold itself** (it schedules `HoldTimer(ConditionCancel)` items) — source: ADR-0011
- **Never add lock-on, snapping, magnetism or aim assist** — source: ADR-0011, ADR-0015
- **Never use `UPlayerMappableInputConfig` or the Raw Input plugin; never make a gameplay command a Common UI action** — source: ADR-0011
- **Never load a pool asset (`LoadSynchronous`, `RequestAsyncLoad`) from gameplay code after the front end; never use soft references with a per-round load of only the active `K` rows** — timing reveals the subset — source: ADR-0012
- **Never create a `UMaterialInstanceDynamic`, set a material parameter, scale, tint or add a component per pawn** (lint: `CreateDynamicMaterialInstance`/`SetScalarParameterValue`/`SetVectorParameterValue` under `Core/SharedPawn` and `Core/CharacterPool`) — source: ADR-0012
- **Never read controller, team, binding, local-ness, `BeginPlay` state or viewer in `ApplyAppearanceVisuals`** — source: ADR-0012
- **Never set a mesh in the pawn Blueprint or its construction script** — source: ADR-0012
- **Never consume the reserved `CPA_TAKE` stream tag** — source: ADR-0012
- **Never change `D` or any `c_a` after the deal; never draw a replacement uniformly from the active set** (breaks conservation, F-6) — source: ADR-0012
- **Never warm only the active `K` rows** — source: ADR-0012
- **Never log, replicate or reflect the root sub-seed, stream state or participant-to-card map** (only `DealDigest`, active set, counts, extras, flags, server-only) — source: ADR-0012
- **Never ship the `Warn` floor policy, the entropy-injection hook or an `AppearanceId` debug overlay in Shipping** (`CI.ShippingSymbolAudit`) — source: ADR-0012
- **Never build appearances at run time (leader-pose, skeletal mesh merge, Mutable), use a `UDataTable` pool, or replicate the deck** — source: ADR-0012
- **Never select an animation asset, layer or graph by controller, team, participant state or `AppearanceId`** — source: ADR-0013
- **Never use `USkeletalMeshComponentBudgeted`, the AnimationBudgetAllocator or the SignificanceManager for pawns** (ADR revision required); never use engine URO — it is player-keyed in 5.8.4 — source: ADR-0013
- **Never let anything kind-varying (spawn index, NetGUID, actor name, registration order, controller) enter a tier decision; never tier by world distance alone** — source: ADR-0013
- **Never use a frame count for motion-matching search cadence** — source: ADR-0013
- **Never let gameplay rules read anim notifies, bone/socket transforms or curves** — source: ADR-0013
- **Never apply a performance fallback to some pawns or clients only, drop character shadows, or adapt to who is on screen** — source: ADR-0013
- **Never drive footsteps from anim notifies** — audibility would depend on tier — source: ADR-0013
- **Never cite RTX 5090 numbers as acceptance evidence** — source: ADR-0013
- **Never ship the development-only `WasRecentlyRendered` rendered-flag fallback** (remove once ADR-0015's flag exists) — source: ADR-0013
- **Never let Round and the Generator include each other's headers** — source: ADR-0014
- **Never call `UNavigationSystemV1::Build()` at runtime** (blocks the game thread) — source: ADR-0014
- **Never let anything affect navigation after the freeze**: no dynamic obstacle or nav modifier on post-`OnLayoutReady` spawns; no code other than the layout service adds or removes a navigation build lock; never use a fully dynamic navmesh — source: ADR-0014
- **Never let clients run the Generator, draw geometry randomness or hold navigation data** — source: ADR-0014
- **Never spawn a replicated actor from the layout contract** — source: ADR-0014
- **Never add kit instances with per-instance `AddInstance` loops or one actor per module** — source: ADR-0014
- **Never fork predicate logic between the grid and world backends** — source: ADR-0014
- **Never add a field to `FLayoutManifest` beyond §8.1** (Visibility Policy change request + schema change, always-ask) — source: ADR-0014
- **Never replicate the Manifest as a single `TArray`, or send it by reliable RPCs** — source: ADR-0014
- **Never cook graybox markers as runtime actors or put them in a server-only data layer** — source: ADR-0014
- **Never log `FSiteAssignment`, zone → team, respawn picks or any sub-seed value outside the server-only Event Log header** (attempt records, failed predicate ids, `LayoutAttempt`, `LayoutHash`, `Seed` may go to `LogStealthLayout`) — source: ADR-0014
- **Never put a `UCameraComponent`, `USpringArmComponent` or Gameplay Cameras component on the Shared Pawn or its Blueprints** — source: ADR-0015
- **Never re-enable `bUseClientSideCameraUpdates` or call `ServerUpdateCamera`** — source: ADR-0015
- **Never run the boom, collision, mask or any trace for a non-local controller** — source: ADR-0015
- **Never let the mask read team, binding, participant state, controller kind or a server-only header** — source: ADR-0015
- **Never skip the eye-origin test by distance** — source: ADR-0015
- **Never toggle `CastShadow` per frame; never hide masked pawns with per-pawn material instances** — shadows/GI/RT presence remain — source: ADR-0015
- **Never call `SetVisibility` on a pawn from the camera directly** — source: ADR-0015
- **Never use spectator pawns, spectator state or spectator RPCs** — source: ADR-0015, ADR-0001
- **Never introduce a second "sight" trace channel; never use GPU occlusion queries or scene depth for sight tests** — source: ADR-0015
- **Never add aim assist, snapping, magnetism, slowdown, zoom, shake or FOV punches to the camera** — source: ADR-0015
- **Never disable the mask in first person; never add a server-side per-viewer view check in MVP** (deferred to ADR-0025) — source: ADR-0015
- **Never route a ping through `ServerPawnCommand`, the action slot, the resolution queue or `FActionState`, never name a pawn in it, and never change the sender's pawn (no RPC, field, pose, gesture, `ForceNetUpdate`, world sound or effect)** — Pillar 1 — source: ADR-0011, ADR-0001 (user decision 2026-10-25)
- **Never send the walk-slower modifier to the server as a command or field, and never apply it to gamepad input** — source: ADR-0011 (user decision 2026-10-25)
- **Never bind `ShowTeamNames` in the MVP** (deferred to VS) — source: ADR-0011, ADR-0021 (user decision 2026-10-25)

### Performance Guardrails

- **Movement shaping**: well under 5 % of the 3.0 ms CMC game-thread line (40 server-simulated NPCs + proxies) — source: ADR-0010
- **Pawn network**: `TraversalState` 7 bytes per change; total within the ≤ 36 KB/s pawn line (SCL F9) — source: ADR-0010
- **Command interface**: stage 2 a few µs/frame at the IIF F3 worst case (≈ 5 items/frame), inside the 0.5 ms queue line; uplink ≤ 20 reliable RPCs/s per connection (transport bucket); at most one reply RPC per connection per drain — source: ADR-0011
- **Character pool**: deal ≤ 0.2 ms once per round; client `ApplyAppearance` ≈ 2-5 ms one-off at round start [U]; pool ≤ 40 MB textures; `AppearanceId` ≈ 100-150 B per client per round, 0 B/s steady — source: ADR-0012
- **Animation**: `C_anim` ≤ 4.0 ms (GT + workers, p95) and GT ≤ 2.0 ms at 60 characters on an RTX 3060-class PC at 1080p; tier A ≤ 8; tier subsystem ≤ 0.05 ms GT — source: ADR-0013
- **Layout (server, `Preparing`)**: game-thread frames ≤ 33 ms; Generator worker ≤ 1 s per attempt; kit spawn + collision + navmesh ≤ 3.0 s wall time (graybox navmesh ≤ 1 s); Validate + Select ≤ ~1.5 s in ≤ 4 ms GT slices; ≤ 45 s worst case including reseeds — source: ADR-0014
- **Layout (server, `Active`)**: no navmesh rebuild; runtime services ≤ 0.1 ms average and ≤ 0.5 ms in a respawn-pick frame; ≤ 1 pick per frame; ≤ ~400 `ViewBlock` traces per pick — source: ADR-0014
- **Layout (clients)**: Manifest build in ≤ 10 ms slices, ≤ 3 s p95; ≤ 5,000 instances, ~48 ISM components, ≤ 20,000 collision shapes; Manifest 13.5-31.5 KB once per round (≤ 4 chunks), zero layout traffic in `Active` — source: ADR-0014
- **Camera**: mask ≤ 0.5 ms GT (≤ 160 traces per frame cap); boom 2 traces/frame; aim ray ≤ 4 traces per query; zero camera network traffic — source: ADR-0015

### Engine API Constraints (5.8.4)

- **`UMotionWarpingComponent::GetOwnerAdapter()`** replaces the removed `GetCharacterOwner` (5.8); **MotionWarping is Beta** (`WarpTargets` protected member and API may change — on the engine-upgrade checklist) — source: ADR-0010, ADR-0018
- **`AGameNetworkManager` forced-update values are read from the class default object**; `ForcePositionUpdate` advances `CurrentClientTimeStamp` and `check`s `AutonomousProxy` — source: ADR-0010
- **Enhanced Input flush CVar is `EnhancedInput.IgnoreHeldDigitalActionKeysOnFlush`** (default true, marked for removal); `bIgnoreAllPressedKeysUntilRelease` covers Boolean actions only; `Completed` does not fire if a trigger reports `Ongoing` the same frame; Common UI flushes pressed keys on the next tick after `Menu` — source: ADR-0011, ADR-0021
- **`UEnhancedInputUserSettings`** replaces `UPlayerMappableInputConfig` (deprecated 5.3) — source: ADR-0011
- **`USkeletalMeshComponent::SetSkeletalMeshAsset`** (not the deprecated `USkinnedMeshComponent::SetSkeletalMesh`); the `bool` overload of `HasPendingInitOrStreaming()` is deprecated in 5.8 — source: ADR-0012
- **PSO component precache is off under `GIsEditor`**: PIE results are not evidence for warm-up — source: ADR-0012
- **PoseSearch `FAnimNode_MotionMatching::SearchThrottleTime`/`bUseInertialBlend` and `USkinnedMeshComponent` external tick-rate control** are the animation APIs; 5.8 removed deprecated animation symbols from 5.0-5.6 — source: ADR-0013
- **`au.EnableRelativeRenderCostVoiceLimit` defaults to 1** (footstep parity is verified under it) — source: ADR-0013
- **`SetNavigationAutoUpdateEnabled` is editor-only; `IsNavigationDirty()` misses areas queued under a build lock** (use `GetNumDirtyAreas()`); `DefaultMaxRepArraySize` is not enforced — the binding limit is the 64 KB constructed bunch — source: ADR-0014
- **`USceneComponent::SetVisibility(bool, EVisibilityPropagation)`** overload; `EFirstPersonPrimitiveType` exists but is not used — source: ADR-0015
- **Spikes owning [U] claims**: traversal, S8, S14, H1, ST1, P1 (ADR-0010); V1-V6 (ADR-0011); CPA-V1..V5 (ADR-0012); R3, ANIM-V1..V6 (ADR-0013); R4, freeze, chunked Manifest (ADR-0014); camera mask spike OQ5/OQ10 (ADR-0015) — source: ADR-0010..0015

---

## Feature Layer Rules

*Applies to: City Generator, Crowd AI, Concealment, Assassination and Defence Reversal, NPC-Kill Penalty and Informants, Objectives, telemetry probes*

### Required Patterns

**City Generator (layout provider)**
- **Providers read only `FLayoutAttemptParams`** — source: ADR-0014
- **The Generator runs as a single cancellable worker task over pure data, inside `FPublicOnlyScope`** — source: ADR-0014, ADR-0006

**Crowd AI**
- **Crowd code reaches a pawn only through `FCrowdCommandSink`/`FCrowdMotionSink`, which call only `UPawnCommandComponent` entry points** — source: ADR-0016
- **`AStealthCrowdController` does not create a `PathFollowingComponent`** (fallback if verification (2) fails: create it, never tick it, never request a move; the AC-CNB-02 tick counter reads 0) — source: ADR-0016
- **Avoidance is expressed as `Move` offsets (`bUseRVOAvoidance` false for every pawn)** — source: ADR-0016
- **The crowd AIController writes `Look` yaw and pitch into the control rotation in its `UpdateControlRotation` override** — source: ADR-0016
- **Every sink call checks the pawn's current control epoch; async path results also match the request's leg id** — source: ADR-0016
- **StateTree assets use the crowd schema with `ScheduledTickPolicy = Allowed` and are recompiled after any schema change; the Director asserts `StateTree.Component.ScheduledTickEnabled` at init; the StateTree component ticks in `TG_PrePhysics` after the Director and before the controller** — source: ADR-0016
- **Decision cadence is one global `CrowdDecisionPerSec` on a per-pawn phase-aligned grid, re-imposed after every tree tick; reaction and behaviour delays live in the controller's per-frame due-time list** — source: ADR-0016
- **The controller steps its due-time list and follower before `Super::Tick`; the virtual camera, input quantisation and `Move`/`Look` emission run every controller tick** — source: ADR-0016
- **Driver changes go through `UStealthPawnFactory::RequestHandOver` and ADR-0010's hand-over pass; `ICrowdControl` only registers the driver** — source: ADR-0016
- **Halt and handover are direct calls that latch the sink and stop or switch the tree; Halt latches every sink synchronously inside the round-end step, before any further crowd tick can run** — source: ADR-0016
- **Perception uses only public facts and server line of sight, excluding pawns `ICrowdPerceptionFilter` marks not perceivable / not an obstacle, identically for every kind** — source: ADR-0016
- **The driver kind comes from the `ICrowdControl` call** — source: ADR-0016
- **Ambient schedule events are `TimerFired(CrowdSchedule)` queue items; emergence spawns go through `RequestEmergence` and `SpawnSharedPawn`; absorptions through `RequestTeardown`** — source: ADR-0016
- **Every behavioural draw is `FCrowdDraws::Draw`/`DrawBelow`/`DrawWeighted(Ordinal, Decision, ...)` on domain `CrowdBehaviour`** (rejection retries extend the tuple); continuous samples use `UnitFraction53` and the named inverse CDFs with golden vectors; schedule draws use `AmbientSchedule` counter-as-Attempt; start placement uses `NpcRoutes`; decision counters are keyed by ordinal in the Director — source: ADR-0016
- **Rates and profiles come from the frozen tuning snapshot and `UCrowdMotionProfileData`, referenced by the registered key `CrowdMotionProfileAsset`; the two StateTrees by `CrowdFullStateTreeAsset` / `CrowdRestrictedStateTreeAsset`** — source: ADR-0016, ADR-0004
- **Legs along a `CrowdRoute` edge follow the edge's pre-baked path (server-only, stored with the route graph); other legs use `FindPathAsync`** — source: ADR-0014, ADR-0016
- **A cancelled schedule item resolves `Voided(Revalidation)` by ledger re-check** — source: ADR-0016
- **Cost cuts are global values from the fixed cut order** — source: ADR-0016
- **Crowd diagnostics use `UE_LOGF` with no participant, team or binding data** — source: ADR-0016

**Concealment**
- **`UConcealmentComponent` exists on every pawn with identical class and flags and never replicates** — source: ADR-0017
- **Bushes are built locally from the Manifest on every machine** — source: ADR-0017
- **Every Hide enters through `UPawnCommandComponent`** — source: ADR-0017
- **Commit, crossing and end are `TimerFired(ConcealmentTransition)` queue items carrying the transition generation** — source: ADR-0017
- **Cull state is pushed only through `IConcealmentCullSource` inside the slot; un-cull happens in the same slot as exit execution or break; a concealed kill un-culls at the death write together with `FDeathState.bDiedConcealed = true`** — source: ADR-0003, ADR-0017
- **Owner/Team `bIsConcealed` is set at commit and cleared at the crossing; for a killed-concealed pawn it clears at the server body removal or the participant's rebind, whichever comes first** — source: ADR-0017
- **Client presentation hides and shows bodies only through `EvaluateBodyHidden` from `EstimatedServerNow()`; every non-owner pawn actor is created presentation-hidden and evaluated in `PostNetInit`; clips are seeked, never restarted** — source: ADR-0017
- **A fresh non-owner actor starts hidden before any RepNotify and does not clear while a living teammate entry's `PawnRef` is unresolved on that client** — source: ADR-0017
- **Hiding applies the full §6 flag set and the presentation aim channel response** — source: ADR-0017
- **Collision state is computed per simulated move from that move's own server-mapped timestamp (or a saved-move field) on server and owner, written only on change** — source: ADR-0017
- **`bCastHiddenShadow`, `bAffectIndirectLightingWhileHidden` and `bRayTracingFarField` are false on the class defaults of every pawn primitive (audited)** — source: ADR-0017
- **After the latch, transition items complete only through the ADR-0002 post-latch exemption; a guard-failure exit is an internal transition** — source: ADR-0017
- **The spot pick uses the secret `BushSpotPick` stream with an unbiased ranged draw** — source: ADR-0017
- **Concealment log rows go to the Event Log store only** — source: ADR-0017

**Encounter outcome timing (Assassination and Defence Reversal)**
- **`FActionState` serialises to exactly 208 bits (incl. `StartServerTime`) and `FDeathState` to exactly 65 bits (incl. `bDiedConcealed`) for every value; `FStrikeResult` carries `ContactServerTime` for every `Result`** — source: ADR-0018
- **`FActionState.Seq` advances by one on every write of that pawn** — source: ADR-0018
- **At the Attack's slot only the attacker's `FActionState` (and its movement/root-motion state) changes on the wire** — source: ADR-0018
- **`WarpPoint` is derived only from an honoured `PawnRef`'s extrapolation or the submitted `WorldPoint` (clamped)** — source: ADR-0018
- **Every outcome-revealing write (`FDeathState`, `Disabled`, re-open, `Counter`) is a typed staged write at `ContactServerTime − StrikeNetLeadSec`**, decides nothing new and runs after the latch — source: ADR-0018
- **`VacateCard`, Roster transitions, strike accounting and holds' server cancels happen in the slot** — source: ADR-0018
- **An NPC victim is driven exactly as if the strike had whiffed until its death write** — source: ADR-0018
- **`FStrikeResult` is sent for every executed Attack and `FDefenceResult` for every reversal by a bound defender, as entries of the ADR-0011 reply batch, same shape for every result** — source: ADR-0018
- **Client presentation goes through one idempotent `Refresh(EstimatedServerNow())` called from every RepNotify, re-open and the host's direct path** — source: ADR-0018
- **Own-role presentation subscribes only to `OnRoleChangePresented`** — source: ADR-0018
- **Result plates are matched to their strike positionally on the pawn handle captured at the press** — source: ADR-0018
- **Bodies are released from their controller at `DeathServerTime + DeathFallSec` and torn down after linger and grace through the queue, identically for every victim kind** — source: ADR-0018

**Informants (Penalty)**
- **Penalty state lives only in `FPenaltyCore` keyed by `ParticipantId`** — source: ADR-0019
- **Every Penalty mutation runs inside a queue slot** (an `OnPawnKilled` follow-on, a Roster-event flush, an `InformantRespawn` item, or an `IInformantBodyProvider` call from Round's slot) — source: ADR-0019
- **The exile runs `RequestTransition(StrikeLimit)`, `ReleaseCard`, `AdoptReleasedPawn` and `RequestHandOver(Ai)` in one call stack inside the killing Attack's slot** — source: ADR-0019
- **A respawn only takes over a pawn returned by `PickRespawnPawn`, the only random draw of the sequence** — source: ADR-0019
- **Commit order: pick → eligibility pre-check → `TryReleaseNpc` → `TakeOverCrowdPawn` → (`Applied` only) `TakeNpcCard` → `RecordAppearance`, in one call stack**; a released pawn not taken over is handed back with `AdoptReleasedPawn` in the same stack — source: ADR-0019
- **Respawns for `Active → Informant` causes are scheduled only from Round's `OnInformantAwaitingBody` call** — source: ADR-0019
- **Reclaim recomputes `T_due` from the kept `t_lost` and `t_floor`; retries reuse the request's `RespawnRequestSeq`; widening comes only from LSC** — source: ADR-0019

**Objectives**
- **`FSiteAssignment` is read only through `UStealthLayoutService::GetSiteAssignment_Server()` inside `UObjectivesService`/`FObjectivesCore`** — source: ADR-0020
- **Team-audience objective data travels only in `UObjectiveTeamViewComponent::View`, written only by `UObjectivesService` inside a queue slot, the `OnRoundActive` step or the Roster rebind handler; every write marks the push-model property dirty** — source: ADR-0020
- **The Interact predicate reads only the court, its claim count and the pawn's legality class, and returns `Reject(NotNow)` or `Allow`** — source: ADR-0020
- **The hold duration is one value for every court (`GetInteractHoldSec()` has no court parameter)** — source: ADR-0020
- **Completion is judged only in `OnInteractEnded(Completed)` inside the `HoldTimer(Complete)` slot, against live state, with gate (a)-(e); the final calls `DeclareObjectiveWin(Team, CurrentKey())` in that slot** — source: ADR-0020
- **The rival delay is drawn only from `FSecretStream(GetServerSubSeed(StageSignal, Ordinal), Tag("OC_STAGE"))` in integer milliseconds; release times lie on the grid `t0 + k·Q` anchored at `ActivePhaseStart`** — source: ADR-0020
- **Card text and clue phrases are rendered client-side from cooked data; only indices and enums travel** — source: ADR-0020
- **Objectives Event Log entries go through the Round Event Log `Append` only** — source: ADR-0020
- **Gate (c) compares the control epoch recorded at Begin to the current one and requires `!IsHandOverPending`; an interact-end call outside a slot only releases the claim** — source: ADR-0020

**Telemetry probes and detectors**
- **Probe call sites are on the path every pawn takes; detectors run the same code at the same cadence for every Shared Pawn, read only published server state, keep one exposure clock per subject key (pawn + binding epoch) and close open records on a binding change** — source: ADR-0023
- **Crowd pace: `CrowdFullPacePct` 55 and C4 group-drift pace matching active (the interim full-pace rule is lifted because IIF R15a exists)** — source: ADR-0016 (user decision 2026-10-25)

### Forbidden Approaches

- **Never let a provider call `GetServerSubSeed`, read `FSiteAssignment`, or read world state other than its own geometry** — source: ADR-0014
- **Never bound the Generator candidate loop by wall clock; never time-slice the Generator on the game thread** (fallback only) — source: ADR-0014
- **Never call `MoveTo*`, `AITask_MoveTo`, a StateTree MoveTo task, `RequestDirectMove`, `AddMovementInput`, `SetActorRotation`, `LaunchCharacter` or write CMC velocity from Crowd code** — path following bypasses the command interface (a statistical tell) — source: ADR-0016
- **Never use `SetFocus`/`SetFocalPoint` or `FaceRotation` in the crowd controller** — the engine zeroes pitch — source: ADR-0016
- **Never give the NPC sink an `Attack` method** (the restricted sink exposes only `Move` and `Look`) — source: ADR-0016
- **Never time crowd delays in decision-tick counts or the engine timer manager** — source: ADR-0016
- **Never call `Possess`/`UnPossess` from Crowd code** — source: ADR-0016
- **Never rely on a StateTree event alone for Halt or handover; Halt never ends holds or brakes pawns** — source: ADR-0016
- **Never use `UAIPerceptionComponent` or team affiliation** — source: ADR-0016
- **Never include Roster, Objectives-assignment or Visibility Policy fact headers in Crowd code** — source: ADR-0016
- **Never use `FMath::Rand*` or `FRandomStream` in Crowd code** — source: ADR-0016
- **Never adapt crowd rates or profiles during a round; never make a cost cut depend on distance, visibility, viewer or kind, change replication settings or lower `NpcCount`** — source: ADR-0016
- **Never reschedule a `Rejected_Latched`/`Rejected_StaleEpoch` item** — source: ADR-0016
- **Never route crowd Event Log entries through `UE_LOG`/`UE_LOGF`** — source: ADR-0016
- **Never ship a crowd UI, debug draw or indicator in Shipping** (AC-CNB-25) — source: ADR-0016
- **Never use Mass Entity/Mass Crowd or Detour Crowd; never use BT/StateTree with stock `MoveTo`; never put crowd timers on `FTimerManager`; never replace device-shaped input with a turn-smoothing filter** (fallback only) — source: ADR-0016
- **Never add a concealment field to the pawn's replicated set; never replicate a concealed flag** — source: ADR-0017
- **Never spawn a replicated actor for concealment; never use per-bush replicated actors** — source: ADR-0017
- **Never let Hide legality, replies or timing read occupancy, controller kind or team; never add a dwell, capacity, reveal or timeout key** — source: ADR-0017
- **Never drive concealment transitions or the hide by anim notifies, engine timers, feature ticks or overlap** — source: ADR-0017
- **Never re-open a killed-concealed pawn before the death write, and never draw its body on a non-owner client** — source: ADR-0017
- **Never use `SetActorHiddenInGame`/`bHidden` for concealment** — `bHidden` replicates and feeds relevancy — source: ADR-0017
- **Never toggle `bCastHiddenShadow`, `bAffectIndirectLightingWhileHidden` or `bRayTracingFarField` at runtime** — source: ADR-0017
- **Never store collision state from a notify** — source: ADR-0017
- **Never let a guard-failure exit be a `PawnCommand`** — source: ADR-0017
- **Never use `bServerAcceptClientAuthoritativePosition` for the owner warp** — source: ADR-0017, ADR-0003
- **Never use `RoundSeed`, a public seed or `mod` for the bush spot pick** — source: ADR-0017
- **Never read a hide-pose start from `ContactServerTime`; read `FActionState.StartServerTime`** — source: ADR-0017, ADR-0018
- **Never log concealment rows through `UE_LOG`/`UE_LOGF`; never ship `bConcealServerCull = false` outside internal development builds** — source: ADR-0017
- **Never use an optional, conditional or magnitude-packed field in `FActionState` or `FDeathState`** — source: ADR-0018
- **Never let `FActionState.Seq` carry, derive from or correlate with a `ClientCommandSeq`** — exposes command cadence NPCs cannot reproduce — source: ADR-0018
- **Never change anything on the victim or a defender before the death write** — source: ADR-0018
- **Never read a culled or concealed pawn's position for `WarpPoint`** — source: ADR-0018
- **Never replicate the outcome at the slot and delay client presentation; never multicast a strike event or spawn a replicated effect actor** — source: ADR-0018
- **Never run `VacateCard`, Roster transitions or strike accounting in the staging pass** — source: ADR-0018
- **Never send any outcome field besides `FStrikeResult`/`FDefenceResult`** — source: ADR-0018
- **Never present an outcome on receipt, snap, or use arrival time except as the matcher's floor and lost-data cap** — source: ADR-0018
- **Never react to the arrival of the Roster's owner state in own-role presentation** — source: ADR-0018
- **Never show the attacker's plate from an inferred contact time when `FStrikeResult.ContactServerTime` is present (the matcher is fallback and defender-only)** — source: ADR-0018
- **Never set a strike or counter warp target through `UMotionWarpingComponent`'s replicated `WarpTargets`** — source: ADR-0018
- **Never release or remove bodies with engine timers or by destroying the possessed pawn** — source: ADR-0018
- **Never make Penalty state a replicated property; never let `StrikeCount` reach a client before `Ended`** — source: ADR-0019
- **Never use an engine timer, feature tick or latent action in Penalty** — source: ADR-0019
- **Never leave a surviving exiled body `Retired`** — source: ADR-0019
- **Never spawn a pawn for a respawn, re-possess a dead body, or keep the participant's old look** — source: ADR-0019
- **Never commit a card move for any takeover result other than `Applied`; never move the card before the takeover** — source: ADR-0019
- **Never schedule a respawn from a Penalty subscription to Roster state events, or when latched** — source: ADR-0019
- **Never schedule a timer for a respawn cause on a `Disconnected` participant; never restart or skip the delay on reclaim** — source: ADR-0019
- **Never relax the unseen test** — source: ADR-0019
- **Never write the tally to C-TeamView on a strike; `T_due` reaches only its owner, through the owner part** — source: ADR-0019
- **Never add an informant-only movement, collision, sight or region rule; never have Penalty call `BindPawn` itself** — source: ADR-0019
- **Never spawn an actor for a court, objective or site, replicated or not** (court fixtures stay local non-replicated layout components) — source: ADR-0020
- **Never give Objectives a tick function, `FTimerManager` timer or latent action** (only `TimerFired(StageSignal)` items) — source: ADR-0020
- **Never include the Objectives `Public/Server/` headers from client modules, Crowd, IIF or Presentation** — source: ADR-0020
- **Never put objective data on the GameState, `FRoundPublicState`, a PlayerState, a pawn, a fixture or an RPC; never put it in the Roster's C-TeamView part** — source: ADR-0020
- **Never put a locked site, an opponent site, opponent progress other than `RivalPrelimCount`, anyone's hold progress, or the final before the team's preliminaries are complete in the view** — source: ADR-0020
- **Never return `AllowNoEffect` at Begin from the Interact predicate** — source: ADR-0020
- **Never set a per-court hold duration** (`FObjectiveDef` has no hold-time field since 2026-10-25) — source: ADR-0020
- **Never let a completion cancel another pawn's hold; never judge completion from an `OnActionCompleted` subscription** — source: ADR-0020
- **Never use per-team timers, a fixed delay or the engine timer manager for the rival signal** — source: ADR-0020
- **Never replicate card text (`FText`) or clue strings** — source: ADR-0020
- **Never log site ids, the assignment or draws outside the Event Log; never `UE_LOG` objective entries** — source: ADR-0020
- **Never branch on player vs NPC in Objectives except the gate's server-only queries (§5.4) and the bound-only `TendCompleted` Event Log entry** — source: ADR-0020
- **Never port the prototype `AStealthObjective` or its replicated `bCompleted`/`InteractorCount`** — source: ADR-0020
- **Never make a probe conditional on controller kind, team or participant state; never pass or compute a kind or team in producers or detectors; never rely on owner-pushed tells instead of detectors; never record telemetry client-side** — source: ADR-0023

### Performance Guardrails

- **Crowd**: ≤ 1.5 ms p99 / ≤ 0.8 ms mean per frame at 60 pawns, 60 fps (provisional pending the 60-character spike; F5 estimate ≈ 0.74 ms mean); < 10 MB memory; no network of its own — source: ADR-0016
- **Concealment**: negligible server CPU inside the 1.0 ms "camera traces + concealment/IIF/objectives misc" line; client hide/show rebuilds the render proxy (profile in R5); one owner-only bool on the PlayerController — source: ADR-0017
- **Encounter**: µs per Attack inside the 0.5 ms queue line; 18 + 8 bytes replicated state per pawn; one forced update per strike and per death-write pawn — source: ADR-0018
- **Penalty**: O(1) inside the 0.5 ms queue line; the respawn pick is ADR-0014's budget (≤ 1 pick per frame) — source: ADR-0019
- **Objectives**: ≤ 0.05 ms average on the listen host; ≈ 16 bytes per view change per team member — source: ADR-0020
- **City Generator**: worker ≤ 1 s per attempt off the game thread (J ≤ 8 candidates) — source: ADR-0014

### Engine API Constraints (5.8.4)

- **StateTree (no engine-reference page; facts from 5.8.4 source)**: `UStateTreeComponent::ScheduleTickFrame` is protected; scheduled ticks honoured only when `ScheduledTickPolicy` allows (cached on the compiled tree — recompile after schema change); `StateTree.Component.ScheduledTickEnabled` off forces every-frame ticks; the component overwrites its schedule after every tree tick; brain components tick in `TG_DuringPhysics` unless set — source: ADR-0016
- **`AAIController::OnPossess` creates a replicating `UGameplayTasksComponent` if the pawn has none** — hence the default non-replicated one on every pawn — source: ADR-0016, ADR-0010
- **`FindPathAsync` results arrive ≥ 2 frames after a `TG_PrePhysics` request** — source: ADR-0016
- **Mass Gameplay / Mass Crowd are Experimental in 5.8** — source: ADR-0016
- **A primitive hidden with `SetVisibility(false)` gets no render proxy unless `bCastHiddenShadow`, `bAffectIndirectLightingWhileHidden` or `bRayTracingFarField` is set; `AActor::bHidden` replicates and feeds `IsNetRelevantFor`** — source: ADR-0017
- **`UMotionWarpingComponent` replicates `WarpTargets` by default (`COND_SimulatedOnly`)**; disable it in the subclass with `DISABLE_REPLICATED_PROPERTY_FAST` (leaves a `COND_Never` row) — source: ADR-0018
- **Fixed-width `NetSerialize`: return value means "mapped" (return `true`); `Identical` must compare every field incl. `Seq`; use `SerializeBits` (the base archive writes 32 bits for `SerializeInt`) and test width on `FNetBitWriter`** — source: ADR-0018
- **Destroying a possessed pawn runs `AController::PawnPendingDestroy`** (behaviour differs by controller kind) — source: ADR-0018
- **Push model: `Net.IsPushModelEnabled` defaults false; the registered sub-object list is off by default** — source: ADR-0020
- **Spikes owning [U] claims**: 50-character crowd spike and verifications (1)-(8) (ADR-0016); S-CON-1, R5, S9/S10, OQ9 (ADR-0017); byte capture, S10, plate order (ADR-0018); takeover invisibility, H1 (ADR-0019); OBJ-V1..V3 (ADR-0020); TEL-V1..V3 (ADR-0023) — source: ADR-0016..0020, ADR-0023

---

## Presentation Layer Rules

*Applies to: HUD and teammate markers, menus, UI scale, rendering configuration, per-viewer presentation*

### Required Patterns

- **Every HUD and marker layer is a UMG widget added with `AddToPlayerScreen` at its HUD-R4 ZOrder** — source: ADR-0021
- **Widgets apply a view-model value only when it differs from the last applied value** — source: ADR-0021
- **Layers below ZOrder 100 are `HitTestInvisible`, non-focusable, with no input bindings** — source: ADR-0021
- **All HUD widgets are created once at HUD init and toggled by visibility** — source: ADR-0021
- **World-anchored drawing (markers, interact ring, context prompt) happens in `SStealthWorldAnchorLeaf::OnPaint` from the rendered frame's camera view; the leaf is volatile and never inside an invalidation or retainer box** — source: ADR-0021
- **The screen fade is drawn by the HUD fade layer at ZOrder 20 from Camera's published fade** — source: ADR-0021
- **Presentation builds view-models by sampling its carriers every frame by value** (the listen host runs the same path) — source: ADR-0021
- **Presentation timing uses only `EstimatedServerNow()`; plate and card timelines are sampled from `now − t0` in C++** — source: ADR-0021, ADR-0002
- **Widgets are dumb (no UMG property bindings, no Blueprint logic beyond layout and style); `FHudViewModel::Dump()` is deterministic** — source: ADR-0021
- **Player-facing text comes from string tables** — source: ADR-0021
- **Common UI is used only for the menu root; menus open through `UStealthMenuStack::PushMenu`, which ends outstanding gameplay Begins synchronously first; `IMC_Menu` contains no gameplay action and is held only by the persistent menu root** — source: ADR-0021, ADR-0011
- **The Informant input context follows `PresentedOwnState` and is selected only by the §8 `UseInformantContext` function** — source: ADR-0021
- **`IsMarkedAlly` is served only from the local marker model built from the own controller's C-TeamView** — source: ADR-0021
- **DPI comes only from `UStealthDpiScalingRule` (`H/1080`); `s_ui` is applied once by the layer roots and the L0 leaf; `ApplicationScale` stays 1.0** — source: ADR-0021
- **Presentation reads only its own controller's carriers, the GameState public structs and client-local queries** — source: ADR-0001, ADR-0021
- **`AStealthHUD` and all client-side wiring live in `StealthPresentation`** — source: ADR-0007
- **UI Blueprints reference only classes and functions on the checked-in UI reference allowlist** (DataValidation audit fails the build) — source: ADR-0007
- **Every pawn primitive takes its render flags from the class default** — source: ADR-0022
- **Every pool mesh carries the same skin-cache value on every LOD (provisional `Enabled`; spike R2 decides)** — source: ADR-0022
- **Kit modules and large opaque props (> 2k tris, no WPO, not masked) enable Nanite** — source: ADR-0022
- **`M_Character_Master` is Masked with the `CPD_EyeMaskAlpha` dither as its only opacity input** — source: ADR-0022, ADR-0015
- **Performance claims come from cooked Development standalone captures on the reference preset** — source: ADR-0022
- **A fallback is applied to every client through shipped config** — source: ADR-0022
- **Each owner ADR's code declares its `Stealth.Budget.<Line>` Insights scope** — source: ADR-0022
- **Config keys ADR-0022 pins change only by a user-approved config change recorded against ADR-0022** — source: ADR-0022
- **Team ping rings, edge rings and sender labels are painted by the L0 world-anchor leaf from the own controller's `UTeamPingViewComponent`, beneath markers, collapsed with L0; each `(SenderHandle, PingSeq)` is shown once, for `PingLifetimeSec` from its `ServerTime` on `EstimatedServerNow()`; entries present at a rebuild are marked shown** — source: ADR-0021 (user decision 2026-10-25)
- **The round summary (HUD-E21) is two string-table lines filled from the integer `FRoundResult.TeamSummary`, shown with the end banner at its F10 time, only in `Ended`** — source: ADR-0021 (user decision 2026-10-25)
- **Team feed reconnecting/back lines come only from the teammate `bReconnecting` bit (HUD-R10a); the marker label adds "· reconnecting" and the marker shape never changes** — source: ADR-0021 (user decision 2026-10-25)

### Forbidden Approaches

- **Never use `AddToViewport` or the `AHUD` canvas for game UI** (lint) — the canvas cannot be ordered against the UMG fade — source: ADR-0021
- **Never make a layer below ZOrder 100 a `UCommonActivatableWidget`** — source: ADR-0021
- **Never create or destroy a widget during a round in response to gameplay** — source: ADR-0021
- **Never compute world-anchored positions in a UMG binding, widget tick or game-thread tick; never use one widget per marker positioned by tick** — one frame of lag — source: ADR-0021
- **Never add a `UWidgetComponent` or any other component to a pawn for presentation** — source: ADR-0021
- **Never depend on a RepNotify having fired; never drive widgets event-style from RepNotify/RPC handlers** — the host gets no RepNotify for its own writes — source: ADR-0021
- **Never sample plate or card timelines from widget animation assets** — source: ADR-0021
- **Never carry text on the wire** — source: ADR-0021
- **Never let Common UI add or remove a gameplay `IMC_*` context; never give a pushed menu an `InputMapping`; never route the HUD input layer through Common UI** — source: ADR-0021
- **Never drive the Informant context from the raw own `State`** — source: ADR-0021
- **Never send `IsMarkedAlly` to the server** — source: ADR-0021
- **Never call `SetApplicationScale`, apply `s_ui` inside the DPI rule, or use the engine DPI curve** — source: ADR-0021
- **Never iterate actors, read a pawn's `PlayerState`/`Controller`/`Owner`, call `IsPlayerControlled`/`IsBotControlled`/`CanViewerKnow`, include a `Public/Server/` header, or write anything replicated from Presentation** — source: ADR-0021, ADR-0001
- **Never use `TActorIterator`, `TActorRange`, `GetAllActorsOfClass`, `GetAllActorsWithInterface`, `GetPlayerControllerIterator`, `GetGameMode`/`GetAuthGameMode`, or `GetPlayerState`/`GetController` on a non-local pawn in `StealthPresentation`** (lint h) — source: ADR-0007
- **Never call `GetGameMode`, `GetAllActorsOfClass`/`WithInterface`, `GetPlayerState` on a pawn, `Get Player Controller` with a non-literal-0 index, or reference a `Server/` class from a UI Blueprint** — on the listen host these return authoritative objects — source: ADR-0007
- **Never construct presentation objects in `StealthGame` beyond setting `HUDClass`** — source: ADR-0007
- **Never port the prototype `AStealthHUD` drawing code** — source: ADR-0021
- **Never set a render flag, LOD bias, skin-cache usage or material on one pawn based on kind, team, binding or viewer** — source: ADR-0022
- **Never use `Auto` or per-component skin-cache overrides on pool meshes; never flip `r.SkinCache.DefaultBehavior` to Inclusive** — source: ADR-0022
- **Never enable Nanite on characters, bushes, decorative foliage or small props** (Nanite skeletal meshes are unproven; spike R6 may revisit) — source: ADR-0022
- **Never create a per-pawn MID for any pawn; never swap a pawn to a masked material variant during fades** — source: ADR-0022, ADR-0015
- **Never enable dynamic resolution, MegaLights, hardware ray tracing or Substrate, or change a scalability group at runtime outside the shipped preset applier** — cost, leak surface, and load-adaptive scaling correlates with who is on screen — source: ADR-0022
- **Never cite PIE/editor numbers as budget evidence** — source: ADR-0022
- **Never apply a fallback through gameplay tuning or a per-client heuristic** — source: ADR-0022
- **Never add free-text team chat or in-engine voice in the MVP; never give a ping a feed line, plate or per-ping widget** — source: ADR-0021 (user decision 2026-10-25)

### Performance Guardrails

- **HUD**: ≤ 0.3 ms CPU (AC-HUD-22), ≤ 0.5 ms with markers; UI GPU ≤ 0.5 ms at 1080p on the reference GPU (inside ADR-0022's frame budget) — source: ADR-0021
- **Kit rendering (R1, gate)**: kit Lumen + Nanite at 1,000 / 3,000 / 5,000 runtime ISM instances ≤ 5.0 ms GPU on an RTX 3060 at 1080p — source: ADR-0022
- **Characters (R2, gate)**: character skinning + character shadows ≤ 2.5 ms GPU with 60 characters and VSM — source: ADR-0022
- **Listen host (RC-1)**: game thread ≤ 12.0 ms p95 with 10 connections on 3060-class hardware — source: ADR-0022
- **Client GPU headroom (RC-2)**: whole-frame GPU ≥ 2.0 ms headroom on the client reference scene — source: ADR-0022
- **Masked character master (RC-3)**: GPU delta vs opaque ≤ 0.3 ms at 60 characters — source: ADR-0022
- **Client render and RHI threads**: ≤ 12.0 ms p95 each on the client reference scene — source: ADR-0022 §4.3
- **Client GPU total**: ≤ 14.6 ms p95 (≥ 2.0 ms headroom, RC-2) — source: ADR-0022 (note: ADR-0021 cites a "14.4 ms allocation"; ADR-0022 owns the figure)
- **Draw calls / triangles**: non-Nanite main-view draw calls ≤ 1,500 (PROPOSED; Generator share ≈ 300-500); non-Nanite tris on screen before shadows ≤ 800k (ASSUMED) — source: ADR-0022
- **Memory**: texture streaming pool 1.0 GB; pool resident ≤ 40 MB; layout navmesh is server-only, ~10-30 MB [U, R4] — source: ADR-0022, ADR-0012, ADR-0014

### Engine API Constraints (5.8.4)

- **`AddToPlayerScreen` widgets sit in the `Player` slot (100), below `AddToViewport` widgets (200); a camera fade is applied inside the scene render and can never cover UMG** — source: ADR-0021
- **Common UI routing requires `UCommonGameViewportClient`; the CommonUI plugin is not yet enabled in `StealthGame.uproject`**; `UCommonUIActionRouterBase::OnActiveInputModeChanged` is the input-mode event — source: ADR-0021
- **A custom `UDPICustomScalingRule` result is cached per viewport size outside the editor; `SetApplicationScale` is process-wide** — source: ADR-0021
- **`r.Substrate` is ReadOnly with default 0 for existing projects (this project has Substrate off); `r.MegaLights.EnableForProject` default 0; `r.RayTracing` default 0; the project sets `r.SkinCache.DefaultBehavior=0` (Exclusive)** — source: ADR-0022
- **CVar priority: a `DefaultEngine.ini [SystemSettings]` value overrides any `sg.*` group; `UGameUserSettings` persists `sg.*` in a user-editable file** — source: ADR-0022
- **Spikes owning [U] claims**: UI-V1..V4 (ADR-0021); R1, R2, RC-1..RC-3, R6-R9 (ADR-0022) — source: ADR-0021, ADR-0022

---

## Global Rules (All Layers)

### Naming Conventions

| Element | Convention | Example | Source |
|---------|-----------|---------|--------|
| Classes | PascalCase with UE prefix (`A` Actor, `U` UObject, `F` struct, `E` enum, `I` interface); project types carry `Stealth` | `AStealthPawn`, `FPenaltyCore` | `naming.classes` in project.yaml; prefixes from technical-preferences.md; `Stealth` prefix from ADR-0007 |
| Variables | PascalCase; booleans `b` prefix | `MoveSpeed`, `bIsConcealed` | `naming.variables` in project.yaml; technical-preferences.md |
| Signals/Events | PascalCase `On<Event>` | `OnRoleChangePresented` | `naming.signals` in project.yaml |
| Files | PascalCase, matching the class without prefix; project header names unique across modules; assets `[Prefix]_[Name]` (`BP_`, `M_`, `MI_`, `SM_`, `SK_`, `T_`, `WBP_`) | `StealthRosterService.h`, `M_Character_Master` | `naming.files` in project.yaml; technical-preferences.md; ADR-0007 |
| Constants | SCREAMING_SNAKE — **UNVERIFIED** (not sourceable from the engine reference; confirm before relying on it) | `MAX_PARTICIPANTS` | `naming.constants` in project.yaml |
| Levels/Maps | PascalCase (`L_` prefix) | `L_Graybox` | `naming.scenes` in project.yaml; technical-preferences.md |
| Log categories | `LogStealth<Module>` plus optional per-system | `LogStealthTuning` | ADR-0007 |
| Tests | `StealthGame.<System>.<Scenario>` / `….PIE` / `….MP.<Scenario>` | `StealthGame.Queue.TieOrder` | ADR-0008 |
| Insights budget scopes | `Stealth.Budget.<Line>` | `Stealth.Budget.Crowd` | ADR-0022 |

### Performance Budgets

`project.yaml` has no `performance.*` keys; values come from `.claude/docs/technical-preferences.md`.

| Target | Value | Source |
|--------|-------|--------|
| Framerate | 60 fps (RTX 3060-class, 1080p) | technical-preferences.md |
| Frame budget | 16.6 ms; animation (all characters) ≤ 4 ms; AI + nav (server) ≤ 1.5 ms | technical-preferences.md (agrees with ADR-0013, ADR-0016) |
| Network | ≤ 50 KB/s per client | technical-preferences.md (agrees with ADR-0001, ADR-0004, ADR-0020) |
| Draw calls | [TO BE CONFIGURED]; ADR-0022: ≤ 1,500 non-Nanite main-view draw calls (Accepted, conditional on R1/R2) | technical-preferences.md; ADR-0022 |
| Memory ceiling | [TO BE CONFIGURED]; ADR-0022: texture streaming pool ≤ 1.0 GB, total GPU memory reported only until R1/R2 | technical-preferences.md; ADR-0022 |

### Approved Libraries / Addons

- **Third-party libraries**: none approved yet — source: technical-preferences.md
- **Engine modules/plugins approved for use by the module table**: `Core`, `CoreUObject`, `Engine`, `NetCore`, `DeveloperSettings`, `Json` (Foundation); `EnhancedInput`, `AIModule`, `NavigationSystem`, `PoseSearch`, `MotionWarping` (Beta), `StateTree`, `GameplayStateTree` (Gameplay); `UMG`, `CommonUI` (not yet enabled), `Slate`, `SlateCore` (Presentation); `UnrealEd`, `ToolsetRegistry`, `UMGEditor`/`Kismet` (editor module only); CQTest and Gauntlet for the harness (non-Shipping) — source: ADR-0007, ADR-0008, ADR-0021

### Forbidden APIs (Unreal Engine 5.8.4)

These APIs are deprecated, removed or obsolete for UE 5.8 and apply to what this project builds:
- `UE_LOG` in new code — use `UE_LOGF` (5.8) — source: `deprecated-apis.md`, ADR-0007
- `CppStandardVersion.Cpp17` / `Cpp14` — obsolete in 5.8; use the default (C++20) — source: `deprecated-apis.md`
- Raw Input plugin (`RawInput`) — deprecated 5.8; Game Input for Windows if device input is ever needed — source: `deprecated-apis.md`, ADR-0011
- PhysicsMover — removed in 5.8 (and every Mover/ChaosMover/NetworkPrediction type is banned by ADR-0010) — source: `deprecated-apis.md`, ADR-0010
- Legacy input binding (`InputComponent->BindAction()`/`BindAxis()` by name, `GetInputAxisValue()`) — use Enhanced Input — source: `deprecated-apis.md`, ADR-0011
- `UPlayerMappableInputConfig` — deprecated 5.3; use `UEnhancedInputUserSettings` — source: ADR-0011
- `TSharedPtr<T>` for UObjects — use `TObjectPtr<T>` in `UPROPERTY` members; manual RTTI — use `Cast<T>()`/`IsA<T>()` — source: `deprecated-apis.md`
- `FSmartName` / `FSmartNameMapping` / `FSmartNameContainer` (animation code) — deprecated 5.8; remove the use — source: `deprecated-apis.md`
- Cascade particle systems — use Niagara for any VFX — source: `deprecated-apis.md`
- *Note (not forbidden):* prefer `UE_PLATFORM_*` over `#if PLATFORM_*` in new code — optional migration per `deprecated-apis.md`
- `AGameModeBase::DispatchPostLogin` — deprecated 5.6; override `PostLogin`/`OnPostLogin` — source: ADR-0005
- `UMotionWarpingComponent::GetCharacterOwner` — removed 5.8; use `GetOwnerAdapter()` — source: ADR-0010
- `USkinnedMeshComponent::SetSkeletalMesh` — deprecated since 5.1; use `USkeletalMeshComponent::SetSkeletalMeshAsset` — source: ADR-0012
- `UStreamableRenderAsset::HasPendingInitOrStreaming(bool)` — `UE_DEPRECATED(5.8)`; use the parameterless overload — source: ADR-0012
- Direct access to `NetUpdateFrequency`, `MinNetUpdateFrequency`, `NetCullDistanceSquared` (deprecated 5.5) and `NetServerMaxTickRate` (deprecated 5.3) — use the setters/getters — source: ADR-0003
- 8-bit `APawn::RemoteViewPitch` — deprecated 5.6; use `RemoteViewPitch16`/`SetRemoteViewPitch` — source: ADR-0001, ADR-0003

### Engine-reference corrections (ADR facts win)

- **Input flush CVar**: the 5.8.4 registered name is `EnhancedInput.IgnoreHeldDigitalActionKeysOnFlush`; the engine reference's `EnhancedInput.IgnoreHeldKeysOnFlush` is stale — source: ADR-0011 E2, ADR-0021
- **`DOREPLIFETIME` is not deprecated**: `deprecated-apis.md` lists it as replaced by `DOREPLIFETIME_CONDITION`; the 5.8.4 engine registers `APawn` properties with plain `DOREPLIFETIME`, and this project uses `DOREPLIFETIME`, `DOREPLIFETIME_CONDITION` and the reset/disable helpers as each ADR specifies. Not listed as forbidden — source: ADR-0001 E1/E4/E5
- **Engine-reference recommendations superseded by ADRs** (do not follow them here): Substrate, MegaLights, hardware ray tracing (ADR-0022 keeps them off); a `Server_Move` RPC on the pawn and `DOREPLIFETIME` examples for gameplay state on the pawn (ADR-0011, ADR-0001); hiding via `SetActorHiddenInGame` as in the object-pooling example (ADR-0017 bans `bHidden` for concealment: it replicates and feeds relevancy); per-instance `HISM->AddInstance` loops (ADR-0014 requires batch `AddInstances`); `UE_LOG` examples (ADR-0007); Event Tick → timers advice (gameplay timers are queue items, ADR-0002) — source: `current-best-practices.md` vs the named ADRs

### Cross-Cutting Constraints

- **Pillar-1 kind-blindness**: no code observable by another client may differ between player- and AI-driven pawns — class, components, replication, movement, animation, audio, render flags, tiering, fallbacks, telemetry probes — source: ADR-0001, ADR-0010, ADR-0011, ADR-0013, ADR-0016, ADR-0022, ADR-0023
- **Default-deny wire**: every replicated property, condition, RPC or class needs a Visibility Policy row and allowlist row; new replicated fields and schema changes are always-ask (user decision) — source: ADR-0001, ADR-0003, ADR-0014, ADR-0023
- **One clock, one order**: gameplay timing goes through the resolution queue (`ScheduleAt`, `TimerFired`, `HoldTimer`); client presentation from `EstimatedServerNow()` only; no `FTimerManager`, feature tick or latent action for gameplay — source: ADR-0002, ADR-0011, ADR-0016, ADR-0017, ADR-0018, ADR-0019, ADR-0020
- **Secrets stay server-side**: sub-seeds, `DealDigest`, assignments, bindings, Event Log content never reach logs, traces, Blueprint, URLs or the wire — source: ADR-0006, ADR-0007, ADR-0012, ADR-0014, ADR-0020, ADR-0023
- **Data-driven values**: gameplay numbers come from validated tuning, never literals, ini or CVars — source: ADR-0004
- **Server/View header split**: client and presentation code include only `Public/View/` headers — source: ADR-0007
- **Formerly user-pending schema items are decided (2026-10-25) and must be implemented as specified**: `T_due` on the C-TeamView owner part, `FActionState.StartServerTime`, `FDeathState.bDiedConcealed`, `FStrikeResult.ContactServerTime`, teammate `RetireAtServerTime`, feed `Seq`, the `ServerWorldReady` ack stage — source: ADR-0001, ADR-0003, ADR-0005, ADR-0017, ADR-0018, ADR-0019
- **Evidence standard**: performance evidence from cooked Development standalone captures on 3060-class reference hardware; PIE/editor or RTX 5090 numbers are not evidence — source: ADR-0013, ADR-0022
- **Doc comments on public APIs; dependency injection over singletons (plain-C++ cores)** — source: coding-standards.md, ADR-0007

### Unresolved (engine-reference entries left out)

- **Gameplay Ability System rows** (`UGameplayAbility::AbilityTags`, `FActiveGameplayEffectHandle(int32)`): no ADR adopts GAS for production code, though the project inherits GAS sample content — when using GAS: use `GetAssetTags()`/`SetAssetTags()` and `GenerateNewHandle`/`GetInstantExecutedHandle()`.
- **World Partition / World Composition / Level Streaming Volumes / `LoadStreamLevel`**: no ADR adopts level streaming (ADR-0005 uses seamless travel per round); applicability unclear.
- **Sound Cue → MetaSounds**: ADR-0013 fixes footstep sound class/attenuation but not the asset type; applicability unclear.
- **Substrate (legacy material nodes)**: contradicted by ADR-0022 (Substrate stays off); excluded.
- **Not applicable to the code the ADRs describe**: Control Rig Blueprint `FindBone`/`FindControl`/`FindNull`, Waveform Editor fade functions, plugin `PluginName.ini` naming, `r.Water.SingleLayer.ForceVelocity`, `EWindowType::GameWindow`, IK Retargeter/legacy Control Rig rows.

## Changes in 2026-10-25b

Regenerated after the user decisions of 2026-10-25 (recorded as "user decision 2026-10-25" Amendments in ADR-0001, 0003, 0004, 0005, 0011, 0012, 0013, 0014, 0015, 0016, 0017, 0018, 0019, 0020, 0021, 0022, 0023). Rules added or changed:
- Foundation: Roster C-TeamView part fields (owner tally re-push, `T_due`; teammate `RetireAtServerTime`) and their writers; fallback feed `Seq`; `T_due` single-carrier ban replaces "never implement the carrier"; `StartServerTime`/feed-`Seq` rule replaces "never implement"; replication line 36 KB/s at 60 pawns and feed ≤ 104 B/s.
- Core: pawn-network line 36 KB/s; animation gate at 60 characters.
- Feature: concealed kill un-culls at the death write with `bDiedConcealed` (replaces `MarkKilledConcealed` and the slot-B ban); `FActionState` 208 bits, `FDeathState` 65 bits, `FStrikeResult.ContactServerTime`; tally never written to C-TeamView per strike; `InteractSecOverride` gone; Crowd asset keys and pre-baked route-edge paths; Crowd budget at 60 pawns.
- Presentation: GPU character lines measured at 60 characters.
- Global: user-pending list replaced by "decided, implement as specified"; conditional-acceptance rows for ADR-0016/0017/0019 updated.
- Tuning (ADR-0004, no separate rule line): `NpcKillStrikeLimit` hard minimum 2 (`bTestAllowsStrikeLimitOne` test presets only); `LatencyWarnRttSec` retired; TDC CR-10 renames; `LayoutExactRefineMaxCount`; `NpcCount` 50.

## Changes in 2026-10-25c

Updated after the user's scope decisions of 2026-10-25 ("accept all recommendations"; recorded as "User scope decisions (2026-10-25)" Amendments in ADR-0001, 0002, 0003, 0004, 0005, 0011, 0016, 0021). Rules added or changed:
- Foundation: teammate `bReconnecting` bit (Roster push step only; never to non-teammates); `UTeamPingViewComponent` C-TeamView part and its writer/clears; `FRoundResult.TeamSummary` computed at the latch, published only at the round-end step.
- Core: `IA_WalkSlower` client-local keyboard pace modifier; `IA_Ping` → `ServerTeamPing` controller RPC with server validation; pings never touch the pawn or the command path; `ShowTeamNames` not bound in MVP.
- Feature: Crowd `CrowdFullPacePct` 55 and C4 active (interim full-pace rule lifted).
- Presentation: L0 ping rings; text-only round summary from integers + string tables; reconnecting/back feed lines and the marker label suffix; no free-text chat or in-engine voice in MVP.
- Tuning (ADR-0004, no separate rule line): new owner `InformantPings` (`PingLifetimeSec`, `PingBucketSize`, `PingBucketPerSec`, `PingRangeCm`, `PingRangeSlackCm`) and invariant I-PNG-1.
- Budget (ADR-0003): ping part ≤ 50 B/s per teammate connection; `FRoundResult` +6 bytes at `Ended`.
