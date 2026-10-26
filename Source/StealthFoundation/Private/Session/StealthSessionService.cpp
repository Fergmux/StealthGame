#include "Server/Session/StealthSessionService.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Server/Roster/StealthRosterStore.h"
#include "Server/StealthFaultSink.h"

namespace StealthSessionServicePrivate
{
	const FName FaultSource(TEXT("SessionService"));
}

void USessionService::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<URosterStore>();
	InitializeService();
}

void USessionService::Deinitialize()
{
	HandleGameInstanceShutdown();
	if (WorldCleanupHandle.IsValid())
	{
		FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
		WorldCleanupHandle.Reset();
	}
	Stores.Reset();
	Super::Deinitialize();
}

void USessionService::InitializeService(const FSessionServiceInitOverrides& Overrides)
{
	AuthorityOverride = Overrides.bAuthority;
	FaultSinkOverride = Overrides.FaultSink;
	EntropyOverride = Overrides.Entropy;
	RosterOverride = Overrides.Roster;
	Core.GetSeeds().SetFaultSink(&GetFaultSink());
	Core.GetSeeds().SetAuthorityQuery([this]() { return HasAuthority(); });
	if (!WorldCleanupHandle.IsValid())
	{
		WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(this, &USessionService::HandleWorldCleanup);
	}
	bServiceInitialized = true;
}

USessionService* USessionService::Get(const UWorld* World)
{
	const UGameInstance* GameInstance = World != nullptr ? World->GetGameInstance() : nullptr;
	return GameInstance != nullptr ? GameInstance->GetSubsystem<USessionService>() : nullptr;
}

bool USessionService::HasAuthority() const
{
	if (AuthorityOverride.IsSet())
	{
		return AuthorityOverride.GetValue();
	}
	const UGameInstance* GameInstance = GetGameInstance();
	const UWorld* World = GameInstance != nullptr ? GameInstance->GetWorld() : nullptr;
	return World != nullptr ? World->GetNetMode() != NM_Client : !IsRunningClientOnly();
}

IStealthFaultSink& USessionService::GetFaultSink()
{
	return FaultSinkOverride != nullptr ? *FaultSinkOverride : static_cast<IStealthFaultSink&>(LogFaultSink);
}

URosterStore* USessionService::GetRoster() const
{
	if (RosterOverride.IsValid())
	{
		return RosterOverride.Get();
	}
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance != nullptr ? GameInstance->GetSubsystem<URosterStore>() : nullptr;
}

EStealthSessionOpenResult USessionService::OpenSession(const FSessionOpenParams& Params)
{
	if (!HasAuthority() || !IsInGameThread())
	{
		GetFaultSink().Report(EStealthFault::SeedMisuse, StealthSessionServicePrivate::FaultSource, 0);
		return EStealthSessionOpenResult::CouldNotHost;
	}
	if (Core.IsOpen())
	{
		return EStealthSessionOpenResult::Opened;
	}
	// A store that survives into a new session is a defect (ADR-0005 Guideline 12): record it and clear it first.
	TArray<FName> NonEmpty;
	bStoresEmptyAtLastOpen = AreAllStoresEmpty(&NonEmpty);
	if (!bStoresEmptyAtLastOpen)
	{
		GetFaultSink().Report(EStealthFault::InvariantViolation, StealthSessionServicePrivate::FaultSource, NonEmpty.Num());
		EndSession(ESessionEndReason::SessionEnding);
	}

	EStealthSessionOpenResult Result = EStealthSessionOpenResult::CouldNotHost;
	if (EntropyOverride != nullptr)
	{
		Result = Core.Open(*EntropyOverride, &GetFaultSink());
	}
	else
	{
#if !UE_BUILD_SHIPPING
		// Test injection (ADR-0006 Guideline 8): -StealthServerSecret=<32 hex> pins the secret for a reproducible run.
		TArray<uint8> OverrideBytes;
		if (FDeterministicEntropySource::FromCommandLine(FCommandLine::Get(), OverrideBytes))
		{
			FDeterministicEntropySource Override(OverrideBytes);
			Result = Core.Open(Override, &GetFaultSink());
		}
		else
#endif
		{
			FOsEntropySource Entropy; // ADR-0006 section 2: the OS CSPRNG, never a GUID or a time seed
			Result = Core.Open(Entropy, &GetFaultSink());
		}
	}
	if (Result != EStealthSessionOpenResult::Opened)
	{
		return Result; // could not host: no weaker source, nothing begun
	}
	if (Params.bBeginRosterMatch)
	{
		if (URosterStore* Roster = GetRoster(); Roster != nullptr && Roster->IsStoreInitialized() && !Roster->IsMatchOpen())
		{
			Roster->OpenSession(Params.TeamSize);
		}
	}
	return Result;
}

