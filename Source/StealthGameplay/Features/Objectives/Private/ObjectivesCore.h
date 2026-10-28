#pragma once

#include "CoreMinimal.h"
#include "ObjectivesClaims.h"
#include "Server/ObjectivesSeams.h"
#include "Server/StealthTuningSubstructs.h"
#include "View/ObjectiveChainData.h"
#include "View/StealthTuningPublicSubstructs.h"

class IStealthFaultSink;

/**
 * The headless Objectives core (ADR-0020 sections 2, 3, 5; GDD objectives-chain.md OC-3, OC-4, OC-7, OC-8, OC-10, OC-17;
 * stories oc-003, oc-004, oc-006, oc-008, oc-009). Plain C++: no UWorld, no tick, no timer, no actor, no engine time. It reads
 * the world only through the injected seams of Server/ObjectivesSeams.h, so headless specs drive it with stubs. The store is
 * round-scoped and under 2 KB; every transition is O(1) in the chain size.
 *
 * Lifecycle (OC-17):
 *   ValidateChain        at Preparing (the service maps a failure to Round's PreparingFailed(ObjectiveChainInvalid))
 *   Bind                 at OnLayoutReady: Prelim1(T) -> P1[T], Prelim2(T) -> P2[T], Final -> Final, every site Locked; the
 *                        assignment is read through IObjectivesAssignmentSource::GetSiteAssignment_Server only
 *   HandleActiveEntry    at OnRoundActive: both teams Stage1, their Prelim1 sites Live
 *   OnInteractExecuted / OnInteractEnded   the interact-target owner calls (claims; the completion gate (a)-(e))
 *   ApplyCountedCompletion                 the ONLY writer of site state (the gate and the debug completion share it)
 *   BuildTeamView        the team view (story oc-010): a pure function of the store; PushView writes it change-only
 *   HandleRoundEnded     frozen: nothing changes afterwards
 *   Reset                OnRosterReset / Deinitialize (every site state, claim, view cache and the rival count)
 *
 * Site state per (team, definition) is Locked -> Live -> Completed with no backward transition and exactly three triggers:
 * Active entry, the team's own counted completion of every definition of a stage, and Reset. Nothing keyed on elapsed time
 * exists (SP-3: waiting earns nothing). SP-4: a team that has not unlocked the final cannot complete it, because the gate's
 * (d) sees the court as not Live for that team, and this core never sets the final Live for a team early.
 */

/** A bound site's state for one team (OC-2 runtime site state). */
enum class EObjectiveSiteState : uint8
{
	Locked,
	Live,
	Completed,
};

/** ValidateChain's verdict (ADR-0020 Key Interfaces); the service maps ObjectiveChainInvalid to PreparingFailed. */
enum class EChainValidation : uint8
{
	Valid,
	ObjectiveChainInvalid,
};

/** The first failing clause (travels with the result so the log is useful). Values are stable: append only. */
enum class EChainValidationClause : uint8
{
	None,
	/** No chain asset (zero cooked chains, or the selected one did not load). */
	NoChain,
	/** More than one cooked chain and no selection rule (ADR-0020 section 2: no silent pick). */
	AmbiguousChain,
	/** Rule 2: ObjectivesPerStageCount is not 1. */
	PerStageCount,
	/** Rule 3: a row's CompletionsRequiredCount is not 1. */
	CompletionsRequired,
	/** More than 16 rows (the CompletedDefMask width). */
	TooManyRows,
	/** Rule 1: two rows share an ObjectiveId (or an id is None). */
	DuplicateId,
	/** Rule 1: a row's SiteRole does not match its StageKind. */
	RoleStageMismatch,
	/** Rule 1: an empty (or whitespace-only) card title. */
	EmptyCardTitle,
	/** Rule 1: an empty (or whitespace-only) card line. */
	EmptyCardLine,
	/** Rule 1: a team does not have exactly ObjectivesPerStageCount Prelim1 and Prelim2 rows of its own eligibility. */
	TeamStageRows,
	/** Rule 1: not exactly one Final row, or the Final is not eligible for Both. */
	FinalRow,
	/** A prelim row is eligible for Both (a prelim of one team never advances the other). */
	PrelimEligibility,
	/** Bind: the assignment is missing a site (or a site is reused across roles). */
	AssignmentIncomplete,
};

/** A validation or bind result: the verdict, the first failing clause and the row it was found on (INDEX_NONE for none). */
struct FChainValidationResult
{
	EChainValidation Result = EChainValidation::Valid;
	EChainValidationClause Clause = EChainValidationClause::None;
	int32 Row = INDEX_NONE;

	bool IsValid() const { return Result == EChainValidation::Valid; }

