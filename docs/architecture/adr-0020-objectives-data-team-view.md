# ADR-0020: Objectives data and team-view sub-structs

## Status
Accepted — 2026-10-25, by the user (blanket acceptance of ADR-0001..0023, recorded by the orchestrator). **Conditional:** every spike or verification item named in the earlier status text below must still report before implementation depends on it; a failed spike is handled by a superseding ADR, not by editing this one.

*Earlier status text (kept for its gating conditions):* Proposed — awaiting user acceptance. It cannot be Accepted before ADR-0001 (C-TeamView carrier), ADR-0002 (queue, `TimerFired(StageSignal)`, latch), ADR-0004 (tuning schema rows and invariants I-OC1..I-OC3), ADR-0006 (`StageSignal` domain), ADR-0011 (with the interact-target amendment requested in §5) and ADR-0014 (court SocketIds, `FSiteAssignment` server query) are Accepted, the Objectives Chain GDD has passed its pending re-review, and the game-designer has signed off the two GDD rule interpretations this ADR makes (§2 hold-time override rejected; §5.3 "physically occupied" = active-hold claims).

**Gate record (2026-10-24):** TD-ADR CONCERNS → REVISED (rebuild hook replaced by a Roster rebind-event subscription per ADR-0005's world-wiring rule; gate (c) records the control epoch with the claim and requires it unchanged plus no pending hand-over; interact-target lifecycle rules added — release-only call outside a slot, `D` read once, claim kept through a pending debounced End, single owner, drain-end claim consistency check; push model mandatory on the view; designer sign-off made an acceptance condition; I-OC2 wording; `TendCompleted` branch named as the only exception). Engine validation (unreal-specialist lens): MINOR NOTES → applied (relevancy citation corrected to `Controller.cpp:67` / `ActorReplication.cpp:388-400`; registered sub-object list is off by default and must be set on the controller; components register after BeginPlay, not by `AddReplicatedSubObject`; `bWithPushModel` default per target type; primary-asset override must return an invalid id for the CDO and match the scan entry; `FText` localisation needs the asset path in a gather target; `ReplicatedUsing=OnRep_View` added, presentation still polls).

## Date
2026-10-24

## Last Verified
2026-10-24 (design against `design/gdd/objectives-chain.md` as revised and reconciled 2026-10-24 and `design/gdd/information-visibility-policy.md` R3/R4/R7.4a; engine claims re-read against the UE 5.8.4 install at `C:/Program Files/Epic Games/UE_5.8`)

## Decision Makers
Autonomous design pipeline (`/architecture-decision`, rigor full); technical-director (TD-ADR) and unreal-specialist validation; acceptance reserved for the user.

## Summary
Objectives Chain needs a data shape for its chain, a server-only binding of definitions to secret courts, a team-only view that never reaches the other team, a completion gate that is judged inside the hold's queue slot, and a delayed stage-level rival signal that confirms no sighting. This ADR decides: one cooked `UObjectiveChainData` asset of `FObjectiveDef` rows; a server-only `UObjectivesService` over a headless `FObjectivesCore` that binds courts from `FSiteAssignment` at `OnLayoutReady` and owns per-team progress; `FObjectiveTeamView` carried by a `UObjectiveTeamViewComponent` C-TeamView part (ADR-0001) written only by Objectives; the completion gate as an inline follow-on of the IIF `HoldTimer(Complete)` slot through a consumer-owned interact-target interface; and `RivalPrelimCount` released by `TimerFired(StageSignal)` items on one global grid from integer-millisecond delays drawn from the `StageSignal` secret stream.

## Engine Compatibility

| Field | Value |
|-------|-------|
| **Engine** | Unreal Engine 5.8 (installed 5.8.4, CL 58902052) |
| **Domain** | Core (gameplay rules and data) + Networking (team carrier) |
| **Layer** | Feature |
| **Knowledge Risk** | LOW for the rules and data (pre-cutoff `UPrimaryDataAsset`, Asset Manager, `UActorComponent` replication); MEDIUM where the view rides the C-TeamView replication mechanism, whose HIGH-risk parts (owner-only relevancy, sub-object registration) ADR-0001 owns and its spikes verify |
| **References Consulted** | `docs/engine-reference/unreal/VERSION.md`, `modules/networking.md`, `breaking-changes.md`, `deprecated-apis.md`; `docs/architecture/architecture.md` (Engine Knowledge Gap Summary, Module Ownership Feature layer, Data Flow §1-§3, API Boundaries Feature/Presentation, Performance budget, Required ADRs row ADR-020); ADR-0001, ADR-0002, ADR-0004 (sections), ADR-0006 (sections), ADR-0010 (sections), ADR-0011, ADR-0014 (sections), ADR-0017 (format); registry staging `production/session-state/adr-registry-staging/ADR-0001..0017.yaml`; prototype `AStealthObjective` and `production/first-playable-report.md` (evidence only) |
| **Post-Cutoff APIs Used** | None. Everything used predates 5.4; push-model marking is optional (E3) |
| **Verification Required** | **OBJ-V1** (with ADR-0001 S-series): `UObjectiveTeamViewComponent` on `AStealthPlayerController` reaches only its owner in a multi-client run, including after a rejoin and after seamless travel (AC-OC-03, AC-IVP-19). **OBJ-V2**: two `RivalPrelimCount` writes in one server frame reach the client as one change (AC-OC-09 "0 → 2 in one update"). **OBJ-V3**: `UObjectiveChainData` cooks with `AlwaysCook` and resolves by `FPrimaryAssetId` on a cooked client (ADR-0012's identical CPA-V check covers the mechanism) |

**Engine facts this ADR relies on** ([V] read in the 5.8.4 source this session or verified by a cited ADR; [D] engine-reference; [U] unverified, owned by the named check):

| # | Fact | Tag |
|---|---|---|
| E1 | Controllers are `bOnlyRelevantToOwner` (set in the `AController` constructor) and the owner-first relevancy check honours it; components a controller owns replicate only with it, so a replicated component on the controller reaches only the owning connection | [V] `Controller.cpp:67`; `ActorReplication.cpp:388-400` (engine gate); ADR-0001 E6 |
| E2 | The registered sub-object list is **off by default** (`net.SubObjects.DefaultUseSubObjectReplicationList` false; actors read it at construction); replicated components are listed by the actor and registered for replication after BeginPlay; `AddReplicatedSubObject` is for non-component sub-objects | [V] `ActorComponent.cpp:115-117`; `Actor.cpp:292, 3808-3810`; `ActorReplication.cpp:658-664`; `Actor.h:4147-4158` (engine gate) |
| E3 | Push model: `MARK_PROPERTY_DIRTY_FROM_NAME` and `FDoRepLifetimeParams::bIsPushBased` exist; the macros compile to nothing without `WITH_PUSH_MODEL`; `bWithPushModel` defaults to true for Editor targets and false otherwise; `Net.IsPushModelEnabled` defaults to **false**, in which case push-based properties are compared like any other | [V] `Net/Core/Public/Net/Core/PushModel/PushModel.h:227-234, 454, 480-489`; `PushModel.cpp:434-437`; `UnrealNetwork.h:146`; `TargetRules.cs:1522-1527` (engine gate) |
| E4 | A replicated property changed several times between two net updates of its actor is sent once, with its last value | [D] property replication compares current state at net update time (`modules/networking.md`) / OBJ-V2 |
| E5 | `UPrimaryDataAsset` reports its native class name as its primary asset type unless `GetPrimaryAssetId()` is overridden (the base walks up to the native class for a CDO); `EPrimaryAssetCookRule::AlwaysCook` exists. The override must return an invalid id for the CDO and a type name that matches the `DefaultGame.ini` scan entry exactly | [V] `DataAsset.cpp:73-103`; `AssetManagerTypes.h:53` (engine gate); ADR-0012 E7 |
| E6 | `FText` in a cooked data asset is localisable only if the asset's path is inside a localisation gather target; text read client-side from a local asset never travels on the wire | [U] (engine gate); the implementing story adds the chain asset folder to the gather target |
| E7 | `uint16` / `uint8` `UPROPERTY` members (`FUInt16Property`, `FByteProperty`) and a `TArray` of `USTRUCT`s inside a replicated struct replicate on the legacy driver; members changed within one net update arrive in one bunch | [V] `UnrealType.h:2190, 2404` / [U] one-bunch arrival: OBJ-V2 |

## ADR Dependencies

| Field | Value |
|-------|-------|
| **Depends On** | ADR-0001 (C-TeamView: `UTeamViewComponentBase` parts on the owner-only PlayerController; Team audience; "feature team sub-structs ride C-TeamView"; the controller's `bReplicateUsingRegisteredSubObjectList = true`; push-model part properties). ADR-0002 (queue slots, inline follow-ons, `ScheduleAt`, `TimerFired(StageSignal)` resolver registration, `Rejected_Latched`, `CurrentKey`, `ServerNow`, epoch drop on Reset). ADR-0004 (Objectives tuning sub-structs and invariants I-OC1..I-OC3 as `Check` rows; integer-ms rounding at freeze). ADR-0006 (`ESeedDomain::StageSignal = 10`, `FSecretStream`, tag `OC_STAGE`, Attempt = round-global completion ordinal). ADR-0007 (folders, View/Server split, composition-root registration). ADR-0010 (`GetControlEpoch`, `IsHandOverPending` for gate (c)). ADR-0011 (`ICommandPredicate` for `Interact`; `HoldTimer(Complete)`; action hub; this ADR **requests** an interact-target amendment, §5). ADR-0014 (`OnLayoutReady`, `GetSiteAssignment_Server`, court SocketIds, zero replicated actors). ADR-0005 (`RebindView` and its "subscribe to Roster events" rule, controller replacement at travel). ADR-0003 (allowlist rows for the new component) |
| **Enables** | ADR-0021 (HUD stage card, own/rival steps and ring colour read `FObjectiveTeamView`); ADR-0016 (Crowd decoys get `Allow`/full-fixture `Reject(NotNow)` from the same predicate); ADR-0023 (telemetry reads the F5 Event Log fields) |
| **Blocks** | Objectives Chain epic; HUD stage card / objective steps stories; Crowd decoy-tend story (predicate dependency) |
| **Ordering Note** | Feature layer; built after the IIF and Layout Service stories it consumes. The two requested amendments (ADR-0011 interact-target interface; ADR-0001/ADR-0005 part-source rebuild hook) must be folded into those ADRs before either is Accepted, or this ADR's §5/§6 fall back to the alternatives recorded there |

## Context

### Problem Statement

The Objectives Chain GDD fixes behaviour (two secret preliminaries per team, one shared final, a 3 s tend, no time limit, stall pressure without timers or reveals) and marks four decisions "-> ADR": the chain data-asset class, the team-view carrier layout, the completion gate's place in the queue, and how the rival signal is scheduled. They are needed before the Objectives epic and the HUD's objective elements can be built. The prototype shows what goes wrong without them: `AStealthObjective` is a replicated actor placed **only** at objective sites, with `bCompleted` and `InteractorCount` replicated to every client and per-character progress ticked locally (`Source/StealthGame/Public/StealthObjective.h`), so every client can list the live and final sites, see which are being worked and see completions — exactly the leaks IVP L24/L33 and SP-1 forbid. The first-playable report proves the chain logic works end to end (test D PASS, 5v5 PASS) but not secretly.

### Constraints

- **User-locked:** one shared final objective; about 3 s interaction; no time limit (Session/round rules; GDD OC-14, SP-3).
- ADR-0001 / IVP R4: Team data only in a `UTeamViewComponentBase` part on each teammate's own PlayerController; no team-filtered actor, no team relevancy; a new replicated field needs an IVP row and an allowlist row.
- IVP R3: own `ObjectiveProgress` Team; opponent progress `ServerOnly` except `RivalPrelimCount` (ruling-B shape); `FSiteAssignment` `ServerOnly`; `SiteKnownToTeam` Team, presentation only, never ingress (R7.4a).
- ADR-0002: every state change inside a queue slot; timers only as due-time-heap items; `ObjectiveCompletion` is not an item but a follow-on in `HoldTimer(Complete)`.
- ADR-0006: hidden decisions draw only from server sub-seeds; no engine RNG.
- ADR-0011: the predicate's `Allow`/`AllowNoEffect`/`Reject` may depend only on site and legality class; reply, timing and hold duration identical for every court.
- ADR-0014: the layout contract spawns zero replicated actors; fixtures are built locally; `FSiteAssignment` only through `GetSiteAssignment_Server()`.
- Listen host holds `FSiteAssignment` in memory (IVP R10, accepted until the Dedicated Server).

### Requirements

- Chain asset with `FObjectiveDef` rows (OC-2), one MVP chain (OC-16), validated at `Preparing` (AC-OC-01).
- Binding at `OnLayoutReady`, server only (OC-3, LSC AC-51).
- Per-team one-way progress, unlocks only by counted completions (OC-4, SP-3).
- Team view with exactly the OC-5 contents, pushed at `Active` entry, never before (OC-17, AC-OC-29), rebuilt on rejoin.
- Predicate from (court, public stand-point occupancy, legality class) only (OC-7, OC-8).
- Completion gate (a)-(e) at the `HoldTimer(Complete)` slot; final calls `DeclareObjectiveWin(team, Key)` in that slot (OC-10, AC-OC-06/07).
- `RivalPrelimCount`: fresh per-completion delay on `StageSignal` keyed by ordinal, global grid, count not order, no timer when nothing is pending, discarded at the latch (OC-12, F4, AC-OC-09/10/16).
- No world signature of a completion (AC-OC-12); no Objectives tick, clock or timer outside the queue (SP-3, SP-6).

## Decision

### 1. Placement

| Piece | Kind | Where | Runs on |
|---|---|---|---|
| `UObjectiveChainData` | `UPrimaryDataAsset`, primary asset type `ObjectiveChain`, cook rule `AlwaysCook`, overrides `GetPrimaryAssetId()` (E5) | `StealthGameplay/Public/View/Features/Objectives/` (client-safe: card text is read client-side) | server and clients (loaded once per session, held by `UObjectivesChainRegistry`, a `UGameInstanceSubsystem` in `View/`) |
| `FObjectivesCore` | plain C++, no `UWorld` | `StealthGameplay/Private/Features/Objectives/` | server; headless specs inject stubs |
| `UObjectivesService` | `UWorldSubsystem` facade, `NotBlueprintType`, authority only | `StealthGameplay/Public/Server/Features/Objectives/` (guarded header) | server |
| `UObjectiveTeamViewComponent` | `UTeamViewComponentBase` part (ADR-0001 §5) carrying one replicated `FObjectiveTeamView` | `StealthGameplay/Public/View/Features/Objectives/` | created as a default subobject of `AStealthPlayerController` (root module); written only by `UObjectivesService` on the server; read by Presentation via `FindComponentByClass` on its own controller |

Objectives spawns **no actor** of any kind: there is no per-court objective actor, replicated or server-only (ADR-0014 Guideline 6; LSC AC-51's actor-parity clause is therefore satisfied vacuously). Court fixtures are the layout's locally built, non-replicated components. Objectives has **no tick function, no `FTimerManager` use and no latent action**; it runs only inside queue slots, inside the `OnLayoutReady` and `OnRoundActive` server delegates, and in the Roster's rebuild call (§6).

### 2. Chain data (OC-2, OC-3, OC-16)

```
UObjectiveChainData { uint16 ChainSerial (authored, stable, unique); TArray<FObjectiveDef> Definitions; }
FObjectiveDef { FName ObjectiveId; EObjectiveStage StageKind {Prelim1, Prelim2, Final};
                EObjectiveEligibility TeamEligibility {Team0, Team1, Both}; EObjectiveSiteRole SiteRole {P1, P2, Final};
                TOptional-like float InteractSecOverride (bOverrideInteractSec + value); int32 CompletionsRequiredCount;
                FText CardTitle; FText CardLine; EClueForm ClueForm {AreaAndFeature}; }
```

- **Row identity on the wire is the row index** (`uint8 DefIndex` into `Definitions`), never the `FName`. Clients render card title and line from their own cooked copy of the asset (E6); only indices travel. `ObjectiveId` is logged server-side.
- **MVP:** exactly one chain asset (`DA_ObjectiveChain_Cacao`, OC-16) is cooked; `UObjectivesService` selects it for every round. A second cooked chain without a selection rule is a Preparing failure (no silent choice).
- **Validation at `Preparing`** (`FObjectivesCore::ValidateChain`, AC-OC-01), any failure → Round's `PreparingFailed(ObjectiveChainInvalid)`:
  1. per team exactly `ObjectivesPerStageCount` rows of `Prelim1` and of `Prelim2` with that team's eligibility, exactly one `Final` row with `Both`; `SiteRole` matches `StageKind`; ids unique; card text non-empty;
  2. `ObjectivesPerStageCount` must be 1 until LSC Selection draws several courts per stage (OC-3, GDD OQ2);
  3. `CompletionsRequiredCount` = 1 (MVP; >1 needs a GDD rule for partial progress);
  4. **`bOverrideInteractSec` must be false on every row** (see §5: the hold duration must be identical at every court; `D` is fixed at Begin, so a per-definition override would make it depend on the court's role, an IVP R7.4a / AC-OC-08 timing oracle). The field is kept only because GDD OC-2 lists it; removing it from the definition is a data-schema change to the GDD and is left to the user (pending decision), while validation rejects any set value.
- Row count is bounded by 16 (`CompletedDefMask` width, §4); validation rejects more.

### 3. Server state, binding and progress (OC-1, OC-3, OC-4, OC-17)

- **Store** (`FObjectivesCore`, round-scoped): `FChainBinding` per (team, DefIndex) → `FCourtFixtureId` (court SocketId, §3.1) and site state `Locked | Live | Completed`; per team `StageIndex ∈ {Stage1, Stage2, Final}`, `CompletedDefMask`; the stand-point claims per fixture (§5.3); the pending rival releases (§7); the round-global prelim completion ordinal counter; `ActivePhaseStart` (t0) and the round epoch. Cleared on `OnRosterReset` and in `Deinitialize`; never carried across rounds (the world, and with it the service, is new each round).
- **Bind at `OnLayoutReady`** (server delegate, game thread, Preparing): read `UStealthLayoutService::GetSiteAssignment_Server()` (ADR-0014 §7.4) and bind `Prelim1(T) → P1[T]`, `Prelim2(T) → P2[T]`, `Final → Final`; every site `Locked`. In the same call compute, for each bound site, its clue (`FClueTag`, §4) with the role-blind clue function and count how many courts of the layout share it; more than `ClueMatchMaxCount` logs `ClueTooBroad` once per `LayoutHash` (AC-OC-28) and changes nothing else. The assignment is never copied anywhere a client could reach and never logged outside the server-only Event Log header (ADR-0014 Guideline 13).
- **`Active` entry** (`OnRoundActive(RoundEpoch, ActivePhaseStart)`, raised inside Round's single R7 step): store t0 = `ActivePhaseStart`, set both teams' `Prelim1` sites `Live`, `StageIndex = Stage1`, and write each team's view (§6). Before this step no client holds any site id or card (AC-OC-29).
- **Progress** changes only in the completion follow-on (§5.4), in one slot: mark `Completed`, set the next stage's rows `Live` for that team only, write that team's view. No other code path writes site state (OC-4, SP-3). Non-Shipping `DebugObjectiveCompletion(Team)` (Round `DebugItem`, ADR-0002) calls the same `ApplyCountedCompletion` function in its own slot.

**3.1 Court identity.** `FCourtFixtureId { uint16 SocketId; }` is the court's LSC `SocketId` (LSC R1/R2.6: a stable canonical integer, as `BushId` is the bush's SocketId). It is public geometry: every client builds the court fixture components locally from layout data with the same SocketId, so an id names a court and nothing about its role. It is declared in `StealthGameplay/Public/View/Core/` (a Core layout type the IIF already uses, ADR-0011 `InteractBegin(FCourtFixtureId)`); ADR-0014 is asked to own the declaration (GDD/ADR sync, Related).

### 4. The team view (`FObjectiveTeamView`, OC-5, IVP R3 rows)

```cpp
USTRUCT() struct FObjectiveLiveSite {
    UPROPERTY() FCourtFixtureId Court;   // SiteKnownToTeam (presentation only)
    UPROPERTY() uint8 DefIndex = 0xFF;   // stage card row in the cooked chain asset
    UPROPERTY() FClueTag Clue;           // role-blind clue of that court: {uint8 AreaType, uint8 FeatureKind}
};
USTRUCT() struct FObjectiveTeamView {
    UPROPERTY() uint32 RoundEpoch = 0;                 // stale-view guard on the client
    UPROPERTY() uint16 ChainSerial = 0;                // which cooked chain asset the indices refer to
    UPROPERTY() EObjectiveTeamStage StageIndex = EObjectiveTeamStage::None;   // None | Stage1 | Stage2 | Final (GDD name)
    UPROPERTY() TArray<FObjectiveLiveSite> LiveSites;  // own current stage only; <= ObjectivesPerStageCount (MVP 1)
    UPROPERTY() uint16 CompletedDefMask = 0;           // own completed rows (own prelims; never the final)
    UPROPERTY() uint8 RivalPrelimCount = 0;            // 0..2, OC-12 releases only
};
UCLASS() class UObjectiveTeamViewComponent : public UTeamViewComponentBase {
    UPROPERTY(ReplicatedUsing=OnRep_View) FObjectiveTeamView View;  // one push-based property; COND_None (the controller is owner-only, E1)
};
```

Field-to-fact map (each field is an encoding of a fact the IVP R3 Team rows already grant; no new fact or audience):

| Field | IVP R3 row | Note |
|---|---|---|
| `StageIndex`, `CompletedDefMask` | `ObjectiveProgress` own team (Team) | own team only |
| `LiveSites[].Court` | `SiteKnownToTeam` (Team) | drives ring colour, card, credit display only; ingress never reads it (R7.4a) |
| `LiveSites[].DefIndex` | `ObjectiveProgress` own team | selects the card row; card text is local data |
| `LiveSites[].Clue` | derived from `SiteKnownToTeam` + public court data | the clue is a pure role-blind function of public geometry (AC-OC-13), so it adds nothing a teammate holding `Court` could not compute; it is sent so clients need no client-side clue derivation and the graybox works |
| `RivalPrelimCount` | `RivalPrelimCount` (Team, ruling B) | integer only; no id, site, time or order |
| `RoundEpoch`, `ChainSerial` | none needed (bookkeeping, no gameplay fact) | `RoundEpoch` duplicates the Roster owner part's value |

Rules:
- The view **never** holds a locked site, an opponent site, opponent progress other than `RivalPrelimCount`, hold progress of anyone, or the final before the team's own preliminaries are complete (OC-5, SP-1). The final's completion is **not** written to the view: the round-end step presents it (Round).
- **Every member** of team T, Active or Informant, receives the same view (Team audience includes the team's informants, IVP R3).
- **Width:** about 12 bytes plus 4 per live site; written at most a few times per round per controller.
- **Clue (`FClueTag`)**: `{AreaType, FeatureKind}` taken from LSC's per-court public `ClueTag` record (LSC change request 8; GDD OQ3). Until LSC publishes `ClueTag`, `FeatureKind = None` and the clue is area type only (the GDD's stated fallback). The phrase ("A court in the market, by the water steps") is built client-side from a localised phrase table; no string travels.

### 5. Tend predicate, hold duration and completion gate (OC-6 to OC-11, OC-14)

**5.1 One interact-target owner.** ADR-0011 registers `ICommandPredicate` per action type but its action-hub events carry neither the target court nor the slot key, and it takes `D` "from the target owner" without naming an interface. This ADR decides the interface and **requests it as an amendment to ADR-0011** (consumer-owned, Core, registered weakly from the composition root like the predicate):

```cpp
// StealthGameplay/Public/Server/Core/InteractTargetOwner.h  (Core, consumer-owned by the IIF)
UINTERFACE(meta=(CannotImplementInterfaceInBlueprint)) class UInteractTargetOwner : public UInterface { GENERATED_BODY() };
class IInteractTargetOwner { GENERATED_BODY()
public:
    virtual double GetInteractHoldSec() const = 0;                                   // one value for every target
    virtual void OnInteractExecuted(const AStealthPawn& Pawn, FCourtFixtureId Court, FQueueKey Key) = 0;
    virtual void OnInteractEnded(const AStealthPawn& Pawn, FCourtFixtureId Court,
                                 EInteractEnd End /* Completed | Cancelled */, FQueueKey Key) = 0;
};
// UActionHub::RegisterInteractTargetOwner(TWeakInterfacePtr<IInteractTargetOwner>)  (amendment)
```

The IIF calls `OnInteractExecuted` inline in the Begin's slot after it has executed the command, and `OnInteractEnded` inline in the slot that ends the hold (`HoldTimer(Complete)` → `Completed`; release, `ConditionCancel`, `Superseded`, `Illegal`, `PawnRemoved` → `Cancelled`), **before** `OnActionCompleted`/`OnActionCancelled` fan out, with `CurrentKey()` as `Key`. Both calls happen for every pawn kind, with no caller identity. Lifecycle rules of the amendment:
- **Outside a slot.** The round-end step cancels frozen holds in ADR-0002's staging pass, where `CurrentKey()` is not valid; there the IIF passes an invalid `FQueueKey` and `End = Cancelled`, and Objectives treats the call as a **claim release only** (no gate, no log). Pawn teardown likewise releases the claim.
- **`D` is read once**, at execution, to schedule `HoldTimer(Complete)`; a later tuning publish (PIE only) never changes a running hold.
- **A pending debounced End keeps the claim** until the `DebounceExpiry` slot ends the hold (the hold is still running until then, IIF R8).
- **Single owner.** Exactly one `IInteractTargetOwner` is registered for `Interact`; if a second kind of interact target is ever added, the amendment must route per target kind.
- **Consistency check** (non-Shipping): at drain end, each fixture's claim count must equal the number of IIF `Interacting` slots targeting it; a mismatch reports to the fault sink.

**5.2 Hold duration.** `GetInteractHoldSec()` returns the frozen `ObjectiveInteractSec` (latch `Action`, ADR-0004) and takes **no court parameter**, so `D` cannot depend on a court's role by construction (AC-OC-08 timing parity, IVP R7.4a). The client ring reads the same key from the client tuning face.

**5.3 Predicate** (`ICommandPredicate::CanBegin` for `Interact`, side-effect-free), inputs only (court, its claimed stand points, legality class):
1. `Roster.GetLegalityClass(Pawn)` ∈ {`Informant`, `RoundEnded`} → `Reject(NotNow)`;
2. `ClaimCount(Court) ≥ FixtureInteractSideCount` → `Reject(NotNow)` (full fixture, OC-8; identical at every court and for every pawn, AC-OC-04);
3. otherwise `Allow`, at every court whatever its role or state. `AllowNoEffect` is never returned at Begin: whether the tend counts is decided only at the completion slot (OC-7; the IIF's `AllowNoEffect` outcome is the completion that changes nothing).
A static check (AC-OC-04) asserts the predicate translation unit calls no Roster team/binding query, no `FSiteAssignment` accessor and no `FObjectivesCore` site-state accessor.

**Stand-point claims (decision).** "Physically occupied" (OC-8) is implemented as **claims by active holds**: `OnInteractExecuted` claims one of the fixture's `FixtureInteractSideCount` faces (the free face nearest the pawn's position in that slot; ties by face index), recording the pawn's control epoch with it (§5.4 (c)), and `OnInteractEnded` releases it. A body merely standing at a face blocks it physically (collision, `OutOfReach` at Begin through the IIF's reach and facing check) but holds no claim. The claim table is server-only, kind-blind and mirrors what every observer can see (who is tending where), so the reply it produces is no oracle. A completion releases only its own claim and never cancels other holds at the court (OC-8, AC-OC-05).

**5.4 Completion gate** (`OnInteractEnded(..., Completed, Key)`, inline in the `HoldTimer(Complete)` slot; OC-10, Round R13 `ObjectiveCompletion` row). The completion **counts** only if, read live at this slot:
- (a) Round phase `Active` and `!UResolutionQueue::IsLatched()` (the queue already voids the item after the latch, so this is a defensive re-check);
- (b) `Roster.GetParticipant(Pawn)` (server-only binding lookup) yields P with `GetState(P) == Active`;
- (c) `Session.GetConnectionState(P) == Connected` (Session's state table, not the Roster's collapsed value; cross-review S-I4) **and** P has been in control of the pawn for the whole hold: the pawn's control epoch now equals the one recorded with the claim at `OnInteractExecuted` (`UStealthPawnFactory::GetControlEpoch`, ADR-0010 §4.4) **and** `!IsHandOverPending(Pawn)`. `HoldTimer` items carry only the `ActionEpoch`, so without this a hold begun under one controller could complete under another (a takeover, `PawnRecovery`, or a Player → AI hand-over committed mid-hold). These two ADR-0010 queries are injected into the core; Round's R13 "human-controlled" re-validation is defined in exactly these terms (GDD sync, Related);
- (d) the court is bound to a row eligible for `GetTeam(P)` whose site state is `Live`;
- (e) that row is not `Completed`.
Otherwise nothing changes and nothing is emitted beyond the IIF's normal `OnActionCompleted` and its `FHoldStatus` (identical bytes for counted and no-effect completions; AC-OC-08).

If it counts:
- **stage** (`Prelim1`/`Prelim2`): `ApplyCountedCompletion` marks the row `Completed`, applies the OC-4 unlock for that team, appends `ObjectiveStage(team, stage, ObjectiveId, ParticipantId, Key)` to the Round Event Log, writes the team's view (§6), and schedules the rival release (§7);
- **final**: appends `FinalCompleted(team, Key)` and calls `Round.DeclareObjectiveWin(team, Key)` **in the same slot** (AC-OC-07). Nothing else.

For every completion by a bound pawn (counted or not) it appends server-only `TendCompleted(ParticipantId, Court, Counted | NoEffect)` (Round change request 4; F3 measurement). Unbound (NPC decoy) completions append nothing. This is a bound/unbound branch, allowed only because its sole effect is a server-only Event Log entry that never reaches any client (named exception in the Guidelines). None of these entries goes through `UE_LOG` (Round R19).

**Parity statement (AC-OC-12).** A counted and a no-effect completion run the same IIF path, write the same `FActionState`, and produce the same replication to every non-team connection: the only extra traffic of a counted completion is the team view on the completing team's own controllers (owner-only relevancy, E1). No fixture, court, pawn or GameState property changes.

### 6. Writing the view (one writer, change-only, rebuildable)

- `UObjectivesService::PushTeamView(ETeam T)` builds the team's `FObjectiveTeamView` from the core store and writes it to `UObjectiveTeamViewComponent::View` of **each current controller** of team T's participants (Roster `GetTeamMembers(T)`, controller from Session's connection table), only if the value differs; with push model enabled it marks the property dirty (E3), otherwise the normal compare path sends it. Writes happen inside slots or inside the `OnRoundActive` step; several writes in one frame reach the client as one change (E4, OBJ-V2), which is how a 0 → 2 rival rise arrives as one update.
- **Rebuild on controller replacement and rejoin.** ADR-0005 replaces PlayerControllers at seamless travel and rejoin re-creates one; Roster `RebindView` re-pushes the Roster part and, by ADR-0005's rule, does not depend on providers registered at world wiring — "any consumer that needs world wiring subscribes to Roster events instead". Objectives therefore **subscribes to the Roster's rebind event** `URosterStore::OnParticipantViewRebound(FParticipantId)` (ADR-0005 §2; the Roster GDD's `OnViewRebound(ParticipantId)`), raised after `RebindView` and writes that participant's team view **inline in the handler** (on the server, game thread). At seamless travel no write is needed: Objectives state lives for one round, so a new controller's default (empty) view is already correct until `Active` entry. On a rejoin inside a round, the handler writes the team's current view (AC-OC-15).
- **Push model is mandatory for this part** (ADR-0001 defines `UTeamViewComponentBase` with push-model properties): `View` is registered with `FDoRepLifetimeParams{ .bIsPushBased = true }` and every write calls `MARK_PROPERTY_DIRTY_FROM_NAME(UObjectiveTeamViewComponent, View, this)`, so the view keeps updating whether `Net.IsPushModelEnabled` is on or off (E3). `View` uses `ReplicatedUsing=OnRep_View`, but presentation never depends on the RepNotify (ADR-0021 samples carriers each frame; the listen host gets no RepNotify for its own writes).
- **Sub-object registration**: the component replicates as a component of `AStealthPlayerController`, which must set `bReplicateUsingRegisteredSubObjectList = true` (default false in 5.8.4, E2; ADR-0001 owns the controller setting); components are registered by the engine after BeginPlay, not with `AddReplicatedSubObject`.
- The listen host's own controller holds the same component written by the same code; the host's HUD reads it through its own controller like any client (ADR-0001 host parity).
- **Allowlist (ADR-0003 §8):** one row for `UObjectiveTeamViewComponent::View`, audience Team, carrier C-TeamView, mapped to the IVP rows of the §4 table. Added with the implementing story; the reflection audit fails the build otherwise.

### 7. The rival signal (`RivalPrelimCount`, OC-12, F4)

All arithmetic in **integer milliseconds** (ADR-0006 §5; `MinMs`, `MaxMs`, `QMs` rounded once at tuning freeze):

1. On each counted **preliminary** completion e (either team), increment the round-global ordinal `k` (both teams together, 1-based) and draw `Δ_ms = UniformInRange(FSecretStream(GetServerSubSeed(StageSignal, k), Tag("OC_STAGE")), MinMs, MaxMs)`. The draw is keyed by `k` only — never by team, step, completer or court (AC-OC-10).
2. `t_c_ms = llround(ServerNow() × 1000)` at the completion slot; `t0_ms = llround(ActivePhaseStart × 1000)`; `t_rel_ms = t0_ms + QMs × ceil((t_c_ms + Δ_ms − t0_ms) / QMs)` (integer ceiling division in `int64`; the numerator is always positive because `t_c ≥ t0` and `Δ_ms ≥ 30,000`, asserted). Equal integer-ms release times give identical double due times, so releases on one grid tick resolve in one drain.
3. `ScheduleAt(t_rel_ms / 1000.0, TimerFired{StageSignal, ViewingTeam = opponent of e's team, Ordinal = k}, Expected = round epoch)`; record it as pending. One item per pending release; nothing is scheduled when nothing is pending (AC-OC-16).
4. The `StageSignal` resolver (registered by the composition root with `RegisterTimerResolver(ETimerKind::StageSignal, …)`, ADR-0002 §5) increments that viewing team's released count, appends `StageSignalReleased(viewingTeam, count)` (server-only), and writes the viewing team's view. Two releases due on one grid tick resolve in the same drain and reach clients as one change (count may rise 0 → 2; releases are unordered, so no coupling between steps). The count never decreases and never exceeds 2 (asserted).
5. The final is never counted. After the latch the queue returns `Rejected_Latched` for pending releases (nothing published in `Ended`); `Reset` drops them by epoch; `OnRosterReset` clears the pending list.
6. `bRivalSignalEnabled == false`, or a 1v0 dev round (`bAllowEmptyTeamDev` with an empty team), schedules nothing; `RivalPrelimCount` stays 0 and is never written (AC-OC-09).

The grid anchor t0 is Round's server-only `ActivePhaseStart`; the grid is shared by both teams, so a release time carries no team-specific timing.

### 8. Tuning (ADR-0004 rows)

`FObjectivesTuning_Public` (audience All, client face for owner ring prediction): `ObjectiveInteractSec`, `TendCueDelaySec`, `FixtureInteractSideCount`. `FObjectivesTuning_Server`: `ObjectivesPerStageCount`, `bRivalSignalEnabled`, `RivalSignalDelayMinSec`, `RivalSignalDelayMaxSec`, `RivalSignalReleaseQuantumSec`, `ClueMatchMaxCount`, `ObjectiveStallGapDefectSec`, `ObjectiveFinalDwellDefectSec`. Ranges, defaults and latches exactly as the GDD Tuning Knobs table. Invariants as ADR-0004 `Check` rows (never repair a value): **I-OC1** `ObjectiveInteractSec ≥ CrowdAmbientTouchMaxSec + 0.5` → Warn; **I-OC2** `W_eff = Max − Min + Q`: < 100 s Reject, < 163 s Warn; and Reject unless `Max ≥ Min + 30`; **I-OC3** `TendCueDelaySec + 0.25 ≥ CrowdDecoyEarlyReleaseMinSec` and `TendCueDelaySec ≤ ObjectiveInteractSec − 1.0` → Reject.

### 9. Presentation inputs this ADR exposes (consumed by ADR-0021)

A View helper (`StealthGameplay/Public/View/Features/Objectives/ObjectiveViewHelpers.h`, pure functions): `IsOwnLiveSite(const FObjectiveTeamView&, FCourtFixtureId)`; `GetStageCardRow(const FObjectiveTeamView&, const UObjectiveChainData&)`; `BuildClueText(FClueTag)`; `RingIsMarigold(View, OwnHoldTarget, HoldServerStartTime, EstimatedServerNow, TendCueDelaySec)` = own live site **and** `EstimatedServerNow − HoldServerStartTime ≥ TendCueDelaySec` (OC-6; Bone otherwise; never Marigold for an informant). The ring's own-hold target and start come from the owner's outstanding Begin and `FHoldStatus` (ADR-0011); the server sends nothing extra for the ring. Rival steps fill from `RivalPrelimCount` only and never show a final step (HUD-R11).

### Architecture Diagram

```
 PREPARING (server)                                   ACTIVE (server, inside queue slots only)
 UObjectivesChainRegistry (GI, View) ─ DA_ObjectiveChain_Cacao (cooked, AlwaysCook; also on clients)
        │ ValidateChain ──► fail: Round PreparingFailed(ObjectiveChainInvalid)
 OnLayoutReady ─► UObjectivesService.Bind(GetSiteAssignment_Server)      ── server memory only
        │          clue counts ─► ClueTooBroad (Event Log, once per LayoutHash)
 OnRoundActive(Epoch, ActivePhaseStart) ─► P1 Live (both teams), t0 ─► PushTeamView(T0), PushTeamView(T1)

 IIF slot (Begin) ─ ICommandPredicate.CanBegin: legality class + claim count ─► Allow | Reject(NotNow)
                 └─ IInteractTargetOwner.OnInteractExecuted ─► claim a face
 IIF slot (HoldTimer Complete) ─ IInteractTargetOwner.OnInteractEnded(Completed, Key)
        ├─ gate (a)-(e) false ─► nothing (TendCompleted NoEffect if bound)
        ├─ stage ─► Completed + unlock ─► Event Log ObjectiveStage ─► PushTeamView(T)
        │          └─► k++ ; Δ = FSecretStream(StageSignal, k) ; t_rel on grid ─► ScheduleAt(TimerFired StageSignal)
        └─ final ─► Event Log FinalCompleted ─► Round.DeclareObjectiveWin(T, Key)   (same slot)
 TimerFired(StageSignal) slot ─► RivalPrelimCount[opponent]++ ─► PushTeamView(opponent)

 WIRE: AStealthPlayerController (owner-only) ──► UObjectiveTeamViewComponent.View (one struct)
       team T members only; never GameState, pawn, fixture or PlayerState
 CLIENT: HUD (ADR-0021) reads own controller's View + cooked chain asset + client tuning + FHoldStatus
```

### Key Interfaces

```cpp
// --- StealthGameplay/Public/View/Core/ (client-safe; ADR-0014 asked to own) -----------------------
USTRUCT() struct FCourtFixtureId { GENERATED_BODY() UPROPERTY() uint16 SocketId = 0xFFFF; };
USTRUCT() struct FClueTag { GENERATED_BODY() UPROPERTY() uint8 AreaType = 0; UPROPERTY() uint8 FeatureKind = 0; };

// --- StealthGameplay/Public/View/Features/Objectives/ ---------------------------------------------
UENUM() enum class EObjectiveStage : uint8 { Prelim1, Prelim2, Final };
UENUM() enum class EObjectiveEligibility : uint8 { Team0, Team1, Both };
UENUM() enum class EObjectiveSiteRole : uint8 { P1, P2, Final };
UENUM() enum class EObjectiveTeamStage : uint8 { None, Stage1, Stage2, Final };
USTRUCT() struct FObjectiveDef { /* §2 */ };
UCLASS() class UObjectiveChainData : public UPrimaryDataAsset {
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly) uint16 ChainSerial = 0;
    UPROPERTY(EditDefaultsOnly) TArray<FObjectiveDef> Definitions;          // <= 16 rows
    virtual FPrimaryAssetId GetPrimaryAssetId() const override;          // ("ObjectiveChain", GetFName())
};
USTRUCT() struct FObjectiveLiveSite { /* §4 */ };
USTRUCT() struct FObjectiveTeamView { /* §4 */ };
UCLASS() class UObjectiveTeamViewComponent : public UTeamViewComponentBase { /* §4: one replicated View */ };

// --- StealthGameplay/Public/Server/Features/Objectives/ (guarded) --------------------------------
UCLASS(NotBlueprintType, NotBlueprintable)
class UObjectivesService : public UWorldSubsystem,
                           public ICommandPredicate,        // Interact (ADR-0011)
                           public IInteractTargetOwner,     // §5.1 (ADR-0011 amendment)
                           public IQueueResolver            // TimerFired(StageSignal) (ADR-0002)
{                          // + binds URosterStore::OnParticipantViewRebound (§6) from the composition root
    GENERATED_BODY()
public:
    EChainValidation ValidateSelectedChain(const FStealthServerTuning&) const;   // Preparing
    void HandleLayoutReady(uint64 LayoutId, int32 Seed);                        // bind
    void HandleRoundActive(FEpochToken Epoch, double ActivePhaseStart);         // P1 live, push views
    void DebugCompleteStage(ETeam Team);                                        // !UE_BUILD_SHIPPING, Round DebugItem slot
private:
    FObjectivesCore Core;   // plain C++; injected: IObjectivesRosterView, ISessionConnectionView,
                            // IRoundObjectiveHooks {IsActive, DeclareObjectiveWin, AppendEvent},
                            // IPawnControlView {GetControlEpoch, IsHandOverPending} (ADR-0010),
                            // IQueueTimers {ScheduleAt, CurrentKey, ServerNow}, ISecretDraw {StageSignal}
};
```

### Implementation Guidelines

- Objectives must never spawn an actor for a court, an objective or a site, replicated or not; court fixtures stay the layout's local non-replicated components.
- Objectives must have no tick function, no `FTimerManager` timer and no latent action; its only timers are `TimerFired(StageSignal)` items scheduled while a release is pending.
- `FSiteAssignment` must be read only through `UStealthLayoutService::GetSiteAssignment_Server()` inside `UObjectivesService`/`FObjectivesCore`; no client module, Crowd, IIF or Presentation code may include the Objectives `Public/Server/` headers.
- Team-audience objective data must travel only in `UObjectiveTeamViewComponent::View`; it must never appear on the GameState, `FRoundPublicState`, a PlayerState, a pawn, a fixture or an RPC.
- The view must never contain a locked site, an opponent site, opponent progress other than `RivalPrelimCount`, anyone's hold progress, or the final before the team's preliminaries are complete; it must be written only by `UObjectivesService` and only inside a queue slot, the `OnRoundActive` step or the Roster rebind-event handler; every write must mark the push-model property dirty.
- The Interact predicate must read only the court, its claim count and the pawn's legality class; it must return `Reject(NotNow)` or `Allow` and never `AllowNoEffect` at Begin.
- The hold duration must be one value for every court (`GetInteractHoldSec()` has no court parameter); a chain row with `bOverrideInteractSec` must fail validation.
- Completion must be judged only in `OnInteractEnded(Completed)` inside the `HoldTimer(Complete)` slot, against live state, with gate (a)-(e); the final must call `DeclareObjectiveWin(Team, CurrentKey())` in that slot.
- A completion must never cancel another pawn's hold.
- The rival delay must be drawn only from `FSecretStream(GetServerSubSeed(StageSignal, Ordinal), Tag("OC_STAGE"))` in integer milliseconds, keyed only by the round-global prelim ordinal; release times must lie on the grid `t0 + k·Q` anchored at `ActivePhaseStart`.
- Card text and clue phrases must be rendered client-side from cooked data; only indices and enums may travel.
- Event Log entries (`ObjectiveStage`, `FinalCompleted`, `TendCompleted`, `StageSignalReleased`, `ClueTooBroad`) must go through the Round Event Log `Append` only, never `UE_LOG`; site ids, the assignment and draws must never be logged elsewhere.
- No code may branch on player vs NPC except the gate's server-only Roster/Session/control-epoch queries in §5.4 and the bound-only `TendCompleted` Event Log entry (server-only, never replicated).
- Gate (c) must compare the control epoch recorded with the claim at Begin to the current one and require `!IsHandOverPending`; an interact-end call outside a slot (invalid key) must only release the claim.
- The prototype `AStealthObjective` and its replicated `bCompleted`/`InteractorCount` must be replaced, not ported.

## Alternatives Considered

### Alternative 1: A replicated objective actor per site (the prototype)
- **Description**: `AStealthObjective` at each objective court with replicated completion and interactor state, relevant to everyone or filtered by team.
- **Pros**: Simple; proven in the prototype; easy world-space visuals.
- **Cons**: Actors exist only at objective courts (site oracle), replicate `bCompleted`/`InteractorCount` to everyone (completion and activity oracle), and a team filter would be a team-relevancy rule ADR-0001 bans; LSC R2.6 requires zero replicated actors or identical ones on every court.
- **Rejection Reason**: Violates IVP R3/R4, L24/L33 and SP-1 by construction.

### Alternative 2: Objective fields inside the Roster's C-TeamView part
- **Description**: Add stage, live sites and rival count to `URosterTeamViewComponent`'s owner/teammate structs.
- **Pros**: One component; one rebuild path already exists.
- **Cons**: The Roster (Foundation) would own feature content and need feature types; ADR-0001 §5 says the Roster owns none of the feature parts' contents.
- **Rejection Reason**: Layer inversion and ownership violation.

### Alternative 3: Owner-only RPCs instead of replicated view state
- **Description**: `ClientObjectiveUpdate(...)` reliable RPCs to each teammate on change.
- **Pros**: Exact event delivery; no property comparison.
- **Cons**: Rejoin and controller replacement need a full re-send path anyway; presentation would become event-driven rather than state-derived; two rises in one tick become two events (a "0 → 1 → 2" stutter the GDD forbids); ordering against other owner replies adds cases.
- **Rejection Reason**: State on the carrier is idempotent and rebuildable; RPCs add cases without benefit.

### Alternative 4: Completion via `OnActionCompleted` subscription
- **Description**: Objectives listens to the action hub and looks up the pawn's last target.
- **Pros**: No IIF interface change.
- **Cons**: The delegate carries neither the court nor the key; the IIF slot state is already `Idle` when the event fires; the gate would depend on listener order relative to other subscribers.
- **Rejection Reason**: The gate must run deterministically inside the slot with the court and key in hand (OC-10, AC-OC-07); a small consumer-owned interface states that contract explicitly.

### Alternative 5: Rival signal with per-team timers or a fixed delay
- **Description**: Release each team's progress after a fixed or per-team delay, or with the engine timer manager.
- **Pros**: Simpler arithmetic.
- **Cons**: Withdrawn `RivalStageStep` shape (IVP ruling B, L34); a fixed delay confirms a watched tend; engine timers break ADR-0002's single order.
- **Rejection Reason**: Fails F4's `LR_sig ≤ 2.4` bound and ADR-0002.

### Alternative 6: Replicating card text (`FText`) and clue strings
- **Description**: Send the stage card and clue as text in the view.
- **Pros**: Clients need no chain asset.
- **Cons**: Larger payload; localisation would follow the server's culture; text change detection on every write.
- **Rejection Reason**: Indices into cooked, localised data are smaller and culture-correct.

### Alternative 7: Per-definition hold duration (`InteractSecOverride` honoured per row)
- **Description**: Let a definition set its own `D`, as OC-2 describes.
- **Pros**: Design flexibility for future chains.
- **Cons**: `D` would differ between a live site and every other court, a timing oracle (IVP R7.4a, AC-OC-08); the client ring predicts `D` from public tuning and would be wrong.
- **Rejection Reason**: Breaks hidden-site parity; deferred to a future chain-wide override that also reaches clients.

## Consequences

### Positive
- No objective data reaches the other team by construction: the only carrier is the owner-only controller part, and no actor exists to leak.
- The gate, the unlock and the win run in one slot with the hold's key; ties with strikes and the other team's final are resolved by key compare alone.
- The rival signal is reproducible in headless tests from a pinned sub-seed (AC-OC-09/10), and nothing is scheduled while nothing is pending (AC-OC-16).
- `FObjectivesCore` is plain C++ with injected views, so all logic ACs run headless.
- The hold duration is role-blind by signature, not by convention.

### Negative
- One amendment to ADR-0011 is needed (interact-target interface and its lifecycle rules); until it lands, the Objectives stories cannot start.
- Two GDD rule interpretations need game-designer sign-off before acceptance (§2, §5.3).
- `InteractSecOverride` is unusable in MVP (validation rejects it), narrowing the GDD's per-definition field.
- Clients carry both teams' card text in the cooked asset (static, public chain data; the secret is the court, not the text).
- A stand-point claim models "occupied" as "being tended"; a body standing idle at a face blocks only physically.

## Risks
- **R1 Claim vs physical occupancy diverge** (a pawn tends from a face another body physically blocks): the IIF's reach/facing check governs Begin; claims only cap concurrent holds at 4. Mitigation: AC-OC-31 and a PIE crowding test; if playtests show confusion, add a physical overlap test at the claimed face (server-only, kind-blind).
- **R2 Rebind event fires before the new controller's component is ready**: the handler checks the component exists and, if not, writes on the controller's `BeginPlay`-time registration notice (component default is empty, so nothing wrong is ever shown); AC-OC-15 covers rejoin.
- **R3 `ClueTag` not delivered by LSC**: area-type-only clue (GDD OQ3 fallback); AC-OC-13 still holds.
- **R4 Two writes in a frame sent separately** (OBJ-V2 fails under push model with immediate flush): both writes are within one drain before replication runs, so the risk is only a test artefact; if observed, write views once at drain end from a dirty-team set.
- **R5 Host reads `FSiteAssignment`**: accepted (IVP R10) until the Dedicated Server; code discipline and the include guard keep it out of presentation.
- **R6 `ActivePhaseStart` vs `ServerNow` precision**: both doubles on the server; integer-ms conversion at use keeps the grid exact.

## GDD Requirements Addressed

| GDD System | Requirement | How This ADR Addresses It |
|------------|-------------|--------------------------|
| objectives-chain.md | OC-1 scope; all state server-side, round-scoped, reset on `OnRosterReset` | §1, §3 store and lifetime |
| objectives-chain.md | OC-2 `FObjectiveDef` fields; OC-3 chain per team, binding at `OnLayoutReady`, `ObjectivesPerStageCount` > 1 rejected | §2 asset and validation; §3 bind |
| objectives-chain.md | OC-4 per-team one-way progress | §3 progress only in the completion follow-on |
| objectives-chain.md | OC-5 team view contents; final location only after own prelims; presentation only | §4 struct and field map |
| objectives-chain.md | OC-6 prompt at every court; cue after `TendCueDelaySec`; I-OC3 | §5.3 Allow at every court; §8 invariant; §9 ring rule |
| objectives-chain.md | OC-7 predicate reads only site, occupancy, legality; OC-8 4 stand points, full-fixture reject, no stacking, no cancel | §5.3 predicate and claims; §5.4 no cancel |
| objectives-chain.md | OC-9 decoys and feints through the same path | §5 kind-blind interface, no caller identity |
| objectives-chain.md | OC-10 completion gate (a)-(e) in the `HoldTimer(Complete)` slot; `DeclareObjectiveWin` | §5.4 |
| objectives-chain.md | OC-11 locked-site oracle guard | §5.3/§5.4 identical path; §5.2 role-blind `D` |
| objectives-chain.md | OC-12 `RivalPrelimCount`: ordinal-keyed fresh delay, global grid, count not order, no timer when idle, discarded at latch, Team carrier | §7 |
| objectives-chain.md | OC-14 hold time 3 s, hard 2-6 s | §5.2, §8 |
| objectives-chain.md | OC-15 stage card and clue, no world marker; `ClueTooBroad` | §4 clue; §3 clue count; §9 |
| objectives-chain.md | OC-16 one MVP chain | §2 |
| objectives-chain.md | OC-17 bind at `OnLayoutReady`, push at `Active`, rejoin rebuild | §3, §6 |
| objectives-chain.md | SP-1..SP-8 (hidden final, nothing to camp, waiting earns nothing, no timer, no reveal, signal felt not shown) | §4 view limits, §5 identical paths, §7 no idle timer, Guidelines "no tick/timer" |
| objectives-chain.md | AC-OC-01..32 (test hooks) | headless `FObjectivesCore`; `StealthGame.Objectives.*` (Validation Criteria) |
| information-visibility-policy.md | R3 `ObjectiveProgress` own Team / opponent ServerOnly; `RivalPrelimCount` Team (ruling B); `SiteAssignment` ServerOnly; `SiteKnownToTeam` presentation only | §4 field map; §5.3 predicate ignores site state; §3 assignment server-only |
| information-visibility-policy.md | R4 Team carrier = C-TeamView only; L24/L33/L34; AC-IVP-19/40/41 | §4, §6, §7 |
| round-match-framework.md | R13 `ObjectiveCompletion` follow-on row; `TimerFired(StageSignal)`; R7 `OnRoundActive`/`ActivePhaseStart`; R19 Event Log | §5.4, §7, §3 |
| architecture.md | TR-oc-001..007 | §2 (001), §3 (002), §4/§6 (003), §5.3 (004), §5.4 (005), §7 (006), Guidelines (007) |

## Performance Implications
- **CPU**: Predicate O(1); completion O(team size) view writes; at most ~5 counted completions and ~4 releases per round. Steady-state cost ≈ 0 (no tick). Counted inside the architecture's "concealment/IIF/objectives misc" 1.0 ms line; Objectives claims ≤ 0.05 ms average on the listen host.
- **Memory**: < 2 KB server store; one small component per controller; the chain asset is a few KB of text.
- **Load Time**: One small cooked asset loaded at session start.
- **Network**: About 16 bytes per view change per team member, a handful of changes per round (≤ 10 controllers): negligible against the 50 KB/s per-client budget. Nothing to non-team connections.

## Migration Plan
The prototype's `AStealthObjective` (replicated actor per site), its local progress ticking, `UStealthSettings::ObjectiveInteractTime` and the city generator's objective relocation are replaced, not migrated (ADR-0007 promotion rule). The prototype keeps compiling until the Objectives epic lands; deleting its files needs user approval. Project config needed by the implementing story (outside this run's write scope): Asset Manager primary asset type `ObjectiveChain` with `AlwaysCook` in `DefaultGame.ini`.

## Validation Criteria
- Headless (`StealthGame.Objectives.*`, ADR-0008 layout): `ChainBindAndValidate` (AC-OC-01, incl. override rejection), `ProgressOnlyByCompletion` (02), `PredicateSiteAndClassOnly` (04, static include check), `NoStackNoCancel` (05), `CompletionGate` (06), `FollowOnInSlot` (07), `RivalSignalReleaseGrid` (09), `RivalSignalDistribution` (10), `ResetAndRejoin` (15), `WaitingEarnsNothing` (16), `FinalNeedsOwnUnlock` (17), `RivalSignalBound` (22, with ADR-0004), `ClueBoundAndRoleBlind` (13), `ClueTooBroadLogged` (28), `NoTeamViewBeforeActive` (29), `TendCueDelay` (32).
- PIE-2/net harness: `TeamViewContents` (03), `HiddenSiteOracle` (08, byte-exact owner replies), `CompletionNoWorldSignature` (12), `IngressAcceptsAnyCourt` (26, with AC-IVP-41), `EndToEndCacaoChain` (14), AC-IVP-19 and AC-IVP-40.
- Allowlist audit passes with the `UObjectiveTeamViewComponent::View` row; include lint shows no client module including Objectives `Public/Server/` headers.

## Related
- Depends on ADR-0001, ADR-0002, ADR-0004, ADR-0005, ADR-0006, ADR-0007, ADR-0010, ADR-0011, ADR-0014; enables ADR-0021, ADR-0016 (decoy predicate), ADR-0023.
- Design: `design/gdd/objectives-chain.md`, `design/gdd/information-visibility-policy.md`, `design/gdd/round-match-framework.md`, `design/gdd/input-interaction-framework.md`, `design/gdd/level-socket-contract.md`, `design/gdd/hud.md`.
- **Amendment requests (not applied here):** ADR-0011 — add `IInteractTargetOwner` and `UActionHub::RegisterInteractTargetOwner` with the §5.1 lifecycle rules (release-only call with an invalid key outside a slot, `D` read once, claim kept through a pending debounced End, single owner, drain-end consistency check); Roster/ADR-0005 — name the Roster event raised after `RebindView` that feature parts subscribe to (§6); ADR-0014 — own the `FCourtFixtureId` (= court SocketId) declaration and publish per-court `ClueTag` when LSC delivers it (§3.1, §4).
- **GDD sync requests (not applied here):** objectives-chain.md OC-2 — `InteractSecOverride` is rejected by validation until a chain-wide, client-visible override exists (§2, Alternative 7); OC-8 — "physically occupied" is implemented as active-hold claims (§5.3); both need game-designer sign-off (acceptance condition); removing `InteractSecOverride` from OC-2 is a pending user decision (schema_changes); round-match-framework.md — define R13's "human-controlled" re-validation as "control epoch unchanged since the hold's Begin and no hand-over pending" (§5.4 (c)), and list the Event Log entries `TendCompleted`, `StageSignalReleased`, `ClueTooBroad` (already requested by Objectives CR 4).

## Amendments
Architecture-review fix pass (`docs/architecture/architecture-review-2026-10-25.md`); decision unchanged, Status unchanged (Proposed).
- §6, Key Interfaces: the rebind event is named `URosterStore::OnParticipantViewRebound(FParticipantId)` (ADR-0005 §2), which is the Roster GDD's `OnViewRebound(ParticipantId)` — architecture-review fix 2026-10-25 (other finding).
- `TimerFired(StageSignal)` items use ADR-0002 §2a's generic Foundation `FTimerPayload` (`Subject` = court fixture, `Seq` = completion ordinal `k`, `Phase` = team) — architecture-review fix 2026-10-25 (Conflict 1).
- Requests answered (2026-10-24): the ADR-0011 `IInteractTargetOwner` amendment, the ADR-0005 rebind-event naming and the ADR-0014 `FCourtFixtureId` ownership (Related) are applied by their owners. The Status line's "ADR-0011 (with the interact-target amendment requested in §5)" condition is met. The GDD sign-offs and pending user decisions it names are unchanged — architecture-review fix 2026-10-25 (Conflict 9).
