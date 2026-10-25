#include "StealthGameMode.h"
#include "StealthBush.h"
#include "StealthCharacter.h"
#include "StealthCityGenerator.h"
#include "StealthGame.h"
#include "StealthGameState.h"
#include "StealthHUD.h"
#include "StealthNpcController.h"
#include "StealthObjective.h"
#include "StealthPlayerController.h"
#include "StealthPlayerState.h"
#include "StealthRules.h"
#include "StealthSettings.h"

#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "NavigationSystem.h"
#include "TimerManager.h"

#define LOCTEXT_NAMESPACE "StealthGameMode"

AStealthGameMode::AStealthGameMode()
{
	GameStateClass = AStealthGameState::StaticClass();
	PlayerStateClass = AStealthPlayerState::StaticClass();
	PlayerControllerClass = AStealthPlayerController::StaticClass();
	HUDClass = AStealthHUD::StaticClass();
	bUseSeamlessTravel = false;
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
	DefaultPawnClass = CharacterClass;
}

void AStealthGameMode::BeginPlay()
{
	Super::BeginPlay();
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

bool AStealthGameMode::IsRoundInProgress() const
{
	const AStealthGameState* GS = GetStealthGameState();
	return GS && GS->RoundPhase == EStealthRoundPhase::InProgress;
}

UClass* AStealthGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
	return CharacterClass ? CharacterClass.Get() : Super::GetDefaultPawnClassForController_Implementation(InController);
}

// ---- Players ------------------------------------------------------------------------------------

void AStealthGameMode::PostLogin(APlayerController* NewPlayer)
{
	if (AStealthPlayerState* PS = NewPlayer ? NewPlayer->GetPlayerState<AStealthPlayerState>() : nullptr)
	{
		int32 PlayersPerTeam[STEALTH_NUM_TEAMS] = { 0, 0 };
		for (APlayerState* Other : GameState->PlayerArray)
		{
			const AStealthPlayerState* OtherPS = Cast<AStealthPlayerState>(Other);
			if (OtherPS && OtherPS != PS && OtherPS->Team < STEALTH_NUM_TEAMS)
			{
				++PlayersPerTeam[OtherPS->Team];
			}
		}
		PS->Team = FStealthRules::ChooseTeamForNewPlayer(PlayersPerTeam, UStealthSettings::Get()->MaxPlayersPerTeam);
		PS->ResetForRound();
		if (PS->Team == STEALTH_NO_TEAM)
		{
			UE_LOG(LogStealth, Warning, TEXT("Both teams are full; %s joins as an informant."), *PS->GetPlayerName());
			PS->LifeState = EStealthLifeState::Informant;
		}
	}

	Super::PostLogin(NewPlayer); // spawns the pawn through ChoosePlayerStart / SetPlayerDefaults

	if (const AStealthPlayerState* PS = NewPlayer ? NewPlayer->GetPlayerState<AStealthPlayerState>() : nullptr)
	{
		MessageController(NewPlayer, FText::Format(LOCTEXT("Joined", "You are {0}. Blend in. Watch for who doesn't."), GetTeamName(PS->Team)), false, 8.f);
	}
	TryScheduleRoundStart();
}

void AStealthGameMode::Logout(AController* Exiting)
{
	Super::Logout(Exiting);
	// A disconnected player no longer counts for either side.
	if (IsRoundInProgress())
	{
		GetWorldTimerManager().SetTimerForNextTick(this, &AStealthGameMode::CheckEliminationVictory);
	}
}