	static FChainValidationResult Fail(EChainValidationClause InClause, int32 InRow = INDEX_NONE)
	{
		FChainValidationResult Out;
		Out.Result = EChainValidation::ObjectiveChainInvalid;
		Out.Clause = InClause;
		Out.Row = InRow;
		return Out;
	}
};

/** One bound definition (ADR-0020 FChainBinding): its court (the LSC SocketId; names a court, nothing about its role). */
struct FChainBinding
{
	FCourtFixtureId Court;
	EObjectiveStage Stage = EObjectiveStage::Prelim1;
	EObjectiveEligibility Eligibility = EObjectiveEligibility::Team0;
	FName ObjectiveId;
};

/** How an interact end was treated. */
enum class EObjectivesCompletion : uint8
{
	/** A cancel, or an end outside a slot (invalid key): the claim was released, the gate was not entered, nothing logged. */
	ClaimReleaseOnly,
	/** A completion that failed the gate: the IIF's AllowNoEffect outcome, nothing changed. */
	NoEffect,
	/** A counted preliminary stage. */
	CountedStage,
	/** A counted final: FinalCompleted logged and DeclareObjectiveWin called in the slot. */
	CountedFinal,
};

/** The interact end kinds (ADR-0020 5.1 EInteractEnd: Completed | Cancelled). */
enum class EObjectivesInteractEnd : uint8
{
	Completed,
	Cancelled,
};

/** What ApplyCountedCompletion did. */
struct FObjectivesAppliedCompletion
{
	bool bApplied = false;
	EObjectiveStage Stage = EObjectiveStage::Prelim1;
	/** The completion finished its stage and made the next stage Live for that team (the final is never "next"). */
	bool bUnlockedNext = false;
};

/** The core's injected seams (all optional until a story needs them; a missing one makes its feature a no-op). */
struct FObjectivesCoreDeps
{
	const IObjectivesRosterView* Roster = nullptr;
	const IObjectivesSessionView* Session = nullptr;
	const IObjectivesPawnControlView* PawnControl = nullptr;
	IRoundObjectiveHooks* Round = nullptr;
	const IObjectivesCourtGeometry* Courts = nullptr;
	IObjectivesViewSink* Views = nullptr;
	IObjectivesRivalRelease* RivalRelease = nullptr;
	IStealthFaultSink* Faults = nullptr;
};

class STEALTHGAMEPLAY_API FObjectivesCore
{
public:
	/** Teams (0 and 1). */
	static constexpr int32 TeamCount = 2;

	FObjectivesCore() = default;

	/** (Re)binds the seams. */
	void Initialize(const FObjectivesCoreDeps& InDeps) { Deps = InDeps; }

	/** Sets the frozen tuning the core reads (the latch-Round keys are read at Bind; D at every Begin). */
	void SetTuning(const FObjectivesTuning_Public& InPublic, const FObjectivesTuning_Server& InServer);

	const FObjectivesTuning_Public& GetPublicTuning() const { return PublicTuning; }
	const FObjectivesTuning_Server& GetServerTuning() const { return ServerTuning; }

	// --- Preparing and OnLayoutReady (stories oc-003, oc-005) -------------------------------------------------------

	/**
	 * ADR-0020 section 2 validation rules 1-5 (rule 4, the removed bOverrideInteractSec, stays a numbered placeholder and checks
	 * nothing): per team exactly ObjectivesPerStageCount Prelim1 and Prelim2 rows with that team's eligibility, exactly one
	 * Final with Both, SiteRole matching StageKind, unique ids, non-empty card text, ObjectivesPerStageCount 1,
	 * CompletionsRequiredCount 1, at most 16 rows. A null chain is NoChain. Pure.
	 */
	static FChainValidationResult ValidateChain(const UObjectiveChainData* Chain, const FObjectivesTuning_Server& Tuning);

	/**
	 * Validates Chain, reads the assignment through Source.GetSiteAssignment_Server() and binds every definition to its court
	 * with every site Locked. Any failure (an invalid chain, no assignment, a missing or reused site) binds nothing and returns
	 * the failure. A second Bind replaces the first (a new layout). Never logs the assignment.
	 */
	FChainValidationResult Bind(const UObjectiveChainData* Chain, const IObjectivesAssignmentSource& Source);

	bool IsBound() const { return bBound; }

	// --- Active entry, round end, reset (story oc-004) ------------------------------------------------------------------

	/**
	 * OnRoundActive (inside Round's Active-entry step): stores t0 and the epoch, sets both teams Stage1 with their Prelim1
	 * sites Live, and pushes each team's view. False (nothing changed) when not bound or already Active for this epoch.
	 */
	bool HandleActiveEntry(uint32 InRoundEpoch, double InActivePhaseStart);

