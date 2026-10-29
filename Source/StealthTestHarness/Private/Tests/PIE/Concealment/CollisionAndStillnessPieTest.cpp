// Concealment stories con-011 (AC3, GDD AC-CON-28 PIE part; the physical check deferred from con-008 AC1b) and con-013
// (AC1 server part, GDD AC-CON-09): listen server + 1 client on L_Prototype, no crowd.
//   CollisionAndAvoidance: the host's pawn (Outside) walking straight into a hide volume for 2 s is blocked at the planter
//     (its capsule never enters the footprint); then the host and the client's pawn enter the same bush together: neither
//     collides with the other or the bush during the warp (both end at the anchor, Concealed), and a third, Outside pawn
//     standing at the exit side still blocks (an exit there picks another side).
//   InsideViewAndStillness: the concealed host holds Move forward for 2 s while sweeping Look through 180 degrees: server
//     displacement 0 cm (< 0.5 cm) and body yaw unchanged; a Prepare and an Interact are refused (no Brace pose, still
//     Concealed: neither is a break).

#include "CoreMinimal.h"
#include "Tests/PIE/StealthPIENetworkComponent.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS && defined(ENABLE_PIE_NETWORK_TEST) && ENABLE_PIE_NETWORK_TEST

#include "CQTest.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/IConsoleManager.h"
#include "InputKeyEventArgs.h"
#include "Server/ConcealmentService.h"
#include "StealthGameMode.h"
#include "StealthSettings.h"
#include "Tests/PIE/Assassination/AssassinationPieSupport.h"
#include "View/BushRuntimeSubsystem.h"
#include "View/CourtFixtureId.h"

