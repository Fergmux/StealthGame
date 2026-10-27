#include "Server/Roster/StealthRosterStore.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Misc/Guid.h"
#include "Roster/StealthRosterPushStep.h"
#include "Server/Random/StealthServerSeeds.h"
#include "Server/StealthLogFaultSink.h"
#include "UObject/UObjectArray.h"

// URosterStore (story psr-061; ADR-0005 section 2, ADR-0007 facade split). The adapters below are the only place
// engine objects meet FRosterCore's interfaces.

namespace RosterStoreLocal
{
	/** Fault-sink source of the facade's own reports (a dead weak registration). */
	const FName FacadeFaultSource(TEXT("RosterStore"));

	/** Registration slot ids in DeadRegistration faults. */
	constexpr int64 RoundPhaseSlot = 1;

	/** Packs a live object's array index and serial number into the core's 64-bit object key (0 = null). */
	uint64 ObjectKeyOf(const UObject* Object)
	{
		if (Object == nullptr)
		{
			return 0;
		}
		const int32 Index = GUObjectArray.ObjectToIndex(Object);
		const int32 Serial = GUObjectArray.AllocateSerialNumber(Index);
		return (static_cast<uint64>(static_cast<uint32>(Serial)) << 32) | static_cast<uint64>(static_cast<uint32>(Index));
	}

}

/** The engine adapters of one store. Members outlive the core (the store destroys the core first). */
struct FRosterStoreAdapters
{
	explicit FRosterStoreAdapters(URosterStore& InStore) : Store(InStore), Authority(*this), Liveness(*this), Clock(*this), Random(*this), RoundPhase(*this) {}

	URosterStore& Store;

	/** Authority from the game instance's net mode: a client process (NM_Client) has none (R2). */
	struct FAuthority final : IRosterAuthority
	{
		explicit FAuthority(FRosterStoreAdapters& InOwner) : Owner(InOwner) {}
		virtual bool HasAuthority() const override
		{
			if (Owner.AuthorityOverride.IsSet())
			{
				return Owner.AuthorityOverride.GetValue();
			}
			const UGameInstance* GameInstance = Owner.Store.GetGameInstance();
			const UWorld* World = GameInstance != nullptr ? GameInstance->GetWorld() : nullptr;
			return World != nullptr ? World->GetNetMode() != NM_Client : !IsRunningClientOnly();
		}
		FRosterStoreAdapters& Owner;
	};

	struct FThreadCheck final : IRosterThreadCheck
	{
		virtual bool IsGameThread() const override { return IsInGameThread(); }
	};

	/** Liveness from the pawn object behind the handle (R10 Stale rule). */
	struct FLiveness final : IRosterPawnLiveness
	{
		explicit FLiveness(FRosterStoreAdapters& InOwner) : Owner(InOwner) {}
		virtual ERosterPawnLiveness GetLiveness(const FRosterPawnHandle& Pawn) const override
		{
			const TWeakObjectPtr<APawn>* Found = Owner.Store.PawnsByKey.Find(Pawn.ObjectKey);
			const APawn* Resolved = Found != nullptr ? Found->Get() : nullptr;
			if (Resolved == nullptr)
			{
				return ERosterPawnLiveness::Destroyed;
			}
			if (!IsValid(Resolved) || Resolved->IsActorBeingDestroyed())
			{
				return ERosterPawnLiveness::PendingKill;
			}
			return ERosterPawnLiveness::Valid;
		}
		FRosterStoreAdapters& Owner;
	};

	/** The server clock: the game instance's world time (process time before a world exists). */
	struct FClock final : IStealthClock
	{
		explicit FClock(FRosterStoreAdapters& InOwner) : Owner(InOwner) {}
		virtual double GetServerTimeSeconds() const override
		{
			if (Owner.ClockOverride != nullptr)
			{
				return Owner.ClockOverride->GetServerTimeSeconds();
			}
			const UGameInstance* GameInstance = Owner.Store.GetGameInstance();
			const UWorld* World = GameInstance != nullptr ? GameInstance->GetWorld() : nullptr;
			return World != nullptr ? World->GetTimeSeconds() : FPlatformTime::Seconds();
		}
		virtual uint64 GetServerTickIndex() const override
		{
			return Owner.ClockOverride != nullptr ? Owner.ClockOverride->GetServerTickIndex() : GFrameCounter;
		}
		FRosterStoreAdapters& Owner;
	};

