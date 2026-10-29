#include "Server/StealthCompositionRoot.h"

#include "Server/ActionHub.h"
#include "Server/AssassinationAttackPredicate.h"
#include "Server/AssassinationPreparePredicate.h"
#include "Server/ConcealmentService.h"
#include "Server/Visibility/StealthConcealmentCull.h"
#include "Server/StealthTuningInvariants.h"
#include "StealthConcealmentWiring.h"
#include "StealthBushGatedPredicate.h"
#include "Server/EncounterService.h"
#include "Server/StealthPenaltyDueAdapter.h"
#include "Server/StealthPenaltyService.h"
#include "Server/StealthTuningSubstructs.h"
#include "Server/Core/InteractHoldStandIn.h"
#include "Server/ObjectivesService.h"
#include "Server/StealthServerTuning.h"
#include "StealthCommandWiring.h"
#include "View/StealthCourtSocket.h"
#include "StealthObjectivesWiring.h"
#include "StealthPenaltyWiring.h"
#include "View/ObjectivesChainRegistry.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Hash/CityHash.h"
#include "Misc/CommandLine.h"
#include "Misc/Guid.h"
#include "Server/Core/Round/StealthRoundMatchStore.h"
#include "Server/CrowdDirector.h"
#include "Server/StealthCrowdController.h"
#include "Server/Core/Round/StealthRoundRosterGateway.h"
#include "Server/Roster/StealthRosterStore.h"
#include "Server/Session/StealthSessionService.h"
#include "StealthCharacter.h"
#include "StealthGame.h"
#include "StealthGameMode.h"
#include "StealthGameState.h"
#include "StealthNpcController.h"
#include "StealthPlayerState.h"
#include "StealthSettings.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "View/StealthRosterTeamView.h"
#include "View/StealthTuningPublicSubstructs.h"

// The D53 integration composition root (ADR-0007 section 4): production stores drive L_Prototype; prototype gameplay
// keeps running through the thin adapters below.

namespace StealthCompositionLocal
{
	/** The MVP identity key (unauthenticated, Session rule 6): a 128-bit hash of the player's unique net id. */
	FIdentityKey MakeIdentityKey(const APlayerController* Player)
	{
		FString Source;
		if (const APlayerState* State = Player->GetPlayerState<APlayerState>())
		{
			const FUniqueNetIdPtr NetId = State->GetUniqueId().IsValid() ? State->GetUniqueId().GetUniqueNetId() : nullptr;
			if (NetId.IsValid())
			{
				Source = NetId->ToString();
			}
		}
		if (Source.IsEmpty())
		{
			// No online identity (some PIE setups): a fresh key per login, so a reconnect is a new participant.
			Source = FGuid::NewGuid().ToString();
		}
		const FTCHARToUTF8 Utf8(*Source);
		const uint64 Hi = CityHash64(reinterpret_cast<const char*>(Utf8.Get()), Utf8.Length());
		const uint64 Lo = CityHash64WithSeed(reinterpret_cast<const char*>(Utf8.Get()), Utf8.Length(), 0x5EA1C0DE5EA1C0DEull);
		return FIdentityKey(Hi, Lo);
	}
}

// -------------------------------------------------------------------------------------------------------------------
// UStealthMatchAdapters
// -------------------------------------------------------------------------------------------------------------------

USessionService* UStealthMatchAdapters::GetSession() const
{
	const UGameInstance* GameInstance = Cast<UGameInstance>(GetOuter()->GetOuter());
	return GameInstance != nullptr ? GameInstance->GetSubsystem<USessionService>() : nullptr;
}

UWorld* UStealthMatchAdapters::GetGameWorld() const
{
	const UGameInstance* GameInstance = Cast<UGameInstance>(GetOuter()->GetOuter());
	return GameInstance != nullptr ? GameInstance->GetWorld() : nullptr;
}

bool UStealthMatchAdapters::CanRequestRoundTravel() const
{
	return GetGameWorld() != nullptr;
}

void UStealthMatchAdapters::RequestLobbyTravel()
{
	UWorld* World = GetGameWorld();
	if (World == nullptr)
	{
		return;
	}
	TWeakObjectPtr<UWorld> WeakWorld(World);
	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [WeakWorld]()
	{
		if (URoundMatchStore* Round = URoundMatchStore::Get(WeakWorld.Get()); Round != nullptr && Round->GetMatchCore() != nullptr)
		{
			// Session's OnLobbyTravelComplete stand-in: every participant is terminal (R10 (7)).
			Round->GetMatchCore()->NotifyLobbyTravelComplete();
		}
	}));
}

void UStealthMatchAdapters::GetLivePawns(TArray<FRoundLivePawn>& OutPawns) const
{
	OutPawns.Reset();
	const UWorld* World = GetGameWorld();
	if (World == nullptr)
	{
		return;
	}
	for (TActorIterator<AStealthCharacter> It(World); It; ++It)
	{
		AStealthCharacter* Character = *It;
		if (Character == nullptr || Character->IsDying())
		{
			continue;
		}
		FRoundLivePawn& Live = OutPawns.AddDefaulted_GetRef();
		Live.Pawn.Pawn = Character;
		Live.PawnId = Character->GetUniqueID();
		const AController* Controller = Character->GetController();
		const bool bPlayerOwned = Character->GetStealthPlayerState() != nullptr;
		Live.Driver = Controller != nullptr && Controller->IsPlayerController() ? ERoundPawnDriver::HumanDriven
			: (bPlayerOwned ? ERoundPawnDriver::AiDriven : ERoundPawnDriver::Npc);
	}
}

void UStealthMatchAdapters::Halt()
{
	if (UWorld* World = GetGameWorld())
	{
		// The production crowd (ADR-0016 section 7): the Director latches every sink, clears Move and stops the trees.
		if (UCrowdDirector* Director = UCrowdDirector::Get(World))
		{
			Director->Halt();
		}
	}
}

void UStealthMatchAdapters::RequestPawnStop(const FRoundLivePawn& Pawn, double BoundSec)
{
	AStealthCharacter* Character = Cast<AStealthCharacter>(Pawn.Pawn.Pawn.Get());
	if (Character == nullptr)
	{
		return;
	}
	if (AController* Controller = Character->GetController())
	{
		Controller->StopMovement();
	}
	// Held in place for the rest of Ended (the prototype has no stop-and-blend path yet; the round resets the pawns).
	Character->HoldInPlace(FMath::Max(static_cast<float>(BoundSec), 1.f) * 60.f);
	// The crowd's part of the round end already ran in Halt (every crowd sink latched, held Move cleared).
}

FPawnHandle UStealthMatchAdapters::GetBoundPawn(FParticipantId Participant) const
{
	FPawnHandle Handle;
	const UStealthCompositionRoot* Root = UStealthCompositionRoot::Get(GetGameWorld());
	if (const APlayerController* Player = Root != nullptr ? Root->GetController(Participant) : nullptr)
	{
		Handle.Pawn = Player->GetPawn();
	}
	return Handle;
}

int32 UStealthMatchAdapters::GetCompletedPreliminaryStages(int32 Team) const
{
	// Objectives story oc-009: the production store (UObjectivesService) replaces the prototype TeamStage counter.
	const UObjectivesService* Objectives = UObjectivesService::Get(GetGameWorld());
	return Objectives != nullptr && (Team == 0 || Team == 1) ? FMath::Clamp(Objectives->GetCompletedPreliminaryStages(Team), 0, 2) : 0;
}

