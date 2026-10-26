#include "View/Input/PawnCommandClient.h"

#include "View/Core/StealthLocomotionModel.h"

#include "CollisionQueryParams.h"
#include "CommonUITypes.h"
#include "Input/CommonUIActionRouterBase.h"
#include "Engine/HitResult.h"
#include "Engine/LocalPlayer.h"
#include "Framework/Application/SlateApplication.h"
#include "Engine/NetDriver.h"
#include "Engine/PackageMapClient.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Server/Core/StealthPawnCommandComponent.h"
#include "View/Core/StealthPawn.h"
#include "View/Core/WalkSlowerScale.h"

// ---- FPawnCommandClientCore -----------------------------------------------------------------------------------

namespace PawnCommandClientPrivate
{
	ECommandKind BeginKind(FPawnCommandClientCore::EHeld Held)
	{
		return Held == FPawnCommandClientCore::EHeld::Interact ? ECommandKind::InteractBegin : ECommandKind::PrepareBegin;
	}

	ECommandKind EndKind(FPawnCommandClientCore::EHeld Held)
	{
		return Held == FPawnCommandClientCore::EHeld::Interact ? ECommandKind::InteractEnd : ECommandKind::PrepareEnd;
	}

	/** The asset folder of story iif-007 (InputAssetSpec.h InputFolder). */
	const TCHAR* const InputFolder = TEXT("/Game/StealthGame/Input");

	const FName ActionNames[] = {
		TEXT("IA_Move"), TEXT("IA_Look"), TEXT("IA_CameraToggle"), TEXT("IA_ShoulderSwap"), TEXT("IA_WalkSlower"), TEXT("IA_Ping"), TEXT("IA_Menu"),
		TEXT("IA_Run"), TEXT("IA_Traverse"), TEXT("IA_Interact"), TEXT("IA_Prepare"), TEXT("IA_Attack"), TEXT("IA_Hide"),
	};

	FSoftObjectPath AssetPath(const FName Name)
	{
		return FSoftObjectPath(FString::Printf(TEXT("%s/%s.%s"), InputFolder, *Name.ToString(), *Name.ToString()));
	}
}

FClientCommandSeq FPawnCommandClientCore::NextSeq()
{
	FClientCommandSeq Seq;
	Seq.PossessionEpoch = OwnerPossessionEpoch;
	Seq.Seq = ++LastSeq;
	return Seq;
}

void FPawnCommandClientCore::SendCommand(ECommandKind Kind, const FNetCommandArgs& Args)
{
	FNetPawnCommand Cmd;
	Cmd.Kind = Kind;
	Cmd.Seq = NextSeq();
	Cmd.Args = Args;
	if (Send)
	{
		Send(Cmd);
	}
}

bool FPawnCommandClientCore::OnHeldStarted(EHeld Held, const FNetCommandArgs& Args)
{
	bool& bOutstanding = Outstanding[static_cast<int32>(Held)];
	if (bOutstanding)
	{
		return false;
	}
	bOutstanding = true;
	SendCommand(PawnCommandClientPrivate::BeginKind(Held), Args);
	return true;
}

bool FPawnCommandClientCore::OnHeldTerminal(EHeld Held)
{
	bool& bOutstanding = Outstanding[static_cast<int32>(Held)];
	if (!bOutstanding)
	{
		// The engine's later Completed/Canceled after a flush already ended this press: swallowed (idempotent End).
		++Swallowed;
		return false;
	}
	bOutstanding = false;
	SendCommand(PawnCommandClientPrivate::EndKind(Held), FNetCommandArgs());
	return true;
}

void FPawnCommandClientCore::OnInstant(ECommandKind Kind, const FNetCommandArgs& Args)
{
	SendCommand(Kind, Args);
}

int32 FPawnCommandClientCore::EndAllOutstanding(EEndReason Reason)
{
	int32 Ended = 0;
	for (int32 Index = 0; Index < static_cast<int32>(EHeld::Count); ++Index)
	{
		if (Outstanding[Index])
		{
			Outstanding[Index] = false;
			SendCommand(PawnCommandClientPrivate::EndKind(static_cast<EHeld>(Index)), FNetCommandArgs());
			++Ended;
		}
	}
	return Ended;
}

