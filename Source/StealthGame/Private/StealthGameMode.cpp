#include "StealthGameMode.h"
#include "StealthBush.h"
#include "Server/ConcealmentService.h"
#include "View/BushRuntimeSubsystem.h"
#include "StealthCharacter.h"
#include "StealthCityGenerator.h"
#include "StealthGame.h"
#include "StealthGameState.h"
#include "StealthHUD.h"
#include "StealthNpcController.h"
#include "Server/ActionHub.h"
#include "Server/CrowdDirector.h"
#include "Server/EncounterService.h"
#include "Server/Core/StealthPawnCommandComponent.h"
#include "StealthPlayerController.h"
#include "StealthPlayerState.h"
#include "StealthRules.h"
#include "StealthSettings.h"
#include "Server/Core/Round/StealthRoundMatchStore.h"
#include "Server/Core/StealthPawnFactory.h"
#include "Server/Roster/StealthRosterStore.h"
#include "Server/StealthCompositionRoot.h"
#include "Server/StealthPenaltyHarnessSeams.h"
#include "Server/StealthPenaltyService.h"
#include "Server/ObjectivesService.h"

#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "Server/StealthGameSession.h"
#include "NavigationSystem.h"
#include "TimerManager.h"

#define LOCTEXT_NAMESPACE "StealthGameMode"

AStealthGameMode::AStealthGameMode()
{
	GameStateClass = AStealthGameState::StaticClass();
	PlayerStateClass = AStealthPlayerState::StaticClass();
	PlayerControllerClass = AStealthPlayerController::StaticClass();
	HUDClass = AStealthHUD::StaticClass();
	// ADR-0010: no engine default pawn path; every pawn comes from UStealthPawnFactory.
	DefaultPawnClass = nullptr;
	// ADR-0005 section 1 (story st-023): seamless round travel; players never start as spectators; the engine's session
	// approval never refuses (admission is Session's PreLogin gates).
	bUseSeamlessTravel = true;
	bStartPlayersAsSpectators = false;
	GameSessionClass = AStealthGameSession::StaticClass();
}

bool AStealthGameMode::PlayerCanRestart_Implementation(APlayerController* Player)
{
	// ADR-0005 / ADR-0010: no engine restart path; pawns come only from UStealthPawnFactory.
	return false;
}

void AStealthGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	CharacterClass = UStealthSettings::Get()->CharacterClass.LoadSynchronous();
	if (!CharacterClass)
	{
		UE_LOG(LogStealth, Error, TEXT("UStealthSettings::CharacterClass is not set or failed to load; falling back to AStealthCharacter (no sample animation)."));
		CharacterClass = AStealthCharacter::StaticClass();
	}

	// D53 composition: the production Roster and Round stores drive this world.
	if (UStealthPawnFactory* Factory = UStealthPawnFactory::Get(GetWorld()))
	{
		Factory->SetDefaultPawnClass(CharacterClass.Get());
	}
	if (UStealthCompositionRoot* Root = UStealthCompositionRoot::Get(GetWorld()))
	{
		Root->GetAdapters()->CrowdRadius = UStealthSettings::Get()->CrowdRadius;
		Root->OnServerWorldStarting(this);
	}
}

void AStealthGameMode::InitGameState()
{
	Super::InitGameState();
	if (URoundMatchStore* Round = URoundMatchStore::Get(GetWorld()))
	{
		Round->RegisterPublicStateSink(GetStealthGameState(), GetStealthGameState());
	}
}

void AStealthGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	// Intentionally empty: player pawns are created by UStealthPawnFactory at round start (ADR-0010).
}

void AStealthGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (URoundMatchStore* Round = URoundMatchStore::Get(GetWorld()))
	{
		Round->OnPhaseChanged.Remove(RoundPhaseHandle);
		Round->OnHostMessage.RemoveAll(this);
	}
	UnbindEncounters();
	Super::EndPlay(EndPlayReason);
}

void AStealthGameMode::BeginPlay()
{
	Super::BeginPlay();
	if (URoundMatchStore* Round = URoundMatchStore::Get(GetWorld()))
	{
		RoundPhaseHandle = Round->OnPhaseChanged.AddUObject(this, &AStealthGameMode::HandleRoundPhaseChanged);
		Round->OnHostMessage.AddWeakLambda(this, [](const FString& Message)
		{
			UE_LOGF(LogStealth, Log, "[Round host] %ls", *Message);
		});
	}
	if (UStealthCompositionRoot* Root = UStealthCompositionRoot::Get(GetWorld()))
	{
		Root->GetAdapters()->CrowdCenter = GetCrowdCenter();
	}
	TryScheduleRoundStart();

#if !UE_BUILD_SHIPPING
	// Development perf capture: -StealthPerfSeconds=N records an FPS chart for N seconds after the
	// round starts, then quits (summary in the log, CSV under Saved/Profiling/FPSChartStats).
	float PerfSeconds = 0.f;
	if (FParse::Value(FCommandLine::Get(), TEXT("StealthPerfSeconds="), PerfSeconds) && PerfSeconds > 0.f)
	{
		const float StartDelay = UStealthSettings::Get()->RoundStartDelay + 3.f;
		FTimerHandle StartHandle;
		GetWorldTimerManager().SetTimer(StartHandle, FTimerDelegate::CreateWeakLambda(this, [this, PerfSeconds]()
		{
			GEngine->Exec(GetWorld(), TEXT("startfpschart"));
			FTimerHandle StopHandle;
			GetWorldTimerManager().SetTimer(StopHandle, FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				GEngine->Exec(GetWorld(), TEXT("stopfpschart"));
				UE_LOG(LogStealth, Log, TEXT("Perf capture finished (%d NPCs)."), CrowdNpcs.Num());
				FPlatformMisc::RequestExit(false);
			}), PerfSeconds, false);
		}), StartDelay, false);
	}
#endif
}

AStealthGameState* AStealthGameMode::GetStealthGameState() const
{
	return GetGameState<AStealthGameState>();
}

APawn* AStealthGameMode::SpawnHarnessCrowdlessRespawnPawn(APlayerController* Player)
{
#if WITH_STEALTH_TEST_SEAMS
	return IsRoundInProgress() && StealthPenaltyHarnessSeams::IsCrowdlessTeamStartRespawn() ? SpawnPlayerPawn(Player) : nullptr;
#else
	return nullptr;
#endif
}

bool AStealthGameMode::IsRoundInProgress() const
{
	const URoundMatchStore* Round = URoundMatchStore::Get(GetWorld());
	return Round != nullptr && Round->IsRoundLive();
}

UClass* AStealthGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
	// ADR-0010: the engine's default pawn path is dead; the factory spawns every pawn.
	return nullptr;
}

// ---- Players ------------------------------------------------------------------------------------

void AStealthGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer); // no pawn: HandleStartingNewPlayer is a no-op

	AStealthPlayerState* PS = NewPlayer ? NewPlayer->GetPlayerState<AStealthPlayerState>() : nullptr;
	if (PS)
	{
		PS->ResetForRound();
	}
	// Production Roster registration; Round assigns the team in Lobby (R3) and the composition root mirrors it.
	UStealthCompositionRoot* Root = UStealthCompositionRoot::Get(GetWorld());
	const FParticipantId Id = Root ? Root->RegisterPlayer(NewPlayer) : FParticipantId();
	if (PS)
	{
		if (!Id.IsValid() || PS->Team == STEALTH_NO_TEAM)
		{
			PS->LifeState = EStealthLifeState::Informant;
			MessageController(NewPlayer, LOCTEXT("JoinedLate", "A round is in progress. You join the next one."), false, 8.f);
		}
		else
		{
			MessageController(NewPlayer, FText::Format(LOCTEXT("Joined", "You are {0}. Blend in. Watch for who doesn't."), GetTeamName(PS->Team)), false, 8.f);
		}
	}
	TryScheduleRoundStart();
}

