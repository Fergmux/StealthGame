#include "Server/StealthCompositionRoot.h"

#include "Server/ActionHub.h"
#include "Server/AssassinationAttackPredicate.h"
#include "Server/AssassinationPreparePredicate.h"
#include "Server/EncounterService.h"
#include "Server/StealthPenaltyDueAdapter.h"
#include "Server/StealthPenaltyService.h"
#include "Server/StealthTuningSubstructs.h"
#include "StealthCommandWiring.h"
#include "StealthPenaltyWiring.h"

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
	const UWorld* World = GetGameWorld();
	const AStealthGameState* State = World != nullptr ? World->GetGameState<AStealthGameState>() : nullptr;
	// TeamStage is the stage being worked on (1, 2, then the final): the completed preliminary stages are one fewer.
	return State != nullptr && (Team == 0 || Team == 1) ? FMath::Clamp(State->GetTeamStage(static_cast<uint8>(Team)) - 1, 0, 2) : 0;
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
	}
	PrototypeFallback.Reset();
	PrototypeBushes.Reset();
	PrototypeBodyObserver.Reset();
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

	// Stand-ins of the composition root (StealthCommandWiring.h): the prototype bush query until Concealment, and the
	// Interact / Hide / Traverse dispatch until IIF stage 2. Tuning: the schema defaults until the TDC store feeds features.
	const FEncounterParams Params = FEncounterParams::Defaults();
	PrototypeBushes = MakeShared<FStealthPrototypeBushSpotQuery>(World, Params.Server.OpenSpotToleranceCm); // = BushSpotToleranceCm (W-ASN-9)
	PrototypeFallback = MakeShared<FStealthPrototypeCommandFallback>(InGameMode);
	PrototypeBodyObserver = MakeShared<FStealthPrototypeBodyRemovalObserver>(); // STAND-IN: Concealment's removal observer
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
	Init.Bushes = PrototypeBushes.Get();
	Init.Roster = GetRosterStore();
	// NKP story 005: the production Penalty service is the tally source (its exile sets bExiled); story nkp-021 retired the
	// prototype fallback, so a world with no Penalty subsystem answers every result with tally 0 and no exile.
	UPenaltyService* PenaltyService = World->GetSubsystem<UPenaltyService>();
	Init.Tally = PenaltyService;
	Init.BodyObserver = PrototypeBodyObserver.Get();
	Init.Faults = &CommandFaults;
	if (!Encounters->Initialize(Init))
	{
		UE_LOGF(LogStealth, Error, "Encounters not wired: the service refused its seams.");
		return;
	}
	// HoldTimer: Assassination's Prepare timers and its pose ends; no other owner submits hold timers to the queue yet
	// (the prototype objective keeps its own timer), so the fallback hold resolver is none.
	Encounters->SetFallbackResolvers(PrototypeFallback.Get(), nullptr);
	if (!Encounters->RegisterOn(*Queue))
	{
		UE_LOGF(LogStealth, Error, "Encounters: a PawnCommand / HoldTimer resolver or a DeathWrite / CounterPose applier was already registered.");
	}
	InGameMode->BindEncounterService(*Encounters);

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
}

void UStealthCompositionRoot::HandleRoundPhaseChanged(ERoundPhase OldPhase, ERoundPhase NewPhase)
{
	// Story asn-019 (AC-ASN-41): Round's Reset step clears the encounter round with it (Round story 058): roles Ready,
	// every defence stamp and owner cooldown none, pending writes forgotten, bodies torn down, body items voided.
	if (NewPhase == ERoundPhase::Reset && Encounters != nullptr)
	{
		Encounters->ResetRound();
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
}

void UStealthCompositionRoot::HandleRoundQueueReleasing()
{
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