bool FPawnCommandClientCore::TryAcceptCameraToggle(double Now, double LockoutSec)
{
	if (LastAcceptedCameraToggle.IsSet() && Now - LastAcceptedCameraToggle.GetValue() < LockoutSec)
	{
		return false;
	}
	LastAcceptedCameraToggle = Now;
	return true;
}

// ---- UPawnCommandClient ---------------------------------------------------------------------------------------

UPawnCommandClient::UPawnCommandClient()
{
	SetIsReplicatedByDefault(false);
	PrimaryComponentTick.bCanEverTick = false;

	for (const FName Name : PawnCommandClientPrivate::ActionNames)
	{
		Actions.Add(Name, TSoftObjectPtr<UInputAction>(PawnCommandClientPrivate::AssetPath(Name)));
	}
	CommonContext = TSoftObjectPtr<UInputMappingContext>(PawnCommandClientPrivate::AssetPath(TEXT("IMC_Common")));
	ActiveContext = TSoftObjectPtr<UInputMappingContext>(PawnCommandClientPrivate::AssetPath(TEXT("IMC_Active")));
	InformantContext = TSoftObjectPtr<UInputMappingContext>(PawnCommandClientPrivate::AssetPath(TEXT("IMC_Informant")));
}

void UPawnCommandClient::BeginPlay()
{
	Super::BeginPlay();
	TWeakObjectPtr<UPawnCommandClient> WeakThis(this);
	Core.SetSend([WeakThis](const FNetPawnCommand& Cmd)
	{
		UPawnCommandClient* Self = WeakThis.Get();
		if (Self == nullptr)
		{
			return;
		}
		if (Self->Uplink)
		{
			Self->Uplink->SendPawnCommand(Cmd, Self->PendingHostLocalRef.Get());
		}
		Self->OnOwnerCommandPredicted.Broadcast(Cmd.Kind, Cmd.Seq);
	});

	// ADR-0011 section 9 / ADR-0021 section 7 (second line): a Common UI switch to Menu ends every outstanding Begin
	// synchronously, in the router's callback, before the router's next-tick FlushPressedKeys (E11, spike V5).
	const APlayerController* Controller = GetOwningController();
	if (const ULocalPlayer* Local = Controller && Controller->IsLocalController() ? Controller->GetLocalPlayer() : nullptr)
	{
		if (UCommonUIActionRouterBase* Router = Local->GetSubsystem<UCommonUIActionRouterBase>())
		{
			bMenuMode = Router->GetActiveInputMode() == ECommonInputMode::Menu;
			Router->OnActiveInputModeChanged().AddWeakLambda(this, [this](ECommonInputMode Mode)
			{
				bMenuMode = Mode == ECommonInputMode::Menu;
				if (bMenuMode)
				{
					Core.EndAllOutstanding(FPawnCommandClientCore::EEndReason::MenuOpen);
				}
				// Move is zero for as long as the menu is open (story iif-013 case b); the held vector resumes after it.
				SendMove(RawMove);
			});
		}
		// Window focus loss ends every outstanding Begin (ADR-0011 section 9). The OS-level alt-tab is a manual check.
		if (FSlateApplication::IsInitialized())
		{
			FocusHandle = FSlateApplication::Get().OnApplicationActivationStateChanged().AddWeakLambda(this, [this](bool bActive)
			{
				if (!bActive)
				{
					NotifyFocusLost();
				}
			});
		}
	}
}

void UPawnCommandClient::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (FocusHandle.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().OnApplicationActivationStateChanged().Remove(FocusHandle);
	}
	FocusHandle.Reset();
	Super::EndPlay(EndPlayReason);
}

