// Stories asn-005 to asn-011 (production/epics/assassination-defence-reversal/): UEncounterService, the thin facade over
// FAttackResolverCore, and its production seam adapters. See Server/EncounterService.h.

#include "Server/EncounterService.h"

#include "EngineUtils.h"
#include "Engine/World.h"
#include "Server/ActionHub.h"
#include "Server/BushSpotQuery.h"
#include "Server/DefenceStateComponent.h"
#include "Server/Queue/StealthResolutionQueueCore.h"
#include "Server/Roster/StealthRosterStore.h"
#include "View/Core/StealthLocomotionModel.h"
#include "View/Core/StealthPawn.h"
#include "View/Core/StealthPawnMovementComponent.h"

// ---------------------------------------------------------------------------------------------------------
// Adapters
// ---------------------------------------------------------------------------------------------------------

void FEncounterQueueAdapter::ScheduleAt(double DueServerTime, const FQueueItem& Item)
{
	if (Queue != nullptr)
	{
		Queue->ScheduleAt(DueServerTime, Item);
	}
}

void FEncounterQueueAdapter::SetVoidCode(uint32 OwnerCode)
{
	if (Queue != nullptr && Queue->IsInSlot())
	{
		Queue->SetVoidCode(OwnerCode);
	}
}

bool FEncounterQueueAdapter::StageWrite(const FStagedWrite& Write)
{
	return Queue != nullptr && Queue->StageWrite(Write);
}

FDefenceStateCore* FEncounterComponentDefenceStore::Find(const APawn* Pawn)
{
	UDefenceStateComponent* Component = Pawn != nullptr ? UDefenceStateComponent::Find(*Pawn) : nullptr;
	return Component != nullptr ? &Component->GetCore() : nullptr;
}

FDefenceStateCore* FEncounterComponentDefenceStore::FindOrAdd(const APawn* Pawn)
{
	UDefenceStateComponent* Component = Pawn != nullptr ? UDefenceStateComponent::FindOrAdd(*const_cast<APawn*>(Pawn)) : nullptr;
	return Component != nullptr ? &Component->GetCore() : nullptr;
}

bool FEncounterWorldPawnQuery::MakeFacts(const APawn* Pawn, FEncounterPawnFacts& OutFacts)
{
	const AStealthPawn* Body = Cast<AStealthPawn>(Pawn);
	if (Body == nullptr || !IsValid(Body))
	{
		return false;
	}
	OutFacts = FEncounterPawnFacts();
	OutFacts.Pawn = const_cast<AStealthPawn*>(Body);
	OutFacts.Location = Body->GetActorLocation();
	OutFacts.FacingYawDeg = Body->GetActorRotation().Yaw;
	OutFacts.Velocity = Body->GetVelocity();
	OutFacts.SpawnSerial = static_cast<uint64>(Body->GetUniqueID());
	if (const UStealthMovementComponent* Movement = Body->GetStealthMovement())
	{
		const FStealthMotionState& Motion = Movement->GetMotionState();
		OutFacts.BrakingCmPerSec2 = StealthLocomotion::StopDeceleration(Movement->GetLocomotionTuning(), static_cast<float>(OutFacts.Velocity.Size2D()), Motion.LastGait);
		OutFacts.bRunning = Movement->IsRunning();
		OutFacts.bTraversing = Movement->IsInTraversal();
	}
	return true;
}

void FEncounterWorldPawnQuery::GetPawns(TArray<FEncounterPawnFacts>& OutPawns) const
{
	OutPawns.Reset();
	if (UWorld* Target = World.Get())
	{
		for (TActorIterator<AStealthPawn> It(Target); It; ++It)
		{
			FEncounterPawnFacts Facts;
			if (MakeFacts(*It, Facts))
			{
				OutPawns.Add(Facts);
			}
		}
	}
}

bool FEncounterWorldPawnQuery::GetFacts(const APawn* Pawn, FEncounterPawnFacts& OutFacts) const
{
	return Pawn != nullptr && Pawn->GetWorld() == World.Get() && MakeFacts(Pawn, OutFacts);
}

FRosterVictimKind FEncounterRosterStoreGateway::ResolveVictimKind(const APawn* Pawn)
{
	URosterStore* Roster = Store.Get();
	if (Roster == nullptr || !Roster->IsMatchOpen())
	{
		return FRosterVictimKind::Make(ERosterVictimKind::Npc);
	}
	return Roster->ResolveVictimKind(const_cast<APawn*>(Pawn));
}

