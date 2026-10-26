// Shared Character & Locomotion PIE clauses (ADR-0008 T2), listen server + 1 client on L_Prototype, StealthGame.Locomotion.PIE:
//
// ReleaseBaselineStopUnderLag (story SCL-011, AC-SCL-06 owner-prediction clause): with PktLag 100 ms on every driver (the
// AC's 100 ms lag; about 200 ms round trip), the client walks at 140 cm/s (W held, walking a slow circle so it stays in the
// open street) and releases. The owner-predicted stop (0.75 s, 53 +- 3 cm of path) and the server's stop of the same pawn
// match: completion time within one 60 Hz frame plus one ServerMove send interval (the server applies the owner's moves
// in batches; QA clarification of "within 1 frame": stop-completion time and final position), both stop distances inside
// the 53 +- 3 cm band, and the rest positions agree within 2 cm.
//
// FactoryBindFailNeverReplicates (story SCL-007, AC-SCL-03 client clause): the server factory spawns a participant pawn
// whose BindPawn the real Roster rejects (an unknown participant id); the pawn is destroyed before FinishSpawning, and the
// connected client never receives it (no new client pawn appears at the rejected spawn point over 60 frames).

#include "CoreMinimal.h"
#include "Tests/PIE/StealthPIENetworkComponent.h"

#if WITH_DEV_AUTOMATION_TESTS && defined(ENABLE_PIE_NETWORK_TEST) && ENABLE_PIE_NETWORK_TEST

#include "CQTest.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "InputKeyEventArgs.h"
#include "Server/Core/StealthPawnFactory.h"
#include "StealthGameMode.h"
#include "StealthSettings.h"
#include "View/Core/StealthPawn.h"

namespace LocomotionPieTestData
{
	const TCHAR* const FixtureMap = TEXT("/Game/StealthGame/Maps/L_Prototype");
	const TCHAR* const NetworkTimeoutCeilingSeconds = TEXT("240");
	constexpr int32 LagMs = 100;
	constexpr uint64 WalkFrames = 180;
	constexpr uint64 StopFrames = 120;
	constexpr double CircleYawPerFrameDeg = 0.5;
	constexpr double StopSec = 0.75;
	constexpr double StopCm = 53.0;
	constexpr double StopCmTolerance = 3.0;

	/** One side's stop measurement: time from the first decelerating sample to rest, and the path length. */
	struct FStopTrace
	{
		double LastSpeed = -1.0;
		double LastTime = 0.0;
		FVector LastLocation = FVector::ZeroVector;
		double StartTime = -1.0;
		double EndTime = -1.0;
		double PathCm = 0.0;
		FVector Rest = FVector::ZeroVector;

		void Sample(const AActor* Pawn, double Now, bool bReleased)
		{
			const double Speed = Pawn->GetVelocity().Size2D();
			const FVector Location = Pawn->GetActorLocation();
			if (bReleased && EndTime < 0.0)
			{
				if (StartTime < 0.0 && LastSpeed >= 130.0 && Speed < LastSpeed - 0.01)
				{
					// The stop began at the previous sample (its speed was the last steady one).
					StartTime = LastTime;
					PathCm = FVector::Dist2D(LastLocation, Location);
				}
				else if (StartTime >= 0.0)
				{
					PathCm += FVector::Dist2D(LastLocation, Location);
				}
				if (StartTime >= 0.0 && Speed <= 0.0)
				{
					EndTime = Now;
					Rest = Location;
				}
			}
			LastSpeed = Speed;
			LastTime = Now;
			LastLocation = Location;
		}
	};
}