AActor* AStealthGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	const AStealthPlayerState* PS = Player ? Player->GetPlayerState<AStealthPlayerState>() : nullptr;
	if (PS && PS->Team < STEALTH_NUM_TEAMS)
	{
		const FName WantedTag(*FString::Printf(TEXT("Team%d"), PS->Team));
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
	return Super::ChoosePlayerStart_Implementation(Player);
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

void AStealthGameMode::SwitchTeam(AStealthPlayerController* Player)
{
	AStealthPlayerState* PS = Player ? Player->GetPlayerState<AStealthPlayerState>() : nullptr;
	if (!PS)
	{
		return;
	}
	PS->Team = PS->Team == 0 ? 1 : 0;
	if (APawn* Pawn = Player->GetPawn())
	{
		Pawn->Destroy();
	}
	RestartPlayer(Player);
	MessageController(Player, FText::Format(LOCTEXT("Switched", "You are now {0}."), GetTeamName(PS->Team)));
	CheckEliminationVictory();
}

// ---- Round lifecycle ---------------------------------------------------------------------------

void AStealthGameMode::TryScheduleRoundStart()
{
	AStealthGameState* GS = GetStealthGameState();
	const UStealthSettings* Settings = UStealthSettings::Get();
	if (!GS || GS->RoundPhase != EStealthRoundPhase::WaitingToStart || !Settings->bAutoStartRound)
	{
		return;
	}
	if (GetNumPlayers() < Settings->MinPlayersToStart || GetWorldTimerManager().IsTimerActive(RoundStartTimer))
	{
		return;
	}
	GS->PhaseEndServerTime = GS->GetServerWorldTimeSeconds() + Settings->RoundStartDelay;
	GetWorldTimerManager().SetTimer(RoundStartTimer, this, &AStealthGameMode::StartRound, FMath::Max(0.01f, Settings->RoundStartDelay), false);
}

void AStealthGameMode::StartRound()
{
	AStealthGameState* GS = GetStealthGameState();
	if (!GS)
	{
		return;
	}
	GetWorldTimerManager().ClearTimer(RoundStartTimer);
	GetWorldTimerManager().ClearTimer(RoundRestartTimer);

	CleanupRoundActors();
	AppearanceDeck.Reset();
	GS->ResetRoundState(FMath::Rand());
	GS->BuildCityForSeed(); // clients build the same layout when RoundSeed replicates

	for (TActorIterator<AStealthObjective> It(GetWorld()); It; ++It)
	{
		It->ResetObjective();
	}

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
			PC->UnPossess();
			Pawn->Destroy();
		}
		RestartPlayer(PC);
	}

	GS->RoundPhase = EStealthRoundPhase::InProgress;
	GS->PhaseEndServerTime = 0.0;
	MessageAll(LOCTEXT("RoundStart", "The round begins. Complete your team's tasks. Trust no one in the crowd."), false, 6.f);
	NavWaitStartTime = GetWorld()->GetTimeSeconds();
	GetWorldTimerManager().SetTimer(CrowdSpawnTimer, this, &AStealthGameMode::SpawnCrowdWhenNavReady, 0.25f, true, 0.25f);
}

void AStealthGameMode::SpawnCrowdWhenNavReady()
{
	// The new layout dirties the nav mesh; NPCs need the rebuilt mesh to find streets.
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
	SpawnCrowd();
	UE_LOG(LogStealth, Log, TEXT("Round started (seed %d, %d NPCs, nav wait %.1fs)."), GetStealthGameState()->RoundSeed, CrowdNpcs.Num(), GetWorld()->GetTimeSeconds() - NavWaitStartTime);
}

void AStealthGameMode::AbortRound()
{
	AStealthGameState* GS = GetStealthGameState();
	if (!GS)
	{
		return;
	}
	GetWorldTimerManager().ClearTimer(RoundRestartTimer);
	CleanupRoundActors();
	GS->RoundPhase = EStealthRoundPhase::WaitingToStart;
	GS->ResetRoundState(GS->RoundSeed);
	MessageAll(LOCTEXT("RoundAborted", "Round ended by command."));
	TryScheduleRoundStart();
}

