// SPIKE (measurement, not a gate): SCL story 045, net-emulation and traversal spike
// (production/epics/shared-character-locomotion/story-045-net-emulation-traversal-spike.md; ADR-0010
// Verification Required, ST1 and E12; GDD OQ2/OQ3). Report: production/qa/spikes/2026-10-25_SCL045-net-emulation-traversal.md.
//
// A CQTest in-process PIE session (listen host + 3 clients, AStealthGameMode, L_Prototype) with the engine
// packet-simulation profile StealthLeak1 (PktLag 100 ms, PktLoss 1 %) loaded by name and applied to every
// net driver (so 100 ms each way). Two methods:
//   Starvation: a moving client's outgoing packets are dropped (PktLoss 100 on its driver) and the server
//               position of its pawn is sampled every frame: forced-update cadence (Hz) with the pinned
//               MAXCLIENTUPDATEINTERVAL 0.25 s and with the interval set to one frame (the per-frame alternative).
//   Traversal:  the sample's traversal course in L_Prototype (LevelBlock_Traversable actors, whose ledge
//               splines are authored per placed instance, so blocks cannot be spawned at run time) is surveyed;
//               for each kind (vault: thin wall nearest 0.8 m; hurdle: thin wall nearest 1.2 m; climb: deep block
//               nearest 1.6 m) the clients run repeated traversals through the sample's own TryTraversalAction
//               (client-initiated, as with the Space key); end positions on the server, the owner and every
//               observing proxy are recorded, with the movement mode and the proxies' RepRootMotion state.
// Records: Saved/Harness/<RunId>/scl045-<method>.jsonl, analysed by tools/ci/mp/analyze_scl045.py.
//
// Named StealthSpike.* (not StealthGame.*) on purpose: it is a several-minute measurement, never part of the
// per-commit `Automation RunTests StealthGame.` gate (ADR-0008 section 8). Run it explicitly:
//   UnrealEditor-Cmd <uproject> -ExecCmds="Automation RunTests StealthSpike.Locomotion; Quit" -unattended -nullrhi
// It asserts only that the session ran (worlds, pawns, course); every measurement is recorded, not judged.

#include "CoreMinimal.h"
#include "Tests/PIE/StealthPIENetworkComponent.h"

#if WITH_DEV_AUTOMATION_TESTS && defined(ENABLE_PIE_NETWORK_TEST) && ENABLE_PIE_NETWORK_TEST

#include "CQTest.h"
#include "Components/CapsuleComponent.h"
#include "Components/SplineComponent.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "Engine/NetConnection.h"
#include "Engine/NetDriver.h"
#include "Engine/PackageMapClient.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameNetworkManager.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "StealthGameMode.h"
#include "StealthSettings.h"
#include "UObject/UnrealType.h"

namespace SCL045
{
	const TCHAR* const FixtureMap = TEXT("/Game/StealthGame/Maps/L_Prototype");
	const TCHAR* const Profile = TEXT("StealthLeak1");
	const TCHAR* const TraversableClass = TEXT("/Game/LevelPrototyping/LevelBlock_Traversable.LevelBlock_Traversable_C");
	const TCHAR* const CeilingSeconds = TEXT("2400");
	constexpr int32 ClientCount = 3;
	constexpr double ApproachDistance = 220.0;
	constexpr double LaneWidth = 140.0;
	constexpr int32 TrialsPerKind = 102; // >= 100 per kind, a multiple of the three lanes

	struct FObstacle
	{
		const TCHAR* Kind;
		double Height;    // kit height under test (AC: vault 0.8 m, climb 1.6 m, optional 1.2 m hurdle)
		double MinHeight; // survey window
		double MaxHeight;
		bool bThin;       // depth <= 60 cm (a wall) or >= 100 cm (a block)
	};
	const FObstacle Obstacles[] = {
		{ TEXT("vault"), 80.0, 75.0, 85.0, true },
		{ TEXT("hurdle"), 120.0, 115.0, 125.0, true },
		{ TEXT("climb"), 160.0, 155.0, 165.0, false },
	};
	constexpr int32 HurdleTrials = 30; // optional height: a smaller sample

	/** JSON-lines record writer (one file per method). */
	class FWriter
	{
	public:
		void Open(const FString& Method)
		{
			FString RunId;
			if (!FParse::Value(FCommandLine::Get(), TEXT("-StealthHarnessRunId="), RunId))
			{
				RunId = FDateTime::Now().ToString(TEXT("scl045-%Y%m%d-%H%M%S"));
			}
			const FString Dir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Harness"), RunId);
			IFileManager::Get().MakeDirectory(*Dir, true);
			File.Reset(IFileManager::Get().CreateFileWriter(*FPaths::Combine(Dir, FString::Printf(TEXT("scl045-%s.jsonl"), *Method)), FILEWRITE_AllowRead));
		}
		void Write(const TCHAR* Type, const FString& Fields)
		{
			if (!File.IsValid())
			{
				return;
			}
			const FString Line = FString::Printf(TEXT("{\"t\":%.6f,\"type\":\"%s\"%s%s}\n"), FPlatformTime::Seconds(), Type, Fields.IsEmpty() ? TEXT("") : TEXT(","), *Fields);
			const FTCHARToUTF8 Utf8(*Line);
			File->Serialize(const_cast<ANSICHAR*>(Utf8.Get()), Utf8.Length());
			File->Flush();
		}
		void Close()
		{
			if (File.IsValid())
			{
				File->Close();
				File.Reset();
			}
		}
	private:
		TUniquePtr<FArchive> File;
	};

	static FString V(const FVector& In)
	{
		return FString::Printf(TEXT("[%.2f,%.2f,%.2f]"), In.X, In.Y, In.Z);
	}

	static FString Q(const FString& S)
	{
		return TEXT("\"") + S.Replace(TEXT("\\"), TEXT("\\\\")).Replace(TEXT("\""), TEXT("\\\"")) + TEXT("\"");
	}

	/** Server world and client worlds (by PIE instance) of the running session. */
	struct FWorlds
	{
		UWorld* Server = nullptr;
		TArray<UWorld*> Clients;
	};