void USessionService::EndSession(ESessionEndReason Reason)
{
	if (!bServiceInitialized)
	{
		return;
	}
	Core.End(Reason);
	if (URosterStore* Roster = GetRoster(); Roster != nullptr && Roster->IsStoreInitialized())
	{
		switch (Reason)
		{
		case ESessionEndReason::HostQuit: Roster->NotifyHostQuit(); break;
		case ESessionEndReason::ReturnToMainMenu: Roster->NotifyReturnToMainMenu(); break;
		case ESessionEndReason::SessionEnding: Roster->NotifySessionEnding(); break;
		case ESessionEndReason::GameInstanceShutdown:
		case ESessionEndReason::LastWorldCleanup: Roster->HandleGameInstanceShutdown(); break;
		}
	}
	for (int32 Index = Stores.Num() - 1; Index >= 0; --Index)
	{
		if (!Stores[Index].Owner.IsValid())
		{
			GetFaultSink().Report(EStealthFault::DeadRegistration, StealthSessionServicePrivate::FaultSource, Index);
			Stores.RemoveAt(Index);
			continue;
		}
		Stores[Index].Store->EndMatch(Reason);
	}
	bTravelInProgress = false;
	OnSessionEnded.Broadcast(Reason);
}

void USessionService::HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	// Only this game instance's networked game world, and not the old world of a travel.
	if (World == nullptr || !World->IsGameWorld() || World->GetGameInstance() != GetGameInstance() || bTravelInProgress)
	{
		return;
	}
	if (World->GetNetMode() == NM_Standalone && !Core.IsOpen())
	{
		return; // a menu or standalone world that never hosted
	}
	EndSession(ESessionEndReason::LastWorldCleanup);
}

void USessionService::SetTravelInProgress(bool bInProgress)
{
	bTravelInProgress = bInProgress;
	if (URosterStore* Roster = GetRoster())
	{
		Roster->SetTravelInProgress(bInProgress);
	}
}

bool USessionService::AreAllStoresEmpty(TArray<FName>* OutNonEmpty) const
{
	bool bEmpty = true;
	auto Note = [&bEmpty, OutNonEmpty](FName Name)
	{
		bEmpty = false;
		if (OutNonEmpty != nullptr)
		{
			OutNonEmpty->Add(Name);
		}
	};
	if (!Core.IsEmpty())
	{
		Note(TEXT("Session"));
	}
	if (const URosterStore* Roster = GetRoster(); Roster != nullptr && Roster->IsStoreInitialized())
	{
		TArray<FParticipantId> Ids;
		Roster->GetParticipantIds(Ids);
		if (Roster->IsMatchOpen() || Ids.Num() > 0 || Roster->GetCore().GetMatchId() != 0)
		{
			Note(TEXT("Roster"));
		}
	}
	for (const FRegisteredStore& Entry : Stores)
	{
		if (Entry.Owner.IsValid() && !Entry.Store->IsMatchStateEmpty())
		{
			Note(Entry.Store->GetMatchStoreName());
		}
	}
	return bEmpty;
}

void USessionService::RegisterMatchStore(IStealthMatchStore* Store, UObject* Owner)
{
	if (Store == nullptr || Owner == nullptr)
	{
		return;
	}
	UnregisterMatchStore(Owner);
	Stores.Add({ Store, Owner });
}

void USessionService::UnregisterMatchStore(UObject* Owner)
{
	Stores.RemoveAll([Owner](const FRegisteredStore& Entry) { return Entry.Owner.Get() == Owner || !Entry.Owner.IsValid(); });
}

int32 USessionService::DrawRoundSeed(uint32 RoundIndex)
{
	return Core.DrawRoundSeed(RoundIndex);
}

uint64 USessionService::GetServerSubSeed(ESeedDomain Domain, uint32 Attempt) const
{
	return Core.GetSeeds().GetServerSubSeed(Domain, Attempt);
}
