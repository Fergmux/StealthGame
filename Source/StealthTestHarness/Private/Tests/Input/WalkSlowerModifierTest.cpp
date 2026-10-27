// Story iif-018 (production/epics/input-interaction-framework/story-018-walk-slower-two-client.md), GDD AC 76 server
// clause (R15a, Integration, P WalkSlowerModifier): GIVEN a keyboard walker holding W (turning slowly, so it walks a circle in the open street) WHEN it holds Left Ctrl, then
// Left Alt, then both, then neither, for 3 s each THEN the server-received Move magnitude is 0.8, 0.9, 0.8 and 1.0, the
// pace follows, every speed change follows the baseline acceleration or braking (no step above it) and no RPC other
// than ServerMove is sent; AND holding Run with Left Ctrl gives run speed.
// CQTest: NETWORK_TEST_CLASS(WalkSlowerModifier, "StealthGame.Input.PIE"), listen server + 2 clients (the AC's two PIE
// clients; client 1 walks, client 2 is the second connection), no net emulation. Keys are injected on client 1's
// controller (simulated key events through Enhanced Input, as spike V1).
// What the server measures: the received acceleration magnitude of client 1's pawn over the CMC's max acceleration
// (ServerMove carries the acceleration; the CMC clamps it to 1), and the steady-state 2D speed. Recorded deviations (see
// the story's Completion Notes): since SCL-010 the absolute paces 112 / 126 / 140 cm/s are asserted (WalkSpeedCmPerSec
// 140 and the pace quantiser), the run phase at RunSpeedCmPerSec 450 and with no RPC beyond the move family (the sample
// sprint shim and its UpdateInputState_Server are gone); "baseline acceleration" is the movement model's acceleration
// and braking bound (max acceleration plus max braking deceleration) on the owner's per-frame simulation; the informant
// clause uses stub state (the prototype owner state Informant plus IMC_Informant on the owner; the Roster-driven swap is
// story iif-071) and repeats the four modifier phases; "only ServerMove" allows the engine's
// ServerUpdateCamera background (ADR-0015 disables client camera updates later).
// Determinism (fix 2026-10-26): the round seed and the spawn slot come from the session's OS entropy, so every run walks a
// different city from a different spawn; the old open-loop circle (a fixed yaw per frame) also drifted with the frame
// time and, two runs in three, walked into a wall (speed 0, then a collision step over the limit). The walker now starts
// on a clear circle the test finds on the server (a box overlap over its whole radius, floor traces round its rim) and is
// steered closed-loop round that circle's centre, so no layout, spawn slot or frame time reaches the measurement.

#include "CoreMinimal.h"
#include "Tests/PIE/StealthPIENetworkComponent.h"
#include "Tests/PIE/StealthPIEOpenCircle.h"

#if WITH_DEV_AUTOMATION_TESTS && defined(ENABLE_PIE_NETWORK_TEST) && ENABLE_PIE_NETWORK_TEST

#include "CQTest.h"
#include "CollisionQueryParams.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "WorldCollision.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "Input/RpcSpy.h"
#include "InputKeyEventArgs.h"
#include "StealthGameMode.h"
#include "StealthPlayerController.h"
#include "StealthGameState.h"
#include "StealthPlayerState.h"
#include "StealthSettings.h"
#include "View/Input/PawnCommandClient.h"
#include "View/Core/StealthPawn.h"
#include "View/Core/WalkSlowerScale.h"

namespace WalkSlowerModifierTestData
{
	const TCHAR* const FixtureMap = TEXT("/Game/StealthGame/Maps/L_Prototype");
	const TCHAR* const NetworkTimeoutCeilingSeconds = TEXT("300");
	/** 3 s per phase at the PIE frame rate is measured in frames, never wall time: about 3 s at 60 Hz. */
	constexpr uint64 PhaseFrames = 180;
	/** The steady-state window at the end of each phase. */
	constexpr uint64 SampleFrames = 60;
	constexpr double MagnitudeTolerance = 0.03;
	constexpr double PaceRatioTolerance = 0.05;
	/** Steady-state speed tolerance of the absolute paces (the walker turns a slow circle). */
	constexpr double AbsolutePaceToleranceCm = 3.0;

