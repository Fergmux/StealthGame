#include "StealthPlayerController.h"
#include "StealthGame.h"
#include "StealthGameMode.h"
#include "StealthSettings.h"

#include "StealthBush.h"
#include "StealthGameState.h"
#include "Server/Core/Round/StealthRoundMatchStore.h"
#include "Server/StealthCompositionRoot.h"
#include "View/StealthRosterTeamView.h"
#include "View/ObjectiveTeamViewComponent.h"

#include "Camera/CameraActor.h"
#include "Camera/PlayerCameraManager.h"
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
#include "View/EncounterPresentation.h"
#include "View/EncounterViewSubsystem.h"
#include "View/OwnResultPlateSettings.h"
#include "StealthPlayerState.h"
#include "View/StealthOwnerTallyCache.h"
#include "View/StealthTuningPublicSubstructs.h"
#include "View/PenaltyHudViewModel.h"
#include "Server/StealthTuningInvariants.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/GameStateBase.h"
#include "TimerManager.h"

namespace
{
	constexpr int32 MaxHudMessages = 5;
	/** Short confirmations were missed in playtests; keep every message up at least this long. */
	constexpr float MinHudMessageSeconds = 6.f;
	/** Received reply entries kept for the owner-side readers (tests, the story asn-024 matcher). */
	constexpr int32 MaxKeptReplies = 64;
	/** Owner presentation records kept (story asn-024 / asn-017 observation). */
	constexpr int32 MaxOwnPresentationRecords = 512;
	/** A time counts as reached one millisecond early (the presentation's contact-frame tolerance). */
	constexpr double ReachedEpsilonSec = 0.001;

	/**
	 * Camera's NoPawnFadeSec (the fade to the neutral view, CS-R12.1): a pending-owner constant until Camera registers the
	 * key (TD-TDC-I3); the countdown shows only after this fade completes (UI-NKP-4, story nkp-018).
	 */
	constexpr double NoPawnFadeSec = StealthTuningPendingOwnerKeys::NoPawnFadeSec;

#if WITH_STEALTH_TEST_SEAMS
	bool GPresentOnRosterReceipt = false;
	bool GPresentResultsOnReceipt = false;
	double GReplyLatenessSec = 0.0;
	bool GDropExilingStrikeResults = false;
	int32 GDroppedExilingResults = 0;
#endif
}

#if WITH_STEALTH_TEST_SEAMS
void StealthOwnerPresentationTestSeams::SetPresentOnRosterReceipt(bool bOn) { GPresentOnRosterReceipt = bOn; }
bool StealthOwnerPresentationTestSeams::IsPresentOnRosterReceipt() { return GPresentOnRosterReceipt; }
void StealthOwnerPresentationTestSeams::SetPresentResultsOnReceipt(bool bOn) { GPresentResultsOnReceipt = bOn; }
void StealthOwnerPresentationTestSeams::SetReplyLatenessSec(double Sec) { GReplyLatenessSec = FMath::Max(0.0, Sec); }
void StealthOwnerPresentationTestSeams::SetDropExilingStrikeResults(bool bOn)
{
	GDropExilingStrikeResults = bOn;
	GDroppedExilingResults = 0;
}
int32 StealthOwnerPresentationTestSeams::GetDroppedExilingResults() { return GDroppedExilingResults; }
#endif

const TCHAR* LexOwnPresentationEvent(EOwnPresentationEvent Event)
{
	switch (Event)
	{
	case EOwnPresentationEvent::ResultPlate: return TEXT("ResultPlate");
	case EOwnPresentationEvent::TallyTick: return TEXT("TallyTick");
	case EOwnPresentationEvent::ExileCard: return TEXT("ExileCard");
	case EOwnPresentationEvent::RemovalCard: return TEXT("RemovalCard");
	case EOwnPresentationEvent::BraceSquare: return TEXT("BraceSquare");
	case EOwnPresentationEvent::HudMessage: return TEXT("HudMessage");
	case EOwnPresentationEvent::Countdown: return TEXT("Countdown");
	case EOwnPresentationEvent::PlatePreempted: return TEXT("PlatePreempted");
	default: return TEXT("RoundEnd");
	}
}

