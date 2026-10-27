// Shared Character & Locomotion PIE clauses (ADR-0008 T2), listen server + 1 client on L_Prototype, StealthGame.Locomotion.PIE:
//
// ReleaseBaselineStopUnderLag (story SCL-011, AC-SCL-06 owner-prediction clause): with PktLag 100 ms on every driver (the
// AC's 100 ms lag; about 200 ms round trip), the client walks at 140 cm/s (W held, walking a slow circle so it stays in the
// open street) and releases. The owner-predicted stop (0.75 s, 53 +- 3 cm of path) and the server's stop of the same pawn
// match: completion time within one 60 Hz frame plus one ServerMove send interval (the server applies the owner's moves
// in batches; QA clarification of "within 1 frame": stop-completion time and final position), both stop distances inside
// the 53 +- 3 cm band, and the rest positions agree within 2 cm.
// Determinism (fix 2026-10-27, the WalkSlowerModifier pattern of b764c8d): two sources made it intermittent in full-suite
// runs. (1) Layout: the round seed and spawn slot come from OS entropy, so the old open-loop circle (a fixed yaw per frame
// from the spawn, started during Preparing) crossed a different city every run and could run into an adobe piece or the
// host's pawn. The walk now waits for Active, starts on a clear flat circle found on the server
// (Tests/PIE/StealthPIEOpenCircle.h), is steered closed-loop round it after a warm-up, a rest-and-agree wait and a snap of
// the resting owner onto the server copy (the teleport leaves millimetres the correction threshold never closes), and is
// released only after a steady 140 cm/s (asserted). (2) Server timing: the server's stop was timed on server frame time,
// so its start and end each moved with whichever server frame a batch of the owner's moves landed in (0.733-0.758 s over
// six isolated runs against the owner's 0.758 s, inside a 0.034 s bound; a loaded full-suite run pushed it out). The
// server copy is now sampled once per processed ServerMove and timed on that move's client timestamp
// (FNetworkPredictionData_Server_Character::CurrentClientTimeStamp), so server frame timing cannot reach the comparison;
// the bound (one frame plus one send interval) is unchanged.
//
// FactoryBindFailNeverReplicates (story SCL-007, AC-SCL-03 client clause): the server factory spawns a participant pawn
// whose BindPawn the real Roster rejects (an unknown participant id); the pawn is destroyed before FinishSpawning, and the
// connected client never receives it (no new client pawn appears at the rejected spawn point over 60 frames).

#include "CoreMinimal.h"
#include "Tests/PIE/StealthPIENetworkComponent.h"
#include "Tests/PIE/StealthPIEOpenCircle.h"

#if WITH_DEV_AUTOMATION_TESTS && defined(ENABLE_PIE_NETWORK_TEST) && ENABLE_PIE_NETWORK_TEST

#include "CQTest.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "InputKeyEventArgs.h"
#include "Server/Core/StealthPawnFactory.h"
#include "StealthGameMode.h"
#include "StealthGameState.h"
#include "StealthSettings.h"
#include "View/Core/StealthPawn.h"

namespace LocomotionPieTestData
{
	const TCHAR* const FixtureMap = TEXT("/Game/StealthGame/Maps/L_Prototype");
	const TCHAR* const NetworkTimeoutCeilingSeconds = TEXT("240");
	constexpr int32 LagMs = 100;
	/** Minimum frames of the measured walk before the release (it also needs SteadyFrames at the walk pace). */
	constexpr uint64 WalkFrames = 180;
	/** The measured walk stops waiting for a steady pace after this many frames (the precondition assert then fails). */
	constexpr uint64 WalkMaxFrames = 900;
	/** Consecutive frames at the walk pace (within SteadyToleranceCm of WalkPaceCm) before the release. */
	constexpr uint64 SteadyFrames = 30;
	constexpr double WalkPaceCm = 140.0;
	constexpr double SteadyToleranceCm = 0.5;
	constexpr uint64 StopFrames = 120;
	/** The orbit: 300 cm radius (27 deg/s at 140 cm/s, inside WalkTurnRateDegPerSec 60), steered closed-loop. */
	constexpr double CircleRadiusCm = 300.0;
	constexpr double CircleCorrectionCm = 100.0;
	/** The largest control-yaw step per frame of the orbit steering. */
	constexpr double MaxYawStepDeg = 0.6;
	/** The owner has arrived at the start point when it is this close and this slow, after SettleFrames. */
	constexpr double ArriveDistanceCm = 30.0;
	constexpr double ArriveSpeedCm = 5.0;
	constexpr uint64 SettleFrames = 60;
	/** The unchecked warm-up walk after the teleport (frames; a correction left by the teleport lands here). */
	constexpr uint64 WarmUpWalkFrames = 90;
	/** Owner and server copies agree within this, both below RestSpeedCm, for SettleFrames before the measured walk. */
	constexpr double AgreeDistanceCm = 10.0;
	constexpr double RestSpeedCm = 0.1;
	constexpr uint64 AgreeMaxFrames = 600;
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

