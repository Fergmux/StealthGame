#pragma once

// Shared fixture of the Objectives PIE-2 tests (stories oc-019, oc-020, oc-021; ADR-0008 T2, listen server + clients on
// L_Prototype, NpcCount 0): the server's Objectives service and the assignment it bound (read through the server-only core:
// the test knows the courts, a client never does), each player's machine and team view, the court sockets, a server-side tend
// (a pawn placed at a court submits InteractBegin with its controller's own origin, the StealthBot path) and a client-side
// one (the owning client sends the command over its RPC, the human path), and a per-frame recorder of each client's received
// replicated fields (the in-process stand-in for a wire trace: every replicated property of every replicated actor and
// component of a client world, hashed per frame; a change is a received field).

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS && defined(ENABLE_PIE_NETWORK_TEST) && ENABLE_PIE_NETWORK_TEST && WITH_STEALTH_TEST_SEAMS

#include "Components/ActorComponent.h"
#include "EngineUtils.h"
#include "Server/ObjectivesService.h"
#include "View/StealthCourtSocket.h"
#include "StealthSettings.h"
#include "Tests/PIE/Assassination/AssassinationPieSupport.h"
#include "UObject/UnrealType.h"
#include "View/NetPawnCommand.h"
#include "View/ObjectiveTeamViewComponent.h"

namespace StealthObjectivesPie
{
	using namespace StealthAssassinationPie;

	/** The five roles of the bound assignment. */
	struct FCourts
	{
		FCourtFixtureId P1[2];
		FCourtFixtureId P2[2];
		FCourtFixtureId Final;
		bool IsSet() const { return P1[0].IsSet() && P1[1].IsSet() && P2[0].IsSet() && P2[1].IsSet() && Final.IsSet(); }
	};

	inline UObjectivesService* Service()
	{
		return UObjectivesService::Get(ServerWorld());
	}

	/** The courts the server bound (the service's server-only dev read; a client never learns it). */
	inline FCourts BoundCourts()
	{
		FCourts Out;
		if (const UObjectivesService* Objectives = Service())
		{
			const FSiteAssignment Sites = Objectives->GetBoundSitesForDev();
			Out.P1[0] = Sites.P1[0];
			Out.P1[1] = Sites.P1[1];
			Out.P2[0] = Sites.P2[0];
			Out.P2[1] = Sites.P2[1];
			Out.Final = Sites.Final;
		}
		return Out;
	}

	inline const FObjectiveTeamView& ViewOf(const AStealthPlayerController* Controller)
	{
		static const FObjectiveTeamView Empty;
		const UObjectiveTeamViewComponent* Component = Controller != nullptr ? Controller->GetObjectiveTeamView() : nullptr;
		return Component != nullptr ? Component->GetView() : Empty;
	}

	/** The courts in a view. */
	inline TSet<uint16> CourtsOf(const FObjectiveTeamView& View)
	{
		TSet<uint16> Out;
		for (const FObjectiveLiveSite& Site : View.LiveSites)
		{
			Out.Add(Site.Court.Value);
		}
		return Out;
	}

	/** The court socket's world position on World (null court: zero). */
	inline FVector CourtLocation(UWorld* World, FCourtFixtureId Court)
	{
		const AStealthCourtSocket* Socket = AStealthCourtSocket::FindCourt(World, Court);
		return Socket != nullptr ? Socket->GetActorLocation() : FVector::ZeroVector;
	}

	/** Teleports the server pawn of Controller next to Court (Distance cm along +X of the fixture, on the floor). */
	inline void PlaceAtCourt(APawn* Pawn, FCourtFixtureId Court, double DistanceCm = 140.0)
	{
		UWorld* Server = ServerWorld();
		const FVector Centre = CourtLocation(Server, Court);
		FVector Spot = Centre + FVector(DistanceCm, 0.0, 0.0);
		FHitResult Floor;
		if (Server->LineTraceSingleByChannel(Floor, Spot + FVector(0.0, 0.0, 300.0), Spot - FVector(0.0, 0.0, 600.0), ECC_Visibility))
		{
			Spot.Z = Floor.ImpactPoint.Z + 100.0;
		}
		Pawn->TeleportTo(Spot, (Centre - Spot).GetSafeNormal2D().Rotation(), false, true);
	}

	/** Server-side Interact(Begin) at Court with the pawn's controller's own origin (the StealthBot path). */
	inline void BeginTendAsOwner(AStealthPawn* Pawn, FCourtFixtureId Court, uint32& Seq)
	{
		AsOwner(Pawn, Seq, [Court](UPawnCommandComponent& Command) { Command.InteractBegin(Court); });
	}

	/** Client-side Interact(Begin) at Court: the owning client's command over its RPC (the human path). */
	inline void BeginTendFromClient(AStealthPlayerController* Local, FCourtFixtureId Court, uint32& Seq)
	{
		FNetPawnCommand Cmd;
		Cmd.Kind = ECommandKind::InteractBegin;
		Cmd.Seq.PossessionEpoch = Local->GetOwnerPossessionEpoch();
		Cmd.Seq.Seq = ++Seq;
		Cmd.Args.CourtFixtureId = Court;
		Local->SendPawnCommand(Cmd, nullptr);
	}