	/** Round Ended: every state is frozen; a completion arriving afterwards is ignored. */
	void HandleRoundEnded() { bEnded = true; }

	/** OnRosterReset / Deinitialize: clears the store (unbound, nothing Live, no claims, no view cache, rival counts 0). */
	void Reset();

	bool IsActive() const { return bActive && !bEnded; }
	bool IsEnded() const { return bEnded; }
	uint32 GetRoundEpoch() const { return RoundEpoch; }
	double GetActivePhaseStart() const { return ActivePhaseStart; }

	/**
	 * The only writer of site state (ADR-0020 section 3): marks (Team, DefIndex) Completed and, when that completes every
	 * definition of its stage for Team, makes Team's next stage Live (Prelim1 -> Prelim2 -> Final) for that team only.
	 * Nothing happens unless the core is Active, the row is eligible for Team and its site is Live. Pure over the store.
	 */
	FObjectivesAppliedCompletion ApplyCountedCompletion(int32 Team, int32 DefIndex);

	// --- Queries --------------------------------------------------------------------------------------------------------

	EObjectiveSiteState GetSiteState(int32 Team, int32 DefIndex) const;
	EObjectiveTeamStage GetStageIndex(int32 Team) const;
	/** The team's own completed preliminary rows (never the final; story oc-010's view field). */
	uint16 GetCompletedDefMask(int32 Team) const;
	int32 GetDefinitionCount() const { return Bindings.Num(); }
	const FChainBinding* GetBinding(int32 DefIndex) const { return Bindings.IsValidIndex(DefIndex) ? &Bindings[DefIndex] : nullptr; }
	uint16 GetChainSerial() const { return ChainSerial; }
	/** The row of Team's whose court is Court and whose site is Live, or INDEX_NONE (the gate's (d)). */
	int32 FindLiveDefinition(int32 Team, FCourtFixtureId Court) const;
	/** Team's Live rows (its current stage's sites). */
	void GetLiveDefinitions(int32 Team, TArray<int32>& OutDefIndices) const;
	/** Preliminary stages Team completed this round (Round's TeamSummary, IRoundObjectiveProgress). */
	int32 GetCompletedPreliminaryStages(int32 Team) const;
	/** Counted preliminary completions of both teams (the round-global ordinal k of OC-12). */
	uint32 GetPrelimOrdinal() const { return PrelimOrdinal; }
	/** Rival preliminary completions released to Team (OC-12; 0..2). HOLD: written by stories oc-014/015's resolver only. */
	uint8 GetRivalPrelimCount(int32 Team) const { return Team >= 0 && Team < TeamCount ? RivalPrelimCount[Team] : 0; }

	// --- The team view (story oc-010) -----------------------------------------------------------------------------------

	/**
	 * Team's FObjectiveTeamView, a pure function of the store (ADR-0020 section 4): before Active entry (and after Reset) the
	 * empty default; afterwards RoundEpoch, ChainSerial, StageIndex, the team's OWN Live sites (its current stage only, at most
	 * ObjectivesPerStageCount; the final only once the team's own preliminaries are complete, SP-1) with their row index and
	 * clue, the team's own completed preliminary rows (never the final) and RivalPrelimCount. Never a Locked site, an opponent
	 * site, opponent progress other than RivalPrelimCount, or anyone's hold progress. Every member of Team gets this value.
	 */
	FObjectiveTeamView BuildTeamView(int32 Team) const;

	/** The view last pushed for Team (the change-only cache; the empty default before the first push). */
	const FObjectiveTeamView& GetLastPushedView(int32 Team) const;

	/**
	 * The one path to the view sink: builds Team's view and pushes it only when it differs from the last push (an identical
	 * second push is a no-op). Called at Active entry and after a counted preliminary stage (and, from oc-015, a rival release).
	 */
	void PushView(int32 Team);

	// --- The interact-target owner (stories oc-006, oc-008, oc-009) -----------------------------------------------------

	/** GetInteractHoldSec(): the frozen ObjectiveInteractSec, one value for every court (no court parameter, ADR-0020 5.2). */
	double GetInteractHoldSec() const { return static_cast<double>(PublicTuning.ObjectiveInteractSec); }

	/** FixtureInteractSideCount (the predicate's full-fixture bound). */
	int32 GetFixtureSideCount() const { return FMath::Clamp(PublicTuning.FixtureInteractSideCount, 1, FObjectivesClaimTable::MaxFaces); }

	/** Public occupancy: active-hold claims at Court (the predicate's only court input besides the court itself). */
	int32 GetClaimCount(FCourtFixtureId Court) const { return Claims.GetClaimCount(Court); }