void AStealthGameMode::EndRound(uint8 WinningTeam, EStealthWinReason Reason)
{
	AStealthGameState* GS = GetStealthGameState();
	if (!GS || GS->RoundPhase != EStealthRoundPhase::InProgress)
	{
		return; // first decisive event wins; later ones in the same frame are ignored
	}
	const UStealthSettings* Settings = UStealthSettings::Get();
	GS->RoundPhase = EStealthRoundPhase::RoundOver;
	GS->WinningTeam = WinningTeam;
	GS->WinReason = Reason;
	GS->PhaseEndServerTime = GS->GetServerWorldTimeSeconds() + Settings->RoundRestartDelay;

	for (TActorIterator<AStealthCharacter> It(GetWorld()); It; ++It)
	{
		It->StopDefence();
	}

	const FText ReasonText = Reason == EStealthWinReason::FinalObjective
		? LOCTEXT("ReasonObjective", "completed the final objective")
		: LOCTEXT("ReasonElimination", "eliminated every active enemy");
	MessageAll(FText::Format(LOCTEXT("RoundWon", "{0} win: they {1}."), GetTeamName(WinningTeam), ReasonText), false, Settings->RoundRestartDelay);
	UE_LOG(LogStealth, Log, TEXT("Round over: team %d wins (%s)."), WinningTeam, Reason == EStealthWinReason::FinalObjective ? TEXT("objective") : TEXT("elimination"));

	GetWorldTimerManager().SetTimer(RoundRestartTimer, this, &AStealthGameMode::StartRound, Settings->RoundRestartDelay, false);
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
	FVector Location;
	if (!CharacterClass || !FindCrowdPoint(Location))
	{
		return nullptr;
	}
	const AStealthCharacter* Defaults = CharacterClass->GetDefaultObject<AStealthCharacter>();
	Location.Z += Defaults->GetDefaultHalfHeight() + 5.f;

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;
	AStealthCharacter* Npc = GetWorld()->SpawnActor<AStealthCharacter>(CharacterClass, Location, FRotator(0.f, FMath::FRandRange(-180.f, 180.f), 0.f), Params);
	if (!Npc)
	{
		return nullptr;
	}

	AStealthNpcController* Controller = GetWorld()->SpawnActor<AStealthNpcController>();
	Controller->WanderCenter = GetCrowdCenter();
	Controller->WanderRadius = UStealthSettings::Get()->CrowdRadius;
	Controller->Possess(Npc);

	Npc->SetAppearanceIndex(DrawAppearance());
	CrowdNpcs.Add(Npc);
	return Npc;
}