	static FWorlds FindWorlds()
	{
		FWorlds Out;
		TArray<TPair<int32, UWorld*>> Clients;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* World = Context.World();
			if (Context.WorldType != EWorldType::PIE || World == nullptr)
			{
				continue;
			}
			if (World->GetNetMode() == NM_ListenServer)
			{
				Out.Server = World;
			}
			else if (World->GetNetMode() == NM_Client)
			{
				Clients.Emplace(Context.PIEInstance, World);
			}
		}
		Clients.Sort([](const TPair<int32, UWorld*>& A, const TPair<int32, UWorld*>& B) { return A.Key < B.Key; });
		for (const TPair<int32, UWorld*>& Pair : Clients)
		{
			Out.Clients.Add(Pair.Value);
		}
		return Out;
	}

	static FNetworkGUID GuidOf(const UWorld* World, const UObject* Object)
	{
		const UNetDriver* Driver = World ? World->GetNetDriver() : nullptr;
		return Driver && Driver->GuidCache.IsValid() && Object ? Driver->GuidCache->GetNetGUID(Object) : FNetworkGUID();
	}

	static ACharacter* FindByGuid(const UWorld* World, const FNetworkGUID& Guid)
	{
		const UNetDriver* Driver = World ? World->GetNetDriver() : nullptr;
		return Driver && Driver->GuidCache.IsValid() && Guid.IsValid() ? Cast<ACharacter>(Driver->GuidCache->GetObjectFromNetGUID(Guid, false)) : nullptr;
	}

	static bool GetBool(const UObject* Object, const TCHAR* Name)
	{
		const FBoolProperty* Property = Object ? FindFProperty<FBoolProperty>(Object->GetClass(), Name) : nullptr;
		return Property && Property->GetPropertyValue_InContainer(Object);
	}

	static FString MoveMode(const ACharacter* Character)
	{
		return Character ? Character->GetCharacterMovement()->GetMovementName() : FString(TEXT("none"));
	}

	/** Reads a field of a Blueprint struct variable by the field's authored name prefix (BP fields carry a GUID suffix). */
	static const FProperty* FindStructField(const UStruct* Struct, const TCHAR* Prefix)
	{
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			if (It->GetName().StartsWith(Prefix))
			{
				return *It;
			}
		}
		return nullptr;
	}

	/** {action type display name, obstacle height, obstacle depth} from the pawn's TraversalResult variable. */
	static FString TraversalResultJson(const UObject* Pawn)
	{
		const FStructProperty* Result = Pawn ? FindFProperty<FStructProperty>(Pawn->GetClass(), TEXT("TraversalResult")) : nullptr;
		if (!Result)
		{
			return TEXT("null");
		}
		const void* Data = Result->ContainerPtrToValuePtr<void>(Pawn);
		FString Action = TEXT("?");
		if (const FProperty* ActionField = FindStructField(Result->Struct, TEXT("ActionType")))
		{
			const void* FieldData = ActionField->ContainerPtrToValuePtr<void>(Data);
			if (const FByteProperty* AsByte = CastField<FByteProperty>(ActionField); AsByte && AsByte->Enum)
			{
				Action = AsByte->Enum->GetDisplayNameTextByValue(AsByte->GetPropertyValue(FieldData)).ToString();
			}
			else if (const FEnumProperty* AsEnum = CastField<FEnumProperty>(ActionField))
			{
				Action = AsEnum->GetEnum()->GetDisplayNameTextByValue(AsEnum->GetUnderlyingProperty()->GetSignedIntPropertyValue(FieldData)).ToString();
			}
		}
		auto Real = [&](const TCHAR* Prefix) -> double
		{
			const FProperty* Field = FindStructField(Result->Struct, Prefix);
			if (const FDoubleProperty* AsDouble = CastField<FDoubleProperty>(Field))
			{
				return AsDouble->GetPropertyValue(AsDouble->ContainerPtrToValuePtr<void>(Data));
			}
			if (const FFloatProperty* AsFloat = CastField<FFloatProperty>(Field))
			{
				return AsFloat->GetPropertyValue(AsFloat->ContainerPtrToValuePtr<void>(Data));
			}
			return -1.0;
		};
		return FString::Printf(TEXT("{\"action\":%s,\"obstacleHeight\":%.2f,\"obstacleDepth\":%.2f}"), *Q(Action), Real(TEXT("ObstacleHeight")), Real(TEXT("ObstacleDepth")));
	}

	/** Allocates and initialises a parameter block for Function. */
	static uint8* AllocParams(const UFunction* Function)
	{
		uint8* Params = static_cast<uint8*>(FMemory::Malloc(FMath::Max<int32>(Function->ParmsSize, 1), 16));
		FMemory::Memzero(Params, Function->ParmsSize);
		for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			It->InitializeValue_InContainer(Params);
		}
		return Params;
	}

	static void FreeParams(const UFunction* Function, uint8* Params)
	{
		for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			It->DestroyValue_InContainer(Params);
		}
		FMemory::Free(Params);
	}

	/**
	 * Calls the sample's TryTraversalAction on Pawn (the client's own body), with the inputs from the sample's own
	 * GetTraversalCheckInputs, exactly as the IA_Jump / Space path does.
	 */
	static bool TryTraversal(ACharacter* Pawn, bool& bCheckFailed, bool& bMontageFailed, FString& Signature)
	{
		UFunction* Function = Pawn ? Pawn->FindFunction(TEXT("TryTraversalAction")) : nullptr;
		UFunction* GetInputs = Pawn ? Pawn->FindFunction(TEXT("GetTraversalCheckInputs")) : nullptr;
		if (!Function || !GetInputs)
		{
			return false;
		}
		uint8* InputParams = AllocParams(GetInputs);
		Pawn->ProcessEvent(GetInputs, InputParams);
		const FStructProperty* InputsOut = nullptr;
		for (TFieldIterator<FProperty> It(GetInputs); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			if (It->HasAnyPropertyFlags(CPF_OutParm | CPF_ReturnParm) && CastField<FStructProperty>(*It))
			{
				InputsOut = CastField<FStructProperty>(*It);
			}
		}
		uint8* Params = AllocParams(Function);
		for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			const FStructProperty* AsStruct = CastField<FStructProperty>(*It);
			if (InputsOut && AsStruct && !It->HasAnyPropertyFlags(CPF_OutParm) && AsStruct->Struct == InputsOut->Struct)
			{
				AsStruct->CopyCompleteValue_InContainer(Params, InputsOut->ContainerPtrToValuePtr<void>(InputParams));
			}
		}
		FreeParams(GetInputs, InputParams);
		Pawn->ProcessEvent(Function, Params);
		Signature.Reset();
		bCheckFailed = bMontageFailed = false;
		for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			Signature += It->GetName() + (It->HasAnyPropertyFlags(CPF_OutParm) ? TEXT("(out) ") : TEXT(" "));
			if (const FBoolProperty* AsBool = CastField<FBoolProperty>(*It))
			{
				const bool bValue = AsBool->GetPropertyValue_InContainer(Params);
				if (It->GetName().StartsWith(TEXT("TraversalCheckFailed")))
				{
					bCheckFailed = bValue;
				}
				else if (It->GetName().StartsWith(TEXT("MontageSelectionFailed")))
				{
					bMontageFailed = bValue;
				}
			}
		}
		FreeParams(Function, Params);
		return true;
	}

}