NETWORK_TEST_CLASS(LocomotionPie, "StealthGame.Locomotion.PIE")
{
	TStealthPIENetworkComponent<FBasePIENetworkComponentState> Network{ TestRunner, TestCommandBuilder };
	FPacketSimulationSettings PacketSettings;
	inline static FString PreviousNetworkTimeout;
	inline static int32 PreviousNpcCount = -1;

	TArray<FString> Labels;
	const TCHAR* L(FString Label) { return *Labels.Add_GetRef(MoveTemp(Label)); }

	uint64 Mark = 0;
	bool bReleased = false;
	LocomotionPieTestData::FStopTrace Owner;
	LocomotionPieTestData::FStopTrace Server;
	FVector RejectedAt = FVector::ZeroVector;
	TSet<TWeakObjectPtr<AStealthPawn>> ClientPawnsBefore;
	int32 ClientArrivals = 0;

	BEFORE_ALL()
	{
		if (IConsoleVariable* Timeout = IConsoleManager::Get().FindConsoleVariable(CQTestConsoleVariables::NetworkTimeoutName))
		{
			PreviousNetworkTimeout = Timeout->GetString();
			Timeout->Set(LocomotionPieTestData::NetworkTimeoutCeilingSeconds, ECVF_SetByCode);
		}
		// No crowd: a walking NPC must not block the measured walker (spike SCL-045 pattern).
		UStealthSettings* Settings = GetMutableDefault<UStealthSettings>();
		PreviousNpcCount = Settings->NpcCount;
		Settings->NpcCount = 0;
	}

	AFTER_ALL()
	{
		if (IConsoleVariable* Timeout = IConsoleManager::Get().FindConsoleVariable(CQTestConsoleVariables::NetworkTimeoutName))
		{
			if (!PreviousNetworkTimeout.IsEmpty())
			{
				Timeout->Set(*PreviousNetworkTimeout, ECVF_SetByCode);
			}
		}
		if (PreviousNpcCount >= 0)
		{
			GetMutableDefault<UStealthSettings>()->NpcCount = PreviousNpcCount;
		}
	}

	BEFORE_EACH()
	{
		PacketSettings = FPacketSimulationSettings();
		PacketSettings.PktLag = LocomotionPieTestData::LagMs;
		FNetworkComponentBuilder<FBasePIENetworkComponentState>()
			.AsListenServer()
			.WithClients(1)
			.WithGameMode(AStealthGameMode::StaticClass())
			.WithPacketSimulationSettings(&PacketSettings)
			.Build(Network);
		Network.StartOnFixtureMap(LocomotionPieTestData::FixtureMap);
	}

	static UWorld* WorldOf(ENetMode Mode)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE && Context.World() && Context.World()->GetNetMode() == Mode)
			{
				return Context.World();
			}
		}
		return nullptr;
	}

	static APlayerController* ClientController() { UWorld* W = WorldOf(NM_Client); return W ? W->GetFirstPlayerController() : nullptr; }

	static ACharacter* ServerCopyOfClientPawn()
	{
		UWorld* ServerWorld = WorldOf(NM_ListenServer);
		const APlayerController* Client = ClientController();
		const APlayerState* ClientState = Client ? Client->PlayerState.Get() : nullptr;
		if (!ServerWorld || !ClientState)
		{
			return nullptr;
		}
		for (FConstPlayerControllerIterator It = ServerWorld->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* PC = It->Get();
			if (PC && !PC->IsLocalController() && PC->PlayerState && PC->PlayerState->GetPlayerId() == ClientState->GetPlayerId())
			{
				return Cast<ACharacter>(PC->GetPawn());
			}
		}
		return nullptr;
	}

	static void Key(const FKey& InKey, bool bDown)
	{
		if (APlayerController* PC = ClientController())
		{
			PC->InputKey(FInputKeyEventArgs::CreateSimulated(InKey, bDown ? IE_Pressed : IE_Released, bDown ? 1.f : 0.f));
		}
	}

	/** Client pawns not present before that stand at the rejected spawn point (the rejected pawn, had it replicated). */
	int32 CountArrivalsAt(UWorld* World, const FVector& At) const
	{
		int32 Count = 0;
		for (TActorIterator<AStealthPawn> It(World); It; ++It)
		{
			if (IsValid(*It) && !ClientPawnsBefore.Contains(*It) && FVector::Dist2D(It->GetActorLocation(), At) <= 60.0)
			{
				++Count;
			}
		}
		return Count;
	}

	TEST_METHOD(ReleaseBaselineStopUnderLag)
	{
		using namespace LocomotionPieTestData;
		Network
			.Until(TEXT("the client possesses its pawn on both sides"), []() { return ClientController() && Cast<AStealthPawn>(ClientController()->GetPawn()) && ServerCopyOfClientPawn(); })
			.Then(TEXT("hold W"), [this]() { Key(EKeys::W, true); Mark = GFrameCounter; })
			.Until(TEXT("walk at 140 cm/s"), [this]()
			{
				if (APlayerController* PC = ClientController())
				{
					PC->SetControlRotation(PC->GetControlRotation() + FRotator(0.0, CircleYawPerFrameDeg, 0.0));
				}
				// Seed both traces with the steady walk, so the first decelerating sample after the release is found.
				if (const ACharacter* OwnerPawn = Cast<ACharacter>(ClientController()->GetPawn()))
				{
					Owner.Sample(OwnerPawn, OwnerPawn->GetWorld()->GetTimeSeconds(), false);
				}
				if (const ACharacter* ServerPawn = ServerCopyOfClientPawn())
				{
					Server.Sample(ServerPawn, ServerPawn->GetWorld()->GetTimeSeconds(), false);
				}
				return GFrameCounter >= Mark + WalkFrames;
			})
			.Then(TEXT("release W"), [this]()
			{
				const ACharacter* OwnerPawn = Cast<ACharacter>(ClientController()->GetPawn());
				TestRunner->AddInfo(FString::Printf(TEXT("LOCOPIE owner speed at release %.2f cm/s"), OwnerPawn ? OwnerPawn->GetVelocity().Size2D() : -1.0));
				Key(EKeys::W, false);
				bReleased = true;
				Mark = GFrameCounter;
			})
			.Until(TEXT("both sides come to rest"), [this]()
			{
				if (const ACharacter* OwnerPawn = Cast<ACharacter>(ClientController()->GetPawn()))
				{
					Owner.Sample(OwnerPawn, OwnerPawn->GetWorld()->GetTimeSeconds(), bReleased);
				}
				if (const ACharacter* ServerPawn = ServerCopyOfClientPawn())
				{
					Server.Sample(ServerPawn, ServerPawn->GetWorld()->GetTimeSeconds(), bReleased);
				}
				return GFrameCounter >= Mark + StopFrames;
			})
			.Then(TEXT("assert"), [this]()
			{
				const double OwnerSec = Owner.EndTime - Owner.StartTime;
				const double ServerSec = Server.EndTime - Server.StartTime;
				const double RestGap = FVector::Dist2D(Owner.Rest, Server.Rest);
				const float SendInterval = 1.0f / 60.0f;
				TestRunner->AddInfo(FString::Printf(TEXT("LOCOPIE lag %d ms: owner stop %.3f s over %.2f cm; server stop %.3f s over %.2f cm; rest positions %.2f cm apart"),
					LagMs, OwnerSec, Owner.PathCm, ServerSec, Server.PathCm, RestGap));
				ASSERT_THAT(IsTrue(Owner.StartTime >= 0.0 && Owner.EndTime >= 0.0, TEXT("the owner stopped")));
				ASSERT_THAT(IsTrue(Server.StartTime >= 0.0 && Server.EndTime >= 0.0, TEXT("the server stopped")));
				ASSERT_THAT(IsNear(StopSec, OwnerSec, 1.0 / 60.0 + 1e-3, L(FString::Printf(TEXT("owner stop %.3f s is 0.75 s +- 1 frame"), OwnerSec))));
				ASSERT_THAT(IsNear(StopCm, Owner.PathCm, StopCmTolerance, L(FString::Printf(TEXT("owner stop %.2f cm is 53 +- 3"), Owner.PathCm))));
				ASSERT_THAT(IsNear(StopCm, Server.PathCm, StopCmTolerance, L(FString::Printf(TEXT("server stop %.2f cm is 53 +- 3"), Server.PathCm))));
				ASSERT_THAT(IsNear(OwnerSec, ServerSec, 1.0 / 60.0 + SendInterval + 1e-3, L(FString::Printf(TEXT("owner and server stop times agree (%.3f vs %.3f s)"), OwnerSec, ServerSec))));
				ASSERT_THAT(IsTrue(RestGap <= 2.0, L(FString::Printf(TEXT("owner and server rest positions agree (%.2f cm)"), RestGap))));
			});
	}

	TEST_METHOD(FactoryBindFailNeverReplicates)
	{
		Network
			.Until(TEXT("the client possesses its pawn"), []() { return ClientController() && Cast<AStealthPawn>(ClientController()->GetPawn()) && ServerCopyOfClientPawn(); })
			.Then(TEXT("spawn a pawn whose bind the Roster rejects"), [this]()
			{
				UWorld* ServerWorld = WorldOf(NM_ListenServer);
				UStealthPawnFactory* Factory = UStealthPawnFactory::Get(ServerWorld);
				ASSERT_THAT(IsNotNull(Factory, TEXT("the server factory exists")));
				for (TActorIterator<AStealthPawn> It(WorldOf(NM_Client)); It; ++It)
				{
					ClientPawnsBefore.Add(*It);
				}
				FStealthSpawnRequest Request;
				Request.Transform = ServerCopyOfClientPawn()->GetActorTransform();
				Request.Transform.AddToTranslation(FVector(0.0, 300.0, 0.0));
				RejectedAt = Request.Transform.GetLocation();
				Request.ControlKind = EStealthPawnControlKind::Ai;
				Request.Participant = FParticipantId(0x7fff0001); // never issued: BindPawn answers Rejected_UnknownParticipant
				const FStealthSpawnResult Result = Factory->SpawnSharedPawn(Request);
				TestRunner->AddInfo(FString::Printf(TEXT("LOCOPIE bind-fail spawn: pawn %s, bind code %d"), Result.Pawn ? TEXT("returned") : TEXT("null"), static_cast<int32>(Result.Bind)));
				ASSERT_THAT(IsTrue(Result.Pawn == nullptr && IsRosterRejection(Result.Bind), TEXT("the factory returned the rejection and no pawn")));
				Mark = GFrameCounter;
			})
			.Until(TEXT("60 frames on the client"), [this]()
			{
				ClientArrivals = FMath::Max(ClientArrivals, CountArrivalsAt(WorldOf(NM_Client), RejectedAt));
				return GFrameCounter >= Mark + 60;
			})
			.Then(TEXT("assert"), [this]()
			{
				TestRunner->AddInfo(FString::Printf(TEXT("LOCOPIE bind-fail: new client pawns at the rejected spawn point over 60 frames: %d"), ClientArrivals));
				ASSERT_THAT(IsTrue(ClientArrivals == 0, TEXT("the connected client never received the rejected pawn")));
			});
	}
};

#endif