void AStealthGameMode::SpawnCrowd()
{
	const int32 Wanted = UStealthSettings::Get()->NpcCount;
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

void AStealthGameMode::HandleAssassinationRequest(AStealthCharacter* Attacker, AStealthCharacter* Target, const FVector& StrikePoint)
{
	const UStealthSettings* Settings = UStealthSettings::Get();
	if (!IsRoundInProgress() || !Attacker || !Attacker->IsActiveAgent() || Attacker->IsHeldInPlace() || Attacker->IsPreparingDefence())
	{
		return;
	}
	const float MaxReach = Settings->AssassinationRange * Settings->AssassinationServerTolerance;

	// Blind strike: find a concealed character at the strike point.
	if (!Target)
	{
		if (FVector::Dist2D(StrikePoint, Attacker->GetActorLocation()) > MaxReach)
		{
			return;
		}
		float BestDistance = Settings->BlindStrikeRadius;
		for (TActorIterator<AStealthCharacter> It(GetWorld()); It; ++It)
		{
			AStealthCharacter* Candidate = *It;
			if (Candidate == Attacker || Candidate->IsDying() || !Candidate->IsConcealed())
			{
				continue;
			}
			const float Distance = FVector::Dist2D(Candidate->GetActorLocation(), StrikePoint);
			if (Distance <= BestDistance)
			{
				BestDistance = Distance;
				Target = Candidate;
			}
		}
		if (!Target)
		{
			// A strike at empty air is still visible to anyone watching.
			Attacker->RevealFromConcealment();
			Attacker->HoldInPlace(Settings->AttackerRecoveryTime * 0.5f);
			return;
		}
	}

	if (Target == Attacker || Target->IsDying() || FVector::Dist2D(Target->GetActorLocation(), Attacker->GetActorLocation()) > MaxReach)
	{
		return;
	}

	AStealthPlayerState* AttackerPS = Attacker->GetStealthPlayerState();
	const AStealthPlayerState* TargetPS = Target->GetStealthPlayerState();

	EStealthTargetKind Kind = EStealthTargetKind::Npc;
	if (TargetPS)
	{
		if (TargetPS->Team == AttackerPS->Team)
		{
			Kind = EStealthTargetKind::Teammate;
		}
		else
		{
			Kind = TargetPS->IsActivePlayer() ? EStealthTargetKind::ActiveEnemy : EStealthTargetKind::InformantEnemy;
		}
	}

	const EStealthAssassinationOutcome Outcome = FStealthRules::ResolveAssassination(true, Kind, Target->IsPreparingDefence());
	if (Outcome == EStealthAssassinationOutcome::Rejected)
	{
		return;
	}

	Attacker->RevealFromConcealment();

	switch (Outcome)
	{
	case EStealthAssassinationOutcome::EnemyEliminated:
		Attacker->HoldInPlace(Settings->AttackerRecoveryTime);
		MessagePlayer(Attacker, LOCTEXT("KillEnemy", "Target eliminated: an enemy agent."));
		MessagePlayer(Target, LOCTEXT("YouDied", "You were assassinated. You are now an informant."), true, 6.f);
		EliminatePlayer(Target);
		break;

	case EStealthAssassinationOutcome::Reversed:
		MessagePlayer(Target, LOCTEXT("Reversal", "Reversal! Your attacker is eliminated."), false, 5.f);
		MessagePlayer(Attacker, LOCTEXT("Reversed", "Your target was ready. You have been eliminated and are now an informant."), true, 6.f);
		Target->StopDefence();
		EliminatePlayer(Attacker);
		break;

	case EStealthAssassinationOutcome::NpcKilled:
	{
		Attacker->HoldInPlace(Settings->AttackerRecoveryTime);
		Target->KillCharacter();
		CrowdNpcs.Remove(Target);
		ScheduleNpcReplacement();
		++AttackerPS->NpcKills;
		if (FStealthRules::ShouldRemoveForNpcKills(AttackerPS->NpcKills, Settings->NpcKillLimit))
		{
			ExilePlayer(Attacker);
		}
		else
		{
			MessagePlayer(Attacker, FText::Format(LOCTEXT("KillNpc", "A civilian. Civilians killed: {0}/{1}."), AttackerPS->NpcKills, Settings->NpcKillLimit), true);
		}
		break;
	}

	case EStealthAssassinationOutcome::InformantKilled:
		Attacker->HoldInPlace(Settings->AttackerRecoveryTime);
		MessagePlayer(Attacker, LOCTEXT("KillInformant", "An enemy informant, silenced for now."));
		MessagePlayer(Target, LOCTEXT("InformantDied", "You were silenced. Returning to your team's spawn."), true);
		EliminatePlayer(Target);
		break;

	default:
		break;
	}

	CheckEliminationVictory();
}

void AStealthGameMode::EliminatePlayer(AStealthCharacter* Victim)
{
	AController* Controller = Victim ? Victim->GetController() : nullptr;
	AStealthPlayerState* PS = Victim ? Victim->GetStealthPlayerState() : nullptr;
	if (!Victim || !Controller || !PS)
	{
		return;
	}
	PS->LifeState = EStealthLifeState::Informant;
	Victim->KillCharacter();

	FTimerHandle& Handle = PendingTimers.AddDefaulted_GetRef();
	TWeakObjectPtr<AController> WeakController(Controller);
	GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateUObject(this, &AStealthGameMode::RespawnInformant, WeakController),
		FMath::Max(0.1f, UStealthSettings::Get()->InformantRespawnDelay), false);
}

void AStealthGameMode::ExilePlayer(AStealthCharacter* Offender)
{
	AStealthPlayerState* PS = Offender ? Offender->GetStealthPlayerState() : nullptr;
	if (!PS)
	{
		return;
	}
	PS->LifeState = EStealthLifeState::Informant;
	Offender->StopDefence();
	HandleInteractRequest(Offender, false);
	MessagePlayer(Offender, FText::Format(LOCTEXT("Exiled", "Exiled for killing {0} civilians. You are now an informant: watch, and report to your team."), PS->NpcKills), true, 8.f);
}

void AStealthGameMode::RespawnInformant(TWeakObjectPtr<AController> Controller)
{
	AController* Resolved = Controller.Get();
	if (!Resolved || !IsRoundInProgress())
	{
		return;
	}
	if (APawn* Old = Resolved->GetPawn())
	{
		Resolved->UnPossess();
		Old->Destroy();
	}
	RestartPlayer(Resolved);
}

void AStealthGameMode::CheckEliminationVictory()
{
	if (!IsRoundInProgress())
	{
		return;
	}
	int32 Players[STEALTH_NUM_TEAMS] = { 0, 0 };
	int32 Active[STEALTH_NUM_TEAMS] = { 0, 0 };
	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		const AStealthPlayerState* PS = Cast<AStealthPlayerState>(PlayerState);
		if (!PS || PS->Team >= STEALTH_NUM_TEAMS || PS->IsInactive())
		{
			continue;
		}
		++Players[PS->Team];
		if (PS->IsActivePlayer())
		{
			++Active[PS->Team];
		}
	}
	const uint8 Winner = FStealthRules::EvaluateEliminationWinner(Players, Active);
	if (Winner != STEALTH_NO_TEAM)
	{
		EndRound(Winner, EStealthWinReason::Elimination);
	}
}

