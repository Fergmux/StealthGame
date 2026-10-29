// SPIKE (measurement, not a gate): Concealment story con-010, spike S-CON-1 (production/epics/concealment-bushes/
// story-010-spike-s-con-1-owner-side-warp.md; ADR-0017 section 4; GDD AC-CON-31, OQ8). Report:
// production/qa/spikes/2026-10-28_S-CON-1.md.
//
// A CQTest in-process PIE session (listen host + 1 client, AStealthGameMode, L_Prototype, no crowd) with the engine
// packet-simulation profile StealthLeak2 (PktLag 100 ms, PktLoss 2 %; -SCON1Profile=StealthLeak1 for 1 %) on every driver (the measured RTT is ~226 ms). The
// client's owner hides in a bush and leaves it through the production path (UPawnCommandClient::PressHide -> the reliable
// ServerPawnCommand -> Round's queue -> UConcealmentService -> the server's FRootMotionSource_MoveToForce), the same server
// source for NPC and player pawns. Each owner-side mechanism of ADR-0017 section 4 runs N hides and N exits:
//   0 ServerOnly       the baseline (the owner follows corrections)
//   1 OnStateArrival   (a1) the owner starts the identical source when its FActionState arrives
//   2 PredictOnPress   (b) the owner starts the identical source at the press; server and owner suppress corrections for
//                      the transition plus one round trip, then one normal correction to the fixed end point
// Per transition the owner records press-to-motion (the first owner frame >= 1 cm from the press position, wall clock) and
// every correction it received during the transition (count and the largest |server - owner|), plus the end distance
// between the owner and the server copy. The (a2) owner-only start message is not built: it is tried last by the ADR and
// would add an owner wire element; (a1) and (b) bound it (a2 is a1 plus nothing, its press-to-motion is the same RTT).
// The re-entry cooldown is lifted for the session (a test seam) so the cycles run back to back; everything else is the
// production default.
//
// Named StealthSpike.* on purpose: a several-minute measurement, never part of the per-commit `RunTests StealthGame.` gate.
//   UnrealEditor-Cmd <uproject> -ExecCmds="Automation RunTests StealthSpike.Concealment; Quit" -unattended -nullrhi
// Records: Saved/Harness/SCON1/scon1.jsonl; the summary lines start with "SCON1".

#include "CoreMinimal.h"
#include "Tests/PIE/StealthPIENetworkComponent.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS && defined(ENABLE_PIE_NETWORK_TEST) && ENABLE_PIE_NETWORK_TEST

#include "CQTest.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerState.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
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

namespace SCON1
{
	const TCHAR* const FixtureMap = TEXT("/Game/StealthGame/Maps/L_Prototype");
	const TCHAR* const Profile = TEXT("StealthLeak2");
	const TCHAR* const CeilingSeconds = TEXT("3000");
	/** Transitions per direction per mechanism (AC-CON-31: 100 hides and 100 exits). */
	constexpr int32 CyclesPerMode = 100;
	/** Fewer for the baseline (it only anchors the comparison). */
	constexpr int32 BaselineCycles = 20;
	constexpr double MotionCm = 1.0;
	constexpr double SnapCm = 5.0;
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

NETWORK_TEST_CLASS(ConcealmentOwnerWarp, "StealthSpike.Concealment")
{
	TStealthPIENetworkComponent<FBasePIENetworkComponentState> Network{ TestRunner, TestCommandBuilder };
	FPacketSimulationSettings PacketSettings;
	inline static FString PreviousTimeout;
	inline static int32 PreviousNpcCount = -1;
	inline static int32 PreviousOwnerWarp = 2;

	// The cycle state machine.
	int32 Mode = 0;
	int32 Cycle = 0;
	bool bEnterNext = true;
	enum class EStep : uint8 { Press, Observe, Rest, Done } Step = EStep::Press;
	double PressWall = 0.0;
	double PressServer = 0.0;
	FVector PressLocation = FVector::ZeroVector;
	SCON1::FSample Current;
	TArray<SCON1::FSample> Samples;
	double RestUntil = 0.0;

	BEFORE_ALL()
	{
		if (IConsoleVariable* Timeout = IConsoleManager::Get().FindConsoleVariable(CQTestConsoleVariables::NetworkTimeoutName))
		{
			PreviousTimeout = Timeout->GetString();
			Timeout->Set(SCON1::CeilingSeconds, ECVF_SetByCode);
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
		// -SCON1Profile=<name> picks the packet-simulation profile (default StealthLeak2: PktLag 100 ms, PktLoss 2 %, AC-CON-31).
		FString ProfileName = SCON1::Profile;
		FParse::Value(FCommandLine::Get(), TEXT("SCON1Profile="), ProfileName);
		ASSERT_THAT(IsTrue(PacketSettings.LoadEmulationProfile(*ProfileName), TEXT("the packet-simulation profile loads by name")));
		UE_LOG(LogTemp, Display, TEXT("SCON1 profile %s"), *ProfileName);
		FNetworkComponentBuilder<FBasePIENetworkComponentState>()
			.AsListenServer()
			.WithClients(1)
			.WithGameMode(AStealthGameMode::StaticClass())
			.WithPacketSimulationSettings(&PacketSettings)
			.Build(Network);
		Network.StartOnFixtureMap(SCON1::FixtureMap);
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

	int32 CyclesOf(int32 InMode) const { return InMode == 0 ? SCON1::BaselineCycles : SCON1::CyclesPerMode; }

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
			Current = SCON1::FSample();
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
			if (Current.PressToMotionMs < 0.0 && FVector::Dist(Owner->GetActorLocation(), PressLocation) >= SCON1::MotionCm)
			{
				Current.PressToMotionMs = (NowWall - PressWall) * 1000.0;
			}
			if (NowWall - PressWall < SCON1::ObserveSec)
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
			RestUntil = NowWall + SCON1::RestSec;
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
			Step = Mode > 2 ? EStep::Done : EStep::Press;
			return Step == EStep::Done;
		}
		default:
			return true;
		}
	}

