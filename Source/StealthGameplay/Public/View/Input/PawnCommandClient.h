#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "View/Core/OwnerContextGate.h"
#include "View/Core/StealthGait.h"
#include "View/NetPawnCommand.h"

#include "PawnCommandClient.generated.h"

class APawn;
class APlayerController;
class UEnhancedInputComponent;
class UInputAction;
class UInputMappingContext;
struct FInputActionInstance;

/**
 * The owning client's command builder (ADR-0011 section 9; stories iif-094, iif-012, iif-014, iif-017): edges only,
 * one Begin per press, idempotent Ends, release safety and the client-local camera toggle. Plain C++ so the edge rules
 * are unit-tested headless; UPawnCommandClient adapts Enhanced Input to it.
 *
 * Rules:
 *   - A held command (Interact, Prepare) sends Begin on the first Started of a press and End on the first Completed
 *     or Canceled; a second Started while the Begin is outstanding sends nothing (no repeat, IIF R4).
 *   - EndAllOutstanding sends End for every outstanding Begin (context swap, menu, focus loss, possession change,
 *     ClientRestart); afterwards the engine's later Completed/Canceled for that press is swallowed (idempotent Ends),
 *     and a fresh press is needed for a new Begin.
 *   - Instant commands (Traverse, Attack, Hide) send one command per Started.
 *   - While the Common UI router is in Menu mode, UPawnCommandClient starts no command (spike V5).
 *   - Every command takes (OwnerPossessionEpoch, ++Seq); Seq is strictly increasing per controller.
 *   - The camera toggle never becomes a command: a press is accepted when at least CameraToggleLockoutSec has passed
 *     since the last ACCEPTED toggle; ignored presses do not extend the lockout (story iif-014 pin).
 */
class STEALTHGAMEPLAY_API FPawnCommandClientCore
{
public:
	/** The held command families (one outstanding Begin each). */
	enum class EHeld : uint8
	{
		Interact,
		Prepare,
		Count,
	};

	/** Why EndAllOutstanding ran (ADR-0021 EEndReason). */
	enum class EEndReason : uint8
	{
		ContextSwap,
		MenuOpen,
		FocusLoss,
		PossessionChange,
		ClientRestart,
	};

	using FSend = TFunction<void(const FNetPawnCommand&)>;

	explicit FPawnCommandClientCore(FSend InSend = FSend()) : Send(MoveTemp(InSend)) {}

	void SetSend(FSend InSend) { Send = MoveTemp(InSend); }

	/** Started of a held action. Sends Begin unless one is already outstanding. Returns true when a Begin was sent. */
	bool OnHeldStarted(EHeld Held, const FNetCommandArgs& Args = FNetCommandArgs());

	/** Completed or Canceled of a held action. Sends End for an outstanding Begin; otherwise swallowed. */
	bool OnHeldTerminal(EHeld Held);

	/** Started of an instant action (Traverse, Attack, HideEnter, HideExit). Always sends. */
	void OnInstant(ECommandKind Kind, const FNetCommandArgs& Args = FNetCommandArgs());

	/** Sends End for every outstanding Begin, synchronously; returns how many. */
	int32 EndAllOutstanding(EEndReason Reason);

	/** True while a Begin of Held has been sent and not yet ended. */
	bool IsOutstanding(EHeld Held) const { return Outstanding[static_cast<int32>(Held)]; }

	/** The server-delivered possession epoch (ClientSetPossessionEpoch). */
	void SetOwnerPossessionEpoch(uint16 Epoch) { OwnerPossessionEpoch = Epoch; }
	uint16 GetOwnerPossessionEpoch() const { return OwnerPossessionEpoch; }

	/** The last sequence used (0 before any command). */
	uint32 GetLastSeq() const { return LastSeq; }

	/** Terminal edges swallowed because their Begin had already been ended (idempotent Ends). */
	int32 GetSwallowedCount() const { return Swallowed; }

	/** The camera toggle lockout: true when a press at Now is accepted (and then becomes the new anchor). */
	bool TryAcceptCameraToggle(double Now, double LockoutSec);

private:
	FClientCommandSeq NextSeq();
	void SendCommand(ECommandKind Kind, const FNetCommandArgs& Args);

	FSend Send;
	bool Outstanding[static_cast<int32>(EHeld::Count)] = {};
	uint16 OwnerPossessionEpoch = 0;
	uint32 LastSeq = 0;
	int32 Swallowed = 0;
	TOptional<double> LastAcceptedCameraToggle;
};

