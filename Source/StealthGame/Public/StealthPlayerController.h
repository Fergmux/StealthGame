#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Server/StealthQueuePayloads.h"
#include "View/CommandReplyTypes.h"
#include "View/Input/PawnCommandClient.h"
#include "View/NetPawnCommand.h"
#include "View/OwnResultMatcher.h"
#include "StealthPlayerController.generated.h"

enum class ERoleChangeReason : uint8;
struct FPenaltyHudFrame;
struct FPenaltyExileCardView;
struct FPenaltyCountdownView;
struct FPenaltyTallyView;
enum class EPenaltyCardPhase : uint8;

class URosterTeamViewComponent;
class UObjectiveTeamViewComponent;
class UConcealmentOwnerViewComponent;
struct FObjectiveTeamView;
class UPawnCommandClient;
class UDefenceOwnerComponent;
class AStealthPawn;

/** A message shown on the HUD (kill confirmation, removal notice, stage progress...). */
USTRUCT()
struct FStealthHudMessage
{
	GENERATED_BODY()

	UPROPERTY()
	FText Text;

	UPROPERTY()
	bool bDanger = false;

	double ExpireTime = 0.0;
};

/** A teammate's momentary location ping (informant communication; never tracks an enemy). */
USTRUCT()
struct FStealthPing
{
	GENERATED_BODY()

	UPROPERTY()
	FVector Location = FVector::ZeroVector;

	UPROPERTY()
	FString SenderName;

	double ExpireTime = 0.0;
};

/** The owner HUD's view-model changes (story asn-024; the presentation-log hook of story asn-017). */
enum class EOwnPresentationEvent : uint8
{
	/** A result or defence plate entered the Result slot (Value: EOwnResultPlate). */
	ResultPlate,
	/** The "Wrong targets" tally changed with its plate (Value: the tally). */
	TallyTick,
	/** The exile card (the plate + ExileBeatSec). */
	ExileCard,
	/** The own-role removal card (OnRoleChangePresented; Value: ERoleChangeReason). */
	RemovalCard,
	/** The brace square appeared (Value 1) or was removed (Value 0). */
	BraceSquare,
	/** A HUD message line was shown (server or local). */
	HudMessage,
	/** The round-end presentation (banner) was released. */
	RoundEnd,
	/** The returning countdown's shown text changed (Value: the whole seconds; Text empty when hidden; story nkp-018). */
	Countdown,
	/** A result plate was pushed off the Result slot by the removal card (HUD-R9; story nkp-018). */
	PlatePreempted,
};

/** Log name of an owner presentation event. */
STEALTHGAME_API const TCHAR* LexOwnPresentationEvent(EOwnPresentationEvent Event);

/** One owner presentation change, stamped with this machine's EstimatedServerNow(). */
struct FOwnPresentationRecord
{
	EOwnPresentationEvent Event = EOwnPresentationEvent::HudMessage;
	/** The shown text (plates, cards, messages); empty otherwise. */
	FString Text;
	int32 Value = 0;
	double ServerNow = 0.0;
	/** The time the change answers (the contact, the DeathServerTime); -1 when none. */
	double DueServerTime = -1.0;
	/** When the data behind it was processed (a result's arrival; a held message's receipt); -1 when none. */
	double ArrivalServerTime = -1.0;
	bool bLate = false;
};

/** The Result slot (HUD E07 / E08 stand-in view-model): one plate, the newest preempts. */
struct FOwnResultPlateView
{
	bool bActive = false;
	EOwnResultPlate Plate = EOwnResultPlate::NoContact;
	FText Text;
	/** The F3 timeline origin (EstimatedServerNow() at the show). */
	double ShowServerTime = 0.0;
	double ContactServerTime = -1.0;
	/** EstimatedServerNow() when the removal card pushed the plate off (-1: not preempted; HUD-R9, story nkp-018). */
	double PreemptServerTime = -1.0;
};

