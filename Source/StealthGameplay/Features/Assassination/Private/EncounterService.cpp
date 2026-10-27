// Stories asn-005 to asn-011 (production/epics/assassination-defence-reversal/): UEncounterService, the thin facade over
// FAttackResolverCore, and its production seam adapters. See Server/EncounterService.h.

#include "Server/EncounterService.h"

#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "Server/Core/StealthPawnFactory.h"
#include "View/DefenceOwnerComponent.h"
#include "Server/ActionHub.h"
#include "Server/BushSpotQuery.h"
#include "Server/Core/StealthPawnCommandComponent.h"
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

void FEncounterComponentDefenceStore::ResetAll()
{
	if (UWorld* Target = World.Get())
	{
		for (TActorIterator<APawn> It(Target); It; ++It)
		{
			if (UDefenceStateComponent* Component = UDefenceStateComponent::Find(**It))
			{
				Component->GetCore().Reset();
			}
		}
	}
}

void FEncounterHubReplySink::AppendReply(FReplySinkId Sink, FQueueKey SlotKey, const FCommandReply& Reply)
{
	if (UActionHub* Target = Hub.Get())
	{
		Target->GetReplySinks().Append(Sink, SlotKey, Reply);
	}
}

void FEncounterOwnerCooldownPublisher::PublishCooldownEnd(APawn* Pawn, double CooldownEndServerTime)
{
	if (UDefenceOwnerComponent* Owner = Pawn != nullptr ? UDefenceOwnerComponent::Find(Pawn->GetController()) : nullptr)
	{
		Owner->SetCooldownEndServerTime(CooldownEndServerTime);
	}
}

void FEncounterOwnerCooldownPublisher::ResetAll()
{
	if (UWorld* Target = World.Get())
	{
		for (TActorIterator<AController> It(Target); It; ++It)
		{
			if (UDefenceOwnerComponent* Owner = UDefenceOwnerComponent::Find(*It))
			{
				Owner->ClearCooldownEnd();
			}
		}
	}
}

void FEncounterPawnFactoryHandOver::ReleaseBody(APawn* Body)
{
	if (Body == nullptr || !IsValid(Body))
	{
		return;
	}
	// (a) After the fall the body blocks no pawn (ADR-0018 section 6.2): the capsule ignores the pawn channel (the server
	// keeps its world collision so the body still lies on the floor).
	if (UCapsuleComponent* Capsule = Body->FindComponentByClass<UCapsuleComponent>())
	{
		Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	}
	// (b) The factory's ReleaseBody (ADR-0010; the only release path): crowd controller destroyed, PlayerController left
	// pawnless and playing.
	AController* Former = Body->GetController();
	if (UStealthPawnFactory* Factory = UStealthPawnFactory::Get(Body->GetWorld()))
	{
		Factory->ReleaseBody(Body);
	}
	else if (Former != nullptr)
	{
		Former->UnPossess();
		if (!Former->IsPlayerController())
		{
			Former->Destroy();
		}
	}
	OnReleased.Broadcast(Body, Former);
}