	/** ADR-0006: IRosterRandom draws are GetServerSubSeed(PublicHandle 15 | MatchId 17, Counter). */
	struct FRandom final : IRosterRandom
	{
		explicit FRandom(FRosterStoreAdapters& InOwner) : Owner(InOwner) {}
		virtual uint64 Draw(ERosterRandomDomain Domain, uint64 Counter) override
		{
			return Owner.Seeds.GetServerSubSeed(static_cast<ESeedDomain>(static_cast<uint32>(Domain)), static_cast<uint32>(Counter));
		}
		FRosterStoreAdapters& Owner;
	};

	/** Round's phase, registered weakly by the composition root; none (or a dead owner) reads not Active. */
	struct FRoundPhase final : IRosterRoundPhase
	{
		explicit FRoundPhase(FRosterStoreAdapters& InOwner) : Owner(InOwner) {}
		virtual bool IsRoundPhaseActive() const override
		{
			if (Owner.RoundPhase.Registered == nullptr)
			{
				return false;
			}
			if (!Owner.RoundPhase.RegisteredOwner.IsValid())
			{
				if (!Owner.RoundPhase.bReportedDead)
				{
					Owner.RoundPhase.bReportedDead = true;
					Owner.GetFaultSink().Report(EStealthFault::DeadRegistration, RosterStoreLocal::FacadeFaultSource, RosterStoreLocal::RoundPhaseSlot);
				}
				return false;
			}
			return Owner.RoundPhase.Registered->IsRoundPhaseActive();
		}
		FRosterStoreAdapters& Owner;
		IRosterRoundPhase* Registered = nullptr;
		TWeakObjectPtr<UObject> RegisteredOwner;
		mutable bool bReportedDead = false;
	};

	/** Bridges core events to the store's multicast and the push step. */
	struct FEventBridge final : IRosterEventListener
	{
		explicit FEventBridge(URosterStore& InStore) : Store(InStore) {}
		virtual void OnRosterEvent(const FRosterEvent& Event) override { Store.OnRosterEvent.Broadcast(Event); }
		URosterStore& Store;
	};

	IStealthFaultSink& GetFaultSink() { return FaultSinkOverride != nullptr ? *FaultSinkOverride : static_cast<IStealthFaultSink&>(LogFaultSink); }

	FAuthority Authority;
	FThreadCheck ThreadCheck;
	FLiveness Liveness;
	FClock Clock;
	FRandom Random;
	FRoundPhase RoundPhase;
	FEventBridge Bridge{ Store };
	FStealthLogFaultSink LogFaultSink;
	FStealthServerSeeds Seeds;

	TOptional<bool> AuthorityOverride;
	IStealthFaultSink* FaultSinkOverride = nullptr;
	IStealthClock* ClockOverride = nullptr;
};

URosterStore::URosterStore() = default;
URosterStore::URosterStore(FVTableHelper& Helper) : Super(Helper) {}
URosterStore::~URosterStore() = default;

void URosterStore::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	InitializeStore();
}

void URosterStore::Deinitialize()
{
	HandleGameInstanceShutdown();
	ShutdownStore();
	Super::Deinitialize();
}