ERoundTravelRequestResult UStealthMatchAdapters::RequestRoundTravel(FName MapId, FName PresetId, uint32 RoundIndex)
{
	UWorld* World = GetGameWorld();
	USessionService* Session = GetSession();
	if (World == nullptr || Session == nullptr || !Session->IsSessionOpen())
	{
		return ERoundTravelRequestResult::Refused;
	}
	// Session draws a fresh seed for every start (R5 step 8; F5 with UsedSeeds, st-010). No map travel yet: the same
	// world hosts every round, so the travel is confirmed on the next tick (the deferred acceptance of R5 step 9).
	LastRoundSeed = Session->DrawRoundSeed(RoundIndex);
	TWeakObjectPtr<UWorld> WeakWorld(World);
	const int32 Seed = LastRoundSeed;
	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [WeakWorld, Seed]()
	{
		if (URoundMatchStore* Round = URoundMatchStore::Get(WeakWorld.Get()))
		{
			// P1: FRoundParams.Seed arrives with the accepted travel.
			Round->NotifyRoundTravelAccepted(Seed);
		}
	}));
	return ERoundTravelRequestResult::Accepted;
}

uint32 UStealthMatchAdapters::GetConnectionGeneration(FParticipantId Participant) const
{
	const URosterStore* Roster = URosterStore::Get(GetGameWorld());
	const FParticipantRecord* Record = Roster != nullptr && Roster->IsStoreInitialized() ? Roster->FindRecord(Participant) : nullptr;
	return Record != nullptr ? Record->Connection.ConnectionGeneration : 0;
}

void UStealthMatchAdapters::AcknowledgeConnectedPlayers(bool bWorldReady)
{
	UWorld* World = GetGameWorld();
	URoundMatchStore* Round = URoundMatchStore::Get(World);
	const UStealthCompositionRoot* Root = UStealthCompositionRoot::Get(World);
	if (Round == nullptr || Root == nullptr)
	{
		return;
	}
	TArray<TPair<FParticipantId, APlayerController*>> Players;
	Root->GetConnectedPlayers(Players);
	for (const TPair<FParticipantId, APlayerController*>& Pair : Players)
	{
		FRoundTravelAck Ack;
		Ack.Participant = Pair.Key;
		Ack.Epoch = Round->GetPreparingAckEpoch();
		Ack.ConnectionGeneration = GetConnectionGeneration(Pair.Key);
		if (bWorldReady)
		{
			Round->NotifyParticipantWorldReady(Ack);
		}
		else
		{
			Round->NotifyParticipantTravelComplete(Ack);
		}
	}
}

void UStealthMatchAdapters::SignalPreparingEntered()
{
	UWorld* World = GetGameWorld();
	if (World == nullptr)
	{
		return;
	}
	TWeakObjectPtr<UWorld> WeakWorld(World);
	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this, WeakWorld]()
	{
		URoundMatchStore* Round = URoundMatchStore::Get(WeakWorld.Get());
		if (Round == nullptr || Round->GetPhase() != ERoundPhase::Preparing)
		{
			return;
		}
		// The same world hosts every round: each connected player's "travel" is complete now (Session rule 11 stand-in).
		AcknowledgeConnectedPlayers(/*bWorldReady*/ false);
		// Objectives story oc-005 (ADR-0020 section 2): the one cooked chain is selected and validated at Preparing; an
		// invalid chain, none or several is PreparingFailed(ObjectiveChainInvalid) and nothing is bound.
		const UStealthCompositionRoot* Root = UStealthCompositionRoot::Get(WeakWorld.Get());
		if (Root != nullptr && !Root->ValidateObjectivesChain())
		{
			Round->NotifyObjectiveChainInvalid();
			return;
		}
		Round->NotifyTuningReady(0);
	}));
}

void UStealthMatchAdapters::RequestLayout(int32 Seed, uint64 TuningHash)
{
	const UStealthCompositionRoot* Root = UStealthCompositionRoot::Get(GetGameWorld());
	if (AStealthGameMode* Mode = Root != nullptr ? Root->GetGameMode() : nullptr)
	{
		Mode->BuildRoundLayout(Seed);
	}
	else if (URoundMatchStore* Round = URoundMatchStore::Get(GetGameWorld()))
	{
		Round->NotifyLayoutAborted();
	}
}

ERoundSpawnResult UStealthMatchAdapters::SpawnStartPawn(const FRoundSpawnEntry& Entry, const FEpochToken& ExpectedEpoch)
{
	const UStealthCompositionRoot* Root = UStealthCompositionRoot::Get(GetGameWorld());
	AStealthGameMode* Mode = Root != nullptr ? Root->GetGameMode() : nullptr;
	return Mode != nullptr ? Mode->SpawnStartPawn(Entry, ExpectedEpoch) : ERoundSpawnResult::Failed;
}

void UStealthMatchAdapters::OnStartSpawnComplete(int32 ExpectedPawnCount)
{
	UWorld* World = GetGameWorld();
	if (World == nullptr)
	{
		return;
	}
	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		AcknowledgeConnectedPlayers(/*bWorldReady*/ true);
	}));
}

bool UStealthMatchAdapters::OpenSpawnGate(FParticipantId Participant)
{
	const UStealthCompositionRoot* Root = UStealthCompositionRoot::Get(GetGameWorld());
	AStealthGameMode* Mode = Root != nullptr ? Root->GetGameMode() : nullptr;
	return Mode != nullptr && Mode->OpenSpawnGate(Participant);
}

void UStealthMatchAdapters::StartRound()
{
	if (UWorld* World = GetGameWorld())
	{
		// Round's Active-entry step (R7, CNB-R12): the Director starts every crowd pawn in this one step.
		if (UCrowdDirector* Director = UCrowdDirector::Get(World))
		{
			Director->StartRound();
		}
	}
}

int32 UStealthMatchAdapters::DestroyRoundPawns()
{
	const UStealthCompositionRoot* Root = UStealthCompositionRoot::Get(GetGameWorld());
	AStealthGameMode* Mode = Root != nullptr ? Root->GetGameMode() : nullptr;
	return Mode != nullptr ? Mode->DestroyRoundPawns() : 0;
}

ERoundConnectionState UStealthMatchAdapters::GetConnectionState(FParticipantId Participant) const
{
	const UWorld* World = GetGameWorld();
	const UStealthCompositionRoot* Root = UStealthCompositionRoot::Get(World);
	if (Root != nullptr && Root->GetController(Participant) != nullptr)
	{
		return ERoundConnectionState::Connected;
	}
	const URosterStore* Roster = URosterStore::Get(World);
	const FParticipantRecord* Record = Roster != nullptr && Roster->IsStoreInitialized() ? Roster->FindRecord(Participant) : nullptr;
	if (Record != nullptr && Record->Connection.Projection == ERosterConnection::Dormant)
	{
		return ERoundConnectionState::Dormant;
	}
	return ERoundConnectionState::Disconnected;
}

void UStealthMatchAdapters::KickParticipant(FParticipantId Participant)
{
	if (const UStealthCompositionRoot* Root = UStealthCompositionRoot::Get(GetGameWorld()))
	{
		if (APlayerController* Player = Root->GetController(Participant))
		{
			Player->ClientReturnToMainMenuWithTextReason(NSLOCTEXT("Stealth", "Kicked", "Removed by the host."));
		}
	}
}

