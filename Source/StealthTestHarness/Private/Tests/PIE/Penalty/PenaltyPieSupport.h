#pragma once

// Shared helpers of the NPC-Kill Penalty PIE tests (stories nkp-014, nkp-015; listen server + 2 clients on L_Prototype WITH
// a crowd): the server's Penalty service and Roster, a participant's team anchor (the stand-in respawn region's centre),
// the server-side line-of-sight test the stand-in pick uses, a spawn tracker, and the respawn probe's attempts.

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS && defined(ENABLE_PIE_NETWORK_TEST) && ENABLE_PIE_NETWORK_TEST && WITH_STEALTH_TEST_SEAMS

#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "Server/Roster/StealthRosterStore.h"
#include "Server/StealthCompositionRoot.h"
#include "Server/StealthPenaltyHarnessSeams.h"
#include "Server/StealthPenaltyService.h"
#include "Server/StealthTuningInvariants.h"
#include "StealthPlayerController.h"
#include "Tests/PIE/Assassination/AssassinationPieSupport.h"

namespace StealthPenaltyPie
{
	/** The NPC count of the Penalty PIE rounds (ADR-0019's guardrail measures the pick with 40 NPCs). */
	inline constexpr int32 CrowdNpcCount = 40;

	inline URosterStore* Roster()
	{
		const UStealthCompositionRoot* Root = UStealthCompositionRoot::Get(StealthAssassinationPie::ServerWorld());
		URosterStore* Store = Root != nullptr ? Root->GetRosterStore() : nullptr;
		return Store != nullptr && Store->IsStoreInitialized() ? Store : nullptr;
	}

	inline UPenaltyService* Penalty()
	{
		UWorld* Server = StealthAssassinationPie::ServerWorld();
		UPenaltyService* Service = Server != nullptr ? Server->GetSubsystem<UPenaltyService>() : nullptr;
		return Service != nullptr && Service->IsBound() ? Service : nullptr;
	}

	inline FParticipantId ParticipantOf(const APlayerController* Controller)
	{
		const UStealthCompositionRoot* Root = UStealthCompositionRoot::Get(StealthAssassinationPie::ServerWorld());
		return Root != nullptr && Controller != nullptr ? Root->GetParticipant(Controller) : FParticipantId();
	}

	inline ERosterTeam TeamOf(FParticipantId Participant)
	{
		const URosterStore* Store = Roster();
		const FParticipantRecord* Record = Store != nullptr ? Store->FindRecord(Participant) : nullptr;
		return Record != nullptr ? Record->Team : ERosterTeam::None;
	}

	/** The Roster's bound pawn of Participant (null while it has none). */
	inline APawn* BodyOf(FParticipantId Participant)
	{
		const URosterStore* Store = Roster();
		return Store != nullptr ? Store->GetPawn(Participant) : nullptr;
	}

	inline ERosterTeam OtherTeam(ERosterTeam Team)
	{
		return Team == ERosterTeam::Team0 ? ERosterTeam::Team1 : ERosterTeam::Team0;
	}

	/** The team's spawn anchor (the centroid of its "Team<N>" PlayerStarts), as the stand-in region uses it. */
	inline bool TeamAnchor(ERosterTeam Team, FVector& OutAnchor)
	{
		UWorld* Server = StealthAssassinationPie::ServerWorld();
		if (Server == nullptr || Team == ERosterTeam::None)
		{
			return false;
		}
		const FName Wanted(*FString::Printf(TEXT("Team%d"), static_cast<int32>(Team)));
		FVector Sum = FVector::ZeroVector;
		int32 Count = 0;
		for (TActorIterator<APlayerStart> It(Server); It; ++It)
		{
			if (It->PlayerStartTag == Wanted)
			{
				Sum += It->GetActorLocation();
				++Count;
			}
		}
		OutAnchor = Count > 0 ? Sum / static_cast<double>(Count) : FVector::ZeroVector;
		return Count > 0;
	}

	/**
	 * The stand-in respawn region (Level Socket Contract story 033 replaces it; FStealthPenaltyRespawnGateway's documented
	 * radius): level 0 within InformantRespawnPathCm of the team anchor, level 1 within twice that, level 2 anywhere not
	 * within InformantRespawnPathCm of the enemy anchor.
	 */
	inline bool IsInRespawnRegion(ERosterTeam Team, const FVector& Location, uint8 WideningLevel)
	{
		const double Radius = StealthTuningPendingOwnerKeys::InformantRespawnPathCm;
		FVector Anchor;
		FVector Enemy;
		if (!TeamAnchor(Team, Anchor))
		{
			return false;
		}
		if (WideningLevel == 0)
		{
			return FVector::Dist2D(Location, Anchor) <= Radius;
		}
		if (WideningLevel == 1)
		{
			return FVector::Dist2D(Location, Anchor) <= 2.0 * Radius;
		}
		return !TeamAnchor(OtherTeam(Team), Enemy) || FVector::Dist2D(Location, Enemy) > Radius;
	}

