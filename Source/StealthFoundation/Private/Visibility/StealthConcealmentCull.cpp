// IVP stories 024/025 and Concealment story con-025: the concealment cull (ADR-0003 sections 5.1-5.6, S10-R2/R3). See
// Server/Visibility/StealthConcealmentCull.h.

#include "Server/Visibility/StealthConcealmentCull.h"

#include "Engine/ActorChannel.h"
#include "Engine/NetConnection.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

void FStealthConcealmentCullTick::ExecuteTick(float, ELevelTick, ENamedThreads::Type, const FGraphEventRef&)
{
	if (Owner != nullptr)
	{
		const UWorld* World = Owner->GetWorld();
		const AGameStateBase* State = World != nullptr ? World->GetGameState() : nullptr;
		Owner->TickCull(State != nullptr ? State->GetServerWorldTimeSeconds() : (World != nullptr ? World->GetTimeSeconds() : 0.0));
	}
}

bool UStealthConcealmentCull::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World != nullptr && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE) && World->GetNetMode() != NM_Client;
}

void UStealthConcealmentCull::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	CullTick.Owner = this;
	CullTick.TickGroup = TG_PostUpdateWork;
	CullTick.bCanEverTick = true;
	CullTick.bTickEvenWhenPaused = true;
	CullTick.bAllowTickOnDedicatedServer = true;
	if (InWorld.PersistentLevel != nullptr)
	{
		CullTick.RegisterTickFunction(InWorld.PersistentLevel);
	}
}

void UStealthConcealmentCull::Deinitialize()
{
	if (CullTick.IsTickFunctionRegistered())
	{
		CullTick.UnRegisterTickFunction();
	}
	Records.Reset();
	PendingCloses.Reset();
	Super::Deinitialize();
}

UStealthConcealmentCull* UStealthConcealmentCull::Get(const UWorld* World)
{
	return World != nullptr ? World->GetSubsystem<UStealthConcealmentCull>() : nullptr;
}

void UStealthConcealmentCull::ConfigureTeams(TFunction<int32(const APawn*)> InPawnTeam, TFunction<int32(const AActor*)> InViewerTeam)
{
	PawnTeam = MoveTemp(InPawnTeam);
	ViewerTeam = MoveTemp(InViewerTeam);
}

double UStealthConcealmentCull::ComputeDeadline(double CommitServerTime) const
{
	const double Period = PawnNetUpdateHz > 0.0 ? 1.0 / PawnNetUpdateHz : 0.0;
	const double Earliest = CommitServerTime + CullDelayMs / 1000.0;
	if (Period <= 0.0)
	{
		return Earliest;
	}
	// The first grid point at or after Earliest (1 ms tolerance: a point a hair below Earliest by rounding counts).
	const double Steps = FMath::CeilToDouble((Earliest - GridAnchor) / Period - 0.001 / Period);
	return GridAnchor + FMath::Max(Steps, 0.0) * Period;
}

void UStealthConcealmentCull::SetCullState(APawn* Pawn, bool bConcealed, double CommitServerTime)
{
	if (Pawn == nullptr)
	{
		return;
	}
	FRecord& Record = Records.FindOrAdd(TWeakObjectPtr<APawn>(Pawn));
	if (bConcealed)
	{
		Record.State = EConcealCullState::CullPending;
		Record.CommitServerTime = CommitServerTime;
		Record.CullDeadline = ComputeDeadline(CommitServerTime);
		return;
	}
	// Un-cull (or cancel a pending cull) in the same server step: this frame's flush may re-open the channel, subject to
	// the S10 re-open guard on a connection whose close is still unacknowledged.
	Record.State = EConcealCullState::Visible;
}

EConcealCullState UStealthConcealmentCull::GetState(const APawn* Pawn) const
{
	const FRecord* Record = Pawn != nullptr ? Records.Find(TWeakObjectPtr<APawn>(const_cast<APawn*>(Pawn))) : nullptr;
	return Record != nullptr ? Record->State : EConcealCullState::Visible;
}

double UStealthConcealmentCull::GetDeadline(const APawn* Pawn) const
{
	const FRecord* Record = Pawn != nullptr ? Records.Find(TWeakObjectPtr<APawn>(const_cast<APawn*>(Pawn))) : nullptr;
	return Record != nullptr ? Record->CullDeadline : -1.0;
}

bool UStealthConcealmentCull::IsAlly(const APawn* Pawn, const AActor* Viewer) const
{
	if (Pawn == nullptr || Viewer == nullptr)
	{
		return false;
	}
	const APlayerController* Controller = Cast<APlayerController>(Viewer);
	if (Controller != nullptr && Controller->GetPawn() == Pawn)
	{
		return true; // the owner (its own pawn)
	}
	const int32 PawnSide = PawnTeam ? PawnTeam(Pawn) : -1;
	const int32 ViewerSide = ViewerTeam ? ViewerTeam(Viewer) : -1;
	return PawnSide >= 0 && PawnSide == ViewerSide; // NPCs (-1) have no allies
}