uint64 UStealthMatchAdapters::GetServerSubSeed(ESeedDomain Domain, uint32 Attempt) const
{
	const USessionService* Session = GetSession();
	return Session != nullptr ? Session->GetServerSubSeed(Domain, Attempt) : 0;
}

void UStealthMatchAdapters::ApplyAppearance(AStealthPawn* Pawn, int32 AppearanceId)
{
	// Prototype appearance (the shared balanced deck) until Character Pool & Appearance's ApplyAppearance lands.
	if (AStealthCharacter* Character = Cast<AStealthCharacter>(Pawn))
	{
		Character->SetAppearanceIndex(AppearanceId);
	}
}

ERosterResult UStealthMatchAdapters::BindPawn(FParticipantId Participant, AStealthPawn* Pawn, const TOptional<FRosterEpochToken>& ExpectedEpoch)
{
	URosterStore* Roster = URosterStore::Get(GetGameWorld());
	return Roster != nullptr ? Roster->BindPawn(Participant, Pawn, ExpectedEpoch) : ERosterResult::Rejected_NotAuthority;
}

AController* UStealthMatchAdapters::CreateAiController(AStealthPawn* Pawn)
{
	UWorld* World = Pawn != nullptr ? Pawn->GetWorld() : nullptr;
	if (World == nullptr)
	{
		return nullptr;
	}
	// The production crowd controller (ADR-0016; D53 integration, crowd story cnb-006): one path only. The prototype
	// AStealthNpcController is no longer created (story cnb-039 deletes it). The controller is created Dormant: the
	// Director starts it at Round's Active entry (R6, CNB-R12).
	if (UCrowdDirector* Director = UCrowdDirector::Get(World))
	{
		Director->SetCrowdArea(CrowdCenter, CrowdRadius, CrowdMarketRadius);
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	return World->SpawnActor<AStealthCrowdController>(Params);
}

// -------------------------------------------------------------------------------------------------------------------
// UStealthCompositionRoot
// -------------------------------------------------------------------------------------------------------------------

void UStealthCompositionRoot::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<USessionService>();
	URosterStore* Roster = Collection.InitializeDependency<URosterStore>();
	URoundMatchStore* Round = Collection.InitializeDependency<URoundMatchStore>();

	Adapters = NewObject<UStealthMatchAdapters>(this);
	RosterGateway = NewObject<URoundRosterStoreGateway>(this);
	RosterGateway->Bind(Roster);
	// NKP story 009: Round's informant body provider lives as long as this root (the Round gateways hold it raw).
	InformantBodies = MakeShared<FStealthInformantBodiesProvider>();

	if (Roster != nullptr)
	{
		RosterEventHandle = Roster->OnRosterEvent.AddUObject(this, &UStealthCompositionRoot::HandleRosterEvent);
	}
	if (Round != nullptr)
	{
		QueueReleasingHandle = Round->OnRoundQueueReleasing.AddUObject(this, &UStealthCompositionRoot::HandleRoundQueueReleasing);
		RoundPhaseHandle = Round->OnPhaseChanged.AddUObject(this, &UStealthCompositionRoot::HandleRoundPhaseChanged);
		QueueDrainedHandle = Round->OnRoundQueueDrained.AddUObject(this, &UStealthCompositionRoot::HandleRoundQueueDrained);
		RoundActiveHandle = Round->OnRoundActive.AddUObject(this, &UStealthCompositionRoot::HandleRoundActive);
		Round->SetLocalParticipantResolver([this](const UWorld* World) -> FParticipantId
		{
			const APlayerController* Local = World != nullptr ? World->GetFirstPlayerController() : nullptr;
			return GetParticipant(Local);
		});
	}
}

void UStealthCompositionRoot::Deinitialize()
{
	if (PreparePredicate != nullptr)
	{
		PreparePredicate->Initialize(nullptr, FAssassinationTuning_Public()); // the clock below is released
	}
	if (DueAdapter.IsValid())
	{
		DueAdapter->Unbind();
	}
	if (UPenaltyService* PenaltyService = Penalty.Get())
	{
		PenaltyService->Unbind(); // NKP: before the encounter core it subscribed to goes
	}
	Penalty.Reset();
	PenaltyPawns.Reset();
	PenaltyRespawns.Reset();
	DueAdapter.Reset();
	OwnerViewFeed.Reset();
	if (InformantBodies.IsValid())
	{
		InformantBodies->SetService(nullptr);
	}
	if (Encounters != nullptr)
	{
		Encounters->Shutdown(); // Round (a dependency) still holds its queue here
	}
	if (URoundMatchStore* Round = GetRoundStore())
	{
		Round->OnRoundQueueReleasing.Remove(QueueReleasingHandle);
		Round->OnPhaseChanged.Remove(RoundPhaseHandle);
		Round->OnRoundQueueDrained.Remove(QueueDrainedHandle);
		Round->OnRoundActive.Remove(RoundActiveHandle);
	}
	if (UObjectivesService* ObjectivesService = Objectives.Get())
	{
		ObjectivesService->Unbind();
	}
	Objectives.Reset();
	InteractHolds.Reset();
	ObjectivesAdapters.Reset();
	PrototypeFallback.Reset();
	if (UConcealmentService* ConcealmentService = Concealment.Get())
	{
		ConcealmentService->Unbind();
	}
	Concealment.Reset();
	HerringSites.Reset();
	ConcealmentFlags.Reset();
	ReplyTransport.Reset();
	CommandEpoch.Reset();
	CommandQueue.Reset();
	CommandClock.Reset();
	if (URosterStore* Roster = GetRosterStore())
	{
		Roster->OnRosterEvent.Remove(RosterEventHandle);
	}
	ParticipantByController.Reset();
	Super::Deinitialize();
}

UStealthCompositionRoot* UStealthCompositionRoot::Get(const UWorld* World)
{
	const UGameInstance* GameInstance = World != nullptr ? World->GetGameInstance() : nullptr;
	return GameInstance != nullptr ? GameInstance->GetSubsystem<UStealthCompositionRoot>() : nullptr;
}

URosterStore* UStealthCompositionRoot::GetRosterStore() const
{
	return GetGameInstance() != nullptr ? GetGameInstance()->GetSubsystem<URosterStore>() : nullptr;
}

URoundMatchStore* UStealthCompositionRoot::GetRoundStore() const
{
	return GetGameInstance() != nullptr ? GetGameInstance()->GetSubsystem<URoundMatchStore>() : nullptr;
}