int32 UPawnCommandClient::SetInformantContext(bool bInformant)
{
	if (bInformant == bInformantContext)
	{
		return 0;
	}
	// The Ends go out BEFORE the context change takes effect.
	const int32 Ended = Core.EndAllOutstanding(FPawnCommandClientCore::EEndReason::ContextSwap);
	bInformantContext = bInformant;
	const APlayerController* Controller = GetOwningController();
	const ULocalPlayer* Local = Controller ? Controller->GetLocalPlayer() : nullptr;
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = Local ? Local->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr)
	{
		FModifyContextOptions Options;
		Options.bIgnoreAllPressedKeysUntilRelease = true;
		UInputMappingContext* Remove = (bInformant ? ActiveContext : InformantContext).LoadSynchronous();
		UInputMappingContext* Add = (bInformant ? InformantContext : ActiveContext).LoadSynchronous();
		if (Remove)
		{
			Subsystem->RemoveMappingContext(Remove, Options);
		}
		if (Add)
		{
			Subsystem->AddMappingContext(Add, ActiveContextPriority, Options);
		}
	}
	return Ended;
}

APlayerController* UPawnCommandClient::GetOwningController() const
{
	return Cast<APlayerController>(GetOwner());
}

APawn* UPawnCommandClient::GetControlledPawn() const
{
	const APlayerController* Controller = GetOwningController();
	return Controller ? Controller->GetPawn() : nullptr;
}

UInputAction* UPawnCommandClient::LoadAction(FName Name) const
{
	const TSoftObjectPtr<UInputAction>* Soft = Actions.Find(Name);
	return Soft ? Soft->LoadSynchronous() : nullptr;
}

float UPawnCommandClient::GetWalkSlowerPace() const
{
	return StealthWalkSlower::PaceFromReduction(WalkSlowerReduction);
}

void UPawnCommandClient::ApplyGameplayContexts()
{
	const APlayerController* Controller = GetOwningController();
	const ULocalPlayer* Local = Controller ? Controller->GetLocalPlayer() : nullptr;
	UEnhancedInputLocalPlayerSubsystem* Subsystem = Local ? Local->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	if (Subsystem == nullptr)
	{
		return;
	}
	FModifyContextOptions Options;
	Options.bIgnoreAllPressedKeysUntilRelease = true;
	if (UInputMappingContext* Common = CommonContext.LoadSynchronous())
	{
		Subsystem->AddMappingContext(Common, CommonContextPriority, Options);
	}
	if (UInputMappingContext* Active = ActiveContext.LoadSynchronous())
	{
		Subsystem->AddMappingContext(Active, ActiveContextPriority, Options);
	}
}