	const FObjectivesClaimTable& GetClaims() const { return Claims; }
	FObjectivesClaimTable& GetClaimsMutable() { return Claims; }

	/**
	 * OnInteractExecuted (inline in the Begin's slot, every pawn kind, no caller identity): claims the free face nearest the
	 * pawn and records the pawn's control epoch with it. Returns the face index or INDEX_NONE (every face taken).
	 */
	int32 OnInteractExecuted(const APawn& Pawn, FCourtFixtureId Court, FQueueKey Key);

	/**
	 * OnInteractEnded: always releases the pawn's own claim. A cancel, or any call with an invalid key (round-end staging,
	 * pawn teardown), is a claim release only. A completion with a valid key runs the gate (ADR-0020 5.4) against live state:
	 *   (a) Round Active and the queue not latched; (b) the pawn is bound to P with GetState(P) Active; (c) Session's state of
	 *   P is Connected, the pawn's control epoch equals the one recorded with the claim and no hand-over is pending;
	 *   (d) Court is a Live site of a row eligible for P's team; (e) that row is not Completed.
	 * Counted stage: ApplyCountedCompletion, ObjectiveStage logged, the team view pushed, the rival release scheduled.
	 * Counted final: FinalCompleted logged and Round.DeclareObjectiveWin(team, Key) in this slot; nothing else (no view write,
	 * no release). Every completion by a BOUND pawn appends server-only TendCompleted(P, Court, Counted|NoEffect); an unbound
	 * crowd decoy appends nothing (the named bound/unbound exception: its sole effect is a server-only log entry).
	 * A completion never cancels another pawn's hold.
	 */
	EObjectivesCompletion OnInteractEnded(const APawn& Pawn, FCourtFixtureId Court, EObjectivesInteractEnd End, FQueueKey Key);

	/**
	 * A HoldTimer(Complete) rejected by the latch (the queue's OnRejectedLatched, ADR-0002 8.1): releases the claim and, for a
	 * bound pawn, appends TendCompleted(P, Court, NoEffect) with the Latched flag (Round R19; cross-review C-W35). Nothing else.
	 */
	void OnInteractCompleteLatched(const APawn& Pawn, FCourtFixtureId Court, FQueueKey Key);

	/**
	 * The non-Shipping debug completion's effect (ADR-0020 section 3; called from inside its own Round DebugItem slot): the
	 * same ApplyCountedCompletion as a counted tend on Team's current Live rows, with the same log, view and release effects
	 * (the final declares the win). False when Team has nothing Live (a Locked or Completed stage has no effect).
	 */
#if !UE_BUILD_SHIPPING
	bool ApplyDebugStageCompletion(int32 Team, FQueueKey Key);
#endif

private:
	/** Effects of a counted completion of (Team, DefIndex) by Participant (invalid for the debug path). */
	EObjectivesCompletion ApplyCountedEffects(int32 Team, int32 DefIndex, FParticipantId Participant, FQueueKey Key, bool bDebug);
	void Append(ERoundLogType Type, FQueueKey Key, FParticipantId Participant, TFunctionRef<void(FRoundLogEntry&)> Fill);
	void AppendTend(FParticipantId Participant, FCourtFixtureId Court, ERoundTendResult Result, bool bLatched, FQueueKey Key);
	static bool IsEligible(EObjectiveEligibility Eligibility, int32 Team);
	bool IsStageComplete(int32 Team, EObjectiveStage Stage) const;
	void SetStageLive(int32 Team, EObjectiveStage Stage);

	FObjectivesCoreDeps Deps;
	FObjectivesTuning_Public PublicTuning;
	FObjectivesTuning_Server ServerTuning;

	TArray<FChainBinding, TInlineAllocator<UObjectiveChainData::MaxDefinitions>> Bindings;
	EObjectiveSiteState SiteStates[TeamCount][UObjectiveChainData::MaxDefinitions] = {};
	EObjectiveTeamStage StageIndex[TeamCount] = { EObjectiveTeamStage::None, EObjectiveTeamStage::None };
	uint16 CompletedDefMask[TeamCount] = { 0, 0 };
	/** OC-12 released rival count per viewing team (HOLD for stories oc-014/015: nothing writes it before them). */
	uint8 RivalPrelimCount[TeamCount] = { 0, 0 };
	/** The change-only cache of PushView (story oc-010). */
	FObjectiveTeamView LastPushedView[TeamCount];
	bool bViewPushed[TeamCount] = { false, false };
	uint16 ChainSerial = 0;
	uint32 PrelimOrdinal = 0;
	uint32 RoundEpoch = 0;
	double ActivePhaseStart = 0.0;
	bool bBound = false;
	bool bActive = false;
	bool bEnded = false;

	FObjectivesClaimTable Claims;
};