void UStealthCompositionRoot::OnServerWorldStarting(AStealthGameMode* InGameMode)
{
	GameMode = InGameMode;
	URoundMatchStore* Round = GetRoundStore();
	URosterStore* Roster = GetRosterStore();
	if (Round == nullptr || Roster == nullptr)
	{
		return;
	}

	// Round rule inputs: TeamSize is the TDC Round key's default capped by the prototype's per-team limit (3v3 / 5v5
	// configs); the keys not yet in the TDC schema use their GDD defaults, with the development allowances on.
	FRoundRuleParams Params;
	Params.TeamSize = FMath::Clamp(FMath::Min(FRoundTuning_Public().TeamSize, UStealthSettings::Get()->MaxPlayersPerTeam), 1, 5);
	Params.MinPlayersPerTeamCount = 1;
	Params.MaxTeamImbalanceCount = 1;
#if !UE_BUILD_SHIPPING
	Params.bAllowEmptyTeamDev = true;
	Params.bHostMayAbortActive = true;
#endif
	Params.EndedMinDwellSec = 3.0;
	// The prototype crowd size (UStealthSettings) stands in for TDC's NpcCount until the crowd epic reads the key.
	Params.NpcCount = FMath::Max(UStealthSettings::Get()->NpcCount, 0);
	Params.MapId = FName(*UWorld::RemovePIEPrefix(InGameMode->GetWorld()->GetOutermost()->GetName()));
	Round->SetRuleParams(Params);

	// Session.OpenSession (story st-014): the session secret from the OS CSPRNG and the empty-store check. The Roster
	// match is begun by Round's OpenMatch below (Round latches TeamSize). A failure is "could not host".
	if (USessionService* Session = USessionService::Get(InGameMode->GetWorld()); Session != nullptr && !Session->IsSessionOpen())
	{
		FSessionOpenParams OpenParams;
		OpenParams.TeamSize = Params.TeamSize;
		OpenParams.bBeginRosterMatch = false;
		if (Session->OpenSession(OpenParams) != EStealthSessionOpenResult::Opened)
		{
			UE_LOGF(LogStealth, Error, "%ls", *FStealthServerSeeds::GetCouldNotHostText().ToString());
			return; // no weaker source: no round can start
		}
	}

	// The match core is released by Session's EndMatch fan-out on every exit path; register again on the next start.
	if (!Round->HasMatchCore())
	{
		FRoundGateways Gateways;
		Gateways.Session = Adapters;
		Gateways.Roster = RosterGateway;
		Gateways.Tuning = Adapters;
		Gateways.CharacterPool = Adapters;
		Gateways.LevelSocket = Adapters;
		Gateways.Crowd = Adapters;
		Gateways.Spawner = Adapters;
		Gateways.PawnControl = Adapters;
		Gateways.ObjectiveProgress = Adapters;
		Gateways.InformantBodies = InformantBodies.Get(); // NKP story 009: Penalty's provider (OnInformantAwaitingBody, R18)
		USessionService* Session = USessionService::Get(InGameMode->GetWorld());
		Round->RegisterGateways(Gateways, this, Session != nullptr ? &Session->GetSubSeedSource() : nullptr);
	}
	if (!Roster->IsMatchOpen())
	{
		Round->OpenMatch();
	}

	if (UStealthPawnFactory* Factory = UStealthPawnFactory::Get(InGameMode->GetWorld()))
	{
		Factory->SetHooks(Adapters, this);
	}

	// The pawn command interface (ADR-0011): every pawn's FPawnCommandCore binds these seams at BeginPlay. Every discrete
	// command is a PawnCommand item in Round's queue (stories asn-012/013 integration): a participant's in its own mailbox
	// under its connection generation, AI and server-local ones in the server-local mailbox; the epoch is the Roster's.
	if (UActionHub* Hub = UActionHub::Get(InGameMode->GetWorld()))
	{
		CommandClock = MakeShared<FStealthWorldCommandClock>(InGameMode->GetWorld());
		CommandQueue = MakeShared<FStealthRoundCommandQueue>(Round, Adapters);
		CommandEpoch = MakeShared<FStealthRoundEpochSource>(RosterGateway);
		FPawnCommandCoreDeps Deps;
		Deps.Clock = CommandClock.Get();
		Deps.Queue = CommandQueue.Get();
		Deps.Epoch = CommandEpoch.Get();
		Deps.FaultSink = &CommandFaults;
		Hub->SetCommandWiring(Deps);

		// ADR-0011 section 6 / story asn-003: Assassination's Prepare predicate (Reject(Cooldown) while F3's cooldown runs),
		// registered weakly; this root keeps it alive. Tuning stand-in: the schema defaults (as the Crowd director) until
		// the TDC tuning store feeds features the frozen set.
		if (PreparePredicate == nullptr)
		{
			PreparePredicate = NewObject<UAssassinationPreparePredicate>(this);
		}
		PreparePredicate->Initialize(CommandClock.Get(), FAssassinationTuning_Public());
		Hub->RegisterPredicate(EActionType::Prepare, TWeakInterfacePtr<ICommandPredicate>(PreparePredicate));
		// Story asn-008: Attack's CanBegin is always Allow (no Attack is rejected for a reason about its target, ASN-R1);
		// the lockout is the action slot's Busy at the Attack's queue slot.
		if (AttackPredicate == nullptr)
		{
			AttackPredicate = NewObject<UAssassinationAttackPredicate>(this);
		}
		Hub->RegisterPredicate(EActionType::Attack, TWeakInterfacePtr<ICommandPredicate>(AttackPredicate));

		WireEncounters(InGameMode);
	}
}