void AStealthGameMode::Logout(AController* Exiting)
{
	if (UStealthCompositionRoot* Root = UStealthCompositionRoot::Get(GetWorld()))
	{
		Root->UnregisterPlayer(Cast<APlayerController>(Exiting));
	}
	Super::Logout(Exiting);
	// Production rules (R17, F2): the disconnect opens the grace record; its expiry is a forfeit or a void.
}

AActor* AStealthGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	const AStealthPlayerState* PS = Player ? Player->GetPlayerState<AStealthPlayerState>() : nullptr;
	if (AActor* Start = PS ? FindTeamStart(PS->Team) : nullptr)
	{
		return Start;
	}
	return Super::ChoosePlayerStart_Implementation(Player);
}

AActor* AStealthGameMode::FindTeamStart(uint8 Team) const
{
	if (Team < STEALTH_NUM_TEAMS)
	{
		const FName WantedTag(*FString::Printf(TEXT("Team%d"), Team));
		TArray<APlayerStart*> Candidates;
		for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
		{
			if (It->PlayerStartTag == WantedTag)
			{
				Candidates.Add(*It);
			}
		}
		// Prefer starts nobody is standing on, so teammates never spawn stacked.
		TArray<APlayerStart*> Free = Candidates.FilterByPredicate([this](const APlayerStart* Start)
		{
			for (TActorIterator<AStealthCharacter> It(GetWorld()); It; ++It)
			{
				if (!It->IsDying() && FVector::Dist2D(It->GetActorLocation(), Start->GetActorLocation()) < 90.f)
				{
					return false;
				}
			}
			return true;
		});
		const TArray<APlayerStart*>& Pool = Free.Num() > 0 ? Free : Candidates;
		if (Pool.Num() > 0)
		{
			return Pool[FMath::RandRange(0, Pool.Num() - 1)];
		}
	}
	return nullptr;
}

int32 AStealthGameMode::DrawAppearance()
{
	const int32 PoolSize = UStealthSettings::Get()->GetAppearancePoolSize();
	if (PoolSize <= 0)
	{
		return INDEX_NONE;
	}
	if (AppearanceDeck.Num() == 0)
	{
		for (int32 Index = 0; Index < PoolSize; ++Index)
		{
			AppearanceDeck.Add(Index);
		}
		for (int32 Index = AppearanceDeck.Num() - 1; Index > 0; --Index)
		{
			AppearanceDeck.Swap(Index, FMath::RandRange(0, Index));
		}
	}
	return AppearanceDeck.Pop(EAllowShrinking::No);
}

bool AStealthGameMode::ShouldSpawnAtStartSpot(AController* Player)
{
	// The engine caches a start spot at login, before the team is assigned; always pick a team start instead.
	return false;
}

void AStealthGameMode::SetPlayerDefaults(APawn* PlayerPawn)
{
	Super::SetPlayerDefaults(PlayerPawn);
	if (AStealthCharacter* Character = Cast<AStealthCharacter>(PlayerPawn))
	{
		// Players draw from exactly the same appearance pool as NPCs.
		Character->SetAppearanceIndex(DrawAppearance());
	}
}

// ---- Round lifecycle ---------------------------------------------------------------------------

void AStealthGameMode::TryScheduleRoundStart()
{
	AStealthGameState* GS = GetStealthGameState();
	const UStealthSettings* Settings = UStealthSettings::Get();
	const URoundMatchStore* Round = URoundMatchStore::Get(GetWorld());
	if (!GS || !Round || Round->GetPhase() != ERoundPhase::Lobby || !Settings->bAutoStartRound)
	{
		return;
	}
	if (GetNumPlayers() < Settings->MinPlayersToStart || GetWorldTimerManager().IsTimerActive(RoundStartTimer))
	{
		return;
	}
	GS->PhaseEndServerTime = GS->GetServerWorldTimeSeconds() + Settings->RoundStartDelay;
	GetWorldTimerManager().SetTimer(RoundStartTimer, this, &AStealthGameMode::RequestRoundStart, FMath::Max(0.01f, Settings->RoundStartDelay), false);
}

void AStealthGameMode::RequestRoundStart()
{
	GetWorldTimerManager().ClearTimer(RoundStartTimer);
	URoundMatchStore* Round = URoundMatchStore::Get(GetWorld());
	if (!Round)
	{
		return;
	}
	// The dev auto-start acts as the host (the listen host's participant) through the one host-checked entry point.
	const ERoundCommandResult Result = Round->ExecuteCommand(FRoundCommandCaller::ServerAsHost(Round->GetHostParticipant()), ERoundHostCommand::Start);
	if (Result != ERoundCommandResult::Executed)
	{
		UE_LOGF(LogStealth, Log, "Round start not accepted (%d); waiting for a valid lobby.", static_cast<int32>(Result));
	}
}

void AStealthGameMode::RequestRoundReset()
{
	GetWorldTimerManager().ClearTimer(RoundRestartTimer);
	if (URoundMatchStore* Round = URoundMatchStore::Get(GetWorld()))
	{
		Round->ExecuteCommand(FRoundCommandCaller::ServerAsHost(Round->GetHostParticipant()), ERoundHostCommand::Reset);
	}
}

void AStealthGameMode::HandleRoundPhaseChanged(ERoundPhase OldPhase, ERoundPhase NewPhase)
{
	switch (NewPhase)
	{
	case ERoundPhase::Preparing:
		RunPreparing();
		break;
	case ERoundPhase::Active:
		HandleRoundActive();
		break;
	case ERoundPhase::Ended:
		HandleRoundEnded();
		break;
	case ERoundPhase::Reset:
		if (OldPhase == ERoundPhase::Preparing || OldPhase == ERoundPhase::Active)
		{
			HandleRoundCalledOff();
		}
		else
		{
			CleanupRoundActors();
		}
		break;
	case ERoundPhase::Lobby:
	{
		if (AStealthGameState* GS = GetStealthGameState())
		{
			GS->RoundPhase = EStealthRoundPhase::WaitingToStart;
		}
		// Players who joined during the round register now (Lobby admits new keys again).
		if (UStealthCompositionRoot* Root = UStealthCompositionRoot::Get(GetWorld()))
		{
			for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
			{
				APlayerController* PC = It->Get();
				if (PC && !Root->GetParticipant(PC).IsValid())
				{
					Root->RegisterPlayer(PC);
				}
			}
		}
		TryScheduleRoundStart();
		break;
	}
	default:
		break;
	}
}

APawn* AStealthGameMode::SpawnPlayerPawn(APlayerController* Player)
{
	UStealthPawnFactory* Factory = UStealthPawnFactory::Get(GetWorld());
	const UStealthCompositionRoot* Root = UStealthCompositionRoot::Get(GetWorld());
	const URosterStore* Roster = URosterStore::Get(GetWorld());
	if (!Factory || !Player || !CharacterClass)
	{
		return nullptr;
	}
	const AActor* Start = ChoosePlayerStart(Player);
	FTransform Transform = Start ? Start->GetActorTransform() : FTransform::Identity;
	Transform.SetScale3D(FVector::OneVector);

	FStealthSpawnRequest Request;
	Request.PawnClass = CharacterClass.Get();
	Request.Transform = Transform;
	Request.AppearanceId = DrawAppearance();
	Request.ControlKind = EStealthPawnControlKind::Player;
	Request.PlayerController = Player;
	Request.Participant = Root ? Root->GetParticipant(Player) : FParticipantId();
	if (Roster && Roster->IsMatchOpen())
	{
		Request.ExpectedEpoch = Roster->GetEpochToken();
	}
	const FStealthSpawnResult Result = Factory->SpawnSharedPawn(Request);
	if (Result.Pawn && Request.Participant.IsValid() && Result.Bind != ERosterResult::Applied)
	{
		UE_LOGF(LogStealth, Warning, "Player pawn spawned but not bound to its participant (result %d).", static_cast<int32>(Result.Bind));
	}
	return Result.Pawn;
}