void URosterStore::InitializeStore(const FRosterStoreInitOverrides& Overrides)
{
	ShutdownStore();
	Adapters = MakeUnique<FRosterStoreAdapters>(*this);
	Adapters->AuthorityOverride = Overrides.bAuthority;
	Adapters->FaultSinkOverride = Overrides.FaultSink;
	Adapters->ClockOverride = Overrides.Clock;
	FOsEntropySource OsEntropy; // ADR-0006 section 2 (story st-009): the OS CSPRNG, never a GUID
	Adapters->Seeds.Open(Overrides.Entropy != nullptr ? *Overrides.Entropy : static_cast<IEntropySource&>(OsEntropy), &Adapters->GetFaultSink());

	FRosterCoreDeps Deps;
	Deps.Authority = &Adapters->Authority;
	Deps.ThreadCheck = &Adapters->ThreadCheck;
	Deps.Liveness = &Adapters->Liveness;
	Deps.Clock = &Adapters->Clock;
	Deps.Random = &Adapters->Random;
	Deps.FaultSink = &Adapters->GetFaultSink();
	Deps.RoundPhase = &Adapters->RoundPhase;
	Core = MakeUnique<FRosterCore>(Deps);
	Core->AddListener(&Adapters->Bridge);

	PushStep = MakeUnique<FRosterPushStep>(*Core, Adapters->RoundPhase, [this](const FRosterPawnHandle& Handle) { return ResolvePawn(Handle); });
	Core->AddListener(PushStep.Get());

	if (!WorldCleanupHandle.IsValid())
	{
		WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(this, &URosterStore::HandleWorldCleanup);
	}
}

void URosterStore::ShutdownStore()
{
	if (Core)
	{
		EndMatchOnExitPath();
		Core->RemoveListener(PushStep.Get());
		Core->RemoveListener(&Adapters->Bridge);
	}
	PushStep.Reset();
	Core.Reset();
	Adapters.Reset();
	PawnGenerations.Reset();
	PawnsByKey.Reset();
	if (WorldCleanupHandle.IsValid())
	{
		FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
		WorldCleanupHandle.Reset();
	}
}

bool URosterStore::IsStoreInitialized() const
{
	return Core.IsValid();
}

URosterStore* URosterStore::Get(const UWorld* World)
{
	const UGameInstance* GameInstance = World != nullptr ? World->GetGameInstance() : nullptr;
	return GameInstance != nullptr ? GameInstance->GetSubsystem<URosterStore>() : nullptr;
}

FRosterCore& URosterStore::CoreRef() const
{
	check(Core.IsValid());
	return *Core;
}

// --- Match lifetime ---------------------------------------------------------------------------------------------

ERosterResult URosterStore::OpenSession(int32 TeamSize)
{
	const ERosterResult Result = CoreRef().BeginMatch(TeamSize);
	bMatchOpen = Result == ERosterResult::Applied;
	if (bMatchOpen && PushStep)
	{
		PushStep->Reset();
	}
	return Result;
}

bool URosterStore::IsMatchOpen() const
{
	return Core.IsValid() && bMatchOpen;
}

void URosterStore::EndMatchOnExitPath()
{
	if (!Core)
	{
		return;
	}
	// On a client process the store is empty already and EndMatch would only fault: nothing to end.
	if (Adapters->Authority.HasAuthority())
	{
		Core->EndMatch();
	}
	if (PushStep)
	{
		PushStep->Reset();
	}
	PawnGenerations.Reset();
	PawnsByKey.Reset();
	bMatchOpen = false;
	bTravelInProgress = false;
}

void URosterStore::NotifyHostQuit()
{
	EndMatchOnExitPath();
}

void URosterStore::NotifyReturnToMainMenu()
{
	EndMatchOnExitPath();
}

void URosterStore::NotifySessionEnding()
{
	EndMatchOnExitPath();
}

void URosterStore::HandleGameInstanceShutdown()
{
	EndMatchOnExitPath();
}

void URosterStore::HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	// Only this game instance's game world counts, and not while Session is travelling (the old world of a travel).
	if (World == nullptr || !World->IsGameWorld() || World->GetGameInstance() != GetGameInstance() || bTravelInProgress)
	{
		return;
	}
	EndMatchOnExitPath();
}

// --- Weak registrations ------------------------------------------------------------------------------------------

void URosterStore::RegisterRoundPhase(IRosterRoundPhase* RoundPhase, UObject* Owner)
{
	if (Adapters)
	{
		Adapters->RoundPhase.Registered = RoundPhase;
		Adapters->RoundPhase.RegisteredOwner = Owner;
		Adapters->RoundPhase.bReportedDead = false;
	}
}