void UStealthCompositionRoot::WireEncounters(AStealthGameMode* InGameMode)
{
	URoundMatchStore* Round = GetRoundStore();
	FResolutionQueueCore* Queue = Round != nullptr ? Round->GetRoundQueue() : nullptr;
	FRoundMatchCore* MatchCore = Round != nullptr ? Round->GetMatchCore() : nullptr;
	UWorld* World = InGameMode != nullptr ? InGameMode->GetWorld() : nullptr;
	if (Queue == nullptr || MatchCore == nullptr || World == nullptr || !CommandClock.IsValid())
	{
		UE_LOGF(LogStealth, Error, "Encounters not wired: no round queue (strikes cannot resolve this match).");
		return;
	}
	if (Encounters == nullptr)
	{
		Encounters = NewObject<UEncounterService>(this);
	}
	if (DueAdapter.IsValid())
	{
		DueAdapter->Unbind();
	}
	if (UPenaltyService* PreviousPenalty = Penalty.Get())
	{
		PreviousPenalty->Unbind(); // NKP: it subscribed to the encounter core about to go
	}
	Encounters->Shutdown(); // a previous world's registration on the same queue (Release already ran for a destroyed one)

	// Stand-ins of the composition root (StealthCommandWiring.h): the Interact / Traverse dispatch until IIF stage 2 (Hide
	// goes to Concealment's resolver). Tuning: the schema defaults until the TDC store feeds features.
	const FEncounterParams Params = FEncounterParams::Defaults();
	// Concealment (con-003..014) first: Assassination's bush spot query and body-removal observer are its service.
	UConcealmentService* ConcealmentService = WireConcealment(World);
	// Objectives stories oc-005..009: the Interact hold stand-in (until IIF stage 2) and the Objectives adapters it reads.
	ObjectivesAdapters = MakeShared<FStealthObjectivesAdapters>(World, this, CommandClock.Get());
	FInteractHoldStandIn::FDeps HoldDeps;
	HoldDeps.Hub = UActionHub::Get(World);
	HoldDeps.Queue = CommandQueue.Get();
	HoldDeps.Clock = CommandClock.Get();
	TWeakPtr<FStealthObjectivesAdapters> WeakObjectivesAdapters = ObjectivesAdapters;
	HoldDeps.FindCourtInReach = [WeakObjectivesAdapters](const APawn& Pawn)
	{
		const TSharedPtr<FStealthObjectivesAdapters> Pinned = WeakObjectivesAdapters.Pin();
		return Pinned.IsValid() ? Pinned->FindCourtInReach(Pawn) : FCourtFixtureId();
	};
	HoldDeps.IsLegalityActive = [WeakObjectivesAdapters](const APawn& Pawn)
	{
		const TSharedPtr<FStealthObjectivesAdapters> Pinned = WeakObjectivesAdapters.Pin();
		return Pinned.IsValid() && Pinned->IsLegalityActive(Pawn);
	};
	// Story oc-019 (AC-OC-26, IIF R5): ingress accepts any court fixture of this world, live or not; an id naming no court is
	// an unknown id and is dropped (the stand-in courts are the fixtures until the LSC layout service publishes them).
	if (UActionHub* IngressHub = UActionHub::Get(World))
	{
		IngressHub->SetCourtFixtureValidator([WeakObjectivesAdapters](FCourtFixtureId Court)
		{
			const TSharedPtr<FStealthObjectivesAdapters> Pinned = WeakObjectivesAdapters.Pin();
			return Pinned.IsValid() && Pinned->FindCourtActor(Court) != nullptr;
		});
	}
	InteractHolds = MakeShared<FInteractHoldStandIn>(HoldDeps);
	PrototypeFallback = MakeShared<FStealthPrototypeCommandFallback>(InGameMode, InteractHolds.Get(), ConcealmentService);
	ReplyTransport = MakeShared<FStealthControllerReplyTransport>(World);
	ReplyWorld = World;
	USessionService* Session = USessionService::Get(World);

	FEncounterServiceInit Init;
	Init.World = World;
	Init.Clock = CommandClock.Get();
	Init.Queue = Queue;
	Init.SubSeeds = Session != nullptr ? &Session->GetSubSeedSource() : nullptr;
	Init.Params = Params;
	Init.Log = MatchCore->GetRoundLogWriter();
	Init.Bushes = ConcealmentService; // ASN-006: the real bush spot query (Concealment's occupancy record and footprints)
	Init.Roster = GetRosterStore();
	// NKP story 005: the production Penalty service is the tally source (its exile sets bExiled); story nkp-021 retired the
	// prototype fallback, so a world with no Penalty subsystem answers every result with tally 0 and no exile.
	UPenaltyService* PenaltyService = World->GetSubsystem<UPenaltyService>();
	Init.Tally = PenaltyService;
	Init.BodyObserver = ConcealmentService; // the killed-concealed flags clear at the body's removal (story con-017)
	Init.Faults = &CommandFaults;
	if (!Encounters->Initialize(Init))
	{
		UE_LOGF(LogStealth, Error, "Encounters not wired: the service refused its seams.");
		return;
	}
	// HoldTimer: Assassination's Prepare timers and its pose ends; every other hold timer (Interact's HoldTimer(Complete), story
	// oc-005) goes to the Interact hold stand-in.
	Encounters->SetFallbackResolvers(PrototypeFallback.Get(), InteractHolds.Get());
	if (!Encounters->RegisterOn(*Queue))
	{
		UE_LOGF(LogStealth, Error, "Encounters: a PawnCommand / HoldTimer resolver or a DeathWrite / CounterPose applier was already registered.");
	}
	InGameMode->BindEncounterService(*Encounters);
	// Concealment hears the kills (the outcome slot: the record leaves, Entering victims stay visible) and the death writes
	// (the staging pass: a killed-concealed victim re-opens with bDiedConcealed), stories con-017 / con-018.
	if (ConcealmentService != nullptr)
	{
		TWeakObjectPtr<UConcealmentService> WeakConcealment(ConcealmentService);
		if (FOnPawnKilled* Killed = Encounters->GetOnPawnKilled())
		{
			ConcealKilledHandle = Killed->AddLambda([WeakConcealment](APawn* Victim, APawn*, EVictimKind, EKillCause)
			{
				if (UConcealmentService* Service = WeakConcealment.Get())
				{
					Service->HandlePawnKilled(Victim);
				}
			});
		}
		if (FOnDeathWriteApplied* DeathWrite = Encounters->GetOnDeathWriteApplied())
		{
			ConcealDeathWriteHandle = DeathWrite->AddLambda([WeakConcealment](APawn* Victim, double, bool)
			{
				if (UConcealmentService* Service = WeakConcealment.Get())
				{
					Service->HandleDeathWrite(Victim);
				}
			});
		}
	}

	// NPC-Kill Penalty (stories nkp-001..005, 008): the production service counts strikes from OnPawnKilled, answers the
	// tally, runs the exile of the L-th ungraced NPC kill (Roster StrikeLimit, the Crowd Director's adoption, the factory
	// hand-over through FStealthPenaltyPawnGateway) and the informant kill's RetireBinding, and writes its Event Log rows.
	// The prototype penalty in AStealthGameMode::HandleEncounterPawnKilled stands down while it is bound (nkp-021 deletes
	// it). Tuning: the schema defaults until the TDC store feeds features.
	if (PenaltyService != nullptr)
	{
		PenaltyPawns = MakeShared<FStealthPenaltyPawnGateway>(InGameMode);
		// NKP story 012: the respawn commit's gateways (pick, takeover and card are stand-ins behind the seam until LSC 033,
		// SCL 028 and the Character Pool land; TryReleaseNpc / AdoptReleasedPawn are the Crowd Director's).
		PenaltyRespawns = MakeShared<FStealthPenaltyRespawnGateway>(World, this);
		FPenaltyServiceInit PenaltyInit;
		PenaltyInit.Clock = CommandClock.Get();
		PenaltyInit.Queue = Queue;
		PenaltyInit.Log = MatchCore->GetRoundLogWriter();
		PenaltyInit.Roster = GetRosterStore();
		PenaltyInit.Encounters = Encounters;
		PenaltyInit.Crowd = UCrowdDirector::Get(World);
		PenaltyInit.Pawns = PenaltyPawns.Get();
		PenaltyInit.RespawnGateway = PenaltyRespawns.Get();
		// NKP story 017 (the tally half of roster psr-038): the owner-view feed is now also the service's OwnerView, so the
		// reclaim / rejoin / reset re-push {WrongTargetTally, LastResultSeq} reaches the C-TeamView owner part (PEN-R3).
		OwnerViewFeed = MakeShared<FStealthRosterOwnerViewFeed>(this);
		PenaltyInit.OwnerView = OwnerViewFeed.Get();
		PenaltyInit.Faults = &CommandFaults;
		if (!PenaltyService->Bind(PenaltyInit))
		{
			PenaltyService = nullptr;
		}
	}
	Penalty = PenaltyService;
	if (InformantBodies.IsValid())
	{
		InformantBodies->SetService(PenaltyService); // Round's OnInformantAwaitingBody / R18 readmission / R17.9 reach it
	}
	// NKP story 011: the T_due carrier adapter writes OnDueTimeChanged to the C-TeamView owner part through the Roster's
	// owner-view feed, never while Round's queue is latched.
	if (PenaltyService != nullptr)
	{
		DueAdapter = MakeShared<FPenaltyDueAdapter>(OwnerViewFeed.Get(), [this]()
		{
			const URoundMatchStore* RoundStore = GetRoundStore();
			const FResolutionQueueCore* RoundQueue = RoundStore != nullptr ? RoundStore->GetRoundQueue() : nullptr;
			return RoundQueue != nullptr && RoundQueue->IsLatched();
		}, [this]() -> uint32
		{
			const URosterStore* Store = GetRosterStore();
			return Store != nullptr && Store->IsStoreInitialized() ? Store->GetEpochToken().RoundEpoch : 0u;
		});
		DueAdapter->Bind(*PenaltyService);
	}
	WireObjectives(InGameMode);
}