void AStealthGameMode::RunPreparing()
{
	AStealthGameState* GS = GetStealthGameState();
	if (!GS)
	{
		return;
	}
	GetWorldTimerManager().ClearTimer(RoundStartTimer);
	GetWorldTimerManager().ClearTimer(RoundRestartTimer);

	// The previous round's actors go (its start pawns too, Ai-driven bodies included); Round's readiness sequence (R6)
	// then builds the layout and spawns every pawn.
	DestroyRoundPawns();
	AppearanceDeck.Reset();
	URosterStore* Roster = URosterStore::Get(GetWorld());
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC)
		{
			continue;
		}
		if (AStealthPlayerState* PS = PC->GetPlayerState<AStealthPlayerState>())
		{
			PS->ResetForRound();
			if (PS->Team == STEALTH_NO_TEAM)
			{
				PS->LifeState = EStealthLifeState::Informant;
			}
		}
		if (APawn* Pawn = PC->GetPawn())
		{
			if (Roster && Roster->IsMatchOpen())
			{
				Roster->ClearBinding(Pawn, EEndPlayReason::RemovedFromWorld);
			}
			PC->UnPossess();
			Pawn->Destroy();
		}
	}
	GS->RoundPhase = EStealthRoundPhase::WaitingToStart;
	GS->PhaseEndServerTime = 0.0;
}

void AStealthGameMode::BuildRoundLayout(int32 Seed)
{
	AStealthGameState* GS = GetStealthGameState();
	if (!GS)
	{
		if (URoundMatchStore* Round = URoundMatchStore::Get(GetWorld()))
		{
			Round->NotifyLayoutAborted();
		}
		return;
	}
	// The round seed is Session's draw for this start (FRoundParams.Seed, R5 step 8); the layout must report it back (P4).
	PendingLayoutSeed = Seed;
	GS->ResetRoundState(Seed);
	GS->BuildCityForSeed(); // clients build the same layout when RoundSeed replicates
	NavWaitStartTime = GetWorld()->GetTimeSeconds();
	GetWorldTimerManager().SetTimer(CrowdSpawnTimer, this, &AStealthGameMode::PollLayoutNavReady, 0.25f, true, 0.25f);
}

void AStealthGameMode::PollLayoutNavReady()
{
	// The new layout dirties the nav mesh; the start placement needs the rebuilt mesh to find streets.
	constexpr double MinNavWaitSeconds = 1.5; // the rebuild can start a few frames after the layout changes
	constexpr double MaxNavWaitSeconds = 15.0;
	UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	const double Waited = GetWorld()->GetTimeSeconds() - NavWaitStartTime;
	const bool bNavBusy = NavSystem && NavSystem->IsNavigationBuildInProgress();
	if ((bNavBusy || Waited < MinNavWaitSeconds) && Waited < MaxNavWaitSeconds)
	{
		return;
	}
	GetWorldTimerManager().ClearTimer(CrowdSpawnTimer);
	for (TActorIterator<AStealthCityGenerator> It(GetWorld()); It; ++It)
	{
		It->ValidateReachability();
	}
	UE_LOG(LogStealth, Log, TEXT("Layout ready (seed %d, nav wait %.1fs)."), PendingLayoutSeed, Waited);
	// Concealment story con-002 (AC-CON-27): a layout whose BushSites fail V-BUSH-1 never reaches a round.
	if (const UBushRuntimeSubsystem* Bushes = UBushRuntimeSubsystem::Get(GetWorld()); Bushes != nullptr && Bushes->GetLastReject() != EBushSiteReject::None)
	{
		UE_LOG(LogStealth, Error, TEXT("Layout rejected: a BushSite fails V-BUSH-1 (seed %d)."), PendingLayoutSeed);
		if (URoundMatchStore* Round = URoundMatchStore::Get(GetWorld()))
		{
			Round->NotifyLayoutAborted();
		}
		return;
	}
	// Objectives story oc-005: OnLayoutReady binds the chain to the (stand-in) site assignment before Round hears of the layout.
	if (UStealthCompositionRoot* Root = UStealthCompositionRoot::Get(GetWorld()))
	{
		Root->HandleObjectivesLayoutReady(1, PendingLayoutSeed);
	}
	if (URoundMatchStore* Round = URoundMatchStore::Get(GetWorld()))
	{
		Round->NotifyLayoutReady(1, PendingLayoutSeed);
	}
}

ERoundSpawnResult AStealthGameMode::SpawnStartPawn(const FRoundSpawnEntry& Entry, const FEpochToken& ExpectedEpoch)
{
	if (!Entry.IsPlayer())
	{
		return SpawnNpc() != nullptr ? ERoundSpawnResult::Spawned : ERoundSpawnResult::Failed;
	}
	UStealthPawnFactory* Factory = UStealthPawnFactory::Get(GetWorld());
	const UStealthCompositionRoot* Root = UStealthCompositionRoot::Get(GetWorld());
	if (!Factory || !Root || !CharacterClass)
	{
		return ERoundSpawnResult::Failed;
	}
	APlayerController* PC = Root->GetController(Entry.Participant);
	const uint8 Team = Entry.Team == ERosterTeam::Team1 ? 1 : 0;
	const AActor* Start = FindTeamStart(Team);
	FTransform Transform = Start ? Start->GetActorTransform() : FTransform::Identity;
	Transform.SetScale3D(FVector::OneVector);

	FStealthSpawnRequest Request;
	Request.PawnClass = CharacterClass.Get();
	Request.Transform = Transform;
	Request.AppearanceId = DrawAppearance();
	// The spawn gate: a participant not Connected at P6 gets an Ai-driven, unpossessed pawn (never Player).
	const bool bPlayer = Entry.Control == ERoundSpawnControl::Player && PC != nullptr;
	Request.ControlKind = bPlayer ? EStealthPawnControlKind::Player : EStealthPawnControlKind::Ai;
	Request.PlayerController = bPlayer ? PC : nullptr;
	Request.Participant = Entry.Participant;
	Request.ExpectedEpoch = FRosterEpochToken{ ExpectedEpoch.MatchId, ExpectedEpoch.RoundEpoch };
	const FStealthSpawnResult Result = Factory->SpawnSharedPawn(Request);
	if (!Result.Pawn)
	{
		return Result.Bind != ERosterResult::Applied && Result.Bind != ERosterResult::NoOp_NotBound ? ERoundSpawnResult::BindFailed : ERoundSpawnResult::Failed;
	}
	RoundPlayerPawns.Add(Entry.Participant.Value, Result.Pawn);
	return ERoundSpawnResult::Spawned;
}

bool AStealthGameMode::OpenSpawnGate(FParticipantId Participant)
{
	const UStealthCompositionRoot* Root = UStealthCompositionRoot::Get(GetWorld());
	APlayerController* PC = Root ? Root->GetController(Participant) : nullptr;
	const TWeakObjectPtr<APawn>* Gated = RoundPlayerPawns.Find(Participant.Value);
	if (!PC || !Gated || !Gated->IsValid())
	{
		return false;
	}
	// Prototype hand-back until the takeover stories (ADR-0010 RequestHandOver): the Ai body is replaced by a possessed one.
	APawn* Old = Gated->Get();
	if (URosterStore* Roster = URosterStore::Get(GetWorld()); Roster && Roster->IsMatchOpen())
	{
		Roster->ClearBinding(Old, EEndPlayReason::RemovedFromWorld);
	}
	if (AController* Ai = Old->GetController())
	{
		Ai->UnPossess();
		Ai->Destroy();
	}
	Old->Destroy();
	if (APawn* Pawn = SpawnPlayerPawn(PC))
	{
		RoundPlayerPawns.Add(Participant.Value, Pawn);
	}
	return true;
}