bool FEncounterRosterStoreGateway::AreTeammates(const APawn* A, const APawn* B)
{
	URosterStore* Roster = Store.Get();
	if (Roster == nullptr || !Roster->IsMatchOpen())
	{
		return false;
	}
	const FParticipantRecord* RecordA = Roster->FindRecord(Roster->GetParticipant(const_cast<APawn*>(A)));
	const FParticipantRecord* RecordB = Roster->FindRecord(Roster->GetParticipant(const_cast<APawn*>(B)));
	return RecordA != nullptr && RecordB != nullptr && RecordA->Team != ERosterTeam::None && RecordA->Team == RecordB->Team;
}

bool FEncounterRosterStoreGateway::IsLegalityActive(const APawn* Pawn)
{
	URosterStore* Roster = Store.Get();
	if (Roster == nullptr || !Roster->IsMatchOpen())
	{
		return true;
	}
	return Roster->GetLegalityClass(const_cast<APawn*>(Pawn)) == EParticipantState::Active;
}

void FEncounterRosterStoreGateway::RequestElimination(const APawn* Pawn, ERosterReason Reason, double RetireAtServerTime)
{
	URosterStore* Roster = Store.Get();
	if (Roster == nullptr || !Roster->IsMatchOpen())
	{
		return;
	}
	const FParticipantId Participant = Roster->GetParticipant(const_cast<APawn*>(Pawn));
	if (!Participant.IsValid())
	{
		return;
	}
	FRosterTransitionOptions Options;
	Options.ExpectedEpoch = Roster->GetEpochToken();
	Options.RetireAtServerTimeMs = static_cast<int64>(FMath::RoundToDouble(RetireAtServerTime * 1000.0));
	Roster->RequestTransition(Participant, EParticipantState::Informant, Reason, Options);
}

FParticipantId FEncounterRosterStoreGateway::GetParticipant(const APawn* Pawn)
{
	URosterStore* Roster = Store.Get();
	return Roster != nullptr && Roster->IsMatchOpen() ? Roster->GetParticipant(const_cast<APawn*>(Pawn)) : FParticipantId();
}

uint32 FEncounterRosterStoreGateway::GetTeamIndex(const APawn* Pawn)
{
	URosterStore* Roster = Store.Get();
	const FParticipantRecord* Record = Roster != nullptr && Roster->IsMatchOpen() ? Roster->FindRecord(Roster->GetParticipant(const_cast<APawn*>(Pawn))) : nullptr;
	return Record != nullptr && Record->Team != ERosterTeam::None ? static_cast<uint32>(Record->Team) : 0xFFu;
}

void FEncounterHubEvents::OnActionStarted(APawn* Pawn, EActionType Type)
{
	UActionHub* Target = Hub.Get();
	if (AStealthPawn* Body = Cast<AStealthPawn>(Pawn); Target != nullptr && Body != nullptr)
	{
		Target->OnActionStarted.Broadcast(Body, Type);
	}
}

void FEncounterHubEvents::OnActionCancelled(APawn* Pawn, EActionType Type, ECancelReason Reason)
{
	UActionHub* Target = Hub.Get();
	if (AStealthPawn* Body = Cast<AStealthPawn>(Pawn); Target != nullptr && Body != nullptr)
	{
		Target->OnActionCancelled.Broadcast(Body, Type, Reason);
	}
}

void FEncounterHubEvents::OnActionCompleted(APawn* Pawn, EActionType Type)
{
	UActionHub* Target = Hub.Get();
	if (AStealthPawn* Body = Cast<AStealthPawn>(Pawn); Target != nullptr && Body != nullptr)
	{
		Target->OnActionCompleted.Broadcast(Body, Type);
	}
}

void FEncounterSclStanceSink::SetBraceStance(APawn* Pawn, bool bBraced)
{
	const AStealthPawn* Body = Cast<AStealthPawn>(Pawn);
	UStealthMovementComponent* Movement = Body != nullptr ? Body->GetStealthMovement() : nullptr;
	if (Movement == nullptr)
	{
		return;
	}
	if (bBraced)
	{
		Movement->SetActionPose(EStealthActionPose::Brace);
	}
	else if (Movement->GetActionPose() == EStealthActionPose::Brace)
	{
		Movement->SetActionPose(EStealthActionPose::None); // never clears a pose another writer set
	}
}