bool UStealthConcealmentCull::IsRelevantTo(const APawn* Pawn, const AActor* RealViewer) const
{
	const FRecord* Record = Pawn != nullptr ? Records.Find(TWeakObjectPtr<APawn>(const_cast<APawn*>(Pawn))) : nullptr;
	if (Record == nullptr)
	{
		return true;
	}
	if (Record->State == EConcealCullState::Culled && !IsAlly(Pawn, RealViewer))
	{
		return false;
	}
	// S10-R2: a re-open waits for the acknowledgement of the close on that connection (non-stale entries only).
	const APlayerController* Controller = Cast<APlayerController>(RealViewer);
	const UNetConnection* Connection = Controller != nullptr ? Controller->GetNetConnection() : nullptr;
	if (Connection != nullptr)
	{
		for (const FPendingClose& Entry : PendingCloses)
		{
			if (!Entry.bStale && Entry.Pawn.Get() == Pawn && Entry.Connection.Get() == Connection)
			{
				return false;
			}
		}
	}
	return true;
}

void UStealthConcealmentCull::CloseForNonAllies(APawn* Pawn, double Now)
{
	UWorld* World = GetWorld();
	UNetDriver* Driver = World != nullptr ? World->GetNetDriver() : nullptr;
	if (Driver == nullptr)
	{
		return;
	}
	for (UNetConnection* Connection : Driver->ClientConnections)
	{
		if (Connection == nullptr || IsAlly(Pawn, Connection->PlayerController))
		{
			continue;
		}
		UActorChannel* Channel = Connection->FindActorChannelRef(Pawn);
		if (Channel == nullptr || Channel->Closing)
		{
			continue;
		}
		const int32 ChIndex = Channel->ChIndex;
		Channel->Close(EChannelCloseReason::Relevancy);
		++ClosedChannels;
		if (CloseLog.Num() >= 1024)
		{
			CloseLog.RemoveAt(0, 512, EAllowShrinking::No); // an observation log: bounded over a long session
		}
		CloseLog.Add({ TWeakObjectPtr<APawn>(Pawn), Now });
		if (Channel->Closing)
		{
			FPendingClose& Entry = PendingCloses.AddDefaulted_GetRef();
			Entry.Pawn = Pawn;
			Entry.Connection = Connection;
			Entry.ClosedChannel = Channel;
			Entry.ChIndex = ChIndex;
			Entry.SentServerTime = Now;
		}
	}
}

void UStealthConcealmentCull::UpdatePendingCloses(double Now)
{
	for (int32 Index = PendingCloses.Num() - 1; Index >= 0; --Index)
	{
		FPendingClose& Entry = PendingCloses[Index];
		UNetConnection* Connection = Entry.Connection.Get();
		if (Connection == nullptr || Entry.Pawn.Get() == nullptr)
		{
			PendingCloses.RemoveAt(Index);
			continue;
		}
		// The acknowledgement read from the slot every server frame: the closed channel left its slot, or is no longer Closing.
		const bool bInSlot = Connection->Channels.IsValidIndex(Entry.ChIndex) && Connection->Channels[Entry.ChIndex].Get() == Entry.ClosedChannel;
		const bool bAcked = !bInSlot || !Entry.ClosedChannel->Closing;
		if (bAcked)
		{
			PendingCloses.RemoveAt(Index);
			continue;
		}
		if (!Entry.bStale && Now - Entry.SentServerTime >= ReopenAckTimeoutSec)
		{
			Entry.bStale = true; // S10-R3: the hold is released; the entry stays watched until it is acknowledged
			++ReopenTimeouts;
		}
	}
}

void UStealthConcealmentCull::TickCull(double Now)
{
	for (TPair<TWeakObjectPtr<APawn>, FRecord>& Pair : Records)
	{
		APawn* Pawn = Pair.Key.Get();
		FRecord& Record = Pair.Value;
		if (Pawn != nullptr && Record.State == EConcealCullState::CullPending && Now + 0.001 >= Record.CullDeadline)
		{
			Record.State = EConcealCullState::Culled;
			CloseForNonAllies(Pawn, Now); // in the same frame the deadline passes
		}
	}
	for (auto It = Records.CreateIterator(); It; ++It)
	{
		if (!It->Key.IsValid())
		{
			It.RemoveCurrent(); // a destroyed pawn's record
		}
	}
	UpdatePendingCloses(Now);
}