	static double Percentile(TArray<double> Values, double P)
	{
		if (Values.Num() == 0)
		{
			return -1.0;
		}
		Values.Sort();
		const int32 Index = FMath::Clamp(FMath::CeilToInt(P * Values.Num()) - 1, 0, Values.Num() - 1);
		return Values[Index];
	}

	void Report()
	{
		const FString Dir = FPaths::ProjectSavedDir() / TEXT("Harness/SCON1");
		IFileManager::Get().MakeDirectory(*Dir, true);
		FString Lines;
		for (const SCON1::FSample& S : Samples)
		{
			Lines += FString::Printf(TEXT("{\"mode\":%d,\"enter\":%d,\"pressToMotionMs\":%.2f,\"corrections\":%d,\"maxCorrectionCm\":%.2f,\"endGapCm\":%.2f,\"serverFollowed\":%d}\n"),
				S.Mode, S.bEnter ? 1 : 0, S.PressToMotionMs, S.Corrections, S.MaxCorrectionCm, S.EndGapCm, S.bServerFollowed ? 1 : 0);
		}
		FFileHelper::SaveStringToFile(Lines, *(Dir / TEXT("scon1.jsonl")));
		for (int32 M = 0; M <= 2; ++M)
		{
			for (const bool bEnter : { true, false })
			{
				TArray<double> Motion;
				int32 Snaps = 0, NoMotion = 0, Followed = 0, Count = 0;
				double MaxSnap = 0.0, MaxGap = 0.0;
				for (const SCON1::FSample& S : Samples)
				{
					if (S.Mode != M || S.bEnter != bEnter)
					{
						continue;
					}
					++Count;
					if (S.PressToMotionMs >= 0.0)
					{
						Motion.Add(S.PressToMotionMs);
					}
					else
					{
						++NoMotion;
					}
					Snaps += S.MaxCorrectionCm > SCON1::SnapCm ? 1 : 0;
					MaxSnap = FMath::Max(MaxSnap, S.MaxCorrectionCm);
					MaxGap = FMath::Max(MaxGap, S.EndGapCm);
					Followed += S.bServerFollowed ? 1 : 0;
				}
				const FString Line = FString::Printf(TEXT("SCON1 mode %d %s: n %d, press-to-motion p50 %.1f p95 %.1f max %.1f ms (no motion %d), transitions with a correction > 5 cm %d (max %.1f cm), end gap max %.1f cm, server reached the state %d/%d"),
					M, bEnter ? TEXT("hide") : TEXT("exit"), Count, Percentile(Motion, 0.5), Percentile(Motion, 0.95), Percentile(Motion, 1.0), NoMotion, Snaps, MaxSnap, MaxGap, Followed, Count);
				TestRunner->AddInfo(Line);
				UE_LOG(LogTemp, Display, TEXT("%s"), *Line);
			}
		}
	}

	TEST_METHOD(MeasureEveryMechanism)
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
				Report();
				ASSERT_THAT(IsTrue(Samples.Num() > 0, TEXT("the session measured transitions")));
			});
	}
};

#endif