	/** True when some live pawn of another team's participant has a clear Visibility line to Target (LSC R5b.2 viewers). */
	inline bool IsSeenByOpponents(ERosterTeam Team, const APawn* Target)
	{
		UWorld* Server = StealthAssassinationPie::ServerWorld();
		URosterStore* Store = Roster();
		if (Server == nullptr || Store == nullptr || Target == nullptr)
		{
			return false;
		}
		TArray<FParticipantId> Ids;
		Store->GetParticipantIds(Ids);
		for (const FParticipantId& Id : Ids)
		{
			const FParticipantRecord* Record = Store->FindRecord(Id);
			const APawn* Viewer = Record != nullptr && Record->Team != Team && Record->Team != ERosterTeam::None ? Store->GetPawn(Id) : nullptr;
			if (Viewer == nullptr)
			{
				continue;
			}
			FCollisionQueryParams Params(SCENE_QUERY_STAT(PenaltyPieSeen), false);
			Params.AddIgnoredActor(Viewer);
			Params.AddIgnoredActor(Target);
			FHitResult Hit;
			if (!Server->LineTraceSingleByChannel(Hit, Viewer->GetPawnViewLocation(), Target->GetActorLocation(), ECC_Visibility, Params))
			{
				return true;
			}
		}
		return false;
	}

	/** Every Shared Pawn spawned in the server world while it runs, with its spawn server time and frame. */
	struct FSpawnTracker
	{
		struct FSpawn
		{
			double ServerTime = 0.0;
			uint64 Frame = 0;
		};
		TMap<TWeakObjectPtr<APawn>, FSpawn> Spawned;
		TWeakObjectPtr<UWorld> World;
		FDelegateHandle Handle;

		void Start(UWorld* Server)
		{
			Stop();
			World = Server;
			if (Server != nullptr)
			{
				Handle = Server->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateLambda([this](AActor* Actor)
				{
					if (APawn* Pawn = Cast<APawn>(Actor))
					{
						FSpawn Spawn;
						Spawn.ServerTime = StealthAssassinationPie::ServerNow(World.Get());
						Spawn.Frame = GFrameCounter;
						Spawned.Add(TWeakObjectPtr<APawn>(Pawn), Spawn);
					}
				}));
			}
		}

		void Stop()
		{
			if (UWorld* Server = World.Get(); Server != nullptr && Handle.IsValid())
			{
				Server->RemoveOnActorSpawnedHandler(Handle);
			}
			Handle.Reset();
			World.Reset();
		}

		/** True when Pawn existed before ServerTime (spawned earlier, or before the tracker started). */
		bool ExistedBefore(const APawn* Pawn, double ServerTime) const
		{
			const FSpawn* Found = Spawned.Find(const_cast<APawn*>(Pawn));
			return Found == nullptr || Found->ServerTime < ServerTime;
		}

		/** Pawns spawned at or after ServerTime that the Roster binds to any participant now. */
		int32 CountBoundSpawnedSince(double ServerTime) const
		{
			URosterStore* Store = Roster();
			int32 Count = 0;
			for (const TPair<TWeakObjectPtr<APawn>, FSpawn>& Pair : Spawned)
			{
				APawn* Pawn = Pair.Key.Get();
				Count += Pawn != nullptr && Store != nullptr && Pair.Value.ServerTime >= ServerTime && Store->IsPawnBound(Pawn) ? 1 : 0;
			}
			return Count;
		}
	};

	using EProbeStep = StealthPenaltyHarnessSeams::ERespawnProbeStep;
	using FProbeEvent = StealthPenaltyHarnessSeams::FRespawnProbeEvent;

	/** One commit attempt: one Pick and the gateway calls that followed it in the same call stack. */
	struct FAttempt
	{
		TArray<FProbeEvent> Events;

		const FProbeEvent* Find(EProbeStep Step) const
		{
			return Events.FindByPredicate([Step](const FProbeEvent& Event) { return Event.Step == Step; });
		}
		int32 Count(EProbeStep Step) const
		{
			return Events.FilterByPredicate([Step](const FProbeEvent& Event) { return Event.Step == Step; }).Num();
		}
	};

	/** The probe's events split into attempts (a new attempt at every Pick; events before the first Pick are dropped). */
	inline TArray<FAttempt> Attempts()
	{
		TArray<FAttempt> Out;
		for (const FProbeEvent& Event : StealthPenaltyHarnessSeams::GetRespawnProbeEvents())
		{
			if (Event.Step == EProbeStep::Pick)
			{
				Out.AddDefaulted();
			}
			if (Out.Num() > 0)
			{
				Out.Last().Events.Add(Event);
			}
		}
		return Out;
	}

	inline const TCHAR* LexState(EInformantLifeState State)
	{
		switch (State)
		{
		case EInformantLifeState::Active: return TEXT("Active");
		case EInformantLifeState::Awaiting: return TEXT("Awaiting");
		case EInformantLifeState::Picking: return TEXT("Picking");
		case EInformantLifeState::Embodied: return TEXT("Embodied");
		case EInformantLifeState::Absent: return TEXT("Absent");
		case EInformantLifeState::RoundEnded: return TEXT("RoundEnded");
		default: return TEXT("?");
		}
	}
}

#endif
