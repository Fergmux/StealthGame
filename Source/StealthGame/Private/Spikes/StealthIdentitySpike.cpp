// THROWAWAY SPIKE CODE (S1 / S2 / S14 for ADR-0001, 2026-10-25). See StealthIdentitySpike.h.
#include "Spikes/StealthIdentitySpike.h"

#if !UE_BUILD_SHIPPING

#include "StealthCharacter.h"
#include "StealthNpcController.h"
#include "StealthPlayerState.h"
#include "StealthSettings.h"

#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "Engine/PackageMapClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "Net/UnrealNetwork.h"
#include "UObject/Package.h"

DEFINE_LOG_CATEGORY_STATIC(LogStealthSpike, Log, All);

namespace StealthSpike
{
	static TAutoConsoleVariable<int32> CVarIdentityTrims(
		TEXT("stealth.Spike.IdentityTrims"),
		0,
		TEXT("SPIKE (ADR-0001 S1/S2/S14). Bitmask read when a play session builds replication layouts; set before PIE.\n")
		TEXT(" 1 = pawn PlayerState/Controller/Owner COND_OwnerOnly (ADR-0001 4.1)\n")
		TEXT(" 2 = PlayerState engine-default trims + PlayerState Owner OwnerOnly + ping off (4.3)\n")
		TEXT(" 4 = prototype PlayerState Team/LifeState OwnerOnly (strict 4.3 allowlist)\n")
		TEXT(" 8 = 8 fallback: pawn PlayerState/Controller/Owner disabled (overrides 1)\n")
		TEXT("16 = negative control: with 1, pawn PlayerState back to COND_None (IVP story-001)"),
		ECVF_Default);

	int32 GetIdentityTrims()
	{
		return CVarIdentityTrims.GetValueOnGameThread();
	}

	void ApplyPawnTrims(const UClass* ThisClass, TArray<FLifetimeProperty>& OutLifetimeProps)
	{
		const int32 Trims = GetIdentityTrims();
		if (Trims & PawnLinkDisabled)
		{
			DisableReplicatedLifetimeProperty(ThisClass, APawn::StaticClass(), FName(TEXT("PlayerState")), OutLifetimeProps);
			DisableReplicatedLifetimeProperty(ThisClass, APawn::StaticClass(), FName(TEXT("Controller")), OutLifetimeProps);
			DisableReplicatedLifetimeProperty(ThisClass, AActor::StaticClass(), FName(TEXT("Owner")), OutLifetimeProps);
		}
		else if (Trims & PawnLinkOwnerOnly)
		{
			// Condition-only overload: keeps Owner's push-based flag (ADR-0001 E13).
			ResetReplicatedLifetimeProperty(ThisClass, APawn::StaticClass(), FName(TEXT("PlayerState")), COND_OwnerOnly, OutLifetimeProps);
			ResetReplicatedLifetimeProperty(ThisClass, APawn::StaticClass(), FName(TEXT("Controller")), COND_OwnerOnly, OutLifetimeProps);
			ResetReplicatedLifetimeProperty(ThisClass, AActor::StaticClass(), FName(TEXT("Owner")), COND_OwnerOnly, OutLifetimeProps);
			if (Trims & PawnPlayerStateNegativeControl)
			{
				// Negative control (IVP story-001): the capture must detect PlayerState reaching non-owners.
				ResetReplicatedLifetimeProperty(ThisClass, APawn::StaticClass(), FName(TEXT("PlayerState")), COND_None, OutLifetimeProps);
			}
		}
		UE_LOG(LogStealthSpike, Log, TEXT("[Spike] ApplyPawnTrims(%s) trims=%d"), *GetNameSafe(ThisClass), Trims);
	}