AStealthPlayerController::AStealthPlayerController()
{
	// ADR-0020 E2: replicated components travel only through the registered subobject list.
	bReplicateUsingRegisteredSubObjectList = true;
	TeamView = CreateDefaultSubobject<URosterTeamViewComponent>(TEXT("TeamView"));
	// Story oc-011 (ADR-0020 section 6): the Objectives part of C-TeamView, owner-only through this controller.
	ObjectiveTeamView = CreateDefaultSubobject<UObjectiveTeamViewComponent>(TEXT("ObjectiveTeamView"));
	CommandClient = CreateDefaultSubobject<UPawnCommandClient>(TEXT("CommandClient"));
	// Story asn-016 (ADR-0018 section 1.5): the owner's cooldown end, COND_OwnerOnly through the registered subobject list.
	DefenceOwner = CreateDefaultSubobject<UDefenceOwnerComponent>(TEXT("DefenceOwner"));
}

void AStealthPlayerController::HandleObjectiveViewUpdated(const FObjectiveTeamView& View)
{
	const uint8 Stage = static_cast<uint8>(View.StageIndex);
	const uint8 Previous = LastPresentedObjectiveStage;
	LastPresentedObjectiveStage = Stage;
	if (Previous == static_cast<uint8>(EObjectiveTeamStage::None) || Stage <= Previous)
	{
		return; // the first view of a round, a reset or a re-push: no banked line
	}
	AddHudMessage(View.StageIndex == EObjectiveTeamStage::Final ? NSLOCTEXT("StealthObjectives", "ProdFinalUnlocked", "Your house's preparations are done. Find the ruler's cup.")
		: NSLOCTEXT("StealthObjectives", "ProdStageBanked", "Task complete. The next task is ready."), false, 6.f);
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
	if (IsLocalController() && ObjectiveTeamView)
	{
		ObjectiveTeamView->OnViewUpdated.AddUObject(this, &AStealthPlayerController::HandleObjectiveViewUpdated);
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
	// HUD-R7 display cache (story nkp-017): a rebuilt controller starts from the last tally shown in this round until the
	// owner re-push lands (it never overrides a re-push that passes the gate: the gate only ever raises the value).
	if (IsLocalController())
	{
		const ULocalPlayer* Local = GetLocalPlayer();
		const UStealthOwnerTallyCache* Cache = Local != nullptr ? Local->GetSubsystem<UStealthOwnerTallyCache>() : nullptr;
		int32 CachedTally = 0;
		uint32 CachedSeq = 0;
		if (Cache != nullptr && Cache->Find(CurrentRoundIndex(), CachedTally, CachedSeq))
		{
			ShownWrongTargetTally = CachedTally;
			ShownTallyRoundIndex = CurrentRoundIndex();
			LastShownTallySeq = CachedSeq;
		}
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
	// Story asn-017 / ASN-R23: a pawn possessed as an informant presents the informant role at once (its death already did).
	bInformantPresented = bInformantPresented || PossessedAsInformantPawn.IsValid();
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
	const double ServerNow = UEncounterPresentationSubsystem::EstimatedServerNow(GetWorld());
	const UEncounterViewSubsystem* View = GetLocalPlayer() != nullptr ? GetLocalPlayer()->GetSubsystem<UEncounterViewSubsystem>() : nullptr;
	const FRoleChangeRecord* Record = View != nullptr && !View->GetRoleChanges().IsEmpty() ? &View->GetRoleChanges().Last() : nullptr;
	const double DeathServerTime = Record != nullptr ? Record->DeathServerTime : -1.0;
	// Story asn-024 (art bible 7.6 rule 7): a reversed attacker's "Reversed" plate is released inside this sequence, so the
	// grade, the plate and the removal card enter as one at the shared contact (= its DeathServerTime).
	if (Reason == ERoleChangeReason::ReversedAgainst)
	{
		const void* DyingPawn = Record != nullptr ? static_cast<const void*>(Record->PawnKey) : static_cast<const void*>(GetPawn());
		ResultMatcher.OnReversedAgainstPresented(DyingPawn, ServerNow);
		for (const FOwnResultDue& Due : ResultMatcher.PopDue(ServerNow))
		{
			ShowResultPlate(Due, ServerNow);
		}
	}
	// Story nkp-018 (UI-NKP-4, HUD-E11): the returning countdown shows only once the neutral view is reached: the own fall
	// (DeathFallSec from the death write) plus Camera's NoPawnFadeSec.
	const double FallStart = DeathServerTime >= 0.0 ? DeathServerTime : ServerNow;
	CountdownGateAt = FallStart + static_cast<double>(FAssassinationTuning_Public().DeathFallSec) + NoPawnFadeSec;
	bInformantPresented = true;
	// Story nkp-017 (PEN-R16, HUD-R9, Penalty CR-18): a killed informant has no State change, so it gets NO removal card and
	// no exile cue: its view stays on the falling body and the countdown follows the fade.
	if (Reason == ERoleChangeReason::InformantKilled)
	{
		return;
	}
	// STAND-IN for the HUD epic's removal card: shown at the dispatch (the pawn's own DeathServerTime), never on receipt.
	const FText Card = NSLOCTEXT("StealthOwnerResults", "YouWereEliminated", "You have been eliminated. You are now an informant.");
	LogOwnPresentation(EOwnPresentationEvent::RemovalCard, Card.ToString(), static_cast<int32>(Reason), ServerNow, DeathServerTime);
	AddHudMessage(Card, true, 6.f);
}

void AStealthPlayerController::SetPawn(APawn* InPawn)
{
	// The body being let go: the current pawn (server), or, on the owning client, where the replicated Pawn is already null
	// when AController::OnRep_Pawn calls SetPawn(nullptr), the current view target. Only a LIVE released body (the exile's
	// hand-over to the crowd) keeps the view; a dead body's release keeps the engine's behaviour (Camera's Dying -> NoPawn
	// fade owns that view, and holding a hidden dead body as the view target let its pose evaluation produce NaNs).
	const APawn* Released = GetPawn() != nullptr ? static_cast<const APawn*>(GetPawn()) : (PlayerCameraManager != nullptr ? Cast<APawn>(PlayerCameraManager->GetViewTarget()) : nullptr);
	const AStealthPawn* ReleasedShared = Cast<AStealthPawn>(Released);
	const bool bKeepView = InPawn == nullptr && bAutoManageActiveCameraTarget && IsValid(Released) && !Released->IsActorBeingDestroyed()
		&& (ReleasedShared == nullptr || !ReleasedShared->HasDeathWrite());
	if (bKeepView)
	{
		bAutoManageActiveCameraTarget = false;
	}
	Super::SetPawn(InPawn);
	if (bKeepView)
	{
		bAutoManageActiveCameraTarget = true;
	}
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
	// Story asn-024: the press is recorded with the commanded pawn (an exile releases it later) and its state now.
	if (IsLocalController() && (Cmd.Kind == ECommandKind::Attack || Cmd.Kind == ECommandKind::PrepareBegin))
	{
		if (AStealthPawn* Commanded = Cast<AStealthPawn>(GetPawn()))
		{
			WatchedPawns.Add(Commanded, Commanded);
			ResultMatcher.OnCommandSent(Cmd.Kind, Cmd.Seq, Commanded, Commanded->GetActionState());
		}
	}
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
	const double ReceivedAt = UEncounterPresentationSubsystem::EstimatedServerNow(GetWorld());
#if WITH_STEALTH_TEST_SEAMS
	// Story nkp-017 seams: drop the exiling result (the lost RPC) and / or hold the batch (the injected RPC lateness).
	if (IsLocalController() && (GDropExilingStrikeResults || GReplyLatenessSec > 0.0))
	{
		TArray<FCommandReply> Kept;
		for (const FCommandReply& Reply : Replies)
		{
			if (GDropExilingStrikeResults && Reply.Kind == ECommandReplyKind::StrikeResult && Reply.Strike.bExiled)
			{
				++GDroppedExilingResults;
				continue;
			}
			Kept.Add(Reply);
		}
		if (GReplyLatenessSec > 0.0)
		{
			FHeldReplyBatch& Held = HeldReplyBatches.AddDefaulted_GetRef();
			Held.ReleaseServerTime = ReceivedAt + GReplyLatenessSec;
			Held.Replies = MoveTemp(Kept);
			return;
		}
		ProcessCommandReplies(Kept, ReceivedAt);
		return;
	}
#endif
	ProcessCommandReplies(Replies, ReceivedAt);
}

void AStealthPlayerController::ProcessCommandReplies(const TArray<FCommandReply>& Replies, double ServerNow)
{
	ReceivedReplyCount += Replies.Num();
	UE_LOGF(LogStealth, Verbose, "[cmd] %ls received %d reply(ies)", *GetName(), Replies.Num());
	// Story asn-024: results enter the matcher with their processing time and nothing is presented here (on receipt): the
	// per-frame TickOwnPresentation shows each at its local contact frame.
	for (const FCommandReply& Reply : Replies)
	{
		ReceivedReplies.Add(Reply);
		switch (Reply.Kind)
		{
		case ECommandReplyKind::StrikeResult:
			ResultMatcher.OnStrikeResult(Reply.Strike, ServerNow, GetPawn());
			break;
		case ECommandReplyKind::DefenceResult:
			ResultMatcher.OnDefenceResult(Reply.Seq, Reply.Defence, ServerNow, GetPawn());
			break;
		case ECommandReplyKind::Rejected:
			ResultMatcher.OnRejected(Reply.Seq);
			break;
		default:
			break;
		}
	}
#if WITH_STEALTH_TEST_SEAMS
	if (GPresentResultsOnReceipt)
	{
		// The negative control: every pending result now, on receipt (the forbidden behaviour).
		for (const FOwnResultDue& Due : ResultMatcher.PopDue(ServerNow + 1000.0))
		{
			ShowResultPlate(Due, ServerNow);
		}
	}
#endif
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

void AStealthPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	const double ServerNow = UEncounterPresentationSubsystem::EstimatedServerNow(GetWorld());
#if WITH_STEALTH_TEST_SEAMS
	// Story nkp-017: the lateness seam's held batches arrive now (their processing time is the simulated late arrival).
	for (int32 Index = 0; Index < HeldReplyBatches.Num();)
	{
		if (ServerNow + ReachedEpsilonSec >= HeldReplyBatches[Index].ReleaseServerTime)
		{
			const TArray<FCommandReply> Batch = MoveTemp(HeldReplyBatches[Index].Replies);
			HeldReplyBatches.RemoveAt(Index);
			ProcessCommandReplies(Batch, ServerNow);
			continue;
		}
		++Index;
	}
#endif
	TickOwnPresentation(ServerNow);
}

uint32 AStealthPlayerController::CurrentRoundIndex() const
{
	const AStealthGameState* RoundState = GetWorld() != nullptr ? GetWorld()->GetGameState<AStealthGameState>() : nullptr;
	return RoundState != nullptr ? RoundState->GetRoundPublicState().RoundIndex : 0;
}

void AStealthPlayerController::StoreShownTally()
{
	if (ULocalPlayer* Local = GetLocalPlayer())
	{
		if (UStealthOwnerTallyCache* Cache = Local->GetSubsystem<UStealthOwnerTallyCache>())
		{
			Cache->Store(ShownTallyRoundIndex, ShownWrongTargetTally, LastShownTallySeq);
		}
	}
}

void AStealthPlayerController::ApplyOwnerTallyRepush(double ServerNow)
{
	// HUD-R7 / Penalty CR-12 (b): the C-TeamView owner re-push {WrongTargetTally, LastResultSeq} (written by Penalty only on
	// reclaim, rejoin and reset) is used only when no result plate is pending, only if its LastResultSeq is at least the
	// sequence of the last plate shown, and never lowers the shown tally within a round.
	if (!TeamView || ResultMatcher.HasPending())
	{
		return;
	}
	const FTeamViewOwnerPart& Part = TeamView->GetView().Owner;
	const uint32 Round = CurrentRoundIndex();
	const int32 Shown = Round != ShownTallyRoundIndex ? 0 : ShownWrongTargetTally;
	if (Part.LastResultSeq == 0 || Part.LastResultSeq < LastShownTallySeq || static_cast<int32>(Part.WrongTargetTally) <= Shown)
	{
		return;
	}
	ShownWrongTargetTally = Part.WrongTargetTally;
	ShownTallyRoundIndex = Round;
	LastShownTallySeq = Part.LastResultSeq;
	StoreShownTally();
	LogOwnPresentation(EOwnPresentationEvent::TallyTick, TEXT("repush"), ShownWrongTargetTally, ServerNow);
}

FPenaltyHudFrame AStealthPlayerController::BuildPenaltyHudFrame(double ServerNow) const
{
	FPenaltyHudFrame Frame;
	// UI-NKP-1: the shown tally (HUD-R7) with the public L (the schema default until the TDC client store feeds the HUD;
	// STAND-IN as the cooldown's DefenceCooldownSec above), frozen without the triangle while informant (ruling D4). The view
	// (its formatted text) is rebuilt only when one of its inputs changes.
	const int32 Tally = GetShownWrongTargetTally();
	const int32 Limit = FPenaltyTuning_Public().NpcKillStrikeLimit;
	const bool bFrozen = IsPresentedInformant();
	const int64 TallyKey = (static_cast<int64>(Tally) << 32) | (static_cast<int64>(Limit) << 1) | (bFrozen ? 1 : 0);
	if (TallyKey != CachedTallyKey || !CachedTallyView.IsValid())
	{
		CachedTallyKey = TallyKey;
		CachedTallyView = MakeShared<FPenaltyTallyView>(FPenaltyTallyView::Make(Tally, Limit, bFrozen));
	}
	Frame.Tally = *CachedTallyView;
	Frame.Card = BuildPenaltyCard(ServerNow);
	Frame.Countdown = BuildPenaltyCountdown(ServerNow, Frame.Card.Phase);
	return Frame;
}

FPenaltyExileCardView AStealthPlayerController::BuildPenaltyCard(double ServerNow) const
{
	// UI-NKP-3: the card from its client-local entrance (Hidden at once when it never entered).
	const UOwnResultPlateSettings* Settings = UOwnResultPlateSettings::Get();
	FPenaltyCardTiming Timing;
	Timing.InSec = Settings->HudRemovalCardInSec;
	Timing.HoldSec = Settings->HudRemovalCardHoldSec * Settings->HudReadHoldMult;
	Timing.OutSec = Settings->HudRemovalCardOutSec;
	return FPenaltyExileCardView::Make(ExileCardEnteredAt, ServerNow, Timing);
}

FPenaltyCountdownView AStealthPlayerController::BuildPenaltyCountdown(double ServerNow, EPenaltyCardPhase CardPhase) const
{
	// UI-NKP-4: the owner's own T_due (owner part only), for an awaiting informant, once the neutral view is reached. The
	// countdown waits for the neutral view (the fade) and for the removal card to have collapsed: the HUD never shows two
	// text blocks at once (HUD panel rule; the card's Notice-band neighbour).
	const double Due = TeamView ? TeamView->GetView().Owner.T_due : 0.0;
	const bool bAwaiting = IsPresentedInformant() && GetPawn() == nullptr;
	const bool bFadeComplete = (CountdownGateAt < 0.0 || ServerNow + ReachedEpsilonSec >= CountdownGateAt) && CardPhase == EPenaltyCardPhase::Hidden;
	return FPenaltyCountdownView::Make(bAwaiting, Due, ServerNow, bFadeComplete);
}

bool AStealthPlayerController::IsOwnOutcomeUnpresented(double ServerNow) const
{
	if (ResultMatcher.HasPending())
	{
		return true;
	}
	const AStealthPawn* Own = Cast<AStealthPawn>(GetPawn());
	return Own != nullptr && Own->HasDeathWrite() && ServerNow + ReachedEpsilonSec < Own->GetDeathState().DeathServerTime;
}

void AStealthPlayerController::TickOwnPresentation(double ServerNow)
{
	UWorld* World = GetWorld();
	if (World == nullptr || !IsLocalController())
	{
		return;
	}
	const UOwnResultPlateSettings* PlateSettings = UOwnResultPlateSettings::Get();
	ResultMatcher.SetParams(PlateSettings->MatcherParams());

	// 1 Sample the commanded pawns' FActionState: the matcher's write side, in either order against the results (E6).
	if (AStealthPawn* Own = Cast<AStealthPawn>(GetPawn()))
	{
		WatchedPawns.Add(Own, Own);
	}
	const TArray<const void*> Watched = ResultMatcher.GetWatchedPawns();
	for (auto It = WatchedPawns.CreateIterator(); It; ++It)
	{
		AStealthPawn* WatchedPawn = It.Value().Get();
		if (WatchedPawn == nullptr || (WatchedPawn != GetPawn() && !Watched.Contains(It.Key())))
		{
			It.RemoveCurrent();
			continue;
		}
		ResultMatcher.OnActionStateObserved(WatchedPawn, WatchedPawn->GetActionState(), ServerNow);
	}

	// 2 Results due on this frame: the local contact frame, or on processing when late.
	for (const FOwnResultDue& Due : ResultMatcher.PopDue(ServerNow))
	{
		ShowResultPlate(Due, ServerNow);
	}
	if (ResultPlate.bActive
		&& FOwnResultPlateTimeline::Sample(PlateSettings->TimelineParams(), ServerNow - ResultPlate.ShowServerTime).Phase == EOwnResultPlatePhase::Gone)
	{
		ResultPlate.bActive = false;
	}
	if (ResultPlate.bActive && ResultPlate.PreemptServerTime >= 0.0 && ServerNow >= ResultPlate.PreemptServerTime + PlateSettings->HudPreemptExitSec)
	{
		ResultPlate.bActive = false; // pushed off by the card (HUD-R9)
	}

	// 3 The exile card, ExileBeatSec after its plate (UI-NKP-3, HUD-R9; stories nkp-017 / nkp-018): the card-entrance event
	// is the exile's one clock. The plate still in the Result slot is pushed off (never cross-faded), the card shows once,
	// the countdown may show after Camera's view-release fade, and OnRemovalCardEntered fires for Camera and the exile cue.
	if (ExileCardAt >= 0.0 && ServerNow + ReachedEpsilonSec >= ExileCardAt)
	{
		ExileCardAt = -1.0;
		if (ExileCardEnteredAt < 0.0)
		{
			ExileCardEnteredAt = ServerNow;
			if (ResultPlate.bActive && ResultPlate.PreemptServerTime < 0.0)
			{
				ResultPlate.PreemptServerTime = ServerNow;
				LogOwnPresentation(EOwnPresentationEvent::PlatePreempted, ResultPlate.Text.ToString(), static_cast<int32>(ResultPlate.Plate), ServerNow);
			}
			const FPenaltyExileCardView Card = FPenaltyExileCardView::Make(ServerNow, ServerNow, FPenaltyCardTiming());
			LogOwnPresentation(EOwnPresentationEvent::ExileCard, FString::Printf(TEXT("%s / %s / %s"), *Card.Title.ToString(), *Card.Subtitle.ToString(),
				*Card.Line2.ToString()), 0, ServerNow);
			bInformantPresented = true;
			CountdownGateAt = ServerNow + NoPawnFadeSec;
			OnRemovalCardEntered.Broadcast(ServerNow);
		}
	}

	// 3b The owner tally re-push (HUD-R7 gate) and the returning countdown's shown seconds (logged on change).
	ApplyOwnerTallyRepush(ServerNow);
	{
		const FPenaltyCountdownView Countdown = BuildPenaltyCountdown(ServerNow, BuildPenaltyCard(ServerNow).Phase);
		const int32 Seconds = Countdown.bVisible ? Countdown.Seconds : -1;
		if (Seconds != LoggedCountdownSeconds)
		{
			LoggedCountdownSeconds = Seconds;
			LogOwnPresentation(EOwnPresentationEvent::Countdown, Countdown.bVisible ? Countdown.Text.ToString() : FString(), Seconds, ServerNow);
		}
	}

	// 4 The brace square (UI-ASN-3): a reversal ends it at contact, never at the Counter's or a result's arrival.
	const AStealthPawn* OwnPawn = Cast<AStealthPawn>(GetPawn());
	const bool bSquare = OwnPawn != nullptr && FOwnBraceIndicator::IsShown(OwnPawn->GetActionState(), ServerNow);
	if (bSquare != bBraceSquareShown)
	{
		bBraceSquareShown = bSquare;
		const bool bCounter = OwnPawn != nullptr && OwnPawn->GetActionState().Pose == EActionPose::Counter;
		LogOwnPresentation(EOwnPresentationEvent::BraceSquare, FString(), bSquare ? 1 : 0, ServerNow, bCounter ? OwnPawn->GetActionState().ContactServerTime : -1.0);
	}

	// 5 The presented role (ASN-R23): the Roster's informant state is shown only through the presentation above (which may
	// come before or after the Roster state), Active again at the Roster's own return to Active (a Reset is no outcome); an
	// informant state no presentation follows within the cap is shown then (lost data).
	const AStealthPlayerState* OwnState = GetPlayerState<AStealthPlayerState>();
	const bool bRosterInformant = OwnState != nullptr && OwnState->IsInformant();
	if (!bRosterInformant)
	{
		if (bRosterWasInformant)
		{
			bInformantPresented = false;
			// Active again (a new round): the next exile gets its own card, and no countdown gate is carried over.
			ExileCardEnteredAt = -1.0;
			CountdownGateAt = -1.0;
		}
		UnpresentedInformantSince = -1.0;
	}
	else if (!bInformantPresented)
	{
		UnpresentedInformantSince = UnpresentedInformantSince < 0.0 ? ServerNow : UnpresentedInformantSince;
		if (ServerNow >= UnpresentedInformantSince + ResultMatcher.GetParams().HoldBackSec())
		{
			bInformantPresented = true;
		}
	}
	else
	{
		UnpresentedInformantSince = -1.0;
	}
	bRosterWasInformant = bRosterInformant;

	// 6 The hold buffer (ASN-R23, HUD F10): server lines and the round end that arrived while an own outcome was unpresented
	// are released at its presentation (never before the owner's own plate or own fall), or at the lost-data cap.
	const bool bHold = IsOwnOutcomeUnpresented(ServerNow);
	const double Cap = ResultMatcher.GetParams().HoldBackSec();
	for (int32 Index = 0; Index < HeldMessages.Num();)
	{
		if (!bHold || ServerNow >= HeldMessages[Index].ArrivalServerTime + Cap)
		{
			const FHeldMessage Held = HeldMessages[Index];
			HeldMessages.RemoveAt(Index);
			AddHudMessage(Held.Text, Held.bDanger, Held.Duration, Held.ArrivalServerTime);
			continue;
		}
		++Index;
	}
	const AStealthGameState* RoundState = World->GetGameState<AStealthGameState>();
	const bool bRoundOver = RoundState != nullptr && RoundState->RoundPhase == EStealthRoundPhase::RoundOver;
	if (!bRoundOver)
	{
		RoundEndSeenAt = -1.0;
		bRoundEndPresented = false;
	}
	else if (!bRoundEndPresented)
	{
		RoundEndSeenAt = RoundEndSeenAt < 0.0 ? ServerNow : RoundEndSeenAt;
		if (!bHold || ServerNow >= RoundEndSeenAt + Cap)
		{
			bRoundEndPresented = true;
			LogOwnPresentation(EOwnPresentationEvent::RoundEnd, FString(), 0, ServerNow, -1.0, RoundEndSeenAt);
		}
	}

#if WITH_STEALTH_TEST_SEAMS
	// The asn-017 negative control: the forbidden presentation on the receipt of the owner's outcome state (the Roster
	// informant state, or the own pawn's FDeathState, whichever arrives first; both arrive before the local contact frame).
	const AStealthPlayerState* SeamState = GetPlayerState<AStealthPlayerState>();
	const bool bOutcomeState = (SeamState != nullptr && SeamState->IsInformant()) || (OwnPawn != nullptr && OwnPawn->HasDeathWrite());
	if (GPresentOnRosterReceipt && bOutcomeState && !bSeamSawInformant)
	{
		AddHudMessage(NSLOCTEXT("StealthOwnerResults", "SeamRosterReceipt", "[seam] outcome state received"), true, 4.f, ServerNow);
	}
	bSeamSawInformant = bOutcomeState;
#endif
}

void AStealthPlayerController::ShowResultPlate(const FOwnResultDue& Due, double ServerNow)
{
	// One entrance and one timing for every plate (HUD E07 / E08): the newest takes the Result slot.
	ResultPlate.bActive = true;
	ResultPlate.Plate = Due.Plate;
	ResultPlate.Text = OwnResultPlateText(Due.Plate);
	ResultPlate.ShowServerTime = ServerNow;
	ResultPlate.ContactServerTime = Due.ContactServerTime;
	LogOwnPresentation(EOwnPresentationEvent::ResultPlate, ResultPlate.Text.ToString(), static_cast<int32>(Due.Plate), ServerNow, Due.ContactServerTime,
		Due.ArrivalServerTime, Due.bLate);
	UE_LOGF(LogStealth, Log, "[HUD] %ls plate %ls at %.3f (contact %.3f, arrival %.3f, %ls%ls%ls)", *GetName(), LexOwnResultPlate(Due.Plate), ServerNow,
		Due.ContactServerTime, Due.ArrivalServerTime, LexOwnResultTimeSource(Due.Source), Due.bLate ? TEXT(", late") : TEXT(""),
		Due.bInRoleChange ? TEXT(", in role change") : TEXT(""));
	if (Due.Kind != ECommandReplyKind::StrikeResult)
	{
		return;
	}
	// The tally ticks with the plate (HUD-R7, UI-ASN-2), never lower within a round (a re-push may already show more).
	const uint32 Round = CurrentRoundIndex();
	if (Round != ShownTallyRoundIndex || static_cast<int32>(Due.Strike.TallyAfter) >= ShownWrongTargetTally)
	{
		ShownWrongTargetTally = Due.Strike.TallyAfter;
		ShownTallyRoundIndex = Round;
		LastShownTallySeq = FMath::Max(LastShownTallySeq, Due.Strike.ClientCommandSeq.Seq);
		StoreShownTally();
	}
	LogOwnPresentation(EOwnPresentationEvent::TallyTick, FString(), ShownWrongTargetTally, ServerNow, Due.ContactServerTime);
	// Story asn-021: the attacker's transient grade on the plate frame for a kill or a reversal (a body met a body); a whiff
	// (NoContact) plays none.
	if (Due.Strike.Result != EStrikeResult::NoContact && GetLocalPlayer() != nullptr)
	{
		if (UEncounterViewSubsystem* View = GetLocalPlayer()->GetSubsystem<UEncounterViewSubsystem>())
		{
			View->ApplyTransientGrade(ETransientGradeCause::AttackerContact, ServerNow, Due.ContactServerTime >= 0.0 ? Due.ContactServerTime : ServerNow);
		}
	}
	if (Due.Strike.bExiled)
	{
		ExileCardAt = ServerNow + UOwnResultPlateSettings::Get()->ExileBeatSec;
	}
}

void AStealthPlayerController::AddHudMessage(const FText& Text, bool bDanger, float Duration, double ArrivalServerTime)
{
	FStealthHudMessage& Message = Messages.AddDefaulted_GetRef();
	Message.Text = Text;
	Message.bDanger = bDanger;
	Message.ExpireTime = GetWorld()->GetTimeSeconds() + FMath::Max(Duration, MinHudMessageSeconds);
	if (Messages.Num() > MaxHudMessages)
	{
		Messages.RemoveAt(0);
	}
	LogOwnPresentation(EOwnPresentationEvent::HudMessage, Text.ToString(), bDanger ? 1 : 0, UEncounterPresentationSubsystem::EstimatedServerNow(GetWorld()), -1.0, ArrivalServerTime);
	UE_LOG(LogStealth, Log, TEXT("[HUD] %s (netmode %d) message: %s"), *GetName(), (int32)GetNetMode(), *Text.ToString());
}

void AStealthPlayerController::LogOwnPresentation(EOwnPresentationEvent Event, const FString& Text, int32 Value, double ServerNow, double Due, double Arrival, bool bLate)
{
	if (OwnPresentationLog.Num() >= MaxOwnPresentationRecords)
	{
		OwnPresentationLog.RemoveAt(0, OwnPresentationLog.Num() / 2);
	}
	FOwnPresentationRecord& Record = OwnPresentationLog.AddDefaulted_GetRef();
	Record.Event = Event;
	Record.Text = Text;
	Record.Value = Value;
	Record.ServerNow = ServerNow;
	Record.DueServerTime = Due;
	Record.ArrivalServerTime = Arrival;
	Record.bLate = bLate;
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
	// ASN-R23 hold buffer (stories asn-024 / asn-017): a server line is never shown in this RPC handler. It is queued and the
	// per-frame TickOwnPresentation releases it once no own outcome is unpresented (a pending plate, the own fall not begun),
	// so a line sent with a death write (a round end) waits for the FDeathState that arrives in the same frame.
	const double ServerNow = UEncounterPresentationSubsystem::EstimatedServerNow(GetWorld());
	if (!IsLocalController())
	{
		return;
	}
	FHeldMessage& Held = HeldMessages.AddDefaulted_GetRef();
	Held.Text = Text;
	Held.bDanger = bDanger;
	Held.Duration = Duration;
	Held.ArrivalServerTime = ServerNow;
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