/** The client-local card-entrance event (HUD CR-12 a, OnRemovalCardEntered; Camera's ViewRelease(StrikeLimit) and the exile cue consume it). */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnOwnRemovalCardEntered, double /*ServerNow*/);

#if !UE_BUILD_SHIPPING
/** One possession event, recorded for spike V2 (story iif-009). Development builds only. */
struct FStealthPossessionTraceEvent
{
	enum class EKind : uint8
	{
		/** Server: OnPossess advanced ServerPossessionEpoch to Epoch. */
		ServerOnPossess,
		/** Owner: ClientSetPossessionEpoch(Epoch) arrived. */
		OwnerEpoch,
		/** Owner: ClientRestart ran; Epoch is OwnerPossessionEpoch at that moment. */
		OwnerRestart,
	};
	EKind Kind = EKind::ServerOnPossess;
	uint16 Epoch = 0;
	uint64 Frame = 0;
	double Seconds = 0.0;
	FString Controller;
	FString Pawn;
	/** Recorded in a client world (the owner of a remote connection). */
	bool bClientWorld = false;
	/** The pawn's network GUID value in the recording world (identical on server and owner), 0 when none. */
	uint64 PawnGuid = 0;
};
#endif

/**
 * Player controller: the pawn command transport (ADR-0011 section 2, story iif-094), receives server messages and team
 * pings for the HUD, carries the owner-only C-TeamView Roster component (story psr-062), and exposes development
 * console commands.
 *
 * Command transport (ADR-0011 Key Interfaces): gameplay input is bound HERE, not on the pawn (SetupInputComponent binds
 * every IA_* action through UEnhancedInputComponent into the client-only UPawnCommandClient); discrete commands travel
 * only in the reliable, validated ServerPawnCommand; owner replies return in one reliable ClientCommandReplies batch;
 * the server-owned possession epoch is sent by ClientSetPossessionEpoch in OnPossess before the engine's ClientRestart.
 * The client never names the pawn it commands. The listen host's controller calls IngestCommand(HostLocal) directly
 * (same ingress, same reply flush). IA_Ping uses ServerTeamPing, never ServerPawnCommand. Round control moved to the host-only
 * stealth.round.* commands on the game instance (stories rmf-026/027): StealthStartRound, StealthEndRound and
 * StealthSwitchTeam and their Server RPCs are retired, and no Round command lives on this class.
 */
/** The own tend's ring colour (Objectives OC-6; story oc-016). */
enum class EStealthTendRing : uint8
{
	None,
	Bone,
	Marigold,
};

UCLASS()
class STEALTHGAME_API AStealthPlayerController : public APlayerController, public IPawnCommandUplink
{
	GENERATED_BODY()

public:
	AStealthPlayerController();

	// ---- Pawn command transport (ADR-0011) ----------------------------------------------------------------

	/** Owner -> server: one discrete command. WithValidation refuses only structurally impossible payloads. */
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerPawnCommand(const FNetPawnCommand& Cmd);

	/** Server -> owner: every reply of one drain pass, in queue-key order (at most one call per drain). */
	UFUNCTION(Client, Reliable)
	void ClientCommandReplies(const TArray<FCommandReply>& Replies);

	/** Server -> owner: the possession epoch, sent in OnPossess before ClientRestart (V2). */
	UFUNCTION(Client, Reliable)
	void ClientSetPossessionEpoch(uint16 Epoch);

	/** Owner -> server: a teammate ping (IA_Ping; the informant-pings epic owns the handler). */
	UFUNCTION(Server, Reliable)
	void ServerTeamPing(FVector_NetQuantize Location);

	/** The client-only command builder. */
	UPawnCommandClient* GetCommandClient() const { return CommandClient; }

	/** Server: possessions so far (incremented in OnPossess). Owner: the last epoch the server delivered. */
	uint16 GetServerPossessionEpoch() const { return ServerPossessionEpoch; }
	uint16 GetOwnerPossessionEpoch() const { return OwnerPossessionEpoch; }