	void ApplyPlayerStateTrims(const UClass* ThisClass, TArray<FLifetimeProperty>& OutLifetimeProps)
	{
		const int32 Trims = GetIdentityTrims();
		if (Trims & PlayerStateEngineTrims)
		{
			const UClass* PS = APlayerState::StaticClass();
			ResetReplicatedLifetimeProperty(ThisClass, AActor::StaticClass(), FName(TEXT("Owner")), COND_OwnerOnly, OutLifetimeProps);
			for (const TCHAR* Disabled : { TEXT("Score"), TEXT("StartTime"), TEXT("bIsABot"), TEXT("bIsInactive"), TEXT("bIsSpectator"),
				TEXT("bOnlySpectator"), TEXT("bFromPreviousLevel"), TEXT("CompressedPing") })
			{
				DisableReplicatedLifetimeProperty(ThisClass, PS, FName(Disabled), OutLifetimeProps);
			}
			ResetReplicatedLifetimeProperty(ThisClass, PS, FName(TEXT("PlayerId")), COND_OwnerOnly, OutLifetimeProps);
			ResetReplicatedLifetimeProperty(ThisClass, PS, FName(TEXT("UniqueId")), COND_OwnerOnly, OutLifetimeProps);
		}
		if (Trims & PlayerStatePrototypeFieldsOwnerOnly)
		{
			ResetReplicatedLifetimeProperty(ThisClass, AStealthPlayerState::StaticClass(), FName(TEXT("Team")), COND_OwnerOnly, OutLifetimeProps);
			ResetReplicatedLifetimeProperty(ThisClass, AStealthPlayerState::StaticClass(), FName(TEXT("LifeState")), COND_OwnerOnly, OutLifetimeProps);
		}
		UE_LOG(LogStealthSpike, Log, TEXT("[Spike] ApplyPlayerStateTrims(%s) trims=%d"), *GetNameSafe(ThisClass), Trims);
	}

	// ---- Probe -------------------------------------------------------------------------------

	static const TCHAR* ConditionName(int32 Condition)
	{
		static const TCHAR* Names[] = { TEXT("None"), TEXT("InitialOnly"), TEXT("OwnerOnly"), TEXT("SkipOwner"), TEXT("SimulatedOnly"),
			TEXT("AutonomousOnly"), TEXT("SimulatedOrPhysics"), TEXT("InitialOrOwner"), TEXT("Custom"), TEXT("ReplayOrOwner"), TEXT("ReplayOnly"),
			TEXT("SimulatedOnlyNoReplay"), TEXT("SimulatedOrPhysicsNoReplay"), TEXT("SkipReplay"), TEXT("Dynamic"), TEXT("Never"), TEXT("NetGroup") };
		return (Condition >= 0 && Condition < UE_ARRAY_COUNT(Names)) ? Names[Condition] : TEXT("?");
	}

	static int32 PieInstance(const UWorld* World)
	{
		return World && World->GetOutermost() ? World->GetOutermost()->GetPIEInstanceID() : -1;
	}

	static FString NetModeName(const UWorld* World)
	{
		switch (World ? World->GetNetMode() : NM_MAX)
		{
		case NM_Standalone: return TEXT("Standalone");
		case NM_ListenServer: return TEXT("ListenServer");
		case NM_DedicatedServer: return TEXT("DedicatedServer");
		case NM_Client: return TEXT("Client");
		default: return TEXT("?");
		}
	}

	static FString Guid(const UWorld* World, const UObject* Object)
	{
		if (!Object)
		{
			return TEXT("null");
		}
		const UNetDriver* Driver = World ? World->GetNetDriver() : nullptr;
		if (!Driver || Driver->IsUsingIrisReplication() || !Driver->GuidCache.IsValid())
		{
			return TEXT("noguid");
		}
		return Driver->GuidCache->GetNetGUID(Object).ToString();
	}

	static FString Desc(const UWorld* World, const UObject* Object)
	{
		return Object ? FString::Printf(TEXT("%s{%s}"), *Object->GetName(), *Guid(World, Object)) : FString(TEXT("null"));
	}