int32 AStealthGameMode::DestroyRoundPawns()
{
	int32 Destroyed = CrowdNpcs.Num();
	CleanupRoundActors();
	URosterStore* Roster = URosterStore::Get(GetWorld());
	for (const TPair<uint32, TWeakObjectPtr<APawn>>& Pair : RoundPlayerPawns)
	{
		APawn* Pawn = Pair.Value.Get();
		if (!Pawn)
		{
			continue;
		}
		if (Roster && Roster->IsMatchOpen())
		{
			Roster->ClearBinding(Pawn, EEndPlayReason::RemovedFromWorld);
		}
		if (AController* Controller = Pawn->GetController())
		{
			Controller->UnPossess();
			if (!Controller->IsPlayerController())
			{
				Controller->Destroy();
			}
		}
		Pawn->Destroy();
		++Destroyed;
	}
	RoundPlayerPawns.Reset();
	return Destroyed;
}

void AStealthGameMode::HandleRoundActive()
{
	if (AStealthGameState* GS = GetStealthGameState())
	{
		GS->RoundPhase = EStealthRoundPhase::InProgress;
		GS->PhaseEndServerTime = 0.0;
	}
	MessageAll(LOCTEXT("RoundStart", "The round begins. Complete your team's tasks. Trust no one in the crowd."), false, 6.f);
	UE_LOG(LogStealth, Log, TEXT("Round started (seed %d, %d NPCs)."), GetStealthGameState() ? GetStealthGameState()->RoundSeed : 0, CrowdNpcs.Num());
}

void AStealthGameMode::HandleRoundCalledOff()
{
	AStealthGameState* GS = GetStealthGameState();
	GetWorldTimerManager().ClearTimer(RoundRestartTimer);
	CleanupRoundActors();
	if (GS)
	{
		GS->RoundPhase = EStealthRoundPhase::WaitingToStart;
		GS->ResetRoundState(GS->RoundSeed);
	}
	MessageAll(LOCTEXT("RoundAborted", "The round was called off."));
}

void AStealthGameMode::HandleRoundEnded()
{
	AStealthGameState* GS = GetStealthGameState();
	const URoundMatchStore* Round = URoundMatchStore::Get(GetWorld());
	if (!GS || !Round || !Round->GetMatchCore())
	{
		return;
	}
	// Round's production rules decided the round (R15 latch); this is only the prototype presentation of FRoundResult.
	const FRoundResult& Result = Round->GetMatchCore()->GetPhaseCore().GetLastResult();
	const UStealthSettings* Settings = UStealthSettings::Get();
	const uint8 WinningTeam = Result.WinningTeam;
	const EStealthWinReason Reason = Result.PublicReason == 1 ? EStealthWinReason::FinalObjective : EStealthWinReason::Elimination;
	GS->RoundPhase = EStealthRoundPhase::RoundOver;
	GS->WinningTeam = WinningTeam;
	GS->WinReason = Reason;
	GS->PhaseEndServerTime = GS->GetServerWorldTimeSeconds() + Settings->RoundRestartDelay;

	// The approved reason phrasing (HUD-E18; a forfeit shows the elimination text) and the R9 TeamSummary lines (HUD-E21).
	const FText ReasonText = GetRoundEndReasonText(Result.PublicReason);
	const auto SummaryLine = [&Result, this](uint8 Team)
	{
		const FRoundTeamSummary& Summary = Result.TeamSummary[Team];
		return FText::Format(LOCTEXT("RoundSummaryLine", "{0}: steps {1} of 3, eliminated {2}, wrong targets {3}"), GetTeamName(Team),
			FText::AsNumber(Summary.StepsCompleted), FText::AsNumber(Summary.EliminatedCount), FText::AsNumber(Summary.WrongTargetCount));
	};
	MessageAll(FText::Format(LOCTEXT("RoundWon", "{0} win: {1}.\n{2}\n{3}"), GetTeamName(WinningTeam), ReasonText, SummaryLine(0), SummaryLine(1)), false, Settings->RoundRestartDelay);
	UE_LOG(LogStealth, Log, TEXT("Round over: team %d wins (%s)."), WinningTeam, Reason == EStealthWinReason::FinalObjective ? TEXT("objective") : TEXT("elimination"));

	GetWorldTimerManager().SetTimer(RoundRestartTimer, this, &AStealthGameMode::RequestRoundReset, Settings->RoundRestartDelay, false);
}

void AStealthGameMode::CleanupRoundActors()
{
	GetWorldTimerManager().ClearTimer(CrowdSpawnTimer);
	for (FTimerHandle& Handle : PendingTimers)
	{
		GetWorldTimerManager().ClearTimer(Handle);
	}
	PendingTimers.Reset();

	for (AStealthCharacter* Npc : CrowdNpcs)
	{
		if (IsValid(Npc))
		{
			if (AController* Controller = Npc->GetController())
			{
				Controller->Destroy();
			}
			Npc->Destroy();
		}
	}
	CrowdNpcs.Reset();

	// Bodies left from the previous round.
	for (TActorIterator<AStealthCharacter> It(GetWorld()); It; ++It)
	{
		if (It->IsDying())
		{
			It->Destroy();
		}
	}
}

// ---- Crowd --------------------------------------------------------------------------------------

namespace
{
	/** The graybox plaza the crowd's density model types as Market (AStealthCityGenerator::PlazaRadius default). */
	constexpr float StealthCrowdMarketRadiusCm = 1300.f;
}

FVector AStealthGameMode::GetCrowdCenter() const
{
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->ActorHasTag(TEXT("CrowdCenter")))
		{
			return It->GetActorLocation();
		}
	}
	return FVector::ZeroVector;
}

bool AStealthGameMode::FindCrowdPoint(FVector& OutLocation) const
{
	UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!NavSystem)
	{
		return false;
	}
	FNavLocation Point;
	if (NavSystem->GetRandomReachablePointInRadius(GetCrowdCenter(), UStealthSettings::Get()->CrowdRadius, Point))
	{
		OutLocation = Point.Location;
		return true;
	}
	return false;
}

AStealthCharacter* AStealthGameMode::SpawnNpc()
{
	UStealthPawnFactory* Factory = UStealthPawnFactory::Get(GetWorld());
	if (!CharacterClass || !Factory)
	{
		return nullptr;
	}
	// Start placement (crowd story cnb-017): the Director's F1 placement on the NpcRoutes stream for the round's start
	// list; the prototype FindCrowdPoint only for a mid-round prototype replacement (stories cnb-026/027 replace it).
	FTransform Transform;
	UCrowdDirector* Director = UCrowdDirector::Get(GetWorld());
	if (Director != nullptr)
	{
		Director->SetCrowdArea(GetCrowdCenter(), UStealthSettings::Get()->CrowdRadius, StealthCrowdMarketRadiusCm);
	}
	const bool bRoundStart = !IsRoundInProgress();
	// The list holds half again as many placements as NPCs: a placement whose spawn collides is skipped (the cursor moves
	// on), so a blocked node never stalls the rest of the crowd.
	const int32 Wanted = FMath::Max(UStealthSettings::Get()->NpcCount, 1);
	if (!(bRoundStart && Director != nullptr && Director->RequestStartPlacement(StartPlacementCursor++, Wanted + Wanted / 2, Transform)))
	{
		FVector Location;
		if (!FindCrowdPoint(Location))
		{
			return nullptr;
		}
		const AStealthCharacter* Defaults = CharacterClass->GetDefaultObject<AStealthCharacter>();
		Location.Z += Defaults->GetDefaultHalfHeight() + 5.f;
		Transform = FTransform(FRotator(0.f, FMath::FRandRange(-180.f, 180.f), 0.f), Location);
	}

	// Through the single spawn site; the factory hooks attach the crowd controller.
	FStealthSpawnRequest Request;
	Request.PawnClass = CharacterClass.Get();
	Request.Transform = Transform;
	Request.AppearanceId = DrawAppearance();
	Request.ControlKind = EStealthPawnControlKind::Ai;
	AStealthCharacter* Npc = Cast<AStealthCharacter>(Factory->SpawnSharedPawn(Request).Pawn);
	if (Npc)
	{
		CrowdNpcs.Add(Npc);
	}
	return Npc;
}