	/** Server: the control epoch recorded at OnPossess (stamped by ingress; the factory's epoch when ADR-0010 4.4 lands). */
	uint32 GetPossessedControlEpoch() const { return PossessedControlEpoch; }

	/** Server: this connection's reply sink. */
	FReplySinkId GetReplySink() const { return ReplySink; }

	/** Owner: replies received so far (development HUD and tests). */
	int32 GetReceivedReplyCount() const { return ReceivedReplyCount; }

	/** Owner: the latest received reply entries (at most MaxKeptReplies, oldest first; tests and story asn-024's matcher). */
	const TArray<FCommandReply>& GetReceivedReplies() const { return ReceivedReplies; }

	/**
	 * Owner (Objectives story oc-016, OC-6): the own tend's ring. None without an own Interact hold (from the server's
	 * FHoldStatus replies); otherwise Marigold when ObjectiveViewHelpers::RingIsMarigold holds for the court the hold
	 * targets (the nearest court socket in reach when the hold started; public geometry), the hold's server start time, the
	 * estimated server time and the TendCueDelaySec read at Begin; Bone otherwise. Never Marigold for an informant.
	 */
	EStealthTendRing GetOwnTendRing(double ServerNow) const;

	/** Owner: the own Interact hold's progress 0..1 from its FHoldStatus start (negative without a hold). */
	float GetOwnTendProgress(double ServerNow) const;

	/** Owner: the court the own Interact hold targets (unset without a hold). */
	FCourtFixtureId GetOwnTendTarget() const { return bOwnTend ? OwnTendTarget : FCourtFixtureId(); }

	/**
	 * Owner: the "Wrong targets" tally the HUD shows: FStrikeResult.TallyAfter of the latest result plate, changed only at
	 * that plate's show time (HUD-R7, story asn-024).
	 */
	int32 GetShownWrongTargetTally() const;

	// ---- Owner result presentation (story asn-024; ADR-0018 sections 5.3 / 5.4) ---------------------------------------

	/** Owner: the Result slot's plate (the HUD draws it with FOwnResultPlateTimeline from its ShowServerTime). */
	const FOwnResultPlateView& GetResultPlate() const { return ResultPlate; }

	/**
	 * Owner (story nkp-018): this frame's Penalty HUD elements (UI-NKP-1 tally, UI-NKP-3 exile card, UI-NKP-4 countdown) for the
	 * Presentation overlay and the tests: the shown tally with the public L, frozen while informant; the card from its
	 * client-local entrance; the countdown from the owner part's T_due once the neutral view is reached.
	 */
	FPenaltyHudFrame BuildPenaltyHudFrame(double ServerNow) const;

	/** Owner: EstimatedServerNow() at the exile card's entrance (-1: none yet this life; the card shows once). */
	double GetExileCardEnteredAt() const { return ExileCardEnteredAt; }

	/** Owner: the card-entrance event (fires once per exile on the card's entrance frame; story nkp-017). */
	FOnOwnRemovalCardEntered OnRemovalCardEntered;

	/**
	 * Owner: when the returning countdown may show (the neutral view is reached): the own fall plus NoPawnFadeSec after an
	 * own death, the card entrance plus NoPawnFadeSec after an exile (Camera's ViewRelease fade); -1 when no own removal
	 * is being presented (a rebuild shows it at once).
	 */
	double GetCountdownGateAt() const { return CountdownGateAt; }

	/** Owner: the ClientCommandSeq.Seq of the last result plate shown (or of the re-push applied; HUD-R7 gate). */
	uint32 GetLastShownTallySeq() const { return LastShownTallySeq; }

	/** Owner: true while the brace square is shown (FOwnBraceIndicator on the possessed pawn, sampled each frame). */
	bool IsBraceSquareShown() const { return bBraceSquareShown; }