void URosterStore::UnregisterRoundPhase(UObject* Owner)
{
	if (Adapters && Adapters->RoundPhase.RegisteredOwner.Get() == Owner)
	{
		Adapters->RoundPhase.Registered = nullptr;
		Adapters->RoundPhase.RegisteredOwner.Reset();
	}
}

// --- Pawn handles ------------------------------------------------------------------------------------------------

FRosterPawnHandle URosterStore::MakePawnHandle(const APawn* Pawn)
{
	if (Pawn == nullptr)
	{
		return FRosterPawnHandle();
	}
	const uint64 Key = RosterStoreLocal::ObjectKeyOf(Pawn);
	PawnsByKey.Add(Key, const_cast<APawn*>(Pawn));
	const uint32* Generation = PawnGenerations.Find(FObjectKey(Pawn));
	return FRosterPawnHandle(Key, Generation != nullptr ? *Generation : 0);
}

void URosterStore::NotifyPawnSpawned(const APawn* Pawn)
{
	if (Pawn != nullptr)
	{
		++PawnGenerations.FindOrAdd(FObjectKey(Pawn));
		PawnsByKey.Add(RosterStoreLocal::ObjectKeyOf(Pawn), const_cast<APawn*>(Pawn));
	}
}

APawn* URosterStore::ResolvePawn(const FRosterPawnHandle& Handle) const
{
	const TWeakObjectPtr<APawn>* Found = PawnsByKey.Find(Handle.ObjectKey);
	return Found != nullptr ? Found->Get() : nullptr;
}

// --- Typed server calls --------------------------------------------------------------------------------------------

FRosterRegisterResult URosterStore::RegisterParticipant(const FIdentityKey& Key, const FString& DisplayName) { return CoreRef().RegisterParticipant(Key, DisplayName); }
ERosterResult URosterStore::RemoveParticipant(FParticipantId Id) { return CoreRef().RemoveParticipant(Id); }
ERosterResult URosterStore::SetTeamSize(int32 TeamSize) { return CoreRef().SetTeamSize(TeamSize); }
ERosterResult URosterStore::SetTeamsLocked(bool bLocked) { return CoreRef().SetTeamsLocked(bLocked); }
ERosterResult URosterStore::AssignTeams(TConstArrayView<FRosterTeamAssignment> Batch) { return CoreRef().AssignTeams(Batch); }

ERosterResult URosterStore::NotifyConnectionChanged(FParticipantId Id, ERosterConnection Projection, bool bKicked)
{
	const int64 NowMs = static_cast<int64>(FMath::FloorToDouble(Adapters->Clock.GetServerTimeSeconds() * 1000.0));
	return CoreRef().NotifyConnectionChanged(Id, Projection, NowMs, bKicked);
}

ERosterResult URosterStore::BindPawn(FParticipantId Id, APawn* Pawn, const TOptional<FRosterEpochToken>& ExpectedEpoch)
{
	return CoreRef().BindPawn(Id, MakePawnHandle(Pawn), ExpectedEpoch);
}

ERosterResult URosterStore::ReleaseToCrowd(APawn* Pawn) { return CoreRef().ReleaseToCrowd(MakePawnHandle(Pawn)); }

ERosterResult URosterStore::RetireBinding(APawn* Pawn, int64 RetireAtServerTimeMs, ERosterReason Reason)
{
	return CoreRef().RetireBinding(MakePawnHandle(Pawn), RetireAtServerTimeMs, Reason);
}

ERosterResult URosterStore::ClearBinding(APawn* Pawn, EEndPlayReason::Type Reason)
{
	ERosterEndPlayReason CoreReason = ERosterEndPlayReason::Destroyed;
	switch (Reason)
	{
	case EEndPlayReason::LevelTransition: CoreReason = ERosterEndPlayReason::LevelTransition; break;
	case EEndPlayReason::EndPlayInEditor: CoreReason = ERosterEndPlayReason::EndPlayInEditor; break;
	case EEndPlayReason::RemovedFromWorld: CoreReason = ERosterEndPlayReason::RemovedFromWorld; break;
	case EEndPlayReason::Quit: CoreReason = ERosterEndPlayReason::Quit; break;
	default: break;
	}
	const FRosterPawnHandle Handle = MakePawnHandle(Pawn);
	const ERosterResult Result = CoreRef().ClearBinding(Handle, CoreReason);
	// EndPlay is the pawn's last Roster contact: drop its handle bookkeeping so the maps do not grow with replacements.
	PawnsByKey.Remove(Handle.ObjectKey);
	if (Pawn != nullptr)
	{
		PawnGenerations.Remove(FObjectKey(Pawn));
	}
	return Result;
}

