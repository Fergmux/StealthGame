// Story asn-022: the test-only per-connection relevancy deny seam. See Server/Testing/StealthRelevancyDenySeam.h.

#include "Server/Testing/StealthRelevancyDenySeam.h"

#if WITH_STEALTH_TEST_SEAMS

#include "Engine/ActorChannel.h"
#include "Engine/NetConnection.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "View/Core/StealthPawn.h"

namespace StealthRelevancyDenySeam
{
	namespace Local
	{
		struct FDeny
		{
			TWeakObjectPtr<const AActor> Pawn;
			TWeakObjectPtr<const AActor> Viewer;
			ERelease Release = ERelease::Manual;
		};

		TArray<FDeny>& Denies()
		{
			static TArray<FDeny> Table;
			return Table;
		}

		TArray<FReleaseRecord>& Releases()
		{
			static TArray<FReleaseRecord> Records;
			return Records;
		}

		void Record(const FDeny& Deny, bool bAtDeathWrite)
		{
			FReleaseRecord& Out = Releases().AddDefaulted_GetRef();
			Out.Pawn = Deny.Pawn.Get();
			Out.Viewer = Deny.Viewer.Get();
			Out.Frame = GFrameCounter;
			const UWorld* World = Out.Pawn != nullptr ? Out.Pawn->GetWorld() : nullptr;
			Out.WorldTime = World != nullptr ? World->GetTimeSeconds() : 0.0;
			Out.bAtDeathWrite = bAtDeathWrite;
		}
	}

	bool Deny(AStealthPawn* Pawn, const AActor* Viewer, ERelease Release)
	{
		check(IsInGameThread());
		if (Pawn == nullptr || Viewer == nullptr)
		{
			return false;
		}
		bool bKnown = false;
		for (Local::FDeny& Entry : Local::Denies())
		{
			if (Entry.Pawn.Get() == Pawn && Entry.Viewer.Get() == Viewer)
			{
				Entry.Release = Release;
				bKnown = true;
			}
		}
		if (!bKnown)
		{
			Local::Denies().Add({ Pawn, Viewer, Release });
		}
		// The explicit close of the cull tick (S9): the engine's own relevancy timeout would keep the channel open for seconds.
		const APlayerController* Controller = Cast<APlayerController>(Viewer);
		UNetConnection* Connection = Controller != nullptr ? Controller->GetNetConnection() : nullptr;
		UActorChannel* Channel = Connection != nullptr ? Connection->FindActorChannelRef(TWeakObjectPtr<AActor>(Pawn)) : nullptr;
		if (Channel != nullptr && !Channel->Closing)
		{
			Channel->Close(EChannelCloseReason::Relevancy);
			return true;
		}
		return false;
	}

	void Release(const AActor* Pawn, const AActor* Viewer)
	{
		check(IsInGameThread());
		Local::Denies().RemoveAll([Pawn, Viewer](const Local::FDeny& Entry)
		{
			const bool bMatch = Entry.Pawn.Get() == Pawn && (Viewer == nullptr || Entry.Viewer.Get() == Viewer);
			if (bMatch)
			{
				Local::Record(Entry, false);
			}
			return bMatch;
		});
	}

	void ReleaseAll()
	{
		check(IsInGameThread());
		Local::Denies().Reset();
		Local::Releases().Reset();
	}

	bool IsDenied(const AActor* Pawn, const AActor* RealViewer)
	{
		if (Pawn == nullptr || RealViewer == nullptr || Local::Denies().IsEmpty())
		{
			return false;
		}
		return Local::Denies().ContainsByPredicate([Pawn, RealViewer](const Local::FDeny& Entry)
		{
			return Entry.Pawn.Get() == Pawn && Entry.Viewer.Get() == RealViewer;
		});
	}

	void NotifyDeathWrite(const AActor* Pawn)
	{
		if (Pawn == nullptr || Local::Denies().IsEmpty())
		{
			return;
		}
		Local::Denies().RemoveAll([Pawn](const Local::FDeny& Entry)
		{
			const bool bMatch = Entry.Pawn.Get() == Pawn && Entry.Release == ERelease::AtDeathWrite;
			if (bMatch)
			{
				Local::Record(Entry, true);
			}
			return bMatch;
		});
	}

	const TArray<FReleaseRecord>& GetReleases()
	{
		return Local::Releases();
	}
}

#endif