// ---- Defence & objectives -------------------------------------------------------------------------

void AStealthGameMode::HandleDefenceRequest(AStealthCharacter* Defender)
{
	AStealthPlayerState* PS = Defender ? Defender->GetStealthPlayerState() : nullptr;
	if (!PS || !IsRoundInProgress() || Defender->IsPreparingDefence())
	{
		return;
	}
	const UStealthSettings* Settings = UStealthSettings::Get();
	const double Now = GetStealthGameState()->GetServerWorldTimeSeconds();
	if (FStealthRules::CanStartDefence(Defender->IsActiveAgent(), Defender->IsRunning(), Defender->IsInTraversal(), Now, PS->LastDefenceActivationTime, Settings->DefenceCooldown))
	{
		Defender->StartDefence();
	}
}

void AStealthGameMode::HandleInteractRequest(AStealthCharacter* Character, bool bWantsInteract)
{
	if (!Character)
	{
		return;
	}
	if (!bWantsInteract)
	{
		for (TActorIterator<AStealthObjective> It(GetWorld()); It; ++It)
		{
			It->SetInteractor(Character, false);
		}
		return;
	}
	const AStealthGameState* GS = GetStealthGameState();
	if (!IsRoundInProgress() || !Character->IsActiveAgent() || !GS)
	{
		return;
	}
	const uint8 Team = Character->GetTeam();
	AStealthObjective* Best = nullptr;
	float BestDistance = TNumericLimits<float>::Max();
	for (TActorIterator<AStealthObjective> It(GetWorld()); It; ++It)
	{
		AStealthObjective* Objective = *It;
		if (Objective->IsCompleted() || !Objective->IsInRange(Character)
			|| !FStealthRules::CanCompleteObjective(Team, GS->GetTeamStage(Team), Objective->OwningTeam, Objective->Stage))
		{
			continue;
		}
		const float Distance = FVector::Dist2D(Objective->GetActorLocation(), Character->GetActorLocation());
		if (Distance < BestDistance)
		{
			BestDistance = Distance;
			Best = Objective;
		}
	}
	if (Best)
	{
		Best->SetInteractor(Character, true);
	}
}

