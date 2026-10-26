#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Server/StealthLogFaultSink.h"
#include "Server/Core/Round/StealthCrowdControl.h"
#include "Server/Core/Round/StealthRoundGateways.h"
#include "Server/Core/StealthPawnFactory.h"
#include "Server/Random/StealthRoundSeedDrawer.h"
#include "Server/Random/StealthServerSeeds.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "StealthCompositionRoot.generated.h"

class APlayerController;
class AStealthGameMode;
class URosterStore;
class URoundMatchStore;
class URoundRosterStoreGateway;
struct FRosterEvent;

/**
 * The minimal adapters of the D53 integration batch (decision D53): the Round gateways whose producing systems do not
 * exist yet (Session & Travel, Tuning dry run, Character Pool, Level Socket Contract, Crowd), and the pawn factory's
 * hooks over the prototype (appearance on AStealthCharacter, the prototype NPC controller) and the real Roster.
 *
 * Session adapter: no travel happens yet, so RequestRoundTravel draws the round seed (FStealthRoundSeedDrawer over a
 * development session secret) and confirms the travel on the next tick; the connection state is Connected for a
 * participant with a live controller, else the Roster projection. Each adapter is replaced by its system's facade
 * when that epic lands.
 */
UCLASS(NotBlueprintType, NotBlueprintable)
class STEALTHGAME_API UStealthMatchAdapters : public UObject, public IRoundSessionGateway, public IRoundTuningGateway,
	public IRoundCharacterPoolGateway, public IRoundLevelSocketGateway, public ICrowdControl, public IStealthPawnFactoryHooks
{
	GENERATED_BODY()

public:
	/** Opens the development session secret (the seed source until Session's OS CSPRNG, story st-009). */
	void InitializeAdapters();

	// IRoundSessionGateway
	virtual bool CanRequestRoundTravel() const override;
	virtual ERoundTravelRequestResult RequestRoundTravel(FName MapId, FName PresetId, uint32 RoundIndex) override;
	virtual void RequestLobbyTravel() override {}
	virtual void SetAdmissionMode(ERoundAdmissionMode Mode) override { AdmissionMode = Mode; }
	virtual ERoundConnectionState GetConnectionState(FParticipantId Participant) const override;
	virtual void KickParticipant(FParticipantId Participant) override;
	virtual uint64 GetServerSubSeed(ESeedDomain Domain, uint32 Attempt) const override;

	// IRoundTuningGateway
	virtual void SignalPreparingEntered() override {}
	virtual ERoundPresetPreview PreviewPreset(FName PresetId) const override { return ERoundPresetPreview::Unavailable; }

	// IRoundCharacterPoolGateway
	virtual ERoundAllocateResult AllocateAppearances(TConstArrayView<FParticipantId> Players, int32 NpcCount) override { return ERoundAllocateResult::Ok; }
	virtual uint16 GetAssignedAppearance(FParticipantId Participant) const override { return 0; }
	virtual void DiscardDeal() override {}

	// IRoundLevelSocketGateway
	virtual void RequestLayout(int32 Seed, uint64 TuningHash) override {}
	virtual ERoundPlacementResult RequestStartPlacement(TConstArrayView<FParticipantId> Team0, TConstArrayView<FParticipantId> Team1, int32 NpcCount) override
	{
		return ERoundPlacementResult::Ok;
	}

	// ICrowdControl
	virtual void Halt() override {}
	virtual void DriveBoundPawn(AStealthPawn* Pawn) override {}
	virtual void ReleaseControl(AStealthPawn* Pawn) override {}
	virtual void AdoptReleasedPawn(AStealthPawn* Pawn, const FCrowdAdoptParams& Params) override {}

	// IStealthPawnFactoryHooks
	virtual void ApplyAppearance(AStealthPawn* Pawn, int32 AppearanceId) override;
	virtual ERosterResult BindPawn(FParticipantId Participant, AStealthPawn* Pawn, const TOptional<FRosterEpochToken>& ExpectedEpoch) override;
	virtual AController* CreateAiController(AStealthPawn* Pawn) override;

	/** The seed RequestRoundTravel drew for the round being started (0 before the first start). */
	int32 GetLastRoundSeed() const { return LastRoundSeed; }
	ERoundAdmissionMode GetAdmissionMode() const { return AdmissionMode; }

	/** The crowd area the prototype NPC controller wanders (set by the game mode). */
	FVector CrowdCenter = FVector::ZeroVector;
	float CrowdRadius = 3000.f;

private:
	UWorld* GetGameWorld() const;

	FStealthServerSeeds SessionSeeds;
	TUniquePtr<FStealthRoundSeedDrawer> SeedDrawer;
	int32 LastRoundSeed = 0;
	ERoundAdmissionMode AdmissionMode = ERoundAdmissionMode::Open;
};

/**
 * The composition root's game-instance part (ADR-0007 section 4; decision D53): builds the Round gateways (the real
 * URosterStore through URoundRosterStoreGateway, plus UStealthMatchAdapters), registers them with URoundMatchStore,
 * wires the pawn factory, and keeps Session's controller <-> participant map until the Session epic owns it.
 * Server-authoritative: a client game instance builds nothing.
 */
UCLASS(NotBlueprintType, NotBlueprintable)
class STEALTHGAME_API UStealthCompositionRoot : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	static UStealthCompositionRoot* Get(const UWorld* World);

	/** Server world start (AStealthGameMode::InitGame): rule params, OpenMatch on first use, factory wiring. */
	void OnServerWorldStarting(AStealthGameMode* GameMode);

	/** PostLogin: registers the player's key with the Roster (Round assigns the team) and binds its C-TeamView. */
	FParticipantId RegisterPlayer(APlayerController* Player);
	/** Logout: Lobby removes the record; otherwise the projection becomes Disconnected. */
	void UnregisterPlayer(APlayerController* Player);

	/** Session's map. */
	FParticipantId GetParticipant(const APlayerController* Player) const;
	APlayerController* GetController(FParticipantId Participant) const;
	/** Every mapped (participant, controller) pair with a live controller. */
	void GetConnectedPlayers(TArray<TPair<FParticipantId, APlayerController*>>& Out) const;

	UStealthMatchAdapters* GetAdapters() const { return Adapters; }
	URosterStore* GetRosterStore() const;
	URoundMatchStore* GetRoundStore() const;

	/** A one-line production state summary (dev HUD line and StealthDumpProd). Server: full; client: empty. */
	FString DescribeProductionState(const UWorld* World) const;

private:
	void HandleRosterEvent(const FRosterEvent& Event);
	void MirrorTeamsToPrototype();

	UPROPERTY(Transient)
	TObjectPtr<UStealthMatchAdapters> Adapters;

	UPROPERTY(Transient)
	TObjectPtr<URoundRosterStoreGateway> RosterGateway;

	TWeakObjectPtr<AStealthGameMode> GameMode;
	TMap<TWeakObjectPtr<APlayerController>, FParticipantId> ParticipantByController;

	/** D53 command-path adapters for the current server world (StealthPrototypeCommandQueue.h). */
	TSharedPtr<class FStealthWorldCommandClock> CommandClock;
	TSharedPtr<class FStealthPrototypeCommandQueue> CommandQueue;
	FStealthLogFaultSink CommandFaults;
	FDelegateHandle RosterEventHandle;
	bool bGatewaysRegistered = false;
};
