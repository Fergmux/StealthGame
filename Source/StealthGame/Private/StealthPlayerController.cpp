#include "StealthPlayerController.h"
#include "StealthGame.h"
#include "StealthGameMode.h"
#include "StealthSettings.h"

#include "StealthBush.h"
#include "StealthGameState.h"
#include "Server/Core/Round/StealthRoundMatchStore.h"
#include "Server/StealthCompositionRoot.h"
#include "View/StealthRosterTeamView.h"

#include "Camera/CameraActor.h"
#include "EngineUtils.h"
#include "InputKeyEventArgs.h"
#include "Engine/NetDriver.h"
#include "Engine/PackageMapClient.h"
#include "EnhancedInputComponent.h"
#include "Server/ActionHub.h"
#include "Server/Core/CommandIngress.h"
#include "StealthCharacter.h"
#include "Server/Spikes/StealthSpikeHooks.h" // THROWAWAY SPIKE hook (IVP S10 re-run, story-056)
#include "View/Input/PawnCommandClient.h"
#include "Server/Core/StealthPawnCommandComponent.h"
#include "Server/Core/StealthPawnFactory.h"
#include "View/DefenceOwnerComponent.h"
#include "View/EncounterViewSubsystem.h"
#include "StealthPlayerState.h"
#include "View/StealthTuningPublicSubstructs.h"
#include "GameFramework/GameStateBase.h"
#include "TimerManager.h"

namespace
{
	constexpr int32 MaxHudMessages = 5;
	/** Short confirmations were missed in playtests; keep every message up at least this long. */
	constexpr float MinHudMessageSeconds = 6.f;
	/** Received reply entries kept for the owner-side readers (tests, the story asn-024 matcher). */
	constexpr int32 MaxKeptReplies = 64;
	/** STAND-IN for Penalty's ExileBeatSec (0.8 s): the exile line follows the WrongTarget plate. */
	constexpr float ExileBeatSec = 0.8f;
}

AStealthPlayerController::AStealthPlayerController()
{
	// ADR-0020 E2: replicated components travel only through the registered subobject list.
	bReplicateUsingRegisteredSubObjectList = true;
	TeamView = CreateDefaultSubobject<URosterTeamViewComponent>(TEXT("TeamView"));
	CommandClient = CreateDefaultSubobject<UPawnCommandClient>(TEXT("CommandClient"));
	// Story asn-016 (ADR-0018 section 1.5): the owner's cooldown end, COND_OwnerOnly through the registered subobject list.
	DefenceOwner = CreateDefaultSubobject<UDefenceOwnerComponent>(TEXT("DefenceOwner"));
}

// ---- Pawn command transport (ADR-0011 section 2; story iif-094) -------------------------------------------------

void AStealthPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		if (UActionHub* Hub = UActionHub::Get(GetWorld()))
		{
			ReplySink = Hub->GetReplySinks().Allocate();
		}
	}
	if (IsLocalController() && CommandClient)
	{
		const UStealthSettings* Settings = UStealthSettings::Get();
		CommandClient->SetUplink(this);
		CommandClient->SetAimRangeCm(Settings->AssassinationRange);
		CommandClient->SetCameraToggleLockoutSec(Settings->CameraToggleLockoutSec);
		CommandClient->ApplyGameplayContexts();
		// Prototype adapters (D53) until the camera and assassination epics land: the accepted camera toggle drives the
		// prototype first/third-person switch; owner prediction drives the prototype's local stance and progress.
		CommandClient->OnCameraToggleAccepted.AddWeakLambda(this, [this]()
		{
			if (AStealthCharacter* Character = Cast<AStealthCharacter>(GetPawn()))
			{
				Character->ToggleCameraMode();
			}
		});
		// The role context follows the owner's own Roster state from its C-TeamView (story iif-071).
		if (TeamView)
		{
			TeamView->OnViewUpdated.AddWeakLambda(this, [this](const FTeamView& View)
			{
				if (CommandClient)
				{
					CommandClient->SetOwnerState(static_cast<EParticipantState>(View.Owner.State));
				}
			});
			CommandClient->SetOwnerState(static_cast<EParticipantState>(TeamView->GetView().Owner.State));
		}
		CommandClient->OnOwnerCommandPredicted.AddWeakLambda(this, [this](ECommandKind Kind, FClientCommandSeq)
		{
			if (AStealthCharacter* Character = Cast<AStealthCharacter>(GetPawn()))
			{
				Character->HandleOwnerCommandPredicted(Kind);
			}
		});
	}
}

void AStealthPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority())
	{
		if (UActionHub* Hub = UActionHub::Get(GetWorld()))
		{
			Hub->GetReplySinks().Release(ReplySink);
		}
		ReplySink = FReplySinkId();
	}
	Super::EndPlay(EndPlayReason);
}

void AStealthPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	// Gameplay input lives on the controller (E9); the pawn's SetupPlayerInputComponent binds nothing gameplay-relevant.
	if (UEnhancedInputComponent* Enhanced = Cast<UEnhancedInputComponent>(InputComponent))
	{
		if (CommandClient)
		{
			CommandClient->BindInput(*Enhanced);
		}
	}
}

#if !UE_BUILD_SHIPPING
TArray<FStealthPossessionTraceEvent>& AStealthPlayerController::GetPossessionTrace()
{
	static TArray<FStealthPossessionTraceEvent> Trace;
	return Trace;
}

namespace StealthPossessionTraceLocal
{
	void Record(const AStealthPlayerController& Controller, FStealthPossessionTraceEvent::EKind Kind, uint16 Epoch, const APawn* Pawn)
	{
		FStealthPossessionTraceEvent Event;
		Event.Kind = Kind;
		Event.Epoch = Epoch;
		Event.Frame = GFrameCounter;
		Event.Seconds = FPlatformTime::Seconds();
		Event.Controller = Controller.GetPathName();
		Event.Pawn = GetNameSafe(Pawn);
		Event.bClientWorld = Controller.GetWorld() && Controller.GetWorld()->GetNetMode() == NM_Client;
		const UNetDriver* Driver = Controller.GetWorld() ? Controller.GetWorld()->GetNetDriver() : nullptr;
		Event.PawnGuid = Pawn && Driver && Driver->GetNetGuidCache().IsValid() ? Driver->GetNetGuidCache()->GetNetGUID(Pawn).ObjectId : 0;
		// Bounded (development-only spike trace): the spikes reset it; a long session keeps the latest entries only.
		TArray<FStealthPossessionTraceEvent>& Trace = AStealthPlayerController::GetPossessionTrace();
		if (Trace.Num() >= 4096)
		{
			Trace.RemoveAt(0, 1024, EAllowShrinking::No);
		}
		Trace.Add(MoveTemp(Event));
	}
}
TArray<uint64>& AStealthPlayerController::GetFlushPressedKeysTrace()
{
	static TArray<uint64> Trace;
	return Trace;
}

#define STEALTH_POSSESSION_TRACE(Kind, Epoch, Pawn) StealthPossessionTraceLocal::Record(*this, FStealthPossessionTraceEvent::EKind::Kind, Epoch, Pawn)
#else
#define STEALTH_POSSESSION_TRACE(Kind, Epoch, Pawn)
#endif

void AStealthPlayerController::FlushPressedKeys()
{
#if !UE_BUILD_SHIPPING
	TArray<uint64>& Trace = GetFlushPressedKeysTrace();
	if (Trace.Num() >= 4096)
	{
		Trace.RemoveAt(0, 1024, EAllowShrinking::No);
	}
	Trace.Add(GFrameCounter);
#endif
	Super::FlushPressedKeys();
}

