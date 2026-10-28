// Objectives Chain stories oc-005 (service shell), oc-006 (the interact-target owner), oc-008 (DebugCompleteStage), oc-009
// (the final), oc-011 (the team-view writer) and oc-012 (the Roster rebind and reset handlers): production/epics/objectives-chain/. The Interact predicate is ObjectivesInteractPredicate.cpp (its own
// translation unit for the static check of story oc-007). See Server/ObjectivesService.h.

#include "Server/ObjectivesService.h"

#include "Engine/World.h"
#include "ObjectivesCore.h"
#include "Server/Queue/StealthResolutionQueueCore.h"
#include "Server/Roster/StealthRosterStore.h"
#include "Server/StealthFaultSink.h"
#include "Server/StealthServerTuning.h"
#include "View/Core/StealthPawn.h"
#include "View/ObjectiveTeamViewComponent.h"
#include "View/ObjectivesChainRegistry.h"

namespace ObjectivesServiceLocal
{
	const FName FaultSource(TEXT("Objectives"));

	/** Fault details of the service (EStealthFault::InvariantViolation's Detail). */
	enum class EFaultDetail : int64
	{
		ActiveBeforeLayout = 0x0C0001,
		BindFailed = 0x0C0002,
	};
}

UObjectivesService::UObjectivesService()
	: Core(MakePimpl<FObjectivesCore>())
	, LastCompletion(EObjectivesCompletion::ClaimReleaseOnly)
{
}

bool UObjectivesService::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World != nullptr && World->IsGameWorld() && World->GetNetMode() != NM_Client && Super::ShouldCreateSubsystem(Outer);
}

void UObjectivesService::Deinitialize()
{
	Unbind();
	Super::Deinitialize();
}

UObjectivesService* UObjectivesService::Get(const UWorld* World)
{
	return World != nullptr ? World->GetSubsystem<UObjectivesService>() : nullptr;
}

bool UObjectivesService::Bind(const FObjectivesServiceInit& Init)
{
	Unbind();
	if (Init.Round == nullptr)
	{
		return false;
	}
	FObjectivesCoreDeps Deps;
	Deps.Roster = Init.Roster;
	Deps.Session = Init.Session;
	Deps.PawnControl = Init.PawnControl;
	Deps.Round = Init.Round;
	Deps.Courts = Init.Courts;
	Deps.Views = this; // the service is the core's view sink and the components' only writer
	Deps.RivalRelease = Init.RivalRelease;
	Deps.Faults = Init.Faults;
	Core->Initialize(Deps);
	Core->SetTuning(Init.PublicTuning, Init.ServerTuning);
	Roster = Init.Roster;
	Assignment = Init.Assignment;
	Chains = Init.Chains;
	ViewTargets = Init.ViewTargets;
	ViewObserver = Init.Views;
	Queue = Init.Queue;
	Faults = Init.Faults;
#if !UE_BUILD_SHIPPING
	SubmitDebugStage = Init.SubmitDebugStage;
#endif
	// Story oc-012 (ADR-0005: consumers needing world wiring subscribe to Roster events): the rebind write and the reset clear.
	if (URosterStore* Store = Init.RosterEvents)
	{
		RosterEvents = Store;
		RosterEventHandle = Store->OnRosterEvent.AddUObject(this, &UObjectivesService::HandleRosterEvent);
		ViewReboundHandle = Store->OnParticipantViewRebound.AddUObject(this, &UObjectivesService::HandleViewRebound_Event);
	}
	if (Queue != nullptr)
	{
		bStageSignalRegistered = Queue->RegisterTimerResolver(ETimerKind::StageSignal, this);
	}
	bBound = true;
	return true;
}

void UObjectivesService::Unsubscribe()
{
	if (URosterStore* Store = RosterEvents.Get())
	{
		Store->OnRosterEvent.Remove(RosterEventHandle);
		Store->OnParticipantViewRebound.Remove(ViewReboundHandle);
	}
	RosterEvents.Reset();
	RosterEventHandle.Reset();
	ViewReboundHandle.Reset();
}

void UObjectivesService::Unbind()
{
	Unsubscribe();
	if (Queue != nullptr && bStageSignalRegistered)
	{
		Queue->UnregisterTimerResolver(ETimerKind::StageSignal);
	}
	bStageSignalRegistered = false;
	Queue = nullptr;
	Core->Reset();
	Core->Initialize(FObjectivesCoreDeps());
	Roster = nullptr;
	Assignment = nullptr;
	Chains = nullptr;
	ViewTargets = nullptr;
	ViewObserver = nullptr;
	Faults = nullptr;
#if !UE_BUILD_SHIPPING
	SubmitDebugStage = nullptr;
#endif
	SelectedChain = nullptr;
	bChainSelected = false;
	LayoutId = 0;
	ActiveEpoch = 0;
	bBound = false;
}