	/**
	 * Owner: the role the HUD presents (ASN-R23): informant only from this player's OnRoleChangePresented, its exile card
	 * or a possession made as an informant, never at the Roster state's arrival; Active again when the Roster returns to
	 * Active (a Reset is no outcome). A Roster informant state that no presentation follows within
	 * HudResultPlateMaxHoldBackSec is presented then (the lost-data cap).
	 */
	bool IsPresentedInformant() const { return bInformantPresented; }

	/** Owner: the round-end presentation is released (never before this owner's own plate or own fall, HUD F10). */
	bool IsRoundEndPresented() const { return bRoundEndPresented; }

	/** Owner: every HUD view-model change of this session (bounded; tests and the asn-017 presentation log). */
	const TArray<FOwnPresentationRecord>& GetOwnPresentationLog() const { return OwnPresentationLog; }

	/** Owner: the matcher (tests read its counters). */
	const FOwnResultMatcher& GetResultMatcher() const { return ResultMatcher; }

	/** Owner: an own outcome is still to be presented at ServerNow (a pending result, or the own pawn's fall not begun). */
	bool IsOwnOutcomeUnpresented(double ServerNow) const;

	/** The owner-only cooldown end (story asn-016): the only defence datum a client holds. */
	UDefenceOwnerComponent* GetDefenceOwner() const { return DefenceOwner; }

#if !UE_BUILD_SHIPPING
	/** Every possession event of every AStealthPlayerController in this process (spike V2); cleared by the spike. */
	static TArray<FStealthPossessionTraceEvent>& GetPossessionTrace();

	/** GFrameCounter of every FlushPressedKeys on any AStealthPlayerController (spike V5); cleared by the spike. */
	static TArray<uint64>& GetFlushPressedKeysTrace();
#endif

	/** Records the flush for spike V5 (development builds), then flushes. */
	virtual void FlushPressedKeys() override;

	// IPawnCommandUplink
	virtual void SendPawnCommand(const FNetPawnCommand& Cmd, APawn* HostLocalPawnRef) override;
	virtual void SendTeamPing(const FVector& Location) override;

	virtual void SetupInputComponent() override;
	virtual void OnPossess(APawn* InPawn) override;
	/** A body release (UStealthPawnFactory::IsReleasingBody, story asn-019) keeps the view target on the falling body. */
	virtual void OnUnPossess() override;
	virtual void ClientRestart_Implementation(APawn* NewPawn) override;
	/** Owner: an unpossession or a new pawn arrived by replication; outstanding Begins are ended first (story iif-013 d). */
	virtual void OnRep_Pawn() override;
	/**
	 * ADR-0019 section 4.4 (story nkp-017): an exile's hand-over of the LIVE body to the crowd never cuts the owner's view
	 * (on the server and on the owning client): the engine's auto-managed camera target would move the view to this
	 * controller in SetPawn(nullptr); it is skipped then, so the view stays on the released body until Camera's ViewRelease
	 * (or the next possession) moves it. A dead body's release (a death write) keeps the engine's behaviour.
	 */
	virtual void SetPawn(APawn* InPawn) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	/** Owner: samples the result presentation every frame (story asn-024). */
	virtual void PlayerTick(float DeltaTime) override;
	/** Server: forwards to a non-Shipping spike hook (THROWAWAY, IVP S10 re-run story-056, ADR-0003 S10-R4); otherwise the engine default (nothing). */
	virtual void NotifyActorChannelFailure(UActorChannel* ActorChan) override;

	/** Server -> owning client: show a HUD message. */
	UFUNCTION(Client, Reliable)
	void ClientStealthMessage(const FText& Text, bool bDanger, float Duration);

	/** Server -> teammate clients: show a ping. */
	UFUNCTION(Client, Reliable)
	void ClientReceivePing(FVector_NetQuantize Location, const FString& SenderName);

	/**
	 * Development test driver (no-op in Shipping). Makes player PlayerIndex (0 = host) perform Action
	 * through the real server rule paths: strike_npc | strike_enemy | strike_informant | defend |
	 * interact | ping | report. Debug positioning teleports the pawn next to its target.
	 */
	UFUNCTION(Exec)
	void StealthBot(int32 PlayerIndex, const FString& Action);