void AStealthPlayerController::OnPossess(APawn* InPawn)
{
	// The server owns the epoch: advance it and tell the owner BEFORE the engine's ClientRestart (Super), on the same
	// reliable controller channel, so the owner's epoch is current before it acts on the new pawn (V2). Same-pawn
	// re-possession advances it too.
	++ServerPossessionEpoch;
	// ADR-0010 4.4: the factory's control epoch is recorded here once the hand-over pass lands; until then 0.
	PossessedControlEpoch = 0;
	STEALTH_POSSESSION_TRACE(ServerOnPossess, ServerPossessionEpoch, InPawn);
	ClientSetPossessionEpoch(ServerPossessionEpoch);
	Super::OnPossess(InPawn);
	// Story asn-016: a re-pawn copies the new pawn's cooldown end (none for a pawn that never braced). Tuning stand-in:
	// the schema default until the TDC store feeds features (as the composition root's predicates).
	if (DefenceOwner)
	{
		DefenceOwner->SyncFromPawn(InPawn, FAssassinationTuning_Public().DefenceCooldownSec);
	}
}

void AStealthPlayerController::OnUnPossess()
{
	if (UStealthPawnFactory::IsReleasingBody() && GetPawn() != nullptr)
	{
		// The body's release (story asn-019, ADR-0018 section 6.2): APlayerController::OnUnPossess minus its
		// SetViewTarget(this), so the view stays on the falling body and Camera's Dying -> NoPawn fade owns it (ADR-0015).
		GetPawn()->SetReplicates(true);
		GetPawn()->UnPossessed();
		SetPawn(nullptr);
		return;
	}
	Super::OnUnPossess();
}

void AStealthPlayerController::ClientRestart_Implementation(APawn* NewPawn)
{
	STEALTH_POSSESSION_TRACE(OwnerRestart, OwnerPossessionEpoch, NewPawn);
	// Release safety (ADR-0011 section 9): end every outstanding Begin before the change is applied.
	if (CommandClient)
	{
		CommandClient->NotifyPossessionChanged(NewPawn);
	}
	Super::ClientRestart_Implementation(NewPawn);
	// Story asn-021: the own-role clock's consumer, and the role this pawn was possessed in (STAND-IN for the owner's
	// C-TeamView own State: an informant's pawn is always a fresh respawn, possessed after the informant state replicated).
	const AStealthPlayerState* PS = GetPlayerState<AStealthPlayerState>();
	PossessedAsInformantPawn = PS != nullptr && PS->IsInformant() ? NewPawn : nullptr;
	BindEncounterView();
}

void AStealthPlayerController::BindEncounterView()
{
	if (bEncounterViewBound || !IsLocalController())
	{
		return;
	}
	ULocalPlayer* Local = GetLocalPlayer();
	UEncounterViewSubsystem* View = Local != nullptr ? Local->GetSubsystem<UEncounterViewSubsystem>() : nullptr;
	if (View == nullptr)
	{
		return;
	}
	bEncounterViewBound = true;
	TWeakObjectPtr<AStealthPlayerController> WeakThis(this);
	View->SetPossessedAsInformantQuery([WeakThis](const APawn* Candidate)
	{
		return WeakThis.IsValid() && Candidate != nullptr && WeakThis->PossessedAsInformantPawn.Get() == Candidate;
	});
	View->OnRoleChangePresented.AddUObject(this, &AStealthPlayerController::HandleRoleChangePresented);
}

void AStealthPlayerController::HandleRoleChangePresented(ERoleChangeReason Reason, double LocalTime)
{
	// STAND-IN for the HUD epic's removal card: shown at the dispatch (the pawn's own DeathServerTime), never on receipt.
	if (Reason == ERoleChangeReason::InformantKilled)
	{
		ClientStealthMessage_Implementation(NSLOCTEXT("StealthOwnerResults", "InformantDied", "You were silenced. Returning to your team's spawn."), true, 5.f);
		return;
	}
	ClientStealthMessage_Implementation(NSLOCTEXT("StealthOwnerResults", "YouWereEliminated", "You have been eliminated. You are now an informant."), true, 6.f);
}