void UObjectivesService::SetTuning(const FObjectivesTuning_Public& PublicTuning, const FObjectivesTuning_Server& ServerTuning)
{
	Core->SetTuning(PublicTuning, ServerTuning);
}

void UObjectivesService::ReportFault(int64 Detail) const
{
	if (Faults != nullptr)
	{
		Faults->Report(EStealthFault::InvariantViolation, ObjectivesServiceLocal::FaultSource, Detail);
	}
}

// --- Lifecycle (story oc-005) -----------------------------------------------------------------------------------------

FObjectivesSelection UObjectivesService::ValidateSelectedChain(const FStealthServerTuning& Tuning)
{
	FObjectivesSelection Out;
	Out.Result = EChainValidation::ObjectiveChainInvalid;
	Out.Clause = EChainValidationClause::NoChain;
	SelectedChain = nullptr;
	bChainSelected = false;
	Core->SetTuning(Tuning.Objectives_Public, Tuning.Objectives_Server);

	TArray<const UObjectiveChainData*> Cooked;
	if (Chains != nullptr)
	{
		Chains->GetCookedChains(Cooked);
	}
	Out.CookedChainCount = Cooked.Num();
	if (Cooked.Num() > 1)
	{
		Out.Clause = EChainValidationClause::AmbiguousChain; // MVP: exactly one chain; never a silent pick
		return Out;
	}
	const FChainValidationResult Valid = FObjectivesCore::ValidateChain(Cooked.Num() == 1 ? Cooked[0] : nullptr, Tuning.Objectives_Server);
	Out.Result = Valid.Result;
	Out.Clause = Valid.Clause;
	Out.Row = Valid.Row;
	Out.bValid = Valid.IsValid();
	if (Out.bValid)
	{
		SelectedChain = Cooked[0];
		bChainSelected = true;
	}
	return Out;
}

bool UObjectivesService::HandleLayoutReady(uint64 InLayoutId, int32 Seed)
{
	if (!bBound || !bChainSelected || SelectedChain == nullptr || Assignment == nullptr)
	{
		return false; // nothing selected (Preparing failed): no site is bound and the assignment is never read
	}
	const FChainValidationResult Result = Core->Bind(SelectedChain.Get(), *Assignment);
	if (!Result.IsValid())
	{
		ReportFault(static_cast<int64>(ObjectivesServiceLocal::EFaultDetail::BindFailed));
		return false;
	}
	LayoutId = InLayoutId;
	return true;
}

bool UObjectivesService::HandleRoundActive(FEpochToken Epoch, double ActivePhaseStart)
{
	if (!bBound)
	{
		return false;
	}
	if (!Core->IsBound())
	{
		ReportFault(static_cast<int64>(ObjectivesServiceLocal::EFaultDetail::ActiveBeforeLayout));
		return false;
	}
	if (ActiveEpoch == Epoch.RoundEpoch && Core->IsActive())
	{
		return false; // a second Active entry for the same epoch is ignored
	}
	if (!Core->HandleActiveEntry(Epoch.RoundEpoch, ActivePhaseStart))
	{
		return false;
	}
	ActiveEpoch = Epoch.RoundEpoch;
	return true;
}

void UObjectivesService::HandleRoundEnded()
{
	Core->HandleRoundEnded();
}

void UObjectivesService::HandleRoundReset()
{
	Core->Reset();
	SelectedChain = nullptr;
	bChainSelected = false;
	LayoutId = 0;
	ActiveEpoch = 0;
}

// --- The team view (stories oc-010..oc-012) ---------------------------------------------------------------------------

void UObjectivesService::PushTeamView(uint8 Team, const FObjectiveTeamView& View)
{
	WriteTeamView(Team, View);
	if (ViewObserver != nullptr)
	{
		ViewObserver->PushTeamView(Team, View);
	}
}

int32 UObjectivesService::WriteTeamView(uint8 Team, const FObjectiveTeamView& View)
{
	if (ViewTargets == nullptr)
	{
		return 0;
	}
	TArray<FParticipantId> Members;
	ViewTargets->GetTeamMembers(Team, Members);
	int32 Written = 0;
	for (const FParticipantId Member : Members)
	{
		UObjectiveTeamViewComponent* Component = ViewTargets->FindTeamView(Member);
		Written += Component != nullptr && Component->ApplyView(View) ? 1 : 0;
	}
	ViewWriteCount += Written;
	return Written;
}