void AStealthGameMode::OnObjectiveCompleted(AStealthObjective* Objective, AStealthCharacter* Completer)
{
	AStealthGameState* GS = GetStealthGameState();
	if (!GS || !Objective || !Completer || !IsRoundInProgress())
	{
		return;
	}
	const uint8 Team = Completer->GetTeam();
	if (!FStealthRules::CanCompleteObjective(Team, GS->GetTeamStage(Team), Objective->OwningTeam, Objective->Stage))
	{
		Objective->ResetObjective();
		return;
	}

	if (Objective->Stage == STEALTH_FINAL_STAGE)
	{
		EndRound(Team, EStealthWinReason::FinalObjective);
		return;
	}

	GS->TeamStage[Team] = Objective->Stage + 1;
	MessageTeam(Team, FText::Format(LOCTEXT("StageDone", "Task complete: {0}. Stage {1} unlocked."), Objective->DisplayName, GS->TeamStage[Team]), false, 6.f);
	const uint8 OtherTeam = Team == 0 ? 1 : 0;
	MessageTeam(OtherTeam, FText::Format(LOCTEXT("EnemyAdvanced", "The {0} have advanced to stage {1}."), GetTeamName(Team), GS->TeamStage[Team]), true, 5.f);
	if (GS->TeamStage[Team] == STEALTH_FINAL_STAGE)
	{
		MessageTeam(Team, LOCTEXT("FinalUnlocked", "The final target is revealed to you. Strike before the enemy does."), false, 8.f);
	}
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
		UE_LOG(LogStealth, Log, TEXT("[Bot] Phase=%d Stages=%d/%d Winner=%d NPCs=%d"), (int32)GS->RoundPhase, GS->GetTeamStage(0), GS->GetTeamStage(1), GS->WinningTeam, CrowdNpcs.Num());
		Index = 0;
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It, ++Index)
		{
			const AStealthPlayerState* PS = It->Get() ? It->Get()->GetPlayerState<AStealthPlayerState>() : nullptr;
			const AStealthCharacter* P = It->Get() ? Cast<AStealthCharacter>(It->Get()->GetPawn()) : nullptr;
			if (PS)
			{
				UE_LOG(LogStealth, Log, TEXT("[Bot] #%d %s team=%d state=%s npcKills=%d pawn=%s prepared=%d concealed=%d"), Index, *PS->GetPlayerName(), PS->Team,
					PS->IsInformant() ? TEXT("Informant") : TEXT("Active"), PS->NpcKills, P ? *P->GetActorLocation().ToCompactString() : TEXT("none"),
					P ? P->IsPreparingDefence() : 0, P ? P->IsConcealed() : 0);
			}
		}
		return;
	}

	if (!Pawn)
	{
		UE_LOG(LogStealth, Warning, TEXT("[Bot] Player %d has no pawn."), PlayerIndex);
		return;
	}

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

	if (Action.StartsWith(TEXT("strike_"), ESearchCase::IgnoreCase))
	{
		const FString Kind = Action.RightChop(7).ToLower();
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
		FaceAndApproach(Best, 120.f);
		UE_LOG(LogStealth, Log, TEXT("[Bot] #%d strikes %s (%s, prepared=%d)."), PlayerIndex, *Best->GetName(), *Kind, Best->IsPreparingDefence());
		HandleAssassinationRequest(Pawn, Best, Best->GetActorLocation());
		return;
	}

	if (Action.Equals(TEXT("defend"), ESearchCase::IgnoreCase))
	{
		HandleDefenceRequest(Pawn);
		UE_LOG(LogStealth, Log, TEXT("[Bot] #%d defend -> prepared=%d"), PlayerIndex, Pawn->IsPreparingDefence());
		return;
	}

	if (Action.Equals(TEXT("interact"), ESearchCase::IgnoreCase))
	{
		const uint8 Team = Pawn->GetTeam();
		for (TActorIterator<AStealthObjective> It(GetWorld()); It; ++It)
		{
			if (!It->IsCompleted() && FStealthRules::CanCompleteObjective(Team, GS->GetTeamStage(Team), It->OwningTeam, It->Stage))
			{
				FaceAndApproach(*It, 150.f);
				HandleInteractRequest(Pawn, true);
				UE_LOG(LogStealth, Log, TEXT("[Bot] #%d interacting with '%s' (stage %d)."), PlayerIndex, *It->DisplayName.ToString(), It->Stage);
				return;
			}
		}
		UE_LOG(LogStealth, Warning, TEXT("[Bot] #%d interact: no available objective."), PlayerIndex);
		return;
	}

	if (Action.Equals(TEXT("hide"), ESearchCase::IgnoreCase))
	{
		AStealthBush* Nearest = nullptr;
		float NearestDistance = TNumericLimits<float>::Max();
		for (TActorIterator<AStealthBush> It(GetWorld()); It; ++It)
		{
			const float Distance = FVector::Dist(It->GetActorLocation(), Pawn->GetActorLocation());
			if (Distance < NearestDistance)
			{
				NearestDistance = Distance;
				Nearest = *It;
			}
		}
		if (Nearest)
		{
			Pawn->TeleportTo(FVector(Nearest->GetActorLocation().X, Nearest->GetActorLocation().Y, Pawn->GetActorLocation().Z), Pawn->GetActorRotation());
			UE_LOG(LogStealth, Log, TEXT("[Bot] #%d hides in %s."), PlayerIndex, *Nearest->GetName());
		}
		return;
	}

	if (Action.StartsWith(TEXT("watch"), ESearchCase::IgnoreCase))
	{
		const int32 WatchedIndex = FCString::Atoi(*Action.RightChop(5));
		int32 OtherIndex = 0;
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It, ++OtherIndex)
		{
			if (OtherIndex == WatchedIndex && It->Get() && It->Get()->GetPawn())
			{
				FaceAndApproach(It->Get()->GetPawn(), 500.f);
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
				HandleAssassinationRequest(Pawn, nullptr, StrikePoint);
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