void UPawnCommandClient::BindInput(UEnhancedInputComponent& Input)
{
	using EHeld = FPawnCommandClientCore::EHeld;
	TWeakObjectPtr<UPawnCommandClient> WeakThis(this);
	auto Bind = [&Input, this](FName Name, ETriggerEvent Event, TFunction<void(const FInputActionInstance&)> Handler)
	{
		if (UInputAction* Action = LoadAction(Name))
		{
			Input.BindActionInstanceLambda(Action, Event, [Handler](const FInputActionInstance& Instance) { Handler(Instance); });
		}
	};
	auto Guard = [WeakThis](TFunction<void(UPawnCommandClient&, const FInputActionInstance&)> Body)
	{
		return [WeakThis, Body](const FInputActionInstance& Instance)
		{
			if (UPawnCommandClient* Self = WeakThis.Get())
			{
				Body(*Self, Instance);
			}
		};
	};

	// Continuous: Move every input frame (edges carry no command), Look into the control rotation.
	Bind(TEXT("IA_Move"), ETriggerEvent::Triggered, Guard([](UPawnCommandClient& Self, const FInputActionInstance& I) { Self.HandleMove(I); }));
	Bind(TEXT("IA_Move"), ETriggerEvent::Completed, Guard([](UPawnCommandClient& Self, const FInputActionInstance& I) { Self.HandleMoveCompleted(I); }));
	Bind(TEXT("IA_Look"), ETriggerEvent::Triggered, Guard([](UPawnCommandClient& Self, const FInputActionInstance& I) { Self.HandleLook(I); }));
	Bind(TEXT("IA_WalkSlower"), ETriggerEvent::Triggered, Guard([](UPawnCommandClient& Self, const FInputActionInstance& I) { Self.HandleWalkSlower(I); }));
	Bind(TEXT("IA_WalkSlower"), ETriggerEvent::Completed, Guard([](UPawnCommandClient& Self, const FInputActionInstance& I) { Self.HandleWalkSlower(I); }));
	Bind(TEXT("IA_Run"), ETriggerEvent::Started, Guard([](UPawnCommandClient& Self, const FInputActionInstance&) { Self.bRunHeld = true; Self.SendMove(Self.RawMove); }));
	Bind(TEXT("IA_Run"), ETriggerEvent::Completed, Guard([](UPawnCommandClient& Self, const FInputActionInstance&) { Self.bRunHeld = false; Self.SendMove(Self.RawMove); }));
	Bind(TEXT("IA_Run"), ETriggerEvent::Canceled, Guard([](UPawnCommandClient& Self, const FInputActionInstance&) { Self.bRunHeld = false; Self.SendMove(Self.RawMove); }));

	// Held: Started -> Begin; both Completed and Canceled -> End (E1).
	const TPair<FName, EHeld> HeldActions[] = { { TEXT("IA_Interact"), EHeld::Interact }, { TEXT("IA_Prepare"), EHeld::Prepare } };
	for (const TPair<FName, EHeld>& Held : HeldActions)
	{
		const EHeld Which = Held.Value;
		Bind(Held.Key, ETriggerEvent::Started, Guard([Which](UPawnCommandClient& Self, const FInputActionInstance&)
		{
			// Spike V5 finding: a press processed in the frame after a menu push would otherwise start a hold under the menu.
			if (!Self.bMenuMode)
			{
				Self.Core.OnHeldStarted(Which);
			}
		}));
		Bind(Held.Key, ETriggerEvent::Completed, Guard([Which](UPawnCommandClient& Self, const FInputActionInstance&) { Self.Core.OnHeldTerminal(Which); }));
		Bind(Held.Key, ETriggerEvent::Canceled, Guard([Which](UPawnCommandClient& Self, const FInputActionInstance&) { Self.Core.OnHeldTerminal(Which); }));
	}

	// Instant: Started only.
	Bind(TEXT("IA_Traverse"), ETriggerEvent::Started, Guard([](UPawnCommandClient& Self, const FInputActionInstance&)
	{
		if (!Self.bMenuMode)
		{
			Self.Core.OnInstant(ECommandKind::Traverse);
		}
	}));
	Bind(TEXT("IA_Attack"), ETriggerEvent::Started, Guard([](UPawnCommandClient& Self, const FInputActionInstance&)
	{
		if (Self.bMenuMode)
		{
			return;
		}
		APawn* HostLocalRef = nullptr;
		const FNetCommandArgs Args = Self.BuildAttackArgs(HostLocalRef);
		// The weak ref rides beside the command to the uplink (used only by the listen host's local ingress, E7/V6).
		Self.PendingHostLocalRef = HostLocalRef;
		Self.Core.OnInstant(ECommandKind::Attack, Args);
		Self.PendingHostLocalRef.Reset();
	}));
	Bind(TEXT("IA_Hide"), ETriggerEvent::Started, Guard([](UPawnCommandClient& Self, const FInputActionInstance&)
	{
		// Hide toggles Enter/Exit by state (IIF R15); the prototype has no hidden state on the owner yet, so Enter.
		if (!Self.bMenuMode)
		{
			Self.Core.OnInstant(ECommandKind::HideEnter);
		}
	}));

	// Client-local, never commands.
	Bind(TEXT("IA_CameraToggle"), ETriggerEvent::Started, Guard([](UPawnCommandClient& Self, const FInputActionInstance& I) { Self.HandleCameraToggle(I); }));
	Bind(TEXT("IA_ShoulderSwap"), ETriggerEvent::Started, Guard([](UPawnCommandClient& Self, const FInputActionInstance&) { Self.OnShoulderSwap.Broadcast(); }));
	Bind(TEXT("IA_Ping"), ETriggerEvent::Started, Guard([](UPawnCommandClient& Self, const FInputActionInstance& I) { Self.HandlePing(I); }));
}