NETWORK_TEST_CLASS(NetEmulationTraversal, "StealthSpike.Locomotion.PIE")
{
	TStealthPIENetworkComponent<FBasePIENetworkComponentState> Network{ TestRunner, TestCommandBuilder };
	FPacketSimulationSettings PacketSettings;
	SCL045::FWriter Out;

	inline static FString PreviousTimeout;
	inline static int32 PreviousNpcCount = -1;
	inline static float PreviousForcedUpdateInterval = -1.f;

	BEFORE_ALL()
	{
		if (IConsoleVariable* Timeout = IConsoleManager::Get().FindConsoleVariable(CQTestConsoleVariables::NetworkTimeoutName))
		{
			PreviousTimeout = Timeout->GetString();
			Timeout->Set(SCL045::CeilingSeconds, ECVF_SetByCode);
		}
		// No crowd: the spike measures player bodies on its own course (crowd parity is out of scope).
		UStealthSettings* Settings = GetMutableDefault<UStealthSettings>();
		PreviousNpcCount = Settings->NpcCount;
		Settings->NpcCount = 0;
		PreviousForcedUpdateInterval = GetDefault<AGameNetworkManager>()->MAXCLIENTUPDATEINTERVAL;
	}

	AFTER_ALL()
	{
		if (IConsoleVariable* Timeout = IConsoleManager::Get().FindConsoleVariable(CQTestConsoleVariables::NetworkTimeoutName); Timeout && !PreviousTimeout.IsEmpty())
		{
			Timeout->Set(*PreviousTimeout, ECVF_SetByCode);
		}
		if (PreviousNpcCount >= 0)
		{
			GetMutableDefault<UStealthSettings>()->NpcCount = PreviousNpcCount;
		}
		if (PreviousForcedUpdateInterval >= 0.f)
		{
			GetMutableDefault<AGameNetworkManager>()->MAXCLIENTUPDATEINTERVAL = PreviousForcedUpdateInterval; // also restored if Starvation fails midway
		}
	}

	BEFORE_EACH()
	{
		PacketSettings = FPacketSimulationSettings();
		ASSERT_THAT(IsTrue(PacketSettings.LoadEmulationProfile(SCL045::Profile), TEXT("profile StealthLeak1 loads by name")));
		FNetworkComponentBuilder<FBasePIENetworkComponentState>()
			.AsListenServer()
			.WithClients(SCL045::ClientCount)
			.WithGameMode(AStealthGameMode::StaticClass())
			.WithPacketSimulationSettings(&PacketSettings)
			.Build(Network);
		Network.StartOnFixtureMap(SCL045::FixtureMap);
	}

	AFTER_EACH()
	{
		Out.Close();
	}

	// ---- shared ------------------------------------------------------------------------------

	/** Starts a round when needed; true once every client controls a pawn on the server and in its own world. */
	bool AwaitPawns(const TCHAR* Method)
	{
		const SCL045::FWorlds Worlds = SCL045::FindWorlds();
		if (!Worlds.Server || Worlds.Clients.Num() != SCL045::ClientCount)
		{
			return false;
		}
		AStealthGameMode* GameMode = Worlds.Server->GetAuthGameMode<AStealthGameMode>();
		if (GameMode && !GameMode->IsRoundInProgress() && !bRoundRequested)
		{
			GameMode->StartRound();
			bRoundRequested = true;
		}
		if (!GameMode || !GameMode->IsRoundInProgress())
		{
			return false;
		}
		if (RoundSeenAt < 0.0)
		{
			RoundSeenAt = Worlds.Server->GetTimeSeconds();
		}
		if (Worlds.Server->GetTimeSeconds() - RoundSeenAt < 2.0)
		{
			return false; // let the respawned bodies replicate
		}
		for (UWorld* Client : Worlds.Clients)
		{
			const APlayerController* PC = Client->GetFirstPlayerController();
			const ACharacter* Own = PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr;
			if (!Own || !SCL045::GuidOf(Client, Own).IsValid() || !SCL045::FindByGuid(Worlds.Server, SCL045::GuidOf(Client, Own)))
			{
				return false;
			}
		}
		if (!bHeaderWritten)
		{
			Out.Open(Method);
			const UNetDriver* Driver = Worlds.Server->GetNetDriver();
			const AGameNetworkManager* Gnm = GetDefault<AGameNetworkManager>();
			Out.Write(TEXT("session"), FString::Printf(
				TEXT("\"profile\":%s,\"PktLag\":%d,\"PktLoss\":%d,\"PktLagVariance\":%d,\"appliedTo\":\"server and every client driver (CQTest SetPacketSettings)\",\"iris\":%d,")
				TEXT("\"MAXCLIENTUPDATEINTERVAL\":%.4f,\"MaxClientForcedUpdateDuration\":%.4f,\"MaxMoveDeltaTime\":%.4f,\"clients\":%d"),
				*SCL045::Q(SCL045::Profile), PacketSettings.PktLag, PacketSettings.PktLoss, PacketSettings.PktLagVariance, (int32)Driver->IsUsingIrisReplication(),
				Gnm->MAXCLIENTUPDATEINTERVAL, Gnm->MaxClientForcedUpdateDuration, Gnm->MaxMoveDeltaTime, Worlds.Clients.Num()));
			bHeaderWritten = true;
		}
		return true;
	}

	bool bRoundRequested = false;
	double RoundSeenAt = -1.0;
	bool bHeaderWritten = false;

	// ---- ST1 starvation ------------------------------------------------------------------------

	struct FStarveState
	{
		int32 Phase = 0;        // 0 setup, 1 accelerate, 2 starved, 3 recover, 4 next config, 5 done
		int32 Config = 0;       // 0: pinned MAXCLIENTUPDATEINTERVAL, 1: one frame
		int32 Repeat = 0;
		double PhaseStart = 0.0;
		FVector LastServerLocation = FVector::ZeroVector;
		float SavedInterval = 0.25f;
	} Starve;

	static constexpr int32 StarveRepeats = 5;
	static constexpr double AccelerateSeconds = 1.5;
	static constexpr double StarveSeconds = 2.5;
	static constexpr double RecoverSeconds = 1.5;

	bool StepStarvation()
	{
		const SCL045::FWorlds Worlds = SCL045::FindWorlds();
		UWorld* Client = Worlds.Clients.Num() > 0 ? Worlds.Clients[0] : nullptr;
		if (!Worlds.Server || !Client)
		{
			return false;
		}
		APlayerController* ClientPC = Client->GetFirstPlayerController();
		ACharacter* Own = ClientPC ? Cast<ACharacter>(ClientPC->GetPawn()) : nullptr;
		ACharacter* OnServer = SCL045::FindByGuid(Worlds.Server, SCL045::GuidOf(Client, Own));
		if (!Own || !OnServer)
		{
			return false;
		}
		AGameNetworkManager* Gnm = GetMutableDefault<AGameNetworkManager>();
		const double Now = Worlds.Server->GetTimeSeconds();
		const double InPhase = Now - Starve.PhaseStart;
		auto SetClientLoss = [Client, this](int32 Loss)
		{
			FPacketSimulationSettings Settings = PacketSettings;
			Settings.PktLoss = Loss;
			Client->GetNetDriver()->SetPacketSimulationSettings(Settings);
		};
		switch (Starve.Phase)
		{
		case 0:
			Starve.SavedInterval = Gnm->MAXCLIENTUPDATEINTERVAL;
			Starve.Phase = 1;
			Starve.PhaseStart = Now;
			return false;
		case 1: // the owner drives forward (a real client move stream)
			Own->AddMovementInput(Own->GetActorForwardVector(), 1.f);
			if (InPhase >= AccelerateSeconds)
			{
				SetClientLoss(100);
				Starve.LastServerLocation = OnServer->GetActorLocation();
				Out.Write(TEXT("starveBegin"), FString::Printf(TEXT("\"config\":%d,\"repeat\":%d,\"interval\":%.4f,\"st\":%.6f,\"serverVel\":%.2f,\"serverAccel\":%.2f"),
					Starve.Config, Starve.Repeat, Gnm->MAXCLIENTUPDATEINTERVAL, Now, OnServer->GetVelocity().Size(), OnServer->GetCharacterMovement()->GetCurrentAcceleration().Size()));
				Starve.Phase = 2;
				Starve.PhaseStart = Now;
			}
			return false;
		case 2: // starved: sample every server frame
		{
			Own->AddMovementInput(Own->GetActorForwardVector(), 1.f); // the owner still "holds W"; nothing reaches the server
			const FVector Location = OnServer->GetActorLocation();
			const double Step = FVector::Dist(Location, Starve.LastServerLocation);
			if (Step > 0.01)
			{
				Out.Write(TEXT("serverStep"), FString::Printf(TEXT("\"config\":%d,\"repeat\":%d,\"sinceStarve\":%.6f,\"stepCm\":%.3f,\"serverVel\":%.2f,\"mode\":%s"),
					Starve.Config, Starve.Repeat, InPhase, Step, OnServer->GetVelocity().Size(), *SCL045::Q(SCL045::MoveMode(OnServer))));
			}
			Starve.LastServerLocation = Location;
			if (InPhase >= StarveSeconds)
			{
				SetClientLoss(PacketSettings.PktLoss);
				Out.Write(TEXT("starveEnd"), FString::Printf(TEXT("\"config\":%d,\"repeat\":%d,\"st\":%.6f,\"frameDelta\":%.6f"), Starve.Config, Starve.Repeat, Now, Worlds.Server->GetDeltaSeconds()));
				Starve.Phase = 3;
				Starve.PhaseStart = Now;
			}
			return false;
		}
		case 3:
			if (InPhase >= RecoverSeconds)
			{
				if (++Starve.Repeat >= StarveRepeats)
				{
					Starve.Repeat = 0;
					if (++Starve.Config >= 2)
					{
						Gnm->MAXCLIENTUPDATEINTERVAL = Starve.SavedInterval;
						Out.Write(TEXT("done"), TEXT(""));
						return true;
					}
					// Config 1: the per-frame alternative, as close as the engine allows (interval = one frame).
					Gnm->MAXCLIENTUPDATEINTERVAL = FMath::Max(Worlds.Server->GetDeltaSeconds(), 0.001f);
				}
				Starve.Phase = 1;
				Starve.PhaseStart = Now;
				// Turn round so repeated runs stay on the map.
				if (APlayerController* ServerPC = Cast<APlayerController>(OnServer->GetController()))
				{
					ServerPC->ClientSetRotation(FRotator(0.f, Own->GetActorRotation().Yaw + 180.f, 0.f), true);
				}
			}
			return false;
		default:
			return true;
		}
	}

	TEST_METHOD(Starvation)
	{
		Network
			.UntilServer(TEXT("Pawns ready"), [this](FBasePIENetworkComponentState&) { return AwaitPawns(TEXT("starvation")); })
			.UntilServer(TEXT("Starvation runs"), [this](FBasePIENetworkComponentState&) { return StepStarvation(); });
	}

	// ---- Traversal -----------------------------------------------------------------------------

	enum class ELanePhase : uint8 { Teleport, AwaitArrive, Approach, InAction, Settle, Done };

	struct FLane
	{
		int32 ClientIndex = 0;
		FNetworkGUID Guid;
		ELanePhase Phase = ELanePhase::Teleport;
		double PhaseStart = 0.0;
		FVector Start = FVector::ZeroVector;
		FVector Dir = FVector::ForwardVector;
		bool bActive = true;
		int32 Attempts = 0;
		bool bServerStarted = false, bServerEnded = false, bOwnerStarted = false, bOwnerEnded = false;
		double ServerStartT = 0.0, ServerEndT = 0.0, OwnerStartT = 0.0, OwnerEndT = 0.0;
		FVector ServerEnd, OwnerEnd;
		TMap<int32, FVector> ProxyEnd;
		TMap<int32, double> ProxyEndT;
		TSet<int32> ProxyStarted;
		TSet<int32> ProxyBpStarted;
		TSet<FString> ServerModes, OwnerModes, ProxyModes;
		int32 ProxyRepRootMotionFrames = 0, ProxyFrames = 0, ProxyNetworkedRootMotionFrames = 0;
		double MaxProxyDeviation = 0.0, MaxOwnerDeviation = 0.0;
		FString Result;
	};

	struct FCourseState
	{
		bool bBuilt = false;
		bool bKindReady = false;
		int32 KindIndex = 0;
		int32 Done = 0;
		int32 Trial = 0;
		TArray<FLane> Lanes;
	} Course;

	/** One traversable block of the map's course, measured on the server. */
	struct FBlock
	{
		TWeakObjectPtr<AActor> Actor;
		FVector Min, Max;
		double Height = 0.0, Depth = 0.0, Length = 0.0, FloorZ = 0.0;
		int32 ThinAxis = 0; // 0 = X, 1 = Y
	};
	TArray<FBlock> Survey;

	static bool TraceFloor(UWorld* World, const FVector& At, double& OutZ)
	{
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(SCL045Floor), false);
		FCollisionObjectQueryParams ObjectQuery;
		ObjectQuery.AddObjectTypesToQuery(ECC_WorldStatic);
		ObjectQuery.AddObjectTypesToQuery(ECC_WorldDynamic); // the course floor and blocks are level blocks (no pawns)
		if (World->LineTraceSingleByObjectType(Hit, At + FVector(0, 0, 400), At - FVector(0, 0, 1000), ObjectQuery, Params))
		{
			OutZ = Hit.ImpactPoint.Z;
			return true;
		}
		return false;
	}

	/**
	 * Stages the AC kit heights with the map's own traversable blocks (their ledge splines move with them):
	 * a block is lowered into the ground, or moved to a free spot of open floor, so its visible height is
	 * the kit height. Done identically in the server and every client world (level actors are not replicated).
	 */
	struct FStaging
	{
		const TCHAR* Block;
		double KitHeight;
		bool bRelocate; // move to a free open-floor spot (thin walls that float in the map)
	};

	void StageKitHeights(const SCL045::FWorlds& Worlds)
	{
		static const FStaging Stagings[] = {
			{ TEXT("LevelBlock_Traversable_C_6"), 80.0, false },
			{ TEXT("LevelBlock_Traversable_C_14"), 80.0, false },
			{ TEXT("LevelBlock_Traversable_C_7"), 80.0, false },
			{ TEXT("LevelBlock_Traversable_C_19"), 160.0, false },
			{ TEXT("LevelBlock_Traversable_C_17"), 120.0, true },
			{ TEXT("LevelBlock_Traversable_C_39"), 120.0, true },
			{ TEXT("LevelBlock_Traversable_C_26"), 120.0, true },
		};
		TArray<UWorld*> All = Worlds.Clients;
		All.Insert(Worlds.Server, 0);
		int32 Relocated = 0;
		for (const FStaging& Staging : Stagings)
		{
			FVector Delta = FVector::ZeroVector;
			bool bHaveDelta = false;
			for (UWorld* World : All)
			{
				AActor* Block = nullptr;
				for (TActorIterator<AActor> It(World); It; ++It)
				{
					if (It->GetName() == Staging.Block)
					{
						Block = *It;
						break;
					}
				}
				if (!Block)
				{
					Out.Write(TEXT("stagingMissing"), FString::Printf(TEXT("\"block\":%s"), *SCL045::Q(Staging.Block)));
					continue;
				}
				if (!bHaveDelta)
				{
					FVector Origin, Extent;
					Block->GetActorBounds(true, Origin, Extent);
					FVector Target = Origin;
					double FloorZ = Origin.Z - Extent.Z;
					if (Staging.bRelocate)
					{
						// Open floor south of the course: a row at Y = -2600, 400 cm apart, floor traced.
						Target = FVector(2600.0 + 500.0 * Relocated++, -2600.0, Origin.Z);
						TraceFloor(World, Target, FloorZ);
					}
					// In place: the course blocks rest on the floor, so the floor is the block's own bottom.
					// New top = floor + kit height.
					const double NewCentreZ = FloorZ + Staging.KitHeight - Extent.Z;
					Delta = FVector(Target.X - Origin.X, Target.Y - Origin.Y, NewCentreZ - Origin.Z);
					bHaveDelta = true;
					Out.Write(TEXT("staged"), FString::Printf(TEXT("\"block\":%s,\"kitHeight\":%.1f,\"delta\":%s,\"floorZ\":%.1f"),
						*SCL045::Q(Staging.Block), Staging.KitHeight, *SCL045::V(Delta), FloorZ));
				}
				TInlineComponentArray<USceneComponent*> Components(Block);
				for (USceneComponent* Component : Components)
				{
					Component->SetMobility(EComponentMobility::Movable);
				}
				Block->SetActorLocation(Block->GetActorLocation() + Delta, false, nullptr, ETeleportType::TeleportPhysics);
			}
		}
	}

	bool BuildCourse(const SCL045::FWorlds& Worlds)
	{
		StageKitHeights(Worlds);
		UClass* Traversable = LoadClass<AActor>(nullptr, SCL045::TraversableClass);
		if (!Traversable)
		{
			Out.Write(TEXT("error"), TEXT("\"what\":\"LevelBlock_Traversable class not found\""));
			return false;
		}
		for (TActorIterator<AActor> It(Worlds.Server, Traversable); It; ++It)
		{
			FBlock Block;
			Block.Actor = *It;
			FVector Origin, Extent;
			It->GetActorBounds(true, Origin, Extent);
			Block.Min = Origin - Extent;
			Block.Max = Origin + Extent;
			Block.ThinAxis = Extent.X <= Extent.Y ? 0 : 1;
			Block.Depth = 2.0 * (Block.ThinAxis == 0 ? Extent.X : Extent.Y);
			Block.Length = 2.0 * (Block.ThinAxis == 0 ? Extent.Y : Extent.X);
			// Floor beside the block (in front of its low-coordinate face).
			FVector Probe = Origin;
			Probe[Block.ThinAxis] = Block.Min[Block.ThinAxis] - 120.0;
			Probe.Z = Block.Max.Z;
			double FloorZ = Block.Min.Z;
			TraceFloor(Worlds.Server, Probe, FloorZ);
			Block.FloorZ = FloorZ;
			Block.Height = Block.Max.Z - FloorZ;
			Survey.Add(Block);
			TArray<USplineComponent*> Splines;
			It->GetComponents(Splines);
			Out.Write(TEXT("block"), FString::Printf(TEXT("\"name\":%s,\"min\":%s,\"max\":%s,\"yaw\":%.1f,\"height\":%.1f,\"depth\":%.1f,\"length\":%.1f,\"floorZ\":%.1f,\"ledgeSplines\":%d"),
				*SCL045::Q(It->GetName()), *SCL045::V(Block.Min), *SCL045::V(Block.Max), It->GetActorRotation().Yaw, Block.Height, Block.Depth, Block.Length, Block.FloorZ, Splines.Num()));
		}
		Course.Lanes.SetNum(SCL045::ClientCount);
		for (int32 Index = 0; Index < SCL045::ClientCount; ++Index)
		{
			UWorld* Client = Worlds.Clients[Index];
			Course.Lanes[Index].ClientIndex = Index;
			Course.Lanes[Index].Guid = SCL045::GuidOf(Client, Client->GetFirstPlayerController()->GetPawn());
		}
		Course.bBuilt = Survey.Num() > 0;
		return Course.bBuilt;
	}

	/** Places the lanes for the current kind: the block nearest the kit height, one lane per 140 cm of its length. */
	void PlaceLanesForKind()
	{
		const SCL045::FObstacle& Kind = SCL045::Obstacles[Course.KindIndex];
		TArray<const FBlock*> Candidates;
		for (const FBlock& Block : Survey)
		{
			const bool bThin = Block.Depth <= 60.0;
			const bool bDeep = Block.Depth >= 100.0;
			if (Block.Height >= Kind.MinHeight && Block.Height <= Kind.MaxHeight && (Kind.bThin ? bThin : bDeep))
			{
				Candidates.Add(&Block);
			}
		}
		Candidates.Sort([&Kind](const FBlock& A, const FBlock& B) { return FMath::Abs(A.Height - Kind.Height) < FMath::Abs(B.Height - Kind.Height); });
		int32 Lane = 0;
		for (const FBlock* Block : Candidates)
		{
			const int32 Slots = FMath::Clamp(FMath::FloorToInt32(Block->Length / SCL045::LaneWidth), 0, SCL045::ClientCount);
			for (int32 Slot = 0; Slot < Slots && Lane < SCL045::ClientCount; ++Slot, ++Lane)
			{
				FLane& L = Course.Lanes[Lane];
				const int32 Along = 1 - Block->ThinAxis;
				FVector Face = (Block->Min + Block->Max) * 0.5;
				Face[Along] = Block->Min[Along] + SCL045::LaneWidth * (Slot + 0.5) + (Block->Length - SCL045::LaneWidth * Slots) * 0.5;
				Face[Block->ThinAxis] = Block->Min[Block->ThinAxis];
				Face.Z = Block->FloorZ;
				L.Dir = FVector::ZeroVector;
				L.Dir[Block->ThinAxis] = 1.0;
				L.Start = Face - L.Dir * SCL045::ApproachDistance;
				L.bActive = true;
				Out.Write(TEXT("lane"), FString::Printf(TEXT("\"kind\":%s,\"lane\":%d,\"block\":%s,\"blockHeight\":%.1f,\"blockDepth\":%.1f,\"start\":%s,\"dir\":%s"),
					*SCL045::Q(Kind.Kind), Lane, *SCL045::Q(GetNameSafe(Block->Actor.Get())), Block->Height, Block->Depth, *SCL045::V(L.Start), *SCL045::V(L.Dir)));
			}
		}
		for (; Lane < SCL045::ClientCount; ++Lane)
		{
			Course.Lanes[Lane].bActive = false;
		}
		Course.bKindReady = true;
		if (Candidates.Num() == 0)
		{
			Out.Write(TEXT("noCandidate"), FString::Printf(TEXT("\"kind\":%s"), *SCL045::Q(Kind.Kind)));
		}
	}

	int32 TrialsForKind(int32 KindIndex) const
	{
		return FCString::Strcmp(SCL045::Obstacles[KindIndex].Kind, TEXT("hurdle")) == 0 ? SCL045::HurdleTrials : SCL045::TrialsPerKind;
	}

	void StepLane(const SCL045::FWorlds& Worlds, FLane& Lane)
	{
		UWorld* OwnerWorld = Worlds.Clients[Lane.ClientIndex];
		if (Lane.Phase == ELanePhase::Teleport)
		{
			const APlayerController* OwnerPC = OwnerWorld->GetFirstPlayerController();
			Lane.Guid = SCL045::GuidOf(OwnerWorld, OwnerPC ? OwnerPC->GetPawn() : nullptr);
		}
		ACharacter* OnServer = SCL045::FindByGuid(Worlds.Server, Lane.Guid);
		ACharacter* Own = SCL045::FindByGuid(OwnerWorld, Lane.Guid);
		if (!OnServer || !Own)
		{
			return;
		}
		const double Now = Worlds.Server->GetTimeSeconds();
		const double InPhase = Now - Lane.PhaseStart;
		if (!Lane.bActive)
		{
			Lane.Phase = ELanePhase::Done;
			return;
		}
		auto Enter = [&](ELanePhase Phase) { Lane.Phase = Phase; Lane.PhaseStart = Now; };

		switch (Lane.Phase)
		{
		case ELanePhase::Teleport:
		{
			const float HalfHeight = OnServer->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
			const FVector Spot = Lane.Start + FVector(0.0, 0.0, HalfHeight + 2.0);
			const FRotator Facing = Lane.Dir.Rotation();
			OnServer->GetCharacterMovement()->StopMovementImmediately();
			OnServer->TeleportTo(Spot, Facing, false, true);
			if (APlayerController* ServerPC = Cast<APlayerController>(OnServer->GetController()))
			{
				ServerPC->SetControlRotation(Facing);
				ServerPC->ClientSetRotation(Facing, true);
			}
			FLane Fresh;
			Fresh.ClientIndex = Lane.ClientIndex;
			Fresh.Guid = Lane.Guid;
			Fresh.Start = Lane.Start;
			Fresh.Dir = Lane.Dir;
			Fresh.bActive = Lane.bActive;
			Fresh.Phase = ELanePhase::AwaitArrive;
			Fresh.PhaseStart = Now;
			Lane = Fresh;
			return;
		}
		case ELanePhase::AwaitArrive:
			if ((FVector::Dist2D(Own->GetActorLocation(), Lane.Start) < 30.0 && Own->GetVelocity().Size() < 10.0 && InPhase > 0.4) || InPhase > 2.0)
			{
				Enter(ELanePhase::Approach);
			}
			return;
		case ELanePhase::Approach:
		{
			Own->AddMovementInput(Lane.Dir, 1.f);
			if (InPhase > 0.25 && FMath::Fmod(InPhase, 0.1) < Worlds.Server->GetDeltaSeconds() + 1e-3)
			{
				bool bCheckFailed = false, bMontageFailed = false;
				FString Signature;
				++Lane.Attempts;
				SCL045::TryTraversal(Own, bCheckFailed, bMontageFailed, Signature);
				if (!bSignatureWritten)
				{
					Out.Write(TEXT("tryTraversalSignature"), FString::Printf(TEXT("\"params\":%s"), *SCL045::Q(Signature)));
					bSignatureWritten = true;
				}
			}
			if (SCL045::GetBool(Own, TEXT("DoingTraversalAction")))
			{
				Lane.bOwnerStarted = true;
				Lane.OwnerStartT = Now;
				Enter(ELanePhase::InAction);
			}
			else if (InPhase > 2.5)
			{
				Lane.Result = TEXT("noStart");
				Enter(ELanePhase::Settle);
			}
			return;
		}
		case ELanePhase::InAction:
		{
			const bool bServerDoing = SCL045::GetBool(OnServer, TEXT("DoingTraversalAction"));
			const bool bOwnerDoing = SCL045::GetBool(Own, TEXT("DoingTraversalAction"));
			if (bServerDoing && !Lane.bServerStarted)
			{
				Lane.bServerStarted = true;
				Lane.ServerStartT = Now;
			}
			if (bServerDoing)
			{
				Lane.ServerModes.Add(SCL045::MoveMode(OnServer));
			}
			if (bOwnerDoing)
			{
				Lane.OwnerModes.Add(SCL045::MoveMode(Own));
				Lane.MaxOwnerDeviation = FMath::Max(Lane.MaxOwnerDeviation, FVector::Dist(Own->GetActorLocation(), OnServer->GetActorLocation()));
			}
			if (Lane.bServerStarted && !bServerDoing && !Lane.bServerEnded)
			{
				Lane.bServerEnded = true;
				Lane.ServerEndT = Now;
				Lane.ServerEnd = OnServer->GetActorLocation();
			}
			if (Lane.bOwnerStarted && !bOwnerDoing && !Lane.bOwnerEnded)
			{
				Lane.bOwnerEnded = true;
				Lane.OwnerEndT = Now;
				Lane.OwnerEnd = Own->GetActorLocation();
			}
			for (int32 Observer = 0; Observer < Worlds.Clients.Num(); ++Observer)
			{
				if (Observer == Lane.ClientIndex)
				{
					continue;
				}
				ACharacter* Proxy = SCL045::FindByGuid(Worlds.Clients[Observer], Lane.Guid);
				if (!Proxy)
				{
					continue;
				}
				// A proxy runs the sample's Blueprint traversal only from OnRep_TraversalResult, which does not fire
				// when two traversals produce the same result, so the proxy's action is detected from its replicated
				// movement mode (Flying during the action) or the Blueprint flag, whichever shows first.
				const bool bProxyBpDoing = SCL045::GetBool(Proxy, TEXT("DoingTraversalAction"));
				const bool bProxyDoing = bProxyBpDoing || Proxy->GetCharacterMovement()->MovementMode == MOVE_Flying;
				if (bProxyBpDoing)
				{
					Lane.ProxyBpStarted.Add(Observer);
				}
				if (bProxyDoing || Lane.bServerStarted)
				{
					++Lane.ProxyFrames;
					Lane.ProxyRepRootMotionFrames += Proxy->GetRepRootMotion().bIsActive ? 1 : 0;
					Lane.ProxyNetworkedRootMotionFrames += Proxy->IsPlayingNetworkedRootMotionMontage() ? 1 : 0;
				}
				if (bProxyDoing)
				{
					Lane.ProxyStarted.Add(Observer);
					Lane.ProxyModes.Add(SCL045::MoveMode(Proxy));
				}
				// The proxy's end is taken after the server's (a proxy lags the server); an earlier one-frame mode
				// flicker on a lost packet (seen in the first runs) is not the end of the action.
				if (Lane.ProxyStarted.Contains(Observer) && !bProxyDoing && !Lane.ProxyEnd.Contains(Observer) && Lane.bServerEnded)
				{
					Lane.ProxyEnd.Add(Observer, Proxy->GetActorLocation());
					Lane.ProxyEndT.Add(Observer, Now);
				}
			}
			const bool bAllEnded = Lane.bServerEnded && Lane.bOwnerEnded && Lane.ProxyEnd.Num() == Lane.ProxyStarted.Num() && Lane.ProxyStarted.Num() == Worlds.Clients.Num() - 1;
			if (bAllEnded || InPhase > 6.0)
			{
				Lane.Result = bAllEnded ? TEXT("ok") : TEXT("timeout");
				Enter(ELanePhase::Settle);
			}
			return;
		}
		case ELanePhase::Settle:
			if (InPhase < 1.0)
			{
				return;
			}
			WriteTrial(Worlds, Lane, OnServer, Own);
			Lane.Phase = ELanePhase::Done;
			return;
		default:
			return;
		}
	}

	void WriteTrial(const SCL045::FWorlds& Worlds, FLane& Lane, ACharacter* OnServer, ACharacter* Own)
	{
		FString Proxies;
		for (int32 Observer = 0; Observer < Worlds.Clients.Num(); ++Observer)
		{
			if (Observer == Lane.ClientIndex)
			{
				continue;
			}
			const ACharacter* Proxy = SCL045::FindByGuid(Worlds.Clients[Observer], Lane.Guid);
			const FVector* End = Lane.ProxyEnd.Find(Observer);
			const double* EndT = Lane.ProxyEndT.Find(Observer);
			Proxies += FString::Printf(TEXT("%s{\"observer\":%d,\"started\":%d,\"bpTraversal\":%d,\"end\":%s,\"endT\":%.6f,\"settled\":%s}"), Proxies.IsEmpty() ? TEXT("") : TEXT(","), Observer,
				(int32)Lane.ProxyStarted.Contains(Observer), (int32)Lane.ProxyBpStarted.Contains(Observer), End ? *SCL045::V(*End) : TEXT("null"), EndT ? *EndT : -1.0,
				Proxy ? *SCL045::V(Proxy->GetActorLocation()) : TEXT("null"));
		}
		auto Join = [](const TSet<FString>& Set) { FString S; for (const FString& Item : Set) { S += (S.IsEmpty() ? TEXT("") : TEXT("|")) + Item; } return S; };
		Out.Write(TEXT("trial"), FString::Printf(
			TEXT("\"kind\":%s,\"trial\":%d,\"client\":%d,\"result\":%s,\"attempts\":%d,\"start\":%s,")
			TEXT("\"serverStartT\":%.6f,\"serverEndT\":%.6f,\"serverEnd\":%s,\"serverSettled\":%s,\"ownerStartT\":%.6f,\"ownerEndT\":%.6f,\"ownerEnd\":%s,\"ownerSettled\":%s,")
			TEXT("\"maxOwnerDeviation\":%.2f,\"serverModes\":%s,\"ownerModes\":%s,\"proxyModes\":%s,\"proxyFrames\":%d,\"proxyRepRootMotionFrames\":%d,\"proxyNetworkedRootMotionFrames\":%d,")
			TEXT("\"serverResult\":%s,\"ownerResult\":%s,\"proxies\":[%s]"),
			*SCL045::Q(SCL045::Obstacles[Course.KindIndex].Kind), Course.Trial, Lane.ClientIndex, *SCL045::Q(Lane.Result), Lane.Attempts, *SCL045::V(Lane.Start),
			Lane.ServerStartT, Lane.ServerEndT, Lane.bServerEnded ? *SCL045::V(Lane.ServerEnd) : TEXT("null"), *SCL045::V(OnServer->GetActorLocation()),
			Lane.OwnerStartT, Lane.OwnerEndT, Lane.bOwnerEnded ? *SCL045::V(Lane.OwnerEnd) : TEXT("null"), *SCL045::V(Own->GetActorLocation()),
			Lane.MaxOwnerDeviation, *SCL045::Q(Join(Lane.ServerModes)), *SCL045::Q(Join(Lane.OwnerModes)), *SCL045::Q(Join(Lane.ProxyModes)),
			Lane.ProxyFrames, Lane.ProxyRepRootMotionFrames, Lane.ProxyNetworkedRootMotionFrames,
			*SCL045::TraversalResultJson(OnServer), *SCL045::TraversalResultJson(Own), *Proxies));
	}

	bool bSignatureWritten = false;
	static constexpr int32 MaxRoundsPerKind = 70;

	bool StepTraversal()
	{
		const SCL045::FWorlds Worlds = SCL045::FindWorlds();
		if (!Worlds.Server || Worlds.Clients.Num() != SCL045::ClientCount)
		{
			return false;
		}
		if (!Course.bBuilt)
		{
			if (!BuildCourse(Worlds))
			{
				return true; // reported as an error record; the assertion below fails the run
			}
			return false;
		}
		if (!Course.bKindReady)
		{
			PlaceLanesForKind();
		}
		int32 Active = 0;
		int32 Completed = 0;
		bool bAllDone = true;
		for (FLane& Lane : Course.Lanes)
		{
			StepLane(Worlds, Lane);
			bAllDone &= Lane.Phase == ELanePhase::Done;
			Active += Lane.bActive ? 1 : 0;
			Completed += (Lane.bActive && Lane.Result == TEXT("ok")) ? 1 : 0;
		}
		if (!bAllDone)
		{
			return false;
		}
		// n counts completed traversals (the AC's n >= 100 per kind); a ceiling of rounds bounds a lane that never starts.
		Course.Done += Completed;
		++Course.Trial;
		if (Active == 0 || Course.Done >= TrialsForKind(Course.KindIndex) || Course.Trial >= MaxRoundsPerKind)
		{
			Course.Done = 0;
			Course.Trial = 0;
			Course.bKindReady = false;
			if (++Course.KindIndex >= UE_ARRAY_COUNT(SCL045::Obstacles))
			{
				Out.Write(TEXT("done"), TEXT(""));
				return true;
			}
		}
		for (FLane& Lane : Course.Lanes)
		{
			Lane.Phase = ELanePhase::Teleport;
		}
		return false;
	}

	TEST_METHOD(Traversal)
	{
		Network
			.UntilServer(TEXT("Pawns ready"), [this](FBasePIENetworkComponentState&) { return AwaitPawns(TEXT("traversal")); })
			.UntilServer(TEXT("Traversal trials"), [this](FBasePIENetworkComponentState&) { return StepTraversal(); })
			.ThenServer(TEXT("Course was built"), [this](FBasePIENetworkComponentState&)
			{
				ASSERT_THAT(IsTrue(Course.bBuilt, TEXT("the traversal course was spawned in every PIE world")));
			});
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS && ENABLE_PIE_NETWORK_TEST