void AStealthPlayerController::OnRep_Pawn()
{
	if (CommandClient && IsLocalController())
	{
		CommandClient->NotifyPossessionChanged(GetPawn());
	}
	Super::OnRep_Pawn();
}

void AStealthPlayerController::ClientSetPossessionEpoch_Implementation(uint16 Epoch)
{
	OwnerPossessionEpoch = Epoch;
	STEALTH_POSSESSION_TRACE(OwnerEpoch, Epoch, GetPawn());
	if (CommandClient)
	{
		CommandClient->SetOwnerPossessionEpoch(Epoch);
	}
}

bool AStealthPlayerController::ServerPawnCommand_Validate(const FNetPawnCommand& Cmd)
{
	// Only structurally impossible payloads (enum out of range, non-finite vector); everything else is a silent drop.
	return Cmd.IsStructurallyValid();
}

void AStealthPlayerController::ServerPawnCommand_Implementation(const FNetPawnCommand& Cmd)
{
	IngestOwnCommand(Cmd, nullptr);
}

void AStealthPlayerController::SendPawnCommand(const FNetPawnCommand& Cmd, APawn* HostLocalPawnRef)
{
	if (HasAuthority())
	{
		// The listen host: IngestCommand(HostLocal) with the same checks and the same reply flush (ADR-0011 section 2).
		IngestOwnCommand(Cmd, HostLocalPawnRef);
		return;
	}
	ServerPawnCommand(Cmd);
}

void AStealthPlayerController::IngestOwnCommand(const FNetPawnCommand& Cmd, APawn* HostLocalPawnRef)
{
	// A controller that outlived its world's hub (seamless travel) re-allocates its reply sink in the current world.
	if (UActionHub* Hub = UActionHub::Get(GetWorld()); Hub && !Hub->GetReplySinks().IsLive(ReplySink))
	{
		ReplySink = Hub->GetReplySinks().Allocate();
	}
	FCommandIngressContext Context;
	const UStealthCompositionRoot* Root = UStealthCompositionRoot::Get(GetWorld());
	const FParticipantId Participant = Root ? Root->GetParticipant(this) : FParticipantId();
	Context.Origin.Mailbox = Participant.IsValid() ? FMailboxId::ForParticipant(Participant) : FMailboxId::ServerLocal();
	Context.Origin.Sink = ReplySink;
	Context.ControlEpoch = PossessedControlEpoch;
	Context.HostLocalPawnRef = HostLocalPawnRef;
	IngestCommand(*this, Context, Cmd);
}

void AStealthPlayerController::ClientCommandReplies_Implementation(const TArray<FCommandReply>& Replies)
{
	ReceivedReplyCount += Replies.Num();
	UE_LOGF(LogStealth, Verbose, "[cmd] %ls received %d reply(ies)", *GetName(), Replies.Num());
	for (const FCommandReply& Reply : Replies)
	{
		ReceivedReplies.Add(Reply);
		if (Reply.Kind == ECommandReplyKind::StrikeResult || Reply.Kind == ECommandReplyKind::DefenceResult)
		{
			PresentOwnerResult(Reply);
		}
	}
	if (ReceivedReplies.Num() > MaxKeptReplies)
	{
		ReceivedReplies.RemoveAt(0, ReceivedReplies.Num() - MaxKeptReplies);
	}
}

int32 AStealthPlayerController::GetShownWrongTargetTally() const
{
	// HUD-R7: a new RoundEpoch shows 0 until its first plate.
	const AStealthGameState* RoundState = GetWorld() != nullptr ? GetWorld()->GetGameState<AStealthGameState>() : nullptr;
	return RoundState != nullptr && RoundState->GetRoundPublicState().RoundIndex != ShownTallyRoundIndex ? 0 : ShownWrongTargetTally;
}