UConcealmentService* UStealthCompositionRoot::WireConcealment(UWorld* World)
{
	URoundMatchStore* Round = GetRoundStore();
	FResolutionQueueCore* Queue = Round != nullptr ? Round->GetRoundQueue() : nullptr;
	FRoundMatchCore* MatchCore = Round != nullptr ? Round->GetMatchCore() : nullptr;
	UConcealmentService* Service = UConcealmentService::Get(World);
	UActionHub* Hub = UActionHub::Get(World);
	if (Service == nullptr || Queue == nullptr || MatchCore == nullptr || Hub == nullptr || !CommandClock.IsValid())
	{
		UE_LOGF(LogStealth, Error, "Concealment not wired: no service, hub or round queue (no pawn can hide this match).");
		return nullptr;
	}
	ConcealmentFlags = MakeShared<FStealthConcealmentFlags>(this);
	FConcealmentServiceInit Init;
	Init.Clock = CommandClock.Get();
	Init.Queue = Queue;
	Init.Epoch = CommandEpoch.Get();
	Init.Hub = Hub;
	Init.Roster = GetRosterStore();
	Init.Flags = ConcealmentFlags.Get();
	// Story con-025 (ADR-0003 section 5): the Visibility Policy's concealment cull, audience by the Roster's teams.
	if (UStealthConcealmentCull* Cull = UStealthConcealmentCull::Get(World))
	{
		TWeakObjectPtr<UStealthCompositionRoot> WeakRoot(this);
		Cull->ConfigureTeams([WeakRoot](const APawn* Pawn) -> int32
		{
			URosterStore* Roster = WeakRoot.IsValid() ? WeakRoot->GetRosterStore() : nullptr;
			const FParticipantRecord* Record = Roster != nullptr ? Roster->FindRecord(Roster->GetParticipant(const_cast<APawn*>(Pawn))) : nullptr;
			return Record != nullptr && Record->Team != ERosterTeam::None ? static_cast<int32>(Record->Team) : -1;
		}, [WeakRoot](const AActor* Viewer) -> int32
		{
			const UStealthCompositionRoot* Root = WeakRoot.Get();
			const URosterStore* Roster = Root != nullptr ? Root->GetRosterStore() : nullptr;
			const FParticipantRecord* Record = Roster != nullptr ? Roster->FindRecord(Root->GetParticipant(Cast<APlayerController>(Viewer))) : nullptr;
			return Record != nullptr && Record->Team != ERosterTeam::None ? static_cast<int32>(Record->Team) : -1;
		});
		Cull->SetTiming(FLocomotionTuning_Server().PawnNetUpdateHz, StealthTuningPendingOwnerKeys::ConcealCullDelayMs);
		Init.Cull = Cull;
	}
	Init.Log = MatchCore->GetRoundLogWriter();
	Init.Faults = &CommandFaults;
	// Tuning stand-in: the schema defaults (as every feature) until the TDC store feeds features the frozen set.
	Init.Params.Public = FConcealmentTuning_Public();
	Init.Params.Server = FConcealmentTuning_Server();
	Init.Params.AttackLockoutSec = FAssassinationTuning_Public().AttackAttemptLockoutSec;
	Init.Params.StrikeContactSec = FAssassinationTuning_Public().StrikeContactSec;
	if (!Service->Bind(Init))
	{
		UE_LOGF(LogStealth, Error, "Concealment not wired: the service refused its seams.");
		return nullptr;
	}
	Concealment = Service;
	// The layout's bushes (the stand-in Manifest built them at the layout step; a rebuild after travel lands here too).
	if (const UBushRuntimeSubsystem* Runtime = UBushRuntimeSubsystem::Get(World))
	{
		Service->SetBushes(Runtime->GetBushes());
	}
	// ADR-0011 section 6: the Hide predicate for both directions, weakly; the service is a world subsystem (lives with it).
	Hub->RegisterPredicate(EActionType::HideEnter, TWeakInterfacePtr<ICommandPredicate>(Service));
	Hub->RegisterPredicate(EActionType::HideExit, TWeakInterfacePtr<ICommandPredicate>(Service));
	// Crowd (ADR-0016 sections 3 and 5): the perception filter, and the bush herring sites (CNB-022 switched on).
	if (UCrowdDirector* Director = UCrowdDirector::Get(World))
	{
		Director->SetPerceptionFilter(TWeakInterfacePtr<ICrowdPerceptionFilter>(Service));
		HerringSites = MakeShared<FStealthCrowdHerringSites>(World);
		Director->SetHerringSites(HerringSites.Get());
	}
	return Service;
}

void UStealthCompositionRoot::WireObjectives(AStealthGameMode* InGameMode)
{
	URoundMatchStore* Round = GetRoundStore();
	FResolutionQueueCore* Queue = Round != nullptr ? Round->GetRoundQueue() : nullptr;
	FRoundMatchCore* MatchCore = Round != nullptr ? Round->GetMatchCore() : nullptr;
	UWorld* World = InGameMode != nullptr ? InGameMode->GetWorld() : nullptr;
	UObjectivesService* ObjectivesService = World != nullptr ? World->GetSubsystem<UObjectivesService>() : nullptr;
	UActionHub* Hub = UActionHub::Get(World);
	if (ObjectivesService == nullptr || Hub == nullptr || !ObjectivesAdapters.IsValid() || Queue == nullptr || MatchCore == nullptr)
	{
		UE_LOGF(LogStealth, Error, "Objectives not wired: no service, hub, adapters or round queue (no objective can complete this match).");
		return;
	}
	FStealthObjectivesAdapters& A = *ObjectivesAdapters;
	FObjectivesServiceInit Init;
	Init.Roster = &A;
	Init.Session = &A;
	Init.PawnControl = &A;
	Init.Round = &A;
	Init.Courts = &A;
	Init.Assignment = &A;
	Init.Chains = UObjectivesChainRegistry::Get(World);
	// Story oc-011: the team view is written to each member's UObjectiveTeamViewComponent (owner-only C-TeamView part); story
	// oc-012: the service subscribes to the Roster's rebind and reset events itself.
	Init.ViewTargets = &A;
	Init.RosterEvents = GetRosterStore();
	// Stories oc-013 / oc-014 / oc-015: the stand-in clue records, Session's StageSignal sub-seeds; the service schedules its
	// own TimerFired(StageSignal) on the round queue (RivalRelease left null).
	Init.Clues = &A;
	Init.SecretDraw = &A;
	Init.Queue = Queue;
	Init.Faults = &CommandFaults;
	// Tuning stand-in: the schema defaults (as the Crowd director and Assassination) until the TDC store feeds features.
	Init.PublicTuning = FObjectivesTuning_Public();
	Init.ServerTuning = FObjectivesTuning_Server();
#if !UE_BUILD_SHIPPING
	TWeakObjectPtr<URoundMatchStore> WeakRound(Round);
	Init.SubmitDebugStage = [WeakRound](uint8 Team)
	{
		URoundMatchStore* Store = WeakRound.Get();
		FRoundMatchCore* Core = Store != nullptr ? Store->GetMatchCore() : nullptr;
		return Core != nullptr && Core->SubmitDebugObjectiveStage(Team) == ERoundCommandResult::Executed;
	};
#endif
	if (!ObjectivesService->Bind(Init))
	{
		UE_LOGF(LogStealth, Error, "Objectives not wired: the service refused its seams.");
		return;
	}
	Objectives = ObjectivesService;
	// ADR-0020 section 1 / ADR-0011 section 6: the Interact predicate and the one interact-target owner, registered weakly.
	Hub->RegisterPredicate(EActionType::Interact, TWeakInterfacePtr<ICommandPredicate>(ObjectivesService));
	Hub->RegisterInteractTargetOwner(TWeakInterfacePtr<IInteractTargetOwner>(ObjectivesService));
	// Concealment stories con-013 / con-014 (IIF R9 stand-in): no tend from inside a bush or a hide transition (NotNow).
	if (UConcealmentService* ConcealmentService = Concealment.Get())
	{
		if (InteractBushGate == nullptr)
		{
			InteractBushGate = NewObject<UStealthBushGatedPredicate>(this);
		}
		InteractBushGate->Initialize(TWeakInterfacePtr<ICommandPredicate>(ObjectivesService), ConcealmentService);
		Hub->RegisterPredicate(EActionType::Interact, TWeakInterfacePtr<ICommandPredicate>(InteractBushGate));
	}
#if !UE_BUILD_SHIPPING
	// Round's DebugItem slot for DebugObjectiveCompletion runs the same counted-completion effect (story oc-008).
	TWeakObjectPtr<UObjectivesService> WeakService(ObjectivesService);
	MatchCore->OnObjectiveDebugStage = [WeakService](uint8 Team, FQueueKey Key)
	{
		if (UObjectivesService* Service = WeakService.Get())
		{
			Service->ApplyDebugStage(Team, Key);
		}
	};
#endif
}