void AStealthGameMode::SpawnCrowd()
{
	const int32 Wanted = UStealthSettings::Get()->NpcCount;
	if (CrowdNpcs.Num() == 0)
	{
		StartPlacementCursor = 0;
	}
	int32 Attempts = 0;
	while (CrowdNpcs.Num() < Wanted && Attempts < Wanted * 4)
	{
		++Attempts;
		SpawnNpc();
	}
	if (CrowdNpcs.Num() < Wanted)
	{
		UE_LOG(LogStealth, Warning, TEXT("Spawned %d of %d NPCs. Is there a NavMeshBoundsVolume covering the crowd area?"), CrowdNpcs.Num(), Wanted);
	}
}

void AStealthGameMode::ScheduleNpcReplacement()
{
	FTimerHandle& Handle = PendingTimers.AddDefaulted_GetRef();
	GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		if (IsRoundInProgress())
		{
			CrowdNpcs.RemoveAll([](const TObjectPtr<AStealthCharacter>& Npc) { return !IsValid(Npc) || Npc->IsDying(); });
			SpawnNpc();
		}
	}), FMath::Max(0.1f, UStealthSettings::Get()->NpcReplacementDelay), false);
}

// ---- Assassination ------------------------------------------------------------------------------

void AStealthGameMode::BindEncounterService(UEncounterService& Service)
{
	UnbindEncounters();
	BoundEncounters = &Service;
	if (FOnPawnKilled* Killed = Service.GetOnPawnKilled())
	{
		PawnKilledHandle = Killed->AddUObject(this, &AStealthGameMode::HandleEncounterPawnKilled);
	}
	if (FOnDeathWriteApplied* Death = Service.GetOnDeathWriteApplied())
	{
		DeathWriteHandle = Death->AddUObject(this, &AStealthGameMode::HandleEncounterDeathWrite);
	}
	if (FOnEncounterBodyReleased* Released = Service.GetOnBodyReleased())
	{
		BodyReleasedHandle = Released->AddUObject(this, &AStealthGameMode::HandleEncounterBodyReleased);
	}
	if (UActionHub* Hub = UActionHub::Get(GetWorld()))
	{
		HubStartedHandle = Hub->OnActionStarted.AddUObject(this, &AStealthGameMode::HandleHubActionStarted);
	}
}

void AStealthGameMode::UnbindEncounters()
{
	if (UEncounterService* Service = BoundEncounters.Get())
	{
		if (FOnPawnKilled* Killed = Service->GetOnPawnKilled())
		{
			Killed->Remove(PawnKilledHandle);
		}
		if (FOnDeathWriteApplied* Death = Service->GetOnDeathWriteApplied())
		{
			Death->Remove(DeathWriteHandle);
		}
		if (FOnEncounterBodyReleased* Released = Service->GetOnBodyReleased())
		{
			Released->Remove(BodyReleasedHandle);
		}
	}
	if (UActionHub* Hub = UActionHub::Get(GetWorld()))
	{
		Hub->OnActionStarted.Remove(HubStartedHandle);
	}
	BoundEncounters.Reset();
	PendingEncounterKills.Reset();
}

void AStealthGameMode::HandleEncounterPawnKilled(APawn* Victim, APawn* Killer, EVictimKind Kind, EKillCause Cause)
{
	// Inside the Attack's slot, after the production Roster transition (Assassinated / ReversedAgainst): only server
	// state here. Everything a client sees waits for the death write (ADR-0018 section 3.4).
	PendingEncounterKills.Add(Victim, FPendingEncounterKill{ Killer, Kind, Cause });
	UE_LOG(LogStealth, Log, TEXT("[Encounter] %s %s %s in the slot (victim kind %d)."), Killer != nullptr ? *Killer->GetName() : TEXT("?"),
		Cause == EKillCause::Reversal ? TEXT("reversed and eliminated") : TEXT("killed"), Victim != nullptr ? *Victim->GetName() : TEXT("?"), static_cast<int32>(Kind));

	// NPC-Kill Penalty: the production UPenaltyService counts the strike, answers the tally and runs the exile in this same
	// fan-out (stories nkp-001..008; the prototype penalty was retired by story nkp-021).
}

void AStealthGameMode::HandleEncounterDeathWrite(APawn* Victim, double DeathServerTime, bool bDiedConcealed)
{
	AStealthCharacter* Body = Cast<AStealthCharacter>(Victim);
	FPendingEncounterKill Kill{ nullptr, EVictimKind::Npc, EKillCause::Strike };
	PendingEncounterKills.RemoveAndCopyValue(Victim, Kill);
	UE_LOG(LogStealth, Log, TEXT("[Encounter] death write applied on %s at %.3f (DeathServerTime %.3f, concealed %d)."), Victim != nullptr ? *Victim->GetName() : TEXT("?"),
		GetWorld()->GetTimeSeconds(), DeathServerTime, bDiedConcealed ? 1 : 0);
	if (Body == nullptr)
	{
		return;
	}
	// The attacker's and the defender's owner results travel as FStrikeResult / FDefenceResult entries of their own
	// ClientCommandReplies (story asn-015; the plates are the controller's stand-in until story asn-024). The fallen
	// player's own removal message is its client's, at its OnRoleChangePresented dispatch (story asn-021), never sent here.

	// A player's body (Active or informant): the prototype life state. The Roster transition already ran in the slot; the
	// informant respawn waits for the body's release (story asn-019, HandleEncounterBodyReleased).
	if (AStealthPlayerState* PS = Body->GetStealthPlayerState())
	{
		PS->LifeState = EStealthLifeState::Informant;
	}
	else
	{
		CrowdNpcs.Remove(Body);
		ScheduleNpcReplacement();
	}
	// The fall is the encounter presentation's (Death_Fall from FDeathState on every machine, stories asn-018 / asn-020);
	// the body keeps its controller and capsule here: the release and removal are the queue's BodyRelease / BodyRemoval
	// items (story asn-019). AStealthCharacter::IsDying() reads the same FDeathState (story asn-027 retired the prototype
	// replicated dying flag).
}

void AStealthGameMode::HandleEncounterBodyReleased(APawn* Body, AController* FormerController)
{
	UE_LOG(LogStealth, Log, TEXT("[Encounter] body %s released at %.3f (controller %s)."), Body != nullptr ? *Body->GetName() : TEXT("?"), GetWorld()->GetTimeSeconds(),
		FormerController != nullptr ? *FormerController->GetName() : TEXT("none"));
	// NPC-Kill Penalty stories nkp-009 / nkp-012: the production service owns every informant respawn (the InformantRespawn
	// item at F2's T_due and the takeover of an unseen crowd pawn); the prototype respawn was retired by story nkp-021.
}