	static void DumpWorld(UWorld* World, const FString& Tag)
	{
		if (!World)
		{
			return;
		}
		const UNetDriver* Driver = World->GetNetDriver();
		const APlayerController* LocalPC = World->GetFirstPlayerController();
		UE_LOG(LogStealthSpike, Display, TEXT("[SpikeDump] ==== tag=%s pie=%d mode=%s driver=%s iris=%d trims=%d localPC=%s time=%.2f"),
			*Tag, PieInstance(World), *NetModeName(World), Driver ? *Driver->GetClass()->GetName() : TEXT("none"),
			Driver ? (int32)Driver->IsUsingIrisReplication() : -1, GetIdentityTrims(), *Desc(World, LocalPC), World->GetTimeSeconds());

		if (const AGameStateBase* GS = World->GetGameState())
		{
			UE_LOG(LogStealthSpike, Display, TEXT("[SpikeDump] pie=%d PlayerArray.Num=%d"), PieInstance(World), GS->PlayerArray.Num());
			for (const APlayerState* PS : GS->PlayerArray)
			{
				if (!PS)
				{
					continue;
				}
				const AStealthPlayerState* SPS = Cast<AStealthPlayerState>(PS);
				UE_LOG(LogStealthSpike, Display,
					TEXT("[SpikeDump] pie=%d PS %s name='%s' PlayerId=%d ping=%d StartTime=%d Score=%.1f bot=%d inactive=%d spectator=%d uniqueIdValid=%d team=%d life=%d owner=%s pawn=%s"),
					PieInstance(World), *Desc(World, PS), *PS->GetPlayerName(), PS->GetPlayerId(), (int32)PS->GetCompressedPing(), PS->GetStartTime(), PS->GetScore(),
					(int32)PS->IsABot(), (int32)PS->IsInactive(), (int32)PS->IsSpectator(), (int32)PS->GetUniqueId().IsValid(),
					SPS ? (int32)SPS->Team : -1, SPS ? (int32)SPS->LifeState : -1, *Desc(World, PS->GetOwner()), *Desc(World, PS->GetPawn()));
			}
		}

		int32 Total = 0, RemoteWithLink = 0, RemoteWithPS = 0, RemoteWithCtrl = 0, RemoteWithOwner = 0;
		for (TActorIterator<AStealthCharacter> It(World); It; ++It)
		{
			AStealthCharacter* Pawn = *It;
			++Total;
			const bool bLocal = Pawn->IsLocallyControlled();
			const APlayerState* PS = Pawn->GetPlayerState();
			const AController* Ctrl = Pawn->GetController();
			const AActor* Owner = Pawn->GetOwner();
			if (!bLocal)
			{
				RemoteWithPS += PS ? 1 : 0;
				RemoteWithCtrl += Ctrl ? 1 : 0;
				RemoteWithOwner += Owner ? 1 : 0;
				RemoteWithLink += (PS || Ctrl || Owner) ? 1 : 0;
			}
			UE_LOG(LogStealthSpike, Display,
				TEXT("[SpikeDump] pie=%d PAWN %s local=%d role=%d remoteRole=%d PlayerState=%s Controller=%s Owner=%s IsPlayerControlled=%d IsNpc()=%d dying=%d loc=%s"),
				PieInstance(World), *Desc(World, Pawn), (int32)bLocal, (int32)Pawn->GetLocalRole(), (int32)Pawn->GetRemoteRole(),
				*Desc(World, PS), *Desc(World, Ctrl), *Desc(World, Owner), (int32)Pawn->IsPlayerControlled(), (int32)Pawn->IsNpc(), (int32)Pawn->IsDying(),
				*Pawn->GetActorLocation().ToCompactString());
		}
		UE_LOG(LogStealthSpike, Display,
			TEXT("[SpikeDump] pie=%d SUMMARY pawns=%d nonLocalWithAnyLink=%d (PlayerState=%d Controller=%d Owner=%d)"),
			PieInstance(World), Total, RemoteWithLink, RemoteWithPS, RemoteWithCtrl, RemoteWithOwner);
	}