bool UStealthCompositionRoot::ValidateObjectivesChain() const
{
	UObjectivesService* ObjectivesService = Objectives.Get();
	if (ObjectivesService == nullptr)
	{
		return true; // no Objectives wiring in this world (a harness world without the composition root's game mode)
	}
	const FObjectivesSelection Selection = ObjectivesService->ValidateSelectedChain(FStealthServerTuning());
	if (!Selection.bValid)
	{
		UE_LOGF(LogStealth, Error, "Objectives: the selected chain is invalid (clause %d, row %d, %d cooked chain(s)): PreparingFailed(ObjectiveChainInvalid).",
			static_cast<int32>(Selection.Clause), Selection.Row, Selection.CookedChainCount);
	}
	return Selection.bValid;
}

void UStealthCompositionRoot::HandleObjectivesLayoutReady(uint32 LayoutId, int32 Seed)
{
	if (UObjectivesService* ObjectivesService = Objectives.Get())
	{
		ObjectivesService->HandleLayoutReady(LayoutId, Seed);
	}
}

void UStealthCompositionRoot::HandleRoundActive(uint32 RoundEpoch, double ActivePhaseStart)
{
	UObjectivesService* ObjectivesService = Objectives.Get();
	const URosterStore* Roster = GetRosterStore();
	if (ObjectivesService != nullptr)
	{
		const FEpochToken Epoch(Roster != nullptr && Roster->IsStoreInitialized() ? Roster->GetEpochToken().MatchId : 0, RoundEpoch);
		ObjectivesService->HandleRoundActive(Epoch, ActivePhaseStart);
	}
	// ADR-0003 section 5.3: the concealment cull's global grid starts at the round's Active entry.
	const UConcealmentService* ConcealmentService = Concealment.Get();
	if (UStealthConcealmentCull* Cull = ConcealmentService != nullptr ? UStealthConcealmentCull::Get(ConcealmentService->GetWorld()) : nullptr)
	{
		Cull->SetGridAnchor(ActivePhaseStart);
	}
}

#if !UE_BUILD_SHIPPING
FCourtFixtureId UStealthCompositionRoot::FindObjectivesLiveCourtForDev(FParticipantId Participant) const
{
	const UObjectivesService* ObjectivesService = Objectives.Get();
	if (ObjectivesService == nullptr || !ObjectivesAdapters.IsValid() || !Participant.IsValid())
	{
		return FCourtFixtureId();
	}
	return ObjectivesService->GetLiveCourtForDev(ObjectivesAdapters->GetTeam(Participant));
}

const AActor* UStealthCompositionRoot::FindObjectivesCourtActorForDev(FCourtFixtureId Court) const
{
	return ObjectivesAdapters.IsValid() ? ObjectivesAdapters->FindCourtActor(Court) : nullptr;
}
#endif


void UStealthCompositionRoot::HandleRoundPhaseChanged(ERoundPhase OldPhase, ERoundPhase NewPhase)
{
	// Story asn-019 (AC-ASN-41): Round's Reset step clears the encounter round with it (Round story 058): roles Ready,
	// every defence stamp and owner cooldown none, pending writes forgotten, bodies torn down, body items voided.
	if (NewPhase == ERoundPhase::Reset && Encounters != nullptr)
	{
		Encounters->ResetRound();
	}
	// Objectives (stories oc-004, oc-005): frozen at Ended; cleared at Reset. Every running hold ends outside a slot (the
	// round-end staging pass): a claim release only.
	if (NewPhase == ERoundPhase::Ended || NewPhase == ERoundPhase::Reset)
	{
		if (InteractHolds.IsValid())
		{
			InteractHolds->CancelAll();
		}
		if (UObjectivesService* ObjectivesService = Objectives.Get())
		{
			if (NewPhase == ERoundPhase::Ended)
			{
				ObjectivesService->HandleRoundEnded();
			}
			else
			{
				ObjectivesService->HandleRoundReset();
			}
		}
	}
	// NKP story 001 (PEN-R4): Penalty latches L and the release grace at Preparing (TDC rule CR-11 Latching).
	if (NewPhase == ERoundPhase::Preparing)
	{
		if (UPenaltyService* PenaltyService = Penalty.Get())
		{
			PenaltyService->OnRoundPreparing();
		}
	}
}

void UStealthCompositionRoot::HandleRoundQueueDrained()
{
	// The reply step (ADR-0011 section 8; story asn-015): every reply appended inside this drain's slots, one batched
	// ClientCommandReplies per connection, in queue-key order; connections with nothing get no call.
	UActionHub* Hub = UActionHub::Get(ReplyWorld.Get());
	if (Hub != nullptr && ReplyTransport.IsValid())
	{
		Hub->GetReplySinks().FlushPass(*ReplyTransport);
	}
#if !UE_BUILD_SHIPPING
	// ADR-0020 5.1 (story oc-006): at drain end each fixture's claim count equals the Interacting slots targeting it.
	UObjectivesService* ObjectivesService = Objectives.Get();
	if (ObjectivesService != nullptr && InteractHolds.IsValid())
	{
		TMap<FCourtFixtureId, int32> Slots;
		InteractHolds->GetInteractingSlotsByCourt(Slots);
		ObjectivesService->CheckClaimConsistency(Slots);
	}
#endif
}

void UStealthCompositionRoot::HandleRoundQueueReleasing()
{
	if (UObjectivesService* ObjectivesService = Objectives.Get())
	{
		ObjectivesService->ForgetQueue(); // its StageSignal registration goes with the queue
	}
	if (DueAdapter.IsValid())
	{
		DueAdapter->Unbind();
	}
	if (UPenaltyService* PenaltyService = Penalty.Get())
	{
		PenaltyService->Unbind(); // NKP: its slot context is this queue, its subscription the encounter core
	}
	if (Encounters != nullptr)
	{
		Encounters->Release();
	}
}