ERosterResult URosterStore::InvalidateAllBindings() { return CoreRef().InvalidateAllBindings(); }

ERosterResult URosterStore::RequestTransition(FParticipantId Id, EParticipantState NewState, ERosterReason Reason, const FRosterTransitionOptions& Options)
{
	return CoreRef().RequestTransition(Id, NewState, Reason, Options);
}

ERosterResult URosterStore::ResetAll(uint32 RoundIndex) { return CoreRef().ResetAll(RoundIndex); }

FRosterVictimKind URosterStore::ResolveVictimKind(APawn* Pawn) { return CoreRef().ResolveVictimKind(MakePawnHandle(Pawn)); }
bool URosterStore::IsPawnBound(APawn* Pawn) { return CoreRef().IsPawnBound(MakePawnHandle(Pawn)); }
EParticipantState URosterStore::GetLegalityClass(APawn* Pawn) { return CoreRef().GetLegalityClass(MakePawnHandle(Pawn)); }
int32 URosterStore::CountActive(int32 Team) const { return CoreRef().CountActive(Team); }
int32 URosterStore::CountInformant(int32 Team) const { return CoreRef().CountInformant(Team); }
int32 URosterStore::CountRoster(int32 Team) const { return CoreRef().CountRoster(Team); }
bool URosterStore::IsTeamEliminated(int32 Team) const { return CoreRef().IsTeamEliminated(Team); }
APawn* URosterStore::GetPawn(FParticipantId Id) const { return ResolvePawn(CoreRef().GetPawn(Id)); }
FParticipantId URosterStore::GetParticipant(APawn* Pawn) { return CoreRef().GetParticipant(MakePawnHandle(Pawn)); }
void URosterStore::GetUnboundActiveParticipants(TArray<FParticipantId>& OutIds) const { CoreRef().GetUnboundActiveParticipants(OutIds); }

const FParticipantRecord* URosterStore::FindRecord(FParticipantId Id) const { return CoreRef().FindRecord(Id); }
void URosterStore::GetParticipantIds(TArray<FParticipantId>& OutIds) const { CoreRef().GetParticipantIds(OutIds); }
FRosterEpochToken URosterStore::GetEpochToken() const { return CoreRef().GetEpochToken(); }
const FRosterCore& URosterStore::GetCore() const { return CoreRef(); }

// --- C-TeamView ------------------------------------------------------------------------------------------------------

void URosterStore::RegisterTeamView(FParticipantId Id, URosterTeamViewComponent* View)
{
	if (PushStep)
	{
		PushStep->RegisterView(Id, View);
	}
}

void URosterStore::UnregisterTeamView(FParticipantId Id)
{
	if (PushStep)
	{
		PushStep->UnregisterView(Id);
	}
}

int32 URosterStore::FlushTeamViews()
{
	return PushStep ? PushStep->Flush() : 0;
}

bool URosterStore::IsTeammateReconnecting(FParticipantId Id) const
{
	return PushStep ? PushStep->IsReconnecting(Id) : false;
}

// --- Tick --------------------------------------------------------------------------------------------------------------

void URosterStore::Tick(float DeltaTime)
{
	// At most one push per server frame (ADR-0001 guardrail), even if the tickable is visited twice in a frame.
	if (LastFlushFrame != GFrameCounter)
	{
		LastFlushFrame = GFrameCounter;
		FlushTeamViews();
	}
}

bool URosterStore::IsTickable() const
{
	return !IsTemplate() && Core.IsValid() && bMatchOpen && Adapters.IsValid() && Adapters->Authority.HasAuthority();
}

TStatId URosterStore::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(URosterStore, STATGROUP_Tickables);
}