	/** Development: 1 = overhead camera over the crowd, 0 = back to the pawn. */
	UFUNCTION(Exec)
	void StealthDebugCam(int32 Mode);

	/**
	 * Development: injects one simulated key event into this player's Enhanced Input (as a real key would arrive), so a
	 * PIE session driven over the MCP can exercise the production input path. Down: 1 press, 0 release. No-op in Shipping.
	 */
	UFUNCTION(Exec)
	void StealthInputKey(const FString& KeyName, int32 Down);

	/** Development: logs the production Round phase and Roster counts (server) or the replicated public state (client). */
	UFUNCTION(Exec)
	void StealthDumpProd();

	/**
	 * A command a remote client sent through ServerExec runs inside FRoundRemoteExecScope, so Round's host check
	 * answers NotHost (stories rmf-026/027). Local console input is unchanged.
	 */
	virtual FString ConsoleCommand(const FString& Command, bool bWriteToLog = true) override;

	const TArray<FStealthHudMessage>& GetMessages() const { return Messages; }
	const TArray<FStealthPing>& GetPings() const { return Pings; }

	/** The owner-only C-TeamView Roster part (written only by the Roster push step). */
	URosterTeamViewComponent* GetTeamView() const { return TeamView; }

	/** The owner-only C-TeamView Objectives part (story oc-011; written only by UObjectivesService). The HUD's objective source. */
	UObjectiveTeamViewComponent* GetObjectiveTeamView() const { return ObjectiveTeamView; }

	/** Concealment story con-023: the owner's own concealed flag (owner audience; the server's Concealment core writes it). */
	UConcealmentOwnerViewComponent* GetConcealmentOwnerView() const { return ConcealmentOwnerView; }

	/** Removes expired messages and pings. */
	void PruneExpired(double Now);

protected:
	UFUNCTION(Server, Reliable)
	void ServerStealthBot(int32 PlayerIndex, const FString& Action);

	/** Story oc-016: an own Interact FHoldStatus (Active starts the ring's hold; an end clears it). */
	void HandleOwnTendStatus(const FHoldStatus& Status);

	/** Builds the stage-1 context (mailbox, reply sink, recorded control epoch) and runs IngestCommand. Server. */
	void IngestOwnCommand(const FNetPawnCommand& Cmd, APawn* HostLocalPawnRef);

	/** The client-only command builder (ADR-0011 section 9); inert on a non-local controller. */
	UPROPERTY(VisibleAnywhere, Category = "Stealth")
	TObjectPtr<UPawnCommandClient> CommandClient;

	uint16 ServerPossessionEpoch = 0;
	uint16 OwnerPossessionEpoch = 0;
	uint32 PossessedControlEpoch = 0;
	FReplySinkId ReplySink;
	int32 ReceivedReplyCount = 0;
	TArray<FCommandReply> ReceivedReplies;
	/** Story oc-016: the own Interact hold (its latest Active FHoldStatus) and the court it targets. */
	FHoldStatus OwnTendStatus;
	FCourtFixtureId OwnTendTarget;
	double OwnTendCueDelaySec = 1.0;
	double OwnTendHoldSec = 3.0;
	bool bOwnTend = false;
	int32 ShownWrongTargetTally = 0;
	/** The round (FRoundPublicState::RoundIndex) of the shown tally: a new round shows 0 (HUD-R7). */
	uint32 ShownTallyRoundIndex = 0;
	/** The ClientCommandSeq.Seq of the last shown plate's result, or of the applied re-push (HUD-R7 gate, story nkp-017). */
	uint32 LastShownTallySeq = 0;