void AStealthPlayerController::PresentOwnerResult(const FCommandReply& Reply)
{
	// STAND-IN for story asn-024 (the owner result matcher and the HUD plates): one HUD message per result, the strike's
	// shown at its own ContactServerTime (never earlier, UI-ASN-1), the tally line changing with it (HUD-R7).
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}
	if (Reply.Kind == ECommandReplyKind::DefenceResult)
	{
		ClientStealthMessage_Implementation(NSLOCTEXT("StealthOwnerResults", "Reversal", "Reversal! Your attacker is eliminated."), false, 5.f);
		return;
	}
	const FStrikeResult Strike = Reply.Strike;
	const AGameStateBase* State = World->GetGameState();
	const double Now = State != nullptr ? State->GetServerWorldTimeSeconds() : World->GetTimeSeconds();
	const float Delay = static_cast<float>(FMath::Max(Strike.ContactServerTime - Now, 0.0));
	TWeakObjectPtr<AStealthPlayerController> WeakThis(this);
	auto Show = [WeakThis, Strike]()
	{
		AStealthPlayerController* Self = WeakThis.Get();
		if (Self == nullptr)
		{
			return;
		}
		Self->ShownWrongTargetTally = Strike.TallyAfter;
		// Story asn-021: the attacker's transient grade on its local contact frame for a kill or a reversal (a body met a
		// body); a whiff (NoContact) plays none. STAND-IN call site until story asn-024's matcher owns the plate frame.
		if (Strike.Result != EStrikeResult::NoContact && Self->GetLocalPlayer() != nullptr)
		{
			if (UEncounterViewSubsystem* View = Self->GetLocalPlayer()->GetSubsystem<UEncounterViewSubsystem>())
			{
				const AGameStateBase* GradeState = Self->GetWorld() != nullptr ? Self->GetWorld()->GetGameState() : nullptr;
				View->ApplyTransientGrade(ETransientGradeCause::AttackerContact, GradeState != nullptr ? GradeState->GetServerWorldTimeSeconds() : 0.0, Strike.ContactServerTime);
			}
		}
		const AStealthGameState* RoundState = Self->GetWorld() != nullptr ? Self->GetWorld()->GetGameState<AStealthGameState>() : nullptr;
		Self->ShownTallyRoundIndex = RoundState != nullptr ? RoundState->GetRoundPublicState().RoundIndex : 0;
		switch (Strike.Result)
		{
		case EStrikeResult::RivalEliminated:
			Self->ClientStealthMessage_Implementation(NSLOCTEXT("StealthOwnerResults", "TargetEliminated", "Target eliminated"), false, 4.f);
			break;
		case EStrikeResult::WrongTarget:
			Self->ClientStealthMessage_Implementation(FText::Format(NSLOCTEXT("StealthOwnerResults", "WrongTarget", "Wrong target. Wrong targets: {0}"), Strike.TallyAfter), true, 4.f);
			break;
		case EStrikeResult::NoContact:
			Self->ClientStealthMessage_Implementation(NSLOCTEXT("StealthOwnerResults", "NoContact", "No contact"), false, 4.f);
			break;
		case EStrikeResult::Reversed:
			Self->ClientStealthMessage_Implementation(NSLOCTEXT("StealthOwnerResults", "Reversed", "Reversed"), true, 4.f);
			break;
		}
		if (Strike.bExiled)
		{
			FTimerHandle ExileHandle;
			Self->GetWorldTimerManager().SetTimer(ExileHandle, FTimerDelegate::CreateWeakLambda(Self, [Self]()
			{
				Self->ClientStealthMessage_Implementation(NSLOCTEXT("StealthOwnerResults", "Exiled",
					"Exiled: too many wrong targets. You are now an informant: watch, and report to your team."), true, 8.f);
			}), ExileBeatSec, false);
		}
	};
	if (Delay <= 0.f)
	{
		Show();
		return;
	}
	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, Show), Delay, false);
}