	/** The local controller of a client world (its first player controller). */
	inline AStealthPlayerController* LocalOf(UWorld* World)
	{
		return World != nullptr ? Cast<AStealthPlayerController>(World->GetFirstPlayerController()) : nullptr;
	}

	/**
	 * The per-frame replicated-field recorder of one client world: every replicated property of every replicated actor and
	 * its replicated components, hashed each Sample(); a hash change is a received field "Class.Property". Window() returns
	 * the fields changed since BeginWindow().
	 */
	struct FFieldRecorder
	{
		TWeakObjectPtr<UWorld> World;
		TMap<FString, uint32> Last;
		TSet<FString> Changed;

		static void HashObject(const UObject* Object, const FString& Prefix, TMap<FString, uint32>& Out)
		{
			for (TFieldIterator<FProperty> It(Object->GetClass()); It; ++It)
			{
				const FProperty* Property = *It;
				if (!Property->HasAnyPropertyFlags(CPF_Net))
				{
					continue;
				}
				FString Text;
				Property->ExportTextItem_InContainer(Text, Object, nullptr, nullptr, PPF_None);
				Out.Add(Prefix + Object->GetClass()->GetName() + TEXT(".") + Property->GetName(), GetTypeHash(Text));
			}
		}

		TMap<FString, uint32> Snapshot() const
		{
			TMap<FString, uint32> Out;
			UWorld* W = World.Get();
			if (W == nullptr)
			{
				return Out;
			}
			for (TActorIterator<AActor> It(W); It; ++It)
			{
				const AActor* Actor = *It;
				if (!Actor->GetIsReplicated())
				{
					continue;
				}
				const FString Prefix = Actor->GetName() + TEXT(":");
				HashObject(Actor, Prefix, Out);
				for (const UActorComponent* Component : Actor->GetComponents())
				{
					if (Component != nullptr && Component->GetIsReplicated())
					{
						HashObject(Component, Prefix, Out);
					}
				}
			}
			return Out;
		}

		void BeginWindow()
		{
			Last = Snapshot();
			Changed.Reset();
		}

		void Sample()
		{
			const TMap<FString, uint32> Now = Snapshot();
			for (const TPair<FString, uint32>& Field : Now)
			{
				const uint32* Before = Last.Find(Field.Key);
				if (Before == nullptr || *Before != Field.Value)
				{
					// "Instance:Class.Property" -> "Class.Property" (the field name, the instance dropped).
					int32 Colon = INDEX_NONE;
					Field.Key.FindChar(TEXT(':'), Colon);
					Changed.Add(Colon != INDEX_NONE ? Field.Key.RightChop(Colon + 1) : Field.Key);
				}
			}
			Last = Now;
		}
	};

	/** The Objectives list of AC-OC-12 (owning class or struct names). */
	inline bool IsObjectivesField(const FString& Field)
	{
		return Field.StartsWith(TEXT("ObjectiveTeamViewComponent.")) || Field.Contains(TEXT("ObjectiveTeamView")) || Field.Contains(TEXT("Objective"))
			|| Field.Contains(TEXT("Court")) || Field.Contains(TEXT("Site"));
	}

	/** Shared BEFORE_ALL / AFTER_ALL settings of the Objectives PIE tests. */
	struct FScopedPieSettings
	{
		FString PreviousTimeout;
		int32 PreviousNpcCount = -1;
		bool bPreviousAutoStart = true;

		void Apply(bool bAutoStart = true)
		{
			if (IConsoleVariable* Timeout = IConsoleManager::Get().FindConsoleVariable(CQTestConsoleVariables::NetworkTimeoutName))
			{
				PreviousTimeout = Timeout->GetString();
				Timeout->Set(NetworkTimeoutCeilingSeconds, ECVF_SetByCode);
			}
			UStealthSettings* Settings = GetMutableDefault<UStealthSettings>();
			PreviousNpcCount = Settings->NpcCount;
			Settings->NpcCount = 0;
			bPreviousAutoStart = Settings->bAutoStartRound;
			Settings->bAutoStartRound = bAutoStart;
		}

		void Restore()
		{
			if (IConsoleVariable* Timeout = IConsoleManager::Get().FindConsoleVariable(CQTestConsoleVariables::NetworkTimeoutName); Timeout && !PreviousTimeout.IsEmpty())
			{
				Timeout->Set(*PreviousTimeout, ECVF_SetByCode);
			}
			UStealthSettings* Settings = GetMutableDefault<UStealthSettings>();
			if (PreviousNpcCount >= 0)
			{
				Settings->NpcCount = PreviousNpcCount;
			}
			Settings->bAutoStartRound = bPreviousAutoStart;
		}
	};
}

#endif