FObjectiveTeamView UObjectivesService::BuildTeamView(int32 Team) const
{
	return Core->BuildTeamView(Team);
}

bool UObjectivesService::HandleViewRebound(FParticipantId Participant)
{
	if (!bBound || ViewTargets == nullptr || Roster == nullptr || !Participant.IsValid())
	{
		return false;
	}
	const int32 Team = Roster->GetTeam(Participant);
	UObjectiveTeamViewComponent* Component = Team >= 0 && Team < FObjectivesCore::TeamCount ? ViewTargets->FindTeamView(Participant) : nullptr;
	if (Component == nullptr)
	{
		return false;
	}
	// Inline in the handler: the team's current view (the empty default before Active entry, so no client knows a site early;
	// at seamless travel the new world's service is not Active and the new controller's default is already correct).
	const bool bWritten = Component->ApplyView(Core->BuildTeamView(Team));
	ViewWriteCount += bWritten ? 1 : 0;
	return bWritten;
}

void UObjectivesService::HandleViewRebound_Event(FParticipantId Participant)
{
	HandleViewRebound(Participant);
}

void UObjectivesService::HandleRosterReset()
{
	HandleRoundReset();
	// Nothing carries across rounds: every member's component returns to the empty default (change-only).
	for (uint8 Team = 0; Team < FObjectivesCore::TeamCount; ++Team)
	{
		WriteTeamView(Team, FObjectiveTeamView());
	}
}

void UObjectivesService::HandleRosterEvent(const FRosterEvent& Event)
{
	if (Event.Type == ERosterEventType::RosterReset)
	{
		HandleRosterReset();
	}
}

#if !UE_BUILD_SHIPPING
bool UObjectivesService::DebugCompleteStage(uint8 Team)
{
	return bBound && SubmitDebugStage && SubmitDebugStage(Team);
}

bool UObjectivesService::ApplyDebugStage(uint8 Team, FQueueKey Key)
{
	return bBound && Core->ApplyDebugStageCompletion(Team, Key);
}

int32 UObjectivesService::CheckClaimConsistency(const TMap<FCourtFixtureId, int32>& InteractingSlots)
{
	return Core->GetClaimsMutable().CheckConsistency(InteractingSlots, Faults);
}
#endif

// --- IInteractTargetOwner (story oc-006) ------------------------------------------------------------------------------

double UObjectivesService::GetInteractHoldSec() const
{
	return Core->GetInteractHoldSec();
}

void UObjectivesService::OnInteractExecuted(const AStealthPawn& Pawn, FCourtFixtureId Court, FQueueKey Key)
{
	Core->OnInteractExecuted(Pawn, Court, Key);
}

void UObjectivesService::OnInteractEnded(const AStealthPawn& Pawn, FCourtFixtureId Court, EInteractEnd End, FQueueKey Key)
{
	LastCompletion = Core->OnInteractEnded(Pawn, Court, End == EInteractEnd::Completed ? EObjectivesInteractEnd::Completed : EObjectivesInteractEnd::Cancelled, Key);
}

void UObjectivesService::OnInteractCompleteLatched(const AStealthPawn& Pawn, FCourtFixtureId Court, FQueueKey Key)
{
	Core->OnInteractCompleteLatched(Pawn, Court, Key);
}

// --- IQueueResolver (TimerFired(StageSignal)) ---------------------------------------------------------------------------

EResolveResult UObjectivesService::Resolve(const FQueueItem& Item, FQueueKey Key)
{
	// Stories oc-014/015 schedule and resolve the rival releases; nothing is scheduled before them, so nothing is published.
	return EResolveResult::Voided;
}

int32 UObjectivesService::GetCompletedPreliminaryStages(int32 Team) const
{
	return Core->GetCompletedPreliminaryStages(Team);
}

EObjectiveTeamStage UObjectivesService::GetObjectiveTeamStage(int32 Team) const
{
	return Core->GetStageIndex(Team);
}

#if !UE_BUILD_SHIPPING
FCourtFixtureId UObjectivesService::GetLiveCourtForDev(int32 Team) const
{
	TArray<int32> Live;
	Core->GetLiveDefinitions(Team, Live);
	const FChainBinding* Binding = Live.Num() > 0 ? Core->GetBinding(Live[0]) : nullptr;
	return Binding != nullptr ? Binding->Court : FCourtFixtureId();
}
#endif