void AStealthPlayerController::SendTeamPing(const FVector& Location)
{
	ServerTeamPing(Location);
}

void AStealthPlayerController::ServerTeamPing_Implementation(FVector_NetQuantize Location)
{
	// Prototype handler (D53) until the informant-pings epic owns it.
	AStealthCharacter* Sender = Cast<AStealthCharacter>(GetPawn());
	AStealthGameMode* Mode = GetWorld()->GetAuthGameMode<AStealthGameMode>();
	if (Sender && Mode)
	{
		Mode->HandlePing(Sender, Location);
	}
}

void AStealthPlayerController::ClientStealthMessage_Implementation(const FText& Text, bool bDanger, float Duration)
{
	FStealthHudMessage& Message = Messages.AddDefaulted_GetRef();
	Message.Text = Text;
	Message.bDanger = bDanger;
	Message.ExpireTime = GetWorld()->GetTimeSeconds() + FMath::Max(Duration, MinHudMessageSeconds);
	if (Messages.Num() > MaxHudMessages)
	{
		Messages.RemoveAt(0);
	}
	UE_LOG(LogStealth, Log, TEXT("[HUD] %s (netmode %d) message: %s"), *GetName(), (int32)GetNetMode(), *Text.ToString());
}

void AStealthPlayerController::ClientReceivePing_Implementation(FVector_NetQuantize Location, const FString& SenderName)
{
	FStealthPing& Ping = Pings.AddDefaulted_GetRef();
	Ping.Location = Location;
	Ping.SenderName = SenderName;
	Ping.ExpireTime = GetWorld()->GetTimeSeconds() + UStealthSettings::Get()->PingLifetime;
}

void AStealthPlayerController::PruneExpired(double Now)
{
	Messages.RemoveAll([Now](const FStealthHudMessage& M) { return M.ExpireTime <= Now; });
	Pings.RemoveAll([Now](const FStealthPing& P) { return P.ExpireTime <= Now; });
}

void AStealthPlayerController::StealthInputKey(const FString& KeyName, int32 Down)
{
#if !UE_BUILD_SHIPPING
	const FKey Key(*KeyName);
	if (!Key.IsValid())
	{
		UE_LOGF(LogStealth, Warning, "StealthInputKey: unknown key %ls", *KeyName);
		return;
	}
	InputKey(FInputKeyEventArgs::CreateSimulated(Key, Down != 0 ? IE_Pressed : IE_Released, Down != 0 ? 1.f : 0.f));
#endif
}

void AStealthPlayerController::StealthDumpProd()
{
#if !UE_BUILD_SHIPPING
	if (HasAuthority())
	{
		if (const UStealthCompositionRoot* Root = UStealthCompositionRoot::Get(GetWorld()))
		{
			UE_LOGF(LogStealth, Log, "%ls", *Root->DescribeProductionState(GetWorld()));
		}
	}
	if (const AStealthGameState* GS = GetWorld()->GetGameState<AStealthGameState>())
	{
		const FRoundPublicState& State = GS->GetRoundPublicState();
		UE_LOGF(LogStealth, Log, "[prod] %ls public: phase %ls idx %u changes %u handler %d", *GetName(), LexRoundPhase(State.Phase), State.RoundIndex,
			State.PhaseChangeCount, GS->GetRoundPublicStateHandlerCalls());
	}
	if (TeamView)
	{
		const FTeamView& View = TeamView->GetView();
		UE_LOGF(LogStealth, Log, "[prod] %ls team view: team %d state %d mates %d", *GetName(), View.Owner.Team, View.Owner.State, View.Teammates.Num());
	}
#endif
}

FString AStealthPlayerController::ConsoleCommand(const FString& Command, bool bWriteToLog)
{
	if (!IsLocalController())
	{
		// A remote client's ServerExec: Round's host check sees no local controller (NotHost).
		FRoundRemoteExecScope RemoteScope;
		return Super::ConsoleCommand(Command, bWriteToLog);
	}
	return Super::ConsoleCommand(Command, bWriteToLog);
}

