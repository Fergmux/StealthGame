// Concealment story con-012 (production/epics/concealment-bushes/story-012-owner-warp-no-snap.md; GDD AC-CON-31; ADR-0017
// section 4 amendment "S-CON-1 resolution 2026-10-28", mechanism (b) PredictOnPress), the per-commit form of the spike's
// measurement (production/qa/spikes/2026-10-28_S-CON-1.md ran 100 hides and 100 exits per mechanism; this gate runs 20 of
// each with the production mechanism): listen host + 1 client, StealthLeak2 (PktLag 100 ms, PktLoss 2 %) on every driver,
// L_Prototype, no crowd. The client's owner hides and leaves through the production path (UPawnCommandClient::PressHide ->
// ServerPawnCommand -> Round's queue -> UConcealmentService's FRootMotionSource_MoveToForce; the owner starts the identical
// source at the press, corrections suppressed for the transition plus one round trip, then one normal correction).
//   StealthGame.Concealment.PIE.OwnerWarpNoSnap.HideAndExitStartWithin50msWithoutSnap: every transition starts moving the
//   owner (>= 1 cm) within 50 ms of the press, no correction above 5 cm lands during it, and the server reaches the state.
// AC2 (the recorded mechanism's values): the CVar Stealth.Concealment.OwnerWarp is 2 (PredictOnPress) by default and the
// server source is named ConcealEnter / ConcealExit for NPC and player pawns alike (one path, ConcealmentService::StartWarp);
// bServerAcceptClientAuthoritativePosition is false on the pawn's movement component (asserted).

#include "CoreMinimal.h"
#include "Tests/PIE/StealthPIENetworkComponent.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS && defined(ENABLE_PIE_NETWORK_TEST) && ENABLE_PIE_NETWORK_TEST

#include "CQTest.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Server/ConcealmentService.h"
#include "StealthGameMode.h"
#include "StealthGameState.h"
#include "StealthPlayerController.h"
#include "StealthSettings.h"
#include "View/BushRuntimeSubsystem.h"
#include "View/ConcealmentComponent.h"
#include "View/Core/StealthPawn.h"
#include "View/Core/StealthPawnMovementComponent.h"
#include "View/Input/PawnCommandClient.h"

namespace OwnerWarpNoSnapPie
{
	const TCHAR* const FixtureMap = TEXT("/Game/StealthGame/Maps/L_Prototype");
	const TCHAR* const Profile = TEXT("StealthLeak2");
	const TCHAR* const CeilingSeconds = TEXT("600");
	/** Transitions per direction (the spike ran AC-CON-31's 100 + 100; the per-commit gate runs 20 + 20). */
	constexpr int32 CyclesPerMode = 20;
	constexpr double MotionCm = 1.0;
	constexpr double SnapCm = 5.0;
	constexpr double MaxPressToMotionMs = 50.0;
	/** Per transition: observe this long after the press (transition 0.9 s + RTT + margin). */
	constexpr double ObserveSec = 1.6;
	/** Rest between transitions (the previous one's final correction lands). */
	constexpr double RestSec = 0.5;

	struct FSample
	{
		int32 Mode = 0;
		bool bEnter = true;
		double PressToMotionMs = -1.0;
		int32 Corrections = 0;
		double MaxCorrectionCm = 0.0;
		double EndGapCm = 0.0;
		bool bServerFollowed = false;
	};
}