	/** Owner (HUD-R7, story nkp-017): applies the C-TeamView owner re-push only with no plate pending, a seq >= the last shown, and never lower. */
	void ApplyOwnerTallyRepush(double ServerNow);
	/** Owner: writes the shown tally to the client-local display cache (UStealthOwnerTallyCache). */
	void StoreShownTally();
	/** Owner: the current round's index (FRoundPublicState::RoundIndex; 0 without a game state). */
	uint32 CurrentRoundIndex() const;
	/** Owner: processes one batch of owner replies (the matcher; story asn-024), now. */
	void ProcessCommandReplies(const TArray<FCommandReply>& Replies, double ServerNow);
	/** Owner (story nkp-018): the exile card at ServerNow (UI-NKP-3). */
	FPenaltyExileCardView BuildPenaltyCard(double ServerNow) const;
	/** Owner (story nkp-018): the returning countdown at ServerNow given the card's phase (UI-NKP-4). */
	FPenaltyCountdownView BuildPenaltyCountdown(double ServerNow, EPenaltyCardPhase CardPhase) const;
	/** The tally view of the last BuildPenaltyHudFrame and its inputs (rebuilt only when they change). */
	mutable int64 CachedTallyKey = -1;
	mutable TSharedPtr<FPenaltyTallyView> CachedTallyView;

	/** Owner (story asn-024): one frame of the result presentation at ServerNow (sample, pop, show, release). */
	void TickOwnPresentation(double ServerNow);

	/** Owner: shows a due result in the Result slot (the tally, the attacker's grade and the exile beat with it). */
	void ShowResultPlate(const FOwnResultDue& Due, double ServerNow);

	/** Adds a HUD message line now and logs it. */
	void AddHudMessage(const FText& Text, bool bDanger, float Duration, double ArrivalServerTime = -1.0);

	/** Appends one owner presentation record (bounded). */
	void LogOwnPresentation(EOwnPresentationEvent Event, const FString& Text, int32 Value, double ServerNow, double Due = -1.0, double Arrival = -1.0, bool bLate = false);

	/** The pawn handles the matcher may need sampled (the commanded pawns), by key. */
	TMap<const void*, TWeakObjectPtr<AStealthPawn>> WatchedPawns;

	FOwnResultMatcher ResultMatcher;
	FOwnResultPlateView ResultPlate;
	TArray<FOwnPresentationRecord> OwnPresentationLog;
	bool bBraceSquareShown = false;
	bool bInformantPresented = false;
	/** The Roster state read informant on the last frame. */
	bool bRosterWasInformant = false;
	/** EstimatedServerNow() when the Roster state first read informant without a presentation (-1: none). */
	double UnpresentedInformantSince = -1.0;
	bool bRoundEndPresented = false;
	/** EstimatedServerNow() when the round-end phase was first seen (-1: not over). */
	double RoundEndSeenAt = -1.0;
	/** The exile card's due time (-1: none). */
	double ExileCardAt = -1.0;
	/** The exile card's client-local entrance (-1: none; story nkp-018). */
	double ExileCardEnteredAt = -1.0;
	/** When the countdown may show (-1: at once; story nkp-018). */
	double CountdownGateAt = -1.0;
	/** The countdown's last logged seconds (-1: hidden). */
	int32 LoggedCountdownSeconds = -1;
#if WITH_STEALTH_TEST_SEAMS
	/** Reply batches held by the lateness seam, with their release time (story nkp-017). */
	struct FHeldReplyBatch
	{
		double ReleaseServerTime = 0.0;
		TArray<FCommandReply> Replies;
	};
	TArray<FHeldReplyBatch> HeldReplyBatches;
#endif
	/** Server messages that arrived while an own outcome was unpresented (released at its presentation or the cap). */
	struct FHeldMessage
	{
		FText Text;
		bool bDanger = false;
		float Duration = 0.f;
		double ArrivalServerTime = 0.0;
	};
	TArray<FHeldMessage> HeldMessages;
	/** The owner's outcome state was seen on the last frame (the asn-017 negative-control seam only). */
	bool bSeamSawInformant = false;