		/** One sample at time Now (the owner: its world time; the server copy: the processed move's client timestamp). */
		void Sample(const AActor* Pawn, double Now, bool bReleased)
		{
			if (LastSpeed >= 0.0 && Now == LastTime)
			{
				return; // no new move processed since the last sample (server copy)
			}
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
	/** The orbit centre and start point (world space; the same layout on every PIE world, RoundSeed replicates). */
	FVector CircleCentre = FVector::ZeroVector;
	FVector CircleStart = FVector::ZeroVector;
	/** Consecutive frames of the current wait condition (rest-and-agree, then the steady pace). */
	uint64 RunFrames = 0;

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

	/** The client's pawn on its own machine (the owner copy). */
	static ACharacter* OwnerPawn() { APlayerController* PC = ClientController(); return PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr; }

	/**
	 * The client timestamp of the last ServerMove the server applied to its copy (the owner's move clock), or -1 before the
	 * first one. Server frame timing never reaches a sample taken on it.
	 */
	static double ServerMoveTime(const ACharacter* ServerPawn)
	{
		UCharacterMovementComponent* Movement = ServerPawn ? ServerPawn->GetCharacterMovement() : nullptr;
		const FNetworkPredictionData_Server_Character* Data = Movement ? Movement->GetPredictionData_Server_Character() : nullptr;
		return Data ? static_cast<double>(Data->CurrentClientTimeStamp) : -1.0;
	}

	/** Steers the owner's control yaw one step along the orbit round CircleCentre. */
	void SteerOrbit() const
	{
		using namespace LocomotionPieTestData;
		APlayerController* PC = ClientController();
		const ACharacter* Walker = OwnerPawn();
		if (PC && Walker)
		{
			FRotator Control = PC->GetControlRotation();
			const double Heading = StealthPIEOpenCircle::OrbitYawDeg(Walker->GetActorLocation(), CircleCentre, CircleRadiusCm, CircleCorrectionCm);
			Control.Yaw = StealthPIEOpenCircle::SteerToward(Control.Yaw, Heading, MaxYawStepDeg);
			PC->SetControlRotation(Control);
		}
	}

	/** Points the owner's control yaw along its body facing (a start straight ahead: no idle pivot). */
	static void AlignControlToFacing()
	{
		if (APlayerController* PC = ClientController())
		{
			if (const APawn* Walker = PC->GetPawn())
			{
				FRotator Control = PC->GetControlRotation();
				Control.Yaw = Walker->GetActorRotation().Yaw;
				PC->SetControlRotation(Control);
			}
		}
	}

	/** Samples both traces: the owner on its world time, the server copy on its last processed move's client timestamp. */
	void SampleBoth(bool bAfterRelease)
	{
		if (const ACharacter* Walker = OwnerPawn())
		{
			Owner.Sample(Walker, Walker->GetWorld()->GetTimeSeconds(), bAfterRelease);
		}
		const ACharacter* ServerPawn = ServerCopyOfClientPawn();
		const double MoveTime = ServerMoveTime(ServerPawn);
		if (ServerPawn && MoveTime >= 0.0)
		{
			Server.Sample(ServerPawn, MoveTime, bAfterRelease);
		}
	}

	TEST_METHOD(ReleaseBaselineStopUnderLag)
	{
		using namespace LocomotionPieTestData;
		Network
			.Until(TEXT("the client possesses its pawn on both sides in the Active phase"), []()
			{
				const APlayerController* PC = ClientController();
				const AStealthGameState* State = PC && PC->GetWorld() ? PC->GetWorld()->GetGameState<AStealthGameState>() : nullptr;
				return PC && Cast<AStealthPawn>(PC->GetPawn()) && ServerCopyOfClientPawn() && State && State->GetRoundPublicState().Phase == ERoundPhase::Active;
			})
			.Then(TEXT("move the walker onto an open circle"), [this]()
			{
				ACharacter* ServerPawn = ServerCopyOfClientPawn();
				ASSERT_THAT(IsTrue(ServerPawn != nullptr, TEXT("the server's copy of the walker")));
				const float HalfHeight = ServerPawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
				StealthPIEOpenCircle::FParams Params;
				Params.RadiusCm = CircleRadiusCm;
				const TOptional<FVector> Centre = StealthPIEOpenCircle::FindOpenCircle(WorldOf(NM_ListenServer), ServerPawn->GetActorLocation() - FVector(0.0, 0.0, HalfHeight), ServerPawn, Params);
				ASSERT_THAT(IsTrue(Centre.IsSet(), TEXT("an open, flat circle near the walker's spawn")));
				CircleCentre = Centre.GetValue();
				// Start on the circle's +X point facing the anticlockwise tangent (+Y).
				CircleStart = CircleCentre + FVector(CircleRadiusCm, 0.0, 0.0);
				const FRotator Facing(0.0, 90.0, 0.0);
				ServerPawn->GetCharacterMovement()->StopMovementImmediately();
				ServerPawn->TeleportTo(CircleStart + FVector(0.0, 0.0, HalfHeight + 2.0), Facing, false, true);
				if (APlayerController* ServerPC = Cast<APlayerController>(ServerPawn->GetController()))
				{
					ServerPC->SetControlRotation(Facing);
					ServerPC->ClientSetRotation(Facing, true);
				}
				// The owner is moved in the same frame (both worlds run in this process).
				if (ACharacter* Walker = OwnerPawn())
				{
					Walker->GetCharacterMovement()->StopMovementImmediately();
					Walker->TeleportTo(CircleStart + FVector(0.0, 0.0, HalfHeight + 2.0), Facing, false, true);
				}
				if (APlayerController* OwnerPC = ClientController())
				{
					OwnerPC->SetControlRotation(Facing);
				}
				TestRunner->AddInfo(FString::Printf(TEXT("LOCOPIE open circle centre %s"), *CircleCentre.ToCompactString()));
				Mark = GFrameCounter;
			})
			.Until(TEXT("the owner arrives at the circle"), [this]()
			{
				const ACharacter* Walker = OwnerPawn();
				return Walker && FVector::Dist2D(Walker->GetActorLocation(), CircleStart) < ArriveDistanceCm && Walker->GetVelocity().Size() < ArriveSpeedCm && GFrameCounter >= Mark + SettleFrames;
			})
			// Warm-up (unchecked): a short orbit and a stop, so a correction left over from the teleport (under 100 ms lag the
			// server replays the owner's pre-teleport moves) lands here, never in the measured stop.
			.Then(TEXT("warm up: walk"), [this]() { AlignControlToFacing(); Key(EKeys::W, true); Mark = GFrameCounter; })
			.Until(TEXT("warm up: walk the circle"), [this]() { SteerOrbit(); return GFrameCounter >= Mark + WarmUpWalkFrames; })
			.Then(TEXT("warm up: stop"), [this]() { Key(EKeys::W, false); Mark = GFrameCounter; RunFrames = 0; })
			.Until(TEXT("warm up: at rest and in agreement with the server"), [this]()
			{
				const ACharacter* Walker = OwnerPawn();
				const ACharacter* OnServer = ServerCopyOfClientPawn();
				const bool bAgree = Walker && OnServer && FVector::Dist2D(Walker->GetActorLocation(), OnServer->GetActorLocation()) < AgreeDistanceCm
					&& Walker->GetVelocity().Size() < RestSpeedCm && OnServer->GetVelocity().Size() < RestSpeedCm;
				RunFrames = bAgree ? RunFrames + 1 : 0;
				const bool bGaveUp = GFrameCounter >= Mark + AgreeMaxFrames;
				const bool bDone = (bAgree && RunFrames >= SettleFrames) || bGaveUp;
				if (bDone && Walker && OnServer)
				{
					TestRunner->AddInfo(FString::Printf(TEXT("LOCOPIE warm-up done: owner/server distance %.2f cm%s"),
						FVector::Dist2D(Walker->GetActorLocation(), OnServer->GetActorLocation()), bGaveUp ? TEXT(" (gave up waiting)") : TEXT("")));
				}
				return bDone;
			})
			// Both copies rest, but the teleport and its landing leave them a few millimetres apart, under the CMC's correction
			// threshold, so nothing would ever close the gap (the old spawn start had none). The owner copy is set onto the
			// server copy's exact transform at rest (no move is pending), so the measured walk starts from one state.
			.Then(TEXT("snap the resting owner onto the server copy"), [this]()
			{
				ACharacter* Walker = OwnerPawn();
				const ACharacter* OnServer = ServerCopyOfClientPawn();
				ASSERT_THAT(IsTrue(Walker != nullptr && OnServer != nullptr, TEXT("both copies of the walker")));
				Walker->SetActorLocationAndRotation(OnServer->GetActorLocation(), OnServer->GetActorRotation(), false, nullptr, ETeleportType::TeleportPhysics);
				Mark = GFrameCounter;
			})
			.Until(TEXT("the snapped owner settles"), [this]() { return GFrameCounter >= Mark + SettleFrames; })
			.Then(TEXT("check the start agreement"), [this]()
			{
				const ACharacter* Walker = OwnerPawn();
				const ACharacter* OnServer = ServerCopyOfClientPawn();
				const double Gap = Walker && OnServer ? FVector::Dist2D(Walker->GetActorLocation(), OnServer->GetActorLocation()) : -1.0;
				TestRunner->AddInfo(FString::Printf(TEXT("LOCOPIE start agreement after the snap: %.3f cm"), Gap));
				ASSERT_THAT(IsTrue(Gap >= 0.0 && Gap <= 0.1, TEXT("precondition: owner and server start the measured walk at one position")));
			})
			.Then(TEXT("hold W"), [this]() { AlignControlToFacing(); Key(EKeys::W, true); Mark = GFrameCounter; RunFrames = 0; })
			.Until(TEXT("walk the circle at a steady 140 cm/s"), [this]()
			{
				SteerOrbit();
				// Seed both traces with the steady walk, so the first decelerating sample after the release is found.
				SampleBoth(false);
				const ACharacter* Walker = OwnerPawn();
				const bool bSteady = Walker && FMath::Abs(Walker->GetVelocity().Size2D() - WalkPaceCm) <= SteadyToleranceCm;
				RunFrames = bSteady ? RunFrames + 1 : 0;
				return (GFrameCounter >= Mark + WalkFrames && RunFrames >= SteadyFrames) || GFrameCounter >= Mark + WalkMaxFrames;
			})
			.Then(TEXT("release W"), [this]()
			{
				const ACharacter* Walker = OwnerPawn();
				const double Speed = Walker ? Walker->GetVelocity().Size2D() : -1.0;
				TestRunner->AddInfo(FString::Printf(TEXT("LOCOPIE owner speed at release %.2f cm/s after %llu steady frames"), Speed, RunFrames));
				ASSERT_THAT(IsNear(WalkPaceCm, Speed, SteadyToleranceCm, TEXT("precondition: the owner walks a steady 140 cm/s at the release")));
				Key(EKeys::W, false);
				bReleased = true;
				Mark = GFrameCounter;
			})
			.Until(TEXT("both sides come to rest"), [this]()
			{
				SampleBoth(bReleased);
				return GFrameCounter >= Mark + StopFrames;
			})
			.Then(TEXT("assert"), [this]()
			{
				const double OwnerSec = Owner.EndTime - Owner.StartTime;
				const double ServerSec = Server.EndTime - Server.StartTime;
				const double RestGap = FVector::Dist2D(Owner.Rest, Server.Rest);
				const float SendInterval = 1.0f / 60.0f;
				TestRunner->AddInfo(FString::Printf(TEXT("LOCOPIE lag %d ms: owner stop %.3f s over %.2f cm; server stop %.3f s (move clock) over %.2f cm; rest positions %.2f cm apart"),
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