	static void DumpRepMap(const UClass* Class)
	{
		if (!Class)
		{
			return;
		}
		UClass* MutableClass = const_cast<UClass*>(Class);
		MutableClass->SetUpRuntimeReplicationData();
		TArray<FLifetimeProperty> Props;
		Class->GetDefaultObject()->GetLifetimeReplicatedProps(Props);
		TMap<int32, int32> ConditionByIndex;
		for (const FLifetimeProperty& Prop : Props)
		{
			ConditionByIndex.Add(Prop.RepIndex, (int32)Prop.Condition);
		}
		UE_LOG(LogStealthSpike, Display, TEXT("[SpikeRepMap] class=%s ClassReps=%d trims=%d"), *Class->GetPathName(), Class->ClassReps.Num(), GetIdentityTrims());
		for (int32 Index = 0; Index < Class->ClassReps.Num(); ++Index)
		{
			const FRepRecord& Record = Class->ClassReps[Index];
			const int32* Condition = ConditionByIndex.Find(Index);
			UE_LOG(LogStealthSpike, Display, TEXT("[SpikeRepMap] class=%s parent=%d prop=%s.%s[%d] cond=%s"),
				*Class->GetName(), Index, *GetNameSafe(Record.Property ? Record.Property->GetOwnerStruct() : nullptr),
				Record.Property ? *Record.Property->GetName() : TEXT("?"), Record.Index, Condition ? ConditionName(*Condition) : TEXT("(unregistered)"));
		}
	}