NETWORK_TEST_CLASS(OwnerWarpNoSnap, "StealthGame.Concealment.PIE")
{
	TStealthPIENetworkComponent<FBasePIENetworkComponentState> Network{ TestRunner, TestCommandBuilder };
	FPacketSimulationSettings PacketSettings;
	inline static FString PreviousTimeout;
	inline static int32 PreviousNpcCount = -1;
	inline static int32 PreviousOwnerWarp = 2;

	// The cycle state machine.
	int32 Mode = 2;
	int32 Cycle = 0;
	bool bEnterNext = true;
	enum class EStep : uint8 { Press, Observe, Rest, Done } Step = EStep::Press;
	double PressWall = 0.0;
	double PressServer = 0.0;
	FVector PressLocation = FVector::ZeroVector;
	OwnerWarpNoSnapPie::FSample Current;
	TArray<OwnerWarpNoSnapPie::FSample> Samples;
	double RestUntil = 0.0;

	BEFORE_ALL()
	{
		if (IConsoleVariable* Timeout = IConsoleManager::Get().FindConsoleVariable(CQTestConsoleVariables::NetworkTimeoutName))
		{
			PreviousTimeout = Timeout->GetString();
			Timeout->Set(OwnerWarpNoSnapPie::CeilingSeconds, ECVF_SetByCode);
		}
		UStealthSettings* Settings = GetMutableDefault<UStealthSettings>();
		PreviousNpcCount = Settings->NpcCount;
		Settings->NpcCount = 0;
		if (IConsoleVariable* Warp = IConsoleManager::Get().FindConsoleVariable(TEXT("Stealth.Concealment.OwnerWarp")))
		{
			PreviousOwnerWarp = Warp->GetInt();
		}
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
		if (IConsoleVariable* Warp = IConsoleManager::Get().FindConsoleVariable(TEXT("Stealth.Concealment.OwnerWarp")))
		{
			Warp->Set(PreviousOwnerWarp, ECVF_SetByCode);
		}
	}

	BEFORE_EACH()
	{
		PacketSettings = FPacketSimulationSettings();
		ASSERT_THAT(IsTrue(PacketSettings.LoadEmulationProfile(OwnerWarpNoSnapPie::Profile), TEXT("StealthLeak2 loads by name")));
		FNetworkComponentBuilder<FBasePIENetworkComponentState>()
			.AsListenServer()
			.WithClients(1)
			.WithGameMode(AStealthGameMode::StaticClass())
			.WithPacketSimulationSettings(&PacketSettings)
			.Build(Network);
		Network.StartOnFixtureMap(OwnerWarpNoSnapPie::FixtureMap);
	}

	static UWorld* WorldOf(ENetMode NetMode)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE && Context.World() && Context.World()->GetNetMode() == NetMode)
			{
				return Context.World();
			}
		}
		return nullptr;
	}

	static AStealthPlayerController* ClientController() { UWorld* W = WorldOf(NM_Client); return W ? Cast<AStealthPlayerController>(W->GetFirstPlayerController()) : nullptr; }
	static ACharacter* OwnerPawn() { APlayerController* PC = ClientController(); return PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr; }

	static ACharacter* ServerCopy()
	{
		UWorld* Server = WorldOf(NM_ListenServer);
		const APlayerController* Client = ClientController();
		const APlayerState* ClientState = Client ? Client->PlayerState.Get() : nullptr;
		if (!Server || !ClientState)
		{
			return nullptr;
		}
		for (FConstPlayerControllerIterator It = Server->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* PC = It->Get();
			if (PC && !PC->IsLocalController() && PC->PlayerState && PC->PlayerState->GetPlayerId() == ClientState->GetPlayerId())
			{
				return Cast<ACharacter>(PC->GetPawn());
			}
		}
		return nullptr;
	}

	static double ServerNow(const UWorld* World)
	{
		const AGameStateBase* State = World ? World->GetGameState() : nullptr;
		return State ? State->GetServerWorldTimeSeconds() : 0.0;
	}

	/** The bush nearest the server copy and the entry point 40 cm out of its nearest open side. */
	static bool PickBush(FBushRuntime& OutBush, FVector& OutEntry)
	{
		const UBushRuntimeSubsystem* Bushes = UBushRuntimeSubsystem::Get(WorldOf(NM_ListenServer));
		const ACharacter* Pawn = ServerCopy();
		if (!Bushes || !Pawn || Bushes->GetBushes().Num() == 0)
		{
			return false;
		}
		double Best = TNumericLimits<double>::Max();
		for (const FBushRuntime& Bush : Bushes->GetBushes())
		{
			const double D = FVector::Dist2D(Bush.Anchor, Pawn->GetActorLocation());
			if (D < Best)
			{
				Best = D;
				OutBush = Bush;
			}
		}
		const uint8 Side = ConcealmentGeometry::NearestOpenSide(OutBush.EntryAxisMask, ConcealmentGeometry::YawFromAnchor(OutBush, Pawn->GetActorLocation()));
		OutEntry = OutBush.Anchor + ConcealmentGeometry::SideNormal(Side) * (100.0 + 40.0); // 40 cm out of the planter edge: inside BushEntryReachCm 60
		return true;
	}

	static void SetMode(int32 InMode)
	{
		if (IConsoleVariable* Warp = IConsoleManager::Get().FindConsoleVariable(TEXT("Stealth.Concealment.OwnerWarp")))
		{
			Warp->Set(InMode, ECVF_SetByCode);
		}
	}

	int32 CyclesOf(int32) const { return OwnerWarpNoSnapPie::CyclesPerMode; }

	/** One frame of the measurement; true when every mode ran. */
	bool StepFrame()
	{
		ACharacter* Owner = OwnerPawn();
		ACharacter* Server = ServerCopy();
		AStealthPlayerController* PC = ClientController();
		if (!Owner || !Server || !PC || !PC->GetCommandClient())
		{
			return false;
		}
		UStealthMovementComponent* OwnerMove = Cast<UStealthMovementComponent>(Owner->GetCharacterMovement());
		const double NowWall = FPlatformTime::Seconds();
		switch (Step)
		{
		case EStep::Press:
		{
			SetMode(Mode);
			Current = OwnerWarpNoSnapPie::FSample();
			Current.Mode = Mode;
			Current.bEnter = bEnterNext;
			PressWall = NowWall;
			PressServer = ServerNow(Owner->GetWorld());
			PressLocation = Owner->GetActorLocation();
			if (OwnerMove)
			{
				OwnerMove->ResetClientCorrectionStats();
			}
			// Face the bush's open side on exits the way a player looks out (the exit side is the look yaw's).
			PC->GetCommandClient()->PressHide();
			Step = EStep::Observe;
			return false;
		}
		case EStep::Observe:
		{
			if (Current.PressToMotionMs < 0.0 && FVector::Dist(Owner->GetActorLocation(), PressLocation) >= OwnerWarpNoSnapPie::MotionCm)
			{
				Current.PressToMotionMs = (NowWall - PressWall) * 1000.0;
			}
			if (NowWall - PressWall < OwnerWarpNoSnapPie::ObserveSec)
			{
				return false;
			}
			Current.Corrections = OwnerMove ? OwnerMove->GetClientCorrectionCount() : 0;
			Current.MaxCorrectionCm = OwnerMove ? OwnerMove->GetMaxClientCorrectionCm() : 0.0;
			Current.EndGapCm = FVector::Dist(Owner->GetActorLocation(), Server->GetActorLocation());
			const UConcealmentService* Service = UConcealmentService::Get(Server->GetWorld());
			const EConcealState State = Service ? Service->GetState(Server) : EConcealState::Outside;
			Current.bServerFollowed = bEnterNext ? State == EConcealState::Concealed : State == EConcealState::Outside;
			Samples.Add(Current);
			RestUntil = NowWall + OwnerWarpNoSnapPie::RestSec;
			Step = EStep::Rest;
			return false;
		}
		case EStep::Rest:
		{
			if (NowWall < RestUntil)
			{
				return false;
			}
			if (!bEnterNext)
			{
				++Cycle;
			}
			bEnterNext = !bEnterNext;
			if (Cycle >= CyclesOf(Mode))
			{
				Cycle = 0;
				++Mode;
			}
			Step = Mode > 2 ? EStep::Done : EStep::Press; // the production mechanism only
			return Step == EStep::Done;
		}
		default:
			return true;
		}
	}

	void Assess()
	{
		int32 Moved = 0, Followed = 0;
		double WorstMotion = 0.0, WorstCorrection = 0.0;
		for (const OwnerWarpNoSnapPie::FSample& S : Samples)
		{
			Moved += S.PressToMotionMs >= 0.0 ? 1 : 0;
			Followed += S.bServerFollowed ? 1 : 0;
			WorstMotion = FMath::Max(WorstMotion, S.PressToMotionMs);
			WorstCorrection = FMath::Max(WorstCorrection, S.MaxCorrectionCm);
		}
		const FString Line = FString::Printf(TEXT("OwnerWarpNoSnap: %d transitions, moved %d, server followed %d, worst press-to-motion %.1f ms, worst correction %.2f cm"),
			Samples.Num(), Moved, Followed, WorstMotion, WorstCorrection);
		TestRunner->AddInfo(Line);
		UE_LOG(LogTemp, Display, TEXT("%s"), *Line);
		ASSERT_THAT(AreEqual(Samples.Num(), 2 * OwnerWarpNoSnapPie::CyclesPerMode, TEXT("20 hides and 20 exits measured")));
		ASSERT_THAT(AreEqual(Followed, Samples.Num(), TEXT("the server reached the state of every transition")));
		ASSERT_THAT(AreEqual(Moved, Samples.Num(), TEXT("the owner moved on every transition")));
		ASSERT_THAT(IsTrue(WorstMotion <= OwnerWarpNoSnapPie::MaxPressToMotionMs, TEXT("press-to-motion <= 50 ms on every transition")));
		ASSERT_THAT(IsTrue(WorstCorrection <= OwnerWarpNoSnapPie::SnapCm, TEXT("no correction above 5 cm during any transition")));
	}

	TEST_METHOD(HideAndExitStartWithin50msWithoutSnap)
	{
		Network
			.Until(TEXT("the client owns its pawn in Active, the bushes are built on both machines"), []()
			{
				const APlayerController* PC = ClientController();
				const AStealthGameState* State = PC && PC->GetWorld() ? PC->GetWorld()->GetGameState<AStealthGameState>() : nullptr;
				const UBushRuntimeSubsystem* ClientBushes = UBushRuntimeSubsystem::Get(WorldOf(NM_Client));
				const UBushRuntimeSubsystem* ServerBushes = UBushRuntimeSubsystem::Get(WorldOf(NM_ListenServer));
				const UConcealmentService* Service = UConcealmentService::Get(WorldOf(NM_ListenServer));
				return PC && Cast<AStealthPawn>(PC->GetPawn()) && ServerCopy() && State && State->GetRoundPublicState().Phase == ERoundPhase::Active
					&& ClientBushes && ClientBushes->IsBuilt() && ServerBushes && ServerBushes->IsBuilt() && Service && Service->IsBound();
			})
			.Then(TEXT("lift the re-entry cooldown and stand the owner at a bush"), [this]()
			{
				UConcealmentService* Service = UConcealmentService::Get(WorldOf(NM_ListenServer));
				ASSERT_THAT(IsTrue(Service && Service->GetCore(), TEXT("the server's Concealment core")));
				ASSERT_THAT(AreEqual(static_cast<int32>(UConcealmentComponent::GetOwnerWarpMode()), static_cast<int32>(EConcealOwnerWarp::PredictOnPress), TEXT("the recorded mechanism (b) PredictOnPress is the default")));
				ASSERT_THAT(IsFalse(ServerCopy()->GetCharacterMovement()->bServerAcceptClientAuthoritativePosition, TEXT("bServerAcceptClientAuthoritativePosition is false")));
				FConcealmentParams Params = Service->GetCore()->GetParams();
				Params.Server.BushReenterCooldownSec = 0.0f;
				Service->GetCore()->SetParamsForTest(Params);
				FBushRuntime Bush;
				FVector Entry;
				ASSERT_THAT(IsTrue(PickBush(Bush, Entry), TEXT("a bush near the client's spawn")));
				ACharacter* Server = ServerCopy();
				const double HalfHeight = Server->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
				const FRotator Facing = (Bush.Anchor - Entry).GetSafeNormal2D().Rotation();
				Server->GetCharacterMovement()->StopMovementImmediately();
				Server->TeleportTo(FVector(Entry.X, Entry.Y, Bush.Anchor.Z + HalfHeight + 2.0), Facing, false, true);
				if (ACharacter* Owner = OwnerPawn())
				{
					Owner->GetCharacterMovement()->StopMovementImmediately();
					Owner->TeleportTo(FVector(Entry.X, Entry.Y, Bush.Anchor.Z + HalfHeight + 2.0), Facing, false, true);
				}
				if (APlayerController* PC = ClientController())
				{
					PC->SetControlRotation(FRotator(0.0, Facing.Yaw + 180.0, 0.0)); // looking back out of the entry side
				}
			})
			.Until(TEXT("settle"), [this]() { RestUntil = RestUntil <= 0.0 ? FPlatformTime::Seconds() + 2.0 : RestUntil; return FPlatformTime::Seconds() >= RestUntil; })
			.Then(TEXT("start"), [this]() { RestUntil = 0.0; Step = EStep::Press; })
			.Until(TEXT("every mechanism measured (hides and exits)"), [this]() { return StepFrame(); })
			.Then(TEXT("report"), [this]()
			{
				Assess();
			});
	}
};

#endif