void AStealthGameMode::HandleHubActionStarted(AStealthPawn* Pawn, EActionType Type)
{
	// The brace tell is the pawn's presentation of its FActionState Brace pose (AStealthCharacter::HandleActionStateChanged,
	// story asn-027); only the prototype concealment rule "a strike from a bush reveals the attacker" stays here.
	// Concealment epic: an Attack from inside a bush breaks concealment in UConcealmentService (story con-014).
	(void)Pawn;
	(void)Type;
}

void AStealthGameMode::HandleProductionExile(AStealthCharacter* Body, AController* FormerController)
{
	AStealthPlayerState* PS = FormerController != nullptr ? FormerController->GetPlayerState<AStealthPlayerState>() : nullptr;
	if (PS == nullptr)
	{
		return;
	}
	// The Roster transition, the release, the card, the crowd adoption and the hand-over commit already ran in the slot
	// (UPenaltyService); only the prototype's own mirrors follow here.
	PS->LifeState = EStealthLifeState::Informant;
	// The respawn is Penalty's (stories nkp-009 / nkp-012): Round's OnInformantAwaitingBody(StrikeLimit) schedules the
	// InformantRespawn item and its commit takes over an unseen crowd pawn. The exiled body stays a crowd NPC.
}

// ---- Communication --------------------------------------------------------------------------------

void AStealthGameMode::HandlePing(AStealthCharacter* Sender, const FVector& Location)
{
	AStealthPlayerState* PS = Sender ? Sender->GetStealthPlayerState() : nullptr;
	AStealthGameState* GS = GetStealthGameState();
	if (!PS || !GS || PS->Team >= STEALTH_NUM_TEAMS)
	{
		return;
	}
	// D26 / D33 (ADR-0011 PNG-R5 step 2): a pawn in a bush or any hide transition cannot ping (Emerging included); the
	// drop is silent on the server.
	if (const UConcealmentService* Concealment = UConcealmentService::Get(GetWorld()); Concealment != nullptr && Concealment->IsInBushOrTransition(Sender))
	{
		return;
	}
	const double Now = GS->GetServerWorldTimeSeconds();
	if (Now - PS->LastPingTime < UStealthSettings::Get()->PingCooldown * 0.8f)
	{
		return;
	}
	PS->LastPingTime = Now;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		AStealthPlayerController* PC = Cast<AStealthPlayerController>(It->Get());
		const AStealthPlayerState* OtherPS = PC ? PC->GetPlayerState<AStealthPlayerState>() : nullptr;
		if (OtherPS && OtherPS->Team == PS->Team)
		{
			PC->ClientReceivePing(Location, PS->GetPlayerName());
		}
	}
}

void AStealthGameMode::MessageController(AController* Controller, const FText& Text, bool bDanger, float Duration) const
{
	if (AStealthPlayerController* PC = Cast<AStealthPlayerController>(Controller))
	{
		PC->ClientStealthMessage(Text, bDanger, Duration);
	}
}

void AStealthGameMode::MessagePlayer(const AStealthCharacter* Character, const FText& Text, bool bDanger, float Duration) const
{
	if (Character)
	{
		MessageController(Character->GetController(), Text, bDanger, Duration);
	}
}

void AStealthGameMode::MessageTeam(uint8 Team, const FText& Text, bool bDanger, float Duration) const
{
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const AStealthPlayerState* PS = It->Get() ? It->Get()->GetPlayerState<AStealthPlayerState>() : nullptr;
		if (PS && PS->Team == Team)
		{
			MessageController(It->Get(), Text, bDanger, Duration);
		}
	}
}

void AStealthGameMode::MessageAll(const FText& Text, bool bDanger, float Duration) const
{
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		MessageController(It->Get(), Text, bDanger, Duration);
	}
}

// ---- Development test driver -----------------------------------------------------------------------