	struct FPhase
	{
		const TCHAR* Name;
		bool bCtrl;
		bool bAlt;
		bool bRun;
		double ExpectedMagnitude;
		/** The walker is an informant (prototype owner state + IMC_Informant, which keeps IMC_Common's walk-slower). */
		bool bInformant = false;
		/**
		 * The phase starts from a stop the test forced (not a modifier change) right after the sample's sprint: neither its
		 * transition nor its RPCs (the sample's own UpdateInputState_Server on the Shift release) are checked.
		 */
		bool bSkipStepCheck = false;
	};
	const FPhase Phases[] = {
		{ TEXT("neither (reference)"), false, false, false, StealthPaceSet::Full },
		{ TEXT("Left Ctrl"), true, false, false, StealthPaceSet::Slow },
		{ TEXT("Left Alt"), false, true, false, StealthPaceSet::Middle },
		{ TEXT("both"), true, true, false, StealthPaceSet::Slow },
		{ TEXT("neither"), false, false, false, StealthPaceSet::Full },
		{ TEXT("Run + Left Ctrl"), true, false, true, StealthPaceSet::Full },
		// The first informant phase also absorbs the stop after the sample's sprint and the swap (no step check there).
		{ TEXT("informant: neither (settle)"), false, false, false, StealthPaceSet::Full, true, true },
		{ TEXT("informant: Left Ctrl"), true, false, false, StealthPaceSet::Slow, true },
		{ TEXT("informant: Left Alt"), false, true, false, StealthPaceSet::Middle, true },
		{ TEXT("informant: both"), true, true, false, StealthPaceSet::Slow, true },
	};

	/**
	 * The walker orbits a fixed centre at this radius: 27 deg/s at the 140 cm/s walk (inside WalkTurnRateDegPerSec 60)
	 * and 86 deg/s at the 450 cm/s run (inside RunTurnRateDegPerSec 180).
	 */
	constexpr double CircleRadiusCm = 300.0;
	/** Clearance beyond the circle (capsule radius plus the overshoot of a pace change). */
	constexpr double CircleMarginCm = 120.0;
	/** The radial error (cm) at which the steering points straight back at the circle. */
	constexpr double CircleCorrectionCm = 100.0;
	/** The open-circle search: rings of candidate centres round the spawn, this far apart, out to this distance. */
	constexpr double SearchStepCm = 200.0;
	constexpr double SearchRadiusCm = 6000.0;
	/** The clear box starts this far above the floor (kerb-free ground) and is this tall (a standing pawn). */
	constexpr double BoxFloorGapCm = 5.0;
	constexpr double BoxHeightCm = 190.0;
	/** Rim floor traces must land within this of the centre's floor (a flat disc, no ledge or step). */
	constexpr double FloorFlatnessCm = 4.0;
	constexpr int32 RimTraces = 12;
	/** The owner has arrived at the start point when it is this close and this slow. */
	constexpr double ArriveDistanceCm = 30.0;
	constexpr double ArriveSpeedCm = 5.0;
	/** Frames the walker stands on the circle before the first phase (any correction lands inside them). */
	constexpr uint64 SettleFrames = 60;
	/** The unchecked warm-up walk after the teleport (frames). */
	constexpr uint64 WarmUpWalkFrames = 90;
	/** Owner and server copies agree within this before the first phase (cm; below the CMC's correction threshold). */
	constexpr double AgreeDistanceCm = 10.0;
	/** The largest control-yaw step per frame of the orbit steering (36 deg/s at 60 Hz, inside WalkTurnRateDegPerSec 60). */
	constexpr double MaxYawStepDeg = 0.6;
	/** Both copies at rest below this speed (cm/s) end the warm-up. */
	constexpr double RestSpeedCm = 0.1;
	/** The agreement wait gives up after this many frames (it then logs the distance and goes on). */
	constexpr uint64 AgreeMaxFrames = 600;
	/** The no-step clause is checked in the transition window after each modifier change. */
	constexpr uint64 TransitionFrames = 40;