/** Raised when the owner sends a command (presentation only: owner prediction, ADR-0011 section 9). */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnOwnerCommandPredicted, ECommandKind, FClientCommandSeq);

/** How the client's transport is reached (implemented by the root controller). */
class IPawnCommandUplink
{
public:
	virtual ~IPawnCommandUplink() = default;

	/** Sends one discrete command (ServerPawnCommand, or IngestCommand(HostLocal) on the listen host). */
	virtual void SendPawnCommand(const FNetPawnCommand& Cmd, APawn* HostLocalPawnRef) = 0;

	/** IA_Ping: the controller ping RPC (never a pawn command). */
	virtual void SendTeamPing(const FVector& Location) = 0;
};

/**
 * The client-only command builder component on the controller (ADR-0011 section 9). The controller's
 * SetupInputComponent calls BindInput; the component adds the gameplay mapping contexts (it is the only code that adds
 * or removes IMC_* contexts), turns Enhanced Input edges into commands through FPawnCommandClientCore, feeds Move to the
 * pawn's command component every input frame (walk-slower scaled), turns Look into control rotation, and raises the
 * client-local camera toggle and shoulder swap. Inert on a non-local controller.
 */
UCLASS(NotBlueprintType, NotBlueprintable, ClassGroup = Stealth)
class STEALTHGAMEPLAY_API UPawnCommandClient : public UActorComponent
{
	GENERATED_BODY()

public:
	UPawnCommandClient();

	/** Binds every IA_* action of the gameplay contexts on Input (called from the controller's SetupInputComponent). */
	void BindInput(UEnhancedInputComponent& Input);

	/** Adds IMC_Common and IMC_Active to the local player (bIgnoreAllPressedKeysUntilRelease set, ADR-0011 E3). */
	void ApplyGameplayContexts();

	/** Sets the uplink the commands go through (the owning controller). */
	void SetUplink(IPawnCommandUplink* InUplink) { Uplink = InUplink; }

	/** The possessed pawn changed: every outstanding Begin is ended first, then the new pawn's tick follows the controller. */
	void NotifyPossessionChanged(APawn* NewPawn);

	/** The window lost focus: every outstanding Begin is ended at once (ADR-0011 section 9; story iif-013). */
	void NotifyFocusLost() { Core.EndAllOutstanding(FPawnCommandClientCore::EEndReason::FocusLoss); }

	/**
	 * Swaps the role context: IMC_Informant replaces IMC_Active (or back). Every outstanding Begin is ended BEFORE the
	 * swap, and both the remove and the add set bIgnoreAllPressedKeysUntilRelease, so a held key needs a fresh press
	 * (ADR-0011 section 9, spike V1 3c/3d). Returns how many Ends were sent. The Roster-driven trigger is story iif-071.
	 */
	int32 SetInformantContext(bool bInformant);

	/**
	 * The owner's own raw Roster state arrived (its C-TeamView owner part; story iif-071). The role context follows it
	 * through StealthOwnerContext::UseInformantContext, so the swap (and its flush of held Begins) happens in the frame
	 * the state arrives; with no HUD presenter bound (the MVP) the raw state decides at once.
	 */
	void SetOwnerState(EParticipantState RawState);

	/** The HUD presents the owner's role (ADR-0021 IOwnerCommandClientView::SetPresentedOwnRole); binds the presenter. */
	void SetPresentedOwnRole(EPresentedOwnRole Role);

	/** True while IMC_Informant is the role context. */
	bool IsInformantContext() const { return bInformantContext; }

	/** True while the Common UI router's input mode is Menu (no command starts; Move and Look are zero). */
	bool IsMenuMode() const { return bMenuMode; }

	/** ClientSetPossessionEpoch arrived. */
	void SetOwnerPossessionEpoch(uint16 Epoch) { Core.SetOwnerPossessionEpoch(Epoch); }

	/** Synchronous idempotent Ends (ADR-0021 IOwnerCommandClientView::EndAllOutstanding). */
	int32 EndAllOutstanding(FPawnCommandClientCore::EEndReason Reason) { return Core.EndAllOutstanding(Reason); }

	const FPawnCommandClientCore& GetCore() const { return Core; }

	/** The Attack aim range (prototype AssassinationRange until Camera's GetAimRay and the tuning key land). */
	void SetAimRangeCm(float InRangeCm) { AimRangeCm = InRangeCm; }