FParticipantId UStealthCompositionRoot::RegisterPlayer(APlayerController* Player)
{
	URosterStore* Roster = GetRosterStore();
	URoundMatchStore* Round = GetRoundStore();
	if (Player == nullptr || Roster == nullptr || !Roster->IsMatchOpen())
	{
		return FParticipantId();
	}
	const APlayerState* State = Player->GetPlayerState<APlayerState>();
	const FRosterRegisterResult Result = Roster->RegisterParticipant(StealthCompositionLocal::MakeIdentityKey(Player), State != nullptr ? State->GetPlayerName() : Player->GetName());
	if (!Result.Id.IsValid())
	{
		UE_LOGF(LogStealth, Warning, "Roster refused %ls (result %d).", *Player->GetName(), static_cast<int32>(Result.Result));
		return FParticipantId();
	}
	ParticipantByController.Add(Player, Result.Id);
	if (Result.Result == ERosterResult::Reclaimed)
	{
		Roster->NotifyConnectionChanged(Result.Id, ERosterConnection::Connected);
	}
	if (Round != nullptr && Player->IsLocalController() && !Round->GetHostParticipant().IsValid())
	{
		Round->SetHostParticipant(Result.Id);
	}
	if (URosterTeamViewComponent* View = Player->FindComponentByClass<URosterTeamViewComponent>())
	{
		Roster->RegisterTeamView(Result.Id, View);
	}
	MirrorTeamsToPrototype();
	return Result.Id;
}

void UStealthCompositionRoot::UnregisterPlayer(APlayerController* Player)
{
	URosterStore* Roster = GetRosterStore();
	const FParticipantId Id = GetParticipant(Player);
	ParticipantByController.Remove(Player);
	if (Roster == nullptr || !Id.IsValid() || !Roster->IsMatchOpen() || GetController(Id) != nullptr)
	{
		// Unknown, no match, or a newer controller already holds this participant (a superseding rejoin).
		return;
	}
	Roster->UnregisterTeamView(Id);
	const URoundMatchStore* Round = GetRoundStore();
	if (Round != nullptr && Round->GetPhase() == ERoundPhase::Lobby && !Roster->GetCore().AreTeamsLocked())
	{
		// A Lobby leaver frees the slot (R3: teams change only in Lobby).
		Roster->RemoveParticipant(Id);
	}
	else
	{
		Roster->NotifyConnectionChanged(Id, ERosterConnection::Disconnected);
		// The engine destroys the leaver's pawn next (PawnLeavingGame): release it first so the EndPlay clear is not a
		// live-binding loss (no fault, no BindingLost for a player who left).
		if (Player != nullptr && Player->GetPawn() != nullptr)
		{
			Roster->ReleaseToCrowd(Player->GetPawn());
		}
	}
}

FParticipantId UStealthCompositionRoot::GetParticipant(const APlayerController* Player) const
{
	if (Player == nullptr)
	{
		return FParticipantId();
	}
	const FParticipantId* Found = ParticipantByController.Find(const_cast<APlayerController*>(Player));
	return Found != nullptr ? *Found : FParticipantId();
}

APlayerController* UStealthCompositionRoot::GetController(FParticipantId Participant) const
{
	for (const TPair<TWeakObjectPtr<APlayerController>, FParticipantId>& Pair : ParticipantByController)
	{
		if (Pair.Value == Participant && Pair.Key.IsValid())
		{
			return Pair.Key.Get();
		}
	}
	return nullptr;
}

void UStealthCompositionRoot::GetConnectedPlayers(TArray<TPair<FParticipantId, APlayerController*>>& Out) const
{
	Out.Reset();
	for (const TPair<TWeakObjectPtr<APlayerController>, FParticipantId>& Pair : ParticipantByController)
	{
		if (Pair.Key.IsValid())
		{
			Out.Add({ Pair.Value, Pair.Key.Get() });
		}
	}
	Out.Sort([](const TPair<FParticipantId, APlayerController*>& A, const TPair<FParticipantId, APlayerController*>& B) { return A.Key.Value < B.Key.Value; });
}

void UStealthCompositionRoot::HandleRosterEvent(const FRosterEvent& Event)
{
	if (Event.Type == ERosterEventType::ParticipantRemoved)
	{
		// A host remove (stealth.round.remove) of a connected player: drop the mapping and send them away.
		if (APlayerController* Player = GetController(Event.Id))
		{
			ParticipantByController.Remove(Player);
			Adapters->KickParticipant(Event.Id);
			Player->ClientReturnToMainMenuWithTextReason(NSLOCTEXT("Stealth", "Removed", "Removed by the host."));
		}
	}
	if (Event.Type == ERosterEventType::ConnectionChanged && Event.Connection == ERosterConnection::Connected)
	{
		// A (re)connection during Preparing re-enters the P5 wait set under a new generation: acknowledge again.
		const URoundMatchStore* Round = GetRoundStore();
		if (Round != nullptr && Round->GetPhase() == ERoundPhase::Preparing && Adapters != nullptr)
		{
			Adapters->AcknowledgeConnectedPlayers(/*bWorldReady*/ false);
		}
	}
	if (Event.Type == ERosterEventType::TeamsChanged || Event.Type == ERosterEventType::ParticipantRemoved)
	{
		MirrorTeamsToPrototype();
	}
}

void UStealthCompositionRoot::MirrorTeamsToPrototype()
{
	// Thin adapter: the prototype gameplay (HUD, objectives, assassination) reads AStealthPlayerState::Team. The Roster
	// is the source; this one-way mirror goes away with the prototype (ADR-0001 forbids team data on a PlayerState).
	const URosterStore* Roster = GetRosterStore();
	if (Roster == nullptr || !Roster->IsStoreInitialized())
	{
		return;
	}
	for (const TPair<TWeakObjectPtr<APlayerController>, FParticipantId>& Pair : ParticipantByController)
	{
		AStealthPlayerState* State = Pair.Key.IsValid() ? Pair.Key->GetPlayerState<AStealthPlayerState>() : nullptr;
		const FParticipantRecord* Record = Roster->FindRecord(Pair.Value);
		if (State != nullptr && Record != nullptr)
		{
			State->Team = Record->Team == ERosterTeam::None ? STEALTH_NO_TEAM : static_cast<uint8>(Record->Team);
		}
	}
}

FString UStealthCompositionRoot::DescribeProductionState(const UWorld* World) const
{
	const URosterStore* Roster = GetRosterStore();
	const URoundMatchStore* Round = GetRoundStore();
	if (Roster == nullptr || Round == nullptr || !Roster->IsMatchOpen() || Round->GetMatchCore() == nullptr)
	{
		return FString();
	}
	const FRoundPhaseCore& Phase = Round->GetMatchCore()->GetPhaseCore();
	TArray<FParticipantId> Ids;
	Roster->GetParticipantIds(Ids);
	const UStealthPawnFactory* Factory = UStealthPawnFactory::Get(World);
	return FString::Printf(TEXT("[prod] Round %s  idx %u  changes %u  played %u | Roster %d (T0 %d act %d inf %d / T1 %d act %d inf %d) epoch %u | factory pawns %d, untokened %d"),
		LexRoundPhase(Phase.GetPhase()), Phase.GetCurrentRoundIndex(), Phase.GetPhaseChangeCount(), Phase.GetRoundsPlayedCount(), Ids.Num(),
		Roster->CountRoster(0), Roster->CountActive(0), Roster->CountInformant(0), Roster->CountRoster(1), Roster->CountActive(1), Roster->CountInformant(1),
		Roster->GetEpochToken().RoundEpoch, Factory != nullptr ? Factory->GetSpawnCount() : 0, Factory != nullptr ? Factory->CountUntokenedPawns() : 0);
}