	struct FSample
	{
		double TargetSum = 0.0;
		/** Client-to-server RPCs of this phase beyond the engine move/camera background. */
		FString OtherRpcs;
		double MagnitudeSum = 0.0;
		double SpeedSum = 0.0;
		int32 Count = 0;
		double MaxStepOver = 0.0;
	/** Where the largest step happened (frame after the phase start) and its speeds, for the log. */
	uint64 StepFrame = 0;
	double StepFrom = 0.0;
	double StepTo = 0.0;
	};
}

NETWORK_TEST_CLASS(WalkSlowerModifier, "StealthGame.Input.PIE")
{
	TStealthPIENetworkComponent<FBasePIENetworkComponentState> Network{ TestRunner, TestCommandBuilder };
	/** CQTest keeps the description pointer: the label strings must outlive the command queue. */
	TArray<FString> Labels;
	const TCHAR* L(FString Label) { return *Labels.Add_GetRef(MoveTemp(Label)); }
	inline static FString PreviousNetworkTimeout;
	inline static int32 PreviousNpcCount = -1;

	TUniquePtr<FOutgoingRpcSpy> Spy;
	TArray<WalkSlowerModifierTestData::FSample> Samples;
	uint64 PhaseStart = 0;
	double LastSpeed = -1.0;
	double LastTime = 0.0;
	int32 ClientIndexSeen = 0;
	bool bInformantSet = false;
	uint64 Mark = 0;
	/** Consecutive frames both copies were at rest and in agreement (the warm-up end). */
	uint64 RestFrames = 0;
	/** The orbit centre (world space; the same layout on every PIE world, RoundSeed replicates). */
	FVector CircleCentre = FVector::ZeroVector;
	FVector CircleStart = FVector::ZeroVector;

	BEFORE_ALL()
	{
		if (IConsoleVariable* Timeout = IConsoleManager::Get().FindConsoleVariable(CQTestConsoleVariables::NetworkTimeoutName))
		{
			PreviousNetworkTimeout = Timeout->GetString();
			Timeout->Set(WalkSlowerModifierTestData::NetworkTimeoutCeilingSeconds, ECVF_SetByCode);
		}
		// No crowd: since SCL-018 the crowd walks through Move like a player, and a crowd pawn crossing the walker's circle
		// blocked it (a flaky 'both' phase, 2026-10-26); the measured paces need an open street (spike SCL-045 pattern).
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
		FNetworkComponentBuilder<FBasePIENetworkComponentState>()
			.AsListenServer()
			.WithClients(2)
			.WithGameMode(AStealthGameMode::StaticClass())
			.Build(Network);
		Network.StartOnFixtureMap(WalkSlowerModifierTestData::FixtureMap);
	}

	AFTER_EACH()
	{
		Spy.Reset();
	}

	static UWorld* FirstClientWorld()
	{
		const FWorldContext* Best = nullptr;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE && Context.World() && Context.World()->GetNetMode() == NM_Client && (!Best || Context.PIEInstance < Best->PIEInstance))
			{
				Best = &Context;
			}
		}
		return Best ? Best->World() : nullptr;
	}

	static APlayerController* WalkerController() { UWorld* W = FirstClientWorld(); return W ? W->GetFirstPlayerController() : nullptr; }

	static UWorld* ServerWorldOf()
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE && Context.World() && Context.World()->GetNetMode() == NM_ListenServer)
			{
				return Context.World();
			}
		}
		return nullptr;
	}

	/** The server's copy of the walker's pawn (matched through the walker's server-side controller). */
	static ACharacter* ServerWalker(UWorld* Server)
	{
		const APlayerController* Client = WalkerController();
		const APlayerState* ClientState = Client ? Client->PlayerState.Get() : nullptr;
		for (FConstPlayerControllerIterator It = Server->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* PC = It->Get();
			if (PC && !PC->IsLocalController() && PC->PlayerState && ClientState && PC->PlayerState->GetPlayerId() == ClientState->GetPlayerId())
			{
				return Cast<ACharacter>(PC->GetPawn());
			}
		}
		return nullptr;
	}

	/** The open-circle search with this test's radius and margin (shared helper, Tests/PIE/StealthPIEOpenCircle.h). */
	static TOptional<FVector> FindOpenCircle(UWorld* World, const FVector& Origin, const AActor* Ignore)
	{
		using namespace WalkSlowerModifierTestData;
		StealthPIEOpenCircle::FParams Params;
		Params.RadiusCm = CircleRadiusCm;
		Params.MarginCm = CircleMarginCm;
		Params.SearchStepCm = SearchStepCm;
		Params.SearchRadiusCm = SearchRadiusCm;
		Params.BoxFloorGapCm = BoxFloorGapCm;
		Params.BoxHeightCm = BoxHeightCm;
		Params.FloorFlatnessCm = FloorFlatnessCm;
		Params.RimTraces = RimTraces;
		return StealthPIEOpenCircle::FindOpenCircle(World, Origin, Ignore, Params);
	}

	/**
	 * The control yaw moves toward the orbit heading by at most MaxYawStepDeg per frame (a mouse-like turn inside the walk
	 * turn rate), so the commanded direction never jumps away from the body facing (no idle pivot at a phase start).
	 */
	static double SteerToward(double CurrentYaw, double TargetYaw)
	{
		return StealthPIEOpenCircle::SteerToward(CurrentYaw, TargetYaw, WalkSlowerModifierTestData::MaxYawStepDeg);
	}

	/** Closed-loop orbit: the tangent of the circle round Centre, bent back towards the circle by the radial error. */
	static double OrbitYawDeg(const FVector& Location, const FVector& Centre)
	{
		using namespace WalkSlowerModifierTestData;
		return StealthPIEOpenCircle::OrbitYawDeg(Location, Centre, CircleRadiusCm, CircleCorrectionCm);
	}

	/** Points the walker's control yaw along its body facing (a start straight ahead: no idle pivot). */
	static void AlignControlToFacing()
	{
		if (APlayerController* PC = WalkerController())
		{
			if (const APawn* Owner = PC->GetPawn())
			{
				FRotator Control = PC->GetControlRotation();
				Control.Yaw = Owner->GetActorRotation().Yaw;
				PC->SetControlRotation(Control);
			}
		}
	}

	static void Key(const FKey& InKey, bool bDown)
	{
		if (APlayerController* PC = WalkerController())
		{
			PC->InputKey(FInputKeyEventArgs::CreateSimulated(InKey, bDown ? IE_Pressed : IE_Released, bDown ? 1.f : 0.f));
		}
	}

	TEST_METHOD(KeyboardModifiersScaleTheServerReceivedMove)
	{
		using namespace WalkSlowerModifierTestData;
		Network
			.Until(TEXT("the walker possesses its pawn on both sides"), []()
			{
				UWorld* Server = nullptr;
				for (const FWorldContext& Context : GEngine->GetWorldContexts())
				{
					Server = Context.WorldType == EWorldType::PIE && Context.World() && Context.World()->GetNetMode() == NM_ListenServer ? Context.World() : Server;
				}
				// R6: pawns exist in Preparing but commands are NotNow until Active (the real readiness sequence).
				const AStealthGameState* ClientState = WalkerController() && WalkerController()->GetWorld() ? WalkerController()->GetWorld()->GetGameState<AStealthGameState>() : nullptr;
				return Server && WalkerController() && Cast<AStealthPawn>(WalkerController()->GetPawn()) && ServerWalker(Server)
					&& ClientState != nullptr && ClientState->GetRoundPublicState().Phase == ERoundPhase::Active;
			})
			.Then(TEXT("attach the RPC spy"), [this]()
			{
				Spy = MakeUnique<FOutgoingRpcSpy>(FirstClientWorld());
				ASSERT_THAT(IsTrue(Spy->IsAttached(), TEXT("RPC spy attached to the walker's NetDriver")));
			})
			.Then(TEXT("move the walker onto an open circle"), [this]()
			{
				using namespace WalkSlowerModifierTestData;
				UWorld* Server = ServerWorldOf();
				ACharacter* Walker = Server ? ServerWalker(Server) : nullptr;
				ASSERT_THAT(IsTrue(Walker != nullptr, TEXT("the server's walker")));
				const float HalfHeight = Walker->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
				const TOptional<FVector> Centre = FindOpenCircle(Server, Walker->GetActorLocation() - FVector(0.0, 0.0, HalfHeight), Walker);
				ASSERT_THAT(IsTrue(Centre.IsSet(), TEXT("an open, flat circle near the walker's spawn")));
				CircleCentre = Centre.GetValue();
				// Start on the circle's +X point facing the anticlockwise tangent (+Y).
				CircleStart = CircleCentre + FVector(CircleRadiusCm, 0.0, 0.0);
				const FRotator Facing(0.0, 90.0, 0.0);
				Walker->GetCharacterMovement()->StopMovementImmediately();
				Walker->TeleportTo(CircleStart + FVector(0.0, 0.0, HalfHeight + 2.0), Facing, false, true);
				if (APlayerController* ServerPC = Cast<APlayerController>(Walker->GetController()))
				{
					ServerPC->SetControlRotation(Facing);
					ServerPC->ClientSetRotation(Facing, true);
				}
				// The owner is moved in the same frame (both worlds run in this process), so no correction is pending
				// when the first phase starts.
				if (APlayerController* OwnerPC = WalkerController())
				{
					if (ACharacter* Owner = Cast<ACharacter>(OwnerPC->GetPawn()))
					{
						Owner->GetCharacterMovement()->StopMovementImmediately();
						Owner->TeleportTo(CircleStart + FVector(0.0, 0.0, HalfHeight + 2.0), Facing, false, true);
					}
					OwnerPC->SetControlRotation(Facing);
				}
				TestRunner->AddInfo(FString::Printf(TEXT("WALKSLOWER open circle centre %s"), *CircleCentre.ToCompactString()));
			})
			.Until(TEXT("the owner arrives at the circle"), [this]()
			{
				using namespace WalkSlowerModifierTestData;
				const APlayerController* PC = WalkerController();
				const ACharacter* Owner = PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr;
				Mark = Mark ? Mark : GFrameCounter;
				const bool bArrived = Owner && FVector::Dist2D(Owner->GetActorLocation(), CircleStart) < ArriveDistanceCm && Owner->GetVelocity().Size() < ArriveSpeedCm && GFrameCounter >= Mark + SettleFrames;
				if (bArrived)
				{
					Mark = 0;
				}
				return bArrived;
			})
			// Warm-up (unchecked): a short walk round the circle and a stop, so a correction left over from the teleport (the
			// server replays the owner's first moves after it) lands here, never in the reference phase's step check.
			.Then(TEXT("warm up: walk"), [this]() { AlignControlToFacing(); Key(EKeys::W, true); Mark = GFrameCounter; })
			.Until(TEXT("warm up: walk the circle"), [this]()
			{
				using namespace WalkSlowerModifierTestData;
				APlayerController* PC = WalkerController();
				const ACharacter* Owner = PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr;
				if (PC && Owner)
				{
					FRotator Control = PC->GetControlRotation();
					Control.Yaw = SteerToward(Control.Yaw, OrbitYawDeg(Owner->GetActorLocation(), CircleCentre));
					PC->SetControlRotation(Control);
				}
				return GFrameCounter >= Mark + WarmUpWalkFrames;
			})
			.Then(TEXT("warm up: stop"), [this]() { Key(EKeys::W, false); Mark = GFrameCounter; RestFrames = 0; })
			.Until(TEXT("warm up: at rest and in agreement with the server"), [this]()
			{
				using namespace WalkSlowerModifierTestData;
				const APlayerController* PC = WalkerController();
				const ACharacter* Owner = PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr;
				UWorld* Server = ServerWorldOf();
				const ACharacter* OnServer = Server ? ServerWalker(Server) : nullptr;
				// Fully at rest on both copies (a stop still braking on one of them ends differently on the other, and the
				// server then corrects the owner inside the next phase), then SettleFrames more.
				const bool bAgree = Owner && OnServer && FVector::Dist2D(Owner->GetActorLocation(), OnServer->GetActorLocation()) < AgreeDistanceCm
					&& Owner->GetVelocity().Size() < RestSpeedCm && OnServer->GetVelocity().Size() < RestSpeedCm;
				RestFrames = bAgree ? RestFrames + 1 : 0;
				const bool bGaveUp = GFrameCounter >= Mark + AgreeMaxFrames;
				const bool bDone = (bAgree && RestFrames >= SettleFrames) || bGaveUp;
				if (bDone)
				{
					if (Owner && OnServer)
					{
						TestRunner->AddInfo(FString::Printf(TEXT("WALKSLOWER warm-up done: owner/server distance %.2f cm, owner speed %.2f, server speed %.2f%s"),
							FVector::Dist2D(Owner->GetActorLocation(), OnServer->GetActorLocation()), Owner->GetVelocity().Size(), OnServer->GetVelocity().Size(), bGaveUp ? TEXT(" (gave up waiting)") : TEXT("")));
					}
					Mark = 0;
				}
				return bDone;
			});

		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Phases); ++Index)
		{
			const FPhase Phase = Phases[Index];
			if (Phase.bInformant && (Index == 0 || !Phases[Index - 1].bInformant))
			{
				Network.Then(TEXT("the walker becomes an informant"), [this]()
				{
						// The informant stub state: the prototype owner state on the server (the Roster transition is not
						// driven in this PIE) and the informant context on the owner (story iif-071 wires it to the Roster).
						if (UWorld* Server = ServerWorldOf())
						{
							if (ACharacter* Walker = ServerWalker(Server))
							{
								if (AStealthPlayerState* State = Walker->GetPlayerState<AStealthPlayerState>())
								{
									State->LifeState = EStealthLifeState::Informant;
								}
							}
						}
						if (AStealthPlayerController* Owner = Cast<AStealthPlayerController>(WalkerController()))
						{
							// The swap ignores held keys until release (ADR-0011 E3): release W first; it is pressed afresh below.
							Key(EKeys::W, false);
							Key(EKeys::LeftControl, false);
							Key(EKeys::LeftAlt, false);
							Owner->GetCommandClient()->SetInformantContext(true);
						}
						bInformantSet = true;
									});
				Network.Until(TEXT("the informant context settles"), [this]() { Mark = Mark ? Mark : GFrameCounter; const bool bDone = GFrameCounter >= Mark + 15; if (bDone) { Mark = 0; } return bDone; });
			}
			Network
				.Then(L(FString::Printf(TEXT("phase %s: set the modifiers"), Phase.Name)), [this, Phase, Index]()
				{
					AlignControlToFacing();
					Key(EKeys::W, true);
					Key(EKeys::LeftControl, Phase.bCtrl);
					Key(EKeys::LeftAlt, Phase.bAlt);
					Key(EKeys::LeftShift, Phase.bRun);
					PhaseStart = GFrameCounter;
					Samples.AddDefaulted();
					LastSpeed = -1.0;
					Spy->Records.Reset();
				});
			Network.UntilServer(L(FString::Printf(TEXT("phase %s: 3 s, sampled at the end"), Phase.Name)), [this, Phase](FBasePIENetworkComponentState& Server)
				{
					ACharacter* Walker = ServerWalker(Server.World);
					if (Walker && GFrameCounter > PhaseStart + PhaseFrames - SampleFrames)
					{
						const UCharacterMovementComponent* Move = Walker->GetCharacterMovement();
						FSample& S = Samples.Last();
						S.MagnitudeSum += Move->GetCurrentAcceleration().Size() / FMath::Max(Move->GetMaxAcceleration(), 1.f);
						S.SpeedSum += Walker->GetVelocity().Size2D();
						// The pace the movement model targets (SCL F1: its max speed already applies the quantised pace).
						S.TargetSum += Move->GetMaxSpeed();
						++S.Count;
					}
					// No step above the movement component's own acceleration / braking model on any frame of the owner's
					// simulation (the server applies moves in packed batches, so per-server-tick deltas are not per-move).
					APlayerController* WalkerPC = WalkerController();
					const ACharacter* Owner = WalkerPC ? Cast<ACharacter>(WalkerPC->GetPawn()) : nullptr;
					if (WalkerPC && Owner)
					{
						// Closed loop round the fixed centre: the same circle at every pace and every frame time.
						FRotator Control = WalkerPC->GetControlRotation();
						Control.Yaw = SteerToward(Control.Yaw, OrbitYawDeg(Owner->GetActorLocation(), CircleCentre));
						WalkerPC->SetControlRotation(Control);
					}
					if (Owner && GFrameCounter <= PhaseStart + TransitionFrames)
					{
						const UCharacterMovementComponent* Move = Owner->GetCharacterMovement();
						const double Now = Owner->GetWorld()->GetTimeSeconds();
						const double Speed = Owner->GetVelocity().Size2D();
						if (LastSpeed >= 0.0 && Now > LastTime)
						{
							const double Dt = Now - LastTime;
							const double Friction = (Move->bUseSeparateBrakingFriction ? Move->BrakingFriction : Move->GroundFriction) * FMath::Max(Move->BrakingFrictionFactor, 0.f);
							// A reversal (W to S) combines acceleration with braking friction, so the bound is their sum: still the
							// movement component's own rates, never a snap.
							const double Limit = (Move->GetMaxAcceleration() + Move->GetMaxBrakingDeceleration() + Friction * FMath::Max(Speed, LastSpeed)) * Dt + 1.0;
							const double Over = FMath::Abs(Speed - LastSpeed) - Limit;
							if (Over > Samples.Last().MaxStepOver)
							{
								Samples.Last().MaxStepOver = Over;
								Samples.Last().StepFrame = GFrameCounter - PhaseStart;
								Samples.Last().StepFrom = LastSpeed;
								Samples.Last().StepTo = Speed;
							}
						}
						LastSpeed = Speed;
						LastTime = Now;
					}
					if (GFrameCounter >= PhaseStart + PhaseFrames)
					{
						Samples.Last().OtherRpcs = FOutgoingRpcSpy::Describe(Spy->Excluding([](FName F) { return FOutgoingRpcSpy::IsEngineBackground(F); }));
						return true;
					}
					return false;
				});
		}

		Network.Then(TEXT("release and assert"), [this]()
		{
			Key(EKeys::W, false);
			Key(EKeys::S, false);
			Key(EKeys::LeftControl, false);
			Key(EKeys::LeftAlt, false);
			Key(EKeys::LeftShift, false);
			const FSample& Reference = Samples[0];
			const double ReferenceSpeed = Reference.Count ? Reference.SpeedSum / Reference.Count : 0.0;
			const double ReferenceTarget = Reference.Count ? Reference.TargetSum / Reference.Count : 0.0;
			ASSERT_THAT(IsTrue(ReferenceSpeed > 10.0, L(FString::Printf(TEXT("the walker moved (reference speed %.1f cm/s)"), ReferenceSpeed))));
			for (int32 Index = 0; Index < Samples.Num(); ++Index)
			{
				const FPhase& Phase = Phases[Index];
				const FSample& S = Samples[Index];
				const double Magnitude = S.Count ? S.MagnitudeSum / S.Count : 0.0;
				const double Speed = S.Count ? S.SpeedSum / S.Count : 0.0;
				const double Target = S.Count ? S.TargetSum / S.Count : 0.0;
				TestRunner->AddInfo(FString::Printf(TEXT("WALKSLOWER %s: server-received magnitude %.3f (expected %.2f), target pace %.1f cm/s (ratio %.3f), measured speed %.1f cm/s, max step over limit %.2f (frame %llu, %.1f -> %.1f cm/s)"),
					Phase.Name, Magnitude, Phase.ExpectedMagnitude, Target, ReferenceTarget > 0.0 ? Target / ReferenceTarget : 0.0, Speed, S.MaxStepOver, S.StepFrame, S.StepFrom, S.StepTo));
				ASSERT_THAT(IsNear(Phase.ExpectedMagnitude, Magnitude, MagnitudeTolerance, L(FString::Printf(TEXT("%s: server-received Move magnitude"), Phase.Name))));
				if (Phase.bSkipStepCheck)
				{
					// The settle phase only absorbs the stop after the run and the informant swap.
				}
				else if (!Phase.bRun)
				{
					ASSERT_THAT(IsNear(Phase.ExpectedMagnitude, Target / ReferenceTarget, PaceRatioTolerance, L(FString::Printf(TEXT("%s: pace as a ratio of the walk speed"), Phase.Name))));
					// SCL-010: the absolute pace the server's movement model targets, WalkSpeedCmPerSec x the quantised pace
					// (112 / 126 / 140 cm/s).
					ASSERT_THAT(IsNear(140.0 * Phase.ExpectedMagnitude, Target, AbsolutePaceToleranceCm, L(FString::Printf(TEXT("%s: absolute pace %.1f cm/s"), Phase.Name, Target))));
				}
				else
				{
					ASSERT_THAT(IsNear(450.0, Target, AbsolutePaceToleranceCm, L(FString::Printf(TEXT("%s: the run tier at 450 cm/s (%.1f)"), Phase.Name, Target))));
				}
				if (!Phase.bSkipStepCheck)
				{
					// The open circle keeps the walker clear of everything, so the server's measured speed is the target pace
					// (a wall or pawn on the circle would show here first: the regression guard of the 2026-10-26 flake).
					ASSERT_THAT(IsNear(Target, Speed, AbsolutePaceToleranceCm, L(FString::Printf(TEXT("%s: measured speed %.1f cm/s at the target pace %.1f"), Phase.Name, Speed, Target))));
				}
				if (!Phase.bSkipStepCheck)
				{
					ASSERT_THAT(IsTrue(S.MaxStepOver <= 0.0, L(FString::Printf(TEXT("%s: no speed step above the acceleration/braking limit"), Phase.Name))));
				}
			}
			for (int32 Index = 0; Index < Samples.Num(); ++Index)
			{
				TestRunner->AddInfo(FString::Printf(TEXT("WALKSLOWER %s: other client-to-server RPCs [%s]"), Phases[Index].Name, *Samples[Index].OtherRpcs));
				// Since SCL-010 IA_Run consumes LeftShift and the gait rides ServerMove (FLAG_Custom_0): every phase, Run
				// included, sends nothing but the move family.
				if (!Phases[Index].bSkipStepCheck)
				{
					ASSERT_THAT(IsTrue(Samples[Index].OtherRpcs.IsEmpty(), L(FString::Printf(TEXT("%s: no client-to-server RPC beyond ServerMove: [%s]"), Phases[Index].Name, *Samples[Index].OtherRpcs))));
				}
			}
		});
	}
};

#endif