bool FEncounterConcealmentVisibility::CanViewerKnow(const APawn* Viewer, const APawn* Pawn)
{
	return Pawn != nullptr && (Bushes == nullptr || !Bushes->IsConcealed(Pawn));
}

// ---------------------------------------------------------------------------------------------------------
// UEncounterService
// ---------------------------------------------------------------------------------------------------------

bool UEncounterService::Initialize(const FEncounterServiceInit& Init)
{
	Shutdown();
	if (Init.World == nullptr || Init.Clock == nullptr || Init.Queue == nullptr)
	{
		return false;
	}
	QueueAdapter = MakeUnique<FEncounterQueueAdapter>(Init.Queue);
	DefenceStore = MakeUnique<FEncounterComponentDefenceStore>();
	PawnQuery = MakeUnique<FEncounterWorldPawnQuery>(Init.World);
	RosterGateway = MakeUnique<FEncounterRosterStoreGateway>(Init.Roster != nullptr ? Init.Roster : URosterStore::Get(Init.World));
	HubEvents = MakeUnique<FEncounterHubEvents>(Init.Hub != nullptr ? Init.Hub : UActionHub::Get(Init.World));
	StanceSink = MakeUnique<FEncounterSclStanceSink>();
	DefaultVisibility = MakeUnique<FEncounterConcealmentVisibility>(Init.Bushes);

	FAttackResolverDeps Deps;
	Deps.Clock = Init.Clock;
	Deps.Queue = QueueAdapter.Get();
	Deps.Roster = RosterGateway.Get();
	Deps.Pawns = PawnQuery.Get();
	Deps.Defence = DefenceStore.Get();
	Deps.SubSeeds = Init.SubSeeds;
	Deps.Visibility = Init.Visibility != nullptr ? Init.Visibility : DefaultVisibility.Get();
	Deps.Bushes = Init.Bushes;
	Deps.Events = HubEvents.Get();
	Deps.Stance = StanceSink.Get();
	Deps.Cards = Init.Cards;
	Deps.Log = Init.Log;
	Core = MakeUnique<FAttackResolverCore>(Deps, Init.Params);
	return true;
}

void UEncounterService::Shutdown()
{
	if (RegisteredQueue != nullptr)
	{
		UnregisterFrom(*RegisteredQueue);
	}
	Core.Reset();
	DefaultVisibility.Reset();
	StanceSink.Reset();
	HubEvents.Reset();
	RosterGateway.Reset();
	PawnQuery.Reset();
	DefenceStore.Reset();
	QueueAdapter.Reset();
}

bool UEncounterService::RegisterOn(FResolutionQueueCore& Queue)
{
	if (!Core.IsValid() || RegisteredQueue != nullptr)
	{
		return false;
	}
	bRegisteredCommands = Queue.RegisterResolver(EQueueItemType::PawnCommand, Core.Get());
	bRegisteredHolds = Queue.RegisterResolver(EQueueItemType::HoldTimer, Core.Get());
	if (bRegisteredCommands || bRegisteredHolds)
	{
		RegisteredQueue = &Queue;
	}
	return bRegisteredCommands && bRegisteredHolds;
}

void UEncounterService::UnregisterFrom(FResolutionQueueCore& Queue)
{
	if (RegisteredQueue != &Queue)
	{
		return; // never drops another owner's registration
	}
	if (bRegisteredCommands)
	{
		Queue.UnregisterResolver(EQueueItemType::PawnCommand);
	}
	if (bRegisteredHolds)
	{
		Queue.UnregisterResolver(EQueueItemType::HoldTimer);
	}
	RegisteredQueue = nullptr;
	bRegisteredCommands = false;
	bRegisteredHolds = false;
}

void UEncounterService::SetFallbackResolvers(IQueueResolver* Commands, IQueueResolver* Holds)
{
	if (Core.IsValid())
	{
		Core->SetFallbackResolvers(Commands, Holds);
	}
}

void UEncounterService::BeginDestroy()
{
	Shutdown();
	Super::BeginDestroy();
}

void UEncounterService::ResetRound()
{
	if (Core.IsValid())
	{
		Core->Reset();
	}
}