	/**
	 * Owner (story asn-021): binds this local player's UEncounterViewSubsystem once: the own-role message follows
	 * OnRoleChangePresented (never the server's message or the Roster state's arrival), and the informant query answers
	 * from the pawn recorded at possession.
	 */
	void BindEncounterView();

	/** OnRoleChangePresented consumer (STAND-IN for the HUD removal card, HUD epic): the own-role message. */
	void HandleRoleChangePresented(ERoleChangeReason Reason, double LocalTime);

	/** The pawn this local controller last acknowledged while its player was an informant (the InformantKilled reason). */
	TWeakObjectPtr<APawn> PossessedAsInformantPawn;
	bool bEncounterViewBound = false;

	/** The owner-only cooldown end (story asn-016), replicated to the owning connection only. */
	UPROPERTY(VisibleAnywhere, Category = "Stealth")
	TObjectPtr<UDefenceOwnerComponent> DefenceOwner;

	/** C-TeamView, Roster part (ADR-0001): replicated to the owning connection only. */
	UPROPERTY(VisibleAnywhere, Category = "Stealth")
	TObjectPtr<URosterTeamViewComponent> TeamView;

	/** C-TeamView, Objectives part (ADR-0020 section 6, story oc-011): the team's objective view, owning connection only. */
	UPROPERTY(VisibleAnywhere, Category = "Stealth")
	TObjectPtr<UObjectiveTeamViewComponent> ObjectiveTeamView;

	UPROPERTY(VisibleAnywhere, Category = "Stealth")
	TObjectPtr<UConcealmentOwnerViewComponent> ConcealmentOwnerView;

	/** Owner: a local HUD line when the own team's stage advances (from the view, never from an RPC). */
	void HandleObjectiveViewUpdated(const FObjectiveTeamView& View);
	/** The StageIndex (as uint8) of the last objective view this owner presented. */
	uint8 LastPresentedObjectiveStage = 0;

	/** Overhead debug camera (local only). */
	UPROPERTY(Transient)
	TObjectPtr<AActor> DebugCamera;

	TArray<FStealthHudMessage> Messages;
	TArray<FStealthPing> Pings;
};

#if WITH_STEALTH_TEST_SEAMS
namespace StealthOwnerPresentationTestSeams
{
	/**
	 * Test seam (ADR-0008 section 3; story asn-017 negative control): when on, every local AStealthPlayerController shows a
	 * HUD line the moment its player's outcome state arrives (the Roster informant state or the own pawn's FDeathState,
	 * whichever first), the forbidden on-receipt presentation. Off by default; compiled only with WITH_STEALTH_TEST_SEAMS
	 * (never Shipping).
	 */
	STEALTHGAME_API void SetPresentOnRosterReceipt(bool bOn);
	STEALTHGAME_API bool IsPresentOnRosterReceipt();

	/**
	 * Test seam (story asn-024 negative control): when on, every result plate is shown the moment its reply entry is
	 * processed (the prototype's on-receipt behaviour), so the timing tests are seen to fail against it. Off by default.
	 */
	STEALTHGAME_API void SetPresentResultsOnReceipt(bool bOn);

	/**
	 * Test seam (story nkp-017, the injected FStrikeResult RPC lateness of AC-NKP-38 / AC-NKP-46): every ClientCommandReplies
	 * batch a local controller receives is held this long (EstimatedServerNow) before it is processed, as if the RPC
	 * arrived that much later. 0 (the default) processes on receipt.
	 */
	STEALTHGAME_API void SetReplyLatenessSec(double Sec);

	/**
	 * Test seam (story nkp-017, AC-NKP-38 run 4): when on, a StrikeResult reply that carries bExiled is dropped on receipt
	 * (the lost-RPC case); the count dropped so far is returned by GetDroppedExilingResults. Off by default.
	 */
	STEALTHGAME_API void SetDropExilingStrikeResults(bool bOn);
	STEALTHGAME_API int32 GetDroppedExilingResults();
}
#endif