void UPawnCommandClient::NotifyPossessionChanged(APawn* NewPawn)
{
	// ADR-0011 section 9: every outstanding Begin is ended BEFORE the change takes effect; a fresh press is required.
	Core.EndAllOutstanding(FPawnCommandClientCore::EEndReason::PossessionChange);
	AActor* Controller = GetOwner();
	APawn* Old = LastPawn.Get();
	if (Old && Old != NewPawn)
	{
		// The old body keeps no intent from this controller (it would otherwise re-apply it every tick).
		if (UPawnCommandComponent* OldCommand = Cast<AStealthPawn>(Old) ? Cast<AStealthPawn>(Old)->GetPawnCommand() : nullptr)
		{
			OldCommand->Move(FVector2D::ZeroVector, EGait::Walk);
			if (Controller)
			{
				OldCommand->RemoveTickPrerequisiteActor(Controller);
			}
		}
	}
	LastPawn = NewPawn;
	// The new body's command component applies this controller's Move intent after the controller's input tick. The held
	// vector is applied to NewPawn explicitly: the controller's own pawn pointer may not be updated yet here.
	if (UPawnCommandComponent* Command = Cast<AStealthPawn>(NewPawn) ? Cast<AStealthPawn>(NewPawn)->GetPawnCommand() : nullptr)
	{
		if (Controller)
		{
			Command->AddTickPrerequisiteActor(Controller);
		}
		SendMoveTo(Command, RawMove);
	}
}

void UPawnCommandClient::SendMove(const FVector2D& Raw)
{
	AStealthPawn* Pawn = Cast<AStealthPawn>(GetControlledPawn());
	SendMoveTo(Pawn ? Pawn->GetPawnCommand() : nullptr, Raw);
}

void UPawnCommandClient::SendMoveTo(UPawnCommandComponent* Command, const FVector2D& Raw)
{
	RawMove = Raw;
	if (Command == nullptr)
	{
		LastSentMove = FVector2D::ZeroVector;
		return;
	}
	if (bMenuMode)
	{
		LastSentMove = FVector2D::ZeroVector;
		Command->Move(FVector2D::ZeroVector, EGait::Walk);
		return;
	}
	// Keyboard when a digital Move key is down; otherwise the stick (left unscaled: it already covers the pace set).
	// Known limit: keys rebound away from WASD read as the stick until rebinding (UEnhancedInputUserSettings) lands.
	const APlayerController* Controller = GetOwningController();
	const bool bKeyboard = Controller
		&& (Controller->IsInputKeyDown(EKeys::W) || Controller->IsInputKeyDown(EKeys::A) || Controller->IsInputKeyDown(EKeys::S) || Controller->IsInputKeyDown(EKeys::D));
	LastSentMove = StealthWalkSlower::ScaleMove(Raw, bKeyboard ? EMoveInputDevice::Keyboard : EMoveInputDevice::Gamepad, GetWalkSlowerPace(), bRunHeld);
	if (!bKeyboard)
	{
		// SCL R5.1: the owning client applies the 0.2 stick dead zone before sending (a 0.19 reading is sent as 0).
		LastSentMove = StealthLocomotion::ApplyStickDeadZone(LastSentMove);
	}
	Command->Move(LastSentMove, GetGait());
}

void UPawnCommandClient::HandleMove(const FInputActionInstance& Instance)
{
	SendMove(Instance.GetValue().Get<FVector2D>());
}

void UPawnCommandClient::HandleMoveCompleted(const FInputActionInstance& Instance)
{
	SendMove(FVector2D::ZeroVector);
}

void UPawnCommandClient::HandleLook(const FInputActionInstance& Instance)
{
	APlayerController* Controller = GetOwningController();
	if (Controller && !bMenuMode)
	{
		const FVector2D Look = Instance.GetValue().Get<FVector2D>();
		// Mouse/stick up looks up: pitch input is negated (the project keeps the legacy input scales, DefaultInput.ini).
		Controller->AddYawInput(Look.X);
		Controller->AddPitchInput(-Look.Y);
	}
}

void UPawnCommandClient::HandleWalkSlower(const FInputActionInstance& Instance)
{
	WalkSlowerReduction = Instance.GetTriggerEvent() == ETriggerEvent::Completed ? 0.f : Instance.GetValue().Get<float>();
	SendMove(RawMove);
}