NETWORK_TEST_CLASS(CollisionAndStillness, "StealthGame.Concealment.PIE")
{
	TStealthPIENetworkComponent<FBasePIENetworkComponentState> Network{ TestRunner, TestCommandBuilder };
	inline static FString PreviousTimeout;
	inline static int32 PreviousNpcCount = -1;

	AStealthPlayerController* Host = nullptr;
	AStealthPlayerController* Client = nullptr;
	AStealthPawn* Blocker = nullptr;
	FBushRuntime Bush;
	uint32 Seq = 0;
	double Mark = 0.0;
	double MinEdgeGapCm = TNumericLimits<double>::Max();
	FVector StillAt = FVector::ZeroVector;
	double StillYaw = 0.0;
	double MaxStillDriftCm = 0.0;
	double MaxYawDriftDeg = 0.0;
	bool bKeyDown = false;

	BEFORE_ALL()
	{
		if (IConsoleVariable* Timeout = IConsoleManager::Get().FindConsoleVariable(CQTestConsoleVariables::NetworkTimeoutName))
		{
			PreviousTimeout = Timeout->GetString();
			Timeout->Set(StealthAssassinationPie::NetworkTimeoutCeilingSeconds, ECVF_SetByCode);
		}
		UStealthSettings* Settings = GetMutableDefault<UStealthSettings>();
		PreviousNpcCount = Settings->NpcCount;
		Settings->NpcCount = 0;
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
	}

	BEFORE_EACH()
	{
		FNetworkComponentBuilder<FBasePIENetworkComponentState>()
			.AsListenServer()
			.WithClients(1)
			.WithGameMode(AStealthGameMode::StaticClass())
			.Build(Network);
		Network.StartOnFixtureMap(StealthAssassinationPie::FixtureMap);
	}

	static void Place(APawn* Pawn, const FVector& Ground, double Yaw)
	{
		const ACharacter* Character = Cast<ACharacter>(Pawn);
		const double HalfHeight = Character != nullptr ? Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 86.0;
		if (UCharacterMovementComponent* Move = Character != nullptr ? Character->GetCharacterMovement() : nullptr)
		{
			Move->StopMovementImmediately();
		}
		Pawn->TeleportTo(FVector(Ground.X, Ground.Y, Ground.Z + HalfHeight + 2.0), FRotator(0.0, Yaw, 0.0), false, true);
		if (APlayerController* PC = Cast<APlayerController>(Pawn->GetController()))
		{
			PC->SetControlRotation(FRotator(0.0, Yaw, 0.0));
			if (AStealthPlayerController* Local = StealthAssassinationPie::OwnerMachineController(Cast<AStealthPlayerController>(PC)); Local != nullptr && Local != PC && Local->GetPawn() != nullptr)
			{
				Local->GetPawn()->TeleportTo(FVector(Ground.X, Ground.Y, Ground.Z + HalfHeight + 2.0), FRotator(0.0, Yaw, 0.0), false, true);
				Local->SetControlRotation(FRotator(0.0, Yaw, 0.0));
			}
		}
	}

	bool Ready()
	{
		using namespace StealthAssassinationPie;
		if (!RoundActive() || ServerPlayers().Num() != 2)
		{
			return false;
		}
		const UBushRuntimeSubsystem* Bushes = UBushRuntimeSubsystem::Get(ServerWorld());
		const UConcealmentService* Service = UConcealmentService::Get(ServerWorld());
		return Bushes != nullptr && Bushes->IsBuilt() && Bushes->GetBushes().Num() > 0 && Service != nullptr && Service->IsBound();
	}

	void Roles()
	{
		for (AStealthPlayerController* PC : StealthAssassinationPie::ServerPlayers())
		{
			(PC->IsLocalController() ? Host : Client) = PC;
		}
		Bush = UBushRuntimeSubsystem::Get(StealthAssassinationPie::ServerWorld())->GetBushes()[0];
	}

	/** The host's own input (the same Enhanced Input path a player's W key takes; ADR-0011 section 9). */
	void Key(const FKey& InKey, bool bDown) const
	{
		if (Host != nullptr)
		{
			Host->InputKey(FInputKeyEventArgs::CreateSimulated(InKey, bDown ? IE_Pressed : IE_Released, bDown ? 1.f : 0.f));
		}
	}

	TEST_METHOD(CollisionAndAvoidance)
	{
		using namespace StealthAssassinationPie;
		Network
			.Until(TEXT("ready"), [this]() { return Ready(); })
			.Then(TEXT("the host stands 2 m from the planter's +X edge, facing it"), [this]()
			{
				Roles();
				ASSERT_THAT(IsTrue(Host && Client, TEXT("the host and the client")));
				Place(Host->GetPawn(), Bush.Anchor + FVector(300.0, 0.0, 0.0), 180.0);
				Place(Client->GetPawn(), Bush.Anchor + FVector(0.0, 600.0, 0.0), -90.0);
				Mark = ServerNow(ServerWorld());
			})
			.Until(TEXT("settle"), [this]() { return ServerNow(ServerWorld()) >= Mark + 0.8; })
			.Until(TEXT("the host walks into the hide volume for 2 s"), [this]()
			{
				if (!bKeyDown)
				{
					Key(EKeys::W, true); // forward (toward the anchor)
					bKeyDown = true;
				}
				const FVector At = Host->GetPawn()->GetActorLocation();
				MinEdgeGapCm = FMath::Min(MinEdgeGapCm, ConcealmentGeometry::DistanceToFootprintCm(Bush.Footprint, At));
				return ServerNow(ServerWorld()) >= Mark + 2.8;
			})
			.Then(TEXT("blocked at the planter: the capsule centre never entered the footprint"), [this]()
			{
				Key(EKeys::W, false);
				bKeyDown = false;
				ASSERT_THAT(IsTrue(MinEdgeGapCm >= 20.0, *FString::Printf(TEXT("an Outside pawn is blocked by the hide volume (closest centre-to-edge %.1f cm, capsule radius 30)"), MinEdgeGapCm)));
				// Both stand inside entry reach of the bush (host +X, client +Y), then enter together.
				Place(Host->GetPawn(), Bush.Anchor + FVector(140.0, 0.0, 0.0), 180.0);
				Place(Client->GetPawn(), Bush.Anchor + FVector(0.0, 140.0, 0.0), -90.0);
				Mark = ServerNow(ServerWorld());
			})
			.Until(TEXT("settle at the entries"), [this]() { return ServerNow(ServerWorld()) >= Mark + 0.6; })
			.Then(TEXT("both enter the same bush in one frame"), [this]()
			{
				const uint16 BushId = Bush.Id;
				AsOwner(Cast<AStealthPawn>(Host->GetPawn()), Seq, [BushId](UPawnCommandComponent& Command) { Command.Hide(FHideCommand::Enter(BushId)); });
				AsOwner(Cast<AStealthPawn>(Client->GetPawn()), Seq, [BushId](UPawnCommandComponent& Command) { Command.Hide(FHideCommand::Enter(BushId)); });
				Mark = ServerNow(ServerWorld());
			})
			.Until(TEXT("both transitions end"), [this]() { return ServerNow(ServerWorld()) >= Mark + 1.2; })
			.Then(TEXT("no pawn-pawn or own-bush collision during the warp: both at the anchor, Concealed"), [this]()
			{
				const UConcealmentService* Service = UConcealmentService::Get(ServerWorld());
				for (const AStealthPlayerController* PC : { Host, Client })
				{
					const APawn* Pawn = PC->GetPawn();
					ASSERT_THAT(IsTrue(Service->GetState(Pawn) == EConcealState::Concealed, TEXT("Concealed (the commit guard found the capsule inside)")));
					ASSERT_THAT(IsTrue(FVector::Dist2D(Pawn->GetActorLocation(), Bush.Anchor) <= 5.0, *FString::Printf(TEXT("%s ended at the anchor (%.1f cm off)"), *Pawn->GetName(), FVector::Dist2D(Pawn->GetActorLocation(), Bush.Anchor))));
				}
				// A third, Outside pawn on the +X exit point: the host's exit by +X picks another open side.
				Blocker = SpawnUnboundPawn(ServerWorld(), Host->GetPawn(), 0.0);
				ASSERT_THAT(IsTrue(Blocker != nullptr, TEXT("the third pawn")));
				Place(Blocker, ConcealmentGeometry::ExitPoint(Bush, 0, 30.0), 0.0);
				Mark = ServerNow(ServerWorld());
			})
			.Until(TEXT("settle"), [this]() { return ServerNow(ServerWorld()) >= Mark + 0.3; })
			.Then(TEXT("the host exits toward the blocked side"), [this]()
			{
				AsOwner(Cast<AStealthPawn>(Host->GetPawn()), Seq, [](UPawnCommandComponent& Command) { Command.Hide(FHideCommand::Exit(0)); });
				Mark = ServerNow(ServerWorld());
			})
			.Until(TEXT("the exit ends"), [this]() { return ServerNow(ServerWorld()) >= Mark + 1.2; })
			.Then(TEXT("the Outside pawn still blocks: the exit took another side"), [this]()
			{
				const FVector At = Host->GetPawn()->GetActorLocation();
				ASSERT_THAT(IsTrue(FVector::Dist2D(At, ConcealmentGeometry::ExitPoint(Bush, 0, 30.0)) > 100.0, TEXT("not out by the blocked +X side")));
				ASSERT_THAT(IsTrue(ConcealmentGeometry::DistanceToFootprintCm(Bush.Footprint, At) > 0.0, TEXT("outside the footprint")));
				if (Blocker != nullptr)
				{
					Blocker->Destroy();
				}
			});
	}

	TEST_METHOD(InsideViewAndStillness)
	{
		using namespace StealthAssassinationPie;
		Network
			.Until(TEXT("ready"), [this]() { return Ready(); })
			.Then(TEXT("the host stands at the bush"), [this]()
			{
				Roles();
				Place(Host->GetPawn(), Bush.Anchor + FVector(140.0, 0.0, 0.0), 180.0);
				Place(Client->GetPawn(), Bush.Anchor + FVector(0.0, 700.0, 0.0), -90.0);
				Mark = ServerNow(ServerWorld());
			})
			.Until(TEXT("settle"), [this]() { return ServerNow(ServerWorld()) >= Mark + 0.6; })
			.Then(TEXT("enter"), [this]()
			{
				const uint16 BushId = Bush.Id;
				AsOwner(Cast<AStealthPawn>(Host->GetPawn()), Seq, [BushId](UPawnCommandComponent& Command) { Command.Hide(FHideCommand::Enter(BushId)); });
				Mark = ServerNow(ServerWorld());
			})
			.Until(TEXT("concealed and settled"), [this]() { return ServerNow(ServerWorld()) >= Mark + 1.3; })
			.Then(TEXT("record the still pose"), [this]()
			{
				ASSERT_THAT(IsTrue(UConcealmentService::Get(ServerWorld())->GetState(Host->GetPawn()) == EConcealState::Concealed, TEXT("Concealed")));
				StillAt = Host->GetPawn()->GetActorLocation();
				StillYaw = Host->GetPawn()->GetActorRotation().Yaw;
				Mark = ServerNow(ServerWorld());
			})
			.Until(TEXT("hold Move forward 2 s while sweeping Look"), [this]()
			{
				const double T = ServerNow(ServerWorld()) - Mark;
				if (!bKeyDown)
				{
					Key(EKeys::W, true);
					Key(EKeys::LeftShift, true);
					bKeyDown = true;
				}
				Host->SetControlRotation(FRotator(0.0, StillYaw + 90.0 * FMath::Sin(T * 3.0), 0.0));
				MaxStillDriftCm = FMath::Max(MaxStillDriftCm, FVector::Dist(Host->GetPawn()->GetActorLocation(), StillAt));
				MaxYawDriftDeg = FMath::Max(MaxYawDriftDeg, FMath::Abs(FRotator::NormalizeAxis(Host->GetPawn()->GetActorRotation().Yaw - StillYaw)));
				return T >= 2.0;
			})
			.Then(TEXT("no displacement, no body turn; Prepare and Interact refused without a break"), [this]()
			{
				Key(EKeys::W, false);
				Key(EKeys::LeftShift, false);
				bKeyDown = false;
				ASSERT_THAT(IsTrue(MaxStillDriftCm < 0.5, *FString::Printf(TEXT("server displacement %.2f cm (< 0.5)"), MaxStillDriftCm)));
				ASSERT_THAT(IsTrue(MaxYawDriftDeg < 0.5, *FString::Printf(TEXT("body yaw change %.2f deg (< 0.5)"), MaxYawDriftDeg)));
				AsOwner(Cast<AStealthPawn>(Host->GetPawn()), Seq, [](UPawnCommandComponent& Command) { Command.PrepareBegin(); });
				AsOwner(Cast<AStealthPawn>(Host->GetPawn()), Seq, [](UPawnCommandComponent& Command) { Command.InteractBegin(FCourtFixtureId(1)); });
				Mark = ServerNow(ServerWorld());
			})
			.Until(TEXT("the commands resolve"), [this]() { return ServerNow(ServerWorld()) >= Mark + 0.5; })
			.Then(TEXT("still Concealed, no Brace pose"), [this]()
			{
				const AStealthPawn* Pawn = Cast<AStealthPawn>(Host->GetPawn());
				ASSERT_THAT(IsTrue(UConcealmentService::Get(ServerWorld())->GetState(Pawn) == EConcealState::Concealed, TEXT("neither Prepare nor Interact is a break")));
				ASSERT_THAT(IsTrue(Pawn->GetActionState().Pose != EActionPose::Brace && Pawn->GetActionState().Pose != EActionPose::Interact, TEXT("no brace and no tend from inside")));
				AsOwner(Cast<AStealthPawn>(Host->GetPawn()), Seq, [](UPawnCommandComponent& Command) { Command.PrepareEnd(); });
			});
	}
};

#endif