	/** The camera toggle lockout (IIF CameraToggleLockoutSec, default 0.30 s; client-local). */
	void SetCameraToggleLockoutSec(double InSec) { CameraToggleLockoutSec = InSec; }

	/** Owner prediction (presentation only). */
	FOnOwnerCommandPredicted OnOwnerCommandPredicted;

	/** Accepted camera toggles and shoulder swaps (client-local; Camera owns the effect). */
	FSimpleMulticastDelegate OnCameraToggleAccepted;
	FSimpleMulticastDelegate OnShoulderSwap;

	/** The current Move as sent (after the walk-slower) and the gait, for tests and the dev HUD. */
	FVector2D GetLastSentMove() const { return LastSentMove; }
	EGait GetGait() const { return bRunHeld ? EGait::Run : EGait::Walk; }
	float GetWalkSlowerPace() const;

	/** The gameplay input assets (data: /Game/StealthGame/Input, story iif-007). */
	UPROPERTY(EditDefaultsOnly, Category = "Stealth|Input")
	TMap<FName, TSoftObjectPtr<UInputAction>> Actions;

	UPROPERTY(EditDefaultsOnly, Category = "Stealth|Input")
	TSoftObjectPtr<UInputMappingContext> CommonContext;

	UPROPERTY(EditDefaultsOnly, Category = "Stealth|Input")
	TSoftObjectPtr<UInputMappingContext> ActiveContext;

	UPROPERTY(EditDefaultsOnly, Category = "Stealth|Input")
	TSoftObjectPtr<UInputMappingContext> InformantContext;

	/** Context priorities: above the reparented sample's IMC_Sandbox, so these mappings consume their keys first. */
	UPROPERTY(EditDefaultsOnly, Category = "Stealth|Input")
	int32 CommonContextPriority = 10;

	UPROPERTY(EditDefaultsOnly, Category = "Stealth|Input")
	int32 ActiveContextPriority = 11;

	/**
	 * HudOwnRemovalHoldCapSec + HudRemovalCardDelaySec (ADR-0021 section 8): how long a bound presenter may hold the
	 * informant context back. Read only while a presenter is bound (none in the MVP); the HUD keys replace it when they are
	 * registered.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Stealth|Input")
	float PresentedRoleCapSec = 3.f;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	APlayerController* GetOwningController() const;
	APawn* GetControlledPawn() const;
	UInputAction* LoadAction(FName Name) const;
	void SendMove(const FVector2D& Raw);
	void SendMoveTo(class UPawnCommandComponent* Command, const FVector2D& Raw);
	FNetCommandArgs BuildAttackArgs(APawn*& OutHostLocalRef) const;

	void HandleMove(const FInputActionInstance& Instance);
	void HandleMoveCompleted(const FInputActionInstance& Instance);
	void HandleLook(const FInputActionInstance& Instance);
	void HandleWalkSlower(const FInputActionInstance& Instance);
	void HandleCameraToggle(const FInputActionInstance& Instance);
	void HandlePing(const FInputActionInstance& Instance);

	FPawnCommandClientCore Core;
	IPawnCommandUplink* Uplink = nullptr;
	TWeakObjectPtr<APawn> LastPawn;

	FVector2D RawMove = FVector2D::ZeroVector;
	FVector2D LastSentMove = FVector2D::ZeroVector;
	float WalkSlowerReduction = 0.f;
	bool bRunHeld = false;
	float AimRangeCm = 200.f;
	double CameraToggleLockoutSec = 0.30;
	/** The ping trace length (the prototype's value). */
	float PingTraceCm = 8000.f;
	/**
	 * True while the Common UI action router's active input mode is Menu: no new Begin or instant command starts (a press
	 * processed in the frame after a menu push would otherwise start a hold under the menu; spike V5 same-frame finding).
	 */
	bool bMenuMode = false;
	bool bInformantContext = false;
	EParticipantState RawOwnState = EParticipantState::RoundEnded;
	double RawStateArrival = 0.0;
	bool bPresenterBound = false;
	EPresentedOwnRole PresentedRole = EPresentedOwnRole::Unknown;
	void ReevaluateRoleContext();
	FDelegateHandle FocusHandle;
	/** Set only for the duration of one Attack send (the listen host's weak PawnRef, ADR-0011 section 2). */
	TWeakObjectPtr<APawn> PendingHostLocalRef;
};