void UPawnCommandClient::HandleCameraToggle(const FInputActionInstance& Instance)
{
	const UWorld* World = GetWorld();
	if (World && Core.TryAcceptCameraToggle(World->GetRealTimeSeconds(), CameraToggleLockoutSec))
	{
		OnCameraToggleAccepted.Broadcast();
	}
}

void UPawnCommandClient::HandlePing(const FInputActionInstance& Instance)
{
	const APlayerController* Controller = GetOwningController();
	if (Controller == nullptr || Uplink == nullptr)
	{
		return;
	}
	FVector ViewLocation;
	FRotator ViewRotation;
	Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
	const FVector End = ViewLocation + ViewRotation.Vector() * PingTraceCm;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(StealthPing), false, Controller->GetPawn());
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, ViewLocation, End, ECC_Visibility, Params);
	Uplink->SendTeamPing(bHit ? Hit.ImpactPoint : End);
}

FNetCommandArgs UPawnCommandClient::BuildAttackArgs(APawn*& OutHostLocalRef) const
{
	// IIF R10: WorldPoint always; PawnRef only for a pawn the camera ray hit within reach. No lock-on, snapping or assist.
	// The aim ray is the player's view point until Camera's GetAimRay lands (ADR-0015).
	FNetCommandArgs Args;
	OutHostLocalRef = nullptr;
	const APlayerController* Controller = GetOwningController();
	const APawn* Self = Controller ? Controller->GetPawn() : nullptr;
	if (Controller == nullptr || Self == nullptr)
	{
		return Args;
	}
	FVector ViewLocation;
	FRotator ViewRotation;
	Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
	const FVector Direction = ViewRotation.Vector();
	// Reach is measured from the attacker's body; the ray starts at the camera, so extend it by the camera distance.
	const double CameraToBody = FVector::Dist(ViewLocation, Self->GetActorLocation());
	const FVector End = ViewLocation + Direction * (CameraToBody + AimRangeCm);
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(StealthAttackAim), false, Self);
	FVector Point = End;
	if (GetWorld()->LineTraceSingleByChannel(Hit, ViewLocation, End, ECC_Pawn, Params) || GetWorld()->LineTraceSingleByChannel(Hit, ViewLocation, End, ECC_Visibility, Params))
	{
		Point = Hit.ImpactPoint;
		APawn* HitPawn = Cast<APawn>(Hit.GetActor());
		if (HitPawn && HitPawn != Self && FVector::Dist(HitPawn->GetActorLocation(), Self->GetActorLocation()) <= AimRangeCm)
		{
			OutHostLocalRef = HitPawn;
			if (const UNetDriver* Driver = GetWorld()->GetNetDriver())
			{
				if (Driver->GetNetGuidCache().IsValid())
				{
					Args.SetPawnRefGuid(Driver->GetNetGuidCache()->GetNetGUID(HitPawn));
				}
			}
		}
	}
	// The point is kept within reach of the body (the server clamps again at ingress, story iif-020).
	const FVector FromBody = Point - Self->GetActorLocation();
	Args.WorldPoint = FVector_NetQuantize10(Self->GetActorLocation() + FromBody.GetClampedToMaxSize(AimRangeCm));
	return Args;
}

void UPawnCommandClient::SetOwnerState(EParticipantState RawState)
{
	if (RawState != RawOwnState)
	{
		RawOwnState = RawState;
		const UWorld* World = GetWorld();
		RawStateArrival = World ? World->GetRealTimeSeconds() : 0.0;
	}
	ReevaluateRoleContext();
}

void UPawnCommandClient::SetPresentedOwnRole(EPresentedOwnRole Role)
{
	bPresenterBound = true;
	PresentedRole = Role;
	ReevaluateRoleContext();
}

void UPawnCommandClient::ReevaluateRoleContext()
{
	const UWorld* World = GetWorld();
	const double Now = World ? World->GetRealTimeSeconds() : 0.0;
	SetInformantContext(StealthOwnerContext::UseInformantContext(RawOwnState, bPresenterBound, PresentedRole, Now, RawStateArrival, PresentedRoleCapSec));
}