void AStealthPlayerController::StealthBot(int32 PlayerIndex, const FString& Action)
{
#if !UE_BUILD_SHIPPING
	// Owner-side drive (dev only, run-and-observe for the SCL movement stories): "drive <forward 0..1> <run 0|1> [<yaw>]"
	// holds a Move intent on this controller's own pawn exactly as a held stick does (owner prediction + ServerMove);
	// "drive 0" releases it. PlayerIndex is ignored (the local pawn).
	if (Action.StartsWith(TEXT("drive"), ESearchCase::IgnoreCase))
	{
		TArray<FString> Parts;
		Action.ParseIntoArrayWS(Parts);
		const float Forward = Parts.Num() > 1 ? FCString::Atof(*Parts[1]) : 0.0f;
		const bool bRun = Parts.Num() > 2 && FCString::Atoi(*Parts[2]) != 0;
		if (Parts.Num() > 3)
		{
			SetControlRotation(FRotator(GetControlRotation().Pitch, FCString::Atof(*Parts[3]), 0.0));
		}
		if (AStealthPawn* Body = Cast<AStealthPawn>(GetPawn()))
		{
			Body->GetPawnCommand()->Move(FVector2D(0.0, Forward), bRun ? EGait::Run : EGait::Walk);
		}
		return;
	}
	ServerStealthBot(PlayerIndex, Action);
#endif
}

void AStealthPlayerController::ServerStealthBot_Implementation(int32 PlayerIndex, const FString& Action)
{
#if !UE_BUILD_SHIPPING
	if (AStealthGameMode* GameMode = GetWorld()->GetAuthGameMode<AStealthGameMode>())
	{
		GameMode->DebugBotAction(PlayerIndex, Action);
	}
#endif
}

void AStealthPlayerController::StealthDebugCam(int32 Mode)
{
#if !UE_BUILD_SHIPPING
	if (Mode == 0)
	{
		if (GetPawn())
		{
			SetViewTarget(GetPawn());
		}
		return;
	}
	if (!DebugCamera)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		DebugCamera = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FVector(-4200.f, 0.f, 3200.f), FRotator(-40.f, 0.f, 0.f), Params);
	}
	DebugCamera->SetActorLocationAndRotation(FVector(-4200.f, 0.f, 3200.f), FRotator(-40.f, 0.f, 0.f));

	// Mode 2: frame the bush nearest to this player's pawn, seen from the pawn's side.
	if (Mode == 2 && GetPawn())
	{
		const FVector From = GetPawn()->GetActorLocation();
		const AStealthBush* Nearest = nullptr;
		float NearestDistance = TNumericLimits<float>::Max();
		for (TActorIterator<AStealthBush> It(GetWorld()); It; ++It)
		{
			const float Distance = FVector::Dist(It->GetActorLocation(), From);
			if (Distance < NearestDistance)
			{
				NearestDistance = Distance;
				Nearest = *It;
			}
		}
		if (Nearest)
		{
			const FVector Target = Nearest->GetActorLocation();
			const FVector Eye = Target + (From - Target).GetSafeNormal2D() * 450.f + FVector(0.f, 0.f, 120.f);
			DebugCamera->SetActorLocationAndRotation(Eye, (Target - Eye).Rotation());
		}
	}
	SetViewTarget(DebugCamera);
#endif
}

void AStealthPlayerController::NotifyActorChannelFailure(UActorChannel* ActorChan)
{
	Super::NotifyActorChannelFailure(ActorChan);
#if !UE_BUILD_SHIPPING
	// SPIKE (IVP S10 re-run): queue-only hook, no-op unless the harness installs it.
	if (const StealthSpikeHooks::FChannelFailureHook& Hook = StealthSpikeHooks::GetChannelFailureHook())
	{
		Hook(this, ActorChan);
	}
#endif
}