	static void TakeOver(UWorld* World, int32 PlayerIndex)
	{
		if (!World || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogStealthSpike, Warning, TEXT("[SpikeTakeOver] run on the server world (pie=%d)."), PieInstance(World));
			return;
		}
		APlayerController* PC = nullptr;
		int32 Index = 0;
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It, ++Index)
		{
			if (Index == PlayerIndex)
			{
				PC = It->Get();
				break;
			}
		}
		APawn* Old = PC ? PC->GetPawn() : nullptr;
		if (!PC || !Old)
		{
			UE_LOG(LogStealthSpike, Warning, TEXT("[SpikeTakeOver] player %d has no controller/pawn."), PlayerIndex);
			return;
		}
		AStealthCharacter* Target = nullptr;
		float Best = TNumericLimits<float>::Max();
		for (TActorIterator<AStealthCharacter> It(World); It; ++It)
		{
			if (*It != Old && !It->IsDying() && Cast<AStealthNpcController>(It->GetController()))
			{
				const float Distance = FVector::Dist(It->GetActorLocation(), Old->GetActorLocation());
				if (Distance < Best)
				{
					Best = Distance;
					Target = *It;
				}
			}
		}
		if (!Target)
		{
			UE_LOG(LogStealthSpike, Warning, TEXT("[SpikeTakeOver] no NPC pawn to take over."));
			return;
		}
		AController* NpcController = Target->GetController();
		NpcController->UnPossess();
		PC->UnPossess();
		PC->Possess(Target);
		NpcController->Possess(Old); // AI hand-over of the player's former body
		UE_LOG(LogStealthSpike, Display, TEXT("[SpikeTakeOver] player %d (%s) now possesses %s; former body %s handed to %s"),
			PlayerIndex, *Desc(World, PC), *Desc(World, Target), *Desc(World, Old), *Desc(World, NpcController));
	}

	static void AutoMove(UWorld* World, float Seconds)
	{
		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		if (!Pawn)
		{
			UE_LOG(LogStealthSpike, Warning, TEXT("[SpikeAutoMove] pie=%d no local pawn."), PieInstance(World));
			return;
		}
		UE_LOG(LogStealthSpike, Display, TEXT("[SpikeAutoMove] pie=%d start pawn=%s locallyControlled=%d PawnController=%s loc=%s"),
			PieInstance(World), *Desc(World, Pawn), (int32)Pawn->IsLocallyControlled(), *Desc(World, Pawn->GetController()), *Pawn->GetActorLocation().ToCompactString());
		const double EndTime = FPlatformTime::Seconds() + Seconds;
		TWeakObjectPtr<APawn> WeakPawn(Pawn);
		const int32 Instance = PieInstance(World);
		const FVector Direction = Pawn->GetActorForwardVector();
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakPawn, EndTime, Instance, Direction](float)
		{
			APawn* P = WeakPawn.Get();
			if (!P)
			{
				return false;
			}
			if (FPlatformTime::Seconds() >= EndTime)
			{
				UE_LOG(LogStealthSpike, Display, TEXT("[SpikeAutoMove] pie=%d end pawn=%s loc=%s"), Instance, *P->GetName(), *P->GetActorLocation().ToCompactString());
				return false;
			}
			P->AddMovementInput(Direction, 1.f);
			return true;
		}));
	}

	static void OnWorldInit(UWorld* World, const UWorld::InitializationValues)
	{
		if (!World || !World->IsGameWorld())
		{
			return;
		}
		World->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateLambda([](AActor* Actor)
		{
			if (APlayerState* PS = Cast<APlayerState>(Actor))
			{
				UWorld* W = PS->GetWorld();
				// ADR-0001 4.3 also asks for SetShouldUpdateReplicatedPing(false): it is PROTECTED on APlayerState in 5.8.4
				// (PlayerState.h:298), so only AStealthPlayerState itself can call it. Not done here; CompressedPing is
				// disabled by trim bit 2, so the wire result is the same (only the server-side ping bookkeeping remains).
				UE_LOG(LogStealthSpike, Display, TEXT("[SpikePS] pie=%d SPAWNED %s time=%.2f"), PieInstance(W), *PS->GetName(), W ? W->GetTimeSeconds() : 0.f);
			}
		}));
		World->AddOnActorDestroyedHandler(FOnActorDestroyed::FDelegate::CreateLambda([](AActor* Actor)
		{
			if (APlayerState* PS = Cast<APlayerState>(Actor))
			{
				UWorld* W = PS->GetWorld();
				UE_LOG(LogStealthSpike, Display, TEXT("[SpikePS] pie=%d DESTROYED %s time=%.2f"), PieInstance(W), *PS->GetName(), W ? W->GetTimeSeconds() : 0.f);
			}
		}));
	}

	static FDelegateHandle GWorldInitHandle = FWorldDelegates::OnPostWorldInitialization.AddStatic(&OnWorldInit);

	static FAutoConsoleCommandWithWorldAndArgs CmdDump(TEXT("stealth.Spike.Dump"),
		TEXT("SPIKE: log this world's view of every AStealthCharacter link (PlayerState/Controller/Owner) and the PlayerArray. Arg: tag."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			DumpWorld(World, Args.Num() > 0 ? Args[0] : FString(TEXT("-")));
		}));

	static FAutoConsoleCommandWithWorldAndArgs CmdDumpAll(TEXT("stealth.Spike.DumpAll"),
		TEXT("SPIKE: stealth.Spike.Dump for every PIE/game world in this process. Arg: tag."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld*)
		{
			if (!GEngine)
			{
				return;
			}
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				if (Context.World() && Context.World()->IsGameWorld())
				{
					DumpWorld(Context.World(), Args.Num() > 0 ? Args[0] : FString(TEXT("-")));
				}
			}
		}));

	static FAutoConsoleCommandWithWorldAndArgs CmdRepMap(TEXT("stealth.Spike.RepMap"),
		TEXT("SPIKE: log ClassReps index->property and the CDO lifetime conditions for the pawn and PlayerState classes."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			DumpRepMap(UStealthSettings::Get()->CharacterClass.LoadSynchronous());
			DumpRepMap(AStealthCharacter::StaticClass());
			const AGameModeBase* GameMode = World ? World->GetAuthGameMode() : nullptr;
			DumpRepMap(GameMode && GameMode->PlayerStateClass ? GameMode->PlayerStateClass.Get() : AStealthPlayerState::StaticClass());
		}));

	static FAutoConsoleCommandWithWorldAndArgs CmdTakeOver(TEXT("stealth.Spike.TakeOver"),
		TEXT("SPIKE (server world): player N possesses the nearest crowd pawn; its former body is handed to that NPC's AI controller."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			TakeOver(World, Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 0);
		}));

	static FAutoConsoleCommandWithWorldAndArgs CmdAutoMove(TEXT("stealth.Spike.AutoMove"),
		TEXT("SPIKE (owning client world): feed forward movement input to the local pawn for N seconds (default 2)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			AutoMove(World, Args.Num() > 0 ? FCString::Atof(*Args[0]) : 2.f);
		}));
}

#endif // !UE_BUILD_SHIPPING