void FEncounterPawnFactoryHandOver::RequestTeardown(APawn* Body)
{
	if (Body == nullptr || !IsValid(Body))
	{
		return;
	}
	if (UStealthPawnFactory* Factory = UStealthPawnFactory::Get(Body->GetWorld()))
	{
		Factory->RequestTeardown(Body);
		return;
	}
	if (AController* Controller = Body->GetController())
	{
		Controller->UnPossess(); // never destroy a possessed pawn
		if (!Controller->IsPlayerController())
		{
			Controller->Destroy();
		}
	}
	Body->Destroy();
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

void FEncounterPawnStateWriter::WriteActionState(APawn* Pawn, const FActionStateWrite& Write)
{
	const AStealthPawn* Body = Cast<AStealthPawn>(Pawn);
	if (UPawnCommandComponent* Command = Body != nullptr ? Body->GetPawnCommand() : nullptr)
	{
		Command->WriteActionState(Write);
	}
}

EActionPose FEncounterPawnStateWriter::GetPose(const APawn* Pawn) const
{
	const AStealthPawn* Body = Cast<AStealthPawn>(Pawn);
	return Body != nullptr ? Body->GetActionState().Pose : EActionPose::None;
}

bool FEncounterPawnStateWriter::WriteDeath(APawn* Pawn, const FDeathState& State)
{
	AStealthPawn* Body = Cast<AStealthPawn>(Pawn);
	return Body != nullptr && IsValid(Body) && Body->ApplyDeathWrite(State);
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
	DefenceStore = MakeUnique<FEncounterComponentDefenceStore>(Init.World);
	PawnQuery = MakeUnique<FEncounterWorldPawnQuery>(Init.World);
	RosterGateway = MakeUnique<FEncounterRosterStoreGateway>(Init.Roster != nullptr ? Init.Roster : URosterStore::Get(Init.World));
	HubEvents = MakeUnique<FEncounterHubEvents>(Init.Hub != nullptr ? Init.Hub : UActionHub::Get(Init.World));
	PawnStateWriter = MakeUnique<FEncounterPawnStateWriter>();
	DefaultVisibility = MakeUnique<FEncounterConcealmentVisibility>(Init.Bushes);
	ReplySink = MakeUnique<FEncounterHubReplySink>(Init.Hub != nullptr ? Init.Hub : UActionHub::Get(Init.World));
	OwnerCooldown = MakeUnique<FEncounterOwnerCooldownPublisher>(Init.World);
	HandOver = MakeUnique<FEncounterPawnFactoryHandOver>();

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
	Deps.PawnState = PawnStateWriter.Get(); // FActionState carries the stance (the SCL pose follows it)
	Deps.Cards = Init.Cards;
	Deps.Log = Init.Log;
	Deps.Tally = Init.Tally;
	Deps.Replies = ReplySink.Get();
	Deps.OwnerCooldown = OwnerCooldown.Get();
	Deps.BodyHandOver = HandOver.Get();
	Deps.BodyObserver = Init.BodyObserver;
	Deps.Faults = Init.Faults;
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
	HandOver.Reset();
	OwnerCooldown.Reset();
	ReplySink.Reset();
	DefaultVisibility.Reset();
	PawnStateWriter.Reset();
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
	bRegisteredDeathWrite = Queue.RegisterStagedWriteApplier(EStagedWriteType::DeathWrite, Core->GetStagedWriteApplier());
	bRegisteredCounterPose = Queue.RegisterStagedWriteApplier(EStagedWriteType::CounterPose, Core->GetStagedWriteApplier());
	bRegisteredBodyRelease = Queue.RegisterTimerResolver(ETimerKind::BodyRelease, Core.Get());
	bRegisteredBodyRemoval = Queue.RegisterTimerResolver(ETimerKind::BodyRemoval, Core.Get());
	if (bRegisteredCommands || bRegisteredHolds || bRegisteredDeathWrite || bRegisteredCounterPose || bRegisteredBodyRelease || bRegisteredBodyRemoval)
	{
		RegisteredQueue = &Queue;
	}
	return bRegisteredCommands && bRegisteredHolds && bRegisteredDeathWrite && bRegisteredCounterPose && bRegisteredBodyRelease && bRegisteredBodyRemoval;
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
	if (bRegisteredDeathWrite)
	{
		Queue.UnregisterStagedWriteApplier(EStagedWriteType::DeathWrite);
	}
	if (bRegisteredCounterPose)
	{
		Queue.UnregisterStagedWriteApplier(EStagedWriteType::CounterPose);
	}
	if (bRegisteredBodyRelease)
	{
		Queue.UnregisterTimerResolver(ETimerKind::BodyRelease);
	}
	if (bRegisteredBodyRemoval)
	{
		Queue.UnregisterTimerResolver(ETimerKind::BodyRemoval);
	}
	RegisteredQueue = nullptr;
	bRegisteredCommands = false;
	bRegisteredHolds = false;
	bRegisteredDeathWrite = false;
	bRegisteredCounterPose = false;
	bRegisteredBodyRelease = false;
	bRegisteredBodyRemoval = false;
}

void UEncounterService::Release()
{
	// The queue is gone: forget the registration without touching it, then release everything that pointed into it.
	RegisteredQueue = nullptr;
	bRegisteredCommands = false;
	bRegisteredHolds = false;
	bRegisteredDeathWrite = false;
	bRegisteredCounterPose = false;
	bRegisteredBodyRelease = false;
	bRegisteredBodyRemoval = false;
	Shutdown();
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