void AStealthGameMode::DebugBotAction(int32 PlayerIndex, const FString& Action)
{
#if !UE_BUILD_SHIPPING
	AController* Controller = nullptr;
	int32 Index = 0;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It, ++Index)
	{
		if (Index == PlayerIndex)
		{
			Controller = It->Get();
			break;
		}
	}
	AStealthCharacter* Pawn = Controller ? Cast<AStealthCharacter>(Controller->GetPawn()) : nullptr;
	const AStealthGameState* GS = GetStealthGameState();

	if (Action.Equals(TEXT("report"), ESearchCase::IgnoreCase))
	{
		UE_LOG(LogStealth, Log, TEXT("[Bot] Phase=%d Winner=%d NPCs=%d"), (int32)GS->RoundPhase, GS->WinningTeam, CrowdNpcs.Num());
		if (const UObjectivesService* Objectives = UObjectivesService::Get(GetWorld()))
		{
			UE_LOG(LogStealth, Log, TEXT("[Bot] Objectives: bound=%d team0 stage=%d prelims=%d, team1 stage=%d prelims=%d"), Objectives->IsBound() ? 1 : 0,
				static_cast<int32>(Objectives->GetObjectiveTeamStage(0)), Objectives->GetCompletedPreliminaryStages(0),
				static_cast<int32>(Objectives->GetObjectiveTeamStage(1)), Objectives->GetCompletedPreliminaryStages(1));
		}
		if (const UStealthCompositionRoot* Root = UStealthCompositionRoot::Get(GetWorld()))
		{
			UE_LOG(LogStealth, Log, TEXT("[Bot] %s"), *Root->DescribeProductionState(GetWorld()));
		}
		Index = 0;
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It, ++Index)
		{
			const AStealthPlayerState* PS = It->Get() ? It->Get()->GetPlayerState<AStealthPlayerState>() : nullptr;
			const AStealthCharacter* P = It->Get() ? Cast<AStealthCharacter>(It->Get()->GetPawn()) : nullptr;
			if (PS)
			{
				UE_LOG(LogStealth, Log, TEXT("[Bot] #%d %s team=%d state=%s pawn=%s braced=%d concealed=%d"), Index, *PS->GetPlayerName(), PS->Team,
					PS->IsInformant() ? TEXT("Informant") : TEXT("Active"), P ? *P->GetActorLocation().ToCompactString() : TEXT("none"),
					P ? (P->GetActionState().Pose == EActionPose::Brace ? 1 : 0) : 0, P ? P->IsConcealed() : 0);
			}
		}
		return;
	}

	if (!Pawn)
	{
		UE_LOG(LogStealth, Warning, TEXT("[Bot] Player %d has no pawn."), PlayerIndex);
		return;
	}

	// Bot commands carry this controller's own origin (mailbox, reply sink, recorded control epoch), as ingress would for
	// its client, so the owner results of story asn-015 reach that connection.
	const auto AsOwner = [this, Controller, Pawn](TFunctionRef<void(UPawnCommandComponent&)> Submit)
	{
		UPawnCommandComponent* Command = Pawn->GetPawnCommand();
		AStealthPlayerController* OwnerController = Cast<AStealthPlayerController>(Controller);
		if (Command == nullptr)
		{
			return;
		}
		if (OwnerController == nullptr)
		{
			Submit(*Command);
			return;
		}
		const UStealthCompositionRoot* Root = UStealthCompositionRoot::Get(GetWorld());
		const FParticipantId Participant = Root ? Root->GetParticipant(OwnerController) : FParticipantId();
		FCommandOrigin Origin;
		Origin.Mailbox = Participant.IsValid() ? FMailboxId::ForParticipant(Participant) : FMailboxId::ServerLocal();
		Origin.Sink = OwnerController->GetReplySink();
		FClientCommandSeq Seq;
		Seq.PossessionEpoch = OwnerController->GetServerPossessionEpoch();
		Seq.Seq = ++BotCommandSeq;
		FScopedCommandOrigin Scope(*Command, Origin, Seq, OwnerController->GetPossessedControlEpoch());
		Submit(*Command);
	};

	auto FaceAndApproach = [Pawn](const AActor* Target, float Distance)
	{
		const FVector Away = (Pawn->GetActorLocation() - Target->GetActorLocation()).GetSafeNormal2D();
		const FVector Dir = Away.IsNearlyZero() ? FVector(1.f, 0.f, 0.f) : Away;
		const FVector Spot = Target->GetActorLocation() + Dir * Distance;
		Pawn->TeleportTo(FVector(Spot.X, Spot.Y, Pawn->GetActorLocation().Z), (-Dir).Rotation());
		if (AController* C = Pawn->GetController())
		{
			// Look slightly down at the target; owning clients get the rotation too (they own control rotation).
			const FRotator Facing(-12.f, (-Dir).Rotation().Yaw, 0.f);
			C->SetControlRotation(Facing);
			if (APlayerController* PC = Cast<APlayerController>(C))
			{
				PC->ClientSetRotation(Facing, true);
			}
		}
	};

	if (Action.StartsWith(TEXT("strikespot"), ESearchCase::IgnoreCase))
	{
		// "strikespot [distance cm] [bearing deg]": an Attack at the open spot at that distance and bearing from the body
		// facing, where the pawn stands (no approach): the clip-variant evidence (a whiff picks its variant by the WorldPoint).
		TArray<FString> SpotParts;
		Action.RightChop(10).ParseIntoArrayWS(SpotParts);
		const float SpotDistance = SpotParts.Num() > 0 ? FCString::Atof(*SpotParts[0]) : 150.f;
		const float SpotBearing = SpotParts.Num() > 1 ? FCString::Atof(*SpotParts[1]) : 0.f;
		FAttackPayload Spot;
		Spot.WorldPoint = Pawn->GetActorLocation() + FRotator(0.f, Pawn->GetActorRotation().Yaw + SpotBearing, 0.f).RotateVector(FVector(SpotDistance, 0.f, 0.f));
		UE_LOG(LogStealth, Log, TEXT("[Bot] #%d strikes the spot %.0f cm at bearing %.0f."), PlayerIndex, SpotDistance, SpotBearing);
		AsOwner([&Spot](UPawnCommandComponent& Command) { Command.Attack(Spot); });
		return;
	}

	if (Action.StartsWith(TEXT("strike_"), ESearchCase::IgnoreCase))
	{
		// "strike_<npc|enemy|informant> [distance cm] [bearing deg]": approach to the distance (default 120 cm) with the
		// target at that bearing from the body facing (default 0; positive to the right), for the clip-variant evidence.
		TArray<FString> StrikeParts;
		Action.RightChop(7).ToLower().ParseIntoArrayWS(StrikeParts);
		const FString Kind = StrikeParts.Num() > 0 ? StrikeParts[0] : FString();
		const float StrikeDistance = StrikeParts.Num() > 1 ? FCString::Atof(*StrikeParts[1]) : 120.f;
		const float StrikeBearing = StrikeParts.Num() > 2 ? FCString::Atof(*StrikeParts[2]) : 0.f;
		AStealthCharacter* Best = nullptr;
		float BestDistance = TNumericLimits<float>::Max();
		for (TActorIterator<AStealthCharacter> It(GetWorld()); It; ++It)
		{
			AStealthCharacter* Candidate = *It;
			if (Candidate == Pawn || Candidate->IsDying())
			{
				continue;
			}
			bool bMatch = false;
			if (Kind == TEXT("npc"))
			{
				bMatch = Candidate->IsNpc() && !Candidate->IsConcealed();
			}
			else if (Kind == TEXT("enemy"))
			{
				bMatch = !Candidate->IsNpc() && Candidate->GetTeam() != Pawn->GetTeam() && Candidate->IsActiveAgent();
			}
			else if (Kind == TEXT("informant"))
			{
				bMatch = !Candidate->IsNpc() && Candidate->GetTeam() != Pawn->GetTeam() && Candidate->IsInformant();
			}
			const float Distance = FVector::Dist(Candidate->GetActorLocation(), Pawn->GetActorLocation());
			if (bMatch && Distance < BestDistance)
			{
				BestDistance = Distance;
				Best = Candidate;
			}
		}
		if (!Best)
		{
			UE_LOG(LogStealth, Warning, TEXT("[Bot] #%d strike_%s: no target found."), PlayerIndex, *Kind);
			return;
		}
		FaceAndApproach(Best, StrikeDistance);
		if (!FMath::IsNearlyZero(StrikeBearing))
		{
			const FRotator Turned(0.f, Pawn->GetActorRotation().Yaw - StrikeBearing, 0.f);
			Pawn->SetActorRotation(Turned);
			if (AController* C = Pawn->GetController())
			{
				const FRotator Facing(-12.f, Turned.Yaw, 0.f);
				C->SetControlRotation(Facing);
				if (APlayerController* PC = Cast<APlayerController>(C))
				{
					PC->ClientSetRotation(Facing, true);
				}
			}
		}
		UE_LOG(LogStealth, Log, TEXT("[Bot] #%d strikes %s (%s, %.0f cm, bearing %.0f, braced=%d)."), PlayerIndex, *Best->GetName(), *Kind, StrikeDistance, StrikeBearing,
			Best->GetActionState().Pose == EActionPose::Brace ? 1 : 0);
		// The production path (story asn-012 integration): an Attack command into Round's queue, resolved in its slot.
		FAttackPayload Strike;
		Strike.WorldPoint = Best->GetActorLocation();
		Strike.PawnRef = Best;
		AsOwner([&Strike](UPawnCommandComponent& Command) { Command.Attack(Strike); });
		return;
	}

	if (Action.Equals(TEXT("defend"), ESearchCase::IgnoreCase))
	{
		AsOwner([](UPawnCommandComponent& Command) { Command.PrepareBegin(); }); // resolved in the next drain's slot
		UE_LOG(LogStealth, Log, TEXT("[Bot] #%d defend: PrepareBegin submitted"), PlayerIndex);
		return;
	}

	if (Action.Equals(TEXT("release"), ESearchCase::IgnoreCase))
	{
		AsOwner([](UPawnCommandComponent& Command) { Command.PrepareEnd(); });
		UE_LOG(LogStealth, Log, TEXT("[Bot] #%d release: PrepareEnd submitted"), PlayerIndex);
		return;
	}

	if (Action.StartsWith(TEXT("interact"), ESearchCase::IgnoreCase) && !Action.Equals(TEXT("interactend"), ESearchCase::IgnoreCase))
	{
		// The production path (Objectives stories oc-005..009): stand at a court and submit InteractBegin through the pawn's
		// command component; the hold resolves in Round's queue (the IIF stand-in) and completes in UObjectivesService's slot
		// after ObjectiveInteractSec. "interact" picks this player's team's own live court (a dev read of the server store);
		// "interact <court id>" tends any court (a feint at a non-live court has no effect).
		const UStealthCompositionRoot* Root = UStealthCompositionRoot::Get(GetWorld());
		const FString Arg = Action.RightChop(8).TrimStartAndEnd();
		const AStealthPlayerController* OwnerController = Cast<AStealthPlayerController>(Controller);
		const FParticipantId Participant = Root != nullptr && OwnerController != nullptr ? Root->GetParticipant(OwnerController) : FParticipantId();
		const FCourtFixtureId Court = Arg.IsEmpty() ? (Root != nullptr ? Root->FindObjectivesLiveCourtForDev(Participant) : FCourtFixtureId())
			: FCourtFixtureId(static_cast<uint16>(FCString::Atoi(*Arg)));
		const AActor* CourtActor = Root != nullptr ? Root->FindObjectivesCourtActorForDev(Court) : nullptr;
		if (CourtActor == nullptr)
		{
			UE_LOG(LogStealth, Warning, TEXT("[Bot] #%d interact: no court (live court for this team not found)."), PlayerIndex);
			return;
		}
		FaceAndApproach(CourtActor, 150.f);
		AsOwner([Court](UPawnCommandComponent& Command) { Command.InteractBegin(Court); });
		UE_LOG(LogStealth, Log, TEXT("[Bot] #%d tends court %d: InteractBegin submitted (hold %.1f s)."), PlayerIndex, static_cast<int32>(Court.Value),
			UObjectivesService::Get(GetWorld()) != nullptr ? UObjectivesService::Get(GetWorld())->GetInteractHoldSec() : 0.0);
		return;
	}

	if (Action.StartsWith(TEXT("debugstage"), ESearchCase::IgnoreCase))
	{
		// DebugObjectiveCompletion (Objectives story oc-008): Round's own DebugItem slot completes the team's current stage.
		const int32 Team = FCString::Atoi(*Action.RightChop(10).TrimStartAndEnd());
		UObjectivesService* Objectives = UObjectivesService::Get(GetWorld());
		const bool bQueued = Objectives != nullptr && (Team == 0 || Team == 1) && Objectives->DebugCompleteStage(static_cast<uint8>(Team));
		UE_LOG(LogStealth, Log, TEXT("[Bot] debugstage %d: %s"), Team, bQueued ? TEXT("DebugItem submitted") : TEXT("refused"));
		return;
	}

	if (Action.Equals(TEXT("interactend"), ESearchCase::IgnoreCase))
	{
		AsOwner([](UPawnCommandComponent& Command) { Command.InteractEnd(); });
		UE_LOG(LogStealth, Log, TEXT("[Bot] #%d release: InteractEnd submitted"), PlayerIndex);
		return;
	}

	if (Action.Equals(TEXT("hide"), ESearchCase::IgnoreCase))
	{
		// Concealment epic: stand at the nearest bush's entry (the stand-in Manifest's runtime), then the one Hide(Enter) path.
		const UBushRuntimeSubsystem* Bushes = UBushRuntimeSubsystem::Get(GetWorld());
		const FBushRuntime* Nearest = nullptr;
		double NearestDistance = TNumericLimits<double>::Max();
		for (const FBushRuntime& Bush : Bushes != nullptr ? Bushes->GetBushes() : TArray<FBushRuntime>())
		{
			const double Distance = FVector::Dist2D(Bush.Anchor, Pawn->GetActorLocation());
			if (Distance < NearestDistance)
			{
				NearestDistance = Distance;
				Nearest = &Bush;
			}
		}
		if (Nearest != nullptr)
		{
			const uint8 Side = ConcealmentGeometry::NearestOpenSide(Nearest->EntryAxisMask, ConcealmentGeometry::YawFromAnchor(*Nearest, Pawn->GetActorLocation()));
			const FVector Entry = Nearest->Anchor + ConcealmentGeometry::SideNormal(Side) * 135.0 + FVector(0.0, 0.0, Pawn->GetSimpleCollisionHalfHeight() + 2.0);
			Pawn->TeleportTo(Entry, (Nearest->Anchor - Entry).GetSafeNormal2D().Rotation());
			const uint16 BushId = Nearest->Id;
			AsOwner([BushId](UPawnCommandComponent& Command) { Command.Hide(FHideCommand::Enter(BushId)); });
			UE_LOG(LogStealth, Log, TEXT("[Bot] #%d hides in bush %d (Hide(Enter) submitted)."), PlayerIndex, static_cast<int32>(BushId));
		}
		return;
	}

	if (Action.StartsWith(TEXT("unhide"), ESearchCase::IgnoreCase))
	{
		// "unhide [side 0-3]": the one Hide(Exit) path; the side defaults to the look yaw's.
		const FString Arg = Action.RightChop(6).TrimStartAndEnd();
		const uint8 Side = Arg.IsEmpty() ? ConcealmentGeometry::SideFromYaw(Pawn->GetControlRotation().Yaw) : static_cast<uint8>(FCString::Atoi(*Arg) & 0x3);
		AsOwner([Side](UPawnCommandComponent& Command) { Command.Hide(FHideCommand::Exit(Side)); });
		UE_LOG(LogStealth, Log, TEXT("[Bot] #%d leaves the bush by side %d (Hide(Exit) submitted)."), PlayerIndex, static_cast<int32>(Side));
		return;
	}

	if (Action.StartsWith(TEXT("watch"), ESearchCase::IgnoreCase))
	{
		// "watch<N> [distance cm]": stand facing player N at that distance (default 5 m).
		TArray<FString> WatchParts;
		Action.RightChop(5).ParseIntoArrayWS(WatchParts);
		const int32 WatchedIndex = WatchParts.Num() > 0 ? FCString::Atoi(*WatchParts[0]) : 0;
		const float WatchDistance = WatchParts.Num() > 1 ? FCString::Atof(*WatchParts[1]) : 500.f;
		int32 OtherIndex = 0;
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It, ++OtherIndex)
		{
			if (OtherIndex == WatchedIndex && It->Get() && It->Get()->GetPawn())
			{
				FaceAndApproach(It->Get()->GetPawn(), WatchDistance);
				UE_LOG(LogStealth, Log, TEXT("[Bot] #%d watches #%d."), PlayerIndex, WatchedIndex);
				return;
			}
		}
		UE_LOG(LogStealth, Warning, TEXT("[Bot] #%d watch: player %d has no pawn."), PlayerIndex, WatchedIndex);
		return;
	}

	if (Action.Equals(TEXT("blindstrike"), ESearchCase::IgnoreCase))
	{
		for (TActorIterator<AStealthCharacter> It(GetWorld()); It; ++It)
		{
			AStealthCharacter* Hidden = *It;
			if (Hidden != Pawn && !Hidden->IsDying() && Hidden->IsConcealed() && Hidden->GetTeam() != Pawn->GetTeam())
			{
				FaceAndApproach(Hidden, 130.f);
				const FVector StrikePoint = Hidden->GetActorLocation() + FVector(40.f, 0.f, 0.f);
				UE_LOG(LogStealth, Log, TEXT("[Bot] #%d blind-strikes at %s."), PlayerIndex, *StrikePoint.ToCompactString());
				FAttackPayload Strike;
				Strike.WorldPoint = StrikePoint; // no PawnRef: a TargetSpot strike at the bush (rule 2)
				AsOwner([&Strike](UPawnCommandComponent& Command) { Command.Attack(Strike); });
				return;
			}
		}
		UE_LOG(LogStealth, Warning, TEXT("[Bot] #%d blindstrike: no concealed enemy."), PlayerIndex);
		return;
	}

	if (Action.Equals(TEXT("ping"), ESearchCase::IgnoreCase))
	{
		HandlePing(Pawn, Pawn->GetActorLocation() + Pawn->GetActorForwardVector() * 300.f);
		UE_LOG(LogStealth, Log, TEXT("[Bot] #%d ping sent."), PlayerIndex);
		return;
	}

	UE_LOG(LogStealth, Warning, TEXT("[Bot] Unknown action '%s'."), *Action);
#endif
}

FText AStealthGameMode::GetTeamName(uint8 Team) const
{
	const TArray<FText>& Names = UStealthSettings::Get()->TeamNames;
	if (Names.IsValidIndex(Team))
	{
		return Names[Team];
	}
	return LOCTEXT("NoTeam", "Unassigned");
}

#undef LOCTEXT_NAMESPACE
